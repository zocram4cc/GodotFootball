#!/usr/bin/env python3
"""pdata.Pdata's relocated-cave rule (pdata.py _cave): a block no .pdata entry covers, entered by a jump from a function and returning
into it, is part of that function; a block that calls, returns, reads the machine, or does not return into the function is not.

The image is synthetic: a PE header with one .pdata entry for the function F = IMAGE_BASE + 0x1000, its unwind data zeroed, and
the cave at IMAGE_BASE + 0x1100 (no entry covers it). Usage: python3 test_pdata_cave.py (no PES install needed).
"""
import io
import os
import struct
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import pdata  # noqa: E402

BASE = pdata.IMAGE_BASE
F, CAVE = BASE + 0x1000, BASE + 0x1100
RESUME = F + 0x10  # the body's own instruction the cave returns to
PDATA_RVA, UNWIND_RVA = 0x2000, 0x3000


def jmp(src, dst):
    return b"\xe9" + struct.pack("<i", dst - (src + 5))


def image(cave_body):
    """A flat PE image: F's entry jumps to the cave; F's byte at RESUME returns; the cave is cave_body."""
    buf = bytearray(0x4000)
    struct.pack_into("<I", buf, 0x3C, 0x80)
    buf[0x80:0x84] = b"PE\0\0"
    struct.pack_into("<HHIIIHH", buf, 0x84, 0x8664, 0, 0, 0, 0, 0xF0, 0x22)
    struct.pack_into("<II", buf, 0x98 + 112 + 8 * 3, PDATA_RVA, 12)  # the exception directory: one entry
    struct.pack_into("<III", buf, PDATA_RVA, 0x1000, 0x1080, UNWIND_RVA)  # F = [0x1000, 0x1080), zeroed unwind data
    buf[0x1000:0x1005] = jmp(F, CAVE)
    buf[0x1010:0x1011] = b"\xc3"  # RESUME: ret
    buf[0x1100:0x1100 + len(cave_body)] = cave_body
    return bytes(buf)


GAME_LOGIC = bytes.fromhex("837e3002") + bytes.fromhex("b83e000000")  # cmp dword ptr [rsi+0x30], 2 ; mov eax, 0x3e
RETURNS_INTO_F = GAME_LOGIC + jmp(CAVE + len(GAME_LOGIC), RESUME)


def cave_extent(cave_body):
    pd = pdata.Pdata(io.BytesIO(image(cave_body)))
    ranges = pd.extent(F)
    return ranges, pd


class RelocatedCave(unittest.TestCase):
    def test_a_game_logic_cave_that_returns_into_its_function_is_part_of_it(self):
        ranges, pd = cave_extent(RETURNS_INTO_F)
        self.assertTrue(any(b <= CAVE < e for b, e in ranges), ranges)
        decoded = [i.address for i in pd.decode(pd._md_get(), F)]
        self.assertIn(CAVE, decoded)
        self.assertIn(CAVE + len(GAME_LOGIC), decoded)

    def test_a_cave_that_returns_instead_of_jumping_back_is_not_part_of_the_function(self):
        ranges, _ = cave_extent(GAME_LOGIC + b"\xc3")
        self.assertFalse(any(b <= CAVE < e for b, e in ranges), ranges)

    def test_a_cave_that_calls_is_not_part_of_the_function(self):
        body = GAME_LOGIC + b"\xe8\x00\x00\x00\x00" + jmp(CAVE + len(GAME_LOGIC) + 5, RESUME)
        ranges, _ = cave_extent(body)
        self.assertFalse(any(b <= CAVE < e for b, e in ranges), ranges)

    def test_a_cave_that_reads_the_cpu_is_not_part_of_the_function(self):
        body = GAME_LOGIC + b"\x0f\xa2" + jmp(CAVE + len(GAME_LOGIC) + 2, RESUME)  # cpuid
        ranges, _ = cave_extent(body)
        self.assertFalse(any(b <= CAVE < e for b, e in ranges), ranges)

    def test_a_cave_that_jumps_out_of_the_function_is_not_part_of_it(self):
        body = GAME_LOGIC + jmp(CAVE + len(GAME_LOGIC), BASE + 0x1800)  # back into nothing of F
        ranges, _ = cave_extent(body)
        self.assertFalse(any(b <= CAVE < e for b, e in ranges), ranges)


if __name__ == "__main__":
    unittest.main()
