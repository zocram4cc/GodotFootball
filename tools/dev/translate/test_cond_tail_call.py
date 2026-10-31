#!/usr/bin/env python3
"""A conditional tail call (`jnz other_function`) is a call when taken, never a bare trap (ProcCamera, 31-10-26).

PES 0x141EAE0F0 is `mov rcx,[rcx+8] ; test rcx,rcx ; jnz 0x1423D43D0 ; ret`: the jump leaves the function, so xlate had no
label for it and emitted `if (cond) __builtin_trap();` with no marker. The bounded closure walk (snapdiff) never found the
target, the translated binary died with SIGILL on the first replay that took the branch, and the harness ran on (the
camera engine's replays of live2/pid3234533). Three rules, on a synthetic PE image (pattern of test_tail_jump_loud.py):
  1. a target in the build is called, then the function returns (a condition that is false falls through);
  2. a target outside the build is a loud abort naming the VA, with the OUT-OF-RANGE JCC marker snapdiff's closure walk reads;
  3. closure.py counts the Jcc target as a callee of the function (the crawl pulls it in).
Usage: GF_DIFF_PYTHON=<venv python> python3 test_cond_tail_call.py
"""
import os
import re
import struct
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import pdata  # noqa: E402
pdata.KNOWN = frozenset()  # the synthetic image uses no ledger
import closure  # noqa: E402
import xlate  # noqa: E402

BASE = pdata.IMAGE_BASE
FN, TARGET = BASE + 0x1000, BASE + 0x1000 + 2 * pdata.MAX_RANGE  # pdata.extent absorbs a Jcc target only within MAX_RANGE
UNWIND = 0x600


def build_image(path):
    """FN: test ecx,ecx ; jnz TARGET ; ret.  TARGET: xor edx,edx ; ret.  Two .pdata entries: a jump between them is a tail call."""
    img = bytearray(0x40000)
    struct.pack_into("<I", img, 0x3C, 0x80)
    img[0x80:0x84] = b"PE\0\0"
    struct.pack_into("<HHIIIHH", img, 0x84, 0x8664, 0, 0, 0, 0, 240, 0)
    entries = [(FN, 9), (TARGET, 3)]
    struct.pack_into("<II", img, 0x98 + 112 + 24, 0x400, 12 * len(entries))
    for i, (va, n) in enumerate(entries):  # one UNWIND_INFO each: entries that share one are chained chunks of one function
        struct.pack_into("<III", img, 0x400 + 12 * i, va - BASE, va - BASE + n, UNWIND + 8 * i)
        img[UNWIND + 8 * i:UNWIND + 8 * i + 4] = bytes([0x01, 0, 0, 0])
    for off in range(0x1000, 0x1200):
        img[off] = 0xCC
    jnz = b"\x0f\x85" + ((TARGET - (FN + 2 + 6)) & 0xFFFFFFFF).to_bytes(4, "little")
    code = {FN: b"\x85\xc9" + jnz + b"\xc3", TARGET: b"\x31\xd2\xc3"}
    for va, b in code.items():
        img[va - BASE:va - BASE + len(b)] = b
    with open(path, "wb") as f:
        f.write(img)


_DIR = tempfile.TemporaryDirectory()
_IMAGE = os.path.join(_DIR.name, "img.bin")
build_image(_IMAGE)


def translate(pilot):
    src, _ = xlate.translate_one(_IMAGE, "", "0x%X" % FN, {"0x%X" % v for v in pilot})
    return "\n".join(src)


class CondTailCall(unittest.TestCase):
    def test_target_in_the_build_is_called_then_the_function_returns(self):
        text = translate([FN, TARGET])
        self.assertRegex(text, r'if \(s\.cond\("nz"\)\) \{ pes_fn_%X\(s\); return; \}' % TARGET)
        self.assertNotIn("__builtin_trap", text)

    def test_target_outside_the_build_aborts_loudly_with_the_marker(self):
        text = translate([FN])
        self.assertRegex(text, r'if \(s\.cond\("nz"\)\) \{ /\*OUT-OF-RANGE JCC 0x%X\*/ pes_dispatch_missing\(0x%X\); return; \}'
                         % (TARGET, TARGET))
        self.assertNotIn("__builtin_trap", text)

    def test_closure_crawl_counts_the_jcc_target_as_a_callee(self):
        with open(_IMAGE, "rb") as img:
            row = closure.ClosurePdata(img).decode_row(FN, "t")
        self.assertEqual(row["callees"], {"%08X" % TARGET: "tail"})


if __name__ == "__main__":
    unittest.main()
