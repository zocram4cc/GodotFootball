#!/usr/bin/env python3
"""Instruction forms xlate must implement or refuse loudly (ProcCamera3, 31-10-26).

The FixDemo camera constructor's registry find hashes its key with PMOVSXBD / PMULLD (0x141B70060). xlate emitted both as a
silent `/*UNHANDLED*/(void)0;`, the hash came out wrong, the lookup missed and the replay aborted at the not-found
callback. Now:
  1. each form (PMOVSXBD, PMULLD, PUNPCKLBW, PUNPCKLWD, CVTDQ2PD, MAXSD, MINSD, BSWAP) is translated and the compiled C++
     gives the SDM result on known inputs (checked against Python, never against the translation);
  2. an instruction with no semantics (x87 FLDZ) is a trap in the emitted code, never a no-op;
  3. a cache hint (PREFETCHT0) stays a no-op, marked IGNORED;
  4. gen_translated.silent_unhandled_sites refuses a layer member that still holds the UNHANDLED marker.
Usage: GF_DIFF_PYTHON=<venv python> python3 test_sse_forms.py
"""
import os
import re
import struct
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
LAYER = os.path.join(REPO, "src", "pes", "translated")
sys.path.insert(0, HERE)
import pdata  # noqa: E402
pdata.KNOWN = frozenset()  # the synthetic image uses no ledger
import gen_translated  # noqa: E402
import xlate  # noqa: E402

BASE = pdata.IMAGE_BASE
FN, UNWIND = BASE + 0x1000, 0x600
CODE = bytes.fromhex(
    "660f3821d8"    # pmovsxbd xmm3, xmm0
    "660f3840e1"    # pmulld   xmm4, xmm1
    "660f60e9"      # punpcklbw xmm5, xmm1
    "660f61f1"      # punpcklwd xmm6, xmm1
    "f30fe6f8"      # cvtdq2pd xmm7, xmm0
    "f2440f5fc1"    # maxsd    xmm8, xmm1
    "f2440f5dc9"    # minsd    xmm9, xmm1
    "0fc8"          # bswap eax
    "480fc9"        # bswap rcx
    "c3")
HINT = bytes.fromhex("0f1808c3")  # prefetcht0 [rax] ; ret
X87 = bytes.fromhex("d9eec3")     # fldz ; ret


def build_image(path, code):
    img = bytearray(0x40000)
    struct.pack_into("<I", img, 0x3C, 0x80)
    img[0x80:0x84] = b"PE\0\0"
    struct.pack_into("<HHIIIHH", img, 0x84, 0x8664, 0, 0, 0, 0, 240, 0)
    struct.pack_into("<II", img, 0x98 + 112 + 24, 0x400, 12)
    struct.pack_into("<III", img, 0x400, FN - BASE, FN - BASE + len(code), UNWIND)
    img[UNWIND:UNWIND + 4] = bytes([0x01, 0, 0, 0])
    for off in range(0x1000, 0x1200):
        img[off] = 0xCC
    img[FN - BASE:FN - BASE + len(code)] = code
    with open(path, "wb") as f:
        f.write(img)


_DIR = tempfile.TemporaryDirectory()


def translate(code):
    path = os.path.join(_DIR.name, "img%d.bin" % len(code))
    build_image(path, code)
    src, _ = xlate.translate_one(path, "", "0x%X" % FN, {"0x%X" % FN})
    return "\n".join(src)


def lanes(x):
    return [(x >> (32 * i)) & 0xFFFFFFFF for i in range(4)]


def pack(l):
    return sum((v & 0xFFFFFFFF) << (32 * i) for i, v in enumerate(l))


DRIVER = r"""
#include "pes_state.hpp"
#include "pes_dispatch.hpp"
#include <cstdio>
#include <cstdlib>
PesDispatch pes_dispatch_lookup(uint64_t) { return {nullptr}; }
[[noreturn]] void pes_dispatch_missing(uint64_t) { std::abort(); }
void pes_fn_FN(PesState& s);
static void put(PesState& s, int i, unsigned long long lo, unsigned long long hi) { s.xmm[i][0] = lo; s.xmm[i][1] = hi; }
int main() {
  PesState s;
  put(s, 0, 0x8070F0FF80FF7F01ULL, 0x1111111122222222ULL);   // bytes 01 7f ff 80 / ff f0 70 80 ...
  put(s, 4, 0x0000000200000003ULL, 0x00000010FFFFFFFFULL);
  put(s, 5, 0x0706050403020100ULL, 0x0F0E0D0C0B0A0908ULL);
  put(s, 6, 0x0003000200010000ULL, 0x0007000600050004ULL);
  put(s, 8, 0x4000000000000000ULL, 0);                         // 2.0
  put(s, 9, 0x4000000000000000ULL, 0);                         // 2.0
  // xmm1: low double 3.0 (the maxsd/minsd source); its dwords are the PMULLD/PUNPCK sources
  put(s, 1, 0x4008000000000000ULL, 0x0000000900000003ULL);
  s.gpr[0] = 0x1122334455667788ULL;
  s.gpr[1] = 0x1122334455667788ULL;
  pes_fn_FN(s);
  for (int i : {3, 4, 5, 6, 7, 8, 9}) std::printf("%016llx %016llx\n", (unsigned long long)s.xmm[i][1], (unsigned long long)s.xmm[i][0]);
  std::printf("%016llx %016llx\n", (unsigned long long)s.gpr[0], (unsigned long long)s.gpr[1]);
}
"""


def run_compiled(text):
    d = _DIR.name
    with open(os.path.join(d, "fn.cpp"), "w") as f:
        f.write(text)
    with open(os.path.join(d, "main.cpp"), "w") as f:
        f.write(DRIVER.replace("pes_fn_FN", "pes_fn_%X" % FN))
    exe = os.path.join(d, "t")
    subprocess.run(["g++", "-std=c++17", "-O1", "-I", LAYER, "-o", exe, os.path.join(d, "fn.cpp"), os.path.join(d, "main.cpp")],
                   check=True, capture_output=True, text=True)
    return subprocess.run([exe], check=True, capture_output=True, text=True).stdout.split("\n")


def sx8(b):
    return (b - 256 if b > 127 else b) & 0xFFFFFFFF


class SseForms(unittest.TestCase):
    def test_every_form_gives_the_sdm_result(self):
        out = run_compiled(translate(CODE))
        got = [int(l.replace(" ", ""), 16) for l in out[:7]]
        x0, x1 = 0x1111111122222222_8070F0FF80FF7F01, 0x0000000900000003_4008000000000000
        # PMOVSXBD: the low four bytes of xmm0 (01 7f ff 80) sign-extended to dwords
        self.assertEqual(lanes(got[0]), [sx8(0x01), sx8(0x7F), sx8(0xFF), sx8(0x80)])
        # PMULLD: low 32 bits of each product; xmm4 lanes (3, 2, FFFFFFFF, 0x10) times xmm1 dwords
        d, s = lanes(0x00000010FFFFFFFF_0000000200000003), lanes(x1)
        self.assertEqual(lanes(got[1]), [(a * b) & 0xFFFFFFFF for a, b in zip(d, s)])
        # PUNPCKLBW: low 8 bytes of dest and source interleaved, dest byte first
        db = bytes(range(16))
        sb = struct.pack("<QQ", x1 & (2**64 - 1), x1 >> 64)
        want = b"".join(bytes([db[i], sb[i]]) for i in range(8))
        self.assertEqual(got[2].to_bytes(16, "little"), want)
        # PUNPCKLWD
        dw = struct.pack("<8H", 0, 1, 2, 3, 4, 5, 6, 7)
        want = b"".join(dw[2 * i:2 * i + 2] + sb[2 * i:2 * i + 2] for i in range(4))
        self.assertEqual(got[3].to_bytes(16, "little"), want)
        # CVTDQ2PD: the two low dwords of xmm0 as signed ints -> doubles
        lo, hi = struct.unpack("<ii", struct.pack("<Q", x0 & (2**64 - 1)))
        self.assertEqual(got[4].to_bytes(16, "little"), struct.pack("<dd", float(lo), float(hi)))
        # MAXSD / MINSD: low double only, the upper quad of the destination kept
        self.assertEqual(struct.unpack("<d", got[5].to_bytes(16, "little")[:8])[0], 3.0)
        self.assertEqual(struct.unpack("<d", got[6].to_bytes(16, "little")[:8])[0], 2.0)
        self.assertEqual(got[5] >> 64, 0)
        # BSWAP eax zero-extends; BSWAP rcx swaps all eight bytes
        rax, rcx = [int(v, 16) for v in out[7].split()]
        self.assertEqual(rax, 0x88776655)
        self.assertEqual(rcx, 0x8877665544332211)

    def test_an_instruction_without_semantics_is_a_trap_not_a_no_op(self):
        text = translate(X87)
        self.assertIn("/*UNHANDLED FLDZ", text)
        self.assertRegex(text, r"/\*UNHANDLED FLDZ[^\n]*\*/ ?__builtin_trap\(\);")
        self.assertNotIn("*/(void)0;", text)

    def test_a_cache_hint_stays_a_no_op(self):
        text = translate(HINT)
        self.assertIn("/*IGNORED PREFETCHT0", text)
        self.assertNotIn("UNHANDLED", text)

    def test_the_layer_refuses_a_member_with_the_marker(self):
        with self.assertRaises(SystemExit) as e:
            gen_translated.silent_unhandled_sites({"0x141B70060": {"raw": translate(X87)}})
        self.assertIn("FLDZ", str(e.exception))
        gen_translated.silent_unhandled_sites({"0x141B70060": {"raw": translate(HINT)}})
        gen_translated.silent_unhandled_sites({"0x140480030": {"raw": translate(X87)}})  # listed decode noise


if __name__ == "__main__":
    unittest.main()
