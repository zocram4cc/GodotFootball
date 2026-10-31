#!/usr/bin/env python3
"""proc_camera_cases.py: the inputs and PES's answer of each captured procedural-camera call (0x141F7BFE0), as JSON.

For each rendezvous capture of the hook proc_camera_141F7BFE0 it reads, from the snapshot's memory at the call, what the
engine is fed: the FixDemoCamera's camera record (the 284 bytes behind camera+0x130), the player time the time setter 0x141F7B4A0
left in camera+0x214, and the CAMERA_TARGET_02nn records the camera's two target tables point at (camera+0x10+0x38+8*slot ->
handle +0x10 -> record, 0x30 bytes). By emulating PES's own bytes (frame_trace.run_recorded) it reads the answer: the params
block 0x1437F4A40 (0x60 bytes) after the call. tests/pes_proc_camera_test.cpp feeds the inputs to GF's rig and compares.
Output is PES-derived: keep it in scratch, never commit it.

Usage: GF_DIFF_PYTHON=<venv python> PES_EXE=<pes_full.bin> proc_camera_cases.py --cfg HOOKS_CFG --out FILE SNAP_DIR...
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

PARAMS_VA, PARAMS_BYTES = 0x1437F4A40, 0x60
CAMERA_BYTES, RECORD_BYTES, RECORD_PTR, TIME_OFF = 0x228, 0x11C, 0x130, 0x214
TABLE_OFF, TARGETS, TARGET_BYTES = 0x10 + 0x38, 28, 0x30


def q(sm, va):
    return struct.unpack("<Q", sm.read(va, 8))[0]


def case(snap, orig, scratch):
    sm = fs.SnapMem(snap, restore=orig)
    cam = ft.load_recorded_call(snap)["gpr"]["rcx"]
    camera = sm.read(cam, CAMERA_BYTES)
    record = sm.read(struct.unpack("<Q", camera[RECORD_PTR:RECORD_PTR + 8])[0], RECORD_BYTES)
    targets = []
    for slot in range(TARGETS):
        entry = q(sm, cam + TABLE_OFF + 8 * slot)
        targets.append(sm.read(q(sm, q(sm, entry + 0x10) + 0x10), TARGET_BYTES).hex())
    trace = {"out": scratch, "impure": {}, "stubs": ft.stubs_for_run(stub_table.load())}
    uc, err = ft.run_recorded(snap, orig=orig, trace=trace)
    if err is not None:
        return {"error": err}
    return {"camera": camera.hex(), "record": record.hex(), "targets": targets,
            "time_bits": struct.unpack("<I", camera[TIME_OFF:TIME_OFF + 4])[0],
            "engine_state": struct.unpack("<I", camera[0x128:0x12C])[0],
            "params": bytes(uc.mem_read(PARAMS_VA, PARAMS_BYTES)).hex()}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cfg", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("snaps", nargs="+")
    a = ap.parse_args()
    orig = ft.hook_bytes(a.cfg)
    out = {}
    for i, snap in enumerate(a.snaps):
        out[os.path.basename(snap.rstrip("/"))] = case(snap, orig, a.out + ".scratch%d" % i)
    with open(a.out, "w") as f:
        json.dump(out, f, indent=1)
    print("wrote", a.out)


if __name__ == "__main__":
    main()
