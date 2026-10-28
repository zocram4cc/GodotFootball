# pes-port-StoppageFlow3: merge, late card, end-line outs, phase table (27-10-26)

**Status.** Merged and built; late-card step ported with a red-then-green test. **Not green:** 4 of 96 `pes_*` binaries fail or do not finish (section 2). The end-line throw-in at seed 3, t=246759 ms **did not reproduce** on the merged tree, so it is not fixed (section 3). The 10-minute matches are **not** the StoppageFlow2 gain on the merged tree (section 6).

Branch `StoppageFlow` on `origin` (ls-remote confirmed): `fe103340` (merge) → `5f542177` (late card, bracket test) → `421dbbf0` (goal-kick exemption reverted, HEAD).

## 1. Merge and build

- `git merge origin/main` (`a3d2aaf3`, docs only) then `git merge origin/Cutover` (`fe103340`): both clean, no conflicts. Cutover changed `src/sim/onthepitch/pesoffball.cpp` (`OppNear` counts only active opponents) and `tests/sim/pes_team_tick_test.cpp` (now compiles).
- Full build, `-j2 -k 0`, after the card change: exit 0, zero failed targets (`build_final.log`).
- Switch-off mode (`gfsim_run` 5 min seed 7, no `--pes-loop`): `[determinism] 7ca3d08f68567016`, `[ticks] 31521`. Unchanged.

## 2. Suites (every `build/tests/**/pes_*` binary, one sim slot each, 1800 s limit)

92 binaries exit 0, including the unverified `pes_restart_taking_test`, `pes_goal_kick_test`, `pes_setpiece_taker_test`, and `pes_team_tick_test`. Not green:

| binary | result | status |
|---|---|---|
| `pes_stoppage_timing_test` | 9/10 pass. `AMeasuredStopRunsPesBracketNotFifteenSeconds`: 3 free-kick stops (`e_SetPiece_FreeKick` = 3) take 10330 ms against brackets 4830 / 7330 / 10496. | RED, pre-existing (same 10330 ms stops in the 14:39 log on the StoppageFlow tree). R1: deferred card over-issued on free kicks. Not fixed. Goal kicks (kind 2, 5 takes) and throw-ins (kind 5, 3 takes) pass at 4830. |
| `pes_stall_test` | `ALiveBallNeverLiesStillLongEnoughForTheBreaker`: breaker forces a live ball for 60 ticks in the seed-7 full match. | RED. Not localised. Nearest change: Cutover `OppNear` (`pesoffball.cpp`). |
| `pes_throwin_release_test` | 4/5 pass. `ACarriedThrowInIsHeldAtItsRowsKeyOffsetUntilTheTouch` fails at line 174 (`EXPECT_EQ`, actual 1). | RED. Not localised. |
| `pes_restart_delay_test` | Exit 124 at 1800 s; every test that ran was OK (through `AReleasedThrowInCarriesItsBallInsideTheTouchline`). | Incomplete. A 3600 s run was started (`suite_pes_restart_delay_test.full.log`); its result is not in this report. |

## 3. End-line throw-in, seed 3, t=246759 ms: NOT REPRODUCED

- Evidence: the test's seed-3 run (`AThrowInIsReleasedFromInsideTheLine`, `kSeed = 3`) gives the same event list in three runs: merged tree; merged tree with `OppNear` reverted; the three cases run in file order. There is no stop at 246759 ms. The only ball-out events are touchline outs with `|y| >= 34` (98925, 330074, 384296 ms) and end-line outs during an armed restart (286777, 373111 ms, no new award). No throw-in is awarded at end-line coordinates.
- Code: the only PES-loop writer of `e_SetPiece_ThrowIn` is the touchline branch (`referee.cpp`, `overSideline`: `PesOutOfPlayEvent() && |x| <= F0 && |y| > F1` on `Predict(0)`). The other writer (`GetDeadBallRestart`) is GF-only (`!PesLoop()`).
- The StoppageFlow2 trace (`[stop] ... kind=2 ballX=52.72 ballY=-8.18`) implies a 26 m gap between the referee's `Predict(0)` and the ball. The track cannot produce that here: `Predict(0)` is `track[0]`, one tick behind `positionBuffer`; `Ball::SetPosition` ends in `SetRotation`, which calls `CalculatePrediction()` (`ball.cpp:140`), so teleports refresh the track. The StoppageFlow2 trace build is not recoverable from the same commit (999c300d reproduces this trajectory).
- Result: no red test (a green test would prove nothing), no fix. **OPEN.** Next step: re-trace from the StoppageFlow2 binary, or add an end-line invariant test once the seed-3 award reproduces.

## 4. Late card (PES 9.5, 9.8): ported on the PES path

- Decode: `docs/re/pes-re-foul-choreography.md` 9.5: step 6 is `FOUL_CMN_CARD_Y_B` (`foul_cmn_card_y02`, 340 ticks at 60 fps), and the step is max(2.5 s, scene). 9.8 table: `CARD_Y 40-340`. Hold table `table[5]` = 2.5 s (`pes-re-A5Judge.md` 233, VA `0x14259ce54`).
- Conflict in the decode: the step table row 6 (`pes-re-foul-choreography.md` ~309) says "2.5 s base + scene". §9.5 says max. Used max (the brief's rule). The ball-out builder was not read, so "the step replaces the out-of-play hold" is INF (the offside builder `[3, 6, 0x45]` omits the hold).
- Implementation: `pes::LateCardStep_ms(hold)` = max(hold, 340×1000/60 = 5666 ms); `pes::LateCardOnScreenAt_ms(stop)` = stop + 40×1000/60 = stop + 666 ms. PES path of `Referee::IssueDeferredCards`: card gate and show time use them; the armed restart shifts by `LateCardStep_ms(2500) − 2500` = 3166 ms (`prepareTime`, `startTime`). Switch-off path keeps its 3.0 s stand-in (`kGfCardOnScreen_ms`, unchanged shift). The direct foul card keeps the 3.0 s stand-in (its scene start, 9.8 gate + intro + REF_CARD start, is not ported).
- Red then green: `LateCardShowsAtSceneTickFortyAndRunsTheLongerOfTheHoldAndTheScene` failed to compile (`LateCardStep_ms` missing), then passes. The measured-bracket test uses `LateCardStep − hold` as the late-card extra.
- Mutation: **not run** (budget). Required before landing.
- ponytail: the 340 ticks and 40-tick start are constants with the decode citation; read them from `foul_cmn_card_y02` when the scene table is loaded. The scene's "second frame" start is ignored (INF, one frame).

## 5. End-line outs (corners and goal kicks per 10-minute match)

`gfsim_run --pes-loop --parity-log --minutes 10`; corners and goal kicks from `[parity-restart]`.

| run | corners | goal kicks | end-line outs |
|---|---|---|---|
| GF before, seed 7 (`67e9a1aa74e84e0a`) | 0 | 4 | 4 |
| GF before, seed 3 (`f83ae25fb5e6bdcd`) | 0 | 2 | 2 |
| GF merged + card, seed 7 (`d445a12bfcfe3ce1`) | 2 | 5 | 7 |
| GF merged + card, seed 3 (`b6b3a9a86e453c37`) | 1 | 3 | 4 |
| PES live, one match, H2 only (`pes-re-live-flow.md` 5.2) | 4 | 0 | 4 |

- The premise "GF never sends the ball over the end line" is false: GF sends it out 2–7 times per match. Corners appeared only on the merged tree.
- The split is inverted: GF's end-line outs are 8 goal kicks to 3 corners; PES's H2 sample is 4 corners to 0 goal kicks (n = 1 match). A goal kick is awarded iff the last toucher is the attacking team, so GF's end-line outs are mostly attacker last touches. `EndLineRestart` matches PES's rule (`pes-re-A5Judge.md` 9.1). **OPEN:** which contacts set the last touch at end-line outs (keeper parries, deflections). Not traced.
- The PES live poller (`liveflow.jsonl`, about 2M lines) has more matches. The analyzer fragments stoppages, so only the one-match H2 sample is used here.

## 6. Phase table and match length

Frames at 54 fps, from `[parity-phase]`; medians; before = base runs (`67e9a1aa`, `f83ae25f`), after = merged + card (seeds 7 and 3).

| kind | before n | W→ready | ready→play 1 | W→play 1 | after n | W→ready | ready→play 1 | W→play 1 | PES (live) |
|---|---|---|---|---|---|---|---|---|---|
| corner | 0 | | | | 3 | 173 | 366 | 539 | ready→release 365, W→release 570 |
| free kick | 16 | 438 | 475 | 913 | 9 | 470 | 356 | 826 | ready→release 355, W→release 668 |
| goal kick | 6 | 276 | 175 | 451 | 8 | 173 | 143 | 316 | no sample |
| kick-off | 14 | 1107 | 108 | 1215 | 17 | 1107 | 135 | 1242 | ready→release 134 |
| throw-in | 8 | 276 | 93 | 369 | 9 | 173 | 65 | 373 | no sample (n = 0) |

Match length (`--minutes 10 --pes-loop`):

| tree | seed 7 ticks | seed 7 hash | live share | seed 3 ticks | seed 3 hash | live share |
|---|---|---|---|---|---|---|
| before (`67e9a1aa`) | 58893 | 67e9a1aa74e84e0a | 58.7% | 52602 | f83ae25fb5e6bdcd | 66.3% |
| StoppageFlow2 (`999c300d`) | 46973 | 20cc1c13139bf4df | | 44794 | 5a17c9e545a109c7 | |
| merged + card (`421dbbf0`'s code) | 58645 | d445a12bfcfe3ce1 | 60.3% | 54122 | b6b3a9a86e453c37 | 66.9% |

- **The StoppageFlow2 length gain is not on the merged tree.** Its match lengths are back at the before values. Two changes separate the trees: Cutover's `OppNear` and my late card. Not localised (one 10-minute run with `OppNear` reverted would settle it, budget). A 20% length swing from a small change means one seed each is weak evidence.
- Live share = `[parity-state]` live ticks / total ticks.

## 7. Open items (numbered)

1. Seed-3 end-line throw-in: not reproduced (section 3).
2. `pes_stoppage_timing_test`: free kicks take 10330 ms (R1, over-issued deferred card on free kicks; `IssueDeferredCards` runs on stops with a pending card). Fix R1 first.
3. `pes_stall_test`: breaker forces a live ball 60 ticks in seed 7.
4. `pes_throwin_release_test`: carried throw-in hold, line 174.
5. `pes_restart_delay_test`: finish a full run (3600 s run in progress).
6. Merged-tree match length: localise `OppNear` vs the card change; the StoppageFlow2 length is not robust.
7. Corners and last touch at end-line outs (section 5).
8. Direct foul card on screen at 3.0 s (9.8 scene not ported).
9. Late-card mutation check not run.
10. `flowstats_match.py` fragments stoppages; the PES live sample is one match (H2).
11. Goal-kick ladder timing (`pesgoalkick.hpp`): PES stop-to-take unmeasured (n = 0).
