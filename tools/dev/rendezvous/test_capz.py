#!/usr/bin/env python3
"""capz.py: a full capture packs and comes back byte-identical; a slim (sparse) capture is never packed, so its apparent
size is never mistaken for a full one; an archived slim capture is reported slim and refused by unpack. Plain python3 +
zstd (ctest capz_test)."""
import json
import os
import shutil
import subprocess
import sys
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

    def test_two_concurrent_packers_do_not_collide(self):
        # auto_slim and a manual pack hit one capture at once and shared its .part file (05-11-26, seq24384).
        import threading
        data = os.urandom(32 * PAGE) + bytes(96 * PAGE)
        d = capture(self.root, "seq20", data=data)
        results = []
        threads = [threading.Thread(target=lambda: results.append(capz.locked(capz.pack, d))) for _ in range(2)]
        for t in threads:
            t.start()
        for t in threads:
            t.join()
        self.assertEqual(sorted(results), ["packed", "skip"])
        self.assertEqual(capz.locked(capz.unpack, d), "unpacked")
        self.assertEqual(open(os.path.join(d, "mem.bin"), "rb").read(), data)


HERE = os.path.dirname(os.path.abspath(__file__))
FULL = bytes(range(256)) * 4096  # 1 MiB of PES-like memory


def archived_slim(root):
    """A footprint slim capture as slim.py leaves it: the sparse slim mem.bin (one page kept), mem.bin.zst a link to the
    archived full copy, and the slim record in index.json. Returns (capture dir, archived copy path)."""
    cap = os.path.join(root, "live2", "pid1", "seq1")
    arch = os.path.join(root, "archive", "pid1", "seq1")
    os.makedirs(cap)
    os.makedirs(arch)
    raw, zst = os.path.join(arch, "full.bin"), os.path.join(arch, "mem.bin.zst")
    with open(raw, "wb") as f:
        f.write(FULL)
    subprocess.run(["zstd", "-q", "-f", "-3", raw, "-o", zst], check=True)
    os.remove(raw)
    os.symlink(zst, os.path.join(cap, "mem.bin.zst"))
    with open(os.path.join(cap, "mem.bin"), "wb") as f:
        f.write(FULL[:PAGE])  # the one page the slim copy keeps
        f.truncate(len(FULL))  # the rest is a hole, as slim.py writes it
    with open(os.path.join(cap, "index.json"), "w") as f:
        json.dump({"rv": {"hook": "synthetic"}, "slim": {"mode": "footprint", "full": "pid1/seq1/mem.bin.zst"}}, f)
    return cap, zst


def snapshot(cap, zst):
    """Every byte a refused unpack must leave alone: the slim copy, its link, the archived copy and the index."""
    with open(os.path.join(cap, "mem.bin"), "rb") as f:
        slim_copy = f.read()
    with open(zst, "rb") as f:
        archived_copy = f.read()
    with open(os.path.join(cap, "index.json")) as f:
        index = f.read()
    return (slim_copy, archived_copy, index, os.path.islink(os.path.join(cap, "mem.bin.zst")),
            os.path.exists(os.path.join(cap, capz.UNPACKED_MARKER)))


def outcome_of(fn, d):
    """What fn(d) did: the exception it raised, or its return value (a refusal is asserted on by its message)."""
    try:
        return fn(d)
    except Exception as e:  # noqa: BLE001 - the outcome is asserted on, whatever its class
        return e


class SlimCapture(unittest.TestCase):
    """A footprint slim capture is reported slim, refused by unpack (its full copy is restored only by an explicit act,
    slim.py --restore) and restored by capz.restore. A packed local capture still unpacks (a guard)."""

    def setUp(self):
        self.root = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.root, ignore_errors=True)
        self.cap, self.zst = archived_slim(self.root)

    def test_status_reports_an_archived_slim_capture_as_slim(self):
        self.assertEqual(capz.status(self.cap), "slim")

    def test_unpack_refuses_an_archived_slim_capture_and_names_the_restore_command(self):
        outcome = outcome_of(capz.unpack, self.cap)
        self.assertIsInstance(outcome, Exception, "unpack returned %r for a slim capture" % (outcome,))
        self.assertIn("slim.py --restore", str(outcome))

    def test_a_refused_unpack_leaves_the_slim_copy_and_the_archive_as_they_were(self):
        before = snapshot(self.cap, self.zst)
        outcome_of(capz.unpack, self.cap)
        self.assertEqual(snapshot(self.cap, self.zst), before)

    def test_the_unpack_command_exits_nonzero_on_a_slim_capture(self):
        r = subprocess.run([sys.executable, os.path.join(HERE, "capz.py"), "unpack", self.cap],
                           capture_output=True, text=True)
        self.assertNotEqual(r.returncode, 0, "unpack exited 0 on a slim capture: %s" % r.stdout)
        self.assertIn("slim.py --restore", r.stderr)

    def test_the_explicit_restore_brings_back_the_archived_copy(self):
        self.assertEqual(capz.restore(self.cap), "unpacked")
        with open(os.path.join(self.cap, "mem.bin"), "rb") as f:
            self.assertEqual(f.read(), FULL)
        self.assertFalse(os.path.islink(os.path.join(self.cap, "mem.bin.zst")))
        self.assertTrue(os.path.exists(self.zst))  # the archived copy stays
        self.assertTrue(os.path.exists(os.path.join(self.cap, capz.UNPACKED_MARKER)))

    def test_restore_refuses_a_capture_with_no_archived_copy(self):
        plain = os.path.join(self.root, "live2", "pid1", "seq2")
        os.makedirs(plain)
        with open(os.path.join(plain, "mem.bin"), "wb") as f:
            f.write(FULL[:PAGE])
        self.assertIsInstance(outcome_of(capz.restore, plain), Exception)

    def test_a_packed_local_capture_still_unpacks(self):
        packed = os.path.join(self.root, "live2", "pid1", "seq3")
        os.makedirs(packed)
        raw = os.path.join(self.root, "raw.bin")
        with open(raw, "wb") as f:
            f.write(FULL)
        subprocess.run(["zstd", "-q", "-f", "-3", raw, "-o", os.path.join(packed, "mem.bin.zst")], check=True)
        self.assertEqual(capz.status(packed), "packed")
        self.assertEqual(capz.unpack(packed), "unpacked")
        with open(os.path.join(packed, "mem.bin"), "rb") as f:
            self.assertEqual(f.read(), FULL)



if __name__ == "__main__":
    unittest.main()
