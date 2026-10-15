# Pose veto on PES clips: decision (PoseVeto)

Commit `7cc9917` on `refs/heads/PoseVeto`. Comment-only: the veto stays, and the ponytail note beside `pesClip` (humanoid.cpp) now carries the table. Gate: 691 tests, 100% passed, two standing skips (VfsInstall, EditSaveInstall). Smoke seed 7 unchanged (passes 56-47, dodges 1162-1129, forced 11305, determinism 95515192750cd51b). check_pii passes.

## Measured: 5 seeds, 20 min, clean builds

Veto on = main. Veto off = byIncomingVelocity, bySide and byIncomingBodyDirection dropped for pesClip.

| seed | passes on | passes off | dodge on | dodge off | forced on | forced off | goals on | goals off | shots on | shots off |
|---|---|---|---|---|---|---|---|---|---|---|
| 7 | 56-47 | 39-35 | 1162-1129 | 1753-1456 | 11305 | 12093 | 2-5 | 1-5 | 11-15 | 4-18 |
| 8 | 51-43 | 33-33 | 1177-1083 | 1735-1532 | 13895 | 14159 | 3-6 | 1-3 | 10-15 | 7-7 |
| 9 | 37-49 | 24-23 | 890-1156 | 1582-1658 | 16492 | 10798 | 7-4 | 3-0 | 23-8 | 8-4 |
| 11 | 55-52 | 15-17 | 1216-1439 | 1440-1253 | 17945 | 15124 | 7-6 | 0-1 | 24-18 | 2-6 |
| 1000 | 53-38 | 37-34 | 1059-1184 | 1342-1323 | 13888 | 11556 | 7-2 | 0-2 | 19-12 | 4-8 |

Totals: passes 481 -> 290; goals 49 -> 16; shots 155 -> 68. Passes fall on every seed. Played dodges rise on every seed.

Tackles (pes-clips) and challenges are in the ponytail note.

## Why passing falls (instrumented runs, not committed)

- Played dodge clips, seed 7 veto off (3209): avoidjump 2149, avoidslide 1044, goalnet 16. Veto on (2291): avoidjump 1222, avoidslide 1059, goalnet 10. No ball or post dodge clip played in either run.
- Team in possession (designated possessor is the dodger or a teammate) is 65% of played dodges with the veto off (seed 7: 2070 of 3209).
- Seed 7 with the veto kept for the carrier only: passes 101 (on 103, off 74).
- Seed 7 with the veto kept for the last-touch team: passes 107. Seed 11 with the same: 84 (on 107, off 32). Not fully restored, so more than the carrier is involved.
- Pass kick replaced by a dodge (pass command unselected, dodge applied): 6 to 11 per run. Not the mechanism.
- `GetBallRetainer()` is null during carries. Carrier identity must come from `HasPossession()` or the designated possessor.

## PES reading

- FUN_140a0b350 (human gate), in order: feature 0x27 on; match state 4 off; role-0 player position test (FUN_140a638c0 gates it); newest-event window (team record half == mine and frames < gs*0.2 -> no); team latch +0x255 closed -> continue; roster-slot tail: if team+0x6a4[slot] != 0 and FUN_140a6ce10(record0, carrierIsPlayer, 0) -> no; else yes.
- FUN_140a6ce10 (OBS): busy = record kind 9..14; kinds 6 and 7 only when there is no carrier; kind 5 never with ctx 0.
- FUN_140a627e0(ctx, 0) = MA+0x2601c, the ball carrier id (OBS).
- FUN_140a63830 (OBS, used by the ball gate FUN_140a07bf0): record is a valid player action (team index < 2, player id < 22).
- team+0x6a4[i]: AI reaction-delay frame counter, written by FUN_1405027A0 (docs/re/pes-re-duels-fouls.md §1.1). Meaning INF(hi): decision latency.
- Dodge selectors take kind, speed class, ball band and bearings; none compares incoming clip or body direction (docs/re/pes-re-dodge-port.md §2-3).
- GF stand-ins in HumanDodgeGate: rosterSlotValue = 0 (term never fires); framesSinceEvent = the player's own last touch (PES uses the team's newest record); teamGateOpen = !IsInPlay.

## Not done

- Porting the roster-slot term. Missing GF inputs: a per-player reaction-delay counter (GF has none); the event-kind numbering for 9..14 (not decoded); a carrier source that is not `GetBallRetainer()`.
- The newest-event window term (NetDodge's gate). NetDodge replied that they are not touching HumanDodgeGate and that the roster-slot note stands for whoever ports it.
- Lifting the veto. Blocked on the port above. Also needs a viewer frame of an off-pose avoid clip (AGENTS.md visual rule).
- The GFX_NO_DODGE_OVER_BALL precedence experiment was written but never run; it was dropped with the other instrumentation.

## Next (exact)

1. Decode the ball-event kinds 9..14 (ring from FUN_140a3e190) and the callers of FUN_1405027A0 that write team+0x6a4. Define GF analogues with VA cites.
2. Port FUN_140a0b350's roster-slot tail and newest-event window into HumanDodgeGate (NetDodge's file, with their agreement). TDD with a test per branch. Re-run the 5-seed off measurement.
3. Lift the veto only if passes match or beat the on figures on every seed, and a viewer frame of an off-pose avoid clip is clean. Then update the ponytail note.
