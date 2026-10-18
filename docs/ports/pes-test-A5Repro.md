# A5Repro: throw-in taker red test on origin/main (ball lies over the line)

Branch: `origin/A5Repro` = `c153fd6` (confirmed by `git ls-remote`). Base: `origin/main` = `e0eece6`.
Worktree: `<workdrive>/gf-A5Repro` (detached, `build/`). Commit touches only `tests/sim/pes_restart_delay_test.cpp` (+66). No `src/` change; no fix.

## Result

- Result: RED on origin/main, as intended.
- Binary: `build/tests/sim/pes_restart_delay_test` (Release, Ninja).
- Filter: `PesRestartDelay.AThrowInTakerPlaysABallAction*`, run with `PES21_ROOT=<workdrive>/PES21`: 0 passed, 1 failed (31.5 s).
- Failing assertion: `tests/sim/pes_restart_delay_test.cpp:339`, `EXPECT_EQ(untaken, 0)`:
  `1 throw-in taker(s) played no ball action within 1757 ticks of the stoppage with the ball over the line: taker H4;`
- Same stall, also red (harness helper, `RunMatchWithinTheBound`): lines 89, 92, 94 (`watch.Stalled()`, full time not reached, longest 1758 > limit 1757).
- Precondition, line 338 (`onLocomotion > 0`): passed. Taker H4 was in a Special PES-locomotion clip at release (`functionType` 14, `specialvar1` 102, `walk_1_1`).

## Setup that reproduces it

Test `AThrowInTakerPlaysABallActionWhenTheBallLiesOverTheLine`:
- `RunMatchWithinTheBound(pes_loop = true, LONG_MINUTES = 5)`, seed 7, teams 1 v 2.
- On every throw-in hold (`b.prepared`, not started): `Ball::ResetSituation` to `(restartPos.x, ±(pitchHalfH + lineHalfW + 0.2))`, which is y = ±34.26. The 0.2 m is a test input, not a PES value.
- Requires a ball action (ShortPass, LongPass, HighPass, Shot) from the taker within `RestartLimitTicks` counted from the stoppage start. That is how `RestartWatch` counts it.

Why the push is needed: `referee.cpp:527` puts a dead throw-in ball back on its spot only when `!PesLoop()`. Under pes_loop nothing corrects a ball that a body touch leaves past the line.

## Reason, confirmed in-harness on main (temporary log, reverted)

Temporary `fprintf` in `Humanoid::GetBestCheatableAnimID` (humanoid.cpp, taker name `H4`, in set piece). Reverted with `git checkout`; `git diff` on the branch shows only the test file.

Same setup, first throw-in at tick 7981 (ball pushed to y = -34.26), release at tick 8192. Each stalled tick logged:
- `gbc cands 8..16`, retainer not the taker.
- `oob prune y=-34.26 touch 1` for every candidate. This is the check at `humanoid.cpp:3166-3171`: when the taker does not hold the ball, `|predicted ball y| > pitchHalfH + lineHalfW + 0.11` (34.17) prunes the candidate.
- `gbc ret -1`, every tick, for the whole bound.
- Before the push, the first call returned `1202` (a pass clip) with 15 candidates.

CONFIRMED: `GetBestCheatableAnimID` returns -1 with 8-16 live candidates, all pruned by the line check, while the taker is in a Special locomotion clip.
NOT VERIFIED: which function type the refused call was made for. The ShortPass queue was not logged on main.

## Corrections to the brief's wording

- The clip is not the cause on main. The OOB prune runs before any clip check. The taker's Special clip is present, but the refusal is positional. Without the push, the same taker plays a pass one tick after release (see below).
- The A5Tail note says disabling the OOB check does not change the stall. On main, the OOB prune is the only refusal seen in this setup. The two results are not reconciled. Probably the A5 run had the ball at a different position; not investigated.

## Why main is green without the push

Three throw-ins in a 5-minute seed-7 run on main (diagnostic, not committed):
- Ball on the line at y = -34.06, inside 34.17. Taker 0.27 m off on the spot. A ball action by +1 tick in all three cases: taker H4 passes from Special (type 14 to 4), taker H9 is already on a high pass (type 6), taker A5 passes from Movement (type 1 to 4).
- `ARestartTakerPlaysHisBallActionAfterRelease` (2 min) passes on main: 185.9 s.
- `AThrowInIsReleasedFromItsSpotAndTaken` (5 min) passes on main: 155.9 s.

## Open items

1. No fix implemented, as briefed. A candidate fix belongs where the candidate loop reads the ball line check for throw-ins. Not decided here.
2. Not verified: the queued function type at the refused calls, and whether the taker's clip matters at all. A test with the taker on Movement would need a separate run.
3. Test is red for 1758 ticks of stoppage; the run takes about 32 s plus build.
4. Gate not run (Main gates). Binaries I ran: `pes_restart_delay_test` only. Existing `ARestartTaker*` and `AThrowIn*` on main (green, one process). Temporary diagnostics (3 runs, reverted). The new test: first with the bound counted from release, red only on the harness lines (the watch counts from the stoppage, so the release-based check never fired); then with the bound counted from the stoppage, red at line 339.
5. The worktree shows `extern/godot-cpp` as modified in `git status` (symlink over the gitlink). It is not staged and not committed.
