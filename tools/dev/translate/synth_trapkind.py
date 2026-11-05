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
CLOSURE_FILES = {
    "0x14084B4A0": ("closure_trapkind.json", "the translated closure of 0x14084B4A0 (the touch-kind velocity function): synth_trapkind.py"),
    "0x14079EE70": ("closure_trapendxfer.json", "the closure of 0x14079EE70 (the trap end transition): synth_trapkind.py --root 0x14079EE70"),
    "0x1407BA840": ("closure_trapact4.json", "the closure of 0x1407BA840 (the action-4 Trap request planner): synth_trapkind.py --root 0x1407BA840"),
}
EE70, ACT4 = "0x14079EE70", "0x1407BA840"
REAL = ("pid1572516_398", "pid3439759_3244", "pid1330921_5112")
# pid3741448 seq1938/seq1948 (the proof of 05-11-26: 30/30) can no longer be replayed: auto_slim slimmed them after their unpack and the
# tick's read-set misses a stack page (0x44809698). Only the mod-era tick below enters the transition now.
REAL_EE70 = ("ee_pid1010053_153",)
REAL_A4 = ("ee_pid1010053_153",)  # the transition's entry state; the action virtual's entry is walked from it (build)
CASES = None  # set by main: cases, cases_ee70 or cases_a4
# 0x14079EE70's gates (see cases_a4): the plan [r8+0x5C4] byte (the target's "must not" flag), the word the ee entry reads at rsp+0x28.
EE_GATE = {"r8+0x5C4": "00"}
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
    for base in REAL_EE70:  # the branch that issues the action request (never taken by a captured transition): [r8+0x5C4] poked to 0
        out.append((base, "gate", dict(EE_GATE)))
        out.append((base, "gate_p7", dict(EE_GATE, **{"rsp+0x38": "01"})))
        for stage in (0, 1, 8, 9):  # the planner's answer (the status word the transition tests afterwards) follows the Trap stage
            out.append((base, "gate_s%d" % stage, dict(EE_GATE, **{"rdx+0x245A": "%02x" % stage})))
            out.append((base, "gate_p7s%d" % stage, dict(EE_GATE, **{"rdx+0x245A": "%02x" % stage, "rsp+0x38": "01"})))
        for name, val in (("lo", f32_at(0x1425A358C)), ("hi", f32_at(0x1425EB848))):
            out.append((base, "gate_f" + name, dict(EE_GATE, **{"r8+0x5E4": fhex(val)})))
    return out


def f32_at(va):
    """The float32 at an image VA (the planner's thresholds are read from the image, never typed)."""
    with open(os.environ["PES_EXE"], "rb") as f:
        f.seek(va - 0x140000000)
        return struct.unpack("<f", f.read(4))[0]


def fhex(v):
    return struct.pack("<f", v).hex()


def cases_a4():
    """0x1407BA840, the action-4 (Trap) request planner that 0x1406B4580 reaches through the action table (vt +0x68) from the end
    transition 0x14079EE70. No capture has called it yet (trap_act4_7BA840 is hooked): every case is SYNTHETIC, a real transition entry
    whose gate byte EE_GATE is poked so PES's own bytes run to the planner's entry (build), plus pokes of the planner's own inputs
    (all registers are the planner's entry: rdx = P, r9 = the request the transition built, r8 = the touch plan ring):
      [rdx+0x245A]  the Trap action's stage, the action virtual +0x98 (0 takes the first-half arm, 8/9 the clip arms)
      [rdx+0xAC9]   the action type, the virtual +0x28 (7 = Trap); with [rdx+0xAC8] = 0x5D the second arm
      [r9+0xF8]     the request's float the plan-quality thresholds 0x1425A358C and 0x1425EB848 classify
      [rdx+0x3B18]  the float the early-out compares the plan's second word with"""
    lo, hi = f32_at(0x1425A358C), f32_at(0x1425EB848)
    plans = [{}]
    for stage in (0, 1, 3, 4, 5, 8, 9):
        plans.append({"rdx+0x245A": "%02x" % stage})
    for val in (lo, hi):
        plans.append({"r9+0xF8": fhex(val)})
        plans.append({"r9+0xF8": fhex(val), "rdx+0x245A": "00"})
    for v in (-1.0e6, 0.0, 1.0e6):
        plans.append({"rdx+0x3B18": fhex(v)})
    plans.append({"rdx+0xAC9": "00"})
    plans.append({"rdx+0xAC9": "00", "rdx+0xAC8": "5d"})
    plans.append({"rdx+0xAC9": "00", "rdx+0xAC8": "5d", "rdx+0x245A": "05"})
    plans.append({"rdx+0xAC9": "00", "rdx+0xAC8": "5d", "rdx+0x245A": "00"})
    return [(base, "p%d" % i, (dict(EE_GATE), p)) for base in REAL_A4 for i, p in enumerate(plans)]


def regs_of(state):
    vals = struct.unpack("<49Q", open(os.path.join(state, "call.bin"), "rb").read())
    return dict(zip(sc.GPR_ORDER, vals[:16]))


def pokes_of(state, plan):
    """--poke arguments: an int key is the plan at rcx; "rsp+0x38" is a synth_call address; "r8+0x5C4" is a register plus offset."""
    regs, out = regs_of(state), []
    for off, hexbytes in plan.items():
        if isinstance(off, int):
            addr = "%#x+%#x" % (regs["rcx"], off)
        elif off.split("+")[0] in regs and off.split("+")[0] != "rsp":
            reg, rel = off.split("+", 1)
            addr = "%#x+%s" % (regs[reg], rel)
        else:
            addr = off
        out += ["--poke", "%s=%s" % (addr, hexbytes)]
    return out


def build(states, work, base, name, plan):
    st = os.path.join(states, base)
    cdir = os.path.join(work, "case_%s_%s" % (base, name))
    if ROOT == ACT4:
        # SYNTHETIC entry: the trap end transition's gates are poked (ee_plan, at the transition's entry) so PES's own bytes take
        # the branch that calls 0x1406B4580 and run to the action virtual's entry (synth_walk.py --in-call); plan then pokes that entry.
        ee_plan, plan = plan
        mid = cdir + "_ee"
        subprocess.check_call([sys.executable, os.path.join(HERE, "synth_call.py"), st, mid, "--entry", EE70, "--hook-name",
                               "trap_%s" % EE70[-5:], *pokes_of(st, ee_plan)])
        subprocess.check_call([sys.executable, os.path.join(HERE, "synth_walk.py"), mid, cdir + "_a4", "--in-call", "--entry", ROOT,
                               "--hook-name", "trap_%s" % ROOT[-5:], "--hooks", os.environ["RV_HOOKS"]], stdout=subprocess.DEVNULL)
        shutil.rmtree(mid, ignore_errors=True)
        shutil.rmtree(cdir + "_a4.trace", ignore_errors=True)
        st = cdir + "_a4"
    if ROOT == ACT4 and not plan:
        shutil.move(st, cdir)  # the walked state is the case itself
        return cdir
    cmd = [sys.executable, os.path.join(HERE, "synth_call.py"), st, cdir, "--entry", ROOT, "--hook-name", "trap_%s" % ROOT[-5:],
           *pokes_of(st, plan)]
    subprocess.check_call(cmd)
    if ROOT == ACT4:
        shutil.rmtree(st, ignore_errors=True)
    return cdir


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("states")
    ap.add_argument("work")
    ap.add_argument("--merge", action="store_true", help="write the record into results_trapaction.json")
    ap.add_argument("--reuse", action="store_true", help="skip the build and replay: read WORK/res.json of an earlier run")
    ap.add_argument("--root", default="0x14084B4A0", choices=("0x14084B4A0", "0x14079EE70", "0x1407BA840"))
    a = ap.parse_args()
    global ROOT, REAL, CASES
    CASES = cases
    if a.root == EE70:
        ROOT, REAL, CASES = a.root, REAL_EE70, cases_ee70
    elif a.root == ACT4:
        ROOT, REAL, CASES = a.root, REAL_A4, cases_a4
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
        if ROOT == ACT4:
            note = ("proof snapdiff (unbounded closure): every replayed call bit-exact, %d of %d. EVERY case is SYNTHETIC (%d): no capture "
                    "has called the planner yet, so each is a real end-transition entry state (a captured trap tick) whose gate byte is "
                    "poked so PES's own bytes run to the planner's entry (synth_walk.py --in-call), plus pokes of the planner's inputs "
                    "(the Trap stage, the action type, the request's float, P+0x3B18; synth_trapkind.py --root 0x1407BA840). The "
                    "inputs are chosen, not observed; replace with trap_act4_7BA840 captures when they land." % (
                        raw["replayed"], raw["calls"], synthetic))
        else:
            note = ("proof snapdiff (unbounded closure): every replayed call bit-exact, %d of %d. %d REAL entry states of "
                    "the call inside captured ticks (synth_walk.py --in-call) and %d SYNTHETIC cases whose plan bytes (kind word, "
                    "flags, motion index) are poked (synth_trapkind.py); the synthetic inputs are chosen, not observed."
                    % (raw["replayed"], raw["calls"], len(REAL), synthetic))
        rec = {"aborted": 0, "capped": 0, "cases": 0, "coverage": {ROOT: own}, "dead_blocks": {}, "diffed": True, "failed": 0,
               "failure_class": None, "icount_max": 0, "nonreturn": 0, "note": note,
               "proof": "snapdiff", "requested": 0, "synthetic": True, "synthetic_cases": synthetic,
               "snapdiff": {"calls": raw["calls"], "mismatches": len(raw["mismatches"]), "own_block": own, "replayed": raw["replayed"],
                            "source": "tools/dev/translate/results_trapaction.json"},
               "stubs": [], "translated": True, "va": ROOT}
        closure = CLOSURE_FILES.get(ROOT)
        if closure:  # the functions the proof build compiled: the layer must hold what the proof ran
            gen = os.path.join(a.work, "build", "gen", ROOT[2:])
            members = sorted("0x%s" % f[len("pes_fn_"):-len(".cpp")].upper() for f in os.listdir(gen)
                             if f.startswith("pes_fn_") and f.endswith(".cpp") and "0x%s" % f[len("pes_fn_"):-4].upper() != ROOT.upper())
            rec["closure_file"] = closure[0]
            json.dump({"note": "the functions the proof build of %s compiled (%s): its direct callees and the indirect targets its "
                       "replays reached. VAs only." % (ROOT, closure[1]), "root": ROOT, "members": members},
                      open(os.path.join(HERE, closure[0]), "w"), indent=1)
            open(os.path.join(HERE, closure[0]), "a").write("\n")
        doc = json.load(open(RESULTS))
        doc["results"] = [r for r in doc["results"] if r["va"].upper() != ROOT.upper()] + [rec]
        json.dump(doc, open(RESULTS, "w"), indent=1)
        open(RESULTS, "a").write("\n")
    for d in dirs:
        shutil.rmtree(d, ignore_errors=True)


if __name__ == "__main__":
    main()
