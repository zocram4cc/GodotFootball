#!/usr/bin/env python3
"""synth_trapadmit_specs.py: SYNTHETIC cases of the twelve reaction-gate-4a slot +0x70 handlers (the Trap action's slot +0x10 0x14079BBE0
calls the CURRENT action's slot +0x70 with (action object, P, 7)). One spec per handler, written to synth_specs/trap_admit_<VA>.json.

Every handler is a "static as" state over a real capture of a call of a gate-4a function (any trapadmit_* hook: rdx = P, the same P and
action table in every one): rcx = the action object the handler's action id names in P's table (P + 0xC40 + the object offset the
generated store table pesactiontable_stores.inc gives), rdx = P, r8d = the third argument. The cases poke what the handlers read through
the anime interface (vtable 0x1425E3508): the motion id (slot +0x18 = P+0xAE0), the phase byte (slot +0x20 = P+0x245C), the action type
(slot +0x28 = P+0xAC9), the motion table's first entry (slot +0x38 elapsed: id, seconds), and P+0x2752/0x2460 (the kick code and flag
word). Motion ids are the ones the handlers compare against (their decompiles) and the capture's own table holds their Animetable rows.

Usage (harness Python): python3 synth_trapadmit_specs.py CAPTURE_DIR   (CAPTURE_DIR: an unpacked capture with call.bin)
"""
import json
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
OUT = os.path.join(HERE, "synth_specs")
STORES = os.path.join(REPO, "src", "sim", "onthepitch", "pesactiontable_stores.inc")

TABLE = 0xC40            # P+0xC40: the action table object
POINTERS = 0x1528        # id -> object pointer cells inside it
SLOTS = 0x5E
# (handler VA, [action ids it serves], third arguments to try)
HANDLERS = [
    (0x140782570, [2], [7]),
    (0x140789E60, [3, 6], [7]),
    (0x1407C6F40, [5], [7, 9, 0x12]),
    (0x140724820, [8, 9, 0xA], [7, 4, 0x10, 0x12]),
    (0x140787EC0, [0xB], [7]),
    (0x14072E450, [0x10], [7, 4]),
    (0x14072E360, [0x11], [7]),
    (0x14073B680, [0x15], [7]),
    (0x140744090, [0x44], [7]),
    (0x140779DF0, [0x48], [7, 3]),
    (0x1407E8D20, [0x50], [7]),
    (0x1407E8E10, [0x53], [7]),
]
MAX_CASES = 5
MOTIONS = [0x805, 0x8E4, 0xC0A, 0xCB7, 0x959, 0x339, 0x70F, 0xDA9]  # ids the handlers compare (decompiles) and the trap's own motion
PHASES = [0, 1, 4, 5, 8, 0x10]
SECONDS = [0.0, 0.3, 2.0]


def object_offsets():
    cells = {}
    for line in open(STORES):
        m = re.match(r"\{(0x[0-9A-F]+), (\d), (0x[0-9A-F]+)ULL\}", line)
        if m:
            cells[int(m[1], 16)] = (int(m[2]), int(m[3], 16))
    return {i: cells[POINTERS + 8 * i][1] for i in range(SLOTS) if POINTERS + 8 * i in cells}


def main():
    cap = sys.argv[1]
    call = struct.unpack("<49Q", open(os.path.join(cap, "call.bin"), "rb").read(49 * 8))
    p = call[2]  # rdx
    objs = object_offsets()
    os.makedirs(OUT, exist_ok=True)
    for va, ids, thirds in HANDLERS:
        rcx = p + TABLE + objs[ids[0]]
        fields = [
            {"name": "this", "at": "rcx", "type": "u64", "values": [rcx]},
            {"name": "p", "at": "rdx", "type": "u64", "values": [p]},
            {"name": "third", "at": "r8", "type": "u64", "values": [7]},
            {"name": "motion", "at": "va:0x%X" % (p + 0xAE0), "type": "u16", "values": [0]},
            {"name": "phase", "at": "va:0x%X" % (p + 0x245C), "type": "u8", "values": [0]},
            {"name": "type", "at": "va:0x%X" % (p + 0xAC9), "type": "u8", "values": [ids[0]]},
            {"name": "entry_id", "at": "va:0x%X" % (p + 0x354), "type": "u32", "values": [0]},
            {"name": "seconds", "at": "va:0x%X" % (p + 0x354 + 8), "type": "f32", "values": [0.0]},
            {"name": "count", "at": "va:0x%X" % (p + 0x420), "type": "u32", "values": [1]},
        ]
        cases = []
        for mi, motion in enumerate(MOTIONS):
            for pi, phase in enumerate(PHASES):
                for third in thirds:
                    if (mi + pi) % 3 or (third != thirds[0] and mi % 2):  # a fixed thinning: every third pair, the extra thirds on even motions
                        continue
                    sec = SECONDS[(mi + pi) % len(SECONDS)]
                    for action in ids[:2]:
                        cases.append({"name": "a%x_m%x_p%x_t%x_s%d" % (action, motion, phase, third, int(sec * 10)),
                                      "this": p + TABLE + objs[action], "p": p, "third": third, "motion": motion, "phase": phase,
                                      "type": action, "entry_id": motion, "seconds": sec, "count": 1})
        cases = cases[:: max(1, len(cases) // MAX_CASES)][:MAX_CASES]  # each state is a whole memory copy: a handful per handler
        spec = {"label": "trapadmit%X" % va, "seed": 1, "as": "0x%X" % va, "fields": fields, "cases": cases}
        json.dump(spec, open(os.path.join(OUT, "trap_admit_%X.json" % va), "w"))
        print("%X" % va, len(cases))


if __name__ == "__main__":
    main()
