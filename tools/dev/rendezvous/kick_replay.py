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
import math
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


# ---- situation factor Q (docs/re/pes-re-kick-kinds.md, docs/ports/pes-port-KickQ.md) -----------------------------
# One recorded scatter call replayed with optional input patches; every input Q reads and PES's Q (xmm13 at
# 0x14083E618, where every path of the Q block rejoins) are recorded as exact f32/int bits. A patch writes the field
# at the instruction that first reads it, so the call sees the changed value and nothing else moves.
Q_ENTRY = 0x14083CC30
Q_AP_FIELDS = {   # name: (offset, struct format)
    "kind": (0x2752, "<h"), "b2754": (0x2754, "<b"), "entry2755": (0x2755, "<b"), "first": (0x2756, "<b"),
    "window": (0x2764, "<f"), "flags": (0x2460, "<I"), "velX": (0x2480, "<f"), "velZ": (0x2488, "<f"),
    "stamina": (0x281C, "<f"), "heading": (0x247C, "<f"), "body": (0xBE4, "<f"),
}
HK_MOTION, HK_MOTION_GATE, HK_MOTION_DESC = 0x14083CD7D, 0x14083E036, 0x14083E419   # [rax] = the u16 motion id
HK_VEC, HK_TOUCH, HK_EFF, HK_A, HK_DESC, HK_Q = (0x14083CDCD, 0x14083CE36, 0x14083CF07, 0x14083CF11, 0x14083E42E,
                                                  0x14083E618)
Q_COLUMNS = ["kind", "b2754", "entry2755", "first", "window", "flags", "velX", "velZ", "stamina", "heading", "body",
             "launch0", "launch2", "a", "motion", "touch", "desc", "q", "ballY", "ball1x", "ball1z", "ball2x", "ball2z",
             "ead"]
Q_FLOATS = {"window", "velX", "velZ", "stamina", "heading", "body", "launch0", "launch2", "a", "ballY", "ball1x", "ball1z",
            "ball2x", "ball2z"}
HK_BALL, HK_EAD = 0x14083E44C, 0x14083E547   # E44C: rax = ball record 1 (ball+0xB0), xmm6 its +4 float; E547: al = FUN_1406EAD40
BALL_REC1, BALL_REC2 = 0xB0, 0xD8            # FUN_140A6CF70(ball, 1) / (ball, 2): x, height, z (f32) at +0, +4, +8


def f32_bits(v):
    return struct.unpack("<I", struct.pack("<f", v))[0]


def q_case(snap_dir, cfg, patch=None):
    """Replay one capture with `patch` ({field: value}, plus motion, touch, a, launch=(x, y, z)). Returns the
    inputs Q saw (read after the patch) and PES's Q as f32 bits; desc is the motion-descriptor byte when the
    in-flight branch (sub 2..12) ran, else None."""
    patch = dict(patch or {})
    call = ft.load_recorded_call(snap_dir)
    ap = call["gpr"]["rcx"]
    orig = ft.hook_bytes(cfg)
    rec = {"desc": None, "touch": None}

    def on_code(uc, address, size, data):
        rd = uc.reg_read
        if address == Q_ENTRY:
            for name, (off, fmt) in Q_AP_FIELDS.items():
                if name in patch:
                    uc._ensure(ap + off, 8)
                    uc.mem_write(ap + off, struct.pack(fmt, patch[name]))
        elif address in (HK_MOTION, HK_MOTION_GATE, HK_MOTION_DESC):
            if "motion" in patch:
                uc.mem_write(rd(xc.UC_X86_REG_RAX), struct.pack("<H", patch["motion"]))
            if address == HK_MOTION:
                rec["motion"] = struct.unpack("<H", bytes(uc.mem_read(rd(xc.UC_X86_REG_RAX), 2)))[0]
        elif address == HK_VEC:
            vec = struct.unpack("<Q", bytes(uc.mem_read(rd(xc.UC_X86_REG_RBP) + 0xC8, 8)))[0]
            uc._ensure(vec, 12)
            if "launch" in patch:
                uc.mem_write(vec, struct.pack("<3f", *patch["launch"]))
            rec["launch"] = list(struct.unpack("<3f", bytes(uc.mem_read(vec, 12))))
            for name, (off, fmt) in Q_AP_FIELDS.items():
                uc._ensure(ap + off, 8)
                rec[name] = struct.unpack(fmt, bytes(uc.mem_read(ap + off, struct.calcsize(fmt))))[0]
        elif address == HK_TOUCH:
            if "touch" in patch:
                uc.reg_write(xc.UC_X86_REG_RAX, int(patch["touch"]))
            rec["touch"] = rd(xc.UC_X86_REG_RAX) & 0xFF
        elif address == HK_EFF:
            rec["eff2755"] = rd(xc.UC_X86_REG_R14) & 0xFFFFFFFF
        elif address == HK_A:
            if "a" in patch:
                uc.reg_write(xc.UC_X86_REG_XMM0, f32_bits(patch["a"]))
            rec["a"] = xmm_f32(uc, 0)
        elif address == HK_DESC:   # rax = FUN_1406B0250's result; the ball records are written first, so PES reads them below
            ball = struct.unpack("<Q", bytes(uc.mem_read(rd(xc.UC_X86_REG_RBP) - 0x78, 8)))[0]
            for key, off in (("ball1", BALL_REC1), ("ball2", BALL_REC2)):
                if key in patch:
                    uc._ensure(ball + off, 12)
                    uc.mem_write(ball + off, struct.pack("<3f", *patch[key]))
            if "descv" in patch:
                uc.reg_write(xc.UC_X86_REG_RAX, int(patch["descv"]))
            rec["desc"] = rd(xc.UC_X86_REG_RAX) & 0xFFFFFFFF
        elif address == HK_BALL:   # xmm6 = the ball record's +4 float, loaded at 0x14083E447 (the in-flight branch)
            rec["ballY"] = xmm_f32(uc, 6)
            rax = rd(xc.UC_X86_REG_RAX)
            for key, base in (("ball1", 0), ("ball2", 0x28)):   # record 2 follows record 1 by the record stride 0x28
                x, _, z = struct.unpack("<3f", bytes(uc.mem_read(rax + base, 12)))
                rec[key + "x"], rec[key + "z"] = x, z
        elif address == HK_EAD:    # al = FUN_1406EAD40(ap, 0xB or 0xC): the action class record's bit
            if "ead" in patch:
                uc.reg_write(xc.UC_X86_REG_RAX, int(patch["ead"]))
            rec["ead"] = rd(xc.UC_X86_REG_RAX) & 0xFF
        elif address == HK_Q:
            rec["q"] = f32_bits(xmm_f32(uc, 13))

    uc, err = ft.run_recorded(snap_dir, orig=orig, models=ft.load_import_models(), on_code=on_code)
    rec["err"] = err
    return rec


def q_row(rec):
    """One TSV row: ints decimal, floats as the hex of their f32 bits, desc -1 when the in-flight branch did not run."""
    rec = dict(rec, launch0=rec["launch"][0], launch2=rec["launch"][2])
    out = []
    for c in Q_COLUMNS:
        v = rec.get(c)
        if c == "desc":
            v = -1 if v is None else v
        elif c == "touch":
            v = 0 if v is None else v   # the window block did not run: the touch state was never read
        elif c in ("ballY", "ball1x", "ball1z", "ball2x", "ball2z"):
            v = 0.0 if v is None else v  # the in-flight branch did not run: no ball record was read
        elif c == "ead":
            v = 0 if v is None else v    # FUN_1406EAD40 was not called
        if c == "q":   # already the f32 bits
            out.append("%08x" % v)
        else:
            out.append("%08x" % f32_bits(v) if c in Q_FLOATS else str(v))
    return "\t".join(out)


def kind_rows():
    """The kind table 0x14344E380 (16 bytes per row, row = kind), read from PES_EXE."""
    with open(os.environ["PES_EXE"], "rb") as f:
        f.seek(0x14344E380 - 0x140000000)
        return f.read(151 * 16)


def kinds_with(subs, classes=None):
    """Kinds whose row has a subclass (byte 12) in `subs` and, when given, a class (byte 13) in `classes`."""
    t = kind_rows()
    return [k for k in range(1, 151) if t[16 * k + 12] in subs and (classes is None or t[16 * k + 13] in classes)]


def kinds_with_subclass(subs):
    return kinds_with(subs)


KMH_PER_TICK_UNIT = 54.0 * 60.0 * 60.0 / 1000.0   # km/h per metre-per-tick: tick rate 0x14259D624 x 60 x 60 / 1000
MOTION_A = [0x9B0, 0x9B4, 0xA41, 0xA42, 0xA45]     # the motion-A set of the bucket override (kinds 0x2B / 0x3A)


def q_patches(count, seed, mode="random"):
    """Deterministic input sweep: every field Q reads takes values on both sides of each threshold it is tested
    against. The first 150 cases walk every kind of the table with nothing else changed (mode random).
    focus: only the bucket-driven subclasses 2..7 and 0xC with motion ids whose descriptor byte is not 1 and a first-time
    window always open. flight: the in-flight branch (descriptor patched to 1, subclass 2..12): ball heights and speeds on both
    sides of the knee, the ramp ends and the scalings. aim: the first-time window's thresholds (windows over the whole circle,
    exactly 360.0 included) on class 0..2 kinds, and kinds 0x2B/0x3A with each motion-A id."""
    import random
    rng = random.Random(seed)
    out = [{"kind": k} for k in range(1, 151)] if count and mode == "random" else []
    kinds = kinds_with({2, 3, 4, 5, 6, 7, 12})
    flight_kinds = kinds_with(set(range(2, 13)))
    aim_kinds = kinds_with({2, 3, 4, 5, 6, 7, 12}, {0, 1, 2})
    heights = [0.0, 0.1085, 0.3259, 0.3262, 0.5432, 0.5436, 0.6519, 0.6523, 0.8693, 0.8697, 1.0, 2.54]
    speeds = [0.0, 5.0, 9.99, 10.01, 15.0, 29.9, 30.1, 39.9, 40.1, 49.9, 50.1, 60.0]
    scales = [1.0, 1 / 0.85, 1 / 0.95, 1 / (0.85 * 0.95)]
    for _ in range(count):
        p = {
            "kind": rng.choice(kinds) if mode == "focus" else rng.randint(1, 150), "b2754": rng.choice([0, 9, 9, 2, 3]),
            "entry2755": rng.choice([0, 2, 4, 9, 0xD, 0xF, 0x11, 1, 3]), "first": rng.choice([-1, 0, 1, 1]),
            "window": (rng.uniform(0, 359.9) if mode == "focus"
                       else rng.choice([720.0, rng.uniform(0, 359.9), rng.uniform(0, 359.9)])),
            "flags": rng.choice([0, 0, 1 << 29]), "velX": rng.uniform(-0.6, 0.6), "velZ": rng.uniform(-0.6, 0.6),
            "stamina": rng.uniform(0, 100), "heading": rng.uniform(-400, 400), "body": rng.uniform(-200, 600),
            "launch": (rng.uniform(-30, 30), rng.uniform(0, 8), rng.uniform(-30, 30)),
            "a": rng.uniform(0, 1), "touch": rng.choice([0, 1]),
            "motion": (rng.choice([0, 0x500]) if mode == "focus" else rng.choice(
                [0, 0x9F1, 0x9BE, 0xA20, 0xA7D, 0x9B0, 0x9B4, 0xA41, 0xA42, 0xA45, 0x500])),
        }
        if mode == "flight":
            y = rng.choice(heights) if rng.random() < 0.7 else rng.uniform(0, 1.2)
            step = (rng.choice(speeds) * rng.choice(scales) if rng.random() < 0.7 else rng.uniform(0, 60)) / KMH_PER_TICK_UNIT
            ang = rng.uniform(0, 6.283185307)
            x, z = rng.uniform(-50, 50), rng.uniform(-30, 30)
            p.update({"kind": rng.choice(flight_kinds), "descv": 1, "ead": rng.choice([0, 1]), "window": 720.0,
                      "ball1": (x, y, z),
                      "ball2": (x + step * math.cos(ang), y + rng.uniform(-0.2, 0.2), z + step * math.sin(ang))})
        elif mode == "aim":
            p["descv"] = 2   # not the in-flight branch: only the window and the motion override vary
            if rng.random() < 0.5:
                p.update({"kind": rng.choice(aim_kinds), "first": rng.choice([0, 1]), "entry2755": rng.choice([0, 2, 4, 0xD]),
                          "window": rng.choice([360.0, rng.uniform(0, 359.99), rng.uniform(0, 359.99)]),
                          "motion": 0})
            else:
                p.update({"kind": rng.choice([0x2B, 0x3A]), "motion": rng.choice(MOTION_A), "window": 720.0,
                          "entry2755": rng.choice([0, 2, 4, 0xF, 0x11])})
        out.append(p)
    return out

def q_main(a):
    cfg_patches = q_patches(a.q_count, a.q_seed, a.q_mode)

    with open(a.q, "w") as f:
        f.write("seq\t" + "\t".join(Q_COLUMNS) + "\n")
        for snap in a.snaps:
            seq = os.path.basename(snap.rstrip("/"))
            for i, p in enumerate([{}] + cfg_patches):   # {} is the capture itself
                rec = q_case(snap, a.hooks, p)
                if rec["err"] or "q" not in rec:
                    print("%s case %d: no Q (%s)" % (seq, i, rec["err"]), flush=True)
                    continue
                f.write("%s\t%s\n" % (seq, q_row(rec)))
                f.flush()
            print("%s done" % seq, flush=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--hooks", required=True, help="rv.dll hooks.cfg (its displaced bytes replace the patches)")
    ap.add_argument("--out", default=None, help="scratch JSON (PES-derived)")
    ap.add_argument("--tsv", default=None, help="scratch TSV of the soft-min calls: seq, call, a, b, c, d, out")
    ap.add_argument("--q", default=None, help="scratch TSV of the situation factor Q: each capture, the 150-kind walk "
                    "and --q-count random input cases, inputs and PES's Q as f32 bits (replaces --out/--tsv)")
    ap.add_argument("--q-count", type=int, default=250)
    ap.add_argument("--q-seed", type=int, default=1)
    ap.add_argument("--q-focus", dest="q_mode", action="store_const", const="focus", default="random",
                    help="only the bucket-driven subclasses, descriptor != 1 motions")
    ap.add_argument("--q-flight", dest="q_mode", action="store_const", const="flight",
                    help="the in-flight branch: descriptor 1, ball heights and speeds around every threshold")
    ap.add_argument("--q-aim", dest="q_mode", action="store_const", const="aim",
                    help="first-time window thresholds and the motion-A bucket override")
    ap.add_argument("snaps", nargs="+")
    a = ap.parse_args()
    if a.q:
        return q_main(a)
    if not a.out:
        ap.error("--out or --q is required")
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
