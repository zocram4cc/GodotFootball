#!/usr/bin/env python3
"""driver.py: translate -> build -> diff over the pilot set, one command.

Usage:
  driver.py --image IMG --out-dir BUILD [--spec-dir DIR] [--only 0xVA ...]
            [--count N] [--asm-dir DIR] [--cxx CXX] [--timeout S]

Defaults: --spec-dir tools/dev/diffharness/specs/pilot (committed Map specs);
asm comes from --asm-dir bridge cache when given, else capstone decodes the
user's image at build time (no bridge needed).

Steps per function (deterministic order, sorted VAs):
  1. xlate.translate_one lifts the asm to build-tree C++ (pes_fn_<va>.cpp).
  2. run.py (unicorn over the user's image) generates the build corpus.
  3. diffmain.gen writes the diff main; g++ builds fn + in-set callees +
     main; the binary compares the corpus bit for bit (floats by IEEE bits).
  4. results.json gains per function {va, translated, diffed, cases,
     failure_class, note}. tools/dev/ledger/joins.py --pilot reads it;
     ledger.py --check stays green.

Generated code and corpora live in the build tree only (never committed).
results.json is committed (va/booleans/counts/class/notes; no PES bytes).
FAILURE_CLASSES: engine call | live state | GPU | translator bug |
  struct map gap | harness cannot run. Corpus-stage faults split live state
  (fault touches image .data/.bss globals or their chase targets) from
  struct map gap (fault in synthetic space = spec closure gap). A diffed
  function whose spec reads writable image globals is marked provisional in
  its note (on-disk values, not runtime-proven).
"""
import argparse
import concurrent.futures
import json
import os
import re
import subprocess
import shutil
import sys

FAILURE_CLASSES = ("engine call", "live state", "GPU", "translator bug",
                   "struct map gap", "harness cannot run",
                   "stub model wrong", "unmodellable callee")

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
RUN_PY = os.path.join(REPO, "tools", "dev", "diffharness", "run.py")
DEF_SPEC_DIR = os.path.join(REPO, "tools", "dev", "diffharness",
                            "specs", "pilot")
# The harness Python (unicorn + capstone), the same variable the gate and tests/diff use.
VENV_PY = os.environ.get("GF_DIFF_PYTHON", sys.executable)

sys.path.insert(0, HERE)
import stubs as stub_table
STUB_TABLE = stub_table.load()


def eprint(*a):
    print(*a, file=sys.stderr)


def sh(cmd, timeout=None):
    try:
        r = subprocess.run(cmd, capture_output=True, text=True,
                           timeout=timeout)
        return r.returncode, (r.stdout or "")[-2000:], (r.stderr or "")[-2000:]
    except subprocess.TimeoutExpired as e:
        out = (e.stdout or b"").decode("utf-8", "replace")[-2000:] \
            if isinstance(e.stdout, bytes) else (e.stdout or "")[-2000:]
        return 124, out, "timeout after %ss" % timeout


def load_spec(spec_dir, va):
    for cand in (va[2:] + ".json", va[2:].upper() + ".json",
                 va[2:].lower() + ".json"):
        p = os.path.join(spec_dir, cand)
        if os.path.exists(p):
            return json.load(open(p)), p
    return None, None


def asm_for(asm_dir, va):
    if not asm_dir:
        return ""
    for cand in (va[2:] + ".json", va[2:].upper() + ".json",
                 va[2:].lower() + ".json"):
        p = os.path.join(asm_dir, cand)
        if os.path.exists(p):
            return open(p, encoding="utf-8", errors="replace").read()
    return ""


UNMODELLED = re.compile(r"unmodelled (?:read|write) at (0x[0-9a-fA-F]+)")
IMAGE_BASE = 0x140000000


def fault_class(spec, err):
    """Classify a run.py corpus failure. live state: the faulting address lies in
    the image (an image global, or a chase target read from it). Otherwise the
    spec's synthetic closure is incomplete (struct map gap): a zero or unmapped
    pointer read from a region the spec does not populate."""
    m = UNMODELLED.search(err)
    if m and int(m.group(1), 16) >= IMAGE_BASE:
        return "live state", "harness fault at image VA %s" % m.group(1)
    failed = re.search(r"failed: ([^\n]*)", err)
    lines = [ln for ln in err.strip().splitlines() if ln.strip()]
    # A raised exception ends stderr with its one-line form; never a traceback
    # (its frames carry machine paths).
    detail = failed.group(1) if failed else (lines[-1] if lines else "run.py failed")
    return "struct map gap", "spec closure gap: %s" % detail[:220]


def diff_python():
    return os.environ.get("GF_DIFF_PYTHON") or \
        (VENV_PY if os.path.exists(VENV_PY) else sys.executable)


def translate_one(va, asm_text, image, pilot, stubs=None):
    import xlate
    return xlate.translate_one(image, asm_text, va, pilot, stubs=stubs)


def stubmap_of(spec):
    """{callee VA: stubs.json entry} for the spec's stub_refs (shared with run.py)."""
    out = {}
    for ref in spec.get("stub_refs") or []:
        entry = STUB_TABLE.get(int(ref, 16))
        if entry is None:
            raise SystemExit("stub_refs %s has no stubs.json entry" % ref)
        out["0x%X" % int(ref, 16)] = entry
    return out


def _blocker_dispatchable(text):
    """A harness blocker the runtime dispatch mechanism now handles: any
    indirect call (emitted as a table lookup) or computed jump (emitted as
    a native switch). Import slots with no name stay unmodellable: their
    target is a CRT/engine import outside the image."""
    t = text.strip()
    # Own-function sites are phrased "call <mem> at <site>" (the ImpureMap
    # precheck wording); closure sites "indirect call <mem> at <site>".
    if t.startswith("indirect call ") or t.startswith("call qword ptr ") \
            or t.startswith("call rsi"):
        return "import slot" not in t
    if t.startswith("computed jump at "):
        return True
    return False


def blocked_reason(spec):
    """Why the function cannot be run at all: its harness class, or an unmodellable stub_refs entry."""
    harness = spec.get("harness") or {}
    if str(harness.get("harness_class", "")).startswith("harness cannot run"):
        raw = harness.get("blockers") or ["harness cannot run"]
        # Dispatchable blockers (indirect calls, switch jumps) no longer
        # block: the translator emits a runtime lookup / native switch.
        # A blocker chain ("call A: callee B: indirect ...") blocks only
        # when some link is not dispatchable.
        live = [b for b in raw
                if not all(_blocker_dispatchable(p) for p in
                           re.split(r";?\s*(?:call|callee) [0-9A-Fa-f]+:\s*",
                                    b) if p.strip())]
        if live:
            return "; ".join(live)[:440]
        return None


def run_one(va, args, pilot):
    """Full pipeline for one function. Returns a result dict."""
    import diffmain
    res = {"va": va, "translated": False, "diffed": False, "cases": 0,
           "failure_class": None, "note": "", "stubs": []}
    spec, spec_path = load_spec(args.spec_dir, va)
    if spec is None:
        res["failure_class"] = "struct map gap"
        res["note"] = "no spec in %s" % args.spec_dir
        return res
    res["cases"] = spec.get("count", 0)
    reason = blocked_reason(spec)
    if reason is not None:
        res["failure_class"] = "unmodellable callee"
        res["note"] = reason[:220]
        return res
    gen = os.path.join(args.out_dir, "gen")
    os.makedirs(gen, exist_ok=True)
    tag = va[2:]
    # 1. translate: stubs.json models for the spec's stub_refs (stubs.py)
    stubmap = stubmap_of(spec)
    res["stubs"] = sorted(stubmap)
    try:
        asm_text = asm_for(args.asm_dir, va)
        src, _ = translate_one(va, asm_text, args.image, pilot,
                               stubs=stubmap)
        if src is None:
            res["failure_class"] = "translator bug"
            res["note"] = "xlate: no instructions"
            return res
    except Exception as e:  # noqa: BLE001 - recorded, not raised
        res["failure_class"] = "translator bug"
        res["note"] = "translate: %r" % (e,)[:220]
        return res
    with open(os.path.join(gen, "pes_fn_%s.cpp" % tag), "w") as f:
        f.write("\n".join(src) + "\n")
    res["translated"] = True
    # 2. corpus (harness over the user's image) + harness-reached
    # indirect targets (the driver's callee closure for dispatch).
    corpus = os.path.join(gen, "corp_%s.jsonl" % tag)
    trace = os.path.join(gen, "trace_%s.json" % tag)
    count = ["--count", str(args.count)] if args.count else []
    rc, _, err = sh([diff_python(), RUN_PY, "--exe", args.image,
                     "--spec", spec_path, "--out", corpus,
                     "--trace-calls", trace] + count,
                    timeout=args.timeout)
    if rc != 0:
        cls, note = fault_class(spec, err or "run.py rc=%d" % rc)
        res["failure_class"] = cls
        res["note"] = note
        return res
    nlines = 0
    try:
        with open(corpus) as f:
            for _ in f:
                nlines += 1
    except OSError:
        pass
    if nlines == 0:
        # Empty corpus: the function never reaches its end under the
        # harness (e.g. tail-jump live chase). Nothing to diff.
        res["failure_class"] = "harness cannot run"
        res["note"] = "corpus empty (0 cases)"
        return res
    res["cases"] = nlines
    return res


def build_and_diff(va, spec, args, pilot, nlines):
    """Compile fn + in-set callees + diff main, run, compare. Returns
    (ok, note, failure_class). Callee list comes from the spec precheck
    (committed), so the build needs no analysis scratch."""
    import diffmain
    gen = os.path.join(args.out_dir, "gen")
    tag = va[2:]
    stubmap = stubmap_of(spec)
    # The build declares only callees it links: pilot-universe members that are not stubbed.
    callees = ["0x" + c.upper() for c in (spec.get("precheck", {}) or {}).get("calls", [])]
    callees = [c for c in callees if c in pilot and c not in stubmap]
    # translate callees too (their TUs must exist to link). Closure is
    # transitive: a callee's own callees must also link (e.g. 140A53DA0
    # -> 140439EF0). Edges come from each spec's precheck calls.
    speccache = {va: spec}
    def calls_of(v):
        s = speccache.get(v)
        if s is None:
            s, _ = load_spec(args.spec_dir, v)
            speccache[v] = s
        return ["0x" + c.upper()
                for c in ((s.get("precheck", {}) or {}).get("calls", [])
                          if s else [])]
    need = [va]
    seen = {va}
    queue = [va]
    while queue:
        cur = queue.pop(0)
        for c in calls_of(cur):
            if c in pilot and c not in seen:
                seen.add(c)
                need.append(c)
                queue.append(c)
    # Harness-reached indirect targets: unicorn executes the real bytes,
    # so the trace holds exactly the dispatch targets the cases reach.
    # Each reached target joins the closure (translated + table entry).
    # A reached target outside CODE (e.g. an import thunk) stays a loud
    # PES_DISPATCH_UNTRANSLATED abort, never a silent stub.
    reached = {}
    try:
        reached = json.load(open(os.path.join(gen, "trace_%s.json" % tag)))
    except (OSError, ValueError):
        reached = {}
    for site, tgts in (reached or {}).items():
        for t in tgts:
            ti = int(t, 16)
            if 0x140001000 <= ti < 0x14252E800 and t not in seen:
                seen.add(t)
                need.append(t)
    for cvi, cv in enumerate(need):
        p = os.path.join(gen, "pes_fn_%s.cpp" % cv[2:])
        if not os.path.exists(p):
            asp, _ = load_spec(args.spec_dir, cv)
            try:
                csrc, _ = translate_one(cv, asm_for(args.asm_dir, cv),
                                        args.image, pilot, stubs=stubmap)
                if csrc is None:
                    return False, "callee %s: no instructions" % cv, \
                        "translator bug"
            except Exception as e:  # noqa: BLE001
                return False, "callee %s translate: %r" % (cv, e)[:220], \
                    "translator bug"
            with open(p, "w") as f:
                f.write("\n".join(csrc) + "\n")
    # Tail jumps reference siblings the spec calls list misses (JMP, not
    # CALL). Close over the sibling refs in the generated TUs themselves.
    changed = True
    while changed:
        changed = False
        for cv in list(need):
            p = os.path.join(gen, "pes_fn_%s.cpp" % cv[2:])
            try:
                body = open(p).read()
            except OSError:
                continue
            for m in re.finditer(r"pes_fn_([0-9A-Fa-f]+)\(", body):
                dep = "0x" + m.group(1).upper()
                if dep != cv and dep in pilot and dep not in seen:
                    seen.add(dep)
                    need.append(dep)
                    changed = True
    # translate any newly discovered deps (loop until fixpoint)
    pending = [cv for cv in need
               if not os.path.exists(
                   os.path.join(gen, "pes_fn_%s.cpp" % cv[2:]))]
    while pending:
        cv = pending.pop(0)
        asp, _ = load_spec(args.spec_dir, cv)
        try:
            csrc, _ = translate_one(cv, asm_for(args.asm_dir, cv),
                                    args.image, pilot, stubs=stubmap)
            if csrc is None:
                return False, "callee %s: no instructions" % cv, \
                    "translator bug"
        except Exception as e:  # noqa: BLE001
            return False, "callee %s translate: %r" % (cv, e)[:220], \
                "translator bug"
        with open(os.path.join(gen, "pes_fn_%s.cpp" % cv[2:]), "w") as f:
            f.write("\n".join(csrc) + "\n")
        try:
            body = open(os.path.join(gen, "pes_fn_%s.cpp" % cv[2:])).read()
        except OSError:
            continue
        for m in re.finditer(r"pes_fn_([0-9A-Fa-f]+)\(", body):
            dep = "0x" + m.group(1).upper()
            if dep != cv and dep in pilot and dep not in seen:
                seen.add(dep)
                need.append(dep)
                pending.append(dep)
    main_src = diffmain.gen(spec, va, callees)
    main_p = os.path.join(gen, "diff_%s.cpp" % tag)
    with open(main_p, "w") as f:
        f.write(main_src)
    # support headers into the build tree (committed tools, copied)
    for h in ("pes_state.hpp", "pes_cond.inl", "diffparse.hpp",
              "pes_stubs.hpp", "pes_dispatch.hpp"):
        hp = os.path.join(gen, h)
        if not os.path.exists(hp):
            with open(os.path.join(HERE, h), "rb") as fi, \
                    open(hp, "wb") as fo:
                fo.write(fi.read())
    # Dispatch table: every translated TU in this build. Generated, no hand
    # edits (gen_dispatch.py).
    import subprocess as _sp
    _sp.run([sys.executable, os.path.join(HERE, "gen_dispatch.py"),
             "--out", gen, "--vas"] + sorted(need), check=True)
    bin_p = os.path.join(gen, "d_%s" % tag)
    objs = [os.path.join(gen, "pes_fn_%s.cpp" % cv[2:]) for cv in need]
    rc, _, err = sh([args.cxx, "-std=c++20", "-O1", "-I", gen, "-o", bin_p,
                     main_p] + objs, timeout=300)
    if rc != 0:
        # keep the informative lines: errors plus undefined references
        # (ld failures print the missing symbol, not the word 'error')
        lines = [l.replace(args.out_dir, "<build>")
                 for l in err.splitlines()
                 if "error" in l or "undefined reference" in l][:6]
        return False, "compile: %s" % " | ".join(lines)[:400], \
            "translator bug"
    corpus = os.path.join(gen, "corp_%s.jsonl" % tag)
    rc, out, err = sh([bin_p, corpus, args.image], timeout=args.timeout)
    m = re.search(r"(\d+) cases, (\d+) mismatch", out)
    if rc == 0 and m and int(m.group(2)) == 0:
        note = "%d cases bit-exact" % int(m.group(1))
        pre = spec.get("precheck", {}) or {}
        if pre.get("live_data_reads") or pre.get("live_data_writes"):
            note += "; provisional (live-state globals at on-disk values)"
        return True, note, None
    detail = (m.group(0) + " " if m else "") + \
        " ".join((out + err).splitlines()[-3:])
    # Never commit machine paths: the out dir is build-tree scratch.
    detail = detail.replace(args.out_dir, "<build>")
    return False, "diff: %s" % detail[:240], "translator bug"


def finish_one(va, args, pilot):
    res = run_one(va, args, pilot)
    # Coverage (recorded, never counted as diffed): harness-reached
    # indirect sites and their fan-out. A function whose closure reaches
    # code the cases never execute is never recorded as bit-exact on
    # partial coverage: the sites the cases missed stay visible here.
    try:
        trace = json.load(open(os.path.join(
            args.out_dir, "gen", "trace_%s.json" % va[2:])))
    except (OSError, ValueError):
        trace = {}
    res["coverage"] = {site: sorted(tgts) for site, tgts in
                       (trace or {}).items()}
    if res["translated"] and res["failure_class"] is None and \
            res["cases"]:
        spec, _ = load_spec(args.spec_dir, va)
        ok, note, cls = build_and_diff(va, spec, args, pilot, res["cases"])
        res["diffed"] = ok
        res["failure_class"] = cls
        res["note"] = note
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--image", required=True)
    ap.add_argument("--out-dir", required=True)
    ap.add_argument("--spec-dir", default=DEF_SPEC_DIR)
    ap.add_argument("--asm-dir", default=None)
    ap.add_argument("--only", nargs="*", default=None)
    ap.add_argument("--count", type=int, default=None)
    ap.add_argument("--cxx", default="g++")
    ap.add_argument("--timeout", type=int, default=600)
    ap.add_argument("--jobs", type=int, default=2)
    ap.add_argument("--check", action="store_true",
                    help="re-run every VA results.json records as diffed; exit 1 if any no longer diffs bit-exact. "
                         "Never writes the repo results.json (the gate's pilot test).")
    args = ap.parse_args()
    pilot_json = os.path.join(REPO, "tools", "dev", "translate", "results.json")
    if args.check:
        args.only = [r["va"] for r in json.load(open(pilot_json))["results"] if r["diffed"]]
    is_pilot = os.path.abspath(args.spec_dir) == os.path.abspath(DEF_SPEC_DIR)
    repo_json = os.path.join(REPO, "tools", "dev", "translate",
                             "results.json" if is_pilot else "results_impure.json")

    specs = sorted(f for f in os.listdir(args.spec_dir)
                   if f.endswith(".json"))
    # The pilot (call-closed universe) always spans ALL specs; --only
    # filters which functions get results, not the callee universe.
    # Otherwise callees outside --only would mistranslate as out-of-set
    # stubs and every --only diff with calls would spuriously mismatch.
    pilot = {"0x" + f[:-5].upper() for f in os.listdir(DEF_SPEC_DIR) if f.endswith(".json")}
    pilot |= {"0x" + f[:-5].upper() for f in specs}
    # run-real closure callees (stubs.json) are translated like pilot members
    pilot |= {"0x%X" % va for va, entry in STUB_TABLE.items() if entry["do"] is None}
    if args.only:
        want = {v.lower().replace("0x", "") for v in args.only}
        specs = [f for f in specs if f[:-5].lower() in want]
    os.makedirs(args.out_dir, exist_ok=True)
    # A run generates everything afresh: callee TUs and support headers are reused within a run when they exist, so a
    # gen dir left by an older translator would link stale code (the gate's build dir persists between gates).
    shutil.rmtree(os.path.join(args.out_dir, "gen"), ignore_errors=True)

    results = []
    if args.jobs and args.jobs > 1:
        with concurrent.futures.ThreadPoolExecutor(
                max_workers=args.jobs) as ex:
            futs = {ex.submit(finish_one, "0x" + f[:-5].upper(), args,
                              pilot): f for f in specs}
            by_va = {}
            for fut in concurrent.futures.as_completed(futs):
                r = fut.result()
                by_va[r["va"]] = r
            results = [by_va["0x" + f[:-5].upper()] for f in specs]
    else:
        for f in specs:
            results.append(
                finish_one("0x" + f[:-5].upper(), args, pilot))
    for r in results:
        print("%s translated=%s diffed=%s cases=%s class=%s %s" % (
            r["va"], r["translated"], r["diffed"], r["cases"],
            r["failure_class"], r["note"][:100]))

    # results.json: build copy always; the committed repo copy only from a full run (a subset or a check run
    # must never overwrite the record of the whole set). Sort by VA.
    results.sort(key=lambda r: r["va"])
    payload = {"results": results,
               "note": ("%s results. diffed = bit-exact vs the unicorn harness. "
                        "'provisional' in a note means the spec reads writable image "
                        "globals at on-disk values. 'stubs' lists the callees that "
                        "stubs.json models." % ("Pilot" if is_pilot else "Impure-sample"))}
    build_json = os.path.join(args.out_dir, "results.json")
    json.dump(payload, open(build_json, "w"), indent=1, sort_keys=True)
    if not args.only:
        json.dump(payload, open(repo_json, "w"), indent=1, sort_keys=True)
    att = sum(1 for _ in results)
    tra = sum(1 for r in results if r["translated"])
    dif = sum(1 for r in results if r["diffed"])
    print("driver: attempted %d translated %d diffed %d -> %s"
          % (att, tra, dif, build_json))
    if args.check:
        lost = [r["va"] for r in results if not r["diffed"]]
        if lost:
            print("driver --check: %d recorded-diffed functions no longer diff: %s" % (len(lost), " ".join(lost)))
            return 1
        print("driver --check: all %d recorded-diffed functions still diff bit-exact" % att)
    return 0


if __name__ == "__main__":
    sys.exit(main())
