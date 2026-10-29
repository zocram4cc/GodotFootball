#!/usr/bin/env python3
"""scene_start_oracle.py: PES's placement-mode switch of the FixDemo scene start (0x141F5CD00), run from real captures.

For each rendezvous capture of the scene-start hook (fixdemo_scene_start_141F5CD00), emulate PES's own bytes from the
recorded entry (frame_trace.run_recorded) up to the switch's join (0x141F5D211, just before the RenderPlugin call), and
read what the switch left in the player's stage default: pos player+0xAC0..0xAC8, rotY +0xACC, flip +0xAD0. Also read the
inputs the switch chose from: the step's placement mode (0x141F613A0 = step node +0x10), the scene anchor
(scene+0x110..+0x120) and the step node's own offset (node+0x4C..+0x5C).
Output: one JSON object per capture on stdout (and --out FILE). PES-derived: keep it in scratch, never commit it.

Usage: GF_DIFF_PYTHON=<venv python> PES_EXE=<pes_full.bin> scene_start_oracle.py --cfg HOOKS_CFG [--out FILE] SNAP_DIR...
"""
import argparse
import json
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.normpath(os.path.join(HERE, "..", "translate")))
import frame_trace as ft  # noqa: E402
import unicorn.x86_const as xc  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402

import stubs as stub_table  # noqa: E402

SWITCH_JOIN = 0x141F5D211  # 0x141F5CE7A's cases rejoin here (disassembly: after the last +0xAD0 store)
PLAYER_OFF = 0xE0          # scene+0xE0: the FixDemoPlayer (param_1[0x1c])
STEP_OFF = 0xE8            # scene+0xE8: the current step byte (param_1[0x1d])
ANCHOR_OFF = 0x110         # scene anchor pos 0x110..0x118, rotY 0x11C, flip 0x120
STAGE_OFF = 0xAC0          # player stage default pos 0xAC0..0xAC8, rotY 0xACC, flip 0xAD0
NODE_OFF = 0x4C            # step node: pos 0x4C..0x54, rotY 0x58, flip 0x5C; placement mode at +0x10


def u(uc, va, n):
    return bytes(uc.mem_read(va, n))


def f32(b):
    return struct.unpack("<f", b)[0]


def run(snap, orig, out):
    call = ft.load_recorded_call(snap)
    scene = call["gpr"]["rcx"]
    state = {}

    def on_join(uc, address, size, data):
        player = struct.unpack("<Q", u(uc, scene + PLAYER_OFF, 8))[0]
        state["player"] = player
        state["stage"] = u(uc, player + STAGE_OFF, 0x11)
        state["anchor"] = u(uc, scene + ANCHOR_OFF, 0x11)
        uc.emu_stop()

    def on_code(uc, address, size, data):
        if address == SWITCH_JOIN:
            on_join(uc, address, size, data)

    # The trace form switches on the import and stub models (RtlEnterCriticalSection, memmove, ...); no spec is impure,
    # so nothing is recorded under `out`.
    trace = {"out": out, "impure": {}, "stubs": ft.stubs_for_run(stub_table.load())}
    uc, err = ft.run_recorded(snap, orig=orig, trace=trace, on_code=on_code)
    if "stage" not in state:
        return {"snap": snap, "error": err or "join not reached"}
    st, an = state["stage"], state["anchor"]
    return {"snap": snap, "scene": hex(scene),
            "stage": {"pos": [f32(st[0:4]), f32(st[4:8]), f32(st[8:12])], "rotY": f32(st[0xC:0x10]), "flip": st[0x10]},
            "anchor": {"pos": [f32(an[0:4]), f32(an[4:8]), f32(an[8:12])], "rotY": f32(an[0xC:0x10]), "flip": an[0x10]}}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("snaps", nargs="+")
    ap.add_argument("--cfg", required=True)
    ap.add_argument("--out")
    ap.add_argument("--scratch", required=True, help="directory the (empty) trace is written to")
    a = ap.parse_args()
    orig = ft.hook_bytes(a.cfg)
    rows = [run(s, orig, a.scratch) for s in a.snaps]
    for r in rows:
        print(json.dumps(r), flush=True)
    if a.out:
        with open(a.out, "w") as f:
            json.dump(rows, f, indent=1)


if __name__ == "__main__":
    sys.exit(main())
