#!/usr/bin/env python3
"""synth_chooser.py: a SYNTHETIC kick-chooser call (FUN_1408364D0) on the state a kick_header_833CD0 capture leaves behind.

The kick start FUN_140724CC0 calls the request builder FUN_140833CD0 (0x140724DEF) and, at once, the chooser
(0x140724E02) with the builder's own output as the request:

    0x140724DEB  lea rcx,[rbp-0x30]     ; builder: rcx = request out
    0x140724DEF  call 0x140833CD0       ; rdx = P (rdi)
    0x140724DF4  lea r8,[rbp-0x30]      ; chooser: r8 = the request
    0x140724DFB  lea rcx,[rbp+0x120]    ; chooser: rcx = descriptor out (request + 0x150), rdx = P
    0x140724E02  call 0x1408364D0

No capture holds a chooser call with a real order (the kick_chooser hook sampled mostly "none" answers), but every
kick_header capture holds the state just before one. This tool runs PES's own builder bytes over the capture (the harness,
frame_trace.run_recorded), pokes every byte the builder wrote into a dense copy of the capture's memory, and writes a state
whose recorded call is the chooser with the caller's registers: the registers (rcx, r8) are derived from the capture's builder
call, the request bytes are PES's own output. The case is SYNTHETIC (no PES run called the chooser at this state): label it so
wherever it is cited. frame_trace.py recorded / snapdiff.py replay it like a capture.

Usage: python3 synth_chooser.py HEADER_STATE_DIR OUT_DIR --hooks HOOKS.cfg [--spec-dir D] [--poke-p OFFSET=HEX ...]
"""
import argparse
import json
import os
import struct
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import frame_trace as ft  # noqa: E402
import synth_call as sc  # noqa: E402

CHOOSER = 0x1408364D0
BUILDER = 0x140833CD0
DESCRIPTOR_FROM_REQUEST = 0x150  # rbp+0x120 - (rbp-0x30) at 0x140724DEB / 0x140724DFB
PAGE = 0x1000


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("state")
    ap.add_argument("out")
    ap.add_argument("--hooks", required=True)
    ap.add_argument("--spec-dir", default=os.path.join(ft.DIFF, "specs", "kick"))
    ap.add_argument("--poke-p", action="append", default=[], metavar="OFFSET=HEX",
                    help="bytes written into P (the chooser's rdx) after the builder ran: a chosen, not observed, state")
    a = ap.parse_args()

    index = json.load(open(os.path.join(a.state, "index.json")))
    call = ft.load_recorded_call(a.state)
    if call["entry"] != BUILDER:
        raise SystemExit("the state's recorded call is %#x, not the request builder %#x" % (call["entry"], BUILDER))
    os.makedirs(a.out, exist_ok=True)
    scratch = os.path.join(a.out, "builder_trace")
    trace = {"out": scratch, "impure": ft.load_impure(a.spec_dir), "stubs": ft.stubs_for_run(ft.stub_table.load())}
    os.makedirs(scratch, exist_ok=True)
    uc, err = ft.run_recorded(a.state, orig=ft.hook_bytes(a.hooks), trace=trace)
    if err:
        raise SystemExit("the builder did not replay cleanly: " + err)
    summary = ft.finish_trace(trace)
    written = json.load(open(os.path.join(scratch, "written.json")))

    vals = list(sc.CALL_REC.unpack(open(os.path.join(a.state, "call.bin"), "rb").read()))
    req = call["gpr"]["rcx"]
    vals[sc.GPR_ORDER.index("rcx")] = req + DESCRIPTOR_FROM_REQUEST
    vals[sc.GPR_ORDER.index("r8")] = req
    index["rv"].update(entry_va=CHOOSER, hook="synthetic_%X" % CHOOSER)
    index["label"] = os.path.basename(a.out.rstrip("/"))
    json.dump(index, open(os.path.join(a.out, "index.json"), "w"))
    open(os.path.join(a.out, "call.bin"), "wb").write(sc.CALL_REC.pack(*vals))

    dest = os.path.join(a.out, "mem.bin")
    if os.path.lexists(dest):
        os.remove(dest)
    src = os.path.realpath(os.path.join(a.state, "mem.bin"))
    # dense: SnapMem reads a hole as a page the capture dropped, but a zero page of a full capture is data (a stack page not yet written)
    subprocess.check_call(["cp", "--sparse=never", src, dest])
    n = 0
    with open(dest, "r+b") as f:
        for page, runs in written.items():
            for s, e in runs:
                va = int(page, 16) + s
                data = bytes(uc.mem_read(va, e - s))
                pos, room = sc.file_offset(index, va)
                if len(data) > room:
                    raise SystemExit("builder write at %#x runs past its mapping" % va)
                f.seek(pos)
                f.write(data)
                n += e - s
        for item in a.poke_p:
            off, hexbytes = item.split("=", 1)
            data = bytes.fromhex(hexbytes)
            pos, room = sc.file_offset(index, call["gpr"]["rdx"] + int(off, 0))
            if len(data) > room:
                raise SystemExit("poke at P+%s runs past its mapping" % off)
            f.seek(pos)
            f.write(data)
    json.dump({"synthetic": True, "source": os.path.basename(a.state.rstrip("/")), "builder_written_bytes": n,
               "request": hex(req), "descriptor": hex(req + DESCRIPTOR_FROM_REQUEST)},
              open(os.path.join(a.out, "synthetic.json"), "w"))
    print(json.dumps({"out": a.out, "poked_bytes": n, "builder": summary["calls"]}))


if __name__ == "__main__":
    main()
