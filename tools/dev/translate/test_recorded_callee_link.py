#!/usr/bin/env python3
"""A callee recorded bit-exact in any proven results set is linkable by every layer function; a callee no set records is
not (owner ruling 01-11-26, Main; ProcCamera7). The FixDemo procedural camera engine 0x141F7BFE0 was proven bounded: its
zoom ramp's hold 0x140891FE0 and the matrix leaf 0x140A91A30 are recorded standalone (results_cutscene.json) but no camera
replay entered them, so the engine used to abort there; the preset thunk 0x140892560 is in no results file and stays a gap.

Run: python3 tools/dev/translate/test_recorded_callee_link.py
"""
import json
import os
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import gen_translated  # noqa: E402


def recorded_in_file(fname):
    with open(os.path.join(HERE, fname)) as f:
        return {r["va"] for r in json.load(f)["results"] if r.get("diffed")}


def listed_anywhere(va):
    return any(r["va"] == va for fname, _ in gen_translated.SETS
               for r in json.load(open(os.path.join(HERE, fname)))["results"])


class RecordedCalleeLink(unittest.TestCase):
    def test_a_callee_recorded_in_another_set_is_linkable(self):
        cutscene = recorded_in_file("results_cutscene.json")
        self.assertTrue({"0x140891FE0", "0x140A91A30"} <= cutscene)
        # a different set's closure (the camera engine's, proccam) links them
        self.assertTrue(cutscene <= gen_translated.link_universe(set(), set()))

    def test_a_callee_no_results_file_lists_is_not_linkable(self):
        self.assertFalse(listed_anywhere("0x140892560"))
        self.assertNotIn("0x140892560", gen_translated.link_universe(set(), set()))

    def test_a_listed_but_not_bit_exact_record_is_not_linkable(self):
        # 0x1403FB710 is a record with diffed False in results_cutscene.json (struct map gap)
        self.assertTrue(listed_anywhere("0x1403FB710"))
        self.assertNotIn("0x1403FB710", gen_translated.link_universe(set(), set()))

    def test_the_sets_own_universe_and_closure_are_kept(self):
        self.assertEqual(gen_translated.link_universe({"0xA"}, {"0xB"}) & {"0xA", "0xB"}, {"0xA", "0xB"})


if __name__ == "__main__":
    unittest.main(verbosity=2)
