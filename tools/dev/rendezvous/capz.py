#!/usr/bin/env python3
"""capz.py: compress cold full captures in place, and restore them on demand.

A full rendezvous capture's mem.bin is ~3.8 GB and compresses ~3.7x with zstd -3. Captures that must stay FULL (cited,
or slim refused) but that nothing reads right now are packed to mem.bin.zst; any tool that reads mem.bin (snapdiff,
slim, the oracle dirs that link to it) needs it unpacked first, which takes a few seconds. Slim captures (a sparse
mem.bin of a few tens of MB on disk) are never packed. Unpack writes every page (no sparse holes): a full capture's
only holes must be its index holes (full_snapshot.SnapMem reads any other hole as a page a slim capture dropped).

  capz.py pack   DIR...   mem.bin -> mem.bin.zst; the frame checksum is tested before mem.bin is removed
  capz.py unpack DIR...   mem.bin.zst -> mem.bin (every page allocated)
  capz.py status DIR...   print packed / full / slim per capture dir

DIR may be a capture dir (seqN) or a launch dir (pidN: every seqN under it).
"""
import glob
import os
import subprocess
import sys

SLIM_BYTES = 200 << 20  # auto_slim.FULL_BYTES, on allocated blocks: a slim mem.bin is sparse, its apparent size is full


def allocated(path):
    return os.stat(path).st_blocks * 512


def captures(paths):
    for p in paths:
        if os.path.exists(os.path.join(p, "index.json")):
            yield p
        else:
            yield from sorted(glob.glob(os.path.join(p, "seq*")))


def pack(d):
    raw, zst = os.path.join(d, "mem.bin"), os.path.join(d, "mem.bin.zst")
    if not os.path.exists(raw) or allocated(raw) < SLIM_BYTES:
        return "skip"
    tmp = zst + ".part"
    subprocess.run(["zstd", "-q", "-f", "-T0", "-3", raw, "-o", tmp], check=True)
    subprocess.run(["zstd", "-q", "-t", tmp], check=True)
    os.replace(tmp, zst)
    os.remove(raw)
    return "packed"


def unpack(d):
    raw, zst = os.path.join(d, "mem.bin"), os.path.join(d, "mem.bin.zst")
    if not os.path.exists(zst):
        return "skip"
    tmp = raw + ".part"
    subprocess.run(["zstd", "-q", "-f", "-d", "--no-sparse", zst, "-o", tmp], check=True)
    os.replace(tmp, raw)
    os.remove(zst)
    return "unpacked"


def status(d):
    if os.path.exists(os.path.join(d, "mem.bin.zst")):
        return "packed"
    raw = os.path.join(d, "mem.bin")
    return "full" if os.path.exists(raw) and allocated(raw) >= SLIM_BYTES else "slim"


def main():
    if len(sys.argv) < 3 or sys.argv[1] not in ("pack", "unpack", "status"):
        sys.exit(__doc__)
    op = {"pack": pack, "unpack": unpack, "status": status}[sys.argv[1]]
    for d in captures(sys.argv[2:]):
        print(d, op(d), flush=True)


if __name__ == "__main__":
    main()
