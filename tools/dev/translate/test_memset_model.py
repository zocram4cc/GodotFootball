#!/usr/bin/env python3
"""The named VCRUNTIME140.dll!memset import model (import_stubs.json) against memset's contract, one case per path.

Win64 memset(void *dst, int c, size_t n): writes the low byte of c to n bytes at dst, returns dst, and preserves RDI
and RSI (callee-saved). Loads the model by its (module, export) key, as frame_trace.LazyMap does, and runs its bytes
in unicorn. Usage: GF_DIFF_PYTHON=<venv python> python3 test_memset_model.py
"""
import os
import struct
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import frame_trace as ft  # noqa: E402
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64  # noqa: E402
from unicorn.x86_const import (UC_X86_REG_RAX, UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8,  # noqa: E402
                               UC_X86_REG_RDI, UC_X86_REG_RSI, UC_X86_REG_RSP)

CODE, DATA, STACK = 0x10000, 0x20000, 0x30000
RET = CODE + 0x800  # the stop address the model returns to (never executed)
DST = DATA + 0x100
SENTINEL_RDI, SENTINEL_RSI = 0x1111_2222_3333_4444, 0x5555_6666_7777_8888


def run_memset(dst, c, n, dst_init=b"\xaa" * 32):
    """-> (rax, dst bytes, rdi, rsi) after the model runs with the given arguments."""
    model = ft.load_import_models()[("vcruntime140.dll", "memset")]
    mu = Uc(UC_ARCH_X86, UC_MODE_64)
    mu.mem_map(CODE, 0x1000)
    mu.mem_write(CODE, model)
    mu.mem_map(DATA, 0x2000)
    mu.mem_write(DST, dst_init)
    mu.mem_map(STACK, 0x1000)
    rsp = STACK + 0x800
    mu.mem_write(rsp, struct.pack("<Q", RET))
    mu.reg_write(UC_X86_REG_RCX, dst)
    mu.reg_write(UC_X86_REG_RDX, c)
    mu.reg_write(UC_X86_REG_R8, n)
    mu.reg_write(UC_X86_REG_RDI, SENTINEL_RDI)
    mu.reg_write(UC_X86_REG_RSI, SENTINEL_RSI)
    mu.reg_write(UC_X86_REG_RSP, rsp)
    mu.emu_start(CODE, RET, count=1000)
    size = len(dst_init)
    return (mu.reg_read(UC_X86_REG_RAX), bytes(mu.mem_read(dst, size)) if dst else b"",
            mu.reg_read(UC_X86_REG_RDI), mu.reg_read(UC_X86_REG_RSI))


class MemsetModel(unittest.TestCase):
    def test_returns_destination(self):
        rax, _, _, _ = run_memset(DST, 0x5A, 8)
        self.assertEqual(rax, DST)

    def test_writes_exactly_n_bytes_of_c(self):
        _, mem, _, _ = run_memset(DST, 0x5A, 5)
        self.assertEqual(mem, b"\x5a" * 5 + b"\xaa" * 27)

    def test_zero_length_writes_nothing_and_returns_destination(self):
        rax, mem, _, _ = run_memset(DST, 0x5A, 0)
        self.assertEqual(rax, DST)
        self.assertEqual(mem, b"\xaa" * 32)

    def test_c_is_taken_as_its_low_byte(self):
        _, mem, _, _ = run_memset(DST, 0x1FF, 3)
        self.assertEqual(mem[:4], b"\xff\xff\xff\xaa")

    def test_long_run_is_written_in_full(self):
        _, mem, _, _ = run_memset(DST, 0x01, 0x100, dst_init=b"\xaa" * 0x200)
        self.assertEqual(mem, b"\x01" * 0x100 + b"\xaa" * 0x100)

    def test_callee_saved_registers_are_preserved(self):
        _, _, rdi, rsi = run_memset(DST, 0x5A, 4)
        self.assertEqual((rdi, rsi), (SENTINEL_RDI, SENTINEL_RSI))


if __name__ == "__main__":
    unittest.main(verbosity=2)
