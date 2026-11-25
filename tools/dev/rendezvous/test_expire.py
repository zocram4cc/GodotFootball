#!/usr/bin/env python3
"""test_expire.py: the expiry dry run lists the captures that no record and no open lane cites and that are older
than the cut-off, with their bytes, and deletes nothing."""
import contextlib
import io
import os
import shutil
import sys
import tempfile
import time
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import expire  # noqa: E402

DAY = 86400


class DryRun(unittest.TestCase):
    def setUp(self):
        self.root = tempfile.mkdtemp(prefix="expire_")
        self.addCleanup(shutil.rmtree, self.root, ignore_errors=True)
        self.live2 = os.path.join(self.root, "live2")
        self.now = time.time()
        # launch, seq, age in days
        for launch, seq, age in (("pid1", "seq1", 20), ("pid1", "seq2", 20), ("pid2", "seq1", 20),
                                 ("pid3", "seq1", 3), ("pid4", "seq1", 20), ("pid5", "seq1", 20)):
            d = os.path.join(self.live2, launch, seq)
            os.makedirs(d)
            with open(os.path.join(d, "mem.bin"), "wb") as f:
                f.write(b"x" * 1000)
            os.utime(d, (self.now - age * DAY, self.now - age * DAY))
        self.records = ["proof on live2/pid1/seq2 and the launch live2/pid2, bit-exact"]
        self.cited = os.path.join(self.root, "cited.txt")
        open(self.cited, "w").write("# open lane\npid4/seq1\n")

    def test_lists_old_uncited_captures_only(self):
        got = expire.plan(self.live2, self.records, expire.read_cited([self.cited]), 14, self.now)
        self.assertEqual([token for token, _, _ in got], ["pid1/seq1", "pid5/seq1"])

    def test_reports_their_bytes(self):
        got = expire.plan(self.live2, self.records, expire.read_cited([self.cited]), 14, self.now)
        self.assertTrue(all(nbytes >= 1000 for _, _, nbytes in got), got)

    def test_deletes_nothing(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            expire.main(["--live2", self.live2, "--cited", self.cited, "--days", "14"])
        self.assertIn("dry run", out.getvalue())
        for launch, seq in (("pid1", "seq1"), ("pid1", "seq2"), ("pid2", "seq1"), ("pid3", "seq1"),
                            ("pid4", "seq1"), ("pid5", "seq1")):
            self.assertTrue(os.path.exists(os.path.join(self.live2, launch, seq, "mem.bin")))


if __name__ == "__main__":
    unittest.main()
