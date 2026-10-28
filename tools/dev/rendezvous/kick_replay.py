#!/usr/bin/env python3
"""Kick oracle replay: run each kick capture's recorded call (kick_scatter_83CC30 or kick_postscale_8414A0)
over its snapshot (frame_trace.run_recorded) and dump, per capture:
  - PES's kicker record fields (ap = rcx) before and after the call;
  - the three record arguments (rdx, r8, r9: scatter Y/P/S, post-scale x/y/z) before and after;
  - every call to the soft-min FUN_1408413C0 inside the call: its four f32 arguments (xmm0..xmm3, PES's own
    factors) and its f32 result (xmm0 at the return address).

The output holds PES bytes: write it to scratch, never commit it.

Usage: PES_EXE=<unpacked pes_full.bin> <venv>/bin/python kick_replay.py --hooks <hooks.cfg> --out <file.json>
           [--tsv <file.tsv>] SNAP_DIR...
"""
import argparse
import json
import os
import struct
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "translate"))
import frame_trace as ft  # noqa: E402  (unicorn replay of one recorded call)
import full_snapshot as fs  # noqa: E402
import unicorn.x86_const as xc  # noqa: E402

# The kicker record (ap = rcx of kick_scatter_83CC30 / kick_postscale_8414A0): PES offset, struct format.
KICK_FIELDS = {
    "vtable": (0x0, "<Q"),
    "kind": (0x2752, "<h"),        # W1 init -1, W2..W6 set the kick kind (record-ids §1)
    "b2754": (0x2754, "<B"),
    "uVar14": (0x2755, "<B"),      # motion switch override (0xf/0xd/0x11 or 0)
    "firstTime": (0x2756, "<B"),   # W1 init 0xff, W2 sets AL
    "b2758": (0x2758, "<I"),       # W2..W6 set 0
    "window": (0x2764, "<f"),      # W1 init 720.0f; the scatter's first-time gate is < 360
    "heading": (0x247C, "<f"),     # body heading (Q's Δ term)
    "bodyTurn": (0xBE4, "<f"),     # body turn (Q's Δ term)
    "flags2460": (0x2460, "<I"),   # AI bit = bit 29
    "motionIface": (0xAB8, "<Q"),  # motion interface object (vtable 0x1425E3508 when resident)
}
RECORD_BYTES = 12  # a scatter / post-scale record: three f32 (scale, sign, amp) at +0, +4, +8
SOFT_MIN_VA = 0x1408413C0  # FUN_1408413c0: soft minimum over four factors (args xmm0..3, result xmm0)
REG_NAMES = {"scatter": ("Y", "P", "S"), "postscale": ("x", "y", "z")}


def read_fields(read, ap):
    out = {}
    for name, (off, fmt) in KICK_FIELDS.items():
        n = struct.calcsize(fmt)
        raw = read(ap + off, n)
        out[name] = None if raw is None else struct.unpack(fmt, bytes(raw))[0]
    return out


def read_record(read, va):
    raw = read(va, RECORD_BYTES)
    if raw is None:
        return None
    return list(struct.unpack("<3f", bytes(raw)))


def xmm_f32(uc, n):
    v = uc.reg_read(xc.UC_X86_REG_XMM0 + n)
    raw = bytes(v)[:4] if isinstance(v, (bytes, bytearray)) else (v & 0xFFFFFFFF).to_bytes(4, "little")
    return struct.unpack("<f", raw)[0]


def replay(snap_dir, cfg):
    with open(os.path.join(snap_dir, "index.json")) as f:
        rv = json.load(f)["rv"]
    hook = rv["hook"]
    kind = "scatter" if hook.startswith("kick_scatter") else "postscale"
    call = ft.load_recorded_call(snap_dir)
    gpr = call["gpr"]
    ap, args = gpr["rcx"], (gpr["rdx"], gpr["r8"], gpr["r9"])
    orig = ft.hook_bytes(cfg)
    sm = fs.SnapMem(snap_dir, restore=orig)
    pre_fields = read_fields(sm.read, ap)
    pre_recs = [read_record(sm.read, a) for a in args]

    softmin = []   # one entry per soft-min call: {"args": [4 f32], "out": f32}
    open_call = {"ret": None}

    def on_code(uc, address, size, data):
        if address == SOFT_MIN_VA:
            rsp = uc.reg_read(xc.UC_X86_REG_RSP)
            ret = struct.unpack("<Q", bytes(uc.mem_read(rsp, 8)))[0]
            open_call["ret"] = ret
            softmin.append({"args": [xmm_f32(uc, i) for i in range(4)], "out": None})
        elif open_call["ret"] is not None and address == open_call["ret"]:
            softmin[-1]["out"] = xmm_f32(uc, 0)
            open_call["ret"] = None

    t0 = time.time()
    uc, err = ft.run_recorded(snap_dir, orig=orig, models=ft.load_import_models(), on_code=on_code)
    dt = time.time() - t0

    def post_read(va, n):
        if not uc._ensure(va, n):
            return None
        return uc.mem_read(va, n)

    return {
        "seq": os.path.basename(snap_dir.rstrip("/")),
        "hook": hook,
        "kind": kind,
        "names": REG_NAMES[kind],
        "entry": hex(call["entry"]),
        "err": err,
        "seconds": round(dt, 1),
        "ap": hex(ap),
        "fields": {"pre": pre_fields, "post": read_fields(post_read, ap)},
        "records": {"pre": pre_recs, "post": [read_record(post_read, a) for a in args]},
        "softmin": softmin,
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--hooks", required=True, help="rv.dll hooks.cfg (its displaced bytes replace the patches)")
    ap.add_argument("--out", required=True, help="scratch JSON (PES-derived)")
    ap.add_argument("--tsv", default=None, help="scratch TSV of the soft-min calls: seq, call, a, b, c, d, out")
    ap.add_argument("snaps", nargs="+")
    a = ap.parse_args()
    rows = []
    for snap in a.snaps:
        row = replay(snap, a.hooks)
        rows.append(row)
        print("%s %s err=%s softmin=%d %.1fs" % (row["seq"], row["hook"], row["err"], len(row["softmin"]),
                                                  row["seconds"]), flush=True)
    with open(a.out, "w") as f:
        json.dump(rows, f, indent=1)
    if a.tsv:
        with open(a.tsv, "w") as f:
            f.write("seq\tcall\ta\tb\tc\td\tout\n")
            for row in rows:
                for i, s in enumerate(row["softmin"]):
                    vals = [repr(float(v)) for v in s["args"] + [s["out"]]]
                    f.write("%s\t%d\t%s\n" % (row["seq"], i + 1, "\t".join(vals)))


if __name__ == "__main__":
    main()
