#!/usr/bin/env python3
"""GF_PES_SURVEY at the untranslated-dispatch site, on the generated dispatch TU (gen_dispatch.text) and pes_survey.hpp,
built with a probe that makes a direct and an indirect call to targets the table lacks.

Unset: a missing target aborts as before (a recovery, when installed, runs first). Set: every hit is counted in the list
(hits, first logic frame, caller offset), a recovery runs first and the run goes on, and the process exits with the survey
status, so a survey run never passes as a smoke. Needs a C++ compiler ($CXX, else c++)."""
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

DIRECT_A, DIRECT_B, INDIRECT_C = 0x1234, 0x5678, 0x9ABC
PROBE_FRAME = 7
SURVEY_EXIT = 79  # kPesSurveyExitCode in pes_survey.hpp

PROBE = r"""
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "pes_state.hpp"
#include "pes_dispatch.hpp"
#include "pes_survey.hpp"

struct Recovered { uint64_t target; };
static void Recover(uint64_t t) { throw Recovered{t}; }

int main(int argc, char** argv) {
  const char* mode = argc > 1 ? argv[1] : "plain";
  const bool recover = std::strcmp(mode, "recover") == 0;
  const bool clean = std::strcmp(mode, "clean") == 0;
  pes_survey_set_frame(%(frame)d);
  if (recover) pes_dispatch_missing_handler = &Recover;
  PesState s;
  if (!clean) {
    for (uint64_t t : {%(a)#xULL, %(b)#xULL, %(a)#xULL}) {
      try {
        pes_dispatch_missing(t);
        std::printf("returned %%#llx\n", (unsigned long long)t);
      } catch (const Recovered& r) {
        std::printf("recovered %%#llx\n", (unsigned long long)r.target);
      }
    }
    try {
      PesDispatch d = pes_dispatch_lookup(%(c)#xULL);
      if (d.fn == nullptr) pes_dispatch_missing(%(c)#xULL);  // the translated caller's own check
      d.fn(s);
      std::printf("called %%#llx\n", (unsigned long long)%(c)#xULL);
    } catch (const Recovered& r) {
      std::printf("recovered %%#llx\n", (unsigned long long)r.target);
    }
  }
  std::printf("done\n");
  return 0;
}
""" % {"frame": PROBE_FRAME, "a": DIRECT_A, "b": DIRECT_B, "c": INDIRECT_C}


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
            f.write(gen_dispatch.text([]))  # an empty table: every probe target is one the table lacks
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

    def run_probe(self, mode, survey=None):
        env = {k: v for k, v in os.environ.items() if k != "GF_PES_SURVEY"}
        if survey is not None:
            env["GF_PES_SURVEY"] = os.path.join(self.dir.name, survey)
        return subprocess.run([self.exe, mode], env=env, capture_output=True, text=True, timeout=60)

    def list_rows(self, name):
        return read_list(os.path.join(self.dir.name, name))

    def test_unset_plain_aborts_at_the_first_direct_target(self):
        r = self.run_probe("plain")
        self.assertEqual(r.returncode, -signal.SIGABRT, r.stderr)
        self.assertIn("PES_DISPATCH_UNTRANSLATED: 0x%x" % DIRECT_A, r.stderr)
        self.assertEqual(r.stdout, "")

    def test_unset_recover_runs_the_recovery_for_direct_and_indirect_targets(self):
        r = self.run_probe("recover")
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(r.stdout.split(), ["recovered", "0x1234", "recovered", "0x5678", "recovered", "0x1234",
                                            "recovered", "0x9abc", "done"])

    def test_set_plain_lists_every_hit_and_the_run_goes_on(self):
        r = self.run_probe("plain", survey="plain.txt")
        self.assertEqual(r.returncode, SURVEY_EXIT, r.stderr)
        self.assertNotIn("PES_DISPATCH_UNTRANSLATED", r.stderr)
        self.assertEqual(r.stdout.split(), ["returned", "0x1234", "returned", "0x5678", "returned", "0x1234",
                                            "called", "0x9abc", "done"])
        rows = self.list_rows("plain.txt")
        self.assertEqual(set(rows), {DIRECT_A, DIRECT_B, INDIRECT_C})
        self.assertEqual(rows[DIRECT_A][:2], (2, PROBE_FRAME))
        self.assertEqual(rows[DIRECT_B][:2], (1, PROBE_FRAME))
        self.assertEqual(rows[INDIRECT_C][:2], (1, PROBE_FRAME))
        self.assertTrue(all(caller > 0 for _, _, caller in rows.values()))

    def test_set_recover_runs_the_recovery_first_and_counts_every_hit(self):
        r = self.run_probe("recover", survey="recover.txt")
        self.assertEqual(r.returncode, SURVEY_EXIT, r.stderr)
        self.assertEqual(r.stdout.split(), ["recovered", "0x1234", "recovered", "0x5678", "recovered", "0x1234",
                                            "recovered", "0x9abc", "done"])
        rows = self.list_rows("recover.txt")
        self.assertEqual({t: rows[t][0] for t in rows}, {DIRECT_A: 2, DIRECT_B: 1, INDIRECT_C: 1})

    def test_a_survey_run_with_no_hit_still_exits_with_the_survey_status(self):
        r = self.run_probe("clean", survey="clean.txt")
        self.assertEqual((r.returncode, r.stdout.split()), (SURVEY_EXIT, ["done"]))
        self.assertEqual(self.list_rows("clean.txt"), {})


if __name__ == "__main__":
    unittest.main()
