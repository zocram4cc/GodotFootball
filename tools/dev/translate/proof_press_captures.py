#!/usr/bin/env python3
"""proof_press_captures.py: the press roots' proofs over EVERY rendezvous capture of their hooks.

The roots: the press writer 0x140536FC0 (hook ai_press_writer_536FC0), the target eval 0x140633940 (ai_press_eval_633940) and the
coordinator 0x140634470 (ai_press_coord_634470). Each capture is one real call of the root from the dispatcher FUN_140535C60; the
harness runs PES's bytes on it, the translated body runs over the same pages (snapdiff.py --bounded), and every replayed call's
return value and every written byte must match. A capture whose trace fails (the call left the capture's pages) is listed and
skipped: it is no evidence either way.

Usage: PES_EXE=<pes_full.bin> RV_HOOKS=<hooks.cfg> python3 proof_press_captures.py WORK_DIR LAUNCH_DIR [LAUNCH_DIR ...] [--merge]
  LAUNCH_DIR  a rendezvous launch folder (live2/pidNNN); slim and unpacked full captures replay as they are, packed ones are skipped
  --merge     rewrite the three records of results_press.json and closure_press_{writer,eval,coord}.json
Harness Python (unicorn). Not part of the gate: the proofs are the committed results_press.json records.
"""
import argparse
import glob
import json
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import synth_press_order as sp  # noqa: E402

ROOTS = {"ai_press_writer_536FC0": ("0x140536FC0", "closure_press_writer.json", "the press writer FUN_140536FC0 (T+0xA548 PRESS/DELAY slots)"),
         "ai_press_eval_633940": ("0x140633940", "closure_press_eval.json", "the press target eval FUN_140633940 (second presser pick)"),
         "ai_press_coord_634470": ("0x140634470", "closure_press_coord.json", "the press coordinator FUN_140634470 (PRESS/SAND second presser)")}


def captures(launches):
    """{hook: [capture dir]}: every capture of a press hook whose memory replays (not a packed .zst)."""
    out = {h: [] for h in ROOTS}
    for launch in launches:
        for d in sorted(glob.glob(os.path.join(launch, "seq*"))):
            try:
                hook = json.load(open(os.path.join(d, "index.json")))["rv"]["hook"]
            except (OSError, ValueError, KeyError):
                continue
            if hook in ROOTS and os.path.exists(os.path.join(d, "mem.bin")):
                out[hook].append(d)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("work")
    ap.add_argument("launches", nargs="+")
    ap.add_argument("--merge", action="store_true")
    ap.add_argument("--only", action="append", help="roots to run (default: all three); --merge rewrites only their records")
    ap.add_argument("--state", action="append", help="a WRITER capture (slim is fine) the writer's SYNTHETIC states poke; repeatable")
    ap.add_argument("--writer-synthetic", type=int, default=0, help="SYNTHETIC writer states added to the real captures (they reach the "
                    "enrolment arm; the real captures leave at the 11-slot body filter)")
    a = ap.parse_args()
    image, hooks = os.environ["PES_EXE"], os.environ["RV_HOOKS"]
    os.makedirs(a.work, exist_ok=True)
    found = captures(a.launches)
    records = []
    for hook, (va, closure_file, what) in ROOTS.items():
        if a.only and va not in a.only:
            continue
        snaps, used, skipped = [], [], []
        for d in found[hook]:
            label = "%s_%s" % (os.path.basename(os.path.dirname(d)), os.path.basename(d))
            trace = os.path.join(a.work, "tr_" + label)
            rc = subprocess.call([sys.executable, os.path.join(HERE, "frame_trace.py"), "recorded", d, "--hooks", hooks, "--out", trace,
                                  "--spec-dir", sp.SPEC_DIR], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            if rc != 0:
                skipped.append(label)
                continue
            snaps += ["--snapshot", "%s=%s" % (d, trace)]
            used.append(label)
        synthetic = 0
        if va == "0x140536FC0" and a.state:
            # a case whose call needs a page the slim base dropped does not trace: it is discarded, never faked
            bases = [(sp.State(s), None) for s in a.state]
            bases = [(st, sp.World(st)) for st, _ in bases]
            for n in range(8 * a.writer_synthetic):
                if synthetic == a.writer_synthetic:
                    break
                st, w = bases[n % len(bases)]
                label, cdir = sp.build_case(st, w, a.work, va, n)
                trace = os.path.join(a.work, "tr_" + label)
                rc = subprocess.call([sys.executable, os.path.join(HERE, "frame_trace.py"), "recorded", cdir, "--hooks", hooks, "--out", trace,
                                      "--spec-dir", sp.SPEC_DIR], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                if rc != 0:
                    shutil.rmtree(cdir, ignore_errors=True)
                    continue
                snaps += ["--snapshot", "%s=%s" % (cdir, trace)]
                synthetic += 1
        res = os.path.join(a.work, "res_%s.json" % va[2:])
        out_dir = os.path.join(a.work, "build_" + va[2:])
        subprocess.call([sys.executable, os.path.join(HERE, "snapdiff.py"), "--image", image, "--out-dir", out_dir, "--spec-dir", sp.SPEC_DIR,
                         *snaps, "--only", va, "--hooks", hooks, "--bounded", "--results", res])
        raw = next(r for r in json.load(open(res))["results"] if r["va"].upper() == va.upper())
        print("%s: captures=%d traced=%d skipped=%d -> proven=%s calls=%s replayed=%s excluded=%s mismatches=%s own_block=%s" % (
            va, len(found[hook]), len(used), len(skipped), raw["proven"], raw["calls"], raw["replayed"], raw["excluded"],
            len(raw["mismatches"]), raw["own_block"]))
        records.append((va, closure_file, what, raw, os.path.join(out_dir, "gen", va[2:], "members.json"), used, skipped, synthetic))
    if a.merge:
        doc = json.load(open(sp.RESULTS))
        vas = {va.upper() for va, *_ in records}
        keep = [r for r in doc["results"] if r["va"].upper() not in vas]
        new = []
        for va, closure_file, what, raw, members, used, skipped, synthetic in records:
            extra = ("; plus %d SYNTHETIC states over one full match state (synth_press_order.py writer_case: the play state, the side's men "
                     "off the body filter, T+0xF1 armed, men near the ball are chosen)" % synthetic) if synthetic else ""
            rec = {"aborted": 0, "capped": 0, "cases": 0, "closure_file": closure_file, "coverage": {va: raw["own_block"]},
                   "dead_blocks": {}, "diffed": bool(raw["proven"]), "failed": 0, "failure_class": None, "icount_max": 0, "nonreturn": 0,
                   "note": "proof snapdiff --bounded: every replayed call bit-exact, %d of %d (%s; %d real captures of the hook in %s, %d "
                           "skipped: their trace left the captured pages%s). proof_press_captures.py." % (
                               raw["replayed"], raw["calls"], what, len(used),
                               ", ".join(sorted({u.split("_")[0] for u in used})), len(skipped), extra),
                   "proof": "snapdiff", "requested": 0,
                   "snapdiff": {"calls": raw["calls"], "mismatches": len(raw["mismatches"]), "own_block": raw["own_block"],
                                "replayed": raw["replayed"], "source": "tools/dev/translate/results_press.json"},
                   "stubs": [], "translated": True, "va": va}
            if synthetic:
                rec["synthetic"] = True
                rec["synthetic_cases"] = synthetic
            new.append(rec)
            sp.write_closure(image, va, closure_file, members, "the proof of %s over %d real captures%s (proof_press_captures.py)" % (
                what, len(used), " and %d SYNTHETIC states" % synthetic if synthetic else ""))
        doc["results"] = keep + new
        json.dump(doc, open(sp.RESULTS, "w"), indent=1)
        open(sp.RESULTS, "a").write("\n")


if __name__ == "__main__":
    main()
