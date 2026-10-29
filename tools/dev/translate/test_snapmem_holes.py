#!/usr/bin/env python3
"""test_snapmem_holes.py: a page a slim capture dropped is never read as zeros.

rendezvous/slim.py leaves the pages it dropped as holes of the sparse mem.bin. pread over a hole returns zeros, so a
replay that touches one would run on bytes the game never had. SnapMem must refuse such a page: page_ok is False,
read returns None, and page_dropped names it. Exit 0 green, 1 red, 77 skip (the file system keeps no holes).
"""
import json
import os
import shutil
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import full_snapshot as fs  # noqa: E402

PAGE = 0x1000
BASE = 0x10000


def make_snapshot(d):
    with open(os.path.join(d, "mem.bin"), "wb") as f:
        f.truncate(3 * PAGE)  # three pages: data, a hole, data
        f.seek(0)
        f.write(b"A" * PAGE)
        f.seek(2 * PAGE)
        f.write(b"C" * PAGE)
    mapping = {"va": BASE, "size": 3 * PAGE, "perm": "r--p", "name": "", "off": 0, "deferred": False, "device": False}
    with open(os.path.join(d, "index.json"), "w") as f:
        json.dump({"mappings": [mapping], "holes": []}, f)


def has_hole(path):
    fd = os.open(path, os.O_RDONLY)
    try:
        return os.lseek(fd, PAGE, os.SEEK_DATA) > PAGE  # the middle page is not allocated
    finally:
        os.close(fd)


def main():
    d = tempfile.mkdtemp(dir=HERE, prefix=".snapmem_holes_")
    try:
        make_snapshot(d)
        if not has_hole(os.path.join(d, "mem.bin")):
            print("SKIP: this file system keeps no holes in a sparse file")
            return 77
        sm = fs.SnapMem(d)
        def check(fn):
            try:
                return bool(fn())
            except Exception:  # an absent method is a failed check, named below
                return False

        checks = [
            ("captured page reads its bytes", lambda: sm.page_ok(BASE) and sm.read(BASE, 4) == b"AAAA"),
            ("dropped page is not readable", lambda: not sm.page_ok(BASE + PAGE)),
            ("dropped page reads None, not zeros", lambda: sm.read(BASE + PAGE, 8) is None),
            ("a range over a dropped page reads None", lambda: sm.read(BASE, 3 * PAGE) is None),
            ("dropped page is named as not captured", lambda: sm.page_dropped(BASE + PAGE + 0x10)),
            ("captured page after the hole still reads", lambda: sm.read(BASE + 2 * PAGE, 4) == b"CCCC"),
        ]
        results = [(name, check(fn)) for name, fn in checks]
        for name, ok in results:
            print(("ok   " if ok else "FAIL ") + name)
        return 0 if all(ok for _, ok in results) else 1
    finally:
        shutil.rmtree(d, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
