#!/usr/bin/env python3
"""A signed guard bounds a table index from above only, so a baked window must not start at the table base (the round-6
defect: 0x140A6E320 reads ids -0x80..0x5D from the anime class table 0x1426484F0; a window from its base read 0 for the
negative ids, which PES reads from the image).

Fixtures are synthetic functions (the image is a fake whose bytes are a function of the address). Each one is laid out as
xlate sees it: the guard, the read on the guard's taken path, and the table base loaded by LEA.
  signed:   `cmp ecx,0x5e ; jl READ` (the 1426484F0 shape). The bake must decline, and the read must take the declared
            hull of ro_tables.json (0x1426482F0 covers ids -0x80..0x5D), with the image's bytes at every id.
  control:  `cmp ecx,0x5e ; ja ELSE` with the read on the fall-through (an unsigned bound of 0..0x5E): the bake keeps its
            window from the base, 95 entries. The signed rule must not touch it.
Behavioural assertions only: what the bake returns and which bytes its window holds. Mutation: drop the SIGNED_JCC skip in
_bounded_by_copy and _bounded_by_self; the signed fixture then bakes from the base and both checks fail.
Usage: GF_DIFF_PYTHON=<venv python> python3 test_bake_signed.py
"""
import os
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import xlate  # noqa: E402

BASE = xlate.BASE
FN = BASE + 0x1000
TABLE = 0x1426484F0  # the anime class table (the base the read indexes from)
SCALE = 4


class FakeImage:
    """The image as the translator reads it: VA-addressed bytes that are a function of the address."""
    size = 0x3000000  # covers 0x1426482F0.. (a VA offset of 0x2648xxx)

    def read(self, va, n):
        return bytes(((va + i) * 2654435761 >> 13) & 0xFF for i in range(n))

    def writable(self, va):
        return False


def function_lines(jcc, read_on_taken):
    """The instruction list xlate.emit_function takes: (addr, mnemonic, operands). The read is `mov eax, [rcx+rax*4]`
    with rax = movsxd ecx and rcx = the table base; the guard is `cmp ecx, 0x5e` then `jcc`."""
    guard_target = FN + 0x20
    if read_on_taken:  # jl READ: the read is at the jump target, the else arm falls through
        lines = [
            (FN + 0x00, "CMP", ["ecx", "0x5e"]),
            (FN + 0x03, jcc, ["0x%X" % guard_target]),
            (FN + 0x05, "XOR", ["eax", "eax"]),
            (FN + 0x07, "RET", []),
            (guard_target + 0x00, "MOVSXD", ["rax", "ecx"]),
            (guard_target + 0x04, "LEA", ["rcx", "[0x%X]" % TABLE]),
            (guard_target + 0x0B, "MOV", ["eax", "[rcx+rax*4]"]),
            (guard_target + 0x0E, "RET", []),
        ]
    else:  # ja ELSE: the read falls through, the else arm is at the target
        lines = [
            (FN + 0x00, "CMP", ["ecx", "0x5e"]),
            (FN + 0x03, jcc, ["0x%X" % guard_target]),
            (FN + 0x05, "MOVSXD", ["rax", "ecx"]),
            (FN + 0x09, "LEA", ["rcx", "[0x%X]" % TABLE]),
            (FN + 0x10, "MOV", ["eax", "[rcx+rax*4]"]),
            (FN + 0x13, "RET", []),
            (guard_target + 0x00, "XOR", ["eax", "eax"]),
            (guard_target + 0x02, "RET", []),
        ]
    return lines


def emitter_for(lines):
    em = xlate.Emitter(FakeImage(), "0x%X" % FN, set())
    labels = {int(ops[-1], 16) for _, m, ops in lines if m in ("JL", "JA") and ops}
    em.lines, em.labels, em.guards = lines, labels, xlate.find_guards(lines)
    em.defs = xlate.reaching_defs(lines, {})
    em.cur = next(i for i, (_, m, _) in enumerate(lines) if m == "MOV")
    return em


def image_bytes(va, n):
    return FakeImage().read(va, n)


class SignedGuardBake(unittest.TestCase):
    def setUp(self):
        self.em = emitter_for(function_lines("JL", read_on_taken=True))

    def test_the_bake_declines_a_window_from_the_base(self):
        # A signed guard admits negative indexes, so no window that starts at the table base may be baked from it.
        self.assertIsNone(self.em.bake_ro(xlate.parse_mem("rcx+rax*4"), 32, None))

    def test_the_read_takes_a_window_that_covers_every_admitted_id(self):
        self.em.mem_val("[rcx+rax*4]", 32, None)
        self.assertTrue(self.em.ro_tabs, "the declared hull must serve the read")
        # Ids -0x80..0x5D, the declared range of ro_tables.json 0x1426482F0 (CONFIRMED there; read by the translator).
        lo_id, hi_id = -0x80, 0x5D
        for lo, span in self.em.ro_tabs:
            self.assertLessEqual(lo, TABLE + SCALE * lo_id, "window must start at or below the lowest id")
            self.assertGreaterEqual(lo + span, TABLE + SCALE * hi_id + SCALE, "window must end past the highest id")

    def test_the_window_holds_the_image_bytes_at_every_admitted_id(self):
        self.em.mem_val("[rcx+rax*4]", 32, None)
        (lo, span), data = next(iter(self.em.ro_tabs.items()))
        for i in (-0x80, -0x35, -0x11, -1, 0, 0x5D):
            va = TABLE + SCALE * i
            self.assertEqual(data[va - lo:va - lo + SCALE], image_bytes(va, SCALE),
                             "id %d must read the image's bytes (a zero here is the round-6 trap)" % i)


class UnsignedGuardControl(unittest.TestCase):
    def test_an_unsigned_bound_still_bakes_from_the_base(self):
        em = emitter_for(function_lines("JA", read_on_taken=False))
        self.assertIsNotNone(em.bake_ro(xlate.parse_mem("rcx+rax*4"), 32, None))
        (lo, span), data = next(iter(em.ro_tabs.items()))
        self.assertEqual(lo, TABLE, "0..0x5E is a whole bound: the window starts at the base")
        self.assertEqual(span, (0x5E + 1) * SCALE + SCALE, "95 entries of 4 bytes, the last one whole")
        self.assertEqual(data[:SCALE], image_bytes(TABLE, SCALE))


if __name__ == "__main__":
    unittest.main()
