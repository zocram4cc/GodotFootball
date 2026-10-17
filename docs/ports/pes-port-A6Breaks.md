# A6Breaks: PES period writer (breaks), match speed per frame, HUD clock (not ported)

Branch `A6Breaks` (from `origin/main` 81bc0b1). Red `11fec1d`, green `577b47a`. `git ls-remote origin A6Breaks` = `577b47a`. Not merged.
Source: `docs/re/pes-re-A6Writers.md` §5 (breaks), §6 (match speed), §7 (HUD). Install `PES21_ROOT`; no live reads, no writes.

Verdicts: **CONFIRMED** (install read or decompile recorded in A6Writers), **INF(level)**, **OPEN**.

## What changed

1. **Period writer at the whistle** (`pesclock.{hpp,cpp}`, `referee.cpp`)
   - `PeriodAfterWhistle(period, scoresLevel)`: 1 → 2; 3 → 4 if level else 10; 5 → 6; 7 → 8 if level else 10; other periods unchanged. CONFIRMED from A6Writers §5 (`FUN_1404494E0` at 0x1404494E0, helpers `FUN_1404493D0` 0x1404493D0 and `FUN_1404499D0` 0x1404499D0, writer `FUN_14102CAD0`). I did not re-decompile them in this task.
   - `PesClock::EndPeriod(scoresLevel)` is called at the referee's whistle, before `GetNext` and `SetMatchPhase`.
   - Breaks (2, 4, 6, 8), the shoot-out (9) and full time (10) have no clock: `Advance` and `AccrueStoppageLimit` hold the stamp, F and LIMIT there. `PastPeriodLength` and `TimeUpGateOpen` are false there.
   - **Ponytail, break-end writers 2→3, 4→5, 6→7, 8→9: OPEN** (A6Writers §5). GF starts the next period at the phase change, so no break lasts a frame in the match loop. The record is written to the break and overwritten by `SetPhase` in the same tick. Tests cover the table; the match never observes the break record.
   - **Ponytail, knockout flags** `0x66` (extra time) and `0x67` (shoot-out) in `FUN_1404493D0` and `FUN_1404499D0`: no GF setting. GF plays extra time and a shoot-out after a draw (`MatchProgression::GetNext`), so a level match takes the break that leads to them. A level match never goes 3 → 8.

2. **Match speed** (`PesUnitsPerFrame`, `PesClock` now counts frames)
   - `units per frame = 90.0 · E60 · E5C · (60 / 54) / E58`, the formula of A6Writers §6 (`FUN_140A6C050`, `FUN_140A6BFC0`).
   - `90.0` is read from `.rdata 0x14259be5c` by `LoadPesClockConstants` (`matchSpeedBase`). CONFIRMED: `PesClockInstall.ConstantsComeFromTheExe` asserts 90.0 against the install.
   - `E58` = the match duration (`Match::matchDurationMinutes`), clamped to the menu's 5..90 minutes. **INF(high)**: A6Writers reads E58 as the match-time setting; only E58 = 10 is observed live (10 units a frame, CONFIRMED).
   - `E60` and `E5C` = 1.0. **Ponytail**: both are 1.0 in the live match; GF has no setting for either.
   - `Advance(frames)` and `AccrueStoppageLimit(frames)` replace the game-second calls. `Match::AdvanceMatchFrames(1)` runs once per tick in play. `elapsedSeconds` advances by `90 / (54 · E58)` game-seconds per frame, the same law as `MatchDurationGameTimeFromRealMilliseconds`.
   - Stoppage F and LIMIT take 0.5 per frame at any match length (A6Writers §6, CONFIRMED). Before, they took 0.5 × 54 per game-second, which is 9 a frame at the 5-minute floor. This is corrected.
   - At E58 = 10 the rate is 10 per frame, matching the live value. At GF's default setting the GF rate is unchanged in play (60 per game-second × 90/(54·E58) per frame = 100/E58 per frame), so this is a restatement of the old in-play rate, not a change to it.

3. **HUD clock: NOT ported.** The 2000-minute base is still OPEN (A6Writers §7). No later doc in `docs/re/` decodes it. GF's HUD still shows its own elapsed seconds.

## Finding, not fixed (outside the three items)

`Match::Process` calls `AdvanceMatchFrames(1)` only inside `if (IsInPlay() && !IsInSetPiece())` (`match.cpp` ~877). So in the live loop the frozen branch never runs: F does not accrue in stoppage, and under `pes_loop` LIMIT does not accrue either. Stoppage time is missing from the live clock. The frozen branch is reached only by direct calls (tests). Suggested fix: call `AdvanceMatchFrames(1)` every tick and let `SetFrozen` decide. That touches the off path (elapsed time), so it needs its own hash check. Main to decide.

## Tests (PES21_ROOT=<workdrive>/PES21, no skips)

- `pes_clock_test`: 51 passed (red commit: 15 failed, 36 passed on assertions).
- `pes_period_end_test`: 10 passed (red commit: 7 failed, 3 passed; the 3 are unchanged lifecycle tests).
- `pes_clock_match_test`: 2 passed.
- New tests: `PesMatchSpeed.TheStampMovesOneHundredOverTheMatchMinutesPerFrame`, `PesMatchSpeed`/`PesClock.AStoppageHoldsTheStampAndAddsHalfAUnitPerFrame` (now any-length), `PesPeriodWhistle.{TheHalfTimeWhistleHandsOverToTheBreakWhateverTheScore, ATiedSecondHalfHandsOver…, ATiedSecondExtraTimeHandsOver…, TheExtraTimeFirstHalfHandsOver…, ALevelMatchRunsThroughEveryBreak}`, `PesBreak.ABreakHasNoClock`, `PesPeriodLifecycle.TheMatchSpeedUsesTheClampedMatchLength`, `PesClockInstall.ConstantsComeFromTheExe` (+ `matchSpeedBase`).
- Changed expectations (intentional, the clock is now per frame): the gate and LIMIT tests now pass frames (`Frames(seconds)` = 3 per game-second at 5 minutes); the stoppage F expectation is 0.5 per frame.

Mutations (each applied alone, built, run, reverted; tree re-verified equal to the green copy):

| # | Mutation | Caught by |
|---|---|---|
| M1 | period 3 level → 8 | `ALevelMatchRunsThroughEveryBreak`, `ATiedSecondHalf…` |
| M2 | period 7 level → 10 | `ALevelMatchRunsThroughEveryBreak`, `ATiedSecondExtraTime…` |
| M3 | `EndPeriod` ignores the score | `ATiedSecondHalf…`, `ATiedSecondExtraTime…` |
| M4 | `Advance` moves a break's stamp | `ABreakHasNoClock` |
| M5 | rate drops the E58 divisor | 5 tests: `TheStampMovesOneHundred…`, `GateOpensExactly…`, `AddedMinutesHold…`, `ExtraTimePeriods…`, `StoppageTimeCounts…` |
| M6 | stoppage F scaled by game speed | `AStoppageHoldsTheStamp…`, `StoppageTimeCounts…` |
| M7 | match length not clamped (2 min → 2, not 5) | `TheMatchSpeedUsesTheClampedMatchLength` (50 vs 20) |
| M8 | LIMIT stoppage accrual uncapped | `TheStoppageLimitTakesHalfAUnit…CapsAt21600` |

## Gates and runs

- Off path, 2-min, seed 7 (`gfsim_run --minutes 2 --seed 7`, no `--pes-loop`): `[determinism] a3d5e43a3b7b03af`, the same as the brief's baseline and as origin/main run in this worktree. Only the wall-clock `[profile-split]` line differs.
- `--pes-loop`, 2-min, seed 7, run twice: exit 0 both times, `[determinism] 0da088d2496757bc` both times, `[restarts] 20 stoppages, longest 20.6 s (1110 ticks)`, `[ticks] 49225 sim ticks to full time (cap 194400)`, `[balance] shots 2-1`. No origin/main pes-loop baseline was run.
- Full gate not run (Main gates). 20-min smoke not run (Main's batch).
- Build: `pes_clock_test pes_period_end_test pes_clock_match_test gfsim_run` built clean; one pre-existing warning in `humanoid.cpp:2870` (not mine).

## Open items for Main

1. The live-loop stoppage gap above (`Match::Process` ticks the clock in play only).
2. Break-end writers 2→3, 4→5, 6→7, 8→9 (OPEN, A6Writers §5). Needs the phase-block writers traced, and the phase change moved to the restart when the walk-off is ported.
3. `E60`, `E5C` (ponytail = 1.0) and `E58` as the match duration (INF high). Settle with a live match at another setting.
4. Knockout flags `0x66`/`0x67` (the GF tie rule is a stand-in).
5. HUD origin (2000-minute base): still OPEN. Needs a kickoff sample in a fresh match, or the modulo test at clock > 4000 minutes.
6. Install rate VA: A6Writers §6 names `DAT_14351AE50` for the rate 54; the code's `GAME_SPEED_VA` is `0x14351ae54`, which the install test reads as 54.0. The `0x14351ae50` read was not checked.
