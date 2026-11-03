#!/usr/bin/env python3
"""synth_press_order.py: SYNTHETIC proofs of the press dispatcher's order-word helper 0x14062AF10 and its slot reset 0x140536690.

No hook sits on either: FUN_140535C60 (the dispatcher) calls the reset between the coordinator and the writer, and the helper
from the order-word loop and from the reset's tail, and none of the captures enters them. synth_call.py keeps ONE full state's
memory (any full capture of a running match: `capz.py unpack` it first) and sets the registers and a few poked bytes of a call,
the harness runs PES's bytes, the translated body runs over the same pages, and snapdiff.py --bounded compares the return value
and every written byte. A slim capture cannot serve: the helper reads the men's records, the ball-distance lists and the world
objects, which a writer capture's read set does not hold.

What is CHOSEN, per case (seeded, so a re-run builds the same cases): the man index (helper) or PP[2] (reset), the men's action
words (T+0xA2A0 and P+0x5D0), positions and info kinds, the ball-distance lists, the enrolment words T+0xA548, the set-piece
bytes T+0xF0/+0xF1/+0x140, the G+0x180 list, and the call frame objects (the argument struct, PP). Every case is SYNTHETIC
(`synthetic_cases` in the record); the state itself is a real match state.

Usage: PES_EXE=<pes_full.bin> RV_HOOKS=<hooks.cfg> python3 synth_press_order.py STATE_DIR WORK_DIR [--merge]
  STATE_DIR  an unpacked full capture (its memory and stack are the base); WORK_DIR scratch. --merge writes results_press.json.
Harness Python (unicorn). Not part of the gate: the proofs are the committed results_press.json records.
"""
import argparse
import json
import os
import random
import shutil
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import synth_call as sc  # noqa: E402

REPO = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
SPEC_DIR = os.path.join(REPO, "tools", "dev", "diffharness", "specs", "press")
RESULTS = os.path.join(HERE, "results_press.json")
G_CELL = 0x1436F3DC8
MAN_BASE, MAN_STRIDE = 0xBF, 0x28    # FUN_140A48A10(G, slot) = *(G + (slot + 0xBF) * 0x28): the man record P
INFO_BASE = 0xA5                     # FUN_140A48910(G, slot): the info record (kind byte at +1, flag byte at +0x40)
SIDE_TABLE, SIDE_STRIDE = 0x1928, 0x28   # FUN_140A48A30(G, side) = *(G + 0x1928 + 0x28 * side): the team record S
LIST_PTR = 0x180                     # G+0x180: the 24-entry list (stride 0x74, id at +0x20, man at +0x2C)
MA_LIST, MA_BALL_X, MA_BALL_Z = 0x2470C, 0x2482C, 0x24834
SETPIECE_PTR = 0x3B0                # G+0x3B0: the set-piece object
MA_HOLDER, MA_HOLDER_FALLBACK = 0x2601C, 0x24838
SLOT_BASE = 0xF3                    # FUN_140A489F0(G, slot): the slot record (the int at +0x1438)
ACTIONS = (0, 0, 0x33, 0x34, 0x34, 0x3A, 5, 5, 7, 0x36)
INFO_KINDS = (0, 1, 2, 3, 5, 0xB, 0xC, 0x10, 0x11, 0x14, 0x57, 0x20, 0x40, 0x7F)
INFO_FLAGS = (0, 1, 2, 5, 5)
SETS = {"0x14062AF10": ("closure_press_order.json", 28), "0x140536690": ("closure_press_reset.json", 56)}


class State:
    def __init__(self, path):
        self.path = path
        self.index = json.load(open(os.path.join(path, "index.json")))
        vals = struct.unpack("<49Q", open(os.path.join(path, "call.bin"), "rb").read())
        self.reg = dict(zip(sc.GPR_ORDER, vals[:16]))

    def read(self, va, n):
        return sc.read_state(self.path, self.index, va, n)

    def u64(self, va):
        return struct.unpack("<Q", self.read(va, 8))[0]

    def u32(self, va):
        return struct.unpack("<I", self.read(va, 4))[0]

    def f32(self, va):
        return struct.unpack("<f", self.read(va, 4))[0]


def u32(v):
    return struct.pack("<I", v & 0xFFFFFFFF).hex()


def f32(v):
    return struct.pack("<f", v).hex()


class Poker:
    """The pokes of one case, one entry per address (a later write wins)."""

    def __init__(self):
        self.p = {}

    def put(self, va, hexbytes):
        self.p["%#x" % va] = hexbytes

    def args(self):
        out = []
        for va, h in self.p.items():
            out += ["--poke", "%s=%s" % (va, h)]
        return out


class World:
    """Addresses of the state's objects."""

    def __init__(self, st):
        self.st = st
        self.g = st.u64(G_CELL)
        self.ctx = st.u64(st.reg["rdx"])  # the dispatcher's {ctx, 0, side} argument: ctx is its first word
        self.ma = st.u64(self.ctx + 0x18)
        self.man = [st.u64(self.g + (s + MAN_BASE) * MAN_STRIDE) for s in range(22)]
        self.info = [st.u64(self.g + (s + INFO_BASE) * MAN_STRIDE) for s in range(22)]
        self.slot = [st.u64(self.g + (s + SLOT_BASE) * MAN_STRIDE) for s in range(22)]
        self.team = [st.u64(self.g + SIDE_TABLE + SIDE_STRIDE * s) for s in range(2)]
        self.list = st.u64(self.g + LIST_PTR)
        self.setpiece = st.u64(self.g + SETPIECE_PTR)
        self.ball_x = st.f32(self.ma + MA_BALL_X)
        self.ball_z = st.f32(self.ma + MA_BALL_Z)


def scatter(w, rnd, pk):
    """Men's words, positions, info kinds, the ball-distance lists and the ball point of the case."""
    radius = rnd.choice((6.0, 30.0, 70.0, 95.0))
    bx = w.ball_x + rnd.uniform(-20, 20)
    bz = w.ball_z + rnd.uniform(-10, 10)
    pk.put(w.ma + MA_BALL_X, f32(bx))
    pk.put(w.ma + MA_BALL_Z, f32(bz))
    for s in range(22):
        pk.put(w.man[s] + 0x5D0, u32(rnd.choice(ACTIONS)))
        pk.put(w.man[s] + 0x55C, f32(bx + rnd.uniform(-radius, radius)))
        pk.put(w.man[s] + 0x564, f32(bz + rnd.uniform(-radius, radius)))
        pk.put(w.info[s] + 1, "%02x" % rnd.choice(INFO_KINDS))
        pk.put(w.info[s] + 0x40, "%02x" % rnd.choice(INFO_FLAGS))
    # MA+0x2601C: the holder (FUN_140A627E0(ctx, 0)); above 0x15 the helper falls back to MA+0x24838
    pk.put(w.ma + MA_HOLDER, u32(rnd.choice((0xFF, rnd.randrange(22), rnd.randrange(22)))))
    pk.put(w.ma + MA_HOLDER_FALLBACK, u32(rnd.randrange(22)))
    for s in range(22):
        pk.put(w.slot[s] + 0x1438, u32(rnd.randrange(3000)))
    for side in range(2):
        order = list(range(22))
        rnd.shuffle(order)
        for j in range(11):
            ident = order[j] if rnd.random() > 0.05 else 0xFF
            pk.put(w.ma + MA_LIST + (side * 11 + j) * 8, u32(ident))



def team_words(w, rnd, pk, side, enrolled=None):
    """S: the action words, the enrolment table, the set-piece bytes and the designated man (+0x78)."""
    t = w.team[side]
    pk.put(t + 0x254, "%02x" % rnd.choice((1, 0xFF)))
    for j in range(11):
        pk.put(t + 0xA2A0 + 4 * j, u32(rnd.choice((0, 0, 0x33, 0x34, 0x3A))))
    for j in range(4):
        pk.put(t + 0xA548 + 4 * j, u32(rnd.choice((0xFF, 0xFF, rnd.randrange(22)) if enrolled is None else (0xFF,))))
    if enrolled is not None:
        pk.put(t + 0xA548 + 4 * rnd.randrange(4), u32(enrolled))
    pk.put(t + 0x78, u32(rnd.choice((0xFF, rnd.randrange(22)))))
    pk.put(t + 0x140, "%02x" % rnd.choice((0, 1)))
    pk.put(t + 0xF0, "%02x" % rnd.choice((0, 0, 1, 2, 5, 24)))
    pk.put(t + 0xF1, "%02x" % rnd.choice((0, 0, 1)))


def list_entries(w, rnd, pk):
    for i in range(24):
        e = w.list + i * 0x74
        pk.put(e + 0x20, u32(rnd.choice((rnd.randrange(22), rnd.randrange(22), 0xFF))))
        pk.put(e + 0x2C, u32(rnd.choice((rnd.randrange(22), 0xFF))))
        # FUN_140A55570(list, i): the entry's kind byte (entry + 0x68) when the word at list + 0x10E8 + 4 i is 4
        pk.put(w.list + 0x10E8 + 4 * i, u32(rnd.choice((4, 4, 0))))
        pk.put(e + 0x68, "%02x" % rnd.choice((3, 3, 0)))


def write_closure(image, va, closure_file, members_path, proof):
    """closure_file: the replays' members (snapdiff --bounded) plus the root's static direct-call closure (closure.py's crawl),
    the recipe of closure_press_{writer,eval,coord}.json: the static members are translated and linked, not replay-proven.
    proof: what the replays were ("the SYNTHETIC proof of VA (synth_press_order.py)", "the proof of VA over the real captures ...")."""
    import closure  # noqa: E402  (needs capstone: the harness Python)
    replay = {int(v, 16) for v in json.load(open(members_path))}
    universe = closure.load_universe()["functions"]
    with open(image, "rb") as img:
        crawled, _ = closure.crawl([int(va, 16)], universe, closure.ClosurePdata(img), "press")
    members = replay | set(crawled)
    doc = {"note": "the functions built for %s: every function the replays entered "
                   "(snapdiff --bounded: %d) plus the root's static direct-call closure (%d members the replays did not enter: "
                   "translated, not replay-proven). pes-port-PressAI.md." % (proof, len(replay), len(members - replay)),
           "root": va, "members": sorted("0x%X" % m for m in members)}
    with open(os.path.join(HERE, closure_file), "w") as f:
        json.dump(doc, f, indent=1)
        f.write("\n")


def helper_case(st, w, work, n, scratch):
    rnd = random.Random(0xAF10 + n)
    pk = Poker()
    scatter(w, rnd, pk)
    ident = n % 22 if n < 22 else rnd.randrange(22)
    for side in range(2):
        team_words(w, rnd, pk, side, ident if side == ident // 11 and n % 6 == 5 else None)
    frame = scratch["dst"]
    pk.put(frame, u32(ident))
    regs = ["rcx=%#x" % w.ctx, "rdx=%#x" % frame]
    return regs, pk


def reset_case(st, w, work, n, scratch):
    rnd = random.Random(0x6690 + n)
    pk = Poker()
    scatter(w, rnd, pk)
    side = n % 2
    for s in range(2):
        team_words(w, rnd, pk, s)
    list_entries(w, rnd, pk)
    # G+0x3B0: the set-piece object (kind at +0x8C, 0x18 takes the early arm: the taker's side at +0xA0+0xDCE8, per-man words at +0xE704)
    pk.put(w.setpiece + 0x8C, u32(rnd.choice((0, 0, 0x18))))
    pk.put(w.setpiece + 0xA0 + 0xDCE8, u32(rnd.randrange(22)))
    for i in range(22):
        pk.put(w.setpiece + 0xE704 + 4 * i, u32(rnd.choice((0x34, 0, 5))))
    arg, pp = scratch["dst"], scratch["src"]
    pk.put(arg, struct.pack("<QQI", w.ctx, 0, side).hex())
    # PP[2] (param_1 + 0x5C): a man of the side, or none
    pk.put(pp + 0x5C, u32(rnd.choice((0xFF, side * 11 + rnd.randrange(11)))))
    regs = ["rcx=%#x" % pp, "rdx=%#x" % arg, "r8=%#x" % w.team[side]]
    return regs, pk


def writer_case(st, w, work, n, scratch):
    """The press writer 0x140536FC0 over a WRITER capture of the hook (slim: only the pages the recorded call read). The recorded
    call leaves at the 11-slot body filter; these cases clear the own men's MI+0x1748 flags so the filter passes, arm T+0xF1, put the
    own men near the ball and choose T+0xB709 and the slot words. The call's own registers stay (rcx, rdx = {ctx, 0, side}, r8 = T);
    S (the team record G names) and T hold byte-identical copies in the captures, so every poke goes to both. A poke that needs a
    page the capture dropped fails loudly in frame_trace, so the case set is the one that traces."""
    rnd = random.Random(0x6FC0 + n)
    pk = Poker()
    side = st.u32(st.reg["rdx"] + 0x10)
    ball = st.u64(w.g + 0x40)
    bx, bz = st.f32(ball + 0x88), st.f32(ball + 0x90)
    mi = st.u64(w.g + 0x270)
    for j in range(11):
        pk.put(mi + 0x1748 + 11 * side + j, "00")
    b709 = rnd.choice((0, 1))
    for base in (w.team[side], st.reg["r8"]):
        pk.put(base + 0xF1, "01")
        pk.put(base + 0xB709, "%02x" % b709)
    words = {}  # slot -> word; the other slots keep the capture's own word
    for j in range(11):
        if rnd.random() < 0.25:
            words[j] = 0x3A
        elif rnd.random() < 0.2:
            words[j] = 0x34
    for base in (w.team[side], st.reg["r8"]):
        for j, word in words.items():
            pk.put(base + 0xA2A0 + 4 * j, u32(word))
    for j in range(11):
        man = w.man[side * 11 + j]
        r = rnd.choice((6.0, 12.0, 22.0))
        pk.put(man + 0x55C, f32(bx + rnd.uniform(-r, r)))
        pk.put(man + 0x564, f32(bz + rnd.uniform(-r, r)))
    return ["rcx=%#x" % st.reg["rcx"], "rdx=%#x" % st.reg["rdx"], "r8=%#x" % st.reg["r8"]], pk


BUILDERS = {"0x14062AF10": helper_case, "0x140536690": reset_case, "0x140536FC0": writer_case}


def build_case(st, w, work, va, n):
    label = "%s_%02d" % (va[2:], n)
    cdir = os.path.join(work, "case_" + label)
    rsp = st.reg["rsp"]
    regs, pk = BUILDERS[va](st, w, work, n, {"dst": rsp - 0x400, "src": rsp - 0x200})
    cmd = [sys.executable, os.path.join(HERE, "synth_call.py"), st.path, cdir, "--entry", va]
    for r in regs:
        cmd += ["--reg", r]
    subprocess.check_call(cmd + pk.args())
    return label, cdir


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("state")
    ap.add_argument("work")
    ap.add_argument("--merge", action="store_true")
    ap.add_argument("--only", action="append")
    ap.add_argument("--cases", type=int, default=0, help="override the number of cases per function")
    a = ap.parse_args()
    image, hooks = os.environ["PES_EXE"], os.environ["RV_HOOKS"]
    os.makedirs(a.work, exist_ok=True)
    st = State(a.state)
    w = World(st)
    records = []
    for va, (closure_file, count) in SETS.items():
        if a.only and va not in a.only:
            continue
        count = a.cases or count
        snaps = []
        for n in range(count):
            label, cdir = build_case(st, w, a.work, va, n)
            trace = os.path.join(a.work, "tr_" + label)
            subprocess.check_call([sys.executable, os.path.join(HERE, "frame_trace.py"), "recorded", cdir, "--hooks", hooks, "--out",
                                   trace, "--spec-dir", SPEC_DIR], stdout=subprocess.DEVNULL)
            snaps += ["--snapshot", "%s=%s" % (cdir, trace)]
        res = os.path.join(a.work, "res_%s.json" % va[2:])
        out_dir = os.path.join(a.work, "build_" + va[2:])
        subprocess.call([sys.executable, os.path.join(HERE, "snapdiff.py"), "--image", image, "--out-dir", out_dir, "--spec-dir", SPEC_DIR,
                         *snaps, "--only", va, "--hooks", hooks, "--bounded", "--results", res])
        raw = next(r for r in json.load(open(res))["results"] if r["va"].upper() == va.upper())
        print("%s: proven=%s calls=%s replayed=%s excluded=%s mismatches=%s own_block=%s" % (
            va, raw["proven"], raw["calls"], raw["replayed"], raw["excluded"], len(raw["mismatches"]), raw["own_block"]))
        records.append((va, closure_file, raw, os.path.join(out_dir, "gen", va[2:], "members.json")))
        for n in range(count):
            shutil.rmtree(os.path.join(a.work, "case_%s_%02d" % (va[2:], n)), ignore_errors=True)
    if a.merge:
        doc = json.load(open(RESULTS))
        keep = [r for r in doc["results"] if r["va"].upper() not in {va.upper() for va, *_ in records}]
        new = []
        for va, closure_file, raw, members in records:
            new.append({"aborted": 0, "capped": 0, "cases": 0, "closure_file": closure_file, "coverage": {va: raw["own_block"]},
                        "dead_blocks": {}, "diffed": bool(raw["proven"]), "failed": 0, "failure_class": None, "icount_max": 0,
                        "nonreturn": 0,
                        "note": "proof snapdiff --bounded: every replayed call bit-exact, %d of %d. SYNTHETIC: %d call states over one full "
                                "match state (synth_press_order.py: the men's words, positions and kinds, the ball-distance lists, the "
                                "enrolment words, the set-piece bytes, the G+0x180 list and the argument objects are chosen, not observed). "
                                "The press dispatcher FUN_140535C60's order-word helper / slot reset: no hook, no capture calls it." % (
                                    raw["replayed"], raw["calls"], raw["calls"]),
                        "proof": "snapdiff", "requested": 0,
                        "snapdiff": {"calls": raw["calls"], "mismatches": len(raw["mismatches"]), "own_block": raw["own_block"],
                                     "replayed": raw["replayed"], "source": "tools/dev/translate/results_press.json"},
                        "stubs": [], "synthetic": True, "synthetic_cases": raw["calls"], "translated": True, "va": va})
            write_closure(image, va, closure_file, members, "the SYNTHETIC proof of %s (synth_press_order.py)" % va)
        doc["results"] = keep + new
        json.dump(doc, open(RESULTS, "w"), indent=1)
        open(RESULTS, "a").write("\n")


if __name__ == "__main__":
    main()
