#!/usr/bin/env python3
"""The named ucrtbase.dll!_fdtest import model (import_stubs.json) against the C classifier's contract, one case per class.

int _fdtest(const float *p): 2 = NaN, 1 = infinity, 0 = zero, -2 = subnormal, -1 = normal (the DLL's own bytes, rcx = p, eax =
class). The model is loaded by its (module, export) key as frame_trace.LazyMap does and run in unicorn. rdx is checked against the
register contract the translator's mirror (xlate.import_model_lines, do = fdtest) leaves: the exponent word on the zero, infinity and
NaN paths, its negation (low word) on the subnormal and normal paths.
Usage: GF_DIFF_PYTHON=<venv python> python3 test_fdtest_model.py
"""
import os
import struct
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import frame_trace as ft  # noqa: E402
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64  # noqa: E402
from unicorn.x86_const import UC_X86_REG_RAX, UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_RSP  # noqa: E402

CODE, DATA, STACK = 0x10000, 0x20000, 0x30000
RET = CODE + 0x800


def fdtest(bits):
    """-> (eax, rdx) of the model on the float whose bit pattern is `bits`."""
    model = ft.load_import_models()[("ucrtbase.dll", "_fdtest")]
    mu = Uc(UC_ARCH_X86, UC_MODE_64)
    mu.mem_map(CODE, 0x1000)
    mu.mem_write(CODE, model)
    mu.mem_map(DATA, 0x1000)
    mu.mem_write(DATA, struct.pack("<I", bits))
    mu.mem_map(STACK, 0x1000)
    rsp = STACK + 0x800
    mu.mem_write(rsp, struct.pack("<Q", RET))
    mu.reg_write(UC_X86_REG_RSP, rsp)
    mu.reg_write(UC_X86_REG_RCX, DATA)
    mu.emu_start(CODE, RET)
    return mu.reg_read(UC_X86_REG_RAX) & 0xFFFFFFFF, mu.reg_read(UC_X86_REG_RDX)


def signed(v):
    return v - (1 << 32) if v >= 1 << 31 else v


class FdtestModel(unittest.TestCase):
    def test_every_class(self):
        cases = {0x00000000: 0, 0x80000000: 0, 0x7F800000: 1, 0xFF800000: 1, 0x7FC00000: 2, 0x7F800001: 2, 0xFFC00001: 2,
                 0x00000001: -2, 0x807FFFFF: -2, 0x00800000: -1, 0x3F800000: -1, 0xBF800000: -1, 0x7F7FFFFF: -1}
        for bits, want in cases.items():
            self.assertEqual(signed(fdtest(bits)[0]), want, hex(bits))

    def test_rdx_is_the_exponent_word_or_its_negation(self):
        for bits in (0x7F800000, 0x7FC00000, 0x00000000, 0x80000000):  # inf, nan, zero: the exponent word
            self.assertEqual(fdtest(bits)[1], (bits >> 16) & 0x7F80, hex(bits))
        for bits in (0x3F800000, 0x7F7FFFFF):  # normal: -exponent, low word
            self.assertEqual(fdtest(bits)[1], (-((bits >> 16) & 0x7F80)) & 0xFFFF, hex(bits))
        self.assertEqual(fdtest(0x00000001)[1], 0)  # subnormal: exponent 0, negated 0


if __name__ == "__main__":
    unittest.main()
