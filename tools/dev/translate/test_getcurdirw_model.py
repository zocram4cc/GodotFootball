#!/usr/bin/env python3
"""The kernel32 GetCurrentDirectoryW import model (import_stubs.json code_hex, getcurdirw_model.s) run in unicorn.

The model reads the process's CurrentDirectory through the thread's gs base: TEB+0x60 = PEB, PEB+0x20 = process parameters,
parameters+0x38 = the UNICODE_STRING (Length in bytes), its Buffer at +0x40. A buffer that cannot hold the path and its NUL
(nBufferLength <= characters) gets the size query: characters + 1, nothing written. Otherwise the path is copied with its NUL
and the characters are returned. The test maps its own TEB/PEB/parameters with a known directory.
Usage: GF_DIFF_PYTHON=<venv python> python3 test_getcurdirw_model.py
"""
import os
import struct
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import frame_trace as ft  # noqa: E402
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64  # noqa: E402
from unicorn.x86_const import UC_X86_REG_RAX, UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_RSP, UC_X86_REG_GS_BASE  # noqa: E402

CODE, TEB, PEB, PARAMS, STACK, BUF, DIR = 0x10000, 0x40000, 0x41000, 0x42000, 0x30000, 0x20000, 0x21000
RET = CODE + 0x800
FILL = b"\xAB"  # the buffer's bytes before the call: a write the model should not make shows as a change


def run(n_buffer, directory, buf=BUF):
    """-> (rax, memory at buf, error). The CurrentDirectory is the text `directory` (Length = 2 * its characters)."""
    model = ft.load_import_models()[("kernel32.dll", "GetCurrentDirectoryW")]
    text = directory.encode("utf-16-le")
    mu = Uc(UC_ARCH_X86, UC_MODE_64)
    mu.mem_map(CODE, 0x1000)
    mu.mem_write(CODE, model)
    mu.mem_map(TEB & ~0xFFF, 0x1000)
    mu.mem_write(TEB + 0x60, struct.pack("<Q", PEB))
    mu.mem_map(PEB & ~0xFFF, 0x1000)
    mu.mem_write(PEB + 0x20, struct.pack("<Q", PARAMS))
    mu.mem_map(PARAMS & ~0xFFF, 0x1000)
    mu.mem_write(PARAMS + 0x38, struct.pack("<HHIQ", len(text), len(text) + 2, 0, DIR))
    mu.mem_map(DIR & ~0xFFF, 0x1000)
    mu.mem_write(DIR, text + b"\x00\x00")  # the path in the process's own buffer, with a terminator after it
    mu.mem_map(BUF & ~0xFFF, 0x1000)
    mu.mem_write(BUF, FILL * 0x40)
    mu.mem_map(STACK, 0x1000)
    rsp = STACK + 0x800
    mu.mem_write(rsp, struct.pack("<Q", RET))
    mu.reg_write(UC_X86_REG_GS_BASE, TEB)
    mu.reg_write(UC_X86_REG_RCX, 0xFFFFFFFF00000000 | n_buffer)  # the DWORD's high half is undefined: must not matter
    mu.reg_write(UC_X86_REG_RDX, buf)
    mu.reg_write(UC_X86_REG_RSP, rsp)
    try:
        mu.emu_start(CODE, RET, count=200)
    except Exception as e:
        return None, None, "trap: %s" % e
    return mu.reg_read(UC_X86_REG_RAX), bytes(mu.mem_read(buf, 0x40)), None


class GetCurrentDirectoryWModel(unittest.TestCase):
    DIRECTORY = "Z:\\gf\\pes"  # 9 characters

    def test_the_size_query_returns_the_length_plus_the_nul_and_writes_nothing(self):
        rax, raw, err = run(0, self.DIRECTORY)  # GetCurrentDirectoryW(0, NULL) on the real call shape
        self.assertIsNone(err)
        self.assertEqual(rax, len(self.DIRECTORY) + 1)
        self.assertEqual(raw, FILL * 0x40)

    def test_a_buffer_that_cannot_hold_the_path_and_its_nul_gets_the_size(self):
        rax, raw, err = run(len(self.DIRECTORY), self.DIRECTORY)  # nBufferLength = characters: no room for the NUL
        self.assertIsNone(err)
        self.assertEqual(rax, len(self.DIRECTORY) + 1)
        self.assertEqual(raw, FILL * 0x40)

    def test_a_buffer_that_holds_the_path_gets_the_terminated_copy_and_the_length(self):
        n = len(self.DIRECTORY)
        rax, raw, err = run(n + 1, self.DIRECTORY)
        self.assertIsNone(err)
        self.assertEqual(rax, n)
        self.assertEqual(raw[:2 * n], self.DIRECTORY.encode("utf-16-le"))
        self.assertEqual(raw[2 * n:2 * n + 2], b"\x00\x00")
        self.assertEqual(raw[2 * n + 2:], FILL * (0x40 - 2 * n - 2))  # nothing past the terminator

    def test_an_empty_directory_copies_only_the_terminator(self):
        rax, raw, err = run(1, "")
        self.assertIsNone(err)
        self.assertEqual(rax, 0)
        self.assertEqual(raw[:2], b"\x00\x00")
        self.assertEqual(raw[2:], FILL * 0x3E)


if __name__ == "__main__":
    unittest.main()
