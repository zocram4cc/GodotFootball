#!/usr/bin/env python3
"""copy_livein.py CAPDIR: the pre-state of PES's ReplayHuman frame read (FUN_14217DEF0) on one capture, for the copy tests.

CAPDIR is an unpacked rendezvous capture (call.bin, index.json, mem.bin). The frame is replayed with PES's own bytes
(frame_trace.run_recorded). Writes into CAPDIR (PES-derived, never committed):
  maps.txt      the capture's VA runs, "va size off" hex per line (the snapshot memory the tests read through SnapView);
  livein.tsv    "P <page VA>": every 4 KB page the frame reads before it writes it, plus every page of each block the
                allocator answers (the blocks keep PES's contents: a recycled block carries its previous ones);
                "S <VA>": each palette-entry source the copy stores into the record (the rdx of FUN_1402647D0's xchg,
                in call order, zero skipped); "A <size> <VA>": the frame's outermost allocator answers in call order
                (the pool alloc FUN_1403FB410 and the malloc FUN_1403CF740, with the requested size and the block).
                "I <human> <guard> <active> <palette>": the call's ReplayFrameInputs read at the entry (human = R+0x28; the
                frame's guard, active byte and palette byte for that human).
  after.bin     PES's record palette entry (R+0x50, 0xA0 bytes) after the frame call: the copy tests' expected bytes.
Usage: PES_EXE=<pes_full.bin> copy_livein.py CAPDIR
"""
import collections
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import frame_trace as ft  # noqa: E402
import unicorn.x86_const as xc  # noqa: E402
from unicorn import UC_HOOK_MEM_READ  # noqa: E402

FRAME = 0x14217DEF0       # FUN_14217DEF0: the ReplayHuman frame read
SET_SOURCE = 0x1402647D0  # the xchg that stores the copy source into the record slot (rcx = slot, rdx = source)
POOL_ALLOC = 0x1403FB410  # the pool allocation (rdx = size)
MALLOC = 0x1403CF740      # the aligned allocation entry (rcx = size; a zero size is one byte)
IMAGE_BASE = 0x140000000
PAGE = 0x1000
PALETTE_ENTRIES_OFF = 0x80  # the unit palette's entries (one PALETTE_BYTES entry per palette index)
MANAGER_GLOBAL = 0x1436F5118  # the replay manager (the image global M: the manager object's pointer)
MANAGER_UNIT_OFF = 0x6A8     # the manager's unit
UNIT_PALETTE_OFF = 0x20      # the unit's palette pointer
RECORD_PALETTE_OFF = 0x50  # R+0x50..+0xF0: the copied palette entry (ReplayHumanRecord::bytes)
PALETTE_BYTES = 0xA0
HUMAN_STRIDE = 300        # the frame's human records (FUN_14217DEF0 strides by 300)
FRAME_GUARD_OFF = 0x30C   # the frame's word for a human: 0x7FFF marks the human absent
FRAME_PALETTE_OFF = 0x36B # the frame's palette byte for a human (0..0x26 are valid)


def frame_inputs(u, rec, lst, key):
    """The ReplayFrameInputs of one call, read from PES's memory at the frame's entry: (human, guard, active, palette)."""
    rd = lambda a, n: bytes(u.mem_read(a, n))
    human = int.from_bytes(rd(rec + 0x28, 4), "little")
    frames = int.from_bytes(rd(lst + 0x10, 8), "little")
    fr = frames + human * HUMAN_STRIDE
    return (human, int.from_bytes(rd(fr + FRAME_GUARD_OFF, 4), "little"), rd(rec + 0x18, 1)[0], rd(fr + FRAME_PALETTE_OFF, 1)[0],
            int.from_bytes(rd(key, 2), "little"))


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    cap = sys.argv[1]
    image_path = os.environ.get("PES_EXE")
    if not image_path:
        raise SystemExit("copy_livein: PES_EXE (the unpacked image) is required")
    with open(image_path, "rb") as f:
        img = f.read()
    entries = (FRAME, POOL_ALLOC, MALLOC)
    orig = {va: img[va - IMAGE_BASE:va - IMAGE_BASE + 15] for va in entries}

    index = json.load(open(os.path.join(cap, "index.json")))
    with open(os.path.join(cap, "maps.txt"), "w") as f:
        for m in index["mappings"]:
            if m.get("deferred") or m.get("off") is None:
                continue
            f.write("%x %x %x\n" % (m["va"], m["size"], m["off"]))

    first = {}
    reads = collections.Counter()
    sources = []
    answers = []        # (size, va) of the outermost allocations, in call order
    record = {}         # the frame's record (rcx at FUN_14217DEF0's entry): its bytes are the frame's pre-state
    pending = {}        # return address -> (size) of an outermost allocation in flight
    outer = {"depth": 0}
    hooked = {"done": False}

    def on_read(u, access, addr, size, value, _d):
        pg = addr & ~(PAGE - 1)
        reads[pg] += 1
        first.setdefault(pg, "R")

    def on_write(u, access, addr, size, value, _d):
        first.setdefault(addr & ~(PAGE - 1), "W")

    def on_code(u, addr, size, _d):
        if addr == FRAME and "va" not in record:
            record["va"] = u.reg_read(xc.UC_X86_REG_RCX)
            record["lst"] = u.reg_read(xc.UC_X86_REG_RDX)
            record["key"] = u.reg_read(xc.UC_X86_REG_R8)
        if not hooked["done"]:
            hooked["done"] = True
            u.hook_add(UC_HOOK_MEM_READ, on_read)
        if addr in pending:
            want = pending.pop(addr)
            outer["depth"] -= 1
            answers.append((want, u.reg_read(xc.UC_X86_REG_RAX)))
        if addr == SET_SOURCE:
            src = u.reg_read(xc.UC_X86_REG_RDX)
            if src:
                sources.append(src)
        if addr in (POOL_ALLOC, MALLOC):
            rsp = u.reg_read(xc.UC_X86_REG_RSP)
            ret = int.from_bytes(bytes(u.mem_read(rsp, 8)), "little")
            if addr == POOL_ALLOC:
                size = u.reg_read(xc.UC_X86_REG_RDX)
            else:
                size = u.reg_read(xc.UC_X86_REG_RCX) or 1
            if outer["depth"] == 0:
                pending[ret] = size
            outer["depth"] += 1

    _uc, err = ft.run_recorded(cap, orig=orig, budget=300000000, on_code=on_code, on_write=on_write,
                               models=ft.load_import_models())
    if err is not None:
        raise SystemExit("copy_livein: the frame's replay failed: %s" % err)
    record["in"] = frame_inputs(_uc, record["va"], record["lst"], record["key"])
    rq = lambda a: int.from_bytes(bytes(_uc.mem_read(a, 8)), "little")
    unit = rq(rq(MANAGER_GLOBAL) + MANAGER_UNIT_OFF)
    palette_entry = rq(unit + UNIT_PALETTE_OFF) + PALETTE_ENTRIES_OFF + record["in"][3] * PALETTE_BYTES
    with open(os.path.join(cap, "palette.bin"), "wb") as f:
        f.write(bytes(_uc.mem_read(palette_entry, PALETTE_BYTES)))
    with open(os.path.join(cap, "after.bin"), "wb") as f:
        f.write(bytes(_uc.mem_read(record["va"] + RECORD_PALETTE_OFF, PALETTE_BYTES)))
    live = set(p for p, k in first.items() if k == "R")
    for size, va in answers:
        for pg in range(va & ~(PAGE - 1), va + size, PAGE):
            live.add(pg)
    with open(os.path.join(cap, "livein.tsv"), "w") as f:
        f.write("# PES ReplayHuman frame pre-state (copy_livein.py): P = page, S = copy source, A = allocator answer\n")
        for p in sorted(live):
            f.write("P\t%x\n" % p)
        f.write("R\t%x\n" % record["va"])
        f.write("I\t%d\t%x\t%d\t%d\t%d\n" % record["in"])
        for s in sources:
            f.write("S\t%x\n" % s)
        for size, va in answers:
            f.write("A\t%x\t%x\n" % (size, va))
    print("pages", len(live), "copy sources", ["%x" % s for s in sources], "allocator answers", len(answers))


if __name__ == "__main__":
    main()
