#!/usr/bin/env python3
"""test_goal_cast.py: the goal scene's cast map (src/pes/goal_cast.hpp) against the proc_camera captures of PES goals.

For every capture of GF_CAST_SEQS (path list, ':' separated; each a proc_camera_141F7BFE0 capture dir under the rendezvous
live2 tree) it unpacks the capture (capz.py), reads the FixDemo player's human map and the scene manager's cast list, checks

  * the cast entry of fdc slot 21 (id 0x1315), when the list holds it, is (side = scorer side ^ 1, roster slot 0), the
    scorer's side being that of entry 0x1300;
  * the human map's actor of fdc slot 0 is the scorer: key = side * 11 + roster slot of entry 0x1300;
  * the human map's actor of fdc slot 22 (a referee), when present, has key 22 and roster member 0;
  * every 0x13xx entry's roster slot is the member of its human map actor, key = side * 11 + roster slot (outfield and keeper).

and packs the capture again. Skips without captures; fails when no capture held a slot-21 entry (the rule would be unproven).
Usage: GF_CAST_SEQS=<dir>:<dir> PES_EXE=<pes_full.bin> GF_HOOKS_CFG=<hooks.cfg> <venv python> test_goal_cast.py
"""
import os
import subprocess
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
CAPZ = os.path.normpath(os.path.join(HERE, "..", "rendezvous", "capz.py"))
SIDE_SLOTS = 11


def capture_dirs():
    return [d for d in os.environ.get("GF_CAST_SEQS", "").split(":") if d]


def human_map(sm, cam, dump):
    """{fdc slot: (actor key, roster member)} of the FixDemo player behind the camera (fixdemo_player_dump.player, silenced)."""
    import io
    import contextlib
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        dump.player(sm, cam)
    out = {}
    for line in buf.getvalue().splitlines():
        line = line.strip()
        if line.startswith("fdc Human slot"):
            words = line.split()
            out[int(words[3])] = (int(words[7]), int(words[10]))
    return out


@unittest.skipUnless(capture_dirs() and os.environ.get("PES_EXE") and os.environ.get("GF_HOOKS_CFG"),
                     "GF_CAST_SEQS, PES_EXE and GF_HOOKS_CFG not set")
class GoalCastAgainstCaptures(unittest.TestCase):
    def test_slot_map(self):
        import fixdemo_player_dump as dump
        orig = dump.ft.hook_bytes(os.environ["GF_HOOKS_CFG"])
        keeper_entries = 0
        for seq in capture_dirs():
            subprocess.run([sys.executable, CAPZ, "unpack", seq], check=True, stdout=subprocess.DEVNULL)
            try:
                sm = dump.fs.SnapMem(seq, restore=orig)
                cam = dump.ft.load_recorded_call(seq)["gpr"]["rcx"]
                humans = human_map(sm, cam, dump)
                for _, ents in dump.cast_lists(sm):
                    by_id = {e[0]: e for e in ents}
                    if 0x1300 not in by_id:
                        continue
                    _, _, scorer_side, scorer_roster = by_id[0x1300]
                    if 0 in humans:
                        self.assertEqual(humans[0], (scorer_side * SIDE_SLOTS + scorer_roster, scorer_roster), seq)
                    if 0x1315 in by_id:
                        keeper_entries += 1
                        self.assertEqual(by_id[0x1315][2:], (scorer_side ^ 1, 0), "conceding keeper entry of " + seq)
                    for ident, _, side, roster in ents:
                        slot = ident - 0x1300
                        if 0x1300 <= ident < 0x1316 and slot in humans:
                            self.assertEqual(humans[slot], (side * SIDE_SLOTS + roster, roster), "slot %d of %s" % (slot, seq))
                if 22 in humans:
                    self.assertEqual(humans[22], (22, 0), "referee of " + seq)
            finally:
                subprocess.run([sys.executable, CAPZ, "pack", seq], check=True, stdout=subprocess.DEVNULL)
        self.assertGreater(keeper_entries, 0, "no capture held the conceding keeper's cast entry")


if __name__ == "__main__":
    unittest.main()
