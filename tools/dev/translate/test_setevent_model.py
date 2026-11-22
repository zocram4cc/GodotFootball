#!/usr/bin/env python3
"""The kernel32!SetEvent import model (import_stubs.json, do = return) against its contract.

PES's file-handle state machine 0x1414A9680 calls SetEvent at the end of its completion path (IAT slot 0x14252F6E8, no
arguments set up by the caller). SetEvent returns a BOOL: TRUE (non-zero) on success. The model answers TRUE and changes
nothing else: no thread waits on the event in a GF replay (the producers are not run). Loads the model by its (module,
export) key, as frame_trace.LazyMap does, and runs it in unicorn. Usage: GF_DIFF_PYTHON=<venv python> python3 test_setevent_model.py
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
RET = CODE + 0x800  # the address the model returns to (never executed)


def run_setevent(handle):
    """-> (rax, rcx) after the model runs as SetEvent(handle)."""
    code = ft.load_import_models()[("kernel32.dll", "SetEvent")]
    mu = Uc(UC_ARCH_X86, UC_MODE_64)
    mu.mem_map(CODE, 0x1000)
    mu.mem_write(CODE, code)
    mu.mem_map(STACK, 0x1000)
    rsp = STACK + 0x800
    mu.mem_write(rsp, struct.pack("<Q", RET))
    mu.reg_write(UC_X86_REG_RCX, handle)
    mu.reg_write(UC_X86_REG_RSP, rsp)
    mu.emu_start(CODE, RET, count=100)
    return mu.reg_read(UC_X86_REG_RAX), mu.reg_read(UC_X86_REG_RCX)


class SetEventModel(unittest.TestCase):
    def test_returns_true(self):
        rax, _ = run_setevent(0x4)
        self.assertEqual(rax, 1)

    def test_leaves_the_handle_argument_alone(self):
        _, rcx = run_setevent(0x4)
        self.assertEqual(rcx, 0x4)


if __name__ == "__main__":
    unittest.main()
