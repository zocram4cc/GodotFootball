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

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
RUN_PY = os.path.join(REPO, "tools", "dev", "diffharness", "run.py")


def seed_bank_for(args, va):
    """The capture bank va's corpus draws from. --check gives each recorded VA its own corpus: a result whose record
    says corpus "bank" replays args.seed_bank; every other result replays the harness's random corpus (no bank)."""
    if not args.seed_bank:
        return None
    bank_vas = getattr(args, "bank_vas", None)
    return args.seed_bank if bank_vas is None or va.upper() in bank_vas else None
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

# Abort traps (stubs.json abort-trap): reaching one ends a case as a PES abort, never an output.
ABORT_VAS = frozenset(va for va, e in STUB_TABLE.items() if e["do"] == "abort")

# The image range the translated universe covers: plain code a build may translate (PES .trace section).
CODE_LO, CODE_HI = 0x140001000, 0x14252E800

# The one stub choice for every spec set (stubs.json, do in MODEL_DOS): a callee here is its model, in every set.
MODEL_MAP = {"0x%X" % va: e for va, e in stub_table.models(STUB_TABLE).items()}


def universe_of(spec_dir):
    """The call-closed universe a run over spec_dir translates into: the pilot's specs, the run's specs, and the run-real closure (stubs.json)."""
    pilot = {"0x" + f[:-5].upper() for f in os.listdir(DEF_SPEC_DIR) if f.endswith(".json")}
    pilot |= {"0x" + f[:-5].upper() for f in os.listdir(spec_dir) if f.endswith(".json")}
    # run-real closure callees (stubs.json) are translated like pilot members
    pilot |= {"0x%X" % va for va, entry in STUB_TABLE.items() if entry["do"] is None}
    clash = {"0x%X" % va for va in stub_table.models(STUB_TABLE)} & pilot
    if clash:
        raise SystemExit("universe: %s is a model (stubs.json) and also a spec: a callee is one or the other"
                         % sorted(clash))
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
            spec = json.load(open(p))
            stub_table.check_spec_refs(spec, STUB_TABLE, p)
            return spec, p
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


def recorded_elsewhere(record):
    """0x-prefixed VAs that a committed results*.json other than `record` already records, with any verdict."""
    own = os.path.abspath(record) if record else None
    out = set()
    for name in os.listdir(HERE):
        path = os.path.abspath(os.path.join(HERE, name))
        if name.startswith("results") and name.endswith(".json") and path != own:
            out |= {"0x%X" % int(r["va"], 16) for r in json.load(open(path))["results"]}
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


# Every build models MODEL_MAP (abort traps included: a mid-closure CALL to one must be loud, never OUT-OF-SET zero).
# No spec chooses a stub: stub_refs are validated in load_spec and never read here.
def _why_texts(why):
    if why is None:
        return []
    if isinstance(why, str):
        return [why]
    return list(why)


_LINK = re.compile(r";?\s*(?:call|callee) [0-9A-Fa-f]+:\s*")
_CALL_TOKEN = re.compile(r"(?:call|callee) ([0-9A-Fa-f]+):\s*")


def _cut_at_abort(chain):
    """A blocker chain up to its first abort-trap callee. Reaching a trap ends the case as a PES
    abort (never an output), so the links below the trap are not on any diffed path."""
    for m in _CALL_TOKEN.finditer(chain):
        if int(m.group(1), 16) in ABORT_VAS:
            return chain[:m.start()]
    return chain


def _chain_dispatchable(chain):
    """Every link of one blocker chain is dispatchable (see _blocker_dispatchable), below its trap."""
    parts = [p for p in _LINK.split(_cut_at_abort(chain)) if p.strip()]
    return all(_blocker_dispatchable(p) for p in parts)


def _why_dispatchable(why):
    """A stubs.json why-chain the build can run: every link an indirect
    call or computed jump (runtime dispatch / native switch), or a call
    chain that ends in an abort trap. Import slots, global stores,
    recursion and depth stay unmodellable."""
    texts = _why_texts(why)
    if not texts:
        return True
    for t in texts:
        chains = [c for c in
                  re.split(r";\s*(?=call [0-9A-Fa-f]+:)", t) if c.strip()]
        for c in chains or [t]:
            if not _chain_dispatchable(c):
                return False
    return True


def reached_roots(image, reached):
    """(roots, error) for a harness trace {indirect site: {target: count}}: the in-code targets that start a function,
    sorted. A target is no root when it lies in the same function as a `jmp reg` site (a switch case, a label of the
    function holding the site: 0x140A62047 inside 0x140A61F40; translating from it decodes nothing); a site inside a
    function is no root either. Any other in-code target that is not a function boundary is an error naming it.
    The one rule the diff builds (build_and_diff) and the snapshot builds (snapdiff) share."""
    import xlate
    pd = xlate.pdata_of(image)
    roots = set()
    for site, tgts in reached.items():
        sv = int(site, 16)
        reg_jmp = pd.indirect_kind(sv) == "jmp reg"
        for t in tgts:
            tv = int(t, 16)
            if not CODE_LO <= tv < CODE_HI:
                continue
            if reg_jmp and pd.same_body(sv, tv):
                continue
            if pd.is_boundary(tv):
                roots.add("0x%X" % tv)
            elif pd.containing(tv) != pd.containing(sv):
                return [], "closure walk: indirect target %s (from site %s) is not a function boundary" % (t, site)
    return sorted(roots), None


# ponytail: one budget over every seed's out-of-set walk; a walk over it blocks every unseen seed, leaves included. Upgrade: a budget per seed. Measured 28-10-26 (uncapped walks): 445E90 = 1871 functions, 44C880 = 1865 (depth 80); at 1100 both were refused by budget, and depth 25 blocked 33 functions of 44C880's walk.
EXTRA_CAP = 2000


def tail_rows(spec, image, universe):
    """Out-of-set rows for the tail jumps of the spec's function and of every universe member it reaches: a `jmp t`
    past a function's own body to a t the universe lacks. xlate would emit an abort there (never a silent return);
    the walk pulls t into the build instead, like an out-of-set callee. A CALL or tail jump into the universe is
    followed (that member's own tails matter), a model is left to its model."""
    import xlate
    rows, seen = [], set()
    todo = [spec["va"]] if spec.get("va") else []
    while todo:
        cur = todo.pop()
        if cur in seen:
            continue
        seen.add(cur)
        try:
            lines = xlate.capstone_decode(image, int(cur, 16))
        except Exception:
            continue
        addrs = {a for a, _, _ in lines}
        for _, mn, ops in lines:
            if mn not in ("CALL", "JMP") or not ops or "[" in ops[0]:
                continue
            try:
                t = int(ops[0].strip(), 16)
            except ValueError:
                continue
            tv = "0x%X" % t
            if t in addrs or tv in MODEL_MAP:
                continue
            if tv in universe:
                todo.append(tv)
            elif mn == "JMP":
                rows.append({"va": tv, "how": "call"})
    return rows


def runnable_extra(spec, image, depth_cap=80, universe=None):
    """Out-of-set direct callees the build can translate and call for
    real: stubs.json run-real kinds plus unmodellable entries whose whole
    why-chain is dispatchable. Transitively closed over direct CALLs
    decoded from the image (capstone, no bridge). Returns (extra, blocked)
    where blocked is {va: reason} for callees that stay unmodellable.
    With `universe` (the set the build translates into), the tail jumps of the spec's
    function and its universe callees past their bodies join the seeds (tail_rows)."""
    import xlate
    harness = spec.get("harness") or {}
    seeds = []
    unseeded = {}  # unmodellable out-of-set callees the build cannot run: named, not dropped
    rows = list(harness.get("out_of_set_callees") or [])
    if universe is not None:
        rows += tail_rows(spec, image, universe)
    for e in rows:
        if (e.get("how") or "call") != "call":
            continue
        va = "0x" + str(e.get("va", "")).upper().replace("0X", "")
        entry = STUB_TABLE.get(int(va, 16))
        if entry is None:
            # No stubs.json entry: the harness runs the callee's image bytes (run.py), so the build must too.
            if CODE_LO <= int(va, 16) < CODE_HI:
                seeds.append(va)
            continue
        if entry["do"] is None:
            seeds.append(va)  # run-real: image bytes execute either way
        elif entry["do"] == "unmodellable":
            if _why_dispatchable(entry.get("why")):
                seeds.append(va)
            else:
                unseeded[va] = "unmodellable: %s" % (entry.get("why") or "")[:200]
    extra = []
    blocked = dict(unseeded)  # a case that enters a blocked callee is excluded by run.py, never run silently
    seen = set()
    def direct_calls(va):
        try:
            lines = xlate.capstone_decode(image, int(va, 16))
        except Exception:
            return None
        out = []
        addrs = {a for a, _, _ in lines}
        for _, mn, ops in lines:
            # A CALL, or a JMP past the function's own body (a tail call: xlate emits `pes_fn_X(s); return;` when X is in
            # the universe and a silent `return` when it is not), to a plain address.
            if mn in ("CALL", "JMP") and ops and "[" not in ops[0]:
                try:
                    t = int(ops[0].strip(), 16)
                except ValueError:
                    continue
                if mn == "CALL" or t not in addrs:
                    out.append("0x%X" % t)
        return out
    stack = [(v, 0, [v]) for v in seeds]
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
                if CODE_LO <= ci < CODE_HI:
                    stack.append((c, depth + 1, chain + [c]))
                else:
                    blocked[c] = "call outside code range"
                continue
            if e["do"] in stub_table.MODEL_DOS:
                # Modelled by the stub (MODEL_MAP), never translated: the
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
    """Why the function cannot be run at all: its harness class, or an unmodellable callee."""
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
                if not _chain_dispatchable(c):
                    live.append(c.strip()[:440])
        if live:
            return "; ".join(live)[:440]
        return None


def summary_of(stdout):
    """run.py's stdout summary line: valid, requested, nonreturn, failed, capped, icount_max."""
    m = re.search(r"^summary valid=(\d+) requested=(\d+) nonreturn=(\d+) failed=(\d+) capped=(\d+) icount_max=(\d+)$",
                  stdout or "", re.M)
    if m is None:
        raise SystemExit("driver: run.py printed no summary line")
    keys = ("valid", "requested", "nonreturn", "failed", "capped", "icount_max")
    return dict(zip(keys, (int(v) for v in m.groups())))


def skip_note(res):
    """What the corpus is made of, in the note: valid cases against the requested count, and what was skipped."""
    parts = ["%d valid of %d requested" % (res["cases"], res["requested"])]
    if res["nonreturn"]:
        parts.append("%d non-returning skipped" % res["nonreturn"])
    if res["failed"]:
        parts.append("%d failed skipped" % res["failed"])
    if res["capped"]:
        parts.append("capped after %d non-returning: %d case(s) not generated" % (res["nonreturn"], res["capped"]))
    return "; ".join(parts)


def run_one(va, args, pilot):
    """Full pipeline for one function. Returns a result dict."""
    import diffmain
    res = {"va": va, "translated": False, "diffed": False, "cases": 0,
           "failure_class": None, "note": "", "stubs": [], "nonreturn": 0, "failed": 0, "capped": 0,
           "requested": 0, "icount_max": 0, "aborted": 0}
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
    # 1. translate: the stubs.json models (MODEL_MAP, one table for every spec set).
    # Runnable out-of-set callees join this build's universe so the CALL
    # emitter links them instead of zeroing rax (OUT-OF-SET).
    res["stubs"] = sorted(MODEL_MAP)
    build_pilot = set(pilot) | set(runnable_extra(spec, args.image, universe=pilot)[0])
    try:
        asm_text = asm_for(args.asm_dir, va)
        src, _ = translate_one(va, asm_text, args.image, build_pilot,
                               stubs=MODEL_MAP)
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
    aborts = os.path.join(gen, "aborts_%s.jsonl" % tag)
    # Traced cases: run.py takes its case count from the bank (one case per instance).
    count = [] if args.traced_cases else (["--count", str(args.count)] if args.count else [])
    bank = seed_bank_for(args, va)
    seed_args = (["--seed-state", args.seed_state] if args.seed_state else []) \
        + (["--seed-bank", bank] if bank else []) \
        + (["--perturb"] if args.perturb else []) \
        + (["--traced-cases"] if args.traced_cases else [])
    rc, out, err = sh([diff_python(), RUN_PY, "--exe", args.image,
                     "--spec", spec_path, "--out", corpus,
                     "--trace-calls", trace,
                     "--trace-addrs", addrs, "--aborts", aborts] + count + seed_args,
                    timeout=args.timeout)
    summ = summary_of(out) if rc == 0 else None
    if summ is not None:
        res.update(nonreturn=summ["nonreturn"], failed=summ["failed"], capped=summ["capped"],
                   requested=summ["requested"], icount_max=summ["icount_max"])
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
    if os.path.exists(aborts):
        with open(aborts) as f:
            res["aborted"] = sum(1 for _ in f)
    if nlines == 0:
        # Empty corpus: the function never reaches its end under the
        # harness (e.g. tail-jump live chase). Nothing to diff.
        res["failure_class"] = "harness cannot run"
        res["cases"] = 0
        res["note"] = "corpus empty (0 cases)"
        return res
    res["cases"] = nlines
    if summ is not None and 2 * nlines < summ["requested"]:
        # Half the requested cases must be valid for a verdict: a skipped case is no output, so a function whose cases
        # mostly do not return has no oracle here.
        res["failure_class"] = "harness cannot run"
        return res
    return res


def build_and_diff(va, spec, args, pilot, nlines):
    """Compile fn + in-set callees + diff main, run, compare. Returns
    (ok, note, failure_class). Callee list comes from the spec precheck
    (committed), so the build needs no analysis scratch."""
    import diffmain
    tag = va[2:]
    gen = os.path.join(args.out_dir, "gen", tag)
    # Runnable out-of-set callees (dispatchable switches/indirects): the
    # build translates them and calls them for real, like the harness runs
    # their image bytes. They join the in-set universe for this build only,
    # so the CALL emitter links them instead of zeroing rax (OUT-OF-SET).
    extra, extra_blocked = runnable_extra(spec, args.image, universe=pilot)
    build_pilot = set(pilot) | set(extra)
    # The build declares only callees it links: pilot-universe members that are not stubbed.
    callees = ["0x" + c.upper() for c in (spec.get("precheck", {}) or {}).get("calls", [])]
    callees = [c for c in callees if c in build_pilot and c not in MODEL_MAP]
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
    roots, bad = reached_roots(args.image, reached or {})
    if bad:
        return False, bad, "translator bug"
    for t in roots:
        if t not in seen:
            seen.add(t)
            need.append(t)
    for cvi, cv in enumerate(need):
        p = os.path.join(gen, "pes_fn_%s.cpp" % cv[2:])
        if not os.path.exists(p):
            asp, _ = load_spec(args.spec_dir, cv)
            try:
                csrc, _ = translate_one(cv, asm_for(args.asm_dir, cv),
                                        args.image, build_pilot, stubs=MODEL_MAP)
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
                                    args.image, build_pilot, stubs=MODEL_MAP)
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
    bank = seed_bank_for(args, va)
    if bank:
        # Streamed: write_instances reads one bank instance at a time.
        instances = bankstore.Bank(os.path.join(bank, tag + ".bank.jsonl"))
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
    # ponytail: one g++ over every TU, serial; the budget grows with the TU count (a tail-jump closure of 350-600
    # functions took 300 s and read as "compile: " with no error line). Upgrade: parallel per-TU objects.
    rc, _, err = sh([args.cxx, "-std=c++20", "-O1", "-I", gen, "-o", bin_p,
                     main_p] + objs, timeout=300 + 20 * len(objs))
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
    # Block coverage (recorded for every translated function, never hidden): the fraction of basic blocks the harness
    # cases executed, per function over the translated closure (top + callees the build links). It is a confidence
    # measure, reported per function and subsystem, never a gate (owner ruling 26-10-26: proven = every case I/O
    # bit-exact). Dead blocks (abort traps, int3) leave the denominator.
    res["coverage"] = {}
    res["dead_blocks"] = {}
    if res["translated"]:
        try:
            addrs = json.load(open(os.path.join(
                args.out_dir, "gen", va[2:], "addrs_%s.json" % va[2:])))
        except (OSError, ValueError):
            addrs = []
        spec, _ = load_spec(args.spec_dir, va)
        extra, _ = runnable_extra(spec or {}, args.image, universe=pilot)
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
        res["diffed"] = ok
        res["failure_class"] = cls
        res["note"] = note
        own = (res["coverage"] or {}).get(va, {})
        if ok:
            res["note"] += ("; own-block coverage %.2f" % own["fraction"]) if own else "; coverage not recorded"
    if res["requested"]:
        res["note"] = ("%s; " % res["note"] if res["note"] else "") + skip_note(res)
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
    # Wall-clock safety net per subprocess only; results are decided by run.py's instruction budget, so this must stay
    # far above a loaded machine's runtime (600 s killed run.py at load average 46).
    ap.add_argument("--timeout", type=int, default=3600)
    ap.add_argument("--jobs", type=int, default=2)
    ap.add_argument("--seed-state", default=None,
                    help="dir of seed.py artifacts (<VA>.seed.json): the corpus and the diff main apply them")
    ap.add_argument("--seed-bank", default=None,
                    help="dir of per-spec instance banks (bankstore.py: <VA>.bank.jsonl, see "
                         "bankstore.merge): run.py and the diff main run case n on instance n %% N")
    ap.add_argument("--perturb", action="store_true",
                    help="with --seed-bank: run.py perturbs the real values per (case, field)")
    ap.add_argument("--traced-cases", action="store_true",
                    help="with --seed-bank: every bank instance is one case (every traced call, "
                         "deduplicated), replayed with its real values; no perturbation, no random "
                         "or guided cases")
    ap.add_argument("--check", action="store_true",
                    help="re-run every VA results.json records as diffed; exit 1 if any no longer diffs bit-exact. "
                         "Never writes the repo results.json (the gate's pilot test).")
    ap.add_argument("--record", default=None,
                    help="committed results JSON. A full run writes it; --check re-runs its diffed VAs and never writes it.")
    ap.add_argument("--set", default=None,
                    help="a generator set file (specgen sets/*.txt): its subsystem and role columns are recorded on each result")
    ap.add_argument("--exclude-recorded", action="store_true",
                    help="attempt and record only the specs that no other committed results*.json records. The rest "
                         "stay in the spec dir: they are the callee universe, and their own sets own their records.")
    ap.add_argument("--select", default=None,
                    help="with --check: a select_recorded.py JSON ({results file: [VA...]}); re-run only the recorded VAs it "
                         "lists for --record's file (the fast gate). An empty selection passes: nothing it can break changed.")
    args = ap.parse_args()
    if args.traced_cases and (not args.seed_bank or args.perturb):
        ap.error("--traced-cases replays a --seed-bank with real values: give --seed-bank, not --perturb")
    if args.check and not args.record:
        ap.error("--check re-runs the VAs that --record lists as diffed")
    if args.select and not args.check:
        ap.error("--select narrows a --check run")
    if args.check:
        recs = json.load(open(args.record))["results"]
        # A record with proof snapdiff was proven from real calls (snapdiff.py, not this harness): not re-run here.
        args.only = [r["va"] for r in recs if r["diffed"] and r.get("proof") != "snapdiff"]
        # Corpus provenance (results JSON): a result recorded with corpus "bank" replays its capture bank; the others
        # replay the harness's random corpus. A bank result without --seed-bank is refused, never run without its bank.
        args.bank_vas = {r["va"].upper() for r in recs if r["diffed"] and r.get("corpus") == "bank"}
        if args.bank_vas and not args.seed_bank:
            ap.error("--check: %s record corpus 'bank': give --seed-bank" % " ".join(sorted(args.bank_vas)))
        if args.select:
            picked = {v.upper() for v in json.load(open(args.select)).get(os.path.basename(args.record), [])}
            args.only = [v for v in args.only if v.upper() in picked]
            if not args.only:
                print("driver --check: no recorded function of %s selected; nothing to re-check" % args.record)
                return 0
        if not args.only:
            print("driver --check: nothing recorded as diffed; an empty check is not a pass")
            return 1

    specs = sorted(f for f in os.listdir(args.spec_dir)
                   if f.endswith(".json"))
    if args.exclude_recorded:
        elsewhere = recorded_elsewhere(args.record)
        specs = [f for f in specs if "0x" + f[:-5].upper() not in elsewhere]
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
        print("%s translated=%s diffed=%s cases=%s aborted=%s class=%s %s" % (
            r["va"], r["translated"], r["diffed"], r["cases"], r.get("aborted", 0),
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
        absent = [v for v in args.only if v.upper() not in {r["va"].upper() for r in results}]
        if absent:
            print("driver --check: %d recorded-diffed VAs have no spec in --spec-dir %s: %s"
                  % (len(absent), args.spec_dir, " ".join(absent)))
            return 1
    if args.check:
        lost = [r["va"] for r in results if not r["diffed"]]
        if lost:
            print("driver --check: %d recorded-diffed functions no longer diff: %s" % (len(lost), " ".join(lost)))
            return 1
        print("driver --check: all %d recorded-diffed functions still diff bit-exact" % att)
    return 0


if __name__ == "__main__":
    sys.exit(main())
