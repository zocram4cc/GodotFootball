#!/usr/bin/env python3
"""The kernel32!WaitForSingleObject import model (import_stubs.json, do = wait) against the two callers' contracts.

Two PES callers use the same export. The pump's helper 0x141B389D0 waits with a zero timeout and needs WAIT_TIMEOUT (0x102) to
end its drain loop. The file handle's busy-semaphore acquire 0x1423D90E0 waits INFINITE (0xFFFFFFFF) and loops while the
result is 0x102: an INFINITE wait must answer WAIT_OBJECT_0 (0), or the acquire spins (a replay sat at 0x1423D9117 to the
50M-instruction budget with the old constant 258 for every wait). Loads the model by its (module, export) key, as
frame_trace.LazyMap does, and runs it in unicorn. Usage: GF_DIFF_PYTHON=<venv python> python3 test_wait_model.py
"""
import os
import struct
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import frame_trace as ft  # noqa: E402
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64  # noqa: E402
from unicorn.x86_const import UC_X86_REG_RAX, UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_RSP  # noqa: E402

CODE, STACK = 0x10000, 0x30000
RET = CODE + 0x800  # the address the model returns to (never executed)
INFINITE = 0xFFFFFFFF
WAIT_OBJECT_0, WAIT_TIMEOUT = 0, 0x102


def run_wait(handle, milliseconds):
    """-> rax after the model runs as WaitForSingleObject(handle, milliseconds)."""
    code = ft.load_import_models()[("kernel32.dll", "WaitForSingleObject")]
    mu = Uc(UC_ARCH_X86, UC_MODE_64)
    mu.mem_map(CODE, 0x1000)
    mu.mem_write(CODE, code)
    mu.mem_map(STACK, 0x1000)
    rsp = STACK + 0x800
    mu.mem_write(rsp, struct.pack("<Q", RET))
    mu.reg_write(UC_X86_REG_RCX, handle)
    mu.reg_write(UC_X86_REG_RDX, milliseconds)
    mu.reg_write(UC_X86_REG_RSP, rsp)
    mu.emu_start(CODE, RET, count=100)
    return mu.reg_read(UC_X86_REG_RAX)


class WaitModel(unittest.TestCase):
    def test_an_infinite_wait_is_satisfied(self):
        # The busy-semaphore acquire loops while the result is WAIT_TIMEOUT: an INFINITE wait must not answer it.
        self.assertEqual(run_wait(0x4, INFINITE), WAIT_OBJECT_0)

    def test_a_zero_timeout_expires(self):
        # The pump helper's drain loop ends on WAIT_TIMEOUT; a zero timeout must answer it.
        self.assertEqual(run_wait(0x4, 0), WAIT_TIMEOUT)

    def test_a_finite_timeout_expires(self):
        self.assertEqual(run_wait(0x4, 1000), WAIT_TIMEOUT)


if __name__ == "__main__":
    unittest.main()
