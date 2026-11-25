#!/usr/bin/env python3
"""The ucrtbase strlen import model (import_stubs.json code_hex, strlen_model.s) against its contract: the count of bytes before
the NUL, read only (rcx unchanged, no write). Usage: GF_DIFF_PYTHON=<venv python> python3 test_strlen_model.py"""
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
RET = CODE + 0x800


def run(text):
    model = ft.load_import_models()[("ucrtbase.dll", "strlen")]
    mu = Uc(UC_ARCH_X86, UC_MODE_64)
    mu.mem_map(CODE, 0x1000)
    mu.mem_write(CODE, model)
    mu.mem_map(DATA, 0x1000)
    mu.mem_write(DATA + 0x100, text + b"\xAB" * 0x40)  # the NUL (if any) ends the string; the bytes after it must stay
    mu.mem_map(STACK, 0x1000)
    rsp = STACK + 0x800
    mu.mem_write(rsp, struct.pack("<Q", RET))
    mu.reg_write(UC_X86_REG_RSP, rsp)
    mu.reg_write(UC_X86_REG_RCX, DATA + 0x100)
    mu.emu_start(CODE, RET, count=1000)
    return mu


class StrlenModel(unittest.TestCase):
    def test_counts_the_bytes_before_the_nul(self):
        self.assertEqual(run(b"referee007\x00").reg_read(UC_X86_REG_RAX), 10)

    def test_an_empty_string_is_zero(self):
        self.assertEqual(run(b"\x00").reg_read(UC_X86_REG_RAX), 0)

    def test_reads_only_and_leaves_rcx_as_passed(self):
        mu = run(b"abc\x00")
        self.assertEqual(mu.reg_read(UC_X86_REG_RCX), DATA + 0x100)
        self.assertEqual(bytes(mu.mem_read(DATA + 0x100, 0x44)), b"abc\x00" + b"\xAB" * 0x40)


if __name__ == "__main__":
    unittest.main()
