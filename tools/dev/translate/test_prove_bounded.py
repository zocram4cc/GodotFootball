#!/usr/bin/env python3
"""prove_bounded test: the spec dir and results file are arguments (defaults: the trapaction set), and a merge replaces
only the root's own record. Plain python3, no harness. Run: python3 tools/dev/translate/test_prove_bounded.py"""
import json
import os
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import prove_bounded as pb  # noqa: E402


def rec(va, note):
    return {"va": va, "proven": True, "note": note}


class ProveBoundedArguments(unittest.TestCase):
    def test_defaults_are_the_trapaction_set(self):
        a = pb.build_parser().parse_args(["0x14217DEF0", "work", "--tag", "t"])
        self.assertEqual(a.spec_dir, pb.SPEC_DIR)
        self.assertEqual(a.results, pb.RESULTS)

    def test_a_presentation_proof_names_its_own_spec_dir_and_results(self):
        a = pb.build_parser().parse_args(["0x14217DEF0", "work", "--tag", "t",
                                          "--spec-dir", "/specs/presentation", "--results", "/r/results_presentation.json"])
        self.assertEqual(a.spec_dir, "/specs/presentation")
        self.assertEqual(a.results, "/r/results_presentation.json")


class MergeRecord(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.mkdtemp(dir=os.environ.get("GF_TEST_SCRATCH") or None)
        self.path = os.path.join(self.dir, "results_presentation.json")
        with open(self.path, "w") as f:
            json.dump({"note": "t", "results": [rec("0x141978300", "other set"), rec("0x14217DEF0", "old")]}, f)

    def test_the_root_record_is_replaced_and_the_others_kept(self):
        pb.merge_record(self.path, rec("0x14217DEF0", "new"))
        by_va = {r["va"]: r for r in json.load(open(self.path))["results"]}
        self.assertEqual(by_va["0x14217DEF0"]["note"], "new")
        self.assertEqual(by_va["0x141978300"]["note"], "other set")
        self.assertEqual(len(by_va), 2)


if __name__ == "__main__":
    unittest.main()
