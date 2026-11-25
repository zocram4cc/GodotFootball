#!/usr/bin/env python3
"""test_slim_footprint.py: footprint slim (slim.py) on a synthetic capture.

The capture is one game-image page with a hooked entry that calls CALLEE; CALLEE reads one heap page and the other heap
pages are never read. Behaviours checked:
  1. the footprint runs PES's own bytes, so the page CALLEE reads is kept even where a GF stand-in for CALLEE would skip it;
  2. a footprint run that does not return cleanly (CALLEE faults) keeps the capture full;
  3. a trimmed capture records its dropped ranges and the archive-relative path of its full copy, and a read of a dropped
     page is refused naming that copy and the restore command;
  4. the full original is archived (packed, verified), not deleted, and restores byte for byte.
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
IMG = fs.IMAGE_BASE
CODE, CALLEE, RET = IMG + 0x1000, IMG + 0x1100, IMG + 0x1800
HEAP, STACK, TEB = 0x20000000, 0x7FF00000, 0x30000000
HEAP_DATA = b"PESHEAP!"
CALLEE_READS = b"\x48\x8b\x01\xc3"  # mov rax, [rcx]; ret
CALLEE_FAULTS = b"\x0f\x0b"  # ud2


def entry_code():
    movabs = b"\x48\xb9" + struct.pack("<Q", HEAP)  # mov rcx, HEAP
    call = b"\xe8" + struct.pack("<i", CALLEE - (CODE + len(movabs) + 5))  # call CALLEE
    return movabs + call + b"\xc3"


def make_capture(cap, callee):
    """mem.bin (8 pages: image header and code, 4 heap pages, stack, TEB), index.json and call.bin of one call of CODE."""
    img = bytearray(0x2000)
    img[0:2] = b"MZ"
    struct.pack_into("<I", img, 0x3C, 0x80)
    img[0x80:0x84] = b"PE\0\0"
    struct.pack_into("<I", img, 0x80 + 80, 0x2000)  # OptionalHeader.SizeOfImage
    img[0x1000:0x1000 + len(entry_code())] = entry_code()
    img[0x1100:0x1100 + len(callee)] = callee
    heap = bytearray(0x4000)
    heap[0:len(HEAP_DATA)] = HEAP_DATA
    stack = bytearray(PAGE)
    struct.pack_into("<Q", stack, 0x800, RET)  # the return address at [rsp]
    teb = bytearray(PAGE)
    with open(os.path.join(cap, "mem.bin"), "wb") as f:
        f.write(bytes(img + heap + stack + teb))

    def mapping(va, size, perm, off):
        return {"va": va, "size": size, "perm": perm, "off": off, "name": "", "deferred": False, "device": False}

    maps = [mapping(IMG, 0x2000, "r-xp", 0), mapping(HEAP, 0x4000, "rw-p", 0x2000),
            mapping(STACK, PAGE, "rw-p", 0x6000), mapping(TEB, PAGE, "rw-p", 0x7000)]
    with open(os.path.join(cap, "index.json"), "w") as f:
        json.dump({"mappings": maps, "holes": [], "rv": {"hook": "synthetic", "entry_va": CODE, "teb": TEB,
                                                          "flags": 0x202, "mxcsr": 0x1F80}}, f)
    gprs = [0] * 16
    gprs[4] = STACK + 0x800  # rsp
    with open(os.path.join(cap, "call.bin"), "wb") as f:
        f.write(ft.CALL_REC.pack(*gprs, *([0] * 32), RET))


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
        self.assertEqual(fs.SnapMem(cap).read(HEAP, len(HEAP_DATA)), HEAP_DATA)

    def test_a_run_that_does_not_return_keeps_the_capture_full(self):
        cap = self.capture("seq2", callee=CALLEE_FAULTS)
        mem = os.path.join(cap, "mem.bin")
        before = open(mem, "rb").read()
        status, _, _ = self.trim(cap)
        self.assertTrue(status.startswith("kept full"), status)
        self.assertEqual(open(mem, "rb").read(), before)
        self.assertFalse(os.path.exists(os.path.join(cap, "mem.bin.zst")))

    def test_a_trimmed_capture_records_its_drops_and_names_its_archived_copy(self):
        cap = self.capture("seq3")
        status, _, _ = self.trim(cap)
        self.assertTrue(status.startswith("trimmed"), status)
        rec = json.load(open(os.path.join(cap, "index.json")))["slim"]
        rel = "pid7/seq3/mem.bin.zst"
        self.assertEqual(rec["full"], rel)
        last = HEAP + 3 * PAGE  # never read by the call
        self.assertTrue(any(va <= last < va + n for va, n in rec["dropped"]), rec["dropped"])
        self.assertFalse(any(va <= HEAP < va + n for va, n in rec["dropped"]), rec["dropped"])
        sm = fs.SnapMem(cap)
        self.assertIsNone(sm.read(last, 8))
        msg = sm.dropped_reason(last)
        self.assertIn("GF_CAPTURE_ARCHIVE", msg)
        self.assertIn(rel, msg)
        self.assertIn("slim.py --restore " + cap, msg)

    def test_the_full_original_is_archived_and_restores_byte_for_byte(self):
        cap = self.capture("seq4")
        original = open(os.path.join(cap, "mem.bin"), "rb").read()
        status, _, _ = self.trim(cap)
        self.assertTrue(status.startswith("trimmed"), status)
        link = os.path.join(cap, "mem.bin.zst")
        archived = os.path.join(self.archive, "pid7", "seq4", "mem.bin.zst")
        self.assertTrue(os.path.islink(link))
        self.assertTrue(os.path.exists(archived))
        self.assertEqual(os.path.realpath(link), os.path.realpath(archived))
        slim.restore(cap)
        self.assertEqual(open(os.path.join(cap, "mem.bin"), "rb").read(), original)
        self.assertNotIn("slim", json.load(open(os.path.join(cap, "index.json"))))


if __name__ == "__main__":
    unittest.main()
