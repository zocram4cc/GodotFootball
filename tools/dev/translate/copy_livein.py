#!/usr/bin/env python3
"""copy_livein.py CAPDIR: the pre-state of PES's ReplayHuman frame read (FUN_14217DEF0) on one capture, for the copy tests.

CAPDIR is an unpacked rendezvous capture (call.bin, index.json, mem.bin). The frame is replayed with PES's own bytes
(frame_trace.run_recorded). Writes into CAPDIR (PES-derived, never committed):
  maps.txt      the capture's VA runs, "va size off" hex per line (the snapshot memory the tests read through SnapView);
  livein.tsv    "P <page VA>" for every 4 KB page the frame reads before it writes (its pre-state), and
                "S <VA>" for each palette-entry source the copy stores into the record (the rdx of FUN_1402647D0's
                xchg, in call order, zero values skipped).
Usage: PES_EXE=<pes_full.bin> copy_livein.py CAPDIR
"""
import json
import os
import sys
import collections

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import frame_trace as ft  # noqa: E402
import unicorn.x86_const as xc  # noqa: E402
from unicorn import UC_HOOK_MEM_READ  # noqa: E402

FRAME = 0x14217DEF0       # FUN_14217DEF0: the ReplayHuman frame read
SET_SOURCE = 0x1402647D0  # the xchg that stores the copy source into the record slot (rcx = slot, rdx = source)
IMAGE_BASE = 0x140000000


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    cap = sys.argv[1]
    image_path = os.environ.get("PES_EXE")
    if not image_path:
        raise SystemExit("copy_livein: PES_EXE (the unpacked image) is required")
    with open(image_path, "rb") as f:
        img = f.read()
    orig = {FRAME: img[FRAME - IMAGE_BASE:FRAME - IMAGE_BASE + 15]}

    index = json.load(open(os.path.join(cap, "index.json")))
    with open(os.path.join(cap, "maps.txt"), "w") as f:
        for m in index["mappings"]:
            if m.get("deferred") or m.get("off") is None:
                continue
            f.write("%x %x %x\n" % (m["va"], m["size"], m["off"]))

    first = {}
    reads = collections.Counter()
    sources = []
    hooked = {"done": False}

    def on_read(u, access, addr, size, value, _d):
        pg = addr & ~0xFFF
        reads[pg] += 1
        first.setdefault(pg, "R")

    def on_write(u, access, addr, size, value, _d):
        first.setdefault(addr & ~0xFFF, "W")

    def on_code(u, addr, size, _d):
        if not hooked["done"]:
            hooked["done"] = True
            u.hook_add(UC_HOOK_MEM_READ, on_read)
        if addr == SET_SOURCE:
            src = u.reg_read(xc.UC_X86_REG_RDX)
            if src:
                sources.append(src)

    ft.run_recorded(cap, orig=orig, budget=300000000, on_code=on_code, on_write=on_write,
                    models=ft.load_import_models())
    live = sorted(p for p, k in first.items() if k == "R")
    with open(os.path.join(cap, "livein.tsv"), "w") as f:
        f.write("# PES ReplayHuman frame pre-state (copy_livein.py): P = page read before written, S = copy source\n")
        for p in live:
            f.write("P\t%x\n" % p)
        for s in sources:
            f.write("S\t%x\n" % s)
    print("pages read first", len(live), "copy sources", ["%x" % s for s in sources])


if __name__ == "__main__":
    main()
