# StepLand: MoveStepCell merged into TeamTick; the stale pass target cut from eight spans (max 8.70 s) to three (max 6.00 s) by three non-PES stand-ins removed; FUN_1406d1390 translated; ManRecord made loud; the last stale span is a GOTO_RUN/RUN_LOOP cycle at an ~80 degree turn (09-11-26)

Branch `TeamTick`, worktree `gf-TeamTick`. CONFIRMED = a tool run this session; INF = inference; OPEN = blocker named. Pushed tips (confirmed by `git ls-remote origin TeamTick`): `e9993394` (merges, PassGetReceiver, ManRecord), `d954d52e` (FUN_1406d1390), **`fc644102` (final tip: the press approach skips the receiver)**. Image `pes_full.bin`. **Acceptance is NOT met: the stale-target test is still red (6.00 s > 2.94 s), see section 3.4.**

## 1. Merges (CONFIRMED)
`origin/MoveStepCell` (`cbf97eaa`) then `origin/main` (`2d0759c4`) merged into `TeamTick` (`2e589cfe`): no conflicts, no generated file touched. Full rebuild green.

## 2. The stale-target test, span by span (item 2; seed 7, `pes_loop`, 19,440 ticks of 18.5 ms; probe lines in scratch `pt*.log`)
Spans of a target set with the ball at rest, longer than PES's live bound 2.94 s. The trajectory changes with every fix, so spans do not map one to one; TeamTick7 section 2 had seven (3.3-5.4 s).
| tree | spans > 2.94 s (start tick: length) | worst |
|---|---|---|
| TeamTick7 tip `2e589cfe` (TeamTick7 section 2) | 823: 4.2, 6563: 3.8, 7011: 3.5, 8166: 3.3, 10567: 3.5, 14425: 3.9, 17650: 5.4 | 5.43 s |
| + MoveStepCell merged (`pt1.log`) | 762: 8.19, 1691: 3.78, 2274: 8.70, 3066: 5.63, 3970: 3.37, 12941: 6.00, 14024: 4.09, 16176: 8.33 | 8.70 s |
| + PassGetReceiver stand-in removed (`pt3.log`, partial run to tick 8,500) | 680: 8.61, 1439: 3.89, 2765: 4.56, 4023: 9.26, 6841: 3.43, 7462: 3.69 (and others) | 9.26 s |
| **final tip `fc644102`** (`pt7.log`, full run) | **580: 4.41, 6816: 4.17, 9200: 6.00** | **6.00 s** |

## 3. Root causes (CONFIRMED by per-tick probes)
1. **`Team::PassGetReceiver` returned null while the ball rested** (`GameplayTuning::kStalemateBallSpeed_ms` early return, a `ponytail:` stand-in for the writer's clear, `team.cpp:462-467`). PES's selector `FUN_140931c60` (decompile 0x140931c60) has no ball-speed test; the named man stays the receiver until `FUN_1404FB380` clears `T+0x78` (`pes-port-TeamTick.md` section 1, translated). The named man was therefore no receiver on a resting ball: id 0 at 5.0 m stood IDLE/IDLE_OTHER for 5 s (ticks 2349-2646, `pt2.log`). **Removed**, no replacement.
2. **`EnterIdleTurn` (`FUN_1406d1390`) was a stand-in** (`movestep_group_a.cpp`: "PES's row table at 0x1425dda30 is not decoded; the shared IdleRow is the port's stand-in"). It issued an IDLE clip (motions 4154/4155) in state IDLE_TURN, so a standing man told to face away never turned: id 3 at 8.4 m, ticks 702-849 (`pt5.log`): target heading 247 degrees, facing 67 -> 151 in 150 ticks (the root-facing tail's 0.4 degrees a tick), the man cycling IDLE -> RUN_LOOP -> GOTO_RUN at 0-0.4 m/s; id 5 stuck in IDLE_TURN (state 2, previous 2) for 8 s (ticks 4023-4374, `pt3.log`). **Translated** (commit `d954d52e`): the turn row `DAT_1425dda30[kind]` at the column `DAT_1425dd988[look bucket]`, the look-bucket flip arm, the 167.5 heading clamp (`FUN_140a92950`), the 1/6 s issue (section 4).
3. **A press command shadowed the receiver's approach** (`elizacontroller.cpp:670`, commit `fc644102`): the designated man (= the receiver, `team.cpp:694`) out of reach of the ball queued the PES ActionPress approach first, and `MovementDecision` takes the FIRST movement command, so `_PassGetApproach` never steered him: `desKmh` flipped 8.0 / 13.5 every few ticks (the press class cap on the instantaneous speed sample, MoveStepCell4 Left 5) on a man with `recv=1`. After the change the receiver's command is the approach's (28.8 km/h, `pt8.log`).
4. **The remaining stale span (tick 580-818, 4.41 s; `pt8.log`) is a MOVESTEP defect, not a pass-target or trap one, OPEN.** Man id 2, receiver, 2.9 m from the ball, command class 3 at 28.8 km/h, target heading 292 against facing 210-215 (turn +78..+82 degrees, move bucket 2, look bucket 2): the record alternates GOTO_RUN (7) <-> RUN_LOOP (3) every 9 ticks for 4 s at 0 to 0.36 m/s and the facing drifts the WRONG way (214.8 -> 209.5, away from the heading), until a stand clip ends it. In PES a turn of 80 degrees at class 3 goes through the run-turn states (RUN_TURN_RUN 12 / RUN_TURN_STEP 16), not a GOTO_RUN re-entry loop. Where to start: `movestep_run.cpp` `GotoRun` re-entry (line ~707-713: `EnterGotoRun` again when `45 < turn` with the band words below 1), RUN_LOOP's hand-back to GOTO_RUN, and what applies the clip's root rotation after `EnterGotoRun`'s issue (the facing moved 0.3-0.5 degrees per cycle in the wrong sign). The other two final spans (6816-7041, 9200-9524) were not probed (no budget): INF same family (a standing start at an angle). **MoveStepCell owns this.**

## 4. FUN_1406d1390 (translated, hand port in `movestep_group_a.cpp`; operand pairs from the disassembly)
`0x1406D13DD MOVSS XMM10,[0x1425ddfc8]` = 167.5; `0x1406D1434 COMISS XMM0,[0x1425ddfcc]` = 168.75; `0x1406D15AB MOVSS XMM0,[0x1425ddff8]` = -167.5; `0x1406D1666 MOV EBP,0x54c` (row for the replay action and kind 3 in a stoppage); `0x1406D16BF MOVSS XMM0,[0x1425ddf50]` = 0.1666667. `[0x1425dda30]` = {1353,1353,1354,1355,1357,1358,1359,1360} (kind), `[0x1425dd988]` = {9,11,13,15,17,8,6,4,2,0} (look bucket), ball sample tick `int(54 * [0x14259be1c] 20.0 / [0x14259a764] 60.0 + [0x142565600] 0.5)` = 18. The tail call (`P+0x4d0` vt+0x08) is the AdjustAngle adjuster's apply, a documented no-op (`AdjustAngleApply`, `movestep_turn.cpp`). New reads: `TeamAi::BallOwnerSlot` (`MA+0x2601C`, staged by `Team::RunPesPassTarget`), `TeamAi::MaFlag25930` (`ponytail:` named: `MA+0x25930` has no GF source). New constants and the idle-turn row table load from the install (`LoadRunConstants`, `LoadRunTables`). Not a recorded proof: no PES call of `0x1406D1390` exists (no capture, rig OFF); the proof is the decompile + disassembly read and the unit relations below (labelled: not bit-exact against a real call).

## 5. Item 4: `WriteActorP` / `ManRecord` (CONFIRMED)
PES's accessor `FUN_140a48a10` (decompile) answers `*(G + 0x1DD8 + 0x28 * slot)` for a slot below 26 and entry 0 otherwise, with NO null check; GF binds all 26 entries (`BindSlot`, `BindOfficial`), so `ManRecord` cannot return null in a match: the `-Wstringop-overread` is the compiler seeing `InWindow`'s nullptr arm (note: "source object is likely at address zero"). No null check was added to `WriteActorP` (PES has none). `ManRecord` now stops loudly on an unbound table (`BoundRecord`: stderr + abort) so the nullptr arm is unreachable by construction. Tests (`pes_player_object_test`): every bound slot's record is the window record; a slot past the table reads entry 0; `EXPECT_DEATH` on an unbound slot.

## 6. Tests and mutations (CONFIRMED runs)
- `pes_movestep_test` 120/120 (5 new `EnterIdleTurn.*`), `pes_movestep_run_test` 102, `_step_test` 64, `_turn_test` 81, `pes_player_object_test` 7 (the `ManRecord` suite and its death test).
- Mutants of `EnterIdleTurn`, built, run, reverted (`git checkout`): owner test `0x15 <` -> `>=` killed by `TheLookBucketFlipsWhen...`; replay row without `kind == 3` killed by `TheReplayActionAndAStoppedKindThree...`; clamp 167.5 -> 168.75 killed by `TheLookHeadingStepsFromTheFacing...`; row `[kind]` -> `[0]` and duration 1/6 -> 0.2 killed by `IssuesTheTurnRowOfTheKind...`. Not run: a mutant of `ManRecord`'s abort (the death test covers it by construction), of the PassGetReceiver removal and of the press-skip (end to end only: the stale test).
- Red-first: the stale-target test is the red test (8.70 s on the merged tree); the three fixes are verified by the probes of section 2, not by a green test.

## 7. Items 3, 5, 6 (what is done and what is not)
- Item 3 (PesStall): not reached. `pes_restart_delay_test` was run once on the merged tree plus the receiver change (killed at 33 min to free a slot: its first two tests passed, `PesStall.*` had not run); it was not re-run on the final tip outside the gate. The resting-ball trap admission / contact item (gate 4a, `FUN_1406EDCE0`) is TrapAdmit's / TrapContact's; this branch does not port it.
- Item 5 (surveys): see "Final status" below.
- Item 6 (drifted seeds): see "Final status" below.

## 8. Final status (gate, surveys, seeds)
(filled in below when the gate finishes)

## Final status (from the agent's closing message; supersedes §7–8 above)

Tip `fc644102`. Stale-target test red at 6.00 s with 3 spans (start ticks 580, 6816, 9200; 4.41 s, 4.17 s, 6.00 s).
Remaining root (OPEN, MoveStepCell): at ticks 580–818 the receiver (id 2, 2.9 m from the ball, command class 3 at 28.8 km/h,
target heading 292, facing 210–215) alternates GOTO_RUN (7) and RUN_LOOP (3) every 9 ticks for 4 s at 0–0.36 m/s; PES takes a
run-turn state (12 or 16). Start at `movestep_run.cpp` `GotoRun` ~707–713 (the `EnterGotoRun` re-entry when `45 < turn`).
Not done: PesStall on the final tip, the seed-7/16 surveys and the six PES-path binaries, drifted seeds, the gate verdict (the
gate run on `fc644102` was stopped by Main: the tip was known red and the next slice changes it). Mutants killed: the owner
test, the replay row without `kind == 3`, clamp 167.5 → 168.75, row index `[kind]` → `[0]`, duration 1/6 → 0.2.
