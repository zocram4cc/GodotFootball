#!/usr/bin/env python3
"""GF_PES_SURVEY at the untranslated-dispatch site, pinned on the generated dispatch TU (gen_dispatch.text) and
pes_survey.hpp, built with a probe. Unset, a missing target aborts as before. Set, every hit is listed (hits, first
logic frame, caller offset) and the process exits with a status that is neither a signal nor a pass, with or without a
recovery. A recovery that catches the target lets the run continue, and every hit is still counted. Needs $CXX (c++)."""
import os
import shutil
import signal
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
TRANSLATED = os.path.join(REPO, "src", "pes", "translated")
sys.path.insert(0, HERE)
import gen_dispatch  # noqa: E402

TARGET_A, TARGET_B = 0x1234, 0x5678
PROBE_FRAME = 7
SKIP_STATUS, TIMEOUT_STATUS = 77, 124  # ctest skip and timeout(1): a survey must not be mistaken for either

PROBE = r"""
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "pes_dispatch.hpp"
#include "pes_survey.hpp"

struct Recovered { uint64_t target; };
static void Recover(uint64_t t) { throw Recovered{t}; }
void pes_fn_140000100(PesState&) {}

int main(int argc, char** argv) {
  pes_survey_set_frame(%(frame)d);
  const bool recover = argc > 1 && std::strcmp(argv[1], "recover") == 0;
  if (!recover) pes_dispatch_missing(%(a)#x);  // plain: aborts, or exits with the survey status
  pes_dispatch_missing_handler = &Recover;
  for (uint64_t t : {%(a)#xULL, %(b)#xULL, %(a)#xULL}) {
    try {
      pes_dispatch_missing(t);
    } catch (const Recovered& r) {
      std::printf("recovered %%#llx\n", (unsigned long long)r.target);
    }
  }
  return 0;
}
""" % {"frame": PROBE_FRAME, "a": TARGET_A, "b": TARGET_B}


def read_list(path):
    rows = {}
    with open(path) as f:
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            target, hits, frame, caller = line.split()
            rows[int(target, 16)] = (int(hits), int(frame), int(caller, 16))
    return rows


class PesSurveyTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.dir = tempfile.TemporaryDirectory()
        tmp = cls.dir.name
        gen = os.path.join(tmp, "pes_dispatch_gen.cpp")
        with open(gen, "w") as f:
            f.write(gen_dispatch.text(["0x140000100"]))
        probe = os.path.join(tmp, "probe.cpp")
        with open(probe, "w") as f:
            f.write(PROBE)
        cls.exe = os.path.join(tmp, "probe")
        cxx = os.environ.get("CXX") or shutil.which("c++") or "c++"
        build = subprocess.run([cxx, "-std=c++20", "-O1", "-I", TRANSLATED, probe, gen, "-o", cls.exe],
                               capture_output=True, text=True)
        if build.returncode != 0:
            raise RuntimeError("probe build failed:\n" + build.stderr)

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def run_probe(self, args=(), survey=None):
        env = {k: v for k, v in os.environ.items() if k != "GF_PES_SURVEY"}
        if survey is not None:
            env["GF_PES_SURVEY"] = survey
        return subprocess.run([self.exe] + list(args), env=env, capture_output=True, text=True, timeout=60)

    def list_path(self, name):
        return os.path.join(self.dir.name, name)

    def assert_survey_status(self, rc):
        self.assertGreater(rc, 0)
        self.assertLess(rc, 128)  # not a signal
        self.assertNotIn(rc, (SKIP_STATUS, TIMEOUT_STATUS))

    def test_unset_aborts_as_before(self):
        r = self.run_probe()
        self.assertEqual(r.returncode, -signal.SIGABRT, r.stderr)
        self.assertIn("PES_DISPATCH_UNTRANSLATED: 0x%x" % TARGET_A, r.stderr)

    def test_set_lists_the_hit_and_exits_nonzero(self):
        out = self.list_path("plain.txt")
        r = self.run_probe(survey=out)
        self.assertNotIn("PES_DISPATCH_UNTRANSLATED", r.stderr)
        self.assert_survey_status(r.returncode)
        rows = read_list(out)
        self.assertEqual(set(rows), {TARGET_A})
        hits, frame, caller = rows[TARGET_A]
        self.assertEqual((hits, frame), (1, PROBE_FRAME))
        self.assertGreater(caller, 0)

    def test_recovery_continues_and_every_hit_is_counted(self):
        out = self.list_path("recover.txt")
        r = self.run_probe(["recover"], survey=out)
        self.assertEqual(r.stdout.split(), ["recovered", "0x1234", "recovered", "0x5678", "recovered", "0x1234"])
        self.assert_survey_status(r.returncode)
        rows = read_list(out)
        self.assertEqual({t: rows[t][0] for t in rows}, {TARGET_A: 2, TARGET_B: 1})

    def test_recovery_without_the_survey_is_unchanged(self):
        r = self.run_probe(["recover"])
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(r.stdout.split(), ["recovered", "0x1234", "recovered", "0x5678", "recovered", "0x1234"])


if __name__ == "__main__":
    unittest.main()
