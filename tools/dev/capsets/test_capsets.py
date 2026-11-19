#!/usr/bin/env python3
"""Tests of the capture-set registry: validation, the per-action refusals (slim, delete, pack), the standard run file, gone
marking, the broken-link check and the oracle manifests. Small fixture trees under GF_SCRATCH (default ~/.cache): no PES data.
"""
import json
import os
import shutil
import sys
import tempfile
import unittest
from unittest import mock

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "rendezvous"))
import auto_slim  # noqa: E402
import capz  # noqa: E402
import check  # noqa: E402
import gen_oracle  # noqa: E402
import registry  # noqa: E402

HOOK = "trapopen_14079A470"


def scratch_root():
    root = os.environ.get("GF_SCRATCH") or os.path.expanduser("~/.cache/gf-capsets-test")
    os.makedirs(root, exist_ok=True)
    return root


class Fixture(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix="capsets-", dir=scratch_root())
        self.live2 = os.path.join(self.tmp, "live2")
        os.makedirs(self.live2)

    def tearDown(self):
        shutil.rmtree(self.tmp, ignore_errors=True)

    def capture(self, launch, seq, hook=HOOK):
        d = os.path.join(self.live2, launch, seq)
        os.makedirs(d)
        with open(os.path.join(d, "mem.bin"), "wb") as f:
            f.write(b"x")
        with open(os.path.join(d, "call.bin"), "wb") as f:
            f.write(b"c")
        with open(os.path.join(d, "index.json"), "w") as f:
            json.dump({"rv": {"hook": hook}}, f)
        return d

    def make_reg(self, sets):
        return {"version": 1, "live2": self.live2, "sets": sets}

    def set_(self, name, seqs, status="cited", launch="pid1"):
        return {"name": name, "hook": HOOK, "launch": launch, "seqs": seqs, "status": status, "owner": "test",
                "doc": "docs/test.md"}

    def write_reg(self, reg, name="reg.json"):
        path = os.path.join(self.tmp, name)
        with open(path, "w") as f:
            json.dump(reg, f)
        return path


class RegistryTest(Fixture):
    def test_a_bad_status_is_refused(self):
        with self.assertRaises(registry.RegistryError):
            registry.validate(self.make_reg([self.set_("a", ["seq1"], status="maybe")]))

    def test_a_duplicate_set_name_is_refused(self):
        with self.assertRaises(registry.RegistryError):
            registry.validate(self.make_reg([self.set_("a", ["seq1"]), self.set_("a", ["seq2"])]))

    def test_a_seq_must_be_one_folder_name(self):
        with self.assertRaises(registry.RegistryError):
            registry.validate(self.make_reg([self.set_("a", ["../seq1"])]))

    def test_slim_and_delete_refuse_active_and_protected_only(self):
        reg = registry.validate(self.make_reg([self.set_("act", ["seq1"], "active"), self.set_("cit", ["seq2"], "cited"),
                                               self.set_("pro", ["seq3"], "protected"), self.set_("gon", ["seq4"], "gone")]))
        here = lambda q: os.path.realpath(os.path.join(self.live2, "pid1", q))  # noqa: E731
        for action in ("slim", "delete"):
            guarded = registry.guarded_dirs(reg, action)
            self.assertEqual(guarded, {here("seq1"), here("seq3")}, action)

    def test_pack_refuses_active_only(self):
        reg = registry.validate(self.make_reg([self.set_("act", ["seq1"], "active"), self.set_("pro", ["seq2"], "protected"),
                                               self.set_("cit", ["seq3"], "cited")]))
        self.assertEqual(registry.guarded_dirs(reg, "pack"), {os.path.realpath(os.path.join(self.live2, "pid1", "seq1"))})

    def test_an_unknown_action_is_refused(self):
        reg = registry.validate(self.make_reg([self.set_("a", ["seq1"])]))
        with self.assertRaises(ValueError):
            registry.guarded_dirs(reg, "shred")


class StandardPathTest(unittest.TestCase):
    def test_the_standard_run_file_sits_beside_the_repository_checkout(self):
        p = registry.default_path()
        self.assertEqual(os.path.basename(p), "capsets.json")
        self.assertEqual(os.path.basename(os.path.dirname(p)), "run")
        self.assertEqual(os.path.basename(os.path.dirname(os.path.dirname(p))), "gf-Rendezvous-scratch")

    def test_unset_the_environment_gives_the_standard_file(self):
        with mock.patch.dict(os.environ, {}, clear=False):
            os.environ.pop(registry.ENV, None)
            self.assertEqual(registry.path_of(), registry.default_path())

    def test_the_environment_names_another_file(self):
        with mock.patch.dict(os.environ, {registry.ENV: "/x/y.json"}):
            self.assertEqual(registry.path_of(), "/x/y.json")
            self.assertEqual(registry.path_of("/z.json"), "/z.json")


class MarkGoneTest(Fixture):
    def test_an_entry_with_no_capture_on_disk_becomes_gone_and_keeps_its_doc(self):
        reg = registry.validate(self.make_reg([self.set_("a", ["seq1"], "protected")]))
        new, changes = registry.mark_gone(reg, exists=lambda p: False)
        self.assertEqual(new["sets"][0]["status"], "gone")
        self.assertEqual(new["sets"][0]["doc"], "docs/test.md")
        self.assertEqual(changes, [("a", "gone", 1)])

    def test_a_partly_present_entry_is_split_and_its_missing_part_is_gone(self):
        reg = registry.validate(self.make_reg([self.set_("a", ["seq1", "seq2"], "protected")]))
        new, changes = registry.mark_gone(reg, exists=lambda p: p.endswith("seq1"))
        got = [(s["name"], s["status"], s["seqs"]) for s in new["sets"]]
        self.assertEqual(got, [("a", "protected", ["seq1"]), ("a-gone", "gone", ["seq2"])])
        self.assertEqual(changes, [("a", "split", 1)])

    def test_a_present_entry_and_a_gone_entry_are_untouched(self):
        reg = registry.validate(self.make_reg([self.set_("a", ["seq1"], "protected"), self.set_("g", ["seq9"], "gone")]))
        new, changes = registry.mark_gone(reg, exists=lambda p: True)
        self.assertEqual(new, reg)
        self.assertEqual(changes, [])

    def test_the_cli_prints_the_changes_and_writes_only_with_write(self):
        self.capture("pid1", "seq1")
        path = self.write_reg(self.make_reg([self.set_("a", ["seq1", "seq2"])]))
        self.assertEqual(registry.main(["mark-gone", "--registry", path]), 0)
        with open(path) as f:
            self.assertEqual(len(json.load(f)["sets"]), 1)  # no --write: unchanged
        self.assertEqual(registry.main(["mark-gone", "--registry", path, "--write"]), 0)
        with open(path) as f:
            got = [(s["name"], s["status"]) for s in json.load(f)["sets"]]
        self.assertEqual(got, [("a", "cited"), ("a-gone", "gone")])


class PackRefusalTest(Fixture):
    def setUp(self):
        super().setUp()
        self.saved = capz.SLIM_BYTES
        capz.SLIM_BYTES = 0  # the fixture's files are small: every capture counts as big enough to pack

    def tearDown(self):
        capz.SLIM_BYTES = self.saved
        super().tearDown()

    def test_pack_refuses_an_active_capture_and_leaves_it_alone(self):
        d = self.capture("pid1", "seq1")
        reg = registry.validate(self.make_reg([self.set_("act", ["seq1"], "active")]))
        self.assertEqual(capz.pack(d, guarded=registry.guarded_dirs(reg, "pack")), "refused")
        self.assertTrue(os.path.exists(os.path.join(d, "mem.bin")))
        self.assertFalse(os.path.exists(os.path.join(d, "mem.bin.zst")))

    def test_pack_packs_a_protected_capture(self):
        d = self.capture("pid1", "seq2")
        reg = registry.validate(self.make_reg([self.set_("pro", ["seq2"], "protected")]))
        self.assertEqual(capz.pack(d, guarded=registry.guarded_dirs(reg, "pack")), "packed")
        self.assertTrue(os.path.exists(os.path.join(d, "mem.bin.zst")))
        self.assertFalse(os.path.exists(os.path.join(d, "mem.bin")))


class SlimRefusalTest(Fixture):
    def setUp(self):
        super().setUp()
        self.saved = auto_slim.FULL_BYTES
        auto_slim.FULL_BYTES = 0  # the fixture's files are small: every capture with a mem.bin counts as full
        self.act = self.capture("pid1", "seq1")
        self.prot = self.capture("pid1", "seq2")
        self.cited = self.capture("pid1", "seq3")
        self.reg = registry.validate(self.make_reg([self.set_("act", ["seq1"], "active"),
                                                    self.set_("pro", ["seq2"], "protected"),
                                                    self.set_("cit", ["seq3"], "cited")]))

    def tearDown(self):
        auto_slim.FULL_BYTES = self.saved
        super().tearDown()

    def test_candidates_slim_only_the_cited_capture(self):
        got = auto_slim.candidates(self.live2, 1, set(), 0, set(), registry.guarded_dirs(self.reg, "slim"))
        self.assertEqual(got, [self.cited])

    def test_stale_full_packs_protected_and_cited_but_not_active(self):
        got = auto_slim.stale_full(self.live2, 1, 0, frozenset(), registry.guarded_dirs(self.reg, "pack"))
        self.assertEqual(sorted(got), sorted([self.prot, self.cited]))


class CheckTest(Fixture):
    def test_broken_links_counts_dangling_mem_bin_links_within_depth_four(self):
        root = os.path.join(self.tmp, "root")
        os.makedirs(os.path.join(root, "x1"))
        os.symlink(os.path.join(root, "gone", "mem.bin"), os.path.join(root, "x1", "mem.bin"))  # depth 2: counted
        os.makedirs(os.path.join(root, "ok"))
        with open(os.path.join(root, "ok", "target.bin"), "w") as f:
            f.write("t")
        os.symlink(os.path.join(root, "ok", "target.bin"), os.path.join(root, "ok", "mem.bin"))  # resolves: not counted
        os.makedirs(os.path.join(root, "deep", "x", "y", "z"))
        os.symlink(os.path.join(root, "gone", "mem.bin"), os.path.join(root, "deep", "x", "y", "z", "mem.bin"))  # depth 5
        self.assertEqual(check.broken_links([root], 4), [os.path.join(root, "x1", "mem.bin")])

    def test_missing_captures_names_absent_folders_of_live_sets_only(self):
        self.capture("pid1", "seq1")
        reg = registry.validate(self.make_reg([self.set_("a", ["seq1", "seq9"]), self.set_("g", ["seq8"], "gone")]))
        self.assertEqual(check.missing_captures(reg), {"a": [os.path.join(self.live2, "pid1", "seq9")]})
        self.assertEqual(check.gone_sets(reg), {"g": 1})

    def test_the_report_counts_gone_captures_apart_and_exits_zero(self):
        self.capture("pid1", "seq1")
        path = self.write_reg(self.make_reg([self.set_("a", ["seq1"]), self.set_("g", ["seq8", "seq9"], "gone")]))
        self.assertEqual(check.main(["--registry", path, "--root", self.tmp]), 0)

    def test_an_invalid_registry_exits_2(self):
        bad = os.path.join(self.tmp, "bad.json")
        with open(bad, "w") as f:
            json.dump({"version": 2}, f)
        self.assertEqual(check.main(["--registry", bad, "--root", self.tmp]), 2)


class OracleManifestTest(Fixture):
    def test_the_manifest_names_real_paths_and_writes_no_symlink(self):
        d = self.capture("pid1", "seq1")
        path = self.write_reg(self.make_reg([self.set_("a", ["seq1"])]))
        out = os.path.join(self.tmp, "oracle_a")
        self.assertEqual(gen_oracle.main(["--set", "a", "--out", out, "--registry", path]), 0)
        self.assertEqual(os.listdir(out), ["manifest.json"])
        with open(os.path.join(out, "manifest.json")) as f:
            m = json.load(f)
        self.assertEqual(m["captures"][0]["mem"], os.path.join(d, "mem.bin"))
        self.assertFalse(os.path.islink(os.path.join(out, "manifest.json")))

    def test_a_missing_capture_fails_and_writes_nothing(self):
        path = self.write_reg(self.make_reg([self.set_("a", ["seq1"])]))
        out = os.path.join(self.tmp, "oracle_a")
        self.assertEqual(gen_oracle.main(["--set", "a", "--out", out, "--registry", path]), 1)
        self.assertFalse(os.path.exists(out))

    def test_a_gone_set_has_no_oracle(self):
        self.capture("pid1", "seq1")
        path = self.write_reg(self.make_reg([self.set_("a", ["seq1"], "gone")]))
        out = os.path.join(self.tmp, "oracle_a")
        self.assertEqual(gen_oracle.main(["--set", "a", "--out", out, "--registry", path]), 1)
        self.assertFalse(os.path.exists(out))


if __name__ == "__main__":
    unittest.main()
