#!/usr/bin/env python3
"""The closure walk never leaves a function, never admits a non-boundary target, and a snapdiff that compiled nothing
exits with an error (never a pass). Synthetic image (no game bytes): a flat PE with a hand-built .pdata.

Layout (VA = base + file offset):
  0x1000 A   .pdata  ret
  0x1010 T   no .pdata (a thunk): mov r9d,0xF9 ; xor r8d,r8d ; jmp 0x1100   (a forward tail jump inside the window)
  0x1020 B   .pdata  mov rax,rbx ; ret                                      (the code the old decode ran on into)
  0x1100 C   .pdata  ret                                                    (the tail jump's target)
Regression: PES 0x140A4D340 is T's shape; its decode ran through the next 7,691 instructions and the closure walk then
added 0x140A4EC7F, an address inside an instruction, as a function to translate and compile.

usage: test_closure_walk.py   (harness Python: capstone)
"""
import os
import struct
import sys
import tempfile
import types
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import pdata  # noqa: E402
pdata.KNOWN = frozenset()  # the synthetic images use no ledger
import closure  # noqa: E402
import snapdiff  # noqa: E402
import xlate  # noqa: E402

BASE = pdata.IMAGE_BASE
A, T, B, C = BASE + 0x1000, BASE + 0x1010, BASE + 0x1020, BASE + 0x1100
MID_B = B + 1  # inside B's first instruction
LEAF = BASE + 0x1180  # a pdata-less function that follows jump-table data
SWITCH_SITE, SWITCH_CASE = BASE + 0x1191, BASE + 0x1193  # a pdata-less function `nop ; jmp rcx ; [case] ret`
INT3 = b"\xcc"


def build_image(path):
    img = bytearray(0x20000)
    struct.pack_into("<I", img, 0x3C, 0x80)
    img[0x80:0x84] = b"PE\0\0"
    struct.pack_into("<HHIIIHH", img, 0x84, 0x8664, 0, 0, 0, 0, 240, 0)
    entries = [(0x1000, 0x1001), (0x1020, 0x1024), (0x1100, 0x1101)]
    struct.pack_into("<II", img, 0x98 + 112 + 24, 0x400, 12 * len(entries))
    for i, (b, e) in enumerate(entries):
        struct.pack_into("<III", img, 0x400 + 12 * i, b, e, 0)
    for off in range(0x1000, 0x1200):
        img[off] = INT3[0]
    img[0x1000] = 0xC3
    thunk = bytes.fromhex("41b9f9000000") + bytes.fromhex("4531c0") + b"\xe9" + struct.pack("<i", 0x1100 - (0x1010 + 14))
    img[0x1010:0x1010 + len(thunk)] = thunk
    img[0x1020:0x1024] = bytes.fromhex("4889d8c3")
    img[0x1100] = 0xC3
    img[0x1170:0x1180] = bytes(range(0x80, 0x90))  # jump-table data, then a leaf with no .pdata entry
    img[0x1180] = 0xC3
    img[0x1190:0x1194] = bytes.fromhex("90ffe1c3")
    with open(path, "wb") as f:
        f.write(img)


class ClosureWalk(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.dir = tempfile.TemporaryDirectory()
        cls.image = os.path.join(cls.dir.name, "img.bin")
        build_image(cls.image)

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def test_decode_of_a_thunk_with_a_forward_tail_jump_stops_at_the_jump(self):
        lines = xlate.capstone_decode(self.image, T)
        self.assertEqual([m for _, m, _ in lines], ["MOV", "XOR", "JMP"])

    def test_decode_of_a_pdata_function_is_unchanged(self):
        lines = xlate.capstone_decode(self.image, B)
        self.assertEqual([m for _, m, _ in lines][:2], ["MOV", "RET"])

    def test_closure_row_of_a_thunk_names_its_tail_target_and_ends_with_the_jump(self):
        with open(self.image, "rb") as f:
            row = closure.ClosurePdata(f).decode_row(T, "t")
        self.assertEqual(row["callees"], {"%08X" % C: "tail"})
        self.assertEqual(row["size"], 3)

    def test_boundaries(self):
        with open(self.image, "rb") as f:
            p = pdata.Pdata(f)
            self.assertTrue(p.is_boundary(A))
            self.assertTrue(p.is_boundary(T))  # no .pdata entry
            self.assertTrue(p.is_boundary(LEAF))  # no .pdata entry, and the bytes before it are table data, not int3
            self.assertFalse(p.is_boundary(MID_B))  # inside an instruction of a .pdata function
            self.assertFalse(p.is_boundary(B + 2))  # inside a .pdata function
            self.assertIsNone(p.end_of(T))

    def walk_args(self):
        return types.SimpleNamespace(out_dir=self.dir.name, image=self.image, asm_dir=None, spec_dir=self.dir.name)

    def run_closure(self, jmp_target, reached=None):
        """build_closure over a root whose translation tail-jumps to jmp_target (the translator is a stub)."""
        src = ["void pes_fn_%X(PesState& s) {" % A, "  // OUT-OF-RANGE JMP 0x%X" % jmp_target, "}"]
        real = (snapdiff.drv.translate_one, snapdiff.drv.runnable_extra, snapdiff.drv.load_spec, snapdiff.drv.asm_for)
        calls = []

        def fake_translate(v, asm, image, pilot, stubs=None):
            calls.append(v)
            return (src if v == "0x%X" % A else ["void pes_fn_x(PesState& s) {}"]), []

        snapdiff.drv.translate_one = fake_translate
        # The real one returns the seed's buildable callees (and what they reach): here, the seed's own entries.
        snapdiff.drv.runnable_extra = lambda spec, image, depth_cap=80: (
            [c["va"] for c in spec.get("harness", {}).get("out_of_set_callees", [])], {})
        snapdiff.drv.load_spec = lambda d, v: (None, None)
        snapdiff.drv.asm_for = lambda d, v: ""
        try:
            need, err = snapdiff.build_closure("0x%X" % A, {}, self.walk_args(), {"0x%X" % A}, reached or {})
        finally:
            snapdiff.drv.translate_one, snapdiff.drv.runnable_extra, snapdiff.drv.load_spec, snapdiff.drv.asm_for = real
        return need, err

    def test_walk_refuses_an_address_inside_an_instruction(self):
        need, err = self.run_closure(MID_B)
        self.assertIn("not a function boundary", err or "")
        self.assertNotIn("0x%X" % MID_B, need)

    def test_walk_admits_a_real_function_boundary(self):
        need, err = self.run_closure(T)
        self.assertIsNone(err)
        self.assertIn("0x%X" % T, need)

    def test_an_indirect_site_inside_a_function_is_no_root(self):
        # The harness traces call sites; B+1 is a site inside B, so the function holding it is the root, not the site.
        need, err = self.run_closure(T, {"0x%X" % (B + 1): {"0x%X" % C: 1}})
        self.assertIsNone(err)
        self.assertNotIn("0x%X" % (B + 1), need)
        self.assertIn("0x%X" % C, need)

    def test_a_switch_case_in_its_own_function_is_no_root_and_no_error(self):
        # `jmp reg` inside B to B+2: the harness traces it as an indirect branch, but it is a case label, not a callee.
        need, err = self.run_closure(T, {"0x%X" % (B + 1): {"0x%X" % (B + 2): 1}})
        self.assertIsNone(err)
        self.assertNotIn("0x%X" % (B + 2), need)

    def test_an_indirect_target_inside_an_instruction_is_an_error(self):
        need, err = self.run_closure(T, {"0x%X" % A: {"0x%X" % MID_B: 1}})
        self.assertIn("not a function boundary", err or "")

    def test_a_register_jump_in_a_leaf_is_a_switch_not_a_function(self):
        # The leaf has no .pdata entry, so site and case both look like function starts (0x140848B40 and 0x140848B6E
        # inside the leaf 0x140848AF0, whose `jmp rcx` is a switch). Neither is a root.
        need, err = self.run_closure(T, {"0x%X" % SWITCH_SITE: {"0x%X" % SWITCH_CASE: 1}})
        self.assertIsNone(err)
        self.assertNotIn("0x%X" % SWITCH_SITE, need)
        self.assertNotIn("0x%X" % SWITCH_CASE, need)

    def test_a_register_jump_to_another_function_is_still_a_root(self):
        need, err = self.run_closure(A, {"0x%X" % SWITCH_SITE: {"0x%X" % T: 1}})
        self.assertIsNone(err)
        self.assertIn("0x%X" % T, need)


class NothingCompiled(unittest.TestCase):
    def run_main(self, result):
        real = (snapdiff.run_one, snapdiff.drv.universe_of)
        snapdiff.run_one = lambda va, args, pilot, pairs: result
        snapdiff.drv.universe_of = lambda spec_dir: set()
        argv = sys.argv
        sys.argv = ["snapdiff.py", "--image", "x", "--out-dir", "x", "--snapshot", "s=t", "--only", "0x1000"]
        try:
            snapdiff.main()
            return 0
        except SystemExit as e:
            return e.code
        finally:
            sys.argv = argv
            snapdiff.run_one, snapdiff.drv.universe_of = real

    def result(self, **kw):
        r = {"va": "0x1000", "calls": 6, "replayed": 6, "mismatches": [], "own_block": None, "proven": False,
             "note": "", "translated": False}
        r.update(kw)
        return r

    def test_a_closure_that_did_not_build_is_an_error_not_a_pass(self):
        self.assertEqual(self.run_main(self.result(note="translate 0x140A4EC7F: no instructions")), 1)

    def test_a_built_and_compared_closure_exits_zero(self):
        self.assertEqual(self.run_main(self.result(translated=True, proven=True)), 0)


if __name__ == "__main__":
    unittest.main()
