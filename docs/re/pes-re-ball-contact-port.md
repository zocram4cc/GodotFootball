# A1 port: T1 (same-tick landing) and T3 (live contact mode 1), branch BallContact

Result: **Partially done.** The port is complete as specified. The one-step landing acceptance is met (0.000000 m). The 1 s median falls from 0.71 m to 0.39 m on the recorded samples and from 0.80 m to 0.47 m on a new live capture. It does not reach about 0.35 m. T1 alone reaches it (0.35 m on both sets), and T3 is what adds the rise.

Branch `BallContact`: `fc22bf2` (port), on top of `d8498dc` (red tests). Both confirmed by `git ls-remote origin BallContact`. Base `origin/main` `6691a8a`. Worktree `<workdrive>/gf-BallContact`. Nothing merged to main.

## Changes

- `src/sim/onthepitch/pesballphysics.hpp`, `.cpp`: new `BallTick(c, state, tickIndex, dt_s)`, the one PES tick. A resting ball stays still. A flight step that crosses the ground is the landing: `ContactTick` runs in that same tick, with no position step after it. A ball on the ground takes the contact tick, then steps on its new velocity. Returns true when a contact tick ran.
- `src/sim/onthepitch/pesballtrack.cpp`: `BuildTrack` calls `BallTick`. The duplicated if/else chain is gone.
- `src/sim/onthepitch/ball.cpp`: the live predictor calls `BallTick` (same counter rule as before: only `tick == 1` advances `contactTicks`). The duplicated chain is gone.
- `src/sim/onthepitch/pesball.hpp`: `kContactModeLive = 1` replaces `kContactModeDryNormal` as the `PesBallConstants::contactMode` default. `kContactModeDryNormal = 2` is kept (tests use it as the dry-column constant). The ponytail note names the PES site: `FUN_1408723d0` at 0x140872403 and 0x140872411, and the table at 0x14349f718.
- `src/sim/onthepitch/pesballphysics.cpp`: comment on the launch default.
- `tests/sim/pes_ball_track_test.cpp`: two red tests (commit `d8498dc`).
- `tests/sim/pes_ball_physics_test.cpp`: two comments on the default mode.
- `docs/re/pes-re-ball-track-live.md`: status, §2.7 (applied), §2.9 (port measurements).

## Tests

Added (both fail on `origin/main`, both pass after the port):
- `PesBallTrack.AFlightStepThatCrossesTheGroundLandsOnThatRecord`: red on main with `track[1].y = 0.0227`, expected `r = 0.10868593`. Green after.
- `PesBallContact.ABallWithNoChosenModePlaysTheLiveContactMode`: red on main, the rolling track diverges from the mode 1 track from record 50 on (for example 6.0537 vs 6.0412). Green after.

Mutation checks (each applied, rebuilt, run, reverted; full tree then rebuilt and rerun green):
- M1, landing deferred to the next tick (main order): `PesBallTrack.AFlight...` fails, 9 others pass.
- M2, `kContactModeLive = 2`: `PesBallContact.ABall...` fails, 9 others pass. This build also confirmed the M1 revert.
- M3, position step after the landing contact: `PesBallTrack.AFlight...` fails, 9 others pass.

## Verification (test binaries run directly; gate not run, Main gates)

Run with `PES21_ROOT=<workdrive>/PES21` and `PES21_EDIT_SAVE` set, so install-backed tests run, not skip:
- `pes_ball_track_test` 10/10
- `pes_ball_physics_test` 28/28
- `pes_tick_a1_test` 18/18
- `pes_ball_collision_test` 11/11
- `pes_mover_test` 8/8
- `pes_clock_match_test` 9/9
- `pes_period_end_test` 5/5
- `pes_stoppage_timing_test` 10/10
- `sim_match_test` 8/8

Build: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release`, then `cmake --build build -j3` on those targets, exit 0. Warnings seen are pre-existing, in `animcollection.cpp` and `humanoid.cpp`. An earlier build was killed at its 900 s deadline and ninja printed "premature end of file; recovering" from its deps log. The next build recovered. gfsim_run smoke not run (agents do not run it).

Worktree note: `git status` reports `extern/godot-cpp` as modified because of the brief's symlink. It is not staged or committed.

## Measurement

Harness `a1_port_probe.cpp` (scratch `<workdrive>/gf-BallContact-scratch/`, not committed) calls the worktree's `pesball::BuildTrack` on the recorded samples and on a new capture. Each sample's e54 uses its own start state. The harness reproduces the doc's main row exactly (M1 with mode 2: median 0.7105, mean 0.9016, max 4.2996, 25 within 0.05 m) and the doc's T1 and T1+T3 rows.

| configuration | recorded, 253: median / mean / max / n≤0.05 m | live capture 3, 168: median / mean / max / n≤0.05 m |
|---|---|---|
| main as shipped (landing one tick late, mode 2) | 0.711 / 0.902 / 4.300 / 25 | 0.801 / 0.965 / 5.286 / 17 |
| port, T1 only (mode forced 2) | 0.349 / 0.623 / 4.169 / 43 | 0.357 / 0.637 / 7.025 / 29 |
| **port, T1 + T3 (default)** | **0.386 / 0.708 / 5.587 / 34** | **0.472 / 0.759 / 8.067 / 27** |

One-step landing error (PES airborne at k, on ground at k+1): port max 0.000000 m over 1092 recorded and 872 live landing steps. Main: max 0.167 m, median 0.0030 m on the recorded set. The doc's §2.7 quotes 0.159 m. The difference is the landing rule in the harness.

Live capture 3: 240 s read-only `pread` of `PES2021.exe` via `live/capture.py`, 11,306 snapshots, 168 verified samples via `analyze_samples.py`. PES was running a match with the ball in play. Captures are in the scratch dir, not committed.

## Evidence and verdicts

- T1, same-tick landing: CONFIRMED, live. One-step landing error 0.000000 m over 1092 recorded and 872 live landing steps.
- T3, live contact mode 1: CONFIRMED, live (horizontal keep 1.00000 under mode 1, 1.00304 under mode 2, doc §2.6). Table at VA 0x14349f718 gives mode 1 at (pitch, weather) = (0, 1) and (1, 1) (doc `pes-re-ball-contact-2.md` §3.1).
- Which pitch and weather index the live match uses: OPEN. Writer at `FUN_1408723d0`, 0x140872403 and 0x140872411. Owner call, per the doc.
- T2, lift after a landing: OPEN, not ported (out of scope).
- T4, rolling spin blend: OPEN, not ported.

## Open items

1. Acceptance on the 1 s median: T1 + T3 gives 0.386 m (recorded) and 0.472 m (live), not about 0.35 m. The owner needs to decide whether T3 stays on before T2 is ported. The doc's §2.6 attribution of the T3 cost to T2 is untested. The T1-only setting is `kContactModeLive = 2`, but that is not the decoded live value, so I did not ship it.
2. T2 (lift after a landing) is not ported and is the next item on the doc's own account.
3. The pitch/weather index writer (`FUN_1408723d0`) is not decoded. Until it is, every match plays mode 1, and the ponytail note says so.
4. `kContactModeDryNormal` is now used only by tests. Delete it with the tests if the owner agrees.
5. The `contactTicks` rule in the live predictor (only `tick == 1` advances the counter) is unchanged and not re-examined here.
