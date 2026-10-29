#!/usr/bin/env python3
"""Selector test (ctest translate_select_recorded_test): the fast gate re-checks only the recorded functions a change can
reach. Plain python3, no harness. Synthetic layer: R (recorded) -> C (closure) -> L (leaf); U (recorded) calls nothing.
Run: python3 tools/dev/translate/test_select_recorded.py"""
import json
import os
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import select_recorded as sel  # noqa: E402

LAYER = "src/pes/translated"


def write_fn(root, sub, va, callees=()):
    d = os.path.join(root, LAYER, sub)
    os.makedirs(d, exist_ok=True)
    body = "".join("  pes_fn_%s(s);\n" % c for c in callees)
    with open(os.path.join(d, "pes_fn_%s.cpp" % va), "w") as f:
        f.write("void pes_fn_%s(PesState& s);\nvoid pes_fn_%s(PesState& s) {\n%s}\n" % (va, va, body))


def rec(va, diffed=True, **kw):
    return dict(va=va, diffed=diffed, **kw)


def path(sub, va):
    return "%s/%s/pes_fn_%s.cpp" % (LAYER, sub, va)


class Synthetic(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(dir=os.getcwd())
        root = self.tmp.name
        write_fn(root, "ai", "140000100", ["140000200"])      # R
        write_fn(root, "closure", "140000200", ["140000300"])  # C
        write_fn(root, "closure", "140000300")                 # L
        write_fn(root, "ai", "140000400")                      # U
        write_fn(root, "ai", "140000500", ["140000200"])      # N: recorded but not diffed, also calls C
        self.root = root
        self.base = {"results_core.json": {v: rec(v) for v in ("0x140000100", "0x140000400")}
                     | {"0x140000500": rec("0x140000500", diffed=False)}}
        self.now = self.base

    def tearDown(self):
        self.tmp.cleanup()

    def pick(self, paths, base=None):
        return sel.select(self.root, paths, base if base is not None else self.base, self.now)["results_core.json"]

    def test_closure_callee_selects_its_recorded_caller(self):
        self.assertEqual(self.pick([path("closure", "140000200")]), ["0x140000100"])

    def test_a_leaf_two_levels_down_selects_the_root(self):
        self.assertEqual(self.pick([path("closure", "140000300")]), ["0x140000100"])

    def test_the_root_selects_itself_and_not_its_neighbour(self):
        self.assertEqual(self.pick([path("ai", "140000400")]), ["0x140000400"])

    def test_unrelated_changes_select_nothing(self):
        self.assertEqual(self.pick(["src/gf/sim/match.cpp", "docs/re/x.md", "tests/pes_clock_test.cpp",
                                    "src/pes/translated/translated.hpp"]), [])

    def test_a_changed_spec_selects_its_function(self):
        self.assertEqual(self.pick(["tools/dev/diffharness/specs/core/140000400.json"]), ["0x140000400"])

    def test_a_translator_or_runtime_header_change_selects_everything(self):
        for p in ("tools/dev/translate/xlate.py", "src/pes/translated/pes_state.hpp", "tools/dev/diffharness/run.py"):
            self.assertEqual(self.pick([p]), ["0x140000100", "0x140000400"], p)

    def test_a_new_or_changed_record_is_selected(self):
        now = {"results_core.json": dict(self.base["results_core.json"])}
        now["results_core.json"]["0x140000400"] = rec("0x140000400", cases=9)
        self.now = now
        self.assertEqual(self.pick([]), ["0x140000400"])
        self.now = {"results_core.json": now["results_core.json"] | {"0x140000600": rec("0x140000600")}}
        write_fn(self.root, "ai", "140000600")
        self.assertEqual(self.pick([]), ["0x140000400", "0x140000600"])

    def test_snapdiff_records_are_never_selected(self):
        self.now = {"results_core.json": {"0x140000100": rec("0x140000100", proof="snapdiff")}}
        self.assertEqual(self.pick([path("closure", "140000200")], self.now), [])


class RealLayer(unittest.TestCase):
    """The committed layer and records: a mutated callee of a real recorded function selects that function."""

    def test_a_real_closure_callee_selects_its_recorded_caller(self):
        repo = sel.REPO
        results = sel.load_results(repo)
        graph = sel.layer_graph(repo)
        recorded = {va for _, recs in results.items() for va, r in recs.items() if sel.checked(r)}
        # a recorded function that reaches a callee two or more calls down: both the direct and the deep callee select it
        va = next(v for v in sorted(recorded) if any(graph[c] for c in graph.get(v[2:], ())))
        direct = sorted(graph[va[2:]])[0]
        deep = sorted(c for d in graph[va[2:]] for c in graph[d])[0]
        for callee in (direct, deep):
            out = sel.select(repo, [sel.layer_path(repo, callee)], results, results)
            self.assertIn(va, [v for vs in out.values() for v in vs], callee)


if __name__ == "__main__":
    unittest.main()
