# MoveGateRE: why a 3.5 m/s presser 1.0-1.6 m from a resting ball does not reach the ball (G2), 19-11-26

Status: probe + PES decode + one mutation. No engine code landed, no commit, no gate. Probe worktree `<workdrive>/gf-MoveGateRE` (detached, base `8a65de72` + the r18 diff taken against HEAD, plus a scratch probe) is removed. The probe patch is kept in `<workdrive>/gf-MoveGateRE-scratch/probe-pesmovestep.patch`. Scratch: `verify_move_gate.py`, `xref_entry.py`, `probe-run3.log`, `probe-run4.log`, `mut-gate.log`, `dec/solver.c`, `dec/solver2.c`.
Tags: CONFIRMED = read in the image (verify script) or measured in the probe; INF = inference (reason given); OPEN = not settled (blocker named).
Image: `pes_full.bin`, file offset = VA - 0x140000000. GF numbers are for the probe tree (8a65de72 + r18); main's line numbers differ.

## 0. Constants (verify_move_gate.py, image reads; 33 checks, 0 failures)

```
fps              0x14351ae54  f32 54  bits 42580000   (planner frame rate, read via 0x141E532D0)
class 0.05       0x1425a8a28  f32 0.0500000007  bits 3d4ccccd
class 8.0        0x14259bdf4  f32 8  bits 41000000
class 13.5       0x1425a1ffc  f32 13.5  bits 41580000
class 22.0       0x1425c48c0  f32 22  bits 41b00000
dist1p5          0x14259bddc  f32 1.5  bits 3fc00000
dist 0x14259bdd8 0x14259bdd8  f32 1.29999995  bits 3fa66666   (the port calls this dist1p25; the image holds 1.3)
two              0x142565604  f32 2  bits 40000000
arrival 0.2      0x142598578  f32 0.200000003  bits 3e4ccccd
precise 0.1      0x142588200  f32 0.100000001  bits 3dcccccd
rate keep 0.3    0x142590d98  f32 0.300000012  bits 3e99999a
deg22p5          0x14259d618  f32 22.5  bits 41b40000
deg28p125        0x1425ddf94  f32 28.125  bits 41e10000
deg67p5          0x1425cca34  f32 67.5  bits 42870000
```

Verify script: `verify_move_gate.py` (33 checks, PASS). Mutation: `--mutate` moves the planner call site one byte; exactly that check fails: **MUTATION CAUGHT**. Caveat: the first draft of the script had a 0x400-byte decode window, which missed a call at +0x542 and made the mutation MISSED. Fixed to 0x1000 and re-run.

## 1. PES side (decode)

**1.1 Planner: the ETA is fps times the solver's time, not fps times the distance (CONFIRMED, correction).**
`FUN_1406788B0` (`0x1406788B0`): calls `FUN_1406B9270` at `0x140678924`, reads `fps` from `0x141E532D0` at `0x14067892D` (returns the float at `0x14351AE54` = 54.0), returns `int(fps * out)` clamped to >= 1. `out` is `afStack_28[0]`, written by the solver. `FUN_1406B9270` passes its own `param_5` as the solver's float output (`*param_2 = param_5._0_4_`), and calls `FUN_140707EE0` at `0x1406B935D`. In `FUN_140707EE0` the output accumulates `*param_3 += fVar5 / fVar6` (the time to cross a segment), and `FUN_1407057F0` adds per-segment times built from the interface's speed-profile vtable (`vt+0x38`, `vt+0x40`, `vt+0x48` on `param_2+8`, decompile `dec/solver2.c` lines 45-98). So the ETA is a time profile of the body, not `fps * dist`. The reading in `pes-re-press-life.md` §9 and `pes-re-approach.md` §1.4 is wrong, and is corrected here. OPEN: the three vtable functions (the speed profile) are not decoded, so the ETA's numbers for 1-1.6 m are not known.

**1.2 Speed class `FUN_140A75BE0` (CONFIRMED, no .pdata entry; linear decode from `0x140A75BE0`).** Own speed `v` from `[rcx+0x3fc+16*[rcx+0x108]]`; returns 0 for `v <= 0.05`, 1 for `<= 8`, 2 for `<= 13.5`, 3 for `<= 22`, 4 above. The four cut-points are loaded by `movss` at `0x140A75BF3`, `0x140A75C0E`, `0x140A75C1B`, `0x140A75C2C` (verified).

**1.3 MOVESTEP entry callers (CONFIRMED, full image scan `xref_entry.py`: 4 direct sites, mutation CAUGHT).** `FUN_1406B8340` is called directly from `0x140782552` (MOVE Update, `.pdata` begin `0x1407824A0`), `0x140789E42` (STEP_MOVE, begin `0x140789DD0`), `0x1407D23A2` (DRIBBLE_STEP_MOVE, begin `0x1407D1E60`), and jumped to from `0x1407347B6` (thunk `0x1407347B0`, DEMO_MOVE). These are the PES actions anime 2, 3, 6, 74 (the comment in `pesmovestep.cpp:665-668`). Limit: indirect (vtable) callers were not scanned.

**1.4 Precise word (port claim, INF from the port's comment; not re-decoded here).** `FillMoveTarget` (`src/pes/movestep_common.cpp:117-121`) sets `req[0xe]` when the command id is 0x33, 0x34 or 2 with kind < 2, the flag clear, command class 1..3, and the anime state nonzero. The port cites PES 274-281. GF's presser (command id 2, class from his command speed) gets `precise = 1` from this clause.

**1.5 Stay and arrival (CONFIRMED constants, INF for the arms).** `StayDecision` (port `movestep_group_a.cpp:613-...`): threshold 0.1 m when precise, 0.2 m otherwise (`0x142588200`, `0x142598578`). At 1.0-1.6 m neither arm stays the man, so a 1.25 m man is never "arrived" by this gate. INIT's step-start rule (`pes-re-anim-select.md` §2.3, INF for the port's arms): precise, v <= 22 km/h, class != 4, and (dist <= 1 m or v <= 8 km/h or class < 3) starts a step. RUN_LOOP's TURN_STOP at dist <= 2 m in a turn, and STOP at rate < 0.3 (`0x142590D98`) (`pes-re-anim-select.md` §3.4).

**1.6 What PES's man does at 1.25 m (INF, no PES capture of this distance).** PES's presser is in the MOVE action while MOVESTEP steers him (1.3). The planner's ETA is not the command speed (1.1), so a 1.25 m man is not paced by the ETA. Whether PES's presser crawls at 1-1.6 m is OPEN: it needs the live hook in §7. INF from the constants above: PES's step and run arms give him a step or a run, not a stand.

## 2. GF per-tick trace (probe, one 2-minute seed-7 run of `PesPassTarget.IsSetInFlightAndNeverStaleWhileTheBallRests`, r18)

Probe rows (`[mv]` = MOVESTEP ran; `[mv-skip]` = `ProcessMoveStep` returned at the movement-clip gate). Filter: the man's movement command has `useDesiredTarget`, the ball is at rest (< 0.1 m/s), and the man is 0.8-1.8 m from the ball. Body speed = `GetMovement()` in m/s (CONFIRMED: `actualMovement` is scaled by `1000/kTick_ms = 54`).

| path | rows | what it shows |
|---|---|---|
| `[mv-skip]` (MOVESTEP does not run) | 382 (72%) | fn HighPass 214, Special 135, Trip 31, Sliding 2 (`GetCurrentFunctionType()` of the playing clip); FSM state GOTO_RUN 7 (276 rows), TURN_STOP 6 (72), RUN_LOOP 3 (24), ACCELERATE 9 (10); cmdV 3.50 on 195 of them |
| `[mv]` (MOVESTEP runs) | 151 | states 3, 4, 5, 6, 7, 9 (from-to); the clip is issued on 23 rows only |

Man 26, team 1 (the man ApproachRE tracked), first run, every `[mv-skip]` row from t = 18.67 s (the clip frame runs 28/90 to 56/90):

```
t=18.67 dist=1.784 cmdV=5.00 cmdFn=Movement st=7 fn=HighPass frame=28/90 body=3.04 m/s cur=-1 (GF clip)
t=18.68 dist=1.751 cmdV=5.00 ...                                       frame=29/90 body=3.12
t=18.72 dist=1.684 cmdV=3.50 ...                                       frame=31/90 body=3.28
t=18.80 dist=1.539 cmdV=3.50 ...                                       frame=35/90
t=18.91 dist=1.345 cmdV=3.50 ...                                       frame=41/90
t=19.00 dist=1.196 cmdV=3.50 ...                                       frame=46/90
t=19.18 dist=0.894 cmdV=3.50 ...                                       frame=56/90 (clip still playing)
```

Reading: the command is Movement (cmdFn 1), and the command speed is 5.00 then 3.50, but the body is on a non-movement GF clip (HighPass, 90 frames). `IsMovementClip()` is false, so MOVESTEP never runs for him and his FSM stays in GOTO_RUN. The body moves 3.0-3.3 m/s (the clip's own motion), not at the command. He arrives at 0.89 m while the HighPass clip is still playing. In the probe, the presser's body is steered by the GF clip, not by the FSM.

Discrepancy (not reconciled): ApproachRE reported man 26 at 1.25 m moving 0.14-1.54 m/s. This run shows 3.0-3.3 m/s at 1.2-1.8 m on the HighPass clip. Possible cause: ApproachRE's speed field. Not settled here.

`[mv]` rows (MOVESTEP ran): `st=9>9 prec=0 cls=3` 27 rows (ACCELERATE); `st=7>7 prec=0 cls=2` 26 rows (GOTO_RUN); `st=3>3 prec=1 cls=0` 24 rows (RUN_LOOP, precise, class 0); `st=5>4` / `st=4>5` (STEP_LOOP_CHANGE / STEP_LOOP) 13 + 13 rows, precise, class 1. Command class `ccls` 1 (jog) on 30+29+14 rows. So when the FSM does run on a presser, it runs the step loop or RUN_LOOP with precise = 1, as the PES rules in 1.4-1.5 predict.

## 3. Verdict (3 lines)

1. G2 is not a MOVESTEP speed gate. For 72% of the approach ticks (382 of 533) `ProcessMoveStep` returns at the GF movement-clip gate: the body is on a non-movement GF clip (HighPass, Special, Trip), so the FSM stays frozen at GOTO_RUN and the body moves on that clip, not at the 3.5 m/s command.
2. PES runs MOVESTEP only in MOVE, STEP_MOVE, DRIBBLE_STEP_MOVE and DEMO_MOVE (4 callers verified, 1.3). So PES's approaching presser is steered by MOVESTEP, not by a leftover clip. INF: PES's man does not crawl at 1-1.6 m by design; OPEN until the live hook in §7.
3. Lifting the gate (mutant) makes the stale bound worse (11.44 s vs 8.52 s, single seed) and runs MOVESTEP on 1311 rows. So the gate is a real divergence but NOT the stale root; the stale ball is held elsewhere (G3, holder latch, below).

## 4. GF rule to change (file:line, probe tree)

- **`src/sim/onthepitch/pesmovestep.cpp:671`** `if (!player.CastHumanoid()->IsMovementClip()) {` returns before the FSM, which stops MOVESTEP for every tick the body plays a non-movement clip. This is the GF stand-in for PES's "in MOVE" (the comment at `:669-670` names it). PES's rule is the four entry callers in 1.3, so the stand-in must not hold a presser whose command is Movement in a non-movement clip.
- **`src/sim/onthepitch/player/humanoid/humanoidbase.hpp:277-279`** `IsMovementClip() = currentAnim->functionType == Movement || OnPesLocomotion()`: the clip the body is on decides it, not the command.
- **`src/sim/onthepitch/player/humanoid/humanoid.cpp:604-613`** lists per-type clip-keep arms (ShortPass/HighPass, Deflect/Sliding, BallControl/Trap). OPEN: which arm keeps a HighPass clip while a Movement command is queued (the probe shows it does, for 90 frames, with the command already at 5 then 3.5). The fix is the humanoid's clip switch for a Movement command, not the MOVESTEP gate. Lifting the gate alone is measured (section 5) and does not fix the stale.
- Not changed: `elizacontroller.cpp:962` (the quantiser) is already bypassed for a press target on r18; G1 is closed on r18.

## 5. Mutation (measured, reverted)

- Mutant: `pesmovestep.cpp:671` changed to `if (false) {` (the gate removed for every man), one 2-minute seed-7 run. Result: worst stale **11.44 s** (r18 baseline 8.52 s, bound 2.94 s), MOVESTEP rows 1311 (was 151), skip rows 0. The stale test stays red. Reverted in source (`probe-pesmovestep.patch` has no mutant line). Not rebuilt after the revert: the worktree was removed.
- Probe-only edits (scratch): `MoveProbeRow`, `MoveProbeSkip`, `LastIssue()`, the `GF_MOVE_PROBE` gate. Not for landing.
- Baseline reproduced: r18 probe worst stale 8.5185 s (red, bound 2.94 s; `probe-run4.log`).

## 6. Red-test spec (PES facts; measured on the r18 probe)

Test: `PesPassTarget.PresserBodyIsSteeredByMoveStep` in `tests/sim/pes_pass_target_test.cpp`, on the same `RunPesLoopMatch` seed-7 2-minute match.
- Scenario: `PesPlayState() == 1` (in play), ball at rest (`GetBall()->GetMovement().GetLength() < 0.1`, m/s), a loose presser (`IsLoosePresser`) whose movement command has `useDesiredTarget` and whose distance to the ball is 0.8-1.8 m.
- Assertion: on every such tick, `CastHumanoid()->IsMovementClip()` is true (MOVESTEP steers him). Expected violations: 0.
- PES fact: MOVESTEP's entry runs only in the four actions of 1.3. The approaching presser is in MOVE (INF until the live hook, section 7).
- r18 measured: **382 violating ticks of 533** (72%), HighPass 214, Special 135, Trip 31, Sliding 2 → red.
- Mutation that must turn it red: forcing `functionType = HighPass` on the presser's clip (or the r18 tree itself: red). The gate-removal mutant (section 5) does NOT change this assertion (the clip stays non-movement), so the test pins the body's clip, not the gate.
- Second spec (stale, unchanged): `PesPassTarget.IsSetInFlightAndNeverStaleWhileTheBallRests` stays red (8.52 s, and 11.44 s with the gate removed). It is not fixed by this rule.

## 7. Live hooks wanted (none started; Main owns the rig)

1. `move_entry_1406B8340`: entry of FUN_1406B8340 (`0x1406B8340`), entry-only. Predicate: ball speed < 0.1 m/s and the man 0.8-1.8 m from the ball. Record the man's anime action byte (P+0xAC8) and the MOVESTEP state (P+0x245C). Cap 24, FULL. Answers OPEN 1.6: does PES's presser run the MOVESTEP entry (MOVE, anime 2) at 1-1.6 m, and what action is he in.
2. Optional: the presser's ball-rest approach ETA at the same ticks, from the planner's return value (`0x1406788B0`), to test 1.1.

## 8. Open items

- OPEN 1: PES's speed-profile vtable (`vt+0x38/0x40/0x48` on the interface at `param_2+8`) and `FUN_1407057F0`'s full reduction: the ETA's numbers (1.1).
- OPEN 2: which humanoid clip-keep arm holds a HighPass clip during a Movement command (`humanoid.cpp:604-613`, section 4).
- OPEN 3: PES's presser action at 1-1.6 m (hook 1).
- OPEN 4: the G2 speed discrepancy (0.14-1.54 m/s in ApproachRE vs 3.0-3.3 m/s here).
- OPEN 5: the holder latch (G3) and the W1 clear, which hold the stale ball. Not in this slice.
- Port naming error: `movestep.cpp:272` maps `0x14259bdd8` to `dist1p25`; the image holds 1.3. Not changed.
