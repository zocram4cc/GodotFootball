#!/usr/bin/env python3
"""synth_trapkind.py: the proof of the trap tick's touch-kind velocity function 0x14084B4A0 and its static closure.

0x14084B4A0 (called by the contact-point search 0x14085E3C0 from the trap action tick 0x140797CD0) switches on the touch plan's
kind word [rcx+0x6C] and dispatches to a different velocity rule per kind. The live captures reach only kinds 0 and 9; a GF trap
reaches every kind its clip names, so the whole closure (29 functions, closure.py --spec-set trapaction --tag trapkind) is
translated and proven here:
  REAL     the entry states of 0x14084B4A0 inside three captured ticks (synth_walk.py --in-call --entry 0x14084B4A0: PES's own bytes run
           from the tick's entry to the call; nothing is chosen): pid1572516 seq398 and pid3439759 seq3244 (kind 0), pid1330921 seq5112
           (kind 9). All three run No-Mod gameplay.
  SYNTHETIC cases over those same states: bytes of the plan (rcx) are poked: the kind word and the flag bytes that pick a branch.
           A synthetic case is labelled `synthetic` in results_trapaction.json with its count; its inputs are chosen, not observed.
snapdiff.py (unbounded closure) replays every state on PES's bytes (the harness) and on the translated closure; the return value
and every written byte must match.

Usage (harness Python): PES_EXE=<pes_full.bin> RV_HOOKS=<hooks cfg with the trap hooks> python3 synth_trapkind.py STATES_DIR WORK_DIR [--merge]
  STATES_DIR holds the three entry states, named <pid>_<seq> (synth_walk.py output); WORK_DIR is scratch (one poked case at a time holds a
  sparse copy of a state's memory; each is deleted at the end). Not part of the gate.
"""
import argparse
import json
import os
import shutil
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import synth_call as sc  # noqa: E402

REPO = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
SPEC_DIR = os.path.join(REPO, "tools", "dev", "diffharness", "specs", "trapaction")
RESULTS = os.path.join(HERE, "results_trapaction.json")
ROOT = "0x14084B4A0"
REAL = ("pid1572516_398", "pid3439759_3244", "pid1330921_5112")
REAL_EE70 = ("ee_pid3741448_1938", "ee_pid3741448_1948", "ee_pid1010053_153")
CASES = None  # set by main: cases or cases_ee70
KIND = 0x6C  # the plan's kind word: the switch of 0x14084B4A0
FLAG_F, FLAG_10, FLAG_11 = 0xF, 0x10, 0x11  # bytes the default arm and 0x140856370 test
INDEX, W60, W64, W68 = 0x24, 0x60, 0x64, 0x68  # the plan's motion index and the words 0x140856370 tests


def u32(v):
    return struct.pack("<I", v).hex()


def cases():
    """[(base state, name, {plan offset: bytes hex})]."""
    out = []
    for base in REAL:
        out.append((base, "real", {}))
    for kind in (1, 4, 0x13, 0xB, 0xC, 0xD, 7, 9, 2, 0x12):
        out.append(("pid1572516_398", "kind%X" % kind, {KIND: u32(kind)}))
    for kind in (0xB, 0xD, 0xC, 0, 4, 0x13):
        out.append(("pid1330921_5112", "k9to%X" % kind, {KIND: u32(kind)}))
    out.append(("pid1572516_398", "default_f", {KIND: u32(1), FLAG_F: "01"}))
    out.append(("pid1572516_398", "default_11", {KIND: u32(1), FLAG_11: "01"}))
    out.append(("pid1572516_398", "flag10", {FLAG_10: "01"}))
    out.append(("pid1572516_398", "flagF", {FLAG_F: "01"}))
    for idx in (0, 1, 2, 3, 5, 9, 0xC, 0x10):
        out.append(("pid1572516_398", "idx%X" % idx, {INDEX: u32(idx)}))
    for name, w in (("w64", {W64: u32(1)}), ("w68_0", {W68: u32(0)}), ("w68_2", {W68: u32(2)}), ("w60_0", {W60: u32(0)}),
                    ("w60_3", {W60: u32(3)})):
        out.append(("pid1572516_398", name, w))
        out.append(("pid3439759_3244", name, w))
    return out



def cases_ee70():
    """0x14079EE70, the trap end transition the phase-3 handler 0x140796CE0 calls at the clip's end (it ends the action through
    0x14079D4B0, builds the next request with 0x140581B70 and issues it through 0x1406B4580's per-action virtual). Its seventh
    argument [rsp+0x38] and the fifth/sixth words pick the branch; the REAL states are entries inside two captured ticks."""
    out = [(base, "real", {}) for base in REAL_EE70]
    for base in REAL_EE70:
        out.append((base, "p7", {"rsp+0x38": "01"}))
        for name, w in (("p5_0", {"rsp+0x28": u32(0)}), ("p5_big", {"rsp+0x28": u32(0x7FFF)}), ("p6_0", {"rsp+0x30": u32(0)}),
                        ("p6_big", {"rsp+0x30": u32(0x40)})):
            out.append((base, "p7" + name, dict(w, **{"rsp+0x38": "01"})))
            out.append((base, name, w))
    return out

def build(states, work, base, name, plan):
    st = os.path.join(states, base)
    idx = json.load(open(os.path.join(st, "index.json")))
    vals = struct.unpack("<49Q", open(os.path.join(st, "call.bin"), "rb").read())
    rcx = dict(zip(sc.GPR_ORDER, vals[:16]))["rcx"]
    cdir = os.path.join(work, "case_%s_%s" % (base, name))
    cmd = [sys.executable, os.path.join(HERE, "synth_call.py"), st, cdir, "--entry", ROOT, "--hook-name", "trap_%s" % ROOT[-5:]]
    for off, hexbytes in plan.items():  # an int offset is the plan at rcx; a string is a synth_call poke address (rsp+0x38)
        cmd += ["--poke", "%s=%s" % (off if isinstance(off, str) else "%#x+%#x" % (rcx, off), hexbytes)]
    subprocess.check_call(cmd)
    return cdir


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("states")
    ap.add_argument("work")
    ap.add_argument("--merge", action="store_true", help="write the record into results_trapaction.json")
    ap.add_argument("--reuse", action="store_true", help="skip the build and replay: read WORK/res.json of an earlier run")
    ap.add_argument("--root", default="0x14084B4A0", choices=("0x14084B4A0", "0x14079EE70"))
    a = ap.parse_args()
    global ROOT, REAL, CASES
    CASES = cases
    if a.root == "0x14079EE70":
        ROOT, REAL, CASES = a.root, REAL_EE70, cases_ee70
    image, hooks = os.environ["PES_EXE"], os.environ["RV_HOOKS"]
    os.makedirs(a.work, exist_ok=True)
    snaps, dirs, synthetic = [], [], sum(bool(p) for _, _, p in CASES())
    res = os.path.join(a.work, "res.json")
    if not a.reuse:
        for base, name, plan in CASES():
            cdir = build(a.states, a.work, base, name, plan)
            dirs.append(cdir)
            trace = os.path.join(a.work, "tr_%s_%s" % (base, name))
            subprocess.check_call([sys.executable, os.path.join(HERE, "frame_trace.py"), "recorded", cdir, "--hooks", hooks, "--out",
                                   trace, "--spec-dir", SPEC_DIR], stdout=subprocess.DEVNULL)
            snaps += ["--snapshot", "%s=%s" % (cdir, trace)]
        subprocess.call([sys.executable, os.path.join(HERE, "snapdiff.py"), "--image", image, "--out-dir",
                         os.path.join(a.work, "build"), "--spec-dir", SPEC_DIR, *snaps, "--only", ROOT, "--hooks", hooks, "--results",
                         res])
    raw = next((r for r in json.load(open(res))["results"] if r["va"].upper() == ROOT.upper()), None)
    print("%s: proven=%s calls=%s replayed=%s excluded=%s mismatches=%s" % (
        ROOT, raw and raw["proven"], raw and raw["calls"], raw and raw["replayed"], raw and raw["excluded"], raw and len(raw["mismatches"])))
    if a.merge and raw is not None and raw["proven"]:
        own = raw["own_block"]
        rec = {"aborted": 0, "capped": 0, "cases": 0, "coverage": {ROOT: own}, "dead_blocks": {}, "diffed": True, "failed": 0,
               "failure_class": None, "icount_max": 0, "nonreturn": 0,
               "note": "proof snapdiff (unbounded closure): every replayed call bit-exact, %d of %d. %d REAL entry states of "
                       "the call inside captured ticks (synth_walk.py --in-call) and %d SYNTHETIC cases whose plan bytes (kind word, "
                       "flags, motion index) are poked (synth_trapkind.py); the synthetic inputs are chosen, not observed."
                       % (raw["replayed"], raw["calls"], len(REAL), synthetic),
               "proof": "snapdiff", "requested": 0, "synthetic": True, "synthetic_cases": synthetic,
               "snapdiff": {"calls": raw["calls"], "mismatches": len(raw["mismatches"]), "own_block": own, "replayed": raw["replayed"],
                            "source": "tools/dev/translate/results_trapaction.json"},
               "stubs": [], "translated": True, "va": ROOT}
        doc = json.load(open(RESULTS))
        doc["results"] = [r for r in doc["results"] if r["va"].upper() != ROOT.upper()] + [rec]
        json.dump(doc, open(RESULTS, "w"), indent=1)
        open(RESULTS, "a").write("\n")
    for d in dirs:
        shutil.rmtree(d, ignore_errors=True)


if __name__ == "__main__":
    main()
