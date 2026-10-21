# PES-vs-GF parity: metric script, opt-in observer, landing row 860e2f1 (merged to origin/main 21d8b0c)

> **Main corrections 21-10-26:** 21d8b0c is hash-neutral (the stall report is printf-only; the kickoff re-enable is a test change), so read every "behaviour change" note about it below as void. F7 and open item 3 (a null currentAnim) are void: HumanoidBase allocates it in the constructor and frees it only in the destructor. Open item 1 is decided: period-start kickoffs stay in M3.

Status. Branch `Parity` head `e146ee2` (remote confirmed by `git ls-remote`). origin/main `21d8b0c` already contained the four earlier Parity commits (a11018c, 86edbfc, 3a529ea, e133493) through merge bd15b5e. The Parity branch was fast-forwarded to 21d8b0c, then one comment-only commit (e146ee2) was added. The row measured is main 860e2f1 (its tree equals origin/main 016c504 except tasks/19-10-26.md). No row exists yet for 21d8b0c.

## What is on main

- `tools/dev/parity.py`: runs the three seeds (pes-loop, `--parity-log`, 20 min) under `GF_SIM_LOCK`, parses the output, prints the table. `--record SHA` writes the landing row. `--logs DIR` re-measures saved logs without a sim. Distances print once the PES baseline is filled.
- `apps/gfsim_run.cpp`: `--parity-log`, a read-only `ParityLog` observer. It prints `[parity-state]`, `[parity-stop]`, `[parity-restart]`, `[parity-actions]` after the report and writes nothing back to the sim.
- `tests/parity_metrics_test.py` (ctest `parity_metrics_test`): 14 behaviour tests.
- `docs/plans/pes-parity-metrics.md`: run instructions, PES value convention, per-landing table (row 860e2f1).
- Main, 381b4cf: M8 is now the `[restarts]` stoppage count minus the opening kick-off; distance headers on M8, M9.
- Main, 21d8b0c: `[stalemate-who]` lines; TheKickoffTakerClosesToTheBall re-enabled. This changes sim behaviour, so 21d8b0c is a different measurement from 860e2f1.

## Metric sources

| id | GF source | notes |
|---|---|---|
| M1 | `MoveStepRecord::state` (P+0x245c), all active players, every tick | share of player-ticks over all ticks |
| M2 | same stream: transitions into STOP (8), keyed by the previous observed state | count per 20 in-play min |
| M3-M7 | restart record in the observer: whistle = first non-live tick, release = first live tick, type = last set piece a controller named in the stoppage | frames, median per seed, mean over seeds. The opening kick-off is excluded. Period starts are included (see F1). |
| M8 | `[restarts]` `MeasuredStoppages`, minus the opening kick-off (381b4cf) | count per 20 in-play min |
| M9 | `[restarts]` `LongestTicks` | s |
| M10 | `MatchData::GetPassAttempts` (both teams) | count per 20 in-play min; GF proxy, not PES action id |
| M11 | `MatchData::GetCrosses` (set in humanoid.cpp: wide, attacking third, inward high pass) | GF proxy |
| M12 | `MatchData::GetShots` (clearances excluded) | |
| M13 | `GetChallengesTackles` (standing challenges committed) | |
| M14 | `GetChallengesSlides` | |

## Normalisation (CONFIRMED)

A count becomes `count x 64800 / live`, where `live` is the number of ticks with `IsInPlay() && !IsInSetPiece()` and 64800 = 20 min x 60 x 54 Hz. In pes-loop the clock is frozen exactly outside play (`SetFrozen(!IsInPlay() || IsInSetPiece())`), so `live` is clock time.

`--minutes` sets PES's match-length factor `F = clamp(minutes, 5, 90) / 90` (`matchduration.hpp`). It changes the total length of a run. For seed 7 at `--minutes 20`: 118,759 ticks in total, 95,400 live (about 29.5 in-play minutes). A 2-minute run (clamped to 5) gave 46,308 total and 29,959 live. The live-tick normalisation is therefore required, not cosmetic.

## Determinism proof (860e2f1 code; `--parity-log` is read-only)

| mode | seed | no flag | `--parity-log` |
|---|---|---|---|
| pes-loop | 7 | 522aa55a5bec14fd | 522aa55a5bec14fd |
| pes-loop | 11 | 1392d044f90fb2c5 | 1392d044f90fb2c5 |
| pes-loop | 23 | 05696f71bd00332b | 05696f71bd00332b |
| off | 7 | 4fd4da5695b5ebd5 | 4fd4da5695b5ebd5 |
| off | 11 | a8d8037924cfae59 | a8d8037924cfae59 |
| off | 23 | 20ad16b76a0b82dd | 20ad16b76a0b82dd |

The unmodified 016c504 binary (no flag) also gives 522aa55a5bec14fd for pes-loop seed 7. Every flagged/unflagged pair is identical apart from the `[parity-*]` lines and the wall-clock `[profile-split]` line (0 differing lines per pair). CONFIRMED for the 860e2f1 sim. NOT re-run on 21d8b0c, whose sim behaviour changed (see open items).

## GF means, 860e2f1, seeds 7/11/23 (pes-loop, 20 min)

Raw: determinism 522aa55a5bec14fd / 1392d044f90fb2c5 / 05696f71bd00332b. Sim ticks 118,759 / 124,133 / 120,559. Live ticks 95,400 / 91,099 / 90,213.

| metric | GF mean | min..max | unit |
|---|---|---|---|
| M1 IDLE (1) | 32.87 | 31.86..34.70 | % of player-ticks |
| M1 IDLE_TURN (2) | 1.31 | 1.19..1.53 | % of player-ticks |
| M1 RUN_LOOP (3) | 6.23 | 5.50..6.86 | % of player-ticks |
| M1 STEP_LOOP (4) | 0.00 | 0.00..0.00 | % of player-ticks (13 player-ticks in seed 7, F5) |
| M1 TURN_STOP (6) | 5.94 | 5.50..6.47 | % of player-ticks |
| M1 GOTO_RUN (7) | 14.69 | 13.98..15.32 | % of player-ticks |
| M1 STOP (8) | 0.09 | 0.07..0.10 | % of player-ticks |
| M1 ACCELERATE (9) | 15.35 | 14.82..15.93 | % of player-ticks |
| M1 DECELERATE (10) | 0.13 | 0.10..0.15 | % of player-ticks |
| M1 RUN_TURN_RUN (12) | 21.94 | 18.91..24.72 | % of player-ticks |
| M1 RUN_TO_STEP (14) | 0.02 | 0.01..0.02 | % of player-ticks |
| M1 STEP_TURN_RUN (15) | 1.44 | 1.26..1.53 | % of player-ticks |
| M2 STOP from RUN_LOOP (3) | 587.54 | 514.87..650.14 | count per 20 in-play min |
| M2 STOP from ACCELERATE (9) | 340.71 | 304.98..379.26 | count per 20 in-play min |
| M2 STOP from STEP_TURN_RUN (15) | 123.24 | 109.36..147.25 | count per 20 in-play min |
| M2 STOP from RUN_TO_STEP (14) | 79.30 | 48.23..121.39 | count per 20 in-play min |
| M2 STOP from GOTO_RUN (7) | 77.51 | 54.34..109.90 | count per 20 in-play min |
| M2 STOP from DECELERATE (10) | 12.25 | 8.15..16.52 | count per 20 in-play min |
| M2 STOP from RUN_TURN_RUN (12) | 8.97 | 4.75..11.38 | count per 20 in-play min |
| M2 STOP from STEP_LOOP (4) | 0.24 | 0.00..0.71 | count per 20 in-play min |
| M3 kick-off, whistle-to-release | 713.17 | 711.00..716.50 | frames, median (n 2..3); period starts only (F1) |
| M4 throw-in | 521.00 | 520.00..523.00 | frames, median (n 11..17) |
| M5 goal kick | 552.33 | 552.00..553.00 | frames, median (n 5..15) |
| M6 corner | 814.33 | 802.00..822.00 | frames, median (n 1..5) |
| M7 free kick | 976.33 | 975.00..978.00 | frames, median (n 5..7) |
| M8 stoppages, opening kick-off excluded | 24.72 | 17.66..31.61 | count per 20 in-play min (25.42, 18.34..32.32 before 381b4cf) |
| M9 longest stoppage | 20.03 | 18.50..21.10 | s |
| M10 passes | 37.86 | 28.53..42.68 | count per 20 in-play min |
| M11 crosses | 2.37 | 0.71..5.03 | count per 20 in-play min |
| M12 shots | 7.58 | 2.72..10.77 | count per 20 in-play min |
| M13 tackles (standing) | 2.35 | 1.42..3.59 | count per 20 in-play min |
| M14 slides | 8.92 | 7.47..10.06 | count per 20 in-play min |

M1 means sum to 100.0. The M8 row is the re-measure of the saved logs with the 381b4cf definition (`--logs`, no sim).

## Row recorded

`docs/plans/pes-parity-metrics.md`: `|860e2f1|2026-10-21|pending|pending|pending|pending|pending|pending|pending|`. Distance cells stay pending until the PES round-36 values are filled.

## Findings

F1 (CONFIRMED from logs and code). M3's samples are period-start kick-offs, not kick-offs after a goal. Kick-off whistle ticks: seed 7 at 36650, 77792, 94091; seed 11 at 39742, 80251, 97757; seed 23 at 43408, 84900. All three seeds ended 0-0, so no goal produced a kick-off. The referee sets the match phase on the period-end whistle (`referee.cpp`, line 308), before the observer sees the stoppage, so the observer's phase check cannot drop a period start. My previous report said period starts were skipped. That was wrong. The observer comment now says so (e146ee2); the output is unchanged.

F2 (CONFIRMED). On goal-less seeds, M3 is the period-start walk-off, about 13 s (711 frames at 54 Hz), not a post-goal kick-off. None of the three seeds has a post-goal kick-off sample.

F3 (count CONFIRMED; cause OPEN). Seed 23 has two period-start kick-offs, where seeds 7 and 11 have three. Its `[restarts]` count is 45, which equals 44 named restarts plus the opening. So the missing third period start produced no measured stoppage at all; no log row was dropped. The sim prints nothing at period ends, so the cause is not traced. It could be a sim fault (play running through a period end) or a referee path that never stops play. INF.

F4 (CONFIRMED). `--minutes` sets the match-length factor F (see Normalisation). It is not a stamina-only setting, as my previous report said.

F5 (CONFIRMED). M1 STEP_LOOP (state 4) had 13 player-ticks in seed 7 out of about 2.61 million (0.0005%). The "4 player-ticks" in the previous report came from the 2-minute smoke.

F6 (CONFIRMED that the logs agree; rationale NOT verified). `[restarts]` minus the named restarts is exactly 1 in all three seeds (27/26, 36/35, 45/44), so the opening kick-off is the only measured stoppage the named list leaves out. 381b4cf's stated reason, that PES counts from the first whistle, is not verified here.

F7 (CONFIRMED by reading; NOT run). On 21d8b0c, the stall report in `match.cpp` (the `[stalemate-who]` block) calls `who->CastHumanoid()->GetCurrentAnim()` and reads `a->functionType` and `a->frameNum` with no null check on `a`. It checks only `a->anim`. `GetCurrentAnim()` returns the raw `currentAnim` pointer (`humanoidbase.hpp`, line 238). If that pointer is null for a taker or forced man at a stalemate report, the run crashes.

F8 (CONFIRMED). 21d8b0c re-enables TheKickoffTakerClosesToTheBall, a sim behaviour change. The 860e2f1 numbers do not describe 21d8b0c.

## Verification (this step)

- Merge: origin/main 21d8b0c fast-forwarded the Parity branch (no conflicts; Parity had no commits origin/main lacked).
- `cmake --build build --target gfsim_run -j3`: rc 0 on 21d8b0c and on e146ee2, no errors.
- `python3 tests/parity_metrics_test.py`: 14 tests, OK, on 21d8b0c and on e146ee2.
- Re-measure of the saved 860e2f1 logs with the current script (`--logs`, no `--record`): M8 24.72 (17.66..31.61).
- Comment-only diff in e146ee2 (checked with `git diff`).
- Sims run in this step: none. Gate not run (Main gates).
- Mutations: no test logic changed in this step. The four mutations from the earlier step still stand (scaling, median, distance denominator, re-record).

## Open items

1. Owner decision on M3. Current behaviour: period starts are kept, so M3 has samples on goal-less seeds. Excluding them needs a `period` field on `[parity-restart]` and a re-run; on these three seeds M3 would then have no samples.
2. F3: trace seed 23's missing third period start. Sim or observer?
3. F7: null check on `a` in Main's stall report.
4. Row for 21d8b0c not recorded. It needs a batch run (about 30 min for three seeds under the shared lock) followed by `tools/dev/parity.py --record 21d8b0c`. Decision for Main.
5. Determinism re-proof on 21d8b0c not run. The flag is read-only, but the sim changed, so the proof is for 860e2f1 only.
6. PES baseline (round 36) still pending, so no distances yet.
7. M8's PES-count rationale (381b4cf) unverified.
8. M10 and M11 are GF proxies (pass attempts, the cross flag), not PES action ids as the doc asks.
