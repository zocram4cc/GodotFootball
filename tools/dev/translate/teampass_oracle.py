#!/usr/bin/env python3
"""teampass_oracle.py: build the oracle directory of one pass-target selector capture for tests/pes_teampass_oracle_test.cpp.

The selector 0x1404FB2A0 (rcx = the team brain Y; hooked as team_pass_4FB2A0) is the writer of the team record's pass target T+0x78.
The directory holds PES-derived files (never committed): mem.bin (link to the capture), maps.txt, ok_delta.bin, corpus.jsonl (the
harness's reference row: rax and the call's write log), pages.bin, teb.txt and manifest.tsv, the call's live-in set (every byte the call
reads that it has not written itself, found by replaying the capture with a memory hook, trap_oracle.replay), classified against the
records GF's window holds in PES layout (same format as danger_oracle.py; the kinds here are Y, ctx, ma, e, skip, roster, B, MI, fr, pitch,
side (the brain's own T, idx = its side), sideE (G's table copy), team, anime, man, slot).

Usage: PES_EXE=<pes_full.bin> python3 teampass_oracle.py SNAP CASE_DIR TRACE_DIR HOOKS OUT_DIR   (harness Python: unicorn)
  CASE_DIR  = snapdiff's <out>/gen/1404FB2A0/snap_<label> (corpus.jsonl, maps.txt, ok_delta.bin)
  TRACE_DIR = the frame_trace output of the capture (pool/pages.bin)
"""
import os
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import frame_trace as ft  # noqa: E402
import full_snapshot as fs  # noqa: E402
import trap_oracle as to  # noqa: E402

CTX_BYTES, MA_BYTES, E_BYTES = 0x100, 0x26040, 0x180760  # the window's records (pesworldrecord.hpp, pesevents.hpp)
Y_BYTES, SKIP_BYTES, ROSTER_BYTES = 0x6E00, 0xA0, 24 * 0x74
SIDE_BYTES, TEAM_BYTES = 0xB900, 0xA8 + 0x28 * 0x128
TABLES = (("sideE", 0x1928, 0x28, 2, SIDE_BYTES), ("team", 0x1978, 0x28, 2, TEAM_BYTES), ("anime", 0x19C8, 0x28, 26, 0x32C),
          ("man", 0x1DD8, 0x28, 26, 0xDC4), ("slot", 0x25F8, 0x28, 22, 0x2E50))
POINTERS = (("B", 0x40, 0x2960), ("MI", 0x270, 0x1780), ("fr", 0x360, 0x10), ("pitch", 0x2AA8, 0x48), ("roster", 0x180, ROSTER_BYTES),
            ("skip", 0x3B0, SKIP_BYTES))


def main():
    snap_dir, case_dir, trace_dir, hooks, out_dir = sys.argv[1:6]
    image = open(os.environ["PES_EXE"], "rb").read()
    os.makedirs(out_dir, exist_ok=True)
    snap = fs.SnapMem(snap_dir)

    def u64(a):
        b = snap.read(a, 8)
        return int.from_bytes(b, "little") if b else 0

    regs, live = to.replay(snap_dir, hooks, out_dir)
    g = u64(to.G_CELL)
    y = regs["rcx"]
    ctx = u64(y + 0x6D48)
    own = int.from_bytes(snap.read(y + 8, 4), "little")
    # PES keeps the tick's team record T (Y+0x6D50) beside the copy G's side table names (the two differ by one frame's writes, e.g.
    # T+0x218): the window has one record per side, so T is kind "side" (idx = the brain's side) and the table's entries are "sideE".
    typed = [("G", 0, g, 0x2E58), ("Y", 0, y, Y_BYTES), ("ctx", 0, ctx, CTX_BYTES), ("side", own, u64(y + 0x6D50), SIDE_BYTES),
             ("ma", 0, u64(ctx + 0x18), MA_BYTES), ("e", 0, u64(ctx + 0xE0), E_BYTES)]
    for kind, off, size in POINTERS:
        typed.append((kind, 0, u64(g + off), size))
    for kind, off, stride, count, size in TABLES:
        for k in range(count):
            typed.append((kind, k, u64(g + off + stride * k), size))
    typed = [t for t in typed if t[2]]

    def which(a):
        # The slot records sit 0x2B90 apart (slot 20 -> 21) but are typed 0x2E50 long, so a byte of slot 21 also lies in slot
        # 20's range: the record the byte belongs to is the nearest base below it among the ranges of the first match's kind.
        hits = [(kind, idx, base) for kind, idx, base, size in typed if base <= a < base + size]
        if not hits:
            return None
        kind, idx, base = max((h for h in hits if h[0] == hits[0][0]), key=lambda h: h[2])
        return kind, idx, a - base

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
