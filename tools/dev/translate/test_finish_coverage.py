#!/usr/bin/env python3
"""finish_one's coverage record skips a function block_coverage could not decode (its entry is None) instead of
unpacking it. LayerHeaders found it (30-10-26): diff_impure_recorded died in `for v, (r, t) in cov.items() if
isinstance(cov[v], tuple)`, which unpacks None before the filter runs.

ctest translate_finish_coverage (plain python3). The image, the spec and the build are stubbed; the coverage record,
which is the code under test, is the real finish_one.
Usage: python3 test_finish_coverage.py
"""
import argparse
import os
import sys
import unittest
from unittest import mock

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import driver  # noqa: E402

TOP_VA = "0x140001000"     # the function under test: decodable, 3 of its 4 blocks reached
CALLEE_VA = "0x140002000"  # a closure callee block_coverage could not decode: None
NO_GEN_DIR = os.path.join(HERE, "no-gen-dir")  # never created: finish_one reads no addrs file from it


class FinishOneCoverage(unittest.TestCase):
    def test_undecodable_callee_is_skipped_and_the_decodable_one_recorded(self):
        cov = {TOP_VA: (3, 4), CALLEE_VA: None}
        args = argparse.Namespace(image="unused", out_dir=NO_GEN_DIR, spec_dir="unused")
        stubs = {
            "run_one": mock.Mock(return_value={"translated": True, "failure_class": None, "cases": 0,
                                               "requested": 0, "note": ""}),
            "load_spec": mock.Mock(return_value=({}, None)),
            "runnable_extra": mock.Mock(return_value=([CALLEE_VA], {})),
            "block_coverage": mock.Mock(return_value=cov),
            "dead_blocks": mock.Mock(return_value={}),
        }
        with mock.patch.multiple(driver, **stubs):
            res = driver.finish_one(TOP_VA, args, set())
        self.assertEqual(res["coverage"], {TOP_VA: {"reached": 3, "total": 4, "fraction": 0.75}})


if __name__ == "__main__":
    unittest.main()
