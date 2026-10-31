#!/usr/bin/env python3
"""The named VCRUNTIME140.dll!memchr import model (import_stubs.json) against memchr's contract, one case per path.

Usage: GF_DIFF_PYTHON=<venv python> python3 test_memchr_model.py
"""
import os
import struct
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import frame_trace as ft  # noqa: E402
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64  # noqa: E402
from unicorn.x86_const import UC_X86_REG_RAX, UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8, UC_X86_REG_RSP  # noqa: E402

CODE, DATA, STACK = 0x10000, 0x20000, 0x30000
RET = CODE + 0x800
BUF = DATA + 0x100


def memchr(data, c, n):
    model = ft.load_import_models()[("vcruntime140.dll", "memchr")]
    mu = Uc(UC_ARCH_X86, UC_MODE_64)
    mu.mem_map(CODE, 0x1000)
    mu.mem_write(CODE, model)
    mu.mem_map(DATA, 0x1000)
    mu.mem_write(BUF, data)
    mu.mem_map(STACK, 0x1000)
    rsp = STACK + 0x800
    mu.mem_write(rsp, struct.pack("<Q", RET))
    for reg, val in ((UC_X86_REG_RCX, BUF), (UC_X86_REG_RDX, c), (UC_X86_REG_R8, n), (UC_X86_REG_RSP, rsp)):
        mu.reg_write(reg, val)
    mu.emu_start(CODE, RET, count=10000)
    return mu.reg_read(UC_X86_REG_RAX)


class MemchrModel(unittest.TestCase):
    def test_first_match(self):
        self.assertEqual(memchr(b"abcabc", ord("b"), 6), BUF + 1)

    def test_match_on_the_last_byte_counted(self):
        self.assertEqual(memchr(b"abcd", ord("d"), 4), BUF + 3)

    def test_match_past_the_count_is_not_found(self):
        self.assertEqual(memchr(b"abcd", ord("d"), 3), 0)

    def test_zero_count_reads_nothing(self):
        self.assertEqual(memchr(b"a", ord("a"), 0), 0)

    def test_only_the_low_byte_of_c_counts(self):
        self.assertEqual(memchr(b"xyz", 0x17F00 | ord("z"), 3), BUF + 2)


if __name__ == "__main__":
    unittest.main()
