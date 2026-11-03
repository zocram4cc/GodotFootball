#!/usr/bin/env python3
"""The driver's two caches (buildcache.py) reuse a result only when the reuse is exact.

Object cache: an object is keyed by the compiler, the flags, the TU's name and bytes, and every build-tree header the
TU includes (transitively). Corpus cache: run.py's outputs are reused only while every file run.py loaded, and every
translator value it took from xlate, still reads the same.

ctest translate_buildcache (harness Python: xlate's decode; skips 77 without GF_DIFF_PYTHON).
Usage: python3 test_buildcache.py
"""
import json
import os
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import buildcache as bc  # noqa: E402

BUILDCACHE = os.path.join(HERE, "buildcache.py")
FLAGS = ["-std=c++20", "-O1"]
COMPILER = "/usr/bin/g++ | g++ 16.2.1"

# A stand-in run.py: it imports a sibling module, reads data.txt, tries optional.txt (absent at first), and writes --out.
FAKE_RUN = """\
import argparse
import os

import helper_mod

ap = argparse.ArgumentParser()
ap.add_argument("--out", required=True)
args = ap.parse_args()
here = os.path.dirname(os.path.abspath(__file__))
with open(os.path.join(here, "data.txt")) as f:
    body = f.read()
try:
    with open(os.path.join(here, "optional.txt")) as f:
        body += f.read()
except FileNotFoundError:
    pass
with open(args.out, "w") as f:
    f.write(helper_mod.tag() + body)
print("summary valid=1 requested=1 nonreturn=0 failed=0 capped=0 icount_max=0")
"""


def write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w") as f:
        f.write(text)


class ObjectKey(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.gen = os.path.join(self.tmp.name, "gen")
        write(os.path.join(self.gen, "pes_state.hpp"), "int state();\n")
        write(os.path.join(self.gen, "pes_cond.inl"), "// cond\n")
        write(os.path.join(self.gen, "pes_stubs.hpp"), '#include "pes_state.hpp"\n#include <cstdint>\n')
        self.tu = os.path.join(self.gen, "pes_fn_140001000.cpp")
        write(self.tu, '#include "pes_stubs.hpp"\nint fn() { return 1; }\n')

    def tearDown(self):
        self.tmp.cleanup()

    def key(self, compiler=COMPILER, flags=FLAGS, tu=None, gen=None):
        return bc.tu_object_key(tu or self.tu, compiler, flags, gen or self.gen)

    def test_the_same_tu_and_headers_give_the_same_key(self):
        self.assertEqual(self.key(), self.key())

    def test_the_tu_bytes_change_the_key(self):
        before = self.key()
        write(self.tu, '#include "pes_stubs.hpp"\nint fn() { return 2; }\n')
        self.assertNotEqual(before, self.key())

    def test_an_included_header_changes_the_key_through_its_own_include(self):
        before = self.key()
        write(os.path.join(self.gen, "pes_state.hpp"), "int state(); // changed\n")  # included by pes_stubs.hpp
        self.assertNotEqual(before, self.key())

    def test_a_header_the_tu_does_not_include_leaves_the_key_alone(self):
        before = self.key()
        write(os.path.join(self.gen, "pes_cond.inl"), "// changed, not included\n")
        self.assertEqual(before, self.key())

    def test_the_closure_is_the_build_tree_headers_only(self):
        closure = bc.include_closure(self.tu, self.gen)
        self.assertEqual([name for name, _ in closure], ["pes_state.hpp", "pes_stubs.hpp"])

    def test_flags_and_compiler_change_the_key(self):
        before = self.key()
        self.assertNotEqual(before, self.key(flags=["-std=c++20", "-O2"]))
        self.assertNotEqual(before, self.key(compiler="/usr/bin/clang++ | clang 19"))

    def test_the_same_tu_in_another_build_dir_shares_the_key(self):
        other = os.path.join(self.tmp.name, "other-va", "gen")
        for name in ("pes_state.hpp", "pes_stubs.hpp"):
            write(os.path.join(other, name), open(os.path.join(self.gen, name)).read())
        write(os.path.join(other, "pes_fn_140001000.cpp"), open(self.tu).read())
        self.assertEqual(self.key(), self.key(tu=os.path.join(other, "pes_fn_140001000.cpp"), gen=other))

    def test_a_quoted_include_found_nowhere_makes_the_tu_uncacheable(self):
        write(self.tu, '#include "missing.hpp"\nint fn() { return 1; }\n')
        self.assertIsNone(self.key())


class CorpusEntry(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        d = self.dir = self.tmp.name
        self.run_py = os.path.join(d, "fake_run.py")
        write(self.run_py, FAKE_RUN)
        write(os.path.join(d, "helper_mod.py"), "def tag():\n    return 'H'\n")
        write(os.path.join(d, "data.txt"), "one\n")
        write(os.path.join(d, "unrelated.txt"), "not read by run.py\n")
        self.out = os.path.join(d, "out.txt")
        self.deps = os.path.join(d, "deps.json")

    def tearDown(self):
        self.tmp.cleanup()

    def record(self):
        r = subprocess.run([sys.executable, BUILDCACHE, "record", self.deps, self.run_py, "--out", self.out],
                           capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(r.stdout, "summary valid=1 requested=1 nonreturn=0 failed=0 capped=0 icount_max=0\n")
        with open(self.deps) as f:
            return bc.entry_doc(json.load(f))

    def edit(self, name, text):
        write(os.path.join(self.dir, name), text)

    def test_the_record_writes_run_py_output_and_its_stdout_unchanged(self):
        self.record()
        with open(self.out) as f:
            self.assertEqual(f.read(), "H" + "one\n")

    def test_a_fresh_entry_is_valid(self):
        self.assertTrue(bc.entry_is_valid(self.record()))

    def test_a_data_file_run_py_read_invalidates_the_entry(self):
        doc = self.record()
        self.edit("data.txt", "two\n")
        self.assertFalse(bc.entry_is_valid(doc))

    def test_an_imported_module_invalidates_the_entry(self):
        doc = self.record()
        self.edit("helper_mod.py", "def tag():\n    return 'Z'\n")
        self.assertFalse(bc.entry_is_valid(doc))

    def test_a_file_run_py_tried_and_found_absent_invalidates_the_entry_when_it_appears(self):
        doc = self.record()
        self.assertTrue(bc.entry_is_valid(doc))
        self.edit("optional.txt", "appeared\n")
        self.assertFalse(bc.entry_is_valid(doc))

    def test_a_file_run_py_never_touched_does_not_invalidate_the_entry(self):
        doc = self.record()
        self.edit("unrelated.txt", "changed\n")
        self.assertTrue(bc.entry_is_valid(doc))

    def test_translator_modules_are_value_covered_so_they_are_not_file_dependencies(self):
        files = self.record()["files"]
        for name in ("xlate.py", "flow.py", "pdata.py"):
            self.assertFalse(any(os.path.basename(p) == name for p in files), name)

    def test_a_sys_exit_message_is_printed_and_exits_as_a_direct_run_does(self):
        script = os.path.join(self.dir, "exits.py")
        write(script, "import sys\nsys.exit('boom')\n")
        direct = subprocess.run([sys.executable, script], capture_output=True, text=True)
        wrapped = subprocess.run([sys.executable, BUILDCACHE, "record", self.deps, script], capture_output=True, text=True)
        self.assertEqual((wrapped.returncode, wrapped.stderr), (direct.returncode, direct.stderr))
        self.assertEqual(wrapped.stderr, "boom\n")

    def test_an_uncaught_exception_prints_the_traceback_of_run_py_alone(self):
        script = os.path.join(self.dir, "raises.py")
        write(script, "def inner():\n    raise KeyError('k')\n\n\ninner()\n")
        direct = subprocess.run([sys.executable, script], capture_output=True, text=True)
        wrapped = subprocess.run([sys.executable, BUILDCACHE, "record", self.deps, script], capture_output=True, text=True)
        self.assertEqual(wrapped.returncode, direct.returncode)
        self.assertEqual(wrapped.stderr, direct.stderr)

    def test_the_translator_value_run_py_reads_is_part_of_the_entry(self):
        doc = self.record()
        self.assertEqual(doc["ccmap"], bc.translator_digest())
        self.assertFalse(bc.entry_is_valid(dict(doc, ccmap="not the table")))

    def test_a_decode_that_can_no_longer_be_recomputed_invalidates_the_entry(self):
        doc = self.record()
        missing = os.path.join(self.dir, "no-image.bin")
        self.assertFalse(bc.entry_is_valid(dict(doc, decode=[[missing, 0x140001000, "0"]])))


class CorpusKey(unittest.TestCase):
    def test_the_key_is_stable_and_covers_the_args_and_every_input(self):
        inputs = {"image": "a", "spec": "b", "seed": None, "bank": None, "run.py": "c"}
        key = bc.corpus_key(["--count", "300"], inputs)
        self.assertEqual(key, bc.corpus_key(["--count", "300"], dict(inputs)))
        self.assertNotEqual(key, bc.corpus_key(["--count", "301"], inputs))
        for name in inputs:
            self.assertNotEqual(key, bc.corpus_key(["--count", "300"], dict(inputs, **{name: "changed"})), name)


if __name__ == "__main__":
    unittest.main()
