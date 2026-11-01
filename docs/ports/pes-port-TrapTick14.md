# pes-port-TrapTick14: the seed-7 stall root is MOVESTEP's decision taking the keeper's zero-velocity Deflect; fixed, pes_stall_test GREEN; ceremony census, re-stage and fast gate NOT finished (session closed, 01-11-26)

Branch `StoppageFlow`, worktree `<workdrive>/gf-StoppageFlow`, Release dir `build`. CONFIRMED = a run this session; INF = inference; OPEN = not settled.

## 0. Verdict

| Item | State |
|---|---|
| 1. merge `origin/main`, push | DONE: `6f1865dd` (docs-only delta: ProcCamera6, TrapTick13 report), pushed. The symlink `extern/godot-cpp` survived this merge. A stale predecessor gate on this build dir (slots 3/2, 2 h 50 m old) was killed by Main before my first sim could get a slot. |
| 2. the stall | ROOT FOUND, FIXED, red-first test, `1113e3a8` pushed. `pes_stall_test` GREEN (both tests, 655 s). |
| 3. ceremony census / re-stage | NOT DONE. Seed 1 on the fixed tree: 0 ceremonies, 1 walker (census line below). The fix moved the matches again, so `kCeremonySeeds = {2, 1}` is stale; neither ceremony test was re-run. |
| 4. fast gate | NOT RUN. |

## 1. The stall root (CONFIRMED by a per-tick probe, removed)

Probe on seed 7, ticks 24090..24200, keeper A1 (id 16, team 1), ball at rest at (-45.29, 0.41), keeper at (-42.73, -0.73):
- every tick his MOVESTEP state alternates `17 (IDLE_OTHER) <- 1` and `1 <- 17`; the clip is `movement/idle/000_accel.anim_mirror`, frame 1 then 0 of 27, restarted each tick (the TrapTick13 hypothesis "start clip re-issued every tick" is TRUE as a symptom);
- his decision (`Player::GetPesDecision`) is `fn 9` = `e_FunctionType_Deflect`, velocity 0.00, direction (0, 0), no target, every tick;
- his full queue (`RequestCommand`, dumped for ticks 24090..24093) is `[Deflect v0.00, BallControl (fn 2) v8.00 dir (1,0), Movement (fn 1) v7.65 dir (-0.91, 0.41) toward the ball]`.

Cause: `pesmovestep::MovementDecision` returned the first command that is not a ball action (`IsBallActionFunction`). `_KeeperDeflectCommand` (playercontroller.cpp:213) queues a Deflect for any ball in his box he "attempts a save" on, with `useDesiredMovement=false` and velocity 0; it is the first entry, so the decision was "stand", and MOVESTEP held him in IDLE <-> IDLE_OTHER (the idle ping-pong is the PES idle path with a GF clip: `Idle` takes `EnterIdleOther` each tick for a keeper-kind body, movestep_group_a.cpp:290-296; it is only visible because he never leaves IDLE). The walk toward the ball (third entry) never reached MOVESTEP. The `RED ALERT! NO APPLICABLE ANIM FOUND` / `ResetPosition` fallback (humanoid.cpp:441-472) is NOT on this path (INF: not seen in the probe's tick window; not traced for TrapTick12's A2).

A second entry would have steered him wrongly once Deflect was skipped: the BallControl a rushing keeper queues (elizacontroller.cpp:795, `keeperRush`) carries the dribble direction (upfield, (1,0) here) at sprint, not a heading to a ball he has not reached. PES's rule (elizacontroller.cpp:790 comment, "ball control belongs to the man who has the ball") applies.

Fix (`pesmovestep.cpp` `MovementDecision(queue, holdsBall)`, caller `Think` passes `HasPossession()`): the decision skips ball actions, a Deflect, and a BallControl when the man does not hold the ball. A man with the ball keeps the dribble as before. ponytail: GF has no PES keeper mover (KP_PRESS action 0x3e, not decoded); the movement command's ball magnet walks him (named in the comment at the function).

## 2. Tests (all through simslot, `PES_EXE` set)

`pes_movestep_wire_test`, the three touched cases, each run:
- `MoveStepWire.ADesignatedKeeperNextToABallAtRestWalksToIt` (new, behaviour): keeper placed 2.3 m from a ball at rest 7.2 m off his goal line (seed-7 geometry, built from `pitchHalfW` and the team side), must come within 1 m in 4 s. RED with both skips removed (the origin/main decision): "never came within 1 m ... in 4 s". GREEN with the fix (49 s).
- `MoveStepWire.AKeeperWithoutTheBallIsSteeredByTheWalkBehindHisHandsAndTheDribble` (new, unit): queue `[Deflect, BallControl, Movement]`: no ball -> Movement; ball -> BallControl; lone Deflect -> no decision.
- `MoveStepWire.TheDecisionIsTheFirstMovementCommandNotAnAction` (existing, signature updated): passes.

Mutations (each built, run, reverted):
- A: Deflect skip removed -> unit test AND behaviour test fail (killed).
- B: BallControl skip removed -> unit test fails; the behaviour test STILL PASSES (survived: the synthetic placement's BallControl direction did not lead away from the ball; the unit test pins the branch, the observed stall queue is the evidence for it).
- both removed -> behaviour test fails (the red-first run).

`pes_stall_test` (whole binary, seed 7, 2 min): `TheBreakerNeverForcesABallUnderAnArmedRestart` OK, `ALiveBallNeverLiesStillLongEnoughForTheBreaker` OK (live firings 0). The full `cmake --build build -j3` is green.

## 3. Ceremony census (PARTIAL)

Probe (removed): per seed, the first tick a throw-in is prepared with a non-walking taker (the restart-delay test's definition), the first tick a set-piece throw-in starts with `IsPieceTakerHeld` (the release test's definition), and the walker count. On the fixed tree: `seed 1 ticks 30065 preparedCeremony -1 heldAtStart -1 walkers 1`. Seed 1 had one ceremony at tick 12396 on `b83d56c1` (TrapTick13), so the decision fix moved it: **`kCeremonySeeds = {2, 1}` in `pes_throwin_release_test.cpp:40` and `pes_restart_delay_test.cpp:505` is stale, seed 2 is unchecked on this tree.** I stopped my census loop (pid 1274628 and its sim children) when Main closed the session.

## 4. Unfinished (resume here)

1. Serial census, seeds 1..40, through `tools/dev/simslot.sh` on this tree (each 2-minute match took ~10 min under load 20): the probe is simple to rebuild (see section 3; `CENSUS_SEED` env, a `ZzCensus` test in `pes_throwin_release_test.cpp`, never committed). Re-stage `kCeremonySeeds` in BOTH tests with seeds whose `preparedCeremony` and `heldAtStart` are both >= 0 (the two tests define ceremony differently).
2. Run `PesThrowInRelease.ACeremonyThrowInIsReleasedFromTheHandOverTheSpot` and `PesRestartDelay.AThrowInCeremonyTakerIsPlacedOutsideTheLineAndReleasedWithTheBallInHand` to a result (the second searches 20-minute matches per seed, first ceremony stops the run).
3. `GATE_TIER=fast GATE_SLOTS="3 2" tools/dev/gate.sh <build dir>` with the gate env (`PES_EXE`, `GF_DIFF_PYTHON`, `PES21_ROOT`, `GF_IMPURE_BANKS`) until GREEN. Not run.
4. The walker `placedOnTheWay` question from TrapTick13 section 1 (is the `RED ALERT` fallback still reachable for a man in the Trap action?) is untouched.
5. The same decision rule may matter for outfield men: none of them queue a Deflect, and BallControl is queued for them only with possession in the PES loop, so no outfield change is expected (INF, not measured).
