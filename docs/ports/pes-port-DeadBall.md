# pes-port-DeadBall: the loose dead ball after a taken restart (PES path, pes_loop)

Status: NOT DONE. Acceptance unmet. Branch `DeadBall` (pushed, ls-remote `cf2915cad745bffff90e78733358ee7581cb6397`), not for merge. Base `origin/A3States` `5477448` merged with `origin/main` `f99908a` (merge `44138f2`), then `aaa2eb1` (red test) and `cf2915c` (WIP stand-in).

## What was decoded or changed

1. **Trace (CONFIRMED, live run, A3States+main tree, 2-min seed 7, pes_loop).** After piece 1 (taker H4) is taken, the ball lies at (-10.2, -3.7), `inPlay 1`, no retainer, ball 0.08 m/s. GF's stalemate forcing (`match.cpp` ~836-860) names the closest active man across both teams (H7, H3, H2 at 1.3-6.6 m). The designated man H76 stood 1.07 m from the ball, `ttb 440 ms` (under Eliza's 1000 ms), and his step-at-ball command was generated (`REQ`: `fn1 dir(0.61,-0.80) v8.00 look1`) every ~5 frames. His *originating* command stayed `fn14 dir(0.05,1.00) v0.84` for 130+ ticks, and he walked away, 1.07 m to 4.38 m.
2. **Why the step never moved him (CONFIRMED, code and trace).**
   - The PES mover reads `anim->originatingCommand` (`pesmovestep.cpp` `FillCommandWords`). That field is set only in `HumanoidBase::SelectAnim` (`humanoidbase.cpp:1221`).
   - A requeue into a Movement command is refused while the current clip is Special: `humanoid.cpp:1972-1978` (`currentAnim->functionType != Movement && command == Movement -> return false`). The requeue predicate itself needs `functionType == Movement` (`humanoid.cpp:322-341`).
   - `fn14` is `e_FunctionType_Special`, which is the PES locomotion clip that `PlayIssuedClip` (`humanoidbase.cpp:454`) installs. Enum values from `gamedefines.hpp:52` and `humanoidbase.hpp:25`.
   - Not the cause: no owner (the designation is set), set-piece gating (`IsInSetPiece` is false), or GF's 4 s restart timers (the ball is in play).
3. **PES pickup rule (INF, not ported).** `FUN_1404f4be0` (the chase predicate, `pes-re-possession-latch.md` §17-18) is decoded at structure level only. Its helper bodies are not: anim-class predicates `FUN_140a6e5e0`/`FUN_140a6e340`, `FUN_14043b8b0`, `FUN_140481b00`. The per-player action `FUN_14092c9f0` is CONFIRMED (`pes-re-teamtick.md` §5.1). The command-block writer `FUN_1406b7560` is named in `pesmovestep.cpp` and is the removal site. No complete pickup rule was portable, so the stand-in path was taken.
4. **Stand-in (WIP, `cf2915c`).** `pesmovestep.cpp` `LooseBallChase` (pes_loop only, called from `ProcessMoveStep` before the adapter is built). Conditions: `inPlay`, not in a set piece, no retainer, the designated man, not the keeper, not in possession, ball speed at or under `GameplayTuning::kStalemateBallSpeed_ms`, and distance over 0.1 m (Eliza's step gate). When they hold, it writes a sprint step (`sprintVelocity`, look-at the ball) into the clip's command through the new `HumanoidBase::SetClipCommand` (`humanoidbase.hpp`). `ponytail:` comment names `FUN_1404f4be0`, `FUN_14092c9f0` and `FUN_1406b7560`.
5. **Test (`aaa2eb1`).** `PesRestartDelay.ALooseDeadBallInPlayIsReachedAndPlayed` in `tests/sim/pes_restart_delay_test.cpp`. It counts loose episodes (in play, not in a set piece, no retainer, ball at or under 0.5 m/s) and fails when one exceeds `GameplayTuning::kStalemateForceAction_s * Match::kFramesPerSecond` = 270 ticks. The bound is GF's stalemate clock, a ponytail bound, not a PES number, because PES's pickup time is not decoded.

## Measurements (loose-episode longest, ticks; bound 270)

| Build | Longest | Result |
|---|---|---|
| A3States+main, no stand-in (`aaa2eb1`) | 5847 | red (RED-1) |
| Stand-in writes words only (`FillCommandWords`) | 9755 | red (`FUN_1406b7560` path only; rejected: the body uses the clip command, `humanoid_utils.cpp:298`) |
| Stand-in writes clip command, look-at ball (`cf2915c`) | 939 | red (committed) |
| + touch gate accepts PES locomotion for designated | 11217 | red (reverted) |
| clip command, no look-at | 7224 | red (reverted) |

Off-path (no pes_loop, `PES21_ROOT` unset, `--pes21-root` set): `[determinism] a3d5e43a3b7b03af` (matches the acceptance). The stand-in returns before `LooseBallChase` on the off path; the only off-path edit is an added setter.

## Mutations

- M1, stand-in disabled: the `aaa2eb1` tree (no stand-in) gives 5847 ticks, red. Caught.
- M2 through M5 (drop designated check, drop at-rest gate, drop GK exclusion, drop 0.1 m gate): NOT RUN (the test is not green, so a mutation result would mean nothing yet).

## Confirmed, not confirmed

- CONFIRMED: the stale-command mechanism (items 1-2), the stand-in's effect on the command (the designated man's `oc` changes to the ball-directed step; H21 closed 0.94 m to 0.11 m and moved the ball).
- INF: the touch gate (`humanoid.cpp:526`, Movement only) is a second gap. Its PES-locomotion variant made things worse (11217), so it is not the fix alone.
- OPEN: why the designated man holds at about 2.9 m with `spd 0.00` on motions 2248, 2261, 3346 (`fn14`, `intr 1`). Those motions are not decoded.

## Gate and smokes

- Gate: not run (Main gates).
- Tests run: `pes_restart_delay_test --gtest_filter=PesRestartDelay.ALooseDeadBallInPlayIsReachedAndPlayed`, 0/1 pass (red, 939 ticks).
- 2-min pes_loop seed 7 on this branch: still stalls (the suite's `RunMatchWithinTheBound` fails on stall and game-over). Not full time.
- 20-min pes_loop: not run.

## Coordination

- A5Tail (owner of `pessequencer.*` and the `referee.cpp` restart paths) was informed: the shared root is `Humanoid::SelectAnim`'s ReQueue gate (`humanoid.cpp` ~1978), which the stand-in does not change. The taker-never-kicks stall probably shares it. I did not touch A5Tail's files.

## Open items

1. The chase stops about 2.9 m from the ball: MOVESTEP holds (`spd 0.00`) on motions 2248/2261/3346. Next: log `rec.state` and the issued motion names for the designated man around the stall (the `trace2`/`trace3` runs in scratch have the positions).
2. The touch gate (`humanoid.cpp:526`) needs Movement; the PES-locomotion variant was tested and made it worse. Decide whether the contact belongs to PES's contact writer (`pes-re-ball-contact.md`).
3. PES's loose-ball predicate `FUN_1404f4be0` helper bodies are undecoded (`pes-re-possession-latch.md` §17-18). The stand-in stays until they are.
4. The requeue gate `humanoid.cpp:1972-1978` is shared with A5Tail's taker issue. Not changed here (it would alter the off path).
5. `humanControlled` (`pesoffball.hpp:180`) is never set in the sim, so the stand-in has no human guard. Revisit if a human-controlled path lands.
6. The test bound is GF's stalemate clock (5 s), not a PES number. Replace it when PES's pickup time is decoded.
7. Cosmetic: the new `#include "onthepitch/gameplaytuning.hpp"` in `pes_restart_delay_test.cpp` is not in sorted order.
8. Not run: M2-M5, the 20-min smoke, the gate.

Do not merge `DeadBall` to main. The 2-min pes_loop smoke still stalls on this branch.
