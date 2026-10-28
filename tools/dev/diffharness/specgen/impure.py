"""Impure-mode spec fields: the out-of-set callees, their stub references and the harness class.

The callee set is the ledger's walk (the universe rows' callees), as the impure sample was built. The body supplies
the indirect call sites and the computed-jump notes (classify.py).
"""
from capstone.x86 import X86_OP_IMM

from . import classify as C

NOTE = ("impure-sample spec (ImpureMap). Generated from the image by the pilot analyzer/generator; "
        "out-of-set callees, their stub models and blockers are in harness and stubs.json.")
HARNESS_FIELDS = ("harness_class", "blockers", "out_of_set_callees", "indirect_sites", "switch_jumps")


def indirect_sites(va):
    """Indirect call sites of the body, as 'call <operand> at <VA>'."""
    return ["call %s at %X" % (i.op_str[:30], i.address)
            for i in C.body(va) if i.mnemonic == "call" and i.operands[0].type != X86_OP_IMM]


def harness_fields(va, callees):
    """Classify va's out-of-set callees (VAs) and derive its harness class."""
    C.classify(va)  # fills SWITCH_NOTES for this body
    indirect = indirect_sites(va)
    rows, stub_refs, blockers = [], set(), list(indirect)
    for t in sorted(set(callees)):
        if t in C.PILOT_SIZE:
            continue
        c = C.classify(t)
        stub_refs |= set(c["stubs"])
        if c["cls"] == "unmodellable":
            blockers.append("call %X: %s" % (t, "; ".join(c["reasons"][:2])))
        rows.append({"va": "%X" % t, "how": "call", "cls": c["cls"], "stubs": sorted("%X" % s for s in c["stubs"])})
    if blockers:
        harness_class = "harness cannot run"
    elif stub_refs:
        harness_class = "runnable with stubs"
    else:
        harness_class = "runnable (closure real)"
    return {"harness_class": harness_class, "blockers": blockers, "out_of_set_callees": rows,
            "indirect_sites": indirect, "switch_jumps": list(C.SWITCH_NOTES.get(va, []))}


def mark(spec, fields):
    """Impure identity and harness fields on a spec from spec.build()."""
    spec["name"] = "impure_" + spec["va"][2:]
    spec["note"] = NOTE
    spec["status"] = "impure-sample"
    spec["subsystem"] = "impure-sample"
    spec["harness"] = {k: fields[k] for k in HARNESS_FIELDS}
