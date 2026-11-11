# RootMotion2 (branch RootMotion, worktrees gf-RootMotion2 / scratch gf-RootMotion3): the red attribution, the keeper/kickoff root, the strike adjuster that never kept its state, and the remaining adjuster proofs (11-11-26)

Continues `pes-port-RootMotion.md`. Image `pes_full.bin`. CONFIRMED = a tool run this session; INF = inference; OPEN = blocker named.

## 1. Pushed
`RootMotion` = `f84b1d99` (confirmed by `git ls-remote origin RootMotion`); `c0e69168` is the TrapContact merge, `f84b1d99` adds the 35A0/56D0 proofs, specs, layer and ledger (the tests below ran on c0e69168; f84b1d99 changes only generated files and tools). Commits since `d2a3d5b6`: `2dd6d027` (movement requeue), `93f5f144`... (below), `c0e69168` (merge of `origin/TrapContact` `fa72cb39`).

## 2. Attribution (CONFIRMED)
Base = `TeamTick` `0c3fdaac` (gate `GATE_TIER=branch` in `gf-TeamTick`, own build; the machine ran at load 40-90 with 4 agents, so every 1800 s ctest timeout in it is load, not a verdict).
| test | base | RootMotion d2a3d5b6 | now |
|---|---|---|---|
| `MoveStepWire.ADesignatedKeeperNextToABallAtRestWalksToIt` | RED (nearest 2.3056 m, the keeper never moves) | RED (same 2.3056) | **PASS** (2dd6d027) |
| `MoveStepWire.TheKickoffTakerClosesToTheBall` | RED (closest 0.83 m) | RED (0.6517 vs 0.65) | **PASS** (closest 0.30 m) |
| smoke-off seed 7 | `4952d70f32bce206`, 3 stalls (main `a0fbe2d5cd445a52`, 0) | identical `4952d70f32bce206`, 3 stalls | base-owned (the pes_loop-off path runs no RootMotion code) |
| smoke pes-loop seed 7 | `604236dcaa158970`, 6 stalls | `00a796faee1e21e7`, 12 stalls | `75d3f582c82bcbbc`, **6 stalls** (tip 2dd6d027) |
| `PesPassTarget...NeverStale...` | RED 7.56 s | RED 8.19 s | RED 9.13 s (2dd6d027), **11.07 s at the final tip c0e69168** (5 episodes); 19.28 s on the BJ-merged scratch tree (section 4) |
| `PesForfeitMatch.*` x3, `diff_translated_drift`, `pes_throwin_release_test` | ctest timeouts at 1800/5400 s under load | aborts / timeouts `PesForfeitMatch` all PASS at c0e69168 (`pes_forfeit_test` 9/9) |
| `diff_impure_recorded` | FAILED on the base gate (208 s) | not in d2a3d5b6's red list | not investigated |

## 3. The keeper/kickoff root: a GF requeue replaced the PES locomotion clip (CONFIRMED by probe)
Probe (temporary, reverted) on `ADesignatedKeeper...`: the keeper issued motion 0x8bf (`idle_0_3_run_090`, a 90-degree turn-run start) EVERY SECOND TICK (`serial` +4 per 8 ticks) with `PlayIssuedClip` returning true, yet the body showed GF's own `movement/idle/090_accel.anim_mirror` at frame 0 at every sample, state 7 (GOTO_RUN) for good. Cause: `Humanoid::Process` requeues (`humanoid.cpp` ~333-399): a designated man within 3 m of the ball requeues every second tick (`RequeueOnFrame(frame, id, 2, 1)`), `IsMovementClip()` is true for a PES locomotion clip, and `SelectAnim`'s ReQueue block (line ~2066) exempts `OnPesLocomotion()` from every refusal, so GF's AI Movement command picked a GF idle-accel clip over the clip the FSM had just issued; the FSM's next tick (state 7 waits for the clip it issued; CurrentMotion was -1) issued it again at frame 0. The kickoff taker is the same shape.
Fix (one condition, `humanoid.cpp`): in the ReQueue block, `if (OnPesLocomotion() && command.desiredFunctionType == e_FunctionType_Movement) return false;`: PES has no mover but MOVESTEP for the Move action. Probe after: keeper `idle_0_3_run_090` plays frames 5, 13, 21, 29 and the facing swings 160 -> 258 degrees in 16 ticks (6 deg/tick: PES's turn rate, now with the translated adjuster) and he reaches the ball. Mutation = the base: both tests red without the line (base run), green with it. Smoke pes-loop seed 7: 12 -> 6 stalls.

## 4. Stale target (OPEN, the next root named with evidence)
- Tip 2dd6d027: 9.13 s; `stalemate` episodes now print each man's distance to the ball and speed (`match.cpp`, committed). All six: ball 0.08 m/s in play, the forced man 0.5-1.3 m from it playing GOTO_RUN/ACCELERATE on `idle_0_3_run_*` turn or accel clips, or a dodge/slide, and nobody trapping; the "taker" the line names is the stale `RefereeBuffer` of the last restart (4-44 m away): these are not restart-taker stalls.
- BJ-merged scratch (`2dd6d027` + `origin/MergeBatchBJ` `6f2f78d0`, worktree gf-RootMotion3, not pushed): `pes_pass_target_test` **19.28 s**, `PesStall.ALiveBallNeverLiesStillLongEnoughForTheBreaker` **RED (2418)** (the other 8 PesStall tests pass, including `AHolderIsNeverHandedANewTrap`, `NoTrapStartsFromAnActionGate4aRefuses`, `ATrapTouchIsTheBodyScansGrantAndNothingElse`, `ATrapReachesABallAtRest`). The BJ episodes are TrapContact3's shape: the man 0.8 m from the ball plays `trap_0_0_000_y0_in` (frames 21 and 66 of 121), the ball stays at rest 0.08 m/s, nobody dribbles it afterwards; evidence sent to `agent://TrapContact2` (dribble tick `0x1407B37D0`). So the remaining stale target is TrapAdmit/TrapContact's, not a body-turn problem.

## 5. PesForfeit aborts and pes_restart_delay_test
`PesForfeitMatch.ASideAtTheMinimumKeepsPlaying` aborted on d2a3d5b6 with `PES_DISPATCH_UNTRANSLATED: 0x141eb5f00` (the non-joint reader of the reach test `FUN_140799060`, the closure of my merged TrapContact `bcdf05e8` held it as an out-of-set call). TrapContact2's `fa72cb39` adds it to the closure; merged into RootMotion (`c0e69168`). Not my code.

## 5b. Final-tip runs (CONFIRMED, c0e69168, load 15-30)
`pes_movestep_wire_test` (MoveStepWire.*, RootMotion.*) 36/36 PASS; `pes_forfeit_test` 9/9 PASS; `pes_movestep_test` 121, `_run_test` 110, `_step_test` 75, `_turn_test` 82 PASS (`_turn_entry_test` runs 0 without captures); `pes_pass_target_test`: `EveryPassInFlightHasItsOwnSidesReceiver` PASS, the stale target RED 11.07 s. `pes_restart_delay_test` (PesStall.* / PesRestartDelay.*) was still running when I stopped: no verdict (its earlier PesStall.* run on the BJ scratch tree is in section 4).

## 6. Gap (a): the strike adjuster never kept its state (CONFIRMED by probe, FIXED)
Probe on the BJ scratch tree (temporary, `[arm]` per tick for every man in the Trap action or with A armed; seed 7 pes-loop, 3,856 lines): the trap start DOES arm A (`FUN_141EB39A0`: `ticks` 14-17, `mode` 1) and A steps once (`cnt` 2, `moved=1`), and the next tick `A.state = 0` with `cnt=2 ticks=16 mode=1` left over: nothing finished it. `max cnt` over the whole run is 2. The writer: `WriteActorP` stored `PesActorFacts::kickApproach` (0) into `P+0x430` every tick, and `P+0x430` IS adjuster A's state word (A = P+0x428, +8): GF's old model of "the kick approach is never armed" zeroed the state the trap start had just armed (WritePesTrapActors runs before the trap tick each tick). So the root displacement of TrapContact v1 never applied beyond one tick. Fix (`93f5f144`): `kickApproach` is an `std::optional` oracle input; the match leaves the word to the adjuster (`pestrapactor.cpp/.hpp`, `teamAIcontroller.cpp`); the oracle tests that set it still write it. The stale-target worst case moved 9.13 s -> 11.07 s with the fix on (the trap now walks the man onto the predicted contact point, and the touch is granted or refused on the real geometry); whether the contact gap itself closed is NOT measured (no per-contact gap print on this tree).

## 7. Proofs added (prove_bounded, results_trapaction.json)
| function | cases | verdict |
|---|---|---|
| 0x141EB3910 mode-2 arm (caller 0x14071AF00, FUN_14071AB70) | 1 REAL (live2/pid4152866 `rm_mode2` seq63012) | 1/1, own block 2/2 |
| 0x141EB35A0 yaw arm | 12 SYNTHETIC over the real yaw capture seq006 entered at 0x141EB571C: plain, `to` >= 360, `to` < 0, `from` < 0, `from` >= 360, error above +180 and below -180, override flag with both error signs, byte flags 5/6, 0 and 1 tick | 12/12, own block 21/21 |
| 0x141EB56D0 root-facing write (r8 = 1) | 5 SYNTHETIC (angles 10, 359.5, 180, 0, 26.6) | 5/5, own block 5/8 (the r8 = 0 path, which calls the anime engine, is not entered) |
| 0x141EB3AF0 wrapper, A strike steps | 6 more REAL (pid4152866 seq13560, 14751, 16889, 35275, 36132, 53384; strike mode 1, counters up to 9 of 14) run WITHOUT --merge | 6/6, own block 10/13 (added to the earlier 12 cases in `pes-port-RootMotion.md`) |
Synthetic specs are `synth_specs/yaw_arm.json`, `root_facing.json` (committed). Hooks: `rm_yawarm` landed 0 captures in two launches (predicate `d(r9)>1` is right: r9d is the 4th integer-class argument, `ticks`; the starvation is `EVERY 20` sampling while 56D0's one-tick calls dominate), `rm_mode2` 1. The synthetic proof covers 35A0 completely (21/21), so both hooks can be dropped. FUN_141EB26F0 (mode 3) and FUN_141EB2450: no .pdata function, no hook; 2450 runs inside the wrapper proof; 26F0 is never armed in GF (no ported handler writes the tail word that calls it).

## 8. Mutations of the turn test (CONFIRMED runs in gf-RootMotion3 with a temporary env switch, reverted)
`RootMotion.ATurnTheYawAdjusterIsArmedWithTurnsTheManByItsErrorOverItsTicks` (control: PASS, 62-67 s): (1) `TurnRoot(+turn)` instead of `-turn`: FAIL (median 0.157, 1432 turns); (2) skip `ApplyBodyTail`: FAIL ("no yaw adjuster was ever armed"); (3) the absolute yaw applied as a rate: FAIL (median 0.49, 1395 turns). The two keeper/kickoff tests are their own mutation (the base).

## 9. Left, numbered
1. `GATE_TIER=branch` on the final tip once the machine is quiet (the gates run here were load-confounded: 1800 s timeouts everywhere including the base). Expected reds: the stale target and the breaker (TrapContact3's dribble), `diff_impure_recorded` (base), `pes_throwin_release_test` (base timeout).
2. Re-measure the trap contact gap with the adjuster now holding its state (section 6).
3. Render check (a Godot frame of a turning man, examined): NOT done.
4. The `P+0x2FC` position ring writer (Main's ask): the second scan (stride-12 clusters over all code, disp 0x2FC..0x343) found no function that rotates a 6 x vec3 array at that base inside the P class (0x1406E0000..0x140706000) or the anime layer; the hits at 0x1406DFA00 / 0x1406E1300 are `lea` of the P constructor's sub-objects. OPEN: probe `FUN_141EB50B0`'s index >= 2 readers' captured P (the ring cells) over consecutive captures to see the cadence, then find the writer by write-watch in the emulator.
5. Drop the `rm_yawarm`/`rm_mode2` hooks (proven); `rm_adjwrap` already dropped.
6. Delete `gf-RootMotion-scratch` (logs only now; the synthetic states are deleted), worktrees `gf-RootMotion2/3`.
