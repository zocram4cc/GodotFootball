#!/usr/bin/env python3
"""The named ucrtbase.dll!sqrtf import model (import_stubs.json, code_hex f30f51c0c3 = sqrtss xmm0, xmm0; ret) against the
contract of float sqrtf(float): xmm0 in, xmm0 out. The model is the same exact claim the libm models make (test_libm_models.py
checks the logf/powf/sinf/cosf stubs against ucrtbase; test_finite_model.py checks the _finite/_isnan classifiers): IEEE-754 sqrt
is correctly rounded, so sqrtss gives the bits of the single-precision root. Expected bits: the double root of the argument,
rounded once to single precision (a double carries more than 2p+2 bits, so this rounding is correctly rounded).
Negative and NaN arguments are not checked here (the model's own note: the export's errno path is OPEN for them).
Also: a plain recorded run (frame_trace.run_recorded with no trace) must get the import table, or every ucrtbase export stops it.
Usage: GF_DIFF_PYTHON=<venv python> python3 test_sqrt_model.py
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
from unicorn.x86_const import UC_X86_REG_RSP, UC_X86_REG_XMM0  # noqa: E402

CODE, STACK = 0x10000, 0x30000
RET = CODE + 0x800


def f32_bits(x):
    return struct.unpack("<I", struct.pack("<f", x))[0]


def f32_from_bits(b):
    return struct.unpack("<f", struct.pack("<I", b))[0]


def run_sqrtf(x_bits):
    """-> the low 32 bits of xmm0 after ucrtbase's sqrtf(float with bits x_bits)."""
    model = ft.load_import_models()[("ucrtbase.dll", "sqrtf")]
    mu = Uc(UC_ARCH_X86, UC_MODE_64)
    mu.mem_map(CODE, 0x1000)
    mu.mem_write(CODE, model)
    mu.mem_map(STACK, 0x1000)
    rsp = STACK + 0x800
    mu.mem_write(rsp, struct.pack("<Q", RET))
    mu.reg_write(UC_X86_REG_RSP, rsp)
    mu.reg_write(UC_X86_REG_XMM0, x_bits)
    mu.emu_start(CODE, RET, count=10)
    return mu.reg_read(UC_X86_REG_XMM0) & 0xFFFFFFFF


# Zeros, subnormals, exact roots, values whose root rounds, the largest finite single, and +infinity.
ARGUMENTS = (0.0, -0.0, 1e-45, 2.0, 0.5, 3.0, 10.0, 1.0e-30, 123456.789, 3.4028234663852886e38, math.inf)


class SqrtfModel(unittest.TestCase):
    def test_root_is_the_correctly_rounded_single_precision_sqrt(self):
        for x in ARGUMENTS:
            expected = f32_bits(math.sqrt(f32_from_bits(f32_bits(x))))
            self.assertEqual(run_sqrtf(f32_bits(x)), expected, repr(x))

    def test_perfect_squares_are_exact(self):
        for r in (1.0, 2.0, 1024.0, 0.25):
            self.assertEqual(run_sqrtf(f32_bits(r * r)), f32_bits(r), repr(r))

    def test_a_plain_recorded_run_gets_the_import_table_and_a_caller_table_wins(self):
        # run_recorded with no trace and no models used to pass {} to its LazyMap, so every ucrtbase export stopped the run.
        # A plain replay that reaches sqrtf must answer it from import_stubs.json; a caller's own table is used as given.
        self.assertIn(("ucrtbase.dll", "sqrtf"), ft.import_models_for(None))
        self.assertEqual(ft.import_models_for({}), {})


if __name__ == "__main__":
    unittest.main(verbosity=2)
