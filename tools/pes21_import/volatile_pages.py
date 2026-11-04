#!/usr/bin/env python3
"""volatile_pages.py: the 4 KB pages of the image's `.impdata` section that differ in any full capture.

The game's relocated code lives in `.impdata` (docs/re/pes-re-protection.md). It is byte-identical in the image and in
a live process except for a few state words and self-patched bytes, which sit on a few pages. A slim capture drops
`.impdata` pages; tools/dev/translate/full_snapshot.py serves them from the image except these pages
(IMPDATA_VOLATILE_RVA). Regenerate that constant after a new image dump or a new game build:

    python3 volatile_pages.py --image pes_full.bin CAPTURE_DIR [CAPTURE_DIR ...]

CAPTURE_DIR is a full capture (index.json + mem.bin with the `.impdata` mapping present, not deferred). Prints the
union of differing pages as sorted image RVAs (VA - 0x140000000), plus how many pages each capture differed in. Read-only.
"""
import argparse
import json
import os
import struct
import sys

PAGE = 0x1000
IMAGE_BASE = 0x140000000


def impdata_extent(img):
    head = img.read(PAGE)
    e = struct.unpack_from("<I", head, 0x3C)[0]
    nsec, = struct.unpack_from("<H", head, e + 6)
    opt_size, = struct.unpack_from("<H", head, e + 20)
    for i in range(nsec):
        s = e + 24 + opt_size + 40 * i
        if head[s:s + 8].rstrip(b"\0") == b".impdata":
            vsize, va = struct.unpack_from("<II", head, s + 8)
            return va, (vsize + PAGE - 1) & ~(PAGE - 1)
    raise SystemExit("volatile_pages: the image has no .impdata section")


def differing_pages(img, rva, size, cap_dir):
    """RVAs of the pages where the capture's .impdata differs from the image."""
    idx = json.load(open(os.path.join(cap_dir, "index.json")))
    m = next((m for m in idx["mappings"] if m["va"] == IMAGE_BASE + rva), None)
    if m is None or m["deferred"] or m["size"] != size:
        raise SystemExit("%s: no full .impdata mapping at %#x" % (cap_dir, IMAGE_BASE + rva))
    out = []
    with open(os.path.join(cap_dir, "mem.bin"), "rb") as cap:
        for off in range(0, size, PAGE):
            if os.pread(cap.fileno(), PAGE, m["off"] + off) != os.pread(img.fileno(), PAGE, rva + off):
                out.append(rva + off)
    return out


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[1])
    ap.add_argument("--image", required=True, help="pes_full.bin")
    ap.add_argument("captures", nargs="+")
    args = ap.parse_args(argv)
    with open(args.image, "rb") as img:
        rva, size = impdata_extent(img)
        union = set()
        for d in args.captures:
            pages = differing_pages(img, rva, size, d)
            print("%s: %d pages differ" % (d, len(pages)), file=sys.stderr)
            union.update(pages)
    print("IMPDATA_VOLATILE_RVA = frozenset({%s})" % ", ".join("0x%X" % p for p in sorted(union)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
