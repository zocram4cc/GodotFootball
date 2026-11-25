#!/usr/bin/env python3
"""The ucrtbase __stdio_common_vsnprintf_s import model (import_stubs.json code_hex, vsnprintf_model.s) run in unicorn on the
seven-argument Win64 layout the recorded call uses: options rcx, buf rdx, size r8, count r9, fmt [rsp+0x28], locale
[rsp+0x30], va_list [rsp+0x38]. The recorded call is "referee%03d" into a 0x20-byte buffer with count -1 (_TRUNCATE).
Cases: the recorded format, a string argument, and an overflow (the model's int3 trap must end the run, never a silent write).
Usage: GF_DIFF_PYTHON=<venv python> python3 test_vsnprintf_model.py
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
                               UC_X86_REG_R9, UC_X86_REG_RSP, UC_X86_REG_RBX, UC_X86_REG_RSI, UC_X86_REG_RDI)

CODE, DATA, STACK = 0x10000, 0x20000, 0x30000
RET = CODE + 0x800  # the stop address the model returns to (never executed)
BUF, FMT, ARGS, STR = DATA + 0x100, DATA + 0x200, DATA + 0x300, DATA + 0x400
OPTIONS_TRUNCATE = 4          # the recorded options word
COUNT_TRUNCATE = 0xFFFFFFFFFFFFFFFF  # _TRUNCATE (-1) in r9, as recorded
RBX, RSI, RDI = 0x1111, 0x2222, 0x3333


def run(fmt, args=(), size=0x20, strings=()):
    """-> (rax, buffer bytes up to the NUL, error). args: ints (32-bit va slots) or str (a pointer to a NUL-terminated string)."""
    model = ft.load_import_models()[("ucrtbase.dll", "__stdio_common_vsnprintf_s")]
    mu = Uc(UC_ARCH_X86, UC_MODE_64)
    mu.mem_map(CODE, 0x1000)
    mu.mem_write(CODE, model)
    mu.mem_map(DATA, 0x1000)
    mu.mem_write(FMT, fmt + b"\0")
    mu.mem_write(BUF, b"\xaa" * 0x40)
    slot, str_at = ARGS, STR
    for a in args:
        if isinstance(a, str):
            mu.mem_write(str_at, a.encode() + b"\0")
            mu.mem_write(slot, struct.pack("<Q", str_at))
            str_at += 0x40
        else:
            mu.mem_write(slot, struct.pack("<Q", a & 0xFFFFFFFFFFFFFFFF)[:8])
        slot += 8
    mu.mem_map(STACK, 0x1000)
    rsp = STACK + 0x800
    mu.mem_write(rsp, struct.pack("<Q", RET))
    mu.mem_write(rsp + 0x28, struct.pack("<Q", FMT))     # arg 5: the format
    mu.mem_write(rsp + 0x30, struct.pack("<Q", 0))       # arg 6: the locale
    mu.mem_write(rsp + 0x38, struct.pack("<Q", ARGS))    # arg 7: the va_list
    for reg, val in ((UC_X86_REG_RCX, OPTIONS_TRUNCATE), (UC_X86_REG_RDX, BUF), (UC_X86_REG_R8, size),
                     (UC_X86_REG_R9, COUNT_TRUNCATE), (UC_X86_REG_RSP, rsp), (UC_X86_REG_RBX, RBX),
                     (UC_X86_REG_RSI, RSI), (UC_X86_REG_RDI, RDI)):
        mu.reg_write(reg, val)
    try:
        mu.emu_start(CODE, RET, count=10000)
    except Exception as e:  # the model's int3 on an overflow
        return None, None, "trap: %s" % e
    saved = (mu.reg_read(UC_X86_REG_RBX), mu.reg_read(UC_X86_REG_RSI), mu.reg_read(UC_X86_REG_RDI), mu.reg_read(UC_X86_REG_RSP))
    assert saved == (RBX, RSI, RDI, rsp + 8), saved  # callee-saved registers and the stack pointer restored
    out = bytes(mu.mem_read(BUF, 0x20))
    return mu.reg_read(UC_X86_REG_RAX), out[: out.index(b"\0")] if b"\0" in out else out, None


class VsnprintfModel(unittest.TestCase):
    def test_the_recorded_format_writes_the_padded_referee_name(self):
        rax, text, err = run(b"referee%03d", (7,))
        self.assertIsNone(err)
        self.assertEqual(text, b"referee007")
        self.assertEqual(rax, len(b"referee007"))

    def test_a_string_argument_and_a_second_number(self):
        rax, text, err = run(b"%s-%d", ("PES", 42))
        self.assertIsNone(err)
        self.assertEqual(text, b"PES-42")
        self.assertEqual(rax, 6)

    def test_an_overflow_is_a_trap_not_a_silent_write(self):
        # 40 characters into a 32-byte buffer: the model must stop, as vsprintf's does.
        rax, text, err = run(b"x" * 40, ())
        self.assertIsNotNone(err)


if __name__ == "__main__":
    unittest.main()
