"""Stub table: how each out-of-set PES callee is modelled, shared by run.py and xlate.py.

stubs.json (beside this file) lists every callee the impure sample reaches, with the
evidence for its model. Each entry's kind and model.op select one behaviour name
("do"), which the harness (diffharness/run.py) and the translator (xlate.py) both
implement:

  sqrt_xmm0  return-input: xmm0 = sqrtf(xmm0), IEEE-754 (x86 sqrtss NaN rules)
  noop       engine-noop: no effect on the success path; the call is dropped
  None       run-real kinds: the callee executes its image bytes, no stub
  unmodellable  blocked (see why): the function is not run

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
    "libm": {"logf": "logf_xmm0", "powf": "powf_xmm0"},
    "run-real": {None: None},
    "run-real-closure": {None: None},
    "unmodellable": {None: "unmodellable"},
}
KNOWN_DO = {"sqrt_xmm0", "noop", "abort", "logf_xmm0", "powf_xmm0",
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
