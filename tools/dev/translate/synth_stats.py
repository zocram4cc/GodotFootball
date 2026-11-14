#!/usr/bin/env python3
"""synth_stats.py: the match-statistics block constructor FUN_1408C6B80 as a call over a real dispatcher capture's memory.

The press dispatcher's defence pass (0x140535650) reads each side's statistics block at E + 0x750D0 + side * 0x59650 (E = the event store,
*(ctx + 0xE0); FUN_140A3E8A0 returns E + 0x750D0). The block holds PES's anti-tamper values: nine protected cells at +0x59530 + 0x20 * k (a
salted FNV hash the reader FUN_1408C9190 re-computes, aborting on a mismatch), written by the constructor FUN_1408C6B80 -> the reset
FUN_1408CA340. PES runs the constructor once at match load, before any capture window; its call is rebuilt here over the capture's memory:
rcx = the block of one side (the block is the capture's real one: the constructor overwrites it). The program that runs it is PES's bytes
(snapdiff compares the translation with it); the state is PES's, the call is rebuilt: the record says SYNTHETIC.

Usage: python3 synth_stats.py SRC_CAPTURE_UNPACKED OUT_DIR   (one state per side)
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

CTOR = "0x1408C6B80"
CTX_EVENTS_OFFSET = 0xE0          # *(ctx + 0xE0) = E (FUN_140535650: FUN_140A3E8A0(*(*param_1 + 0xE0)))
STATS_OFFSET = 0x750D0            # FUN_140A3E8A0: E + 0x750D0
STATS_STRIDE = 0x59650            # FUN_140535650: side * 0x59650


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("src")
    ap.add_argument("out")
    a = ap.parse_args()
    idx = json.load(open(os.path.join(a.src, "index.json")))
    if idx["rv"]["hook"] != "press_dispatch_535C60":
        raise SystemExit("not a press_dispatch_535C60 capture: %s" % idx["rv"]["hook"])
    reg = dict(zip(synth.GPR_ORDER, struct.unpack("<49Q", open(os.path.join(a.src, "call.bin"), "rb").read())[:16]))

    def u64(va):
        return struct.unpack("<Q", synth.read_capture(a.src, idx, va, 8))[0]

    ctx = u64(reg["rdx"])          # the argument struct's first word
    events = u64(ctx + CTX_EVENTS_OFFSET)
    os.makedirs(a.out, exist_ok=True)
    for side in range(2):
        block = events + STATS_OFFSET + side * STATS_STRIDE
        spec = {"label": "stats_ctor_side%d" % side, "seed": 7, "count": 1, "as": CTOR,
                "fields": [{"name": "rcx", "at": "rcx", "type": "u64", "values": [block]}]}
        path = os.path.join(a.out, "spec_side%d.json" % side)
        json.dump(spec, open(path, "w"), indent=1)
        out = os.path.join(a.out, "side%d" % side)
        subprocess.check_call([sys.executable, os.path.join(HERE, "synth.py"), a.src, out, "--spec", path])


if __name__ == "__main__":
    main()
