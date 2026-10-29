#!/usr/bin/env python3
"""Rendezvous watcher for the live PES. Run under sudo (the process mem file is mode 600).

Waits for PES2021.exe, then for each capture_<seq>.txt the DLL writes in the game's rendezvous/run dir: one
full snapshot with the captured call (full_snapshot_rv.rv_take: STOP, read, CONT, release), every pause logged.
Every file it writes (snapshots, logs, the release files in the game dir) is chowned to the owner of the run dir,
so nothing root-owned is left. Stops after --max snapshots or when the disk falls under --min-free-gib.
"""
import argparse, glob, json, os, shutil, signal, subprocess, sys, time

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
    ap.add_argument("--per-hook-max", type=int, default=0,
                    help="snapshots per hook per game launch; past it calls are released unsnapshotted (0: no cap)")
    ap.add_argument("--only", default="",
                    help="comma-separated hook names to snapshot; calls of every other hook are released unsnapshotted, "
                         "so rare hooks an agent waits on are not starved by frequent ones filling the disk floor")
    ap.add_argument("--hook-max", default="",
                    help="comma-separated NAME=N per-hook overrides of --per-hook-max (an agent hunting a rare branch "
                         "needs more calls of one hook without raising every hook's cap)")
    ap.add_argument("--arm", default="",
                    help="comma-separated NAME=TRIGGER: calls of NAME are released unsnapshotted until a call of hook "
                         "TRIGGER has been seen in this launch (e.g. cutscene hooks armed by the goal pick, so the "
                         "entrance's varying number of calls cannot use up their cap)")
    a = ap.parse_args()
    only = {h for h in a.only.split(",") if h}
    hook_max = {k: int(v) for k, v in (kv.split("=") for kv in a.hook_max.split(",") if kv)}
    arm = dict(kv.split("=") for kv in a.arm.split(",") if kv)
    st = os.stat(a.run)
    uid, gid = st.st_uid, st.st_gid
    os.makedirs(a.out, exist_ok=True)
    os.chown(a.out, uid, gid)
    log = open(os.path.join(a.out, "watch.log"), "a", buffering=1)
    pauses = open(os.path.join(a.out, "pauses.jsonl"), "a", buffering=1)
    for f in (log, pauses):
        os.chown(f.name, uid, gid)
    say = lambda m: log.write("%s %s\n" % (time.strftime("%H:%M:%S"), m))
    # The DLL numbers captures from 1 on every game start and proceeds as soon as release_<seq> exists, so a previous
    # run's files would release new calls unsnapshotted: move them aside whenever a new PES process appears.
    def clear_run():
        stale = [f for p in ("capture_*.txt", "release_*", "done_*") for f in glob.glob(os.path.join(a.run, p))]
        if stale:
            dest = os.path.join(a.out, time.strftime("stale_run_%Y%m%d_%H%M%S"))
            os.makedirs(dest, exist_ok=True)
            for f in stale:
                shutil.move(f, dest)
            chown_tree(dest, uid, gid)
            say("moved %d stale run files to %s" % (len(stale), dest))

    def release(cap, why):
        open(os.path.join(a.run, "release_%d" % cap["seq"]), "w").close()
        say("seq%d %s released without snapshot: %s" % (cap["seq"], cap["hook"], why))

    # Each launch gets its own folder: the DLL's seq numbers restart at 1, so a shared folder would overwrite the last
    # launch's captures (28-10-26: seq001-080 of one launch lost that way).
    def launch_counts(launch):
        counts = {}
        for idx in glob.glob(os.path.join(launch, "seq*", "index.json")):
            hook = json.load(open(idx)).get("rv", {}).get("hook")
            counts[hook] = counts.get(hook, 0) + 1
        return counts

    pid, seen, taken, launch, per_hook = None, set(), 0, None, {}
    # 60 s: a capture takes as long as it needs (owner, 27-10-26); the bot runs with RIGBOT_ALLOW_FROZEN_FRAMES so a
    # stopped game no longer fails its run. The cap only guards a hung read: past it the game is released at once.
    args = argparse.Namespace(max_pause=60.0, threads=4, chunk_mib=64)
    say("watching %s (pause cap %.1f s)" % (a.run, args.max_pause))
    while taken < a.max:
        if pid is None or not os.path.exists("/proc/%d" % pid):
            pid = pes_pid()
            if pid is None:
                time.sleep(1)
                continue
            say("PES pid %d" % pid)
            # Only the watcher stops the game; a watcher killed mid-snapshot (no finally on SIGKILL) leaves it stopped
            # (28-10-26: frozen ~30 min). Resume it on attach.
            if "T (stopped)" in open("/proc/%d/status" % pid).read():
                os.kill(pid, signal.SIGCONT)
                say("PES pid %d was stopped (a previous watcher died mid-snapshot): resumed" % pid)
            clear_run()
            seen.clear()
            fired = set()
            launch = os.path.join(a.out, "pid%d" % pid)
            os.makedirs(launch, exist_ok=True)
            os.chown(launch, uid, gid)
            per_hook = launch_counts(launch)
        for f in sorted(glob.glob(os.path.join(a.run, "capture_*.txt"))):
            if f in seen:
                continue
            seen.add(f)
            cap = fs.rv_parse_capture(f)
            fired.add(cap["hook"])
            if only and cap["hook"] not in only:
                open(os.path.join(a.run, "release_%d" % cap["seq"]), "w").close()  # not logged: frequent by design
                continue
            if cap["hook"] in arm and arm[cap["hook"]] not in fired:
                open(os.path.join(a.run, "release_%d" % cap["seq"]), "w").close()  # not logged: unarmed by design
                continue
            if shutil.disk_usage(a.out).free < a.min_free_gib << 30:
                release(cap, "disk under %d GiB" % a.min_free_gib)
                continue
            cap_n = hook_max.get(cap["hook"], a.per_hook_max)
            if cap_n and per_hook.get(cap["hook"], 0) >= cap_n:
                release(cap, "per-hook cap %d reached" % cap_n)
                continue
            snap = fs.rv_take(pid, a.run, launch, cap, args, say, pauses)
            for p in glob.glob(os.path.join(a.run, "release_*")):
                os.chown(p, uid, gid)
            d = os.path.join(launch, "seq%03d" % cap["seq"])
            if os.path.isdir(d):
                chown_tree(d, uid, gid)
            if snap is not None:
                per_hook[cap["hook"]] = per_hook.get(cap["hook"], 0) + 1
            taken += snap is not None
            say("seq%d %s calls=%d -> %s" % (cap["seq"], cap["hook"], cap["calls"], "ok" if snap else "aborted"))
        time.sleep(0.0005)
    say("done: %d snapshots" % taken)


if __name__ == "__main__":
    main()
