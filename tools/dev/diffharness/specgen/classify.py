"""Out-of-set callee classifier for the impure specs (capstone; image bytes read through analyze.py).

body(va): control-flow walk from the entry (fall-through, conditional and near unconditional branches) until
every path ends in ret or a tail jump. A branch target more than WINDOW bytes past the entry is not followed:
a tail call into a function within WINDOW bytes after the entry is over-included (ceiling, not observed).

Classes (per callee, recursive over direct calls and tail jumps):
  pilot        in the pilot set (translated and proven; already in the harness and the C++ build)
  run-real     no call through an unmodelled target, no global store: the harness executes the image bytes
  stub         a named model applies (import thunk / engine no-op); nothing else blocks it
  unmodellable a reason blocks it (indirect call, unnamed import slot, global store, recursion, depth)
Computed jumps (jmp reg / jmp [table]) are recorded in SWITCH_NOTES, not blockers: intra-function switch tables;
the harness faults if a target is unmapped.
"""
import collections

from capstone import Cs, CS_ARCH_X86, CS_MODE_64
from capstone.x86 import X86_OP_MEM, X86_OP_IMM

from . import analyze as AN

WINDOW = 0x10000
LOWER = 0x2000
PILOT_SIZE = {}  # VA -> size of the pilot set (see configure)
md = Cs(CS_ARCH_X86, CS_MODE_64)
md.detail = True


def configure(pilot_size):
    """Pilot set: callees classed 'pilot' (translated and proven; already in the harness and the C++ build)."""
    PILOT_SIZE.clear()
    PILOT_SIZE.update(pilot_size)


def rd(va, n):
    return AN.rd(va, n)


def body(va):
    """instructions reachable from va, sorted by address (see module doc for the window rule)."""
    seen = {}
    work = [va]
    while work:
        a = work.pop()
        while a not in seen and len(seen) < 6000:
            ins = next(md.disasm(rd(a, 15), a, count=1), None)
            if ins is None:
                break
            seen[a] = ins
            mn = ins.mnemonic
            if mn == "ret":
                break
            op0 = ins.operands[0] if ins.operands else None
            if mn == "jmp":
                if op0 is not None and op0.type == X86_OP_IMM and va - LOWER <= op0.imm < va + WINDOW and op0.imm not in seen:
                    work.append(op0.imm)
                break
            if mn.startswith("j") and op0 is not None and op0.type == X86_OP_IMM:
                t = op0.imm
                if va - LOWER <= t < va + WINDOW and t not in seen:
                    work.append(t)
            a = ins.address + ins.size
    return [seen[k] for k in sorted(seen)]


def rip_slot(i, op):
    return i.address + i.size + op.mem.disp


def is_rip(i, op):
    return op.type == X86_OP_MEM and op.mem.base != 0 and i.reg_name(op.mem.base) == "rip"


def op_global_write(i):
    if not i.operands:
        return False
    op = i.operands[0]
    return op.type == X86_OP_MEM and is_rip(i, op) and i.mnemonic not in ("lea", "nop", "cmp", "test", "call", "jmp")


# Named models. Each cites where the name comes from. Anything not listed is classified from bytes.
STUB = {
    0x1415A1780: ("stub", "return-input", {"op": "sqrtf", "arg": "xmm0", "ret": "xmm0", "enc": "f32",
                  "exact": "IEEE sqrt is correctly rounded: C++ std::sqrt(float) gives the same bits"},
                  ["thunk jmp [0x1425301F0] = sqrt: docs/re/pes-re-carrier-helpers.md table (FUN_1415A1780 = sqrt)",
                   "docs/re/pes-re-lane-core.md O3 (sqrt import 0x1415A1780, jump via _DAT_1425301F0)"]),
    0x14159F3E0: ("stub", "engine-noop", {"op": "__security_check_cookie", "effect": "none on the success path (validates the stack cookie, returns)"},
                  ["Ghidra name __security_check_cookie at 0x14159F3E0 (get_function_by_address)",
                   "docs/re table row 0x14159F3E0 | __security_check_cookie | engine"]),
    0x1415A17C8: ("unmodellable", None, None,
                  ["thunk to the logf/powf import (docs: 'logf/powf are the imports 0x1415a17c8/0x1415a17ce'); result bits come from the C runtime, outside the image"]),
    0x1415A17CE: ("unmodellable", None, None,
                  ["thunk to the logf/powf import (docs: 'logf/powf are the imports 0x1415a17c8/0x1415a17ce'); result bits come from the C runtime, outside the image"]),
}

MEMO = {}
RUNREAL_SIZE = {}
SWITCH_NOTES = collections.defaultdict(list)   # function VA -> computed-jump addresses (not gating)


def classify(va, stack=()):
    """-> dict(cls, reasons, stubs{va: (kind, model, why)})."""
    if va in PILOT_SIZE:
        return {"cls": "pilot", "reasons": [], "stubs": {}}
    if va in MEMO:
        return MEMO[va]
    if va in stack:
        return {"cls": "unmodellable", "reasons": ["recursion through %X" % va], "stubs": {}}
    if len(stack) > 10:
        return {"cls": "unmodellable", "reasons": ["call depth > 10"], "stubs": {}}
    if va in STUB:
        kind, _, model, why = STUB[va]
        if kind == "stub":
            r = {"cls": "stub", "reasons": [], "stubs": {va: ("stub", model, why)}}
        else:
            r = {"cls": "unmodellable", "reasons": why, "stubs": {}}
        MEMO[va] = r
        return r
    ins = body(va)
    if not ins:
        r = {"cls": "unmodellable", "reasons": ["undecodable entry"], "stubs": {}}
        MEMO[va] = r
        return r
    seen = {i.address for i in ins}
    reasons, subs = [], []
    leaf = True
    for i in ins:
        mn = i.mnemonic
        if mn == "call" or mn == "jmp" or (mn.startswith("j") and i.operands and i.operands[0].type == X86_OP_IMM):
            op = i.operands[0]
            if op.type == X86_OP_IMM:
                t = op.imm
                if t in seen and mn != "call":
                    continue   # intra-function branch
                if mn == "call":
                    leaf = False
                subs.append(("%X" % t, classify(t, stack + (va,))))
            elif mn == "jmp" or mn.startswith("j"):
                if is_rip(i, op):
                    reasons.append("import slot %X has no name in the image (%s)" % (rip_slot(i, op), i.op_str[:28]))
                else:
                    SWITCH_NOTES[va].append("%X" % i.address)   # computed jump: intra-function switch table
            else:
                leaf = False
                reasons.append("indirect call %s at %X" % (i.op_str[:28], i.address))
        elif op_global_write(i):
            reasons.append("global store %s at %X" % (i.op_str[:30], i.address))
    stubs = {}
    for t, sub in subs:
        if sub["cls"] == "unmodellable":
            reasons.append("callee %s: %s" % (t, "; ".join(sub["reasons"][:2])))
        stubs.update(sub["stubs"])
    if SWITCH_NOTES.get(va):
        reasons.append("computed jump at %s: switch case bodies not enumerated (closure incomplete)" % SWITCH_NOTES[va][0])
    if reasons:
        r = {"cls": "unmodellable", "reasons": reasons[:4], "stubs": stubs}
    elif stubs:
        r = {"cls": "stub", "reasons": [], "stubs": stubs}
    else:
        r = {"cls": "run-real", "reasons": [], "stubs": {}}
        RUNREAL_SIZE[va] = len(ins)
    if leaf and not reasons:
        r["reasons"] = ["leaf: no call, no indirect site, no global store (%d instructions)" % len(ins)]
    MEMO[va] = r
    return r


def direct_calls(va):
    return [i.operands[0].imm for i in body(va) if i.mnemonic == "call" and i.operands[0].type == X86_OP_IMM]


def reach_stubs(va):
    """({stub VA: (kind, model, why)}, functions walked): every named model reached from va over direct CALL and
    JMP targets. classify() stops at a pilot callee, but a pilot member of a hook closure is built into the same
    binary and calls the same stubs, so a root's stub_refs must cover it too. Named models end the walk (their
    bodies are not run); every other callee, pilot or not, is walked."""
    stubs, seen, work = {}, {va}, [va]
    while work:
        f = work.pop()
        own = {i.address for i in body(f)}
        for i in body(f):
            if i.mnemonic not in ("call", "jmp") or not i.operands or i.operands[0].type != X86_OP_IMM:
                continue
            t = i.operands[0].imm
            if t in own or t in seen:
                continue
            seen.add(t)
            if t in STUB:
                if STUB[t][0] == "stub":
                    stubs[t] = STUB[t]
                continue
            work.append(t)
    return stubs, len(seen)
