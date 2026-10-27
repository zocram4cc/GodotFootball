#!/usr/bin/env python3
"""traced_cov.py: own-block coverage of the impure functions from traced real calls.

Each --run is a frame_trace output dir: record.json (impure_calls) and executed/<VA>.json (the
function's own live block leaders hit over every traced call of that run). Coverage is the
driver's block_coverage over the union of those leaders across the given runs, so a block counts
when a real call of the function executed it in any run.

Usage: traced_cov.py --image IMG --spec-dir DIR --run RUN [--run RUN ...] --out FILE
Prints per-run reach, the union floor count (own >= 0.50) and the median over all specs.
"""
import argparse
import json
import os
import statistics
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import driver as drv  # noqa: E402

FLOOR = 0.50


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--image", required=True)
    ap.add_argument("--spec-dir", required=True)
    ap.add_argument("--run", action="append", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    tags = sorted(f[:-5] for f in os.listdir(a.spec_dir) if f.endswith(".json"))
    runs = {}
    for r in a.run:
        name = os.path.basename(r.rstrip("/"))
        runs[name] = json.load(open(os.path.join(r, "record.json")))
    rows = {}
    for tag in tags:
        va = "0x" + tag
        per_run, union = {}, set()
        for name, rec in runs.items():
            hits = []
            p = os.path.join(a.run[list(runs).index(name)], "executed", tag + ".json")
            if os.path.exists(p):
                hits = json.load(open(p))
            union |= set(hits)
            calls = rec["impure_calls"].get(tag, 0)
            per_run[name] = {"calls": calls, "hits": hits}
        cov = drv.block_coverage(a.image, [va], sorted(union))[va]
        for name, d in per_run.items():
            pc = drv.block_coverage(a.image, [va], d["hits"])[va] if d["hits"] else (0, cov[1] if cov else 0)
            d["reached"], d["total"] = pc
            d["fraction"] = round(pc[0] / pc[1], 4) if pc and pc[1] else 0.0
            del d["hits"]
        rows[tag] = {
            "va": va,
            "calls": sum(d["calls"] for d in per_run.values()),
            "reached": sum(d["calls"] for d in per_run.values()) > 0,
            "own": {"reached": cov[0], "total": cov[1],
                    "fraction": round(cov[0] / cov[1], 4) if cov and cov[1] else None},
            "per_run": per_run,
        }
    fracs = [row["own"]["fraction"] or 0.0 for row in rows.values()]
    reached = [t for t, row in rows.items() if row["reached"]]
    summary = {
        "specs": len(tags),
        "reached": len(reached),
        "floor_union": sum(1 for t in reached if (rows[t]["own"]["fraction"] or 0) >= FLOOR),
        "median_all_specs": round(statistics.median(fracs), 4),
        "median_reached": round(statistics.median([rows[t]["own"]["fraction"] or 0 for t in reached]), 4)
        if reached else None,
        "runs": list(runs),
    }
    json.dump({"summary": summary, "specs": rows}, open(a.out, "w"), indent=1, sort_keys=True)
    print(json.dumps(summary, indent=1))
    for t in reached:
        row = rows[t]
        print(t, "calls", row["calls"], "union own %s (%d/%d)" % (
            row["own"]["fraction"], row["own"]["reached"], row["own"]["total"]),
            " ".join("%s=%s" % (n, d["fraction"]) for n, d in row["per_run"].items()))


if __name__ == "__main__":
    sys.exit(main())
