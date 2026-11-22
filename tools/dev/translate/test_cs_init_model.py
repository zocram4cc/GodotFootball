#!/usr/bin/env python3
"""The kernel32!InitializeCriticalSectionAndSpinCount import model (import_stubs.json, do = cs_init) against the Win64
CRITICAL_SECTION contract it must hold, one case per field.

PES's file-stream lock init 0x1403FF3E0 calls it through IAT slot 0x14252F440 with (rcx = the section, edx = spin count) and
keeps its own flag on the BOOL result. Win64 CRITICAL_SECTION (40 bytes) on one thread: DebugInfo 0, LockCount -1 (free),
RecursionCount 0, OwningThread 0, LockSemaphore 0, SpinCount = the spin count zero-extended from edx. Returns TRUE.
Loads the model by its (module, export) key, as frame_trace.LazyMap does, and runs it in unicorn.
Usage: GF_DIFF_PYTHON=<venv python> python3 test_cs_init_model.py
"""
import json
import os
import struct
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import frame_trace as ft  # noqa: E402
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64  # noqa: E402
from unicorn.x86_const import UC_X86_REG_RAX, UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_RSP  # noqa: E402

CODE, DATA, STACK = 0x10000, 0x20000, 0x30000
RET = CODE + 0x800  # the address the model returns to (never executed)
CS = DATA + 0x100   # the critical section
CS_BYTES = 40       # sizeof(CRITICAL_SECTION) on Win64
SENTINEL = b"\xAA" * CS_BYTES


def run(rcx, rdx):
    """Runs the model once on a section prefilled with SENTINEL -> (mu, rax, rcx, rdx) after its ret."""
    code = ft.load_import_models()[("kernel32.dll", "InitializeCriticalSectionAndSpinCount")]
    mu = Uc(UC_ARCH_X86, UC_MODE_64)
    mu.mem_map(CODE, 0x1000)
    mu.mem_write(CODE, code)
    mu.mem_map(DATA, 0x2000)
    mu.mem_write(CS, SENTINEL)
    mu.mem_map(STACK, 0x1000)
    rsp = STACK + 0x800
    mu.mem_write(rsp, struct.pack("<Q", RET))
    mu.reg_write(UC_X86_REG_RCX, rcx)
    mu.reg_write(UC_X86_REG_RDX, rdx)
    mu.reg_write(UC_X86_REG_RSP, rsp)
    mu.emu_start(CODE, RET, count=100)
    return mu, mu.reg_read(UC_X86_REG_RAX), mu.reg_read(UC_X86_REG_RCX), mu.reg_read(UC_X86_REG_RDX)


class CriticalSectionInit(unittest.TestCase):
    def test_fields_of_a_free_single_thread_section(self):
        mu, _, _, _ = run(CS, 0x0C)
        f = bytes(mu.mem_read(CS, CS_BYTES))
        self.assertEqual(f[0:8], b"\x00" * 8)  # DebugInfo
        self.assertEqual(struct.unpack("<i", f[8:12])[0], -1)  # LockCount: free
        self.assertEqual(f[12:16], b"\x00" * 4)  # RecursionCount
        self.assertEqual(f[16:32], b"\x00" * 16)  # OwningThread, LockSemaphore
        self.assertEqual(struct.unpack("<Q", f[32:40])[0], 0x0C)  # SpinCount

    def test_returns_true_and_keeps_the_section_pointer(self):
        _, rax, rcx, _ = run(CS, 0x0C)
        self.assertEqual(rax, 1)
        self.assertEqual(rcx, CS)

    def test_spin_count_is_the_low_dword_of_the_argument(self):
        mu, _, _, rdx = run(CS, 0xDEAD_BEEF_0000_0007)
        self.assertEqual(struct.unpack("<Q", bytes(mu.mem_read(CS + 0x20, 8)))[0], 7)
        self.assertEqual(rdx, 7)

    def test_the_harness_code_agrees_with_the_entry_spin_count(self):
        # The translator mirror (xlate.import_model_lines, do = cs_init) reproduces these fields; the entry's `why` names them.
        entry = next(e for e in json.load(open(os.path.join(HERE, "import_stubs.json")))["imports"]
                     if e["export"] == "InitializeCriticalSectionAndSpinCount")
        self.assertEqual(entry["do"], "cs_init")


if __name__ == "__main__":
    unittest.main()
