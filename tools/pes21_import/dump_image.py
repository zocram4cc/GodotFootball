#!/usr/bin/env python3
"""Read-only dump of the running PES 2021 executable image.

Usage: sudo -n python3 dump_image.py OUT.bin [--force] [--pid PID]

Finds the PES2021.exe process, reads its whole image range through
/proc/<pid>/mem (opened O_RDONLY; never writes, never pauses the game),
and writes a flat dump: file offset = VA - 0x140000000. Also writes
OUT.bin.maps (the mappings used, the PE section table, unreadable ranges).

The end address is derived from the PE section table read from the
process's own memory (max section end, page-rounded), not a constant, so
the script keeps working if the image layout changes. Unreadable pages
are zero-filled and listed in the .maps file.

Refuses to overwrite an existing OUT.bin without --force.
"""
import argparse
import os
import struct
import subprocess
import sys

BASE = 0x140000000
PAGE = 0x1000
CHUNK = 1 << 22  # 4 MiB per read: the dump streams, never held whole


def find_pid():
    out = subprocess.check_output(["pgrep", "-x", "PES2021.exe"]).split()
    if not out:
        raise SystemExit("dump_image: no PES2021.exe process found "
                         "(start PES 2021 and reach the main menu first)")
    return int(out[0])


def read_mem(mem, va, n):
    """Read n bytes at VA; short/failed reads return None."""
    try:
        mem.seek(va)
        b = mem.read(n)
    except OSError:
        return None
    return b if len(b) == n else None


def image_end(mem):
    """End of the image from the PE section table in process memory.

    Returns (end_va, sections) with sections a list of (name, va, vsz).
    Falls back to the optional header's SizeOfImage if the section table
    is unreadable.
    """
    dos = read_mem(mem, BASE, 0x40)
    if dos is None or dos[:2] != b"MZ":
        raise SystemExit("dump_image: no MZ header at %#x" % BASE)
    (e_lfanew,) = struct.unpack_from("<I", dos, 0x3C)
    coff = read_mem(mem, BASE + e_lfanew, 24)
    if coff is None or coff[:4] != b"PE\0\0":
        raise SystemExit("dump_image: no PE signature at %#x" % (BASE + e_lfanew))
    count = struct.unpack_from("<H", coff, 6)[0]
    optsz = struct.unpack_from("<H", coff, 20)[0]
    oh = read_mem(mem, BASE + e_lfanew + 24, 32)
    size_of_image = struct.unpack_from("<I", oh, 28)[0] if oh and len(oh) == 32 else 0
    raw = read_mem(mem, BASE + e_lfanew + 24 + optsz, 40 * count)
    sections = []
    if raw is not None:
        for i in range(count):
            name, vsz, rva = struct.unpack_from("<8sII", raw, 40 * i)
            name = name.split(b"\0")[0].decode("ascii", "replace")
            if vsz == 0 and struct.unpack_from("<I", raw, 40 * i + 16)[0] == 0:
                break  # trailing garbage entries past the real table
            if vsz:
                sections.append((name, BASE + rva, vsz))
    if sections:
        end = max(va + vsz for _, va, vsz in sections)
        return (end + PAGE - 1) & ~(PAGE - 1), sections
    if size_of_image:
        return BASE + size_of_image, []
    raise SystemExit("dump_image: could not read the PE section table")


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("out", help="dump file to write")
    ap.add_argument("--force", action="store_true",
                    help="overwrite an existing dump file")
    ap.add_argument("--pid", type=int, default=None,
                    help="PES2021.exe pid (default: first pgrep match)")
    args = ap.parse_args()

    if os.path.exists(args.out) and not args.force:
        raise SystemExit("dump_image: %s exists (use --force to overwrite)"
                         % args.out)
    if os.path.exists(args.out + ".maps") and not args.force:
        raise SystemExit("dump_image: %s exists (use --force to overwrite)"
                         % (args.out + ".maps"))

    pid = args.pid or find_pid()
    maps = []
    for line in open("/proc/%d/maps" % pid):
        rng, perm = line.split()[:2]
        lo, hi = (int(x, 16) for x in rng.split("-"))
        if hi > BASE:
            maps.append((lo, hi, perm))
    if not maps:
        raise SystemExit("dump_image: no mappings at/above %#x for pid %d"
                         % (BASE, pid))

    with open("/proc/%d/mem" % pid, "rb", buffering=0) as mem:
        end, sections = image_end(mem)
        kept = [(max(lo, BASE), min(hi, end), perm)
                for lo, hi, perm in maps if lo < end]
        bad = []
        with open(args.out, "wb") as f:
            f.truncate(end - BASE)
            for lo, hi, perm in kept:
                a = lo
                while a < hi:
                    n = min(CHUNK, hi - a)
                    b = read_mem(mem, a, n)
                    if b is None:
                        bad.append((a, n))
                        b = b"\0" * n
                    f.seek(a - BASE)
                    f.write(b)
                    a += n

    with open(args.out + ".maps", "w") as m:
        m.write("pid %d\n" % pid)
        m.write("base %#x end %#x (from PE section table)\n" % (BASE, end))
        for name, va, vsz in sections:
            m.write("section %s %#x vsz %#x\n" % (name, va, vsz))
        for lo, hi, perm in kept:
            m.write("%#x-%#x %s\n" % (lo, hi, perm))
        for a, n in bad:
            m.write("UNREADABLE %#x+%#x\n" % (a, n))
    print("pid %d: %#x..%#x, %d maps, %d unreadable chunks -> %s"
          % (pid, BASE, end, len(kept), len(bad), args.out))


if __name__ == "__main__":
    main()
