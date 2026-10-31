"""Stub table: how each out-of-set PES callee is modelled, shared by run.py and xlate.py.

stubs.json (beside this file) lists every callee the impure sample reaches, with the
evidence for its model. Each entry's kind and model.op select one behaviour name
("do"), which the harness (diffharness/run.py) and the translator (xlate.py) both
implement:

  sqrt_xmm0  return-input: xmm0 = sqrtf(xmm0), IEEE-754 (x86 sqrtss NaN rules)
  logf_xmm0  libm: xmm0 = logf(xmm0), the host C library (pes_model_logf; see stubs.json caveat for the ucrtbase gap)
  powf_xmm0  libm: xmm0 = powf(xmm0, xmm1), the host C library (pes_model_powf; same caveat)
  noop       engine-noop: no effect on the success path; the call is dropped
  None       run-real kinds: the callee executes its image bytes, no stub
  unmodellable  blocked (see why): the function is not run
  abort      tamper trap: reaching it aborts on both sides

One choice for every spec set (ruling): a callee in MODEL_DOS is always its model, in every spec set; any other
callee is always its translated body (run-real) or blocked. No spec decides it. A spec's legacy `stub_refs` may
only name modelled callees (check_spec_refs).

A do value no implementation knows is a loud error, never a silent default.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT = os.path.join(HERE, "stubs.json")

# IAT slots whose target aborts when reached (tamper traps, not real
# imports). A call through one is loud on both sides (harness error,
# C++ abort()); a corpus behind one proves the path unreached.
# Names from tools/dev/ledger/indirect.py KNOWN_SLOTS.
ABORT_SLOTS = {0x14252FDE0: "abort"}

# kind -> {model.op: do}; None is the op-independent default for the kind.
DO_BY_KIND = {
    "return-input": {"sqrtf": "sqrt_xmm0"},
    "engine-noop": {None: "noop"},
    "abort-trap": {"abort": "abort"},
    "libm": {"logf": "logf_xmm0", "powf": "powf_xmm0", "_isnan": "isnan_xmm0", "floorf": "floorf_xmm0",
             "sinf": "sinf_xmm0", "cosf": "cosf_xmm0", "tanf": "tanf_xmm0", "asinf": "asinf_xmm0",
             "atanf": "atanf_xmm0", "acosf": "acosf_xmm0", "atan2f": "atan2f_xmm0"},
    "run-real": {None: None},
    "run-real-closure": {None: None},
    "unmodellable": {None: "unmodellable"},
}
KNOWN_DO = {"sqrt_xmm0", "noop", "abort", "logf_xmm0", "powf_xmm0", "isnan_xmm0", "floorf_xmm0", "sinf_xmm0",
            "cosf_xmm0", "tanf_xmm0", "asinf_xmm0", "atanf_xmm0", "acosf_xmm0", "atan2f_xmm0",
            "unmodellable", None}


def load(path=DEFAULT):
    """Return {callee VA (int): {"va", "do", "kind", "why"}} for every entry."""
    with open(path, encoding="utf-8") as f:
        data = json.load(f)
    table = {}
    for e in data["stubs"]:
        va = int(e["va"], 16)
        if va in table:
            raise SystemExit("stubs.json: duplicate entry %s" % e["va"])
        kinds = DO_BY_KIND.get(e["kind"])
        if kinds is None:
            raise SystemExit("stubs.json: unknown kind %r at %s" % (e["kind"], e["va"]))
        op = (e.get("model") or {}).get("op")
        do = kinds.get(op, kinds.get(None, "missing"))
        if do == "missing" or do not in KNOWN_DO:
            raise SystemExit("stubs.json: no model for %s (kind %s, op %s)"
                             % (e["va"], e["kind"], op))
        table[va] = {"va": "0x%X" % va, "do": do, "kind": e["kind"],
                     "why": "; ".join(e.get("why") or [])}
    return table

# The do values the translator and the harness both emit: the one set every spec set models the same way.
MODEL_DOS = frozenset({"sqrt_xmm0", "logf_xmm0", "powf_xmm0", "isnan_xmm0", "floorf_xmm0", "sinf_xmm0", "cosf_xmm0",
                       "tanf_xmm0", "asinf_xmm0", "atanf_xmm0", "acosf_xmm0", "atan2f_xmm0", "noop", "abort"})


def models(table):
    """{callee VA (int): entry} for every callee the table models (do in MODEL_DOS)."""
    return {va: e for va, e in table.items() if e["do"] in MODEL_DOS}


def check_spec_refs(spec, table, where):
    """A spec's legacy stub_refs may only name callees the table models; anything else disagrees."""
    for ref in spec.get("stub_refs") or []:
        e = table.get(int(ref, 16))
        if e is None or e["do"] not in MODEL_DOS:
            got = "no entry" if e is None else "do=%s" % e["do"]
            raise SystemExit("%s: stub_refs %s disagrees with stubs.json (%s)" % (where, ref, got))


# The ops a native service may name: each is a GF-native C++ implementation pes_native_<op> (pes_native.cpp).
KNOWN_NATIVE_OPS = frozenset({"heap_alloc", "heap_free", "heap_owns", "heap_trim"})


def natives(path=DEFAULT):
    """{VA (int): op} for the stubs.json "native_services": PES VAs the production dispatch table sends to a GF-native
    implementation. They are not in load()'s table: the harness never sees them and runs PES's bytes there."""
    with open(path, encoding="utf-8") as f:
        data = json.load(f)
    out = {}
    for e in data.get("native_services", []):
        va = int(e["va"], 16)
        if va in out or va in load(path):
            raise SystemExit("stubs.json: native service %s is duplicated or also a stub" % e["va"])
        if e["op"] not in KNOWN_NATIVE_OPS:
            raise SystemExit("stubs.json: native service %s: unknown op %r" % (e["va"], e["op"]))
        out[va] = e["op"]
    return out
