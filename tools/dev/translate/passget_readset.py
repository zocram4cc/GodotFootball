#!/usr/bin/env python3
"""passget_readset.py: the live-in set of one ActionPassGet tick (0x14094E0E0), classified by the record it falls in.

Replays the capture with PES's own bytes (frame_trace's recorded replay, unicorn) and a memory hook, and keeps every byte the call
reads that it has not written itself and that is not the image's own code or constants (trap_oracle.py's method). Each live range is
classified against the records the call reaches: the action object (rcx), the pc record (rdx), the command record R (r8), G, the
per-man tables (side, anime, man, slot, AI), the pointer records (B, MI, fr, pitch), the context chain pc+0x38 -> ctx -> MA (+0x18) / E (+0xE0).

Output (stdout, one JSON object per line): {"kind", "idx", "rel", "len"} per live range, plus {"regs": ...} first.

Usage: PES_EXE=<pes_full.bin> python3 passget_readset.py SNAP HOOKS      (harness Python: unicorn)
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import trap_oracle as to  # noqa: E402
import full_snapshot as fs  # noqa: E402

AI_TABLE, AI_STRIDE, AI_COUNT = 0xF3 * 0x28, 0x28, 22  # FUN_140A489F0: G + (id + 0xF3) * 0x28 (id < 0x16)
THIS_BYTES, PC_BYTES, R_BYTES = 0x2B90, 0x50, 0x1A00


def main():
    snap_dir, hooks = sys.argv[1:3]
    snap = fs.SnapMem(snap_dir)

    def u64(a):
        b = snap.read(a, 8)
        return int.from_bytes(b, "little") if b else 0

    import tempfile
    scratch = tempfile.mkdtemp(prefix="pgrs", dir=os.environ.get("TMPDIR"))  # replay scratch (TMPDIR; never /tmp on this machine)
    regs, live = to.replay(snap_dir, hooks, scratch)
    g = u64(to.G_CELL)
    pc, this, rr = regs["rdx"], regs["rcx"], regs["r8"]
    ctx = u64(pc + 0x38)
    typed = [("this", 0, this, THIS_BYTES), ("pc", 0, pc, PC_BYTES), ("R", 0, rr, R_BYTES), ("G", 0, g, 0x2E58),
             ("ctx", 0, ctx, 0x100), ("MA", 0, u64(ctx + 0x18), 0x26040), ("E", 0, u64(ctx + 0xE0), 0x180760)]
    for kind, off, size in to.POINTERS:
        typed.append((kind, 0, u64(g + off), size))
    for kind, off, stride, count, size in to.TABLES:
        for k in range(count):
            typed.append((kind, k, u64(g + off + stride * k), size))
    for k in range(AI_COUNT):
        typed.append(("ai", k, u64(g + AI_TABLE + AI_STRIDE * k), 0x4000))
    typed = [t for t in typed if t[2]]

    def which(a):
        hits = [(kind, idx, base) for kind, idx, base, size in typed if base <= a < base + size]
        if not hits:
            return None
        # the first kind in the table order wins; inside a kind the nearest base below
        order = {t[0]: i for i, t in reversed(list(enumerate(typed)))}
        kind = min((h[0] for h in hits), key=lambda k: order[k])
        kind, idx, base = max((h for h in hits if h[0] == kind), key=lambda h: h[2])
        return kind, idx, a - base

    print(json.dumps({"regs": {k: hex(v) for k, v in regs.items() if k in ("rcx", "rdx", "r8", "r9", "rsp")}, "ctx": hex(ctx),
                      "side": [hex(u64(g + 0x1928 + 0x28 * s)) for s in range(2)]}))
    rsp = regs["rsp"]
    for lo, hi in to.ranges(b for b in live if b < to.IMG_LO or b >= to.IMG_HI):
        if rsp <= lo < rsp + to.STACK_SPAN:
            print(json.dumps({"kind": "S", "idx": 0, "rel": lo - rsp, "len": hi - lo}))
            continue
        w = which(lo)
        if w and which(hi - 1) and which(hi - 1)[:2] == w[:2]:
            print(json.dumps({"kind": w[0], "idx": w[1], "rel": w[2], "len": hi - lo}))
        else:
            print(json.dumps({"kind": "H", "idx": 0, "rel": lo, "len": hi - lo}))
    for lo, hi in to.ranges(b for b in live if to.IMG_GLOBALS <= b < to.IMG_HI):
        if snap.read(lo, hi - lo) != open(os.environ["PES_EXE"], "rb").read()[lo - to.IMG_LO:hi - to.IMG_LO]:
            print(json.dumps({"kind": "I", "idx": 0, "rel": lo, "len": hi - lo}))


if __name__ == "__main__":
    main()
