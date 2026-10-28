#!/usr/bin/env python3
"""StateSynth: build a real PES object graph by running PES's own G setup
from the image in a unicorn session, with a bump allocator replacing the
per-class allocator (stated), the 4 external name/log/string calls stubbed,
and the result (heap + the .data/.rdata it wrote) snapshotted to the build
tree for the impure specs to read.

Usage:
  PES_EXE=... python3 tools/dev/translate/snapshot.py --image <pes_full.bin>
      --out-dir <scratch/snapshot> [--trace]

The G object is created the way the game does it (FUN_140a48650):
    G  = allocator(0x18, 0x2e58)          // 11864 bytes
    FUN_140a44940(G)                       // set 16 vtables + class bytes, zero inits
    FUN_140a4a300(G)                       // run 40 sub-constructors + player loops
We do not run the allocator for real (its per-class free-list state is
heap-resident at dump time and not reconstructible statically); it is
replaced by a bump allocator. The 4 external calls in the chain
(FUN_1403cf8b0 string-format, FUN_141b55730 log-reg, FUN_140a41170 name
lookup, FUN_141e53520 label helper) feed name strings / a log, not the match
state the impure functions read; each is stubbed with a modelled result.
"""
import argparse
import json
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from diffharness.run import align_up, flat_sections  # noqa: E402
import diffharness.run as rn  # noqa: E402

from unicorn import Uc, UC_ARCH_X86, UC_MODE_64                   # noqa: E402
from unicorn import UC_HOOK_CODE, UC_HOOK_MEM_READ_UNMAPPED, \
    UC_HOOK_MEM_WRITE_UNMAPPED                                    # noqa: E402
from unicorn.x86_const import UC_X86_REG_RAX, UC_X86_REG_RIP, \
    UC_X86_REG_RSP, UC_X86_REG_RCX, UC_X86_REG_RDX                 # noqa: E402

# G setup VAs (see docs/re/pes-re-match-setup.md).
G_SIZE = 0x2e58
G_CTOR_INIT = 0x140a44940     # FUN_140a44940: vtables + class bytes + zero inits (pure)
G_CTOR_BLD = 0x140a4a300      # FUN_140a4a300: sub-constructors + player loops
# Allocator (replaced by bump). FUN_1403fb410(cls, size, align) -> ptr.
ALLOC_VA = 0x1403fb410
ALLOC_RETRY = 0x1403fb860
# External calls stubbed (name/log, not match state).
STR_FMT = 0x1403cf8b0         # (out, cap, fmt, arg) -> writes name into out
LOG_REG = 0x141b55730         # (buf, buf2, obj, fn) -> name/log registration
NAME_LKP = 0x140a41170        # (local) -> name byte (returns non-null ptr)
LABEL_HLP = 0x141e53520       # (x) -> int label (returns a small int)

# Heap window for the bump allocator (fresh G + every sub-allocation).
HEAP_BASE = 0x500000000
HEAP_SIZE = 0x8000000          # 128 MiB, enough for G + ~40 sub-objects
# Scratch window the name-lookup stub can safely return into (a zeroed byte).
SCRATCH = 0x600000000


class SnapshotBuilder:
    def __init__(self, image_path, out_dir, trace=False):
        self.image = image_path
        self.sections = flat_sections(image_path)
        self.out_dir = out_dir
        self.trace = trace
        self.offs = {}            # allocator offset -> VA of each alloc
        self.fault = None
        self.last_rip = 0
        self.entries = []          # (entry_va) every executed instruction
        self.heap = HEAP_BASE
        self.alloc_count = 0
        self.mu = None

    # -- heap -------------------------------------------------------------
    def _bump(self, size, align=0):
        size = size or 0x10
        a = align if align >= 0x10 else 0x10
        self.heap = (self.heap + a - 1) & ~(a - 1)
        va = self.heap
        self.heap += (size + a - 1) & ~(a - 1)
        if self.heap > HEAP_BASE + HEAP_SIZE:
            self.fault = "bump heap exhausted at %#x (size %#x)" % (va, size)
        return va

    # -- hooks ------------------------------------------------------------
    def install(self):
        mu = self.mu
        self.entries = []
        self.fault = None
        self.last_rip = 0
        if getattr(self, "_hooks", False):
            return  # hook_add is cumulative; install exactly once
        self._hooks = True

        def on_code(uc, addr, size, data):
            if addr == getattr(self, "RET_VA", 0):
                uc.emu_stop()
                return
            if (self.last_rip and 0x140000000 <= self.last_rip < 0x160000000
                    and not (0x140000000 <= addr < 0x160000000)):
                print("ESCAPE from %#x -> %#x" % (self.last_rip, addr),
                      file=sys.stderr)
            self.last_rip = addr
            if self.trace:
                self.entries.append(addr)
                if len(self.entries) > 400000:
                    self.entries = self.entries[-40000:]
            # Stub the 4 external calls + the allocator.
            if addr == ALLOC_VA:
                # FUN_1403fb410(cls, size, align) -> bump
                size = uc.reg_read(UC_X86_REG_RDX) & 0xFFFFFFFF
                align = uc.reg_read(UC_X86_REG_R8) & 0xFF
                ptr = self._bump(size, align)
                self.alloc_count += 1
                self.offs[self.alloc_count] = ptr
                uc.reg_write(UC_X86_REG_RAX, ptr)
                self._pop_ret(uc)
                return
            if addr == ALLOC_RETRY:
                self._pop_ret(uc)
                return
            if addr == STR_FMT:
                # (out, cap, fmt, arg): write a NUL (name present, empty body)
                out = uc.reg_read(UC_X86_REG_RCX)
                try:
                    uc.mem_write(out, b"\x01\x00\x00\x00")
                except Exception:
                    pass
                uc.reg_write(UC_X86_REG_RAX, out)
                self._pop_ret(uc)
                return
            if addr == LOG_REG:
                if self.trace:
                    rsp = uc.reg_read(UC_X86_REG_RSP)
                    ret = struct.unpack("<Q", uc.mem_read(rsp, 8))[0]
                    print("LOG_REG hit rcx=%#x ret=%#x" %
                          (uc.reg_read(UC_X86_REG_RCX), ret), file=sys.stderr)
                buf = uc.reg_read(UC_X86_REG_RCX)
                try:
                    uc.mem_write(buf, b"\x01" + b"\x00" * 7)
                except Exception:
                    pass
                uc.reg_write(UC_X86_REG_RAX, buf)
                self._pop_ret(uc)
                return
            if addr == NAME_LKP:
                # returns a *byte* that is later dereferenced as (char*)ptr;
                # hand it a zeroed, mapped scratch byte so it never faults.
                uc.reg_write(UC_X86_REG_RAX, SCRATCH)
                self._pop_ret(uc)
                return
            if addr == LABEL_HLP:
                uc.reg_write(UC_X86_REG_RAX, 0)
                self._pop_ret(uc)
                return

        def on_bad(uc, access, address, size, value, data):
            tail = [hex(a) for a in self.entries[-30:]]
            print("FAULT %s @%#x lastRIP=%#x trail=%s"
                  % ("read" if data == "r" else "write", address,
                     self.last_rip, tail), file=sys.stderr)
            self.fault = "unmodelled %s at %#x (size %d, last RIP %#x)" % (
                "read" if data == "r" else "write", address, size, self.last_rip)
            uc.emu_stop()
            return True

        mu.hook_add(UC_HOOK_CODE, on_code)
        mu.hook_add(UC_HOOK_MEM_READ_UNMAPPED, on_bad, "r")
        mu.hook_add(UC_HOOK_MEM_WRITE_UNMAPPED, on_bad, "w")

    def _pop_ret(self, uc):
        rsp = uc.reg_read(UC_X86_REG_RSP)
        ret = struct.unpack("<Q", uc.mem_read(rsp, 8))[0]
        uc.reg_write(UC_X86_REG_RSP, rsp + 8)
        uc.reg_write(UC_X86_REG_RIP, ret)

    # -- build ------------------------------------------------------------
    def build_emu(self):
        mu = rn.new_x86_64()
        # a one-byte RET we use as the top-level return address (loop guard)
        self.RET_VA = 0x60010000
        mu.mem_map(self.RET_VA & ~0xFFF, 0x1000)
        mu.mem_write(self.RET_VA, b"\xc3")
        for va, size, raw in self.sections:
            base = va & ~0xFFF
            mu.mem_map(base, align_up(va + size) - base)
            with open(self.image, "rb") as f:
                f.seek(raw)
                done = 0
                while done < size:
                    n = min(1 << 24, size - done)
                    mu.mem_write(va + done, f.read(n))
                    done += n
        # heap + scratch + stack
        mu.mem_map(HEAP_BASE, HEAP_SIZE)
        mu.mem_map(SCRATCH & ~0xFFF, 0x10000)
        mu.mem_map(0x10000000, 0x20000)
        self.mu = mu
        return mu

    def run_ctor(self, entry, arg_rcx, timeout_ms=30000):
        from unicorn.x86_const import UC_X86_REG_RCX, UC_X86_REG_RIP
        mu = self.mu
        self.install()
        # stack: RSP at a scratch page whose first qword is a ret target.
        sp = 0x10000000 + 0x20000 - 0x100
        mu.mem_write(sp, struct.pack("<Q", self.RET_VA))  # ret -> RET sentinel
        mu.reg_write(UC_X86_REG_RSP, sp)
        mu.reg_write(UC_X86_REG_RCX, arg_rcx)
        self.last_rip = entry
        try:
            mu.emu_start(entry, 0, timeout=timeout_ms * 1000)
        except Exception as e:
            self.fault = self.fault or ("emu error: %s (last RIP %#x)" % (e, self.last_rip))
        return self.fault


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--image", required=True)
    ap.add_argument("--out-dir", required=True)
    ap.add_argument("--trace", action="store_true")
    args = ap.parse_args()
    os.makedirs(args.out_dir, exist_ok=True)

    sb = SnapshotBuilder(args.image, args.out_dir, args.trace)
    sb.build_emu()
    mu = sb.mu

    # G buffer (the allocator would hand it out; we bump it directly).
    g = sb._bump(G_SIZE, 0x10)
    sb.alloc_count += 1
    sb.offs[sb.alloc_count] = g

    fail = sb.run_ctor(G_CTOR_INIT, g)
    print("FUN_140a44940(G): %s" % (fail or "ok"), file=sys.stderr)
    if fail:
        return
    fail = sb.run_ctor(G_CTOR_BLD, g, timeout_ms=60000)
    print("FUN_140a4a300(G): %s (allocs=%d, last RIP %#x)"
          % (fail or "ok", sb.alloc_count, sb.last_rip), file=sys.stderr)

    # Snapshot: G + every allocated object + the .data/.rdata it wrote.
    snap = {
        "g": g,
        "heap_base": HEAP_BASE,
        "heap_used": sb.heap - HEAP_BASE,
        "alloc_count": sb.alloc_count,
        "allocs": {str(k): v for k, v in sb.offs.items()},
        "fault": sb.fault,
        "data": {},
        "objects": {},
    }
    # G itself
    try:
        blob = bytes(mu.mem_read(g, G_SIZE))
        snap["objects"]["G"] = g
        snap["data"]["G"] = blob.hex()
    except Exception as e:
        snap["objects"]["G"] = None
        snap["note_g"] = str(e)
    # every allocator object (bounded: capture up to 64 KiB of each sub-object)
    for k, va in sb.offs.items():
        if va == g:
            continue
        if not (HEAP_BASE <= va < HEAP_BASE + HEAP_SIZE):
            continue
        sz = 0x10000
        try:
            blob = bytes(mu.mem_read(va, sz))
        except Exception:
            continue
        snap["data"]["obj%d" % k] = blob.hex()
        snap["objects"]["obj%d" % k] = va
    # the .data/.rdata ranges the setup wrote (the whole file is big; we
    # snapshot the allocator-table + G-adjacent .data the ctor may touch).
    json.dump(snap, open(os.path.join(args.out_dir, "snapshot.json"), "w"))
    # raw bytes for the harness to remap (build-tree only, never committed)
    with open(os.path.join(args.out_dir, "G.bin"), "wb") as f:
        if "G" in snap["data"]:
            f.write(bytes.fromhex(snap["data"]["G"]))
    # a trace of entered instruction VAs (for the setup-path doc / coverage)
    if args.trace and sb.entries:
        fns = {}
        for a in sb.entries:
            fns["0x%X" % a] = fns.get("0x%X" % a, 0) + 1
        json.dump(fns, open(os.path.join(args.out_dir, "entries.json"), "w"))
    print("snapshot: G=%#x heap_used=0x%x allocs=%d -> %s"
          % (g, snap["heap_used"], sb.alloc_count, args.out_dir), file=sys.stderr)
    if fail:
        print("STOPPED (setup did not complete): %s" % fail, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
