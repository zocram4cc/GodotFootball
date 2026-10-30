#!/usr/bin/env python3
"""The named VCRUNTIME140.dll!memmove import model (import_stubs.json) against memmove's contract, one case per path.

Win64 memmove(void *dst, const void *src, size_t n): copies n bytes as if through a temporary (overlap-safe), returns
dst, and preserves RDI and RSI (callee-saved). Loads the model by its (module, export) key, as frame_trace.LazyMap does,
and runs its bytes in unicorn. Usage: GF_DIFF_PYTHON=<venv python> python3 test_memmove_model.py
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
                               UC_X86_REG_RDI, UC_X86_REG_RSI, UC_X86_REG_RSP, UC_X86_REG_EFLAGS)

CODE, DATA, STACK = 0x10000, 0x20000, 0x30000
RET = CODE + 0x800  # the stop address the model returns to (never executed)
BUF = DATA + 0x100
SENTINEL_RDI, SENTINEL_RSI = 0x1111_2222_3333_4444, 0x5555_6666_7777_8888
PATTERN = bytes(range(1, 33))


def run_memmove(dst_off, src_off, n):
    """-> (rax, buffer bytes, rdi, rsi, direction flag) after memmove(BUF+dst_off, BUF+src_off, n) over PATTERN."""
    model = ft.load_import_models()[("vcruntime140.dll", "memmove")]
    mu = Uc(UC_ARCH_X86, UC_MODE_64)
    mu.mem_map(CODE, 0x1000)
    mu.mem_write(CODE, model)
    mu.mem_map(DATA, 0x2000)
    mu.mem_write(BUF, PATTERN)
    mu.mem_map(STACK, 0x1000)
    rsp = STACK + 0x800
    mu.mem_write(rsp, struct.pack("<Q", RET))
    mu.reg_write(UC_X86_REG_RCX, BUF + dst_off)
    mu.reg_write(UC_X86_REG_RDX, BUF + src_off)
    mu.reg_write(UC_X86_REG_R8, n)
    mu.reg_write(UC_X86_REG_RDI, SENTINEL_RDI)
    mu.reg_write(UC_X86_REG_RSI, SENTINEL_RSI)
    mu.reg_write(UC_X86_REG_RSP, rsp)
    mu.emu_start(CODE, RET, count=1000)
    return (mu.reg_read(UC_X86_REG_RAX), bytes(mu.mem_read(BUF, len(PATTERN))),
            mu.reg_read(UC_X86_REG_RDI), mu.reg_read(UC_X86_REG_RSI), (mu.reg_read(UC_X86_REG_EFLAGS) >> 10) & 1)


def moved(dst_off, src_off, n):
    """The contract's result: the n bytes at src_off land at dst_off as if copied through a temporary."""
    out = bytearray(PATTERN)
    out[dst_off:dst_off + n] = PATTERN[src_off:src_off + n]
    return bytes(out)


class MemmoveModel(unittest.TestCase):
    def test_returns_destination(self):
        rax, _, _, _, _ = run_memmove(16, 0, 4)
        self.assertEqual(rax, BUF + 16)

    def test_disjoint_copy(self):
        _, mem, _, _, _ = run_memmove(16, 0, 8)
        self.assertEqual(mem, moved(16, 0, 8))

    def test_overlap_destination_above_source_is_copied_backward(self):
        # A forward copy would smear the first bytes across the run: the lane list's insert-shift case.
        _, mem, _, _, _ = run_memmove(4, 0, 12)
        self.assertEqual(mem, moved(4, 0, 12))

    def test_overlap_destination_below_source_is_copied_forward(self):
        _, mem, _, _, _ = run_memmove(0, 4, 12)
        self.assertEqual(mem, moved(0, 4, 12))

    def test_adjacent_destination_at_source_end_is_a_plain_copy(self):
        _, mem, _, _, _ = run_memmove(8, 0, 8)
        self.assertEqual(mem, moved(8, 0, 8))

    def test_zero_length_writes_nothing(self):
        rax, mem, _, _, _ = run_memmove(4, 0, 0)
        self.assertEqual((rax, mem), (BUF + 4, PATTERN))

    def test_callee_saved_registers_and_direction_flag_are_preserved(self):
        _, _, rdi, rsi, df = run_memmove(4, 0, 12)
        self.assertEqual((rdi, rsi, df), (SENTINEL_RDI, SENTINEL_RSI, 0))


if __name__ == "__main__":
    unittest.main(verbosity=2)
