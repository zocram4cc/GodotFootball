#!/usr/bin/env python3
"""Argument-root typing for the impure specs, from the callers' bytes (read-only).

Each direct call or jump (E8/E9) into an impure spec is one site. The function that holds
a site comes from the image's own x64 unwind table (.pdata: exact begin and end of each
function; every impure spec is an exact function begin). Inside that function the
instructions from its entry to the site are walked in order, tracking what each register
holds:

  ("arg", r)       the value the function received in argument register r
  ("rec", C, why)  a pointer to a record of class C: an accessor's result (ACC), or a load
                   of a G table entry (TABLE_DISP; docs/re/pes-re-player-record.md 1.1)
  ("G",)           the G object, loaded from the global at G_VA
  (absent)         unknown: anything else, including loads from records of unknown class

A root of a spec is typed C when every site that resolves it gives C and no site gives
another class. An ("arg", r) origin is resolved through the function's own call sites
(DEPTH levels up), or through the function's typed root when that function is an impure
spec. The walk is linear, not path-sensitive: the result is INF, and the evidence lists
every site that gave it.

Writes impure_roots.json (bindable classes only: seed.py binds team, anime, man, slot) and
impure_roots_evidence.json (every class found, the conflicts, and why each root stays
untyped).
"""
import argparse
import bisect
import collections
import glob
import json
import mmap
import os
import re

import capstone

import pdata

BASE = 0x140000000
G_VA = 0x1436F3DC8
HERE = os.path.dirname(os.path.abspath(__file__))
SPEC = os.path.join(HERE, "..", "diffharness", "specs", "impure")
# accessor VA -> class (docs/re/pes-re-player-record.md 1.1)
ACC = {0x140A48A30: "team", 0x140A489D0: "side", 0x140A48910: "anime",
       0x140A48A10: "man", 0x140A489F0: "slot"}
# G tables: entry k of a class is the pointer at G + base + k * STRIDE
STRIDE = 0x28
TABLES = {"team": (0x1928, 2), "side": (0x1978, 2), "anime": (0x19C8, 26),
          "man": (0x1DD8, 26), "slot": (0x25F8, 22)}
TABLE_DISP = {base + k * STRIDE: cls for cls, (base, n) in TABLES.items() for k in range(n)}
# RealCases seeds (INF): A0 of these impure specs
SEED = {"1404F47A0": "team", "1404F6C30": "team", "1404F8C70": "team",
        "1404FA720": "team", "14064DE60": "man", "140978E70": "man"}
BINDABLE = {"team", "anime", "man", "slot"}   # seed.py instance_roots() binds these
ARG = ("rcx", "rdx", "r8", "r9")
CALLER_SAVED = ("rax", "rcx", "rdx", "r8", "r9", "r10", "r11")
STRING_OPS = {"movsb", "movsw", "movsd", "movsq", "stosb", "stosw", "stosd", "stosq",
              "lodsb", "lodsw", "lodsd", "lodsq"}
ONE_OP_IMPLICIT = {"mul", "div", "idiv", "cqo", "cdq", "cdqe", "cwde"}
WRITE1 = {"mov", "movzx", "movsx", "movsxd", "lea", "add", "sub", "and", "or", "xor",
          "neg", "not", "inc", "dec", "shl", "shr", "sar", "sal", "rol", "ror", "imul",
          "adc", "sbb", "pop", "bswap", "bsf", "bsr", "popcnt", "lzcnt", "tzcnt", "xadd"}
DEPTH = 4
GPR = {}
for _base, _names in {"rax": ("eax", "ax", "al", "ah"), "rbx": ("ebx", "bx", "bl", "bh"),
                      "rcx": ("ecx", "cx", "cl", "ch"), "rdx": ("edx", "dx", "dl", "dh"),
                      "rsi": ("esi", "si", "sil"), "rdi": ("edi", "di", "dil"),
                      "rbp": ("ebp", "bp", "bpl"), "rsp": ("esp", "sp", "spl")}.items():
    GPR[_base] = _base
    for _n in _names:
        GPR[_n] = _base
for _i in range(8, 16):
    GPR["r%d" % _i] = "r%d" % _i
    for _s in "dwb":
        GPR["r%d%s" % (_i, _s)] = "r%d" % _i
FULL = {n for n, b in GPR.items() if n == b}   # the 64-bit register names
MEM = re.compile(r"\[(\w+)(?: ([+-]) (0x[0-9a-fA-F]+|\d+))?\]")
RIP = re.compile(r"\[rip ([+-]) (0x[0-9a-fA-F]+|\d+)\]")


def mem_of(src):
    """(base register, displacement) of a plain [base +- disp] operand, else None."""
    if "fs:" in src or "gs:" in src:
        return None
    m = MEM.search(src)
    if not m or m.group(1) not in GPR:
        return None
    disp = int(m.group(3), 0) if m.group(3) else 0
    return GPR[m.group(1)], (-disp if m.group(2) == "-" else disp)


STACK = ("rsp", "rbp")
NO_WRITE = {"cmp", "test", "bt", "ucomiss", "ucomisd", "comiss", "comisd"}


def clear_slots(org):
    """Forget every spilled stack slot: rsp moved, or a stack location was written in a way
    the origins cannot follow."""
    for k in [k for k in org if isinstance(k, tuple)]:
        del org[k]


def load_origin(ins, src, org):
    """Origin of the value loaded from memory operand src."""
    m = RIP.search(src)
    if m:
        d = int(m.group(2), 0) * (1 if m.group(1) == "+" else -1)
        return ("G",) if ins.address + ins.size + d == G_VA else None
    md = mem_of(src)
    if md is None:
        return None
    base, disp = md
    if base in STACK:   # a reload of a spilled slot
        return org.get(("slot", base, disp))
    if org.get(base) == ("G",) and disp in TABLE_DISP:
        return ("rec", TABLE_DISP[disp], "G table +0x%X" % disp)
    return None


def apply(ins, org):
    """Advance the origin map org over one instruction. Keys are 64-bit register names and
    ("slot", base, disp) for spilled stack slots."""
    mn, ops = ins.mnemonic, ins.op_str
    parts = [p.strip() for p in ops.split(",")] if ops else []
    if mn == "call":
        for r in CALLER_SAVED:
            org.pop(r, None)
        tgt = int(ops, 16) if ops.startswith("0x") else None
        if tgt in ACC:
            org["rax"] = ("rec", ACC[tgt], "accessor 0x%X" % tgt)
        return
    if mn in ("push", "leave", "pop"):
        clear_slots(org)   # rsp moves: the displacement keys no longer name the slots
        if mn != "pop" or not parts or parts[0] not in GPR:
            return
    if parts and "[" in parts[0] and mn != "mov" and mn not in NO_WRITE:
        md = mem_of(parts[0])
        if md and md[0] in STACK:
            clear_slots(org)   # a non-mov write to the stack: not followed
    if mn.startswith("rep") or (mn in STRING_OPS and "xmm" not in ops):
        for r in ("rcx", "rsi", "rdi"):
            org.pop(r, None)
        return
    if mn in ONE_OP_IMPLICIT or (mn == "imul" and len(parts) == 1):
        org.pop("rax", None)
        org.pop("rdx", None)
        return
    if mn == "xchg":
        for p in parts:
            if p in GPR:
                org.pop(GPR[p], None)
        return
    if mn == "mov" and len(parts) == 2 and parts[0] not in GPR:   # a store to memory
        md = mem_of(parts[0])
        if md and md[0] in STACK:
            key = ("slot", md[0], md[1])
            val = org.get(GPR[parts[1]]) if parts[1] in FULL else None
            if val is None:
                org.pop(key, None)
            else:
                org[key] = val
        return
    if not parts or parts[0] not in GPR or not (mn in WRITE1 or mn.startswith(("cmov", "set"))):
        return  # no register written: a compare, a jump, an SSE op
    dst = GPR[parts[0]]
    if dst == "rsp":
        clear_slots(org)
    val = None
    if mn == "lea" and len(parts) == 2:
        md = mem_of(parts[1])
        if md and md[0] in STACK:
            org.pop(("slot", md[0], md[1]), None)   # address taken: that slot may change under a call
        elif md and md[1] == 0 and org.get(md[0]) and org[md[0]][0] == "rec":
            val = org[md[0]]
    elif parts[0] in FULL and len(parts) == 2:
        src = parts[1]
        if mn == "mov" and src in FULL:
            val = org.get(src)
        elif mn == "mov" and "ptr" in src:
            val = load_origin(ins, src, org)
    if val is None:
        org.pop(dst, None)
    else:
        org[dst] = val


class Fn:
    __slots__ = ("begin", "end", "snap", "partial")

    def __init__(self, begin, end, snap, partial):
        self.begin, self.end, self.snap, self.partial = begin, end, snap, partial


def decode(mm, md, begin, end):
    """One walk of a function, joined at every block that a jump enters. snap[addr] =
    (origins before the call or jmp at addr, its direct target or None). A block that
    follows a ret or an unconditional jmp starts from the origins recorded at the jumps
    into it (intersected with the fall-through, if any); a block that no recorded jump
    reaches starts with none."""
    code = bytes(mm[begin - BASE:end - BASE])
    org = {r: ("arg", r) for r in ARG}
    snap, pending, last = {}, {}, begin
    fall = True   # the previous instruction falls through to this one
    for ins in md.disasm(code, begin):
        a, mn, ops = ins.address, ins.mnemonic, ins.op_str
        last = a + ins.size
        if a in pending:
            join = pending.pop(a)
            org = {r: v for r, v in org.items() if join.get(r) == v} if fall else dict(join)
        elif not fall:
            org = {}
        if mn in ("call", "jmp"):
            snap[a] = (dict(org), int(ops, 16) if ops.startswith("0x") else None)
        if mn.startswith("j") and ops.startswith("0x"):
            t = int(ops, 16)
            if begin <= t < end and t > a:
                prev = pending.get(t)
                pending[t] = dict(org) if prev is None else {
                    r: v for r, v in prev.items() if org.get(r) == v}
        apply(ins, org)
        fall = mn not in ("ret", "jmp")
    return Fn(begin, end, snap, last < end)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--image", default=os.environ.get("PES_EXE"))
    ap.add_argument("--out", default=HERE)
    a = ap.parse_args()
    if not a.image:
        ap.error("--image or PES_EXE (the unpacked pes_full.bin)")
    specs = sorted(os.path.basename(p)[:-5] for p in glob.glob(os.path.join(SPEC, "*.json")))
    spec_args = {s: (json.load(open(os.path.join(SPEC, s + ".json"))).get("args") or {})
                 for s in specs}          # register -> the spec's argument name
    spec_at = {int(s, 16): s for s in specs}
    with open(a.image, "rb") as f:
        mm = mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ)
        pd = pdata.Pdata(f)
        funcs, dropped = sorted(pd.entries.items()), pd.dropped  # every entry, chunks included (as the old reader)
        begins = [b for b, _ in funcs]
        ends = [e for _, e in funcs]
        targets = set(begins) | set(spec_at)
        callers = collections.defaultdict(list)   # target VA -> E8/E9 site VAs
        for m in re.finditer(b"[\xe8\xe9]", mm):
            p = m.start()
            if p + 5 > len(mm):
                break
            t = BASE + p + 5 + int.from_bytes(mm[p + 1:p + 5], "little", signed=True)
            if t in targets:
                callers[t].append(BASE + p)
        md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
        cache = {}

        def fn_at(va):
            i = bisect.bisect_right(begins, va) - 1
            if i < 0 or va >= ends[i]:
                return None
            if i not in cache:
                cache[i] = decode(mm, md, begins[i], ends[i])
            return cache[i]

        typed = collections.defaultdict(dict)      # spec -> {argument name: class}
        evidence = collections.defaultdict(dict)   # spec -> {argument name: evidence}
        seeded = set()
        for s, c in SEED.items():
            typed[s]["A0"] = c
            evidence[s]["A0"] = {"class": c, "sites": 0, "how": ["seed: RealCases INF"]}
            seeded.add((s, "A0"))
        memo = {}
        conflicts, retired = [], set()
        untyped_why = collections.defaultdict(dict)

        def resolve(o, fn, depth):
            """(class, why) of origin o held in function fn; (None, why) when unknown."""
            if o is None:
                return None, "origin unknown"
            if o[0] == "rec":
                return o[1], o[2]
            if o[0] == "G":
                return None, "the G object"
            r = o[1]
            sp = spec_at.get(fn.begin)
            if sp is not None:
                name = spec_args[sp].get(r)
                if name is not None and name in typed[sp]:
                    return typed[sp][name], "typed root %s.%s" % (sp, name)
            return from_callers(fn, r, depth)

        def from_callers(fn, r, depth):
            """Class of argument register r of fn, from fn's own call sites."""
            if depth <= 0:
                return None, "depth limit"
            key = (fn.begin, r, depth)
            if key not in memo:
                votes = collections.defaultdict(list)
                for site in callers.get(fn.begin, ()):
                    cfn = fn_at(site)
                    st = cfn.snap.get(site) if cfn else None
                    if st is None or st[1] != fn.begin:
                        continue
                    c, why = resolve(st[0].get(r), cfn, depth - 1)
                    if c:
                        votes[c].append("%X %s" % (site, why))
                if len(votes) == 1:
                    (c, hs), = votes.items()
                    memo[key] = (c, "callers: %d site(s), e.g. %s" % (len(hs), hs[0]))
                else:
                    memo[key] = (None, "callers disagree" if votes else "no caller resolves")
            return memo[key]

        rounds, changed = 0, True
        while changed and rounds < 20:
            changed, rounds = False, rounds + 1
            memo.clear()
            for s in specs:
                sv = int(s, 16)
                for reg, name in spec_args[s].items():
                    if reg not in ARG or (s, name) in retired:
                        continue
                    votes, why = collections.defaultdict(list), collections.Counter()
                    for site in callers.get(sv, ()):
                        fn = fn_at(site)
                        st = fn.snap.get(site) if fn else None
                        if st is None or st[1] != sv:
                            why["site not decoded"] += 1
                            continue
                        c, w = resolve(st[0].get(reg), fn, DEPTH)
                        if c:
                            votes[c].append("%X %s" % (site, w))
                        else:
                            why[w] += 1
                    if votes and (len(votes) > 1 or (name in typed[s] and set(votes) != {typed[s][name]})):
                        conflicts.append({"spec": s, "root": name, "votes":
                                          {k: len(v) for k, v in votes.items()},
                                          "kept": typed[s].get(name)})
                        if (s, name) in seeded:
                            continue            # a seed stays; the conflict is on record
                        retired.add((s, name))
                        typed[s].pop(name, None)
                        evidence[s].pop(name, None)
                        changed = True
                    elif len(votes) == 1:
                        (c, hs), = votes.items()
                        if name not in typed[s]:
                            typed[s][name] = c
                            changed = True
                        evidence[s][name] = {"class": c, "sites": len(hs), "how": hs}
                    elif name not in typed[s]:
                        untyped_why[s][name] = dict(why)

        roots = [(s, name) for s in specs for reg, name in spec_args[s].items() if reg in ARG]
        out_typed = {s: {n: c for n, c in sorted(typed[s].items()) if c in BINDABLE}
                     for s in specs}
        out_typed = {s: v for s, v in out_typed.items() if v}
        unt = {}
        for s, name in roots:
            if name not in typed.get(s, {}):
                unt.setdefault(s, {})[name] = untyped_why.get(s, {}).get(name, {"no site": 1})
        os.makedirs(a.out, exist_ok=True)
        with open(os.path.join(a.out, "impure_roots.json"), "w") as w:
            json.dump(out_typed, w, indent=1, sort_keys=True)
        with open(os.path.join(a.out, "impure_roots_evidence.json"), "w") as w:
            json.dump({"typed": {s: {n: evidence[s][n] for n in sorted(typed[s]) if n in evidence[s]}
                                 for s in sorted(typed) if typed[s]},
                       "not_bindable": sorted("%s.%s=%s" % (s, n, c) for s in typed
                                              for n, c in typed[s].items() if c not in BINDABLE),
                       "conflicts": conflicts, "untyped": unt,
                       "roots_total": len(roots), "roots_typed": sum(len(v) for v in typed.values()),
                       "specs": len(specs), "pdata_functions": len(funcs),
                       "pdata_dropped": dropped, "rounds": rounds},
                      w, indent=1, sort_keys=True)
        print("roots %d; typed %d (bindable %d); untyped %d; conflicts %d; rounds %d; "
              "pdata functions %d (dropped %d)" % (
                  len(roots), sum(len(v) for v in typed.values()),
                  sum(len(v) for v in out_typed.values()), len(roots) - sum(len(v) for v in typed.values()),
                  len(conflicts), rounds, len(funcs), dropped))


if __name__ == "__main__":
    main()
