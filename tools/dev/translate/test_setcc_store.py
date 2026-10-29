#!/usr/bin/env python3
"""SETcc with a memory destination stores the condition byte (0F 90..9F /0, always one byte).

Found at 0x14084CE60 `sete byte ptr [rsp+0x34]` (the trap kind-0 root 0x14079A470 closure): xlate emitted a no-op, so
the byte kept its stale stack value and the following `mov rax,[rsp+0x30]; mov [rdi+0x1c],rax` copied it into the
record. Synthetic image, a translated function compiled and run (no game bytes).

usage: test_setcc_store.py   (harness Python: capstone; g++ on PATH)
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
CODE = bytes.fromhex(
    "83fb01"      # cmp ebx,1
    "0f94442434"  # sete byte ptr [rsp+0x34]
    "c3")         # ret
MAIN = r"""
#include "pes_state.hpp"
#include <cstdio>
#include <cstdlib>
void pes_fn_%s(PesState& s);
int main(int, char** v) {
  PesState s;
  s.mem.segs.push_back({0x10000, std::vector<uint8_t>(0x100, 0xAA)});
  s.gpr[4] = 0x10020;
  s.gpr[3] = std::strtoull(v[1], nullptr, 0);
  pes_fn_%s(s);
  std::printf("%%02x %%02x %%02x\n", (unsigned)s.rd(0x10053, 1), (unsigned)s.rd(0x10054, 1), (unsigned)s.rd(0x10055, 1));
}
"""


def image(path):
    """TE.build_image plus one .pdata entry for F."""
    TE.build_image(path)
    raw = bytearray(open(path, "rb").read())
    n = struct.unpack_from("<I", raw, 0x98 + 112 + 24 + 4)[0] // 12
    struct.pack_into("<I", raw, 0x98 + 112 + 24 + 4, 12 * (n + 1))
    struct.pack_into("<III", raw, 0x400 + 12 * n, 0x1600, 0x1600 + len(CODE), 0x600)
    raw[0x1600:0x1600 + len(CODE)] = CODE
    open(path, "wb").write(raw)


class SetccStore(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pdata.KNOWN = frozenset()
        cls.dir = tempfile.TemporaryDirectory()
        img = os.path.join(cls.dir.name, "img.bin")
        image(img)
        va = "0x%X" % F
        src, _ = xlate.translate_one(img, "", va, set())
        d = cls.dir.name
        open(os.path.join(d, "f.cpp"), "w").write("\n".join(src) + "\n")
        open(os.path.join(d, "m.cpp"), "w").write(MAIN % (va[2:], va[2:]))
        cls.exe = os.path.join(d, "t")
        r = subprocess.run(["g++", "-std=c++20", "-I", RT, "-o", cls.exe, os.path.join(d, "m.cpp"),
                            os.path.join(d, "f.cpp")], capture_output=True, text=True)
        assert r.returncode == 0, r.stderr[-2000:]

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def run_with(self, ebx):
        return subprocess.run([self.exe, str(ebx)], capture_output=True, text=True).stdout.split()

    def test_condition_true_stores_one_and_touches_only_that_byte(self):
        self.assertEqual(self.run_with(1), ["aa", "01", "aa"])

    def test_condition_false_stores_zero_over_the_stale_byte(self):
        self.assertEqual(self.run_with(2), ["aa", "00", "aa"])


if __name__ == "__main__":
    unittest.main()
