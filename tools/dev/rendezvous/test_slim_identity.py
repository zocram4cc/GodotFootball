#!/usr/bin/env python3
"""test_slim_identity.py: a slimmed capture still identifies its modules.

A slim keeps the pages its replay read. A replay that runs a DLL's code never reads the DLL's header or export
directory, so a slim built from the replay alone drops them, and the slimmed capture names no export of the module
(its code is then refused). slim.keep_for keeps the identity pages too (every mapping's first page, each PE's header
and export tables), so the slimmed capture still names the export and the code still runs.
Usage: GF_DIFF_PYTHON=<venv python> python3 test_slim_identity.py      (exit 0 green, 1 red)
"""
import json
import os
import shutil
import struct
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "translate"))
import slim  # noqa: E402
import full_snapshot as fs  # noqa: E402

PAGE = 0x1000
DLL = 0x6FFF0000  # a DLL mapped with its header and export directory on separate pages
CODE_RVA, EXPORT_DIR_RVA = 0x1000, 0x2000


def make_dll(d):
    """A three-page PE: header, code (export memset at RVA 0x1000), export directory (module VCRUNTIME140.dll)."""
    pages = [bytearray(PAGE) for _ in range(3)]
    h = pages[0]
    h[0:2] = b"MZ"
    struct.pack_into("<I", h, 0x3C, 0x80)
    h[0x80:0x84] = b"PE\0\0"
    struct.pack_into("<H", h, 0x94, 240)  # SizeOfOptionalHeader
    struct.pack_into("<H", h, 0x98, 0x20B)  # PE32+
    struct.pack_into("<I", h, 0x98 + 56, 0x3000)  # SizeOfImage
    struct.pack_into("<I", h, 0x98 + 60, 0x400)  # SizeOfHeaders
    struct.pack_into("<II", h, 0x98 + 112, EXPORT_DIR_RVA, 0x100)  # export data directory
    pages[1][0x10:0x20] = b"\x90" * 16
    e = pages[2]  # export directory at RVA 0x2000 (page offset 0)
    struct.pack_into("<I", e, 12, 0x2100)  # Name
    struct.pack_into("<I", e, 20, 1)  # NumberOfFunctions
    struct.pack_into("<I", e, 24, 1)  # NumberOfNames
    struct.pack_into("<IIII", e, 28, 0x2200, 0x2208, 0x2210, 0)  # functions, names, ordinals
    e[0x100:0x100 + 17] = b"VCRUNTIME140.dll\0"
    struct.pack_into("<I", e, 0x200, CODE_RVA)
    struct.pack_into("<I", e, 0x208, 0x2220)
    struct.pack_into("<H", e, 0x210, 0)
    e[0x220:0x227] = b"memset\0"
    with open(os.path.join(d, "mem.bin"), "wb") as f:
        for p in pages:
            f.write(p)
    mapping = {"va": DLL, "size": 3 * PAGE, "perm": "r--p", "name": "", "off": 0, "deferred": False, "device": False}
    with open(os.path.join(d, "index.json"), "w") as f:
        json.dump({"mappings": [mapping], "holes": []}, f)


def main():
    d = tempfile.mkdtemp(dir=HERE, prefix=".slim_identity_")
    try:
        cap = os.path.join(d, "cap")
        dst = os.path.join(d, "slim")
        os.makedirs(cap)
        os.makedirs(dst)
        make_dll(cap)
        code_off = CODE_RVA  # the replay read only the code page: file offset of the DLL's code page
        size = os.path.getsize(os.path.join(cap, "mem.bin"))
        keep = slim.keep_for(cap, {code_off}, size)
        slim.sparse_copy(os.path.join(cap, "mem.bin"), os.path.join(dst, "mem.bin"), keep)
        shutil.copy(os.path.join(cap, "index.json"), os.path.join(dst, "index.json"))
        sm = fs.SnapMem(dst)
        checks = [
            ("the module's header page is kept although the replay never read it", lambda: 0 in keep),
            ("the export directory page is kept", lambda: EXPORT_DIR_RVA in keep),
            ("the slimmed capture still names the module's export", lambda: sm.export_of(DLL + CODE_RVA + 0x10) == ("VCRUNTIME140.dll", "memset", 0x10)),
            ("the slimmed capture still reads the code", lambda: sm.read(DLL + CODE_RVA + 0x10, 4) == b"\x90" * 4),
        ]

        def check(fn):
            try:
                return bool(fn())
            except Exception:  # an absent function or method is a failed check, shown below
                return False

        results = [(name, check(fn)) for name, fn in checks]
        for name, ok in results:
            print(("ok   " if ok else "FAIL ") + name)
        return 0 if all(ok for _, ok in results) else 1
    finally:
        shutil.rmtree(d, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
