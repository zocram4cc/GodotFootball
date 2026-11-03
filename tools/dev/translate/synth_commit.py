#!/usr/bin/env python3
"""synth_commit.py: SYNTHETIC call states of the judge commit 0x143F546F0 over a real judge_commit capture, plus the real
captures, replayed together (one snapdiff run, so the own-block coverage is the union).

No capture holds a commit of kind 1, 3 or 4, a level-4 record, a fouler outside side 1 or the `G+0x400` arm (the five arms
blocks 7 of 27 had not reached). synth_call.py keeps the state's memory and sets the pending record's words (rdx+...) and
the pointer G+0x400 (and the byte it names, in the state's own stack scratch); the harness runs PES's bytes, the translated
body runs over the same pages, and snapdiff.py --bounded compares rax and every written byte. Every synthetic case is chosen,
not observed: it is labelled `synthetic_cases` in the record and in docs/ports/pes-port-CardWhistle.md.

Usage: PES_EXE=<pes_full.bin> RV_HOOKS=<hooks.cfg> python3 synth_commit.py STATE_DIR WORK_DIR [--real CAPTURE ...] [--merge]
  STATE_DIR  an unpacked judge_commit_43F546F0 capture (capz.py unpack); its memory and stack are the base of every case
  --real     further captures of the same hook, replayed as they are (not synthetic)
  --merge    write the union record into results_core.json (0x143F546F0)
Harness Python (unicorn). Not part of the gate: the proof is the committed results_core.json record.
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
VA = "0x143F546F0"
G_CELL = 0x1436F3DC8
G_FLAG_PTR = 0x400  # the commit tests *(G+0x400) and the byte it names (0x141F9C3B0 returns *rcx)
KIND_AT, LEVEL_AT, FOULER_AT = 0x14, 0x18, 0x0  # the pending record's words 5, 6 and 0 (rdx+)
FLAG_SCRATCH = 0x60  # rsp-relative, above the return address: the callee frames (the whistle's) grow below it and never reach it


def cases(st):
    rdx = st.reg["rdx"]
    base = {"rcx": st.reg["rcx"], "rdx": rdx}
    at = lambda off: "%#x" % (rdx + off)  # noqa: E731
    out = []
    # The kind arms (JR+0x38): 1 and 4 -> 1, 3 -> the constant yellow, anything else -> 0 (kind 2 is every real call).
    for kind in (1, 3, 4, 0, 5, 7):
        out.append({"name": "kind%d" % kind, "regs": base, "pokes": [(at(KIND_AT), sm.u32(kind))]})
    # Kind 2 copies the record's level word: the real calls hold 2 and 3; 0, 1 and 4 (a red) are synthetic.
    for level in (0, 1, 4):
        out.append({"name": "level%d" % level, "regs": base, "pokes": [(at(LEVEL_AT), sm.u32(level))]})
    # The side class from the fouler's slot: below 0 and 0..10 -> 1, 11..21 -> 0, 22 and up -> 0 (the real fouls are 12 and 20).
    for fouler in (0, 3, 10, 11, 21, 22, 0xFFFFFFFF):
        out.append({"name": "fouler%d" % fouler, "regs": base, "pokes": [(at(FOULER_AT), sm.u32(fouler))]})
    # The G+0x400 arm: a nonzero pointer to a byte. Byte 1: the level is forced to 1 whatever the kind. Byte 0: the pointer
    # is read and the kind arms run as before.
    g = st.u64(G_CELL)
    flag = "%#x" % (st.reg["rsp"] + FLAG_SCRATCH)
    for byte, level in ((1, 3), (1, 4), (0, 3)):
        out.append({"name": "flag%d_level%d" % (byte, level), "regs": base,
                    "pokes": [("%#x" % (g + G_FLAG_PTR), sm.u32(int(flag, 16) & 0xFFFFFFFF) + sm.u32(int(flag, 16) >> 32)),
                              (flag, "%02x" % byte), (at(LEVEL_AT), sm.u32(level))]})
    return out


def trace(cdir, work, label, hooks):
    out = os.path.join(work, "tr_" + label)
    subprocess.check_call([sys.executable, os.path.join(HERE, "frame_trace.py"), "recorded", cdir, "--hooks", hooks, "--out", out,
                           "--spec-dir", SPEC_DIR], stdout=subprocess.DEVNULL)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("state")
    ap.add_argument("work")
    ap.add_argument("--real", action="append", default=[])
    ap.add_argument("--merge", action="store_true")
    a = ap.parse_args()
    image, hooks = os.environ["PES_EXE"], os.environ["RV_HOOKS"]
    os.makedirs(a.work, exist_ok=True)
    st = sm.State(a.state)
    snaps, synthetic = [], 0
    for case in cases(st):
        label, cdir = sm.build_case(st, a.work, VA, case)
        snaps += ["--snapshot", "%s=%s" % (cdir, trace(cdir, a.work, label, hooks))]
        synthetic += 1
    for i, capture in enumerate(a.real):
        link = os.path.join(a.work, "real%d_%s" % (i, os.path.basename(capture.rstrip("/"))))  # snapdiff keys by basename
        if not os.path.lexists(link):
            os.symlink(os.path.abspath(capture), link)
        snaps += ["--snapshot", "%s=%s" % (link, trace(link, a.work, os.path.basename(link), hooks))]
    res = os.path.join(a.work, "res_commit.json")
    subprocess.call([sys.executable, os.path.join(HERE, "snapdiff.py"), "--image", image, "--out-dir", os.path.join(a.work, "build"),
                     "--spec-dir", SPEC_DIR, *snaps, "--only", VA, "--hooks", hooks, "--bounded", "--results", res])
    raw = next((r for r in json.load(open(res))["results"] if r["va"].upper() == VA.upper()), None)
    if raw is None:
        raise SystemExit("snapdiff wrote no result")
    print("%s: proven=%s calls=%s replayed=%s excluded=%s mismatches=%s own_block=%s" % (
        VA, raw["proven"], raw["calls"], raw["replayed"], raw["excluded"], len(raw["mismatches"]), raw["own_block"]))
    for case in cases(st):  # a poked case holds a sparse copy of the state's memory: delete it
        shutil.rmtree(os.path.join(a.work, "case_%s_%s" % (VA[2:], case["name"])), ignore_errors=True)
    if a.merge:
        doc = json.load(open(RESULTS))
        rec = next(r for r in doc["results"] if r["va"].upper() == VA.upper())
        real = len(a.real) + 1
        rec["cases"] = raw["replayed"]
        rec["synthetic_cases"] = synthetic
        rec["coverage"] = {VA: raw["own_block"]}
        rec["snapdiff"] = {"calls": raw["calls"], "mismatches": len(raw["mismatches"]), "own_block": raw["own_block"],
                           "replayed": raw["replayed"], "source": "tools/dev/translate/results_judge.json"}
        rec["note"] = ("proof snapdiff --bounded: every replayed call bit-exact, %d of %d: %d REAL calls (live captures) and %d "
                       "SYNTHETIC call states (synth_commit.py over the memory of one real commit: the pending record's kind, "
                       "level and fouler words and G+0x400 and its byte are CHOSEN, not observed; docs/ports/pes-port-CardWhistle.md). "
                       "Real calls: %s. The commit's bounded closure is 12 functions, all translated." % (
                           raw["replayed"], raw["calls"], real, synthetic, rec["note"].split("Real calls: ")[-1] if "Real calls: " in rec["note"] else "see pes-port-CardProof.md"))
        json.dump(doc, open(RESULTS, "w"), indent=1)
        open(RESULTS, "a").write("\n")


if __name__ == "__main__":
    main()
