#!/usr/bin/env python3
"""JRCXZ, STC, CLC and CMC (found in 0x14039E110 and the obfuscated thunk 0x14CB479E4, reached from the Lua libraries the scripting init opens).

JRCXZ jumps when RCX is zero and touches no flag and no register; STC / CLC / CMC set, clear and flip CF. The snippets are assembled with the
host assembler, translated, compiled and run (synthetic image, no game bytes); the expected values are the instructions' definitions.

usage: test_jrcxz_stc.py   (harness Python: capstone; g++ and as on PATH)
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
import test_extent as TE  # noqa: E402
import xlate  # noqa: E402

BASE = pdata.IMAGE_BASE
F = BASE + 0x1600
RT = os.path.join(HERE, "..", "..", "..", "src", "pes", "translated")
SNIPPETS = {
    # rax = 1 when the jump is not taken, 2 when taken; the stack byte at rsp-8 proves RCX was not decremented (rcx is returned in rdx).
    "jrcxz": "jrcxz 1f\nmov eax, 1\nmov rdx, rcx\nret\n1:\nmov eax, 2\nmov rdx, rcx\nret\n",
    # rax = CF after: input rdi bit 0 selects the first operation, rsi the instruction under test by name below.
    "stc": "clc\nstc\npushfq\npop rax\nand eax, 1\nret\n",
    "clc": "stc\nclc\npushfq\npop rax\nand eax, 1\nret\n",
    "cmc0": "clc\ncmc\npushfq\npop rax\nand eax, 1\nret\n",
    "cmc1": "stc\ncmc\npushfq\npop rax\nand eax, 1\nret\n",
}
MAIN = r"""
#include "pes_state.hpp"
#include <cstdio>
#include <cstdlib>
void pes_fn_%(va)s(PesState& s);
int main(int, char** v) {
  PesState s;
  s.mem.segs.push_back({0x10000, std::vector<uint8_t>(0x100, 0)});
  s.gpr[4] = 0x10080;
  s.gpr[1] = std::strtoull(v[1], nullptr, 0);
  pes_fn_%(va)s(s);
  std::printf("%%llu %%llu\n", (unsigned long long)s.gpr[0], (unsigned long long)s.gpr[2]);
}
"""


class JrcxzAndCarryFlag(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pdata.KNOWN = frozenset()
        cls.dir = tempfile.TemporaryDirectory()
        d = cls.dir.name
        cls.exes = {}
        for name, text in SNIPPETS.items():
            asm = os.path.join(d, name + ".s")
            open(asm, "w").write(".intel_syntax noprefix\n.text\n" + text)
            obj = os.path.join(d, name + ".o")
            subprocess.run(["as", "-o", obj, asm], check=True)
            binf = os.path.join(d, name + ".bin")
            subprocess.run(["objcopy", "-O", "binary", "-j", ".text", obj, binf], check=True)
            code = open(binf, "rb").read()
            img = os.path.join(d, name + ".img")
            TE.build_image(img)
            raw = bytearray(open(img, "rb").read())
            n = struct.unpack_from("<I", raw, 0x98 + 112 + 24 + 4)[0] // 12
            struct.pack_into("<I", raw, 0x98 + 112 + 24 + 4, 12 * (n + 1))
            struct.pack_into("<III", raw, 0x400 + 12 * n, 0x1600, 0x1600 + len(code), 0x600)
            raw[0x1600:0x1600 + len(code)] = code
            open(img, "wb").write(raw)
            va = "0x%X" % F
            src, _ = xlate.translate_one(img, "", va, set())
            open(os.path.join(d, name + "_f.cpp"), "w").write("\n".join(src) + "\n")
            open(os.path.join(d, name + "_m.cpp"), "w").write(MAIN % {"va": va[2:]})
            exe = os.path.join(d, name)
            r = subprocess.run(["g++", "-std=c++20", "-I", RT, "-o", exe, os.path.join(d, name + "_m.cpp"),
                                os.path.join(d, name + "_f.cpp")], capture_output=True, text=True)
            assert r.returncode == 0, r.stderr[-2000:]
            cls.exes[name] = exe

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def run_with(self, name, rcx):
        return [int(x) for x in subprocess.run([self.exes[name], str(rcx)], capture_output=True, text=True).stdout.split()]

    def test_jrcxz_jumps_only_on_zero_and_leaves_rcx_alone(self):
        self.assertEqual(self.run_with("jrcxz", 0), [2, 0])
        self.assertEqual(self.run_with("jrcxz", 1), [1, 1])
        self.assertEqual(self.run_with("jrcxz", 0x100000000), [1, 0x100000000])  # only the low dword set: RCX, not ECX, is tested

    def test_stc_clc_cmc_set_clear_and_flip_the_carry(self):
        self.assertEqual(self.run_with("stc", 0)[0], 1)
        self.assertEqual(self.run_with("clc", 0)[0], 0)
        self.assertEqual(self.run_with("cmc0", 0)[0], 1)
        self.assertEqual(self.run_with("cmc1", 0)[0], 0)


if __name__ == "__main__":
    unittest.main()
