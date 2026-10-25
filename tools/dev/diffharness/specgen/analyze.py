#!/usr/bin/env python3
"""Usage: venv/bin/python analyze.py  -> analysis.json (in this dir).

Script-read record-access map for each pilot function. Decodes the unpacked
image (file offset = VA - 0x140000000) with capstone; the pilot 'size' is an
instruction count. Tracks, per register, which argument-rooted object its value
points into. Object identity = (root, path, off): root A0..A3 = Windows x64 arg
registers at entry, path = pointer fields followed from the root, IMG = absolute
image address. Origins are candidate sets: ("ALT", (o1, o2, ...)) where the
value may be any of them (a join at a branch merge keeps every candidate, so a
pointer that is rcx+0x22c64 on one path and rcx+0x22a7c on the other stays
resolved). None = unknown. Pilot-set callees are walked with the caller's
origins; their return origins (union over all rets) flow back into rax.
Anything not modelled is flagged.
"""
import json, os, struct
from capstone import Cs, CS_ARCH_X86, CS_MODE_64, CS_AC_READ, CS_AC_WRITE
from capstone.x86 import X86_OP_REG, X86_OP_MEM, X86_OP_IMM

BASE = 0x140000000
ARGREGS = ("rcx", "rdx", "r8", "r9")
ARGROOTS = ("A0", "A1", "A2", "A3")
CALLER_SAVED = ("rax", "rcx", "rdx", "r8", "r9", "r10", "r11")
STACK_ARG0 = 0x28  # entry-relative [rsp+0x28] = 5th arg (shadow 0x20 + return addr 8)
MAX_ALTS = 16
ELEM = 0xE1EE  # path sentinel: element of a global pointer table (its own object)
MOVLIKE = ("mov", "movss", "movsd", "movq", "movd", "movups", "movaps", "movdqu", "movdqa",
           "movlps", "movhps", "movlpd", "movhpd", "lea", "movzx", "movsx", "movsxd", "cvtsi2ss", "cvtsi2sd")

fh = None
SECS = []


def rd(va, n):
    fh.seek(va - BASE)
    return fh.read(n)


def open_image(path):
    """Bind the analyzer to one unpacked image and read its PE section table."""
    global fh
    fh = open(path, "rb")
    e_lfanew = struct.unpack("<I", rd(BASE + 0x3C, 4))[0]
    nsec = struct.unpack("<H", rd(BASE + e_lfanew + 6, 2))[0]
    optsz = struct.unpack("<H", rd(BASE + e_lfanew + 0x14, 2))[0]
    SECS.clear()
    for i in range(nsec):
        s = rd(BASE + e_lfanew + 0x18 + optsz + 40 * i, 40)
        name = s[:8].rstrip(b"\0").decode()
        vsz, va_rel = struct.unpack("<II", s[8:16])
        SECS.append((name, BASE + va_rel, max(vsz, 1)))


def section(va):
    for n, a, s in SECS:
        if a <= va < a + s:
            return n
    return "none"


RC = {}
for n in ["ax", "bx", "cx", "dx"]:
    f = "r" + n
    RC.update({f: f, "e" + n: f, n: f, n[0] + "l": f, n[0] + "h": f})
for n in ["si", "di", "bp", "sp"]:
    f = "r" + n
    RC.update({f: f, "e" + n: f, n: f, n + "l": f})
for i in range(8, 16):
    f = "r%d" % i
    RC.update({f: f, f + "d": f, f + "w": f, f + "b": f})


def canon(n):
    return RC.get(n, n)


def kind_of(mn, width):
    if mn.endswith("ss") and width == 4:
        return "f32"
    if mn.endswith("sd") and width == 8:
        return "f64"
    if width == 16 or mn.startswith(("movups", "movaps", "movdqu", "movdqa", "movlps", "movhps", "movlpd", "movhpd")):
        return "vec"
    return "int"


def new_acc():
    return {"w": set(), "r": False, "wr": False, "k": set(), "idx": False, "ptr": False}


def alts(o):
    """Candidate origins of a value (empty for unknown)."""
    if o is None:
        return []
    if o[0] == "ALT":
        return list(o[1])
    return [o]


def mk(xs):
    """Candidate set from origins; any unknown (None) member makes the whole value unknown."""
    out = []
    for x in xs:
        if x is None:
            return None
        if x not in out:
            out.append(x)
    if not out or len(out) > MAX_ALTS:
        return None
    return out[0] if len(out) == 1 else ("ALT", tuple(out))


def join(vals):
    """Union of candidate origins over paths. A path with no origin (null or scalar) is dropped:
    code only dereferences the pointer paths, so the known candidates still describe every deref."""
    known = [v for v in vals if v is not None]
    if not known:
        return None
    return mk([x for v in known for x in alts(v)])


def merge_states(states):
    """Join register-origin, rsp and stack-slot states at a branch target."""
    keys = set()
    for o, _, _ in states:
        keys |= set(o)
    o = {k: join([s[0].get(k) for s in states]) for k in keys}
    rs = [s[1] for s in states]
    r = rs[0] if all(x == rs[0] for x in rs) else None
    skeys = set()
    for _, _, st in states:
        skeys |= set(st)
    st = {k: join([s[2].get(k) for s in states]) for k in skeys}
    return o, r, st


class Walker:
    def __init__(self, size_of):
        self.md = Cs(CS_ARCH_X86, CS_MODE_64)
        self.md.detail = True
        self.size_of = size_of
        self.acc = {}          # (root, path, off) -> dict
        self.calls = set()
        self.flags = set()
        self.stack_args = {}   # index -> set(widths)
        self.live_in = []
        self.arg_roots = set() # A-roots passed to any callee (must be bound as regions)
        self.rets = []         # top-level return summaries

    def rec(self, key, width, rw, mn, idx=False):
        d = self.acc.setdefault(key, new_acc())
        d["w"].add(width)
        d["r"] |= "r" in rw
        d["wr"] |= "w" in rw
        d["k"].add(kind_of(mn, width))
        d["idx"] |= idx

    def load(self, addrs, idx, stk):
        out = []
        for a in addrs:
            if a[0] == "STK":
                out.append(stk.get(a[2]))
            elif idx and a[0] == "IMG" and len(a[1]) == 1:
                # element of a global pointer table (object a[1][0]): the element is its own object
                out.append(("IMG", (a[1][0], ELEM), 0))
            elif idx:
                out.append(None)   # element of an unknown array: pointer not tracked
            else:
                out.append((a[0], a[1] + (a[2],), 0))
        return mk(out) if addrs else None

    def walk(self, va, origins, toplevel, depth, chain):
        """Returns the union of rax origins over every ret (None = unknown)."""
        if depth > 20 or va in chain:
            self.flags.add("recursion-cut@%X" % va)
            return None
        size = self.size_of.get(va)
        if size is None:
            self.flags.add("unknown-callee@%X" % va)
            return None
        insns = list(self.md.disasm(rd(va, size * 15 + 16), va, count=size))
        end = insns[-1].address + insns[-1].size if insns else va
        if not insns or insns[-1].mnemonic not in ("ret", "jmp"):
            self.flags.add("last-not-ret@%X" % va)
        origins = dict(origins)
        origins["rsp"] = ("STK", (), 0)
        stk = {}
        rsp = 0
        snaps = {}
        prev_mn = None
        touched = set()
        rets = []
        last_rax_w = None
        for ins in insns:
            mn = ins.mnemonic
            ops = ins.operands
            term = prev_mn in ("ret", "jmp")
            prev_mn = mn
            if ins.address in snaps:
                states = snaps.pop(ins.address)
                if not term:
                    states.append((dict(origins), rsp, dict(stk)))
                origins, rsp, stk = merge_states(states)
            regops = [o for o in ops if o.type == X86_OP_REG]
            if toplevel:
                for i, o in enumerate(ops):
                    if o.type != X86_OP_REG:
                        continue
                    r = canon(ins.reg_name(o.reg))
                    if r in ARGREGS or r in ("xmm0", "xmm1", "xmm2", "xmm3"):
                        if r in touched:
                            continue
                        touched.add(r)
                        is_write_dst = i == 0 and mn in MOVLIKE
                        if not is_write_dst and (o.access & CS_AC_READ):
                            self.live_in.append(r)
            if mn.startswith("rep") or mn.startswith("stos") or (mn.startswith("movs") and not ops):
                self.flags.add("implicit-mem:%s@%X" % (mn, ins.address))
            if mn.startswith("f") and not mn.startswith("fmadd"):
                self.flags.add("x87?:%s@%X" % (mn, ins.address))
            if mn in ("rdtsc", "cpuid", "syscall", "in", "out"):
                self.flags.add("unsupported:%s@%X" % (mn, ins.address))

            if mn == "call":
                tgt = ops[0].imm if ops and ops[0].type == X86_OP_IMM else None
                sub = {r: origins.get(r) for r in ARGREGS}
                for r in ARGREGS:
                    for a in alts(sub[r]):
                        if a[0] in ARGROOTS:
                            self.arg_roots.add(a[0])
                    if toplevel and sub[r] is not None and r not in touched:
                        touched.add(r)
                        self.live_in.append(r)   # passed through: callee may read it
                rv = None
                if tgt is None:
                    self.flags.add("indirect-call@%X" % ins.address)
                elif tgt in self.size_of:
                    self.calls.add("%X" % tgt)
                    rv = self.walk(tgt, sub, False, depth + 1, chain | {va})
                else:
                    self.flags.add("callee-outside-set@%X" % tgt)
                for r in CALLER_SAVED:
                    origins[r] = None
                origins["rax"] = rv
                continue
            if mn.startswith("j"):
                if ops and ops[0].type == X86_OP_IMM:
                    t = ops[0].imm
                    if not (va <= t < end):
                        self.flags.add("tailjump@%X" % ins.address)
                    elif t > ins.address:
                        snaps.setdefault(t, []).append((dict(origins), rsp, dict(stk)))
                continue
            if mn == "ret":
                rets.append(origins.get("rax"))
                if toplevel:
                    self.rets.append({"at": "%X" % ins.address, "rax_ptr": origins.get("rax") is not None,
                                      "rax_w": last_rax_w, "xmm0_w": None})
                continue
            if mn in ("nop", "int3") or mn.startswith("prefetch") or mn.startswith("endbr"):
                continue
            if mn == "leave":
                rbp = origins.get("rbp")
                if rbp is not None and any(a[0] == "STK" for a in alts(rbp)):
                    a = [x for x in alts(rbp) if x[0] == "STK"][0]
                    rsp = a[2]
                    origins["rsp"] = ("STK", (), rsp)
                    origins["rbp"] = stk.get(rsp)
                    rsp += 8
                else:
                    self.flags.add("leave-unknown@%X" % ins.address)
                    rsp = None
                continue
            if mn == "push":
                r = canon(ins.reg_name(ops[0].reg)) if ops[0].type == X86_OP_REG else None
                if rsp is not None:
                    rsp -= 8
                    stk[rsp] = origins.get(r) if r else None
                    origins["rsp"] = ("STK", (), rsp)
                continue
            if mn == "pop":
                r = canon(ins.reg_name(ops[0].reg)) if ops[0].type == X86_OP_REG else None
                if rsp is not None:
                    if r:
                        origins[r] = stk.get(rsp)
                    rsp += 8
                    origins["rsp"] = ("STK", (), rsp)
                continue
            if mn in ("sub", "add") and ops and ops[0].type == X86_OP_REG and canon(ins.reg_name(ops[0].reg)) == "rsp":
                if len(ops) == 2 and ops[1].type == X86_OP_IMM and rsp is not None:
                    rsp += -ops[1].imm if mn == "sub" else ops[1].imm
                    origins["rsp"] = ("STK", (), rsp)
                else:
                    rsp = None
                    self.flags.add("rsp-unknown@%X" % ins.address)
                continue

            # memory operands -> candidate addresses
            mem_items = []
            for o in ops:
                if o.type != X86_OP_MEM:
                    continue
                m = o.mem
                bname = canon(ins.reg_name(m.base)) if m.base else None
                iname = canon(ins.reg_name(m.index)) if m.index else None
                idx = bool(m.index)
                if bname == "rip":
                    addrs = [("IMG", (), ins.address + ins.size + m.disp)]
                elif bname is None:
                    addrs = [("IMG", (), m.disp)]
                elif bname == "rsp":
                    addrs = [("STK", (), rsp + m.disp)] if rsp is not None else []
                else:
                    bo = origins.get(bname)
                    addrs = [("STK", (), a[2] + m.disp) if a[0] == "STK" else (a[0], a[1], a[2] + m.disp)
                             for a in alts(bo)]
                mem_items.append((o, addrs, idx))

            for o, addrs, idx in mem_items:
                if mn == "lea" or o.size == 0:
                    continue
                if not addrs:
                    self.flags.add("unresolved-mem@%X" % ins.address)
                    continue
                rw = ("r" if o.access & CS_AC_READ else "") + ("w" if o.access & CS_AC_WRITE else "")
                for addr in addrs:
                    root, path, off = addr
                    if root == "STK":
                        if toplevel and isinstance(off, int) and off >= STACK_ARG0:
                            self.stack_args.setdefault((off - STACK_ARG0) // 8, set()).add(o.size)
                        if o.access & CS_AC_WRITE:
                            src = ops[1] if (mn == "mov" and len(ops) == 2 and ops[0] is o
                                             and ops[1].type == X86_OP_REG) else None
                            stk[off] = origins.get(canon(ins.reg_name(src.reg))) if src is not None else None
                        continue
                    key = (root, path, ("IDX", off) if idx else off)
                    self.rec(key, o.size, rw, mn, idx)
                    if path:
                        self.acc.setdefault((root, path[:-1], path[-1]), new_acc())["ptr"] = True

            # register-origin propagation
            dst = regops[0] if regops and ops[0].type == X86_OP_REG else None
            if dst is not None and canon(ins.reg_name(dst.reg)) == "rax":
                last_rax_w = dst.size
            if mn == "mov" and len(ops) == 2:
                if dst is not None:
                    d = canon(ins.reg_name(dst.reg))
                    src = ops[1]
                    if dst.size != 8:
                        origins[d] = None
                    elif src.type == X86_OP_REG:
                        origins[d] = origins.get(canon(ins.reg_name(src.reg)))
                    elif src.type == X86_OP_MEM:
                        item = next((x for x in mem_items if x[0] is src), None)
                        origins[d] = self.load(item[1], item[2], stk) if item else None
                    else:
                        origins[d] = None
                continue
            if mn == "lea" and dst is not None:
                item = next((x for x in mem_items if x[0] is ops[1]), None)
                if dst.size != 8 or item is None or ops[1].mem.index:
                    origins[canon(ins.reg_name(dst.reg))] = None   # indexed address: element, not tracked
                else:
                    origins[canon(ins.reg_name(dst.reg))] = mk(item[1])
                continue
            if mn == "add" and dst is not None and dst.size == 8 and len(ops) == 2 and ops[1].type == X86_OP_REG \
                    and ops[1].size == 8:
                # lane-array address: rdx = index*stride (scalar, untracked) + rcx (base object) keeps the base origin
                d = canon(ins.reg_name(dst.reg))
                s = canon(ins.reg_name(ops[1].reg))
                if origins.get(d) is None and origins.get(s) is not None:
                    origins[d] = origins[s]
                    continue
                if origins.get(s) is None and origins.get(d) is not None:
                    continue
            if mn in ("add", "sub") and dst is not None and len(ops) == 2 and ops[1].type == X86_OP_IMM:
                d = canon(ins.reg_name(dst.reg))
                delta = ops[1].imm if mn == "add" else -ops[1].imm
                if dst.size != 8:
                    origins[d] = None
                else:
                    origins[d] = mk([(a[0], a[1], a[2] + delta) if a[0] != "STK" else ("STK", (), a[2] + delta)
                                     for a in alts(origins.get(d))])
                continue
            for o in ops:
                if o.type == X86_OP_REG and o.access & CS_AC_WRITE:
                    origins[canon(ins.reg_name(o.reg))] = None
        return join(rets)


def summarize(va, size, size_of):
    w = Walker(size_of)
    w.walk(va, {"rcx": ("A0", (), 0), "rdx": ("A1", (), 0), "r8": ("A2", (), 0), "r9": ("A3", (), 0)},
           True, 0, frozenset())
    fields = []
    for (root, path, off), v in sorted(w.acc.items(), key=lambda kv: (kv[0][0], kv[0][1], str(kv[0][2]))):
        off_s = ("IDX+0x%X" % off[1]) if isinstance(off, tuple) else ("0x%X" % off)
        sect = None
        if root == "IMG":
            if not path and isinstance(off, int):
                sect = section(off)
            elif path and isinstance(path[0], int):
                sect = section(path[0])
        fields.append({
            "obj": root + "".join(".%X" % p for p in path),
            "root": root,
            "path": ["0x%X" % p for p in path],
            "off": off_s,
            "w": sorted(v["w"]),
            "r": v["r"], "wr": v["wr"],
            "kind": sorted(v["k"]),
            "ptr": v["ptr"],
            "sect": sect,
        })
    return {
        "va": "0x%X" % va, "size": size,
        "fields": fields,
        "calls": sorted(w.calls),
        "arg_roots": sorted(w.arg_roots),
        "stack_args": {str(k): sorted(v) for k, v in sorted(w.stack_args.items())},
        "live_in": sorted(set(w.live_in)),
        "rets": w.rets,
        "flags": sorted(w.flags),
    }


def analyze_functions(funcs, size_of):
    """funcs: [{va (int), size, status, subsystem}] -> one summary row per function, in order."""
    return [dict(summarize(f["va"], int(f["size"]), size_of), status=f["status"], subsystem=f["subsystem"])
            for f in funcs]
