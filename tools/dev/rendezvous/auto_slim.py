#!/usr/bin/env python3
"""auto_slim.py: keep the rendezvous capture disk under control while the watcher runs.

Every --interval seconds, slims (slim.py: read-set only, verified byte-identical replay, else left full) each full
capture in the newest launch folders that is older than --min-age and whose hook is not in --keep (hooks an agent
needs FULL memory of, beyond the recorded call). A capture slim refused is remembered in <live2>/auto_slim_refused.txt
and never retried. Run with the harness Python and PES_EXE set, like slim.py.

Usage: auto_slim.py --live2 DIR --hooks HOOKS_CFG [--keep h1,h2] [--min-age 900] [--interval 600] [--launches 2]
"""
import argparse
import glob
import json
import os
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
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
    folders = sorted(glob.glob(os.path.join(live2, "pid*")), key=os.path.getmtime)[-launches:]
    now = time.time()
    out = []
    for f in folders:
        for d in sorted(glob.glob(os.path.join(f, "seq*"))):
            if d in refused or now - os.path.getmtime(d) < min_age or not full(d):
                continue
            h = hook_of(d)
            if h and h not in keep:
                out.append(d)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--live2", required=True)
    ap.add_argument("--hooks", required=True, help="the hooks.cfg the captures were taken with")
    ap.add_argument("--keep", default="", help="comma-separated hooks never slimmed")
    ap.add_argument("--min-age", type=float, default=900.0)
    ap.add_argument("--interval", type=float, default=600.0)
    ap.add_argument("--launches", type=int, default=2, help="how many newest launch folders to scan")
    a = ap.parse_args()
    keep = {h for h in a.keep.split(",") if h}
    refused_path = os.path.join(a.live2, "auto_slim_refused.txt")
    while True:
        refused = set(open(refused_path).read().split()) if os.path.exists(refused_path) else set()
        todo = candidates(a.live2, a.launches, keep, a.min_age, refused)
        if todo:
            print(time.strftime("%H:%M:%S"), "slimming", len(todo), flush=True)
            subprocess.run([sys.executable, os.path.join(HERE, "slim.py"), "--min-age", "0", "--hooks", a.hooks] + todo)
            with open(refused_path, "a") as r:
                for d in todo:
                    if full(d):
                        r.write(d + "\n")
        time.sleep(a.interval)


if __name__ == "__main__":
    main()
