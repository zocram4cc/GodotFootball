#!/usr/bin/env python3
"""PES-vs-GF parity: plays GF's headless match at fixed seeds and measures the metric set in
docs/plans/pes-parity-metrics.md (M1-M14).

Each seed runs `gfsim_run --pes-loop --parity-log`. The counts it prints are scaled to 20 in-play
minutes (the match clock's own ticks), as the doc's rate unit says. The sim is deterministic, so a
landing's row can be measured again from its SHA at any time.

  GF_SIM_LOCK=/path/to/gf-sim.lock tools/dev/parity.py --pes21-root DIR [--record SHA]
  tools/dev/parity.py --logs DIR [--record SHA]     # measure logs from an earlier run

The PES baseline is read from the doc's "PES baseline" table: one row per metric, `|M3|142 (n=9)|`,
or per state for M1 and M2, `|M1:8|4.2|`. Distances appear once a row is filled.
"""
import argparse
import fcntl
import os
import re
import statistics
import subprocess
import sys
from collections import namedtuple

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DOC = os.path.join(REPO, "docs", "plans", "pes-parity-metrics.md")
SEEDS = (7, 11, 23)
PES_TICK_HZ = 54  # logic frames per second: the install's game speed (GF's pes::kPesTickHz)
IN_PLAY_MINUTES = 20  # the doc's rate unit
SECONDS_PER_MINUTE = 60
RATE_TICKS = IN_PLAY_MINUTES * SECONDS_PER_MINUTE * PES_TICK_HZ
VM_LIMIT_BYTES = 2000000 * 1024  # the shared machine's `ulimit -v 2000000` for every sim
RESTART_METRIC = {"kickoff": "M3", "throwin": "M4", "goalkick": "M5", "corner": "M6", "freekick": "M7"}
ACTION_METRIC = {"passes": "M10", "crosses": "M11", "shots": "M12", "tackles": "M13", "slides": "M14"}
METRIC_IDS = ["M%d" % i for i in range(1, 15)]
COLUMNS = [("M1",), ("M2",), ("M3", "M4", "M5", "M6", "M7"), ("M8",), ("M9",), ("M10", "M11", "M12", "M13", "M14")]
NAME = {
    "M3": "whistle-to-release, kickoff", "M4": "whistle-to-release, throw-in",
    "M5": "whistle-to-release, goal kick", "M6": "whistle-to-release, corner",
    "M7": "whistle-to-release, free kick", "M8": "stoppages", "M9": "longest stoppage",
    "M10": "passes", "M11": "crosses", "M12": "shots", "M13": "tackles (standing)", "M14": "slides",
}
UNIT = {"M9": "s", "M3": "frames, median", "M4": "frames, median", "M5": "frames, median",
        "M6": "frames, median", "M7": "frames, median"}

Run = namedtuple("Run", "determinism stoppages longest_s ticks live state stop restarts actions")
Summary = namedtuple("Summary", "mean lo hi seeds")


def _pairs(text):
    return {int(k): int(v) for k, v in re.findall(r"(\d+):(\d+)", text)}


def parse_run(text):
    """Reads one gfsim_run output. Raises ValueError when the parity lines are missing."""
    det = re.search(r"^\[determinism\] ([0-9a-f]+)$", text, re.M)
    rest = re.search(r"^\[restarts\] (\d+) stoppages, longest ([\d.]+) s", text, re.M)
    state = re.search(r"^\[parity-state\] ticks (\d+) live (\d+) \| (.*)$", text, re.M)
    stop = re.search(r"^\[parity-stop\] (.*)$", text, re.M)
    actions = re.search(r"^\[parity-actions\] (.*)$", text, re.M)
    if not (det and rest and state and stop and actions):
        raise ValueError("gfsim output lacks the parity lines: run gfsim_run with --parity-log")
    live = int(state.group(2))
    if live <= 0:
        raise ValueError("no in-play ticks in this run")
    restarts = [(kind, int(frames)) for kind, frames in re.findall(
        r"^\[parity-restart\] (\w+) whistle=\d+ release=\d+ frames=(\d+)$", text, re.M)]
    acts = {name: (int(a), int(b)) for name, a, b in re.findall(
        r"(passes|crosses|shots|tackles|slides) (\d+)-(\d+)", actions.group(1))}
    return Run(det.group(1), int(rest.group(1)), float(rest.group(2)), int(state.group(1)), live,
               _pairs(state.group(3)), _pairs(stop.group(1)), restarts, acts)


def _rate(count, live):
    return count * RATE_TICKS / live


def seed_values(run):
    """One seed's metric values. Keys: M1:<state> share %, M2:<state> per 20 min, M8, M9,
    M3..M7 median frames with n:M3..n:M7 counts, M10..M14 per 20 min."""
    v = {}
    total = sum(run.state.values())
    for s, n in run.state.items():
        v["M1:%d" % s] = 100.0 * n / total if total else 0.0
    for s, n in run.stop.items():
        v["M2:%d" % s] = _rate(n, run.live)
    # [restarts] counts the opening kickoff; PES's stoppage count starts at the first whistle, so drop it.
    v["M8"] = _rate(max(run.stoppages - 1, 0), run.live)
    v["M9"] = run.longest_s
    by_kind = {}
    for kind, frames in run.restarts:
        if kind in RESTART_METRIC:
            by_kind.setdefault(RESTART_METRIC[kind], []).append(frames)
    for metric, frames in by_kind.items():
        v[metric] = float(statistics.median(frames))
        v["n:" + metric] = len(frames)
    for name, (a, b) in run.actions.items():
        v[ACTION_METRIC[name]] = _rate(a + b, run.live)
    return v


def summarise(per_seed):
    out = {}
    for key in {k for values in per_seed for k in values}:
        vals = [values[key] for values in per_seed if key in values]
        out[key] = Summary(sum(vals) / len(vals), min(vals), max(vals), len(vals))
    return out


def pes_baseline(doc):
    """The PES values in the doc's baseline table; rows without a number (pending) are skipped."""
    out = {}
    for line in doc.splitlines():
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if len(cells) < 2 or not re.fullmatch(r"M\d+(:\d+)?", cells[0]):
            continue
        num = re.search(r"-?\d+(?:\.\d+)?", cells[1])
        if num:
            out[cells[0]] = float(num.group(0))
    return out


def distances(gf, pes):
    """|GF - PES| / PES per key; keys PES reports as zero have no relative distance."""
    return {k: abs(gf[k] - v) / v for k, v in pes.items() if k in gf and v != 0}


def metric_distances(dists):
    groups = {}
    for key, d in dists.items():
        groups.setdefault("M" + re.match(r"M(\d+)", key).group(1), []).append(d)
    return {m: sum(v) / len(v) for m, v in groups.items()}


def mean_distance(dists):
    md = metric_distances(dists)
    return sum(md.values()) / len(md) if md else None


def _cell(md, ids):
    vals = [md[i] for i in ids if i in md]
    return "pending" if not vals else "%.1f%%" % (100.0 * sum(vals) / len(vals))


def record_row(doc, sha, date, dists):
    """Puts this landing's row in the per-landing table, replacing an earlier row for the same SHA."""
    md = metric_distances(dists)
    cells = [_cell(md, ids) for ids in COLUMNS] + [_cell(md, METRIC_IDS)]
    row = "|%s|%s|%s|" % (sha, date, "|".join(cells))
    lines = doc.splitlines()
    head = next(i for i, line in enumerate(lines) if line.startswith("|landing (main SHA)|"))
    end = head + 2
    while end < len(lines) and lines[end].startswith("|"):
        end += 1
    body = [line for line in lines[head + 2:end] if not line.startswith("|%s|" % sha)]
    lines[head + 2:end] = body + [row]
    return "\n".join(lines) + "\n"


def _sort_key(key):
    m = re.fullmatch(r"(?:n:)?M(\d+)(?::(\d+))?", key)
    return (int(m.group(1)), int(m.group(2) or 0))


def _label(key):
    if key.startswith("M1:"):
        return "M1 MOVESTEP state %s" % key[3:], "% of player-ticks"
    if key.startswith("M2:"):
        return "M2 STOP entries from state %s" % key[3:], "count per 20 in-play min"
    metric = key.split(":")[-1]
    unit = UNIT.get(metric, "count per 20 in-play min")
    return "%s %s" % (metric, NAME[metric]), unit


def print_report(seeds, runs, summary, pes, dists):
    print("seeds %s | determinism %s" % (",".join(map(str, seeds)),
                                         " ".join(runs[s].determinism for s in seeds)))
    print("| metric | GF mean | min..max | %sunit |" % ("PES | dist | " if pes else ""))
    print("|---|---|---|%s---|" % ("---|---|" if pes else ""))
    for key in sorted((k for k in summary if not k.startswith("n:")), key=_sort_key):
        s = summary[key]
        if key.startswith("M1:") and s.mean == 0:
            continue
        label, unit = _label(key)
        if key in RESTART_METRIC.values():
            n = summary["n:" + key]
            label += " (n %d..%d)" % (n.lo, n.hi)
        extra = ""
        if pes:
            p = pes.get(key)
            extra = "%s | %s | " % ("-" if p is None else "%.2f" % p,
                                    "-" if key not in dists else "%.1f%%" % (100 * dists[key]))
        print("| %s | %.2f | %.2f..%.2f | %s%s |" % (label, s.mean, s.lo, s.hi, extra, unit))
    if pes:
        mean = mean_distance(dists)
        print("mean distance over metrics with a PES value: %s" % ("-" if mean is None else "%.1f%%" % (100 * mean)))


def _run_seeds(args, seeds):
    lock = os.environ.get("GF_SIM_LOCK")
    if not lock:
        sys.exit("set GF_SIM_LOCK to the shared sim lock file")
    outdir = args.scratch
    os.makedirs(outdir, exist_ok=True)

    def limit():
        import resource
        resource.setrlimit(resource.RLIMIT_AS, (VM_LIMIT_BYTES, VM_LIMIT_BYTES))

    files, procs = {}, {}
    with open(lock, "a") as fh:
        fcntl.flock(fh, fcntl.LOCK_EX)
        for seed in seeds:
            files[seed] = open(os.path.join(outdir, "seed%d.out" % seed), "w")
            cmd = ["nice", "-n", "19", "ionice", "-c3", args.binary, "--db-sql", args.db_sql,
                   "--home", "1", "--away", "2", "--minutes", str(args.minutes), "--seed", str(seed),
                   "--pes-loop", "--parity-log", "--pes21-root", args.pes21_root]
            procs[seed] = subprocess.Popen(cmd, cwd=REPO, stdout=files[seed], stderr=subprocess.STDOUT,
                                           preexec_fn=limit)
        codes = {seed: p.wait() for seed, p in procs.items()}
    for f in files.values():
        f.close()
    failed = [s for s, c in codes.items() if c != 0]
    if failed:
        sys.exit("gfsim_run failed for seeds %s: see %s" % (failed, outdir))
    return {seed: os.path.join(outdir, "seed%d.out" % seed) for seed in seeds}


def main(argv=None):
    p = argparse.ArgumentParser(description="Measure the PES-vs-GF parity metrics (M1-M14).")
    p.add_argument("--pes21-root", default=os.environ.get("PES21_ROOT"),
                   help="PES 2021 install (default: $PES21_ROOT)")
    p.add_argument("--seeds", default=",".join(map(str, SEEDS)))
    p.add_argument("--minutes", type=int, default=20)
    p.add_argument("--binary", default=os.path.join(REPO, "build", "apps", "gfsim_run"))
    p.add_argument("--db-sql", default=os.path.join(REPO, "assets", "fixtures", "teams.sql"))
    p.add_argument("--scratch", default=os.path.join(REPO, "build", "parity"))
    p.add_argument("--logs", help="measure seed<N>.out logs from this directory instead of running")
    p.add_argument("--doc", default=DOC)
    p.add_argument("--record", metavar="SHA", help="write this landing's row into the doc")
    args = p.parse_args(argv)
    seeds = [int(s) for s in args.seeds.split(",")]
    if args.logs:
        logs = {s: os.path.join(args.logs, "seed%d.out" % s) for s in seeds}
    else:
        if not args.pes21_root:
            p.error("need --pes21-root or PES21_ROOT")
        logs = _run_seeds(args, seeds)
    runs = {}
    for s in seeds:
        with open(logs[s]) as fh:
            runs[s] = parse_run(fh.read())
    summary = summarise([seed_values(runs[s]) for s in seeds])
    with open(args.doc) as fh:
        doc = fh.read()
    pes = pes_baseline(doc)
    dists = distances({k: s.mean for k, s in summary.items()}, pes)
    print_report(seeds, runs, summary, pes, dists)
    if args.record:
        date = subprocess.run(["git", "-C", REPO, "show", "-s", "--format=%cs", args.record],
                              capture_output=True, text=True, check=True).stdout.strip()
        with open(args.doc, "w") as fh:
            fh.write(record_row(doc, args.record, date, dists))
        print("recorded %s in %s" % (args.record, args.doc))
    return 0


if __name__ == "__main__":
    sys.exit(main())
