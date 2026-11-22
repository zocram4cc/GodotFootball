#!/usr/bin/env python3
"""proof_unit5.py: the snapdiff proof of the carrier manager's unit vfuncs from SYNTHETIC states: the Cross unit end test 0x14211A340
(state 5, vtable 0x142BF0150 slot +0x18), the shared end test 0x14211A550 (units 1 and 3: vtables 0x142BF0030 and 0x142BF00C0 slot +0x18)
and the shared stub 0x140A4C0F0 (`return 1`, slot +0x30 of the vtables 0x142BF0030, 0x142BF0078 and 0x142BF0228). The end tests are
called by the manager tick 0x14211E2D0 while the manager's state word +0xA0 names their unit; the stub by 0x14211E210 (after the tick).

Why synthetic: the units run only while a carrier is in the unit's situation (the Cross unit needs a carrier in the cross zone
imageA+0x278 / +0x294 and a won start roll 0x14211AB10; units 1 and 3 need their start tests won first). Live PES reaches them rarely
(docs/ports/pes-port-TeamTick5.md) and the rig has no capture of any of the three (0x14211A550 and 0x140A4C0F0 cannot take a rendezvous hook: a
call and a six-byte body in the first 14 bytes), so there is no real call to replay. synth.py runs PES's own bytes forward from a
danger_builder_2119390 capture (a carrier's think step, `start: return`) to the entry of the manager tick 0x14211D710, sets the manager
state and draws the inputs the vfunc reads (DRAWS), and captures the state where the run first enters the root. The imageA zone records are
the danger builder's own output (PES's), not drawn. The harness then runs PES's bytes on that state and the translated closure over the same
pages (snapdiff --bounded): the return value and every written byte must match. A state is 3.7 GiB: each seed's states are replayed and
deleted before the next seed is drawn.

Usage (harness Python): PES_EXE=<pes_full.bin> PES21_ROOT=<install> python3 proof_unit5.py WORK --source CAPTURE [--merge]
         [--roots 0x14211A550,0x140A4C0F0] [--seeds 1,2] [--count 8] [--synth-hooks CFG] [--hooks CFG]
  --source: an unpacked danger_builder_2119390 capture (live2/pid1792161/seq16184)
  --hooks: an rv.dll config for the replay (default $PES21_ROOT/rendezvous/hooks.cfg; a root with no hook line replays unpatched)
  --synth-hooks: the config the source capture's launch used (hooks.cfg.97hooks: names danger_builder_2119390)
--merge adds each proven root's record to results_teampass.json and the members to closure_teampass.json (the other roots stay).
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
MANAGER_TICK = "0x14211D710"  # FUN_14211D710(manager, ctx, unit info, state, imageA, plan, side record, proposal): rcx manager, r8 unit info, r9 state
MANAGER_STATE = ("mgr_state", "reg:rcx+0xA0", "u32")  # the manager state word: the unit the tick runs (0x14211E2D0's switch)
STATE_WORD = ("state_word", "reg:r9+0x0", "u32", {"values": [0]})  # the manager tick runs only while this is 0
# The plan record is the sixth argument of the manager tick, read from the stack at entry: [rsp+0x30] (arg 6; ctx[4] of the vfuncs);
# its +8 word is what 0x141052E30 returns (a one-load getter), the "plan phase" the end tests of units 1 and 3 compare against 3.
PLAN_PHASE = ("plan_phase", "reg:rsp+0x30+0x8", "i32", {"range": [-2, 6]})
# (name, address expression, type, range or values): the carrier inputs the Cross unit reads (decompile of 0x14211A340: P = state record r9, the
# unit info r8). The zone's centre and extents are imageA's (PES's builder output), the pitch record G+0x2AA8 is the capture's.
CROSS = [
    ("sign", "reg:r8+0x14", "f32", {"values": [-1.0, 1.0]}),                       # the attack sign: with the z sign it picks +0x278 or +0x294
    ("z", "reg:r9+0x48", "f32", {"range": [-36.0, 36.0]}),                        # P+0x40.z: P+0x40 is the carrier's position copy
    ("held", "reg:r9+0x6f", "u8", {"values": [0, 1]}),                             # the byte the unit answers with while inside the zone
]
# root -> hook name of the written states, manager states that reach it, and the draws: label -> the fields drawn besides the state.
ROOTS = {
    "0x14211A340": {"hook": "ti_unit_14211A340", "states": [5],
                    "draws": {"wide": CROSS + [("x", "reg:r9+0x40", "f32", {"range": [-55.0, 55.0]})],   # the whole pitch: mostly outside the zone
                              "zone": CROSS + [("x", "reg:r9+0x40", "f32", {"range": [-54.0, -31.0]})]}},  # the zone's x band: P+0x6F decides
    "0x14211A550": {"hook": "synthetic_14211A550", "states": [1, 3], "draws": {"plan": [PLAN_PHASE]}},
    # states 1, 2 and 8 run the stub (0x14211E210 calls slot +0x30 of the current state's unit); a case whose end test ended the state
    # first runs the base unit's slot instead and is dropped by "reach"
    "0x140A4C0F0": {"hook": "synthetic_140A4C0F0", "states": [1, 2, 8], "draws": {"plan": [PLAN_PHASE]}},
}


def spec(root, seed, count, label):
    info = ROOTS[root]
    fields = [MANAGER_STATE + ({"values": info["states"]},), STATE_WORD] + info["draws"][label]
    return {"label": "unit_%s_%s" % (root[2:], label), "seed": seed, "count": count, "start": "return", "entry": MANAGER_TICK, "stop": root,
            "reach": root, "hook": info["hook"], "fields": [dict(name=n, at=at, type=t, **how) for n, at, t, how in fields]}


def capz(op, cap):
    subprocess.check_call([sys.executable, os.path.join(REPO, "tools", "dev", "rendezvous", "capz.py"), op, cap], stdout=subprocess.DEVNULL)


def replay(root, caps, work, image, hooks, tag):
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
                     *snaps, "--only", root, "--hooks", hooks, "--bounded", "--results", res])
    raw = next((r for r in json.load(open(res))["results"] if r["va"].upper() == root.upper()), None)
    gen = os.path.join(build, "gen", root[2:])
    members = {"0x%s" % f[len("pes_fn_"):-len(".cpp")].upper() for f in os.listdir(gen)
               if f.startswith("pes_fn_") and f.endswith(".cpp")} - {root} - set(gen_dispatch.natives())  # natives are GF's, never layer members
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


def prove(root, a, image, hooks):
    """The seeds' draws of one root, replayed and deleted state by state: (total result, members, synthetic case count) or None."""
    total, members, n_synth = None, set(), 0
    for seed in [int(s) for s in a.seeds.split(",") if s]:
        for label in ROOTS[root]["draws"]:
            tag = "%s_s%d_%s" % (root[2:], seed, label)
            out = os.path.join(a.work, "states_" + tag)
            spec_path = os.path.join(a.work, "spec_%s.json" % tag)
            json.dump(spec(root, seed, a.count, label), open(spec_path, "w"), indent=1)
            shutil.rmtree(out, ignore_errors=True)
            info = json.loads(subprocess.check_output([sys.executable, os.path.join(HERE, "synth.py"), a.source, out, "--spec", spec_path,
                                                       "--hooks", a.synth_hooks], text=True).strip().splitlines()[-1])
            print("%s: %s" % (tag, info), flush=True)
            states = [os.path.join(out, n) for n in info["written"]]
            if states:
                raw, mem = replay(root, states, a.work, image, hooks, tag)
                print("%s: calls=%s replayed=%s mismatches=%s own_block=%s" % (
                    tag, raw["calls"], raw["replayed"], len(raw["mismatches"]), raw["own_block"]), flush=True)
                total, members, n_synth = add(total, raw), members | mem, n_synth + raw["replayed"]
            shutil.rmtree(out, ignore_errors=True)  # 3.7 GiB a state
    return None if total is None else (total, members, n_synth)


def merge(root, total, members, n_synth):
    own = total["own_block"]
    note = ("proof snapdiff --bounded: every replayed call bit-exact, %d of %d, all SYNTHETIC states (synth.py: PES's own bytes run forward "
            "from a danger_builder_2119390 capture to the manager tick %s with the manager state drawn from %s; the inputs drawn are the "
            "case, proof_unit5.py ROOTS; own-block coverage is the best seed's). No real call: live PES reaches the unit rarely and the "
            "function cannot take a rendezvous hook." % (total["replayed"], total["calls"], MANAGER_TICK, ROOTS[root]["states"]))
    rec = {"aborted": 0, "capped": 0, "cases": 0, "coverage": {root: own}, "dead_blocks": {}, "diffed": True, "failed": 0,
           "failure_class": None, "icount_max": 0, "nonreturn": 0, "note": note, "proof": "snapdiff", "requested": 0,
           "snapdiff": {"calls": total["calls"], "mismatches": 0, "own_block": own, "replayed": total["replayed"],
                        "source": "tools/dev/translate/results_teampass.json"},
           "stubs": [], "translated": True, "va": root, "closure_file": CLOSURE_FILE, "synthetic": True, "synthetic_cases": n_synth}
    doc = json.load(open(RESULTS))
    doc["results"] = [r for r in doc["results"] if r["va"].upper() != root] + [rec]
    json.dump(doc, open(RESULTS, "w"), indent=1)
    open(RESULTS, "a").write("\n")
    closure_path = os.path.join(HERE, CLOSURE_FILE)
    closure = json.load(open(closure_path))
    roots = sorted(set(closure["root"]) | {root})
    closure["root"] = roots
    closure["members"] = sorted((set(closure["members"]) | members) - set(roots))
    json.dump(closure, open(closure_path, "w"), indent=1)
    open(closure_path, "a").write("\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("work")
    ap.add_argument("--source", required=True)
    ap.add_argument("--roots", default=",".join(ROOTS))
    ap.add_argument("--seeds", default="1,2")
    ap.add_argument("--count", type=int, default=8)
    ap.add_argument("--merge", action="store_true")
    ap.add_argument("--hooks", default=None)
    ap.add_argument("--synth-hooks", default=None)
    a = ap.parse_args()
    image = os.environ["PES_EXE"]
    hooks = a.hooks or os.path.join(os.environ["PES21_ROOT"], "rendezvous", "hooks.cfg")
    os.makedirs(a.work, exist_ok=True)
    failed = False
    for root in [r.strip() for r in a.roots.split(",") if r.strip()]:
        got = prove(root, a, image, hooks)
        if got is None:
            print("%s: no case" % root)
            failed = True
            continue
        total, members, n_synth = got
        print("%s: proven=%s calls=%s replayed=%s mismatches=%s own_block=%s (synthetic %d)" % (
            root, total["proven"], total["calls"], total["replayed"], len(total["mismatches"]), total["own_block"], n_synth), flush=True)
        if a.merge and total["proven"]:
            merge(root, total, members, n_synth)
        failed = failed or not total["proven"]
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
