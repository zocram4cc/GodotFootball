#!/usr/bin/env python3
"""rv_watch.sweep_done / parked_captures: a call the DLL has finished (done_<seq>) loses its three run files and its
`seen` entry; a call still parked (capture, maybe release, no done) keeps every file, since the DLL may still be polling
its release; a pass never walks a capture it swept. Plain python3 (ctest rv_sweep_test)."""
import os
import shutil
import tempfile
import unittest

import rv_watch


class SweepDone(unittest.TestCase):
    def setUp(self):
        self.run = tempfile.mkdtemp()

    def tearDown(self):
        shutil.rmtree(self.run)

    def touch(self, *names):
        for n in names:
            open(os.path.join(self.run, n), "w").close()

    def test_finished_call_is_swept_and_forgotten(self):
        self.touch("capture_7.txt", "release_7", "done_7")
        seen = {os.path.join(self.run, "capture_7.txt")}
        self.assertEqual(rv_watch.sweep_done(self.run, os.listdir(self.run), seen), 1)
        self.assertEqual(os.listdir(self.run), [])
        self.assertEqual(seen, set())

    def test_parked_call_keeps_its_files(self):
        self.touch("capture_8.txt", "release_8", "capture_9.txt")
        seen = {os.path.join(self.run, "capture_8.txt"), os.path.join(self.run, "capture_9.txt")}
        self.assertEqual(rv_watch.sweep_done(self.run, os.listdir(self.run), seen), 0)
        self.assertEqual(sorted(os.listdir(self.run)), ["capture_8.txt", "capture_9.txt", "release_8"])
        self.assertEqual(len(seen), 2)

    def test_other_seqs_are_untouched(self):
        self.touch("capture_10.txt", "release_10", "done_10", "capture_100.txt", "release_100", "rv_dll.log")
        rv_watch.sweep_done(self.run, os.listdir(self.run), set())
        self.assertEqual(sorted(os.listdir(self.run)), ["capture_100.txt", "release_100", "rv_dll.log"])

    def test_a_pass_walks_only_captures_it_did_not_sweep(self):
        # List once, sweep, walk the listing: a capture swept in this pass must not be walked (its file is gone;
        # rv_parse_capture raised FileNotFoundError on 03-11-26).
        self.touch("capture_5.txt", "release_5", "done_5", "capture_6.txt")
        names = os.listdir(self.run)
        rv_watch.sweep_done(self.run, names, set())
        walk = rv_watch.parked_captures(names)
        self.assertEqual(walk, ["capture_6.txt"])
        self.assertTrue(all(os.path.exists(os.path.join(self.run, n)) for n in walk))

    def test_a_done_file_still_being_written_is_left_alone(self):
        # The DLL writes done_<seq>.tmp, then renames it. Deleting the .tmp made the rename fail, so done_<seq> never
        # appeared and the call's capture and release were never swept (03-11-26: 3000 left behind in 3 minutes).
        self.touch("capture_4.txt", "release_4", "done_4.tmp")
        names = os.listdir(self.run)
        self.assertEqual(rv_watch.sweep_done(self.run, names, set()), 0)
        self.assertEqual(sorted(os.listdir(self.run)), ["capture_4.txt", "done_4.tmp", "release_4"])
        self.assertEqual(rv_watch.parked_captures(names), ["capture_4.txt"])


if __name__ == "__main__":
    unittest.main()
