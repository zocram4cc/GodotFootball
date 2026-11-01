# pes-port-TrapTick13: StoppageFlow throw-in reds were seed drift plus an AV-merge effect, not a trap-cutover regression; the stall test is still RED on a new shape (keeper idling 2.3 m from a still ball); fast gate NOT run (01-11-26)

Branch `StoppageFlow`, worktree `<workdrive>/gf-StoppageFlow`, Release dir `build`. CONFIRMED = a run this session; INF = inference; OPEN = not settled.

## 0. Verdict

| Item | State |
|---|---|
| 1. merge `origin/main`, push | DONE twice: `7197c527` (docs-only delta of TrapTick12/ProcCamera6/CutsceneGate), then batch AV (`cd9d4a18`) as `b83d56c1`; both pushed (`git ls-remote` confirmed). The merge replaced `extern/godot-cpp` (symlink) with an empty directory: restore it with `rmdir` + `ln -s` before cmake. |
| 2. throw-in reds | `AThrowInTakerWalks...`: **PASSED** (127 s) on `b83d56c1`. `ACeremonyThrowInIsReleasedFromTheHandOverTheSpot`: **PASSED** (766 s) with `kCeremonySeeds = {2, 1}`. `AThrowInCeremonyTaker...`: run with the same list, see section 2 for its state. |
| 3. stall | **NOT FIXED.** The merged tree's seed-7 stall is a different shape from TrapTick12's (section 3). |
| 4. fast gate | **NOT RUN** (budget; the machine was at load 20-90 and out of memory for part of the session). |

## 1. Walked taker (CONFIRMED)

The gate's failure on `3e4fa2e6` was `placedOnTheWay == 1` (restart_delay_test.cpp:489): the walker's humanoid placement count (`HumanoidBase::ResetPosition`) changed between his prepare and his release. The gate log of that run carries 342 `Humanoid::Process: RED ALERT! NO APPLICABLE ANIM FOUND` lines; that fallback (humanoid.cpp:441-472) calls `ResetPosition(spatialState.position, ball)` when `RequestCommand` queues nothing at a clip switch, so it bumps the placement count without moving the man. `_TrapCommand` (playercontroller.cpp) now returns true with an empty queue for a man already in the Trap action (the trap tick drives him), which is one way to reach it (INF: not traced to the walker himself). The pes-loop smoke logs show the fallback more often on this branch than on main's tree (15 against 4 `RED ALERT` lines in the two gates' `gate.smoke-pesloop.log`).
After the AV merge the walker is not placed any more: a probe (removed) over seed 3's twenty-minute match, counting the taker's placement from before prepare to release, saw three walker throw-ins (ticks 3069, 18831, 21552) with `placedDelta 0` each; seed 21's two minutes: three walkers, `placedDelta 0`. The test itself then PASSED (127 s). Whether the AV merge removed the cause or only moved the match away from it is OPEN (no ablation run).

## 2. Ceremony throw-ins (CONFIRMED, census partial)

On this tree main's list `{5, 7, 9}` is stale, as `{2, 15, 16}` was on main after StyleCutover2. A 1..40 census was started (2-minute pes-loop matches, probe in `pes_throwin_release_test.cpp`, removed) but the machine ran at load 25-94 and memory ran out (a first launch of four processes was killed by the OOM or died mid-run), so only seeds 1 and 21 completed and seed 2 reported its first ceremony: **seed 1: 1 ceremony (held), 0 walkers (first ceremony prepared at tick 12396); seed 2: ceremony prepared at tick 1136; seed 21: 3 walkers, 0 ceremony.** Seeds 3..20 and 22..40 are NOT censused: `{2, 1}` is the verified list, not the complete one.
Re-staged (assertions untouched): `kCeremonySeeds = {2, 1}` in `pes_throwin_release_test.cpp` and `pes_restart_delay_test.cpp`. It is not a regression: ceremony throw-ins exist on this branch. Tests: `PesThrowInRelease.ACeremonyThrowInIsReleasedFromTheHandOverTheSpot` PASSED (766 s under load). `PesRestartDelay.AThrowInCeremonyTakerIsPlacedOutsideTheLineAndReleasedWithTheBallInHand` was still running when this was written (3,500+ s under load; seed 2 first). No result is claimed for it.

## 3. The stall (CONFIRMED by a per-tick probe, removed)

`pes_stall_test` (merged tree): `ALiveBallNeverLiesStillLongEnoughForTheBreaker` FAILED with 192 live firings (`TheBreakerNeverForcesABallUnderAnArmedRestart` OK). The TrapTick12 shape (a ball at rest at (-46, 31), nearest man 17-20 m) does not occur any more; the seed-7 match now stalls elsewhere:
- ball at rest 0.08 m/s at (-45.29, 0.41) from t=24100, **inside the goal area**, play live, no restart armed; the forced man is the keeper A1 (id 16, team 1), **2.3 m from the ball**, designated possession man the whole time, `GetTimeNeededToGetToBall_ms` 590-680;
- the keeper's MOVESTEP state is 1 (IDLE) <- 17 (IDLE_OTHER), speed 0.00-0.36 m/s, his clip is `movement/idle/000_accel.anim_mirror` at **frame 0 of 27** at the report (the start clip is restarted instead of running on); A1's nearest team-mate is 20 m away and idle too; no loose presser on either side (keepers are excluded from the picker, `PesLooseWriterTick`);
- the breaker fires at 5 s and again at 8 s and the ball only then moves.
So the defect is not "nobody is sent": the one man who is sent (the keeper, whose rush race is won by a wide margin: his 0.6 s against the opponents' 8 s) is commanded and does not start walking. The same signature was in TrapTick12's A2 (state 4/5 alternating, idle then walk then idle at 0.5 m/s). INF: the common cause is a command that restarts the start-from-standstill clip every tick (GF's idle `000_accel` selection or MOVESTEP's walk start), not the chaser selection; it was not traced (no command dump, no ablation). The first thing to read is the keeper branch of `ElizaController::RequestCommand` (elizacontroller.cpp:704-776: `keeperChases` then `GetOnTheBallCommands`, whose movement for a ball 2 m away on the PES path is queued only through `_MovementCommand`) and `Humanoid::SelectAnim` for a movement command at standstill with a low desired speed.
Reproduction: `pes_stall_test` seed 7, 2 minutes, `PES_EXE` set; first `[stalemate]` report at 5.0 s idle, ~tick 24400.

## 4. Environment notes
- Running tests directly needs `PES21_ROOT` and `PES_EXE` (the unpacked image); without `PES_EXE` the match throws `PES trap data missing` and the census silently counted zero throw-ins.
- The predecessor's gate (`gate.sh` on this build dir, slots 3/2) was still alive when this run began and was re-running these tests on the rebuilt binaries; its output is meaningless for this tree.
- Memory: each match process holds 2.5-3 GB; four at once with other agents' Godot tests (12 GB) ran the machine out of memory.

## 5. Unfinished
1. `pes_stall_test` live case (section 3): trace A1's command queue and the clip selection on t=24100..24400 of seed 7, then fix and add the red-first test.
2. `AThrowInCeremonyTaker...` result on `{2, 1}`; the rest of the 1..40 census.
3. Mutations: none run (no engine change was made).
4. `GATE_TIER=fast GATE_SLOTS="3 2" tools/dev/gate.sh <absolute build dir>` on StoppageFlow: not run; it can be GREEN only after item 1.
