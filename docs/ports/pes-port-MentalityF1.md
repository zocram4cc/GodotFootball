# PES attack level F1: the pure forward line of sub_140682770 (branch MentalityF1)

Status: F1 done and pushed. The module is pure and unwired (F4 wires it). Verdicts: CONFIRMED = decompile, asm or install read in this pass; INF = inference, reason given; OPEN = blocker named.

## Commits (`git ls-remote origin MentalityF1` confirmed each)
- `a7eeefd` red: header and tests, no implementation. Not observed red at link (see Red evidence).
- `717f0a8` green: `src/sim/onthepitch/pesforward.{hpp,cpp}`, `tests/sim/pes_forward_test.cpp`, `tests/sim/CMakeLists.txt` (3 lines).
- `ca1453a` merge of origin/main `21d8b0c` (base was `84018a6`). Merged with `--no-recurse-submodules`: the default fetch exits 128 on the `extern/godot-cpp` symlink the worktree needs.

## Module contract (namespace `pes::forward`)
- `LoadConstants(exe)`: eight floats read by VA (table below). `LoadForwardClasses(exe)`: the forward arms from the pick jump table.
- `DepthFactor(k, level)`: 0 at level 0; `fullHigh` at the ceiling; else `level * depthPerLevel`.
- `LineTarget(f, deep, high) = deep + (high - deep) * f`.
- `BlendTarget(deep, highest, v108) = deep + (highest - deep) * v108` (v108 is an input, per Main; its gate and `highest` are OPEN).
- `ForwardLine(k, classes, in)` returns the protected slot and per-slot targets.
- `Input`: `level` (TeamData+0xB8A4), `possession` (T+0x255), `ballSide` (dir * ballY), `deep`, `high`, `floorRef` ([[ctx]]+0x44), `counterSlot`, `forwardCount`, `slots[11]` {`cls`, `onPitch`, `lineupScore` (+0x3A9C f0), `y` (+0x3AA4)}, `family[11]` {`line`, `rank`} as PES reads it (see Open 3).

## Install constants (read by VA; values checked against the install by the install test)
| VA | value | use | site |
|---|---|---|---|
| 0x142592228 | 0.25 | depth per level | 0x140682BE4 MULSS (CONFIRMED) |
| 0x1425656C0 | 4.0 | ceiling (rung) | 0x140682BD5 COMISS (CONFIRMED) |
| 0x14256AE1C | 1.0 | full factor: ceiling, Counter Target | 0x14068330E MOVSS (CONFIRMED) |
| 0x142598578 | 0.2 | rank scale, mode 1 passes | 0x1406832CF MOVAPS XMM6,XMM15; decompile l.499 (CONFIRMED) |
| 0x14259D5D0 | 0.65 | rank scale, mode 1 fails | 0x1406832D9 MOVSS (CONFIRMED) |
| 0x142591140 | +30.0 | role 9 bump in the scan | 0x14068305C ADDSS (CONFIRMED) |
| 0x14259BE84 | -10.0 | count > 2 bump (inert at the guard) | 0x140683089 ADDSS (CONFIRMED) |
| 0x1425C6614 | -100000.0 | keep-replace guard | 0x140682C81 MOVSS XMM12; 0x140683091 COMISS (CONFIRMED) |

Install values read in this pass (python, seek-based): 0.25, 4.0, 1.0, 0.20000000298, 0.64999997616, 30.0, -10.0, -100000.0.

Forward arms: pick jump table at 0x140683D60, one int32 RVA per class, class c at index c-1. Classes 7, 8, 9 point at 0x140683027 (the arm, CONFIRMED by the table read); classes 1-6 at 0x140683099. The line table at 0x140683D3C reads 0x140682D71 (classes 1-2), 0x140682D63 (3-6), 0x140682D6A (7-9) (CONFIRMED); not used by F1 (see Open 5).

## Decoded semantics (sub_140682770 and helpers)
1. **Factor (CONFIRMED).** Decompile l.233-243: level 0 gives 0; 0 < level < 4 gives level * 0.25; level >= 4 gives 1.0.
2. **Target (CONFIRMED, asm 0x14068331B-0x14068333B).** `target = B + (A - B) * f'` with A = local_4a0[line] (high) and B = local_4a0[line+3] (deep).
3. **Protected scan (CONFIRMED, asm 0x140682FA2-0x1406830B3).** Per eligible forward in slot order: `CALL 0x140A63950` with R8D=0xF and R9D=slot (Counter Target, per slot). If set, or `CMP byte [obj+0x129],1` (obj = [ctx+0x118]), take the slot and stop. Otherwise score = `+0x3A9C f0` (+30 if class 9); if count > 2 and mode 0 holds, -10; keep if `guard <= score` (COMISS at 0x140683091, CMOVNC at 0x140683095; NaN fails). Last eligible forward wins.
4. **Rank block (CONFIRMED, asm 0x140683285-0x1406832E7).** Runs when count > 2, the slot is not protected, mode 0 holds, and level < 4 (`CMP BL,4` at 0x1406832AE). Factor x0.2 if mode 1 passes (0x1406832CF), else x0.65 (0x1406832D9).
5. **Counter (CONFIRMED, asm 0x140683305-0x14068330E).** Counter Target holder: factor = 1.0, after the rank block.
6. **Floor (CONFIRMED, asm 0x140683340-0x140683363).** Protected slot only: `TEST BL,BL` at 0x14068334E skips when the level is 0; else `max(-ref, target)` (XORPS sign flip at 0x14068335F, MAXSS at 0x140683363). BL is the level byte: 0x1406832A5 and 0x1406832E7 reload BL from [RSP+0x21] before the gate on both paths.
7. **Mode predicate FUN_1406405C0 (CONFIRMED by decompile l.29-116; asm call sites 0x1406832A0 mode 0, 0x1406832C6 mode 1, 0x140683080 mode 0).**
   - Mode 0, both families: the candidate is the first or the last of his line.
   - Mode 1 out of possession: `dir * ballY > 0` picks the last, else the first. Asm cross-check of the polarity: open (decompile only).
   - Mode 1 in possession: the line's members (B != 0, same family line) sorted by Y; the first or the last is picked by the same sign.
8. **Y-sort (CONFIRMED, decompile of FUN_140A79050, 37 lines).** Insertion sort on the high-dword float key with strict shifts: ascending and stable. The merge (FUN_140A79240) applies only at n >= 33; team lists have 11 entries.
9. **Line lists (CONFIRMED, decompile of FUN_140682320, 95 lines).** Slots in order, on-pitch, B != 0. Line from B: 3-6 -> 1, 7-9 -> 2, else 0. Each line struct is 0x68 bytes; the forward line (2) starts at obj+0xD0, so count (+0x58) = obj+0x128 and active (+0x59) = obj+0x129. Both are incremented under the same on-pitch guard, so count == active.
10. **Y key (CONFIRMED).** The 12-byte records at +0x3A9C hold the score at +0 and Y at +8 (+0x3AA4 in puVar8).
11. **Blend v108 (CONFIRMED decompile l.612-636).** v108 = clamp((rec - lo)/(hi - lo), 0, 1) with lo/hi = [0x24]+20*line (+0, +4). Target = deep + (v158 - deep) * v108. The v158 max-fold and the gate are open (Open 2).
12. **Write order (CONFIRMED decompile l.489-512 vs 651-652).** Each slot's family record is written after its predicates. So PES reads the previous pass's record for the candidate and for unwritten slots (Open 3).

## Corrections to the doc and plan
- **C2 (scan byte).** `[ctx+0x118]+0x129` is line 2's ACTIVE count (obj+0xD0+0x59), not an independent advance byte. The early break means "one forward live".
- **C10 (floor gate).** The gate is the level byte (BL reloaded from [RSP+0x21]), not FUN_140687A80's return (stored at [RSP+0x20]). The level test is confirmed; C10's "INF until decoded" is resolved.
- **Counter test in the pick loop.** It is per slot (R8D=0xF, R9D=slot). The decompile's one-argument form is an artifact.
- **Plan R8, path A factor.** Decompile l.320-323 multiplies by `fVar3` = 0x142565600 (0.5), not 0.2 as the plan's R8 says. Asm not checked: INF. Not in F1 scope; F2/F3 must settle it.
- **RE-2 (FUN_1406405C0)** is now decoded (item 7), except the asm polarity cross-check.

## Tests (`pes_forward_test`, 16)
Green on the merged tree (`ca1453a`): 16/16 with `PES21_ROOT` = the install; with `PES21_ROOT` unset, 14 pass and 2 install tests skip.
- ForwardFactor.LevelQuarterWithTheTopRungFull: f(0)=0, f(1)=0.25, f(4)=1.0.
- ForwardLine.LineTargetIsDeepPlusFactorSpan: deep -20, high 0, f 0.25 gives -15.
- ForwardLine.BlendMovesTowardTheHighestByV108: v108 0, 0.5, 1 gives -40, -20, 0.
- ForwardLine.TwoForwardsShareTheLineAtMinusOne: level 1, floor slack, both -30.
- ForwardLine.ProtectedIsTheLastEligibleForward: slots 1 (CF) and 4 (SS) gives 4.
- ForwardLine.CounterTargetBreaksTheScan: counter at 1 gives protected 1.
- ForwardLine.ASingleForwardIsProtectedEvenBelowTheGuard: one forward at -200000 is still protected.
- ForwardLine.ThreeAtMinusOneFirstDropsToFivePercentWhenBallIsNotOnHisSide: first -38, middle -30, last -20 (floored).
- ForwardLine.ThreeAtMinusOneFirstDropsToSixteenPercentWhenBallIsOnHisSide: first -33.5.
- ForwardLine.CounterTargetOverridesTheSheet: holder at 0 (high).
- ForwardLine.LevelZeroHasNoFloor: all at deep -40, the last not floored.
- ForwardLine.InPossessionTheLowestYIsPickedNotTheFirstListed: Y 5, -3, 2; ball side -1; slot 1 gets -33.5.
- ForwardLine.InPossessionTheHighestYIsPickedWhenBallIsOnHisSide: ball side +1; slot 1 gets -38.
- ForwardLine.InPossessionTiesKeepSlotOrder: equal Y; slot 1 is picked first (-38).
- ForwardInstall.ConstantsAreTheExeFloats: the eight install values.
- ForwardInstall.ArmsAreSevenEightNine: classes 7, 8, 9 only.

## Mutations (14; each applied to pesforward.cpp, rebuilt, run, reverted; final restore checked: diff against HEAD 0 lines, 16/16)
| # | mutation | test(s) that failed (verbatim value) |
|---|---|---|
| M1 | depth per level 0.2 (literal) | LevelQuarterWithTheTopRungFull (0.2 vs 0.25) |
| M2 | line target factor x2 | LineTargetIsDeepPlusFactorSpan (-10 vs -15) |
| M3 | blend weight fixed 0.5 | BlendMovesTowardTheHighestByV108 (-20 vs -40; -20 vs 0) |
| M4 | rank block at count > 1 | TwoForwardsShareTheLineAtMinusOne (-33.5 vs -30) |
| M5 | first eligible protected | ProtectedIsTheLastEligibleForward (1 vs 4) |
| M6 | counter break removed | CounterTargetBreaksTheScan (4 vs 1) |
| M7 | one-forward break removed | ASingleForwardIsProtectedEvenBelowTheGuard (-1 vs 3) |
| M8 | out-of-possession ball side flipped | ThreeAtMinusOne x2 (-33.5 vs -38; -38 vs -33.5) |
| M9 | counter no longer takes factor 1.0 | CounterTargetOverridesTheSheet (-20 vs 0) |
| M10 | floor not gated by level | LevelZeroHasNoFloor (-20 vs -40) |
| M11 | Y-sort pick flipped | InPossession x3 (-38 vs -33.5; -33.5 vs -38; tie) |
| M12 | ties in reverse slot order | InPossessionTiesKeepSlotOrder (-33.5 vs -38) |
| M13 | arm index c instead of c-1 | ArmsAreSevenEightNine (class 6 and class 9 wrong) |
| M14 | rankFirstScale read from 0x14259D5D0 | ConstantsAreTheExeFloats (0.65 vs 0.2) |

## Red evidence
- The red commit's link was never observed: the first red build was interrupted by a duplicate build that I cancelled, and the archive step then failed on a missing object. The test object compiled. Recorded as not observed.
- Red against the rank-only implementation (before the Y branch): `pes_forward_test.cpp:169` `-38` vs `-33.5`; `:185` `-33.5` vs `-38`. The tie test passed there. It pins stable order and is a regression pin, not a red-first test.

## Smoke (2 min, seed 7, both modes, under `flock` on the merged tree `ca1453a`; exit 0 both)
- Plain: `[determinism] 5b9a33ccde3a8ad1`; HOM 1-1; passes 11-14; shots 3-2; 49485 ticks (cap 194400); restarts 24 stoppages, longest 23.1 s (bound 32.5 s); no RestartWatch.
- `--pes-loop`: `[determinism] d5bcdf7b73938eaa`; HOM 0-0; passes 11-10; shots 3-1; 46308 ticks (cap 194400); restarts 14 stoppages, longest 16.4 s; no RestartWatch.
- Comparison with main: not measured. tasks/19-10-26.md line 102 records 5b9a33ccde3a8ad1 as the predicted switch-off hash of the combined tree and as the measured value of "main + RoleArrays alone". Main at 1ff023d measured 75a11cdb2b3c8ec5. I did not build origin/main at 84018a6 for a direct comparison.
- Determinism rerun (same hash twice): not done; one run per mode.

## Open items
1. F2: the line max-fold (R4b, RE-3 open) producing v158 (`highest`); the path-A factor disagreement (0.5 decompile vs 0.2 plan R8) must be settled first.
2. F3/F4: the blend gate. It involves FUN_1408C5900(ctx+0x8E08), ctx+0x108/0x109, local_52f == local_530 and the counter exemption. The meaning of the +0 float of the +0x3BA4 record (v108's source) is open. Main's note covers v108.
3. F4: the family state across passes. The module reads `family` as PES reads it: the previous pass's records for unwritten slots. The caller must keep the state and write this pass's records after each slot. The write is keyed by the flag F (ctx+0x10C, EC600 arg4); the read is keyed by T+0x255. They match except the parity sub-path (W2).
4. The writers of `lineupScore` (+0x3A9C f0) and `y` (+0x3AA4) are open (the lineup feed and the Y source).
5. LineMapFromTable (0x140683D3C) is not implemented: the module needs only the arms. Add it when F2 or F4 needs line indices.
6. Counter Target: the caller maps the team's instruction-15 record (FUN_140A63950's tables at ctx+0x25CF8 / +0x25D38) to `counterSlot`.
7. The integer 4 (CMP BL,4 at 0x1406832AE; CMP R13B,4 at 0x140682E24) is the same rung as the float ceiling (0x1425656C0); the module uses the float.
8. The mode-predicate polarity (dir * ballY) is decompile-only; asm cross-check open.
9. Not run: gate (Main gates); determinism rerun; origin/main 2-minute comparison.
