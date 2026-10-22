# PES MOVESTEP: STOP's fallback exit (FUN_1406d1390) and the body link (ClipWords round 2)

> **Main note 22-10-26:** landed on main in batch B (`94b8f66`, gate GREEN 1708/1708; 20-min smokes off `464f398685b80578`, pes-loop `6a0fc698e4fd6fbe`).

Branch `ClipWords`: `504bb15` (merge of fix `72fc34a` with origin/main `32c2cd5`, no rebase).
`ls-remote`: `504bb153b7e00403f5889ca13eb972c5facdc56a refs/heads/ClipWords`.
Status: priority 1 PARTLY done (STOP stall fixed; body link NOT shipped). Priorities 2 and 3 NOT started.

## 1. Decode (VA; verdict)

- CONFIRMED (asm + decompile): FUN_1406c9e20 (STOP). IDLE arm reads req[0xd] (the look bucket) at
  VA 0x1406ca4ea (`MOV EAX,[RDI+0x34]`). Magnitude test: `MOVSS XMM0,[0x1425cc688]` at 0x1406ca506,
  `COMISS XMM0,XMM1` at 0x1406ca50e, `JA 0x1406ca529` at 0x1406ca511; XMM1 = |req[7]| (CVTPD2PS at
  0x1406ca502 after the ANDPS abs mask). Constant at 0x1425cc688 is the port's `deg3375`; its value is
  NOT re-read from the image in this report (Main to check).
- CONFIRMED: the else arm is `CALL 0x1406d1390` at VA 0x1406ca51f (decompile line 279). FUN_1406d1390
  is the IDLE_TURN exit: state word 0x39 = 2, dwell 2 = 0x3e2aaaab (0.1667), row from DAT_1425dda30
  (not decoded; the port's IdleRow stand-in stays).
- CONFIRMED (decompile): the record carries req[0x17..0x2e]. FUN_1406b7560 copies in from
  P+0x2770..P+0x27d4 (byte 0x26 from P+0x27b0); FUN_1406b6a90 copies back (P+0x2770 <- req[0x17] ...).
  FUN_1406b8340 runs init (FUN_1406b68d0), fill (FUN_1406b7560), handler, tail, then FUN_1406b6a90.
- CONFIRMED (decompile): link order per logic tick: Player::Process (player.cpp:461) runs
  ProcessMoveStep, the controller, then Humanoid::Process (player.cpp:494). Humanoid::Process sets
  Switch at a clip's last frame (humanoid.cpp:259-262) and selects a clip from the AI queue (lines
  371-392, SelectAnim). PlayIssuedClip (humanoidbase.cpp:461-517) makes the FSM's clip the body's anim.
- INF (cited, not re-verified here): PES interruption is by dwell timers, not clip end
  (docs/re/pes-re-anim-select.md section 0, item 5).

## 2. Shipped (72fc34a)

- STOP reads req[0xd] and |req[7]| (the port read req[8] and the signed error).
- The else arm calls EnterIdleTurn (FUN_1406d1390). The else arm was empty, so a stop with a turned body
  held forever. EnterIdleTurn is declared in movestep.hpp and defined outside group A's anonymous
  namespace. The group-B probe stub records calls (tests/movestep_probe_stubs.cpp).

## 3. Tests and evidence

- Red then green: `MoveStepWire.AStopOnATurnedBodyLeavesTheStop`. Match: seed 7, 2 min, teams 1 vs 2
  (the trace configuration). 600 warm-up ticks, then six snapshots: every active player is forced into
  STOP and steps 60 ticks on its live command.
  - HEAD: 2 of 22 still in STOP after 60 ticks (FAILED, observed).
  - 72fc34a: 0 of 132 still in STOP (PASSED).
- Mutation, drop the fallback call: still PASSED. NOT caught. The sampled geometry does not reach the
  arm (look error 33.75 to 90 deg, target away, StayDecision false). Gap.
- Mutation of the bucket word and the magnitude: not run. Gap.
- Suites on 504bb15: pes_movestep_test 98/98, pes_movestep_run_test 44/44, pes_movestep_turn_test 43/43,
  pes_movestep_step_test 14/14, sim/pes_mover_test 8/8, sim/pes_movestep_wire_test 15/15.
- Smoke, 2 min, seed 7, teams 1 vs 2, `--pes21-root`:
  - switch-off: exit 0, `[determinism] a3d5e43a3b7b03af` (unchanged from HEAD).
  - `--pes-loop`: exit 0 on both runs, `[determinism] fa17da07e5ec29bb` (rerun identical). Before this
    commit it was `b9af943785c6ed27`; it moved as expected (the MOVESTEP path changed).
  - Tripwire: passes 9-4, shots 1-0 (not zero). The [balance*] lines were not targeted.
- State mix, wire test (seed 1000, 900 ticks), HEAD -> 72fc34a: RUN_LOOP 5038 -> 540; GOTO_RUN 2701 ->
  4286; STOP 23 -> 24; ACCELERATE 11989 -> 6349; IDLE 0 -> 3125; IDLE_TURN 0 -> 319; RUN_TURN_RUN 0 -> 5156.
  This is a real behaviour change. It is NOT checked against PES's live distribution. Review before relying on it.

## 4. Measured and NOT shipped (reverted; full diff saved at scratch `p1_full_working.diff`)

- R2, dwell persistence (req[0x17..0x2e] carried in MoveStepRecord, copy-in and copy-back as above).
  The test's forced STOP inherits the dwell of the state it replaced: 13 to 14 of 132 still held after 60
  ticks. So the test cannot judge R2 as written. ACCELERATE, same wire run (seed 1000, 900 ticks): HEAD 11989;
  72fc34a without R2 6349; 72fc34a with R2 and no hold 9517; 72fc34a with R2 and a STOP-only hold 9358. Each point
  is one run, and none is attributed to a cause. Needs a STOP entry that resets the dwell, as EnterStop does,
  before R2 can ship.
- R3, body hold (no end-of-clip switch and no AI movement clip while the FSM holds).
  - HEAD: test `AnAiMovementClipDoesNotTakeTheBodyWhileTheFsmHoldsAPose` FAILED, 3811 player-ticks
    (first at tick 103), with IDLE, IDLE_TURN, IDLE_OTHER and STOP in the hold set. Observed.
  - With that 4-state hold: PASSED, but holding IDLE froze 95% of player-ticks (IDLE 18924 of 19800)
    and removed trips. Mutation (drop the Switch gate): 34012 violations.
  - STOP-only hold: 14 of 132 forced STOPs held (the R2 artefact above).
  - CONFOUND: every hold run had R2 on. The STOP-only hold WITHOUT R2 has NOT been run. That is the
    first thing to measure next run.
- Correction to the 72fc34a body: its ACCELERATE sentence ("9.5k of 19.8k player-ticks (HEAD 12k of ~20k with
  other states)") compares an R2 run with HEAD using rounded counts and a different denominator. Use the exact
  counts in the ACCELERATE line of this section. Whether R2 causes the drop is not established. The commit is
  pushed and not amended.

## 5. Open (numbered)

1. Body link (R3): run STOP-only hold WITHOUT R2 first (humanoid PesPoseHeld at the Switch and in the
   requeue loop; HoldsBodyPose = kStop; test AnAiMovementClipDoesNotTakeTheBodyWhileTheFsmHoldsAStop, all in
   the saved diff). Then decide.
2. R2 dwell persistence: needs a STOP entry path for the test that resets the dwell (EnterStop semantics),
   then re-measure ACCELERATE and the state mix.
3. IDLE exit: holding IDLE freezes the match. The IDLE exit to GOTO_RUN (StayDecision, FUN_1406b6bb0
   polarity) is not decoded. Required before any IDLE hold.
4. Placeholders in movestep_run.cpp (lines 240-241): `StepLetsRunTurnPlaceholder` and
   `StepStopsPlaceholder` return false and are used by STOP (lines 959, 972). They fake PES answers
   (FUN_1406b70d0, FUN_1406b6ee0; not decoded here). Violates the no-placeholder rule. Open.
5. Test gap: the fallback call and the bucket and magnitude fix are not caught by any test (see 3).
6. Priority 2 (targets), NOT started. Decode notes for the next run:
   - Taker and pad ball-control approach point: `GetBallPrediction(250 + defaultTouchOffset_ms).Get2D()`
     (AIfunctions.cpp:1068-1071). Callers: elizacontroller.cpp:439 (taker), playercontroller.cpp:552.
   - Pad run-to-ball point: `targetPos` at AIfunctions.cpp:1005. Caller: playercontroller.cpp:572.
   - A pure-AI blend (autoBias 1, playercontroller.cpp:556 and 577) can carry it. Blends with pad input
     have no decoded PES target.
   - Trip backup (humanoidbase.cpp:1420-1430) and pad free movement: no PES target decoded.
   - First red test: the set-piece taker's approach command carries a target at the kick-off (taker from
     `GetPieceTaker()`; the ball is at rest, so the target is within 1 m of it).
7. Priority 3, NOT started: BandSpeedKmh (anime info vt+0x60) and Speed byte 0x27. Stand-ins unchanged:
   BandSpeedKmh = current speed in m/s (ClipWords round 1); Speed = physical_velocity.
8. The seed-7 trace's stuck STOP (player 4, tick 1258) does not recur in the same run after the fix. The
   forced-snapshot test samples the geometry instead.
