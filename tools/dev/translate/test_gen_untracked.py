#!/usr/bin/env python3
"""gen_translated --check fails on a generated file that exists on disk but is not tracked by git.

The MergeLane2 merge 4425ef0f regenerated seven pes_fn_* into the worktree without adding them: the disk comparison passed
there while a checkout of the commit could not link. The check must name such a file as drift.
"""
import os
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import gen_translated  # noqa: E402

TRACKED = "core/pes_fn_140000010.cpp"
LOOSE = "core/pes_fn_140000020.cpp"
TEXT = "// generated\n"


def git(cwd, *args):
    subprocess.run(["git", *args], cwd=cwd, check=True, capture_output=True)


class UntrackedLayerFile(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = self.tmp.name
        os.makedirs(os.path.join(self.root, "core"))
        for rel in (TRACKED, LOOSE):
            with open(os.path.join(self.root, rel), "w") as f:
                f.write(TEXT)
        git(self.root, "init", "-q")
        git(self.root, "add", TRACKED)

    def tearDown(self):
        self.tmp.cleanup()

    def test_an_untracked_generated_file_is_drift(self):
        drift = gen_translated.check_layer(self.root, {TRACKED: TEXT, LOOSE: TEXT})
        self.assertEqual(drift, ["untracked %s" % LOOSE])

    def test_a_fully_tracked_layer_has_no_drift(self):
        git(self.root, "add", LOOSE)
        self.assertEqual(gen_translated.check_layer(self.root, {TRACKED: TEXT, LOOSE: TEXT}), [])

    def test_an_exported_tree_outside_git_is_not_drift(self):
        with tempfile.TemporaryDirectory() as plain:
            os.makedirs(os.path.join(plain, "core"))
            with open(os.path.join(plain, TRACKED), "w") as f:
                f.write(TEXT)
            self.assertEqual(gen_translated.check_layer(plain, {TRACKED: TEXT}), [])


if __name__ == "__main__":
    unittest.main()
