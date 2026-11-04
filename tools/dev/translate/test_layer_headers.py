#!/usr/bin/env python3
"""Adding one translated function must not recompile the layer.

Two properties, both needing no image and no compiler:
  1. No pes_fn_<VA>.cpp reaches (through any #include chain inside the layer) translated.hpp or the dispatch table,
     the two files that change whenever a function is added.
  2. Regenerating the committed layer with one extra function changes exactly: the new TU, translated.hpp and
     pes_dispatch_gen.cpp (the one TU that names every function).
"""
import glob
import os
import re
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import gen_translated  # noqa: E402

LAYER = gen_translated.ROOT
INCLUDE = re.compile(r'^\s*#\s*include\s*"([^"]+)"', re.M)
ALL_FUNCTIONS = ("translated.hpp", "pes_dispatch_gen.hpp", "pes_dispatch_gen.cpp")
NEW_VA = "0x14FFFFFF0"  # past the image's code, so it can never collide with a committed function


def include_closure(rel):
    """Layer files reachable from rel through quoted includes (the layer's own headers only)."""
    seen, todo = set(), [rel]
    while todo:
        cur = todo.pop()
        path = os.path.join(LAYER, cur)
        if cur in seen or not os.path.exists(path):
            continue
        seen.add(cur)
        with open(path) as f:
            todo += INCLUDE.findall(f.read())
    return seen


def committed_members():
    members = {}
    for path in glob.glob(os.path.join(LAYER, "*", "pes_fn_*.cpp")):
        sub, name = os.path.relpath(path, LAYER).split(os.sep)
        with open(path) as f:
            members["0x" + name[len("pes_fn_"):-len(".cpp")]] = {
                "raw": f.read(), "recorded": True, "sub": sub, "status": ""}
    return members


class LayerHeaders(unittest.TestCase):
    def test_no_function_tu_includes_an_all_functions_file(self):
        tus = sorted(glob.glob(os.path.join(LAYER, "*", "pes_fn_*.cpp")))
        self.assertGreater(len(tus), 100)
        bad = [os.path.relpath(p, LAYER) for p in tus
               if set(ALL_FUNCTIONS) & include_closure(os.path.relpath(p, LAYER))]
        self.assertEqual(bad[:5], [])

    def test_one_added_function_changes_only_its_tu_and_the_two_lists(self):
        members = committed_members()
        before = gen_translated.layer_files(members)
        for rel, text in before.items():  # the committed layer is what the generator writes from its own members
            with open(os.path.join(LAYER, rel)) as f:
                self.assertEqual(f.read(), text, rel)
        members[NEW_VA] = {"raw": "void pes_fn_%s(PesState& s) {}\n" % NEW_VA[2:], "recorded": True,
                           "sub": "closure", "status": ""}
        after = gen_translated.layer_files(members)
        changed = {rel for rel in after if before.get(rel) != after[rel]}
        self.assertEqual(changed, {os.path.join("closure", "pes_fn_%s.cpp" % NEW_VA[2:]),
                                   "translated.hpp", "pes_dispatch_gen.cpp"})
        self.assertEqual(set(before) - set(after), set())


if __name__ == "__main__":
    unittest.main()
