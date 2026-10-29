#!/usr/bin/env python3
"""danger_oracle.py: build the oracle directory of one danger-builder capture for tests/pes_danger_oracle_test.cpp.

The danger builder 0x142119390 (rcx = imageA, rdx = ctx, r8 = unit info BP+0x1C, r9 = state BP+0x78) is PES's one writer of
imageA+0x31C1, the danger flag. The directory holds PES-derived files (never committed): mem.bin (link to the capture),
maps.txt, ok_delta.bin, corpus.jsonl (the harness's reference row: its write log), pages.bin, teb.txt and manifest.tsv, the
call's live-in set (every byte the call reads that it has not written itself, found by replaying the capture with a memory
hook, trap_oracle.replay), classified against the records GF's window holds in PES layout. Same format as trap_oracle.py, with
four more record kinds: ctx (rdx), ma (*(ctx+0x18)), e (*(ctx+0xE0)) and bp (the carrier block, r8 - 0x1C).

manifest.tsv, one record per line, tab separated:
  R   reg  value                      entry registers (rcx rdx r8 r9 rsp) and the first stack words
  T   kind idx rel len                a live range inside a record the window holds (G, B, MI, pitch, side, anime, man, slot, ctx, ma, e, bp)
  H   va   len                        a live range outside every window record
  I   va   len                        a live range in the image's writable globals whose value differs from the file
  S   off  len                        a live range of the caller's stack, relative to the entry rsp

Usage: PES_EXE=<pes_full.bin> python3 danger_oracle.py SNAP CASE_DIR TRACE_DIR HOOKS OUT_DIR [walk]   (harness Python: unicorn)
  CASE_DIR  = snapdiff's <out>/gen/142119390/snap_<label> (corpus.jsonl, maps.txt, ok_delta.bin)
  TRACE_DIR = the frame_trace output of the capture (pool/pages.bin)
  walk      = the capture is a carrier-walk 0x14210F110 call (rcx = BP, rdx = {unit info, ctx}: the window's unit object), CASE_DIR
              = <out>/gen/14210F110/snap_<label>; the BP record is rcx, ctx is *(rdx+8), and *rdx's 16 bytes are the "unit" kind.
"""
import os
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import frame_trace as ft  # noqa: E402
import full_snapshot as fs  # noqa: E402
import trap_oracle as to  # noqa: E402

BP_BYTES = 0x4300   # the window's carrier block: unit info at +0x1C, state at +0x78, imageA at +0xEC (through its +0x31D0), the walk's lists to +0x4300
BP_UNIT = 0x1C      # r8 - BP
CTX_BYTES, MA_BYTES, E_BYTES = 0x100, 0x27000, 0x180760  # the window's records (pesworldrecord.hpp, pesevents.hpp)
UNIT_BYTES = 0x10   # the walk's rdx: {unit info VA, ctx VA}


def main():
    snap_dir, case_dir, trace_dir, hooks, out_dir = sys.argv[1:6]
    walk = len(sys.argv) > 6 and sys.argv[6] == "walk"
    image = open(os.environ["PES_EXE"], "rb").read()
    os.makedirs(out_dir, exist_ok=True)
    snap = fs.SnapMem(snap_dir)

    def u64(a):
        b = snap.read(a, 8)
        return int.from_bytes(b, "little") if b else 0

    regs, live = to.replay(snap_dir, hooks, out_dir)
    g = u64(to.G_CELL)
    ctx = u64(regs["rdx"] + 8) if walk else regs["rdx"]
    bp = regs["rcx"] if walk else regs["r8"] - BP_UNIT
    typed = [("G", 0, g, 0x2E58), ("bp", 0, bp, BP_BYTES), ("ctx", 0, ctx, CTX_BYTES),
             ("ma", 0, u64(ctx + 0x18), MA_BYTES), ("e", 0, u64(ctx + 0xE0), E_BYTES)]
    if walk:
        typed.append(("unit", 0, regs["rdx"], UNIT_BYTES))
    for kind, off, size in to.POINTERS:
        typed.append((kind, 0, u64(g + off), size))
    for kind, off, stride, count, size in to.TABLES:
        for k in range(count):
            typed.append((kind, k, u64(g + off + stride * k), size))
    typed = [t for t in typed if t[2]]

    def which(a):
        for kind, idx, base, size in typed:
            if base <= a < base + size:
                return kind, idx, a - base
        return None

    lines = ["R\t%s\t%#x" % (r, regs[r]) for r in to.REGS]
    for off in range(0, 0x80, 8):
        lines.append("R\tstk%#x\t%#x" % (off, u64(regs["rsp"] + off)))
    rsp = regs["rsp"]
    for lo, hi in to.ranges(b for b in live if b < to.IMG_LO or b >= to.IMG_HI):
        if rsp <= lo < rsp + to.STACK_SPAN:
            lines.append("S\t%#x\t%d" % (lo - rsp, hi - lo))
            continue
        a = lo
        while a < hi:  # one line per run of bytes in the same record (a merged range may cross a record's end)
            w = which(a)
            end = a + 1
            while end < hi and (which(end) or (None,))[:2] == (w or (None,))[:2]:
                end += 1
            lines.append("T\t%s\t%d\t%#x\t%d" % (w[0], w[1], w[2], end - a) if w else "H\t%#x\t%d" % (a, end - a))
            a = end
    for lo, hi in to.ranges(b for b in live if to.IMG_GLOBALS <= b < to.IMG_HI):
        if snap.read(lo, hi - lo) != image[lo - to.IMG_LO:hi - to.IMG_LO]:
            lines.append("I\t%#x\t%d" % (lo, hi - lo))
    open(os.path.join(out_dir, "manifest.tsv"), "w").write("\n".join(lines) + "\n")
    for name in ("maps.txt", "ok_delta.bin", "corpus.jsonl"):
        shutil.copy(os.path.join(case_dir, name), os.path.join(out_dir, name))
    shutil.copy(os.path.join(trace_dir, "pool", "pages.bin"), os.path.join(out_dir, "pages.bin"))
    link = os.path.join(out_dir, "mem.bin")
    if os.path.lexists(link):
        os.remove(link)
    os.symlink(os.path.join(os.path.abspath(snap_dir), "mem.bin"), link)
    open(os.path.join(out_dir, "teb.txt"), "w").write(hex(ft.trace_teb(trace_dir, snap)) + "\n")
    print("manifest: %d lines, %d typed records" % (len(lines), len(typed)))


if __name__ == "__main__":
    main()
