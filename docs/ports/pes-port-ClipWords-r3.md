# PES MOVESTEP round 3 (ClipWords): placeholders, mutation gap, persistence, STOP hold

> **Main note 22-10-26:** landed on main in batch B (`94b8f66`, gate GREEN 1708/1708; 20-min smokes off `464f398685b80578`, pes-loop `6a0fc698e4fd6fbe`).

Branch `ClipWords`, pushed (ls-remote confirmed at `1b19695`):
- `3a5872a` PES MOVESTEP: StepStops and StepLetsRunTurn ported, placeholders removed.
- `b111759` PES MOVESTEP: STOP's idle-turn exit covered by three tests.
- `1b19695` PES MOVESTEP: STOP holds the body pose; BandSpeedKmh and Speed byte comments corrected.
- `7607d62` merge of origin/main (no rebase). `git fetch` and `git merge` recurse into the `extern/godot-cpp`
  symlink and fail, so both ran with `-c submodule.recurse=false`.

Tests (build passed on each): movestep 98, run 47, turn 43, step 18, wire 16 (all green on `1b19695`).
`pes_mover_test` was not rebuilt this round.

## 1. Placeholders (priority 1.1): done

- FUN_1406b6ee0 -> `StepStops` (movestep_step.cpp). Class and command-class stop, live bearing and second-dwell
  hold (live only), bucket rules with the look gates blend2, blend04, rate07 (11.25 and 90 degrees).
- FUN_1406b70d0 -> `StepLetsRunTurn(host, req)`. Flag gate (req[0xf] byte, req[0xe]). Front, back and centre
  rows (0x1425dde70, 0x1425dde80, 0x1425dde90) are loaded into `StepTables` by `LoadStepTables`. Each scale is 0
  up to its first angle, rises, and is 1.0 past its last: PES's initial fVar16 is 1.0. A first draft used 0 there;
  the decompile (line 57) corrected it.
- Deleted: `StepLetsRunTurnPlaceholder` and `StepStopsPlaceholder`; the group-B `EnterTurnStop` seam;
  group D's stub `StepLetsRunTurn(host, req, k)` (it returned false, and was the third placeholder for the same
  PES function); the old group-C bodies, `StepStopsNow`, `Distances()`, `g_distances`, `TurnDistances`.
- Target words: `FillMoveTarget` never stored the move target. PES stores it at req[0x1a6] and req[0x1a8]
  (FUN_1406b7560: `param_2[0x1a6] = local_110`). Now stored as `kMoveTargetX` and `kMoveTargetZ`.
- Frame (ponytail): the live bearing and `DisplacementHeading` take the ground pair as (out[0], 0 - out[1]).
  The adapter copies GF's raw coords, and PES's z is -GF y (the fill's target uses the same sign).
- Red then green: `MoveStepStops` and `MoveStepLetsRunTurn` failed on the group-C bodies before the port (observed).
  One mutation each, restored: the command-class stop (new test `AMissingCommandClassStopsAStepThatHasAClass`) and the
  bucket-one saturation (`BucketOneGateTakesTheFullScalePastSixtySevenPoint5`). Each failed its own test.
- Wire state counts did not move. Seed 1000, 900 ticks, 19800 player-ticks: IDLE 3125, IDLE_TURN 319, RUN_LOOP 540,
  GOTO_RUN 4286, STOP 24, ACCELERATE 6349, RUN_TURN_RUN 5156, TURN_STOP 1. These equal 72fc34a exactly.
  Frozen-tick share (IDLE plus IDLE_TURN): 3444 of 19800 = 17.4%.

## 2. Mutation gap (priority 1.3): done (`b111759`)

The else arm of STOP (FUN_1406c9e20, VA 0x1406ca51f: `EnterIdleTurn`) had no test that reached it. Three tests now
run `Stop` with the dwell past both limits and a turned body:
- `TakesTheIdleTurnExitWhenTheLookBucketIsOffStraightAheadAndTheErrorIsLarge`: look bucket 2, error +40 takes the exit.
- `ComparesTheFacingErrorByMagnitudeForTheIdleTurnExit`: error -40 takes it; +20 does not.
- `ReadsTheLookBucketNotTheMoveBucketForTheIdleTurnExit`: look 2 with move 0 takes it; look 0 with move 2 does not.

Mutations, each restored: removing the call fails all three; the move bucket in place of the look bucket fails the
bucket-source test only; a signed error in place of the magnitude fails the magnitude test only.

## 3. Persistence (priority 1.2): implemented, measured, NOT shipped

- Implementation, reverted and saved at `<workdrive>/gf-ClipWords-scratch/r3_r2_persistence.diff`: `MoveStepRecord::carried`
  holds req[0x17..0x2e]; `CarryIn` runs before the dispatch and `CarryOut` after the tail (FUN_1406b7560 copy-in,
  FUN_1406b6a90 copy-back). The adapter built a fresh `MoveReq` each tick, so the dwell clocks reset every tick. That
  is the defect. The unit test `CarriesTheDwellWordsOutAndBackInButNoOtherWord` was red (compile) and green.
- Measured with it (seed 1000, 900 ticks): IDLE 5864, IDLE_TURN 335, RUN_LOOP 1962, TURN_STOP 851, GOTO_RUN 186,
  STOP 1006, ACCELERATE 9594, DECELERATE 2. Frozen-tick share (IDLE plus IDLE_TURN): 6199 of 19800 = 31.3%,
  against 17.4% without it.
- `MoveStepWire.AStopOnATurnedBodyLeavesTheStop` FAILED with it (1 of 16). The test sets the forced STOP's state
  directly, so the forced STOP inherits the dwell clocks of the state it replaced. PES enters STOP through `EnterStop`,
  which resets the dwell. The test must enter STOP the same way before this can ship.
- Not shipped: the state mix moves by a factor of two in IDLE, and a test is red. Next: the forced-STOP entry, then
  re-measure.

## 4. Body hold (priority 1.2): STOP only, shipped (`1b19695`)

- `HoldsBodyPose(kStop)` in movestep.hpp. humanoid.cpp: the end-of-clip Switch is gated, and the requeue loop skips
  Movement commands while the pose is held.
- Test `MoveStepWire.AnAiMovementClipDoesNotTakeTheBodyWhileTheFsmHoldsAStop`: red before the hold (3 player-ticks of
  AI movement clips over a held STOP, first at tick 1254); green after. The forced-STOP exit test still passes.
- Measured: the FSM state counts equal 72fc34a's. The hold changes which clip the body plays, not the FSM state.
  STOP is 24 of 19800 player-ticks (0.12%), so the held share is negligible.
- IDLE hold: NOT measured this round. The IDLE exit to GOTO_RUN is not decoded (round-2 open item 3), so it stays
  blocked. The round-2 measurement (IDLE hold froze 95%, IDLE 18924 of 19800) was made with persistence on, so it is
  confounded.

## 5. Corrections from Main's audit (this round)

- The `BandSpeedKmh` stand-in and its declaration no longer cite `vt+0x60`. That is clip duration (FUN_1406E7EB0,
  docs/re/pes-re-speed-inputs.md section 4), not a band speed. The stand-in returns the player's movement length for
  every band and ignores the physique argument. The identifier stays, as the PES name, until the port replaces it.
- The Speed byte note now cites Speed = clamp(trunc(base[0x27] * A1..A5), 40, 99) via FUN_14092AF10. Its base row has
  no GF source, so physical_velocity stands in for it.
- Round-2 report ACCELERATE wording corrected (`pes-port-ClipWords-r2.md`): exact counts only. HEAD 11989;
  72fc34a without R2 6349; with R2 and no hold 9517; with R2 and a STOP-only hold 9358 (R2 on). The 72fc34a body's
  ACCELERATE sentence compared an R2 run with HEAD using rounded counts; the commit is pushed and not amended.

## 6. Open

1. Priority 2 (targets for taker, pad, trip, basic): NOT started. Decode notes are in the round-2 report, item 6
   (taker approach point at AIfunctions.cpp:1068-1071; pad run-to-ball `targetPos` at AIfunctions.cpp:1005).
2. Band speed, queued by Main (not started). Entry points from Main's audit: the band reader is anime slot +0x38 =
   FUN_1406B9E50, called from FUN_1406D2030 and FUN_1406C1070 as (anime, req[0x1F]) when req[0x1C] != 0. It returns the rate
   fraction [0.15, 0.3, 0.6, 1.0] from 0x1425DDE10 for bands 0-3, and 0 for band > 3 or band 0 with kind 3/6. Speed to
   rate is FUN_1406B9E80 over the 11-knot table at 0x1425DE010, with speed/(3.6*G), G = FUN_141AF3180(P+0xA08). Delete
   `BandSpeedKmh` and `CurrentSpeedKmh` as a cutover. Constants from the install. Check first: movestep_common.cpp
   loads the 0x1425DE010 table as "ten records", and the audit says 11 knots.
3. Persistence: forced-STOP entry (dwell reset, as EnterStop) before R2 can ship; then re-measure.
4. IDLE hold: blocked on the undecoded IDLE exit to GOTO_RUN.
5. Cross-group frame defect (reported, not changed): PositionAt and BallSample copy GF's raw coords. Group A, the
   DisplacementHeading and group D readers use index 2 as ground z; movestep_style.cpp (lines 42-43, 55-56),
   movestep_ramps.cpp (115) and movestep_run.cpp (1208) use index 1. The fix belongs in the adapter (copy PES order:
   out[1] = height, out[2] = -coords[1]), with those readers moved to index 2. It changes every group's measured
   state, so it needs Main's decision.

## 7. Final checks on `1b19695`

- Builds passed: gfsim_run, pes_movestep_wire_test, pes_mover_test, pes_movestep_test, run, turn, step.
- Tests: pes_mover_test 8/8; pes_movestep_test 98/98; run 47/47; turn 43/43; step 18/18; wire 16/16.
- Smokes (seed 7, 2 min, teams 1 vs 2, `--pes21-root`):
  - switch-off: exit 0, `[determinism] a3d5e43a3b7b03af` (unchanged from HEAD; the hold fires only while a record is in STOP).
  - `--pes-loop`: exit 0, `[determinism] 2ad841345a899df8`. On `3a5872a` it was `b73487bf7162a422` (rerun identical). Before round 3 it was `fa17da07e5ec29bb`.
- Not run this round: the tripwire lines of the smoke; the `[balance*]` lines.
