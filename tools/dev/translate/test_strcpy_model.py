#!/usr/bin/env python3
"""The named ucrtbase.dll!strcpy_s import model (import_stubs.json) against strcpy_s's contract, one case per path.

Loads the model by its (module, export) key, as frame_trace.LazyMap does, and runs its bytes in unicorn.
Usage: GF_DIFF_PYTHON=<venv python> python3 test_strcpy_model.py
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
DST, SRC = DATA + 0x100, DATA + 0x200
EINVAL, ERANGE = 0x16, 0x22  # errno values strcpy_s returns (value 22, value 34)


def run_strcpy_s(dst, size, src, dst_init=b"\xaa" * 16, src_bytes=b""):
    model = ft.load_import_models()[("ucrtbase.dll", "strcpy_s")]
    mu = Uc(UC_ARCH_X86, UC_MODE_64)
    mu.mem_map(CODE, 0x1000)
    mu.mem_write(CODE, model)
    mu.mem_map(DATA, 0x1000)
    mu.mem_write(DST, dst_init)
    mu.mem_write(SRC, src_bytes.ljust(0x40, b"\0"))
    mu.mem_map(STACK, 0x1000)
    rsp = STACK + 0x800
    mu.mem_write(rsp, struct.pack("<Q", RET))
    mu.reg_write(UC_X86_REG_RCX, dst)
    mu.reg_write(UC_X86_REG_RDX, size)
    mu.reg_write(UC_X86_REG_R8, src)
    mu.reg_write(UC_X86_REG_RSP, rsp)
    mu.emu_start(CODE, RET, count=1000)
    return mu.reg_read(UC_X86_REG_RAX) & 0xFFFFFFFF, bytes(mu.mem_read(DST, 16))


class StrcpySModel(unittest.TestCase):
    def test_null_destination_is_einval_and_writes_nothing(self):
        rc, mem = run_strcpy_s(0, 8, SRC, src_bytes=b"abc")
        self.assertEqual(rc, EINVAL)
        self.assertEqual(mem, b"\xaa" * 16)

    def test_zero_size_is_einval_and_writes_nothing(self):
        rc, mem = run_strcpy_s(DST, 0, SRC, src_bytes=b"abc")
        self.assertEqual(rc, EINVAL)
        self.assertEqual(mem, b"\xaa" * 16)

    def test_null_source_clears_first_byte_and_is_einval(self):
        rc, mem = run_strcpy_s(DST, 8, 0)
        self.assertEqual(rc, EINVAL)
        self.assertEqual(mem, b"\x00" + b"\xaa" * 15)

    def test_copy_with_nul_that_fits_returns_zero(self):
        rc, mem = run_strcpy_s(DST, 4, SRC, src_bytes=b"abc")
        self.assertEqual(rc, 0)
        self.assertEqual(mem[:4], b"abc\0")
        self.assertEqual(mem[4:], b"\xaa" * 12)  # nothing written past the NUL

    def test_empty_source_copies_only_the_nul(self):
        rc, mem = run_strcpy_s(DST, 1, SRC, src_bytes=b"")
        self.assertEqual(rc, 0)
        self.assertEqual(mem[:1], b"\0")

    def test_source_too_long_clears_destination_and_is_erange(self):
        rc, mem = run_strcpy_s(DST, 4, SRC, src_bytes=b"abcd")
        self.assertEqual(rc, ERANGE)
        self.assertEqual(mem[0], 0)


if __name__ == "__main__":
    unittest.main()
