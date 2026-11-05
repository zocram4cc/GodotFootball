#!/usr/bin/env python3
"""Keep the live PES rig's CPU cap matched to demand (owner, 05-11-26).

PES runs capped so gates and agents get the cores (pes_cap.sh: cgroup cpu.max, one pinned core, nice 19). At 10% of a core
a requested capture takes hours, so this raises the cap while an agent waits on one:

- `--wanted` lists hook names agents are waiting on, one per line. While any of them has fewer captures on disk than its
  cap (the watcher's --hook-max / --per-hook-max, counted over every launch like the watcher does), PES gets `--high`.
- Otherwise it gets `--low`.
- `--override` holds a percentage that wins over both, e.g. 100 during a restart so the bot's kick-off wait cannot time
  out on a starved game. Delete the file to return to demand.

Every pass also moves new PES / wineserver processes into the cgroup, so a restart needs no manual pes_cap.sh.
Run as root (cgroup writes): sudo -n python3 pes_capd.py --live2 ... --watch-script ... --wanted ...
"""
import argparse
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import rv_watch  # noqa: E402  (capture_counts: the watcher's own count)

CGROUP_PERIOD_US = 100000
PES_CORE = "15"  # the core pes_cap.sh pins the rig to
PES_PROCESS_PATTERNS = (("-f", "PES2021.exe"), ("-x", "wineserver"))


def watch_caps(script_text):
    """({hook: cap} from --hook-max, default cap from --per-hook-max) of a watcher launch script."""
    default = re.search(r"--per-hook-max[= ]+(\d+)", script_text)
    caps = {}
    for flag in re.findall(r"--hook-max[= ]+(\S+)", script_text):
        for item in flag.strip("'\"").split(","):
            name, _, n = item.partition("=")
            if n.isdigit():
                caps[name] = int(n)
    return caps, int(default.group(1)) if default else 0


def decide(wanted, caps, default_cap, counts, low, high, override=None):
    """The cap percentage: override if given, high while a wanted hook is below its cap, else low."""
    if override is not None:
        return override
    for hook in wanted:
        cap = caps.get(hook, default_cap)
        if cap == 0 or counts.get(hook, 0) < cap:
            return high
    return low


def read_lines(path):
    try:
        return [l.strip() for l in open(path) if l.strip() and not l.startswith("#")]
    except FileNotFoundError:
        return []


def read_override(path):
    lines = read_lines(path)
    return int(lines[0]) if lines and lines[0].isdigit() else None


def pes_pids():
    pids = []
    for flag, pat in PES_PROCESS_PATTERNS:
        out = subprocess.run(["pgrep", flag, pat], capture_output=True, text=True).stdout
        pids += [int(p) for p in out.split()]
    return pids


def adopt(cgroup, pids):
    """Move pids not yet in the cgroup into it, pinned to the rig core at nice 19. Returns the pids moved."""
    members = set(int(p) for p in read_lines(os.path.join(cgroup, "cgroup.procs")))
    moved = []
    for pid in pids:
        if pid in members:
            continue
        subprocess.run(["taskset", "-a", "-p", "-c", PES_CORE, str(pid)], capture_output=True)
        with open(os.path.join(cgroup, "cgroup.procs"), "w") as f:
            f.write(str(pid))
        for tid in os.listdir("/proc/%d/task" % pid):
            subprocess.run(["renice", "-n", "19", "-p", tid], capture_output=True)
        moved.append(pid)
    return moved


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--live2", required=True, help="the watcher's --out (launch folders pid*/seq*)")
    ap.add_argument("--watch-script", required=True, help="the watcher launch script holding --hook-max / --per-hook-max")
    ap.add_argument("--wanted", required=True, help="file: hook names agents are waiting on, one per line")
    ap.add_argument("--override", default="", help="file: a percentage that wins over demand (absent: demand)")
    ap.add_argument("--cgroup", default="/sys/fs/cgroup/pescap")
    ap.add_argument("--low", type=int, default=10, help="percent of one core when no capture is wanted")
    ap.add_argument("--high", type=int, default=50, help="percent of one core while a wanted hook is below its cap")
    ap.add_argument("--interval", type=float, default=60.0)
    a = ap.parse_args()
    os.makedirs(a.cgroup, exist_ok=True)
    current = None
    while True:
        caps, default_cap = watch_caps(open(a.watch_script).read())
        wanted = read_lines(a.wanted)
        pct = decide(wanted, caps, default_cap, rv_watch.capture_counts(a.live2), a.low, a.high,
                     read_override(a.override) if a.override else None)
        moved = adopt(a.cgroup, pes_pids())
        if pct != current:
            with open(os.path.join(a.cgroup, "cpu.max"), "w") as f:
                f.write("%d %d" % (pct * CGROUP_PERIOD_US // 100, CGROUP_PERIOD_US))
            print(time.strftime("%H:%M:%S"), "cap %d%% (wanted: %s)" % (pct, ",".join(wanted) or "none"), flush=True)
            current = pct
        if moved:
            print(time.strftime("%H:%M:%S"), "adopted", moved, flush=True)
        time.sleep(a.interval)


if __name__ == "__main__":
    main()
