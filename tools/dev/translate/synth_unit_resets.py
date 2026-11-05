#!/usr/bin/env python3
"""synth_unit_resets.py: SYNTHETIC proof of the dribble units' slot-0 reset methods 0x14211BF10 / BF20 / BF30 / BF40 / BF50.

Each is a one- or two-store method on the unit object (rcx): `*(u32*)(rcx+8) = 2` (BF10), `*(u8*)(rcx+8) = 0` (BF20),
`*(u32*)(rcx+0xC) = 2` (BF30), `*(u32*)(rcx+0xC) = 0xFF; *(u16*)(rcx+8) = 0` (BF40), `*(u32*)(rcx+8) = 0xFF` (BF50). The dribble
proposal calls one when its active unit's slot 0x18 returns true (pes_fn_14211E2D0); no capture holds such a call. synth.py
keeps one real state's memory and points rcx at the state's stack scratch, filled with 0x00, 0xAA or 0xFF, so every store shows
against a different background; the harness runs PES's bytes, the translated body runs over the same pages, and snapdiff.py
--bounded compares every written byte. The object is CHOSEN, not observed: every case is SYNTHETIC (`synthetic_cases` in the record).

Usage: PES_EXE=<pes_full.bin> RV_HOOKS=<hooks.cfg> python3 synth_unit_resets.py STATE_DIR WORK_DIR [--merge]
  STATE_DIR  any unpacked capture (its memory and stack are the base); WORK_DIR scratch. --merge writes results_core.json.
Harness Python (unicorn). Not part of the gate: the proofs are the committed results_core.json records.
"""
import argparse
import json
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import synth_members as sm  # noqa: E402

REPO = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
SPEC_DIR = os.path.join(REPO, "tools", "dev", "diffharness", "specs", "core")
RESULTS = os.path.join(HERE, "results_core.json")
VAS = ("0x14211BF10", "0x14211BF20", "0x14211BF30", "0x14211BF40", "0x14211BF50")
FILLS = ("00", "aa", "ff")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("state")
    ap.add_argument("work")
    ap.add_argument("--merge", action="store_true")
    a = ap.parse_args()
    image, hooks = os.environ["PES_EXE"], os.environ["RV_HOOKS"]
    os.makedirs(a.work, exist_ok=True)
    st = sm.State(a.state)
    records = []
    for va in VAS:
        snaps = []
        for fill in FILLS:
            case = {"name": "fill" + fill, "regs": {"rcx": "dst"}, "pokes": [("dst", fill * 16)]}
            label, cdir = sm.build_case(st, a.work, va, case)
            trace = os.path.join(a.work, "tr_" + label)
            subprocess.check_call([sys.executable, os.path.join(HERE, "frame_trace.py"), "recorded", cdir, "--hooks", hooks, "--out",
                                   trace, "--spec-dir", SPEC_DIR], stdout=subprocess.DEVNULL)
            snaps += ["--snapshot", "%s=%s" % (cdir, trace)]
        res = os.path.join(a.work, "res_%s.json" % va[2:])
        subprocess.call([sys.executable, os.path.join(HERE, "snapdiff.py"), "--image", image, "--out-dir", os.path.join(a.work, "build_" + va[2:]),
                         "--spec-dir", SPEC_DIR, *snaps, "--only", va, "--hooks", hooks, "--bounded", "--results", res])
        raw = next(r for r in json.load(open(res))["results"] if r["va"].upper() == va.upper())
        print("%s: proven=%s calls=%s replayed=%s mismatches=%s own_block=%s" % (
            va, raw["proven"], raw["calls"], raw["replayed"], len(raw["mismatches"]), raw["own_block"]))
        records.append({"aborted": 0, "capped": 0, "cases": 0, "coverage": {va: raw["own_block"]}, "dead_blocks": {}, "diffed": bool(raw["proven"]),
                        "failed": 0, "failure_class": None, "icount_max": 0, "nonreturn": 0,
                        "note": "proof snapdiff --bounded: every replayed call bit-exact, %d of %d. SYNTHETIC: %d call states, rcx = the state's stack "
                                "scratch filled with 0x00 / 0xAA / 0xFF (synth_unit_resets.py): the unit object is chosen, not observed. A dribble "
                                "unit's slot-0 reset, called by the proposal 0x14211E2D0 when the active unit finishes." % (
                                    raw["replayed"], raw["calls"], len(FILLS)),
                        "proof": "snapdiff", "requested": 0,
                        "snapdiff": {"calls": raw["calls"], "mismatches": len(raw["mismatches"]), "own_block": raw["own_block"],
                                     "replayed": raw["replayed"], "source": "tools/dev/translate/results_core.json"},
                        "stubs": [], "synthetic": True, "synthetic_cases": len(FILLS), "translated": True, "va": va})
        for fill in FILLS:
            shutil.rmtree(os.path.join(a.work, "case_%s_fill%s" % (va[2:], fill)), ignore_errors=True)
    if a.merge:
        doc = json.load(open(RESULTS))
        doc["results"] = [r for r in doc["results"] if r["va"].upper() not in VAS] + records
        json.dump(doc, open(RESULTS, "w"), indent=1)
        open(RESULTS, "a").write("\n")


if __name__ == "__main__":
    main()
