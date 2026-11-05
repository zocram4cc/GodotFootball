#!/usr/bin/env python3
"""proof_teampass.py: the snapdiff proof of the team tick's pass-target selector chain from REAL captured calls.

The chain 0x1404FB2A0 (the selector, team tick step 40c) -> 0x1404FB380 (in play and the restart arm) / 0x1404FB780 (the stoppage arm)
-> 0x1404F0E50 (the holder tree) sets the team record's pass target T+0x78 (docs/ports/pes-port-TeamTick.md). Each root is hooked in the
rendezvous rig (team_pass_4FB2A0, team_pass_4FB380, team_pass_4FB780, team_pass_F0E50); a capture of one is one real call of that root at
its entry. This tool replays every capture of a root (frame_trace.py recorded, then snapdiff.py: PES's own bytes in the harness against
the translated closure over the capture's pages; the return value and every written byte must match) and, with --merge, writes the
record into results_teampass.json. No synthetic case: the inputs are all observed.

Usage (harness Python): PES_EXE=<pes_full.bin> python3 proof_teampass.py WORK_DIR [--merge] [--hooks CFG] ROOT=CAPTURE_DIR[,CAPTURE_DIR...] ...
  e.g. proof_teampass.py w --merge 0x1404FB380=live2/pid281026/seq709,live2/pid281026/seq754
The captures must be unpacked (capz.py unpack): pack them again afterwards.
"""
import argparse
import json
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
SPEC_DIR = os.path.join(REPO, "tools", "dev", "diffharness", "specs", "teampass")
RESULTS = os.path.join(HERE, "results_teampass.json")
CLOSURE_FILE = "closure_teampass.json"
DEFAULT_HOOKS = "rendezvous/hooks.cfg"  # under the PES install (PES21_ROOT)


def replay(root, caps, work, image, hooks):
    snaps = []
    for cap in caps:
        trace = os.path.join(work, "tr_%s_%s" % (root[2:], os.path.basename(cap.rstrip("/"))))
        subprocess.check_call([sys.executable, os.path.join(HERE, "frame_trace.py"), "recorded", cap, "--hooks", hooks, "--out", trace,
                               "--spec-dir", SPEC_DIR], stdout=subprocess.DEVNULL)
        snaps += ["--snapshot", "%s=%s" % (cap, trace)]
    res = os.path.join(work, "res_%s.json" % root[2:])
    build = os.path.join(work, "build_%s" % root[2:])
    subprocess.call([sys.executable, os.path.join(HERE, "snapdiff.py"), "--image", image, "--out-dir", build, "--spec-dir", SPEC_DIR,
                     *snaps, "--only", root, "--hooks", hooks, "--results", res])
    raw = next((r for r in json.load(open(res))["results"] if r["va"].upper() == root.upper()), None)
    return raw, build


def record(root, raw, caps, build):
    own = raw["own_block"]
    note = ("proof snapdiff (unbounded closure): every replayed call bit-exact, %d of %d, all REAL captured calls of the root "
            "(rendezvous hook team_pass_%s; no synthetic case). proof_teampass.py." % (raw["replayed"], raw["calls"], root[-5:]))
    rec = {"aborted": 0, "capped": 0, "cases": 0, "coverage": {root: own}, "dead_blocks": {}, "diffed": True, "failed": 0,
           "failure_class": None, "icount_max": 0, "nonreturn": 0, "note": note, "proof": "snapdiff", "requested": 0,
           "snapdiff": {"calls": raw["calls"], "mismatches": len(raw["mismatches"]), "own_block": own, "replayed": raw["replayed"],
                        "source": "tools/dev/translate/results_teampass.json"},
           "stubs": [], "translated": True, "va": root, "closure_file": CLOSURE_FILE}
    gen = os.path.join(build, "gen", root[2:])
    members = sorted("0x%s" % f[len("pes_fn_"):-len(".cpp")].upper() for f in os.listdir(gen)
                     if f.startswith("pes_fn_") and f.endswith(".cpp") and "0x%s" % f[len("pes_fn_"):-4].upper() != root.upper())
    return rec, members


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("work")
    ap.add_argument("roots", nargs="+", help="ROOT=CAPTURE[,CAPTURE...]")
    ap.add_argument("--merge", action="store_true")
    ap.add_argument("--hooks", default=None)
    a = ap.parse_args()
    image = os.environ["PES_EXE"]
    hooks = a.hooks or os.path.join(os.environ["PES21_ROOT"], DEFAULT_HOOKS)
    os.makedirs(a.work, exist_ok=True)
    doc = json.load(open(RESULTS)) if os.path.exists(RESULTS) else {"results": []}
    closure_path = os.path.join(HERE, CLOSURE_FILE)
    members_all = set(json.load(open(closure_path))["members"]) if os.path.exists(closure_path) else set()
    for spec in a.roots:
        root, caps = spec.split("=", 1)
        root = "0x%X" % int(root, 16)
        raw, build = replay(root, caps.split(","), a.work, image, hooks)
        print("%s: proven=%s calls=%s replayed=%s excluded=%s mismatches=%s own_block=%s" % (
            root, raw and raw["proven"], raw and raw["calls"], raw and raw["replayed"], raw and raw["excluded"],
            raw and len(raw["mismatches"]), raw and raw["own_block"]))
        if a.merge and raw is not None and raw["proven"]:
            rec, members = record(root, raw, caps.split(","), build)
            members_all |= set(members)
            doc["results"] = [r for r in doc["results"] if r["va"].upper() != root.upper()] + [rec]
    if a.merge:
        json.dump(doc, open(RESULTS, "w"), indent=1)
        open(RESULTS, "a").write("\n")
        roots = {r["va"] for r in doc["results"]}
        members_all -= roots
        json.dump({"note": "the functions the proof builds of the team pass-target chain compiled (direct callees and the indirect targets "
                           "the replays reached): proof_teampass.py. VAs only.", "root": sorted(roots), "members": sorted(members_all)},
                  open(os.path.join(HERE, CLOSURE_FILE), "w"), indent=1)
        open(os.path.join(HERE, CLOSURE_FILE), "a").write("\n")


if __name__ == "__main__":
    main()
