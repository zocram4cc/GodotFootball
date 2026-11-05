#!/usr/bin/env python3
"""test_branch_select.py: the GATE_TIER=branch selector (tools/dev/branch_select.py) on sample diffs, against the real
build directory. Each check states the rule it relies on (branch_select.py's docstring):

  - a source file in a test binary's build closure selects that binary, so every test of a gfsim binary (a seeded family
    among them) runs for a sim source, and a pesfmt-only binary does not;
  - a changed test source selects its own binary only;
  - a docs change selects only the always-on tests (the PII scan and the five recorded re-checks);
  - a changed assets file falls back to every test (sim data is read by any test through GF_SOURCE_DIR);
  - a changed CMake line naming a test selects that test; a changed CMake line naming none falls back to every test;
  - the selection reaches ctest as a test-number index (-I) that ctest lists back exactly, at the full build's size (the
    name regex it replaced overflowed ctest's regex engine at 2,311 names).

Run by ctest (branch_select_test): test_branch_select.py <build dir> <repo root> <scratch dir>."""
import os
import re
import shutil
import subprocess
import sys
import unittest

BUILD, REPO, SCRATCH = (os.path.abspath(a) for a in sys.argv[1:4])
sys.argv = sys.argv[:1]
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import branch_select  # noqa: E402


def diff(path, added=()):
    """A unified diff (-U0 shape) that changes one file: its header plus the added lines."""
    lines = [f"diff --git a/{path} b/{path}", f"--- a/{path}", f"+++ b/{path}"]
    if added:
        lines.append(f"@@ -0,0 +1,{len(added)} @@")
        lines += ["+" + a for a in added]
    return "\n".join(lines) + "\n"


def cases_of(binary):
    """Every ctest entry that runs the binary (a gtest binary is one entry per case)."""
    return {t["name"] for t in branch_select.ctest_tests(BUILD)
            if t["command"] and os.path.basename(t["command"][0]) == binary}


class BranchSelect(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        shutil.rmtree(SCRATCH, ignore_errors=True)
        os.makedirs(SCRATCH)

    def select(self, text):
        return set(branch_select.select(BUILD, REPO, text))

    def assertBinary(self, binary, got, selected):
        cases = cases_of(binary)
        self.assertTrue(cases, f"no ctest entry runs {binary}")
        if selected:
            self.assertTrue(cases <= got, f"{binary} should be selected")
        else:
            self.assertFalse(cases & got, f"{binary} should not be selected")

    def test_sim_source_selects_every_gfsim_binary_and_no_pesfmt_only_binary(self):
        got = self.select(diff("src/sim/simcontext.cpp"))
        self.assertBinary("pes_judge_test", got, selected=True)
        self.assertBinary("pes_restart_delay_test", got, selected=True)  # a seeded family: its matches run this sim
        self.assertBinary("pes_fdc_test", got, selected=False)  # links pesfmt only

    def test_test_source_selects_its_own_binary_only(self):
        got = self.select(diff("tests/sim/pes_offside_test.cpp"))
        self.assertBinary("pes_offside_test", got, selected=True)
        self.assertBinary("pes_judge_test", got, selected=False)
        self.assertBinary("pes_restart_delay_test", got, selected=False)

    def test_docs_select_only_the_always_on_tests(self):
        got = self.select(diff("docs/ports/README.md"))
        self.assertIn("repo_pii_scan", got)
        self.assertIn("diff_core_recorded", got)
        self.assertBinary("pes_judge_test", got, selected=False)
        self.assertBinary("pes_offside_test", got, selected=False)

    def test_assets_fall_back_to_every_test(self):
        got = self.select(diff("assets/fixtures/teams.sql"))
        self.assertBinary("pes_fdc_test", got, selected=True)
        self.assertBinary("pes_judge_test", got, selected=True)

    def test_cmake_line_naming_a_test_selects_that_test(self):
        got = self.select(diff("tests/CMakeLists.txt", ["set_tests_properties(pes_fdc_test PROPERTIES TIMEOUT 999)"]))
        self.assertBinary("pes_fdc_test", got, selected=True)
        self.assertBinary("pes_judge_test", got, selected=False)

    def test_cmake_line_naming_no_test_falls_back_to_every_test(self):
        got = self.select(diff("tests/CMakeLists.txt", ["set(GF_SHARED_FLAG 1)"]))
        self.assertBinary("pes_judge_test", got, selected=True)

    def assertRunsExactly(self, names):
        """ctest -N -I lists exactly `names`: the gate's selection reaches ctest this way (-R overflowed at 2,311 names)."""
        arg = branch_select.ctest_index(BUILD, names)
        out = subprocess.run(["ctest", "--test-dir", BUILD, "-N", "-I", arg],
                             capture_output=True, text=True, check=True).stdout
        self.assertNotIn("No tests were found", out)
        self.assertEqual(set(re.findall(r"Test\s+#\d+: (\S+)", out)), set(names))

    def test_every_built_test_runs_under_ctest_as_an_index(self):
        everything = {t["name"] for t in branch_select.ctest_tests(BUILD)}
        self.assertGreater(len(everything), 1000)
        self.assertRunsExactly(everything)

    def test_a_sim_selection_runs_under_ctest_as_an_index(self):
        self.assertRunsExactly(set(self.select(diff("src/sim/simcontext.cpp"))))


if __name__ == "__main__":
    unittest.main(verbosity=2)
