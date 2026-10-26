#!/usr/bin/env python3
"""Argument-root typing for the impure specs, from the callers' bytes (read-only).

1. Scan the unpacked image for direct call/jmp (E8/E9) sites into each impure spec VA.
2. Label where rcx/rdx/r8/r9 come from at each site, decoding the 40 instructions
   before it with capstone: a G-table accessor result (class from
   docs/re/pes-re-player-record.md 1.1), a 64-bit copy of a register (pass-through), a
   32-bit copy (int: truncated, never a pointer), a field, an address, a global, the
   stack, or a call that clobbers the register.
3. Entry aliases: the 64-bit registers that hold each entry argument (rcx..r9) in the
   first 40 instructions of a spec, before any call.
4. Propagate: a root is typed by an accessor result at a site, or by passing the
   enclosing spec's own typed root (the site's nearest spec VA below it, within 0x8000,
   is taken as the enclosing function: an approximation, recorded in the evidence).
   Seeds: the team/man roots already classed from spec evidence in
   docs/ports/pes-port-RealCases.md (INF). Conflicts are reported, not resolved.

Writes:
  impure_roots.json           {spec: {root: class}}  (seed.py --bank-classes)
  impure_roots_evidence.json  {typed: {spec: {root: {class, how, sites}}}, conflicts, ...}
Usage: PES_EXE=<unpacked pes_full.bin> python3 -B argroots.py [--out DIR]
"""
import argparse
import collections
import glob
import json
import mmap
import os
import re

import capstone

BASE = 0x140000000
G_VA = 0x1436F3DC8
HERE = os.path.dirname(os.path.abspath(__file__))
SPEC = os.path.join(HERE, "..", "diffharness", "specs", "impure")
# accessor VA -> class (docs/re/pes-re-player-record.md 1.1; rows 0xA1 team, 0xA3 side,
# 0xA5 anime, 0xBF man, 0xF3 slot)
ACC = {0x140A48A30: "team", 0x140A489D0: "side", 0x140A48910: "anime",
       0x140A48A10: "man", 0x140A489F0: "slot"}
SEED = {"1404F47A0": "team", "1404F6C30": "team", "1404F8C70": "team",
        "1404FA720": "team", "14064DE60": "man", "140978E70": "man"}
ARG64 = {"rcx": "rcx", "rdx": "rdx", "r8": "r8", "r9": "r9"}   # entry argument registers
ARGREG = {"rcx": ("rcx", "ecx", "cx", "cl"), "rdx": ("rdx", "edx", "dx", "dl"),
          "r8": ("r8", "r8d", "r8w", "r8b"), "r9": ("r9", "r9d", "r9w", "r9b")}
REG2A = {"rcx": "A0", "rdx": "A1", "r8": "A2", "r9": "A3"}
WRITE_MN = {"mov", "lea", "movzx", "movsx", "movsxd", "xor", "add", "sub", "imul", "shl",
            "shr", "sar", "and", "or", "neg", "not", "cmove", "cmovne", "cmova", "cmovb",
            "cmovae", "cmovg", "cmovl", "cmovle", "cmovge", "cmovbe", "cmovs", "cmovns",
            "sete", "setne", "pop", "inc", "dec"}
R64 = {"eax": "rax", "ebx": "rbx", "ecx": "rcx", "edx": "rdx", "esi": "rsi", "edi": "rdi",
       "ebp": "rbp", "esp": "rsp"}
RIP = re.compile(r"\[rip ([+-]) (0x[0-9a-fA-F]+)\]")
PTR64 = re.compile(r"r(ax|bx|cx|dx|si|di|bp|sp|[0-9]+)")       # 64-bit register
INT32 = re.compile(r"e(ax|bx|cx|dx|si|di|bp|sp)|r[0-9]+d")     # 32-bit register


def r64(name):
    if name in R64:
        return R64[name]
    m = re.fullmatch(r"(r\d+)d", name)
    return m.group(1) if m else name


def dst(ins):
    return ins.op_str.split(",")[0].strip()


def src(ins):
    p = ins.op_str.split(",", 1)
    return p[1].strip() if len(p) > 1 else ""


def label(ins):
    s = src(ins)
    if ins.mnemonic == "xor" and s == dst(ins):
        return "zero"
    m = RIP.search(s)
    if m:
        d = int(m.group(2), 16) * (1 if m.group(1) == "+" else -1)
        va = ins.address + ins.size + d
        return "glob:G" if va == G_VA else "glob:0x%X" % va
    m = re.search(r"\[(\w+)(?: \+ (0x[0-9a-fA-F]+))?\]", s)
    if m and ins.mnemonic in ("mov", "movzx", "movsx", "movsxd"):
        return "fld:%s+%s" % (m.group(1), m.group(2) or "0")
    m = re.search(r"\[(\w+) \+ (0x[0-9a-fA-F]+)\]", s)
    if m and ins.mnemonic == "lea":
        return "adr:%s+%s" % (m.group(1), m.group(2))
    if "rsp" in s or "rbp - " in s:
        return "stk"
    if PTR64.fullmatch(s):  # a 64-bit copy of a register
        return "pass:%s" % s
    if INT32.fullmatch(s):
        return "int:%s" % r64(s)     # a 32-bit copy: truncated, not a pointer
    return "other:%s" % ins.mnemonic


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--image", default=os.environ.get("PES_EXE"))
    ap.add_argument("--out", default=HERE)
    a = ap.parse_args()
    if not a.image:
        ap.error("--image or PES_EXE (the unpacked pes_full.bin)")
    specs = sorted(os.path.basename(p)[:-5] for p in glob.glob(SPEC + "/*.json"))
    spec_va = sorted(int(s, 16) for s in specs)
    targets = {int(s, 16) for s in specs}
    f = open(a.image, "rb")
    mm = mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ)
    hits = collections.defaultdict(list)
    for m in re.finditer(b"[\xe8\xe9]", mm):
        p = m.start()
        if p + 5 > len(mm):
            break
        t = BASE + p + 5 + int.from_bytes(mm[p + 1:p + 5], "little", signed=True)
        if t in targets:
            hits[t].append(BASE + p)
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)

    def window(site):
        lo = site - 0x80
        raw = bytes(mm[lo - BASE: site - BASE + 16])
        for st in range(0, 0x80):
            seq = []
            for ins in md.disasm(raw[st:], lo + st):
                if ins.address == site:
                    return seq
                if ins.address > site:
                    break
                seq.append(ins)
        return None

    def source(seq, reg):
        names = ARGREG[reg]
        for idx in range(len(seq) - 1, -1, -1):
            ins = seq[idx]
            if ins.mnemonic == "call":
                return "clob:%s" % ins.op_str
            if ins.mnemonic in WRITE_MN and dst(ins) in names:
                if ins.mnemonic in ("mov", "lea") and src(ins) in ("rax", "eax"):
                    for j in range(idx - 1, max(-1, idx - 6), -1):
                        prev = seq[j]
                        if prev.mnemonic == "call":
                            if prev.op_str.startswith("0x"):
                                tgt = int(prev.op_str, 16)
                                return "acc:%s" % ACC[tgt] if tgt in ACC else "call:0x%X" % tgt
                            return "call:indirect"
                return label(ins)
        return "entry"

    def enclosing(site):
        cand = [v for v in spec_va if v <= site]
        return "%X" % cand[-1] if cand and site - cand[-1] <= 0x8000 else None

    sites = []
    for s in specs:
        for site in hits.get(int(s, 16), []):
            seq = window(site)
            if seq is None:
                continue
            sites.append({"spec": s, "site": "%X" % site, "encl": enclosing(site),
                          "args": {r: source(seq, r) for r in ARGREG}})
    aliases = {}
    for s in specs:
        va = int(s, 16)
        al = {r: set() for r in ARGREG}
        for n, ins in enumerate(md.disasm(bytes(mm[va - BASE: va - BASE + 0x200]), va)):
            if n >= 40 or ins.mnemonic in ("call", "jmp", "ret"):
                break
            if ins.mnemonic == "mov" and PTR64.fullmatch(dst(ins)):
                for r in ARG64:
                    if src(ins) == r:
                        al[r].add(dst(ins))
        aliases[s] = {r: sorted(v) for r, v in al.items()}

    cls, how = {}, {}
    for s, c in SEED.items():
        cls[(s, "A0")] = c
        how[(s, "A0")] = {"how": "seed: RealCases INF (team child slots / argmap team-field, man-field)",
                          "sites": []}
    conflicts = []
    changed = True
    while changed:
        changed = False
        for st in sites:
            S, E = st["spec"], st["encl"]
            for reg, lab in st["args"].items():
                k = REG2A[reg]
                new = None
                if lab.startswith("acc:"):
                    new = (lab[4:], "accessor result at %s" % st["site"], [st["site"]])
                elif lab.startswith("pass:") and E in aliases:
                    R = lab[5:]
                    for m_, names in aliases[E].items():
                        if R in names and (E, REG2A[m_]) in cls:
                            new = (cls[(E, REG2A[m_])], "pass of %s.%s at %s" % (E, REG2A[m_], st["site"]),
                                   [st["site"]])
                if new:
                    old = cls.get((S, k))
                    if old is None:
                        cls[(S, k)] = new[0]
                        how[(S, k)] = {"how": new[1], "sites": new[2]}
                        changed = True
                    elif old != new[0]:
                        conflicts.append({"spec": S, "root": k, "kept": old, "other": new[0], "site": st["site"]})
    typed = collections.defaultdict(dict)
    evid = collections.defaultdict(dict)
    for (s, k), c in sorted(cls.items()):
        typed[s][k] = c
        evid[s][k] = dict(how[(s, k)], **{"class": c})
    os.makedirs(a.out, exist_ok=True)
    json.dump(typed, open(os.path.join(a.out, "impure_roots.json"), "w"), indent=1, sort_keys=True)
    json.dump({"typed": evid, "conflicts": conflicts, "sites": len(sites), "specs": len(specs)},
              open(os.path.join(a.out, "impure_roots_evidence.json"), "w"), indent=1, sort_keys=True)
    print("sites %d; typed root slots %d over %d specs; conflicts %d" % (
        len(sites), len(cls), len(typed), len(conflicts)))


if __name__ == "__main__":
    main()
