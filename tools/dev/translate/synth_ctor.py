#!/usr/bin/env python3
"""synth_ctor.py: a constructor call state built from a real full capture, with no emulation.

A constructor of a PES object is never hooked (it runs once per object, long before any capture), so no live capture holds
its entry. A full capture's memory is the real heap, allocator tables and statics the constructor reads; the entry state
is the capture's own registers with rcx pointing at the block to construct into and rip at the constructor. The proof
(frame_trace.py recorded, snapdiff.py --bounded) compares PES's bytes against the translation over that memory, so any
block of the right size is a valid case: the constructor writes every field it owns. mem.bin is a symlink to the source's.

  --rcx stack   the block sits in the unused stack below the captured rsp (rsp - 0x3000, 0x10-aligned)
  --rcx 0xVA    an explicit block (an existing live object: the constructor re-initialises it)

Usage: synth_ctor.py SRC_CAPTURE OUT_DIR --entry 0xVA --hook-name NAME [--rcx stack|0xVA]
"""
import argparse
import json
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import frame_trace as ft  # noqa: E402

STACK_BELOW = 0x3000


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("src")
    ap.add_argument("out")
    ap.add_argument("--entry", required=True, type=lambda s: int(s, 16))
    ap.add_argument("--hook-name", required=True)
    ap.add_argument("--rcx", default="stack")
    a = ap.parse_args()
    index = json.load(open(os.path.join(a.src, "index.json")))
    raw = open(os.path.join(a.src, "call.bin"), "rb").read()
    vals = list(ft.CALL_REC.unpack(raw))
    rsp = vals[ft.GPR_ORDER.index("rsp")]
    rcx = ((rsp - STACK_BELOW) & ~0xF) if a.rcx == "stack" else int(a.rcx, 16)
    vals[ft.GPR_ORDER.index("rcx")] = rcx
    index["rv"].update(hook=a.hook_name, entry_va=a.entry)
    index["label"] = os.path.basename(a.out.rstrip("/"))
    os.makedirs(a.out, exist_ok=True)
    json.dump(index, open(os.path.join(a.out, "index.json"), "w"))
    open(os.path.join(a.out, "call.bin"), "wb").write(ft.CALL_REC.pack(*vals))
    link = os.path.join(a.out, "mem.bin")
    if os.path.lexists(link):
        os.remove(link)
    os.symlink(os.path.abspath(os.path.join(a.src, "mem.bin")), link)
    print("wrote %s: entry %#x rcx %#x" % (a.out, a.entry, rcx))


if __name__ == "__main__":
    main()
