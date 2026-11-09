#!/usr/bin/env python3
"""synth_trapscan_specs.py: writes synth_specs/trap_scan.json, the SYNTHETIC cases of the body contact scan FUN_1406EDCE0 (match::AnimePlayer's
per-frame ball-touch scan, called from 0x1409F1640 with rcx = P and rdx = the anime driver's output record).

Source capture: a full state of a man in the Trap action (P = rdx of the capture's call, P+0xAC8 = 7, the joint block valid). The cases
choose what the scan reads (the flags word P+0x2460, the ball's sample 1, the owner byte, the bone modes); the program that runs them
is PES's own bytes. The Trap action's reach test (FUN_140799060) compares the ball's sample 1 with the contact point it predicts: the hit
bone's joint position + the joint's rotation applied to the motion's hit offset (cm / 100 * P+0x3B18); this script computes that point from
the capture's own bytes, so the "hit" cases put the ball on it.

Usage: python3 synth_trapscan_specs.py SRC_CAPTURE_DIR [CASE ...]; then synth.py --spec synth_specs/trap_scan.json, then
prove_bounded.py 0x1406EDCE0 (docs/ports/pes-port-TrapContact.md). Without CASE names every case is written."""
import json
import mmap
import os
import struct
import sys

OUT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "synth_specs")
G_VA = 0x1436F3DC8
MOTION_MANAGER_VA = 0x1436F23C0   # DAT_1436F23C0: the motion database (its first qword is the record array)
MOTION_RECORD_BYTES = 0x390
BALL_OFFSET = 0x40            # G+0x40: the ball record
SAMPLE1 = 0xB0                # ball record + 0xB0: sample 1 (FUN_140A6CF70 index 1), x, y, z
OWNER = 0x279C                # ball record: the owner's slot byte (>= 0x16 = nobody)
FLAGS = 0x2460                # P: the per-tick flag word (bit 8 = hand branch, bit 9 = contact pending, bit 10 = touched, bit 7 = forced)
MODES = 0x2704                # P: int[19] per-bone action mode (2 = ball sensor)
JOINT_POS = 0x538             # P: the joint block's positions, 12 bytes a bone
JOINT_ROT = 0x700             # P: the joint block's rotations, 16 bytes a bone
HEIGHT_SCALE = 0x3B18         # P: height / 180
MOTION = 0xAE0                # P: the playing motion
BONE_FOOT_R, BONE_HAND_L = 6, 14
OUT_ZERO_BYTES = 0x200        # the driver's output record is zeroed before the scan
OUT_ABOVE_RSP = 0x400         # the output record lives above the call's rsp (the state is a call, nothing resumes in the caller's frames)
BIT_FORCED, BIT_HAND, BIT_CONTACT_PENDING, BIT_TOUCH = 0x80, 0x100, 0x200, 0x400
FAR_M, LIFT_M, OTHER_OWNER = 3.0, 0.2, 3


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


def rotate(v, q):
    """v rotated by the unit quaternion q = (x, y, z, w): v + 2w(qv x v) + 2 qv x (qv x v)."""
    def cross(a, b):
        return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
    t = cross(q[:3], v)
    t = (2 * t[0], 2 * t[1], 2 * t[2])
    t2 = cross(q[:3], t)
    return tuple(v[i] + q[3] * t[i] + t2[i] for i in range(3))


def predicted_contact(cap, p):
    """The contact point FUN_140799060 predicts for the playing motion's first hit (joint-block branch)."""
    base = cap.u(cap.u(MOTION_MANAGER_VA, "Q"), "Q")
    record = base + cap.u(p + MOTION, "H") * MOTION_RECORD_BYTES
    hit = cap.u(record + 0x20, "Q")  # the first hit record
    bone = ((cap.u(hit, "I") >> 27) & 0x1F) + 1
    offset = [cap.u(hit + 4 * w, "f") * 0.01 * cap.u(p + HEIGHT_SCALE, "f") for w in (0x16, 0x17, 0x15)]
    joint = [cap.u(p + JOINT_POS + 12 * bone + 4 * i, "f") for i in range(3)]
    quat = [cap.u(p + JOINT_ROT + 16 * bone + 4 * i, "f") for i in range(4)]
    return bone, [joint[i] + r for i, r in enumerate(rotate(offset, quat))]


def main():
    cap = Cap(sys.argv[1])
    p, rsp = cap.call[2], cap.call[4]  # the capture's call: rdx = P
    ball = cap.u(cap.u(G_VA, "Q") + BALL_OFFSET, "Q")
    out = rsp + OUT_ABOVE_RSP
    flags = cap.u(p + FLAGS, "I")
    modes = [cap.u(p + MODES + 4 * i, "i") for i in range(19)]
    sample = [cap.u(ball + SAMPLE1 + 4 * i, "f") for i in range(3)]
    owner = cap.u(ball + OWNER, "B")
    bone, pred = predicted_contact(cap, p)
    base = {"flags": flags, "foot_mode": modes[BONE_FOOT_R], "hand_mode": modes[BONE_HAND_L], "owner": owner,
            "bx": sample[0], "by": sample[1], "bz": sample[2]}
    on = dict(bx=pred[0], by=pred[1], bz=pred[2])
    cases = [
        # the ball on the predicted contact point: the reach test passes, then the touch chain runs
        dict(name="reach_hit", flags=flags | BIT_CONTACT_PENDING, **on),
        # the ball three metres off: the reach test fails and the pending bit is cleared
        dict(name="reach_miss", flags=flags | BIT_CONTACT_PENDING, bx=pred[0] + FAR_M, by=pred[1], bz=pred[2]),
        # touched flag already set, nothing pending: the touch chain without the reach test
        dict(name="touched", flags=flags | BIT_TOUCH),
        # the foot bone a ball sensor, no ball contact recorded
        dict(name="sensor_foot", foot_mode=2),
        # the ball on the point but owned by another man: the pending bit is only cleared
        dict(name="reach_other_owner", flags=flags | BIT_CONTACT_PENDING, owner=OTHER_OWNER, **on),
        # the ball on the point, a hand bone a sensor with the hand branch flag
        dict(name="sensor_hand", flags=flags | BIT_HAND, hand_mode=2, **on),
        # forced flag (bit 7) set: the touch chain
        dict(name="forced", flags=flags | BIT_FORCED, **on),
        # the ball 0.2 m above the point: the vertical bound of the reach test
        dict(name="reach_high", flags=flags | BIT_CONTACT_PENDING, bx=pred[0], by=pred[1] + LIFT_M, bz=pred[2]),
    ]
    keep = set(sys.argv[2:])
    if keep:
        cases = [c for c in cases if c["name"] in keep]
    fields = [
        {"name": "this", "at": "rcx", "type": "u64", "values": [p]},
        {"name": "out_ptr", "at": "rdx", "type": "u64", "values": [out]},
        {"name": "out_zero", "at": "va:0x%X" % out, "type": "hex", "values": ["00" * OUT_ZERO_BYTES]},
        {"name": "flags", "at": "va:0x%X" % (p + FLAGS), "type": "u32", "values": [flags]},
        {"name": "foot_mode", "at": "va:0x%X" % (p + MODES + 4 * BONE_FOOT_R), "type": "i32", "values": [base["foot_mode"]]},
        {"name": "hand_mode", "at": "va:0x%X" % (p + MODES + 4 * BONE_HAND_L), "type": "i32", "values": [base["hand_mode"]]},
        {"name": "owner", "at": "va:0x%X" % (ball + OWNER), "type": "u8", "values": [owner]},
        {"name": "bx", "at": "va:0x%X" % (ball + SAMPLE1), "type": "f32", "values": [sample[0]]},
        {"name": "by", "at": "va:0x%X" % (ball + SAMPLE1 + 4), "type": "f32", "values": [sample[1]]},
        {"name": "bz", "at": "va:0x%X" % (ball + SAMPLE1 + 8), "type": "f32", "values": [sample[2]]},
    ]
    spec = {"label": "trapscan", "seed": 1, "as": "0x1406EDCE0", "fields": fields,
            "cases": [dict(base, this=p, out_ptr=out, out_zero="00" * OUT_ZERO_BYTES, **c) for c in cases]}
    json.dump(spec, open(os.path.join(OUT_DIR, "trap_scan.json"), "w"), indent=1)
    print("trap_scan.json", len(cases), "cases; P=%#x ball=%#x out=%#x hit bone %d predicted %s" % (p, ball, out, bone, pred))


if __name__ == "__main__":
    main()
