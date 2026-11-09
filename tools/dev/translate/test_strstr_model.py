#!/usr/bin/env python3
"""The named VCRUNTIME140.dll string and virtual-memory import models (import_stubs.json: strstr, strchr, strrchr, VirtualAlloc, VirtualFree) against their contracts, one case per path.

Usage: GF_DIFF_PYTHON=<venv python> python3 test_strstr_model.py
"""
import os
import struct
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import frame_trace as ft  # noqa: E402
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64  # noqa: E402
from unicorn.x86_const import UC_X86_REG_GS_BASE, UC_X86_REG_RAX, UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8, UC_X86_REG_RSP  # noqa: E402

CODE, DATA, STACK = 0x10000, 0x20000, 0x30000
RET = CODE + 0x800
HAY, NEEDLE = DATA + 0x100, DATA + 0x400


def strstr(hay, needle):
    model = ft.load_import_models()[("vcruntime140.dll", "strstr")]
    mu = Uc(UC_ARCH_X86, UC_MODE_64)
    mu.mem_map(CODE, 0x1000)
    mu.mem_write(CODE, model)
    mu.mem_map(DATA, 0x1000)
    mu.mem_write(HAY, hay + b"\0")
    mu.mem_write(NEEDLE, needle + b"\0")
    mu.mem_map(STACK, 0x1000)
    rsp = STACK + 0x800
    mu.mem_write(rsp, struct.pack("<Q", RET))
    for reg, val in ((UC_X86_REG_RCX, HAY), (UC_X86_REG_RDX, NEEDLE), (UC_X86_REG_RSP, rsp)):
        mu.reg_write(reg, val)
    mu.emu_start(CODE, RET, count=100000)
    return mu.reg_read(UC_X86_REG_RAX)


class StrstrModel(unittest.TestCase):
    def test_match_at_the_start(self):
        self.assertEqual(strstr(b"body_anime", b"body"), HAY)

    def test_match_after_a_partial_prefix(self):
        # "bobody": the first "bo" matches and fails at "b"; the scan must restart one byte on, not skip the partial match.
        self.assertEqual(strstr(b"bobody", b"body"), HAY + 2)

    def test_match_at_the_end(self):
        self.assertEqual(strstr(b"the body", b"body"), HAY + 4)

    def test_absent_needle_is_null(self):
        self.assertEqual(strstr(b"cpk_dat/common", b"body"), 0)

    def test_needle_longer_than_hay_is_null(self):
        self.assertEqual(strstr(b"bod", b"body"), 0)

    def test_empty_needle_matches_at_hay(self):
        self.assertEqual(strstr(b"abc", b""), HAY)

    def test_empty_hay_with_a_needle_is_null(self):
        self.assertEqual(strstr(b"", b"a"), 0)


def scan(name, s, c):
    model = ft.load_import_models()[("vcruntime140.dll", name)]
    mu = Uc(UC_ARCH_X86, UC_MODE_64)
    mu.mem_map(CODE, 0x1000)
    mu.mem_write(CODE, model)
    mu.mem_map(DATA, 0x1000)
    mu.mem_write(HAY, s + b"\0")
    mu.mem_map(STACK, 0x1000)
    rsp = STACK + 0x800
    mu.mem_write(rsp, struct.pack("<Q", RET))
    for reg, val in ((UC_X86_REG_RCX, HAY), (UC_X86_REG_RDX, c), (UC_X86_REG_RSP, rsp)):
        mu.reg_write(reg, val)
    mu.emu_start(CODE, RET, count=100000)
    return mu.reg_read(UC_X86_REG_RAX)


class StrchrModels(unittest.TestCase):
    def test_strchr_first_match(self):
        self.assertEqual(scan("strchr", b"a/b/c", ord("/")), HAY + 1)

    def test_strchr_absent_is_null(self):
        self.assertEqual(scan("strchr", b"abc", ord("z")), 0)

    def test_strchr_nul_is_found(self):
        self.assertEqual(scan("strchr", b"abc", 0), HAY + 3)

    def test_strchr_only_the_low_byte_counts(self):
        self.assertEqual(scan("strchr", b"xyz", 0x17F00 | ord("y")), HAY + 1)

    def test_strrchr_last_match(self):
        self.assertEqual(scan("strrchr", b"a/b/c", ord("/")), HAY + 3)

    def test_strrchr_absent_is_null(self):
        self.assertEqual(scan("strrchr", b"abc", ord("z")), 0)

    def test_strrchr_nul_is_found(self):
        self.assertEqual(scan("strrchr", b"abc", 0), HAY + 3)

    def test_strrchr_single_match_at_the_start(self):
        self.assertEqual(scan("strrchr", b"/abc", ord("/")), HAY)


ARENA = ft.OS_ARENA_BASE
BLOCK = 0x400000


class VirtualMemoryModels(unittest.TestCase):
    """KERNEL32 VirtualAlloc / VirtualFree: the os_arena bump allocator and the forms the model refuses."""

    @staticmethod
    def call(name, args, cell=0):
        model = ft.load_import_models()[("kernel32.dll", name)]
        mu = Uc(UC_ARCH_X86, UC_MODE_64)
        mu.mem_map(CODE, 0x1000)
        mu.mem_write(CODE, model)
        mu.mem_map(ARENA, 0x1000)
        mu.mem_write(ARENA, struct.pack("<Q", cell))
        mu.mem_map(STACK, 0x1000)
        rsp = STACK + 0x800
        mu.mem_write(rsp, struct.pack("<Q", RET))
        for reg, val in zip((UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8), args):
            mu.reg_write(reg, val)
        mu.reg_write(UC_X86_REG_RSP, rsp)
        try:
            mu.emu_start(CODE, RET, count=1000)
        except Exception:
            return None, None  # the model's int3
        return mu.reg_read(UC_X86_REG_RAX), struct.unpack("<Q", bytes(mu.mem_read(ARENA, 8)))[0]

    def test_first_commit_is_the_second_arena_block(self):
        self.assertEqual(self.call("VirtualAlloc", (0, 0x1000, 0x3000)), (ARENA + BLOCK, ARENA + 2 * BLOCK))

    def test_next_commit_follows_the_cell_and_rounds_the_size_up(self):
        self.assertEqual(self.call("VirtualAlloc", (0, BLOCK + 1, 0x3000), cell=ARENA + 2 * BLOCK),
                         (ARENA + 2 * BLOCK, ARENA + 4 * BLOCK))

    def test_an_exact_block_size_takes_one_block(self):
        self.assertEqual(self.call("VirtualAlloc", (0, BLOCK, 0x3000), cell=ARENA + BLOCK), (ARENA + BLOCK, ARENA + 2 * BLOCK))

    def test_reset_returns_the_address_and_leaves_the_cell(self):
        self.assertEqual(self.call("VirtualAlloc", (0x2E000000, 0x10000, 0x80000), cell=7), (0x2E000000, 7))

    def test_fixed_address_commit_is_refused(self):
        self.assertEqual(self.call("VirtualAlloc", (0x2E000000, 0x1000, 0x3000)), (None, None))

    def test_other_type_is_refused(self):
        self.assertEqual(self.call("VirtualAlloc", (0, 0x1000, 0x1000)), (None, None))

    def test_free_release_and_decommit_return_true(self):
        self.assertEqual(self.call("VirtualFree", (ARENA + BLOCK, 0, 0x8000))[0], 1)
        self.assertEqual(self.call("VirtualFree", (ARENA + BLOCK, 0x1000, 0x4000))[0], 1)

    def test_free_other_type_is_refused(self):
        self.assertEqual(self.call("VirtualFree", (ARENA + BLOCK, 0, 0x1000)), (None, None))


class WaitModel(unittest.TestCase):
    """KERNEL32 WaitForSingleObject: an INFINITE wait is satisfied, any finite timeout expires (the pump's helper polls with 0)."""

    @staticmethod
    def wait(ms):
        model = ft.load_import_models()[("kernel32.dll", "WaitForSingleObject")]
        mu = Uc(UC_ARCH_X86, UC_MODE_64)
        mu.mem_map(CODE, 0x1000)
        mu.mem_write(CODE, model)
        mu.mem_map(STACK, 0x1000)
        rsp = STACK + 0x800
        mu.mem_write(rsp, struct.pack("<Q", RET))
        mu.reg_write(UC_X86_REG_RCX, 0xA78)
        mu.reg_write(UC_X86_REG_RDX, ms)
        mu.reg_write(UC_X86_REG_RSP, rsp)
        mu.emu_start(CODE, RET, count=100)
        return mu.reg_read(UC_X86_REG_RAX)

    def test_infinite_wait_is_satisfied(self):
        self.assertEqual(self.wait(0xFFFFFFFF), 0)

    def test_zero_timeout_expires(self):
        self.assertEqual(self.wait(0), 0x102)

    def test_finite_timeout_expires(self):
        self.assertEqual(self.wait(1000), 0x102)


class TlsAllocModel(unittest.TestCase):
    """KERNEL32 TlsAlloc: the lowest clear bit of the PEB's TlsBitmapBits (PEB = gs:[0x60], bits at +0x328), set and returned; -1 when all 64 are taken."""

    TEB, PEB = DATA + 0x800, DATA + 0x200

    def call(self, bits):
        model = ft.load_import_models()[("kernel32.dll", "TlsAlloc")]
        mu = Uc(UC_ARCH_X86, UC_MODE_64)
        mu.mem_map(CODE, 0x1000)
        mu.mem_write(CODE, model)
        mu.mem_map(DATA, 0x1000)
        mu.mem_write(self.TEB + 0x60, struct.pack("<Q", self.PEB))
        mu.mem_write(self.PEB + 0x328, struct.pack("<Q", bits))
        mu.mem_map(STACK, 0x1000)
        rsp = STACK + 0x800
        mu.mem_write(rsp, struct.pack("<Q", RET))
        mu.reg_write(UC_X86_REG_GS_BASE, self.TEB)
        mu.reg_write(UC_X86_REG_RSP, rsp)
        mu.emu_start(CODE, RET, count=100)
        return mu.reg_read(UC_X86_REG_RAX), struct.unpack("<Q", bytes(mu.mem_read(self.PEB + 0x328, 8)))[0]

    def test_takes_the_lowest_clear_bit(self):
        self.assertEqual(self.call(0x1FF), (9, 0x3FF))
        self.assertEqual(self.call(0b10110111), (3, 0b10111111))

    def test_a_full_bitmap_returns_minus_one_and_changes_nothing(self):
        self.assertEqual(self.call(0xFFFFFFFFFFFFFFFF), (0xFFFFFFFF, 0xFFFFFFFFFFFFFFFF))


class SystemInfoModel(unittest.TestCase):
    """KERNEL32 GetSystemInfo: the three dwords the callers read (page size +4, processors +0x20, granularity +0x28), no other byte."""

    def test_writes_three_dwords_and_nothing_else(self):
        model = ft.load_import_models()[("kernel32.dll", "GetSystemInfo")]
        mu = Uc(UC_ARCH_X86, UC_MODE_64)
        mu.mem_map(CODE, 0x1000)
        mu.mem_write(CODE, model)
        mu.mem_map(DATA, 0x1000)
        info = DATA + 0x100
        mu.mem_write(info, b"\xAB" * 0x30)
        mu.mem_map(STACK, 0x1000)
        rsp = STACK + 0x800
        mu.mem_write(rsp, struct.pack("<Q", RET))
        mu.reg_write(UC_X86_REG_RCX, info)
        mu.reg_write(UC_X86_REG_RSP, rsp)
        mu.emu_start(CODE, RET, count=100)
        got = bytes(mu.mem_read(info, 0x30))
        expect = bytearray(b"\xAB" * 0x30)
        struct.pack_into("<I", expect, 4, 0x1000)
        struct.pack_into("<I", expect, 0x20, 0x10)
        struct.pack_into("<I", expect, 0x28, 0x10000)
        self.assertEqual(got, bytes(expect))


class SetLastErrorModel(unittest.TestCase):
    """KERNEL32 SetLastError: TEB.LastErrorValue (+0x68) = ecx, rax = the thread block's self pointer (gs:[0x30]), nothing else written."""

    TEB = DATA + 0x800

    def test_stores_the_error_dword_and_returns_the_thread_block(self):
        model = ft.load_import_models()[("kernel32.dll", "SetLastError")]
        mu = Uc(UC_ARCH_X86, UC_MODE_64)
        mu.mem_map(CODE, 0x1000)
        mu.mem_write(CODE, model)
        mu.mem_map(DATA, 0x1000)
        mu.mem_write(self.TEB, b"\xAB" * 0x100)
        mu.mem_write(self.TEB + 0x30, struct.pack("<Q", self.TEB))
        mu.mem_map(STACK, 0x1000)
        rsp = STACK + 0x800
        mu.mem_write(rsp, struct.pack("<Q", RET))
        mu.reg_write(UC_X86_REG_GS_BASE, self.TEB)
        mu.reg_write(UC_X86_REG_RSP, rsp)
        mu.reg_write(UC_X86_REG_RCX, 0xDEAD00000057)
        mu.emu_start(CODE, RET, count=100)
        expect = bytearray(b"\xAB" * 0x100)
        struct.pack_into("<Q", expect, 0x30, self.TEB)
        struct.pack_into("<I", expect, 0x68, 0x57)
        self.assertEqual(bytes(mu.mem_read(self.TEB, 0x100)), bytes(expect))
        self.assertEqual(mu.reg_read(UC_X86_REG_RAX), self.TEB)


if __name__ == "__main__":
    unittest.main()
