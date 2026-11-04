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
  A   name value                      an actor fact GF holds as state, read from the capture: height_bits (PM+0x30), stamina_cur/
                                      stamina_max/breath (the slot record's hash-checked integers), speed_byte (slot +0x97), g_bits
                                      (the physique scale G, a named oracle input)

Usage: PES_EXE=<pes_full.bin> python3 trap_oracle.py SNAP CASE_DIR TRACE_DIR HOOKS OUT_DIR   (harness Python: unicorn)
  CASE_DIR  = snapdiff's <out>/gen/14079A470/snap_<label> (corpus.jsonl, maps.txt, ok_calls.bin, ok_delta.bin)
  TRACE_DIR = the frame_trace output of the capture (pool/pages.bin)
"""
import mmap
import collections
import os
import shutil
import struct
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
TABLES = (("side", 0x1928, 0x28, 2, 0x4C30), ("anime", 0x19C8, 0x28, 26, 0x32C), ("man", 0x1DD8, 0x28, 26, 0xDC4),
          ("slot", 0x25F8, 0x28, 22, 0x2E50))
POINTERS = (("B", 0x40, 0x2960), ("MI", 0x270, 0x1770), ("fr", 0x360, 0x10), ("pitch", 0x2AA8, 0x48))
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


PM_VTABLE = 0x1426329A8   # match::Player's vtable (docs/re/pes-re-trap-inputs.md 1.1): PM+0x7D8 = P, PM+0x30 = height (cm)
FNV_BASIS, FNV_PRIME, M64 = 0xCBF29CE484222325, 0x100000001B3, (1 << 64) - 1


def checked_int(snap, cell):
    """The hash-checked integer PES keeps at `cell` (FUN_140A581C0's decode, 0x140A581C0..): hash u64 at +0, counter at +8,
    ring index at +0xC, ring of four u32 at +0x10. Exits when the stored hash does not match: the decode is then wrong."""
    u32 = lambda a: int.from_bytes(snap.read(a, 4), "little")
    count, ring = u32(cell + 8) * 3 & 0xFFFFFFFF, u32(cell + 0x10 + 4 * (u32(cell + 0xC) & 3))
    salt = [count & 0xFF, (count + 4) >> 8 & 0xFF, (count + 8) >> 16 & 0xFF, (count + 12) >> 24 & 0xFF]
    out = [((ring >> (8 * i)) + salt[i] & 0xFF) ^ (salt[i] * 0x3F & 0xFF) for i in range(4)]
    h = FNV_BASIS
    for b in out:
        h = (h ^ b) * FNV_PRIME & M64
    if h != int.from_bytes(snap.read(cell, 8), "little"):
        sys.exit("checked integer at %#x: stored hash differs" % cell)
    return int.from_bytes(bytes(out), "little")


def find_player(snap, p):
    """PM: the match::Player whose +0x7D8 holds P. The 8-byte pattern is searched in the writable mappings."""
    pat = p.to_bytes(8, "little")
    mm = mmap.mmap(snap.fd, 0, prot=mmap.PROT_READ)
    for m in snap.maps:
        if "w" not in m["perm"] or m["deferred"]:
            continue
        at = mm.find(pat, m["off"], m["off"] + m["size"])
        while at >= 0:
            pm = m["va"] + at - m["off"] - 0x7D8
            if (at - m["off"]) % 8 == 0 and int.from_bytes(snap.read(pm, 8) or bytes(8), "little") == PM_VTABLE:
                return pm
            at = mm.find(pat, at + 1, m["off"] + m["size"])
    return None  # a slim capture drops the player object's page


def actor_facts(snap, p, g):
    """A lines: the actor facts GF holds as state, read from the capture (the height from the player object, the stamina
    tank and breath reserve from the slot record as FUN_140A58550/FUN_140A581C0 decode them, the Speed ability byte
    +0x97, and G = *(*(P+0xA08)+0xF0)+0xC0, which the oracle keeps as a named input)."""
    u64 = lambda a: int.from_bytes(snap.read(a, 8), "little")
    slot = snap.read(p + 0x2458, 1)[0]
    rec = u64(g + 0x25F8 + 0x28 * slot)
    pm = find_player(snap, p)
    if pm is not None:
        height_bits = int.from_bytes(snap.read(pm + 0x30, 4), "little")
    else:
        # a slim capture has no PM: the height is the whole-cm value whose /180 is the P+0x3B18 the call reads (FUN_1406EB210)
        f32 = lambda v: struct.unpack("<f", struct.pack("<f", v))[0]
        scale = struct.unpack("<f", snap.read(p + 0x3B18, 4))[0]
        cm = float(round(scale * 180))
        if f32(cm / 180.0) != scale:
            sys.exit("P+0x3B18 = %r is not a whole-cm height / 180" % scale)
        height_bits = struct.unpack("<I", struct.pack("<f", cm))[0]
    return ["A\theight_bits\t%#x" % height_bits,
            "A\tstamina_cur\t%d" % checked_int(snap, rec + 0xC8),
            "A\tstamina_max\t%d" % checked_int(snap, rec + 0xE8),
            "A\tbreath\t%d" % checked_int(snap, rec + 0x108),
            "A\tspeed_byte\t%d" % snap.read(rec + 0x97, 1)[0],
            "A\tg_bits\t%#x" % int.from_bytes(snap.read(u64(u64(p + 0xA08) + 0xF0) + 0xC0, 4), "little")]


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
        # Slot records sit 0x2B90 apart (slot 20 -> 21) but are typed 0x2E50 long (danger_oracle.py, the same defect): a byte of
        # slot 21 also lies in slot 20's range, so the record it belongs to is the nearest base below it among the ranges of the
        # first match's kind.
        hits = [(kind, idx, base) for kind, idx, base, size in typed if base <= a < base + size]
        if not hits:
            return None
        kind, idx, base = max((h for h in hits if h[0] == hits[0][0]), key=lambda h: h[2])
        return kind, idx, a - base

    lines = ["R\t%s\t%#x" % (r, regs[r]) for r in REGS]
    for off in range(0, 0x80, 8):
        lines.append("R\tstk%#x\t%#x" % (off, u64(regs["rsp"] + off)))
    rsp = regs["rsp"]
    # The heap objects the chain reaches from P and G that GF holds as state (or names as inputs): "N name rel len" lines, rel
    # from the object's start. An (P+0x3AC0) and what it points to: MA (+0x18) and the event store E (+0xE0); the physique chain
    # GX (P+0xA08) -> GY (GX+0xF0); the table-B squad records SBn (G+0x1978+0x28n). Everything else outside the window stays "H".
    an, gx = u64(regs["rdx"] + 0x3AC0), u64(regs["rdx"] + 0xA08)
    heap = [("An", an, 0x100), ("MA", u64(an + 0x18), 0x26040), ("E", u64(an + 0xE0), 0x180760), ("GX", gx, 0x100),
            ("GY", u64(gx + 0xF0), 0xD0)] + [("SB%d" % s, u64(g + 0x1978 + 0x28 * s), 0x30) for s in range(2)]

    def named(a):
        for name, base, size in heap:
            if base and base <= a < base + size:
                return (name, base)
        return None
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
            n = named(lo)
            if n is not None and named(hi - 1) == n:
                lines.append("N\t%s\t%#x\t%d" % (n[0], lo - n[1], hi - lo))
            else:
                lines.append("H\t%#x\t%d" % (lo, hi - lo))
    # the writable image globals the chain reads: only those whose live value is not the file's are state (pointers to heap
    # objects, and G); the others are the image's own bytes, which the translated code already has
    for lo, hi in ranges(b for b in live if IMG_GLOBALS <= b < IMG_HI):
        if snap.read(lo, hi - lo) != image[lo - IMG_LO:hi - IMG_LO]:
            lines.append("I\t%#x\t%d" % (lo, hi - lo))
    lines += actor_facts(snap, regs["rdx"], g)
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
