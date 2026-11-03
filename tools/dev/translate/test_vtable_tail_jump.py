#!/usr/bin/env python3
"""A `jmp qword ptr [reg + disp]` (a vtable tail call) is a dispatched tail call, never an unresolved trap (CutAnchor3).

141F61CC0 ends with `jmp [rax+0x18]` (the virtual 141F616F0): xlate emitted `__builtin_trap(); /* UNRESOLVED JMP */`, so
the translated radius 0x141F5EC60 died with SIGILL where PES entered the target. The jump reads the target and looks it
up in the dispatch table like an indirect CALL (an untranslated target aborts naming its VA), then returns; `jmp rax`
(no memory operand, no resolved table) stays a loud trap. Synthetic image (test_extent.build_image + one .pdata entry).
Usage: GF_DIFF_PYTHON=<venv python> python3 test_vtable_tail_jump.py
"""
import os
import struct
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import pdata  # noqa: E402
import test_extent as TE  # noqa: E402
import xlate  # noqa: E402

BASE = pdata.IMAGE_BASE
F = BASE + 0x1600


def image(path, code):
    """TE.build_image plus one .pdata entry for F holding `code`."""
    TE.build_image(path)
    raw = bytearray(open(path, "rb").read())
    n = struct.unpack_from("<I", raw, 0x98 + 112 + 24 + 4)[0] // 12
    struct.pack_into("<I", raw, 0x98 + 112 + 24 + 4, 12 * (n + 1))
    struct.pack_into("<III", raw, 0x400 + 12 * n, 0x1600, 0x1600 + len(code), 0x600)
    raw[0x1600:0x1600 + len(code)] = code
    open(path, "wb").write(raw)


class VtableTailJump(unittest.TestCase):
    def translate(self, code):
        pdata.KNOWN = frozenset()
        with tempfile.TemporaryDirectory() as d:
            img = os.path.join(d, "img.bin")
            image(img, code)
            src, _ = xlate.translate_one(img, "", "0x%X" % F, set())
        return "\n".join(src)

    def test_jmp_through_memory_is_a_dispatched_tail_call(self):
        text = self.translate(bytes([0xFF, 0x60, 0x18]))  # jmp qword ptr [rax + 0x18]
        self.assertNotIn("UNRESOLVED", text)
        self.assertIn("pes_dispatch_lookup", text)
        self.assertIn("pes_dispatch_missing(_t)", text)  # an untranslated target is loud

    def test_jmp_register_stays_loud(self):
        text = self.translate(bytes([0xFF, 0xE0]))  # jmp rax: the prepass cannot resolve it, so it traps
        self.assertIn("UNRESOLVED JMP", text)


if __name__ == "__main__":
    unittest.main()
