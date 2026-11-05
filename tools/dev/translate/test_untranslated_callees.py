#!/usr/bin/env python3
"""untranslated_callees.py, pinned on a synthetic layer: the calls reached from the root (and only those), their call
sites, a cycle that terminates, a root the layer lacks, and the hook plan's refusals (no .pdata entry, not an entry, no
branch-free 14-byte prologue). Needs capstone (rv_hooks), the window check the plan uses."""
import contextlib
import io
import os
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.abspath(os.path.join(HERE, "..", "rendezvous")))
import untranslated_callees as uc  # noqa: E402
import rv_hooks as rh  # noqa: E402

ROOT, MID, UNREACHED = 0x100000100, 0x100000200, 0x100000B00
MID_GAP, LEAF_GAP, UNREACHED_GAP = 0x100000A00, 0x100000900, 0x100000C00

# ROOT calls MID and has a gap at line 3; MID calls ROOT back (a cycle) and has a gap at line 3. UNREACHED is not called.
FUNCTIONS = {
    ROOT: "  pes_fn_%X(s);\n  /*OUT-OF-SET CALL 0x%X*/ pes_dispatch_missing(0x%X);\n" % (MID, LEAF_GAP, LEAF_GAP),
    MID: "  pes_fn_%X(s);\n  /*OUT-OF-SET CALL 0x%X*/ pes_dispatch_missing(0x%X);\n" % (ROOT, MID_GAP, MID_GAP),
    UNREACHED: "  /*OUT-OF-SET CALL 0x%X*/ pes_dispatch_missing(0x%X);\n" % (UNREACHED_GAP, UNREACHED_GAP),
}

# mov [rsp+8],rbx; mov [rsp+10h],rsi; push rdi; sub rsp,20h: 15 bytes of whole, branch-free, rip-free instructions.
PROLOGUE = bytes.fromhex("48895c2408" "4889742410" "57" "4883ec20")
NOPS = b"\x90" * 49


class UntranslatedCalleesTest(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()
        group = os.path.join(self.dir.name, "layer", "grp")
        os.makedirs(group)
        for va, body in FUNCTIONS.items():
            with open(os.path.join(group, "pes_fn_%X.cpp" % va), "w") as f:
                f.write("void pes_fn_%X(PesState& s) {\n%s}\n" % (va, body))
        self.layer = os.path.join(self.dir.name, "layer")
        self.index = uc.layer_index(self.layer)

    def tearDown(self):
        self.dir.cleanup()

    def test_lists_the_calls_reached_from_the_root_with_their_call_sites(self):
        gaps, walked = uc.reach(self.index, [ROOT])
        self.assertEqual(sorted(gaps), [LEAF_GAP, MID_GAP])
        self.assertEqual(gaps[LEAF_GAP], [(ROOT, 3)])
        self.assertEqual(gaps[MID_GAP], [(MID, 3)])
        self.assertEqual(walked, {ROOT, MID})

    def test_a_marker_the_root_cannot_reach_is_not_listed(self):
        gaps, _ = uc.reach(self.index, [ROOT])
        self.assertNotIn(UNREACHED_GAP, gaps)

    def test_the_command_line_prints_each_call_site_and_exits_0(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            rc = uc.main(["--layer", self.layer, "%X" % ROOT])
        self.assertEqual(rc, 0)
        self.assertIn("0x%X  call sites: 0x%X:3" % (LEAF_GAP, ROOT), out.getvalue())

    def test_a_root_the_layer_lacks_exits_2(self):
        with contextlib.redirect_stderr(io.StringIO()):
            rc = uc.main(["--layer", self.layer, "0x100000F00"])
        self.assertEqual(rc, uc.EXIT_ROOT_NOT_TRANSLATED)

    def test_hook_rows_and_hooks_cfg_follow_the_prologue_check(self):
        ok, ret, mid_entry, no_pdata = 0x100001000, 0x100002000, 0x100003004, 0x100004000
        code = {ok: PROLOGUE + NOPS, ret: b"\xc3" + b"\x90" * 63}
        entries = {ok: ok, ret: ret, mid_entry: 0x100003000, no_pdata: None}
        rows, cfg, refused = uc.plan_hooks(
            {ok: "a", ret: "b", mid_entry: "c", no_pdata: "d"}, code_at=code.__getitem__,
            entry_of=entries.__getitem__, first=1, every=20, prefix="callee", displaced_window=rh.displaced_window)
        self.assertEqual(len(rows), 4)  # hook_plan TARGETS rows are emitted for every target
        self.assertEqual(rows[0], '    ("callee_100001000", 0x100001000, "a"),')
        self.assertEqual(cfg, ["hook callee_100001000 0x100001000 15 1 20 %s" % PROLOGUE.hex()])
        self.assertEqual([va for va, _ in refused], [ret, mid_entry, no_pdata])
        self.assertIn("branch", refused[0][1])
        self.assertIn("not a function entry", refused[1][1])
        self.assertIn("not inside any .pdata", refused[2][1])


if __name__ == "__main__":
    unittest.main()
