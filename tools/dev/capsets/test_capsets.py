#!/usr/bin/env python3
"""Tests of the capture-set registry: validation, the slim and pack refusal (auto_slim, capz), the broken-link check and the
oracle manifests. Small fixture trees under GF_SCRATCH (default ~/.cache): no PES data, no capture bytes.
"""
import json
import os
import shutil
import sys
import tempfile
import unittest

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

    def capture(self, launch, seq, mem_bytes=b"x", hook=HOOK):
        d = os.path.join(self.live2, launch, seq)
        os.makedirs(d)
        with open(os.path.join(d, "mem.bin"), "wb") as f:
            f.write(mem_bytes)
        with open(os.path.join(d, "call.bin"), "wb") as f:
            f.write(b"c")
        with open(os.path.join(d, "index.json"), "w") as f:
            json.dump({"rv": {"hook": hook}}, f)
        return d

    def reg(self, sets):
        return {"version": 1, "live2": self.live2, "sets": sets}

    def set_(self, name, seqs, status="cited", launch="pid1"):
        return {"name": name, "hook": HOOK, "launch": launch, "seqs": seqs, "status": status, "owner": "test",
                "doc": "docs/test.md"}


class RegistryTest(Fixture):
    def test_a_bad_status_is_refused(self):
        bad = self.reg([self.set_("a", ["seq1"], status="maybe")])
        with self.assertRaises(registry.RegistryError):
            registry.validate(bad)

    def test_a_duplicate_set_name_is_refused(self):
        bad = self.reg([self.set_("a", ["seq1"]), self.set_("a", ["seq2"])])
        with self.assertRaises(registry.RegistryError):
            registry.validate(bad)

    def test_a_seq_must_be_one_folder_name(self):
        bad = self.reg([self.set_("a", ["../seq1"])])
        with self.assertRaises(registry.RegistryError):
            registry.validate(bad)

    def test_guarded_dirs_are_the_active_and_protected_captures_only(self):
        reg = self.reg([self.set_("act", ["seq1"], "active"), self.set_("cit", ["seq2"], "cited"),
                        self.set_("pro", ["seq3"], "protected")])
        guarded = registry.guarded_dirs(registry.validate(reg))
        self.assertIn(os.path.realpath(os.path.join(self.live2, "pid1", "seq1")), guarded)
        self.assertIn(os.path.realpath(os.path.join(self.live2, "pid1", "seq3")), guarded)
        self.assertNotIn(os.path.realpath(os.path.join(self.live2, "pid1", "seq2")), guarded)


class PackRefusalTest(Fixture):
    def test_pack_refuses_a_guarded_capture_and_leaves_its_mem_alone(self):
        d = self.capture("pid1", "seq1")
        self.assertEqual(capz.pack(d, guarded={os.path.realpath(d)}), "refused")
        self.assertTrue(os.path.exists(os.path.join(d, "mem.bin")))
        self.assertFalse(os.path.exists(os.path.join(d, "mem.bin.zst")))

    def test_pack_does_not_refuse_a_cited_capture(self):
        d = self.capture("pid1", "seq2")
        self.assertEqual(capz.pack(d, guarded=set()), "skip")  # small mem.bin: not packed, but not refused either


class SlimRefusalTest(Fixture):
    def setUp(self):
        super().setUp()
        self.saved = auto_slim.FULL_BYTES
        auto_slim.FULL_BYTES = 0  # the fixture's files are small: every capture with a mem.bin counts as full
        self.guarded_d = self.capture("pid1", "seq1")
        self.cited_d = self.capture("pid1", "seq2")

    def tearDown(self):
        auto_slim.FULL_BYTES = self.saved
        super().tearDown()

    def test_candidates_skip_a_guarded_capture(self):
        got = auto_slim.candidates(self.live2, 1, set(), 0, set(), {os.path.realpath(self.guarded_d)})
        self.assertEqual(got, [self.cited_d])

    def test_stale_full_skips_a_guarded_capture(self):
        got = auto_slim.stale_full(self.live2, 1, 0, frozenset(), {os.path.realpath(self.guarded_d)})
        self.assertEqual(got, [self.cited_d])


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
        got = check.broken_links([root], 4)
        self.assertEqual(got, [os.path.join(root, "x1", "mem.bin")])

    def test_missing_captures_names_the_absent_folders(self):
        self.capture("pid1", "seq1")
        reg = registry.validate(self.reg([self.set_("a", ["seq1", "seq9"])]))
        self.assertEqual(check.missing_captures(reg), {"a": [os.path.join(self.live2, "pid1", "seq9")]})

    def test_an_invalid_registry_exits_2(self):
        bad = os.path.join(self.tmp, "bad.json")
        with open(bad, "w") as f:
            json.dump({"version": 2}, f)
        self.assertEqual(check.main(["--registry", bad, "--root", self.tmp]), 2)


class OracleManifestTest(Fixture):
    def test_the_manifest_names_real_paths_and_writes_no_symlink(self):
        d = self.capture("pid1", "seq1")
        path = os.path.join(self.tmp, "reg.json")
        with open(path, "w") as f:
            json.dump(self.reg([self.set_("a", ["seq1"])]), f)
        out = os.path.join(self.tmp, "oracle_a")
        self.assertEqual(gen_oracle.main(["--set", "a", "--out", out, "--registry", path]), 0)
        self.assertEqual(os.listdir(out), ["manifest.json"])
        with open(os.path.join(out, "manifest.json")) as f:
            m = json.load(f)
        self.assertEqual(m["captures"][0]["mem"], os.path.join(d, "mem.bin"))
        self.assertFalse(os.path.islink(os.path.join(out, "manifest.json")))

    def test_a_missing_capture_fails_and_writes_nothing(self):
        path = os.path.join(self.tmp, "reg.json")
        with open(path, "w") as f:
            json.dump(self.reg([self.set_("a", ["seq1"])]), f)
        out = os.path.join(self.tmp, "oracle_a")
        self.assertEqual(gen_oracle.main(["--set", "a", "--out", out, "--registry", path]), 1)
        self.assertFalse(os.path.exists(out))


if __name__ == "__main__":
    unittest.main()
