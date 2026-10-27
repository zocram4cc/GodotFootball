#!/usr/bin/env python3
"""lockstep.py: first divergence between the harness trace and a GF_XLATE_TRACE build's trace.

Usage: lockstep.py HARNESS.pcs TRANS.pcs [--context N]

Both files are 392-byte records: the PC, then the pre-instruction state (gpr[16] in encoding order, xmm[16][2]).
Harness records at import-model addresses (>= 0x100000000000) are dropped: the translation runs those models inline.
Reports the first instruction where the PCs differ (control divergence), or else the first record whose state
differs at the same PC. For a state divergence the PREVIOUS instruction is the producer: its effect differs.
"""
import argparse
import struct

REC = 8 + 16 * 8 + 16 * 16
GPRS = ("rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15")


def load(path, drop_import_models):
    raw = open(path, "rb").read()
    recs = [raw[i:i + REC] for i in range(0, len(raw) - REC + 1, REC)]
    out = []
    for r in recs:
        pc = struct.unpack_from("<Q", r, 0)[0]
        if drop_import_models and pc >= 0x100000000000:
            continue
        out.append(r)
    return out


def pc_of(r):
    return struct.unpack_from("<Q", r, 0)[0]


def state_diff(a, b):
    """Named fields where the pre-instruction states differ (gpr and xmm, one entry each)."""
    diffs = []
    ga = struct.unpack_from("<16Q", a, 8)
    gb = struct.unpack_from("<16Q", b, 8)
    for i, (x, y) in enumerate(zip(ga, gb)):
        if x != y:
            diffs.append("%s harness=%#x trans=%#x" % (GPRS[i], x, y))
    for i in range(16):
        xa = a[136 + 16 * i:136 + 16 * i + 16]
        xb = b[136 + 16 * i:136 + 16 * i + 16]
        if xa != xb:
            diffs.append("xmm%d harness=%s trans=%s" % (i, xa[::-1].hex(), xb[::-1].hex()))
    return diffs


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("harness")
    ap.add_argument("trans")
    ap.add_argument("--context", type=int, default=5)
    args = ap.parse_args()
    h = load(args.harness, drop_import_models=True)
    t = load(args.trans, drop_import_models=False)
    print("harness records %d (import models dropped), translated %d" % (len(h), len(t)))
    n = min(len(h), len(t))
    for i in range(n):
        if pc_of(h[i]) != pc_of(t[i]):
            print("CONTROL divergence at record %d: harness %#x, translated %#x" % (i, pc_of(h[i]), pc_of(t[i])))
            for j in range(max(0, i - args.context), i):
                print("  common %d %#x" % (j, pc_of(h[j])))
            return 1
    print("PCs identical over %d records" % n)
    for i in range(n):
        d = state_diff(h[i], t[i])
        if d:
            print("STATE divergence at record %d, pc %#x (producer: pc %#x)" % (
                i, pc_of(h[i]), pc_of(h[i - 1]) if i else 0))
            for line in d[:12]:
                print("  " + line)
            return 2
    print("state identical over %d records" % n)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
