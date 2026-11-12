#!/usr/bin/env python3
"""synth_dribblereach_specs.py: writes synth_specs/drib_reach_1407b7720.json, the SYNTHETIC cases of the Dribble action's reach test
FUN_1407B7720 (the action's vtable slot +0xB8, called by the body contact scan FUN_1406EDCE0 when P+0x2460 bit 9 is set; rcx = the action,
rdx = P, r8 = the ball record G+0x40).

Source capture: a full dribble-tick capture (live2/pid1270903, hook dribbletick_1407B37D0): P = rdx of its call, a man in the Dribble action
(P+0xAC8 = 4) with his motion in P+0xAE0, the hit index in P+0x2471 and the mirror byte in P+0x2472. The cases choose what the test reads (the
ball's sample 1, the bone modes P+0x2704, the sub-state byte P+0xAD8, the mirror byte, the joint-mask table); the program that runs them is
PES's own bytes. The test compares the ball's sample 1 with the contact point the motion's hit data predicts: the hit bone's joint position +
the joint's rotation applied to the hit offset (hit[0x16], hit[0x17], hit[0x15]) * 0.01 * 0.5 (DAT_14256AE18, DAT_142565600), z negated when
mirrored; this script computes that point from the capture's own bytes, so the "hit" cases put the ball on it. Reach: horizontal <
DAT_14259B89C (0.4 m; DAT_1425CE490 = 0.62 m when the sub-state byte is 5), vertical < DAT_142565600 (0.5 m).

Usage: python3 synth_dribblereach_specs.py SRC_CAPTURE_DIR [CASE ...]; then synth.py --spec synth_specs/drib_reach_1407b7720.json, then
prove_bounded.py 0x1407B7720 (docs/ports/pes-port-TrapContact6.md). Without CASE names every case is written."""
import json
import mmap
import os
import struct
import sys

OUT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "synth_specs")
IMAGE = os.environ.get("PES_EXE", "/home/z/ghidra-projects/scratch/Unpacker/pes_full.bin")
IMAGE_BASE = 0x140000000
G_VA = 0x1436F3DC8
MOTION_MANAGER_VA = 0x1436F23C0
MOTION_RECORD_BYTES = 0x390
BALL_OFFSET = 0x40
SAMPLE1 = 0xB0
MODES = 0x2704
JOINT_POS, JOINT_ROT = 0x538, 0x700
MOTION, SEGMENT, MIRROR, SUBSTATE, JOINT_READY, NONJOINT_BYTE = 0xAE0, 0x2471, 0x2472, 0xAD8, 0xA04, 0xA05
MASK_VA = 0x14351E540
MIRROR_BONE_TABLE_VA = 0x1425DC5A0
OFFSET_SCALE_VA, HIT_SCALE_VA, MIRROR_NEG_VA = 0x14256AE18, 0x142565600, 0x142588218
NARROW_VA, WIDE_VA = 0x14259B89C, 0x1425CE490
WIDE_SUBSTATE = 5
NO_BONE_MODE = 0x11
BONES = 19
FAR_M, LIFT_M = 3.0, 0.6


class Cap:
    def __init__(self, d):
        self.index = json.load(open(os.path.join(d, "index.json")))
        self.mem = mmap.mmap(os.open(os.path.join(d, "mem.bin"), os.O_RDONLY), 0, access=mmap.ACCESS_READ)
        self.call = struct.unpack("<49Q", open(os.path.join(d, "call.bin"), "rb").read()[:49 * 8])
        self.image = open(IMAGE, "rb")

    def read(self, va, n):
        for m in self.index["mappings"]:
            if m["va"] <= va < m["va"] + m["size"]:
                o = m["off"] + va - m["va"]
                return self.mem[o:o + n]
        self.image.seek(va - IMAGE_BASE)  # the image's own cells (constants, tables)
        return self.image.read(n)

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


def predicted_contact(cap, p, mirror):
    """The contact point FUN_1407B7720 predicts (joint-block branch): FUN_1406B0180's bone, FUN_1406B0540's offset."""
    base = cap.u(cap.u(MOTION_MANAGER_VA, "Q"), "Q")
    record = base + cap.u(p + MOTION, "H") * MOTION_RECORD_BYTES
    segment = cap.u(p + SEGMENT, "B")
    if cap.u(record + 1, "B") <= segment or cap.u(record + 0x20 + 8 * segment, "Q") == 0:
        raise SystemExit("the capture's motion has no hit record %d: pick another capture" % segment)
    hit = cap.u(record + 0x20 + 8 * segment, "Q")
    bone = ((cap.u(hit, "I") >> 27) & 0x1F) + 1
    if mirror:
        bone = cap.u(MIRROR_BONE_TABLE_VA + 4 * bone, "I")
    if bone > 0x12:
        bone = 0
    scale = cap.u(OFFSET_SCALE_VA, "f")
    hit_scale = cap.u(HIT_SCALE_VA, "f")
    neg = cap.u(MIRROR_NEG_VA, "f")
    off = [cap.u(hit + 4 * w, "f") * scale for w in (0x16, 0x17, 0x15)]
    if mirror:
        off[2] *= neg
    off = [o * hit_scale for o in off]
    joint = [cap.u(p + JOINT_POS + 12 * bone + 4 * i, "f") for i in range(3)]
    quat = [cap.u(p + JOINT_ROT + 16 * bone + 4 * i, "f") for i in range(4)]
    return bone, [joint[i] + r for i, r in enumerate(rotate(off, quat))]


def main():
    cap = Cap(sys.argv[1])
    p = cap.call[2]  # the capture's call: rdx = P
    this = cap.call[1]
    ball = cap.u(cap.u(G_VA, "Q") + BALL_OFFSET, "Q")
    sample = [cap.u(ball + SAMPLE1 + 4 * i, "f") for i in range(3)]
    mirror = cap.u(p + MIRROR, "B")
    substate = cap.u(p + SUBSTATE, "B")
    modes = cap.read(p + MODES, 4 * BONES).hex()
    mask = cap.read(MASK_VA, BONES).hex()
    a05 = cap.u(p + NONJOINT_BYTE, "B")
    if not cap.u(p + JOINT_READY, "B"):
        raise SystemExit("the capture's joint block is not valid (P+0xA04 = 0): pick another capture")
    bone, pred = predicted_contact(cap, p, mirror)
    _, pred_mirrored = predicted_contact(cap, p, 1)
    _, pred_plain = predicted_contact(cap, p, 0)
    narrow, wide = cap.u(NARROW_VA, "f"), cap.u(WIDE_VA, "f")
    between = (narrow + wide) / 2.0  # a distance only the wide (sub-state 5) test accepts
    on = dict(bx=pred[0], by=pred[1], bz=pred[2])
    cases = [
        # the ball on the predicted contact point: the reach test passes
        dict(name="reach_hit", **on),
        # the ball three metres off: horizontal miss
        dict(name="reach_miss", bx=pred[0] + FAR_M, by=pred[1], bz=pred[2]),
        # the ball 0.6 m above the point: the vertical bound
        dict(name="reach_high", bx=pred[0], by=pred[1] + LIFT_M, bz=pred[2]),
        # between the narrow and the wide bound, sub-state as captured (not 5): a miss
        dict(name="between_narrow", bx=pred[0] + between, by=pred[1], bz=pred[2], substate=substate if substate != WIDE_SUBSTATE else 0),
        # the same ball, sub-state 5: the wide bound accepts it
        dict(name="between_wide", bx=pred[0] + between, by=pred[1], bz=pred[2], substate=WIDE_SUBSTATE),
        # every bone mode 0x11: no bone to reach with, whatever the ball
        dict(name="no_bones", modes=(b"\x11\x00\x00\x00" * BONES).hex(), **on),
        # mirrored motion: the mirror bone table and the negated z offset
        dict(name="mirror_hit", mirror=1, bx=pred_mirrored[0], by=pred_mirrored[1], bz=pred_mirrored[2]),
        dict(name="plain_hit", mirror=0, bx=pred_plain[0], by=pred_plain[1], bz=pred_plain[2]),
        # the joint-mask table zeroed and P+0xA05 = 0: the non-joint branch FUN_141EB5F00
        dict(name="nonjoint", mask="00" * BONES, a05=0, **on),
    ]
    keep = set(sys.argv[2:])
    if keep:
        cases = [c for c in cases if c["name"] in keep]
    base = {"bx": sample[0], "by": sample[1], "bz": sample[2], "mirror": mirror, "substate": substate, "modes": modes, "mask": mask,
            "a05": a05}
    fields = [
        {"name": "this", "at": "rcx", "type": "u64", "values": [this]},
        {"name": "p", "at": "rdx", "type": "u64", "values": [p]},
        {"name": "ball", "at": "r8", "type": "u64", "values": [ball]},
        {"name": "bx", "at": "va:0x%X" % (ball + SAMPLE1), "type": "f32", "values": [sample[0]]},
        {"name": "by", "at": "va:0x%X" % (ball + SAMPLE1 + 4), "type": "f32", "values": [sample[1]]},
        {"name": "bz", "at": "va:0x%X" % (ball + SAMPLE1 + 8), "type": "f32", "values": [sample[2]]},
        {"name": "mirror", "at": "va:0x%X" % (p + MIRROR), "type": "u8", "values": [mirror]},
        {"name": "substate", "at": "va:0x%X" % (p + SUBSTATE), "type": "u8", "values": [substate]},
        {"name": "modes", "at": "va:0x%X" % (p + MODES), "type": "hex", "values": [modes]},
        {"name": "mask", "at": "va:0x%X" % MASK_VA, "type": "hex", "values": [mask]},
        {"name": "a05", "at": "va:0x%X" % (p + NONJOINT_BYTE), "type": "u8", "values": [a05]},
    ]
    spec = {"label": "dribblereach", "seed": 1, "as": "0x1407B7720", "fields": fields,
            "cases": [dict(base, this=this, p=p, ball=ball, **c) for c in cases]}
    json.dump(spec, open(os.path.join(OUT_DIR, "drib_reach_1407b7720.json"), "w"), indent=1)
    print("drib_reach_1407b7720.json", len(cases), "cases; P=%#x ball=%#x motion=%#x hit bone %d predicted %s"
          % (p, ball, cap.u(p + MOTION, "H"), bone, pred))


if __name__ == "__main__":
    main()
