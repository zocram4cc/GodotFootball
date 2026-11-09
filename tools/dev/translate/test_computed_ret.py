#!/usr/bin/env python3
"""`push target; ret` in the packer's obfuscated code (.impdata) is a jump to `target`, not a return (found in the thunks 0x143E98DA0 /
0x143DFA780 the Lua scripting init calls: the translator returned at once, leaving the pushed word on the stack and the real function
unrun, so the caller's registers came back wrong).

Synthetic image: F = `push <T>; ret` (T an address the test dispatches to a function that sets rax), compiled with a test dispatch table and
run. In the packer's range the entered function runs and F returns with the stack as it was; outside that range `ret` still just returns.
usage: test_computed_ret.py   (harness Python: capstone; g++ on PATH)
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
TARGET = 0x1234560  # the (imm32) word F pushes: dispatched to `entered` below
RT = os.path.join(HERE, "..", "..", "..", "src", "pes", "translated")
CODE = bytes([0x68]) + struct.pack("<I", TARGET) + bytes([0xC3])  # push imm32 ; ret
MAIN = r"""
#include "pes_state.hpp"
#include "pes_dispatch.hpp"
#include <cstdio>
void pes_fn_%(va)s(PesState& s);
static void entered(PesState& s) { s.gpr[0] = 0x77; }
PesDispatch pes_dispatch_lookup(uint64_t t) { return t == 0x1234560ULL ? PesDispatch{entered} : PesDispatch{nullptr}; }
void pes_dispatch_missing(uint64_t t) { std::printf("missing %%llx\n", (unsigned long long)t); std::exit(3); }
void (*pes_dispatch_missing_handler)(uint64_t) = nullptr;
int main() {
  PesState s;
  s.mem.segs.push_back({0x10000, std::vector<uint8_t>(0x100, 0)});
  s.gpr[4] = 0x10080;
  s.wr(s.gpr[4], 0, 8);  // the caller's return slot, as every translated call leaves it
  s.gpr[0] = 1;
  pes_fn_%(va)s(s);
  std::printf("%%llx %%llx\n", (unsigned long long)s.gpr[0], (unsigned long long)(s.gpr[4] - 0x10080));
}
"""


def run(ranges):
    pdata.KNOWN = frozenset()
    saved = list(xlate.COMPUTED_RET_RANGES)
    xlate.COMPUTED_RET_RANGES[:] = ranges
    try:
        with tempfile.TemporaryDirectory() as d:
            img = os.path.join(d, "img.bin")
            TE.build_image(img)
            raw = bytearray(open(img, "rb").read())
            n = struct.unpack_from("<I", raw, 0x98 + 112 + 24 + 4)[0] // 12
            struct.pack_into("<I", raw, 0x98 + 112 + 24 + 4, 12 * (n + 1))
            struct.pack_into("<III", raw, 0x400 + 12 * n, 0x1600, 0x1600 + len(CODE), 0x600)
            raw[0x1600:0x1600 + len(CODE)] = CODE
            open(img, "wb").write(raw)
            va = "0x%X" % F
            src, _ = xlate.translate_one(img, "", va, set())
            open(os.path.join(d, "f.cpp"), "w").write("\n".join(src) + "\n")
            open(os.path.join(d, "m.cpp"), "w").write(MAIN % {"va": va[2:]})
            exe = os.path.join(d, "t")
            r = subprocess.run(["g++", "-std=c++20", "-I", RT, "-o", exe, os.path.join(d, "m.cpp"), os.path.join(d, "f.cpp")],
                               capture_output=True, text=True)
            assert r.returncode == 0, r.stderr[-2000:]
            return subprocess.run([exe], capture_output=True, text=True).stdout.split()
    finally:
        xlate.COMPUTED_RET_RANGES[:] = saved


class ComputedRet(unittest.TestCase):
    def test_in_the_packers_range_ret_enters_the_pushed_target_and_leaves_the_stack_as_it_was(self):
        self.assertEqual(run([(F, F + 0x100)]), ["77", "0"])

    def test_outside_it_ret_just_returns_and_the_pushed_word_stays(self):
        self.assertEqual(run([]), ["1", "fffffffffffffff8"])  # rsp stays 8 below where it started: nothing popped the pushed word


if __name__ == "__main__":
    unittest.main()
