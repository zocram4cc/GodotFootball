#!/usr/bin/env python3
"""Rendezvous watcher for the live PES. Run under sudo (the process mem file is mode 600).

Waits for PES2021.exe, then for each capture_<seq>.txt the DLL writes in the game's rendezvous/run dir: one
full snapshot with the captured call (full_snapshot_rv.rv_take: STOP, read, CONT, release), every pause logged.
Every file it writes (snapshots, logs, the release files in the game dir) is chowned to the owner of the run dir,
so nothing root-owned is left. Stops after --max snapshots or when the disk falls under --min-free-gib.
"""
import argparse, glob, json, os, shutil, subprocess, sys, time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import full_snapshot_rv as fs  # noqa: E402


def chown_tree(path, uid, gid):
    for root, dirs, files in os.walk(path):
        for n in dirs + files:
            os.chown(os.path.join(root, n), uid, gid)
    os.chown(path, uid, gid)


def pes_pid():
    r = subprocess.run(["pgrep", "-x", "PES2021.exe"], capture_output=True, text=True)
    return int(r.stdout.split()[0]) if r.stdout.split() else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--run", required=True, help="the game's rendezvous\\run directory (where the DLL writes captures)")
    ap.add_argument("--out", required=True, help="where snapshots go (PES-derived: never commit)")
    ap.add_argument("--max", type=int, default=40)
    ap.add_argument("--min-free-gib", type=int, default=150)
    a = ap.parse_args()
    st = os.stat(a.run)
    uid, gid = st.st_uid, st.st_gid
    os.makedirs(a.out, exist_ok=True)
    os.chown(a.out, uid, gid)
    log = open(os.path.join(a.out, "watch.log"), "a", buffering=1)
    pauses = open(os.path.join(a.out, "pauses.jsonl"), "a", buffering=1)
    for f in (log, pauses):
        os.chown(f.name, uid, gid)
    say = lambda m: log.write("%s %s\n" % (time.strftime("%H:%M:%S"), m))
    pid, seen, taken = None, set(glob.glob(os.path.join(a.run, "capture_*.txt"))), 0
    # 60 s: a capture takes as long as it needs (owner, 27-10-26); the bot runs with RIGBOT_ALLOW_FROZEN_FRAMES so a
    # stopped game no longer fails its run. The cap only guards a hung read: past it the game is released at once.
    args = argparse.Namespace(max_pause=60.0, threads=4, chunk_mib=64)
    say("watching %s (ignoring %d old captures, pause cap %.1f s)" % (a.run, len(seen), args.max_pause))
    while taken < a.max:
        if pid is None or not os.path.exists("/proc/%d" % pid):
            pid = pes_pid()
            if pid is None:
                time.sleep(1)
                continue
            say("PES pid %d" % pid)
        for f in sorted(glob.glob(os.path.join(a.run, "capture_*.txt"))):
            if f in seen:
                continue
            seen.add(f)
            cap = fs.rv_parse_capture(f)
            if shutil.disk_usage(a.out).free < a.min_free_gib << 30:
                open(os.path.join(a.run, "release_%d" % cap["seq"]), "w").close()
                say("seq%d %s released without snapshot: disk under %d GiB" % (cap["seq"], cap["hook"], a.min_free_gib))
                continue
            snap = fs.rv_take(pid, a.run, a.out, cap, args, say, pauses)
            for p in glob.glob(os.path.join(a.run, "release_*")):
                os.chown(p, uid, gid)
            d = os.path.join(a.out, "seq%03d" % cap["seq"])
            if os.path.isdir(d):
                chown_tree(d, uid, gid)
            taken += snap is not None
            say("seq%d %s calls=%d -> %s" % (cap["seq"], cap["hook"], cap["calls"], "ok" if snap else "aborted"))
        time.sleep(0.0005)
    say("done: %d snapshots" % taken)


if __name__ == "__main__":
    main()
