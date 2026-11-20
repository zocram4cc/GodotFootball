#!/usr/bin/env python3
"""The import models the ReplayHuman create path calls (import_stubs.json, the code_hex the harness runs), one contract test
each: kernel32!InitializeCriticalSectionAndSpinCount, kernel32!SetLastError, ucrtbase!getenv, winmm!timeGetTime. Each model is
loaded by its (module, export) key, as frame_trace.LazyMap does, and run in unicorn. Plain python3 with unicorn.
Usage: GF_DIFF_PYTHON=<venv python> python3 test_create_path_models.py"""
import os
import struct
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import frame_trace as ft  # noqa: E402
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64  # noqa: E402
from unicorn.x86_const import (UC_X86_REG_RAX, UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_RSP,  # noqa: E402
                               UC_X86_REG_GS_BASE)

CODE, DATA, STACK = 0x10000, 0x20000, 0x30000
RET = CODE + 0x800  # the address the model returns to (never executed)
CS = DATA + 0x100   # the critical section
TEB = DATA + 0x400  # the thread block: gs:[0x30] holds its own address
SENTINEL = 0xAA


def run(module, export, rcx=0, rdx=0, prefill=None):
    """Runs the model's code_hex once; returns (mu, rax, rcx, rdx) after its ret."""
    code = ft.load_import_models()[(module, export)]
    mu = Uc(UC_ARCH_X86, UC_MODE_64)
    mu.mem_map(CODE, 0x1000)
    mu.mem_write(CODE, code)
    mu.mem_map(DATA, 0x2000)
    mu.mem_write(TEB + 0x30, struct.pack("<Q", TEB))
    if prefill is not None:
        mu.mem_write(CS, prefill)
    mu.mem_map(STACK, 0x1000)
    rsp = STACK + 0x800
    mu.mem_write(rsp, struct.pack("<Q", RET))
    mu.reg_write(UC_X86_REG_RCX, rcx)
    mu.reg_write(UC_X86_REG_RDX, rdx)
    mu.reg_write(UC_X86_REG_RSP, rsp)
    mu.reg_write(UC_X86_REG_GS_BASE, TEB)
    mu.emu_start(CODE, RET, count=100)
    return mu, mu.reg_read(UC_X86_REG_RAX), mu.reg_read(UC_X86_REG_RCX), mu.reg_read(UC_X86_REG_RDX)


class CriticalSectionInit(unittest.TestCase):
    """Win64 CRITICAL_SECTION (40 bytes) on one thread: DebugInfo 0, LockCount -1, RecursionCount 0, OwningThread 0,
    LockSemaphore 0, SpinCount = the spin count, zero-extended from edx. Returns TRUE."""

    def test_fields_of_a_free_single_thread_section(self):
        mu, rax, _, _ = run("kernel32.dll", "InitializeCriticalSectionAndSpinCount", CS, 0x0C, prefill=b"\xAA" * 40)
        f = bytes(mu.mem_read(CS, 40))
        self.assertEqual(f[0:8], b"\x00" * 8)  # DebugInfo
        self.assertEqual(struct.unpack("<i", f[8:12])[0], -1)  # LockCount: free
        self.assertEqual(f[12:16], b"\x00" * 4)  # RecursionCount
        self.assertEqual(f[16:32], b"\x00" * 16)  # OwningThread, LockSemaphore
        self.assertEqual(struct.unpack("<Q", f[32:40])[0], 0x0C)  # SpinCount

    def test_returns_true_and_keeps_the_section_pointer(self):
        _, rax, rcx, _ = run("kernel32.dll", "InitializeCriticalSectionAndSpinCount", CS, 0x0C, prefill=b"\xAA" * 40)
        self.assertEqual(rax, 1)
        self.assertEqual(rcx, CS)

    def test_spin_count_is_the_low_dword_of_the_argument(self):
        mu, _, _, rdx = run("kernel32.dll", "InitializeCriticalSectionAndSpinCount", CS, 0xDEAD_BEEF_0000_0007,
                            prefill=b"\xAA" * 40)
        self.assertEqual(struct.unpack("<Q", bytes(mu.mem_read(CS + 0x20, 8)))[0], 7)
        self.assertEqual(rdx, 7)


class SetLastErrorModel(unittest.TestCase):
    """SetLastError(ecx = code): TEB.LastErrorValue (self pointer at gs:[0x30], +0x68) = code; rax = the TEB self pointer."""

    def test_stores_the_code_in_the_thread_block(self):
        mu, rax, _, _ = run("kernel32.dll", "SetLastError", rcx=0x57)
        self.assertEqual(struct.unpack("<I", bytes(mu.mem_read(TEB + 0x68, 4)))[0], 0x57)
        self.assertEqual(rax, TEB)


class ConstantZeroModels(unittest.TestCase):
    """getenv (no name set in this process) and timeGetTime (a fixed clock of 0) both return 0 and touch nothing."""

    def test_getenv_returns_null(self):
        _, rax, _, _ = run("ucrtbase.dll", "getenv", rcx=CS)
        self.assertEqual(rax, 0)

    def test_time_get_time_returns_zero(self):
        _, rax, _, _ = run("winmm.dll", "timeGetTime")
        self.assertEqual(rax, 0)

class SystemInfoModel(unittest.TestCase):
    """GetSystemInfo(rcx = buffer): the Win64 SYSTEM_INFO fields the allocator boot reads, rax and rdx left as they were."""
    BUF = DATA + 0x200

    def test_writes_the_fields_the_boot_reads(self):
        mu, rax, rcx, rdx = run("kernel32.dll", "GetSystemInfo", rcx=self.BUF, rdx=0x55)
        dword = lambda off: struct.unpack("<I", bytes(mu.mem_read(self.BUF + off, 4)))[0]
        self.assertEqual(dword(0x00), 9)  # processor architecture AMD64
        self.assertEqual(dword(0x04), 0x1000)  # page size
        self.assertEqual(dword(0x20), 16)  # dwNumberOfProcessors: the PES host's count (captured 0x143AC3DC8)
        self.assertEqual(dword(0x28), 0x10000)  # allocation granularity
        self.assertEqual(struct.unpack("<Q", bytes(mu.mem_read(self.BUF + 0x10, 8)))[0], 0x7FFFFFFEFFFF)
        self.assertEqual(rcx, self.BUF)
        self.assertEqual(rdx, 0x55)


class BootAllocatorModels(unittest.TestCase):
    """The allocator boot's OS calls: VirtualAlloc returns the base of the one block the port maps for it, VirtualFree
    succeeds, and TlsAlloc hands out index 0 (the index the captured replay reads, DAT_1437067A0)."""

    CELL = 0x2600000000  # the boot window's bump cell (xlate BOOT_CELL)
    FIRST = 0x2600001000  # the first block (the port writes it into the cell)

    def bump(self, size, cell, hint=0):
        """VirtualAlloc(NULL, size, ...) run in unicorn over the bump cell holding cell: returns (rax, the cell after)."""
        code = ft.load_import_models()[("kernel32.dll", "VirtualAlloc")]
        mu = Uc(UC_ARCH_X86, UC_MODE_64)
        mu.mem_map(CODE, 0x1000)
        mu.mem_write(CODE, code)
        mu.mem_map(self.CELL, 0x1000)
        mu.mem_write(self.CELL, struct.pack("<Q", cell))
        mu.mem_map(STACK, 0x1000)
        rsp = STACK + 0x800
        mu.mem_write(rsp, struct.pack("<Q", RET))
        mu.reg_write(UC_X86_REG_RCX, hint)
        mu.reg_write(UC_X86_REG_RDX, size)
        mu.reg_write(UC_X86_REG_RSP, rsp)
        mu.emu_start(CODE, RET, count=100)
        return mu.reg_read(UC_X86_REG_RAX), struct.unpack("<Q", bytes(mu.mem_read(self.CELL, 8)))[0]

    def test_the_first_block_starts_at_the_cell(self):
        rax, cell = self.bump(0x400000, self.FIRST)
        self.assertEqual(rax, self.FIRST)
        self.assertEqual(cell, self.FIRST + 0x400000)

    def test_successive_blocks_are_disjoint(self):
        rax, cell = self.bump(0x3000, self.FIRST + 0x3000)
        self.assertEqual(rax, self.FIRST + 0x3000)
        self.assertEqual(cell, self.FIRST + 0x6000)

    def test_a_commit_hint_returns_itself_and_leaves_the_cell(self):
        # the region allocator commits at the end of its block (0x2600400000): the hint is returned, the cell untouched
        rax, cell = self.bump(0x400000, self.FIRST + 0x7F0000, hint=0x2600400000)
        self.assertEqual(rax, 0x2600400000)
        self.assertEqual(cell, self.FIRST + 0x7F0000)

    def test_virtual_free_succeeds(self):
        _, rax, _, _ = run("kernel32.dll", "VirtualFree", rcx=0x2600000000, rdx=0)
        self.assertEqual(rax, 1)

    def test_tls_alloc_returns_index_zero(self):
        _, rax, _, _ = run("kernel32.dll", "TlsAlloc")
        self.assertEqual(rax, 0)

if __name__ == "__main__":
    unittest.main()
