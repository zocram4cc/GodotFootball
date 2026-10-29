#!/usr/bin/env python3
"""trap_oracle.py: build the oracle directory of one kind-0 trap capture for tests/pes_trap_oracle_test.cpp.

The directory holds PES-derived files (never committed): mem.bin (link to the capture), maps.txt, ok_calls.bin, ok_delta.bin,
corpus.jsonl (the harness's reference rows, from a snapdiff run), pages.bin, teb.txt, and manifest.tsv, the live-in set of the
call: every byte the chain reads that it has not written itself, found by replaying the capture with a memory hook
(readset.py's replay with a written-byte set), classified against the records GF's window holds in PES layout.

manifest.tsv, one record per line, tab separated:
  R   reg  value                      entry registers (rcx rdx r8 r9 rsp) and the first stack words
  T   kind idx rel len                a live range inside a record the window holds (G, P, B, MI, pitch, side, anime, man, slot)
  H   va   len                        a live range outside every window record (heap: loader data, call frame)
  I   va   len                        a live range in the image's writable globals whose value differs from the file
  F   rel  kind idx trel              a pointer cell in P that points into another window record
  S   off  len                        a live range of the caller's stack, relative to the entry rsp

Usage: PES_EXE=<pes_full.bin> python3 trap_oracle.py SNAP CASE_DIR TRACE_DIR HOOKS OUT_DIR   (harness Python: unicorn)
  CASE_DIR  = snapdiff's <out>/gen/14079A470/snap_<label> (corpus.jsonl, maps.txt, ok_calls.bin, ok_delta.bin)
  TRACE_DIR = the frame_trace output of the capture (pool/pages.bin)
"""
import collections
import os
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import frame_trace as ft  # noqa: E402
import full_snapshot as fs  # noqa: E402
import unicorn.x86_const as xc  # noqa: E402
from unicorn import UC_HOOK_MEM_READ, UC_HOOK_MEM_WRITE  # noqa: E402

IMG_LO, IMG_GLOBALS, IMG_HI = 0x140000000, 0x143000000, 0x143800000  # code and constants / writable globals
REGS = ("rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15")
G_CELL = 0x1436F3DC8  # the image global that holds G
# The records the window holds: (kind, table offset in G or None, entry stride, count, record bytes). The sizes are the window's.
TABLES = (("side", 0x1928, 0x28, 2, 0x260), ("anime", 0x19C8, 0x28, 26, 0x32C), ("man", 0x1DD8, 0x28, 26, 0xDC4),
          ("slot", 0x25F8, 0x28, 22, 0x2E50))
POINTERS = (("B", 0x40, 0x2960), ("MI", 0x270, 0x1760), ("fr", 0x360, 0x10), ("pitch", 0x2AA8, 0x48))
P_BYTES = 0x3B40
STACK_SPAN = 0x300  # the entry frame: rsp .. rsp + STACK_SPAN is the caller's call frame


def replay(snap, hooks, scratch):
    written, live, regs = set(), set(), {}

    def on_read(uc, access, addr, size, value, data):
        if IMG_LO <= addr < IMG_GLOBALS:
            return
        for b in range(addr, addr + size):
            if b not in written:
                live.add(b)

    def on_write(uc, access, addr, size, value, data):
        for b in range(addr, addr + size):
            written.add(b)

    def on_code(uc, addr, size, data):
        if not regs:
            for r in REGS:
                regs[r] = uc.reg_read(getattr(xc, "UC_X86_REG_" + r.upper()))
            uc.hook_add(UC_HOOK_MEM_READ, on_read)
            uc.hook_add(UC_HOOK_MEM_WRITE, on_write)

    trace = {"out": os.path.join(scratch, os.path.basename(snap.rstrip("/"))), "impure": {},
             "stubs": ft.stubs_for_run(ft.stub_table.load())}
    os.makedirs(trace["out"], exist_ok=True)
    _, err = ft.run_recorded(snap, orig=ft.hook_bytes(hooks), trace=trace, on_code=on_code)
    if err is not None:
        sys.exit("replay failed: %s" % err)
    return regs, live


def ranges(addrs):
    out = []
    for b in sorted(addrs):
        if out and b - out[-1][1] <= 8:
            out[-1][1] = b + 1
        else:
            out.append([b, b + 1])
    return out


def main():
    snap_dir, case_dir, trace_dir, hooks, out_dir = sys.argv[1:6]
    image = open(os.environ["PES_EXE"], "rb").read()
    os.makedirs(out_dir, exist_ok=True)
    snap = fs.SnapMem(snap_dir)

    def u64(a):
        b = snap.read(a, 8)
        return int.from_bytes(b, "little") if b else 0

    regs, live = replay(snap_dir, hooks, out_dir)
    g = u64(G_CELL)
    typed = [("G", 0, g, 0x2E58), ("P", 0, regs["rdx"], P_BYTES)]
    for kind, off, size in POINTERS:
        typed.append((kind, 0, u64(g + off), size))
    for kind, off, stride, count, size in TABLES:
        for k in range(count):
            typed.append((kind, k, u64(g + off + stride * k), size))
    typed = [t for t in typed if t[2]]

    def which(a):
        # PES's records of one kind sit closer than the window's record size (slot 20 and 21 are 0x2B90 apart, the table's
        # size is 0x2E50): an address belongs to the record that starts last at or before it.
        best = None
        for kind, idx, base, size in typed:
            if base <= a < base + size and (best is None or base > best[3]):
                best = (kind, idx, a - base, base)
        return best[:3] if best else None

    lines = ["R\t%s\t%#x" % (r, regs[r]) for r in REGS]
    for off in range(0, 0x80, 8):
        lines.append("R\tstk%#x\t%#x" % (off, u64(regs["rsp"] + off)))
    rsp = regs["rsp"]
    for lo, hi in ranges(b for b in live if b < IMG_LO or b >= IMG_HI):
        if rsp <= lo < rsp + STACK_SPAN:
            lines.append("S\t%#x\t%d" % (lo - rsp, hi - lo))
            continue
        w = which(lo)
        if w and which(hi - 1) and which(hi - 1)[:2] == w[:2]:
            lines.append("T\t%s\t%d\t%#x\t%d" % (w[0], w[1], w[2], hi - lo))
            if w[0] == "P":  # an 8-byte cell of P that points into another window record
                for a in range(lo, hi - 7):
                    t = which(u64(a)) if u64(a) > 0x10000 else None
                    if t and t[0] != "P":
                        lines.append("F\t%#x\t%s\t%d\t%#x" % (a - regs["rdx"], t[0], t[1], t[2]))
        else:
            lines.append("H\t%#x\t%d" % (lo, hi - lo))
    # the writable image globals the chain reads: only those whose live value is not the file's are state (pointers to heap
    # objects, and G); the others are the image's own bytes, which the translated code already has
    for lo, hi in ranges(b for b in live if IMG_GLOBALS <= b < IMG_HI):
        if snap.read(lo, hi - lo) != image[lo - IMG_LO:hi - IMG_LO]:
            lines.append("I\t%#x\t%d" % (lo, hi - lo))
    open(os.path.join(out_dir, "manifest.tsv"), "w").write("\n".join(lines) + "\n")
    for name in ("maps.txt", "ok_calls.bin", "ok_delta.bin", "corpus.jsonl"):
        shutil.copy(os.path.join(case_dir, name), os.path.join(out_dir, name))
    shutil.copy(os.path.join(trace_dir, "pool", "pages.bin"), os.path.join(out_dir, "pages.bin"))
    link = os.path.join(out_dir, "mem.bin")
    if os.path.lexists(link):
        os.remove(link)
    os.symlink(os.path.join(os.path.abspath(snap_dir), "mem.bin"), link)
    open(os.path.join(out_dir, "teb.txt"), "w").write(hex(ft.trace_teb(trace_dir, snap)) + "\n")
    print("manifest: %d lines, %d typed records" % (len(lines), len(typed)))


if __name__ == "__main__":
    main()
