#!/usr/bin/env python3
"""Image reads from the driver's worker threads return the bytes at their own addresses.

driver.py runs its functions on a thread pool (--jobs). Every worker reads the image through xlate.pdata_of, which used to
hand all of them one Pdata: one file object, whose read is a seek then a read, so a thread switching between the two read
another thread's offset. The decoded instructions were then wrong at random, so a direct CALL or JMP target could be an
address that no instruction starts at (the gate's diff_impure_recorded: 0x1404405D0 gained the callee 0x14183820B, which
is the middle of an instruction, and failed only on some runs).
Check: many concurrent 16-byte reads through pdata_of, each compared with the file's bytes at that address. Usage:
python3 test_pdata_threads.py
"""
import concurrent.futures
import os
import random
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import pdata  # noqa: E402
import test_extent  # noqa: E402  (the synthetic PE image the extent tests use)
import xlate  # noqa: E402

READ = 16
READS = 40000
THREADS = 8
PADDING = 64 << 20


class ConcurrentImageReads(unittest.TestCase):
    def test_worker_threads_read_their_own_bytes(self):
        pdata.KNOWN = frozenset()  # the synthetic image uses no ledger
        with tempfile.TemporaryDirectory() as d:
            image = os.path.join(d, "img.bin")
            test_extent.build_image(image)
            with open(image, "ab") as f:  # 64 MB of data after the headers: most reads miss the file's buffer
                f.write(os.urandom(PADDING))
            size = os.path.getsize(image)
            rnd = random.Random(7)
            offsets = [rnd.randrange(0, size - READ) for _ in range(READS)]
            fd = os.open(image, os.O_RDONLY)
            try:
                def matches(off):
                    got = xlate.pdata_of(image).read(pdata.IMAGE_BASE + off, READ)
                    return got == os.pread(fd, READ, off)  # a syscall between reads: the threads interleave
                with concurrent.futures.ThreadPoolExecutor(max_workers=THREADS) as ex:
                    results = list(ex.map(matches, offsets, chunksize=32))
            finally:
                os.close(fd)

            self.assertEqual(results.count(False), 0,
                             "%d of %d concurrent reads returned another address's bytes"
                             % (results.count(False), READS))


if __name__ == "__main__":
    unittest.main()
