# PES MOVESTEP: FUN_1406b7560 command-word fill (ClipWords)

Branch `ClipWords`: `03b182d` (merge of fill commit `1ceae1c` into origin/main `3df0fd8`).
`ls-remote`: `03b182dbfa7695da5b72afce9e9e22d3441c0097 refs/heads/ClipWords`.
Upstream commits since the start base (`533fd5c`) touch docs and tasks only, so sim and pes code is identical to the base.

## 1. Decode of FUN_1406b7560 (raw asm, CONFIRMED unless marked)

Operands: P = AnimePlayer (R14), req = RDI, cmd = R13. Origin = P+0x8 (x), P+0xc (height), P+0x10 (z). Facing = P+0x20 (req[2]).
Command block: cmd+0x14 = look point A (x, y, z at +0x14, +0x18, +0x1c); cmd+0x20 = move target B (x, y, z at +0x20, +0x24, +0x28).

| req word | value | source / operand | asm |
|---|---|---|---|
| [1] | bearing of B minus origin, deg | B = cmd+0x20 | 0x1406b7b38 CALL 0x140a92d40 (args from [RSP+0x48..0x50] = B) |
| [0x11] | horizontal distance origin to B | B = cmd+0x20 | 0x1406b786c CALL 0x140a8f860 (RDX = B, RCX = origin), store [RDI+0x44] |
| [0x12] | = [0x11] when byte cmd+0x6c is 0 | else distance to point list cmd+0x68 | 0x1406b7893..0x1406b78b1 |
| [3] | bearing of A minus origin, deg | A = cmd+0x14 | 0x1406b7c36 CALL 0x140a92d40 |
| [4] | body direction = wrap360(DAT_1425dd660[flag] + facing), the flag from the record rule (BodyBucketFlag) | ported: centre table from the install (AngleBucketConstants.centre), FoldFull wrap. Flag = 1 for the default producer value 1.0 (FUN_14119D380). OPEN: the per-player record source (flag write 0x1411944E6, loader FUN_141194330). The continuous body turn (0x1406B798C, state-byte override 0x1406B7CA7) is not ported | 0x1406b7c83 (vt+0x80 choice), store 0x1406b7e25 |
| [5] | wrap(req[1] - req[2]) = target minus facing | | 0x1406b7f0e (XMM0 = facing, XMM1 = req[1]) |
| [6] | wrap(req[1] - req[4]) = target minus body | | 0x1406b7e2a (XMM0 = body, XMM1 = req[1]) |
| [7] | wrap(req[3] - req[2]) = look minus facing | | 0x1406b7f23 |
| [8] | bucket(req[6]) | vt+0x60 (FUN_1406b9b20), 10 | 0x1406b7f55, store [RDI+0x20] |
| [0xc] | bucket(req[5]) | | 0x1406b7f3d, store [RDI+0x30] |
| [0xd] | bucket(req[7]) | | 0x1406b7f6d, store [RDI+0x34] |
| [9] | FUN_1406b8de0(cmd[0x17]): 1..5 to {0,1,2,3,-1} | cmd+0x5c | 0x1406b7f7c vt+0x58 |
| [0xa] | cmd[0x172] desired km/h | cmd+0x5c8 | decompile (INF on the exact store) |
| [0xb] | byte cmd+0x5c5 (use class rate) | | decompile |
| [0xe] | cmd[0x15] with 2 to 1 | cmd+0x54 | 0x1406b7584 (ECX = [R8+0x54]); decompile |
| [0x13] | dword cmd+0x50 | | 0x1406b7fe6 |
| [0x14] | dword cmd+0x60 | | 0x1406b7fed |
| [0] | move kind, cmd[0x16] with 8 to 2 | cmd+0x58 | decompile |

Derived rules on [0xe] (not ported): kinds 3 or 6 with class under 4 clear the word when |req[3] - req[1]| < DAT_14259d618 (asm 0x1406b7ebe..0x1406b7efd, CONFIRMED); a second clause keyed on P+0x3725 and on kinds 4/5 and 2/0x33/0x34 (decompile, INF).
Ball substitutions (not ported, decompile, INF): A (look) when |origin to A| < DAT_142598578 takes the ball prediction at index int(54 * speed4 / 60 + half) = 4 ticks (speed4 at VA 0x1425656c0, half at 0x142565600); B (target) when req[0x11] <= 0 takes the ball at index int(54 * kmh8 / 60 + half) = 7 ticks (kmh8 at VA 0x14259bdf4). The 54 is FUN_141e532d0 (VA 0x14351ae54) and 60 is VA 0x14259a764.
Walk-down branch (asm 0x1406b7743..0x1406b77d6): req[0xa] replaced by FUN_140a76070(2) under phase and route conditions. Not ported.

Corrections to docs/re/pes-re-anim-select.md 1.1 (for Main to archive): req[1] is the bearing to cmd+0x20 and req[3] the bearing to cmd+0x14 (the doc had them swapped); req[0x11] is the distance to cmd+0x20; req[5] is target minus facing and req[6] is target minus body (the doc had req[6] as target minus facing); the buckets are req[0xc] = bucket(req[5]), req[8] = bucket(req[6]), req[0xd] = bucket(req[7]); req[9] is the command's band, not the action class.

## 2. Changes (1ceae1c, merged as 03b182d)

- `src/pes/movestep.hpp`: `MoveTargetInput` and `FillMoveTarget` declared. `MoveStepRecord` gains `actionAnime` (P+0xac8, the anime action; a change is FUN_1406eebb0's entry) and `liveKind` (req[0x1e], carried tick to tick).
- `src/pes/movestep_common.cpp`: `FillMoveTarget` beside `FoldFull` (it had to live there: placing it in movestep.cpp pulled movestep_common.o into the turn test and collided with its SpeedToRate probe stub).
- `src/sim/gamedefines.hpp`: `PlayerCommand` gains `useDesiredTarget`, `desiredTarget` (cmd+0x20) and `preciseArrival` (cmd[0x15]), defaulted in the constructor.
- `src/sim/onthepitch/player/controller/elizacontroller.cpp`: target points set on the step-out (also `preciseArrival`), retreat, step-to-ball and chase commands.
- `src/sim/onthepitch/pesmovestep.cpp`: `FillCommandWords` member replaces the old free function (target from the command or the 1-second projection, look from cmd+0x14 or the target). req[9] now comes from the command band (`CommandClassFor(CommandSpeedClassOf(velocity))`). req[0xa] from the command desired km/h. The action class and foot band are stored when AnimeState changes (action entry), not per tick. `liveKind` is copied in and out instead of being overwritten with the FSM state each tick (this made IDLE re-issue every tick). `BandSpeedKmh` stand-in returns m/s (callers scale by speed3600; it returned km/h, so the band speed was 3.6 times too large).
- `tests/pes_movestep_test.cpp`: six `MoveFill.*` tests (below).

## 3. Verification

Test binaries, run with PES21_ROOT on the pushed tree: pes_movestep_test 98/98 (92 baseline + 6 new); pes_movestep_run_test 44/44; pes_movestep_turn_test 43/43; pes_movestep_step_test 14/14; pes_movestep_wire_test 14/14; sim/pes_mover_test 8/8. Baseline on 533fd5c: 92, 44, 43, 14, 14, 8, all green.
Wire `MoveStepWire.PesLoopWritesPlaybackOnlyWhenItIssues`: failed before the liveKind fix (quiet 0 of 300); after it, quiet 299 of 300. Cause: req[0x1e] was overwritten with the FSM state each tick, so IDLE saw a kind mismatch and re-issued every tick (the baseline never reached IDLE: it stayed in GOTO_RUN).
Red: the six MoveFill tests were written before FillMoveTarget existed. I did not build between writing them and writing the function, so no behavioural red run was observed. Disclosed; the mutations below are the evidence that each test discriminates.
Mutations (each applied with edit, built, run, then reverted; tree clean after):
- M1 req[9] = 0: SpeedBandIsTheCommandsOwnBandForTwoCommands FAILS.
- M2 precise inverted: PreciseWordIsTheCommandsStopFlagForTwoCommands FAILS.
- M3 req[1] from the look heading: HeadingAndDistanceAreToTheMoveTargetNotTheLookPoint FAILS.
- M4 req[0x11] from the look distance: same test FAILS. The first version of that test used a look point at the same 5 m distance as the target, so M4 survived; the look point was moved to 7.2 m and then M4 was caught.
- M5 look error without wrap: LookHeadingAndFacingErrorWrapAroundTheTurn FAILS.
- M6 req[0xc] and req[0xd] swapped: TurnsAndTheirBucketsAreTheTargetTurnTheBodyTurnAndTheLookError FAILS.
- M7 req[0xa] = 0: DesiredSpeedIsTheCommandsDesiredKmhForTwoCommands FAILS.

Smoke (2 min, seed 7, teams 1 vs 2, --pes21-root; no lock, per assignment; both exit 0):
- switch-off (no --pes-loop): `[determinism] a3d5e43a3b7b03af`. origin/main 533fd5c gives the same hash: UNCHANGED.
- --pes-loop: `[determinism] b9af943785c6ed27`. origin/main gives `0e9b3f7fe4db1ec6`: moved, as expected (the MOVESTEP path changed).
`ninja -t deps`: no `#deps 0` objects. `git status` on src, tests, docs, tasks: clean after the trace removal. PII grep on changed files: clean.
Gate: not run (Main gates).

## 4. Traced approaches (--pes-loop, seed 7, 2 min; temporary per-tick print of players whose command has a target point, removed before commit)

- Player 9 (chaser): ACCELERATE (9) to RUN_LOOP (3) at dTgt 1.30 m, dBall 1.30 m, speed 0.00 to 0.59 m/s (t=13738..13739). Later RUN_LOOP (3) to TURN_STOP (6) at dTgt 1.49 m for a new target (t=14271). PES's RUN_LOOP exit at 2 m or less is TURN_STOP (anime-select 3.4, item 6): the decision matches.
- Player 4: GOTO_RUN (7) to STOP (8) at dTgt 0.96 m (t=1258). The FSM stays in STOP while the body speeds up from 0.00 to 3.81 m/s (t=1330), dTgt 0.96 to 8.00 m. The decision matches PES's stop entry, but the body does not stop: GF's own controller keeps moving the player on the sprint command. This is the MOVESTEP-to-humanoid link gap (open item 1).
- Goal-kick taker: not traced. Its approach (AI_GetBallControlMovement) has no target point, so the 2.3 m front-gate rule in docs/ports/pes-port-DeadBall.md (item 6) is not verified here.

## 5. Open (numbered)

1. Body is not driven by the MOVESTEP decision: STOP and TURN_STOP are decided, but GF's controller keeps moving the player (trace, player 4). Needs the MOVESTEP-to-humanoid link.
2. BandSpeedKmh: the PES band speed (anime info record, vt+0x60) is not decoded. The stand-in is current ground speed in m/s for every band.
3. Speed byte 0x27: GF's physical_velocity (0..1 stat) still stands in. There is no reader for the PES roster byte; MoveTopRate takes the 0..1 stat.
4. Move kind req[0] (cmd[0x16]): no GF writer. The init kind 3 stays.
5. Derived precise rules on req[0xe] (kinds 2/0x33/0x34 near target, 4/5 at low class, P+0x3725): not ported. The GF flag is set only on the step-out.
6. Ball substitutions for a zero-length look or target (ball at 4 and 7 ticks, see section 1): not ported; the stand-in takes the facing.
7. Body direction req[4]: vt+0x80 bucket not ported (stand-in: facing).
8. req[0x12] point list (cmd+0x68): not ported.
9. req[0xb] (cmd+0x5c5), req[0x13..0x14] and the byte words: not written by the adapter (zero).
10. Walk-down branch (req[0xa] = FUN_140a76070(2)) not ported.
11. cmd[0x17] = 5 remaps to 3 on the decompile path (INF, asm not checked). No GF producer yields 5.
12. Speed band from GF velocity levels is a stand-in: dribble to jog (2), walk level to run (3), sprint to dash (4), idle to 0. GF has no walk band. Source: PES run-entry bands (kmh5/kmh11/kmh16 constants, anime-select 2.3).
13. Target point only on the step-out, retreat, step-to-ball and chase producers. Taker approach, pad input, trips and basic movement still project one second ahead (named ponytail).
14. docs/ports/pes-movestep-MoveStepWire.md items 5 and 6: item 6 (speed class per tick) is resolved by this change; item 5 (Speed byte, BandSpeedKmh) is partly resolved (units) and open.
15. Derived `preciseArrival` on GF producers beyond the step-out: none set; PES's writers of cmd[0x15] are not decoded.
