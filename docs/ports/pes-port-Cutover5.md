# PES port Cutover5: the +17% is the writer's timing (a different match); restart placements fixed; bearing seam explained; the 5-s set-piece wait predates the branch and is OPEN

Branch `Cutover` (origin): `744ad7fa` (the fix), on top of `05a7b4b6` (merge of origin/main, docs only). `git ls-remote origin Cutover` = `744ad7fa`. Gate not run (Main gates). Labels: CONFIRMED = read from code or disassembly, a test run, or a repro this pass; INF = inference; OPEN = blocker named.

## 1. Result

- Item 1 (baselines, main `ab0f449e`): 5 min `67d115b984bab251`, 25398 ticks; 2 min `8377aae321f84ea8`, 38139 ticks; switch-off 5 min `7ca3d08f68567016`, 31521 ticks. CONFIRMED.
- Item 2 (bisect): the writer move alone moves the count (+17%: 25398 to 29719). The bearing switch changes nothing once the writer is at the tick start (W = HEAD per tick), but it does change the old-timing trajectory (main vs B). CONFIRMED.
- Item 3 (judgement): no defect is shown in the timing itself (PES's writer timing is undecoded). The restart and in-play numbers are GF stand-ins, not PES measurements. Section 3.
- Item 4 (stall): the 5-s wait is on main too (same first stalemate line), so it predates this branch. Its cause is OPEN. Not caused by item 6.
- Item 5 (bearings): the S1 pair writer and the cone both use EF0's arithmetic. The 302 are EF0 returning 360.0 where E50 folds to 0.0 (the north seam). CONFIRMED (arithmetic); the count is INF.
- Item 6 (restart placements): placements did not reach the man records in their tick. RED observed, fixed in `744ad7fa`, green, mutation RED. No seed-7 hash changes.

## 2. Bisect (seed 7, `--pes-loop --parity-log`, 5 min unless noted; probe: per-tick ball and player positions, read-only, gated by `GF_PROBE`)

| Build | hash | ticks | in-play | hold | goals | stalemate lines |
|---|---|---|---|---|---|---|
| main `ab0f449e` (old per-build writer, stand-in bearings) | 67d115b984bab251 | 25398 | 19691 | 5707 | 1-2 | 36 |
| B: HEAD with the writer's 3 files reverted (old timing, EF0 bearings) | d149a69e614fe25d | 26353 | 19679 | 6674 | 0-2 | 32 |
| W: writer commit `196e3add` alone (new timing, stand-in bearings) | 9df315a3c6d7d58b | 29719 | 20227 | 9492 | 1-4 | 50 |
| HEAD `05a7b4b6` (new timing, EF0 bearings; Cutover4) | 9df315a3c6d7d58b | 29719 | 20227 | 9492 | 1-4 | 50 |
| fix `744ad7fa` | 9df315a3c6d7d58b | 29719 | 20227 | 9492 | 1-4 | 50 |

- W and HEAD agree at every tick (probe diff: no divergence over 29,719 ticks). The bearing switch changes nothing with the new timing. CONFIRMED.
- Main vs B (old timing, both): different hashes and goals (1-2 vs 0-2). So under the old timing the bearing switch changes the trajectory. This is the sensitivity to a 0.16 degree change, not a single decision I have isolated. CONFIRMED (hashes), mechanism INF.
- B vs HEAD (same bearings, only the writer timing differs): first divergent tick 1095. Slot 10 (H11, the man the stalemate breaker later forces) is at (-1.42850, -9.11813) in B and (-1.42890, -9.11736) in HEAD: 0.9 mm, ball identical. CONFIRMED (probe).
- Mechanism: under the old timing a team-stage reader (the S1 rows, the mark table or the run world; all read man records) sees each man one build behind. At tick 1095 that changes H11's target by a fraction of a millimetre, which grows. INF on which reader: the probe does not split the three readers. The match then differs: HEAD scores 5 (1-4), B scores 2 (0-2). Each goal costs a 1251-tick kick-off stoppage (`[parity-restart] kickoff ... frames=1251`), so the hold ticks go from 6674 to 9492. The +17% is three more goals, not a slower clock. CONFIRMED (goal counts and stoppage lengths).
- Main vs HEAD at seed 7: 25398 vs 29719 (+17.0%). Main has 3 goals (1-2), HEAD 5 (1-4).

## 3. Judgement against PES

- Writer timing: PES's per-tick movement writer is OPEN (docs/re/pes-re-player-record.md section 2.2). No PES timing exists to pick either side. The tick-start choice is kept as the cleaner invariant (readers see the tick's state), and the trajectory change is recorded, not called a regression. INF.
- Restart timings (not PES-matched; pre-existing): kick-off after a goal runs 1250-1252 frames (23.2 s) in main and in HEAD. PES's goal celebration is 16.3-17.9 s (docs/re/pes-re-A5Judge.md, the 19 s GF constant and the ~5 s gap). Kick-off release is a 125-frame stand-in (`pessequencer.cpp`); PES's live kick-off releases are 132-134 frames (docs/re/pes-re-kickoff-trigger.md section 2). Untouched here.
- In-play share (seed 7, 5 min): main 19691/25398 = 77.5%; B 74.7%; HEAD 68.1%. No live PES in-play share exists in docs/re, so neither is shown closer to PES. OPEN.

## 4. The 5-second set-piece wait (item 4): predates the branch; cause OPEN

- Main 2-min (`mainp_pl2`, 38139 ticks, 37 stalemate lines) has the same first stalemate line as HEAD: `piece 1 taker H2 start-in -1.0s`, ball at (-0.6, -9.4), inPlay 1, setPiece 0. CONFIRMED. So the wait is pre-existing, not new.
- The shape: buffer `piece 1` = `e_SetPiece_KickOff` (`src/sim/gametypes.hpp:34`), `prepared 1 started 1`; the match still counts play live (`inPlay 1`). Taker H2 is in a run-back clip (`run_2_2_back_090_mirror`). The stalemate breaker forces A10 after 5 s (`[stalemate]` at 5.0, 8.0, 11.0 s). CONFIRMED (log).
- Item 6 is not its cause: the fix's 2-min run is identical to HEAD (39261 ticks, 47 stalemate lines). CONFIRMED.
- HEAD 5-min: an in-play stretch of 1757 ticks (32.5 s, the restart bound) with the ball static at (-26.37, -16.06) from tick 12164 to 13920. Slot 2 stands 1.0-1.1 m from it with live = 1 and no set piece; the possessor neither passes nor shoots. CONFIRMED (probe). Not checked on main (its probe crashed; section 8).
- Blocker: the referee buffer's state at the kick-off arming (why a kick-off is armed while `inPlay` reads 1) and the possessor's decision flags at tick 12164. Neither is in the probe. Next: a per-tick trace of the referee buffer and the decision flags for `--minutes 2`, on main and HEAD.

## 5. The bearings: 0x140439E50 vs 0x140439EF0 (item 5)

- S1 pair writer, `FUN_14045B420` owner loop (file offset = VA - 0x140000000, disassembled): the bearing is inline. `CALL 0x140A92D40` (atan2) at `0x14045BCF3`; `COMISS XMM9,XMM6 / JBE` at `0x14045BCFB`; `ADDSS XMM6,2pi` at `0x14045BD01` for r < 0; `MULSS XMM6,180/pi` at `0x14045BD06`; no compare after the scale. The 180/360/-360 constants (`0x142598580`, `0x14259858C`, `0x142598598`) sit in the second pass at `0x14045BB2D-BB4A`, which writes A_tab and is not the pair. This is EF0's arithmetic. CONFIRMED.
- Cone / mark table: `FUN_140439EF0` (docs/re/pes-re-mark-cone.md section 2). The count gate calls EF0 twice (docs/re/pes-re-mark-table.md:156). CONFIRMED.
- E50 (`0x140439E50`, `src/pes/translated/ai/pes_fn_140439E50.cpp`): the same arithmetic, then `if deg >= 360: deg -= 360` (`CMP` with 0x14259858C, `!CF` branch). Its callers (0x1404AB8E0 for the +0x5EC head, 0x1407F4760 holder leaves, press and first touch) are not the S1 or mark-table readers. CONFIRMED.
- The 302: in binary32, 2pi x 180/pi (`0x1425795E4` x `0x14259BE40`) is exactly 360.0 (checked by float32 arithmetic). So for atan2 r in (-2.4e-7, 0), r + 2pi rounds to 2pi and the product is 360.0. EF0 keeps 360.0, as the S1 writer does; E50 folds it to 0.0. Same bearing, the north seam. CONFIRMED arithmetic; the count 302 is INF (not re-counted in this pass).
- Decision: every PES reader in scope uses EF0 (S1 pair table, mark table, cone). Nothing to change. E50 stays for its own callers.

## 6. Restart placements (item 6): defect confirmed, fixed

- Placements: `TeamAIController::PrepareSetPiece` (`teamAIcontroller.cpp:1442`; `ResetPosition` at 1504, 1524, 2045-2136), reached from `Referee::PrepareSetPiece` (`referee.cpp:783`) and `Match::ResetSituation` (`match.cpp:517`, `606`). All run in the referee stage, which in PES mode runs after the writer.
- Defect: readers in the same tick (S1 rows, mark table, run world, holder recompute) read the pre-placement spots. The ball and event stages and the referee read no records (grep of `matchevents.cpp`, `ball.cpp`, `referee.cpp`, `pessequencer.cpp`: 0 hits), so moving the writer is safe. CONFIRMED.
- RED (observed on `05a7b4b6`): `PesTickOrder.RestartPlacementsReachTheRecordsTheSameTick`, first placement at tick 0: 13 men, each record 3.6-26.5 m from where the man stands after the tick.
- Fix (`744ad7fa`, `match.cpp`): the writer runs after the referee stage, before the holder recompute and the first team-stage reader. PES mode only; GF mode unchanged (switch-off hash equal, section 7).
- GREEN: `pes_tick_order_test` 4/4.
- Mutation: the pre-fix HEAD (writer at the head) is the RED build above; putting the writer back reproduces the RED.
- Effect on the repro: none at seed 7 (both lengths): the lag reached no decision there. So this fix does not explain the stall.

## 7. Proof (simslot; PES21_ROOT=<workdrive>/PES21; PES_EXE set)

- Suites on `744ad7fa`: pes_tick_order 4/4, pes_mark_table 19/19, pes_scorerows 12/12, pes_offball 76/76, pes_event_wiring 4/4, pes_contact 37/37, pes_team_sheet 5/5, pes_world 6/6. All pass.
- Switch-off 5 min (no `--pes-loop`): main `7ca3d08f68567016` 31521 ticks; fix `7ca3d08f68567016` 31521 ticks. Equal.
- `--pes-loop` 5 min: main `67d115b984bab251` 25398; HEAD and fix `9df315a3c6d7d58b` 29719.
- `--pes-loop` 2 min: main `8377aae321f84ea8` 38139; HEAD and fix `1b5b777d77932430` 39261.
- The probe was compiled in for these runs but only runs under `GF_PROBE`. It is not committed. The Cutover worktree's `apps/gfsim_run.cpp` is back at HEAD.

## 8. Main baseline (item 1), and the base worktree

- Main (`ab0f449e`, the same code as the Cutover base; its three extra commits are docs): 5 min pes-loop `67d115b984bab251`, 25398 ticks, in-play 19691, hold 5707, goals 1-2, 8 stoppages, 36 stalemate lines, idle 37 episodes / 32 stalls. 2 min pes-loop `8377aae321f84ea8`, 38139 ticks, in-play 26969, hold 11170, goals 1-1, 12 stoppages, 37 stalemate lines, idle 48 / 42. Switch-off 5 min `7ca3d08f68567016`, 31521 ticks, 18 stoppages.
- The 5-min main hash and tick count match the Cutover3 reference (`67d115b984bab251`, 25398).
- Main's probe crashed at tick 0 (a segfault after the first active man; not investigated). So main has no per-tick trace, and its in-play static-ball check is not run. Its plain runs above are complete.
- `<workdrive>/gf-CutoverBase` was removed (`git worktree remove --force`), per the brief.

## 9. Open items

1. The 5-s set-piece wait (predates the branch, present on main): cause OPEN; next step is the referee-buffer and decision-flag trace in section 4.
2. The 32.5-s static in-play stretch on HEAD (ticks 12164-13920): OPEN, not checked on main.
3. In-play share vs PES: no live PES measurement in docs/re. OPEN.
4. PES's per-tick movement writer timing (docs/re/pes-re-player-record.md section 2.2): OPEN. The tick-start choice is kept, not verified.
5. The 302 count: the mechanism is CONFIRMED; the count is INF.
6. Pre-existing, not changed here: goal celebration 19 s vs PES 16.3-17.9 s; kick-off release 125 frames vs PES 132-134 frames.
