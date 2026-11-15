#!/usr/bin/env python3
"""synth_squad.py: the squad record initializer FUN_1408C7510 and the team-info constructor FUN_140A3B3A0 as calls over a real capture.

PES builds each side's squad records (0x28 records of 0xF0 bytes, the match setup's player blocks: FUN_1408C6560 = base + side * 0x3A10 +
k * 0xF0 + 0x68) with FUN_1408C7510 (FUN_1408C64E0's loop), the match build fills their fields, and FUN_141C280 / 0x14041C280 calls the
constructor FUN_140A3B3A0(info, side, pointers) which copies each record whose id (record + 4) is nonzero into the team-info record's entry
(+0xA8 + 0x128 k, from +0x30) and numbers the slots. Both run once at match load, before any capture window, so their calls are rebuilt over a
real capture's memory (a stack scratch below rsp, inside the captured stack mapping): the record initializer over a zeroed and a filled
record; the constructor over a zeroed info record, 0x28 records whose bytes are the capture's REAL entries (entry + 0x30, 0xF0 bytes) and a
pointer array, for both sides and with some ids cleared. The program that runs them is PES's bytes; the record says SYNTHETIC.

Usage: python3 synth_squad.py SRC_CAPTURE_UNPACKED OUT_DIR
"""
import argparse
import json
import os
import random
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import synth  # noqa: E402

G_CELL = 0x1436F3DC8
INFO_TABLE, INFO_STRIDE = 0xA3, 0x28   # FUN_140A489D0(G, side) = *(G + (side + 0xA3) * 0x28)
ENTRY0, ENTRY_STRIDE, ENTRY_COPY, REC_BYTES = 0xA8, 0x128, 0x30, 0xF0
INFO_BYTES = 0x3000


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("src")
    ap.add_argument("out")
    a = ap.parse_args()
    idx = json.load(open(os.path.join(a.src, "index.json")))
    reg = dict(zip(synth.GPR_ORDER, struct.unpack("<49Q", open(os.path.join(a.src, "call.bin"), "rb").read())[:16]))

    def rd(va, n):
        return synth.read_capture(a.src, idx, va, n)

    def u64(va):
        return struct.unpack("<Q", rd(va, 8))[0]

    g = u64(G_CELL)
    rnd = random.Random(7)
    base = (reg["rsp"] - 0x30000) & ~0xFFF          # inside the captured stack mapping, below every live frame
    info_va, recs_va, ptrs_va = base, base + INFO_BYTES, base + INFO_BYTES + 0x28 * REC_BYTES
    os.makedirs(a.out, exist_ok=True)

    def run(name, entry, fields):
        spec = {"label": name, "seed": 7, "count": 1, "as": entry, "fields": fields}
        path = os.path.join(a.out, name + ".json")
        json.dump(spec, open(path, "w"))
        subprocess.check_call([sys.executable, os.path.join(HERE, "synth.py"), a.src, os.path.join(a.out, name), "--spec", path])

    # the record initializer: a zeroed record and a filled one
    for tag, fill in (("zero", bytes(REC_BYTES)), ("noise", bytes(rnd.randrange(256) for _ in range(REC_BYTES)))):
        run("squadrec_" + tag, "0x1408C7510",
            [{"name": "rcx", "at": "rcx", "type": "u64", "values": [recs_va]},
             {"name": "rec", "at": "va:%#x" % recs_va, "type": "hex", "values": [fill.hex()]}])
    # the constructor: both sides' real entries as the source records; the second variant clears every third id
    for side in (0, 1):
        info = u64(g + (side + INFO_TABLE) * INFO_STRIDE)
        recs = [bytearray(rd(info + ENTRY0 + k * ENTRY_STRIDE + ENTRY_COPY, REC_BYTES)) for k in range(0x28)]
        for tag, clear in (("real", False), ("holes", True)):
            rr = [bytearray(r) for r in recs]
            if clear:
                for k in range(0, 0x28, 3):
                    rr[k][4:8] = bytes(4)
            blob = b"".join(bytes(r) for r in rr)
            ptrs = b"".join(struct.pack("<Q", recs_va + k * REC_BYTES) for k in range(0x28))
            run("teaminfo_side%d_%s" % (side, tag), "0x140A3B3A0",
                [{"name": "rcx", "at": "rcx", "type": "u64", "values": [info_va]},
                 {"name": "rdx", "at": "rdx", "type": "u64", "values": [side]},
                 {"name": "r8", "at": "r8", "type": "u64", "values": [ptrs_va]},
                 {"name": "info", "at": "va:%#x" % info_va, "type": "hex", "values": ["00" * INFO_BYTES]},
                 {"name": "recs", "at": "va:%#x" % recs_va, "type": "hex", "values": [blob.hex()]},
                 {"name": "ptrs", "at": "va:%#x" % ptrs_va, "type": "hex", "values": [ptrs.hex()]}])


if __name__ == "__main__":
    main()
