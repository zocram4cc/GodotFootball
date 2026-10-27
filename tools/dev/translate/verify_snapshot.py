#!/usr/bin/env python3
"""Check the snapshot premise for the deferred read (full_snapshot.py step 4).

Private read-only mappings (code and rodata) are read after CONT. That gives the same bytes as a
read inside the pause only if the process does not change them. This compares every deferred
mapping across the given snapshots (identical bytes expected), and the game image's executable
mappings against the unpacked image (pes_full.bin, offset = VA - 0x140000000). Writable mappings
are counted too: the snapshots are expected to differ there, where state moves.

Usage: python3 verify_snapshot.py IMAGE SNAP_DIR [SNAP_DIR ...]
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import full_snapshot as fs  # noqa: E402

CHUNK = 16 << 20
IMG_BASE = 0x140000000


def differs(sa, sb, va, size):
    """True when size bytes at va differ between the two snapshots; None if unreadable."""
    off = 0
    while off < size:
        n = min(CHUNK, size - off)
        a, b = sa.read(va + off, n), sb.read(va + off, n)
        if a is None or b is None:
            return None
        if a != b:
            return True
        off += n
    return False


def main():
    image_path, snaps = sys.argv[1], sys.argv[2:]
    sms = [fs.SnapMem(s) for s in snaps]
    base = sms[0]
    img_size = os.path.getsize(image_path)
    tally = {"ro_same": 0, "ro_diff": [], "rw_same": 0, "rw_diff": 0, "unreadable": 0,
             "code_match": 0, "code_mismatch": []}
    for m in base.maps:
        if m["name"].startswith("/dev/"):
            continue
        twins = [sm.mapping_of(m["va"]) for sm in sms[1:]]
        if any(t is None or t["va"] != m["va"] or t["size"] != m["size"] for t in twins):
            tally["unreadable"] += 1
            continue
        d = [differs(base, sm, m["va"], m["size"]) for sm in sms[1:]]
        if None in d:
            tally["unreadable"] += 1
            continue
        changed = any(d)
        if "w" in m["perm"]:
            tally["rw_diff" if changed else "rw_same"] += 1 if not changed else 0
            tally["rw_diff"] += 1 if changed else 0
        elif changed:
            tally["ro_diff"].append("%s %#x+%#x" % (m["name"] or "(anon)", m["va"], m["size"]))
        else:
            tally["ro_same"] += 1
        if "x" in m["perm"] and IMG_BASE <= m["va"] < IMG_BASE + img_size:
            hi = min(m["va"] + m["size"], IMG_BASE + img_size)
            with open(image_path, "rb") as f:
                f.seek(m["va"] - IMG_BASE)
                file_bytes = f.read(hi - m["va"])
            snap_bytes = base.read(m["va"], hi - m["va"])
            if snap_bytes == file_bytes:
                tally["code_match"] += 1
            else:
                tally["code_mismatch"].append("%#x+%#x" % (m["va"], hi - m["va"]))
    print("snapshots=%d" % len(snaps))
    print("read-only private mappings: identical across snapshots %d; differing %d %s"
          % (tally["ro_same"], len(tally["ro_diff"]), tally["ro_diff"]))
    print("writable mappings: identical %d; differing (state moved) %d" % (tally["rw_same"], tally["rw_diff"]))
    print("executable image mappings equal to %s: %d; differing %d %s"
          % (os.path.basename(image_path), tally["code_match"], len(tally["code_mismatch"]),
             tally["code_mismatch"][:5]))
    print("mappings skipped (unreadable or not in every snapshot): %d" % tally["unreadable"])


if __name__ == "__main__":
    main()
