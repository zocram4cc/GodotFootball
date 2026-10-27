# pes-port-StoppageFlow: PES restart phases vs GF (27-10-26)

**Status.** Two PES-sourced fixes are in: D1 (set-play ready point) and D2 (timed release gate counted from the ready point). GF switch-off mode is unchanged. The free-kick and corner gates now match PES's live play-3-to-release spans. **Open: `pes_throwin_release_test` is 3/4 red with the fix (HEAD is 4/4), see section 6. Not merged-ready until resolved.** GF's PES-loop match length is not within a stated margin of PES's; the causes are named in section 3.

## 1. PES reference (frame-exact)

Source: LiveFlow `liveflow.jsonl`, G `0x1c821d60`, H2 only (H1 not observed). Definitions: whistle = last play-1 frame before the stoppage; **ready** = first play-3 frame (`MI+0x116C = 3`); release = first play-1 frame after. Frames from `MI+0x1198`. Gaps in the poller log are at most 291 frames per restart; frame numbers give exact spans.

| kind (`MI+0x1554`, reason) | n | whistle to ready (median) | ready to release (median) | whistle to release (median) | per restart |
|---|---|---|---|---|---|
| kick-off (1, reason 3) | 4 | 1349 | 134 | 1483 | W to ready: 1467, 1231, 1713, 684; ready to release: 134 in 4 of 4 |
| corner (4, reason 4) | 4 | 180 | 365 | 570 | W to ready: 232, 180, 179, 179; ready to release: 378, 490, 351, 349 |
| free kick (5, reason 5) | 1 | 313 | 355 | 668 | hold 0x16 = 280, ramp = 32 |
| goal kick, throw-in | 0 | | | | none in H2 |

Pre-ready structure: step `0x16` (hold and UI window) lasts 147 frames for corners 2 to 4, 196 for corner 1, 280 for the free kick and 650 to 1679 for kick-offs. The 0x01 to 0x15 ramp is 31 to 35 frames. Ready comes when 0x15 starts (setpiece-choreography 1.2: 0x15 at +313, play 3 at +314, fade-in at +333).

**Corrections to earlier docs.**
- The four kind-1 "TK" restarts are **kick-offs**: reason 3, ball released at the centre (0, ±3 to 4). The goal-kick label in `pes-re-live-flow.md` 5.2 is withdrawn.
- The wait/place/armed counts in `pes-re-live-flow.md` 5.2 are poll counts, not frames.
- GF's parity-log `release` is `IsInPlay && !IsInSetPiece`, which runs 25 to 109 frames after PES's play-1 onset. Compare on `play1` (`[parity-phase]`).

## 2. Per-kind phases, GF before and after (PES-loop)

GF before: `67e9a1aa74e84e0a` runs (10 min seed 7 and others). GF after: fix runs. n is restarts pooled over the runs. W to ready: before = `prepareTime`, after = prepare + 33 frames (the ready point). Ready to play 1: before = `startTime` to release, after = counted from the ready point.

| kind | W to ready, before to after | ready to play 1, before to after | whistle to play 1, before to after | PES whistle to release |
|---|---|---|---|---|
| free kick, 5.0 s hold, no card (n=2 before, 2 after) | 276 to 308 | 475 to 356 | 751 to 664 | 668 |
| free kick, +3 s card delay (n=7 before) | 438 to 470 | 475 to 356 | 913 to 826 | n/a |
| corner | none (GF produced 0 corners) | n/a | n/a | 570 |
| goal kick (n=4 before) | 276 (141) to 308 (173) | 175 to 143 | 451 to 451 | none |
| throw-in (n=3 before) | 276 to 308 | 93 to 61 | 369 to 369 | none |
| kick-off after a goal (n=9 before) | 1107 to 1107 | 108 to 108 | 1215 to 1215 | 1483 (PES score 0-0) |

Free kick, after fix: play-3-to-release = 356 against PES 355 (the gate is 355 and play 1 comes one tick later). Corner, after fix (n=1): play-3-to-release = 366 against PES's median 365.

## 3. Departures and fixes

**D1 (fixed).** GF set play 3 at `prepareTime`, before the UI window and teleport. PES writes `MI+0x116C = 3` when step 0x15 starts (`FUN_14044BB40`: value 0x15 calls `FUN_140A3CD90(Flow+0x2790, 3)`), after the 0x14 teleport window. Fix: `RefereeBuffer::ReadyForTake` (`referee.hpp`). The ready point is prepare + stop stamp (40 ms) + UI (200 ms) + 0x14 window (370 ms), via `FoulSequence::ReadyDelay_ms` (`foulsequence.cpp`) and `Timing::readyAt_ms` (`pessequencer.cpp`). `readyDelay_ms = 0` means no ready point, so the GF path is unchanged.

**D2 (fixed).** The timed release gate (free kick 355, corner 365, penalty 54; `pessequencer.cpp`) was counted from `startTime` (armed, after the fade-in and tail, about 77 frames later). PES counts from ready (free kick play-3-to-release = 355 exactly; corners 349 to 490, median 365). Fix: `ReleasesRestart` counts from `prepareTime + readyDelay_ms` on the PES path (`referee.cpp`).

**Remaining, named and not fixed.**
- **R1 card delay (GF-only).** `IssueDeferredCards` adds 3 s (`kPesCardOnScreen_ms = 3000`, 162 frames) to prepare. It hit 8 of 9 free kicks in the 10-min seed-7 before-run. PES's late card is a scene (step 6, 340 ticks) in its own programme; no booked free kick is in the H2 sample.
- **R2 kick-off gate.** PES's play-3-to-release is 134 in 4 of 4 kick-offs. GF releases at armed (`ReleaseFrames(KickOff) = 0`), 108 frames after prepare. Goal kick-offs are owned by `GoalSequence` (no ready point modelled). The mechanism of PES's 134 is unresolved.
- **R3 corners never produced by GF.** Zero corners in every GF run (5 PES-loop runs plus the smoke). `EndLineRestart` is correct under its tests. The cause is which team touched last at end-line outs; not traced. OPEN.
- **R4 throw-in gate.** PES releases 118 to 194 frames after ready on the taker's arrival (`pes-re-setpiece-choreography` 9.3, live). GF releases on arrival at armed: 61 frames after ready. Not fixed (taker walk).
- **R5 goal kick.** PES 238 to 253 frames from ready (`pes-re-A5Judge` OPEN-6). GF 143. Not fixed: the ladder is not timed on PES H2 samples.
- **R6 hold variance.** PES's step 0x16 varies (147 to 196 for corners, 280 for free kicks, 650 to 1679 for kick-offs). This is the UI/substitution sub-state window, not ported.
- **R7 goals and kind mix.** GF 10-min seed 7 has 6 goals and 27 restarts (kick-offs after goals, 1107 frames each). PES H2 has a 0-0 score and 9 restarts (corners 4, kick-offs 4, free kick 1, no goal kick or throw-in). GF has 9 free kicks, 3 throw-ins, 4 goal kicks, 1 corner in that run.
- **R8 kick-offs with 0-0.** PES's four kick-offs occurred with the score unchanged. Unexplained (open).

## 4. Match length and in-play share

GF PES-loop runs, switch-off, and the in-play share (GF `live` = `IsInPlay && !IsInSetPiece`):

| run | ticks before | live share before | ticks after | live share after | det before | det after |
|---|---|---|---|---|---|---|
| PES-loop 10 min seed 7 | 58893 | 58.7% | 46973 | 72.4% | 67e9a1aa74e84e0a | 20cc1c13139bf4df |
| PES-loop 10 min seed 3 | 52602 | 66.3% | 44794 | 78.4% | f83ae25fb5e6bdcd | 5a17c9e545a109c7 |
| PES-loop 5 min seed 3 | 27548 | 70.6% | 22712 | 73.9% | 2c0749ccc8ca6735 | 3ab779e407b5f585 |
| PES-loop 5 min seed 7 | 28663 | 86.0% | 28663 | 86.0% | 45e830a988ff01cf | identical |
| switch-off 5 min seed 7 | 31521 | n/a | 31521 | n/a | 7ca3d08f68567016 | 7ca3d08f68567016 |

PES reference: 48,708 frames (one match, H1 inferred at 21,729). In-play share H2 64.3% (play 1 frames). The GF `live` definition differs from PES's play 1, so shares are not like-for-like.

**Margin.** 10-min seed 7 after-fix is 46,973 frames (−3.6% from PES). Before-fix is 58,893 (+20.9%). Seed 3 after-fix is −8.0%. These numbers are **not** a faithful match-length match: the after-fix runs still carry R1, R2, R3 and R7 (goal celebrations and a different kind mix). The stoppage phases now match PES where measured; the length is not established by them.

## 5. Proof

- **Red then green.** `PesRestartSequencer.TheSetPlayReadyPointComesAfterTheTeleportWindowAndBeforeTheFadeIn` was compile-red (missing `readyAt_ms`), then assertion-red on my own expectation. That expectation omitted the 0x15 window; corrected from setpiece 1.2. Result: 16/16 green. `PesRestartDelay.PesLoopRestartsAreArmedAtTheirPesSpotsSidesAndReleaseFrames` plus the play-state assertion were compile-red, then 3/3 green with the other two restart-delay tests.
- **Mutations (each reverted).** M1, `ReadyForTake` returns `prepared`: RED (play-state assertion, `pes_restart_delay_test.cpp:154`, set piece 5). M2, gate counted from armed: RED (`released late for set piece 3`, 443 frames against 357).
- **Suites (final build).** `pes_restart_sequencer_test` 16/16. `pes_stoppage_timing_test` 10/10. `pes_restart_taking_test` 3/3. `pes_goal_kick_test` 19/19. `pes_setpiece_taker_test` 9/9. `pes_restart_delay_test`: all completed tests OK until the 1800 s wrapper cut it off (exit 124; not failures; remaining tests not run). `pes_throwin_release_test`: 1/4 (see section 6).
- **GF mode.** Switch-off 5 min seed 7 = `7ca3d08f68567016`, 31521 ticks, before and after.
- **Build.** `origin/Cutover` does not compile `tests/sim/pes_team_tick_test.cpp` (uses `marktable::Facts::holderX` and `Candidate::posX`, absent). Pre-existing and not touched. All other targets built with `-k 0`.
- **Instrumentation.** `apps/gfsim_run.cpp` gained `[parity-phase]` lines (prepare, play3, start, play1). Read-only; sim hashes unchanged by it.

## 6. Open: throw-in suite regression

`pes_throwin_release_test` at HEAD: 4/4. With the fix: 3 FAILED (`no throw-in was released within two minutes`; `no carried throw-in was released`; `no carried throw-in was lifted`), 1 passed. The test runs a 2-minute seed-7 match with no `RestartWatch`. Under the fix, `gfsim_run` (which has `RestartWatch`) still shows a throw-in at 26819 to 27080 in its 2-minute seed-7 run, so the trajectories differ between the harness and gfsim. Not diagnosed. Next step: trace `PieceTakerArrived` and the throw-in release dispatch's play-state edge in the harness run (`teamAIcontroller.cpp` around `ReleaseDispatchInput`). The test was not weakened or skipped.

## 7. LiveFlow data

Second poller (`poller.py`, read-only, `sudo -n`) started on pid 2802803 at 2026-10-27T08:39:54Z. It wrote no rows (the process was not alive at stop; `sudo -n kill` reported "No such process"). PES sample count is unchanged: 9 restarts from one match. No further PES reads.

## 8. Next

1. Resolve section 6 (throw-in), then re-run the throw-in suite.
2. Settle R3 (end-line last-toucher), R2 (kick-off gate), R4 and R5 (taker gates) from PES traces before porting.
3. Decide whether the goal kick-off and card timings (R1, R2) belong in this branch or a follow-up.
