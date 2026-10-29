#!/usr/bin/env python3
"""test_image_identity.py: a slim capture still identifies the game image and serves its read-only pages.

rendezvous/slim.py drops every page a replay did not read, so the image's header page is often gone and its code and
read-only pages with it. The image's load base and SizeOfImage come from the image file (PES_EXE: pes_full.bin), not
from the capture: an image-range address is the image's, and its code and read-only pages (not the import address
table) are read from the image file when the capture dropped them. Writable pages are never served from the file,
except those of `.impdata` (the relocated game code, rwx in the process) that are not in IMPDATA_VOLATILE_RVA.
Exit 0 green, 1 red, 77 skip (the file system keeps no holes in a sparse file).
"""
import json
import os
import shutil
import struct
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import full_snapshot as fs  # noqa: E402

PAGE = 0x1000
IB = fs.IMAGE_BASE
# image layout (RVA): 0 header, 1 code (.trace), 2 read-only data, 3 read-only data holding the IAT, 4 writable data,
# 5 and 6 .impdata (rwx in the process; page 6 is volatile)
SECTIONS = [(0x1000, 0x1000, b".trace\0\0\0", 0x60000020), (0x2000, 0x1000, b".rdata\0\0", 0x40000040),
            (0x3000, 0x1000, b".idata\0\0", 0x40000040), (0x4000, 0x1000, b".data\0\0\0", 0xC0000020),
            (0x5000, 0x2000, b".impdata", 0xE0000020)]
SIZE_OF_IMAGE = 0x7000
VOLATILE_PAGE = 0x6000
IAT = (0x3000, 0x40)


def make_image(path):
    img = bytearray(SIZE_OF_IMAGE)
    img[0:2] = b"MZ"
    e_lfanew, opt = 0x80, 0x98
    struct.pack_into("<I", img, 0x3C, e_lfanew)
    img[e_lfanew:e_lfanew + 4] = b"PE\0\0"
    struct.pack_into("<H", img, e_lfanew + 6, len(SECTIONS))
    struct.pack_into("<H", img, e_lfanew + 20, 240)  # SizeOfOptionalHeader (PE32+)
    struct.pack_into("<H", img, opt, 0x20B)
    struct.pack_into("<Q", img, opt + 24, IB)
    struct.pack_into("<I", img, opt + 56, SIZE_OF_IMAGE)
    struct.pack_into("<I", img, opt + 60, 0x400)
    struct.pack_into("<II", img, opt + 112 + 12 * 8, *IAT)
    for i, (va, vs, name, ch) in enumerate(SECTIONS):
        off = opt + 240 + 40 * i
        img[off:off + 8] = name
        struct.pack_into("<IIII", img, off + 8, vs, va, vs, va)
        struct.pack_into("<I", img, off + 36, ch)
    for va in range(PAGE, SIZE_OF_IMAGE, PAGE):  # every page after the header carries its own tag
        img[va:va + 8] = struct.pack("<Q", 0x1000 + va)
    img[IAT[0]:IAT[0] + 8] = b"IATSLOT!"
    with open(path, "wb") as f:
        f.write(img)
    return bytes(img)


def make_slim_snapshot(d, image):
    """A slim capture: the image's header page and rdata/IAT/data/.impdata pages are dropped; the code page is kept.
    The image is one read-only mapping up to .impdata, which is a separate rwx mapping (as in the process)."""
    with open(os.path.join(d, "mem.bin"), "wb") as f:
        f.truncate(SIZE_OF_IMAGE)
        f.seek(PAGE)
        f.write(image[PAGE:2 * PAGE])
    mapping = {"va": IB, "size": 0x5000, "perm": "r--p", "name": "", "off": 0, "deferred": False, "device": False}
    impdata = {"va": IB + 0x5000, "size": 0x2000, "perm": "rwxp", "name": "", "off": 0x5000, "deferred": False,
               "device": False}
    with open(os.path.join(d, "index.json"), "w") as f:
        json.dump({"mappings": [mapping, impdata], "holes": []}, f)


def has_hole(path):
    fd = os.open(path, os.O_RDONLY)
    try:
        return os.lseek(fd, 0, os.SEEK_DATA) > 0  # the header page (page 0) is not allocated
    finally:
        os.close(fd)


def main():
    d = tempfile.mkdtemp(dir=HERE, prefix=".image_identity_")
    try:
        image_path = os.path.join(d, "image.bin")
        image = make_image(image_path)
        make_slim_snapshot(d, image)
        if not has_hole(os.path.join(d, "mem.bin")):
            print("SKIP: this file system keeps no holes in a sparse file")
            return 77
        fs.IMPDATA_VOLATILE_RVA = frozenset({VOLATILE_PAGE})  # the synthetic image's volatile page, not the game's
        sm = fs.SnapMem(d, image=image_path)
        ro_page = IB + 2 * PAGE
        iat_page = IB + IAT[0]
        writable_page = IB + 4 * PAGE
        code = IB + PAGE
        impdata_page = IB + 0x5000
        volatile_page = IB + VOLATILE_PAGE
        checks = [
            ("captured code page reads its bytes", lambda: sm.read(code, 8) == image[PAGE:PAGE + 8]),
            ("dropped header page is read-only, so the image serves it (its MZ)", lambda: sm.read(IB, 2) == b"MZ"),
            ("code address is in the image, with the header dropped", lambda: sm.module_base(code) == IB),
            ("dropped read-only page is served from the image", lambda: sm.read(ro_page, 16) == image[2 * PAGE:2 * PAGE + 16]),
            ("dropped read-only page is served, not dropped", lambda: sm.page_ok(ro_page) and not sm.page_dropped(ro_page)),
            ("IAT page is never served from the image", lambda: sm.read(iat_page, 8) is None and not sm.page_ok(iat_page)),
            ("writable page is never served from the image", lambda: sm.read(writable_page, 8) is None and sm.page_dropped(writable_page)),
            ("no image: dropped read-only page is named as not captured",
             lambda: fs.SnapMem(d, image="").page_dropped(ro_page) and fs.SnapMem(d, image="").read(ro_page, 8) is None),
            ("no image: header-dropped image address is not a module", lambda: fs.SnapMem(d, image="").module_base(code) is None),
            ("dropped .impdata page (rwx in the process) is served from the image",
             lambda: sm.read(impdata_page, 16) == image[0x5000:0x5010] and not sm.page_dropped(impdata_page)),
            ("a read ending at the volatile page's edge is served; one crossing into it is refused",
             lambda: sm.read(impdata_page + PAGE - 8, 16) is None and sm.read(impdata_page + PAGE - 8, 8) == image[0x5FF8:0x6000]),
            ("volatile .impdata page is never served", lambda: sm.read(volatile_page, 8) is None and sm.page_dropped(volatile_page)),
        ]

        def check(fn):
            try:
                return bool(fn())
            except Exception:  # an absent method is a failed check, named below
                return False

        results = [(name, check(fn)) for name, fn in checks]
        for name, ok in results:
            print(("ok   " if ok else "FAIL ") + name)
        return 0 if all(ok for _, ok in results) else 1
    finally:
        shutil.rmtree(d, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
