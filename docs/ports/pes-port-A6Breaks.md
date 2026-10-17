# A6Breaks: PES period writer (breaks), match speed per frame, live-loop stoppage clock (HUD not ported)

Branch `A6Breaks` (from `origin/main` 81bc0b1). Head `3527b13`, `git ls-remote origin A6Breaks` = `3527b13`. Commits in order: red `11fec1d` (clock and break tests), green `577b47a` (breaks, match speed), red `0b05c57` (stoppage through `Match::Process`), green `3527b13` (clock ticks every frame under pes_loop). Not merged.
Source: `docs/re/pes-re-A6Writers.md` §5 (breaks), §6 (match speed), §7 (HUD). Install `PES21_ROOT`; no live reads, no writes.

Verdicts: **CONFIRMED** (install read or decompile recorded in A6Writers), **INF(level)**, **OPEN**.

## What changed

1. **Period writer at the whistle** (`pesclock.{hpp,cpp}`, `referee.cpp`)
   - `PeriodAfterWhistle(period, scoresLevel)`: 1 → 2; 3 → 4 if level else 10; 5 → 6; 7 → 8 if level else 10; other periods unchanged. CONFIRMED from A6Writers §5 (`FUN_1404494E0` at 0x1404494E0; helpers `FUN_1404493D0` 0x1404493D0 and `FUN_1404499D0` 0x1404499D0; writer `FUN_14102CAD0`). The helpers were not re-decompiled in this work.
   - `PesClock::EndPeriod(scoresLevel)` is called at the referee's whistle, before `MatchProgression::GetNext` and `SetMatchPhase`.
   - Breaks (2, 4, 6, 8), the shoot-out (9) and full time (10) have no clock: `Advance` and `AccrueStoppageLimit` hold the stamp, F and LIMIT there; `PastPeriodLength` and `TimeUpGateOpen` are false.
   - **Ponytail, break-end writers 2→3, 4→5, 6→7, 8→9: OPEN** (A6Writers §5). GF starts the next period at the phase change, so no break lasts a frame in the match loop.
   - **Ponytail, knockout flags** `0x66` (extra time) and `0x67` (shoot-out) in `FUN_1404493D0` / `FUN_1404499D0`: no GF setting. GF plays extra time and a shoot-out after a draw (`MatchProgression::GetNext`), so a level match takes the break that leads to them.

2. **Match speed** (`PesUnitsPerFrame`; `PesClock` counts frames)
   - `units per frame = 90.0 · E60 · E5C · (60 / 54) / E58` (A6Writers §6; `FUN_140A6C050`, `FUN_140A6BFC0`).
   - `90.0` is read from `.rdata 0x14259be5c` into `matchSpeedBase`. CONFIRMED: `PesClockInstall.ConstantsComeFromTheExe` asserts 90.0 against the install.
   - `E58` = the match duration, clamped to the menu's 5..90 minutes. **INF(high)**: A6Writers reads E58 as the match-time setting; only E58 = 10 is observed live (10 units a frame, CONFIRMED).
   - `E60` = `E5C` = 1.0. **Ponytail**: both are 1.0 in the live match; GF has no setting for either.
   - Stoppage F and LIMIT take 0.5 per frame at any match length (A6Writers §6, CONFIRMED). Before, they took 0.5 × 54 per game-second, which is 9 a frame at the 5-minute floor. Corrected.

3. **Live-loop stoppage clock** (`match.cpp`, `Match::Process`), the finding from the previous report, now fixed on the PES path
   - Before: the clock ticked only when `IsInPlay() && !IsInSetPiece()`, so PES's stoppage writers (F and LIMIT +0.5 a frame, FUN_14044CE80 step 4) never ran in a live match.
   - Now: `if (pesLoop || (IsInPlay() && !IsInSetPiece())) AdvanceMatchFrames(1);`. Under `pes_loop` the clock ticks every frame and `SetFrozen(!IsInPlay() || IsInSetPiece())` decides whether the stamp moves or F and LIMIT accrue. The off path keeps its in-play gate, so its tick is unchanged.
   - Side effect (INF, not gameplay): the elapsed display clock (`elapsedSeconds`) now includes stoppage under pes_loop. Its only reader is the added-time formula, which gets tier 0 under pes_loop and so adds 0 minutes.

4. **HUD clock: NOT ported.** The 2000-minute base is still OPEN (A6Writers §7). GF's HUD still shows its own elapsed seconds.

## Tests (PES21_ROOT=<workdrive>/PES21, no skips)

- `pes_clock_test`: 51 passed.
- `pes_period_end_test`: 11 passed (10 before this step, plus the new stoppage-through-Process test).
- `pes_clock_match_test`: 2 passed.
- Red for the live-loop fix: `PesPeriodLifecycle.PesLoopAStoppageThroughProcessGrowsFAndLimit` (`0b05c57`). It runs 54 frames of stoppage through `Match::Process` under pes_loop and expects F and LIMIT to grow by 27 and the stamp to hold. On `577b47a`: F 0 vs 27, LIMIT 0 vs 27 (assertions). The stoppage-held assert passed. On `3527b13`: passes.
- Earlier red (`11fec1d`): `pes_clock_test` 15 failed / 36 passed; `pes_period_end_test` 7 failed / 3 passed.

Mutations (each applied alone, built, run, reverted; tree re-verified equal to the committed green):

| # | Mutation | Caught by |
|---|---|---|
| M1 | period 3 level → 8 | `ALevelMatchRunsThroughEveryBreak`, `ATiedSecondHalf…` |
| M2 | period 7 level → 10 | `ALevelMatchRunsThroughEveryBreak`, `ATiedSecondExtraTime…` |
| M3 | `EndPeriod` ignores the score | `ATiedSecondHalf…`, `ATiedSecondExtraTime…` |
| M4 | `Advance` moves a break's stamp | `ABreakHasNoClock` |
| M5 | rate drops the E58 divisor | 5 tests (`TheStampMovesOneHundred…`, `GateOpensExactly…`, `AddedMinutesHold…`, `ExtraTimePeriods…`, `StoppageTimeCounts…`) |
| M6 | stoppage F scaled by game speed | `AStoppageHoldsTheStamp…`, `StoppageTimeCounts…` |
| M7 | match length not clamped (2 min → 2, not 5) | `TheMatchSpeedUsesTheClampedMatchLength` (50 vs 20) |
| M8 | stoppage LIMIT uncapped | `TheStoppageLimitTakesHalfAUnit…CapsAt21600` |
| M9 | pes_loop clock back to in-play gate (the original bug) | `PesLoopAStoppageThroughProcessGrowsFAndLimit` (F 0 vs 27, LIMIT 0 vs 27) |

## Runs

- **Off path, 2-min, seed 7** (no `--pes-loop`): `[determinism] a3d5e43a3b7b03af`, `[ticks] 38693`. Same hash as the baseline in this worktree and as the brief's value. Off path unchanged by construction: the in-play gate is kept verbatim.
- **`--pes-loop`, 20-min, seed 7**: exit 0, reaches full time. `[determinism] 6cdbb4f0fca30a74`, `[ticks] 135081 sim ticks to full time (cap 194400)`, `[restarts] 44 stoppages, longest 23.1 s (1246 ticks), bound 32.5 s (1757 ticks)`, `[balance] shots 4-4`, `possession 37%-63%`.
- **`--pes-loop`, 2-min, seed 7**, twice: exit 0, `[determinism] 7f95fb0ea876d7ff` both times, `[restarts] 23 stoppages, longest 20.6 s (1110 ticks)`, `[ticks] 51818`. This hash moved from `0da088d2496757bc` (pre-fix) as expected: stoppage time now reaches F and LIMIT, so the time-up and stoppage counts change.
- Full gate not run (Main gates). No origin/main `--pes-loop` baseline run in this worktree.
- Build: targets `gfsim_run pes_clock_test pes_period_end_test pes_clock_match_test` built clean. One pre-existing warning at `humanoid.cpp:2870` (not mine).

## Open items for Main

1. Break-end writers 2→3, 4→5, 6→7, 8→9 (OPEN, A6Writers §5). Needs the phase-block writers traced; the phase change moves to the restart when the walk-off is ported.
2. `E60`, `E5C` (ponytail = 1.0) and `E58` as the match duration (INF high). Settle with a live match at another setting.
3. Knockout flags `0x66`/`0x67` (the GF tie rule is a stand-in).
4. HUD origin (2000-minute base): OPEN. Needs a kickoff sample in a fresh match, or the modulo test at clock > 4000 minutes.
5. Install rate VA: A6Writers §6 names `DAT_14351AE50` for the rate 54; the code's `GAME_SPEED_VA` is `0x14351ae54` (read as 54.0 by the install test). The `0x14351ae50` read was not checked.
6. The 2-min `--pes-loop` hash changed with the live-loop fix. Main's batch smoke baseline needs refreshing for pes_loop.
