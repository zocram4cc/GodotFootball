# PES throw-in taker port (ThrowInTaker): WIP, not merged to MergeRA

> **Main audit 19-10-26 (WIP `5aac720`):** `0x14044e3c1` `mov eax,0xaaaaaaab` (/3) and `0x14044e40a` `cmp ebx,4` (rank bound) match. The 0.3 m walk target cites no PES site (flagged). 2 of 10 restart tests red; not on MergeRA.

Status: pushed to `refs/heads/ThrowInTaker` at `5aac720` (ls-remote confirmed). `MergeRA` is still
`ceb7991` and was NOT updated: acceptance is not met (2 of 10 `PesRestartDelay` tests red).
Worktree `<workdrive>/gf-ThrowInTaker`, scratch `<workdrive>/gf-ThrowInTaker-scratch`.

## Decoded (CONFIRMED: bridge decompile + raw disassembly)

- `FUN_14044e230` (seamless taker, EV-TI-0006): rank 0 unless `team+0x4c00[slot] == 0` (keeper),
  then rank 1 with no comparison. Otherwise a running best over ranks 1..3: a rank replaces the best
  when `arrival + fps/3 < best`. `/3` is `MOV EAX,0xaaaaaaab` @`0x14044e3c1` (multiply-shift);
  loop bound `CMP EBX,0x4` @`0x14044e40a`.
- Release: A5Judge §9.2-9.4 (taker reach and kick action); throw-in §6.4 (path A's armed steps are
  appended after the taker's pickup).
- Not used: the 0.7 m throw spot (§3.2, DAT_14257a194); the final walk stops 0.3 m off the ball.

## Port (PES path only, `match->PesLoop()`; GF path unchanged)

- Pick: `pes::SeamlessThrowInTaker` replaces the `technical_setpiece` pick for PES throw-ins
  (`teamAIcontroller.cpp`, PrepareSetPiece). Arrival order: GF `GetTimeNeededToGetToBall_ms`.
- Walk: no `ResetPosition` for the PES throw-in taker. He walks to a point 0.3 m off the ball on his
  own side, closing at `2/s` times the gap (sprint cap). `elizacontroller.cpp` stand-still branch.
- Release: `MayRelease(ThrowIn, …, takerArrived)` returns the arrival within 1 m of the walk target.
  `kThrowInReleaseFrames = 118` is deleted.

## Tests

- `SeamlessThrowIn.*` (3, `pes_setpiece_team_test`): red (returned -1) then green. Mutation M1 (bias
  dropped): `ALaterRankMustBeatTheBiasToTakeIt` FAILED; reverted, 21/21 green.
- `PesRestartSequencer.AThrowInIsReleasedOnItsTakersArrivalAndNotBefore`: red (timer released it) then
  green. Mutation M2 (118-frame timer restored): FAILED; reverted, 11/11 green.
- `PesRestartDelay.AThrowInTakerWalksToHisSpotAndIsNotPlacedOnIt`: red (`awayAtPlacement 0 vs 0`, the
  taker was teleported to the ball) then green. Mutation M3 (teleport restored): NOT RUN (budget).
- Full `pes_restart_delay_test`, last complete run (green3 source; the final source differs only by a
  reverted referee line, restored to the green3 text): 8 of 10 green. Red:
  `EveryStoppageRestartsWithinTheInstallBoundAtTwentyMinutes`, `AThrowInTakerPlaysABallActionWhenTheBallLiesOverTheLine`.
- Other binaries (run before the final walk change, so indicative): `pes_restart_taking_test` 3/3,
  `pes_setpiece_taker_test` 8/8.

## Smokes (final source, `--seed 7 --minutes 2`, rc 0)

- Switch-off: `[determinism] 20f28a3df9b0ce12` (unchanged from the baseline).
- `--pes-loop`: `[determinism] 07ac044b0b3aa3f6` (ceb7991 records `b9c01f554816959c`; moved by the walk).
  Same-hash rerun not done.

## Open (numbered)

1. 20-min seed 7 stalls: a throw-in taker (H2, piece 5 at x=-34.7, z=-34.3) never reached his walk
   target within 1 m. The restart ran 1758 ticks against the bound of 1757. Base passes this test.
   Cause not isolated: a walk blocked by the crowd at the ball, or the refused-walk mode described at
   `pes_restart_delay_test.cpp:110-113`. Next: log the taker's anim, velocity, position and command
   at the stall.
2. `AThrowInTakerPlaysABallActionWhenTheBallLiesOverTheLine` (line 332): no throw-in taker is in a
   PES locomotion clip at release. Releasing at arrival, before the armed step, did not fix it, and it
   broke `PesLoopRestartsAreArmed...` (line 148). Next: find which anim family MOVESTEP reports at the
   walk speeds (base passed only through the stand-still wander, `elizacontroller.cpp:234`).
3. Walk mutation (M3, teleport restored) not run.
4. PES path B (ceremony taker, `FUN_1404a93a0`, §2.1) and the EV-TI-0007 path test are not ported:
   every PES throw-in takes the seamless rule.
5. Arrival order is GF's time-to-ball, not the AI list at `AIctx+0x2470c` (ponytail).
6. Walk speed is the sprint cap; the PES MOVE speed class (`cmd[0x17]`) is not decoded. Arrival radius
   1 m stands in for MOVESTEP's stop (`FUN_1406b6bb0`).
7. The taker stands 0.3 m off the ball (GF's teleport distance). PES carries the ball out to F1 + 0.7
   first (§3.2); the carry is not ported.
8. Goal kick shares the release rule (`pessetpiecetaker.hpp:21`, A5 §4.7 "taker reach + kick") but the
   taker is the keeper outright, and his stand is a teleport (`teamAIcontroller.cpp` ~1589-1593). Not
   ported. Corners share neither the taker nor the release: timer-based (3.5 x fps), measured 365-379.
9. Lint: `teamAIcontroller.cpp` still includes `pessequencer.hpp` (unused after the stand-spot removal).
10. Main's request, 20-min pes-loop seeds 11 and 13 on MergeRA: not run (budget). Seed 7 fails here
    (item 1).
11. Gate not run (Main gates).
