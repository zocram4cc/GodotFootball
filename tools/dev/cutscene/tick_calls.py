#!/usr/bin/env python3
"""tick_calls.py: a synthetic recorded call of the DemoInfo tick 0x14047B460 over the full snapshot of a captured recompute
call (recompute_471A80, real call seq203 of pid2024691: the tick's own call into 0x140471A80).

The kind-4 step handler 0x140446B20 calls the tick as (rcx = D+0x1E8, rdx = ctx, r8 = list, r9 = D+0x2790, arg5 = D,
arg6 = the handler's rdx), with list = *(D+0x17860) and ctx = *list (the caller reads **(D+0x17860) for ctx). The recompute
call is made from inside the tick with (A0, ctx, D, arg6), so its recorded registers give D = r8, A0 = rcx, ctx = rdx and
arg6 = r9. The tick's own arguments are rebuilt from those and the snapshot's memory. The tick's return address and its
stack arguments (arg5 at [rsp+0x28], arg6 at [rsp+0x30]) are in the call record and the copied memory, so the replay runs
the tick's whole body on the state the recompute saw. One call per snapshot. Output is PES-derived: scratch only.

  OUT/tick/<label>/   entry 0x14047B460 (rcx A0, rdx ctx, r8 list, r9 D+0x2790): mem.bin is a dense copy (stack args patched)

Usage: tick_calls.py --out OUT [--image PES_EXE] SNAP ...   (SNAP = a rendezvous capture of recompute_471A80, unpacked)
"""
import argparse
import json
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.normpath(os.path.join(HERE, "..", "translate")))
sys.path.insert(0, os.path.normpath(os.path.join(HERE, "..", "diffharness")))
sys.path.insert(0, HERE)
import goal_event_calls as gec  # noqa: E402  (write_snap)
import frame_trace as ft  # noqa: E402
import full_snapshot as fs  # noqa: E402

TICK = 0x14047B460
TICK_RET = 0x140446D32  # the return address in the handler 0x140446B20 (after the tick's call)
A0_OFF, CTX_OFF, LIST_OFF, PARAM4_OFF = 0x1E8, 0x17860, 0x17860, 0x2790
STACK_ARG5, STACK_ARG6 = 0x28, 0x30  # callee-entry offsets of the 5th and 6th arguments (after the return address)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("snaps", nargs="+")
    ap.add_argument("--out", required=True)
    ap.add_argument("--image", default=os.environ.get("PES_EXE"))
    a = ap.parse_args()
    for snap in a.snaps:
        pid, seq = snap.rstrip("/").split(os.sep)[-2:]
        label = "%s_%s" % (seq, pid)
        mcall = ft.load_recorded_call(snap)
        m = fs.SnapMem(snap, image=a.image)
        g = mcall["gpr"]
        d, a0, ctx_rec, arg6 = g["r8"], g["rcx"], g["rdx"], g["r9"]
        if a0 != d + A0_OFF:
            raise SystemExit("%s: rcx %#x is not D+0x1E8 (D = r8 %#x)" % (snap, a0, d))
        lst = m.u64(d + LIST_OFF)
        ctx = m.u64(lst)
        if ctx != ctx_rec:
            raise SystemExit("%s: **(D+0x17860) %#x is not the recompute's ctx %#x" % (snap, ctx, ctx_rec))
        idx = json.load(open(os.path.join(snap, "index.json")))
        call = dict(mcall, entry=TICK, ret=TICK_RET)
        regs = dict(g, rcx=a0, rdx=ctx, r8=lst, r9=d + PARAM4_OFF)
        rsp = g["rsp"]
        patch = [(rsp, struct.pack("<Q", TICK_RET)), (rsp + STACK_ARG5, struct.pack("<Q", d)),
                 (rsp + STACK_ARG6, struct.pack("<Q", arg6))]
        gec.write_snap(os.path.join(a.out, "tick", label), idx, TICK, "tick_47B460", call, regs,
                       os.path.join(snap, "mem.bin"), patch=patch)
        print(label, json.dumps({"D": hex(d), "A0": hex(a0), "list": hex(lst), "ctx": hex(ctx), "arg6": hex(arg6),
                                 "rsp": hex(rsp)}), flush=True)


if __name__ == "__main__":
    sys.exit(main())
