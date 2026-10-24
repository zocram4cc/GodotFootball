#!/usr/bin/env python3
"""Check a user's PES 2021 image dump against the committed section hashes.

Usage: python3 verify_image.py [--image PES_EXE] [--hashes image_hashes.json]

Reads each stable (byte-identical across two dumps 11 days apart) section
from the dump at file offset = VA - 0x140000000, hashes it, and compares
with tools/pes21_import/image_hashes.json. Exits 1 naming every section
that differs, so the user knows their dump is not the image the pilot
specs were authored against. Sections the game self-patches at a few sites
(the code section .trace and .rdata) are hashed with those sites zeroed
(each entry's "mask": [va, length] pairs), so the bytes the translator
reads are still verified. .data (live globals) and .impdata are checked for
presence only, never hashed.
"""
import hashlib
import json
import os
import struct
import sys

BASE = 0x140000000
HERE = os.path.dirname(os.path.abspath(__file__))
CHUNK = 1 << 22


def section_bounds(image):
    """(name, va, vsz) from the dump's own PE section table."""
    with open(image, "rb") as f:
        dos = f.read(0x40)
    if dos[:2] != b"MZ":
        raise SystemExit("verify_image: %s has no MZ header" % image)
    (e_lfanew,) = struct.unpack_from("<I", dos, 0x3C)
    with open(image, "rb") as f:
        f.seek(e_lfanew)
        coff = f.read(24)
        if coff[:4] != b"PE\0\0":
            raise SystemExit("verify_image: %s has no PE signature" % image)
        count = struct.unpack_from("<H", coff, 6)[0]
        optsz = struct.unpack_from("<H", coff, 20)[0]
        f.seek(e_lfanew + 24 + optsz)
        out = []
        for _ in range(count):
            raw = f.read(40)
            if len(raw) < 40:
                break
            name, vsz, rva = struct.unpack_from("<8sII", raw, 0)
            rsz = struct.unpack_from("<I", raw, 16)[0]
            name = name.split(b"\0")[0].decode("ascii", "replace")
            if vsz == 0 and rsz == 0:
                break
            out.append((name, BASE + rva, vsz))
    return out


def hash_range(image, va, size, mask=()):
    """SHA-256 of [va, va+size) with every masked [mva, mva+len) zeroed."""
    h = hashlib.sha256()
    with open(image, "rb") as f:
        rem, off = size, va - BASE
        while rem > 0:
            n = min(CHUNK, rem)
            f.seek(off)
            b = f.read(n)
            if len(b) != n:
                raise SystemExit(
                    "verify_image: %s too short for section at %#x" % (image, va))
            lo = BASE + off
            for mva, mlen in mask:
                s, e = max(mva, lo), min(mva + mlen, lo + n)
                if s < e:
                    b = b[:s - lo] + bytes(e - s) + b[e - lo:]
            h.update(b)
            rem -= n
            off += n
    return h.hexdigest()


def verify(image, hashes):
    sections = {name: (va, vsz) for name, va, vsz in section_bounds(image)}
    bad, missing = [], []
    for name, want in sorted(hashes["sections"].items()):
        if name not in sections:
            missing.append(name)
            continue
        va, vsz = sections[name]
        if vsz != int(want["vsz"], 16):
            bad.append("%s (size %s, want %s)" % (name, hex(vsz), want["vsz"]))
            continue
        mask = [(int(a, 16), n) for a, n in want.get("mask", [])]
        if hash_range(image, va, vsz, mask) != want["sha256"]:
            bad.append(name)
    # Unstable sections must exist but are never hashed.
    for name in hashes.get("unstable", []):
        if name not in sections:
            missing.append(name + " (unstable, presence only)")
    return bad, missing


def main():
    ap = __import__("argparse").ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--image", default=os.environ.get("PES_EXE", ""),
                    help="dump file (default: $PES_EXE)")
    ap.add_argument("--hashes", default=os.path.join(HERE, "image_hashes.json"))
    args = ap.parse_args()
    if not args.image or not os.path.isfile(args.image):
        print("verify_image: no image (pass --image or set PES_EXE); skipping")
        return 77
    hashes = json.load(open(args.hashes))
    bad, missing = verify(args.image, hashes)
    if missing:
        print("verify_image: FAIL %s missing sections: %s"
              % (args.image, ", ".join(missing)))
        return 1
    if bad:
        print("verify_image: FAIL %s differs in: %s" % (args.image, ", ".join(bad)))
        print("verify_image: take a fresh dump with tools/pes21_import/dump_image.py; "
              "the pilot specs were authored against the hashed image")
        return 1
    print("verify_image: OK %s (%d sections match)" % (args.image, len(hashes["sections"])))
    return 0


if __name__ == "__main__":
    sys.exit(main())
