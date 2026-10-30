#!/usr/bin/env python3
"""The named ucrtbase.dll!_finite and _isnan import models (import_stubs.json) against their contracts: int _finite(double x)
is 1 when x is neither an infinity nor a NaN; int _isnan(double x) is 1 when x is a NaN; both 0 otherwise. Loads each model by
its (module, export) key as frame_trace.LazyMap does and runs its bytes in unicorn; also checks the stack store the real
exports make (x into the caller's home slot).
Usage: GF_DIFF_PYTHON=<venv python> python3 test_finite_model.py
"""
import math
import os
import struct
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import frame_trace as ft  # noqa: E402
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64  # noqa: E402
from unicorn.x86_const import UC_X86_REG_RAX, UC_X86_REG_RSP, UC_X86_REG_XMM0  # noqa: E402

CODE, STACK = 0x10000, 0x30000
RET = CODE + 0x800


def run_export(export, x_bits):
    """-> (rax, the qword stored at [rsp + 8]) after ucrtbase's export(double with bits x_bits)."""
    model = ft.load_import_models()[("ucrtbase.dll", export)]
    mu = Uc(UC_ARCH_X86, UC_MODE_64)
    mu.mem_map(CODE, 0x1000)
    mu.mem_write(CODE, model)
    mu.mem_map(STACK, 0x1000)
    rsp = STACK + 0x800
    mu.mem_write(rsp, struct.pack("<Q", RET))
    mu.reg_write(UC_X86_REG_RSP, rsp)
    mu.reg_write(UC_X86_REG_XMM0, x_bits)
    mu.emu_start(CODE, RET, count=100)
    return mu.reg_read(UC_X86_REG_RAX), struct.unpack("<Q", mu.mem_read(rsp + 8, 8))[0]


def bits(d):
    return struct.unpack("<Q", struct.pack("<d", d))[0]


FINITE = (0.0, -0.0, 1.0, -2.5, 5e-324, 1.7976931348623157e308, -1.7976931348623157e308)
NON_FINITE_NAN = (0x7FF8000000000000, 0xFFF8000000000000, 0x7FF0000000000001, 0xFFFFFFFFFFFFFFFF)
INFINITIES = (bits(math.inf), bits(-math.inf))


class UcrtClassifyModels(unittest.TestCase):
    def test_finite_is_one_for_finite_values_and_zero_for_infinities_and_nans(self):
        for d in FINITE:
            self.assertEqual(run_export("_finite", bits(d))[0], 1, d)
        for b in INFINITIES + NON_FINITE_NAN:
            self.assertEqual(run_export("_finite", b)[0], 0, hex(b))

    def test_isnan_is_one_only_for_nans(self):
        for b in NON_FINITE_NAN:
            self.assertEqual(run_export("_isnan", b)[0], 1, hex(b))
        for b in [bits(d) for d in FINITE] + list(INFINITIES):
            self.assertEqual(run_export("_isnan", b)[0], 0, hex(b))

    def test_both_store_the_argument_in_the_callers_home_slot(self):
        for export in ("_finite", "_isnan"):
            self.assertEqual(run_export(export, bits(-3.25))[1], bits(-3.25), export)


if __name__ == "__main__":
    unittest.main(verbosity=2)
