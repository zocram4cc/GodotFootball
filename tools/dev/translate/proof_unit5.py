#!/usr/bin/env python3
"""proof_unit5.py: the snapdiff proof of the carrier manager's Cross unit end test 0x14211A340 (state 5, vtable 0x142BF0150 slot +0x18,
called by the manager tick 0x14211E2D0 while the manager's state word +0xA0 is 5) from SYNTHETIC states.

Why synthetic: the unit runs only while a carrier stands in the cross zone (imageA+0x278 / +0x294) and has won the Cross start roll
(0x14211AB10). Live PES reaches it rarely (docs/ports/pes-port-TeamTick5.md) and the rig's hook on it has not landed a capture, so there is no
real call to replay. synth.py runs PES's own bytes forward from a danger_builder_2119390 capture (a carrier's think step, `start: return`) to the
entry of the manager tick 0x14211D710, sets the manager state to 5 and draws the carrier's position, the attack sign and the unit's
`P+0x6F` byte (FIELDS), and captures the state where the run first enters 0x14211A340. The imageA zone records are the danger builder's own
output (PES's), not drawn. The harness then runs PES's bytes on that state and the translated closure over the same pages (snapdiff
--bounded): the return value and every written byte must match. A state is 3.7 GiB: each seed's states are replayed and deleted before the
next seed is drawn. Two draws, WIDE (the whole pitch: mostly outside the zone, the end test answers true) and ZONE (x in the zone's x
band: the zone test passes about half the time and the `P+0x6F` byte decides).

Usage (harness Python): PES_EXE=<pes_full.bin> PES21_ROOT=<install> python3 proof_unit5.py WORK --source CAPTURE [--merge]
         [--seeds 1,2] [--count 8] [--synth-hooks CFG] [--hooks CFG]
  --source: an unpacked danger_builder_2119390 capture (live2/pid1792161/seq16184)
  --hooks: the rv.dll config naming ti_unit_14211A340 (default $PES21_ROOT/rendezvous/hooks.cfg)
  --synth-hooks: the config the source capture's launch used (hooks.cfg.97hooks: names danger_builder_2119390)
--merge adds the record to results_teampass.json and the members to closure_teampass.json (the other roots stay).
"""
import argparse
import json
import os
import shutil
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_dispatch  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
SPEC_DIR = os.path.join(REPO, "tools", "dev", "diffharness", "specs", "teampass")
RESULTS = os.path.join(HERE, "results_teampass.json")
CLOSURE_FILE = "closure_teampass.json"
ROOT = "0x14211A340"
HOOK = "ti_unit_14211A340"
MANAGER_TICK = "0x14211D710"  # FUN_14211D710(manager, ctx, unit info, state, imageA, plan, side record, proposal): rcx manager, r8 unit info, r9 state
CROSS_STATE = 5               # the manager state word (+0xA0) whose sub-object's slot +0x18 is ROOT (0x14211E2D0's switch)
MANAGER_STATE_OFFSET = 0xA0
# (name, address expression, type, range or values): the carrier inputs the unit reads (decompile of 0x14211A340: P = state record r9, the
# unit info r8). The zone's centre and extents are imageA's (PES's builder output), the pitch record G+0x2AA8 is the capture's.
COMMON = [
    ("mgr_state", "reg:rcx+0x%X" % MANAGER_STATE_OFFSET, "u32", {"values": [CROSS_STATE]}),
    ("state_word", "reg:r9+0x0", "u32", {"values": [0]}),                          # the manager tick runs only while this is 0
    ("sign", "reg:r8+0x14", "f32", {"values": [-1.0, 1.0]}),                       # the attack sign: with the z sign it picks +0x278 or +0x294
    ("z", "reg:r9+0x48", "f32", {"range": [-36.0, 36.0]}),                        # P+0x40.z: P+0x40 is the carrier's position copy
    ("held", "reg:r9+0x6f", "u8", {"values": [0, 1]}),                             # the byte the unit answers with while inside the zone
]
X_RANGES = {"wide": [-55.0, 55.0], "zone": [-54.0, -31.0]}


def spec(seed, count, band):
    fields = COMMON + [("x", "reg:r9+0x40", "f32", {"range": X_RANGES[band]})]
    return {"label": "unit5_%s" % band, "seed": seed, "count": count, "start": "return", "entry": MANAGER_TICK, "stop": ROOT,
            "reach": ROOT, "hook": HOOK, "fields": [dict(name=n, at=at, type=t, **how) for n, at, t, how in fields]}


def capz(op, cap):
    subprocess.check_call([sys.executable, os.path.join(REPO, "tools", "dev", "rendezvous", "capz.py"), op, cap], stdout=subprocess.DEVNULL)


def replay(caps, work, image, hooks, tag):
    """snapdiff --bounded over the state directories `caps`: returns (raw result, member VAs)."""
    snaps = []
    for cap in caps:
        trace = os.path.join(work, "tr_%s_%s" % (tag, os.path.basename(cap.rstrip("/"))))
        subprocess.check_call([sys.executable, os.path.join(HERE, "frame_trace.py"), "recorded", cap, "--hooks", hooks, "--out", trace,
                               "--spec-dir", SPEC_DIR], stdout=subprocess.DEVNULL)
        snaps += ["--snapshot", "%s=%s" % (cap, trace)]
    res = os.path.join(work, "res_%s.json" % tag)
    build = os.path.join(work, "build_%s" % tag)
    subprocess.call([sys.executable, os.path.join(HERE, "snapdiff.py"), "--image", image, "--out-dir", build, "--spec-dir", SPEC_DIR,
                     *snaps, "--only", ROOT, "--hooks", hooks, "--bounded", "--results", res])
    raw = next((r for r in json.load(open(res))["results"] if r["va"].upper() == ROOT.upper()), None)
    gen = os.path.join(build, "gen", ROOT[2:])
    members = {"0x%s" % f[len("pes_fn_"):-len(".cpp")].upper() for f in os.listdir(gen)
               if f.startswith("pes_fn_") and f.endswith(".cpp")} - {ROOT} - set(gen_dispatch.natives())  # natives are GF's, never layer members
    return raw, members


def add(total, raw):
    if total is None:
        return raw
    for key in ("calls", "replayed", "excluded"):
        total[key] += raw[key]
    total["mismatches"] += raw["mismatches"]
    total["proven"] = total["proven"] and raw["proven"]
    total["snapshots"].update(raw["snapshots"])
    if raw["own_block"]["fraction"] > total["own_block"]["fraction"]:
        total["own_block"] = raw["own_block"]
    return total


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("work")
    ap.add_argument("--source", required=True)
    ap.add_argument("--seeds", default="1,2")
    ap.add_argument("--count", type=int, default=8)
    ap.add_argument("--merge", action="store_true")
    ap.add_argument("--hooks", default=None)
    ap.add_argument("--synth-hooks", default=None)
    a = ap.parse_args()
    image = os.environ["PES_EXE"]
    hooks = a.hooks or os.path.join(os.environ["PES21_ROOT"], "rendezvous", "hooks.cfg")
    os.makedirs(a.work, exist_ok=True)
    total, members, n_synth = None, set(), 0
    for seed in [int(s) for s in a.seeds.split(",") if s]:
        for band in X_RANGES:
            out = os.path.join(a.work, "states_%d_%s" % (seed, band))
            spec_path = os.path.join(a.work, "spec_%d_%s.json" % (seed, band))
            json.dump(spec(seed, a.count, band), open(spec_path, "w"), indent=1)
            shutil.rmtree(out, ignore_errors=True)
            info = json.loads(subprocess.check_output([sys.executable, os.path.join(HERE, "synth.py"), a.source, out, "--spec", spec_path,
                                                       "--hooks", a.synth_hooks], text=True).strip().splitlines()[-1])
            print("seed %d %s: %s" % (seed, band, info), flush=True)
            states = [os.path.join(out, n) for n in info["written"]]
            if states:
                raw, mem = replay(states, a.work, image, hooks, "s%d_%s" % (seed, band))
                print("seed %d %s: calls=%s replayed=%s mismatches=%s own_block=%s" % (
                    seed, band, raw["calls"], raw["replayed"], len(raw["mismatches"]), raw["own_block"]), flush=True)
                total, members, n_synth = add(total, raw), members | mem, n_synth + raw["replayed"]
            shutil.rmtree(out, ignore_errors=True)  # 3.7 GiB a state
    if total is None:
        raise SystemExit("no case")
    print("%s: proven=%s calls=%s replayed=%s mismatches=%s own_block=%s (synthetic %d)" % (
        ROOT, total["proven"], total["calls"], total["replayed"], len(total["mismatches"]), total["own_block"], n_synth))
    if not (a.merge and total["proven"]):
        return
    own = total["own_block"]
    note = ("proof snapdiff --bounded: every replayed call bit-exact, %d of %d, all SYNTHETIC states (synth.py: PES's own bytes run forward "
            "from a danger_builder_2119390 capture to the manager tick %s with the manager state set to %d; the carrier fields drawn are the "
            "case, proof_unit5.py COMMON; own-block coverage is the best seed's). No real call: live PES reaches the unit rarely."
            % (total["replayed"], total["calls"], MANAGER_TICK, CROSS_STATE))
    rec = {"aborted": 0, "capped": 0, "cases": 0, "coverage": {ROOT: own}, "dead_blocks": {}, "diffed": True, "failed": 0,
           "failure_class": None, "icount_max": 0, "nonreturn": 0, "note": note, "proof": "snapdiff", "requested": 0,
           "snapdiff": {"calls": total["calls"], "mismatches": 0, "own_block": own, "replayed": total["replayed"],
                        "source": "tools/dev/translate/results_teampass.json"},
           "stubs": [], "translated": True, "va": ROOT, "closure_file": CLOSURE_FILE, "synthetic": True, "synthetic_cases": n_synth}
    doc = json.load(open(RESULTS))
    doc["results"] = [r for r in doc["results"] if r["va"].upper() != ROOT] + [rec]
    json.dump(doc, open(RESULTS, "w"), indent=1)
    open(RESULTS, "a").write("\n")
    closure_path = os.path.join(HERE, CLOSURE_FILE)
    closure = json.load(open(closure_path))
    roots = sorted(set(closure["root"]) | {ROOT})
    closure["root"] = roots
    closure["members"] = sorted((set(closure["members"]) | members) - set(roots))
    json.dump(closure, open(closure_path, "w"), indent=1)
    open(closure_path, "a").write("\n")


if __name__ == "__main__":
    main()
