#!/usr/bin/env python3
"""driver --record merges: a run writes its attempted rows over the record and keeps every other row.

Root cause (M6): a run that did not attempt a row (--only, or --exclude-recorded skipping a VA another results file
records) overwrote the record with its own rows only, so the record lost every row it did not attempt.
Plain python3, no image: the per-function run (driver.finish_one) is stubbed; everything else is the real main().
Run: python3 tools/dev/translate/test_record_merge.py"""
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


def row(va, diffed):
    return {"va": va, "translated": True, "diffed": diffed, "cases": 1, "aborted": 0, "failure_class": "",
            "note": ""}


def rows_of(path):
    with open(path) as f:
        return {r["va"]: r for r in json.load(f)["results"]}

def vas_of(path):
    with open(path) as f:
        return [r["va"] for r in json.load(f)["results"]]


class RecordMerge(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.dir = tmp.name
        self.spec_dir = os.path.join(self.dir, "specs")
        os.makedirs(self.spec_dir)
        # the run's specs: 0x1000 and 0x2000 (file names are the VA without 0x)
        for va in ("1000", "2000"):
            with open(os.path.join(self.spec_dir, va + ".json"), "w") as f:
                f.write("{}")
        self.record = os.path.join(self.dir, "results_test.json")
        # the committed record: 0x1000 (attempted by the run, stale verdict), 0x2000 (attempted by the run, not
        # recorded elsewhere), 0x3000 (not in the spec dir: a row no run of this spec dir attempts)
        self.write_record([row("0x3000", True), row("0x1000", False), row("0x2000", True)])
        self.env = mock.patch.dict(os.environ, {"GF_DIFF_CACHE": os.path.join(self.dir, "cache")})
        self.env.start()
        self.addCleanup(self.env.stop)

    def write_record(self, results):
        with open(self.record, "w") as f:
            json.dump({"results": results, "note": "fixture"}, f, indent=1, sort_keys=True)

    def run_driver(self, extra, verdicts):
        """Run driver.main with the per-function run stubbed: verdicts maps VA -> diffed for the attempted rows."""
        def fake_finish(va, args, pilot):
            return row(va, verdicts[va])
        argv = ["driver.py", "--image", "unused.bin", "--out-dir", os.path.join(self.dir, "out"),
                "--spec-dir", self.spec_dir, "--jobs", "1", "--record", self.record] + extra
        out = io.StringIO()
        with mock.patch.object(sys, "argv", argv), mock.patch.object(driver, "finish_one", fake_finish), \
                contextlib.redirect_stdout(out):
            return driver.main()

    def test_only_run_replaces_its_row_and_keeps_every_other_row(self):
        self.assertEqual(self.run_driver(["--only", "0x1000"], {"0x1000": True}), 0)
        got = rows_of(self.record)
        self.assertEqual(sorted(got), ["0x1000", "0x2000", "0x3000"])
        self.assertEqual(vas_of(self.record), ["0x3000", "0x1000", "0x2000"])  # the record keeps its row order
        self.assertTrue(got["0x1000"]["diffed"])  # attempted: the new verdict
        self.assertEqual(got["0x2000"], row("0x2000", True))  # not attempted: kept as it was
        self.assertEqual(got["0x3000"], row("0x3000", True))

    def test_exclude_recorded_keeps_the_row_another_results_file_owns(self):
        # 0x2000 is recorded by another results file: --exclude-recorded skips it, so the run attempts 0x1000 only
        with mock.patch.object(driver, "recorded_elsewhere", return_value={"0x2000"}):
            self.assertEqual(self.run_driver(["--exclude-recorded"], {"0x1000": True}), 0)
        got = rows_of(self.record)
        self.assertEqual(sorted(got), ["0x1000", "0x2000", "0x3000"])
        self.assertEqual(got["0x2000"], row("0x2000", True))
        self.assertTrue(got["0x1000"]["diffed"])

    def test_a_check_run_never_writes_the_record(self):
        with open(self.record, "rb") as f:
            before = f.read()
        self.run_driver(["--check"], {"0x2000": True})
        with open(self.record, "rb") as f:
            self.assertEqual(f.read(), before)


if __name__ == "__main__":
    unittest.main()
