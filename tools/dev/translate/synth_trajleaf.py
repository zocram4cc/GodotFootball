#!/usr/bin/env python3
"""synth_trajleaf.py: the proof of the touch-trajectory leaf 0x140852C00 (FUN_140852c00: a switch on a motion index that clamps a float
against per-index thresholds; its only caller 0x1408495B0 is a proven function, but no capture of that caller survived the disk floor).

0x140852C00 has no .pdata entry and reads nothing but its three scalars (ecx = the index, dl = the mirror flag, xmm2 = the float) and the
image's constants, so every case is SYNTHETIC: synth.py keeps the memory of one real capture (any FULL state: only its stack is
used) and sets the call's registers. The cases are chosen from the decompile's thresholds (each index's constants read from the image, and
the float's neighbours on both sides of each), so every arm of the switch and each side of every compare runs. snapdiff.py --bounded
replays each state on PES's bytes (the harness) and on the translated body; the return value and every written byte must match.

Usage (harness Python): PES_EXE=<pes_full.bin> RV_HOOKS=<hooks cfg> python3 synth_trajleaf.py STATE_DIR WORK_DIR [--merge]
  STATE_DIR a FULL capture (unpacked); WORK_DIR scratch (one small dir per case, deleted at the end). Not part of the gate.
"""
import argparse
import json
import os
import shutil
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
SPEC_DIR = os.path.join(REPO, "tools", "dev", "diffharness", "specs", "trapaction")
RESULTS = os.path.join(HERE, "results_trapaction.json")
VA = "0x140852C00"
IMAGE_BASE = 0x140000000
# The constants each motion index compares against (the decompile's DAT_ addresses; the values are read from the image below).
THRESHOLDS = {
    0x244: (0x1425C48FC, 0x14259BE30),
    0x24F: (0x1425AA1C8,),
    0x2CD: (0x14259BE84,),
    0xD0D: (0x1425C5958, 0x14259BE5C),
    0xD3E: (0x142588210, 0x14259BE84),
    0xD4D: (0x14259D618,),
    0xD4E: (0x1425CFA50,),
    0xD51: (0x1425CFA50, 0x14259D618),
    0xD56: (0x1425CFA50,),
    0xD6D: (0x14259BE84, 0x14259D618),
    0xD8F: (0x14259BE84,),
    0xDA0: (0x1425CFA50, 0x14259D618),
    0xDAC: (0x1425AA1C8, 0x14259BE30),
    0xDAD: (0x1425C4900, 0x14259BE5C),
}
OTHER_INDEXES = (0, 1, 0x243, 0x245, 0xD0E, 0xD3F, 0xD57, 0xFFF, 0xFFFFFFF0)  # fall to the unchanged return, below and above 0xD0E
BIG = 1.0e9


def read_f32(image, va):
    with open(image, "rb") as f:
        f.seek(va - IMAGE_BASE)
        return struct.unpack("<f", f.read(4))[0]


def bits(x):
    return struct.unpack("<I", struct.pack("<f", x))[0]


def step(x, d):
    """The float32 d steps from x (d = +-1: the neighbour toward +-inf), for non-negative x and 0."""
    b = bits(x) + d
    return struct.unpack("<f", struct.pack("<I", b))[0]


def floats(image, consts, mirror):
    out = [0.0, BIG, -BIG]
    for i, va in enumerate(consts):
        c = read_f32(image, va)
        out += [c, step(c, 1), -c]
        if i == 0 and not mirror:
            out.append(step(c, -1))
    return sorted(set(out))


def cases(image):
    out = []
    for idx, consts in THRESHOLDS.items():
        for flag in (0, 1):
            for x in floats(image, consts, flag):
                out.append(("i%X_m%d_%08x" % (idx, flag, bits(x)), idx, flag, x))
    for idx in OTHER_INDEXES:
        for flag in (0, 1):
            for x in (0.0, 1.5, -1.5):
                out.append(("o%X_m%d_%08x" % (idx, flag, bits(x)), idx, flag, x))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("state")
    ap.add_argument("work")
    ap.add_argument("--merge", action="store_true", help="write the record into results_trapaction.json")
    a = ap.parse_args()
    image, hooks = os.environ["PES_EXE"], os.environ["RV_HOOKS"]
    os.makedirs(a.work, exist_ok=True)
    snaps, dirs = [], []
    cs = cases(image)
    # One synth.py spec holds every case: a static state of the capture's memory called as VA (rv.entry_va), registers set.
    spec = {"label": "trajleaf", "seed": 1, "as": VA,
            "fields": [{"name": "idx", "at": "ecx", "type": "u32", "values": [0]},
                       {"name": "flag", "at": "edx", "type": "u32", "values": [0]},
                       {"name": "x", "at": "xmm2", "type": "f32", "values": [0.0]}],
            "cases": [{"name": name, "idx": idx, "flag": flag, "x": x} for name, idx, flag, x in cs]}
    spec_path = os.path.join(a.work, "spec.json")
    with open(spec_path, "w") as f:
        json.dump(spec, f)
    states = os.path.join(a.work, "states")
    subprocess.check_call([sys.executable, os.path.join(HERE, "synth.py"), a.state, states, "--spec", spec_path], stdout=subprocess.DEVNULL)
    for name, _, _, _ in cs:
        cdir = os.path.join(states, name)
        trace = os.path.join(a.work, "tr_" + name)
        subprocess.check_call([sys.executable, os.path.join(HERE, "frame_trace.py"), "recorded", cdir, "--hooks", hooks, "--out",
                               trace, "--spec-dir", SPEC_DIR], stdout=subprocess.DEVNULL)
        snaps += ["--snapshot", "%s=%s" % (cdir, trace)]
        dirs.append(cdir)
    res = os.path.join(a.work, "res.json")
    subprocess.call([sys.executable, os.path.join(HERE, "snapdiff.py"), "--image", image, "--out-dir", os.path.join(a.work, "build"),
                     "--spec-dir", SPEC_DIR, *snaps, "--only", VA, "--hooks", hooks, "--bounded", "--results", res])
    raw = next((r for r in json.load(open(res))["results"] if r["va"].upper() == VA.upper()), None)
    print("%s: proven=%s calls=%s replayed=%s excluded=%s mismatches=%s" % (
        VA, raw and raw["proven"], raw and raw["calls"], raw and raw["replayed"], raw and raw["excluded"], raw and len(raw["mismatches"])))
    if a.merge and raw is not None and raw["proven"]:
        own = raw["own_block"]
        rec = {"aborted": 0, "bounded": True, "capped": 0, "cases": 0, "coverage": {VA: own}, "dead_blocks": {}, "diffed": True,
               "failed": 0, "failure_class": None, "icount_max": 0, "nonreturn": 0,
               "note": ("proof snapdiff --bounded: every replayed call bit-exact, %d of %d. SYNTHETIC: %d call states whose registers "
                        "(the motion index, the mirror flag, the float) are chosen over the memory of one real capture "
                        "(synth.py, synth_trajleaf.py); the thresholds each index compares against are read from the image."
                        % (raw["replayed"], raw["calls"], len(cs))),
               "proof": "snapdiff", "requested": 0, "synthetic": True, "synthetic_cases": len(cs),
               "snapdiff": {"calls": raw["calls"], "mismatches": len(raw["mismatches"]), "own_block": own, "replayed": raw["replayed"],
                            "source": "tools/dev/translate/results_trapaction.json"},
               "stubs": [], "translated": True, "va": VA}
        doc = json.load(open(RESULTS))
        doc["results"] = [r for r in doc["results"] if r["va"].upper() != VA.upper()] + [rec]
        json.dump(doc, open(RESULTS, "w"), indent=1)
        open(RESULTS, "a").write("\n")
    for d in dirs:
        shutil.rmtree(d, ignore_errors=True)


if __name__ == "__main__":
    main()
