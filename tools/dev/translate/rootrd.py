#!/usr/bin/env python3
"""rootrd.py: the kick root-motion reader's oracle rows (rootmotion.tsv) for tests/pes_kick_root_test.cpp.

The reader FUN_141AFDF10 takes rcx = B (the anime chain's child), rdx and r8 (the two output pointers of FUN_1423A3FD0: it writes its
four-float vector to whichever is non-null; the real caller sets rdx and leaves r8 = 0, GF's RunRootMotion sets r8 and rdx = 0), xmm3 =
the time in seconds, and the motion in its 5th argument at [rsp + 0x28] on entry. The chain B is the kicker's own (the engine's
BodyAnimControl in the cut), so an oracle row holds only the call's own inputs and PES's answer; it names no P and no manifest.

Each pair names a capture (call.bin, index.json, mem.bin: a rendezvous capture of the reader, or a synth.py state of it) and a snapdiff
case directory of the same call (gen/141AFDF10/snap_<label>/corpus.jsonl, whose row holds the bytes PES's own run wrote). One row per pair:

    motion (decimal) TAB time bits (hex u32) TAB output (32 hex digits: the 16 bytes at the output pointer, in memory order)

Usage: rootrd.py OUT.tsv CAP_DIR:CASE_DIR [CAP_DIR:CASE_DIR ...]     (harness Python; PES_EXE names the image for SnapMem)
  Rows are appended to OUT.tsv in the order given. A pair whose corpus row is not ok, or whose output bytes the run did not write, is an
  error (never a zero row).
"""
import json
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import full_snapshot as fs  # noqa: E402

CALL_REC = struct.Struct("<49Q")  # frame_trace.py entry_record: 16 GPRs (rax, rcx, rdx, rbx, rsp, rbp, rsi, rdi, r8..r15), 16 XMM (lo, hi), ret
GPR_RCX, GPR_RDX, GPR_RSP, GPR_R8 = 1, 2, 4, 8
XMM3_LO = 16 + 2 * 3  # xmm k's low qword is record slot 16 + 2k
ARG5 = 0x28           # the 5th argument at entry: [rsp + 0x28]
OUT_BYTES = 16        # four floats


def call_args(cap_dir):
    """-> (motion, time bits, output pointer, the record): the output pointer is rdx when the call sets it, else r8."""
    rec = CALL_REC.unpack(open(os.path.join(cap_dir, "call.bin"), "rb").read())
    snap = fs.SnapMem(cap_dir)
    motion_bytes = snap.read(rec[GPR_RSP] + ARG5, 8)
    if motion_bytes is None:
        raise SystemExit("%s: the motion argument at [rsp+0x28] is not in the capture" % cap_dir)
    motion = struct.unpack("<I", motion_bytes[:4])[0]
    seconds_bits = rec[XMM3_LO] & 0xFFFFFFFF
    out_va = rec[GPR_RDX] if rec[GPR_RDX] != 0 else rec[GPR_R8]
    if out_va == 0:
        raise SystemExit("%s: neither rdx nor r8 names an output pointer" % cap_dir)
    return motion, seconds_bits, out_va, rec


def written_output(case_dir, out_va):
    """PES's bytes at [out_va, out_va + 16) from the case's corpus row: the last write of each byte."""
    rows = [json.loads(line) for line in open(os.path.join(case_dir, "corpus.jsonl")) if line.strip()]
    if len(rows) != 1:
        raise SystemExit("%s: want one corpus row, got %d" % (case_dir, len(rows)))
    row = rows[0]
    if not row.get("ok"):
        raise SystemExit("%s: the corpus row is not ok (%s)" % (case_dir, row.get("reason")))
    w = row["w"]
    last = {}
    for i in range(0, len(w), 18):  # 16 hex digits of address, 2 of value
        last[int(w[i:i + 16], 16)] = int(w[i + 16:i + 18], 16)
    out = bytearray()
    for k in range(OUT_BYTES):
        if out_va + k not in last:
            raise SystemExit("%s: PES's run did not write output byte %#x" % (case_dir, out_va + k))
        out.append(last[out_va + k])
    return bytes(out)


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    out_path, pairs = sys.argv[1], sys.argv[2:]
    with open(out_path, "a") as f:
        for pair in pairs:
            cap_dir, case_dir = pair.split(":", 1)
            motion, seconds_bits, out_va, _ = call_args(cap_dir)
            out = written_output(case_dir, out_va)
            f.write("%d\t%x\t%s\n" % (motion, seconds_bits, out.hex()))
            print("%s motion %d time %.6f s -> %s" % (os.path.basename(cap_dir.rstrip("/")), motion,
                                                      struct.unpack("<f", struct.pack("<I", seconds_bits))[0], out.hex()))


if __name__ == "__main__":
    main()
