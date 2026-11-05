#!/usr/bin/env python3
"""auto_slim never slims a capture an agent unpacked (capz's marker) or one whose hook a keep file lists."""
import json
import os
import shutil
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import auto_slim  # noqa: E402
import capz  # noqa: E402


class Candidates(unittest.TestCase):
    def setUp(self):
        self.live2 = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.live2)
        auto_slim.full = lambda d: True  # every capture counts as full; the test is about the selection rules
        self.dirs = {}
        for seq, hook in (("seq1", "plain"), ("seq2", "wanted"), ("seq3", "plain")):
            d = os.path.join(self.live2, "pid1", seq)
            os.makedirs(d)
            json.dump({"rv": {"hook": hook}}, open(os.path.join(d, "index.json"), "w"))
            self.dirs[seq] = d
        open(os.path.join(self.dirs["seq3"], capz.UNPACKED_MARKER), "w").close()
        self.keep_file = os.path.join(self.live2, "wanted.txt")
        open(self.keep_file, "w").write("# comment\nwanted\n")

    def test_marker_and_keep_file_protect_their_captures(self):
        keep = auto_slim.read_keep_files([self.keep_file, os.path.join(self.live2, "absent.txt")])
        self.assertEqual(keep, {"wanted"})
        got = auto_slim.candidates(self.live2, 1, keep, 0, set())
        self.assertEqual(got, [self.dirs["seq1"]])

    def test_without_them_every_full_capture_is_a_candidate(self):
        os.remove(os.path.join(self.dirs["seq3"], capz.UNPACKED_MARKER))
        got = auto_slim.candidates(self.live2, 1, set(), 0, set())
        self.assertEqual(got, [self.dirs[s] for s in ("seq1", "seq2", "seq3")])


if __name__ == "__main__":
    unittest.main()
