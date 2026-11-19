# pes-port StepAudit4 — movestep handlers (branch `MoveStepCell`)

Tip: `ddfd6db9` (pushed `MoveStepCell`, verified `git ls-remote`). Worktree `<workdrive>/gf-StepAudit3`, build `<workdrive>/gf-StepAudit3/build` (Release, ccache), scratch `<workdrive>/gf-StepAudit3-scratch`. Prior report StepAudit3 covers `7954923d`; this file replaces it. SED was never used on repo sources (`run_oracle.sh` in scratch got one `sed -i` probe-line append). `CMakeLists.txt` gained `src/pes/movestep_issue.cpp` via an early `sed -i` (disclosed, StepAudit3 item).

## 1. Done this session

- **EnterGotoRun re-port (`src/pes/movestep_run.cpp`)** from the asm `a_1406ce820.txt`/`dis_ce820.txt` (tip `aff4639f`). Fixed invented arms (0x5c9/0x5cc gone), byte-vs-word 0x3d/0x3e/0x3f reads, jostle COLUMN vs kind, `AngleBuckets().centre` table, precise path with weight² blends, tail word writes. Ponytails cite their PES sites (anchor heading, pitch limit, jostle target, anime record floats, predicted velocity, action sub).
- **Oracle, FUN_1406ce820: 1,452 rows green** (tip `a446a5bc`): 12 real + perturbed over `caps/pid3597187_*` (12 captures), all call kinds exercised (`--ret-dump` at record windows, probes incl. `sub`, `pitch`, `jostle`). `RowHost` shared by turn-entry and goto-entry tests (`tests/movestep_row_host.hpp`); `ActionIndex` answers probe `slot` (rcx+0x2458).
- **Oracle harness fix**: `--entry` was decorative — `run_recorded` always emulated the recorded call's entry, so the first d2590 batch silently re-ran ce820 (rows_ts.txt, withdrawn). `frame_trace.run_recorded(..., start=...)` now starts at the override entry with the recorded registers/memory; `handler_oracle.py` passes `start=self.entry`.
- **EnterTurnStop re-port (`src/pes/movestep_step.cpp`)** from `d_1406d2590.c` + call-site asm (`--sites` list). Fixed: UNSIGNED `bucket-3<2||bucket-8<2` gate (old signed test admitted buckets 0..2); flip reads the LEFT bucket (`uVar9` before overwrite); missing `0x8b5..0x8b9` motion group; `PickCell` motion-0 default at `movestep_issue.cpp` (was `-1`); `0xe2d` arm mirror via joints 3/6 `PositionAt` + shared `RotateXZ`; `FUN_1405aef10` plain-difference body math (new local `PlainDiffDeg`); heading clamp vs `rk.deg1125`/`rk.deg225`; history words `kBucketPrev=old kLiveBucket`, `kLiveBucket=bucket`; `MoveHost::BodyDeg` (P+0x28) added, adapter bound to bodyDirectionVec (ponytail).
- **RotateXZ shared** (`movestep.cpp`/`movestep.hpp`, table-sine version); local RunConstants copy in `movestep_run.cpp` removed.
- **Red tests** in `pes_movestep_step_test.cpp` (all passing): unsigned-gate exclusion, mirrored-pair flip, live-bucket tail words, empty-cell motion 0 (replaces unfaithful `-1` test — `neg r9b; sbb r10w,r10w` + empty exits leave eax=0 in `ck_empty.py` disasm of `FUN_1406abb30`).
- **Turn-stop oracle: 156 rows green** (`rows_ts2.txt`: 12 real + 12×12 perturbed): request words incl. kind fold, stop row/mirror/blend, heading stamp, history words. Needs slot probe (`slot=rcx+0x2458:1`) — rows_ts/rows_ts2 lack it, covered by ctx fallback for now.
- **Test bindings**: `BodyDeg` on `RateHost` (stub 0.0), `FakeHost` (step), `RowHost`/`TurnStopHost` (ctx P+0x28), `UnboundHost` loud stub.
- **Suites**: movestep_test 115, run 110, turn 82, step 79 — all pass.

## 2. Smokes on `ddfd6db9` + scratch merge (22:14–00:12)

- Switch-off smoke: GREEN, `a0fbe2d5cd445a52` (= main). 2-min seed-7, `smoke_off.log`: 5-3 goals, `[idle] 10 episodes, stalls 1`.
- Pes-loop smoke on `ddfd6db9`: RED — RC=1, `[restart] stalled: no restart within 1758 ticks (limit 1757 ticks)`. Log ends 54 lines after `Done creating match!`: the match never starts (opening-restart failure, pre-kickoff — not the old mid-match stall).
- Pes-loop smoke on the RootMotion scratch merge (`9a483337`, since dropped): IDENTICAL stall, RC=1, same 1758 ticks. Requeue shape KILLED (`2dd6d027` present there). The RM log shows the stall clusters on goal kick, taker H1 (keeper): `piece=2 taker H1`, `movestep 1<-17` (Idle <- IdleOther), clip `gkmovelow_guard_0_0[_mirror]`, 0.0–0.2 m/s, 40–49 m from the ball. Keeper idles in state 1 while `MayRelease(GoalKick)` waits on `StepGoalKickLadder` ← `PickGoalKickMate`/`PickGoalKickReceiver`. `2dd6d027` ancestry on `ddfd6db9`: NOT ancestor.
- Wire bisection on the scratch merge (all 42 TESTs, `timeout 300` each): 41 PASS, 1 FAIL. The earlier 1800 s hang was load (40+ with TeamTick gate + TrapContact long test), not a wedged test — each TEST runs 30–60 s alone. The red: `TheKickoffTakerClosesToTheBall`, closest 0.6695 m vs 0.65 bound (seed 1003, staged in `a8cc8f5b`); 2 cm margin — the requeue guard reshaped the approach. Re-stage by the test's own seed census (1000–1011), do not weaken the bound. Old STOP red `AStopOnATurnedBodyLeavesTheStop` PASSES on the merge.

## 3. Goal-kick ladder suspects (for the restart stall)

- (a) Ladder never releases: frame math (`frames(fps·seconds+rounding)`, `GetSimFrame`=`iterations`, `kPesTickHz`=54) vs `in.lofted` always -1 (`PickGoalKickReceiver` over `technical_highpass` ratings) — check phase/elapsed per tick with a temporary DIAG.
- (b) `PrepareGoalKickStand`/`pieceHeld` holds the keeper while the ladder needs him walking; `IsPieceTakerHeld` in `ProcessMoveStep` freezes his FSM in Idle (`1<-17`).
- (c) `PrepareSetPiece(GoalKick)` stand placement vs kicker reach.
- TrapContact2 owns gap (b) (GF-clip men, motion 0xFFFF) on `ClipOwner`: hand over the kickoff-taker margin if their census touches seed 1003.

## 4. Resume list (numbered)

1. **Goal-kick ladder DIAG** (temporary, remove before commit): phase/elapsed/`in.lofted` per tick on one goal-kick stoppage + `SetPieceTakerConstants` window values + stand placement. Then fix and re-smoke.
2. **Kickoff-taker re-stage**: seed census 1000–1011 per the test comment.
3. **STEP_LOOP keep-tick tail re-port** (`a_1406c4c10.txt` 0x1406c5a05..0x1406c5d3f; decompile `d_1406c4c10.c` 786-893) + `gateA` cluster fix (4th term `distance<=4` is wrong; PES compares `|headTurn|<=112.5`, DAT_1425cb30c — asm `1406c55d2`/`MOVSS XMM6,[0x1425cb30c]`).
4. **Red tests for turn-entry call sites, EnterTurnStop, keep-tick tail** (partially done for EnterTurnStop; sites + tail remain).
5. **Oracle proofs**: TURN_STOP (widen to 150×12 with slot probe); STEP_LOOP_CHANGE (hook `stepaudit_1406C6480`); IdleTurn entry (`mv_1406D1390`); IdleGotoRun (`mv_1406BDF10`); `FUN_1406ce820` (done, 1452).
6. **Handler audit**: Init, Idle, IdleTurn, IdleOther, Accelerate/Decelerate loops, PK idles, TimingStep, Demo (`EnterGotoTurn` mutation run; `HoldsBodyPose` widen one state at a time).
7. **Final `GATE_TIER=branch` on the final tip** + report update with hashes.
