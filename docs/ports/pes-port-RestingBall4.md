# RestingBall4: seed-7 pes-loop match finishes with the PES trap end; trajectory leaf 0x140852C00 translated; the keeper "NO APPLICABLE ANIM" was the goal-kick strike's empty queue; PressAI's tree still aborts at 0x140862CE0 (05-11-26)

Branch `RestingBall3` (pushed; `git ls-remote`: b0998bfd leaf, 876f816d keeper fix + test, 0c68ec30 merge of main 605a227d). Image `pes_full.bin`. CONFIRMED = a tool run this session; INF = inference; OPEN = blocker named. Merged `origin/main` (6440f6a0, then GateQuick 605a227d) and the unlanded `origin/SurveyTool` commits (GF_PES_SURVEY and `untranslated_callees.py`): Main must land SurveyTool before or with this branch. `origin/SynthTool` was NOT merged (it deletes synth_walk.py/synth_call.py, which conflicts with RestingBall3's `synth_trapkind.py` changes).

## 1. Verdict

| Item | State |
|---|---|
| 1. Survey (one seed-7 PES-wait match) | **Done.** `GF_PES_SURVEY` on the stand-in tree lists ONE target: `0x140852C00` (2 hits, first frame 1794, caller `pes_fn_1408495B0`). It never reached `0x1407A38E0`. |
| 2. Translate and prove the list | `0x140852C00`: **done, 264/264 bit-exact, SYNTHETIC** (section 2). `0x1407A38E0`: **not translated** (section 3); not reached by seed 7 on this tree. `0x14079EE70` re-proof on real states: no new capture reached its branch (none of the new captures were calls of it), not redone. |
| 3. Keeper `NO APPLICABLE ANIM` | **Fixed, with a test** (section 4). Cause is NOT the PES wait. |
| 4. PesStall seed 7, trap-end stand-in gone | **My tree: all 4 PesStall tests pass** (ATrapClip 511 s; TheBreaker; ALiveBall; the new fallback test), the match reaches full time. **PressAI's tree (48275157 + this branch): NOT green**: `PES_DISPATCH_UNTRANSLATED 0x140862CE0` (section 5). |
| 5. Ablation / census / GATE | Ablation done (section 6). Census: not run (merge lane). Gate: section 7. |

## 2. `0x140852C00` (CONFIRMED)
- FUN_140852c00 (decompile): a leaf with no `.pdata` entry; `ecx` = a motion index, `dl` = mirror flag (negates the float), `xmm2` = a float clamped against per-index thresholds (switch over 0x244, 0x24f, 0x2cd, 0xd0d, 0xd3e, 0xd4d..0xdad). Callers: `0x1408496DB`, `0x14084973F`, `0x140849770` (all in `0x1408495B0`, itself in the proven trap closure). Its three `OUT-OF-SET CALL` markers in the layer aborted the PES-wait match at frame ~1794.
- `closure.py --spec-set trapaction --tag trajleaf --subsystem restart 0x140852C00` (universe row, spec, set file), `gen_translated.py` (5291 functions, 3753 recorded), `ledger.py --check` current. Held-out lines now three (`0x1403BB670` -> mistranslated `0x14037CB50`; `0x1405549C0`; `0x14202BA30`); `0x1403BB670` is new on main, not mine.
- Proof `tools/dev/translate/synth_trajleaf.py`: 264 cases over the memory of the real capture `live2/pid130478/seq7134` (only its stack is used): each index of the switch, both mirror values, the float at each threshold (constants read from the image), one ulp either side and its negation; plus indices that fall through. `snapdiff.py --bounded`: `calls=264 replayed=264 mismatches=0`. Every case is **SYNTHETIC** (registers chosen), recorded in `results_trapaction.json` (`synthetic: true, synthetic_cases: 264`). No real call exists: the `traj_caller_8495B0` sample of 05-11 10:40 was lost to the disk floor.

## 3. `0x1407A38E0` (action-4 start virtual), OPEN
- Decompile: sets `P+0x375D = 0`, `FUN_1406EC5F0(P)`; if false `FUN_1406EEBB0(P,2)` and return; else `FUN_1406EEBB0(P,4)`, `FUN_1407B6D80`, `FUN_1407A4D30`, `FUN_1407B68B0` (+ a switch on `P+0x3A94`). Static closure of the three callees: 659 members, **349 not in the layer**, mostly pre-match / animation / cutscene code (cold for a trap), so the proof must be capture-bounded.
- Main's watcher facts: ZERO sampled calls of `act4_start_7A38E0` in any launch, one each for `7B68B0`/`7B6D80`/`traj_caller`, 44 of `7A4D30` (all from `0x1407B3944`, another action). **PES almost never calls it.** The five hooks left `wanted.txt`.
- On this tree: with the stale-pass-target stand-in ON, the survey lists no `0x1407A38E0`. With it OFF (scratch toggle, ablation below) the pes-loop smoke also runs to full time with no `PES_DISPATCH_UNTRANSLATED`. RestingBall3's abort at t ~ 712 s came from the old trajectory; after the goal-kick fix it is not reached on seed 7. If TeamTick2's real pass target does reach it, that is a divergence from PES (Main's note).

## 4. Keeper `NO APPLICABLE ANIM` (CONFIRMED)
- Same seed, no PES wait (scratch toggle `CurrentMotionOf`): **13 fallbacks** (same count with the wait and no stand-in; 5-6 in other variants). So it is not new with the wait.
- Cause: `ElizaController::RequestCommand` set-piece branch `if (!GoalKickInContact()) return;` (elizacontroller.cpp:346). From the ladder's release to the strike's contact tick the keeper's queue is empty; when his clip ends (`interruptAnim == Switch`) `Humanoid::Process` finds nothing and calls its `ResetPosition` fallback. Probe at the six alerts: `inPlay 1 inSetPiece 1 piece 2 (GoalKick) taker 1`, player 16 (the keeper), frames 5705..5755 = the strike window (`GKP` probe: release f5700, contact f5775).
- Fix: during the strike the keeper gets a stand `Movement` command (idle velocity, looking at the ball), `ponytail:` naming the PES strike clip (sub_140775780). Test `PesStall.ANoManIsEverHandedNoCommandAtAClipEnd` (binds `blunted::BindLog`, counts the message over the seed-7 match, expects 0). It passes after the fix and the log counts above are its red state; **I did not run the new test on the unfixed tree and did not run the mutation (revert the stand command), budget**: do both first.

## 5. PressAI's tree (`48275157` + this branch, scratch worktree `gf-RestingBall-press` `ed0ae187`, local only)
- `PesStall.*` aborts: `PES_DISPATCH_UNTRANSLATED: 0x140862ce0`. Survey: 1 hit, first frame 5049, caller `pes_fn_14085E3C0` (the contact-point search); that tree also shows 6 idle stalls (survey run, with the call no-op'd, so not a verdict).
- `closure.py --dry-run 0x140862CE0`: **34 members, only two not in the layer: `0x140862CE0` and `0x14085E1C0`.** Decompile of `0x140862CE0` (`FUN_140862370`, `FUN_141e532d0`, `FUN_140a6cf70`, a loop; float in/out).
- Proof inputs: `synth_walk.py --in-call --entry 0x140862CE0` on the three real ticks (`pid1572516/seq398`, `pid3439759/seq3244`, `pid1330921/seq5112`, now unpacked with the capz marker: **re-pack them**) fails with `UC_ERR_READ_UNMAPPED` (INF: those ticks never reach it, and the slim read set ends). A hook on `0x140862CE0` (it is a `.pdata` function? not checked) or on its caller `0x14085E3C0` is needed: EVERY 1.

## 6. Ablation (seed 7, 2 min, `--minutes 2`; scratch env toggles, reverted from the tree)
| run | switch-off | pes-loop |
|---|---|---|
| main baseline | 399e180f39459c2b | 85c7d7c515cf92ae |
| this branch | a0fbe2d5cd445a52 | cae52312062a4e50 |
| stale-pass-target stand-in OFF | **399e180f39459c2b** (= baseline) | befe6547b971b879 |
| PES wait OFF (CurrentMotionOf as main; the hold stays deleted) | a0fbe2d5cd445a52 | 78917d4ec7fbc41e |
Reading: **the switch-off move is entirely `Team::PassGetReceiver`'s stand-in**; the PES wait, the translations and the goal-kick stand command touch only the pes-loop hash. The pes-loop move is the sum of the three (hold deleted + wait, stand-in, goal-kick command, leaf); not split further. Smoke on this branch: pes-loop 35119 ticks to full time, 23 stoppages (longest 23.6 s), 0 idle stalls, 0 fallbacks; switch-off finishes.

## 7. Gate
- `GATE_TIER=branch` cannot finish on a sim-source branch: it selects 2311 tests and `ctest -R` fails (`RegularExpression::compile(): Expression too big. No tests were found!!!`, `build/gate.ctest.log`), then `gate.sh` exits 1 silently. Told GateQuick. Its smokes in that run: switch-off `rc=0`, pes-loop `cae52312062a4e50`.
- **`GATE fast`: RED (run on 0c68ec30, ctest 2323/2326 done when read; smokes ran: switch-off rc=0, pes-loop cae52312062a4e50).** Failing: `MoveStepWire.CurrentMotionIsThePesMotionTheBodyPlays` (its helper did not list the trap family: FIXED in 07a41c65, passes alone); `PesForfeitMatch.ARedCardRemovesTheOffenderAndTheSideIsNotForfeited`, `Loops/GoalSceneHold.TheKickOffWaitsForTheSceneAndThenKeepsItsSpacing/PesLoop`, `pes_throwin_release_test`, `pes_restart_delay_test` (all four **aborted**: the gate log holds `PES_DISPATCH_UNTRANSLATED` for **0x1407a38e0 (twice), 0x140862ce0 and 0x140862ae0**, i.e. other seeds/matches DO reach the action-4 start and the 0x140862xxx contact-search leaves that seed 7 does not); `diff_impure_recorded` (failed in 3 min, reason not read). The census/resume work is therefore: translate and prove `0x140862CE0`, `0x140862AE0` (new), `0x14085E1C0` and `0x1407A38E0` and their closures, on the matches that reach them (find them with `GF_PES_SURVEY` on those tests' seeds). Gate log: `gf-RestingBall-scratch/gate2.log`, `build/Testing/Temporary/LastTest.log.tmpe2ce6`.
- Census of drifted seeds: not run (rule 6). Expect seeded families in the switch-off lane to drift (the stand-in moves the switch-off hash).

## 8. Open, numbered (resume state)
1. PressAI's tree: translate `0x140862CE0` + `0x14085E1C0` (34-member closure, 2 new), prove (hook needed, see section 5), then `PesStall.*` there.
2. `0x1407A38E0` + `0x1407B68B0`/`7B6D80`/`7A4D30`: capture-bounded proof if a real call ever lands; PES basically never calls it (INF: GF's trap-end planner request reaches it only on trajectories PES does not produce).
3. Run `PesStall.ANoManIsEverHandedNoCommandAtAClipEnd` on the pre-fix tree and the revert mutation.
4. **Latent MOVESTEP defect (not fixed)**: on the pre-goal-kick-fix trajectory a throw-in taker (id 17, seed 7, frame 10751) was placed at (44.7,-21.6) and flapped STEP_LOOP(4) <-> STEP_LOOP_CHANGE(5) every tick, re-issuing `walk_1_1_relax` / `045_accel` at frame 0-2 for 1400 ticks (0.5 m/s), ending in `[restart] stalled`. Facing was 65 deg off the target (`dir 0.39,-0.92`, target `-0.33,-0.94`). `movestep_step.cpp` ~857: `kDwellLimit` is written `SetU(uint32_t(clipBlend))` (0 for a 0.2 float) where every other writer uses `SetF`; INF that the dwell gate then never holds, not checked against PES (`0x1406C5FF1..0x1406C6019` stores did not match that field; the function layout differs from the port's comments). Present on this branch after the fix only by luck of trajectory.
5. Replace the stale-pass-target stand-in with PES's team tick (TeamTick2).
6. Census (merge lane), GATE fast result.

## 9. Verification actually run
`synth_trajleaf.py` (264/264), `gen_translated.py` + `ledger.py --check`, `pes_restart_delay_test --gtest_filter=PesStall.*` on this tree (4 passed) and on PressAI's tree (abort), 5 survey runs, 4 ablation runs + 3 probe runs (scratch, reverted), `GATE_TIER=branch` (failed in ctest selection, section 7). No mutation tests.
