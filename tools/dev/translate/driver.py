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
import sys

FAILURE_CLASSES = ("engine call", "live state", "GPU", "translator bug",
                   "struct map gap", "harness cannot run")

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
RUN_PY = os.path.join(REPO, "tools", "dev", "diffharness", "run.py")
DEF_SPEC_DIR = os.path.join(REPO, "tools", "dev", "diffharness",
                            "specs", "pilot")
VENV_PY = "/mnt/Extra4/gf-DiffHarnessRE-scratch/venv/bin/python"

sys.path.insert(0, HERE)


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


def fault_class(spec, err):
    """Classify a run.py corpus failure from its stderr + spec precheck."""
    pre = spec.get("precheck", {}) if spec else {}
    m = re.search(r"(unmodelled (?:read|write)|Invalid memory mapping)[^\n]*"
                  r"(0x[0-9a-fA-F]+)?", err)
    addr = int(m.group(2), 16) if m and m.group(2) else None
    live = pre.get("live_data_reads") or pre.get("live_data_writes")
    if addr is not None and addr >= 0x140000000:
        # Fault inside the image: a global (or its chase target) the spec
        # does not model -> live state.
        return "live state", "harness fault at image VA %#x: %s" % (
            addr, (m.group(0) if m else "")[:160])
    if live:
        return "live state", "spec reads/writes live globals %s; %s" % (
            live, (m.group(0) if m else err[-160:]))
    tag = (m.group(0) if m else err[-200:]).replace("\n", " ")
    return "struct map gap", "spec closure gap: %s" % tag[:220]


def diff_python():
    return os.environ.get("GF_DIFF_PYTHON") or \
        (VENV_PY if os.path.exists(VENV_PY) else sys.executable)


def translate_one(va, asm_text, image, pilot, stubs=None):
    import xlate
    return xlate.translate_one(image, asm_text, va, pilot, stubs=stubs)


def run_one(va, args, pilot):
    """Full pipeline for one function. Returns a result dict."""
    import diffmain
    res = {"va": va, "translated": False, "diffed": False, "cases": 0,
           "failure_class": None, "note": ""}
    spec, spec_path = load_spec(args.spec_dir, va)
    if spec is None:
        res["failure_class"] = "struct map gap"
        res["note"] = "no spec in %s" % args.spec_dir
        return res
    res["cases"] = spec.get("count", 0)
    gen = os.path.join(args.out_dir, "gen")
    os.makedirs(gen, exist_ok=True)
    tag = va[2:]
    # 1. translate (spec stubs, if any, become fixed returns in the TU)
    stubmap = {}
    for _s in (spec.get("stubs") or {}).values():
        stubmap["0x" + _s["va"][2:].upper()] = _s
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
    # 2. corpus (harness over the user's image)
    corpus = os.path.join(gen, "corp_%s.jsonl" % tag)
    count = ["--count", str(args.count)] if args.count else []
    rc, _, err = sh([diff_python(), RUN_PY, "--exe", args.image,
                     "--spec", spec_path, "--out", corpus] + count,
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
    callees = ["0x" + c.upper() for c in (spec.get("precheck", {}) or {}).get("calls", [])]
    stubmap = {}
    for _s in (spec.get("stubs") or {}).values():
        stubmap["0x" + _s["va"][2:].upper()] = _s
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
    for cvi, cv in enumerate(need):
        p = os.path.join(gen, "pes_fn_%s.cpp" % cv[2:])
        if os.path.exists(p):
            continue
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
    main_src = diffmain.gen(spec, va, callees)
    main_p = os.path.join(gen, "diff_%s.cpp" % tag)
    with open(main_p, "w") as f:
        f.write(main_src)
    # support headers into the build tree (committed tools, copied)
    for h in ("pes_state.hpp", "pes_cond.inl", "diffparse.hpp"):
        hp = os.path.join(gen, h)
        if not os.path.exists(hp):
            with open(os.path.join(HERE, h), "rb") as fi, \
                    open(hp, "wb") as fo:
                fo.write(fi.read())
    bin_p = os.path.join(gen, "d_%s" % tag)
    objs = [os.path.join(gen, "pes_fn_%s.cpp" % cv[2:]) for cv in need]
    rc, _, err = sh([args.cxx, "-std=c++20", "-O1", "-I", gen, "-o", bin_p,
                     main_p] + objs, timeout=300)
    if rc != 0:
        # keep the informative lines: errors plus undefined references
        # (ld failures print the missing symbol, not the word 'error')
        lines = [l for l in err.splitlines()
                 if "error" in l or "undefined reference" in l][:6]
        return False, "compile: %s" % " | ".join(lines)[:400], \
            "translator bug"
    corpus = os.path.join(gen, "corp_%s.jsonl" % tag)
    rc, out, err = sh([bin_p, corpus], timeout=args.timeout)
    m = re.search(r"(\d+) cases, (\d+) mismatch", out)
    if rc == 0 and m and int(m.group(2)) == 0:
        note = "%d cases bit-exact" % int(m.group(1))
        pre = spec.get("precheck", {}) or {}
        if pre.get("live_data_reads") or pre.get("live_data_writes"):
            note += "; provisional (live-state globals at on-disk values)"
        return True, note, None
    detail = (m.group(0) + " " if m else "") + \
        " ".join((out + err).splitlines()[-3:])
    return False, "diff: %s" % detail[:240], "translator bug"


def finish_one(va, args, pilot):
    res = run_one(va, args, pilot)
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
    args = ap.parse_args()

    specs = sorted(f for f in os.listdir(args.spec_dir)
                   if f.endswith(".json"))
    # The pilot (call-closed universe) always spans ALL specs; --only
    # filters which functions get results, not the callee universe.
    # Otherwise callees outside --only would mistranslate as out-of-set
    # stubs and every --only diff with calls would spuriously mismatch.
    pilot = {"0x" + f[:-5].upper() for f in specs}
    if args.only:
        want = {v.lower().replace("0x", "") for v in args.only}
        specs = [f for f in specs if f[:-5].lower() in want]
    os.makedirs(args.out_dir, exist_ok=True)

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

    # results.json: committed (repo copy) + build copy. Sort by VA.
    results.sort(key=lambda r: r["va"])
    payload = {"results": results,
               "note": "Pilot translation results. diffed = bit-exact vs "
                       "the unicorn harness. 'provisional' in a note means "
                       "the spec reads writable image globals at on-disk "
                       "values."}
    build_json = os.path.join(args.out_dir, "results.json")
    json.dump(payload, open(build_json, "w"), indent=1, sort_keys=True)
    repo_json = os.path.join(REPO, "tools", "dev", "translate",
                             "results.json")
    json.dump(payload, open(repo_json, "w"), indent=1, sort_keys=True)
    att = sum(1 for _ in results)
    tra = sum(1 for r in results if r["translated"])
    dif = sum(1 for r in results if r["diffed"])
    print("driver: attempted %d translated %d diffed %d -> %s"
          % (att, tra, dif, build_json))
    return 0


if __name__ == "__main__":
    sys.exit(main())
