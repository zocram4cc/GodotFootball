#!/usr/bin/env python3
"""press_inputs.py: decode the inputs of PES's ActionPress (0x140962440) from one rendezvous capture.

  press_inputs.py fields  CAPDIR        typed read of the entry registers: A0 (presser action), A2 (dispatch context),
                                        the planner owner (A2+0x40) and the team ctx (A2+0x38), with each vtable check
  press_inputs.py holders CAPDIR VA...  every word-aligned 8-byte cell in the capture that holds each value (hex)
  press_inputs.py pvalues CAPDIR        every AnimePlayer P (its vtable 0x1425E34D8 at P+0), listed by holder VA

CAPDIR is a capture folder (seqN) with call.bin, index.json and mem.bin.zst. The memory is streamed through zstd;
nothing is unpacked to disk. Reads only. Docs: docs/re/pes-re-press-inputs.md.
"""
import json
import os
import struct
import subprocess
import sys

RCX_INDEX = 1  # call.bin words: rax, rcx, rdx, rbx, rsp, rbp, rsi, rdi, r8, r9, ...
RDX_INDEX = 2
R8_INDEX = 8
SLOT_OFFSET = 0x4  # A2+4: u32 slot id
CTX_OFFSET = 0x38  # A2+0x38: the team AI ctx (PesCtxRecord)
OWNER_OFFSET = 0x40  # A2+0x40: the planner owner, an AnimePlayer P
A2_BYTES = 0x80  # A2 read window
A0_BYTES = 0x100  # A0 read window (the presser action fields sit at A0+0x30..0x7C)
ACTION_FIELDS = (  # (offset, kind) as the translated ActionPress reads and writes them
    (0x30, "u8"), (0x48, "u32"), (0x58, "u32"), (0x5C, "u8"),
    (0x68, "f32x3"), (0x74, "u8"), (0x78, "f32"), (0x7C, "f32"),
)
ACTION_VTABLE = 0x142634948  # A0's vtable, stored by the press action constructor block at 0x140961F0E
P_VTABLE = 0x1425E34D8  # P's vtable, stored at P+0 by 0x1406E0408 (inside P's constructor)
CTX_VTABLE = 0x14259D590  # the team ctx's vtable (ctx+0 in every capture read)
WORD_BYTES = 8


class Capture:
    def __init__(self, folder):
        self.folder = folder
        with open(os.path.join(folder, "index.json")) as fh:
            self.index = json.load(fh)
        with open(os.path.join(folder, "call.bin"), "rb") as fh:
            call = fh.read()
        words = len(call) // WORD_BYTES
        self.regs = struct.unpack("<%dQ" % words, call[: words * WORD_BYTES])

    def _mapping(self, va):
        return next((m for m in self.index["mappings"] if m["va"] <= va < m["va"] + m["size"]), None)

    def read(self, requests):
        """requests: [(va, n)]. Returns {va: bytes} for the mapped ones, in one pass over the stream."""
        spans = []
        for va, n in requests:
            m = self._mapping(va)
            if m is not None:
                spans.append((m["off"] + va - m["va"], va, n))
        raw = os.path.join(self.folder, "mem.bin")
        if os.path.exists(raw):
            out = {}
            with open(raw, "rb") as fh:
                for va, n in requests:
                    m = self._mapping(va)
                    if m is None:
                        continue
                    fh.seek(m["off"] + va - m["va"])
                    out[va] = fh.read(n)
            return out
        spans.sort()
        proc = subprocess.Popen(["zstd", "-dc", os.path.join(self.folder, "mem.bin.zst")], stdout=subprocess.PIPE)
        pos, out = 0, {}
        for off, va, n in spans:
            while pos < off:
                chunk = proc.stdout.read(min(1 << 22, off - pos))
                if not chunk:
                    break
                pos += len(chunk)
            out[va] = proc.stdout.read(n)
            pos += len(out[va])
        proc.kill()
        return out


def u(buf, off, n):
    return int.from_bytes(buf[off : off + n], "little")


def f32(buf, off):
    return struct.unpack("<f", buf[off : off + 4])[0]


def cmd_fields(cap):
    rcx, rdx, r8 = cap.regs[RCX_INDEX], cap.regs[RDX_INDEX], cap.regs[R8_INDEX]
    got = cap.read([(r8, A2_BYTES), (rcx, A0_BYTES)])
    a2, a0 = got[r8], got[rcx]
    owner, ctx = u(a2, OWNER_OFFSET, WORD_BYTES), u(a2, CTX_OFFSET, WORD_BYTES)
    peek = cap.read([(owner, WORD_BYTES), (ctx, WORD_BYTES)])
    print("A0 %#x  A1 %#x  A2 %#x" % (rcx, rdx, r8))
    print("A0 vtable      %#x  (expect %#x)" % (u(a0, 0, WORD_BYTES), ACTION_VTABLE))
    print("A2+%#x slot    %d" % (SLOT_OFFSET, u(a2, SLOT_OFFSET, 4)))
    print("A2+%#x ctx     %#x  ctx+0 vtable %#x (expect %#x)" % (CTX_OFFSET, ctx, u(peek.get(ctx, bytes(8)), 0, 8), CTX_VTABLE))
    print("A2+%#x owner   %#x  owner+0 vtable %#x (expect %#x)" % (OWNER_OFFSET, owner, u(peek.get(owner, bytes(8)), 0, 8), P_VTABLE))
    for off, kind in ACTION_FIELDS:
        if kind == "u8":
            print("A0+%#x u8    %d" % (off, u(a0, off, 1)))
        elif kind == "u32":
            print("A0+%#x u32   %d" % (off, u(a0, off, 4)))
        elif kind == "f32":
            print("A0+%#x f32   %.6f" % (off, f32(a0, off)))
        elif kind == "f32x3":
            print("A0+%#x f32x3 %s" % (off, ["%.6f" % f32(a0, off + 4 * i) for i in range(3)]))


def cmd_holders(cap, values):
    ix = cap.index["mappings"]
    pats = {v: struct.pack("<Q", v) for v in values}
    proc = subprocess.Popen(["zstd", "-dc", os.path.join(cap.folder, "mem.bin.zst")], stdout=subprocess.PIPE, bufsize=1 << 24)
    chunk_size, pos, tail = 1 << 26, 0, b""
    hits = {v: [] for v in values}
    while True:
        chunk = proc.stdout.read(chunk_size)
        if not chunk:
            break
        buf = tail + chunk
        base = pos - len(tail)
        for v, pat in pats.items():
            i = buf.find(pat)
            while i != -1:
                if (base + i) % WORD_BYTES == 0:
                    hits[v].append(base + i)
                i = buf.find(pat, i + 1)
        tail = buf[-WORD_BYTES:]
        pos += len(chunk)
    proc.kill()
    for v in values:
        holders = []
        for fo in hits[v]:
            m = next((m for m in ix if m["off"] <= fo < m["off"] + m["size"]), None)
            holders.append("%#x" % (m["va"] + fo - m["off"]) if m else "file%#x" % fo)
        print("value %#x: %d holders: %s" % (v, len(hits[v]), " ".join(holders)))


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(2)
    cmd, folder = sys.argv[1], sys.argv[2]
    capture = Capture(folder)
    if cmd == "fields":
        cmd_fields(capture)
    elif cmd == "holders":
        cmd_holders(capture, [int(x, 16) for x in sys.argv[3:]])
    elif cmd == "pvalues":
        cmd_holders(capture, [P_VTABLE])
    else:
        print(__doc__)
        sys.exit(2)
