#!/usr/bin/env python3
# Behaviour tests for tools/dev/parity.py: the log parser, the per-20-in-play-minute scaling, the
# seed summary, the PES distance and the per-landing row. Run by ctest (parity_metrics_test).
import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tools", "dev"))
import parity  # noqa: E402

# 20 in-play minutes at the install's 54 Hz logic frames.
TWENTY_MINUTES_TICKS = 20 * 60 * 54
DOC = (
    "# PES-vs-GF parity metrics\n\n"
    "|landing (main SHA)|date|M1 dist|M2 dist|M3-M7 dist|M8|M9|M10-M14 dist|mean dist|\n"
    "|---|---|---|---|---|---|---|---|---|\n"
)


def pairs(counts):
    return " ".join("%d:%d" % kv for kv in sorted(counts.items()))


def gf_output(live=TWENTY_MINUTES_TICKS, states=None, stop=None, restarts=(), stoppages=0,
              longest_s=0.0, actions=None, determinism="00000000000000aa"):
    counts = {"passes": (0, 0), "crosses": (0, 0), "shots": (0, 0), "tackles": (0, 0), "slides": (0, 0)}
    counts.update(actions or {})
    lines = [
        "[determinism] %s" % determinism,
        "[restarts] %d stoppages, longest %.1f s (%d ticks), bound 60.0 s (3240 ticks)"
        % (stoppages, longest_s, round(longest_s * 54)),
        "[ticks] %d sim ticks to full time (cap 3240000)" % (live + 1000),
        "[parity-state] ticks %d live %d | %s" % (live + 1000, live, pairs(states or {})),
        "[parity-stop] %s" % pairs(stop or {}),
    ]
    lines += ["[parity-restart] %s whistle=0 release=%d frames=%d" % (kind, f, f) for kind, f in restarts]
    lines.append("[parity-actions] " + " ".join(
        "%s %d-%d" % (k, *counts[k]) for k in ("passes", "crosses", "shots", "tackles", "slides")))
    return "\n".join(lines) + "\n"


class ParseAndScale(unittest.TestCase):
    def test_movestep_share_is_of_all_player_ticks(self):
        v = parity.seed_values(parity.parse_run(gf_output(states={1: 30, 3: 60, 8: 10})))
        self.assertAlmostEqual(v["M1:1"], 30.0)
        self.assertAlmostEqual(v["M1:3"], 60.0)
        self.assertAlmostEqual(v["M1:8"], 10.0)

    def test_counts_scale_to_twenty_in_play_minutes(self):
        # 40 in-play minutes of play: the per-20-minute rate is half the raw count.
        run = parity.parse_run(gf_output(live=2 * TWENTY_MINUTES_TICKS, stoppages=8,
                                         actions={"passes": (10, 6)}))
        v = parity.seed_values(run)
        self.assertAlmostEqual(v["M8"], 4.0)
        self.assertAlmostEqual(v["M10"], 8.0)

    def test_stop_entries_are_counted_by_predecessor(self):
        v = parity.seed_values(parity.parse_run(gf_output(stop={1: 5, 3: 2})))
        self.assertAlmostEqual(v["M2:1"], 5.0)
        self.assertAlmostEqual(v["M2:3"], 2.0)
        self.assertNotIn("M2:8", v)

    def test_longest_stoppage_and_hash_come_from_the_run_output(self):
        run = parity.parse_run(gf_output(longest_s=42.0, determinism="00ff"))
        self.assertAlmostEqual(parity.seed_values(run)["M9"], 42.0)
        self.assertEqual(run.determinism, "00ff")

    def test_output_without_parity_lines_is_rejected(self):
        with self.assertRaises(ValueError):
            parity.parse_run("[determinism] 00ff\n[restarts] 1 stoppages, longest 1.0 s (54 ticks)\n")


class Restarts(unittest.TestCase):
    def test_median_frames_per_restart_type_with_n(self):
        run = parity.parse_run(gf_output(restarts=[
            ("throwin", 100), ("throwin", 300), ("throwin", 200), ("corner", 400), ("corner", 100)]))
        v = parity.seed_values(run)
        self.assertAlmostEqual(v["M4"], 200.0)
        self.assertAlmostEqual(v["M6"], 250.0)
        self.assertEqual(v["n:M4"], 3)
        self.assertEqual(v["n:M6"], 2)
        self.assertNotIn("M3", v)  # no kickoff in this run


class Summary(unittest.TestCase):
    def test_mean_and_range_over_seeds(self):
        s = parity.summarise([{"M9": 1.0}, {"M9": 3.0}, {"M9": 2.0}])
        self.assertEqual((s["M9"].mean, s["M9"].lo, s["M9"].hi), (2.0, 1.0, 3.0))

    def test_key_missing_in_a_seed_is_averaged_over_the_seeds_that_have_it(self):
        s = parity.summarise([{"M3": 100.0}, {}, {"M3": 300.0}])
        self.assertEqual(s["M3"].mean, 200.0)
        self.assertEqual(s["M3"].seeds, 2)


class Distance(unittest.TestCase):
    def test_pending_baseline_gives_no_values(self):
        self.assertEqual(parity.pes_baseline("|M1-M14|pending (round 36)||\n"), {})

    def test_relative_distance_and_mean_over_metrics(self):
        pes = parity.pes_baseline("|M8|10|one match|\n|M9|20|one match|\n")
        d = parity.distances({"M8": 12.0, "M9": 15.0}, pes)
        self.assertAlmostEqual(d["M8"], 0.2)
        self.assertAlmostEqual(d["M9"], 0.25)
        self.assertAlmostEqual(parity.mean_distance(d), 0.225)

    def test_state_rows_are_averaged_into_metric_m1(self):
        pes = parity.pes_baseline("|M1:1|10|x|\n|M1:3|40|x|\n")
        d = parity.distances({"M1:1": 15.0, "M1:3": 30.0}, pes)
        self.assertAlmostEqual(parity.metric_distances(d)["M1"], (0.5 + 0.25) / 2)


class Record(unittest.TestCase):
    def test_rerecording_a_landing_replaces_its_row(self):
        once = parity.record_row(DOC, "860e2f1", "2026-10-21", {})
        twice = parity.record_row(once, "860e2f1", "2026-10-21", {})
        rows = [line for line in twice.splitlines() if line.startswith("|860e2f1|")]
        self.assertEqual(rows, ["|860e2f1|2026-10-21|pending|pending|pending|pending|pending|pending|pending|"])

    def test_row_carries_percent_distances_when_pes_is_filled(self):
        text = parity.record_row(DOC, "abc1234", "2026-10-21", {"M8": 0.2, "M9": 0.25})
        row = [line for line in text.splitlines() if line.startswith("|abc1234|")][0]
        cells = row.strip("|").split("|")
        self.assertEqual(len(cells), 9)
        self.assertEqual(cells[2], "pending")  # M1: no value
        self.assertEqual(cells[5], "20.0%")  # M8
        self.assertEqual(cells[6], "25.0%")  # M9
        self.assertEqual(cells[8], "22.5%")  # mean over the metrics with a value


if __name__ == "__main__":
    unittest.main()
