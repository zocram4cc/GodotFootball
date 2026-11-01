#!/usr/bin/env python3
"""synth_call.py: a SYNTHETIC call state of one function, over a real (forward-emulated or captured) state's memory.

Some translated functions sit on branches no captured or forward-emulated state ever enters (the members of the dribble
proposal chain: the record fillers, the kind step's helpers). PES's own bytes still run them: this tool takes a state
directory (index.json, call.bin, mem.bin: a rendezvous capture, or synth_walk.py's output), keeps its memory and its stack,
and writes a new state whose call is `--entry` with the registers you set. frame_trace.py recorded / snapdiff.py replay it like
a capture: the harness runs PES's bytes from the entry, the translated body runs over the same pages, and every written byte and
the return value must match. The registers (and the --poke bytes) are chosen, not observed: label every case that uses one as
SYNTHETIC wherever it is cited (synth_members.py does, in results_dribble.json).

  --reg rcx=0x1234 --reg edx=3         a 64-bit register (rax..r15) or its low 32 bits (eax..r15d), zero-extended
  --xmm 0=1.5                          the low float32 of xmmN (the rest zero)
  --poke 0x20400000+0xA0=0a000000      bytes written into OUT's own copy of mem.bin at a VA (hex VA or rsp+hex, optional +offset)
No --poke: mem.bin is a link to the state's (nothing copied). With --poke: a sparse copy (`cp --sparse=always`), deleted by the
caller when the case is done (it is as large as the state's apparent size, mostly holes).

Usage: python3 synth_call.py STATE_DIR OUT_DIR --entry 0xVA [--reg ...] [--xmm ...] [--poke ...] [--hook-name NAME]
"""
import argparse
import json
import os
import shutil
import struct
import subprocess

CALL_REC = struct.Struct("<49Q")  # 16 GPRs, 16 XMM (low, high), the return address at [rsp]
GPR_ORDER = ("rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15")
LOW32 = {"e" + g[1:]: g for g in GPR_ORDER[:8]}
LOW32.update({"r%dd" % n: "r%d" % n for n in range(8, 16)})


def parse_regs(items):
    """{register: value} from NAME=VALUE strings (a 32-bit name writes the zero-extended low half)."""
    out = {}
    for item in items:
        name, value = item.split("=", 1)
        v = int(value, 0)
        if name in GPR_ORDER:
            out[name] = v & 0xFFFFFFFFFFFFFFFF
        elif name in LOW32:
            out[LOW32[name]] = v & 0xFFFFFFFF
        else:
            raise SystemExit("unknown register %r" % name)
    return out


def file_offset(index, va):
    for m in index["mappings"]:
        if m["va"] <= va < m["va"] + m["size"]:
            return m["off"] + va - m["va"], m["va"] + m["size"] - va
    raise SystemExit("poke address %#x is not mapped in the state" % va)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("state")
    ap.add_argument("out")
    ap.add_argument("--entry", required=True, type=lambda s: int(s, 16))
    ap.add_argument("--reg", action="append", default=[])
    ap.add_argument("--xmm", action="append", default=[])
    ap.add_argument("--poke", action="append", default=[])
    ap.add_argument("--hook-name", default=None)
    a = ap.parse_args()

    index = json.load(open(os.path.join(a.state, "index.json")))
    vals = list(CALL_REC.unpack(open(os.path.join(a.state, "call.bin"), "rb").read()))
    for name, v in parse_regs(a.reg).items():
        vals[GPR_ORDER.index(name)] = v
    for item in a.xmm:
        n, f = item.split("=", 1)
        vals[16 + 2 * int(n)] = struct.unpack("<I", struct.pack("<f", float(f)))[0]
        vals[17 + 2 * int(n)] = 0
    rsp = vals[GPR_ORDER.index("rsp")]
    vals[48] = struct.unpack("<Q", read_state(a.state, index, rsp, 8))[0]  # the return address stays the state's own

    os.makedirs(a.out, exist_ok=True)
    rv = index["rv"]
    rv.update(entry_va=a.entry, hook=a.hook_name or "synthetic_%X" % a.entry)
    index["label"] = os.path.basename(a.out.rstrip("/"))
    json.dump(index, open(os.path.join(a.out, "index.json"), "w"))
    open(os.path.join(a.out, "call.bin"), "wb").write(CALL_REC.pack(*vals))

    dest = os.path.join(a.out, "mem.bin")
    if os.path.lexists(dest):
        os.remove(dest)
    src = os.path.realpath(os.path.join(a.state, "mem.bin"))
    if not a.poke:
        os.symlink(src, dest)
        return
    subprocess.check_call(["cp", "--sparse=always", src, dest])
    with open(dest, "r+b") as f:
        for item in a.poke:
            where, hexbytes = item.split("=", 1)
            base, _, off = where.partition("+") if not where.startswith("rsp") else ("rsp", "+", where[4:])
            va = (rsp if base == "rsp" else int(base, 0)) + (int(off, 0) if off else 0)
            data = bytes.fromhex(hexbytes)
            pos, room = file_offset(index, va)
            if len(data) > room:
                raise SystemExit("poke at %#x runs past its mapping" % va)
            f.seek(pos)
            f.write(data)


def read_state(state, index, va, n):
    pos, _ = file_offset(index, va)
    with open(os.path.join(state, "mem.bin"), "rb") as f:
        f.seek(pos)
        return f.read(n)


if __name__ == "__main__":
    main()
