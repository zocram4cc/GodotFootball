# PES throw-in taker port (ThrowInTaker, branch `ThrowInPathB`)

**Status: not green, not pushed.** Committed locally on `ThrowInPathB`, which sits on `GoalKick`. `GoalKick` carries the goal-kick port, ClipWords' tip, and the RunLoopTail merge `be31198`. The acceptance is not met on this tree; the blockers are below.

## Decoded (docs/re/pes-re-throwin-choreography.md)

- Seamless candidate (2.2, FUN_14044e230): rank 0 unless he is a keeper (then rank 1); ranks 0 to 3; a rank replaces the best when `arrival + fps/3 < best`. Eligible when the reach is at most 20 m and the arrival is under 4 s (216 frames at 54 fps; EV-TI-0007).
- Ball gate (3.1, FUN_140444f00): ball at most 12 km/h, at most 1.5 m high, and `|restart.x - ball.x|` at most 20 m.
- Ceremony taker (2.1, FUN_1404a93a0): pass one takes roles 2 to 6 and the unknown; the flank filter applies to roles 2 and 5; DMF and AMF count +10 m, SB -20 m; the search cap is 500 m (DAT_14259be74); the fallback is the nearest man on the pitch.
- Pickup reach: the install's DAT_14259222c is 5.0 m, read by `LoadSetPieceTakerConstants`.
- Path B placement (F0+162): the taker's body stands 1.02 m outside the touchline over the restart point, facing the pitch, with the ball.

## Port (PES path only; the GF path is unchanged)

- `teamAIcontroller.cpp`, PrepareSetPiece: ball gate, then a seamless candidate (path A: walks to the ball, picks it up within the 5 m reach, carries it to the carry spot, released on arrival) or a ceremony taker (path B: placed at the stand point with the ball, no walk). The closest-man fallback is off on the PES throw-in path.
- `player/controller/elizacontroller.cpp`, PES path, a set-piece taker without the ball: a throw-in carrier within the pickup reach retains the ball; a free-kick, corner or penalty taker runs to the live ball, replacing the `AI_GetBallControlMovement` hover. The walk and run commands set `useDesiredTarget`. The goal kick (ladder) and kick-off are unchanged.
- `humanoid.cpp`: the carrier's out-of-bounds exemption. `pesstoppage.cpp`: the carry spot constants.
- Tests: the four path-A throw-in tests count walking takers only; the new path-B test `AThrowInCeremonyTakerIsPlacedOutsideTheLineAndReleasedWithTheBallInHand`; `ThrowInPickupReachIsReadFromTheExe`.
- Mutation: the full-back bonus sign flipped fails `CeremonyThrowInTaker.TheSameFlankFullBackTakesItUnlessAMidfielderIsTwentyMetresNearer`; restored, 5 of 5 green.

## Results (this tree: GoalKick `fe5065d` plus the port, walk pace as the approach stand-in)

Pure suites: `pes_setpiece_team` 32/32, `pes_restart_sequencer` 15/15, `pes_goal_kick` 19/19, `pes_setpiece_taker` 9/9, `pes_movestep_wire` 23/23.

Restart group, `pes_restart_delay_test`, seed 7 (`LONG` 5 minutes, `TWENTY` 20 minutes):

- Green: `EveryStoppageRestartsWithinTheInstallBound` (2 min), `AMoverWhoseClipEndsGoesOnAndMoves`, `ASeedThreePesIssuePlaysAtAPositiveRate`, `AFinishedTrapDoesNotFreezeAMover`.
- Red, stall (1758 ticks, no restart): `EveryStoppageRestartsWithinTheInstallBoundWithPesLoop`, `PesLoopRestartsAreArmedAtTheirPesSpotsSidesAndReleaseFrames`, `AWhistleLeavesNoFlyingBallBehind`, `AGoalKickIsReleasedOnThePesLadder`, `TheSeedTwentyThreeKickOffRestarts`, `ARestartTakerPlaysHisBallActionAfterRelease`, `AThrowInIsReleasedFromItsSpotAndTaken`, `AThrowInIsCarriedToItsSpotAndReleasedThere`, `AThrowInTakerPlaysABallActionWhenTheBallLiesOverTheLine`, `AThrowInTakerWalksToHisSpotAndIsNotPlacedOnIt`, `AThrowInCeremonyTakerIsPlacedOutsideTheLineAndReleasedWithTheBallInHand`, `EveryStoppageRestartsWithinTheInstallBoundAtTwentyMinutes` (stall at 208 s).
- Red, no throw-in in the window: `AThrowInIsNotAwardedAgainOnItsOwnSpot` (2 minutes of seed 7 have no throw-in).

Before the RunLoopTail merge (`4e168cc` plus the same port): `EveryStoppageRestartsWithinTheInstallBoundWithPesLoop` passed, `EveryStoppageRestartsWithinTheInstallBoundAtTwentyMinutes` passed (700 s), and `ARestartTakerPlaysHisBallActionAfterRelease` passed. The five-minute throw-in tests then stalled or found no throw-in.

## Blockers

1. **RunLoopTail (`be31198`) creeps set-piece bodies.** A stopped body commanded toward a target behind it enters RUN_TURN_RUN. The entry clip is refused: the table column is the raw look bucket, which names empty cells on odd buckets (ClipWords' port; the fix is stashed, pending Main's scope ruling). The body then creeps at about 0.03 deg per tick, and RUN_TURN_RUN never exits, so every walker and carrier stalls the restart. Not caused by this port; the merged tree is red because of it.
2. **Seed 7 has no seamless throw-in** in its five-minute or twenty-minute runs. All three throw-ins in 20 minutes are path B: the four nearest teammates are 36 to 50 m from the spot, outside the 20 m reach. Seed 11 has a seamless throw-in at tick 2307 (taker 15.8 m away, arrival 110 frames). The four path-A tests cannot see one on seed 7. A scenario decision is needed: the path-A tests on seed 11, and the ceremony test on the twenty-minute run.
3. The seamless walk has not completed a pickup on any tree yet (blocked by 1).

## Ponytails (stand-ins, PES site named)

- Walk pace for the approach (`walkVelocity`, the approach gain capped by it): PES's run speed (MOVE class, cmd[0x17]) is not decoded. The sprint command enters the RUN_TURN_RUN creep (blocker 1). Revert to the sprint ceiling once the entry is ported.
- Arrival order is GF's time-to-ball; AIctx+0x2470c is not decoded. The flank is the man's current side.
- The placed body centre stands at 1.02 m for the release point (F0+162).
- Free-kick, corner and penalty approach: the run to the live ball stands in for PES's approach, which is not decoded.

## Next

- ClipWords: the RUN_TURN_RUN entry port (their scope ruling from Main).
- Main: whether ThrowInPathB lands with or without `be31198` (red for the restart group with it); the scenario for the path-A and ceremony tests.
- Then: restore the sprint approach if the entry is fixed, and re-run the restart group.
