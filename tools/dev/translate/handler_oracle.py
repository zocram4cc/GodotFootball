#!/usr/bin/env python3
"""handler_oracle.py: the reference rows of a hand-ported PES handler, from PES's own bytes.

One rendezvous capture (a real call) is replayed in the unicorn harness with a hook on every `call` instruction of the
handler's own body. Each call is logged in execution order: the call site, the argument registers (GPRs, XMM, the first
stack arguments), the 32 bytes behind each pointer argument before and after the call, and the result (rax, xmm0, the 32
bytes behind rax, and any `--ret-dump` window behind rax). The request block (the handler's `req` argument, `--req-words`
words) is dumped at the entry and at the return, with rax; so are the context object (`--ctx-dump` bytes behind
`--ctx-reg`) and the `--probe` reads (memory the handler reads directly: `name=q(q(0x1436F3DC8)+0x270)+0x116c:4`).
A hand port is proven when, fed the same request words and answering its host calls from the rows, it writes the same
request words and returns the same value (tests/pes_movestep_turn_entry_test.cpp).

Modes: `real` (the capture as it stands: a REAL call) and `perturbed` (`--perturb SPEC.json --count N --seed S`: request
words redrawn before the run; the callees still run PES's bytes over the capture's memory, so every host answer is PES's
own for that input; SYNTHETIC inputs). SPEC.json = [{"word": idx, "choices": [raw u32, ...]} | {"word": idx, "f32":
[lo, hi]}] drawn independently per case.

Output (one block per case, whitespace separated tokens, `-` for empty hex):
  case LABEL MODE ENTRY
  regs RCX RDX R8 R9                (entry registers)
  req_in N w0 w1 ...                (decimal u32)          req_out N w0 ...      rax N
  ctx HEX                           probe NAME HEX
  call SITE RAX XMM0LO64 RCX RDX R8 R9 X0 X1 X2 X3 STACK0 IN_RCX IN_RDX IN_R8 IN_R9 OUT_RCX OUT_RDX OUT_R8 OUT_R9 RETMEM EXTRA
  end

Usage: PES_EXE=<pes_full.bin> python3 handler_oracle.py SNAP_DIR HOOKS_CFG OUT.txt [--req-reg rdx] [--req-words 432]
           [--ctx-reg rcx --ctx-dump 64] [--probe NAME=EXPR ...] [--ret-dump SITE:OFF:LEN ...]
           [--perturb SPEC.json --count N --seed S] [--sites]       (harness Python: unicorn, capstone)
  --sites  prints the handler's call sites (address, text) and exits.
"""
import argparse
import json
import os
import random
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import frame_trace as ft  # noqa: E402
import pdata  # noqa: E402
import capstone  # noqa: E402
from capstone import x86  # noqa: E402
import unicorn.x86_const as xc  # noqa: E402

PES_EXE = os.environ.get("PES_EXE", "")
PTR_LO, PTR_HI = 0x10000, 0x7FFFFFFFFFFF
DUMP = 32  # bytes behind a pointer argument or result
GPRS = ("rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15")
ARG_REGS = ("rcx", "rdx", "r8", "r9")


def call_sites(entry):
    """Every call instruction of the function at entry."""
    with open(PES_EXE, "rb") as f:
        pd = pdata.Pdata(f)
        md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
        md.detail = True
        insns = pd.decode(md, entry)
    if insns is None:
        sys.exit("%#x is not a function start" % entry)
    return [i for i in insns if i.mnemonic == "call"]


def hexs(b):
    return b.hex() if b else "-"


class Oracle:
    def __init__(self, snap, hooks, entry, args):
        self.snap, self.hooks, self.entry, self.args = snap, hooks, entry, args
        self.sites = {i.address: i for i in call_sites(entry)}
        self.lazy = None
        self.ret_dumps = {}
        for spec in args.ret_dump:
            site, off, ln = spec.split(":")
            self.ret_dumps[int(site, 0)] = (int(off, 0), int(ln, 0))
        outer = self

        class Capturing(ft.LazyMap):  # run_recorded builds its LazyMap internally: keep a handle for reads and writes
            def __init__(self, *a, **k):
                super().__init__(*a, **k)
                outer.lazy = self

        ft.LazyMap = Capturing

    def readable(self, uc, addr, n):
        if not PTR_LO <= addr < PTR_HI:
            return None
        if self.lazy is not None and not self.lazy.ensure(addr, n):
            return None
        try:
            return bytes(uc.mem_read(addr, n))
        except Exception:
            return None

    @staticmethod
    def reg(name):
        return getattr(xc, "UC_X86_REG_" + name.upper())

    def target_of(self, uc, ins):
        op = ins.operands[0]
        if op.type == x86.X86_OP_IMM:
            return op.imm
        if op.type == x86.X86_OP_REG:
            return uc.reg_read(self.reg(ins.reg_name(op.reg)))
        base = 0 if op.mem.base == 0 else uc.reg_read(self.reg(ins.reg_name(op.mem.base)))
        idx = 0 if op.mem.index == 0 else uc.reg_read(self.reg(ins.reg_name(op.mem.index))) * op.mem.scale
        at = (base + idx + op.mem.disp) & 0xFFFFFFFFFFFFFFFF
        raw = self.readable(uc, at, 8)
        return struct.unpack("<Q", raw)[0] if raw else None

    def address(self, uc, body):
        """q(q(0x1436F3DC8)+0x270)+0x116c / rcx+0xae0 -> int: q(E) dereferences 8 bytes, + adds, rNN is an entry register."""
        toks = re.findall(r"q\(|\)|\+|0x[0-9a-fA-F]+|\d+|r[a-z0-9]+", body)
        pos = [0]

        def term():
            t = toks[pos[0]]
            pos[0] += 1
            if t == "q(":
                at = total()
                pos[0] += 1  # the closing paren
                raw = self.readable(uc, at, 8)
                return struct.unpack("<Q", raw)[0] if raw else 0
            if t[0] == "r":
                return uc.reg_read(self.reg(t))
            return int(t, 0)

        def total():
            v = term()
            while pos[0] < len(toks) and toks[pos[0]] == "+":
                pos[0] += 1
                v += term()
            return v

        return total()

    def probe(self, uc, expr):
        """name=EXPR:N -> the N bytes (default 4) at the address EXPR names."""
        body, _, n = expr.partition(":")
        return self.readable(uc, self.address(uc, body), int(n or 4)) or b""

    def call_args(self, uc):
        r = {k: uc.reg_read(self.reg(k)) for k in ("rcx", "rdx", "r8", "r9", "rsp")}
        out = {k: r[k] for k in ARG_REGS}
        out["xmm"] = [ft._xmm_int(uc, i) & 0xFFFFFFFFFFFFFFFF for i in range(4)]
        raw = self.readable(uc, r["rsp"] + 0x20, 8)
        out["stack0"] = struct.unpack("<Q", raw)[0] if raw else 0
        out["mem"] = {k: self.readable(uc, r[k], DUMP) or b"" for k in ARG_REGS}
        return out

    def run(self, perturb=None):
        log, pending = [], []
        state = {"first": True}
        a = self.args

        def on_code(uc, addr, size, data):
            if state["first"]:
                state["first"] = False
                req_addr = uc.reg_read(self.reg(a.req_reg))
                state["req_addr"] = req_addr
                if perturb:
                    for target, size, value in perturb:
                        at = req_addr + 4 * target if isinstance(target, int) else self.address(uc, target)
                        if self.readable(uc, at, size) is not None:
                            uc.mem_write(at, value.to_bytes(size, "little"))
                raw = self.readable(uc, req_addr, 4 * a.req_words)
                state["req_in"] = struct.unpack("<%dI" % a.req_words, raw)
                state["regs_in"] = [uc.reg_read(self.reg(k)) for k in ARG_REGS]
                ctx = uc.reg_read(self.reg(a.ctx_reg))
                state["ctx"] = self.readable(uc, ctx, a.ctx_dump) if a.ctx_dump else b""
                state["probes"] = [(p.partition("=")[0], self.probe(uc, p.partition("=")[2])) for p in a.probe]
            if pending and addr == pending[-1]["next"]:
                row = pending.pop()
                rax = uc.reg_read(self.reg("rax"))
                row["rax"] = rax
                row["xmm0"] = ft._xmm_int(uc, 0) & 0xFFFFFFFFFFFFFFFF
                row["retmem"] = self.readable(uc, rax, DUMP) or b""
                off, ln = self.ret_dumps.get(row["site"], (0, 0))
                row["extra"] = (self.readable(uc, rax + off, ln) or b"") if ln else b""
                row["after"] = {k: self.readable(uc, row["args"][k], DUMP) or b"" for k in ARG_REGS}
                log.append(row)
                return
            ins = self.sites.get(addr)
            if ins is not None:
                pending.append({"site": addr, "target": self.target_of(uc, ins), "args": self.call_args(uc),
                                "next": addr + size})

        uc, err = ft.run_recorded(self.snap, orig=ft.hook_bytes(self.hooks), trace=self.trace(), on_code=on_code)
        if err is not None:
            return None, err
        raw = self.readable(uc, state["req_addr"], 4 * a.req_words)
        return {"regs": state["regs_in"], "req_in": list(state["req_in"]),
                "req_out": list(struct.unpack("<%dI" % a.req_words, raw)), "ctx": state["ctx"],
                "probes": state["probes"], "rax": uc.reg_read(self.reg("rax")), "calls": log}, None

    def trace(self):
        scratch = os.path.join(os.environ.get("TMPDIR_SCRATCH", "."), "handler_oracle_trace",
                               os.path.basename(self.snap.rstrip("/")))
        os.makedirs(scratch, exist_ok=True)
        return {"out": scratch, "impure": {}, "stubs": ft.stubs_for_run(ft.stub_table.load())}


def write_case(f, case, label, mode, entry):
    f.write("case %s %s %#x\n" % (label, mode, entry))
    f.write("regs %s\n" % " ".join("%x" % r for r in case["regs"]))
    f.write("req_in %d %s\n" % (len(case["req_in"]), " ".join(str(w) for w in case["req_in"])))
    f.write("req_out %d %s\n" % (len(case["req_out"]), " ".join(str(w) for w in case["req_out"])))
    f.write("rax %d\n" % case["rax"])
    f.write("ctx %s\n" % hexs(case["ctx"]))
    for name, data in case["probes"]:
        f.write("probe %s %s\n" % (name, hexs(data)))
    for c in case["calls"]:
        ar = c["args"]
        f.write("call %#x %x %x %x %s %s %x %s %s %s %s\n" % (
            c["site"], c["target"] or 0, c["rax"], c["xmm0"], " ".join("%x" % ar[k] for k in ARG_REGS),
            " ".join("%x" % x for x in ar["xmm"]), ar["stack0"],
            " ".join(hexs(ar["mem"][k]) for k in ARG_REGS), " ".join(hexs(c["after"][k]) for k in ARG_REGS),
            hexs(c["retmem"]), hexs(c["extra"])))
    f.write("end\n")


def draws(spec, rng):
    """-> [(target, nbytes, value)]: target is a request word index (int, 4 bytes) or an address expression (str)."""
    out = []
    for item in spec:
        target, size = (item["word"], 4) if "word" in item else (item["mem"], item["len"])
        if "choices" in item:
            out.append((target, size, rng.choice(item["choices"])))
        else:
            lo, hi = item["f32"]
            out.append((target, size, struct.unpack("<I", struct.pack("<f", rng.uniform(lo, hi)))[0]))
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("snap")
    ap.add_argument("hooks")
    ap.add_argument("out")
    ap.add_argument("--entry", type=lambda s: int(s, 0), default=None)
    ap.add_argument("--req-reg", default="rdx")
    ap.add_argument("--req-words", type=int, default=0x1B0)
    ap.add_argument("--ctx-reg", default="rcx")
    ap.add_argument("--ctx-dump", type=int, default=0)
    ap.add_argument("--probe", action="append", default=[])
    ap.add_argument("--ret-dump", action="append", default=[])
    ap.add_argument("--perturb")
    ap.add_argument("--count", type=int, default=0)
    ap.add_argument("--seed", type=int, default=7)
    ap.add_argument("--sites", action="store_true")
    a = ap.parse_args()
    call = ft.load_recorded_call(a.snap)
    entry = a.entry or call["entry"]
    orc = Oracle(a.snap, a.hooks, entry, a)
    if a.sites:
        for addr, ins in sorted(orc.sites.items()):
            print("%#x  %s %s" % (addr, ins.mnemonic, ins.op_str))
        return
    label = os.path.basename(a.snap.rstrip("/"))
    rng = random.Random(a.seed)
    spec = json.load(open(a.perturb)) if a.perturb else []
    with open(a.out, "a") as f:
        case, err = orc.run()
        if err:
            sys.exit("%s: %s" % (label, err))
        write_case(f, case, label, "real", entry)
        for i in range(a.count):
            case, err = orc.run(draws(spec, rng))
            if err:
                print("%s #%d dropped: %s" % (label, i, err), file=sys.stderr)
                continue
            write_case(f, case, "%s#%d" % (label, i), "perturbed", entry)


if __name__ == "__main__":
    main()
