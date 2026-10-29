#!/usr/bin/env python3
"""The logf_xmm0 / powf_xmm0 models (stubs.json 1415A17C8 / 1415A17CE) against C99 Annex F and PES's own ucrtbase.

Three things, on the real C++ model (pes_stubs.hpp, built by g++) and the harness's Python model (run.py):
  1. Special values (zero, negative, NaN, infinity, denormal, exact powers, overflow, underflow) equal the C99 Annex F
     results, which is the contract of the ucrtbase PES calls.
  2. The C++ and Python models agree on every input: the translator and the harness model one function.
  3. When PES_UCRT_SNAP names a full snapshot of the running game (else skipped), its ucrtbase logf/powf machine
     skipped), its logf/powf machine code is run in unicorn on the same inputs: special values are bit-identical, an
     ordinary argument is within one ulp, and the count of one-ulp differences is printed (the documented gap).
Usage: [PES_UCRT_SNAP=<snapshot dir>] GF_DIFF_PYTHON=<venv python> python3 test_libm_models.py
"""
import math
import os
import random
import struct
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
sys.path.insert(0, os.path.join(REPO, "tools", "dev", "diffharness"))
import run  # noqa: E402  (the harness's Python models)

INF, NAN = math.inf, math.nan
SNAP = os.environ.get("PES_UCRT_SNAP", "")  # a full snapshot of PES running (rendezvous live2/pid1694737/seq009)
LOGF_THUNK_SLOT_VALUE, POWF_THUNK_SLOT_VALUE = 0x6FFFFEC7D5E0, 0x6FFFFEC7F090  # image slots 0x142530278 / 0x142530290
MIN_DENORMAL = 2.0 ** -149
DRIVER = r"""
#include "pes_stubs.hpp"
#include <cstdio>
#include <cstring>
int main() {
  char op; unsigned a, b;
  while (std::scanf(" %c %x %x", &op, &a, &b) == 3) {
    float x, y; std::memcpy(&x, &a, 4); std::memcpy(&y, &b, 4);
    float r = op == 'L' ? pes_model_logf(x) : pes_model_powf(x, y);
    unsigned o; std::memcpy(&o, &r, 4); std::printf("%08x\n", o);
  }
}
"""


def bits(f):
    return struct.unpack("<I", struct.pack("<f", f))[0]


def flt(b):
    return struct.unpack("<f", struct.pack("<I", b))[0]


def is_nan_bits(b):
    return (b & 0x7F800000) == 0x7F800000 and (b & 0x7FFFFF) != 0


def ulp_gap(a, b):
    """Distance in floats between two finite or infinite bit patterns (sign-magnitude order)."""
    def key(v):
        return -(v & 0x7FFFFFFF) if v >> 31 else v
    return abs(key(a) - key(b))


# C99 Annex F.9.4.3 (log) and F.9.7.4 (pow). expected: a float (compared by bits, so the sign of zero counts) or NAN.
LOG_ROWS = [(0.0, -INF), (-0.0, -INF), (1.0, 0.0), (-1.0, NAN), (-INF, NAN), (-1e-45, NAN), (INF, INF), (NAN, NAN)]
POW_ROWS = [
    (0.0, -3.0, INF), (-0.0, -3.0, -INF), (0.0, -2.0, INF), (-0.0, -2.0, INF), (0.0, -INF, INF), (-0.0, -INF, INF),
    (0.0, 3.0, 0.0), (-0.0, 3.0, -0.0), (0.0, 2.0, 0.0), (-0.0, 2.0, 0.0), (0.0, INF, 0.0), (-0.0, INF, 0.0),
    (-1.0, INF, 1.0), (-1.0, -INF, 1.0), (1.0, NAN, 1.0), (1.0, 5.0, 1.0), (1.0, -INF, 1.0),
    (NAN, 0.0, 1.0), (5.0, 0.0, 1.0), (5.0, -0.0, 1.0), (INF, 0.0, 1.0), (-INF, 0.0, 1.0),
    (-2.0, 0.5, NAN), (-8.0, 0.3333333432674408, NAN), (-1e-45, 1.5, NAN),
    (0.5, -INF, INF), (2.0, -INF, 0.0), (0.5, INF, 0.0), (2.0, INF, INF),
    (-INF, -3.0, -0.0), (-INF, -2.0, 0.0), (-INF, 3.0, -INF), (-INF, 2.0, INF), (INF, -1.0, 0.0), (INF, 1.0, INF),
    (NAN, 2.0, NAN), (2.0, NAN, NAN), (NAN, NAN, NAN),
    (2.0, 128.0, INF), (-2.0, 129.0, -INF), (2.0, -150.0, 0.0), (2.0, -149.0, MIN_DENORMAL), (-2.0, -149.0, -MIN_DENORMAL),
    (MIN_DENORMAL, 1.0, MIN_DENORMAL), (2.0, 10.0, 1024.0), (-2.0, 3.0, -8.0), (-3.0, 2.0, 9.0),
]
SPECIAL_LOG = [x for x, _ in LOG_ROWS] + [MIN_DENORMAL, 5e-39, 2.0 ** -126, 1.17549435e-38, 3.4028235e38]
SPECIAL_POW = [(x, y) for x, y, _ in POW_ROWS] + [(MIN_DENORMAL, 0.5), (5e-39, 2.0), (MIN_DENORMAL, 1.5), (1e-20, 2.0)]


def sample():
    """Ordinary arguments: deterministic, spread over the ranges PES's callers use plus full bit patterns."""
    rnd = random.Random(1)
    logs = [flt(rnd.getrandbits(32)) for _ in range(4000)] + [rnd.uniform(0, 100) for _ in range(2000)] \
        + [rnd.uniform(0.5, 2.0) for _ in range(2000)]
    pows = [(rnd.uniform(0, 10), rnd.uniform(-10, 10)) for _ in range(2000)] \
        + [(rnd.uniform(0, 2), rnd.uniform(0, 1)) for _ in range(1500)] \
        + [(flt(rnd.getrandbits(32)), flt(rnd.getrandbits(32))) for _ in range(1500)] \
        + [(10 ** rnd.uniform(-30, -1), rnd.uniform(1, 3)) for _ in range(1500)]  # includes subnormal results
    return [flt(bits(x)) for x in logs], [(flt(bits(x)), flt(bits(y))) for x, y in pows]


class Cpp:
    """The real pes_model_* from pes_stubs.hpp, built on first use."""
    exe = None
    _dir = None

    @classmethod
    def build(cls):
        cls._dir = tempfile.TemporaryDirectory()
        src = os.path.join(cls._dir.name, "m.cpp")
        with open(src, "w") as f:
            f.write(DRIVER)
        cls.exe = os.path.join(cls._dir.name, "m")
        subprocess.run(["g++", "-O1", "-std=c++17", "-I", os.path.join(REPO, "src", "pes", "translated"), src,
                        "-o", cls.exe], check=True)

    @classmethod
    def run(cls, rows):
        """rows: [("L", x, 0.0) | ("P", x, y)] -> [result bits]."""
        if cls.exe is None:
            cls.build()
        text = "".join("%s %x %x\n" % (op, bits(x), bits(y)) for op, x, y in rows)
        out = subprocess.run([cls.exe], input=text, capture_output=True, text=True, check=True).stdout.split()
        return [int(v, 16) for v in out]


def python_model(op, x, y):
    return run.logf_f32_bits(bits(x)) if op == "L" else run.powf_f32_bits(bits(x), bits(y))


def expected_ok(got, want):
    return is_nan_bits(got) if want != want else got == bits(want)


class Models(unittest.TestCase):

    def test_logf_special_values_follow_annex_f(self):
        got = Cpp.run([("L", x, 0.0) for x, _ in LOG_ROWS])
        for (x, want), g in zip(LOG_ROWS, got):
            self.assertTrue(expected_ok(g, want), "logf(%r) = %#x, want %r" % (x, g, want))

    def test_logf_of_denormals_and_extremes_is_the_logarithm_to_one_ulp(self):
        xs = [MIN_DENORMAL, 5e-39, 2.0 ** -126, 3.4028235e38]
        for x, g in zip(xs, Cpp.run([("L", x, 0.0) for x in xs])):
            self.assertLessEqual(ulp_gap(g, bits(math.log(x))), 1, "logf(%r) = %#x" % (x, g))

    def test_powf_special_values_follow_annex_f(self):
        got = Cpp.run([("P", x, y) for x, y, _ in POW_ROWS])
        for (x, y, want), g in zip(POW_ROWS, got):
            self.assertTrue(expected_ok(g, want), "powf(%r, %r) = %#x, want %r" % (x, y, g, want))

    def test_cpp_and_harness_models_agree_on_every_input(self):
        logs, pows = sample()
        rows = [("L", x, 0.0) for x in logs + SPECIAL_LOG] + [("P", x, y) for x, y in pows + SPECIAL_POW]
        for (op, x, y), g in zip(rows, Cpp.run(rows)):
            self.assertEqual(g, python_model(op, x, y), "%s(%r, %r)" % (op, x, y))


@unittest.skipUnless(os.path.isdir(SNAP), "no ucrtbase snapshot (PES_UCRT_SNAP)")
class AgainstUcrtbase(unittest.TestCase):
    """The snapshot's ucrtbase logf/powf code, run in unicorn: the PES behaviour the models stand for."""

    @classmethod
    def setUpClass(cls):
        sys.path.insert(0, HERE)
        import full_snapshot as fs
        from unicorn import Uc, UC_ARCH_X86, UC_MODE_64, UC_HOOK_MEM_FETCH_UNMAPPED
        from unicorn.x86_const import UC_X86_REG_RSP
        snap = fs.SnapMem(SNAP, image="")
        lo, hi = 0x6FFFFEC60000, 0x6FFFFED50000  # the module: header, code, read-only data, writable data
        cls.mu = mu = Uc(UC_ARCH_X86, UC_MODE_64)
        for m in snap.maps:
            if lo <= m["va"] < hi:
                mu.mem_map(m["va"], m["size"])
                for off in range(0, m["size"], 0x1000):
                    page = snap.read(m["va"] + off, 0x1000)
                    if page:
                        mu.mem_write(m["va"] + off, bytes(page))

        def external(uc, access, addr, size, value, ud):
            # A call out of the module (the errno setter): map a page that returns 0.
            base = addr & ~0xFFF
            uc.mem_map(base, 0x1000)
            uc.mem_write(base, b"\x90" * 0xFFD + b"\x31\xc0\xc3")
            return True

        mu.hook_add(UC_HOOK_MEM_FETCH_UNMAPPED, external)
        mu.mem_map(0x10000000, 0x10000)
        mu.mem_map(0x20000000, 0x1000)
        mu.mem_write(0x20000000, b"\xf4")
        cls.rsp = UC_X86_REG_RSP

    def call(self, fn, *args):
        from unicorn.x86_const import UC_X86_REG_XMM0, UC_X86_REG_RIP
        mu = self.mu
        mu.mem_write(0x10008000, struct.pack("<Q", 0x20000000))
        mu.reg_write(self.rsp, 0x10008000)
        for i, a in enumerate(args):
            mu.reg_write(UC_X86_REG_XMM0 + i, bits(a))
        mu.emu_start(fn, 0x20000000, count=100000)
        return mu.reg_read(UC_X86_REG_XMM0) & 0xFFFFFFFF

    def test_special_values_are_bit_identical_and_ordinary_ones_within_one_ulp(self):
        logs, pows = sample()
        rows = [("L", x, 0.0) for x in logs + SPECIAL_LOG] + [("P", x, y) for x, y in pows + SPECIAL_POW]
        special = {("L", bits(x), 0) for x in SPECIAL_LOG} | {("P", bits(x), bits(y)) for x, y in SPECIAL_POW}
        diffs = {"L": 0, "P": 0}
        count = {"L": 0, "P": 0}
        for (op, x, y), g in zip(rows, Cpp.run(rows)):
            u = self.call(LOGF_THUNK_SLOT_VALUE, x) if op == "L" else self.call(POWF_THUNK_SLOT_VALUE, x, y)
            count[op] += 1
            # NaN results too are compared by bits (the sign the invalid-operation path gives).
            if u != g:
                diffs[op] += 1
                self.assertNotIn((op, bits(x), bits(y) if op == "P" else 0), special,
                                 "%s(%r, %r): model %#x, ucrtbase %#x" % (op, x, y, g, u))
                self.assertLessEqual(ulp_gap(u, g), 1, "%s(%r, %r): model %#x, ucrtbase %#x" % (op, x, y, g, u))
        print("\nucrtbase vs model: logf %d of %d inputs differ by one ulp, powf %d of %d" %
              (diffs["L"], count["L"], diffs["P"], count["P"]))


if __name__ == "__main__":
    unittest.main(verbosity=2)
