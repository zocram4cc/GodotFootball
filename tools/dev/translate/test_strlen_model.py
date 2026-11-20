#!/usr/bin/env python3
"""The named ucrtbase.dll!strlen import model (import_stubs.json) against strlen's contract, run in unicorn.

Contract: rax = the count of bytes before the first NUL at rcx; rcx unchanged; no memory written.
The cases are the string's length, the empty string, a string whose NUL is the last byte of the mapped data, and a
string whose content is not NUL-terminated before a sentinel (the count must stop at the first NUL, not at the sentinel).
Usage: GF_DIFF_PYTHON=<venv python> python3 test_strlen_model.py
"""
import os
import struct
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import frame_trace as ft  # noqa: E402
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64  # noqa: E402
from unicorn.x86_const import UC_X86_REG_RAX, UC_X86_REG_RCX, UC_X86_REG_RSP  # noqa: E402

CODE, DATA, STACK = 0x10000, 0x20000, 0x30000
RET = CODE + 0x800  # the stop address the model returns to (never executed)


def run_strlen(text, mapped_tail=0x40):
    """Map text (NUL-terminated by the caller) at DATA+0x100 and return (rax, rcx after, bytes at DATA..DATA+0x1000 changed)."""
    model = ft.load_import_models()[("ucrtbase.dll", "strlen")]
    mu = Uc(UC_ARCH_X86, UC_MODE_64)
    mu.mem_map(CODE, 0x1000)
    mu.mem_write(CODE, model)
    mu.mem_map(DATA, 0x1000)
    s_addr = DATA + 0x100
    blob = text + b"\0" * mapped_tail
    mu.mem_write(s_addr, blob[: 0x1000 - 0x100])
    before = bytes(mu.mem_read(DATA, 0x1000))
    mu.mem_map(STACK, 0x1000)
    rsp = STACK + 0x800
    mu.mem_write(rsp, struct.pack("<Q", RET))
    mu.reg_write(UC_X86_REG_RCX, s_addr)
    mu.reg_write(UC_X86_REG_RSP, rsp)
    mu.emu_start(CODE, RET, count=100000)
    after = bytes(mu.mem_read(DATA, 0x1000))
    return mu.reg_read(UC_X86_REG_RAX), mu.reg_read(UC_X86_REG_RCX), after != before, s_addr


class StrlenModel(unittest.TestCase):
    def test_counts_the_bytes_before_the_first_nul(self):
        rax, _, _, _ = run_strlen(b"metamethod\0")
        self.assertEqual(rax, len(b"metamethod"))

    def test_the_empty_string_is_zero(self):
        rax, _, _, _ = run_strlen(b"\0")
        self.assertEqual(rax, 0)

    def test_stops_at_the_first_nul_not_at_a_later_text(self):
        # Bytes after the NUL are non-zero: a model that scanned on would count them.
        rax, _, _, _ = run_strlen(b"ab\0cdefgh", mapped_tail=0x40)
        self.assertEqual(rax, 2)

    def test_leaves_rcx_and_memory_alone(self):
        rax, rcx, wrote, s_addr = run_strlen(b"xyz\0")
        self.assertEqual(rcx, s_addr)
        self.assertFalse(wrote)


if __name__ == "__main__":
    unittest.main()
