# A4Walk: Dribble chooser, unit table and walk, Keep removed

Source: `pes-re-A4Carrier.md` §10 steps 1–2 and §5, §6; Main's IRC items 1–4 (chooser, walk rules, restart-veto input, skill 34 and Through tests).
Branch `A4Walk`, worktree `<workdrive>/gf-A4Walk`, from `origin/main` a17df0d.
Commits: `0bb02b8` (walk v1, MiddleShot COM 5, Cross comment), `74da11f` (pes_loop wiring), `61eb71d` (this step). `git ls-remote origin A4Walk` = `61eb71d`.

## Result: Partially done

Done: item 1 (chooser, faithful flow, constants from the exe; Keep proven unreachable and removed), item 3 (restart-veto input named, with the GF part wired), item 4 (skill 34 and Through→LongPass, red-first). Partial: item 2. Rules 1, 3, 5, 6, 9, 10 and the set-piece tail are ported. Rules 7, 8 and 11 are not, nor are the non-Basic plan-tail flags (see §Open items).

## 15. Keep is dead in PES (C-now)

Main's ruling applied: Keep has no reachable producer, so `IsKeep`, `kDribbleKeep` and the Keep test are deleted.

- The chooser `FUN_14212F1C0` returns only 4, 5 or 6. Its asm writes `EDI` (the return) as `MOV EDI,0x5` at `0x14212F287` (free lane, default), `MOV EDI,0x6` at `0x14212F30A` (feint), `MOV EDI,0x4` at `0x14212F37B`, `0x14212F455` and `0x14212F828` (gates), and at the loop end `MOV ECX,EDI; MOV EDI,0x4; CMOVZ ECX,EDI` (blocked → 4). The epilogue `0x14212FDD3: MOV EAX,EDI; RET` has no other write.
- The caller `FUN_14212FE70` (Dribble, unit 16) sets `iVar10` only to 4 (`0x14212FE70` L86, default), to 3 (L462, L484), and from the chooser (L439). Its dispatch maps 3, 4, 5, 6 to subs 3, 4, 5, 6. The keep arm (L524–525, `FUN_142119E90`) is reached only for a value outside 3..6.
- `FUN_142119E90` has one direct call site in `.text`, at `0x142130D31` (scan of all call and jmp sites, `calls_142119e90.tsv`), which is that arm.

Conclusion: no PES path stores sub 2. The earlier note "Keep = sub 2 via the chooser's non-3–6 result" (carrier-units §2.1, A4 report §0.3) is wrong for this exe. Sub 2 stays undefined here. The walk now carries only sub 3–6 for dribbles.

## Changes

- `pescarrier.hpp/.cpp`:
  - `DribbleChooser` (0x14212F1C0): feint gate (0x14212E970 caller input), vertical gate (team mode 1), branch P (plan ∉{1,2,3} and not turning: side band 40 + 20·pressure against the team attack, 0x14259BE2C and 0x14259BE1C) or branch Q (turning or plan 1–3: 60 against the plan heading, 0x14259A764), box-pressure gate, the fast-carrier gate (speed > 10 and off-motion > 40), then the free-lane gate (speed ≤ 15 or motion within 60, room ≥ 12, 0x14259BE0C and 0x14259BE04). Returns 4 (blocked), 5 (free) or 6 (feint).
  - `DribbleSubKind`: the dispatch around it (0x14212FE70 L433–465). Plan Long runs no chooser and keeps mode 3. The override (L442–462) is an input, not decoded to its callees.
  - `BuildUnitList` (0x1421112C0): the list builder, decoded switch over the plan. Basic is `23,22,0,1,17,14,11,12,2,3,10,4`; the wide-side tail `7,6` and the human 9 are conditional. My previous fixed list always included 7 and 6, which PES does not do for Basic.
  - `WalkCarrierUnits` (0x14210F110) over the table in build order: rule 1 (kind 22 answers first), rule 3 (first-time gate, inert while the tier gate is off), rule 5 (restart vetoes), rule 6 (cut-back replaces a shot outside the close band with kind 3), rule 9 (cross override with skill 14), rule 10 (owner check in play), and the set-piece tail (the designated kicker's restriction).
  - `MiddleShotSkill`: skill 39, 8, 33 or 34 (0x14211B628's gate, report 4.4).
  - `PesPassFunctionType`: through → `e_FunctionType_LongPass`, lofted and centring → `HighPass`, short → `ShortPass`.
  - `CarrierConstants`: seven chooser fields, loaded from 0x142591140, 0x14259A764, 0x14259BE2C, 0x14259BE1C, 0x142588210, 0x14259BE0C, 0x14259BE04.
- `elizacontroller.cpp`: the PES tail builds the chooser input, the unit list and the table, runs the walk, and maps kind 0 to the shot and kind 4 to the pass. The GF path is unchanged, and so is its pass mapping. MiddleShot's skill comes from `MiddleShotSkill`.
- `tests/sim/pes_carrier_test.cpp`: 67 tests in total. New: chooser, dispatch, list builder, walk rules, MiddleShot skill and pass mapping (28 tests); 23 of them fail on the stubs, and the rest pin behaviour the stubs already matched. The install relation now also checks the chooser constants. The Keep tests are removed.

## Tests (red → green)

- Red: the new tests were written against stubs (`BuildUnitList` → `{}`, `WalkCarrierUnits` → none, `DribbleChooser` → 4, `MiddleShotSkill` → false, `PesPassFunctionType` → ShortPass). 23 failed on assertions, 44 old tests passed. One test was then corrected: `KindTwentyTwoWinsOutsideSetPieces` had the shot first, and the decode walks table order, so 22 is the second slot. That fix is in the commit, and I say so here.
- Green: `pes_carrier_test` 67/67, including the install relation for the chooser constants.
- Mutants, applied together, rebuilt, run, reverted (`git diff` empty after revert). Each caught by its named test:
  - M1 veto bypass → `ThePKAndClearStatesVetoPassesButNotShots`
  - M2 owner check off → `AnOpponentWithTheBallRejectsAPassButATeammateDoesNot`
  - M3 rule 6 off → `ACutBackReplacesAShotOutsideTheCloseBand`
  - M4 rule 9 off → `ACrossOverrideNeedsSkillFourteenAndAListedCross`
  - M5 Rising Shots dropped → `MiddleShotSkillCountsRisingShotsOnItsOwn`
  - M6 through → ShortPass → `ThroughIsGFsLongPassOnThePesPath`
  - M7 wide-side tail on plan mode alone → `TheWideSideTailAddsSevenAndSixOnlyWhenThePlanAsksForIt`
  - M8 feint → mode 4 → `TheFeintNeedsItsGateAndTheCarrierPastHalfway`
  - M9 pressure term dropped → `TheSideBandWidensByTwentyPerUnitOfPressure`
  - M10 Long skip removed → `TheLongImageRunsNoChooserAndKeepsModeThree`

Build: `pes_carrier_test` and `gfsim_run` build clean; no warnings from the changed files.

## Smoke (2 min, seed 7, `PES21_ROOT` set; agent policy: no lock, no gate)

- Off path: `d4a22e3211c6940e`, **unchanged** from main. 32125 ticks. Passes 7–11, shots 3–4.
- On path: `0dcdba0d34bf11c3` on two reruns (deterministic). 37192 ticks to full time (cap 194400, no hang). Passes 16–18, shots 2–1, on target 1–1. Tripwire passes.

## Item 3: restart-veto input

- Rule 5 needs `MI+0x116C` (1 play, 2 stopped, 3 set piece) and `MI+0x1775` (FSM byte: 3, 9, 10 veto passes and clears). The `0x116C` mode is wired from GF: `IsInSetPiece` → 3, `IsInPlay` → 1, otherwise 2. Documented in `pes-re-net-dodge.md:108`.
- `MI+0x1775` has no GF state, and its writers are open (`pes-re-foundation.md` E9, "writers unknown"). It stays at 0 with `ponytail:` in the controller. Named, not guessed.

## Open items

1. **Rule 7** (clear → pass, forced-clear test, `0x14210F110` L200–432 and the 0x14211E… helpers). Not ported. Clear (kind 1) has no producer in GF, so it is inert.
2. **Rule 8** (pass vetoes: Forward's receiver-passed-to-carrier test, L471–536, `FUN_140A4C820`; Safety's close-band and keeper test, L443–469; the lane battery L541–633, `FUN_14063DE20`). Not ported. Forward (4) is live, so this is the main gap in the sim.
3. **Rule 11** (arming gate, `FUN_142111610`, 293 lines in the list-run loop). Not ported; separate from the walk.
4. **Rule 3** is ported but inert: the tier gate needs PES's reaction tier, and GF has none.
5. **Chooser inputs** are stand-ins (all `ponytail:` in the controller): the feint gate `0x14212E970`, turning `0x140A6E300`, team mode `0x141052E30`, box pressure, the velocity sample `0x140A6C6D0`, the room test `0x140A4DDC0` (nearest opponent used), the free-lane loop `0x14063CD90` and `0x140A53DF0` (carrier pressure class used), the caller pressure scalar (param_5), and the override L442–462. The sub-kinds 3–6 map to the same GF output: heading search at sprint speed. Mode 3/4 and feint movement have no GF counterpart yet.
6. **Plan flags**: PES's plan record flags (+0xD8, +0xD9, +0x10), plan mode (+0xC4), imageA+0xB44 and the cross-line test are defaults in the list builder. So plans 5–9 list no PassForward in the PES path, and the GF pass is suppressed while those plans run, until their units exist. The on-path smoke still has passes, but the tripwire is coarse.
7. **Designated kicker** (MA+0x7C) has no GF accessor. `carrierIsKicker` stays false. `passAllowedInSetPiece` is true (`0x14063E3A0` not decoded).
8. **Rule 9 kind set**: the decompile (L635–644) gives kinds ∉ {7, 8, 23}, because kind 24 passes the `|| kind == 0x18` arm. The brief (report §5 rule 9) says ∉ {7, 8, 23, 24}. I follow the decompile, and the walk test does not pin kind 24.
9. Repo has no changelog. `docs/re` is not edited (RE archive pipeline).
