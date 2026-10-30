#!/usr/bin/env python3
"""Kick motion oracle: PES's own motion pick, called as functions over a kick_scatter_83CC30 snapshot.

The Kick action start (FUN_140727230, docs/ports/pes-port-KickMotion.md) picks the kicker's motion in two steps:
  row    = FUN_14071E6F0(ap, request, kind, band, side, scale, 0, 0)  (the [kind][band][side] clip table + fix-ups)
  motion = FUN_14071E570(ap, row, col, flag)                          (Animetable cell (row, col) + player swaps)
and Q reads the descriptor byte FUN_1406B0250(table, motion, 0). This script calls those functions in unicorn with the
snapshot's memory and writes TSVs into --out (PES-derived: scratch only, never committed; the C++ tests read them through
PES_KICK_MOTION_DIR):
  capture.tsv   the capture's own kicker fields, motion id, descriptor, row and motion re-picked
  rows.tsv      FUN_14071E6F0 over (kind 1..150, band 0..11, side 0..1, request 8/9/10, scale grid), ap null and real
  motions.tsv   FUN_14071E570 over every populated row x col 0..17 x flag 0/1, with FUN_1406B0250 of the result
  desc.tsv      FUN_1406B0250 over every motion 0..0x1124 plus 0xE42, 0xF7C, 0x1125, 0x2000
  cliptable.hex the [kind][band][side] clip table bytes (manager+8, 0x1C50 bytes)
  bucket.tsv    FUN_140723DE0 over a (turn angle, kind, req) grid

Usage: PES_EXE=<unpacked pes_full.bin> <venv>/bin/python kick_motion_replay.py --hooks <hooks.cfg> --out DIR SNAP_DIR
"""
import argparse
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "translate"))
import frame_trace as ft  # noqa: E402
import full_snapshot as fs  # noqa: E402
import run as rn  # noqa: E402
import unicorn.x86_const as xc  # noqa: E402
from unicorn import UC_HOOK_MEM_READ_UNMAPPED, UC_HOOK_MEM_WRITE_UNMAPPED  # noqa: E402

FN_ROW, FN_MOTION, FN_BUCKET, FN_DESC, FN_TABLE = 0x14071E6F0, 0x14071E570, 0x140723DE0, 0x1406B0250, 0x1406B0ED0
MANAGER_GLOBAL = 0x1436F2378
CLIP_TABLE_BYTES = 0x1C50  # 151 kinds x 12 bands x 2 sides x s16
BUDGET = 5_000_000
AP_FIELDS = {"kind": (0x2752, "<h"), "band": (0x2754, "<b"), "col": (0x2755, "<b"), "side": (0x2756, "<b"),
             "scale": (0x2758, "<I")}
NO_ROW = 0xBE5


def f32_bits(v):
    return struct.unpack("<I", struct.pack("<f", v))[0]


def s32(v):
    return struct.unpack("<i", struct.pack("<I", v & 0xFFFFFFFF))[0]


class Snap:
    def __init__(self, snap_dir, cfg):
        self.call = ft.load_recorded_call(snap_dir)
        self.ap = self.call["gpr"]["rcx"]
        sm = fs.SnapMem(snap_dir, restore=ft.hook_bytes(cfg))
        self.uc = rn.new_x86_64()
        models = ft.load_import_models()
        models[("ucrtbase.dll", "sqrtf")] = bytes.fromhex("F30F51C0C3")  # sqrtss xmm0, xmm0; ret
        self.lazy = ft.LazyMap(sm, models)
        self.lazy.attach(self.uc)
        self.uc.hook_add(UC_HOOK_MEM_READ_UNMAPPED | UC_HOOK_MEM_WRITE_UNMAPPED,
                         lambda u, access, addr, size, value, data: self.lazy.handle(addr) or self.lazy._refuse(
                             u, self.lazy.unmapped_reason(addr) or "unmapped %#x" % addr))
        for va in (self.call["entry"], self.call["gpr"]["rsp"], self.call["teb"]):
            assert self.lazy.ensure(va, 8), hex(va)
        self.uc.reg_write(xc.UC_X86_REG_GS_BASE, self.call["teb"])
        self.uc.reg_write(xc.UC_X86_REG_EFLAGS, self.call["flags"])
        self.uc.reg_write(xc.UC_X86_REG_MXCSR, self.call["mxcsr"])
        self.ret = self.call["ret"]

    def rd(self, va, n):
        self.uc._ensure(va, n)
        return bytes(self.uc.mem_read(va, n))

    def wr(self, va, data):
        self.uc._ensure(va, len(data))
        self.uc.mem_write(va, data)

    def q(self, va):
        return struct.unpack("<Q", self.rd(va, 8))[0]

    def field(self, name):
        off, fmt = AP_FIELDS[name]
        return struct.unpack(fmt, self.rd(self.ap + off, struct.calcsize(fmt)))[0]

    def call_fn(self, entry, regs, xmm=None, stack=()):
        """One call under the Windows x64 convention. xmm = {n: f32 bits}; stack = the 32-bit words of the arguments
        past the fourth (each in its own 8-byte slot, from [rsp+0x28]). Returns rax."""
        uc = self.uc
        rsp = ((self.call["gpr"]["rsp"] - 0x2000) & ~0xF) - 8  # the snapshot's own stack, below the recorded frame
        self.wr(rsp, struct.pack("<Q", self.ret) + bytes(0x20))
        for i, w in enumerate(stack):
            self.wr(rsp + 0x28 + 8 * i, struct.pack("<Q", w & 0xFFFFFFFF))
        uc.reg_write(xc.UC_X86_REG_RSP, rsp)
        for reg in ft.REGS_RESET:
            uc.reg_write(getattr(xc, "UC_X86_REG_" + reg.upper()), 0)
        for reg, val in regs.items():
            uc.reg_write(rn.reg_const(reg), val)
        for n, bits in (xmm or {}).items():
            uc.reg_write(xc.UC_X86_REG_XMM0 + n, bits)
        uc._bad = None
        try:
            uc.emu_start(entry, self.ret, count=BUDGET)
        except Exception as e:
            raise RuntimeError("emu error %s rip %#x" % (e, uc.reg_read(xc.UC_X86_REG_RIP)))
        if uc._bad is not None or uc.reg_read(xc.UC_X86_REG_RIP) != self.ret:
            raise RuntimeError("call did not return: %s" % (uc._bad,))
        return uc.reg_read(xc.UC_X86_REG_RAX)

    def virtual(self, slot):
        iface = self.ap + 0xAB8
        return self.call_fn(self.q(self.q(iface) + slot), {"rcx": iface})

    def ap_motion(self):
        """The kicker's live motion id, the u16 behind ap+0xAB8's vtable slot +0x18 (what Q reads)."""
        return struct.unpack("<H", self.rd(self.virtual(0x18), 2))[0]

    def row(self, ap, request, kind, band, side, scale_bits):
        # FUN_14071E6F0(ap, request, kind, band, side, scale, flagA, flagB): the last four are stack arguments
        return s32(self.call_fn(FN_ROW, {"rcx": ap, "rdx": request, "r8": kind, "r9": band},
                                stack=(side, scale_bits, 0, 0)) & 0xFFFFFFFF)

    def motion(self, row, col, flag):
        return s32(self.call_fn(FN_MOTION, {"rcx": self.ap, "rdx": row & 0xFFFFFFFF, "r8": col, "r9": flag})
                   & 0xFFFFFFFF)

    def table(self):
        self.call_fn(FN_TABLE, {})
        return self.uc.reg_read(xc.UC_X86_REG_RAX)

    def descriptor(self, motion):
        return self.call_fn(FN_DESC, {"rcx": self.table(), "rdx": motion & 0xFFFFFFFF, "r8": 0}) & 0xFF

    def bucket(self, angle, kind, req):
        return s32(self.call_fn(FN_BUCKET, {"rdx": kind}, xmm={0: f32_bits(angle), 2: f32_bits(req)}) & 0xFFFFFFFF)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--hooks", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("snap")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    s = Snap(a.snap, a.hooks)
    path = lambda n: os.path.join(a.out, n)

    flds = {n: s.field(n) for n in AP_FIELDS}
    sub = s.virtual(0x28) & 0xFFFFFFFF
    motion = s.ap_motion()
    row = s.row(s.ap, sub, flds["kind"], flds["band"], flds["side"], flds["scale"])
    mi = s.q(s.q(0x1436F3DC8) + 0x270)  # match info: tick counter +0x1198, side +0x1554 (the fix-up gates of FUN_14071E6F0)
    tick = struct.unpack("<I", s.rd(mi + 0x1198, 4))[0]
    half = struct.unpack("<i", s.rd(mi + 0x1554, 4))[0]
    with open(path("capture.tsv"), "w") as f:
        f.write("kind\tband\tcol\tside\tscale\tsub\tmotion\tdesc\trow\trepick\ttick\thalf\n")
        f.write("\t".join(str(x) for x in (flds["kind"], flds["band"], flds["col"], flds["side"], flds["scale"], sub,
                                           motion, s.descriptor(motion), row, s.motion(row, flds["col"], 0), tick, half)) + "\n")

    table = s.rd(s.q(s.q(MANAGER_GLOBAL) + 8), CLIP_TABLE_BYTES)
    with open(path("cliptable.hex"), "w") as f:
        f.write(table.hex() + "\n")

    scales = [0.0] + [float(d) for d in range(-180, 181, 15)]
    rows = set()
    with open(path("rows.tsv"), "w") as f:
        f.write("apnull\trequest\tkind\tband\tside\tscale\trow\n")
        for kind in range(1, 151):
            for band in range(12):
                for side in range(2):
                    for request in (8, 9, 10):
                        for scale in scales if kind in (0x77, 0x78) else scales[:1]:
                            for apv in (0, s.ap):
                                r = s.row(apv, request, kind, band, side, f32_bits(scale))
                                if apv == 0 and scale == 0.0 and request == 8:
                                    rows.add(r)
                                f.write("%d\t%d\t%d\t%d\t%d\t%08x\t%d\n" % (apv == 0, request, kind, band, side,
                                                                           f32_bits(scale), r))
    with open(path("motions.tsv"), "w") as f:
        f.write("row\tcol\tflag\tmotion\tdesc\n")
        for r in sorted(rows):
            if r < 0 or r >= NO_ROW:
                continue
            for col in range(18):
                for flag in (0, 1):
                    m = s.motion(r, col, flag)
                    f.write("%d\t%d\t%d\t%d\t%d\n" % (r, col, flag, m, s.descriptor(m) if 0 <= m < 0x2000 else -1))
    with open(path("desc.tsv"), "w") as f:
        f.write("motion\tdesc\n")
        for m in list(range(0x1125)) + [0xE42, 0xF7C, 0x1125, 0x2000]:
            f.write("%d\t%d\n" % (m, s.descriptor(m)))
    with open(path("bucket.tsv"), "w") as f:
        f.write("angle\tkind\treq\tbucket\n")
        for kind in range(1, 151):
            for angle in [float(d) for d in range(-180, 181, 5)]:
                for req in (-1.0, 0.0, 1.0):
                    f.write("%08x\t%d\t%08x\t%d\n" % (f32_bits(angle), kind, f32_bits(req), s.bucket(angle, kind, req)))
    print("wrote", a.out, "distinct rows", len(rows))


if __name__ == "__main__":
    main()
