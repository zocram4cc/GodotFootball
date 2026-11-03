#!/usr/bin/env python3
"""capz.py: a full capture packs and comes back byte-identical; a slim (sparse) capture is never packed, so its apparent
size is never mistaken for a full one; an unpacked slim capture stays sparse. Plain python3 + zstd (ctest capz_test)."""
import os
import shutil
import tempfile
import unittest

import capz

PAGE = 4096


def capture(root, name, data=None, sparse_size=0):
    d = os.path.join(root, name)
    os.makedirs(d)
    open(os.path.join(d, "index.json"), "w").write("{}")
    with open(os.path.join(d, "mem.bin"), "wb") as f:
        if data is not None:
            f.write(data)
        else:
            f.write(b"\x5a" * PAGE)
            f.truncate(sparse_size)
    return d


class Capz(unittest.TestCase):
    def setUp(self):
        self.root = tempfile.mkdtemp()
        self.saved = capz.SLIM_BYTES
        capz.SLIM_BYTES = 64 * PAGE

    def tearDown(self):
        capz.SLIM_BYTES = self.saved
        shutil.rmtree(self.root)

    def test_full_capture_round_trips_byte_identical(self):
        data = os.urandom(32 * PAGE) + bytes(96 * PAGE)
        d = capture(self.root, "seq1", data=data)
        self.assertEqual(capz.pack(d), "packed")
        self.assertFalse(os.path.exists(os.path.join(d, "mem.bin")))
        self.assertEqual(capz.status(d), "packed")
        self.assertEqual(capz.unpack(d), "unpacked")
        self.assertEqual(open(os.path.join(d, "mem.bin"), "rb").read(), data)

    def test_sparse_slim_capture_is_not_packed(self):
        d = capture(self.root, "seq2", sparse_size=4096 * PAGE)
        self.assertEqual(capz.status(d), "slim")
        self.assertEqual(capz.pack(d), "skip")
        self.assertTrue(os.path.exists(os.path.join(d, "mem.bin")))

    def test_unpacked_full_capture_has_no_holes(self):
        # A full capture's zero pages must come back allocated: SnapMem reads a hole of a capture as a page a slim pass
        # dropped and refuses the replay (03-11-26: sparse unpack made zero heap pages unreadable).
        data = os.urandom(8 * PAGE) + bytes(120 * PAGE)
        d = capture(self.root, "seq3", data=data)
        raw = os.path.join(d, "mem.bin")
        capz.pack(d)
        capz.unpack(d)
        self.assertEqual(capz.allocated(raw), os.path.getsize(raw))

    def test_launch_dir_expands_to_its_captures(self):
        a = capture(self.root, "seq10", sparse_size=PAGE)
        b = capture(self.root, "seq11", sparse_size=PAGE)
        self.assertEqual(list(capz.captures([self.root])), sorted([a, b]))


if __name__ == "__main__":
    unittest.main()
