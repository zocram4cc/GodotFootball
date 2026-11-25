#!/usr/bin/env python3
"""capz.py: compress cold full captures in place, and restore them on demand.

A full rendezvous capture's mem.bin is ~3.8 GB and compresses ~3.7x with zstd -3. Captures that must stay FULL (cited,
or slim refused) but that nothing reads right now are packed to mem.bin.zst; any tool that reads mem.bin (snapdiff,
slim, the oracle dirs that link to it) needs it unpacked first, which takes a few seconds. Slim captures (a sparse
mem.bin of a few tens of MB on disk) are never packed. A footprint slim capture's full copy is archived behind a link
(mem.bin.zst): unpack refuses it, and only `slim.py --restore` brings it back, as an explicit act. Unpack writes every
page (no sparse holes): a full capture's only holes must be its index holes (full_snapshot.SnapMem reads any other hole
as a page a slim capture dropped).

  capz.py pack   DIR...   mem.bin -> mem.bin.zst; the frame checksum is tested before mem.bin is removed
  capz.py unpack DIR...   mem.bin.zst -> mem.bin (every page allocated); refuses an archived slim capture (exit 1)
  capz.py status DIR...   print packed / full / slim per capture dir (an archived slim capture is slim)

DIR may be a capture dir (seqN) or a launch dir (pidN: every seqN under it).
"""
import fcntl
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


# Left in a capture dir by unpack, removed by pack: a capture that was ever packed must stay FULL (it was kept or cited),
# so auto_slim never slims one an agent has unpacked (05-11-26: two unpacked real trap states were slimmed mid-proof and
# could no longer be replayed).
UNPACKED_MARKER = ".unpacked"


def pack(d):
    raw, zst = os.path.join(d, "mem.bin"), os.path.join(d, "mem.bin.zst")
    if not os.path.exists(raw) or allocated(raw) < SLIM_BYTES:
        return "skip"
    tmp = zst + ".part"
    subprocess.run(["zstd", "-q", "-f", "-T0", "-3", raw, "-o", tmp], check=True)
    subprocess.run(["zstd", "-q", "-t", tmp], check=True)
    os.replace(tmp, zst)
    os.remove(raw)
    if os.path.exists(os.path.join(d, UNPACKED_MARKER)):
        os.remove(os.path.join(d, UNPACKED_MARKER))
    return "packed"


class Refused(Exception):
    """capz will not do this to a footprint slim capture: its full copy is restored only by an explicit act."""


SLIM_REFUSAL = ("slim capture: its full copy is archived; restore it only by an explicit act: "
                "python3 tools/dev/rendezvous/slim.py --restore %s")


def archived(d):
    """A footprint slim capture: mem.bin is the slim copy and mem.bin.zst is a link to the archived full copy."""
    return os.path.islink(os.path.join(d, "mem.bin.zst"))


def unpack(d):
    zst = os.path.join(d, "mem.bin.zst")
    if archived(d):
        raise Refused(SLIM_REFUSAL % d)
    if not os.path.exists(zst):
        return "skip"
    return _unpack(d)


def restore(d):
    """slim.py --restore only: the archived full copy becomes mem.bin; the link goes, the archived copy stays."""
    if not archived(d):
        raise Refused("nothing to restore: %s has no archived full copy" % d)
    return _unpack(d)


def _unpack(d):
    raw, zst = os.path.join(d, "mem.bin"), os.path.join(d, "mem.bin.zst")
    tmp = raw + ".part"
    subprocess.run(["zstd", "-q", "-f", "-d", "--no-sparse", zst, "-o", tmp], check=True)
    os.replace(tmp, raw)
    open(os.path.join(d, UNPACKED_MARKER), "w").close()
    os.remove(zst)  # a packed copy, or an archive link (the archived copy itself stays)
    return "unpacked"


def status(d):
    if archived(d):
        return "slim"
    if os.path.exists(os.path.join(d, "mem.bin.zst")):
        return "packed"
    raw = os.path.join(d, "mem.bin")
    return "full" if os.path.exists(raw) and allocated(raw) >= SLIM_BYTES else "slim"


# Held around every pack / unpack of one capture dir: auto_slim, Main and agents all run capz, and two packers on one dir
# shared the .part file and one crashed (05-11-26, pid281026/seq24384). The second waits, then finds nothing to do.
LOCK_NAME = ".capz.lock"


def locked(op, d):
    # Read-only open: a lock file that a root packer left (root:root 644) must still lock for a user.
    fd = os.open(os.path.join(d, LOCK_NAME), os.O_RDONLY | os.O_CREAT, 0o644)
    try:
        fcntl.flock(fd, fcntl.LOCK_EX)
        return op(d)
    finally:
        os.close(fd)


def main():
    if len(sys.argv) < 3 or sys.argv[1] not in ("pack", "unpack", "status"):
        sys.exit(__doc__)
    op = {"pack": pack, "unpack": unpack, "status": status}[sys.argv[1]]
    refused = False
    for d in captures(sys.argv[2:]):
        try:
            print(d, op(d) if op is status else locked(op, d), flush=True)
        except Refused as e:
            print(d, "refused:", e, file=sys.stderr, flush=True)
            refused = True
    sys.exit(1 if refused else 0)


if __name__ == "__main__":
    main()
