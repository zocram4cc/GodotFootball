#!/usr/bin/env python3
"""traced_cov.main over a function block_coverage cannot decode (None) that a run recorded hits in. The report
leaves it out of the counts and never unpacks None (the hazard test_finish_coverage.py covers in finish_one).

ctest translate_traced_cov (plain python3). The decoder is stubbed (block_coverage needs a PES image); the report is
the real traced_cov.main over real run and spec directories.
Usage: python3 test_traced_cov.py
"""
import contextlib
import io
import json
import os
import sys
import tempfile
import unittest
from unittest import mock

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import driver  # noqa: E402
import traced_cov  # noqa: E402

BLOCK = 0x10
DECODED_VA = "0x140001000"    # four 0x10-byte blocks; the run executes the first two
UNDECODED_VA = "0x140002000"  # block_coverage gives None, yet the run recorded hits in it


def stub_block_coverage(image, vas, executed):
    """block_coverage's contract: {va: (reached, total)}, a block counted when an executed address lands in it;
    None when the function cannot be decoded."""
    hits = [int(a, 16) for a in executed]
    out = {}
    for va in vas:
        if va == UNDECODED_VA:
            out[va] = None
            continue
        starts = [int(va, 16) + i * BLOCK for i in range(4)]
        out[va] = (sum(any(s <= h < s + BLOCK for h in hits) for s in starts), len(starts))
    return out


class TracedCovUndecodable(unittest.TestCase):
    def test_undecodable_function_with_recorded_hits_is_left_out_of_the_counts(self):
        with tempfile.TemporaryDirectory(dir=HERE) as d:
            specs = os.path.join(d, "specs")
            os.makedirs(specs)
            for va in (DECODED_VA, UNDECODED_VA):
                with open(os.path.join(specs, va[2:] + ".json"), "w") as f:
                    json.dump({}, f)
            run = os.path.join(d, "run_a")
            os.makedirs(os.path.join(run, "executed"))
            with open(os.path.join(run, "record.json"), "w") as f:
                json.dump({"impure_calls": {"140001000": 3, "140002000": 2}}, f)
            with open(os.path.join(run, "executed", "140001000.json"), "w") as f:
                json.dump(["0x140001000", "0x140001010"], f)
            with open(os.path.join(run, "executed", "140002000.json"), "w") as f:
                json.dump(["0x140002000"], f)
            out = os.path.join(d, "out.json")
            argv = ["traced_cov.py", "--image", "unused", "--spec-dir", specs, "--run", run, "--out", out]
            with mock.patch.object(driver, "block_coverage", new=stub_block_coverage), \
                    mock.patch.object(sys, "argv", argv), contextlib.redirect_stdout(io.StringIO()):
                traced_cov.main()
            with open(out) as f:
                report = json.load(f)
        rows, summary = report["specs"], report["summary"]
        self.assertEqual(rows["140001000"]["own"], {"reached": 2, "total": 4, "fraction": 0.5})
        self.assertEqual(rows["140002000"]["per_run"]["run_a"]["reached"], 0)
        self.assertIsNone(rows["140002000"]["own"]["fraction"])
        self.assertEqual(summary["reached"], 2)
        self.assertEqual(summary["floor_union"], 1)


if __name__ == "__main__":
    unittest.main()
