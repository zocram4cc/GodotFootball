#!/usr/bin/env python3
"""A function's body is its .pdata range plus the unwind chunks chained to it: every decoder reads exactly that.

Synthetic image (no game bytes), VA = base + file offset:
  0x1200 H   .pdata root   test rax,rax ; jne 0x1230 ; ret ; [0x1206] mov eax,2 ; ret     (0x1206 is reached only from the cold chunk)
  0x120c..   garbage between the parts, with a `jmp 0x1301` (an address inside G)          (the old decode ran through it)
  0x1230 C1  chunk of H    mov eax,1                                                       (chained to H)
  0x1235 C2  chunk of C1   jmp 0x1206                                                      (contiguous with C1; chained to C1: a chain of two)
  0x1300 G   .pdata root   mov rax,rbx ; ret
Regression: PES 0x14084DF20 (range ends at 0x14084E20F with `jmp 0x140857010`) decoded on through the 64 KiB window and
reached `jmp 0x140850CA6`, an address inside .pdata function 0x140850A79; the closure walk then refused it.
The real-image tests need PES_EXE (the unpacked image) and skip without it.

usage: test_extent.py   (harness Python: capstone)
"""
import os
import struct
import sys
import tempfile
import unittest

import capstone

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import closure  # noqa: E402
import pdata  # noqa: E402
import xlate  # noqa: E402

BASE = pdata.IMAGE_BASE
H, C1, C2, G = BASE + 0x1200, BASE + 0x1230, BASE + 0x1235, BASE + 0x1300
H_ISLAND = BASE + 0x1206
GAP = range(BASE + 0x120C, BASE + 0x1230)
REAL = os.environ.get("PES_EXE")


def build_image(path):
    img = bytearray(0x20000)
    struct.pack_into("<I", img, 0x3C, 0x80)
    img[0x80:0x84] = b"PE\0\0"
    struct.pack_into("<HHIIIHH", img, 0x84, 0x8664, 0, 0, 0, 0, 240, 0)
    root_u, c1_u, c2_u = 0x600, 0x610, 0x630
    entries = [(0x1200, 0x120C, root_u), (0x1230, 0x1235, c1_u), (0x1235, 0x123A, c2_u), (0x1300, 0x1304, root_u)]
    struct.pack_into("<II", img, 0x98 + 112 + 24, 0x400, 12 * len(entries))
    for i, e in enumerate(entries):
        struct.pack_into("<III", img, 0x400 + 12 * i, *e)
    img[root_u:root_u + 4] = bytes([0x01, 0, 0, 0])  # version 1, no flags, no codes
    img[c1_u:c1_u + 4] = bytes([0x21, 0, 0, 0])      # flags 4 = UNW_FLAG_CHAININFO, no codes
    struct.pack_into("<III", img, c1_u + 4, 0x1200, 0x120C, root_u)   # the parent RUNTIME_FUNCTION follows the codes
    img[c2_u:c2_u + 4] = bytes([0x21, 0, 0, 0])
    struct.pack_into("<III", img, c2_u + 4, 0x1230, 0x1235, c1_u)
    for off in range(0x1000, 0x1400):
        img[off] = 0xCC
    img[0x1200:0x120C] = bytes.fromhex("4885c0" "752b" "c3" "b802000000" "c3")
    img[0x1210:0x1215] = b"\xe9" + struct.pack("<i", 0x1301 - 0x1215)
    img[0x1230:0x1235] = bytes.fromhex("b801000000")
    img[0x1235:0x123A] = b"\xe9" + struct.pack("<i", 0x1206 - 0x123A)
    img[0x1300:0x1304] = bytes.fromhex("4889d8c3")
    with open(path, "wb") as f:
        f.write(img)


def addrs(lines):
    return [a for a, _, _ in lines]


class SyntheticExtent(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.dir = tempfile.TemporaryDirectory()
        cls.image = os.path.join(cls.dir.name, "img.bin")
        build_image(cls.image)
        cls.f = open(cls.image, "rb")
        cls.p = pdata.Pdata(cls.f)

    @classmethod
    def tearDownClass(cls):
        cls.f.close()
        cls.dir.cleanup()

    def test_extent_is_the_range_plus_its_chunks_with_contiguous_parts_joined(self):
        self.assertEqual(self.p.extent(H), [(H, H + 0xC), (C1, C1 + 0xA)])
        self.assertEqual(self.p.extent(G), [(G, G + 4)])

    def test_a_chunk_belongs_to_the_root_it_chains_to_and_is_no_function_start(self):
        for chunk_addr in (C1, C1 + 2, C2, C2 + 5 - 1):
            self.assertEqual(self.p.containing(chunk_addr), H)
        self.assertFalse(self.p.is_boundary(C1))
        self.assertFalse(self.p.is_boundary(C2))
        self.assertFalse(self.p.is_boundary(C2 + 2))
        self.assertTrue(self.p.is_boundary(H))
        self.assertTrue(self.p.is_boundary(G))
        self.assertIsNone(self.p.end_of(C1))
        self.assertIsNone(self.p.extent(C1))

    def test_the_generator_decode_is_the_extent_and_nothing_else(self):
        lines = xlate.capstone_decode(self.image, H)
        self.assertEqual([m for _, m, _ in lines], ["TEST", "JNE", "RET", "MOV", "RET", "MOV", "JMP"])
        self.assertEqual(addrs(lines), [H, H + 3, H + 5, H_ISLAND, H + 0xB, C1, C2])
        self.assertFalse(set(addrs(lines)) & set(GAP))

    def test_code_reached_only_from_a_chunk_is_decoded(self):
        self.assertIn(H_ISLAND, addrs(xlate.capstone_decode(self.image, H)))

    def test_the_closure_decode_agrees_and_a_jump_inside_the_extent_is_no_tail_callee(self):
        row = closure.ClosurePdata(self.f).decode_row(H, "t")
        self.assertEqual(row["size"], 7)
        self.assertEqual(row["callees"], {})

    def test_the_next_function_is_untouched(self):
        self.assertEqual([m for _, m, _ in xlate.capstone_decode(self.image, G)], ["MOV", "RET"])


@unittest.skipUnless(REAL, "PES_EXE not set")
class RealExtent(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.f = open(REAL, "rb")
        cls.p = pdata.Pdata(cls.f)

    @classmethod
    def tearDownClass(cls):
        cls.f.close()

    def decode(self, va):
        return xlate.capstone_decode(REAL, va)

    def inside(self, va, a):
        return any(b <= a < e for b, e in self.p.extent(va))

    def test_0x14084DF20_does_not_run_past_its_end(self):
        va = 0x14084DF20
        lines = self.decode(va)
        self.assertEqual(self.p.extent(va), [(va, self.p.end_of(va))])
        self.assertTrue(all(self.inside(va, a) for a in addrs(lines)))
        self.assertNotIn(0x140850CA6, [int(o[0], 16) for _, m, o in lines
                                       if m.startswith("J") and o and o[0].startswith("0x")])

    def chained(self, va, nchunks):
        ext = self.p.extent(va)
        self.assertGreater(len(ext), 1)
        self.assertEqual(self.p.end_of(va), ext[0][1])
        lines = self.decode(va)
        self.assertTrue(all(self.inside(va, a) for a in addrs(lines)))
        for begin, _ in ext[1:]:
            self.assertIn(begin, addrs(lines))  # every chunk is decoded from its first byte
            self.assertEqual(self.p.containing(begin), va)
            self.assertFalse(self.p.is_boundary(begin))

    def test_0x140007140_decodes_its_chunks(self):
        self.chained(0x140007140, 2)

    def test_0x14001175c_decodes_its_chunks(self):
        self.chained(0x14001175c, 2)

    def test_0x14002CF80_decodes_its_chunks(self):
        self.chained(0x14002CF80, 2)


if __name__ == "__main__":
    unittest.main()
