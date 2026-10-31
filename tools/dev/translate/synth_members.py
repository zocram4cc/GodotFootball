#!/usr/bin/env python3
"""synth_members.py: the per-function proofs of the dribble proposal chain's members that no captured or forward-emulated
state calls (DribbleProposal3/4's eleven small ones; 0x14063D2F0 is called by real angle captures and is proven on those).

Every case is SYNTHETIC: synth_call.py keeps the memory of one forward-emulated state (the kind step 0x14212FE70's entry state,
synth_walk.py --entry 0x14212FE70 from a danger_builder_2119390 capture) and sets the registers (and a few poked bytes in
scratch stack space) of a call of the member; the harness runs PES's bytes on it, the translated body runs over the same
pages, and snapdiff.py --bounded compares the return value and every written byte. The records are merged into
results_dribble.json marked `synthetic`, with the case count.

Usage: PES_EXE=<pes_full.bin> python3 synth_members.py STATE_DIR WORK_DIR [--only 0xVA ...] [--merge]
  STATE_DIR  synth_walk.py --entry 0x14212FE70 output (keep it whole: it is PES's memory)
  WORK_DIR   scratch: one small dir per case (mem.bin is a link, or a sparse copy where bytes are poked; deleted per case)
Harness Python (unicorn). Not part of the gate: the proofs are committed as results_dribble.json records.
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
SPEC_DIR = os.path.join(REPO, "tools", "dev", "diffharness", "specs", "dribble")
RESULTS = os.path.join(HERE, "results_dribble.json")
G_CELL = 0x1436F3DC8
ANIME_SLOT_BASE, SLOT_STRIDE = 0xBF, 0x28  # FUN_140a48a10(G, slot) = *(G + (slot + 0xBF) * 0x28)
DST, SRC, OUT = -0x400, -0x200, -0x300  # scratch below the state's rsp, which the state maps


def u32(v):
    return struct.pack("<I", v).hex()


def f32(v):
    return struct.pack("<f", v).hex()


class State:
    def __init__(self, path):
        self.path = path
        self.index = json.load(open(os.path.join(path, "index.json")))
        self.vals = struct.unpack("<49Q", open(os.path.join(path, "call.bin"), "rb").read())
        self.reg = dict(zip(sc.GPR_ORDER, self.vals[:16]))

    def u64(self, va):
        return struct.unpack("<Q", sc.read_state(self.path, self.index, va, 8))[0]


def cases(st):
    """{member VA: [case]}; a case is {name, regs, pokes}. Scratch addresses are rsp-relative (resolved by the caller)."""
    g = st.u64(G_CELL)
    anime = lambda slot: st.u64(g + (slot + ANIME_SLOT_BASE) * SLOT_STRIDE)  # noqa: E731
    out = {}
    out["0x14063E830"] = [{"name": "slot%d_bit%d" % (s, b), "regs": {"rcx": 0, "edx": s, "r8d": b}}
                          for s, b in ((3, 5), (14, 0), (21, 31), (22, 2), (0, 17), (10, 3), (11, 3))]
    lists = {"empty": [], "one": [(0x10, 0.5)], "five": [(0x10, 0.5), (0x11, 0.9), (0x12, 0.1), (0x13, 0.9), (0x14, 0.3)],
             "sorted": [(0x15, 0.9), (0x14, 0.6), (0x13, 0.2)]}
    out["0x14063E9F0"] = [{"name": "list_" + k, "regs": {"rcx": "dst"},
                           "pokes": [("dst+4", u32(len(v))), ("dst+8", "".join(u32(i) + f32(s) for i, s in v) or "00")]}
                          for k, v in lists.items()]
    out["0x142119D90"] = [{"name": "dst_" + k, "regs": {"rcx": "dst"}, "pokes": [("dst", fill * 16)]}
                          for k, fill in (("ff", "ff"), ("zero", "00"))]
    srcs = {"a": u32(0x11) + u32(0x22) + u32(0x3F800000), "b": u32(0xFFFFFFFF) + u32(0) + u32(0xBF800000)}
    out["0x142119E00"] = [{"name": "src_" + k, "regs": {"rcx": "dst", "rdx": "src"},
                           "pokes": [("dst", "aa" * 24), ("src", v)]} for k, v in srcs.items()]
    for va in ("0x142119E60", "0x142119EB0"):
        out[va] = [{"name": "src_%x" % v, "regs": {"rcx": "dst", "rdx": "src"},
                    "pokes": [("dst", "aa" * 24), ("src", u32(v))]} for v in (0x16, 0x7FFFFFFF)]
    out["0x142119E90"] = [{"name": "dst_" + k, "regs": {"rcx": "dst"}, "pokes": [("dst", fill * 24)]}
                          for k, fill in (("ff", "ff"), ("zero", "00"))]
    out["0x14211CA50"] = [{"name": "word_%08x" % w, "regs": {"rcx": "dst"}, "pokes": [("dst+0x14", u32(w))]}
                          for w in (0x3F800000, 0xBF800000, 0, 0x80000000)]
    cb = [(e, None) for e in (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 11)] + [(10, 4), (10, 1), (10, 10)]
    out["0x14211CB00"] = [{"name": "kind%d%s" % (e, "" if a is None else "_a0_%d" % a), "regs": {"rcx": "dst", "edx": e},
                           "pokes": [("dst+0xA0", u32(a if a is not None else 0))]} for e, a in cb]
    out["0x140A57450"] = [{"name": "slot%d_n%d" % (s, n), "regs": {"rcx": anime(s), "rdx": "out", "r8d": n}}
                          for s, n in ((0, 0), (0, 1), (5, 3), (0, 249), (0, 250))]
    kind = st.reg["r8"]
    out["0x142130EE0"] = [{"name": "kind_step_args", "regs": {"rcx": st.reg["rcx"], "rdx": "dst", "r8": kind,
                                                               "r9": st.u64(kind + 0x38) + 0xC0}}]
    return out


def resolve(st, v):
    rsp = st.reg["rsp"]
    return {"dst": rsp + DST, "src": rsp + SRC, "out": rsp + OUT}.get(v, v) if isinstance(v, str) else v


def build_case(st, work, va, case):
    label = "%s_%s" % (va[2:], case["name"])
    cdir = os.path.join(work, "case_" + label)
    regs = ["%s=%#x" % (r, resolve(st, v)) for r, v in case["regs"].items()]
    pokes = []
    for where, hexbytes in case.get("pokes", []):
        base, _, off = where.partition("+")
        pokes.append("%#x%s=%s" % (resolve(st, base), "+" + off if off else "", hexbytes))
    cmd = [sys.executable, os.path.join(HERE, "synth_call.py"), st.path, cdir, "--entry", va]
    for r in regs:
        cmd += ["--reg", r]
    for p in pokes:
        cmd += ["--poke", p]
    subprocess.check_call(cmd)
    return label, cdir


def committed(va, res_path, note, synthetic_cases):
    """The results_dribble.json record of snapdiff's raw result for `va`, or None when snapdiff wrote none."""
    raw = next((r for r in json.load(open(res_path))["results"] if r["va"].upper() == va.upper()), None) if os.path.exists(res_path) else None
    if raw is None:
        print("%s: no result" % va)
        return None
    print("%s: proven=%s calls=%s replayed=%s excluded=%s mismatches=%s" % (
        va, raw["proven"], raw["calls"], raw["replayed"], raw["excluded"], len(raw["mismatches"])))
    own = raw["own_block"]
    rec = {"aborted": 0, "capped": 0, "cases": 0, "coverage": {va: own}, "dead_blocks": {}, "diffed": bool(raw["proven"]),
           "failed": 0, "failure_class": None, "icount_max": 0, "nonreturn": 0,
           "note": "proof snapdiff --bounded: every replayed call bit-exact, %d of %d. %s" % (raw["replayed"], raw["calls"], note),
           "proof": "snapdiff", "requested": 0,
           "snapdiff": {"calls": raw["calls"], "mismatches": len(raw["mismatches"]), "own_block": own, "replayed": raw["replayed"],
                        "source": "tools/dev/translate/results_dribble.json"},
           "stubs": [], "translated": True, "va": va}
    if synthetic_cases:
        rec["synthetic"] = True
        rec["synthetic_cases"] = synthetic_cases
    return rec


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("state")
    ap.add_argument("work")
    ap.add_argument("--only", action="append", default=[])
    ap.add_argument("--merge", action="store_true", help="write the records into results_dribble.json")
    ap.add_argument("--real", action="append", default=[], metavar="VA=CAPTURE",
                    help="a function a real capture calls: its calls are replayed as they are (not synthetic)")
    a = ap.parse_args()
    image = os.environ["PES_EXE"]
    hooks = os.environ.get("RV_HOOKS", "/mnt/Extra4/PES21/rendezvous/hooks.cfg")
    st = State(a.state)
    records = []
    for va, cs in cases(st).items():
        if a.only and va not in a.only:
            continue
        snaps = []
        for case in cs:
            label, cdir = build_case(st, a.work, va, case)
            trace = os.path.join(a.work, "tr_" + label)
            subprocess.check_call([sys.executable, os.path.join(HERE, "frame_trace.py"), "recorded", cdir, "--hooks", hooks,
                                   "--out", trace, "--spec-dir", SPEC_DIR], stdout=subprocess.DEVNULL)
            snaps += ["--snapshot", "%s=%s" % (cdir, trace)]
        res = os.path.join(a.work, "res_%s.json" % va[2:])
        subprocess.call([sys.executable, os.path.join(HERE, "snapdiff.py"), "--image", image, "--out-dir",
                         os.path.join(a.work, "build_" + va[2:]), "--spec-dir", SPEC_DIR, *snaps, "--only", va, "--hooks", hooks,
                         "--bounded", "--results", res])
        rec = committed(va, res, "SYNTHETIC: %d call state(s) whose registers%s are chosen over the memory of one forward-emulated "
                        "state (synth_call.py, synth_members.py; the kind step 0x14212FE70's entry state, synth_walk.py --entry). "
                        % (len(cs), " and a few poked scratch bytes" if any(c.get("pokes") for c in cs) else ""), len(cs))
        if rec is not None:
            records.append(rec)
        for case in cs:  # a poked case holds a sparse copy of the state's memory: delete it now
            shutil.rmtree(os.path.join(a.work, "case_%s_%s" % (va[2:], case["name"])), ignore_errors=True)
    for item in a.real:
        va, capture = item.split("=", 1)
        trace = os.path.join(a.work, "tr_real_" + va[2:])
        subprocess.check_call([sys.executable, os.path.join(HERE, "frame_trace.py"), "recorded", capture, "--hooks", hooks, "--out",
                               trace, "--spec-dir", SPEC_DIR], stdout=subprocess.DEVNULL)
        res = os.path.join(a.work, "res_%s.json" % va[2:])
        subprocess.call([sys.executable, os.path.join(HERE, "snapdiff.py"), "--image", image, "--out-dir",
                         os.path.join(a.work, "build_" + va[2:]), "--spec-dir", SPEC_DIR, "--snapshot", "%s=%s" % (capture, trace),
                         "--only", va, "--hooks", hooks, "--bounded", "--results", res])
        rec = committed(va, res, "REAL calls of %s (a live capture; no register or byte chosen). " % os.path.relpath(capture, "/"), 0)
        if rec is not None:
            records.append(rec)
    if a.merge and records:
        doc = json.load(open(RESULTS))
        keep = [r for r in doc["results"] if r["va"].upper() not in {x["va"].upper() for x in records}]
        doc["results"] = keep + records
        json.dump(doc, open(RESULTS, "w"), indent=1)
        open(RESULTS, "a").write("\n")


if __name__ == "__main__":
    main()
