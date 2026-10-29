#!/usr/bin/env python3
"""stamina_scan.py: every match::Player of a full capture, with his slot record's three hash-checked stamina cells raw.

One TSV line per player: capture, PM, slot, then the cells at slot record +0xC8 (tank cur), +0xE8 (tank max) and +0x108 (breath
reserve) as `va:hash:key:index:ring0:ring1:ring2:ring3` (hex; the layout of docs/re/pes-re-stamina-drain.md 1: u64 hash @+0, u32 key
@+8, u32 index @+0xC, u32 ring[4] @+0x10). The record is `*(G + 0x25F8 + 0x28 * slot)` (G_CELL's global, as trap_oracle.py finds it).
tests/pes_stamina_cells_test.cpp compares the GF port's cells to these, bit for bit. A slim capture drops the player objects' pages: a
capture with no reachable player prints nothing for it.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import full_snapshot as fs  # noqa: E402
import physique_scan as ps  # noqa: E402

G_CELL, P_OFF, SLOT_OFF, TABLE_OFF, TABLE_STRIDE = 0x1436F3DC8, 0x7D8, 0x2458, 0x25F8, 0x28
CELLS = (0xC8, 0xE8, 0x108)


def main():
    for cap in sys.argv[1:]:
        snap = fs.SnapMem(cap)
        name = os.path.basename(os.path.dirname(cap.rstrip("/"))) + "/" + os.path.basename(cap.rstrip("/"))
        u64 = lambda a: int.from_bytes(snap.read(a, 8) or bytes(8), "little")
        u32 = lambda a: int.from_bytes(snap.read(a, 4) or bytes(4), "little")
        g = u64(G_CELL)
        for pm in ps.players(snap):
            p = u64(pm + P_OFF)
            if p == 0 or snap.read(p + SLOT_OFF, 1) is None:
                continue
            slot = snap.read(p + SLOT_OFF, 1)[0]
            rec = u64(g + TABLE_OFF + TABLE_STRIDE * slot)
            cols = []
            for off in CELLS:
                c = rec + off
                if snap.read(c, 0x20) is None:
                    break
                cols.append("%x:%x:%x:%x:%s" % (c, u64(c), u32(c + 8), u32(c + 0xC),
                                                ":".join("%x" % u32(c + 0x10 + 4 * i) for i in range(4))))
            if len(cols) == len(CELLS) and rec != 0:
                print("%s\t%#x\t%d\t%s" % (name, pm, slot, "\t".join(cols)))


if __name__ == "__main__":
    main()
