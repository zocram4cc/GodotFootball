#!/usr/bin/env python3
"""test_synth.py: synth.py writes the same states from the same spec, and each state differs from its fixture only where the
spec says.

The fixture is a small rendezvous-layout capture built here: two pages, and a call whose rcx points at them and whose stack
holds a pointer to them. One spec perturbs a f32 at a register address, a u32 behind a pointer chain, the xmm3 argument, the
rdx argument and a hex poke. Checks: the same spec and seed twice give byte-identical index.json, call.bin and mem.bin; another
seed gives other states; every index says SYNTHETIC; the perturbed bytes and registers differ from the fixture; nothing else in
mem.bin, call.bin or index.json differs; a spec with an unknown key is refused. Exit 0 green, 1 red. Needs no unicorn.
"""
import json
import os
import shutil
import struct
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
SYNTH = os.path.join(HERE, "synth.py")
PAGE = 0x1000
BASE = 0x20000
STACK = BASE + PAGE
CALL = struct.Struct("<49Q")  # frame_trace's call record: 16 GPRs, 16 XMM (lo, hi), the return address
GPR = ("rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15")
RET_SLOT = 48
XMM3_LO = 16 + 2 * 3  # the call record's slot of xmm3's low qword
PTR_OFF = 0x10        # the stack word that points at BASE
BALL_OFF, CHAIN_OFF, POKE_OFF = 0x8, 0x20, 0x100  # file offsets (mapping at BASE, offset 0) of the three perturbed windows
POKE = bytes.fromhex("0a0b0c0d")
SPEC = {
    "label": "fixture",
    "seed": 7,
    "count": 4,
    "fields": [
        {"name": "ball_x", "at": "reg:rcx+0x8", "type": "f32", "range": [-30.0, 30.0]},
        {"name": "chain_u32", "at": "reg:rsp+0x10+0x20", "type": "u32", "values": [0x11111111, 0x22222222, 0x33333333]},
        {"name": "xmm3", "at": "xmm3", "type": "f32", "range": [0.5, 4.0]},
        {"name": "rdx", "at": "rdx", "type": "u64", "range": [1, 1000]},
        {"name": "poke", "at": "va:0x20100", "type": "hex", "values": ["0a0b0c0d"]},
    ],
}


def fail(msg):
    print("FAIL: " + msg)
    sys.exit(1)


def make_fixture(d):
    page0 = bytes(range(256)) * (PAGE // 256)
    stack = bytearray(bytes(range(256)) * (PAGE // 256))
    stack[PTR_OFF:PTR_OFF + 8] = struct.pack("<Q", BASE)
    with open(os.path.join(d, "mem.bin"), "wb") as f:
        f.write(page0 + bytes(stack))
    vals = [0] * 49
    vals[GPR.index("rcx")] = BASE
    vals[GPR.index("rsp")] = STACK
    vals[RET_SLOT] = 0x140001234
    with open(os.path.join(d, "call.bin"), "wb") as f:
        f.write(CALL.pack(*vals))
    index = {"label": "fixture", "rv": {"entry_va": 0x140001000, "hook": "fixture", "teb": 0, "flags": 0x202, "mxcsr": 0x1F80},
             "mappings": [{"va": BASE, "size": 2 * PAGE, "perm": "rw-p", "name": "", "off": 0, "deferred": False,
                           "device": False}],
             "holes": []}
    with open(os.path.join(d, "index.json"), "w") as f:
        json.dump(index, f)


def run_synth(src, out, spec, seed):
    spec = dict(spec, seed=seed)
    spec_path = out + ".spec.json"
    with open(spec_path, "w") as f:
        json.dump(spec, f)
    return subprocess.run([sys.executable, SYNTH, src, out, "--spec", spec_path], capture_output=True, text=True)


def read(path):
    with open(path, "rb") as f:
        return f.read()


def states_of(out):
    return sorted(n for n in os.listdir(out) if os.path.isdir(os.path.join(out, n)))


def main():
    work = tempfile.mkdtemp(prefix="synth_test_", dir=os.getcwd())
    try:
        src = os.path.join(work, "fixture")
        os.makedirs(src)
        make_fixture(src)
        src_mem, src_call = read(os.path.join(src, "mem.bin")), read(os.path.join(src, "call.bin"))
        src_index = json.load(open(os.path.join(src, "index.json")))

        a, b = os.path.join(work, "a"), os.path.join(work, "b")
        for out in (a, b):
            r = run_synth(src, out, SPEC, 7)
            if r.returncode != 0:
                fail("synth.py refused the fixture spec: %s" % (r.stderr or r.stdout).strip()[-300:])
        names = states_of(a)
        if len(names) != SPEC["count"] or names != states_of(b):
            fail("states %s vs %s, want %d" % (names, states_of(b), SPEC["count"]))

        for n in names:
            for leaf in ("index.json", "call.bin", "mem.bin"):
                if read(os.path.join(a, n, leaf)) != read(os.path.join(b, n, leaf)):
                    fail("%s/%s differs between two runs of the same seed" % (n, leaf))
        print("ok: %d states byte-identical across two runs of seed 7" % len(names))

        c = os.path.join(work, "c")
        r = run_synth(src, c, SPEC, 8)
        if r.returncode != 0:
            fail("synth.py refused seed 8: %s" % r.stderr.strip()[-300:])
        if all(read(os.path.join(a, n, "mem.bin")) == read(os.path.join(c, n, "mem.bin")) for n in names):
            fail("seed 8 gives the same states as seed 7: the seed is not used")
        print("ok: seed 8 gives other states")

        for n in names:
            idx = json.load(open(os.path.join(a, n, "index.json")))
            if idx.get("synthetic", {}).get("label") != "SYNTHETIC":
                fail("%s index is not labelled SYNTHETIC" % n)
            mem = read(os.path.join(a, n, "mem.bin"))
            if len(mem) != len(src_mem):
                fail("%s mem.bin size %d, source %d" % (n, len(mem), len(src_mem)))
            diff = [i for i in range(len(mem)) if mem[i] != src_mem[i]]
            windows = [(BALL_OFF, 4), (CHAIN_OFF, 4), (POKE_OFF, 4)]
            stray = [i for i in diff if not any(lo <= i < lo + k for lo, k in windows)]
            if stray:
                fail("%s mem.bin changed outside the perturbed fields at %#x" % (n, stray[0]))
            for lo, k in windows:
                if mem[lo:lo + k] == src_mem[lo:lo + k]:
                    fail("%s perturbed bytes at %#x equal the fixture" % (n, lo))
            if mem[POKE_OFF:POKE_OFF + 4] != POKE:
                fail("%s hex poke not written" % n)
            call = read(os.path.join(a, n, "call.bin"))
            vals, want = CALL.unpack(call), CALL.unpack(src_call)
            changed = {i for i in range(len(vals)) if vals[i] != want[i]}
            if changed - {XMM3_LO, GPR.index("rdx")} or not {XMM3_LO, GPR.index("rdx")} <= changed:
                fail("%s call.bin changed slots %s, want xmm3 and rdx only" % (n, sorted(changed)))
            rest_idx = {k: v for k, v in idx.items() if k not in ("label", "synthetic")}
            if rest_idx != {k: v for k, v in src_index.items() if k != "label"}:
                fail("%s index.json changed beyond label and synthetic" % n)
        print("ok: perturbed fields differ; nothing else in mem.bin, call.bin or index.json differs")

        retarget = {"label": "rt", "seed": 1, "as": "0x140002000", "cases": [{"name": "rt", "edx": 5}],
                    "fields": [{"name": "edx", "at": "edx", "type": "u32"}]}
        r = run_synth(src, os.path.join(work, "rt"), retarget, 1)
        if r.returncode != 0:
            fail("synth.py refused the retarget spec: %s" % r.stderr.strip()[-300:])
        rt = os.path.join(work, "rt", "rt")
        rt_idx = json.load(open(os.path.join(rt, "index.json")))
        rt_vals = CALL.unpack(read(os.path.join(rt, "call.bin")))
        if rt_idx["rv"]["entry_va"] != 0x140002000 or rt_vals[GPR.index("rdx")] != 5:
            fail("retarget: entry %#x, rdx %#x; want 0x140002000 and 5" % (rt_idx["rv"]["entry_va"], rt_vals[GPR.index("rdx")]))
        if read(os.path.join(rt, "mem.bin")) != src_mem:
            fail("a retarget with no memory write changed mem.bin")
        print("ok: as retargets the entry; a 32-bit register writes zero-extended")

        # A static spec moves the call's return address ("ret") and writes memory; a captured all-zero page (a fresh heap page the
        # allocator touches) must stay data in the written state, not become a hole (a hole reads as "page not captured").
        zsrc = os.path.join(work, "zsrc")
        os.makedirs(zsrc)
        make_fixture(zsrc)
        with open(os.path.join(zsrc, "mem.bin"), "r+b") as f:
            f.seek(PAGE)
            f.write(bytes(PAGE))
        new_ret = 0x140005678
        ret_spec = {"label": "rz", "seed": 1, "as": "0x140002000", "cases": [{"name": "rz"}],
                    "fields": [{"name": "ret", "at": "ret", "type": "u64", "values": [new_ret]},
                               {"name": "poke", "at": "va:0x20100", "type": "hex", "values": ["0a0b0c0d"]}]}
        r = run_synth(zsrc, os.path.join(work, "rz"), ret_spec, 1)
        if r.returncode != 0:
            fail("synth.py refused the ret spec: %s" % (r.stderr or r.stdout).strip()[-300:])
        rz = os.path.join(work, "rz", "rz")
        rz_vals, base_vals = CALL.unpack(read(os.path.join(rz, "call.bin"))), CALL.unpack(src_call)
        if rz_vals[RET_SLOT] != new_ret or [v for i, v in enumerate(rz_vals) if i != RET_SLOT] != [v for i, v in enumerate(base_vals) if i != RET_SLOT]:
            fail("ret field: slot %#x, want %#x with every other slot unchanged" % (rz_vals[RET_SLOT], new_ret))
        fd = os.open(os.path.join(rz, "mem.bin"), os.O_RDONLY)
        try:
            try:
                at = os.lseek(fd, PAGE, os.SEEK_DATA)
            except OSError:  # ENXIO: no data at or after PAGE, the page is a hole
                at = None
            if at != PAGE:
                fail("a captured all-zero page became a hole in the written state")
        finally:
            os.close(fd)
        print("ok: ret moves the return address; a captured zero page stays data")
        bad = dict(SPEC, bogus=1)
        bad_path = os.path.join(work, "bad.json")
        with open(bad_path, "w") as f:
            json.dump(bad, f)
        r = subprocess.run([sys.executable, SYNTH, src, os.path.join(work, "bad"), "--spec", bad_path],
                           capture_output=True, text=True)
        if r.returncode == 0:
            fail("a spec with an unknown key was accepted")
        print("ok: unknown spec key refused")
        print("PASS test_synth")
    finally:
        shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    main()
