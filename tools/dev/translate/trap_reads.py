#!/usr/bin/env python3
"""trap_reads.py: the exact loader-data bytes the kind-0 trap chain reads in captured calls, in logical coordinates.

For each capture directory (a rendezvous capture of 0x14079A470: call.bin, mem.bin, index.json) the call is replayed under
unicorn (trap_oracle.replay: every byte the call reads that it has not written itself), and the bytes that sit in loader data
are named by what they belong to, with the value the capture holds. <NAME>.reads.tsv, one byte per line, tab separated:

  M motion off byte          a byte of the 0x390-byte motion record `motion` (table base = *(*(0x1436F23C0)))
  D off byte                 a byte of the default record (*(table + 8))
  F motion kind idx off byte a byte read through the record's list pointer list(kind)[idx]: the raw file record
  C row col part byte        the Animetable buffer *(*(0x1436F2378)): part 0/1 = the s16 cell's bytes, 2 = the flag byte
  T off byte / G off byte    the motion-table singleton *(0x1436F23C0) / the anime-table manager *(0x1436F2378)
  R off byte                 the constant registry object g = *(0x1437F4808); A idx part byte: array entry idx of arr = *g
  E off byte                 the entry arr[0x3E]; P off byte: its payload *(entry + 8), the constant::Trap runtime object
  X va len byte              an image global read whose capture value is not the file's (one byte per line)
  O va len                   live bytes in none of the above (P's pointer targets, thread data; window records and the
                             stack are removed first)

<NAME>.full.tsv: `anime <hex>` (the whole Animetable buffer), `trap <hex>` (the Trap runtime object, 0x1D8 bytes) and
`rec motion kind idx hex` for every list entry of kinds 0, 1, 6, 13 of every motion (the raw file record behind the pointer).

Usage: PES_EXE=<pes_full.bin> python3 trap_reads.py HOOKS OUT_DIR CAPTURE_DIR...   (harness Python: unicorn)
Writes OUT_DIR/<pidNNN_seqNNN>.{reads,full}.tsv (PES-derived: never committed).
"""
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "rendezvous"))
import trap_oracle as to  # noqa: E402
from full_snapshot_rv import SnapMem  # noqa: E402

TABLE_CELL, MANAGER_CELL, REGISTRY_CELL = 0x1436F23C0, 0x1436F2378, 0x1437F4808
MOTION_SLOTS, MOTION_BYTES = 0x1125, 0x390
TRAP_ID = 0x3E
LIST_OFFSET = {0: 0x18, 1: 0x20, 2: 0x48, 3: 0x78, 4: 0x90, 5: 0xA0, 6: 0xC8, 7: 0xE0, 8: 0x100, 9: 0x110, 10: 0x1B0,
               11: 0x1B8, 12: 0x208, 13: 0x210}  # FUN_1406AE810's table
RECORD_BYTES = {0: 36, 1: 108, 2: 32, 3: 16, 4: 4, 5: 12, 6: 8, 7: 4, 8: 4, 9: 4, 10: 4, 11: 8, 12: 16, 13: 20}
OBJECT_SPAN = {"table_obj": 0xD8, "manager": 0x40, "obj": 0x40}  # generous read windows; only live bytes are kept
ANIMETABLE_CELLS = 0xBE5 * 0x12
ANIMETABLE_BYTES = ANIMETABLE_CELLS * 3  # s16 cells then one flag byte per cell (FUN_1406AA440)
TRAP_BYTES = 0x1D8  # constant::Trap


def write_reads(path, out):
    """One byte per line, tab separated: tag then the coordinates then the byte (see the module docstring)."""
    reg = out["registry"]
    rows = [("M", int(m), int(o), v) for m, d in out["motion_rec"].items() for o, v in d.items()]
    rows += [("D", int(o), v) for o, v in out["default_rec"].items()]
    rows += [("F",) + tuple(r) for r in out["file_rec"]] + [("C",) + tuple(r) for r in out["cells"]]
    rows += [("T", int(o), v) for o, v in out["table_obj"].items()] + [("G", int(o), v) for o, v in out["manager"].items()]
    rows += [("R", int(o), v) for o, v in reg["obj"].items()] + [("E", int(o), v) for o, v in reg["entry"].items()]
    rows += [("A", int(i), int(o), v) for i, d in reg["arr"].items() for o, v in d.items()]
    rows += [("P", int(o), v) for o, v in reg["payload"].items()]
    rows += [("X", a, n, v) for a, n, v in out["globals"]] + [("O", a, n) for a, n in out["other"]]
    with open(path, "w") as f:
        for r in rows:
            f.write("\t".join(str(x) for x in r) + "\n")


def main():
    hooks, out_dir = sys.argv[1], sys.argv[2]
    os.makedirs(out_dir, exist_ok=True)
    image = open(os.environ["PES_EXE"], "rb").read()
    for cap in sys.argv[3:]:
        name = "_".join(cap.rstrip("/").split("/")[-2:])
        snap = SnapMem(cap)

        def u64(a):
            b = snap.read(a, 8)
            return struct.unpack("<Q", b)[0] if b else 0

        def u8(a):
            b = snap.read(a, 1)
            return b[0] if b else None

        regs, live = to.replay(cap, hooks, out_dir)
        tbl, mgr, g = u64(TABLE_CELL), u64(MANAGER_CELL), u64(REGISTRY_CELL)
        base, dflt, buf = u64(tbl), u64(tbl + 8), u64(mgr)
        # the records GF's window holds (trap_oracle.py): G, P, the pointed records and the per-slot tables, and the stack
        gw = u64(to.G_CELL)
        window = [(gw, 0x2E58), (regs["rdx"], to.P_BYTES)]
        window += [(u64(gw + off), size) for _, off, size in to.POINTERS]
        window += [(u64(gw + off + stride * k), size) for _, off, stride, count, size in to.TABLES for k in range(count)]
        window = [(b, n) for b, n in window if b]
        live = {a for a in live if not any(b <= a < b + n for b, n in window)
                and not regs["rsp"] <= a < regs["rsp"] + to.STACK_SPAN}
        arr = u64(g)
        entry = u64(arr + 8 * TRAP_ID)
        payload = u64(entry + 8)
        out = {"capture": cap, "motion_rec": {}, "default_rec": {}, "file_rec": [], "cells": [], "table_obj": {}, "manager": {},
               "registry": {"obj": {}, "arr": {}, "entry": {}, "payload": {}}, "globals": [], "other": []}
        touched = set()
        rest = set()
        for a in sorted(live):
            if base <= a < base + MOTION_SLOTS * MOTION_BYTES:
                m, off = divmod(a - base, MOTION_BYTES)
                out["motion_rec"].setdefault(str(m), {})[str(off)] = u8(a)
                touched.add(m)
            elif dflt <= a < dflt + MOTION_BYTES:
                out["default_rec"][str(a - dflt)] = u8(a)
            elif buf <= a < buf + ANIMETABLE_BYTES:
                i = a - buf
                cell, byte = divmod(i, 2) if i < ANIMETABLE_CELLS * 2 else (i - ANIMETABLE_CELLS * 2, 2)  # byte 2: the flag byte
                out["cells"].append([cell // 0x12, cell % 0x12, byte, u8(a)])
            elif tbl <= a < tbl + OBJECT_SPAN["table_obj"]:
                out["table_obj"][str(a - tbl)] = u8(a)
            elif mgr <= a < mgr + OBJECT_SPAN["manager"]:
                out["manager"][str(a - mgr)] = u8(a)
            elif g <= a < g + OBJECT_SPAN["obj"]:
                out["registry"]["obj"][str(a - g)] = u8(a)
            elif arr <= a < arr + 8 * 0x100:
                out["registry"]["arr"].setdefault(str((a - arr) // 8), {})[str((a - arr) % 8)] = u8(a)
            elif entry <= a < entry + 0x20:
                out["registry"]["entry"][str(a - entry)] = u8(a)
            elif payload <= a < payload + 0x400:
                out["registry"]["payload"][str(a - payload)] = u8(a)
            elif to.IMG_GLOBALS <= a < to.IMG_HI:
                if snap.read(a, 1) != image[a - to.IMG_LO:a - to.IMG_LO + 1]:
                    out["globals"].append([a, 1, u8(a)])
            else:
                rest.add(a)
        # the raw file records: bytes read through a touched motion's list pointers
        for m in sorted(touched):
            rec = base + m * MOTION_BYTES
            for kind, off in LIST_OFFSET.items():
                for i in range(u8(rec + kind) or 0):
                    p = u64(rec + off + 8 * i)
                    for a in range(p, p + RECORD_BYTES[kind]):
                        if a in rest:
                            out["file_rec"].append([m, kind, i, a - p, u8(a)])
                            rest.discard(a)
        out["other"] = [[lo, hi - lo] for lo, hi in to.ranges(rest)]
        write_reads(os.path.join(out_dir, name + ".reads.tsv"), out)
        # the whole structures, for the full-table comparison (what the chain did not read in this call included)
        with open(os.path.join(out_dir, name + ".full.tsv"), "w") as f:
            f.write("anime\t%s\ntrap\t%s\n" % (snap.read(buf, ANIMETABLE_BYTES).hex(), snap.read(payload, TRAP_BYTES).hex()))
            for kind in (0, 1, 6, 13):
                for m in range(MOTION_SLOTS):
                    rec = base + m * MOTION_BYTES
                    for i in range(u8(rec + kind) or 0):
                        f.write("rec\t%d\t%d\t%d\t%s\n" % (m, kind, i, snap.read(u64(rec + LIST_OFFSET[kind] + 8 * i), RECORD_BYTES[kind]).hex()))
        print(name, "motions", sorted(hex(m) for m in touched), "cells", len(out["cells"]), "file_rec", len(out["file_rec"]),
              "payload", len(out["registry"]["payload"]), "other ranges", len(out["other"]))


if __name__ == "__main__":
    main()
