#!/usr/bin/env python3
"""synth_members.py: the per-function proofs of the dribble proposal chain's members that no captured or forward-emulated
state calls (DribbleProposal3/4's eleven small ones; 0x14063D2F0 is called by real angle captures and is proven on those).

Every case is SYNTHETIC: synth_call.py keeps the memory of one forward-emulated state (the kind step 0x14212FE70's entry state,
synth_walk.py --entry 0x14212FE70 from a danger_builder_2119390 capture) and sets the registers (and a few poked bytes in
scratch stack space) of a call of the member; the harness runs PES's bytes on it, the translated body runs over the same
pages, and snapdiff.py --bounded compares the return value and every written byte. The records are merged into
results_dribble.json marked `synthetic`, with the case count.

Usage: PES_EXE=<pes_full.bin> RV_HOOKS=<hooks.cfg> python3 synth_members.py STATE_DIR WORK_DIR [--only 0xVA ...] [--merge]
  STATE_DIR  synth_walk.py --entry 0x14212FE70 output (keep it whole: it is PES's memory)
  WORK_DIR   scratch: one small dir per case (mem.bin is a link, or a sparse copy where bytes are poked; deleted per case)
Harness Python (unicorn). Not part of the gate: the proofs are committed as results_dribble.json records.
"""
import argparse
import glob
import json
import os
import shutil
import math
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import driver as drv  # noqa: E402
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

    def read(self, va, n):
        return sc.read_state(self.path, self.index, va, n)

    def rd32(self, va):
        return struct.unpack("<I", self.read(va, 4))[0]

    def rdf(self, va):
        return struct.unpack("<f", self.read(va, 4))[0]


def arm_cases(st, regs, kind):
    """The arms of 0x142130EE0 that sit behind records it reads through pointers (the match-info pair at *(ctx+0xE0), the speed
    records at e0+0x24710, the compared man's anime record, the ball prediction at *(G+0x40), the roster row's skill word). Every
    address is derived from the forward-emulated state's own pointers; every value is CHOSEN (synthetic), from the function's
    own constants read out of the image: the geometry puts the compared man's path across (or clear of) the 5 m segment the
    function lays from the ball's first prediction sample along its direction argument."""
    g, rcx = st.u64(G_CELL), st.reg["rcx"]
    man, ctx, frame = st.u64(kind), st.u64(kind + 8), st.u64(kind + 0x20)
    e0, mi = st.u64(ctx + 0x18), st.u64(ctx + 0xE0)
    slot, other = st.rd32(man), st.rd32(rcx + 0x264)
    pred = lambda f: st.u64(g + 0x40) + (f * 5 + 0x11) * 8  # noqa: E731  FUN_140a6cf70
    anime = lambda s: st.u64(g + (s + ANIME_SLOT_BASE) * SLOT_STRIDE)  # noqa: E731
    rec = lambda s: e0 + 0x24710 + (st.read(e0 + 0x247BE + s, 1)[0] + (0xB if s >= 0xB else 0)) * 8  # noqa: E731
    p7 = mi + 0x180698  # FUN_140a3e190(mi, 0)
    assert st.rd32(mi + 0x1900) > 1
    head = st.rd32(mi + 0x1904) - 1
    p8 = mi + (head + 8 if head < 0 else head) * 800 + 20  # FUN_140a3e190(mi, 1), a record that is not kind 0x37
    assert st.rd32(p8 - 20) != 0x37
    side = int(slot > 10)
    team = st.u64(g + (side + 0xA3) * SLOT_STRIDE)  # FUN_140a489d0
    entry = st.rd32(team + 4 * (slot % 11 + 1))
    assert st.rd32(team) == side and entry < 0x28
    skills = team + entry * 0x128 + 0xA8 + 0x30 + 0x48  # FUN_140a3af90 + FUN_141b59e20: the u64 FUN_14063e960 shifts
    fps, half = st.rdf(0x14351AE54), st.rdf(0x142565600)
    steps = int(fps * half + half)  # the man's path ends this many prediction frames on
    amp, turn, quarter = st.rdf(0x14259222C), st.rdf(0x14259858C), st.rdf(0x14259BE5C)
    c1 = st.rdf(0x1425A9BE4)
    ball = struct.unpack("<3f", st.read(pred(0), 12))
    ang = st.rdf(regs["r9"])
    sc_ = lambda a: (math.sin(math.radians(a)), math.cos(math.radians(a)))  # noqa: E731  heading: dx = sin, dz = cos
    v3 = lambda *f: "".join(f32(x) for x in f)  # noqa: E731
    at = lambda va, off=0: ("%#x" % va) + ("+%#x" % off if off else "")  # noqa: E731

    def put(va, data, off=0):
        return (at(va, off), data)

    base = [put(frame, u32(0), 0x3C), put(rec(slot), v3(1.0)), put(rec(other), v3(2.0))]  # the main body: speed A <= C1, A <= B

    def lay(a):
        """The 5 m segment's offset from the ball for direction argument `a`: FUN_140a92c80 (x) and its +90 twin (z), replayed with
        float32 steps from the image's constants and its 513-entry sine table (a direction past a turn lands off the circle)."""
        f32r = lambda x: struct.unpack("<f", struct.pack("<f", x))[0]  # noqa: E731
        one, minus = st.rdf(0x14256AE1C), st.rdf(0x142588218)
        half_turn, scale = st.rdf(0x142598580), st.rdf(0x1426550E8)

        def sine(a):
            if 0.0 < a:
                if a >= turn:
                    a = f32r(a + st.rdf(0x142598598))
            else:
                a = f32r(a + turn)
            sign = one
            if a >= half_turn:
                sign, a = minus, f32r(a + st.rdf(0x1425DDFFC))
            if a >= quarter:
                a = f32r(half_turn - a)
            a, sign = f32r(f32r(a / quarter) * scale), f32r(sign * amp)
            i = int(a)
            if i > 0x1FF or i < 0:  # the unsigned compare: a negative index is past the table too
                return sign
            t = f32r(a - i)
            return f32r(f32r(f32r(f32r(one - t) * st.rdf(0x142655950 + 4 * i)) + f32r(t * st.rdf(0x142655950 + 4 * i + 4))) * sign)

        return sine(a), sine(f32r(a + quarter))

    def path(side_, dist, a=None):
        """Man pos/vel crossing the ball segment (laid for direction `a`) `dist` m from the ball, coming from side `side_`."""
        e = lay(ang if a is None else a)
        n = math.hypot(*e)
        d = (e[0] / n, e[1] / n)
        perp = (d[1], -d[0])
        hit = (ball[0] + dist * d[0], ball[2] + dist * d[1])
        h = 1.5
        p0 = (hit[0] + side_ * h * perp[0], hit[1] + side_ * h * perp[1])
        vel = (-2 * side_ * h * perp[0] / steps, -2 * side_ * h * perp[1] / steps)
        return [put(anime(other), v3(p0[0], ball[1], p0[1]), 0x55C), put(anime(other), v3(vel[0], 0.0, vel[1]), 0x574),
                put(pred(0), v3(*ball))]

    def still(side_):
        """Man standing 2 m from the ball, off the direction by 30 degrees to one side: no crossing (p3 == p4)."""
        d = sc_(ang + side_ * 30)
        return [put(anime(other), v3(ball[0] + 2 * d[0], 0.0, ball[2] + 2 * d[1]), 0x55C), put(anime(other), v3(0, 0, 0), 0x574)]

    def aim(d1, d2):
        """The two points the clamped angle differences read (rcx+0x280, rcx+0x28C): d1/d2 degrees off the direction."""
        out = []
        for off, dd in ((0x280, d1), (0x28C, d2)):
            a = sc_(ang + dd)
            out.append(put(rcx, v3(ball[0] + 10 * a[0], 0.0, ball[2] + 10 * a[1]), off))
        return out

    cs = {}
    # The match-info pair: each of the three gate tests failing, the compare failing four ways, and both records equal.
    for name, f in (("a_first", (6, 0, 0)), ("a_second", (6, 6, 0)), ("a_third", (6, 6, 0x11))):
        cs[name] = [put(p7, u32(f[0])), put(p7, u32(f[1]), 0x38), put(p7, u32(f[2]), 0x40)]
    gate = [put(p7, u32(6)), put(p7, u32(6), 0x38), put(p7, u32(0x11), 0x40)]
    same = [put(p8, st.read(p7, 0x44).hex()), *[put(p8, u32(v), o) for v, o in ((6, 0), (6, 0x38), (0x11, 0x40))]]
    cs["a_vec_differs"] = gate + [put(p8, u32(7), 0x40), put(p8, u32(1), 4), put(p8, u32(2), 8)]
    cs["a_equal"] = gate + same
    for k, (o, v) in {"a_kind_differs": (0, 7), "a_second_differs": (0x38, 7), "a_third_differs": (0x40, 0x12)}.items():
        cs[k] = gate + same + [put(p8, u32(v), o)]
    # The speed records: man's own over the threshold, and over the compared man's.
    near = [put(frame, u32(0), 0x3C), put(anime(other), v3(*ball), 0x55C), put(anime(other), v3(0, 0, 0), 0x574)]
    cs["rec_over_c1"] = near + [put(rec(slot), v3(c1 * 2)), put(rec(other), v3(c1 * 4))]
    cs["rec_over_other"] = near + [put(rec(slot), v3(c1 / 2)), put(rec(other), v3(c1 / 4))]
    # The man off the pitch slots (>0x15: no record read), and the left-hand side's slot.
    cs["slot_over"] = base + still(1) + aim(100, 100) + [put(man, u32(0x16))]
    cs["slot_left"] = base + still(1) + aim(100, 100) + [put(man, u32(5))]
    cs["own_still"] = base + still(1) + aim(100, 100) + [put(anime(slot), v3(0, 0, 0), 0x574)]  # his own velocity zero: no heading call
    # No crossing: the heading side (two signs) x the two clamped angle differences (each over / under a quarter turn).
    for s_ in (1, -1):
        for d1, d2 in ((100, 100), (100, 40), (40, 100), (40, 40)):
            cs["still%+d_%d_%d" % (s_, d1, d2)] = base + still(s_) + aim(d1, d2)
    # A crossing 3 m out (inside the 2..5 m window) from each side; the direction argument below 0, in range, and past a turn.
    for s_ in (1, -1):
        for tag, delta in (("", 0.0), ("_neg", -turn), ("_over", turn)):
            cs["cross%+d%s" % (s_, tag)] = base + path(s_, 3.0, ang + delta) + [put(regs["r9"], v3(ang + delta))] + aim(100, 100)
        cs["cross%+d_near" % s_] = base + path(s_, 1.0) + aim(100, 40)  # closer than the window: the clamped differences
    # The skill-word gate (bit 0x25) with +0x2B8 clear: from both branches; set but +0x2B8 raised; clear.
    bit = put(skills, u32(st.rd32(skills + 4) | 0x20), 4)
    for s_ in (1, -1):
        cs["skill%+d_cross" % s_] = base + path(s_, 3.0) + [bit]
        cs["skill%+d_still" % s_] = base + still(s_) + aim(100, 40) + [bit]
    cs["skill_but_busy"] = base + path(1, 3.0) + [bit, put(rcx, u32(1), 0x2B8)]
    # The closing arms (only the crossing branch reaches the +0x4C/offset tests): kind 3 or 7 at p7+0x38, the man's +0x4C below 2,
    # the heading difference at 60 or more, and the one that passes every test.
    heading = st.rdf(anime(slot) + 0x554)
    close = [put(man, u32(2), 0x4C), put(rcx, v3(heading + 10), 0x2A0)]
    cs["end_all_pass"] = base + path(1, 3.0) + close + [put(p7, u32(5), 0x38)]
    cs["end_all_pass_still"] = base + still(1) + aim(100, 40) + close + [put(p7, u32(5), 0x38)]
    for k in (3, 7):
        cs["end_kind%d" % k] = base + path(1, 3.0) + close + [put(p7, u32(k), 0x38)]
    cs["end_low_4c"] = base + path(1, 3.0) + close + [put(p7, u32(5), 0x38), put(man, u32(1), 0x4C)]
    cs["end_wide_heading"] = base + path(-1, 3.0) + close + [put(p7, u32(5), 0x38), put(rcx, v3(heading + 100), 0x2A0)]
    return [{"name": k, "regs": regs, "pokes": v} for k, v in cs.items()]


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
    regs = {"rcx": st.reg["rcx"], "rdx": "dst", "r8": kind, "r9": st.u64(kind + 0x38) + 0xC0}
    out["0x142130EE0"] = [{"name": "kind_step_args", "regs": regs}]
    # The gated arms: the man's slot (*(u32*)*kind, 0..0x15 in play, split at 0xB by the side test) and the kind object's u32 at
    # rcx+0x264 (above 0x15 returns the early answer; below it, selects the compared man).
    slot_at, r264 = "%#x" % st.u64(kind), "%#x+0x264" % st.reg["rcx"]
    out["0x142130EE0"] += [{"name": "slot%d" % s, "regs": regs, "pokes": [(slot_at, u32(s))]} for s in (0, 5, 11, 15)]
    out["0x142130EE0"] += [{"name": "r264_%d" % v, "regs": regs, "pokes": [(r264, u32(v))]} for v in (0x16, 0x20, 0, 11)]
    # The prediction frame the ball's position is read at (*(kind+0x20) + 0x3C): the distance test's arm.
    frame_at = "%#x" % (st.u64(kind + 0x20) + 0x3C)
    out["0x142130EE0"] += [{"name": "frame%d" % f, "regs": regs, "pokes": [(frame_at, u32(f))]} for f in (0, 3, 40, 255)]
    out["0x142130EE0"] += arm_cases(st, regs, kind)
    return out


def resolve(st, v):
    rsp = st.reg["rsp"]
    names = {"dst": rsp + DST, "src": rsp + SRC, "out": rsp + OUT}
    return names[v] if v in names else int(v, 0) if isinstance(v, str) else v  # a hex string is an absolute address


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
    ap.add_argument("--chunk", type=int, default=0,
                    help="cases per snapdiff run (each poked case is a sparse copy of the state's memory, ~2 GB): the runs' replays "
                         "and mismatches add up, the block coverage is the union of the harness's executed addresses")
    ap.add_argument("--merge", action="store_true", help="write the records into results_dribble.json")
    ap.add_argument("--real", action="append", default=[], metavar="VA=CAPTURE",
                    help="a function a real capture calls: its calls are replayed as they are (not synthetic)")
    a = ap.parse_args()
    image = os.environ["PES_EXE"]
    hooks = os.environ["RV_HOOKS"]  # the rv.dll config of the capture's launch (hook displaced bytes)
    st = State(a.state)
    records = []
    for va, cs in cases(st).items():
        if a.only and va not in a.only:
            continue
        step = a.chunk or len(cs)
        out_dir = os.path.join(a.work, "build_" + va[2:])
        raw_all, addrs = None, set()
        for first in range(0, len(cs), step):
            part = cs[first:first + step]
            snaps = []
            for case in part:
                label, cdir = build_case(st, a.work, va, case)
                trace = os.path.join(a.work, "tr_" + label)
                subprocess.check_call([sys.executable, os.path.join(HERE, "frame_trace.py"), "recorded", cdir, "--hooks", hooks,
                                       "--out", trace, "--spec-dir", SPEC_DIR], stdout=subprocess.DEVNULL)
                snaps += ["--snapshot", "%s=%s" % (cdir, trace)]
            res = os.path.join(a.work, "res_%s_%d.json" % (va[2:], first))
            subprocess.call([sys.executable, os.path.join(HERE, "snapdiff.py"), "--image", image, "--out-dir", out_dir,
                             "--spec-dir", SPEC_DIR, *snaps, "--only", va, "--hooks", hooks, "--bounded", "--results", res])
            raw = next((r for r in json.load(open(res))["results"] if r["va"].upper() == va.upper()), None) if os.path.exists(res) else None
            if raw is None:
                raw_all = None
                break
            if raw_all is None:
                raw_all = dict(raw, mismatches=[], calls=0, replayed=0, excluded=0, proven=True)
            for k in ("calls", "replayed", "excluded"):
                raw_all[k] += raw[k]
            raw_all["mismatches"] += raw["mismatches"]
            raw_all["proven"] = raw_all["proven"] and raw["proven"]
            for case in part:  # a poked case holds a sparse copy of the state's memory: delete it now
                shutil.rmtree(os.path.join(a.work, "case_%s_%s" % (va[2:], case["name"])), ignore_errors=True)
        for path in glob.glob(os.path.join(out_dir, "gen", va[2:], "snap_*", "addrs.json")):
            addrs |= set(json.load(open(path)))
        if raw_all is not None and addrs:
            cov = drv.block_coverage(image, [va], sorted(addrs)).get(va)
            raw_all["own_block"] = {"reached": cov[0], "total": cov[1], "fraction": round(cov[0] / cov[1], 4)}
        res = os.path.join(a.work, "res_%s.json" % va[2:])
        json.dump({"results": [raw_all] if raw_all else []}, open(res, "w"))
        rec = committed(va, res, "SYNTHETIC: %d call state(s) whose registers%s are chosen over the memory of one forward-emulated "
                        "state (synth_call.py, synth_members.py; the kind step 0x14212FE70's entry state, synth_walk.py --entry). "
                        % (len(cs), " and a few poked scratch bytes" if any(c.get("pokes") for c in cs) else ""), len(cs))
        if rec is not None:
            records.append(rec)
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
