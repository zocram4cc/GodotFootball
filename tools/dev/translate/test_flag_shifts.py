#!/usr/bin/env python3
"""SHLD, SHRD, RCL, RCR, PUSHFQ and POPFQ against the CPU (found in the obfuscated .impdata thunks 0x143E98DA0 / 0x143DFA780 / 0x14EDB812B, which
the layer's closure reaches from the scripting init FUN_14029E940: they hold these instructions and nothing else the translator lacked).

Each case is one snippet `bt r8,0 ; <instruction> ; pushfq ; pop rax ; mov [r9],rdi ; ret`. The translated function runs on a PesState and
the same bytes run natively (assembled with the host assembler); the result register and the flags the translator tracks must agree: CF always,
ZF/SF for the double shifts, OF for a count of 1. Synthetic image, no game bytes.

usage: test_flag_shifts.py   (harness Python: capstone; g++ and as on PATH)
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
CF, ZF, SF, OF = 0x1, 0x40, 0x80, 0x800
# (name, instruction text, flags compared when the count is not 1, flags compared when it is)
CASES = [
    ("shrd", "shrd rdi, rsi, cl", CF | ZF | SF, CF | ZF | SF | OF),
    ("shld", "shld rdi, rsi, cl", CF | ZF | SF, CF | ZF | SF | OF),
    ("rcr", "rcr rdi, cl", CF, CF | OF),
    ("rcl", "rcl rdi, cl", CF, CF | OF),
    ("rcr32", "rcr edi, cl", CF, CF | OF),
    ("rcl32", "rcl edi, cl", CF, CF | OF),
]
TAIL = "bt r8, 0\n{ins}\npushfq\npop rax\nmov [r9], rdi\nret\n"
MAIN = r"""
#include "pes_state.hpp"
#include <cstdio>
#include <cstdlib>
void pes_fn_%(va)s(PesState& s);
extern "C" unsigned long native(unsigned long rdi, unsigned long rsi, unsigned long rdx, unsigned long rcx, unsigned long r8, unsigned long* out);
int main(int, char** v) {
  unsigned long rdi = std::strtoull(v[1], nullptr, 0), rsi = std::strtoull(v[2], nullptr, 0), cl = std::strtoull(v[3], nullptr, 0), cf = std::strtoull(v[4], nullptr, 0);
  unsigned long nout = 0, nflags = native(rdi, rsi, 0, cl, cf, &nout);
  PesState s;
  s.mem.segs.push_back({0x10000, std::vector<uint8_t>(0x100, 0)});
  s.gpr[4] = 0x10080;
  s.gpr[7] = rdi; s.gpr[6] = rsi; s.gpr[1] = cl; s.gpr[8] = cf; s.gpr[9] = 0x10000;
  pes_fn_%(va)s(s);
  std::printf("%%lx %%lx %%lx %%lx\n", nout, nflags, s.rd(0x10000, 8), s.gpr[0]);
}
"""


class FlagShifts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pdata.KNOWN = frozenset()
        cls.dir = tempfile.TemporaryDirectory()
        d = cls.dir.name
        cls.exes = {}
        for name, ins, _, _ in CASES:
            asm = os.path.join(d, name + ".s")
            open(asm, "w").write(".intel_syntax noprefix\n.text\n.globl snippet\nsnippet:\n" + TAIL.format(ins=ins))
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
            # The native reference: the same snippet as a SysV function (rdi, rsi, rdx, rcx, r8, r9 are the arguments the snippet reads).
            nat = os.path.join(d, name + "_n.s")
            open(nat, "w").write(".intel_syntax noprefix\n.text\n.globl native\nnative:\n" + TAIL.format(ins=ins))
            exe = os.path.join(d, name)
            r = subprocess.run(["g++", "-std=c++20", "-I", RT, "-o", exe, os.path.join(d, name + "_m.cpp"),
                                os.path.join(d, name + "_f.cpp"), nat], capture_output=True, text=True)
            assert r.returncode == 0, r.stderr[-2000:]
            cls.exes[name] = exe

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def check(self, name, rdi, rsi, cl, cf):
        _, _, many, one = next(c for c in CASES if c[0] == name)
        out = subprocess.run([self.exes[name], str(rdi), str(rsi), str(cl), str(cf)], capture_output=True, text=True).stdout.split()
        nres, nflags, tres, tflags = (int(x, 16) for x in out)
        masked = cl & (31 if "32" in name else 63)
        # A zero count leaves ZF/SF/OF as they were: the CPU's are not ours to compare.
        mask = CF if masked == 0 else (many if masked != 1 else one)
        self.assertEqual(tres, nres, "%s rdi=%#x rsi=%#x cl=%d cf=%d: result" % (name, rdi, rsi, cl, cf))
        self.assertEqual(tflags & mask, nflags & mask, "%s rdi=%#x rsi=%#x cl=%d cf=%d: flags" % (name, rdi, rsi, cl, cf))

    def test_every_instruction_matches_the_cpu_over_a_grid(self):
        values = [0, 1, 0x8000000000000000, 0xFFFFFFFFFFFFFFFF, 0x123456789ABCDEF0, 0x80000000, 0x7FFFFFFF, 0xDEADBEEFCAFEF00D]
        for name, _, _, _ in CASES:
            for rdi in values:
                for rsi in (0, 0xFFFFFFFFFFFFFFFF, 0x0F0F0F0F5A5A5A5A):
                    for cl in (1, 2, 5, 31, 33, 63):  # not 32 / 64: a masked count of 0 leaves the destination (and its upper half) to the CPU's quirk
                        for cf in (0, 1):
                            self.check(name, rdi, rsi, cl, cf)


if __name__ == "__main__":
    unittest.main()
