# PES MOVESTEP: FUN_1406b7560 command-word fill (ClipWords)

> **Main note 22-10-26:** landed on main in batch B (`94b8f66`, gate GREEN 1708/1708; 20-min smokes off `464f398685b80578`, pes-loop `6a0fc698e4fd6fbe`).

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
| [4] | body direction = facing + bucket angle | vt+0x80 choice (cVar5), table DAT_1425dd660 | 0x1406b7c83, store 0x1406b7e25 |
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

## 6. Decision read every tick (ClipWords-decision, this round)

- Code: pushed to `ClipWords-decision` as `edebd8a`, with the correction commit `89d3cb1`. `ClipWords-wip` (`2c07ac2`) and `ClipWords` (`216c9c8`) are unchanged. The adapter reads the AI decision (`PesDecision`, stamped with its frame) that `pesmovestep::Think` stores after the controller's process. Before, it read the clip's `originatingCommand`. Thinking before the controller's process crashed: `Match::GetMentalImage` returns a raw pointer that the match frees on reset, so a pointer cached from the previous tick dangled.
- Loose-episode harness (`tools/dev/loose_episodes.sh` from origin/FirstTouchPort 40d970e; seeds 7 11 23 42; `total over / excess`):
  - clip command (this tree's HEAD behaviour): 19 / 71428 (7: 2/18829, 11: 7/24285, 23: 7/27766, 42: 3/548).
  - decision every tick (pushed): 65 / 47097 over three seeds (7: 17/15215, 23: 20/16396, 42: 28/15486). Seed 11 has no result: a kick-off stoppage ran 1758 ticks against a limit of 1757 (taker H4 stood 0.5 m from the ball at centre for 32 s).
  - decision every 25 frames (PES cadence): 83 / 61183 (7: 26/17860, 11: 22/14316, 23: 15/11468, 42: 20/17539). Not kept.
  - every tick plus body hold in every MOVESTEP state: seed 11 stalls at its first stoppage; 7: 3/24340, 23: 4/26839, 42: 1/10914. Not kept.
- Reading: the decision freshness lowers total excess but raises the count of episodes over the bound. About 31% of the controller's movement decisions have speed 0 (seed 7 probe: 11317 of 36613 movement decisions), where the stale clip never had speed 0. Set-piece takers are the regression to fix first.
- Seed 57 (Main's repro is on origin/FirstTouchPort base 0ca40ef, not on this tree): on this tree, team 0 player id 4 is in ACCELERATE (state 9) at frames 595-625 and 6295-6310 with an 8.00 m/s decision, clip `idle_0_3_run_090_push_aside_mirror`, body speed 0.00, ball 27.55 m away. Seed 57 here: 47 episodes, 25 over, excess 11617; the longest episode starts at frame 20585 (3410 ticks).
- The ACCELERATE handler does have exits at low ground speed: GOTO_RUN when `groundSpeed < kmh16` and StayDecision (`movestep_run.cpp:919-923`), TURN_STOP when `ground < kmh8` (`:1898-1901`). The probe stays in ACCELERATE anyway, so an arm that should fire is not reached. Next: instrument `Accelerate`'s arm choice at seed 57 frame 601.
- Body ownership (ThrowInTaker's point): holding the body in every MOVESTEP state stalls the first restart on seed 11. A hold must be scoped to the walk clips; not done.
- Smokes: not run for this experiment. The wire suite passes 20/20 on the experiment's code.
- `origin/main` (5b3ca51) is 22 commits ahead of this branch; not merged.
- Not done: sign migration (item 2). Crawl (item 3) narrowed to Accelerate's arm selection.

Open (added):

16. Set-piece takers under a per-tick decision: seed 11 kick-off, taker stood 0.5 m from the ball for 1758 ticks.
17. ACCELERATE at zero body speed (seed 57, player 4): exits exist at low ground speed; the arm that should fire is not identified.

## 7. Stall gates on ClipWords-decision (89d3cb1), traced

Corrections to section 6:
- "decision every 25 frames (PES cadence)" was my own construction, not a PES port. The raw asm of the player wrapper (0x140925750-0x1409257a6) computes `counter % 25 == obj+0x18` at 0x92578f and passes it as the fourth argument (r8) to 0x8bfcf0; 0x8bfcf0-0x8bfd58 never reads %r8. The driver FUN_140925470 is called unconditionally at 0x925769. The 25-frame gate is dead. How often the wrapper runs is undecoded. The comments in 89d3cb1 that say PES thinks every 25 frames are wrong.
- "Total stalled time is lower (about 64k against about 77k)" was the new-frame variant, which is not pushed. The pushed per-tick code on seeds 7, 23, 42 gives over 65, excess 47097; the clip-command baseline on the same seeds gives over 12, excess 47143. No improvement.
- The ground-speed exits cited (movestep_run.cpp:919-923, 1898-1901) are not in Accelerate (1438-1732). Accelerate's exits are the cls -1 arms, the turn and GOTO arms, and ClipEndExit (blend <= live, flag, or bVar23).

Seed 57 presser (id 4, team 0), frames 590-1754 and 6290-6319:
- Not the 2.22 m/s and 1.07 m case (that is base 0ca40ef). Here he stands at (-27.49, -17.21), 27.5 m from the ball, with decision Movement 8.00 m/s and no target (projected 8 m).
- State 9 (ACCELERATE) every tick; body 0.00 m/s. Clip idle_0_3_run_090_push_aside_mirror (motion 2243) at clipFrame 177, the last frame; clipRate 1.00.
- Each tick: live 177 >= blend 74, so the blend-point exit runs ClipEndExit, and the issue is `[issue] motion=0 playback=1.000 took=0`, serial +1 per tick. The motion is 0 and PlayIssuedClip refuses it.
- Gate, PES FUN_1406bb570: blend-point exit = decompile line 668, instructions 0x1406bc88a `CMP R13D,[RSP+0x160]` and 0x1406bc892 `JGE 0x1406bc8a1` (clip-end block). Clip-end issue = decompile line 717, `CALL [RAX]` at 0x1406bca77 (request block, foot band, 0).
- Port causes: (a) req[0x30] (motion) is outside the carried words (CarryIn carries 0x17-0x2e), and no writer sets it on steady ACCELERATE ticks, so every re-issue carries 0. PES's FSM writes req[0x30] in FUN_1406ba900 at 0x1406baeac, 0x1406bb148, 0x1406bb1b7, 0x1406bb1ea; the port's dispatch does not reproduce those writes. (b) GF PlayIssuedClip (humanoidbase.cpp:469-472) returns after setting only the rate when the motion equals the current one, so a same-motion re-issue never restarts a finished clip. PES's issue restarts (movestep.hpp:475). Even a motion-correct re-issue leaves live at 177, so the blend exit recurs.

Seed 11 kick-off taker (id 3, team 0), frames 0-1758:
- Frames 0-50: GOTO_RUN (7), mid-delay clips, IDLE (1). Frame 50: GOTO_RUN issue at playback 1.000. RUN_LOOP (3) by frame 52 at 0.75 m/s.
- Frame 52, decision Movement stand (fv 0.00): `[issue] motion=2220 playback=0.000 mirror=1 took=1`. The return address resolves to pes::movestep::RunLoop (file offset 0x2179f0). Source: RunLoop's TURN_STOP exit, movestep_run.cpp:2362-2364, which issues with req.F(kPlaybackDefault) (req[0x4b]) and then RequestState(kTurnStop).
- req[0x4b] is never copied in: the port has no record field for P+0x3750 (the port's own note says default 1.0), CarryIn does not carry 0x4b, and MoveReq starts at 0. PES's copy-in writes req[0x4b] from [R14+0x3750] in FUN_1406b7560: load at 0x1406b827d, store at 0x1406b8284.
- The stance clip plays at rate 0.00 from frame 53 to 1758 (clipFrame 0), never ends, and the humanoid does not requeue it. From frame 108 the queue is [ShortPass fv 0.00, Movement fv 1.25]; the adapter takes the front (the action) as the movement decision (class -1, speed 0). The kick is never selected. The watch fires at 1758 ticks, taker 0.5 m from the ball at centre.
- Not verified: which PES word RunLoop's TURN_STOP issue reads (FUN_1406c0170's disassembly has no direct +0x12c read), and why the stale-command baseline does not reach this exit on this seed (not traced).

Candidate ports (not implemented, awaiting Main's call):
1. Playback copy-in: model P+0x3750 in MoveStepRecord (default 1.0), copy it into req[0x4b] as FUN_1406b7560 does (0x1406b8284), and confirm the RunLoop TURN_STOP issue reads the PES word.
2. Five integer writes into the playback word: movestep_turn.cpp:1727, 2166, 2236, 2446, 2582. AnimeSlot() is a stub returning 0 (movestep_turn.cpp:166); PlaybackOf() returns int(RateCeiling). PES stores the float from vt+0x50 (FUN_1406d2f60: CALL [RAX+0x50] at 0x1406d3429, MOVSS [RDI+0xd4] at 0x1406d343f).
3. Motion word: source req[0x30] for the clip-end re-issue from PES's FSM writers (FUN_1406ba900), or carry it.
4. A same-motion re-issue of a finished clip must restart (humanoidbase.cpp:469-472), as PES's issue does.
5. The decision should be the first movement command in the queue, not an action.
6. Fix the 25-frame comments in 89d3cb1.

Open:
18. Baseline (clip-command) seed-11 trace: whether the kick-off reaches the RunLoop exit at frame 52.
19. PES word read by RunLoop's TURN_STOP issue (FUN_1406c0170).
20. PES writer of req[0x30] on the ACCELERATE clip-end path.

## 8. Integrated tip ClipWords-integrated-wip = c8d3ad6 (since a1d2fc7)

`ClipWords-decision` stays green-only at c8906a5 on origin. The integrated history is on `ClipWords-integrated-wip`, with merges of origin/main (8f0e242, 06cfaa0) and origin/StepFamily (through af8baab, ae854dc).

Own commits, in order:
- `c8906a5` MOVESTEP releases a finished ball action's hold to IDLE (trap freeze). 8-seed excess 73516 (a1d2fc7: 80643); seeds 7, 11 and 5 are worse.
- `843fba4` EnterAccelerate takes PES's gate |bodyDir - facing| > 5.625 (0x1425ddf84; compare at 0x1406cd0a1), the clamp 0.8 * |bodyDir - facing| (0x142572348; multiply at 0x1406cd0c6), and desired = heading - stamp (call FUN_140a92950 at 0x1406cd0df). Its value test is red with the heading-bearing gate (column 9, want 6).
- `3c09257` merge of RoleArrays 9d31415 (StepHeading), resolved per PES. MergeRA's clamp is bearingMag * 0.8, and bearingMag is |heading - facing|, not |bodyDir - facing|: recorded for every later merge. Its GOTO_RUN desired and speed-for-turn calls have no PES helper call behind them; they keep the heading stub (ponytail).
- `cc1ba63` the trap hand-back test checks the release tick (red under the committed freeze, green here).
- `790f14b` RUN_LOOP's swing arm writes no state (PES arm E, dec_c0170 123-262). Two tests now assert no state request.
- `704a56b`, `87a0cfe` canaries: APlayedClipAdvances on; TheFsmReachesStopAndAccelerate off, then on.
- `f9298d7` a ladder in the run-turn arm, with its row issue removed. Superseded by c8d3ad6.
- `c8d3ad6` RUN_LOOP's entry: T1, T2, E2 and the E3 selector replace the run-turn and keeper stand-ins; bVar19 reads req[0x1F].
- `7396445` the away-starter jostle test sets a live command (a walking speed, the precise arrival) and a rate ceiling: the three PES preconditions for the style arm.

Gates on c8d3ad6: run 80, movestep 104, step 35, turn 51; restart G1 2/2, G2 3/3, G3 3/3, G4 4/4; wire 23 passed (1 disabled on main: DISABLED_TheKickoffTakerClosesToTheBall). AThrowInTakerPlaysABallActionWhenTheBallLiesOverTheLine is green. tools/dev/gate.sh on c8d3ad6: 1490 tests, 3 failed: RunToStep.StopsWhenTheHeadingErrorIsPastNinetyDegrees and repo_pii_scan (both StepFamily's files, reported to StepFamily), and PesMoveStepJostle.AwayStarterJostlesTheNearOpponentInRunLoop (fixed in 7396445, not yet re-run in the gate). gate.sh exited 0 despite the failures. The 8-seed harness: not run.

Settled by the decompile (dec_c0170.c; dec_b70d0.txt; stop_dec.c). Line numbers are the decompile's.
1. T1 (114-116): (bucket 0 or 5, and 0.15 < req[0x4A]) or FUN_1406b70d0 == 0. FUN_1406b70d0 returns 1 when the run turn may still be taken: the STOP handler takes FUN_1406d6780 on a nonzero return (stop_dec.c 115-120). TeamDecode's "blocked" wording was wrong and is corrected in their report. The port's StepLetsRunTurn has the same sense.
2. T2 (117): req[0xE] != 1, or req[9] == -1, or req[0x2D] < req[0x2E], goes to the swing body (119-263). Ported in c8d3ad6.
3. E2 (265-266): (speed - 0.6) * 0.5 + 0.6 < req[0x4A] goes to the swing body. Ported in c8d3ad6. The speed is the rate ceiling: vt+0x18 of the +0xAC0 interface is FUN_1406eadf0 = P+0x377c (docs/re/pes-re-movestep-wave1.md:208), and the STOP handler's (vt+0x18 - 0.6) confirms it.
4. The selector (267-286), TeamDecode's table with bucket 6 corrected: bucket 0 and 5 take the style arm; bucket 1 and 6 take the ladder only under bVar19; buckets 2, 3, 4, 7, 8 and 9 take the ladder. Ported in c8d3ad6.
5. bVar19 (53-56) is req[0x1F] < 1 with req[0] in {3, 6}. The old keeperKind read req[9], the command class, not bVar19. Fixed in c8d3ad6.
6. Arm 4's threshold (110) was already correct: (RateCeiling - 0.6) * 0.5, with rate06 = 0.6 (0x14257a190) and half = 0.5 (0x142565600). An earlier version of this section called it a divergence; that was wrong.
7. Not ported: E0 (120: a dwell still running returns with nothing written); PES's tail (408-466, the T1-false path); the req[0x39] value FUN_1406d6780 writes (open in TeamDecode's report). GOTO_RUN's desired and speed-for-turn heading stubs stay (ponytail, see the 3c09257 entry).

### 8.1 Since c8d3ad6: the req[4] port, the switch-off chain, the throw-in stall

- `c4d998d` req[4] is the bucket centre from the facing (PES 0x1406B7E25). DAT_1425dd660 is loaded from the install into AngleBucketConstants.centre. req[4] = FoldFull(centre[flag] + facing). The flag is BodyBucketFlag(1.0) = 1, the rule TeamDecode audited (0x141194330: 1 iff record[0x3C] * 100 is in [0, 2^32]). Red, then green: three MoveFill tests. The no-wrap and facing-for-turn mutants each fail their own test. Suites: movestep 107, step 36, run 80, wire 23.
- `5522fc8` comment: the per-player record source is the open input (flag write 0x1411944E6, loader FUN_141194330). Flag 1 for every player is PES's default producer value (FUN_14119D380), not per-player data.
- `fbaef6f` docs: the port doc's req[4] row (docs/ports/pes-port-ClipWords.md, row 4).
- Regression from the flag: AThrowInIsReleasedFromItsSpotAndTaken passed at the b65ae74 gate and fails at flag 1 (throwIns 0). It passes with the flag held at 0, so the cause is the flag, not rounding. Main ruled (A): it joins AThrowInTaker and the 20-minute bound as ThrowInPathB's acceptance.
- Throw-in stall: the set-piece taker's command has no move target (tgt=0), so MOVESTEP walks a projected direction. Flag-1 whole-run histograms: H6 (id 5) RUN_LOOP 69%, IDLE 20%, ACCELERATE 9%, GOTO_RUN 1%; H4 (id 3) RUN_LOOP 61%, IDLE 23%, GOTO_RUN 9%. ThrowInTaker's flag-0 window (frames 8514-10271, H6): IDLE 642, GOTO_RUN 610, RUN_LOOP 343, IDLE_TURN 153 of 1758. The stop arm is not the stall. The set-piece command path (elizacontroller 454-466) is ThrowInTaker's.
- Gate on b65ae74 (before the port): 1491 tests, 2 failed, both throw-in restart tests (AThrowInTaker, EveryStoppage20).
- Switch-off chain (2-minute, seed 7): 533fd5c a3d5e43a3b7b03af; b15ac5f and 1ff023d alone 75a11cdb2b3c8ec5 (main's lane batch between 533fd5c and b15ac5f, intended); 1ff023d + RoleArrays (9d31415 merged with -X theirs, clean) 5b9a33ccde3a8ad1, equal to the tip. So the tip is main + RoleArrays, and the MOVESTEP commits are switch-off-neutral in aggregate. The earlier attribution to 1ceae1c was wrong: its parent is already a3d5. UBSan (default checks, abort on first) is clean on 1ceae1c. -fno-strict-aliasing -fwrapv changes no hash (533fd5c, 1ceae1c, b15ac5f).
- The first-parent bisect of the 20f28a3 to 5b9a move names the 27a2092 merge of main 8f0e242 (46 files of main's older content). It is a merge effect whose end state is main + RoleArrays.
- Open: the tail 408-466 and E0 (120), after landing; the per-player record source; the continuous body turn (0x1406B798C, state-byte override 0x1406B7CA7); req[0x39] (FUN_1406d6780).

### 8.2 RunLoopTail (branch `RunLoopTail`, commit `be31198`, pushed)

- The T1-false tail (decompile 408-466) and E0 (120) are ported in `RunLoopTail` (`movestep_run.cpp`). They are the two open items of 8.1. The commit message carries the citations, the red and green test list, and the mutants.
- Wire: `MoveStepWire.APlayedClipAdvancesAndIsNotRestartedByItsOwnMotion` counted one restart: player 18, tick 363, motion 2250 before and after (same Anim), frame 17 to 0. A gdb watchpoint and tick probes found the writer, `PlayIssuedClip`'s reset, after a set-piece placement had put the body on the idle clip (frame 5). `Referee::PrepareSetPiece` ran `ResetPosition` for 49 players that tick. This is not a missed hold: the body was off the clip. Main ruled (A): `HumanoidBase::GetPlacementCount`, and the test skips only the placed player's tick. Restarts 0, same-clip ticks 8165 (was 8166). Mutant (counter removed): red, restarted 1.
- Verified on `be31198`: `pes_movestep_run_test` 86, `pes_movestep_test` 107, `pes_movestep_step_test` 36, `pes_movestep_turn_test` 51, `pes_movestep_wire_test` 23. `tools/check_pii.sh` exit 0.
- Throw-ins (`pes_restart_delay_test`, `*AThrowIn*`): `AThrowInIsReleasedFromItsSpotAndTaken` OK, `AThrowInIsNotAwardedAgainOnItsOwnSpot` OK, `AThrowInTakerPlaysABallActionWhenTheBallLiesOverTheLine` FAILED (onLocomotion 0). ThrowInPathB owns it.
- ThrowInTaker's H11 free-kick stall: fixed on their build in `elizacontroller` (set-piece walk and free-kick run now set `useDesiredTarget` and `desiredTarget`). Not a tail issue.
- The 98% "against command" metric (ThrowInTaker's `trace-rows.awk`, window 4823..6439, player 22, seed 7, 5 min, `--pes-loop`) on `be31198`: 770 moving rows, 217 against (28%). Their reference: 1205 moving, 1176 against (98%). Not comparable tick for tick: at frame 4822 our player 22 is in state 12 at (22.48, -20.65), theirs in state 3 at (-21.15, -56.74), so the runs diverge before the window. The 98% does not reproduce on the tail build. No `fbaef6f` baseline run was made (it needs a second build).
- The metric probe was reverted; the tree is clean at `be31198`.

