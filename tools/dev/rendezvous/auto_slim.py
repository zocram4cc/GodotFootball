#!/usr/bin/env python3
"""auto_slim.py: keep the rendezvous capture disk under control while the watcher runs.

Every --interval seconds, slims (slim.py: read-set only, verified byte-identical replay, else left full) each full
capture in the newest launch folders that is older than --min-age and whose hook is not in --keep (hooks an agent
needs FULL memory of, beyond the recorded call). A capture slim refused is remembered in <live2>/auto_slim_refused.txt
and never retried. With --pack-after S, a capture still full S seconds after it landed (kept, or slim refused) is
packed with capz.py (mem.bin -> mem.bin.zst, ~3.7x smaller), so kept captures stop pushing the disk under the watcher's
floor; `capz.py unpack DIR` restores one before a tool reads it. Run with the harness Python and PES_EXE set, like slim.py.

Usage: auto_slim.py --live2 DIR --hooks HOOKS_CFG [--keep h1,h2] [--min-age 900] [--interval 600] [--launches 2]
                    [--pack-after SECONDS]
"""
import argparse
import glob
import json
import os
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import capz  # noqa: E402  (UNPACKED_MARKER)
FULL_BYTES = 200 << 20  # a slim capture keeps a few thousand pages (~30-40 MB); a full one is ~3.8 GB


def full(d):
    m = os.path.join(d, "mem.bin")
    return os.path.exists(m) and os.path.exists(os.path.join(d, "call.bin")) and os.stat(m).st_blocks * 512 >= FULL_BYTES


def hook_of(d):
    try:
        return json.load(open(os.path.join(d, "index.json")))["rv"]["hook"]
    except (OSError, ValueError, KeyError):
        return None


def candidates(live2, launches, keep, min_age, refused):
    """Full captures to slim: older than min_age, not refused before, hook not kept, and never packed (a capture with
    capz's unpacked marker was kept or cited, and an agent is reading it)."""
    folders = sorted(glob.glob(os.path.join(live2, "pid*")), key=os.path.getmtime)[-launches:]
    now = time.time()
    out = []
    for f in folders:
        for d in sorted(glob.glob(os.path.join(f, "seq*"))):
            if d in refused or now - os.path.getmtime(d) < min_age or not full(d):
                continue
            if os.path.exists(os.path.join(d, capz.UNPACKED_MARKER)):
                continue
            h = hook_of(d)
            if h and h not in keep:
                out.append(d)
    return out


def read_keep_files(paths):
    """Hook names listed in the keep files (one per line, # comments), re-read every pass so a hook an agent starts
    replaying is kept from the next pass on (wanted.txt, the protected list)."""
    hooks = set()
    for p in paths:
        try:
            hooks |= {l.strip() for l in open(p) if l.strip() and not l.startswith("#")}
        except FileNotFoundError:
            pass
    return hooks


def stale_full(live2, launches, pack_after, pin=frozenset()):
    """Captures still full pack_after seconds after landing, except those of pinned hooks (an agent is replaying them;
    packing removes the mem.bin under it)."""
    folders = sorted(glob.glob(os.path.join(live2, "pid*")), key=os.path.getmtime)[-launches:]
    now = time.time()
    return [d for f in folders for d in sorted(glob.glob(os.path.join(f, "seq*")))
            if full(d) and now - os.path.getmtime(d) >= pack_after and hook_of(d) not in pin]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--live2", required=True)
    ap.add_argument("--hooks", required=True, help="the hooks.cfg the captures were taken with")
    ap.add_argument("--keep", default="", help="comma-separated hooks never slimmed")
    ap.add_argument("--keep-file", action="append", default=[],
                    help="file of hooks never slimmed, one per line, re-read every pass (repeatable)")
    ap.add_argument("--min-age", type=float, default=900.0)
    ap.add_argument("--interval", type=float, default=600.0)
    ap.add_argument("--launches", type=int, default=2, help="how many newest launch folders to scan")
    ap.add_argument("--pack-after", type=float, default=0.0, help="pack captures still full this long after landing (0: never)")
    ap.add_argument("--pin", default="", help="comma-separated hooks never packed (an agent is replaying them now)")
    a = ap.parse_args()
    keep = {h for h in a.keep.split(",") if h}
    refused_path = os.path.join(a.live2, "auto_slim_refused.txt")
    while True:
        refused = set(open(refused_path).read().split()) if os.path.exists(refused_path) else set()
        todo = candidates(a.live2, a.launches, keep | read_keep_files(a.keep_file), a.min_age, refused)
        if todo:
            print(time.strftime("%H:%M:%S"), "slimming", len(todo), flush=True)
            subprocess.run([sys.executable, os.path.join(HERE, "slim.py"), "--min-age", "0", "--hooks", a.hooks] + todo)
            with open(refused_path, "a") as r:
                for d in todo:
                    if full(d):
                        r.write(d + "\n")
        if a.pack_after:
            stale = stale_full(a.live2, a.launches, a.pack_after, {h for h in a.pin.split(",") if h})
            if stale:
                print(time.strftime("%H:%M:%S"), "packing", len(stale), flush=True)
                subprocess.run([sys.executable, os.path.join(HERE, "capz.py"), "pack"] + stale)
        time.sleep(a.interval)


if __name__ == "__main__":
    main()
