#!/usr/bin/env python3
"""No translated function reads a read-only image address live: the GF runtime maps no image, so such a read is 0.

The harness maps the image flat, so a live read passes every diff. The generator must therefore bake each one, or fail
(gen_translated's lint, fed by xlate.LIVE_RO). Synthetic image with a .text section holding code and a byte table:
  A  loop pointer walk over the table (no guard, no index register: no bound the translator can see)
  B  base register holding the table address, constant displacement (a constant address: baked as an absolute read)
  C  parameter-indexed byte read (no guard)
Undeclared, A and C are findings. Declared in ro_tables.json (here: the window list), they are baked and the compiled
function, run with NO image view, answers from the table.

usage: test_image_reads.py   (harness Python: capstone; g++ on PATH)
"""
import os
import struct
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import pdata  # noqa: E402
import xlate  # noqa: E402

BASE = pdata.IMAGE_BASE
TAB_RVA = 0x1700
TAB = BASE + TAB_RVA
FA_RVA, FB_RVA, FC_RVA = 0x1600, 0x1640, 0x1680
RT = os.path.join(HERE, "..", "..", "..", "src", "pes", "translated")


def lea_rcx(rva):
    return bytes.fromhex("488d0d") + struct.pack("<i", TAB_RVA - (rva + 7))   # lea rcx,[rip+table]


FA = (lea_rcx(FA_RVA) + bytes.fromhex("31c0")                                # lea rcx,[table] ; xor eax,eax
      + bytes.fromhex("0fb611") + bytes.fromhex("85d2") + bytes.fromhex("7407")  # loop: movzx edx,[rcx]; test; jz done
      + bytes.fromhex("01d0") + bytes.fromhex("48ffc1") + bytes.fromhex("ebf2")  # add eax,edx; inc rcx; jmp loop
      + bytes.fromhex("c3"))                                                 # done: ret
FB = lea_rcx(FB_RVA) + bytes.fromhex("8b4104") + bytes.fromhex("c3")         # lea rcx,[table]; mov eax,[rcx+4]; ret
FC = lea_rcx(FC_RVA) + bytes.fromhex("0fb60411") + bytes.fromhex("c3")       # lea rcx,[table]; movzx eax,[rcx+rdx]; ret
TABLE = bytes([3, 4, 5, 0, 0x11, 0x22, 0x33, 0x44])
FUNCS = ((FA_RVA, FA), (FB_RVA, FB), (FC_RVA, FC))
MAIN = r"""
#include "pes_state.hpp"
#include <cstdio>
#include <cstdlib>
void pes_fn_%s(PesState& s);
int main(int, char** v) {
  PesState s;
  s.gpr[2] = std::strtoull(v[1], nullptr, 0);
  pes_fn_%s(s);
  std::printf("%%llx\n", (unsigned long long)s.gpr[0]);
}
"""


def image(path):
    img = bytearray(0x20000)
    struct.pack_into("<I", img, 0x3C, 0x80)
    img[0x80:0x84] = b"PE\0\0"
    struct.pack_into("<HHIIIHH", img, 0x84, 0x8664, 1, 0, 0, 0, 240, 0)  # one section
    struct.pack_into("<Q", img, 0x80 + 24 + 24, BASE)
    struct.pack_into("<II", img, 0x98 + 112 + 24, 0x400, 12 * len(FUNCS))
    for i, (rva, code) in enumerate(FUNCS):
        struct.pack_into("<III", img, 0x400 + 12 * i, rva, rva + len(code), 0x600)
        img[rva:rva + len(code)] = code
    img[0x600:0x604] = bytes([0x01, 0, 0, 0])
    # .text: read-only code and data (flags: no 0x80000000 write bit)
    img[0x188:0x188 + 40] = b".text\0\0\0" + struct.pack("<IIIIIIHHI", 0x2000, 0x1000, 0x2000, 0x1000, 0, 0, 0, 0, 0x60000020)
    img[TAB_RVA:TAB_RVA + len(TABLE)] = TABLE
    open(path, "wb").write(img)


class ImageReads(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pdata.KNOWN = frozenset()
        cls.dir = tempfile.TemporaryDirectory()
        cls.img = os.path.join(cls.dir.name, "img.bin")
        image(cls.img)

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()
        xlate._RO_WINDOWS[:] = []

    def translate(self, rva, windows):
        xlate._RO_WINDOWS[:] = windows
        va = "0x%X" % (BASE + rva)
        src, _ = xlate.translate_one(self.img, "", va, set())
        return va, src

    def run_fn(self, rva, arg, windows=None):
        va, src = self.translate(rva, windows or [(TAB, TAB + len(TABLE), None)])
        d = self.dir.name
        open(os.path.join(d, "f.cpp"), "w").write("\n".join(src) + "\n")
        open(os.path.join(d, "m.cpp"), "w").write(MAIN % (va[2:], va[2:]))
        exe = os.path.join(d, "t")
        r = subprocess.run(["g++", "-std=c++20", "-I", RT, "-o", exe, os.path.join(d, "m.cpp"), os.path.join(d, "f.cpp")],
                           capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stderr[-2000:])
        return subprocess.run([exe, str(arg)], capture_output=True, text=True).stdout.strip()

    def findings(self, rva):
        va, _ = self.translate(rva, [(0x1, 0x2, None)])  # non-empty: skips the ro_tables.json load
        return sorted(v for _, v in xlate.LIVE_RO[va])

    def test_an_undeclared_pointer_walk_is_a_finding(self):
        self.assertEqual(self.findings(FA_RVA), [TAB])

    def test_an_undeclared_parameter_index_is_a_finding(self):
        self.assertEqual(self.findings(FC_RVA), [TAB])

    def test_a_constant_displacement_is_baked_not_flagged(self):
        self.assertEqual(self.findings(FB_RVA), [])
        self.assertEqual(self.run_fn(FB_RVA, 0), "44332211")

    def test_the_lint_lists_function_site_and_address(self):
        va, _ = self.translate(FA_RVA, [(0x1, 0x2, None)])
        (fn, site, read), = xlate.live_reads({va: xlate.LIVE_RO[va]})
        self.assertEqual((fn, read), (va, TAB))
        self.assertEqual(site, BASE + FA_RVA + 9)  # the movzx in the loop

    def test_a_declared_pointer_walk_reads_the_table_with_no_image_view(self):
        self.assertEqual(self.run_fn(FA_RVA, 0), "c")  # 3 + 4 + 5, stops at the 0 byte
        self.assertEqual(xlate.LIVE_RO["0x%X" % (BASE + FA_RVA)], set())

    def test_a_declared_parameter_index_reads_the_table_with_no_image_view(self):
        self.assertEqual([self.run_fn(FC_RVA, i) for i in (0, 2, 7)], ["3", "5", "44"])
        self.assertEqual(xlate.LIVE_RO["0x%X" % (BASE + FC_RVA)], set())

    def test_overlapping_declared_tables_bake_their_hull(self):
        # both hold the read's start; the wider one (-8..+4) alone would miss index 7, the other (0..+8) holds it
        self.assertEqual(self.run_fn(FC_RVA, 7, [(TAB - 8, TAB + 4, None), (TAB, TAB + 8, None)]), "44")

    def test_a_table_served_by_a_runtime_binding_stays_live_and_is_not_a_finding(self):
        bound = [(TAB, TAB + len(TABLE), "a binding")]
        va, src = self.translate(FC_RVA, bound)
        self.assertEqual(xlate.LIVE_RO[va], set())
        self.assertIn("s.rd(", "\n".join(src))


if __name__ == "__main__":
    unittest.main()
