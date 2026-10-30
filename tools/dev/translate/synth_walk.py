#!/usr/bin/env python3
"""synth_walk.py: a carrier-walk (0x14210F110) call state PES never captured, built from a real one.

No live walk call reached the Clear block 0x14210F4F2: the Clear unit 0x142109EB0 (kind 1) answers type 5 only while the
danger byte imageA+0x31C1 is set AND its unit is enabled that frame (0x142111610), and the walk runs only for a carrier
that decides (BP+0x18 & 0x10). So this tool takes a capture of a real call that runs inside the unit step 0x142110D30 before
the answer list 0x142110370 and the walk: a danger_builder_2119390 capture (the builder's return) or a carrier_clear_2109EB0
capture (the Clear unit's return). It runs PES's own bytes from that call's return until the walk's entry and writes that
state in rendezvous format (index.json, call.bin, a sparse-copied mem.bin overwritten with the emulated pages).
frame_trace.py recorded / snapdiff.py replay it like a live capture. The source must be a FULL capture: a slimmed one holds
only the pages its own call read, and the forward run faults on the rest.

--patch-clear (a danger_builder source, where the Clear unit never ran): the kind-1 answer record is set to type 5, the Clear
unit's output (layout EndClear.md §1). SYNTHETIC, label it as such wherever it is cited: everything the walk reads is PES's
state except that type word. A carrier_clear source needs no patch (the unit wrote the record) and is real state. In both, the
libm calls of the forward run (cosf, sinf, ...) use Python float32 models, not Wine's ucrtbase, so lane lists the forward run
builds can differ from a live run's in the last ulp. The walk itself calls no libm.

--entry VA [--hook-name NAME] (default: the walk): stop at another function's entry inside the same unit step instead, e.g. the
turn-reference builder 0x14211D710 or the proposal writer 0x1421102D0 that run before the walk. The state is then a real
(forward-emulated) entry state of that function; only the walk (--entry omitted) may take --patch-clear.

Usage: PES_EXE=<pes_full.bin> python3 synth_walk.py CAPTURE_DIR OUT_DIR [--hooks hooks.cfg] [--patch-clear] [--entry VA --hook-name NAME]   (harness Python: unicorn)
"""
import argparse
import json
import math
import os
import shutil
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import frame_trace as ft  # noqa: E402
import stubs as stub_table  # noqa: E402
import unicorn.x86_const as xc  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402

WALK = 0x14210F110
ANSWER_LIST = 0x3E20     # BP+0x3E20: the priority list (u32 count, kinds), ranks at +0x68, answer records at +0x84 (0x1C each)
ANSWER_RECORD = 0x84
ANSWER_STRIDE = 0x1C
KIND_CLEAR = 1
TYPE_CLEAR = 5
# ucrtbase's float classifier, which the builder's closure calls: FP_NAN 2, FP_INFINITE 1, FP_ZERO 0, FP_SUBNORMAL -2, else -1.
FDTEST = ("8B01 25FFFFFF7F 7427 3D0000807F 730D 3D00008000 721C B8FFFFFFFF C3 3D0000807F 7406 B802000000 C3 B801000000 C3 "
          "31C0 C3 B8FEFFFFFF C3").replace(" ", "")
LIBM1 = {"sinf": math.sin, "cosf": math.cos, "tanf": math.tan, "atanf": math.atan, "acosf": math.acos, "asinf": math.asin,
         "logf": math.log, "expf": math.exp, "floorf": math.floor, "ceilf": math.ceil}
LIBM2 = {"atan2f": math.atan2, "powf": math.pow, "fmodf": math.fmod}
NOOP1 = ("SetEvent", "ResetEvent", "EnterCriticalSection", "LeaveCriticalSection")  # return TRUE; the replay is one thread
ZERO1 = ("_crt_atexit", "atexit", "_onexit")  # a function-local static's registration: return 0 (success), run nothing


def f32(bits):
    return struct.unpack("<f", struct.pack("<I", bits & 0xFFFFFFFF))[0]


def bits32(f):
    try:
        return struct.unpack("<I", struct.pack("<f", f))[0]
    except OverflowError:
        return 0x7F800000 if f > 0 else 0xFF800000


def xmm_low(uc, n):
    v = uc.reg_read(xc.UC_X86_REG_XMM0 + n)
    return (int.from_bytes(v, "little") if isinstance(v, (bytes, bytearray)) else v) & 0xFFFFFFFF


def patch_imports():
    """The replay harness stops at an unmodelled OS export; give the float libm and the lock calls these models."""
    real_models, real_intr = ft.load_import_models, ft.LazyMap._intr

    def models(path=None):
        m = real_models()
        m[("ucrtbase.dll", "_fdtest")] = bytes.fromhex(FDTEST)
        return m

    def intr(self, uc, intno, data):
        rip = uc.reg_read(xc.UC_X86_REG_RIP)
        _, name, _ = self.sm.export_of(rip)
        if name in LIBM1 or name in LIBM2 or name in NOOP1 or name in ZERO1:
            if name in NOOP1 or name in ZERO1:
                uc.reg_write(xc.UC_X86_REG_RAX, 1 if name in NOOP1 else 0)
            else:
                a, b = f32(xmm_low(uc, 0)), f32(xmm_low(uc, 1))
                try:
                    r = LIBM1[name](a) if name in LIBM1 else LIBM2[name](a, b)
                except (ValueError, ZeroDivisionError, OverflowError):
                    r = float("nan")
                uc.reg_write(xc.UC_X86_REG_XMM0, bits32(r))
            rsp = uc.reg_read(xc.UC_X86_REG_RSP)
            uc.reg_write(xc.UC_X86_REG_RIP, struct.unpack("<Q", bytes(uc.mem_read(rsp, 8)))[0])
            uc.reg_write(xc.UC_X86_REG_RSP, rsp + 8)
            return
        return real_intr(self, uc, intno, data)

    ft.load_import_models, ft.LazyMap._intr = models, intr


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("src")
    ap.add_argument("out")
    ap.add_argument("--hooks", default=None, help="rv.dll config (its displaced bytes replace the hook patches)")
    ap.add_argument("--budget", type=int, default=20_000_000)
    ap.add_argument("--patch-clear", action="store_true",
                    help="write type 5 into the kind-1 answer record (a danger_builder source: the Clear unit never ran); "
                         "off for a carrier_clear_2109EB0 source, whose unit wrote the record itself")
    ap.add_argument("--entry", type=lambda s: int(s, 16), default=WALK, help="stop at this function's entry (default: the walk)")
    ap.add_argument("--hook-name", default="carrier_walk_210F110", help="rv.hook of the written state")
    a = ap.parse_args()
    patch_imports()
    orig = ft.hook_bytes(a.hooks) if a.hooks else None
    call = ft.load_recorded_call(a.src)
    trace = {"out": a.out + ".trace", "impure": ft.load_impure(os.path.join(ft.DIFF, "specs", "impure")),
             "stubs": ft.stubs_for_run(stub_table.load())}
    os.makedirs(trace["out"], exist_ok=True)
    uc, err = ft.run_recorded(a.src, orig=orig, trace=trace)
    if err:
        sys.exit("the recorded builder call did not return: %s" % err)

    reached = []

    def on_code(u, address, size, data):
        if address == a.entry:
            bp = u.reg_read(xc.UC_X86_REG_RCX)
            if a.patch_clear and a.entry == WALK:
                u.mem_write(bp + ANSWER_LIST + ANSWER_RECORD + ANSWER_STRIDE * KIND_CLEAR, struct.pack("<I", TYPE_CLEAR))
            reached.append(True)
            u.emu_stop()

    uc._ret_sentinel = 0xDEAD0000  # the run continues past the builder's return address
    uc.hook_add(UC_HOOK_CODE, on_code)
    uc.emu_start(call["ret"], 0, count=a.budget)
    if not reached:
        sys.exit("the walk was not reached from this capture: rip %#x, %s" % (uc.reg_read(xc.UC_X86_REG_RIP), getattr(uc, "_bad", None)))

    index = json.load(open(os.path.join(a.src, "index.json")))
    rv = index["rv"]
    rv.update(hook=a.hook_name, entry_va=a.entry, flags=uc.reg_read(xc.UC_X86_REG_EFLAGS),
              mxcsr=uc.reg_read(xc.UC_X86_REG_MXCSR))
    index["label"] = os.path.basename(a.out.rstrip("/"))
    os.makedirs(a.out, exist_ok=True)
    json.dump(index, open(os.path.join(a.out, "index.json"), "w"))
    open(os.path.join(a.out, "call.bin"), "wb").write(ft.entry_record(uc))
    subprocess.check_call(["cp", "--sparse=always", os.path.join(a.src, "mem.bin"), os.path.join(a.out, "mem.bin")])
    with open(os.path.join(a.out, "mem.bin"), "r+b") as f:
        for begin, end, _ in uc.mem_regions():
            for m in index["mappings"]:
                if m["va"] <= begin < m["va"] + m["size"]:
                    stop = min(end + 1, m["va"] + m["size"])
                    f.seek(m["off"] + begin - m["va"])
                    f.write(bytes(uc.mem_read(begin, stop - begin)))
                    break
    shutil.rmtree(trace["out"], ignore_errors=True)
    print("wrote %s: walk entry rcx=%#x" % (a.out, uc.reg_read(xc.UC_X86_REG_RCX)))


if __name__ == "__main__":
    main()
