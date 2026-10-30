#!/usr/bin/env python3
"""foul_gate_calls.py: synthetic recorded calls of the foul run object's gate 0x14047EB00(obj) over a full in-play snapshot.

The foul step handlers 0x140446960 (kind 5) and 0x1404466F0 (kind 9) call 0x14047EB00(D+0x6D8) every frame their DemoInfo waits
(D = [0x1436F3F88]); the object is {+0 scene id, +4 and +5 flags, +8 countdown, +0xC ready}. No captured snapshot holds a live foul
DemoInfo, so each call runs on a synthetic object placed in the dead stack above rsp (the gate reads nothing of the object but those
fields) of ONE dense copy of the snapshot whose match info names a fouler and a victim (MI+0x1590, MI+0x1594): the harness (PES's bytes)
and the translated gate must agree on rax, xmm0 and every written byte on every variant. This proves the translation, not the foul
timing: the timing oracle is the live poller's rows (live_timeline_poll.py `Fo`).  Output is PES-derived: scratch only.

Usage: foul_gate_calls.py --out OUT --fouler N --victim N SNAP    (SNAP = any full rendezvous capture; OUT/foul/<label>_<i>/ per variant)
"""
import argparse
import json
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.normpath(os.path.join(HERE, "..", "translate")))
sys.path.insert(0, os.path.normpath(os.path.join(HERE, "..", "diffharness")))
import goal_event_calls as gec  # noqa: E402  (write_snap)
import frame_trace as ft  # noqa: E402
import full_snapshot as fs  # noqa: E402

FOUL_GATE = 0x14047EB00
G_SLOT = 0x1436F3DC8
MI_FOULER, MI_VICTIM = 0x1590, 0x1594
PAD, STRIDE = 0x100, 0x10  # the objects sit this far above rsp, inside the stack mapping of the capture's own frame
# (scene, byte +4, byte +5, countdown): the scene ranges and flags the gate branches on.
VARIANTS = [(0x20008, 0, 0, 0), (0x20008, 0, 0, 1), (0x20008, 0, 0, 40), (0x20008, 0, 0, 162), (0x20008, 1, 0, 0), (0x20008, 0, 1, 0),
            (0x20008, 1, 1, 7), (0x20015, 0, 0, 0), (0x20016, 0, 0, 30), (0x20018, 0, 0, 162), (0x20019, 0, 0, 0), (0x20022, 0, 0, 0),
            (0x20022, 0, 0, 50), (0x20001, 0, 0, 100), (0x20021, 0, 0, 0), (0x30001, 0, 0, 0)]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("snap")
    ap.add_argument("--out", required=True)
    ap.add_argument("--fouler", type=int, required=True)
    ap.add_argument("--victim", type=int, required=True)
    ap.add_argument("--near", action="store_true", help="patch the fouler, victim and referee 1-3 m apart (the distance arms' open side)")
    ap.add_argument("--image", default=os.environ.get("PES_EXE"))
    a = ap.parse_args()
    pid, seq = a.snap.rstrip("/").split(os.sep)[-2:]
    mcall = ft.load_recorded_call(a.snap)
    m = fs.SnapMem(a.snap, image=a.image)
    mi = m.u64(m.u64(G_SLOT) + 0x270)
    idx = json.load(open(os.path.join(a.snap, "index.json")))
    base = (mcall["gpr"]["rsp"] + PAD) & ~0xF
    patch = [(mi + MI_FOULER, struct.pack("<I", a.fouler)), (mi + MI_VICTIM, struct.pack("<I", a.victim))]
    for i, (scene, b4, b5, cd) in enumerate(VARIANTS):
        patch.append((base + i * STRIDE, struct.pack("<IBBHiB", scene, b4, b5, 0, cd, 0)))
    if a.near:  # man records G+(slot+0xBF)*0x28: x at +0x55C, z at +0x564; the referee is slot 0x16
        g = m.u64(G_SLOT)
        for slot, (x, z) in ((a.fouler, (0.0, 0.0)), (a.victim, (1.0, 0.0)), (0x16, (3.0, 0.0))):
            man = m.u64(g + (slot + 0xBF) * 0x28)
            patch += [(man + 0x55C, struct.pack("<f", x)), (man + 0x564, struct.pack("<f", z))]
    first = None
    for i in range(len(VARIANTS)):
        label = "%s_%s_%d_%d_%d" % (seq, pid, a.fouler, a.victim, i)
        d = os.path.join(a.out, "foul", label)
        regs = dict(mcall["gpr"], rcx=base + i * STRIDE, rdx=0)
        if first is None:
            gec.write_snap(d, idx, FOUL_GATE, "foul_gate_47EB00", mcall, regs, os.path.join(a.snap, "mem.bin"), patch=patch)
            first = os.path.join(d, "mem.bin")
        else:
            gec.write_snap(d, idx, FOUL_GATE, "foul_gate_47EB00", mcall, regs, first)
        print(label, VARIANTS[i], flush=True)


if __name__ == "__main__":
    sys.exit(main())
