#!/usr/bin/env python3
"""prove_bounded.py: replay captured or synthetic calls of ROOT on a capture-bounded closure and record the proof.

Each snapshot (a REAL capture dir, unpacked, or a SYNTHETIC state dir from synth.py) is traced (frame_trace.py recorded, with the
root's spec in SPEC_DIR) and replayed (snapdiff.py --bounded --only ROOT): PES's bytes against the translated closure, the return
value and every written byte. With --merge a proven run is written to results_trapaction.json (record: bounded, the real and
synthetic counts, own-block coverage) and the functions the replays entered go to closure_<tag>.json, which gen_translated.py
reads so the layer holds exactly what the proof ran.

Usage (harness Python): PES_EXE=<pes_full.bin> RV_HOOKS=<hooks cfg of the captures> prove_bounded.py ROOT WORK --tag TAG
         [--real CAPDIR ...] [--synth STATEDIR ...] [--note TEXT] [--merge]
  The root's spec must exist (closure.py --members-file with the root only). A snapshot that the PES run cannot replay (a page the
  capture lacks) is excluded by snapdiff and makes the run unproven: pass only states that replay. WORK is scratch.
"""
import argparse
import json
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
SPEC_DIR = os.path.join(REPO, "tools", "dev", "diffharness", "specs", "trapaction")
RESULTS = os.path.join(HERE, "results_trapaction.json")


def build_parser():
    ap = argparse.ArgumentParser()
    ap.add_argument("root")
    ap.add_argument("work")
    ap.add_argument("--tag", required=True, help="names closure_<tag>.json")
    ap.add_argument("--real", action="append", default=[], help="a REAL capture of a call of the root (unpacked)")
    ap.add_argument("--synth", action="append", default=[], help="a SYNTHETIC state of the root (synth.py output)")
    ap.add_argument("--note", default="", help="added to the record's note")
    ap.add_argument("--merge", action="store_true")
    ap.add_argument("--spec-dir", default=SPEC_DIR, help="the root's spec directory (default: the trapaction set)")
    ap.add_argument("--results", default=RESULTS, help="the results file the record merges into (default: results_trapaction.json)")
    ap.add_argument("--budget", type=int, default=None, help="instruction budget of each recorded trace (default: frame_trace's 50M); a boot builder runs longer (PES's name tables)")
    return ap


def merge_record(results_path, rec):
    """Write rec into results_path, replacing the record of the same VA and keeping every other record."""
    doc = json.load(open(results_path))
    doc["results"] = [r for r in doc["results"] if r["va"].upper() != rec["va"].upper()] + [rec]
    with open(results_path, "w") as f:
        json.dump(doc, f, indent=1)
        f.write("\n")


def main(argv=None):
    a = build_parser().parse_args(argv)
    root = a.root.upper().replace("0X", "0x")
    image, hooks = os.environ["PES_EXE"], os.environ["RV_HOOKS"]
    os.makedirs(a.work, exist_ok=True)
    snaps = []
    for d in a.real + a.synth:
        trace = os.path.join(a.work, "tr_" + os.path.basename(d.rstrip("/")))
        subprocess.check_call([sys.executable, os.path.join(HERE, "frame_trace.py"), "recorded", d, "--hooks", hooks, "--out", trace,
                               "--spec-dir", a.spec_dir] + (["--budget", str(a.budget)] if a.budget else []), stdout=subprocess.DEVNULL)
        snaps += ["--snapshot", "%s=%s" % (d, trace)]
    res = os.path.join(a.work, "res.json")
    out_dir = os.path.join(a.work, "build")
    subprocess.call([sys.executable, os.path.join(HERE, "snapdiff.py"), "--image", image, "--out-dir", out_dir, "--spec-dir", a.spec_dir,
                     *snaps, "--only", root, "--hooks", hooks, "--bounded", "--results", res])
    raw = next((r for r in json.load(open(res))["results"] if r["va"].upper() == root.upper()), None)
    print("%s: proven=%s calls=%s replayed=%s excluded=%s mismatches=%s own_block=%s" % (
        root, raw and raw["proven"], raw and raw["calls"], raw and raw["replayed"], raw and raw["excluded"],
        raw and len(raw["mismatches"]), raw and raw["own_block"]))
    if not (a.merge and raw is not None and raw["proven"]):
        return
    own = raw["own_block"]
    note = ("proof snapdiff --bounded: every replayed call bit-exact, %d of %d: %d REAL captured call(s)%s; the closure is "
            "capture-bounded (the functions the replays entered), every other call aborts loudly. %s"
            % (raw["replayed"], raw["calls"], len(a.real),
               (" and %d SYNTHETIC state(s) (synth.py: values chosen over a real capture's memory)" % len(a.synth)) if a.synth else "",
               a.note)).strip()
    rec = {"aborted": 0, "bounded": True, "capped": 0, "cases": 0, "closure_file": "closure_%s.json" % a.tag, "coverage": {root: own},
           "dead_blocks": {}, "diffed": True, "failed": 0, "failure_class": None, "icount_max": 0, "nonreturn": 0, "note": note,
           "proof": "snapdiff", "requested": 0,
           "snapdiff": {"calls": raw["calls"], "mismatches": len(raw["mismatches"]), "own_block": own, "replayed": raw["replayed"],
                       "source": os.path.relpath(a.results, REPO)},
           "stubs": [], "translated": True, "va": root}
    if a.synth:
        rec["synthetic"] = True
        rec["synthetic_cases"] = len(a.synth)
    members = sorted(m for m in json.load(open(os.path.join(out_dir, "gen", root[2:], "members.json"))) if m.upper() != root.upper())
    with open(os.path.join(HERE, "closure_%s.json" % a.tag), "w") as f:
        json.dump({"note": "the functions the capture-bounded proof of %s entered (prove_bounded.py). VAs only." % root, "root": root,
                   "members": members}, f, indent=1)
        f.write("\n")
    merge_record(a.results, rec)


if __name__ == "__main__":
    main()
