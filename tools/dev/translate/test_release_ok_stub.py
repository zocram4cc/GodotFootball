#!/usr/bin/env python3
"""The heap-boundary stub of the pool region release 0x1403FC080 (stubs.json, do = release_ok) against its two callers.

0x1403FC080(addr, size) releases the committed OS regions of a range (VirtualQuery, then VirtualFree) and returns 0 when every
region was released, else -1. Its callers branch on the result: 0 adds the released size to the pool's count (0x1403FB5B0's
caller) and updates the pool's bytes (0x1403FE710's caller). A stub that leaves rax as it was would make those branches read
garbage, so the stub must answer 0. Policy 6: a heap allocator entry is a boundary, and the OS region release is not GF's.
Usage: python3 test_release_ok_stub.py (no unicorn needed)
"""
import os
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import stubs  # noqa: E402
import xlate  # noqa: E402

REGION_RELEASE = 0x1403FC080


class RegionReleaseStub(unittest.TestCase):
    def test_the_stub_table_models_the_release_as_success(self):
        self.assertEqual(stubs.load()[REGION_RELEASE]["do"], "release_ok")

    def test_the_translator_stub_answers_zero(self):
        # The C++ for the stub sets the return register to 0 (success); the call's own ret pops the return address.
        self.assertEqual(xlate.stub_lines("release_ok"), ["s.gpr[0] = 0ULL;"])

    def test_the_stub_is_a_model_in_every_spec_set(self):
        self.assertIn("release_ok", stubs.MODEL_DOS)


if __name__ == "__main__":
    unittest.main()
