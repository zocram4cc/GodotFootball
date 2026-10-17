# BallTick handover state — Milestone A1 (54 Hz sim + PES ball tick)

Worktree `<workdrive>/gf-BallTick`, branch `BallTick` (detached-then-branch from
`origin/main` `40d3525`), scratch `<workdrive>/gf-BallTick-scratch`.
NOTHING committed, NOTHING pushed. Build directory `build/` configured and green
(RelWithDebInfo, `nice -n 19 cmake --build build -j3` → rc=0 on the last full run).

## Rebase status (done, verified)

`origin/Wave1MBF` (`faae222` "AI reads interpolate", `5714da4` "pesballtrack",
`7f61a5a` "pesmover accessors") is rebased onto `origin/main` `40d3525`. All three
MBF commits are in the branch history at their ORIGINAL SHAs. Two conflicts, both in
`tests/sim/CMakeLists.txt` (both sides add different `add_executable` blocks), were
resolved by keeping both sides. There is no third commit yet — all A1 work below is
uncommitted working-tree changes.

## What each file changed, and what covers it

### The tick itself

| file | change | tests |
|---|---|---|
| `src/sim/onthepitch/match.hpp` | `kTick_ms` → `static constexpr double kTick_ms = 1000.0 * pesball::kPesTick_s`; added `kFramesPerSecond` (54); added `GetSimFrame()`, `IsFrameSecond()`; `GetSimTime_ms()` now returns double, `GetActualTime_ms()`/`GetMatchTime_ms()` cast; time members (`actualTime_ms`, `matchTime_ms`, `goalScoredTimer`, `stalemate*`, `matchTimeExact_ms`) → double | **partly**: `pes_tick_a1_test.PesA1Tick.TheSimTickIsPesOneFiftyFourthOfASecond` asserts `kPesTick_s` ≈ 0.0185185. No test asserts `kTick_ms`'s value or `IsFrameSecond`/`GetSimFrame`. |
| `src/sim/onthepitch/pesballphysics.hpp` | `kGfTick_s = kPesTick_s`, `kGfTickHz = pes::kPesTickHz`; `KeepFactorPerStep` doc + `ContactTick` doc updated | `pes_ball_physics_test.PesBallConversion.KeepFactorIsItselfOnTheSimsOwnTick` (rewritten) |
| `src/sim/onthepitch/pesball.hpp` | header comment: tick is PES's, factors apply as themselves | — |

### Frame-counted timers converted at the call site

| file | change | tests |
|---|---|---|
| `src/sim/onthepitch/match.cpp` | `% 1000` gates in `ProcessAutoSubstitutions` and `UpdateBallHeatmap` → `!IsFrameSecond()` | none |
| `src/sim/onthepitch/teamAIcontroller.cpp` | `UpdateTactics` gate → `match->IsFrameSecond()` | none |
| `src/sim/onthepitch/player/player.cpp` | per-second position-history gate → `IsFrameSecond()`; `desiredTimeToBall_ms -= Match::kTick_ms`; `possessionDuration_ms += Match::kTick_ms`; tactical-situation gate → `(GetSimFrame() + GetID()) % 10 == 0` | none |
| `src/sim/onthepitch/player/player.hpp` | `possessionDuration_ms`, `desiredTimeToBall_ms` → double; accessors return/accept double | none |
| `src/sim/onthepitch/team.cpp` | dynamic roles gate → `(GetSimFrame() + 20 * id) % 40 == 0` | none |
| `src/sim/onthepitch/player/humanoid/humanoid.cpp` | requeue predicates (was `%20/30/40/50/80` on ms) → `(GetSimFrame() + 2) % 2/3/4/5/8 == 0`; debug print → `GetSimFrame() % 25 == 0` | none |
| `src/sim/onthepitch/player/controller/elizacontroller.cpp` | carrier `nowFrame` = `possessionDuration * 54/1000`; safety-veto elapsed → `/ Match::kTick_ms`; lane/receiver `* Match::kTick_ms * 0.001 / kGfTicksPerPesTick` → `* pesball::kPesTick_s`; dodger `framesSinceEvent` → `/ Match::kTick_ms` | none |
| `src/sim/onthepitch/referee.cpp` | `MayTakeRestart` elapsed frames → `/ Match::kTick_ms`, fps → `Match::kFramesPerSecond` | none |
| `src/sim/onthepitch/pesstamina.{hpp,cpp}` | deleted `kGfTick_s`/`kPesTick_s` (now identity); drain and breath deltas applied unscaled | none |
| `src/sim/onthepitch/pesdefenderdecisions.cpp` | `FramesToTicks` is identity now | none |
| `src/sim/onthepitch/pescarrier.hpp` | deleted dead `PesFramesToGfTicks` / `kGfTicksPerPesTick` | `pes_carrier_test.PesCarrierCadence.*` (rewritten, still passing) |

### The live ball on PES's tick

| file | change | tests |
|---|---|---|
| `src/sim/onthepitch/ball.cpp` | `CalculatePrediction` rewritten: loop is `tick = 1..kTrackLength` on `kPesTick_s`, writes `track[tick]` directly and `track[0] = positionBuffer`; the separate interpolated sampler call (`BuildTrack`) is GONE; GF's `predictions[]` grid removed throughout; netting moved into the `tick == 1` branch and fed back into the Fox state; `contactTicks` increments only when `tick == 1`; `GetAveragePosition` counts ticks instead of `total * 10`; `Process` sets `positionBuffer = track[1]` | `pes_tick_a1_test.PesA1LiveBall.*` (3 tests) via the pure sampler, not through Ball |
| `src/sim/onthepitch/ball.hpp` | `predictions[]` member deleted; `Predict(ms)` = `pesball::TrackAt(track, ms)` (floor read); `GetTrack()` restored; `GetPreviousPosition()` added; `CalculatePrediction` comment | `pes_ball_track_test` (8/8) |
| `src/sim/onthepitch/pesballtrack.hpp` | `TrackAt` is now a FLOOR read; `TrackIndexForMs` (new); `TrackIndexForAiMs` (new); `TrackRecord(track, n, records)` overload (new); forward decls added for two-pass compilation | `pes_tick_a1_test.PesA1Tick.TheTrackIsReadByFloorNotByInterpolation`, `pes_ball_track_test.ReadsBetweenTwoRecordsFloorToTheEarlierRecord` |
| `src/sim/onthepitch/AIsupport/mentalimage.{hpp,cpp}` | `GetBallPrediction` uses `TrackRecord` + `TrackIndexForAiMs` (floor, horizon-clamped); new `GetBallPredictionAtRecord(time_ms, records)` | none directly |
| `src/sim/onthepitch/AIsupport/AIfunctions.cpp` | the intercept `ballMovementRough` (site ~872) = record difference over 1 record × `pes::kPesTickHz`; the `ballMovement3D` site (~1130) likewise via `TrackRecord(ball->GetTrack(), TrackIndexForMs(10.0), 1)` | `pes_tick_a1_test.PesA1FiniteDifference.RecordDifferenceTimesTheTickRateIsMetresPerSecond`, `.OneRecordSpacingNeverYieldsZeroMovement`, `.TenMillisecondsFurtherIsOnePesRecordAhead` |
| `src/sim/onthepitch/player/humanoid/humanoid.cpp` | the three remaining 10 ms finite differences: incoming-ball-direction (~2144) over 3 records; touch decision (~2859) and anim touch frame (~3148) over 1 record × `pes::kPesTickHz`; new `kIncomingDirectionRecords = 3`, `kBallMovementRecords = 1` in the file's anonymous namespace | none of these three sites has a dedicated test |
| `src/sim/onthepitch/matchdata.{hpp,cpp}` | `AddPossessionTime_10ms(int)` → `AddPossessionTime(int, double tick_ms)`; `matchTime_ms`, `possessionTime_ms[]` → double | none |
| `src/sim/onthepitch/matchreport.cpp` | determinism fold uses `match.GetSimFrame()` instead of `GetSimTime_ms() / kTick_ms` | none |

### Render interpolation (GF side)

| file | change | tests |
|---|---|---|
| `src/gdext/gf_match.{hpp,cpp}` | `TICK_S = 1/54`; new bound methods `player_position_previous`, `ball_position_previous`; `step()` snapshots the previous logic frame's player positions and ball position into `previousPlayers_`/`previousBall_` | none |
| `godot/match/match.gd` | new `_interpolate(alpha)` called after the fixed-step accumulator drains; `_place` no longer writes positions (only facing + locomotion phase + camera); `SNAP_DISTANCE_SQ` teleport snap; `_lastAlpha` state | none (GDScript, no test harness) |

### Test files changed

| file | change |
|---|---|
| `tests/sim/pes_tick_a1_test.cpp` | NEW, 9 tests, all passing |
| `tests/sim/CMakeLists.txt` | added `pes_tick_a1_test` target |
| `tests/sim/pes_ball_track_test.cpp` | 3 tests rewritten from interpolation to floor reads (8/8 pass) |
| `tests/sim/pes_ball_physics_test.cpp` | `PesBallConversion.KeepFactor*` rewritten (25 pass, 3 install-skipped) |
| `tests/sim/pes_carrier_test.cpp` | cadence test rewritten for frame==tick |
| `tests/sim/sim_match_test.cpp` | throw-in wait types → double (compile fix only) |

## NOT covered by any test or mutation yet

1. **Every frame-counted timer conversion.** The substitutions gate, heatmap gate,
   `UpdateTactics` gate, position-history gate, tactical-situation gate, dynamic
   roles gate, requeue cadence (all five branches), debug print. Zero tests. The
   conversion is mechanical (ms-modulo → frame-modulo) but nothing pins the
   resulting cadence.
2. **All three humanoid finite-difference sites** (~2144, ~2859, ~3148) beyond what
   the pure-function A1 tests assert about the arithmetic. No Ball-level or
   MentalImage-level test drives them.
3. **`AIfunctions.cpp:1130`** (`ballMovement3D`) — no test.
4. **`MentalImage::GetBallPrediction` / `GetBallPredictionAtRecord`** — no test.
5. **`Ball::Process` / `CalculatePrediction`** at the class level. The A1 live-ball
   tests exercise `pesball::BuildTrack` directly, NOT the `Ball` object. The
   netting-inside-the-track, `contactTicks` counting once, and
   `positionBuffer = track[1]` wiring are all untested.
6. **`IsFrameSecond()` / `GetSimFrame()`** — no test.
7. **`MatchData::AddPossessionTime`** — no test.
8. **Render interpolation** (`gf_match.cpp`, `match.gd`) — no test at all.
9. **Mutations**: none written for the 9 A1 tests, nor for any rewritten test.

## Not started at all (acceptance criteria outstanding)

- Full `ctest` gate — launched as `bg_165`, result never observed. MUST be re-run.
- 8-seed paired table (7, 8, 9, 11, 1000, 2, 3, 4) against main.
- Track error vs live ball at 1 s, ≤ 0.05 m (MBF's diag measured 0.045 m).
- No commit, no push to `refs/heads/BallTick`. `tools/check_pii.sh` not run.

## Known risk to check first

`actualTime_ms` is now a `double`, so any remaining `%` on it, or any
`(unsigned long)` truncation in a comparison that assumed exact 10 ms multiples,
is a silent cadence change. Consumers found but NOT audited:
`referee.cpp` (many `GetActualTime_ms()` comparisons),
`matchpressure.cpp`/`pitchconditions.cpp` (`matchTime_ms` params),
`penaltyshootoutcontroller.cpp` (`nextEventTime_ms`, `kickStartTime_ms`),
`teamAIcontroller.cpp:2169` (`static_cast<long>(GetActualTime_ms() * 54/1000)` — still
correct but worth confirming), and `humanoid_utils.cpp`. A `grep`-driven pass over
`GetActualTime_ms()`/`GetSimTime_ms()`/`kTick_ms` consumers is the first task.
