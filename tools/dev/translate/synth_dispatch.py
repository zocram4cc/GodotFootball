#!/usr/bin/env python3
"""synth_dispatch.py: the press dispatcher FUN_140535C60 as a call over a real teamsel_1404FB2A0 capture's memory.

The selector capture (rv.hook teamsel_1404FB2A0, rcx = the team-tick object Y) is taken inside FUN_1404F6C30, which calls the dispatcher a few
instructions later (0x1404F7955); every operand of that call is a field of Y or of the caller's own frame, so the call is rebuilt from the
capture's memory: rcx = Y+0x4200, rdx = a frame {Y+0x6D48 word, Y+0x62B4, Y+8 (the side), flag byte}, r8 = the word at Y+0x6D50 (T),
r9 = the selector's AL (the caller's SIL: 0 or 1, both cases drawn). The state is PES's, the call is rebuilt: the capture says SYNTHETIC and the
program that runs it is PES's bytes (snapdiff compares the translation with it). The state is the one at the selector, the in-call stand-in of
the order the dispatcher ran in (it runs after the selector wrote T+0x78); a different T+0x78 is a different valid state, not a wrong one.

Usage: python3 synth_dispatch.py SRC_CAPTURE_UNPACKED OUT_DIR [--cases N] [--seed S]
"""
import argparse
import json
import os
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import synth  # noqa: E402

DISPATCH = "0x140535C60"
OFF_FRAME, OFF_T, OFF_SIDE = 0x6D48, 0x6D50, 0x8        # Y fields the caller reads
OFF_CALLEE_OBJ, OFF_SIDE_CTX = 0x4200, 0x62B4           # rcx = Y+0x4200; frame word 1 = Y+0x62B4
OFF_FLAG_A, OFF_FLAG_B = 0x6C48, 0x6C54                 # the frame's flag byte is (Y+0x6C48 != 0 or Y+0x6C54 != 0)
FRAME_ABOVE_RSP = 0x100                                 # inside the caller's dead frame (the call never returns into it)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("src")
    ap.add_argument("out")
    ap.add_argument("--cases", type=int, default=4)
    ap.add_argument("--seed", type=int, default=7)
    a = ap.parse_args()
    idx = json.load(open(os.path.join(a.src, "index.json")))
    if idx["rv"]["hook"] != "teamsel_1404FB2A0":
        raise SystemExit("not a teamsel capture: %s" % idx["rv"]["hook"])
    reg = dict(zip(synth.GPR_ORDER, struct.unpack("<49Q", open(os.path.join(a.src, "call.bin"), "rb").read())[:16]))
    y, rsp = reg["rcx"], reg["rsp"]

    def rd(va, n):
        return synth.read_capture(a.src, idx, va, n)

    def u64(va):
        return struct.unpack("<Q", rd(va, 8))[0]

    def u32(va):
        return struct.unpack("<I", rd(va, 4))[0]

    frame = rsp + FRAME_ABOVE_RSP
    flag = 1 if (u32(y + OFF_FLAG_A) or u32(y + OFF_FLAG_B)) else 0
    fields = [
        {"name": "rcx", "at": "rcx", "type": "u64", "values": [y + OFF_CALLEE_OBJ]},
        {"name": "rdx", "at": "rdx", "type": "u64", "values": [frame]},
        {"name": "r8", "at": "r8", "type": "u64", "values": [u64(y + OFF_T)]},
        {"name": "r9", "at": "r9", "type": "u64", "values": [0, 1]},
        {"name": "f_ctx", "at": "va:%#x" % frame, "type": "u64", "values": [u64(y + OFF_FRAME)]},
        {"name": "f_obj", "at": "va:%#x" % (frame + 8), "type": "u64", "values": [y + OFF_SIDE_CTX]},
        {"name": "f_side", "at": "va:%#x" % (frame + 0x10), "type": "u32", "values": [u32(y + OFF_SIDE)]},
        {"name": "f_flag", "at": "va:%#x" % (frame + 0x14), "type": "u8", "values": [flag]},
    ]
    spec = {"label": "dispatch_%s" % os.path.basename(a.src.rstrip("/")), "seed": a.seed, "count": a.cases, "as": DISPATCH, "fields": fields}
    os.makedirs(a.out, exist_ok=True)
    spec_path = os.path.join(a.out, "spec.json")
    json.dump(spec, open(spec_path, "w"), indent=1)
    subprocess.check_call([sys.executable, os.path.join(HERE, "synth.py"), a.src, a.out, "--spec", spec_path])


if __name__ == "__main__":
    main()
