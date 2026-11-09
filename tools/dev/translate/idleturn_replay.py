#!/usr/bin/env python3
"""IDLE_TURN entry replay (StepLand2): emulate recorded PES calls of FUN_1406D1390 (hook mv_1406D1390) from their captured entry
states (frame_trace.run_recorded) and compare what the ORIGINAL wrote with what the port's EnterIdleTurn
(src/pes/movestep_group_a.cpp) computes from the same inputs. Per call it prints the req words before and after, the FUN_1406ABB30
(row, column) the original looked up, and a verdict for each deterministic term:
  row     = DAT_1425dda30[req[0]] (or 0x54c for the replay action / kind 3 with the match stopped or MA+0x25930 set)
  column  = DAT_1425dd988[req[0xd] after the flip arm]
  words   = req[0x18] == req[3] after, req[0x2d] == 0, req[0x2e] == 1/6, req[0x38] byte 1, req[0x39] == 2, req[0x1e] == req[0], req[0x3c] byte 0
  step    = req[3] after == the clamped step of the facing toward req[3] before (<= 167.5), unless the flip arm ran
  flip    = when req[0xd] changed (4 <-> 9): req[7] after is -/+167.5 and req[3] after is facing +/- 167.5 (clamped by the step)
Usage: GF_DIFF_PYTHON=<venv python> PES_EXE=<pes_full.bin> python3 idleturn_replay.py [--hooks hooks.cfg] SNAP_DIR...
(each SNAP_DIR is a seqNNN directory with an unpacked mem.bin: capz.py unpack first, pack afterwards). Exit 1 on any disagreement."""
import argparse
import json
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import frame_trace as ft  # noqa: E402

ENTRY, ABB30, ABD60 = 0x1406D1390, 0x1406ABB30, 0x1406ABD60
IDLE_TURN_ROW, COLUMN_TABLE = 0x1425DDA30, 0x1425DD988
REPLAY_ROW = 0x54C
STATE_IDLE_TURN = 2
WORDS = 0x80  # req words 0..0x7f cover every word the entry touches (up to 0x51 / 0x4a)
IMAGE_BASE = 0x140000000


def image_f32(image, va):
    with open(image, "rb") as f:
        f.seek(va - IMAGE_BASE)
        return struct.unpack("<f", f.read(4))[0]


def image_u32(image, va):
    with open(image, "rb") as f:
        f.seek(va - IMAGE_BASE)
        return struct.unpack("<I", f.read(4))[0]


def f32(w):
    return struct.unpack("<f", struct.pack("<I", w))[0]


def replay(snap, image, hooks):
    call = ft.load_recorded_call(snap)
    rec = {"snap": os.path.basename(snap.rstrip("/")), "abb30": [], "abd60": []}
    st = {}

    def rd(uc, name):
        import unicorn.x86_const as xc
        return uc.reg_read(getattr(xc, "UC_X86_REG_" + name.upper()))

    def u32(uc, va):
        return struct.unpack("<I", bytes(uc.mem_read(va, 4)))[0]

    def block(uc, req):
        return list(struct.unpack("<%dI" % WORDS, bytes(uc.mem_read(req, 4 * WORDS))))

    def on_code(uc, address, size, user):
        if address == ENTRY and "req" not in st:
            st["req"] = rd(uc, "rdx")
            st["before"] = block(uc, st["req"])
        elif address == ABB30:
            rec["abb30"].append({"row": u32(uc, rd(uc, "rdx")), "column": u32(uc, rd(uc, "r8"))})
        elif address == ABD60:
            rec["abd60"].append({"row": u32(uc, rd(uc, "rdx")), "column": u32(uc, rd(uc, "r8"))})

    uc, err = ft.run_recorded(snap, orig=ft.hook_bytes(hooks) if hooks else None, on_code=on_code, models=ft.load_import_models())
    rec["err"] = err
    b, a = st["before"], block(uc, st["req"])
    rec["before"], rec["after"] = b, a
    kind = b[0]
    why = []
    row_t = image_u32(image, IDLE_TURN_ROW + 4 * kind)
    col_t = image_u32(image, COLUMN_TABLE + 4 * a[0xD])
    got = rec["abb30"][-1] if rec["abb30"] else None
    if got is None:
        why.append("no FUN_1406ABB30 call")
    else:
        if got["column"] != col_t:
            why.append("column %d != DAT_1425dd988[req[0xd]=%d] = %d" % (got["column"], a[0xD], col_t))
        if got["row"] not in (row_t, REPLAY_ROW):
            why.append("row 0x%x not DAT_1425dda30[%d] = 0x%x nor 0x54c" % (got["row"], kind, row_t))
        rec["row_kind"] = "table" if got["row"] == row_t else "replay"
    if a[0x18] != a[3]:
        why.append("req[0x18] != req[3]")
    if a[0x2D] != 0:
        why.append("req[0x2d] != 0")
    if abs(f32(a[0x2E]) - image_f32(image, 0x1425DDF50)) > 0:
        why.append("req[0x2e] != 1/6")
    if (a[0x38] & 0xFF) != 1 or a[0x39] != STATE_IDLE_TURN:
        why.append("state request is not IDLE_TURN")
    if a[0x1E] != kind:
        why.append("req[0x1e] != req[0]")
    if (a[0x3C] & 0xFF) != 0:
        why.append("req[0x3c] byte != 0")
    flipped = a[0xD] != b[0xD]
    rec["flipped"] = flipped
    clamp = image_f32(image, 0x1425DDFC8)
    if flipped:
        want_err = image_f32(image, 0x1425DDFF8) if a[0xD] == 4 else clamp
        if f32(a[7]) != want_err:
            why.append("flip: req[7] %r != %r" % (f32(a[7]), want_err))
    else:
        # the step: facing -> req[3] before, at most 167.5 degrees, folded into [0, 360)
        facing, look = f32(b[2]), f32(b[3])
        d = (look % 360.0) - (facing % 360.0)
        d = d - 360.0 if d > 180.0 else (d + 360.0 if d < -180.0 else d)
        step = max(-clamp, min(clamp, d))
        want = (facing % 360.0 + step) % 360.0
        if abs(f32(a[3]) - want) > 1e-2:
            why.append("step: req[3] %r != %r" % (f32(a[3]), want))
    rec["why"] = why
    rec["ok"] = err is None and not why
    return rec


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--hooks", default=os.environ.get("RV_HOOKS_CFG"))
    ap.add_argument("--image", default=os.environ.get("PES_EXE"))
    ap.add_argument("snaps", nargs="+")
    a = ap.parse_args()
    status = 0
    for snap in a.snaps:
        rec = replay(snap, a.image, a.hooks)
        out = {k: v for k, v in rec.items() if k not in ("before", "after")}
        out["changed"] = {hex(i): [hex(x), hex(y)] for i, (x, y) in enumerate(zip(rec["before"], rec["after"])) if x != y}
        print(json.dumps(out))
        status |= 0 if rec["ok"] else 1
    return status


if __name__ == "__main__":
    sys.exit(main())
