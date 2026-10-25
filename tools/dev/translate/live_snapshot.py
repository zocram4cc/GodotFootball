#!/usr/bin/env python3
"""Live full-process snapshot of a running PES 2021, taken read-only via
/proc/<pid>/mem (run as root; the process is owned by the user).

Reads every readable mapping (heap included, not just the image range) while
the match-info record MI = *(G+0x270) is non-null (a match step; the bot
churns matches continuously). Writes to <out>:
  maps.txt      - the /proc/<pid>/maps at capture
  live.json     - index: each mapping (VA, size, perms, file) + readable flag,
                  G/MI anchors, capture time
  <lo>-<hi>.bin - the raw bytes of each readable mapping
Nothing is written to the process; only pread. Never committed (PES-derived).
"""
import json
import os
import struct
import sys

G_PTR_VA = 0x1436F3DC8   # DAT_1436f3dc8: the global G pointer
MI_OFF = 0x270
MAX_MAP = (2 << 30)     # 2 GiB per mapping cap (the heap is ~400 MB)


def main():
    pid = int(sys.argv[1])
    out = sys.argv[2]
    os.makedirs(out, exist_ok=True)
    with open("/proc/%d/maps" % pid) as f:
        maps = f.readlines()
    open(os.path.join(out, "maps.txt"), "w").write("".join(maps))

    mem = open("/proc/%d/mem" % pid, "rb", buffering=0)
    index = []
    total = 0
    nbad = 0

    # Anchor first: G and MI must be non-null (a match is on the pitch).
    mem.seek(G_PTR_VA)
    g = struct.unpack("<Q", mem.read(8))[0]
    mem.seek(g + MI_OFF)
    mi = struct.unpack("<Q", mem.read(8))[0]
    if mi == 0:
        print("MI is null (no match step); retry later")
        return 2
    print("G=%#x MI=%#x" % (g, mi))

    for line in maps:
        f = line.split()
        rng, perms = f[0], f[1]
        lo, hi = [int(x, 16) for x in rng.split("-")]
        name = f[5] if len(f) > 5 else ""
        entry = {"va": lo, "hi": hi, "size": hi - lo, "perms": perms, "file": name}
        if "r" in perms and hi - lo <= MAX_MAP:
            try:
                mem.seek(lo)
                blob = mem.read(hi - lo)
                if len(blob) != hi - lo:
                    raise OSError("short read")
                fn = "%x-%x.bin" % (lo, hi)
                with open(os.path.join(out, fn), "wb") as w:
                    w.write(blob)
                entry["file_out"] = fn
                total += len(blob)
                index.append(entry)
                continue
            except Exception as e:
                entry["read_error"] = str(e)[:80]
                nbad += 1
                index.append(entry)
                continue
        else:
            entry["skipped"] = ("no read perm" if "r" not in perms
                                else "too large (%d)" % (hi - lo))
            index.append(entry)

    index.sort(key=lambda e: e["va"])
    meta = {"pid": pid, "g": g, "mi": mi, "total_bytes": total,
            "n_mappings": len(maps), "n_unreadable": nbad, "mappings": index}
    json.dump(meta, open(os.path.join(out, "live.json"), "w"))
    print("captured %d MB in %d mappings (%d failed)" % (total >> 20,
          sum(1 for e in index if "file_out" in e), nbad))
    return 0


if __name__ == "__main__":
    sys.exit(main())
