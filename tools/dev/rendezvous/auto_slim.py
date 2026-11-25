#!/usr/bin/env python3
"""auto_slim.py: keep the rendezvous capture disk under control while the watcher runs.

Every --interval seconds, footprint-slims (slim.py) each full capture in the newest launch folders that is older than
--min-age, unless it is kept: the first KEEP_FIRST_IN_PLAY in-play captures of each hook in its launch (in_play: the
watcher's own predicate, cached in index.json), a capture named in a keep file (one 'pidN/seqM' per line), or a hook
named in --keep or in a keep file (one hook name per line; keep files are re-read every pass). A capture slim refused is
remembered in <live2>/auto_slim_refused.txt and never retried. With --pack-after S, a capture still full S seconds after
it landed (kept, or slim refused) is packed with capz.py (mem.bin -> mem.bin.zst, ~3.7x smaller), so kept captures stop
pushing the disk under the watcher's floor; `capz.py unpack DIR` restores one before a tool reads it. slim.py needs the
archive root in $GF_CAPTURE_ARCHIVE.

Usage: auto_slim.py --live2 DIR --hooks HOOKS_CFG [--keep h1,h2] [--keep-file F ...] [--min-age 900] [--interval 600]
                    [--launches 2] [--pack-after SECONDS] [--pin h1,h2]
"""
import argparse
import collections
import glob
import json
import os
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "translate"))
import capz  # noqa: E402  (UNPACKED_MARKER)
import full_snapshot as fs  # noqa: E402
FULL_BYTES = 200 << 20  # a slim capture keeps a few thousand pages (~30-40 MB); a full one is ~3.8 GB
KEEP_FIRST_IN_PLAY = 2  # the first in-play captures of each hook in a launch stay full: the proofs replay them
GAME_GLOBAL, MATCH_OFF, PLAY_OFF = 0x1436F3DC8, 0x270, 0x116C  # the watcher's in-play predicate, d(q(q(G)+0x270)+0x116c)==1


def full(d):
    m = os.path.join(d, "mem.bin")
    return os.path.exists(m) and os.path.exists(os.path.join(d, "call.bin")) and os.stat(m).st_blocks * 512 >= FULL_BYTES


def hook_of(d):
    try:
        return json.load(open(os.path.join(d, "index.json")))["rv"]["hook"]
    except (OSError, ValueError, KeyError):
        return None


def seq_key(d):
    return int(os.path.basename(d)[3:])  # seqNNN: the watcher numbers captures in landing order


def cap_name(d):
    return os.path.join(os.path.basename(os.path.dirname(d)), os.path.basename(d))  # 'pidN/seqM'


def in_play(d):
    """The watcher's in-play predicate on a full capture's memory: the match is in open play (MI = *(G+0x270) is set and
    MI+0x116C == 1, the test full_snapshot waits for before it pauses the game)."""
    sm = fs.SnapMem(d)
    g = sm.u64(GAME_GLOBAL)
    mi = sm.u64(g + MATCH_OFF) if g else 0
    return bool(mi) and sm.i32(mi + PLAY_OFF) == 1


def write_json(path, obj):
    tmp = path + ".part"
    with open(tmp, "w") as f:
        json.dump(obj, f)
    os.replace(tmp, path)


def cached_in_play(d):
    """in_play(d), cached in the capture's index.json as "in_play". None while the capture is not full (packed, slim, or
    unreadable): its state is unknown, and an unknown capture never counts as in play."""
    path = os.path.join(d, "index.json")
    try:
        with open(path) as f:
            idx = json.load(f)
        if "in_play" not in idx:
            if not full(d):
                return None
            idx["in_play"] = in_play(d)
            write_json(path, idx)
        return idx["in_play"]
    except (OSError, ValueError, SystemExit):
        return None


def first_in_play(folder, inplay):
    """The first KEEP_FIRST_IN_PLAY in-play captures of each hook in one launch folder, in landing order: kept full."""
    seen, out = collections.Counter(), set()
    for d in sorted(glob.glob(os.path.join(folder, "seq*")), key=seq_key):
        h = hook_of(d)
        if h and seen[h] < KEEP_FIRST_IN_PLAY and inplay(d):
            seen[h] += 1
            out.add(d)
    return out


def candidates(live2, launches, keep, min_age, refused, inplay=lambda d: False):
    """Full captures to slim: older than min_age, not refused before, not kept (its hook, its own cap_name, or among the
    first in-play captures of its hook in its launch), and never packed (a capture with capz's unpacked marker was kept or
    cited, and an agent is reading it)."""
    folders = sorted(glob.glob(os.path.join(live2, "pid*")), key=os.path.getmtime)[-launches:]
    now = time.time()
    out = []
    for f in folders:
        protected = first_in_play(f, inplay)
        for d in sorted(glob.glob(os.path.join(f, "seq*")), key=seq_key):
            if d in refused or d in protected or now - os.path.getmtime(d) < min_age or not full(d):
                continue
            if os.path.exists(os.path.join(d, capz.UNPACKED_MARKER)):
                continue
            h = hook_of(d)
            if h and h not in keep and cap_name(d) not in keep:
                out.append(d)
    return out


def read_keep_files(paths):
    """The names listed in the keep files (one per line, # comments), re-read every pass so a hook or a capture an agent
    starts replaying is kept from the next pass on: a hook name (wanted.txt) or a 'pidN/seqM' capture path."""
    names = set()
    for p in paths:
        try:
            names |= {l.strip() for l in open(p) if l.strip() and not l.startswith("#")}
        except FileNotFoundError:
            pass
    return names


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
                    help="file of hooks or 'pidN/seqM' captures never slimmed, one per line, re-read every pass (repeatable)")
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
        todo = candidates(a.live2, a.launches, keep | read_keep_files(a.keep_file), a.min_age, refused,
                          inplay=cached_in_play)
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
