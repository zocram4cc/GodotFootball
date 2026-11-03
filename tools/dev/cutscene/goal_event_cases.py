#!/usr/bin/env python3
"""goal_event_cases.py: the inputs and PES's answers of the goal event position (0x140476C50) and run radius (0x140474CF0)
on each captured goal, as JSON, for tests/pes_goal_anchor_test.cpp.

Reads the synthetic calls goal_event_calls.py builds (OUT/event_pos/<label>, OUT/radius/<label>, OUT/expect.json) and, from
PES's own bytes replayed on them (frame_trace.run_recorded), the inputs the calls read that the anchor cases do not pin:
  - the cut list (expect.json "cuts": the DemoInfo scene list the builder 0x140477BC0 made),
  - the A0 random generator's state before the call (A0+0x114 after the call, the LCG of 0x140A92530 rewound one draw),
  - the FixDemo ctx's pick cache (ctx+0x30, a std::map<u32 cut id, u32 row>) as the radius call left it: every node the
    walk can read (the radius call looked each of its cuts up, so every pick it used is there),
  - the scorer's side and squad key (MI+0x15C8/+0x15CC), the field info F0..F8, and PES's A0+0x30 / A0+0xC.
Output is PES-derived: keep it in scratch, never commit it.

Usage: GF_DIFF_PYTHON=<venv python> goal_event_cases.py --calls OUT --cfg HOOKS_CFG --out FILE
"""
import argparse
import json
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.normpath(os.path.join(HERE, "..", "translate")))
sys.path.insert(0, os.path.normpath(os.path.join(HERE, "..", "diffharness")))
import frame_trace as ft  # noqa: E402
import stubs as stub_table  # noqa: E402

G_VA, CTX_VA = 0x1436F3DC8, 0x1437F5D50
LCG_MUL, LCG_ADD, MASK = (-0x2D22B2E3) & 0xFFFFFFFF, 0x4513, 0x7FFFFFFF


def rewind(state, draws):
    inv = pow(LCG_MUL, -1, 1 << 32)
    for _ in range(draws):
        state = ((state - LCG_ADD) * inv) & MASK
    return state


def reader(uc):
    def rd(va, n):
        try:
            return bytes(uc.mem_read(va, n))
        except Exception:
            return None
    return rd


def picks(rd, ctx):
    """Walk the std::map at ctx+0x30 (MSVC _Tree: head, isnil byte at +0x19, key u32 at +0x1C, value u32 at +0x20)."""
    head = struct.unpack("<Q", rd(ctx + 0x30, 8))[0]
    out, stack, seen = {}, [struct.unpack("<Q", rd(head + 8, 8))[0]], set()
    while stack:
        n = stack.pop()
        if n in seen or n == head:
            continue
        seen.add(n)
        b = rd(n, 0x24)
        if b is None or b[0x19]:
            continue
        out[struct.unpack("<I", b[0x1C:0x20])[0]] = struct.unpack("<I", b[0x20:0x24])[0]
        stack += [struct.unpack("<Q", b[0:8])[0], struct.unpack("<Q", b[0x10:0x18])[0]]
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--calls", required=True)
    ap.add_argument("--cfg", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    orig = ft.hook_bytes(a.cfg)
    trace = {"out": os.path.join(a.calls, "trace"), "impure": {}, "stubs": ft.stubs_for_run(stub_table.load())}
    expect = json.load(open(os.path.join(a.calls, "expect.json")))
    rows = []
    for label, want in sorted(expect.items()):
        d = os.path.join(a.calls, "radius", label)
        uc, err = ft.run_recorded(d, orig=orig, trace=trace)
        call = ft.load_recorded_call(d)
        rd = reader(uc)
        q = lambda va: struct.unpack("<Q", rd(va, 8))[0]
        u = lambda va: struct.unpack("<I", rd(va, 4))[0]
        a0 = call["gpr"]["rcx"]
        pick_map = picks(rd, q(CTX_VA)) if rd(CTX_VA, 8) else {}  # a call that never reaches the wrapper maps no ctx
        # G, MI and the field info are read by the event-position call; a capture that lacks the page the call stops at
        # still maps everything before it.
        uc, _ = ft.run_recorded(os.path.join(a.calls, "event_pos", label), orig=orig, trace=trace)
        rd = reader(uc)
        g = q(G_VA)
        mi = q(g + 0x270)
        field = [struct.unpack("<I", rd(q(g + 0x2AA8) + 4 * i, 4))[0] for i in range(9)]
        post = u(a0 + 0x114)
        rows.append({"label": label, "err": err, "sceneId": u(a0 + 8), "cuts": want["cuts"],
                     "radius": struct.pack("<f", want["radius"]).hex(),
                     "eventPos": [struct.pack("<f", v).hex() for v in want["eventPos"]],
                     "side": u(mi + 0x15C8), "place": u(mi + 0x15CC), "field": ["%08X" % v for v in field],
                     "rngPost": post, "rngPre": rewind(post, 1), "picks": sorted(pick_map.items())})
        print(json.dumps(rows[-1]), flush=True)
    with open(a.out, "w") as fh:
        json.dump(rows, fh, indent=1)


if __name__ == "__main__":
    sys.exit(main())
