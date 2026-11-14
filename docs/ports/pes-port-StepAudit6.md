# pes-port StepAudit6 (branch `MoveStepCell`, worktree `<workdrive>/gf-StepAudit3`, local branch `StepAudit5-pred`) — leftovers of `aea2e6ce`

## 1. DECELERATE's rate site — DONE, commit `0f5d53cc` (pushed, `ls-remote` confirms)
Asm over the live Ghidra bridge (`/disassemble_function?address=0x1406bcd20`, saved `gf-StepAudit5-scratch/a_1406bcd20.txt`). The site is 0x1406bd6a9..0x1406bd6d8: `[R14+0x18]` (vt+0x18, the playing motion) -> `FUN_140a6e290`; if clear -> `FUN_140a6e2e0`; if either answers set: `CMP EBP,R12D; JGE skip; MOVSS XMM7,[0x14259d618]` (22.5). EBP is the live frame; R12D is set at 0x1406bcdf4/0x1406bceed and clamped by `CMP R12D,[RSP+0x138]; CMOVG` — the clip's end clamped by hit0, i.e. the port's `hitEnd`; the Ghidra variable `fVar9` was reused at line 335 (ground speed), which is why the decompile misled. XMM7 is the bearing arm's allowance (180, or 90 when the hit carries no offset). Port: `movestep_run.cpp` Arm E, `if (MotionPredicateSet(motion) && liveFrame < int(hitEnd)) turnAllowance = k.deg225;` before the sign damping. Red test `Decelerate.TheBearingArmAllowanceDropsToTwentyTwoAndAHalfForARunningOrHaltedClip` (90 for 0x8bb, 22.5 for 0xc08 and 0xc09; reads the FIRST yaw-lock write via a new `SyntheticHost::firstF0bec`, because later arms overwrite it). Confirmed red with `movestep_run.cpp` swapped back to `aea2e6ce`: that test alone fails (115 pass). Run suite 116 pass, turn 86.

## 2. Mutation runs of the `FacingAdjusterBusy` tests — DONE (all red)
- `movestep_turn.cpp` `FacingAdjusterBusy(host)` held `false`: `RunTurnRun`, `RunTurnStep`, `StepTurnRun`, `StepTurnStep` `.TheReorientWindowStaysShutWhileTheFacingAdjusterIsBusy` all FAIL (82 others pass).
- `movestep_run.cpp` both `reorientClear = true` (ACCELERATE 1641, DECELERATE 1984): `TurnLock.ABusyFacingAdjusterClosesTheLockInAccelerateAndDecelerate` FAILS (115 others pass).
Mutations reverted (file copy back / `git checkout`); no `MUTATION` string remains.

## 3. STEP_LOOP clip-issue A arm — DONE, commit `e619442c` (pushed)
All three ponytails translated from the asm/decompile (`a_1406c4c10.txt`, `d_1406c4c10.c`), plus two defects found on the way:
- **Cell-changed flag and the keep test.** The decompile reuses `bVar19`: at the keep test (`d_1406c4c10.c` 505, asm 0x1406c5940..0x1406c5951 `TEST BL,BL`) it is the cell-changed flag `local_res10` ([RSP+0x178], asm 0x1406c553c..0x1406c559d), NOT the wide-turn band drop `slowTurn` my own `46d6be94` had tested. It is set when the live band (`req[0x1f]`) or live kind (`req[0x1e]`) differs from this tick's, or when both are band 0 / kind 3 and the clip's blend seconds are below the dwell run, and only while the band walk did not apply (`local_res18`). `slowTurn` stays where PES uses it (the column window). So a man whose band or kind changes now always takes the arms below the keep (before, the band never moved on a keep tick).
- **Row, folds, mirror.** The row is read from the kind PES RE-READS after the kind-3 store (the port used the stale kind) and only on the issue path (the folds draw the generator: 0x53a->0x53b 50 %, 0x542->0x543 20 % in the columns 0 and 0x11; 0x52e->0x530 when the bucket moved, the drift was taken or a 50 % draw; command id 5 at band 0 and kind 2/3 forces 0x530). The mirror is the cell's own bit (FUN_1406abd60), not `BucketIsLeft` (the stand-in is gone; the same-clip keep and the issue use it).
- **Blend** (asm 0x1406c5f5d..0x1406c5fbd): 0x1425ddf4c (0.1333) unless `((!cellChanged && !drift) || bucketMoved) && (|req[7]| >= 11.25 || !precise)`, then 0x1425ddf50 (1/6), or 0.2 past 67.5 degrees; the port chose 0.2/0.1333 by look delta alone.
- **Foot start and transition** (0x1406c6076..0x1406c61a6): transition 0 and start `FUN_1406b97d0` when the mirror differs from the playing one, the bucket moved, the drift was taken, the cell did not change, or the column is in the 0x2a015 foot set (then the start is the matching foot record's frame, `(cancel ticks + 1) / tps`, the pair's sides swapped when mirrored); otherwise transition 2 and start 0. The port issued transition 0 always. The rate walk now uses the re-read kind too.
- Not ported (not in Main's list, named here): the kind-4 playback clamp (d_1406c4c10.c 676-690: constants 0x14259bddc / 0x14259bdd8 / 0x142572348 are not StepScalars fields).
Red tests (all three FAIL with `movestep_step.cpp` swapped back to `0f5d53cc`, 85 pass): `StepLoop.ADifferentLiveKindIsACellChangeAndTheTickIsNotKept`, `StepLoop.TheBlendStaysAtTheDefaultWhenTheCellChangedAndTheBodyTurnDrifted`, `StepLoop.ASameMirrorCellChangeBlendsInWithTransitionTwoFromZero`; the keep-tick fixture now sets the live kind/band equal to this tick's (its old premise relied on the wrong flag). Suites on `e619442c`: step 88, run 116, movestep 115, turn 86.
Smokes (seed 7, 2 min): switch-off `a0fbe2d5cd445a52` (baseline); pes-loop **full time `33cd221d6c85de80`, 20 stoppages, longest 23.7 s, 31,327 ticks, idle stalls 0** (was 16 stoppages / 4 stalls on `46d6be94`; main baseline `17941efc16b4a6a5`).

## 4. Oracle proofs (handler_oracle.py rows over PES's own bytes; rows never committed, tests skip without the env var)
Captures were copied to `gf-StepAudit5-scratch/caps` (the live2 dirs hold a root-owned `.capz.lock`, so `capz.py unpack` runs on copies) and unpacked there.
### 4a. STEP_LOOP_CHANGE (`FUN_1406c6480`, hook `stepaudit_1406C6480`) — DONE, commit `cb2f6164`
Rows: 3 real calls (pid3332804 seq346/735/1578) + 450 perturbed (kind, class, buckets, dwells, headings, turn, rate), `--entry 0x1406c6480`, new test `pes_movestep_stepchange_entry_test` (`GF_PES_STEPCHANGE_ORACLE`). 453 cases: **180 finish in the body and match word for word, 273 delegate** to another ported handler (1406d5d60 stop, 1406d38d0, 1406ce820, 1406d2f60, 1406ccd00, 1406ba030): the rows carry those handlers' reads nowhere, so the test proves the port reaches a read it cannot answer (PES made the call, the port must too) and does NOT compare its words past that point. Two PES disagreements found and fixed (both RED on the real call first, then on the perturbed rows):
- **xmm11 is 0.7, not 0.5, after `0x1406c6b48`** (`MOVSS XMM11,[0x14257a194]`): the restart scale (0x1406c6c05 `MULSS XMM7,XMM11`) and the root-facing window (0x1406c6c93 `MULSS XMM1,XMM11`) both use 0.7. The port had 0.5 for both (`k.half`), so the PES write of the root-facing draw came later/never. Fixed with `k.s0p7`.
- **The blended root facing adds `[RSI+0x20]`, the man's own P+0x20 (`host.FacingDeg()`), not req[2]** (0x1406c6c2b `ADDSS XMM6,[RSI+0x20]`). They coincide in a real tick only by construction; 177 perturbed cases separated them.
Unit tests: the four STEP_LOOP_CHANGE tests whose elapsed was 0.5 of the limit now use 0.8 (PES's window is 0.7); the synthetic host has a `facingDeg`; new `StepLoopChange.TheRootFacingWindowIsSevenTenthsOfTheLimitAndTheBlendAddsTheMansOwnFacing` (red on the old code: 251.1 vs 100). `UnboundHost` now throws `UnboundRead` instead of aborting (an uncaught one fails the test with the read's name; the delegating oracle tests catch it); `RowHost::Need` throws too.
### 4b. IDLE_TURN entry (`FUN_1406d1390`, hook `mv_1406D1390`) — DONE, commit `12c0f17b`
The group-A `EnterIdleTurn` was a stand-in (IdleRow picker, the wrong dwell constants 0.1667 vs the bits 0x3e2aaaab, a byte kLiveKind store); it is re-ported from the asm (a_1406d1390.txt): the pre-step (match phase 1, look bucket 4/9, |req[7]| > 168.75: ball sample 20/60 s ahead, heading to it, carrier slot, flip of bucket/error/look heading by 167.5), `FUN_140a92950(req[2], req[3], 167.5)`, the row `0x1425dda30[kind]` in the column `0x1425dd988[bucket]` or the replay row 0x54c (anime 0x4a, or kind 3 with phase 2 or the route flag G+0x25930), `WriteIssue`, words 0x18/0x2d/0x2e (immediate 0x3e2aaaab)/0x38/0x39/0x1e/0x3c. New tables `StepTables::idleTurnRow`, scalars `deg167p5`/`neg167p5`, host read `RouteFlag()` (adapter: `ponytail:` clear until a route writer exists). Rows: 12 real calls (pid3597187) + 1,440 perturbed over request words and the memory the handler reads (phase, route flag, anime state, facing). **1,452 cases pass word for word** (121 reach the pre-step, 516 take the replay row). TURN_STOP's `0x1406cb8e9` call into it (was `return; // ponytail: not translated`) is wired (`TurnStop.AStopThatCannotStayWithTheFacingFarOffExitsThroughTheIdleTurnEntry`).
### 4c. GOTO_RUN update (`FUN_1406bdf10`, hook `mv_1406BDF10`) — HARNESS WIRED, PORT DIVERGES (not fixed)
`tests/pes_movestep_gotorun_test.cpp` (`GF_PES_GOTORUN_ORACLE`) with 6 real rows (seq003/006/009/015/030/031): the port's first read PES did not make is `host.CommandSpeedClass()` (movestep_run.cpp 764, a discarded "no-op class read"), so every real case throws. The function carries named stand-ins (the `FUN_140a92950` call at 0x1406be2a8 "is not there", `MatchPhase() != 0x4a` where PES reads the anime state `vt+0x28 == 0x4a`, the desired direction "not decoded"): a faithful re-port from `d_1406bdf10.c` is the next job; the rows and test are ready for it.
### 4d. TURN_STOP entry widened (`FUN_1406d2590`) — DONE, no port change needed
Re-made the rows from the 12 captures (pid3597187 seq002/012/020/036/044/055/064/074/082/094/102/113), `gf-StepAudit3-scratch/hooks_step.cfg`, `perturb_d2590.json`, 150 perturbed per capture, probes g/phase/m2c0type/m2c0flag/pitch/sub/**slot**/jostle: **1,812 cases (12 real + 1,800 perturbed) pass word for word** on `3cd3dd3d` (`pes_movestep_turnstop_entry_test`, `GF_PES_TURNSTOP_ORACLE`). (The older `rows_ts.txt` predates the slot probe and no longer replays: `RowHost::Need` throws on the unmade `1406ea810` call.)
### 4e. GOTO_RUN edits — commit `3cd3dd3d`
The oracle host (`pes_movestep_gotorun_test`) now answers P+0x245d (probe `prev`), the foot band (`FUN_1406e80b0`, vt+0x78) and the rate ceiling (`FUN_1406eadf0`). Two asm-clear defects fixed in `GotoRun` (movestep_run.cpp): the keeper/replay predicate is `AnimeState() == 0x4a` (vt+0x28), not `MatchPhase() == 0x4a` (twice, 0x1406be3be and the re-entry gate), and the discarded `CommandSpeedClass()` read is gone (PES's `0x1406be4c9` branch reads vt+0x28 and drops it). **The oracle is still red: on real seq003 the port reaches `FootBand()` where PES's flow ends after `FUN_140a6e260` (0x1406be51d).** Known further gaps (from `d_1406bdf10.c` / the rows): the re-entry gate (0x1406be1xx) tests `|FUN_14043a0a0| > 45 || |req[6]| > 45 || byte(req+0x3f)`, the port tests signed `req[6]`/`req[7]` and a mirror word; it compares the clip frame with `hit0` where PES compares with `iVar11` — **Ghidra reuses `iVar11`**: the clip end (`FUN_1406b1d00`) until 0x1406be166 reassigns it to `int(fps*20/60+0.5)` when `req[0x20]` is 0 or 5; the desired-direction `FUN_140a92950` call at 0x1406be2a8 is missing; the stop arm is the `0x1406be51d` `FUN_140a6e260` motion-predicate gate. A faithful re-port from the asm is the next job (rows: `gr_all.txt` = 6 real + 600 perturbed, probes phase/g/prev; the generator is `gf-StepAudit5-scratch/gr_gen.sh`).
### 4f. Handler audit and `HoldsBodyPose`: NOT reached (budget). The audit still stands as listed in StepAudit4 §4.6.

## 5. FINAL STATE (supersedes the interim wording of 4c/4e above) — tip `900df284` on `MoveStepCell`
| item | commit | result |
|---|---|---|
| 1 DECELERATE rate site | `0f5d53cc` | done, red test confirmed |
| 2 FacingAdjusterBusy mutations | — | all red, reverted |
| 3 STEP_LOOP A arm | `e619442c` | 3 red tests, ponytails gone (kind-4 playback clamp named) |
| 4a STEP_LOOP_CHANGE oracle | `cb2f6164` | 453 rows (3 real): 180 word for word, 273 up to a delegate; two PES fixes: xmm11 is 0.7 after 0x1406c6b48 (restart scale, root-facing window), the blend adds P+0x20 not req[2] |
| 4b IDLE_TURN entry oracle | `12c0f17b` | stand-in replaced by the asm port; 1,452 rows (12 real) pass; TURN_STOP's call at 0x1406cb8e9 wired |
| 4c GOTO_RUN oracle | `3cd3dd3d`, `05a9c542` | re-ported from `a_1406bdf10.txt`; 606 rows (6 real): 260 word for word, 346 up to a delegate (the delegated entries are proven by their own rows: ce820 1,452; d2590 1,812; the rest unproven past the entry) |
| 4d TURN_STOP widened | (no change) | 1,812 rows (12 real, 150 perturbed per capture, slot probe) pass |
| 6 HoldsBodyPose | `900df284` | GOTO_RUN joins the hold set (see 5.1) |
Unit suites on the tip: movestep 116, run 114, step 90, turn 86 (wire test not re-run, its known `TheKickoffTakerClosesToTheBall` 0.6695 vs 0.65 is Main's census item).

### 5.1 The pes-loop regression and its cause (Main's rule: find the GF input, do not step back to the old port)
Smokes (seed 7, 2 min, pes-loop): `e619442c` full time `33cd221d6c85de80`, 20 stoppages, idle stalls 0; `12c0f17b` (IdleTurn re-port) full time `f915019cbdd293cc`, 6 stoppages, 8 idle stalls (a taker stood IDLE with the jockey clip while the forced man ping-ponged ACCELERATE/RUN_LOOP/GOTO_RUN); `05a9c542` (GOTO_RUN re-port) hard failure `[restart] stalled: no restart within 1758 ticks` (exit 1). Temporary exit histogram + sampled prints in `GotoRun` (removed): the men sat in GOTO_RUN returning at 0x1406bec55 (`elapsed < local_104`, local_104 = the clip's blend-or-last cancel tick), with command class 3, class word 2, previous band 3 (due = false, tail = 0x1406beb72 ACCELERATE, which PES takes only once the clip frame reaches local_104: 41..59 ticks on motions 2237/2244/2245/2250/3176/4152). **GF input that differs: the body was released at the clip's end** (`HoldsBodyPose` held STOP/ACCELERATE/RUN_TURN_RUN only): PES keeps a finished clip as the current motion until the next issue and GOTO_RUN's decoded exit reads it; on a switched body the frame never reaches the tick. Fix: `HoldsBodyPose` includes GOTO_RUN. Smoke on that code (diag build, same logic): **full time `9b1c3e30a8a2f99c`, 20 stoppages, longest 23.7 s, 32,512 ticks, idle 2 episodes / 2 stalls** (the diag build prints to stderr only). Remaining episode shape: a set-piece taker (piece 3, H11) still at frame 1 of a GOTO_RUN clip (`7<-1`, idle_0_3_run_135) at the 5 s mark and in RUN_TURN_STEP (`13<-4`) at 8 s while an away man is forced from 3.5 m: a long STEP/TURN cascade on the approach, recovered by 10 s; the IDLE family exit to GOTO_RUN (IdleTurn/IdleOther hold) is the next candidate (`HoldsBodyPose` comment names it).
Hash caveat: the final-tip hashes come from the gate's own smokes (below).

### 5.2 Not reached / resume list
- Handler audit of Init, Idle, IdleOther, Accelerate/Decelerate loops, PK idles, TimingStep, Demo (`EnterGotoTurn` mutation run) — not done; the one `HoldsBodyPose` widening decided on evidence is GOTO_RUN. Next widening candidates need their own oracle rows first (IdleTurn handler, STEP_LOOP, TURN_STOP update).
- GOTO_RUN: delegated cases compare only the reach; the yaw-lock write P+0xbec is not compared; `GotoRunPrecise` (movestep_run.cpp ~1232) was not examined.
- STEP_LOOP kind-4 playback clamp (d_1406c4c10.c 676-690) not ported.
- Rows are never committed; capture copies and rows live in `gf-StepAudit5-scratch` (to be deleted at the end).
- Process slips (recorded): `cat >>` appended a test once, `sed -i` once on a build file, `sed -i` on my own scratch script gr_gen.sh; none in tracked sources except the first.

### 5.3 Gate on `900df284` (killed after 2.6 h at pes_throwin_release_test; ctest 2320/2422 done) and the second hold
Gate-run smokes on `900df284`: off `a0fbe2d5cd445a52` (baseline, 10 stoppages, idle stalls 1), pes-loop `9b1c3e30a8a2f99c` (20 stoppages, longest 23.7 s, 32,512 ticks, idle 2 episodes / 2 stalls). ctest failures: `PesForfeitMatch.ARedCardRemovesTheOffenderAndTheSideIsNotForfeited` (full time never reached) and `Loops/GoalSceneHold.TheKickOffWaitsForTheSceneAndThenKeepsItsSpacing/PesLoop` (timeout; re-run directly: a restart taker idles >100 s). Both pass on `cb2f6164` and on `12c0f17b`, and on `900df284` with IDLE_TURN added to the hold (13.6 min and 8.9 min under load 37). **Episode shape:** after a goal or a card the restart taker (piece 3 / 5) stands in IDLE (`1<-1`, `1<-2`, `1<-4`) with a near/mid jockey standby clip while the forced man cycles IDLE_TURN/STEP_LOOP/IDLE at 5 m: a ping-pong IDLE -> IDLE_TURN -> IDLE -> STEP_LOOP. **GF input:** IDLE_TURN's entry (FUN_1406d1390) now issues PES's turn rows (0x1425dda30[kind], column 0x1425dd988[bucket]), whose clips are longer than the old idle rows; the IdleTurn exit (`total <= frame || fixed <= frame`, group A) reads the finished clip, but `HoldsBodyPose` released the body at the clip's end, as it did for GOTO_RUN. Fix `12fa66d8`: IDLE_TURN joins the hold set (comment names that the IdleTurn/IDLE/IDLE_OTHER handlers themselves are not oracle-checked yet; that is the next oracle job). I did not step back to the old entry.
Final gate (below) is on `12fa66d8`.

## 6. FINAL — tip `d88d0e6f` on `MoveStepCell` (supersedes 4c, 4e, 5 above)

### 6.1 Commits (all pushed; `ls-remote` = `d88d0e6f`)
- `0f5d53cc` DECELERATE rate site (item 1). `e619442c` STEP_LOOP A arm (item 3).
- `cb2f6164` STEP_LOOP_CHANGE oracle; two PES fixes: xmm11 is 0.7 after `0x1406c6b48` (restart scale and root-facing window), and the blend adds P+0x20, not req[2].
- `12c0f17b` IDLE_TURN entry (`FUN_1406d1390`) re-ported from its asm; TURN_STOP's call at `0x1406cb8e9` wired.
- `99275c2b`, `3cd3dd3d`, `05a9c542` GOTO_RUN (`FUN_1406bdf10`): harness, edits, asm re-port.
- `900df284` HoldsBodyPose += GOTO_RUN. `12fa66d8` HoldsBodyPose += IDLE_TURN (reverted by `d88d0e6f`).
- `6621e733` IDLE_TURN handler (`FUN_1406bfe70`) from its decompile: the IDLE issue sets the due byte req[0x2f] (never set before, so the IDLE clip was never issued), blend and second dwell 0.2 (were 0.05), `Percent(0x32)` drawn for every kind (was kinds 3 and 6), precise flag as an integer compare (was a float compare, always false), toe-off gate on the adjuster (`FUN_141eb3320`, was `IsReplay`).
- `d88d0e6f` HoldsBodyPose = STOP, ACCELERATE, RUN_TURN_RUN, GOTO_RUN (IDLE_TURN comes off again).

### 6.2 Oracle proofs (rows are PES-derived, never committed; the tests skip without their env var)
- STEP_LOOP_CHANGE (`FUN_1406c6480`): 453 cases, 3 real: 180 word for word, 273 reach a delegate (`pes_movestep_stepchange_entry_test`).
- IDLE_TURN entry (`FUN_1406d1390`): 1,452 cases, 12 real: all word for word (`pes_movestep_idleturn_entry_test`).
- TURN_STOP entry (`FUN_1406d2590`), widened with the slot probe: 1,812 cases, 12 real: all word for word (`pes_movestep_turnstop_entry_test`).
- GOTO_RUN (`FUN_1406bdf10`): 606 cases, 6 real: 260 word for word, 346 reach a delegate (its entries are proven by their own rows) (`pes_movestep_gotorun_test`).
- IDLE_TURN handler (`FUN_1406bfe70`): no oracle rows; fixed from its decompile with red-first unit tests (`TheIdleIssuePastTheFixedTickIsDueWithATwoTenthsBlendAndDwell`).
- HoldsBodyPose: `TheStatesWhoseExitWaitsOnTheFinishedClipHoldTheBody` (`pes_movestep_test`).

### 6.3 The reds of the `12fa66d8` gate, attributed
| red | attribution | at `d88d0e6f` |
|---|---|---|
| `MoveStepWire.APlayedClipAdvancesAndIsNotRestartedByItsOwnMotion` | own: IDLE_TURN hold (`12fa66d8`). Alone on `6621e733`: restarted 1 in 13,776 same-clip ticks | PASS: restarted 0 in 13,656 advanced ticks |
| `PesRestartDelay.EveryStoppageRestartsWithinTheInstallBoundWithPesLoop` (longest 1758 > 1757) and `ARestartTakerPlaysHisBallActionAfterRelease` and `AWhistleLeavesNoFlyingBallBehind` | own: the pes-loop restart stall. `05a9c542` (GOTO_RUN, no hold) stalls; `900df284` (GOTO hold) does not; `12fa66d8` and `6621e733` stall; alone on `6621e733` the three fail | PASS, all three |
| `PesStall.ALiveBallNeverLiesStillLongEnoughForTheBreaker` (the breaker forced a live ball for 7,480 ticks) | own, OPEN (6.4) | FAIL, 7,480 ticks |
| `PesRestartDelay.AReleasedThrowInCarriesItsBallInsideTheTouchline`, `ABallRestingPastTheTouchlineIsJudgedOutWhenTheGateReleases` | in the same binary; pass alone at `d88d0e6f` | PASS |
| `pes_throwin_release_test` (5400 s timeout) | not mine (Main's ruling) | not attributed |
| `PesForfeitMatch.AYellowThenRedStillOnThePitchAtFullTimeForfeitsThreeNil` | known, BLOCKED (ForfeitOrder) | expected red |

### 6.4 Bisect table (pes-loop, seed 7, 2 min unless noted; restart trio = `pes_restart_delay_test`)
| tip | pes-loop smoke | open-play idle episodes | restart trio | APlayed | GoalSceneHold/PesLoop | ARedCard |
|---|---|---|---|---|---|---|
| `e619442c` | full time `33cd221d6c85de80`, 20 stoppages, longest 23.7 s | 0 | — | — | — | — |
| `12c0f17b` | full time `f915019cbdd293cc`, 6 stoppages | 8 | — | — | — | — |
| `05a9c542` | restart stall (exit 1) | — | — | — | — | — |
| `900df284` | full time `9b1c3e30a8a2f99c`, 20 stoppages | 2 | — | — | FAIL (first gate, timeout) | FAIL (first gate) |
| `12fa66d8` | restart stall (gate, rc 1) | — | FAIL 3/3 (gate) | FAIL (gate) | PASS (gate) | PASS (gate) |
| `6621e733` | restart stall (exit 1) | — | FAIL 3/3 (979 s) | FAIL, restarted 1 | — | — |
| E2: `6621e733` with IDLE held (not committed) | restart stall (exit 1), no open-play stalemate line | — | FAIL 3/3 | PASS | PASS | FAIL (exit 1) |
| `d88d0e6f` | full time `810c1f985d5d0fbf`, 26 stoppages, longest 28.1 s (1,516 ticks), 42,390 ticks | 9 | PASS 3/3 | PASS | PASS (256 s) | PASS (941 s) |

Reading: the restart stall and the APlayed step are both the IDLE_TURN hold. The GOTO_RUN stall is the released GOTO_RUN clip (motion -1 in the handler); its hold fixed it. The open-play idle episodes first appear with the IDLE_TURN entry port (`12c0f17b`), and the IDLE hold does not remove them (6.5).

### 6.5 Open, own: open-play idle episodes (`PesStall.ALiveBall…`, smoke idle count 9)
- Shape (the `stalemate-who` lines of the `d88d0e6f` smoke): men in IDLE (state 1, from STEP_LOOP 4 or ACCELERATE) playing AI standby clips (`near_delay_standby_goalline_noloop_mirror`, `mid_jocky_standby_noloop_mirror`) that restart at frame 1 or 2 each tick, while the ball lies at 0.08 m/s; the breaker forces the nearest man.
- Consumer that differs (measured in the code): `humanoid.cpp:310` switches a clip at its last frame unless `PesPoseHeld`, and `humanoid.cpp:435` lets AI Movement commands select clips unless held. IDLE is not held, so the AI's standby clip replaces the FSM's IDLE clip. PES's MOVESTEP owns the body in every MOVE state.
- Holding IDLE (E2) fixes that direction and breaks the other: the restart stalls, with no stalemate line, so the restart taker never walks to his spot. [INFERENCE: in GF the walk is an AI Movement command, which the hold skips at `humanoid.cpp:435`; PES walks through MOVESTEP.]
- Not done: the walk through MOVESTEP (IDLE to GOTO_RUN for a walk target, decoded from the StayDecision inputs), or the AI's chaser assignment (why nobody goes for the loose ball in those episodes; not checked).

### 6.6 Gate on `d88d0e6f`
(to be filled from `gf-StepAudit5-scratch/gate_final.out` when the run ends)

### 6.7 Resume list
1. Open, own (6.5): MOVESTEP-driven walking for IDLE, then the IDLE hold can be tried again; or the chaser assignment. Either removes `PesStall.ALiveBall…`.
2. Handler audit (Init, Idle, IdleOther, Accelerate and Decelerate loops, PK idles, TimingStep, Demo; StepAudit4 §4.6): not done. The IDLE_TURN handler needs its own oracle rows.
3. GOTO_RUN: the 346 delegated cases compare only the reach; the yaw-lock write P+0xbec is not compared; `GotoRunPrecise` (movestep_run.cpp, ~line 1232) was not examined.
4. STEP_LOOP kind-4 playback clamp (d_1406c4c10.c 676-690): not ported (constants 0x14259bddc, 0x14259bdd8, 0x142572348 are not StepScalars fields).
5. Batch-BK notes (carried from StepAudit5, not re-verified here): the RootMotion merge scratch (`StepAudit5-bk`, local only) gave switch-off `4952d70f32bce206` and pes-loop `0de822c8a16dd356` with 17 idle stalls before the A-arm re-port. Seed 1003 census item: the kick-off taker margin (TrapContact2's gap (b)); not touched here.
6. Scratch to delete at the end: `gf-StepAudit5-scratch` (bins, logs, rows, captures).

### 6.8 Process notes
- Edit-tool slips: the `cat >>` append and one `sed -i` on a build file (sources were not affected beyond those); a bad anchor left a stray comment line, found and removed before the commit.
- The forfeit test, `pesworldrecord.*`, and RootMotion's files were not touched.

## 7. Breaker red `PesStall.ALiveBallNeverLiesStillLongEnoughForTheBreaker`: root-cause trace (OPEN, no fix committed)

Tip unchanged (`d88d0e6f`). Diagnostics ran in the detached worktree `gf-StepAudit5-dev` (patch kept at `gf-StepAudit5-scratch/idle_walk_diag.patch`, reverted; not committed).

### 7.1 What the stall is (measured, seed 7, pes-loop, 2 min, 42,390 ticks)
- 106 `[stalemate]` lines. Every episode is a restart that was already released and taken or abandoned: `setPiece 0`, `inPlay 1`, `buf active 0 started 1`, the ball lies at 0.08 m/s, and the breaker forces the nearest man.
- The stale `started` buffer is NOT the cause: a take at `Referee` (line ~747, `TouchAnim && !TouchPending`) fires on real contacts (pass/highpass/throw clips with the taker within about 1 m of the ball, 19 takes logged). It leaves `taker` and `desiredSetPiece` set, which only the breaker reads.
- The real stall: the nearest man is IDLE (MOVESTEP state 1) with a live Movement command (`fn 1`, speed 2.2-3.8 m/s), 10-11 m from the ball, and does not walk. Per-tick trace of A5 (`[idle-walk-diag]`, t=380.2-380.9 s):
  - Cycle of 3 ticks: IDLE (1), liveKind word 2, then STEP_LOOP (4) with liveKind 0 and dwell limit 0.15, then IDLE again with liveKind 2 and dwell 0.00/0.03. Some entries pass through IDLE_TURN (2) with dwell 0.20.
  - IDLE re-issues its clip whenever `U(kLiveKind) != kind` (kind 0) after the dwell gate; the clip restarts at frame 1-2 each cycle. This matches the stall clips.
- Ruled out, measured: (a) cmd speed class -1 (only 11 walk-command IDLE samples, all at the ball); (b) the carry: the dwell and live-kind words are in the carried range (req 0x17..0x2e, indices 0x1a/0x1b/0x1e/0x2d/0x2e); (c) the take rule (see above).

### 7.2 Open point (the next step)
- Which path moves IDLE to STEP_LOOP and back. The ping-pong is between `Idle` (movestep_group_a.cpp ~309) and the STEP_LOOP entry (movestep_group_a.cpp ~298 / movestep_run.cpp `EnterGotoRun`, movestep_run.cpp ~708), and the IDLE-turn entry (`EnterIdleTurn`). PES reference: `FUN_1406bf0c0` (IDLE, `d_1406bf0c0.c`), `FUN_1406b6bb0` (StayDecision, read in full: returns stay only when the move target is farther than the threshold AND the bearing gate passes), `FUN_1407824A0` (MOVE action Update: tries commands 0x52/0x53 through `FUN_1406b4580`, otherwise `FUN_1406b8340`).
- Next experiment: trace the request words and the handler taken at each tick for A5 (which handler returns, which `RequestState` fires), then compare the IDLE exit set with PES's (IDLE: `FUN_1406bf0c0`'s exits). Red test: a man in IDLE with a live walk command and a liveKind different from his kind must not re-issue the IDLE clip each cycle; to be written once the exit is found.
- Status: OPEN and own. Not left as an expected red: the fix is not in, so the red stands on `d88d0e6f` until this lands. No hold widening (E2 showed that stalls the restart).

### 7.3 Gate on `d88d0e6f`
- Started ~08:00 (14-11). Smokes done: `smoke-off rc=0`, `smoke-pesloop rc=0` (`gate_final.out`). ctest ran 2423 tests at 10:20 and is still running; no verdict yet. Read `gf-StepAudit5-scratch/gate_final.out` and `done_gate` (`exit=N`) for the verdict.
- Known expected red: `PesForfeitMatch.AYellowThenRed…` (BLOCKED, ForfeitOrder). Own red: `PesStall.ALiveBall…` (7.2). Not mine: `pes_throwin_release_test`.
