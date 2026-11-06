# PES port: MoveStepCell (branch MoveStepCell): failed MOVESTEP issues were one wrong selector call; the stuck RUN_TURN_RUN man was a mis-ported handler; PES's empty-cell answer is 0, not -1 (CHECKPOINT, 05-11-26)

Branch `MoveStepCell` (worktree gf-MoveStepCell, scratch gf-MoveStepCell-scratch), from main 8fa4314e. Image `pes_full.bin`. CONFIRMED = a tool run this session (decompile, asm, translated oracle, test); INF = inference; OPEN = blocker named.

Branch `MoveStepCell` (worktree gf-MoveStepCell, scratch gf-MoveStepCell-scratch), from main 8fa4314e, merged with origin/main ebbc3741 (clean). Pushed: tip `0e7dfcc9` (`git ls-remote origin MoveStepCell`). Image `pes_full.bin`. CONFIRMED = a tool run this session (decompile, asm, translated oracle, test); INF = inference; OPEN = blocker named. **NOT FINISHED: the branch gate did not complete and the pes-loop smoke shows two stalls (section 6).**

| item | state |
|---|---|
| 1. reproduce, red tests | DONE. Main, seed 7 pes-loop, 2 min: **451 issues with motion -1, 2,281 cell lookups that found no cell** (state at issue 3: 154, 4: 134, 16: 70, 5: 28, 15: 22, 12: 11, 7: 6; every one with the row from the stop table, column 1/3/5/7). Red tests: `MoveStepWire.NoMoveStepIssueRequestsNoMotion` (21 no-motion issues in 1500 ticks on main), `MoveStepWire.NoManSitsInRunTurnRunOnAClipPesDoesNotNumber` (a man 2,755 ticks in state 12), `EnterStop.TheRunStopArmReadsTheTurnStopRowAtTheBucketsColumn` (unit). |
| 2. why -1 | DONE, section 1 and 2. |
| 3. root fix | DONE for the -1 issues (section 1) and for the RUN_TURN_RUN exit (section 3). |
| 4. ablation, drifted seeds, gate | see section 6 (filled when run). |

## 1. The failed issues: one call site, two wrong inputs (CONFIRMED)

Backtrace of every `Issue` with motion -1 over the seed-7 run (dladdr offsets mapped through `nm`): **140 of 140 came from `pes::movestep::EnterStop` (the STOP entry, FUN_1406D5D60), run-stop arm**, reached from STEP_LOOP, RUN_LOOP, RUN_TURN_STEP, STEP_LOOP_CHANGE, RUN_TURN_RUN, STEP_TURN_RUN, GOTO_RUN.

PES (decompile `FUN_1406d5d60` lines 184-199; vtable `0x1425E35B0`, slot +0xE0 read from the image = `0x1406BA0D0`): the run-stop arm calls the +0xAC0 interface's **vt+0xE0 = FUN_1406BA0D0(out, &class, &kind, &bucket, 0)**, which reads the row from **DAT_1425DDB50[kind + class*8]** (the turn-stop table) and the column from **DAT_1425DD988[bucket]** (the 10-entry bucket-to-column table: 9,11,13,15,17,8,6,4,2,0), then FUN_1406ABB30 / FUN_1406ABD60. The port called **StopRowFor** (DAT_1425DDAD0, the FUN_1406B9D80 table, which EnterTurnStop uses through vt+0xD0) and passed the **raw bucket (0..9) as the column**. Row 0x582 has no cell at columns 1, 3, 5, 7, so the odd buckets found nothing and the request carried motion -1. The comment at the call said "the stop row (FUN_1406b9d80)"; its own doc (pes-re-runturn-picks.md 5d) names vt+0xD0 for EnterTurnStop only.

Fix (`movestep_run.cpp` EnterStop): `TurnStopRowFor(kind(8->2), class clamp 0..2)` and `bucketColumn[bucket]`. After it, **0 issues with motion -1** in the 1500-tick check (was 21).

## 2. What PES does with an empty cell (CONFIRMED, corrects pes-re-runturn-picks.md 3 and pes-re-animselect.md)

`FUN_1406ABB30(table, &row, &col, flag)` (raw asm `0x1406ABB30`): `NEG R9B; ... SBB R10W,R10W` makes R10W = **0xFFFF when the 4th argument is non-zero, 0 when it is zero**, and that is the answer for an empty or out-of-range cell. Every one of its 80+ callers I checked (script over the `xrefs_to` list: `xor r9d,r9d` before the call) passes **0**, so PES's empty-cell answer is **motion 0**, not -1 (the docs' "-1" holds only for flag != 0, which no caller uses). `FUN_1406ABDD0` (populated?) is the separate -1/0 test the entries use first. The request recorder `FUN_1406D8620` (vt+0x10) stores motion/mirror unchanged (motion >= 0x1125 zeroes its clip-length words); **the tail of FUN_1406B8340 (decompile lines 236-248) issues the single-clip SetMotion only when `-1 < motion`**, so a motion of -1 is never played in PES. GF's `PlayIssuedClip(-1)` returning false matches that outcome; the defect was upstream (section 1). OPEN: the adapter's `Cell` still answers -1 for an empty cell where PES's flag-0 lookup answers 0; with section 1 no site issues an empty cell in the 1500-tick check, so the difference is latent (one `found ? motion : 0` in `MoveHostAdapter::Cell` once a site that reaches it is shown).

## 3. The stuck man: RUN_TURN_RUN was mis-ported (CONFIRMED by asm), plus two input errors

Trace of the stuck men (state 12, `prev 3`): the turn clip runs, ends, the humanoid hands the body to GF's own movement clip (`HoldsBodyPose` covered only STOP and ACCELERATE), `CurrentMotion` becomes -1 and the handler never meets an exit; and even with the clip held at its last frame (hold widened to the turn states) the handler kept the man there for 1,899 ticks: its exits were never reached.

Read against the full instruction stream of FUN_1406C14C0 (`0x1406C14C0..0x1406C2E6A`, 1,603 lines of asm), the old port differed in: **E = min(row end, fixed frames)** (CMOVG at 0x1406C16B9; the port took the max), the three draws' arguments (asm 0x1406C1957..0x1406C19AF: req[0x18]/req[1], req[0x18]/req[3], req[2]/req[3]), the replay flag, the look distance word (req[0x12], not 0x11), the reorient window's step clamp (22.5, not 67.5) and its stores (the turn-lock words 0x3c..0x3f and 0x18), the damping tail's gate (90, not 45) and limit table, the ladder's exits to the run (`FUN_1406D2030` through vt+0 after the class/rate/speed from vt+0x38 / 0x68 / 0x48), and the clip predicates `FUN_140A6E290` / `FUN_140A6E2E0`, which are pure tests of the motion id and were stubbed false. RUN_TURN_RUN is **re-ported from the stream** (`movestep_turn.cpp`), with the ground speed taken from the displacement per tick:

- **`PositionAt(0/1)` fed GF's movement in metres per second where PES reads metres per tick** (every reader multiplies it by the tick rate): ground speeds were 54 times too high (a keeper turning at 1.22 m/s read 237 km/h). Fixed in `MoveHostAdapter::PositionAt` (divide by the tick rate).
- **Word indices:** the group-D port used byte offsets as word indices for the foot flag (0x98 for word 0x26), the slip words (0xe8/0xec/0xf0..0xfc/0x60 for words 0x3a/0x3b/0x3c..0x3f/0x18), `req[0x6c]/[0xb8]/[0x140]` (words 0x1b/0x2e/0x50). `kFootFlag`, `kSlipActive/Angle`, `kSlip*`, `kClampWord` now name the PES words; the tail's special case for SLIPDOWN is gone (same words as the root-facing write).
- `IsRunningMotion` / `IsHaltedMotion` (FUN_140A6E290 / FUN_140A6E2E0) hand-ported; the translated functions are the oracle in `MoveStepWire.TheMotionSetPredicatesAgreeWithTheTranslatedFunctions` (all 65,536 motion ids).
- `HoldsBodyPose` is STOP, ACCELERATE and **RUN_TURN_RUN** (PES's anime player keeps a finished clip as the current motion until the next issue, and RUN_TURN_RUN now exits from it). Widening it to the other turn states (DECELERATE, TURN_STOP, SLIPDOWN, RUN_TURN_STEP, STEP_TURN_RUN, STEP_TURN_STEP) froze a man on a finished clip for 2,500 ticks (their ports do not exit reliably), so it was NOT kept.

Stand-ins left (each a `ponytail:` naming its PES site): `FacingAdjusterBusy` (FUN_141EB3320 on P+0x4d0, AnimeLayer's), `ForwardDisplacementKmh` uses `std::sin` for FUN_140A92C80's table sine, `ClipFading` (FUN_140A6E4F0, other handlers), the roster reads of the turn lock (`PlayerRowId/FlagD`).

## 4. Tests

Run (all green on the tip before the merge): `pes_movestep_turn_test` 56, `pes_movestep_run_test` 102, `pes_movestep_step_test` 56, `pes_movestep_test` 115, `pes_movestep_wire_test` 34 (this includes the two new red-on-main tests and the oracle). Pre-merge full wire suite: 33/34 with the old hold, 34/34 after narrowing it (the two new tests re-run: 4,366 issues, 0 with no motion; longest RUN_TURN_RUN stretch on the AI's clip 0 ticks, on a finished PES clip 0 ticks).
New tests: `EnterStop.TheRunStopArmReadsTheTurnStopRowAtTheBucketsColumn` (unit; red on the old call), `MoveStepWire.NoMoveStepIssueRequestsNoMotion` (red on main: 21 of 1,417 issues), `MoveStepWire.NoManSitsInRunTurnRunOnAClipPesDoesNotNumber` (red on main: 2,755 ticks), `MoveStepWire.TheMotionSetPredicatesAgreeWithTheTranslatedFunctions` (oracle, 65,536 ids x 2), `SimTail.CopiesTheSlipFacingOnlyWhenTheSlipFlagIsSet`. Updated to PES (their old premises were the mis-port): five `RunTurnRun.*` tests (window ends at min(end, fixed); a finished clip exits to the run) and the old SLIPDOWN tail test.
**Mutations: NOT RUN.** Resume list: revert `TurnStopRowFor` -> `StopRowFor` in EnterStop (expect the unit test red); `bucketColumn[bucket]` -> `bucket` (red); `E = min` -> `max` in RunTurnRun (turn tests red); drop the `/ TicksPerSecond()` in `PositionAt` (no test pins it yet: add one: a man moving 1 m/s reads ~3.6 km/h ground speed); drop RUN_TURN_RUN from `HoldsBodyPose` (the stuck test red).

## 5. Hooks asked of Main

`stop_entry_1406D5D60` (run-stop arm predicate) and `turnstop_sel_1406BA0D0` are live in `live2/pid2254416`; 0x1406ABB30 is a leaf outside `.pdata` (refused). Not yet replayed: the captures would give a real-call oracle for the STOP entry's selector and the empty-cell answer.

## 6. Ablation, drifted seeds, gate

**Smoke (2 min, seed 7, the gate's own run on the tip merged with main ebbc3741):** switch-off `399e180f39459c2b` (= main, unchanged; `[idle] 12 episodes, stalls 0`). pes-loop **`954745590bf2867d`** (main: `85c7d7c515cf92ae`), `[idle] 2 episodes, stalls 2` (main: 0 episodes). The stalls are set-piece takers/held men (`[stalemate-who] H11: movestep 1<-7 (IDLE) fn 14 clip mid_jocky_standby_noloop frame 2/114`, forcing H7/H4); my earlier 2-minute run with the wider hold (before narrowing it) also showed `[stalemate]` 5 s and 11 s idles. INF (not shown): the trajectory moved (ground speeds are now 54 times lower, RUN_TURN_RUN exits to the run) and exposed the latent resting-ball / taker-hold stall that TeamTick4 and RestingBall5 describe, not a new defect; needs the ablation to attribute.
**Ablation: NOT RUN.** The smoke move has four candidate causes, each a separate commit-sized revert: (a) EnterStop selector (51d09dab, `movestep_run.cpp`), (b) `PositionAt` per-tick displacement (`pesmovestep.cpp`), (c) RUN_TURN_RUN re-port (`movestep_turn.cpp`), (d) the word-index fixes (`movestep.hpp`/`movestep_turn.hpp`: `kFootFlag` 0x98 -> 0x26 etc., which also changes every group's foot-flag store). Revert them one at a time on a scratch copy and run `gfsim_run --seed 7 --minutes 2 --pes-loop` through simslot; (b) is the most likely mover (it changes every ground-speed gate in RUN_LOOP, STOP and the ladders).
**Drifted seeds (merge-lane censuses): NOT RUN.**
**GATE branch: NOT COMPLETE.** `GATE_TIER=branch` selected all 2,274 tests (sim sources changed, so every gfsim binary is selected); after 60 minutes (the shell's cap) it had run 1,187 of 2,274 with no failure printed (`build/gate.ctest.log`, `build/gate.branch.log`), then was killed by my own 3,600 s timeout. The smoke gate's two stalls above are the open red. Not `GATE branch: GREEN`.

## 7. Left (numbered, exact resume)

1. Attribute the pes-loop smoke move and the 2 stalls by the ablation above; if (b) is the cause, the stalls are the latent resting-ball/taker stall and belong to RestingBall/TeamTick5; if not, fix.
2. Re-run `GATE_TIER=branch` with a timeout of at least 4 h (the full select is ~2,274 tests at -j2); then census the drifted throw-in/PesStall seeds in the merge lane.
3. Mutations listed in section 4; add the `PositionAt` unit pin.
4. Replay the `stop_entry_1406D5D60` / `turnstop_sel_1406BA0D0` captures in `live2/pid2254416` against the translated selector (real-call oracle for section 1), and settle the empty-cell answer (section 2, OPEN) with a real capture of an empty cell.
5. The other turn handlers (SLIPDOWN FUN_1406C2F80, RUN_TURN_STEP FUN_1406C3470, STEP_TURN_RUN FUN_1406C4C10..., STEP_TURN_STEP) carry the same class of mis-port (the clamp min/max, draw arguments, byte-offset words); re-port each from its instruction stream as RUN_TURN_RUN was, then widen `HoldsBodyPose`.
6. `AnimeLayer` owns the playback side: tell it that `FacingAdjusterBusy` (FUN_141EB3320 on P+0x4d0) and the finished-clip-as-current-motion rule are the two anime-player facts this slice leans on.
