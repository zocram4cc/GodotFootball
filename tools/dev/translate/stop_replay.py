#!/usr/bin/env python3
"""STOP entry and turn-stop selector replay (MoveStepCell2): emulate recorded PES calls of FUN_1406D5D60 (the STOP entry,
hook stop_entry_1406D5D60) and FUN_1406BA0D0 (the turn-stop selector, hook turnstop_sel_1406BA0D0) from their captured
entry states (frame_trace.run_recorded) and print what the ORIGINAL code did:
  selector: the (class, kind, bucket) it was called with, the (row, column) it passed to FUN_1406ABB30 and the motion and
            mirror it returned; checked against DAT_1425ddb50[kind + class * 8] and DAT_1425dd988[bucket] read from the image.
  STOP entry: req[0], req[0x1f], req[0x20] at entry, the arm it took, the arguments of the vt+0xE0 call (the selector) and
            req[0x20] / req[8] / req[0x25] after the call.
Usage: GF_DIFF_PYTHON=<venv python> PES_EXE=<pes_full.bin> python3 stop_replay.py [--hooks hooks.cfg] SNAP_DIR...
(each SNAP_DIR is a seqNNN directory with an unpacked mem.bin: capz.py unpack first, pack afterwards). Exit 1 when a call does
not return cleanly or the selector disagrees with the image tables."""
import argparse
import json
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import frame_trace as ft  # noqa: E402

SELECTOR, ABB30, ABD60, STOP_ENTRY = 0x1406BA0D0, 0x1406ABB30, 0x1406ABD60, 0x1406D5D60
ROW_TABLE, COLUMN_TABLE = 0x1425DDB50, 0x1425DD988
IMAGE_BASE = 0x140000000


def image_u32(image, va):
    with open(image, "rb") as f:
        f.seek(va - IMAGE_BASE)
        return struct.unpack("<I", f.read(4))[0]


def replay(snap, image, hooks):
    import unicorn.x86_const as xc
    call = ft.load_recorded_call(snap)
    entry = call["entry"]
    rec = {"snap": os.path.basename(snap.rstrip("/")), "entry": hex(entry), "selector_calls": [], "abb30": [], "abd60": []}
    state = {}

    def u32(uc, va):
        return struct.unpack("<I", bytes(uc.mem_read(va, 4)))[0]

    def rd(uc, name):
        return uc.reg_read(getattr(xc, "UC_X86_REG_" + name.upper()))

    def on_code(uc, address, size, user):
        if address == entry and entry == STOP_ENTRY and "req" not in state:
            req = rd(uc, "rdx")
            state["req"] = req
            rec["req_entry"] = {"kind": u32(uc, req), "class_1f": u32(uc, req + 0x7c), "w20": u32(uc, req + 0x80),
                                "w8": u32(uc, req + 0x20), "w25": u32(uc, req + 0x94), "w9": u32(uc, req + 0x24)}
        if address == SELECTOR:
            rsp = rd(uc, "rsp")
            cls, kind, bucket = u32(uc, rd(uc, "r8")), u32(uc, rd(uc, "r9")), u32(uc, u32_ptr(uc, rsp + 0x28))
            rec["selector_calls"].append({"class": cls, "kind": kind, "bucket": bucket, "out": rd(uc, "rdx")})
            state["sel_out"] = rd(uc, "rdx")
        elif address == ABB30:
            rec["abb30"].append({"row": u32(uc, rd(uc, "rdx")), "column": u32(uc, rd(uc, "r8")), "flag": rd(uc, "r9") & 0xff})
        elif address == ABD60:
            rec["abd60"].append({"row": u32(uc, rd(uc, "rdx")), "column": u32(uc, rd(uc, "r8"))})

    def u32_ptr(uc, va):
        return struct.unpack("<Q", bytes(uc.mem_read(va, 8)))[0]

    uc, err = ft.run_recorded(snap, orig=ft.hook_bytes(hooks) if hooks else None, on_code=on_code)
    rec["err"] = err
    if entry == STOP_ENTRY and "req" in state:
        req = state["req"]
        rec["req_exit"] = {"w20": u32(uc, req + 0x80), "w8": u32(uc, req + 0x20), "w25": u32(uc, req + 0x94),
                           "w39": u32(uc, req + 0xe4), "w1e": u32(uc, req + 0x78)}
    ok = err is None
    for c, a in zip(rec["selector_calls"], rec["abb30"]):
        want_row = image_u32(image, ROW_TABLE + 4 * (c["kind"] + c["class"] * 8))
        want_col = image_u32(image, COLUMN_TABLE + 4 * c["bucket"])
        c["row"], c["column"] = a["row"], a["column"]
        c["tables_agree"] = (a["row"], a["column"]) == (want_row, want_col)
        ok = ok and c["tables_agree"]
    rec["ok"] = ok
    return rec


def u32(uc, va):
    return struct.unpack("<I", bytes(uc.mem_read(va, 4)))[0]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--hooks", default="/mnt/Extra4/PES21/rendezvous/hooks.cfg")
    ap.add_argument("--image", default=os.environ.get("PES_EXE"))
    ap.add_argument("snaps", nargs="+")
    a = ap.parse_args()
    status = 0
    for snap in a.snaps:
        rec = replay(snap, a.image, a.hooks)
        print(json.dumps(rec))
        status |= 0 if rec["ok"] else 1
    return status


if __name__ == "__main__":
    sys.exit(main())
