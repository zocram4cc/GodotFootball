# pes-port-DeadBall: requeue gate treats a MOVESTEP locomotion clip as Movement; restart-taker test green; 20-min pes_loop still stalls at a goal kick

Status: NOT DONE against acceptance. `origin/DeadBall` at `32212a9` (test `00e01c2` before it). Restart-taker test green. Loose-ball test red (4370 ticks, bound 270). 2-min pes_loop reaches full time. 20-min pes_loop stalls at a keeper goal kick (1758 ticks). Off-path 2-min hash matches. Not for merge.

## Root cause for the taker (CONFIRMED)

- MOVESTEP clips are issued by `HumanoidBase::PlayIssuedClip` (`humanoidbase.cpp`) as Special clips of the PES locomotion family (`fn 14`). They exist only under `pes_loop`.
- GF's requeue gates tested `currentAnim->functionType == e_FunctionType_Movement`: the requeue predicate (`humanoid.cpp` ~322-341) and `Humanoid::SelectAnim`'s ReQueue block (~1972-2058). A Movement or ball-action command was never selected over a PES clip.

## Change (`00e01c2` test, `32212a9` fix)

- `humanoidbase.hpp/cpp`: `OnPesLocomotion()` (the Special locomotion clip; `PlayIssuedClip` reuses it) and `IsMovementClip()` (a GF Movement clip, or that). The `IsMovementClip()` comment names PES's action setter `FUN_1406eebb0` (`AnimePlayer::ChangeAction`, `pes-re-fox-registry.md`, line 79), which switches the action out of MOVE only for a ball action.
- `humanoid.cpp`: `IsMovementClip()` replaces Movement at 325 and 327 (predicate) and 1976, 1979, 1984, 1989, 2024, 2047 (ReQueue gates). The freshness gate at 1960-1968 is unchanged.
- Off `pes_loop` no PES clip exists, so the off path is unchanged (checked).
- `LooseBallChase` and `SetClipCommand` removed (see loose-ball measurements).

## Tests

- `PesRestartDelay.ARestartTakerPlaysHisBallActionAfterRelease` (`tests/sim/pes_restart_delay_test.cpp`). Every restart's taker must show a pass, lofted kick, shot or throw animation within `RestartLimitTicks` (the install bound `RestartWatch` enforces) of release. Failures name the untaken restarts.
  - Red without the gate fix: 2 takers untaken, 270-tick bound (CONFIRMED, `taker_red.log`).
  - Green with the fix and the restart-limit bound (CONFIRMED, `bound_taker.log`, `final_taker.log`).
  - Mutation A1, `IsMovementClip()` without the locomotion term: red (CONFIRMED, `mutA1_taker.log`).
  - Mutation A2, requeue predicate on plain Movement only: red (CONFIRMED, `mutA2_taker.log`).
  - Both restored; `git status` clean at `32212a9`.
  - Note: the 270-tick version went red on a keeper goal kick (piece 2, taker A1), which the restart-limit bound does not.
- `PesRestartDelay.ALooseDeadBallInPlayIsReachedAndPlayed` (from `aaa2eb1`, unchanged). Longest loose episode, bound 270 ticks (GF's stalemate clock, a ponytail bound):
  - No stand-in, no gate: 5847.
  - Stand-in writing the step to the clip command, with possession guard: 3496.
  - Stand-in without possession guard: 6438.
  - Gate fix, no stand-in (final): 4370.
  - Gate fix plus the same gate in the contact trigger: 3496 (identical, dropped).
  - Red in every configuration, so a mutation is not meaningful yet.

## Smokes

- 2-min `--pes-loop`, seed 7, final code: full time, 44023 ticks, `[determinism] b462bee36a3ae01b`, exit 0 (CONFIRMED, `final_smoke2.log`).
- 20-min `--pes-loop`, seed 7, final code: STALLS. `[restart] stalled: no restart within 1758 ticks (limit 1757)` at about 4 min, exit 1 (CONFIRMED, `final_smoke20.log`). Context: `piece 2, taker H1` (the home keeper), `started 1`, `buf active 0`, ball at 47.8, 2.6 (the goal-kick spot), forced keeper 9-11 m from the ball.
- Off path, 2-min, `PES21_ROOT` unset, `--pes21-root` set, no `--pes-loop`: `[determinism] a3d5e43a3b7b03af`, matches the acceptance (CONFIRMED, `final_off2.log`).
- Gate: not run (Main).

## Keeper goal kick (CONFIRMED trajectory, INF cause)

- Traced keeper H1 (`gk_trace20.log`, t=62262-62478): 2.30 m to 4.08 m from the spot after release, on `fn1` Movement commands at 7.8 to 10 m/s pointing away (toward his goal). The goal-kick HighPass (set-piece branch, `elizacontroller.cpp` ~225-300, entered on `IsInSetPiece && GetPieceTaker()==player`) is never his clip.
- INF: the HighPass is refused by `SelectAnim`'s ball-distance optimisation (`humanoid.cpp` ~1936-1956) while he is 2+ m off, and no walk-to-ball command reaches him. Not verified.
- Same behaviour on A5Tail's tree (their message), so it is not the picker.

## Open items

1. 20-min stall at the keeper goal kick (above). Decide whether the taker's approach (set-piece walk-to-ball for the keeper) or the SelectAnim distance gate for a set-piece HighPass owns the fix. Blocks the 20-min acceptance.
2. Loose ball. The designated man reaches the ball (for example A823 at 0.25 m, ball 1.06 to 0.08 m/s after a nudge), but `AI_HasPossession` (`AIfunctions.cpp:1119`) puts GF's on-ball branch in charge. That branch walks him away and the ball is left. Contact touches only fire while passing the ball (`humanoid.cpp` ~526-531), and PES clips have no touch frame (`touchFrame -1`). Needs PES's first-touch or carrier-contact decode.
3. MOVESTEP holds at about 3 m from the ball with a sprint command (`look=1`, look point on the ball; the FSM stops on the look distance, `movestep_step.cpp:222`, `movestep_run.cpp:2250/2400`). Seen on the keeper trace and in the loose-ball trace. Owned by A3States' movement path; not changed here because it moves every PES-path player.
4. The loose-ball chase stand-in was dropped (it helped about 900 ticks and did not reach the bound). Main to decide whether to keep it as a documented ponytail.
5. `FUN_1406eebb0` is cited from `pes-re-fox-registry.md` and not re-verified here.
6. A5Tail informed of the gate fix (`32212a9`) and the keeper trajectory.
