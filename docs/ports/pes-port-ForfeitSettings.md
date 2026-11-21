# ForfeitSettings: match options (extra time, shoot-out) feed the regulation-end progression

Status: DONE. Branch `ForfeitSettings` at `b28f2166` (pushed, `ls-remote` confirmed), on top of origin/main `2682f213`. Round 2 (Main's stall attribution and affected-test sweep) below.

## 1. E and its flags (CONFIRMED live and static)
- `G = q(0x1436F3DC8)`, `E = q(G+0x2C0)`. Live on pid 3218130 (read-only pread): G = 0x2CE38100, E = 0x10E1F534.
- Live flags (exhibition match, sampled twice): E+0x66 = 0 (extra time off), E+0x67 = 1 (shoot-out on a level score). E+0x68..0x6B = 0; E+0x64 = E+0x65 = 1; E+0x17 = 7; E+0x5C = 1.0f.
- Writer chain (static, CONFIRMED for the chain, INF for the field meaning): `FUN_142159F40` (0x142159F40) fills E's settings fields from a tagged record array; its only caller is `FUN_14214B8D0` (0x14214B8D0), called from `FUN_142160350` (0x142160350, save-slot reader). The hook on 0x142159F40 never fired (the save is read before hooks install). The two ff_ hooks on 0x141019BF0 / 0x1408E0DB0 are not the E writer.

## 2. Default and save reader
- `src/pes/editsave.hpp` `ReadEditSave` decodes only the player table (data block, records at +0x7C, stride 0x138). No reader exists for E+0x66/0x67, so the defaults are the live measured pair, labelled `ponytail:`. Upgrade path: read the user's save (`~/Documents/KONAMI/eFootball PES 2021 SEASON UPDATE/<id>/save/EDIT00000000`) settings records in FUN_142159F40's layout.

## 3. GF change (CONFIRMED by build and tests)
- `MatchSetup`: `extraTime = false`, `penaltyShootout = true` (the measured pair).
- `MatchProgression::Options {extraTime, penaltyShootout}`; `GetNext(phase, scoresLevel, options)`:
  - 2nd half, level: extra time if `extraTime`, else the shoot-out if `penaltyShootout`, else full time. Not level: full time.
  - 2nd extra time, level: the shoot-out if `penaltyShootout`, else full time.
- `PeriodAfterWhistle(period, scoresLevel, options)` and `PesClock::EndPeriod(scoresLevel, options)` follow the tie tests FUN_1404493D0 (period 3: 4 / 8 / 10) and FUN_1404499D0 (period 7: 8 / 10). Periods 1 → 2 and 5 → 6 unchanged.
- `Match::GetProgressionOptions()` stores the setup options; `referee.cpp` passes them at the regulation whistle. The always-extra-time path is deleted (no shim).
- NOT PORTED (labelled `ponytail:` in `pesclock.cpp`):
  - E+0x68 (doubled-tally flag on the period-3 tie test): live 0. GF has no aggregate tally; the branch that uses it is not taken at the live value.
  - E+0x69 (doubled side-1 total on the period-7 tie test): live 0; same reason.
  - E+0x65 (FUN_140443E30's side argument on the extra-time route): live 1; GF has no side argument there.
  - E+0x6A / E+0x6B (aggregate tallies added to the side totals): live 0; not added.

## 4. Assertions moved to explicit settings
- `tests/pes_clock_test.cpp`: `PesPeriodWhistle.TheHalfTimeWhistleHandsOverToTheBreakWhateverTheScore`, `ATiedSecondHalfHandsOverToTheExtraTimeBreakAndAnUntiedOneToFullTime`, `ATiedSecondExtraTimeHandsOverToTheShootOutBreakAndAnUntiedOneToFullTime` (plus a full-time case), `TheExtraTimeFirstHalfHandsOverToItsBreakWhateverTheScore`, `ALevelMatchRunsThroughEveryBreak`, `PesBreak.ABreakHasNoClock`: explicit `kExtraTimeAndShootOut` (or `kShootOutOnly` / `kFullTimeOnly` where named). New: `ATiedSecondHalfWithoutExtraTimeGoesToTheShootOutOrFullTime`.
- `tests/sim/pes_period_end_test.cpp`: `PesPeriodLifecycle.EachTransitionStartsItsOwnPESPeriod`: `GetNext` calls pass `{true, true}`.
- `tests/sim/pes_forfeit_test.cpp`: every `BuildMatch` (now takes options) and the post-match period check pass `kFullTimeOnly` (`{false, false}`, PES's full-time route). The forfeit scenario is pinned to full time: a level score ends at full time, where the forfeit is written. No assertion weakened.

## 5. Round 1 results (`PES21_ROOT=<workdrive>/PES21`, `PES_EXE` = the unpacked image, sim slots)
- `pes_clock_test`: 52/52 PASSED.
- `pes_period_end_test`: 11/11 PASSED.
- `pes_forfeit_test`, `AYellowThenRed*` + `PesForfeitInstall.*` + `PesForfeitRule.*`: 4/4 PASSED (the scenario test: 887 s, full time reached).

## 6. Round 2: stall attribution (MEASURED)
- `PesForfeitMatch.ASideAtTheMinimumKeepsPlaying` on the branch: STALL (in-play stalemate, `[stalemate] 68.0s idle … forcing H9`, timeout rc=124 at 900 s).
- The same test on origin/main `2682f213` (detached worktree `<workdrive>/gf-ForfeitSettings-base`, its own build dir, origin/main test file): the SAME stall, rc=124, same signature (68.0 s, ball at -23.5,-9.9, A11 taker, H9 forced).
- Verdict: INHERITED. It is not caused by this change; the stall is before any regulation whistle.

## 7. Round 2: affected binaries (IsGameOver / GetNext / PeriodAfterWhistle / EndPeriod callers)
Binaries that run full matches to game over, or assert on the progression, were run in full on the branch build:
- `pes_restart_taking_test`: 3/3 PASSED (`AShortMatchFinishesWellInsideTheSimCap`, `TheTakerTakesTheKickBeforeTheWhistle`, `TheSecondHalfRestartIsAlsoTaken`).
- `sim_match_test`: 13/13 PASSED (incl. `SameSeedPlaysTheSameMatch`, `AnotherSeedPlaysAnotherMatch`, `PesLoopStepsTheTeamTickFromTheStrategy`).
- `pes_stoppage_timing_test`: 10/10 PASSED.
- `pes_challenge_counters_test`: 3/3 PASSED.
- `pes_clock_test` 52/52, `pes_period_end_test` 11/11 (above).
- `pes_forfeit_test`: the 4 scenario-family tests PASSED; the stall above is inherited. `ASideShortOfTheMinimumEndsTheMatchAtItsNextStoppage` PASSED in round 1.
- Not run (their `IsGameOver` loops stop at game over and assert nothing about the progression route): `pes_foul_whistle_test`, `pes_goal_scene_hold_test`, `pes_holder_latch_test`, `pes_mark_table_test`, `pes_press_approach_test`, `pes_defender_decisions_test`, `sim_substitution_test`. Listed for Main to run at the gate if wanted.
- Gate not run (Main gates per landing).

## 8. Open items
- Save reader for the settings records (upgrade path above). Until then the measured pair is the default for every GF match, including the Godot UI.
- No gfsim CLI flag for the two options (set through MatchSetup).
- E+0x65 / E+0x68 / E+0x69 / E+0x6A / E+0x6B not ported (section 3).
- Inherited stall in `ASideAtTheMinimumKeepsPlaying` (section 6), on origin/main too.
- Worktree `<workdrive>/gf-ForfeitSettings-base` (detached origin/main, build dir) is kept for Main to remove.

## Scratch
- `<workdrive>/gf-ForfeitSettings-scratch/` (readmem.py, scan scripts, decompiles, test logs `base_keep.log`, `branch_keep.log`, `aff_*.log`). Not committed.
