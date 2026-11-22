#!/usr/bin/env python3
"""json_union_merge keeps both sides' records and refuses only a field both sides changed differently."""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from json_union_merge import Conflict, dumps, merge  # noqa: E402

BASE = {"note": "n", "results": [{"va": "0x1", "diffed": True}]}


class JsonUnionMerge(unittest.TestCase):
    def test_both_sides_add_records_and_keep_each_other(self):
        ours = {"note": "n", "results": BASE["results"] + [{"va": "0x2"}]}
        theirs = {"note": "n", "results": BASE["results"] + [{"va": "0x3"}]}
        self.assertEqual([r["va"] for r in merge(BASE, ours, theirs)["results"]], ["0x1", "0x2", "0x3"])

    def test_a_removal_on_one_side_survives_an_untouched_other_side(self):
        base = {"members": ["0xA", "0xB"]}
        self.assertEqual(merge(base, {"members": ["0xA"]}, {"members": ["0xA", "0xB", "0xC"]}),
                         {"members": ["0xA", "0xC"]})

    def test_one_record_edited_in_different_fields_merges_field_by_field(self):
        ours = {"note": "n", "results": [{"va": "0x1", "diffed": True, "closure_file": "c.json"}]}
        theirs = {"note": "n", "results": [{"va": "0x1", "diffed": False}]}
        self.assertEqual(merge(BASE, ours, theirs)["results"], [{"va": "0x1", "diffed": False, "closure_file": "c.json"}])

    def test_the_same_field_changed_differently_is_a_conflict(self):
        with self.assertRaises(Conflict):
            merge(BASE, {"note": "a", "results": []}, {"note": "b", "results": []})

    def test_ro_tables_keep_their_one_record_per_line_form(self):
        text = '[\n  {"va": "0x1", "bytes": "0x8"},\n  {"va": "0x2", "bytes": "0x4"}\n]\n'
        self.assertEqual(dumps([{"va": "0x1", "bytes": "0x8"}, {"va": "0x2", "bytes": "0x4"}], text), text)

    def test_a_sorted_file_without_a_final_newline_sorts_the_merged_keys_in(self):
        self.assertEqual(dumps({"b": 1, "a": 2, "c": 3}, '{\n "a": 2,\n "b": 1\n}'), '{\n "a": 2,\n "b": 1,\n "c": 3\n}')

    def test_an_unsorted_file_keeps_its_order(self):
        text = '{\n "b": 1,\n "a": 2\n}\n'
        self.assertEqual(dumps({"b": 1, "a": 2}, text), text)

    def test_a_record_repeated_in_every_version_stays_repeated(self):
        dup = {"va": "0x1", "bytes": "0x8"}
        out = merge([dup, dup], [dup, dup, {"va": "0x2"}], [dup, dup, {"va": "0x3"}])
        self.assertEqual(out, [dup, dup, {"va": "0x2"}, {"va": "0x3"}])


if __name__ == "__main__":
    unittest.main()