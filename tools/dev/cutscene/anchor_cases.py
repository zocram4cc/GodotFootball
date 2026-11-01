#!/usr/bin/env python3
"""anchor_cases.py: the inputs and PES's answer of each captured goal anchor call (0x140474F00), as one JSON file.

For each rendezvous capture of the anchor hook (cut_anchor_calc_474F00) it reads, from the snapshot's memory at the call,
the state the anchor reads (the goal DemoInfo A0, the match info MI, the field info, the scorer's man record and run
object, the scene row, the fps global) and, by emulating PES's own bytes (frame_trace.run_recorded), the outputs
(*r8 = pos, *r9 = rotY). tests/pes_goal_anchor_test.cpp feeds the inputs to GF's anchor and compares.
Output is PES-derived: keep it in scratch, never commit it.

Each argument is CALC_SNAP_DIR[:WRITER_SNAP_DIR]; the writer capture (cut_anchor_write_47D280, the call just before the calc's) adds
the writer's results: scene+0x110..0x118 pos, +0x11C rotY (wrapped), +0x120 flip.

Usage: GF_DIFF_PYTHON=<venv python> PES_EXE=<pes_full.bin> anchor_cases.py --cfg HOOKS_CFG --image PES_FULL_BIN --out FILE CALC[:WRITER]...
"""
import argparse
import json
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.normpath(os.path.join(HERE, "..", "translate")))
sys.path.insert(0, os.path.normpath(os.path.join(HERE, "..", "diffharness")))
import frame_trace as ft  # noqa: E402
import full_snapshot as fs  # noqa: E402
import stubs as stub_table  # noqa: E402

G_VA, DB_VA, FPS_VA = 0x1436F3DC8, 0x1436F4FB8, 0x14351AE54
ROW_BYTES = 0x3C


def writer_result(snap, orig, scratch):
    call = ft.load_recorded_call(snap)
    trace = {"out": scratch, "impure": {}, "stubs": ft.stubs_for_run(stub_table.load())}
    uc, err = ft.run_recorded(snap, orig=orig, trace=trace)
    if err is not None:
        return {"error": err}
    scene = struct.unpack("<Q", bytes(uc.mem_read(call["gpr"]["rcx"] + 0x1E0, 8)))[0]
    b = bytes(uc.mem_read(scene + 0x110, 0x11))
    return {"pos": list(struct.unpack("<3f", b[:12])), "rotY": struct.unpack("<f", b[12:16])[0], "flip": b[16]}


def case(snap, image, orig, scratch):
    m = fs.SnapMem(snap, image=image)
    q = lambda va: struct.unpack("<Q", m.read(va, 8))[0]
    d = lambda va: struct.unpack("<I", m.read(va, 4))[0]
    f = lambda va: struct.unpack("<f", m.read(va, 4))[0]
    f3 = lambda va: [f(va), f(va + 4), f(va + 8)]
    call = ft.load_recorded_call(snap)
    a0 = call["gpr"]["rcx"]
    g = q(G_VA)
    mi = q(g + 0x270)
    side, place = d(mi + 0x15C8), d(mi + 0x15CC)
    if d(mi + 0x1588) == 1:
        slot = d(mi + 0x1590)
    else:
        info = q(g + 0x1978 + side * 0x28)
        slot = d(info) * 11 + [i for i in range(11) if d(info + 4 + 4 * i) == place][0]
    man = q(g + 0x1DD8 + slot * 0x28)
    run = q(g + 0x21E8 + slot * 0x28)
    field = q(g + 0x2AA8)
    sid = d(a0 + 8)
    row = q(q(DB_VA) + 0x448) + ((sid & 0xFFFF) - 1) * ROW_BYTES
    trace = {"out": scratch, "impure": {}, "stubs": ft.stubs_for_run(stub_table.load())}
    uc, err = ft.run_recorded(snap, orig=orig, trace=trace)
    out = None
    if err is None:
        r8, r9 = call["gpr"]["r8"], call["gpr"]["r9"]
        pos = [struct.unpack("<f", bytes(uc.mem_read(r8 + 4 * i, 4)))[0] for i in range(3)]
        out = {"pos": pos, "rotY": struct.unpack("<f", bytes(uc.mem_read(r9, 4)))[0]}
    return {"snap": snap, "error": err, "sceneId": sid, "radius": f(a0 + 0xC), "eventPos": f3(a0 + 0x30),
            "mode1588": d(mi + 0x1588), "side": side, "place": place, "slot": slot,
            "pos": f3(man + 0x55C), "vel": f3(man + 0x574), "facing": f(man + 0x554), "moveMode": d(man + 0x5B0),
            "target": f3(run + 0x18), "half": [f(field), f(field + 4)], "fps": f(FPS_VA),
            "row": m.read(row, ROW_BYTES).hex(), "pes": out}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("snaps", nargs="+")
    ap.add_argument("--cfg", required=True)
    ap.add_argument("--image", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--scratch", required=True, help="directory the (empty) trace is written to")
    a = ap.parse_args()
    os.makedirs(a.scratch, exist_ok=True)
    orig = ft.hook_bytes(a.cfg)
    rows = []
    for arg in a.snaps:
        calc, _, writer = arg.partition(":")
        row = case(calc, a.image, orig, a.scratch)
        if writer:
            row["writer"] = writer_result(writer, orig, a.scratch)
        rows.append(row)
    with open(a.out, "w") as fh:
        json.dump(rows, fh, indent=1)
    for r in rows:
        print(json.dumps(r), flush=True)


if __name__ == "__main__":
    sys.exit(main())
