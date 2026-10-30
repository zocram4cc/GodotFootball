#!/usr/bin/env python3
"""A `jmp qword ptr [reg + disp]` (a vtable tail call) is a dispatched tail call, never an unresolved trap (CutAnchor3).

141F61CC0 ends with `jmp [rax+0x18]` (the virtual 141F616F0): xlate emitted `__builtin_trap(); /* UNRESOLVED JMP */`, so
the translated radius 0x141F5EC60 died with SIGILL where PES entered the target. The jump now reads the target and
looks it up in the dispatch table like an indirect CALL (an untranslated target aborts naming its VA), then returns.
Usage: GF_DIFF_PYTHON=<venv python> python3 test_vtable_tail_jump.py
"""
import os
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import xlate  # noqa: E402

BASE = 0x140000000
FN = 0x140001000


class VtableTailJump(unittest.TestCase):
    def translate(self, code):
        buf = bytearray(FN - BASE + xlate.DECODE_WINDOW)
        buf[FN - BASE:FN - BASE + len(code)] = code
        with tempfile.NamedTemporaryFile(suffix=".bin") as f:
            f.write(buf)
            f.flush()
            src, _ = xlate.translate_one(f.name, "", "0x%X" % FN, {"0x%X" % FN})
        return "\n".join(src)

    def test_jmp_through_memory_is_a_dispatched_tail_call(self):
        text = self.translate(bytes([0xFF, 0x60, 0x18]))  # jmp qword ptr [rax + 0x18]
        self.assertNotIn("UNRESOLVED", text)
        self.assertIn("pes_dispatch_lookup", text)
        self.assertIn("pes_dispatch_missing(_t)", text)  # an untranslated target is loud
        self.assertIn("0x18", text)  # the slot read is [rax + 0x18]

    def test_jmp_register_stays_loud(self):
        text = self.translate(bytes([0xFF, 0xE0]))  # jmp rax: the prepass cannot resolve it, so it traps
        self.assertIn("UNRESOLVED JMP", text)


if __name__ == "__main__":
    unittest.main()
