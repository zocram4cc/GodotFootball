#!/usr/bin/env python3
"""driver.py: translate -> build -> diff over the pilot set, one command.

Usage:
  driver.py --image IMG --out-dir BUILD [--spec-dir DIR] [--only 0xVA ...]
            [--count N] [--asm-dir DIR] [--cxx CXX] [--timeout S] [--record FILE] [--set FILE] [--check]

Defaults: --spec-dir tools/dev/diffharness/specs/pilot (committed Map specs);
asm comes from --asm-dir bridge cache when given, else capstone decodes the
user's image at build time (no bridge needed).
--record FILE: a full run (no --only) writes its results there; --check re-runs the diffed VAs FILE lists and never writes it.
--set FILE: a generator set file (specgen sets/*.txt); each result gets the subsystem and role of its row.

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

# Coverage floor (provisional): a function is `diffed` only when its
# own-block coverage fraction reaches COVERAGE_FLOOR. Value from the
# pilot/impure baselines: pilot median 86% (37/216 below 50%), impure
# median 17% (25/31 below 50%). 0.50 keeps the pilot's proven bulk while
# forcing the impure sliver upward; revisit once guided generation lands.
COVERAGE_FLOOR = 0.50

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
RUN_PY = os.path.join(REPO, "tools", "dev", "diffharness", "run.py")
DEF_SPEC_DIR = os.path.join(REPO, "tools", "dev", "diffharness",
                            "specs", "pilot")
# The committed translated layer: the runtime headers the generated code includes.
TRANSLATED_DIR = os.path.join(REPO, "src", "pes", "translated")
# The harness Python (unicorn + capstone), the same variable the gate and tests/diff use.
VENV_PY = os.environ.get("GF_DIFF_PYTHON", sys.executable)

sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(REPO, "tools", "dev", "diffharness"))
import stubs as stub_table
import bankstore
STUB_TABLE = stub_table.load()


def universe_of(spec_dir):
    """The call-closed universe a run over spec_dir translates into: the pilot's specs, the run's specs, and the run-real closure (stubs.json)."""
    pilot = {"0x" + f[:-5].upper() for f in os.listdir(DEF_SPEC_DIR) if f.endswith(".json")}
    pilot |= {"0x" + f[:-5].upper() for f in os.listdir(spec_dir) if f.endswith(".json")}
    # run-real closure callees (stubs.json) are translated like pilot members
    pilot |= {"0x%X" % va for va, entry in STUB_TABLE.items() if entry["do"] is None}
    return pilot


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

def read_attribution(path):
    """{VA: (subsystem, role)} from a generator set file's third and fourth columns (`select` writes them)."""
    out = {}
    with open(path, encoding="utf8") as f:
        for line in f:
            parts = line.split("#")[0].rstrip("\n").split("\t")
            if len(parts) >= 4 and parts[0].strip():
                out["0x%X" % int(parts[0].strip(), 16)] = (parts[2].strip(), parts[3].strip())
    return out


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
    if "abort trap" in err:
        # The tamper path fired on some case: genuinely unrunnable input.
        return "unmodellable callee", \
            "abort trap reached: %s" % err.strip().splitlines()[-1][:200]
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


# Abort traps model in every build (a mid-closure CALL to one must be loud,
# never OUT-OF-SET zero). Merged at each translate call, never recorded in
# res["stubs"] (build-wide, not a spec input).
ABORTMAP = {"0x%X" % va: entry for va, entry in STUB_TABLE.items()
            if entry["do"] == "abort"}
def _why_texts(why):
    if why is None:
        return []
    if isinstance(why, str):
        return [why]
    return list(why)


def _why_dispatchable(why):
    """A stubs.json why-chain the build can run: every link an indirect
    call or computed jump (runtime dispatch / native switch). Import
    slots, global stores, recursion and depth stay unmodellable."""
    texts = _why_texts(why)
    if not texts:
        return True
    for t in texts:
        chains = [c for c in
                  re.split(r";\s*(?=call [0-9A-Fa-f]+:)", t) if c.strip()]
        for c in chains or [t]:
            parts = [p for p in
                     re.split(r";?\s*(?:call|callee) [0-9A-Fa-f]+:\s*", c)
                     if p.strip()]
            if not parts:
                # A bare reason ("recursion", "call depth > 10").
                return False
            if not all(_blocker_dispatchable(p) for p in parts):
                return False
    return True


def runnable_extra(spec, image, depth_cap=25):
    """Out-of-set direct callees the build can translate and call for
    real: stubs.json run-real kinds plus unmodellable entries whose whole
    why-chain is dispatchable. Transitively closed over direct CALLs
    decoded from the image (capstone, no bridge). Returns (extra, blocked)
    where blocked is {va: reason} for callees that stay unmodellable."""
    import xlate
    harness = spec.get("harness") or {}
    seeds = []
    for e in harness.get("out_of_set_callees") or []:
        if (e.get("how") or "call") != "call":
            continue
        va = "0x" + str(e.get("va", "")).upper().replace("0X", "")
        entry = STUB_TABLE.get(int(va, 16))
        if entry is None:
            continue
        if entry["do"] is None:
            seeds.append(va)  # run-real: image bytes execute either way
        elif entry["do"] == "unmodellable" and \
                _why_dispatchable(entry.get("why")):
            seeds.append(va)
    extra = []
    blocked = {}
    seen = set()
    def direct_calls(va):
        try:
            lines = xlate.capstone_decode(image, int(va, 16))
        except Exception:
            return None
        out = []
        for _, mn, ops in lines:
            if mn == "CALL" and ops and "[" not in ops[0]:
                try:
                    out.append("0x%X" % int(ops[0].strip(), 16))
                except ValueError:
                    pass
        return out
    stack = [(v, 0, [v]) for v in seeds]
    EXTRA_CAP = 60
    while stack:
        if len(extra) + len(stack) > EXTRA_CAP:
            for v, _, _ in stack:
                if v not in seen:
                    blocked[v] = "closure larger than %d" % EXTRA_CAP
            break
        va, depth, chain = stack.pop()
        if va in seen:
            continue
        if depth > depth_cap:
            blocked[va] = "call depth > %d" % depth_cap
            continue
        calls = direct_calls(va)
        if calls is None:
            blocked[va] = "undecodable from the image"
            continue
        seen.add(va)
        extra.append(va)
        for c in calls:
            if c in seen:
                continue
            if any(c == v for v in chain):
                blocked[c] = "recursion through %s" % c
                continue
            e = STUB_TABLE.get(int(c, 16))
            if e is None:
                # No stubs entry: in-set (the normal closure handles it) or
                # plain code the harness runs as image bytes. Plain CODE
                # joins the build; anything outside CODE stays blocked.
                ci = int(c, 16)
                if 0x140001000 <= ci < 0x14252E800:
                    stack.append((c, depth + 1, chain + [c]))
                else:
                    blocked[c] = "call outside code range"
                continue
            if e["do"] == "abort":
                # Modelled by the stub (ABORTMAP), never translated: the
                # thunk body is one indirect jump the translator cannot see
                # through, while the stub is loud on both sides.
                continue
            if e["do"] is None or \
                    (e["do"] == "unmodellable" and
                     _why_dispatchable(e.get("why"))):
                stack.append((c, depth + 1, chain + [c]))
            else:
                blocked[c] = "; ".join(_why_texts(e.get("why")) or [e["do"]])[:200]
    return extra, blocked


def _blocker_dispatchable(text):
    """A harness blocker the build now handles: any indirect call (a table
    lookup) or computed jump (a native switch). Abort-trap import slots
    stay loud on both sides (harness error, C++ abort()), so a corpus
    behind one proves the path unreached. Recursion and bounded depth are
    attempted: both languages recurse, and corpus success is the proof.
    Other import slots and global stores stay unmodellable."""
    t = text.strip()
    if "import slot" in t:
        # Bare slot reasons ("callee T: import slot V ...") as well as site
        # phrasings: only abort traps proceed (loud on both sides).
        m = re.search(r"import slot ([0-9A-Fa-f]+)", t)
        return m is not None and \
            int(m.group(1), 16) in stub_table.ABORT_SLOTS
    # Own-function sites are phrased "call <mem> at <site>" (the ImpureMap
    # precheck wording); closure sites "indirect call <mem> at <site>".
    if t.startswith("indirect call ") or t.startswith("call qword ptr ") \
            or t.startswith("call rsi"):
        return True
    if t.startswith("computed jump at "):
        return True
    if t.startswith("recursion through ") or t.startswith("call depth"):
        return True
    return False


def blocked_reason(spec):
    """Why the function cannot be run at all: its harness class, or an unmodellable stub_refs entry."""
    harness = spec.get("harness") or {}
    if str(harness.get("harness_class", "")).startswith("harness cannot run"):
        raw = harness.get("blockers") or ["harness cannot run"]
        # Dispatchable blockers (indirect calls, switch jumps) no longer
        # block: the translator emits a runtime lookup / native switch.
        # Each blocker holds '; '-joined chains ("call A: callee B: indirect
        # ..."); a chain blocks only when some link is not dispatchable.
        live = []
        for b in raw:
            chains = [c for c in
                      re.split(r";\s*(?=call [0-9A-Fa-f]+:)", b) if c.strip()]
            for c in chains or [b]:
                links = [p for p in
                         re.split(r";?\s*(?:call|callee) [0-9A-Fa-f]+:\s*", c)
                         if p.strip()]
                if not all(_blocker_dispatchable(p) for p in links):
                    live.append(c.strip()[:440])
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
    tag = va[2:]
    # Per-VA build dir: parallel builds share nothing (parallel runs with
    # overlapping closures raced on shared TUs and the dispatch header).
    gen = os.path.join(args.out_dir, "gen", tag)
    os.makedirs(gen, exist_ok=True)
    # 1. translate: stubs.json models for the spec's stub_refs (stubs.py).
    # Runnable out-of-set callees join this build's universe so the CALL
    # emitter links them instead of zeroing rax (OUT-OF-SET).
    stubmap = stubmap_of(spec)
    res["stubs"] = sorted(stubmap)
    build_pilot = set(pilot) | set(runnable_extra(spec, args.image)[0])
    try:
        asm_text = asm_for(args.asm_dir, va)
        src, _ = translate_one(va, asm_text, args.image, build_pilot,
                               stubs={**stubmap, **ABORTMAP})
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
    # indirect targets (the driver's callee closure for dispatch) +
    # executed addresses (block coverage).
    corpus = os.path.join(gen, "corp_%s.jsonl" % tag)
    trace = os.path.join(gen, "trace_%s.json" % tag)
    addrs = os.path.join(gen, "addrs_%s.json" % tag)
    count = ["--count", str(args.count)] if args.count else []
    seed_args = (["--seed-state", args.seed_state] if args.seed_state else []) \
        + (["--seed-bank", args.seed_bank] if args.seed_bank else []) \
        + (["--perturb"] if args.perturb else [])
    rc, _, err = sh([diff_python(), RUN_PY, "--exe", args.image,
                     "--spec", spec_path, "--out", corpus,
                     "--trace-calls", trace,
                     "--trace-addrs", addrs] + count + seed_args,
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
    tag = va[2:]
    gen = os.path.join(args.out_dir, "gen", tag)
    stubmap = stubmap_of(spec)
    # Runnable out-of-set callees (dispatchable switches/indirects): the
    # build translates them and calls them for real, like the harness runs
    # their image bytes. They join the in-set universe for this build only,
    # so the CALL emitter links them instead of zeroing rax (OUT-OF-SET).
    extra, extra_blocked = runnable_extra(spec, args.image)
    build_pilot = set(pilot) | set(extra)
    # The build declares only callees it links: pilot-universe members that are not stubbed.
    callees = ["0x" + c.upper() for c in (spec.get("precheck", {}) or {}).get("calls", [])]
    callees = [c for c in callees if c in build_pilot and c not in stubmap]
    callees += [c for c in extra if c not in callees]
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
            if c in build_pilot and c not in seen:
                seen.add(c)
                need.append(c)
                queue.append(c)
    for c in extra:
        if c not in seen:
            seen.add(c)
            need.append(c)
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
                                        args.image, build_pilot, stubs={**stubmap, **ABORTMAP})
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
                if dep != cv and dep in build_pilot and dep not in seen:
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
                                    args.image, build_pilot, stubs={**stubmap, **ABORTMAP})
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
            if dep != cv and dep in build_pilot and dep not in seen:
                seen.add(dep)
                need.append(dep)
                pending.append(dep)
    seed_art = None
    if args.seed_state:
        seed_path_ = os.path.join(args.seed_state, tag + ".seed.json")
        if os.path.exists(seed_path_):
            seed_art = json.load(open(seed_path_))
    instances = [seed_art]
    if args.seed_bank:
        # Streamed: write_instances reads one bank instance at a time.
        instances = bankstore.Bank(os.path.join(args.seed_bank, tag + ".bank.jsonl"))
    inst_p = os.path.join(gen, "inst_%s.bin" % tag)
    diffmain.write_instances(spec, instances, inst_p)
    main_src = diffmain.gen(spec, va, callees)
    main_p = os.path.join(gen, "diff_%s.cpp" % tag)
    with open(main_p, "w") as f:
        f.write(main_src)
    # support headers into the build tree: the committed runtime headers, plus the harness's diffparse.hpp
    for h in ("pes_state.hpp", "pes_cond.inl", "diffparse.hpp",
              "pes_stubs.hpp", "pes_dispatch.hpp"):
        hp = os.path.join(gen, h)
        if not os.path.exists(hp):
            src = os.path.join(HERE if h == "diffparse.hpp" else TRANSLATED_DIR, h)
            with open(src, "rb") as fi, \
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
    rc, out, err = sh([bin_p, corpus, args.image, inst_p], timeout=args.timeout)
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


def dead_blocks(image, va):
    """{block leader: reason} for blocks unreachable from the entry under
    any input: abort-trap/error blocks and padding. Excluded from the
    coverage denominator, each with its reason.

    - abort traps: a block whose first instruction is a CALL into a
      stubs.json abort entry (tamper/error paths the harness must never
      take; reaching one is a loud harness failure, not a case).
    - int3 padding: 0xCC bytes capstone decodes as INT3 (alignment
      between functions, never a real block).
    """
    import xlate
    try:
        lines = xlate.capstone_decode(image, int(va, 16))
    except Exception:
        return {}
    if not lines:
        return {}
    addrs = [a for a, _, _ in lines]
    aset = set(addrs)
    leaders = {addrs[0]}
    for i, (a, mn, ops) in enumerate(lines):
        m = mn.upper()
        tgt = None
        if (m in xlate.CCMAP or m == "JMP") and ops:
            try:
                tgt = int(ops[0].strip(), 16)
            except ValueError:
                tgt = None
        if tgt is not None and tgt in aset:
            leaders.add(tgt)
        if m in xlate.CCMAP or m in ("JMP", "CALL", "RET"):
            if i + 1 < len(addrs):
                leaders.add(addrs[i + 1])
    bounds = sorted(leaders)
    by_addr = {a: (mn, ops) for a, mn, ops in lines}
    abort_vas = {int(v, 16) if isinstance(v, str) else v
                 for v, e in STUB_TABLE.items() if e.get("do") == "abort"}
    out = {}
    for bi, b in enumerate(bounds):
        mn, ops = by_addr.get(b, (None, []))
        if mn == "INT3":
            out["0x%X" % b] = "int3 padding"
            continue
        if mn == "CALL" and ops:
            try:
                if int(ops[0].strip(), 16) in abort_vas:
                    out["0x%X" % b] = "abort-trap block"
            except ValueError:
                pass
    return out


def block_bounds(image, va):
    """Basic-block layout of one function: (addrs, bounds, live), or None when undecodable.
    addrs: the capstone decode. bounds: every leader (entry, Jcc/JMP targets, fallthroughs
    after branches). live: the leaders that are not dead (dead_blocks: abort traps, int3)."""
    import xlate
    try:
        lines = xlate.capstone_decode(image, int(va, 16))
    except Exception:
        return None
    if not lines:
        return None
    addrs = [a for a, _, _ in lines]
    aset = set(addrs)
    leaders = {addrs[0]}
    for i, (a, mn, ops) in enumerate(lines):
        m = mn.upper()
        tgt = None
        if (m in xlate.CCMAP or m == "JMP") and ops:
            try:
                tgt = int(ops[0].strip(), 16)
            except ValueError:
                tgt = None
        if tgt is not None and tgt in aset:
            leaders.add(tgt)
        if m in xlate.CCMAP or m in ("JMP", "CALL", "RET"):
            if i + 1 < len(addrs):
                leaders.add(addrs[i + 1])
    bounds = sorted(leaders)
    dead = {int(k, 16) for k in dead_blocks(image, va)}
    return addrs, bounds, [b for b in bounds if b not in dead]


def block_coverage(image, vas, executed):
    """{va: (reached, total)} basic-block coverage per function. A block counts when any
    executed address lands in it (block_bounds gives the blocks). Undecodable functions map
    to None (never counted). Dead blocks leave the denominator."""
    exe = set(int(a, 16) for a in executed)
    out = {}
    for va in vas:
        lay = block_bounds(image, va)
        if lay is None:
            out[va] = None
            continue
        addrs, bounds, live = lay
        reached = 0
        for b in live:
            bi = bounds.index(b)
            end = bounds[bi + 1] if bi + 1 < len(bounds) else None
            hit = False
            for a in addrs:
                if a < b or (end is not None and a >= end):
                    continue
                if a in exe:
                    hit = True
                    break
            if hit:
                reached += 1
        out[va] = (reached, len(live) or len(bounds))
    return out


def finish_one(va, args, pilot):
    res = run_one(va, args, pilot)
    # Block coverage (recorded for every translated function, never hidden):
    # the fraction of basic blocks the harness cases executed, per function
    # over the translated closure (top + callees the build links). A function
    # is diffed only with its coverage recorded AND at/above COVERAGE_FLOOR;
    # below-floor bit-exact stays visible as a failure instead of passing
    # silently. Dead blocks (abort traps, int3) leave the denominator.
    res["coverage"] = {}
    res["dead_blocks"] = {}
    if res["translated"]:
        try:
            addrs = json.load(open(os.path.join(
                args.out_dir, "gen", va[2:], "addrs_%s.json" % va[2:])))
        except (OSError, ValueError):
            addrs = []
        spec, _ = load_spec(args.spec_dir, va)
        extra, _ = runnable_extra(spec or {}, args.image)
        in_set = ["0x" + c.upper()
                  for c in ((spec or {}).get("precheck", {}) or {}).get("calls", [])]
        vas = [va] + [c for c in in_set + extra if c != va]
        cov = block_coverage(args.image, vas, addrs or [])
        res["coverage"] = {
            v: {"reached": r, "total": t,
                "fraction": round(r / t, 4) if t else 0.0}
            for v, (r, t) in cov.items() if isinstance(cov[v], tuple)}
        try:
            res["dead_blocks"] = dead_blocks(args.image, va)
        except Exception:
            res["dead_blocks"] = {}
    if res["translated"] and res["failure_class"] is None and \
            res["cases"]:
        spec, _ = load_spec(args.spec_dir, va)
        ok, note, cls = build_and_diff(va, spec, args, pilot, res["cases"])
        if ok and not res["coverage"]:
            # Never record bit-exact without coverage behind it.
            res["diffed"] = False
            res["failure_class"] = "harness cannot run"
            res["note"] = "diff passed but no coverage recorded"
        else:
            own = (res["coverage"] or {}).get(va, {})
            if ok and own and own.get("fraction", 0) < COVERAGE_FLOOR:
                # Bit-exact on a sliver: proven, but not the function.
                res["diffed"] = False
                res["failure_class"] = "harness cannot run"
                res["note"] = "bit-exact %d cases at own-block %.2f < floor %.2f" % (
                    res["cases"], own.get("fraction", 0), COVERAGE_FLOOR)
            else:
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
    ap.add_argument("--seed-state", default=None,
                    help="dir of seed.py artifacts (<VA>.seed.json): the corpus and the diff main apply them")
    ap.add_argument("--seed-bank", default=None,
                    help="dir of per-spec instance banks (bankstore.py: <VA>.bank.jsonl, see "
                         "bankstore.merge): run.py and the diff main run case n on instance n %% N")
    ap.add_argument("--perturb", action="store_true",
                    help="with --seed-bank: run.py perturbs the real values per (case, field)")
    ap.add_argument("--check", action="store_true",
                    help="re-run every VA results.json records as diffed; exit 1 if any no longer diffs bit-exact. "
                         "Never writes the repo results.json (the gate's pilot test).")
    ap.add_argument("--record", default=None,
                    help="committed results JSON. A full run writes it; --check re-runs its diffed VAs and never writes it.")
    ap.add_argument("--set", default=None,
                    help="a generator set file (specgen sets/*.txt): its subsystem and role columns are recorded on each result")
    args = ap.parse_args()
    if args.check and not args.record:
        ap.error("--check re-runs the VAs that --record lists as diffed")
    if args.check:
        args.only = [r["va"] for r in json.load(open(args.record))["results"] if r["diffed"]]
        if not args.only:
            print("driver --check: nothing recorded as diffed")
            return 0

    specs = sorted(f for f in os.listdir(args.spec_dir)
                   if f.endswith(".json"))
    # The pilot (call-closed universe) always spans ALL specs; --only
    # filters which functions get results, not the callee universe.
    # Otherwise callees outside --only would mistranslate as out-of-set
    # stubs and every --only diff with calls would spuriously mismatch.
    pilot = universe_of(args.spec_dir)
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

    attrib = read_attribution(args.set) if args.set else {}
    for r in results:
        if r["va"] in attrib:
            r["subsystem"], r["role"] = attrib[r["va"]]
    # results.json: build copy always; the committed repo copy only from a full run (a subset or a check run
    # must never overwrite the record of the whole set). Sort by VA.
    results.sort(key=lambda r: r["va"])
    payload = {"results": results,
               "note": ("%s results. diffed = bit-exact vs the unicorn harness. "
                        "'provisional' in a note means the spec reads writable image "
                        "globals at on-disk values. 'stubs' lists the callees that "
                        "stubs.json models." % os.path.basename(os.path.normpath(args.spec_dir)))}
    build_json = os.path.join(args.out_dir, "results.json")
    json.dump(payload, open(build_json, "w"), indent=1, sort_keys=True)
    if args.record and not args.only and not args.check:
        json.dump(payload, open(args.record, "w"), indent=1, sort_keys=True)
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
