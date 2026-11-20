#!/usr/bin/env python3
"""The ucrtbase toupper import model (import_stubs.json code_hex, toupper_model.s) run in unicorn against toupper's contract:
c - 0x20 for 'a'..'z', else c, the result in eax with rax zero-extended; ecx unchanged; no memory write.
Usage: GF_DIFF_PYTHON=<venv python> python3 test_toupper_model.py
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

CODE, STACK = 0x10000, 0x30000
RET = CODE + 0x800


def run(c):
    model = ft.load_import_models()[("ucrtbase.dll", "toupper")]
    mu = Uc(UC_ARCH_X86, UC_MODE_64)
    mu.mem_map(CODE, 0x1000)
    mu.mem_write(CODE, model)
    mu.mem_map(STACK, 0x1000)
    rsp = STACK + 0x800
    mu.mem_write(rsp, struct.pack("<Q", RET))
    mu.reg_write(UC_X86_REG_RCX, 0xABCD000000000000 | c)  # the high half must not reach eax
    mu.reg_write(UC_X86_REG_RSP, rsp)
    mu.emu_start(CODE, RET, count=100)
    return mu.reg_read(UC_X86_REG_RAX), mu.reg_read(UC_X86_REG_RCX)


class ToupperModel(unittest.TestCase):
    def test_lower_case_letters_map_to_upper(self):
        self.assertEqual(run(ord("a"))[0], ord("A"))
        self.assertEqual(run(ord("z"))[0], ord("Z"))

    def test_other_bytes_are_unchanged(self):
        for c in (ord("A"), ord("Z"), ord("0"), ord("`"), ord("{"), ord("_"), 0x00, 0xFF):
            self.assertEqual(run(c)[0], c)

    def test_rcx_is_left_alone(self):
        rax, rcx = run(ord("q"))
        self.assertEqual(rcx, 0xABCD000000000000 | ord("q"))


if __name__ == "__main__":
    unittest.main()
