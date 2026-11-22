#!/usr/bin/env python3
"""passget_oracle.py: build the oracle directory of one ActionPassGet tick capture (0x14094E0E0) for tests/pes_passget_oracle_test.cpp.

The directory is trap_oracle.py's (PES-derived, never committed): mem.bin (link to the capture), maps.txt, ok_calls.bin, ok_delta.bin,
corpus.jsonl (the harness's reference row: rax and the call's write log, from a snapdiff run), pages.bin, teb.txt and manifest.tsv, the
live-in set of the call (every byte it reads that it has not written itself), classified against the records the window holds:

  R   reg  value                      entry registers (rcx rdx r8 r9 rsp)
  T   kind idx rel len                a live range inside a record the window holds: the action object `this` (rcx), `pc` (rdx), the
                                      command record `R` (r8), the side / anime / man / slot / B / MI / pitch / fr records, ctx (pc+0x38),
                                      `ma` (ctx+0x18) and `e` (ctx+0xE0)
  H   va   len                        a live range outside every window record (heap: loader data, thunks, the call's own stack)
  I   va   len                        a live range in the image's writable globals whose value differs from the file
  A   arg  value                      the u32 r9 points at (the action id the caller passes by pointer)

Usage: PES_EXE=<pes_full.bin> python3 passget_oracle.py SNAP CASE_DIR TRACE_DIR HOOKS OUT_DIR   (harness Python: unicorn)
"""
import os
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import frame_trace as ft  # noqa: E402
import full_snapshot as fs  # noqa: E402
import trap_oracle as to  # noqa: E402

THIS_BYTES, PC_BYTES, R_BYTES = 0x2B90, 0x50, 0x1A00
TEAM_TABLE, TEAM_BYTES = 0x1978, 0x4C30  # side table B (the team-info records), sized as the window's


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
    pc, this, rr, rsp = regs["rdx"], regs["rcx"], regs["r8"], regs["rsp"]
    ctx = u64(pc + 0x38)
    typed = [("this", 0, this, THIS_BYTES), ("pc", 0, pc, PC_BYTES), ("R", 0, rr, R_BYTES), ("ctx", 0, ctx, 0x100),
             ("ma", 0, u64(ctx + 0x18), 0x26040), ("e", 0, u64(ctx + 0xE0), 0x180760)]
    for kind, off, size in to.POINTERS:
        typed.append(({"B": "B", "MI": "MI", "fr": "fr", "pitch": "pitch"}[kind], 0, u64(g + off), size))
    for kind, off, stride, count, size in to.TABLES:
        for k in range(count):
            typed.append((kind, k, u64(g + off + stride * k), size))
    for k in range(2):
        typed.append(("team", k, u64(g + TEAM_TABLE + 0x28 * k), 0x30))
    typed = [t for t in typed if t[2]]
    order = {}
    for i, t in enumerate(typed):
        order.setdefault(t[0], i)

    def which(a):
        hits = [(kind, idx, base) for kind, idx, base, size in typed if base <= a < base + size]
        if not hits:
            return None
        kind = min((h[0] for h in hits), key=lambda k: order[k])
        kind, idx, base = max((h for h in hits if h[0] == kind), key=lambda h: h[2])
        return kind, idx, a - base

    lines = ["R\t%s\t%#x" % (r, regs[r]) for r in to.REGS]
    for lo, hi in to.ranges(b for b in live if b < to.IMG_LO or b >= to.IMG_HI):
        if rsp <= lo < rsp + to.STACK_SPAN:
            lines.append("S\t%#x\t%d" % (lo - rsp, hi - lo))
            continue
        w = which(lo)
        if w and which(hi - 1) and which(hi - 1)[:2] == w[:2]:
            lines.append("T\t%s\t%d\t%#x\t%d" % (w[0], w[1], w[2], hi - lo))
        else:
            lines.append("H\t%#x\t%d" % (lo, hi - lo))
    for lo, hi in to.ranges(b for b in live if to.IMG_GLOBALS <= b < to.IMG_HI):
        if snap.read(lo, hi - lo) != image[lo - to.IMG_LO:hi - to.IMG_LO]:
            lines.append("I\t%#x\t%d" % (lo, hi - lo))
    lines.append("A\targ\t%d" % int.from_bytes(snap.read(regs["r9"], 4), "little"))
    open(os.path.join(out_dir, "manifest.tsv"), "w").write("\n".join(lines) + "\n")
    for name in ("maps.txt", "ok_calls.bin", "ok_delta.bin", "corpus.jsonl"):
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
