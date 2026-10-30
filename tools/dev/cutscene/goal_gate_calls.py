#!/usr/bin/env python3
"""goal_gate_calls.py: synthetic recorded calls of the goal run object's gate 0x140472640(A0, ctx, DemoInfo) over the full
snapshot of a captured goal anchor write (cut_anchor_write_47D280: DemoInfo state 5, the frame after the gate opened).

The kind-4 step handler 0x140446B20 calls 0x140472640(D+0x1E8, **(D+0x17860), D) every frame while the goal waits (D = DemoInfo,
[0x1436F3F88]). At the capture A0+0xEC (the ready byte the DemoInfo copies to D+0x1D4) is set and latched; the call here is
replayed with that one byte cleared in a dense copy of the snapshot, so the gate runs its whole body on the scorer's state of
the capture (two frames after it opened) and must set the byte again. One call per snapshot. Output is PES-derived: scratch only.

  OUT/gate/<label>/   entry 0x140472640 (rcx A0, rdx ctx, r8 D): mem.bin is a dense copy with A0+0xEC = 0 (delete after the run)
  OUT/expect.json     label -> {ready, timer, radius, spot, scorer facing}: what A0 and the man records hold

Usage: goal_gate_calls.py --out OUT [--check HOOKS_CFG] SNAP ...   (SNAP = a rendezvous capture folder of cut_anchor_write_47D280)
With --check, PES's bytes (unicorn) run each call and A0+0xEC after it must equal the captured one (1).
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
import stubs as stub_table  # noqa: E402

GATE = 0x140472640
G_SLOT, ROOT_SLOT = 0x1436F3DC8, 0x1436F3F88
A0_OFF, CTX_OFF, READY = 0x1E8, 0x17860, 0xEC
MAN_TABLE, MAN_FACING, MAN_POS = 0xBF, 0x554, 0x55C


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("snaps", nargs="+")
    ap.add_argument("--out", required=True)
    ap.add_argument("--image", default=os.environ.get("PES_EXE"))
    ap.add_argument("--check", help="rv.dll hooks config: run PES's bytes on each call and read A0+0xEC after it")
    a = ap.parse_args()
    expect = {}
    for snap in a.snaps:
        pid, seq = snap.rstrip("/").split(os.sep)[-2:]
        label = "%s_%s" % (seq, pid)
        mcall = ft.load_recorded_call(snap)
        m = fs.SnapMem(snap, image=a.image)
        g, d = m.u64(G_SLOT), m.u64(ROOT_SLOT)
        a0, ctx = d + A0_OFF, m.u64(m.u64(d + CTX_OFF))
        idx = json.load(open(os.path.join(snap, "index.json")))
        regs = dict(mcall["gpr"], rcx=a0, rdx=ctx, r8=d)
        gec.write_snap(os.path.join(a.out, "gate", label), idx, GATE, "goal_gate_472640", mcall, regs,
                       os.path.join(snap, "mem.bin"), patch=[(a0 + READY, b"\x00")])
        scorer = m.u64(m.u64(g + 0x270) + 0x1590)
        man = m.u64(g + (MAN_TABLE + (scorer if scorer < 22 else 0)) * 0x28)
        expect[label] = {"ready": m.read(a0 + READY, 1)[0], "timer": struct.unpack("<i", m.read(a0 + 0x14, 4))[0],
                         "radius": struct.unpack("<f", m.read(a0 + 0xC, 4))[0],
                         "spot": list(struct.unpack("<3f", m.read(a0 + 0x30, 12))), "a0": a0, "ctx": ctx, "demoinfo": d,
                         "facing_slot0": struct.unpack("<f", m.read(man + MAN_FACING, 4))[0]}
        print(label, json.dumps(expect[label]), flush=True)
        if a.check:
            dd = os.path.join(a.out, "gate", label)
            trace = {"out": os.path.join(a.out, "trace"), "impure": {}, "stubs": ft.stubs_for_run(stub_table.load())}
            os.makedirs(trace["out"], exist_ok=True)
            uc, err = ft.run_recorded(dd, orig=ft.hook_bytes(a.check), trace=trace)
            got = bytes(uc.mem_read(a0 + READY, 1))[0] if err is None else err
            print("ORACLE %s ready after the gate: %s (captured %d) %s" % (label, got, expect[label]["ready"],
                  "OK" if got == expect[label]["ready"] else "MISMATCH"), flush=True)
    with open(os.path.join(a.out, "expect.json"), "w") as f:
        json.dump(expect, f, indent=1)


if __name__ == "__main__":
    sys.exit(main())
