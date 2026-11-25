#!/usr/bin/env python3
"""test_slim_footprint.py: footprint slim (slim.py) on synthetic captures.

The capture is one game-image page with a hooked entry that calls CALLEE; CALLEE reads one heap page, and the heap's
other pages are never read. The heap is two 256 KiB blocks (the emulator maps a block on first touch). Behaviours checked:
  1. the footprint runs PES's own bytes, so the page CALLEE reads is kept even where a GF stand-in for CALLEE would skip it;
  2. a footprint run that does not return cleanly (CALLEE faults) keeps the capture full;
  3. a trimmed capture records its dropped ranges and the archive-relative path of its full copy, and a read of a dropped
     page is refused naming that copy and the restore command;
  4. the full original is archived (packed, verified) and removed locally only after that, and restores byte for byte;
  5. the page of a module's export entry is kept although no replay reads it: the import model that replaces the entry
     is only mapped where that page exists, so a slim without it refuses the call.
Usage: GF_DIFF_PYTHON=<venv python> python3 test_slim_footprint.py      (exit 0 green, 1 red)
"""
import json
import os
import shutil
import struct
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "translate"))
import frame_trace as ft  # noqa: E402
import full_snapshot as fs  # noqa: E402
import slim  # noqa: E402

PAGE = 0x1000
BLOCK = 0x40000  # frame_trace.LazyMap maps this much on each fault
IMG = fs.IMAGE_BASE
CODE, CALLEE, RET = IMG + 0x1000, IMG + 0x1100, IMG + 0x1800
HEAP, HEAP_SIZE, STACK, TEB = 0x20000000, 2 * BLOCK, 0x7FF00000, 0x30000000
HEAP_READ = HEAP + 2 * PAGE  # the page CALLEE reads: not the first page, which identity_reads always keeps
HEAP_DATA = b"PESHEAP!"
HEAP_UNREAD = HEAP + BLOCK + 0x1000  # block 1, past the page the neighbour rule keeps: never read by any call
CALLEE_READS = b"\x48\x8b\x01\xc3"  # mov rax, [rcx]; ret
CALLEE_FAULTS = b"\x0f\x0b"  # ud2
DLL, DLL_EXPORT_ENTRY = 0x6FFF0000, 0x5000  # a module; its export 'memset' enters on page 5, which no replay reads


def entry_code():
    movabs = b"\x48\xb9" + struct.pack("<Q", HEAP_READ)  # mov rcx, HEAP_READ
    call = b"\xe8" + struct.pack("<i", CALLEE - (CODE + len(movabs) + 5))  # call CALLEE
    return movabs + call + b"\xc3"


def make_capture(cap, callee):
    """mem.bin (image header and code, the heap, stack and TEB pages), index.json and call.bin of one call of CODE."""
    img = bytearray(0x2000)
    img[0:2] = b"MZ"
    struct.pack_into("<I", img, 0x3C, 0x80)
    img[0x80:0x84] = b"PE\0\0"
    struct.pack_into("<I", img, 0x80 + 80, 0x2000)  # OptionalHeader.SizeOfImage
    img[0x1000:0x1000 + len(entry_code())] = entry_code()
    img[0x1100:0x1100 + len(callee)] = callee
    heap = bytearray(HEAP_SIZE)
    heap[2 * PAGE:2 * PAGE + len(HEAP_DATA)] = HEAP_DATA
    stack = bytearray(PAGE)
    struct.pack_into("<Q", stack, 0x800, RET)  # the return address at [rsp]
    teb = bytearray(PAGE)
    with open(os.path.join(cap, "mem.bin"), "wb") as f:
        f.write(bytes(img) + bytes(heap) + bytes(stack) + bytes(teb))

    def mapping(va, size, perm, off):
        return {"va": va, "size": size, "perm": perm, "off": off, "name": "", "deferred": False, "device": False}

    maps = sorted([mapping(IMG, 0x2000, "r-xp", 0), mapping(HEAP, HEAP_SIZE, "rw-p", 0x2000),
                   mapping(STACK, PAGE, "rw-p", 0x2000 + HEAP_SIZE),
                   mapping(TEB, PAGE, "rw-p", 0x2000 + HEAP_SIZE + PAGE)],
                  key=lambda m: m["va"])  # a snapshot index lists its mappings in VA order
    with open(os.path.join(cap, "index.json"), "w") as f:
        json.dump({"mappings": maps, "holes": [], "rv": {"hook": "synthetic", "entry_va": CODE, "teb": TEB,
                                                          "flags": 0x202, "mxcsr": 0x1F80}}, f)
    gprs = [0] * 16
    gprs[4] = STACK + 0x800  # rsp
    with open(os.path.join(cap, "call.bin"), "wb") as f:
        f.write(ft.CALL_REC.pack(*gprs, *([0] * 32), RET))


def make_module(cap):
    """A six-page PE module at DLL with one export, 'memset', entering page 5; the export directory is on page 2."""
    pages = [bytearray(PAGE) for _ in range(6)]
    h = pages[0]
    h[0:2] = b"MZ"
    struct.pack_into("<I", h, 0x3C, 0x80)
    h[0x80:0x84] = b"PE\0\0"
    struct.pack_into("<H", h, 0x94, 240)  # SizeOfOptionalHeader
    struct.pack_into("<H", h, 0x98, 0x20B)  # PE32+
    struct.pack_into("<I", h, 0x98 + 56, 0x6000)  # SizeOfImage
    struct.pack_into("<I", h, 0x98 + 60, 0x400)  # SizeOfHeaders
    struct.pack_into("<II", h, 0x98 + 112, 0x2000, 0x100)  # export data directory
    e = pages[2]  # the export directory at RVA 0x2000
    struct.pack_into("<I", e, 12, 0x2100)  # Name
    struct.pack_into("<I", e, 20, 1)  # NumberOfFunctions
    struct.pack_into("<I", e, 24, 1)  # NumberOfNames
    struct.pack_into("<IIII", e, 28, 0x2200, 0x2208, 0x2210, 0)  # functions, names, ordinals
    e[0x100:0x100 + 17] = b"VCRUNTIME140.dll\0"
    struct.pack_into("<I", e, 0x200, DLL_EXPORT_ENTRY)
    struct.pack_into("<I", e, 0x208, 0x2220)
    struct.pack_into("<H", e, 0x210, 0)
    e[0x220:0x227] = b"memset\0"
    pages[5][0:4] = b"\x90" * 4
    data = b"".join(bytes(p) for p in pages)
    with open(os.path.join(cap, "mem.bin"), "wb") as f:
        f.write(data)
    m = {"va": DLL, "size": len(data), "perm": "r--p", "off": 0, "name": "", "deferred": False, "device": False}
    with open(os.path.join(cap, "index.json"), "w") as f:
        json.dump({"mappings": [m], "holes": []}, f)
    return len(data)


class StandInForCallee:
    """A GF stand-in for CALLEE: the stub table returns at once and CALLEE's bytes never run (stubs.json's 'return')."""

    def __enter__(self):
        self.old = ft.stub_table.load
        ft.stub_table.load = lambda: {CALLEE: {"va": "0x%X" % CALLEE, "do": "return"}}
        return self

    def __exit__(self, *exc):
        ft.stub_table.load = self.old


class FootprintSlim(unittest.TestCase):
    def setUp(self):
        os.environ.pop("PES_EXE", None)  # SnapMem reads the game image from PES_EXE; these captures carry their own
        self.tmp = tempfile.mkdtemp(prefix="slim_footprint_")
        self.addCleanup(shutil.rmtree, self.tmp, ignore_errors=True)
        self.hooks = os.path.join(self.tmp, "hooks.cfg")
        open(self.hooks, "w").close()
        self.specs = os.path.join(self.tmp, "specs")
        os.makedirs(self.specs)
        self.work = os.path.join(self.tmp, "work")
        self.archive = os.path.join(self.tmp, "archive")

    def capture(self, seq, callee=CALLEE_READS):
        cap = os.path.join(self.tmp, "live2", "pid7", seq)
        os.makedirs(cap)
        make_capture(cap, callee)
        return cap

    def trim(self, cap):
        return slim.trim(cap, self.hooks, self.specs, self.work, self.archive)

    def test_footprint_keeps_the_page_a_standin_would_skip(self):
        cap = self.capture("seq1")
        with StandInForCallee():
            status, _, _ = self.trim(cap)
        self.assertTrue(status.startswith("trimmed"), status)
        self.assertEqual(fs.SnapMem(cap).read(HEAP_READ, len(HEAP_DATA)), HEAP_DATA)

    def test_a_run_that_does_not_return_keeps_the_capture_full(self):
        cap = self.capture("seq2", callee=CALLEE_FAULTS)
        mem = os.path.join(cap, "mem.bin")
        with open(mem, "rb") as f:
            before = f.read()
        status, _, _ = self.trim(cap)
        self.assertTrue(status.startswith("kept full"), status)
        with open(mem, "rb") as f:
            self.assertEqual(f.read(), before)
        self.assertFalse(os.path.exists(os.path.join(cap, "mem.bin.zst")))

    def test_a_trimmed_capture_records_its_drops_and_names_its_archived_copy(self):
        cap = self.capture("seq3")
        status, _, _ = self.trim(cap)
        self.assertTrue(status.startswith("trimmed"), status)
        with open(os.path.join(cap, "index.json")) as f:
            rec = json.load(f)["slim"]
        rel = "pid7/seq3/mem.bin.zst"
        self.assertEqual(rec["full"], rel)
        self.assertTrue(any(va <= HEAP_UNREAD < va + n for va, n in rec["dropped"]), rec["dropped"])
        self.assertFalse(any(va <= HEAP_READ < va + n for va, n in rec["dropped"]), rec["dropped"])
        sm = fs.SnapMem(cap)
        self.assertIsNone(sm.read(HEAP_UNREAD, 8))
        msg = sm.dropped_reason(HEAP_UNREAD)
        self.assertIn("GF_CAPTURE_ARCHIVE", msg)
        self.assertIn(rel, msg)
        self.assertIn("slim.py --restore " + cap, msg)

    def test_the_full_original_is_archived_and_restores_byte_for_byte(self):
        cap = self.capture("seq4")
        with open(os.path.join(cap, "mem.bin"), "rb") as f:
            original = f.read()
        status, _, _ = self.trim(cap)
        self.assertTrue(status.startswith("trimmed"), status)
        link = os.path.join(cap, "mem.bin.zst")
        archived = os.path.join(self.archive, "pid7", "seq4", "mem.bin.zst")
        self.assertTrue(os.path.islink(link))
        self.assertTrue(os.path.exists(archived))
        self.assertEqual(os.path.realpath(link), os.path.realpath(archived))
        self.assertFalse(os.path.exists(os.path.join(cap, "mem.full")))  # the full copy leaves the capture
        slim.restore(cap)
        with open(os.path.join(cap, "mem.bin"), "rb") as f:
            self.assertEqual(f.read(), original)
        with open(os.path.join(cap, "index.json")) as f:
            self.assertNotIn("slim", json.load(f))

    def test_an_export_entry_page_is_kept_although_no_replay_reads_it(self):
        cap = os.path.join(self.tmp, "live2", "pid7", "seq5")
        os.makedirs(cap)
        size = make_module(cap)
        keep = slim.keep_for(cap, set(), size)
        self.assertIn(DLL_EXPORT_ENTRY, keep)


if __name__ == "__main__":
    unittest.main()
