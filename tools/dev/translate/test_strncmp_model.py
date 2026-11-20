#!/usr/bin/env python3
"""The ucrtbase strncmp import model (import_stubs.json code_hex, strncmp_model.s) run in unicorn against strncmp's contract.

Contract: rax = (unsigned) s1[i] - s2[i] at the first differing byte i < n, else 0; a NUL that agrees ends the compare with 0.
Cases: a prefix that agrees (0), a difference that is positive and negative, a count of 0, a NUL inside the count, and a
difference beyond the count (must be 0: the count bounds the compare).
Usage: GF_DIFF_PYTHON=<venv python> python3 test_strncmp_model.py
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
RET = CODE + 0x800  # the stop address the model returns to (never executed)
S1, S2 = DATA + 0x100, DATA + 0x200


def run(a, b, n):
    """-> (eax as a signed int, rcx - S1, rdx - S2, r8) after strncmp(a, b, n); a and b are NUL-terminated (or not)."""
    model = ft.load_import_models()[("ucrtbase.dll", "strncmp")]
    mu = Uc(UC_ARCH_X86, UC_MODE_64)
    mu.mem_map(CODE, 0x1000)
    mu.mem_write(CODE, model)
    mu.mem_map(DATA, 0x1000)
    mu.mem_write(S1, a.ljust(0x80, b"\x7f"))
    mu.mem_write(S2, b.ljust(0x80, b"\x7f"))
    mu.mem_map(STACK, 0x1000)
    rsp = STACK + 0x800
    mu.mem_write(rsp, struct.pack("<Q", RET))
    mu.reg_write(UC_X86_REG_RCX, S1)
    mu.reg_write(UC_X86_REG_RDX, S2)
    mu.reg_write(UC_X86_REG_R8, n)
    mu.reg_write(UC_X86_REG_RSP, rsp)
    mu.emu_start(CODE, RET, count=10000)
    eax = mu.reg_read(UC_X86_REG_RAX) & 0xFFFFFFFF
    if eax & 0x80000000:
        eax -= 1 << 32
    return eax, mu.reg_read(UC_X86_REG_RCX) - S1, mu.reg_read(UC_X86_REG_RDX) - S2, mu.reg_read(UC_X86_REG_R8)


class StrncmpModel(unittest.TestCase):
    def test_an_agreeing_prefix_is_zero(self):
        self.assertEqual(run(b"/Assets/pes16", b"/Assets", 7)[0], 0)

    def test_the_sign_of_the_first_difference(self):
        self.assertGreater(run(b"b", b"a", 1)[0], 0)
        self.assertLess(run(b"a", b"b", 1)[0], 0)
        self.assertEqual(run(b"a", b"b", 1)[0], -1)

    def test_a_count_of_zero_compares_nothing(self):
        self.assertEqual(run(b"x", b"y", 0)[0], 0)

    def test_a_nul_that_agrees_ends_the_compare(self):
        self.assertEqual(run(b"ab\0cd", b"ab\0ef", 5)[0], 0)

    def test_a_difference_beyond_the_count_is_not_seen(self):
        self.assertEqual(run(b"abcX", b"abcY", 3)[0], 0)

    def test_unsigned_bytes(self):
        self.assertEqual(run(b"\xff", b"\x01", 1)[0], 0xFE)


if __name__ == "__main__":
    unittest.main()
