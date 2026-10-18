# Restart taker's mental image: reproduction and SetPosition refresh (branch A5Tail2)

Result: Blocked on reproduction. The assigned fix (Ball::SetPosition refresh) is NOT shipped: it breaks an existing test and no test observes it. The prepared-tick guard test is committed but passes on base, so it is not a reproduction.

## Branch and SHAs
- Worktree: <workdrive>/gf-A5Tail2, branch A5Tail2 (not A5Tail, which another worktree owns). Base 6b50897.
- 6ae2f4c: red-labelled test commit. Its test is the prepared-tick version, which passes on base (see below). Pushed.
- ce74a39: ring-wide refresh attempt. Pushed; it is a dead end (see mutations A and E).
- 4faa303: head. Pushed; `ls-remote` confirms `refs/heads/A5Tail2` = 4faa30372ecb28d308005ec6da427a106c9f7cbc.
  - Its message says "Test: ... (guard; passes on base)", but the commit ALSO carries the source revert of ce74a39 back to the 6ae2f4c state. A stale index from `git checkout <commit> -- <path>` was included. Net source diff vs 6ae2f4c: empty. Net diff vs A5Tail: tests/sim/pes_restart_delay_test.cpp only. History was not rewritten.

## What was decoded or checked (CONFIRMED by code or run)
- Ball::SetPosition production callers: humanoid.cpp:1335 (keeper hold, mid-tick, after the snapshot). Tests call it directly at pes_tick_a1_test.cpp:318, 337, 395.
- Restart paths do NOT call SetPosition. They use Ball::ResetSituation: referee.cpp:532 (throw-in hold), referee.cpp:750 via Match::ResetSituation (match.cpp:481, which clears mentalImages at 491 and sets a constant track). penaltyshootoutcontroller.cpp:156, 234.
- Ball::Touch and Ball::Stop call CalculatePrediction() then UpdateLatestMentalImageBallPredictions(). That refresh copies GetTrack() into mentalImages[0] only (match.cpp:443). Without the CalculatePrediction() before it, the refresh copies a stale track, so it is a no-op.
- Match::Process order (pes_loop): ball stage, event stage, referee stage, then mentalImages.insert of a new snapshot (match.cpp:778-784). Between Process calls, mentalImages[0] is the PREVIOUS tick's snapshot.

## Test results
Prepared-tick test (final form): per prepared, dead, not-started tick, compares Predict(0) with GetMentalImage(300)->GetBallPrediction(300), fail at >= 0.5 m, LONG (5 min), pes_loop.
- On base code: PASSED (first run, bg_13, 238.6 s). The reported 2.5 m / 0.28 vs 2.7 m discrepancy is NOT reproduced on prepared ticks in this harness.
- Failing on base only on pre-prepare dead-ball ticks (MINUTES, dead filter): 2.4999 m at dead tick 11 of a set-piece-2 stoppage (bg_20). That tick is within about 11 ticks (about 200 ms) of the whistle, inside the 300 ms reaction window. INF: this is the reaction-lag model (an image older than the whistle still carries the flight), not a stale-track bug. Unverified against PES.

## Mutations (each run, rebuilt, reverted)
- A. Ring-wide refresh (every mentalImages[i]): new dead-tick test green; AThrowInIsReleasedFromItsSpotAndTaken regressed (base PASSED at bg_31; ring-wide FAILED at 1758 vs 1757, bg_29). Rejected.
- B. Index-0 refresh only (assigned Touch/Stop form), dead-tick test: red at 2.4999 m, tick 11 (bg_23).
- C. Delete SetPosition's refresh line, keep CalculatePrediction: prepared-tick test green (bg_35, LONG). Not discriminating.
- D. Delete SetPosition's CalculatePrediction, keep refresh: prepared-tick test green (bg_36, LONG). Not discriminating.
- E. Whole SetPosition change reverted (base ball.cpp): pes_tick_a1_test 18/18 PASS (bg_39). With the change: PesA1Sim.TheImageSomeMillisecondsAgoIsThatAgeNotGridStepsOfTen FAILS (ballXOfAge(1)-ballXOfAge(2) = 0 vs 1.0 +- 0.15; bg_38). The SetPosition refresh writes the current track into mentalImages[0], which between Process calls is the previous tick's snapshot. Rejected.

## Binaries run and counts
- pes_restart_sequencer_test, final tree: 13/13 PASS (with PES21_ROOT; one run without it skipped 10, discarded).
- pes_restart_delay_test, final tree, guard test: PASS (239 s).
- pes_restart_delay_test, full suite on base source (bg_21, earlier dead-filter variant of my test): 2 PASS (EveryStoppageRestartsWithinTheInstallBound, AThrowInIsReleasedFromItsSpotAndTaken). FAIL: EveryStoppageRestartsWithinTheInstallBoundWithPesLoop, PesLoopRestartsAreArmedAtTheirPesSpotsSidesAndReleaseFrames, AThrowInKeepsItsWriterSpotThroughPrepare, AWhistleLeavesNoFlyingBallBehind, ARestartTakerPlaysHisBallActionAfterRelease. Each shows "LongestTicks 1758 vs LimitTicks 1757".
- Same 5 fail on the SetPosition-fix state (bg_37). Pre-existing, not caused by any change here.
- pes_tick_a1_test: base 18/18 PASS; with SetPosition fix 17/18 (regression, see E).
- On the SetPosition-fix state only (not base): pes_ball_track_test 14/14, pes_ball_physics_test 43/43, pes_keeper_test 24/24, pes_image_test 18/18, pes_setpiece_taker_test 8/8, pes_restart_taking_test 3/3, sim_match_test 10/10 (bg_38).
- Gate: not run (Main gates). Smoke: not run.

## Open items
1. Reproduce the reported prepared-throw-in discrepancy (0.28 m real vs 2.7 m seen). Needs the exact commit and MatchSetup that A5Tail used; not reproduced on A5Tail head 6b50897 with seed 7, LONG, pes_loop.
2. Decide whether PES's mental image ring shows a 300 ms reaction-lag ghost after a whistle (pre-prepare dead-ball reads at about 2.5 m). Needs PES RE of the MA ring (docs/re/pes-re-tick-order.md section 4 is referenced as not located).
3. Pre-existing one-tick restart-bound overrun (1758 vs 1757) in 2-minute pes_loop runs. It fails 5 tests on base. Separate issue, not in scope here.
4. Commit 4faa303 message understates its content (includes the src revert). Not rewritten; the branch must not be force-pushed without approval.
5. The worktree's extern/godot-cpp symlink makes `git status` error ("expected submodule path ... not to be a symbolic link"); commits worked with `-c status.submodule=none`.
