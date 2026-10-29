#!/usr/bin/env python3
"""A tail jump to a target the layer does not hold is never a silent return (NativeSeam, 29-10-26).

xlate emitted `/*OUT-OF-RANGE JMP t*/return;` for a `jmp t` past the function's own body when t was not in the
universe: the call chain ended there and the caller saw a normal return (14047BCA0 -> 14047BB10, the vector growth that
never allocated). Three rules, one test each, on a synthetic flat image (offset = VA - 0x140000000):
  1. xlate: an untranslated tail target is a loud abort naming the VA, never a bare return.
  2. driver.runnable_extra, given the universe, follows the spec function's tail jumps into the closure.
  3. gen_translated.silent_tail_sites names any in-code untranslated tail target that survives into the layer.
Usage: GF_DIFF_PYTHON=<venv python> python3 test_tail_jump_loud.py
"""
import os
import re
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import driver  # noqa: E402
import gen_translated  # noqa: E402
import xlate  # noqa: E402

BASE = 0x140000000
# The tail targets lie past FN's decode window: a nearer forward jmp is read as a local jump (dead code after it).
FN = 0x140001000
TAIL, TAIL2 = FN + xlate.DECODE_WINDOW + 0x1000, FN + 2 * xlate.DECODE_WINDOW + 0x1000
CALLEE = TAIL2 + 0x100


def rel32(op, at, target):
    return bytes([op]) + ((target - (at + 5)) & 0xFFFFFFFF).to_bytes(4, "little")


class Image:
    """FN: xor eax,eax; jmp .+2 (local); call CALLEE; jmp TAIL.  TAIL: xor ecx,ecx; jmp TAIL2.  TAIL2: xor edx,edx; ret.
    CALLEE (an out-of-universe call, the spec's out_of_set_callees decide it, not the tail walk): ret."""

    def __enter__(self):
        buf = bytearray(CALLEE - BASE + xlate.DECODE_WINDOW)
        code = {FN: b"\x31\xC0\xEB\x00" + rel32(0xE8, FN + 4, CALLEE) + rel32(0xE9, FN + 9, TAIL),
                TAIL: b"\x31\xC9" + rel32(0xE9, TAIL + 2, TAIL2), CALLEE: b"\xC3",
                TAIL2: b"\x31\xD2\xC3"}
        for va, b in code.items():
            buf[va - BASE:va - BASE + len(b)] = b
        self.f = tempfile.NamedTemporaryFile(suffix=".bin")
        self.f.write(buf)
        self.f.flush()
        return self.f.name

    def __exit__(self, *a):
        self.f.close()


def translate(image, pilot):
    src, _ = xlate.translate_one(image, "", "0x%X" % FN, {"0x%X" % v for v in pilot})
    return "\n".join(src)


class TailJumpLoud(unittest.TestCase):
    def test_untranslated_tail_target_aborts_loudly_naming_the_va(self):
        with Image() as img:
            text = translate(img, [FN])
        self.assertIn("pes_dispatch_missing(0x%X)" % TAIL, text)
        self.assertIsNone(re.search(r"OUT-OF-RANGE JMP 0x%X\*/\s*return;" % TAIL, text))

    def test_translated_tail_target_is_called_then_returns(self):
        with Image() as img:
            text = translate(img, [FN, TAIL])
        self.assertIn("{ pes_fn_%X(s); return; }" % TAIL, text)
        self.assertNotIn("pes_dispatch_missing(0x%X)" % TAIL, text)  # (the out-of-universe CALLEE stays an abort)

    def test_closure_walk_pulls_in_the_tail_chain_of_the_spec_function(self):
        spec = {"va": "0x%X" % FN, "harness": {}}
        with Image() as img:
            extra, blocked = driver.runnable_extra(spec, img, universe={"0x%X" % FN})
            self.assertEqual(sorted(extra), ["0x%X" % TAIL, "0x%X" % TAIL2])  # the target, and its own tail target
            self.assertEqual(blocked, {})
            # without a universe the function's own tails are not walked (the callers that have none are unchanged)
            self.assertEqual(driver.runnable_extra(spec, img)[0], [])

    def test_a_tail_target_already_in_the_universe_is_not_extra(self):
        spec = {"va": "0x%X" % FN, "harness": {}}
        with Image() as img:
            extra, _ = driver.runnable_extra(spec, img, universe={"0x%X" % v for v in (FN, TAIL, TAIL2)})
        self.assertEqual(extra, [])

    def test_a_universe_members_own_tail_target_is_pulled_in_too(self):
        spec = {"va": "0x%X" % FN, "harness": {}}
        with Image() as img:
            extra, _ = driver.runnable_extra(spec, img, universe={"0x%X" % v for v in (FN, TAIL)})
        self.assertEqual(extra, ["0x%X" % TAIL2])

    def test_layer_check_names_an_in_code_untranslated_tail_site(self):
        members = {"0x%X" % FN: {"raw": "x\n  /*OUT-OF-RANGE JMP 0x%X*/ pes_dispatch_missing(0x%X);\n" % (TAIL, TAIL)}}
        with self.assertRaises(SystemExit) as cm:
            gen_translated.silent_tail_sites(members)
        self.assertIn("0x%X" % FN, str(cm.exception))
        self.assertIn("0x%X" % TAIL, str(cm.exception))

    def test_layer_check_tolerates_decode_noise_outside_the_code_range(self):
        members = {"0x%X" % FN: {"raw": "/*OUT-OF-RANGE JMP 0x15B23CA30*/ pes_dispatch_missing(0x15B23CA30);\n"}}
        gen_translated.silent_tail_sites(members)  # a jump after a ret into a non-image address is dead decode noise


if __name__ == "__main__":
    unittest.main(verbosity=2)
