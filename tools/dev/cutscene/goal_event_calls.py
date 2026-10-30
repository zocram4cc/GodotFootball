#!/usr/bin/env python3
"""goal_event_calls.py: synthetic recorded calls of the goal event position (0x140476C50) and the goal radius
(0x140474CF0) over the full snapshot of a captured goal anchor call.

The goal DemoInfo builder 0x140478190 writes A0+0x30 = *0x140476C50(A0, &out, ctx, 0) and A0+0xC = 0x140474CF0(A0,
&cutList); the anchor hook (cut_anchor_calc_474F00) then snapshots the process with A0 (rcx) and ctx (rdx, the
builder's param_2) intact, so the same call can be replayed from that snapshot and must give back what A0 holds.
A0 = DemoInfo + 0x1E8; DemoInfo+0xC is the cut count, DemoInfo+0x10+0x1C*i the cut ids the builder put into the list
(0x140477BC0 output). The call is a rendezvous-style snapshot folder (index.json with rv.hook, call.bin, mem.bin) that
replay_verdicts.py / frame_trace.py recorded / snapdiff.py take as they are:
  OUT/event_pos/<label>/   entry 0x140476C50 (rcx A0, rdx out buffer, r8 ctx, r9 0): mem.bin is a symlink
  OUT/radius/<label>/      entry 0x140474CF0 (rcx A0, rdx std::vector<u32> of the cut ids): mem.bin is a copy with the
                           vector written into the dead stack above rsp (the capture itself is never written)
  OUT/expect.json          label -> {eventPos, radius, cuts}: what A0 holds
Output is PES-derived: scratch only, never committed.

Usage: goal_event_calls.py --out OUT CALC_SNAP[:MEM_SNAP] ...
  CALC_SNAP is the capture whose call.bin gives A0 and ctx; MEM_SNAP (default CALC_SNAP) the full snapshot of the
  same process whose memory is replayed (a slim CALC_SNAP, e.g. pid2621613/seq1532, takes its writer capture seq1531).
"""
import argparse
import json
import os
import shutil
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.normpath(os.path.join(HERE, "..", "translate")))
sys.path.insert(0, os.path.normpath(os.path.join(HERE, "..", "diffharness")))
import stubs as stub_table  # noqa: E402
import frame_trace as ft  # noqa: E402
import full_snapshot as fs  # noqa: E402

EVENT_POS, RADIUS = 0x140476C50, 0x140474CF0
DEMO_INFO_TO_A0 = 0x1E8
CUT_STRIDE = 0x1C
PAD = 0x400  # the synthetic call's scratch lives this far above rsp (the caller's dead frames)


def pack_call(call, gpr):
    vals = [gpr[n] for n in ft.GPR_ORDER]
    for x in call["xmm"]:
        vals += [x & ((1 << 64) - 1), x >> 64]
    vals.append(call["ret"])
    return ft.CALL_REC.pack(*vals)


def write_snap(d, idx, entry, name, call, gpr, mem_src, patch=None):
    os.makedirs(d, exist_ok=True)
    idx = json.loads(json.dumps(idx))
    idx["rv"].update({"hook": name, "entry_va": entry})
    with open(os.path.join(d, "index.json"), "w") as f:
        json.dump(idx, f)
    with open(os.path.join(d, "call.bin"), "wb") as f:
        f.write(pack_call(call, gpr))
    dst = os.path.join(d, "mem.bin")
    if os.path.lexists(dst):
        os.remove(dst)
    if patch is None:
        os.symlink(mem_src, dst)
        return
    shutil.copyfile(mem_src, dst)  # a dense copy; the capture stays read-only
    fd = os.open(dst, os.O_WRONLY)
    try:
        for va, data in patch:
            m = next(m for m in idx["mappings"] if m["va"] <= va < m["va"] + m["size"])
            os.pwrite(fd, data, m["off"] + va - m["va"])
    finally:
        os.close(fd)

def check(out, label, want, orig):
    """PES's own bytes (unicorn) on both synthetic calls must give back what A0 holds, bit for bit."""
    import unicorn.x86_const as xc
    scratch = os.path.join(out, "trace")
    trace = {"out": scratch, "impure": {}, "stubs": ft.stubs_for_run(stub_table.load())}
    os.makedirs(scratch, exist_ok=True)
    d = os.path.join(out, "event_pos", label)
    uc, err = ft.run_recorded(d, orig=orig, trace=trace)
    call = ft.load_recorded_call(d)
    got = struct.unpack("<3f", bytes(uc.mem_read(call["gpr"]["rdx"], 12))) if err is None else err
    ok_pos = list(got) == want["eventPos"] if err is None else False
    d = os.path.join(out, "radius", label)
    uc, err2 = ft.run_recorded(d, orig=orig, trace=trace)
    r = struct.unpack("<f", (uc.reg_read(xc.UC_X86_REG_XMM0) & 0xFFFFFFFF).to_bytes(4, "little"))[0] if err2 is None else err2
    print("ORACLE %s eventPos %s %s radius %s %s" % (label, "OK" if ok_pos else "MISMATCH", got,
                                                      "OK" if err2 is None and r == want["radius"] else "MISMATCH", r),
          flush=True)



def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("snaps", nargs="*")
    ap.add_argument("--out", required=True)
    ap.add_argument("--image", default=os.environ.get("PES_EXE"))
    ap.add_argument("--oracle-cfg", help="rv.dll hooks config: run PES's bytes on each call and compare with A0 (unicorn)")
    ap.add_argument("--check-only", action="store_true", help="no build: check the calls already under --out")
    a = ap.parse_args()
    if a.check_only:
        with open(os.path.join(a.out, "expect.json")) as f:
            for label, want in json.load(f).items():
                check(a.out, label, want, ft.hook_bytes(a.oracle_cfg))
        return
    expect = {}
    for arg in a.snaps:
        calc, _, mem = arg.partition(":")
        mem = mem or calc
        pid, seq = calc.rstrip("/").split(os.sep)[-2:]
        label = "%s_%s" % (seq, pid)  # replay_verdicts.py globs seq*/index.json
        call, mcall = ft.load_recorded_call(calc), ft.load_recorded_call(mem)
        a0, ctx = call["gpr"]["rcx"], call["gpr"]["rdx"]
        m = fs.SnapMem(mem, image=a.image)
        rd = lambda va, n: m.read(va, n)
        base = a0 - DEMO_INFO_TO_A0
        n = struct.unpack("<I", rd(base + 0xC, 4))[0]
        cuts = [struct.unpack("<I", rd(base + 0x10 + CUT_STRIDE * i, 4))[0] for i in range(n)]
        expect[label] = {"eventPos": list(struct.unpack("<3f", rd(a0 + 0x30, 12))),
                         "radius": struct.unpack("<f", rd(a0 + 0xC, 4))[0], "cuts": cuts}
        idx = json.load(open(os.path.join(mem, "index.json")))
        rsp = mcall["gpr"]["rsp"]
        base_regs = dict(mcall["gpr"])
        out_buf = rsp + PAD
        regs = dict(base_regs, rcx=a0, rdx=out_buf, r8=ctx, r9=0)
        write_snap(os.path.join(a.out, "event_pos", label), idx, EVENT_POS, "goal_event_pos_476C50", mcall, regs,
                   os.path.join(mem, "mem.bin"))
        vec = out_buf  # {begin, end, cap}, then the ids
        data = out_buf + 0x20
        blob = struct.pack("<3Q", data, data + 4 * n, data + 4 * n)
        regs = dict(base_regs, rcx=a0, rdx=vec)
        write_snap(os.path.join(a.out, "radius", label), idx, RADIUS, "goal_radius_474CF0", mcall, regs,
                   os.path.join(mem, "mem.bin"),
                   patch=[(vec, blob), (data, struct.pack("<%dI" % n, *cuts))])
        print(label, json.dumps(expect[label]), flush=True)
        if a.oracle_cfg:
            check(a.out, label, expect[label], ft.hook_bytes(a.oracle_cfg))
    with open(os.path.join(a.out, "expect.json"), "w") as f:
        json.dump(expect, f, indent=1)


if __name__ == "__main__":
    sys.exit(main())
