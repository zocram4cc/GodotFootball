#!/usr/bin/env python3
"""Tests for readset.py's --windows output: each access is tagged with the region it lies in, and the windows keep exact bounds.

Pure functions only (no replay): run with the harness Python, which has unicorn.
"""
import os
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import readset as rs  # noqa: E402

RSP = 0x1C4FEC28  # a capture's entry rsp (seq027): its stack is the window around it
TEB = 0xABF7000


class ClassifyTest(unittest.TestCase):
    def test_the_os_arena_is_its_own_source(self):
        self.assertEqual(rs.classify(rs.ft.OS_ARENA_BASE + 0x1234, RSP, TEB), "os_arena")

    def test_addresses_near_entry_rsp_are_the_stack(self):
        self.assertEqual(rs.classify(RSP - 0xE40, RSP, TEB), "stack")

    def test_the_thread_block_is_teb(self):
        self.assertEqual(rs.classify(TEB + 0x30, RSP, TEB), "teb")

    def test_other_capture_memory_is_the_match(self):
        self.assertEqual(rs.classify(0x17F8B08, RSP, TEB), "match")

    def test_the_arena_wins_over_the_stack_when_rsp_is_near_it(self):
        self.assertEqual(rs.classify(rs.ft.OS_ARENA_BASE, rs.ft.OS_ARENA_BASE + 8, TEB), "os_arena")


class WindowsTest(unittest.TestCase):
    def classify(self, addr):
        return rs.classify(addr, RSP, TEB)

    def test_windows_are_per_source_with_exact_bounds(self):
        acc = {(0x17F8B08, 4): 1, (0x17F8B10, 8): 1, (rs.ft.OS_ARENA_BASE + 0x40, 8): 1}
        got = rs.windows_of(acc, self.classify)
        self.assertIn(("match", 0x17F8B08, 0x17F8B18, 2), got)
        self.assertIn(("os_arena", rs.ft.OS_ARENA_BASE + 0x40, rs.ft.OS_ARENA_BASE + 0x48, 1), got)
        self.assertEqual(len(got), 2)

    def test_accesses_more_than_the_page_gap_apart_are_separate_windows(self):
        acc = {(0x17F8B08, 4): 1, (0x17FBB08, 4): 1}  # three pages apart
        got = [w for w in rs.windows_of(acc, self.classify) if w[0] == "match"]
        self.assertEqual(len(got), 2)


class TsvTest(unittest.TestCase):
    def test_the_tsv_names_the_call_and_gives_one_line_per_window(self):
        lines = rs.windows_tsv("seq027", "trapopen_14079A470", None, RSP, TEB,
                               {(0x17F8B08, 4): 1}, {(rs.ft.OS_ARENA_BASE, 8): 1})
        self.assertEqual(lines[0].split("\t")[:3], ["call", "seq027", "trapopen_14079A470"])
        self.assertEqual(lines[0].split("\t")[3], "-")
        body = [ln.split("\t") for ln in lines[1:]]
        self.assertEqual(sorted(b[1] for b in body), ["match", "os_arena"])
        for b in body:
            self.assertEqual(len(b), 5)
            self.assertEqual(b[0], "window")


if __name__ == "__main__":
    unittest.main()
