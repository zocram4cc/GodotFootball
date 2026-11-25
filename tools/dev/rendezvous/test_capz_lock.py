#!/usr/bin/env python3
"""test_capz_lock.py: capz's lock works when the lock file is read-only (a root packer created it).

A lock file that a root process left (root:root 644) cannot be opened for writing by a user, so every user pack or
unpack of that capture died with PermissionError. Locking needs only a read-only descriptor.
Usage: python3 test_capz_lock.py      (exit 0 green, 1 red; the test needs a non-root user to show the failure)
"""
import os
import shutil
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import capz  # noqa: E402


class ReadOnlyLock(unittest.TestCase):
    def test_locked_op_runs_when_the_lock_file_is_read_only(self):
        d = tempfile.mkdtemp(prefix="capz_lock_")
        self.addCleanup(shutil.rmtree, d, ignore_errors=True)
        open(os.path.join(d, "mem.bin.zst"), "wb").close()
        lock = os.path.join(d, capz.LOCK_NAME)
        open(lock, "w").close()
        os.chmod(lock, 0o444)
        self.assertEqual(capz.locked(capz.status, d), "packed")


if __name__ == "__main__":
    unittest.main()
