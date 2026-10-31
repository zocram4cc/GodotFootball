#!/usr/bin/env python3
"""The named ucrtbase.dll!__stdio_common_vsprintf_s import model (import_stubs.json, source vsprintf_model.s) against the
printf contract it claims, one case per conversion, plus the loud stops (int3) for what it does not model.

Loads the model by its (module, export) key, as frame_trace.LazyMap does, and runs its bytes in unicorn.
Usage: GF_DIFF_PYTHON=<venv python> python3 test_vsprintf_model.py
"""
import os
import struct
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import frame_trace as ft  # noqa: E402
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64, UcError  # noqa: E402
from unicorn.x86_const import (UC_X86_REG_RAX, UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8, UC_X86_REG_R9,  # noqa: E402
                               UC_X86_REG_RSP, UC_X86_REG_RBX, UC_X86_REG_RSI, UC_X86_REG_RDI)

CODE, DATA, STACK = 0x10000, 0x20000, 0x30000
RET = CODE + 0x800  # the stop address the model returns to (never executed)
BUF, FMT, ARGS, STR = DATA + 0x100, DATA + 0x400, DATA + 0x500, DATA + 0x600
SENTINEL = b"\xaa"


def run(fmt, args=(), count=64, strings=()):
    """-> (eax, buffer bytes, error). args: ints (32-bit slots) or str (a pointer to a NUL-terminated string)."""
    model = ft.load_import_models()[("ucrtbase.dll", "__stdio_common_vsprintf_s")]
    mu = Uc(UC_ARCH_X86, UC_MODE_64)
    mu.mem_map(CODE, 0x1000)
    mu.mem_write(CODE, model)
    mu.mem_map(DATA, 0x2000)
    mu.mem_write(BUF, SENTINEL * 128)
    mu.mem_write(FMT, fmt.encode() + b"\0")
    slots, spos = b"", STR
    for a in args:
        if isinstance(a, str):
            mu.mem_write(spos, a.encode() + b"\0")
            slots += struct.pack("<Q", spos)
            spos += 0x40
        else:
            slots += struct.pack("<Q", a & 0xFFFFFFFF)
    mu.mem_write(ARGS, slots)
    mu.mem_map(STACK, 0x2000)
    rsp = STACK + 0x1000
    mu.mem_write(rsp, struct.pack("<Q", RET))
    mu.mem_write(rsp + 0x30, struct.pack("<Q", ARGS))  # arg 6: the va_list (a char* to the first variadic slot)
    for reg, val in ((UC_X86_REG_RCX, 0), (UC_X86_REG_RDX, BUF), (UC_X86_REG_R8, count), (UC_X86_REG_R9, FMT),
                     (UC_X86_REG_RSP, rsp), (UC_X86_REG_RBX, 0x1111), (UC_X86_REG_RSI, 0x2222), (UC_X86_REG_RDI, 0x3333)):
        mu.reg_write(reg, val)
    try:
        mu.emu_start(CODE, RET, count=100000)
    except UcError as e:
        return None, bytes(mu.mem_read(BUF, 128)), str(e)
    saved = (mu.reg_read(UC_X86_REG_RBX), mu.reg_read(UC_X86_REG_RSI), mu.reg_read(UC_X86_REG_RDI), mu.reg_read(UC_X86_REG_RSP))
    assert saved == (0x1111, 0x2222, 0x3333, rsp + 8), saved  # callee-saved registers and the stack pointer restored
    return mu.reg_read(UC_X86_REG_RAX) & 0xFFFFFFFF, bytes(mu.mem_read(BUF, 128)), None


class VsprintfModel(unittest.TestCase):
    def check(self, fmt, args, want, **kw):
        rc, buf, err = run(fmt, args, **kw)
        self.assertIsNone(err)
        self.assertEqual(rc, len(want))
        self.assertEqual(buf[:len(want) + 1], want.encode() + b"\0")
        self.assertEqual(buf[len(want) + 1:], SENTINEL * (127 - len(want)))  # nothing written past the NUL

    def test_camera_target_name(self):
        self.check("CAMERA_TARGET_%02d%02d", (2, 7), "CAMERA_TARGET_0207")
        self.check("CAMERA_TARGET_%02d%02d", (2, 27), "CAMERA_TARGET_0227")

    def test_literal_and_percent(self):
        self.check("a%%b", (), "a%b")
        self.check("", (), "")

    def test_string(self):
        self.check("PLAYER_%s_%02d", ("HOME", 3), "PLAYER_HOME_03")

    def test_numbers(self):
        self.check("%d", (0,), "0")
        self.check("%d", (-45,), "-45")
        self.check("%i", (1234567,), "1234567")
        self.check("%u", (0xFFFFFFFF,), "4294967295")
        self.check("%x", (0xBEEF,), "beef")
        self.check("%5d|%05d|%3d", (42, 42, 12345), "   42|00042|12345")
        self.check("%02d", (-5,), "-5")  # the sign counts toward the width
        self.check("%05d", (-42,), "-0042")

    def test_loud_stops(self):
        for fmt, args, count in (("%f", (0,), 64), ("%ld", (1,), 64), ("%.2d", (1,), 64), ("%s", (), 64),
                                 ("abcdef", (), 4), ("x", (), 0), ("%5s", ("a",), 64), ("%5d", (-42,), 64)):
            rc, _, err = run(fmt, args, count=count, strings=())
            self.assertIsNotNone(err, "%r should stop" % fmt)

    def test_exact_fit(self):
        self.check("abc", (), "abc", count=4)


if __name__ == "__main__":
    unittest.main()
