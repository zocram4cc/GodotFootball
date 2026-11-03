#!/usr/bin/env python3
"""physique_scan.py: every match::Player of a full capture, with the physique inputs and the scale PES computed from them.

For each match::Player (vtable 0x1426329A8, as trap_oracle.py finds one) the TSV line holds
  capture  PM  height_bits(PM+0x30)  block(PM+0x784, 0x44 bytes, hex)  P+0xA00 bits  G bits (*(*(P+0xA08)+0xF0)+0xC0)
where P = *(PM+0x7D8) (a heap object, not embedded). The block is the physique object FUN_141EB5810 reads (block[0] = height cm,
block[0x30] = the leg-length slider, byte block[0x40] = the "no scale" flag); the last two columns are what its root-bone arm
stored (docs/ports/pes-port-TrapOpen2.md). tests/pes_physique_test.cpp compares the GF port to those columns bit for bit.
A slim capture drops the player objects' pages: a capture with no reachable player prints nothing for it.
"""
import mmap
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import full_snapshot as fs  # noqa: E402

PM_VTABLE = 0x1426329A8
P_OFF, HEIGHT_OFF, BLOCK_OFF, BLOCK_BYTES = 0x7D8, 0x30, 0x784, 0x44
SCALE_OFF, CHAIN_OFF, CHAIN2_OFF, G_OFF = 0xA00, 0xA08, 0xF0, 0xC0


def players(snap):
    pat = PM_VTABLE.to_bytes(8, "little")
    mm = mmap.mmap(snap.fd, 0, prot=mmap.PROT_READ)
    for m in snap.maps:
        if "w" not in m["perm"] or m["deferred"]:
            continue
        at = mm.find(pat, m["off"], m["off"] + m["size"])
        while at >= 0:
            if (at - m["off"]) % 8 == 0:
                yield m["va"] + at - m["off"]
            at = mm.find(pat, at + 1, m["off"] + m["size"])


def main():
    for cap in sys.argv[1:]:
        snap = fs.SnapMem(cap)
        name = os.path.basename(os.path.dirname(cap.rstrip("/"))) + "/" + os.path.basename(cap.rstrip("/"))
        u64 = lambda a: int.from_bytes(snap.read(a, 8) or bytes(8), "little")
        for pm in players(snap):
            p = u64(pm + P_OFF)  # PM+0x7D8 holds the pointer to P
            if p == 0:
                continue
            block = snap.read(pm + BLOCK_OFF, BLOCK_BYTES)
            height = snap.read(pm + HEIGHT_OFF, 4)
            scale = snap.read(p + SCALE_OFF, 4)
            x = u64(p + CHAIN_OFF)
            y = u64(x + CHAIN2_OFF) if x else 0
            g = snap.read(y + G_OFF, 4) if y else None
            if None in (block, height, scale, g):
                continue
            print("%s\t%#x\t%#x\t%s\t%#x\t%#x" % (name, pm, int.from_bytes(height, "little"), block.hex(),
                                                  int.from_bytes(scale, "little"), int.from_bytes(g, "little")))


if __name__ == "__main__":
    main()
