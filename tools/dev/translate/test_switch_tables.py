#!/usr/bin/env python3
"""A two-level MSVC switch dispatches in the GF runtime, which has no image view.

0x140A63DA0 (the judge's played-the-ball test) is `lea eax,[rdx-8]; cmp eax,N; ja default; lea r8,[image]; cdqe;
movzx eax,[r8+rax+bytemap]; mov ecx,[r8+rax*4+jumptable]; add rcx,r8; jmp rcx`. The generator baked neither table load
(the CDQE hides the guard, and the second index is a byte-map value), so the generated code read 0 from both and every
action fell to the default case: a played ball scored as "no ball played" and the foul judge never saw a foul. The harness
mapped the whole image, so its diffs were bit-exact all the same. Synthetic image, a translated function compiled and run
with no image view (no game bytes).

usage: test_switch_tables.py   (harness Python: capstone; g++ on PATH)
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
FRVA = 0x1600
F = BASE + FRVA
MAP_RVA, JT_RVA = 0x1640, 0x1650
RT = os.path.join(HERE, "..", "..", "..", "src", "pes", "translated")
CODE = (bytes.fromhex("8d42f8")                                    # lea eax,[rdx-8]
        + bytes.fromhex("83f805")                                  # cmp eax,5
        + bytes.fromhex("772b")                                    # ja default (0x33)
        + bytes.fromhex("4c8d05") + struct.pack("<i", -(FRVA + 0xF))  # lea r8,[image base]
        + bytes.fromhex("4898")                                    # cdqe
        + bytes.fromhex("410fb68400") + struct.pack("<I", MAP_RVA)   # movzx eax,byte [r8+rax+map]
        + bytes.fromhex("418b8c80") + struct.pack("<I", JT_RVA)      # mov ecx,[r8+rax*4+jumptable]
        + bytes.fromhex("4c01c1")                                  # add rcx,r8
        + bytes.fromhex("ffe1")                                    # jmp rcx
        + bytes.fromhex("b811000000c3")                            # case 0: mov eax,0x11 ; ret   (0x27)
        + bytes.fromhex("b822000000c3")                            # case 1: mov eax,0x22 ; ret   (0x2D)
        + bytes.fromhex("31c0c3"))                                 # default: xor eax,eax ; ret   (0x33)
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
    struct.pack_into("<II", img, 0x98 + 112 + 24, 0x400, 12)
    struct.pack_into("<III", img, 0x400, FRVA, FRVA + len(CODE), 0x600)
    img[0x600:0x604] = bytes([0x01, 0, 0, 0])
    # .text: read-only code and data (flags: no 0x80000000 write bit)
    img[0x188:0x188 + 40] = b".text\0\0\0" + struct.pack("<IIIIIIHHI", 0x2000, 0x1000, 0x2000, 0x1000, 0, 0, 0, 0, 0x60000020)
    img[FRVA:FRVA + len(CODE)] = CODE
    img[MAP_RVA:MAP_RVA + 6] = bytes([0, 0, 0, 1, 1, 1])                  # actions 8..10 -> case 0, 11..13 -> case 1
    struct.pack_into("<II", img, JT_RVA, FRVA + 0x27, FRVA + 0x2D)
    open(path, "wb").write(img)


class SwitchTables(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pdata.KNOWN = frozenset()
        cls.dir = tempfile.TemporaryDirectory()
        d = cls.dir.name
        img = os.path.join(d, "img.bin")
        image(img)
        va = "0x%X" % F
        src, _ = xlate.translate_one(img, "", va, set())
        open(os.path.join(d, "f.cpp"), "w").write("\n".join(src) + "\n")
        open(os.path.join(d, "m.cpp"), "w").write(MAIN % (va[2:], va[2:]))
        cls.exe = os.path.join(d, "t")
        r = subprocess.run(["g++", "-std=c++20", "-I", RT, "-o", cls.exe, os.path.join(d, "m.cpp"),
                            os.path.join(d, "f.cpp")], capture_output=True, text=True)
        assert r.returncode == 0, r.stderr[-2000:]

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def answer(self, action):
        return subprocess.run([self.exe, str(action)], capture_output=True, text=True).stdout.strip()

    def test_first_group_reaches_its_case(self):
        self.assertEqual([self.answer(a) for a in (8, 9, 10)], ["11"] * 3)

    def test_second_group_reaches_its_case(self):
        self.assertEqual([self.answer(a) for a in (11, 12, 13)], ["22"] * 3)

    def test_past_the_guard_is_the_default(self):
        self.assertEqual(self.answer(0x20), "0")


if __name__ == "__main__":
    unittest.main()
