#!/usr/bin/env python3
"""A JMP-reg switch whose bounds check sits more than 20 instructions above it still resolves.

AnimeLayer (05-11-26): FUN_141B16140 (the anime engine's IK/modify stage) holds two switches under one `cmp esi,8; ja`; the
second JMP is ~33 instructions below the guard. xlate looked back 20, found no guard, emitted `__builtin_trap() /* UNRESOLVED JMP */`
and the translated phase 5 died with SIGILL where PES dispatched to a case. Synthetic image (no game bytes): `cmp edx,1; ja default;`
30 filler instructions, then the table load and the JMP; the compiled function must reach each case and the default.

usage: test_switch_far_guard.py   (harness Python: capstone; g++ on PATH)
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
FRVA, JT_RVA, FILLERS = 0x1600, 0x1900, 30
F = BASE + FRVA
RT = os.path.join(HERE, "..", "..", "..", "src", "pes", "translated")
HEAD = bytes.fromhex("83fa01") + bytes.fromhex("0f87") + b"\0\0\0\0"          # cmp edx,1 ; ja default (patched below)
FILL = bytes.fromhex("41b901000000") * FILLERS                                  # mov r9d,1
LEA_AT = len(HEAD) + len(FILL)
BODY = (bytes.fromhex("4c8d05") + struct.pack("<i", -(FRVA + LEA_AT + 7))       # lea r8,[image base]
        + bytes.fromhex("89d0")                                                # mov eax,edx
        + bytes.fromhex("418b8c80") + struct.pack("<I", JT_RVA)                # mov ecx,[r8+rax*4+jumptable]
        + bytes.fromhex("4c01c1")                                              # add rcx,r8
        + bytes.fromhex("ffe1"))                                               # jmp rcx
CASE0, CASE1, DEFAULT = len(HEAD) + len(FILL) + len(BODY), 0, 0
CASE1 = CASE0 + 6
DEFAULT = CASE1 + 6
CODE = (HEAD[:-4] + struct.pack("<i", DEFAULT - len(HEAD)) + FILL + BODY
        + bytes.fromhex("b811000000c3") + bytes.fromhex("b822000000c3") + bytes.fromhex("31c0c3"))
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
    struct.pack_into("<HHIIIHH", img, 0x84, 0x8664, 1, 0, 0, 0, 240, 0)
    struct.pack_into("<Q", img, 0x80 + 24 + 24, BASE)
    struct.pack_into("<II", img, 0x98 + 112 + 24, 0x400, 12)
    struct.pack_into("<III", img, 0x400, FRVA, FRVA + len(CODE), 0x600)
    img[0x600:0x604] = bytes([0x01, 0, 0, 0])
    img[0x188:0x188 + 40] = b".text\0\0\0" + struct.pack("<IIIIIIHHI", 0x2000, 0x1000, 0x2000, 0x1000, 0, 0, 0, 0, 0x60000020)
    img[FRVA:FRVA + len(CODE)] = CODE
    struct.pack_into("<II", img, JT_RVA, FRVA + CASE0, FRVA + CASE1)
    open(path, "wb").write(img)


class FarGuardSwitch(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pdata.KNOWN = frozenset()
        cls.dir = tempfile.TemporaryDirectory()
        d = cls.dir.name
        img = os.path.join(d, "img.bin")
        image(img)
        va = "0x%X" % F
        src, _ = xlate.translate_one(img, "", va, set())
        cls.text = "\n".join(src)
        open(os.path.join(d, "f.cpp"), "w").write(cls.text + "\n")
        open(os.path.join(d, "m.cpp"), "w").write(MAIN % (va[2:], va[2:]))
        cls.exe = os.path.join(d, "t")
        r = subprocess.run(["g++", "-std=c++20", "-I", RT, "-o", cls.exe, os.path.join(d, "m.cpp"),
                            os.path.join(d, "f.cpp")], capture_output=True, text=True)
        assert r.returncode == 0, r.stderr[-2000:]

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def answer(self, index):
        return subprocess.run([self.exe, str(index)], capture_output=True, text=True).stdout.strip()

    def test_each_case_is_reached(self):
        self.assertEqual([self.answer(0), self.answer(1)], ["11", "22"])

    def test_past_the_guard_is_the_default(self):
        self.assertEqual(self.answer(2), "0")

    def test_the_jump_is_resolved(self):
        self.assertNotIn("UNRESOLVED", self.text)


if __name__ == "__main__":
    unittest.main()
