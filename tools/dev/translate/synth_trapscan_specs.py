#!/usr/bin/env python3
"""synth_trapscan_specs.py: writes synth_specs/trap_scan.json, the SYNTHETIC cases of the body contact scan FUN_1406EDCE0 (match::AnimePlayer's
per-frame ball-touch scan, called from 0x1409F1640 with rcx = P and rdx = the anime driver's output record).

Source capture: a full state of a man in the Trap action (P = rdx of the capture's call, P+0xAC8 = 7, the joint block valid). The cases
choose what the scan reads (the flags word P+0x2460, the ball's sample 1, the owner byte, the bone modes); the program that runs them
is PES's own bytes. Usage: python3 synth_trapscan_specs.py SRC_CAPTURE_DIR; then synth.py --spec synth_specs/trap_scan.json, then
prove_bounded.py 0x1406EDCE0 (docs/ports/pes-port-TrapContact.md)."""
import json
import mmap
import os
import struct
import sys

OUT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "synth_specs")
G_VA = 0x1436F3DC8
BALL_OFFSET = 0x40            # G+0x40: the ball record
SAMPLE1 = 0xB0                # ball record + 0xB0: sample 1 (FUN_140A6CF70 index 1), x, y, z
OWNER = 0x279C                # ball record: the owner's slot byte (>= 0x16 = nobody)
FLAGS = 0x2460                # P: the per-tick flag word (bit 9 = contact pending, bit 10 = touched)
MODES = 0x2704                # P: int[19] per-bone action mode (2 = ball sensor)
MODES_BONE_FOOT_R = 6
JOINT_POS = 0x538             # P: the joint block's positions, 12 bytes a bone
OUT_ZERO_BYTES = 0x200        # the driver's output record is zeroed before the scan
OUT_ABOVE_RSP = 0x400         # the output record lives above the call's rsp (the state is a call, nothing resumes in the caller's frames)
BIT_CONTACT_PENDING = 0x200
BIT_TOUCH = 0x400
FAR_M = 3.0


class Cap:
    def __init__(self, d):
        self.index = json.load(open(os.path.join(d, "index.json")))
        self.mem = mmap.mmap(os.open(os.path.join(d, "mem.bin"), os.O_RDONLY), 0, access=mmap.ACCESS_READ)
        self.call = struct.unpack("<49Q", open(os.path.join(d, "call.bin"), "rb").read()[:49 * 8])

    def read(self, va, n):
        for m in self.index["mappings"]:
            if m["va"] <= va < m["va"] + m["size"]:
                o = m["off"] + va - m["va"]
                return self.mem[o:o + n]
        raise SystemExit("not mapped: %#x" % va)

    def u(self, va, fmt):
        return struct.unpack("<" + fmt, self.read(va, struct.calcsize(fmt)))[0]


def main():
    cap = Cap(sys.argv[1])
    p, rsp = cap.call[2], cap.call[4]  # the capture's call: rdx = P
    ball = cap.u(cap.u(G_VA, "Q") + BALL_OFFSET, "Q")
    out = rsp + OUT_ABOVE_RSP
    flags = cap.u(p + FLAGS, "I")
    modes = [cap.u(p + MODES + 4 * i, "i") for i in range(19)]
    sample = [cap.u(ball + SAMPLE1 + 4 * i, "f") for i in range(3)]
    foot = [cap.u(p + JOINT_POS + 12 * MODES_BONE_FOOT_R + 4 * i, "f") for i in range(3)]
    owner = cap.u(ball + OWNER, "B")
    base = {"flags": flags, "foot_mode": modes[MODES_BONE_FOOT_R], "owner": owner, "bx": sample[0], "by": sample[1], "bz": sample[2]}
    cases = [
        # the ball on the foot: the reach test passes, then the touch chain runs
        dict(name="reach_hit", flags=flags | BIT_CONTACT_PENDING, bx=foot[0], bz=foot[2]),
        # the ball three metres off: the reach test fails and the pending bit is cleared
        dict(name="reach_miss", flags=flags | BIT_CONTACT_PENDING, bx=foot[0] + FAR_M, bz=foot[2]),
        # touched flag already set, nothing pending: the touch chain without the reach test
        dict(name="touched", flags=flags | BIT_TOUCH),
        # the foot bone a ball sensor, no ball contact recorded
        dict(name="sensor_foot", foot_mode=2),
    ]
    fields = [
        {"name": "this", "at": "rcx", "type": "u64", "values": [p]},
        {"name": "out_ptr", "at": "rdx", "type": "u64", "values": [out]},
        {"name": "out_zero", "at": "va:0x%X" % out, "type": "hex", "values": ["00" * OUT_ZERO_BYTES]},
        {"name": "flags", "at": "va:0x%X" % (p + FLAGS), "type": "u32", "values": [flags]},
        {"name": "foot_mode", "at": "va:0x%X" % (p + MODES + 4 * MODES_BONE_FOOT_R), "type": "i32", "values": [base["foot_mode"]]},
        {"name": "owner", "at": "va:0x%X" % (ball + OWNER), "type": "u8", "values": [owner]},
        {"name": "bx", "at": "va:0x%X" % (ball + SAMPLE1), "type": "f32", "values": [sample[0]]},
        {"name": "by", "at": "va:0x%X" % (ball + SAMPLE1 + 4), "type": "f32", "values": [sample[1]]},
        {"name": "bz", "at": "va:0x%X" % (ball + SAMPLE1 + 8), "type": "f32", "values": [sample[2]]},
    ]
    spec = {"label": "trapscan", "seed": 1, "as": "0x1406EDCE0", "fields": fields,
            "cases": [dict({k: v for k, v in base.items()}, this=p, out_ptr=out, out_zero="00" * OUT_ZERO_BYTES, **c) for c in cases]}
    for c in spec["cases"]:
        c["flags"] = c.get("flags", flags)
    json.dump(spec, open(os.path.join(OUT_DIR, "trap_scan.json"), "w"), indent=1)
    print("trap_scan.json", len(cases), "cases; P=%#x ball=%#x out=%#x foot=%s" % (p, ball, out, foot))


if __name__ == "__main__":
    main()
