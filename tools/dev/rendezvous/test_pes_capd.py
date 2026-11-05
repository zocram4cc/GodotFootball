#!/usr/bin/env python3
"""pes_capd's decision: the cap rises only while a wanted hook is short of its captures, and the override wins."""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pes_capd  # noqa: E402

LOW, HIGH = 10, 50


class Decide(unittest.TestCase):
    def test_a_wanted_hook_below_its_cap_raises_the_cap(self):
        self.assertEqual(pes_capd.decide(["a"], {"a": 12}, 6, {"a": 11}, LOW, HIGH), HIGH)

    def test_a_wanted_hook_at_its_cap_leaves_it_low(self):
        self.assertEqual(pes_capd.decide(["a"], {"a": 12}, 6, {"a": 12}, LOW, HIGH), LOW)

    def test_a_hook_without_an_override_uses_the_default_cap(self):
        self.assertEqual(pes_capd.decide(["b"], {}, 6, {"b": 5}, LOW, HIGH), HIGH)
        self.assertEqual(pes_capd.decide(["b"], {}, 6, {"b": 6}, LOW, HIGH), LOW)

    def test_nothing_wanted_is_low_even_with_hooks_below_cap(self):
        self.assertEqual(pes_capd.decide([], {"a": 12}, 6, {}, LOW, HIGH), LOW)

    def test_the_override_wins(self):
        self.assertEqual(pes_capd.decide(["a"], {"a": 12}, 6, {}, LOW, HIGH, override=100), 100)
        self.assertEqual(pes_capd.decide([], {}, 6, {}, LOW, HIGH, override=100), 100)

    def test_watch_caps_reads_both_flags(self):
        text = "rv_watch.py --per-hook-max 6 --hook-max judge=30,kick_root_141AFDF10=12 --only x"
        self.assertEqual(pes_capd.watch_caps(text), ({"judge": 30, "kick_root_141AFDF10": 12}, 6))


if __name__ == "__main__":
    unittest.main()
