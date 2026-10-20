# PES role-class arrays port (RoleArrays)

Status: DONE on `RoleArrays`, pushed. Base `ecd5954`. Commits: `3f574b4` (first offball red), `5964d28` (three scale-defect reds), `8cbf2a1` (the port; `origin/RoleArrays` = `8cbf2a1`). Restart fix on `RoleArrays-on-main` = `d6cb09b` (port rebased onto main `8d8eeba` as `8f57eda`, plus the a92950 fix `9d31415`; `origin/RoleArrays-on-main` = `d6cb09b`).

Summary: PES keeps per-side arrays A (`S+0x3F8C`, T+0x4BD4) and B (`S+0x3FB8`, T+0x4C00) of eleven role CLASSES (0..9). GF had `PlayerFacts::position` holding the 13-value registered position and compared it with class thresholds. The port adds the arrays, reads the class map from the install, and re-bases every off-ball and team class reader onto B. `position` and its mapping are deleted.

## 1. What PES does (docs/re/pes-re-role-writer.md; decode re-checked on `pes_full.bin`)

- Class map `FUN_1408C51F0`: jump table at `0x1408C52A4` (13 RVAs, dispatch `0x1408C5230`). Each case is `41 ba imm32` (MOV R10D,imm) then `41 8b c2 c3` (MOV EAX,R10D; RET). Role 0 is the bare tail (R10D = 0). Result: registered role 0..12 -> class 0..9: 0→0, 1→1, 2→2, 3→2, 4→3, 5→4, 6→5, 7→5, 8→6, 9→8, 10→8, 11→7, 12→9.
- Classes: GK0 CB1 SB2 (LB/RB) DMF3 CMF4 SMF5 (LM/RM) AMF6 SS7 WF8 (LW/RW) CF9.
- Bulk writer `FUN_1404F5F10` (one team) stores A and B with the same value (`0x140A425A0` A, `0x140A42590` B). Phase `f = 2-(flag!=0)`. Calls: setup loop `0x141FBB06A` (flag = team index == 0); team tick `FUN_14051D9E0` head `0x14051DA5A` (flag 0) and tail `0x14051E2E7` (flag = `T+0x255 != 0`). Both gated on `MI+0x1775 == 0`.
- Tail swap `FUN_140A422C0` (B only, A untouched) at `0x14051E31E`, gated on `T+0x255 != 0` and `FUN_140A41BE0` (`S+0x7D8C < 11 && S+0x7D90 < 11`). Its pair `T+0x89D4/0x89D8` has no decoded writer.
- Keeper selector `FUN_140A62610` picks the first active slot with B == 0.

## 2. The GF defect

`PlayerFacts::position` (13-value, `ob::Position`) fed the class thresholds of the reach gate, the attack-level skip, the slot admit rule and the angle branch. Those thresholds are on the class scale, so the reader compared the wrong number. `roleClass` (class map) already existed as a second copy.

## 3. Reader map (final state)

| GF site (after port) | PES site | Threshold | Old source | Effect |
|---|---|---|---|---|
| `ChanceMaskReject` reach path `cls < 4` (pesoffball.cpp) | `FUN_140606CD0` reach `iVar22 < 4` (B, run2.c:557/572) | class | position `< 4` | DMF (class 3) now reach-gated; was not (pos 4) |
| `ChanceMaskReject` attack skip `cls <= 4` at level < 1 | run2.c:562 (`4 < iVar22` exempts) | class | position `<= 4` | CMF (class 4) now skipped at level 0; was not (pos 5). Measured inert on the smoke (A1 below) |
| `AttackSlotMask` admit `cls >= 7` | `FUN_140500200` at `0x500663`: `cmp edx,7; jl`, edx = T+0x4C00[slot] (disassembled) | class | position `>= 7` | RMF (6→5) and AMF (8→6) no longer admitted outright, scored |
| `RankAngleOk` reach gate `cls < 4` and angle branch `cls >= 4` | run2.c:633-644 | class | position `< 4` / `> 3` | DMF leaves the angle branch |
| `StyleLb` stand-in (no style table) `cls == 2` | `FUN_140A64030(id,0x11)` (style bit; stand-in) | class | `position == 2` (LB only) | RB now counts (SB covers LB and RB): deliberate, RB loses the reach path without `ahead > 3` |
| `ChanceMaskReject` keeper `cls == 0` | `FUN_140593430` head guard `T+0x4C00 != 0` | class | position `== 0` | same role set |
| `CounterInput` runner `cls > 4` / supporter `cls > 2` | `FUN_14053E9A0` (`> 4`), `FUN_140541010` (`> 2`) | class | `WideOrForward`/`BeyondBackLine` on role | same role sets (checked by enumeration) |
| `StyleRunDepth` (pes_loop) | `FUN_1405D8B70` (class 7..9) | class | dynamic role class | same mapping; source now B |
| `L596240` `cls >= 3` | `FUN_140596240` | class | dynamic role class | same source via the mirror |
| `FramesToReach` `rec[1] == 3` halving | `FUN_140654E60`: reads the player record +1 byte (`FUN_140A48910(G,id)+1`), NOT a class array | record byte | `position == 0` | GF has no record byte. Keeper stand-in kept in `PlayerFacts::recordByte1`, set in BuildRunWorld. Ponytail |
| `pessubs` `SubRoleClass` (stamina subs) | stamina-sub reader (array A) | class | GF-role copy of the map | NOT re-based (see §6) |
| `peslane::KeeperSlot`, `referee.cpp:47`, `pesclock.hpp:178` | keeper selector; perf tier | class | — | not touched (A4Lanes2 / forbidden files) |

## 4. Port shape

- `src/sim/onthepitch/pesroles.{hpp,cpp}`: `LoadRoleClassMap` (install read via `PeImage::ReadVa`, opcode-checked; fails the constants load on mismatch), `RegisteredRoleOf` (GF role → registered 0..12), `WriteSideRoles`, `WritePhase`, `SwapB` (B only; latches the pair), `TailSwapAllowed`.
- `TeamSheet::SetRegisteredRole` (sheet row +0x28), `TeamConceptConstants::roleClassOfRegistered` (loaded with the other constants, before `InitPlayers`).
- `TeamAIController`: `SetUpRoleArrays()` at `InitPlayers` (setup write, flag = team index == 0, static formation roles); `WriteRoleArrays(head)` at the team tick head (phase flag 0) and tail (flag = possession, then the B swap if requested).
- Registry mirror (ponytail): each bulk write sets the slot's registered role from the slot player's CURRENT GF role (dynamic formation entry), because the readers this replaces used the dynamic role. GF has no sheet phases.
- Removed: `PlayerFacts::position`, `ob::Position`, `ob::RoleClass` (the GF→class copy), `WideOrForward`, `BeyondBackLine`.

## 5. Tests

- Scale-defect reds (commit 5964d28), each failing on base at its divergent assert and passing now: `ADefensiveMidfielderIsClassThreeAndIsReachGated` (base fails line 251), `ACentralMidfielderIsClassFourAndIsSkippedAtAttackLevelZero` (base fails line 271), `ARightMidfielderAndAnAttackingMidfielderAreScoredNotAdmitted` (base fails lines 295-296).
- Fixture change: `OffBallMaskTrace.TheClassifierNamesTheRejectingGate` line 967 expected `kMaskCarrierOrReceiver` for slot 1 with a valid carrier. It passed on base only because unset slots defaulted to `position 0` (keeper). With a carrier, the receiver disjunct needs no carrier (tests 946-950), so the expectation is now `kMaskPass`; the keeper assertion stays explicit at 969-970.
- New `tests/sim/pes_roles_test.cpp` (6 tests): install map relations plus four pinned documented classes (CB1 DMF3 AMF6 CF9), `WritePhase`, `WriteSideRoles`, `SwapB` (B only), refused out-of-range pair, `TailSwapAllowed`.
- Final suites on the final build: pes_roles_test 6/6, pes_offball_test 65/65, pes_team_concepts_test 31/31, pes_subs_test 4/4, pes_style_gate_test 5/5, pes_nearest_opponent_test 2/2, pes_tick_order_test 2/2, pes_challenge_counters_test 3/3, sim_match_test 10/10.

## 6. Mutations (one at a time: edit, rebuild, run, revert, rebuild)

- M1: install map, DMF case read as `imm - 1` (class 2). `pes_roles_test` FAILED at line 33 (expected 3, actual 2). Reverted: 6/6.
- M2: tail swap applied to A instead of B. `pes_roles_test` FAILED `SwapExchangesBOnlyAndLatchesThePair` (lines 70-72). Reverted: 6/6.
- M3: `kReachClassBelow` 4 → 5. `pes_offball_test` FAILED `ADefensiveMidfielderIsClassThreeAndIsReachGated` (line 253: CMF takes the reach path). Reverted: 65/65.
- A1 (ablation, not a mutation): attack-skip `<= 4` → `<= 3`. Switch-off output byte-identical to the unablated build (`20f28a3df9b0ce12`). The skip is inert on this smoke: `level` is the team's mentality rung (`teamAIcontroller.cpp:2465`), and no team in this run is at level 0. Reverted.

## 7. Smokes (`--seed 7 --minutes 2`, `assets/fixtures/teams.sql`, rc 0 on all)

| Build | Switch-off hash | Ticks | Passes | Chance runs | `--pes-loop` hash | Ticks | Passes | Chance runs |
|---|---|---|---|---|---|---|---|---|
| base `ecd5954` | `a3d5e43a3b7b03af` | 38693 | 9-6 | 197-172 | `a817a13303958f90` | 44806 | 10-10 | 103-62 |
| port `8cbf2a1` | `20f28a3df9b0ce12` | 32549 | 11-10 | 86-69 | `fdb8788a96e87230` | 58154 | 5-8 | 77-76 |

- `--pes-loop` completes (rc 0).
- Switch-off determinism: `20f28a3df9b0ce12` reproduced by two builds (mirror and A1 runs), so the move is not run-to-run noise.
- Switch-off move, reader by reader. Measured: only A1 (inert). The rest are reasoned from the gate definitions and not isolated by ablation. Expected direction: fewer chance runs. The largest share is probably the mask admit (RMF/AMF lose outright admit) and the DMF reach gate. Smaller: RB reach loss through the widened StyleLb stand-in, and DMF leaving the angle branch. Unchanged: CounterInput sets, keeper gate, FramesToReach keeper stand-in, and the mirror source (same class mapping as the old `RoleClass`).
- Pes-loop move: the same scale corrections, plus StyleRunDepth now reading B (same mapping, so no source move).

## 8. Open items

1. `pessubs::SubRoleClass` is a GF-role copy of the class map (`pessubs.cpp:188`). The stamina-sub reader should read A; not re-based here because it changes sub behaviour on both paths and needs its own smokes.
2. Array A has no GF reader yet (only the stamina-sub reader would use it).
3. `FUN_140500200` also writes `T+0xb8b8` bits for class > 1 (`bts` at `0x5002be`, loop from `0x500276`). GF's `AttackSlotMask` has no class-1 gate. Not ported; the decode is partial.
4. The angle sub-test: PES's veto (run2.c:642) lists `iVar22 < 4` as a disjunct with the angle and distance tests, and GF's angle branch returns "ok" on an angle failure. The polarity was not re-derived here; the pre-existing RankAngleOk structure is unchanged.
5. Swap pair `T+0x89D4/0x89D8` has no decoded writer. `RequestRoleSwap` has no caller, so the tail swap never fires in GF.
6. `MI+0x1775` (restart mode byte) and `MI+0x1554` are PES-host stubs that return 0 (`pesmovestep.cpp`). The write gates are therefore open.
7. Keeper stand-in for `FramesToReach` record byte: not ported (`FUN_140A48910(G,id)+1` writer not decoded).
8. Head-write timing: the registry mirror at the head reads the dynamic roles as of the head; if `CalculateDynamicRoles` runs later in the tick, readers see a one-tick lag. Not measured.
9. Ghidra bridge (`127.0.0.1:8080`) returned "No function found" for `0x140606CD0` and `0x140500200`. Disassembly used `~/ghidra-projects/scratch/Unpacker/pes_full.bin` (offset = VA - 0x140000000).
10. The 4152 thrash (motion re-issued every few ticks t=28399–28427 before the 045 issue, §9) is a second, unaddressed defect; left alone per Main's ruling.

## 9. Restart-path fix: faithful FUN_140a92950 (RoleArrays-on-main, d6cb09b)

On main+RoleArrays three PES-path restart tests failed and the 20-min `--pes-loop` smoke exited 1. The class-5 admit boundary was ruled to stand, so the fix lives in the restart path. Bisect slices (single-line diagnostics, each reverted): A (mirror off) PASS; D (phase frozen) FAIL; E (CounterInput static) FAIL; F (StyleRunDepth frozen) FAIL; G (run-world static) FAIL; H (registry bypass) FAIL; I (admit 5/6/≥7) PASS; J (keeper bit) FAIL; K (admit 5/≥7) PASS. All trajectory perturbations; none identifies a port defect, so the trail moved to the stalled restart itself.

### The stalled restart (measured, temporary probe since deleted)

Seed 7, kind 3 free kick at (-1.90,-14.96), taker A11, armed t=27923, released t=28398 (`MayRelease` fires on schedule; the 355-frame free-kick stand-in). The taker stands 2.40 m off a dead ball facing it (facing matches the to-ball vector to 0.01), then drifts to 7.5 m and never touches the ball; the watch stops the match at 1758 ticks, so "no throw-in was released" is a side effect. At release his controller queues a pass (refused: ball beyond the 2.0 m cheatable radius, humanoid.cpp:1936-1955) plus a walk to the ball (toward it, 7.82 m/s). From t=28399 the body thrashes between the GF idle clip and `idle_0_3_run_000` (motion 4152); at t=28427 the FSM issues motion 2237 (`idle_0_3_run_045_mirror`), which plays 70+ ticks with no requeue and carries him ~90° off the command.

### Root cause

Group B's `FirstAngle` stub (movestep_run.cpp, declared movestep_run.hpp:52 as `FUN_140a92950`) returned its first argument. `EnterAccelerate` arm 3 therefore computed `AngleDiffDeg(bodyDir*0.8, heading)` instead of PES's `a92950(bodyDir, facing, bearingMag*0.8)`. At the pick tick (words logged at the bucket line: turn=-5.93, bodyDir=278.34, facing=278.34, heading=272.40) that yields desired=-49.74 → bucket 6 / column 6 (the 045 cell) for a ~6° turn. PES (`full_1406ccd00.c:155`, VA `0x1406ccd00`): `uVar17 = FUN_140a92950(param_2[4],param_2[2],fVar24 * 0.8)` — the 0.8 blend is the clamp argument: step the body toward the facing by at most the clamp. The helper's own decompile (fa92950.txt, VA `0x140a92950`) confirms: step toward target by at most clamp, [0,360) fold. Faithful result at the frame: `StepHeading(278.34, 278.34, 4.744)` = 278.34 → desired +5.94 → bucket 0 / column 9 (the 000 cell). Call-arg order for the other sites verified in asm: xmm0=req[0x60], xmm1=req[4], xmm2=clamp (`0x1406be2a8` GOTO_RUN, `0x1406bba05` ACCELERATE, `0x1406bcfe5` DECELERATE). The row is right (Animetable dump of the install: row 0x575 col 8 = m4152, col 6 = m2237M); only the column input is wrong. The mirror axis was verified correct by unit test (negates X, keeps Y), so the clip genuinely carries sideways travel.

### Fix (9d31415)

`pes::StepHeading` ports `FUN_140a92950` in place of the stub (movestep_run.{hpp,cpp}); all six call sites pass PES's (heading, target, clamp): GOTO_RUN yaw-lock/desired/speed (`req[1]`, `req[2]`, `req[0x60]`), ACCELERATE/DECELERATE yaw-lock (`req[2]`, `req[1]`, `req[0x60]`), ACCELERATE-entry bucket (`req[4]`, `req[2]`, `bearingMag*0.8`). Stub deleted. No change to the class-5 rule; no timer/tolerance tuning. FirstTouchPort2 cherry-picked it as `ab42b24` for the same mis-bucket in its presser.

### Tests and mutation

- New (red before: no such symbol; green after): `StepHeading.StepsTowardTheTargetByAtMostTheClamp` (stall-frame values), `.WrapsAcrossZeroAndThreeSixty`, `.TheUnclampedStepReachesTheTarget`.
- Kept: `PesClipsInstall.AMirroredLocomotionClipMirrorsRootTravelSideways` (install clip 045 vs its mirror through the real `AnimCollection` path; pins behavior the trace depended on).
- Mutation (clamp ignored): `WrapsAcrossZeroAndThreeSixty` fails; reverted green.
- Full suites on the fixed tree: pes_restart_delay_test 9/9 (all three named failures + the 20-min test), pes_movestep_run_test 47/47, pes_offball_test 65/65, pes_roles_test 6/6, pes_team_concepts_test 31/31, pes_clips_test 8/8.

### Smokes (seed 7, 2 min, rc 0 both)

- Switch-off: `20f28a3df9b0ce12` (32549 ticks) — byte-identical before and after the fix, so the fix moves no GF-path behavior (MOVESTEP ticks only with pes_loop on).
- `--pes-loop`: `4a631d69d07afa76` (44089 ticks, full time) — moved, as expected: the FSM now picks the 000 cell where it picked 045.

### Why switch-off differs from main's a3d5e43a3b7b03af

The GF-path move comes from the RoleArrays port itself, not the a92950 fix (verified inert above). `ProcessRunManager` is not pes_loop-gated, so the re-based mask readers run on the GF path too (`StyleLb` via the no-table stand-in). The changes that move it: `AttackSlotMask` admit (RMF/AMF scored instead of admitted — slices I/K show this is the dominant lever on the pes path, and the same code runs on the GF path), the DMF reach gate (class 3 < 4), the widened `StyleLb` stand-in (RB now counts), DMF leaving the angle branch. The CMF attack-skip measured inert on this smoke (A1: no team at mentality 0). Unchanged on the GF path: `CounterInput` sets (identical by enumeration), keeper gate, `FramesToReach` keeper stand-in, `StyleRunDepth` (pes_loop-gated), registry writes (no GF-path reader except via `RoleClassOfSlot`), `pessubs` (not re-based). Direction matches: chance runs 197-172 → 86-69. Not isolated by GF-path ablation; shares the mechanism bisected on the pes path.
