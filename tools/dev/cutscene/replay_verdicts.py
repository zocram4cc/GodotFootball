#!/usr/bin/env python3
"""replay_verdicts.py: per-capture hook-entry replay verdicts for the FixDemo and cutscene hooks.

Every rendezvous capture (a SNAP_ROOT/pidN/seqNNN folder, or a SNAP_ROOT/seqNNN folder, with
index.json rv.hook) whose entry VA is in --hook is replayed in two steps:
  1. frame_trace.py recorded traces the captured call from its real entry state (writes the
     callees and the per-snapshot trace under SCRATCH/trace/<label>; resumable);
  2. snapdiff.py replays that call on the harness (PES's bytes) and on the translated function
     over the same snapshot; rax, xmm0 and every written byte must match. snapdiff runs once per
     hook over all of its snapshots (one translated build per hook, not per capture).
Verdict per capture: PROVEN (replayed >= 1, no mismatch), FAIL (a mismatch; its note names the
first diverging field), NOT VERIFIED (no replayable call; the reasons name the blocker).
Coverage is recorded, never a gate (owner ruling 26-10-26).

Usage: replay_verdicts.py --image PES_EXE --cfg HOOKS_CFG --spec-dir SPECS --scratch DIR
                          [--hook VA ...] [--limit N] SNAP_ROOT [SNAP_ROOT ...]
Writes SCRATCH/verdicts.json and SCRATCH/verdicts.md. Scratch only: outputs are PES-derived.
"""
import argparse
import glob
import json
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TRANSLATE = os.path.normpath(os.path.join(HERE, "..", "translate"))
PYTHON = os.environ.get("GF_DIFF_PYTHON", sys.executable)

# The FixDemo evaluator/matcher/callers and the cutscene create/load hooks (ticket item 2).
DEFAULT_HOOKS = [
    "0x141F57840",  # cut create
    "0x141F36200",  # cut load (.fdc reader)
    "0x141F5F330",  # cut load (slot build)
    "0x141F4D2F0",  # FixDemo condition evaluator
    "0x141F4B1D0",  # FixDemo variant matcher
    "0x141F41CE0",  # FixDemo caller
    "0x141F4ACE0",  # FixDemo caller
    "0x141F4B6C0",  # FixDemo caller
    "0x141F50FC0",  # FixDemo caller
]


def captures(roots):
    """Yield (label, capture_dir, hook_name, entry_va) for every seqNNN folder under the roots."""
    for root in roots:
        for idx in sorted(glob.glob(os.path.join(root, "**", "seq*", "index.json"), recursive=True)):
            d = os.path.dirname(idx)
            with open(idx) as f:
                rv = json.load(f).get("rv") or {}
            if not rv.get("hook"):
                continue
            parts = os.path.relpath(d, root).split(os.sep)
            label = "_".join(parts)  # e.g. pid407792_seq006 or seq006
            yield label, d, rv["hook"], "0x%X" % rv["entry_va"]


def run(cmd, log):
    with open(log, "a") as lf:
        lf.write("$ " + " ".join(cmd) + "\n")
        lf.flush()
        p = subprocess.run(cmd, stdout=lf, stderr=subprocess.STDOUT)
    return p.returncode


def verdict_of(res, label):
    """The verdict for one capture from one snapdiff result row (res = that VA's row)."""
    snap = (res.get("snapshots") or {}).get(label) or {}
    replayed = snap.get("replayed", 0)
    mism = [m for m in res.get("mismatches", []) if m.get("snapshot") == label]
    if mism:
        m = mism[0]
        return "FAIL", "; ".join(filter(None, [",".join(m.get("fields", [])), m.get("note") or ""]))
    if replayed > 0:
        return "PROVEN", "%d/%d calls bit-exact" % (replayed, snap.get("calls", 0))
    reasons = res.get("reasons") or {}
    why = "; ".join("%s: %s" % kv for kv in reasons.items()) or res.get("note") or "no replayable call"
    return "NOT VERIFIED", why


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("roots", nargs="+")
    ap.add_argument("--image", required=True)
    ap.add_argument("--cfg", required=True)
    ap.add_argument("--spec-dir", required=True)
    ap.add_argument("--scratch", required=True)
    ap.add_argument("--hook", action="append", default=None, help="hook entry VA (default: the FixDemo/cut set)")
    ap.add_argument("--limit", type=int, default=0, help="stop after N captures (0 = all)")
    a = ap.parse_args()

    hooks = [h.upper() for h in (a.hook or DEFAULT_HOOKS)]
    for sub in ("snaps", "trace", "verdict", "build"):
        os.makedirs(os.path.join(a.scratch, sub), exist_ok=True)
    log = os.path.join(a.scratch, "replay.log")
    os.environ["PES_EXE"] = a.image

    groups, n = {}, 0
    for label, d, hook, va in captures(a.roots):
        if va.upper() not in hooks:
            continue
        if a.limit and n >= a.limit:
            break
        n += 1
        groups.setdefault(va.upper(), []).append((label, d, hook))

    rows = []
    for va in hooks:
        caps = groups.get(va, [])
        if not caps:
            continue
        pairs, labels = [], {}
        for label, d, hook in caps:
            snap = os.path.join(a.scratch, "snaps", label)
            if not os.path.lexists(snap):
                os.symlink(d, snap)
            trace = os.path.join(a.scratch, "trace", label)
            if not os.path.exists(os.path.join(trace, "calls")):
                run([PYTHON, os.path.join(TRANSLATE, "frame_trace.py"), "recorded", snap,
                     "--hooks", a.cfg, "--out", trace, "--spec-dir", a.spec_dir], log)
            pairs += ["--snapshot", "%s=%s" % (snap, trace)]
            labels[label] = hook
        vfile = os.path.join(a.scratch, "verdict", va + ".json")
        if not os.path.exists(vfile):
            run([PYTHON, os.path.join(TRANSLATE, "snapdiff.py"), "--image", a.image,
                 "--out-dir", os.path.join(a.scratch, "build"), "--spec-dir", a.spec_dir,
                 *pairs, "--only", va, "--hooks", a.cfg, "--results", vfile], log)
        try:
            with open(vfile) as f:
                res = {r["va"].upper(): r for r in json.load(f)["results"]}.get(va, {})
        except (OSError, ValueError, KeyError):
            res = {}
        for label, hook in labels.items():
            if res:
                v, why = verdict_of(res, label)
            else:
                v, why = "NOT VERIFIED", "snapdiff produced no result (see replay.log)"
            rows.append({"label": label, "hook": hook, "va": va, "verdict": v, "detail": why})
            print("%-34s %-30s %-13s %s" % (label, hook, v, why), flush=True)

    counts = {}
    for r in rows:
        counts[r["verdict"]] = counts.get(r["verdict"], 0) + 1
    with open(os.path.join(a.scratch, "verdicts.json"), "w") as f:
        json.dump({"counts": counts, "rows": rows}, f, indent=1)
    with open(os.path.join(a.scratch, "verdicts.md"), "w") as f:
        f.write("| capture | hook | verdict | detail |\n|---|---|---|---|\n")
        for r in rows:
            f.write("| %s | %s | %s | %s |\n" % (r["label"], r["hook"], r["verdict"], r["detail"]))
        f.write("\nCounts: %s\n" % json.dumps(counts, sort_keys=True))
    print("counts", json.dumps(counts, sort_keys=True))


if __name__ == "__main__":
    sys.exit(main())
