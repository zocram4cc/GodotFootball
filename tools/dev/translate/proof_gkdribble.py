#!/usr/bin/env python3
"""proof_gkdribble.py: the snapdiff proof of the keeper's GKDribble carrier unit 0x142132E20 (kind 0x13; its callee 0x1421334D0 and the
27-member closure) from SYNTHETIC states and, when the rig has landed them, REAL captured calls (hook tt_142132E20).

SYNTHETIC cases: synth.py runs PES's own bytes forward from a danger_builder_2119390 capture (a carrier's think step, `start: return`)
to the entry of the proposal writer 0x1421102D0, sets the proposal kind to 0x13 and arms the kind object (BP+0x3B30 + 0x38 = 1), draws the
fields below, and captures the state where the run first enters 0x142132E20 (`reach` 0x1421334D0: the draws must open the unit's gate).
Every drawn value is chosen, not observed: the unit's inputs are PES's state except the fields named in FIELDS. The harness then runs PES's
bytes on that state and the translated closure over the same pages (snapdiff --bounded): return value and every written byte must match.
A state is 2 GiB, so each seed's states are replayed and deleted before the next seed is drawn; the counts add up, the own-block coverage
is the best seed's (snapdiff keeps no block set to union).

Usage (harness Python): PES_EXE=<pes_full.bin> PES21_ROOT=<install> python3 proof_gkdribble.py WORK [--merge]
         [--source CAPTURE (unpacked danger_builder capture)] [--seeds 1,2,3] [--count 4] [--real CAPTURE,...] [--hooks CFG] [--synth-hooks CFG]
  --hooks: the rv.dll config of the REAL captures' launch (default $PES21_ROOT/rendezvous/hooks.cfg; must name tt_142132E20)
  --synth-hooks: the config the danger_builder capture's launch used (hooks.cfg.97hooks: names danger_builder_2119390)
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
SPEC_DIR = os.path.join(REPO, "tools", "dev", "diffharness", "specs", "gkdribble")
RESULTS = os.path.join(HERE, "results_gkdribble.json")
CLOSURE_FILE = "closure_gkdribble.json"
ROOT = "0x142132E20"
HOOK = "tt_142132E20"
PROPOSAL_WRITER = "0x1421102D0"   # FUN_1421102D0(BP, kind, step object, record): the driver's call of the kind's unit
TABLE_BUILDER = "0x1421334D0"     # the unit's 32-direction table: reached once the unit's own time gate opens
G_CELL = "mem:0x1436F3DC8"        # *(G): the game object
KIND = 0x13
UNIT_STATE_OFFSET = 0x3B30 + 0x38  # BP + the kind object (FUN_1421105B0 case 0x13) + its state word (FUN_14211EEC0 runs it when 1)
# MA+0x228EC (side +1) / MA+0x22AD4: 11 x 8-byte rows read at +4 + 8 * (the man's row byte + 0 or 0xB); the unit returns while the row's float > 900.0
MA_TABLES = (0x228EC, 0x22AD4)
TABLE_ROWS = 22
# (name, address expression, type, range or values): the inputs the unit reads, drawn over their plausible ranges
FIELDS = [
    ("kind", "edx", "u32", {"values": [KIND]}),
    ("armed", "reg:rcx+0x%X" % UNIT_STATE_OFFSET, "u32", {"values": [1]}),
    ("sign", "reg:r8+0+0x14", "f32", {"values": [-1.0, 1.0]}),                     # S[5]: the attack sign the unit multiplies by
    ("time", G_CELL + "+0x2aa8+0x18", "f32", {"range": [-60.0, 60.0]}),            # the clock the time gate subtracts from 10.0
    ("mx", G_CELL + "+0x20D0+0x55c", "f32", {"range": [-50.0, 50.0]}),             # the man's record (slot 19: (19 + 0xBF) * 0x28) +0x55C
    ("cx", "reg:r8+0x20+0x40", "f32", {"range": [-50.0, 50.0]}),                  # the carrier record's position
    ("cy", "reg:r8+0x20+0x44", "f32", {"range": [-35.0, 35.0]}),
    ("side", G_CELL + "+0x1950+0x250", "f32", {"range": [-180.0, 180.0]}),         # the side record +0x250 (the attack bearing)
]
FIELDS += [("t%x_%d" % (base, k), "reg:r8+0x8+0x18+0x%X" % (base + 4 + 8 * k), "f32", {"range": [0.0, 1500.0]})
           for base in MA_TABLES for k in range(TABLE_ROWS)]


def spec(seed, count):
    return {"label": "gkdribble", "seed": seed, "count": count, "start": "return", "entry": PROPOSAL_WRITER, "stop": ROOT,
            "reach": TABLE_BUILDER, "hook": HOOK,
            "fields": [dict(name=n, at=at, type=t, **how) for n, at, t, how in FIELDS]}


def unpack(cap):
    if os.path.exists(os.path.join(cap, "mem.bin.zst")) and not os.path.exists(os.path.join(cap, "mem.bin")):
        subprocess.check_call([sys.executable, os.path.join(REPO, "tools", "dev", "rendezvous", "capz.py"), "unpack", cap],
                              stdout=subprocess.DEVNULL)
        return True
    return False


def pack(cap):
    subprocess.check_call([sys.executable, os.path.join(REPO, "tools", "dev", "rendezvous", "capz.py"), "pack", cap],
                          stdout=subprocess.DEVNULL)


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
    ap.add_argument("--source", default=None)
    ap.add_argument("--seeds", default="")
    ap.add_argument("--count", type=int, default=4)
    ap.add_argument("--real", default="")
    ap.add_argument("--merge", action="store_true")
    ap.add_argument("--hooks", default=None)
    ap.add_argument("--synth-hooks", default=None)
    a = ap.parse_args()
    image = os.environ["PES_EXE"]
    hooks = a.hooks or os.path.join(os.environ["PES21_ROOT"], "rendezvous", "hooks.cfg")
    os.makedirs(a.work, exist_ok=True)
    total, members, n_synth, n_real = None, set(), 0, 0
    for seed in [int(s) for s in a.seeds.split(",") if s]:
        out = os.path.join(a.work, "states_%d" % seed)
        spec_path = os.path.join(a.work, "spec_%d.json" % seed)
        json.dump(spec(seed, a.count), open(spec_path, "w"), indent=1)
        shutil.rmtree(out, ignore_errors=True)
        info = json.loads(subprocess.check_output([sys.executable, os.path.join(HERE, "synth.py"), a.source, out, "--spec", spec_path,
                                                   "--hooks", a.synth_hooks], text=True).strip().splitlines()[-1])
        print("seed %d: %s" % (seed, info), flush=True)
        states = [os.path.join(out, n) for n in info["written"]]
        if states:
            raw, mem = replay(states, a.work, image, hooks, "s%d" % seed)
            print("seed %d: calls=%s replayed=%s mismatches=%s own_block=%s" % (
                seed, raw["calls"], raw["replayed"], len(raw["mismatches"]), raw["own_block"]), flush=True)
            total, members, n_synth = add(total, raw), members | mem, n_synth + raw["replayed"]
        shutil.rmtree(out, ignore_errors=True)  # 2 GiB a state
    real = [c for c in a.real.split(",") if c]
    for i in range(0, len(real), 4):
        chunk = real[i:i + 4]
        packed = [c for c in chunk if unpack(c)]
        try:
            raw, mem = replay(chunk, a.work, image, hooks, "real%d" % i)
        finally:
            for c in packed:
                pack(c)
        total, members, n_real = add(total, raw), members | mem, n_real + raw["replayed"]
    if total is None:
        raise SystemExit("no case")
    print("%s: proven=%s calls=%s replayed=%s mismatches=%s own_block=%s (synthetic %d, real %d)" % (
        ROOT, total["proven"], total["calls"], total["replayed"], len(total["mismatches"]), total["own_block"], n_synth, n_real))
    if not (a.merge and total["proven"]):
        return
    own = total["own_block"]
    note = ("proof snapdiff --bounded: every replayed call bit-exact, %d of %d: %d REAL captured calls (hook %s), %d SYNTHETIC states "
            "(synth.py: PES's own bytes run forward from a danger_builder_2119390 capture to the proposal writer %s with the kind set to "
            "0x13 and the kind object armed; the fields drawn are the case, proof_gkdribble.py FIELDS; own-block coverage is the best seed's)."
            % (total["replayed"], total["calls"], n_real, HOOK, n_synth, PROPOSAL_WRITER))
    rec = {"aborted": 0, "capped": 0, "cases": 0, "coverage": {ROOT: own}, "dead_blocks": {}, "diffed": True, "failed": 0,
           "failure_class": None, "icount_max": 0, "nonreturn": 0, "note": note, "proof": "snapdiff", "requested": 0,
           "snapdiff": {"calls": total["calls"], "mismatches": 0, "own_block": own, "replayed": total["replayed"],
                        "source": "tools/dev/translate/results_gkdribble.json"},
           "stubs": [], "translated": True, "va": ROOT, "closure_file": CLOSURE_FILE}
    if n_synth:
        rec["synthetic"] = True
        rec["synthetic_cases"] = n_synth
    json.dump({"note": "keeper GKDribble unit 0x142132E20 (pes-port-TeamTick2.md): proof_gkdribble.py.", "results": [rec]},
              open(RESULTS, "w"), indent=1)
    open(RESULTS, "a").write("\n")
    json.dump({"note": "the functions the proof builds of the keeper GKDribble unit compiled: proof_gkdribble.py. VAs only.",
               "root": [ROOT], "members": sorted(members)}, open(os.path.join(HERE, CLOSURE_FILE), "w"), indent=1)
    open(os.path.join(HERE, CLOSURE_FILE), "a").write("\n")


if __name__ == "__main__":
    main()
