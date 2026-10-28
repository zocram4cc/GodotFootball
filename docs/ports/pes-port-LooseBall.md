# PES port LooseBall: the loose-ball stall is traced to the MOVESTEP run/idle cycle; the fix is NOT landed; the loose-ball test stays DISABLED

Branch `origin/LooseBall` = `366889312aa86a5ba9a42af5938fbf823d0f9b9a` (commit `36688931`), off `origin/Cutover` `1472df11`. Worktree `<workdrive>/gf-LooseBall`. The only committed change is the test's name and comment (`ALooseDeadBallInPlayIsReachedAndPlayed` stays DISABLED, with the traced cause). No production code differs from Cutover `1472df11`.

Labels: CONFIRMED = code read, per-tick trace, or test run; INF = inference; OPEN = blocker named.

## 0. Status against the assignment

- Stall cause: **located to the tick, not fixed.** The loose-ball test cannot be green without the fix, so it stays disabled.
- Cutover7's "clip selection, not read" is not the mechanism: GF's humanoid does not choose these clips (`[proc]` shows `int=0` on almost every tick). The clip changes are MOVESTEP's own issues, one per state change (section 3).
- GF mode unchanged: switch-off seed 7, 5 min: hash `7ca3d08f68567016`, 31521 ticks (same as Cutover6/7). CONFIRMED.
- Frames: none captured. No visual claim is made.
- Every `pes_*` binary: only `pes_restart_delay_test` was rebuilt and run (23 PASSED, 0 FAILED, 4 DISABLED, rc 0). The other binaries were not re-swept. The production tree equals Cutover `1472df11`, where Cutover7 swept all 96 green (INF for this branch until re-run).

## 1. Baseline (unmodified production code, the enabled test), 5 min each

| seed | longest loose episode (bound 270 = 5.0 s x 54) | `[stalemate]` breaker prints |
|---|---|---|
| 7 | **369 ticks** (CONFIRMED, 2 runs incl. the trace run) | 2 |
| 3 | **640 ticks** (CONFIRMED) | 2 |

After: unchanged, because no production code changed. Idle episodes are not printed by this test (the metric test `DISABLED_LooseEpisodeMetric` prints them); Cutover7 recorded 2 episodes (seed 7) and 1 (seed 3). Not re-measured here (INF).

## 2. Trace: seed 7, the 369-tick episode (frames 27278-27646)

- Presser: `[ftp] presser=-|A8`, id 23 (A-roster ids 16-26 = A1-A11; `[stalemate-who] A4 (19)` agrees). CONFIRMED.
- Command: the press speed is 12.60 km/h (3.5 m/s), `[disp] cmdKmh=12.60`. Ground speed 0.0-1.7 km/h in the steady cycle (frames 27530-27644); 9.6 km/h only at the first clip switch (frame 27310). CONFIRMED.
- Per tick (`[disp]` = MOVESTEP dispatch, `[disp-after]` = next state, `[issue]` = motion issued, `[proc]` = body clip):
  - IDLE (1) runs a 0.1 s dwell (about 6 ticks), then `->` GOTO_RUN (7) with rate 0.300 to 0.168 and class word 0.
  - GOTO_RUN exits to RUN_LOOP (3) with rate 0.168; RUN_LOOP at rate 0.182 (`SpeedToRate` of 12.6 km/h) exits to IDLE (1) on the same tick.
  - One cycle is 8 ticks. Each state issues a motion (IDLE 2222, RUN_LOOP 4154/4155), and the body clip restarts at frame 1 with speed 0.00. CONFIRMED.
  - The body's own switch/requeue does not drive this: `[proc]` shows `int=0` on almost every tick. The only `[select]` is at a clip's last frame (f=27310, `int=1`). CONFIRMED.

## 3. Mechanism (CONFIRMED from code, trace and test runs)

1. **RUN_LOOP's keep test.** `req[0x4A] < 0.3` (`DAT_142590D98`) sends RUN_LOOP to the IDLE requester `FUN_1406D5D60` (port: `src/pes/movestep_run.cpp:562`, in `EnterStop`'s no-class arm; `docs/re/pes-re-runloop-arms.md` section 3, the E3 ladder).
2. **The rate.** `RequestedRate` takes `SpeedToRate(kDesiredSpeedKmh)` (`src/pes/movestep_run.cpp:177-183`, `src/pes/movestep_common.cpp:356-379`). 12.6 km/h gives 0.182 (CONFIRMED by trace). `kUseClassRate` (req[0xb], `cmd+0x5c5`) has no GF writer (`src/sim/onthepitch/pesmovestep.cpp` `FillCommandWords`).
3. **ACCELERATE cannot take the press.** `EnterAccelerate` arm 1 refuses unless command class == 3 (`src/pes/movestep_run.cpp:2533-2536`, PES `FUN_1406ccd00`). A jog press (class 1 after the `-1` mapping of `FUN_1406b8de0`) never reaches state 9.
4. **The command is quantised.** `src/sim/onthepitch/player/controller/elizacontroller.cpp:922` runs `RangeVelocity` (`src/sim/onthepitch/velocitystate.hpp:27-36`: [1.8, 4.2) m/s -> 3.5) on the press speed. The cap 3.75 m/s (13.5 km/h, PES's class cut `FUN_140A75BE0`, `docs/re/pes-re-press-life.md` 9) becomes 12.6 km/h.
5. **The idle/run stop path is the only exit.** `HoldsBodyPose` holds only STOP and ACCELERATE (`src/pes/movestep.hpp:72`). IDLE and RUN_LOOP are not held, so GF's humanoid never blocks the FSM.
6. **Same loop elsewhere.** `docs/re/pes-re-kicker-standby.md:133` records the same IDLE <-> GOTO_RUN loop at under 5 km/h for a restart taker (SLOW_DWELL, `movestep_run.cpp:553-557`). The loop is in the shared MOVESTEP port, not in the press.

## 4. Experiments (seed 7 unless noted; each rebuilt, run, reverted)

| id | change | result |
|---|---|---|
| E0 | none (HEAD = Cutover `1472df11`) | 369 ticks (seed 3: 640) |
| E1 | press speed bypasses `RangeVelocity` (`elizacontroller.cpp:922`, `if (!pressTarget)`) | **477 ticks: worse**, reverted |
| E2 | the presser's request sets `req[0xb]` (use-class-rate) | **376 ticks: no gain**, reverted |

Neither removes the loop. E1 is not the cause; the 0.3 keep test is still hit (INF from the ticks lost, not re-traced).

## 5. Corrections to the Cutover7 report

- "clip selection, not read" (Cutover7 section 4): read now. The clip is the one MOVESTEP issues for the current state (section 2). CONFIRMED.
- "the far-branch ActionPress is the only movement command on the PES path" (Cutover7): the press command does reach MOVESTEP (`[disp] cmdKmh=12.60`), but MOVESTEP ignores its speed for the run decision (section 3). CONFIRMED.

## 6. Test changes (committed `36688931`)

- `tests/sim/pes_restart_delay_test.cpp`: `ALooseDeadBallInPlayIsReachedAndPlayed` re-marked `DISABLED_`, with the traced cause and the rejected experiments in its comment. No other test or production line changed.
- Verification: `pes_restart_delay_test` 23 PASSED, 0 FAILED, 4 DISABLED, rc 0 (`restart_delay_full.log`, 1885 s). GF switch-off seed 7 5 min: `7ca3d08f68567016`, 31521 ticks.

## 7. Open items (numbered)

1. **Press command class and the `cmd+0x5c5` writer.** Decode the MOVE command built by ActionPress (`FUN_140962440`, `docs/re/pes-re-press-life.md` section 8) and its caller. INF: PES's press sends the presser's own pace class (`FUN_140A75BE0`, 1 = walk, `pes-re-press-life.md` section 8 step 6), which would route to the walk/STEP family. OPEN: the Ghidra MCP bridge (127.0.0.1:8080) refused connections this session.
2. **RUN_LOOP / GOTO / ACCELERATE port vs PES for a standing start.** Audit `movestep_run.cpp` RUN_LOOP arms against `docs/re/pes-re-runloop-arms.md` section 6 (the documented divergences: arm 4 TURN_STOP, the style arm's missing RUN_TO_STEP, E3 ladder). Then `EnterAccelerate` arm 1 (`cls != 3`). OPEN.
3. **GF's quantiser on the PES path.** `elizacontroller.cpp:922` quantises every PES-loop movement command, not only the press. It emulates GF's stick input. Whether PES's AI speeds are continuous is INF (strongly suggested by the press cap). OPEN.
4. **Re-enable the loose-ball test** once it passes: seed 7 <= 270 (now 369), seed 3 <= 270 (now 640).
5. **C3 near-branch approach** (Cutover7's far-to-near move, now known to fail through the IDLE cycle): not re-tried.
6. **C4 goal-kick spot, stale `prepared`/`started` flags, keeper-branch carrier** (`elizacontroller.cpp:412`): not traced (budget).
7. **Visual check** of a loose-ball recovery from the match renderer: not done. No visual claim.
8. **Full `pes_*` sweep** on this branch: not re-run (production identical to Cutover `1472df11`, swept green there).

## 8. Commands run (worktree `<workdrive>/gf-LooseBall`, scratch `<workdrive>/gf-LooseBall-scratch`)

- `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release` rc 0; `cmake --build build -j2` rc 0 (4637 targets, 36 min). Rebuilt `pes_restart_delay_test` and `gfsim_run` rc 0 after the revert.
- `FIRSTTOUCH_TRACE=1 tools/dev/simslot.sh timeout 1800 build/tests/sim/pes_restart_delay_test --gtest_also_run_disabled_tests --gtest_filter=PesRestartDelay.DISABLED_ALooseDeadBallInPlayIsReachedAndPlayed` (trace; seed 7, 369 ticks).
- Enabled test: seed 7 (369) and `GF_SEED=3` (640), each through `simslot.sh`.
- E1 and E2 runs: as above.
- `gfsim_run --db-sql assets/fixtures/teams.sql --home 1 --away 2 --seed 7 --minutes 5 --pes21-root <workdrive>/PES21`: `7ca3d08f68567016`, 31521 ticks.
- `pes_restart_delay_test` (full, `simslot.sh timeout 3600`): 23 PASSED, rc 0.

Temporary instrumentation (`FIRSTTOUCH_TRACE`-gated prints in `pesmovestep.cpp` and `humanoid.cpp`) was reverted before the commit. It is not in `36688931`.
