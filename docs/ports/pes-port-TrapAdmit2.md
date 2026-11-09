# PES port: TrapAdmit2: gate 4a refused every man in a GF clip because GF's "no motion" value was 0xBE5, a real motion; fixed (PES's none index; breaker red 633 -> 138 ticks, the rest is the touch); action rows cite their PES site; ids stored by the switch's rule; 7 handlers + the gate re-proven on REAL calls (09-11-26)

Branch `TrapAdmit` (worktree `gf-TrapAdmit`), final tip `5aa090fc` (`git ls-remote origin TrapAdmit` = `5aa090fccfe1bbd42b519fcfe6f219d848233c1b`), base `add876c4` + `origin/main` `98469b7d` (docs only). Image `pes_full.bin`. CONFIRMED = a tool run this session; INF = inference; OPEN = blocker named.

## 0. Verdict against the acceptance
| Acceptance | State |
|---|---|
| diagnose the 633-tick breaker red: admission or contact | **both, in different stalls** (section 1). 2,806 of 4,293 slow-ball candidates were MOVE men refused because `P+0xAE0` held motion 0xBE5 (admission, mine): fixed at the root, 633 -> 138 firing ticks. The 138 left are ONE stall of an admitted man in the Trap action who never touches the ball: handed to TrapContact2 with the evidence (message sent) |
| GF admits a trap exactly when PES's gate 4a does with PES's own action ids | **MET with named limits** (section 3): rows cite the `FUN_1406EEBB0` site or say "command commit"; header, trip and interfere are INF; MOVE vs STEP_MOVE not derivable from the movestep state (17 real calls) |
| handlers proven on every real call available | **MET** (section 4): STAGGER, FALL_DOWN, DODGE, JOSTLE, `0x1407E8E10`, MOVE, STEP and the gate `0x14079BBE0`; the four that never fired stay SYNTHETIC; Main told to release the pin (done by Main) |
| four mutations, red probe red | all four plus three more killed (section 5); the probe's red on a build of `origin/main` was NOT run (mutation (b) restores main's body refusal: 28 of 75 starts refused, red) |
| `GATE branch` GREEN apart from census-only drift and reds owned elsewhere | **`GATE branch: RED pes_restart_delay_test pes_throwin_release_test`** on the final tip `5aa090fc`: 2313 of 2315 pass; the two reds are the contact stall (TrapContact2) and a throw-in seed drift (census-owned); section 6 |

## 1. Diagnosis (CONFIRMED by a temporary `printf` in `PlayerController::_ReceiveCandidate`, reverted)
Every candidate within 3 m of a ball slower than 1 m/s, with the man's `P+0xAC8/0xAC9`, motion `P+0xAE0`, phase `P+0x245C`, the gate's answer; seed 7, pes-loop, 2 min, tip `add876c4` + probe: 4,293 candidates, 6 breaker reports.

| class | count | meaning |
|---|---|---|
| refused, MOVE (2), motion 0xBE5 | 2,806 | the body plays a GF-own clip; `TrapFactsOf` wrote the "no motion" value `kPNoMotion = 0xBE5` |
| refused, SLIDING (0xB), motion 0xBE5 | 437 | GF's slide clip |
| refused, TACKLE (0xD) | 244 | PES refuses a tackle too (0xD is not in `0x14079BBE0`'s list): correct |
| refused, LONG_PASS (8) / SHOOT (0xA), motion 0xBE5 | 93 / 77 | GF's kick clips |
| refused, DODGE (0x15), PES motion 0x13 | 90 | PES's own handler (cancel point not reached): correct |
| admitted / already in the Trap action | 473 / 73 | |

Five of the six breaker reports (t = 221 s, 421 s, 435 s, 450 s twice) have only refusals in the 6 s before them (men A9, A1, A5, A2: MOVE, motion 0xBE5, phase 12 = RUN_TURN_RUN, GF's `000_accel` clip restarted every tick: StepLand's open MOVESTEP loop keeps the man in a GF clip). The first (t = 186 s) is mixed (A6 DODGE then GF slide; H4 alternating PES motion 0x8C8 (admitted) and 0xBE5 (refused)).

Why: `0xBE5` is the Animetable's CELL row count (`FUN_1406AA440`, `kAnimetableCellCount = 0xBE5 * 0x12`), not "no motion". As a motion number it is a real idle motion: row 0xBE5 of the motion table (`seq050`: `[[0x1436F23C0]] + 0xBE5 * 0x390`, `+5` = 3 cancel entries, entry 0 word 109541, `>> 13 & 0x3FF` = 13 half-frames, about 23 ticks) and it is in the class list of `FUN_140A6E290`, so MOVE's slot +0x70 handler `0x140782570` compares that cancel time with the elapsed ticks of the motion table `P+0x354`, empty for a GF clip (`FUN_1406E68C0` answers 0 for a motion not in the table, decompile): refused for as long as the GF clip plays.

**Post-fix census** (same seed, `runA.log`, probe still in that build): 5,402 candidates, 4,113 admitted, 927 in the Trap action, 362 refused (tackle 177, dodge 76, GF shoot/pass clips 109: all actions PES's gate refuses or whose cancel point is not reached). One breaker report (t = 415 s): A3 is admitted every tick from 2.65 m, a trap starts at 2.65 m and ends after 5 ticks (motion 0xDE6), the second trap starts at t = 414351 at 0.86 m (motion 0x21D, phase 2), and the ball stays untouched while the breaker fires 138 ticks. **That is admitted-and-never-touched: the contact half (`FUN_140799060`, TrapContact2).** The slow approach (A3 walks 2.6 m in 2.2 s) is MOVESTEP's, not the gate's.

## 2. Fix (root)
`pes::kPNoMotion = kMemberNoneIndex` (0xFFFF, the "none" index of the member's motion cells, `FUN_141EB42B0`), past the 0x1125-motion table: no class predicate names it (`FUN_140A6E290` 0xBE7/0x8C7/0x8CB/0xB4F/0xBE5/0xBE9/0xC08/0xC0F, `FUN_140A6E2E0` 0xC09 and `(m - 0xC0D) & ~5 == 0`, `FUN_140A6E4D0` 0xBF0/0xC0A/0xC13, the STEP handler's 0xC0A/0xC0D/0xC11/0xC13) and `FUN_1406AFBC0` reads the default record (`param_1[1]`) for it. `// ponytail:` at the constant: GF's own clips stand in for PES motions; MoveStepCell issuing a PES motion for every locomotion request removes the case. Test `PesAnimeAction.ABodyInAGfClipHoldsNoMotionOfPesAnimetable`.

## 3. The body's PES action id
- **PES's writer** (CONFIRMED, `callers.py` over the image: every direct call of `FUN_1406EEBB0`, 125 sites): 117 pass the keep sub `0x5E` (compared at `0x1406EED32` / `0x1406EED5C`), five compute the sub id (`0x1406B533A`, `0x14072EA3B`, `0x1407E707D`, `0x1407EBB93`, `0x1407C4858`), three leave `r8` as an earlier call set it (`0x1407976AA`, `0x1407A3934`, `0x1407D5377`). With a keep sub the stored sub id is the action itself. **49 of 49 captured `P+0xAC8..0xACB` quadruples have sub == action in both pairs** (`pdump.py`, `live2/pid3332804`).
- Constant-action sites per id (names from `0x1434A5470`): MOVE 65, STEP_MOVE 5, DRIBBLE 3 (`0x140743E59`, `0x14077CD2B`, `0x1407A3934`), DRIBBLE_SIDE_BACK 6, DRIBBLE_STEP_MOVE 1, TRAP 2 (`0x1407B0D57`, `0x1407B35E6`), SHORT_PASS 1 (`0x1407822CB`), FEINT 1, THROWIN_LOOP 2, THROWIN 1, WALL_LOOP 1, DEMO_MOVE 6, DEMO_INPLAY_CONTACT/KICK_FAILED/MOVE 1/1/3, DEMO_COMEBACK 1, LINESMAN/LINESMAN_MOVE 1/1, ANIME_DATA_DRIVEN 2, and 21 sites with a computed action. **No constant site writes LONG_PASS, SHOOT, SLIDING, STAGGER, FALL_DOWN, DODGE, TACKLE or the keeper ids: they enter through the pending-command commit (`FUN_1406EEA90` at `0x1406EEB78`, `edx = cmd[1]`, the AI's action byte)**, so their rows are the command GF's body runs, by the table's names.
- Code: `AnimeActionOfClip` cites a PES site per row (and says "command commit" or INF where no site exists: header -> SHOOT, trip/interfere -> TACKLE are INF); `pes::ActionIdsAfterSwitch(now, request, sub)` is the switch's four stores (same-action arm `0x1406EED32..0x1406EED54`, action-change arm `0x1406EED5C..0x1406EED88`, then `0x1406EED8F`) and `TrapFactsOf` calls it instead of its inline previous-pair logic. Not done: running the translated switch itself on each GF action change (its exit/enter virtuals of the other action objects are proven only for MOVE<->2/3, MOVE->TRAP, TRAP->4/8/9, so a general call would abort loudly on the rest): OPEN.
- **MOVE vs STEP_MOVE: not derivable from the movestep state** (the brief said "if PES's mapping says so"; it does not). The 17 real gate-4a calls with a MOVE or STEP_MOVE man: MOVE at phases {0, 3, 5, 7}, STEP_MOVE at {1, 4, 5, 12}; phase 5 holds both ids (`seq243` MOVE, `seq137` STEP), and STEP_MOVE sits in the run-turn phase 12 (`seq216`) and the idle phase 1 (`seq071`). The choice is the AI command's action byte `cmd[1]`; OPEN: its writer. GF stays MOVE (MOVE's handler is the stricter of the two).

## 4. Real-call proofs (CONFIRMED, `prove.sh` -> `prove_bounded.py --merge`; `snapdiff --bounded`: return value and every written byte, PES's bytes against the translated closure; synthetic states over `seq050`, deleted after each replay)
| root | calls | own-block | REAL | SYNTHETIC |
|---|---|---|---|---|
| `0x14072E450` STAGGER | 11/11 | 15/34 | 6 (`seq1050 1053 1058 1062 1066 1168`) | 5 |
| `0x14072E360` FALL_DOWN | 11/11 | 14/17 | 6 (`seq1565 1570 1572 1577 1581 1584`) | 5 |
| `0x14073B680` DODGE | 11/11 | 34/112 | 6 (`seq1452 1453 1455 1456 1457 1459`) | 5 |
| `0x140779DF0` JOSTLE | 11/11 | 29/71 | 6 (`seq226 256 258 260 263 265`) | 5 |
| `0x1407E8E10` DEMO_INPLAY_MOVE | 9/9 | 35/43 | 4 (`seq2103 2109 2110 2112`) | 5 |
| `0x140782570` MOVE | 11/11 | 10/19 | 6 (`seq050 057 062 068 072 075`) | 5 |
| `0x140789E60` STEP_MOVE | 11/11 | 6/15 | 6 (`seq051 060 064 071 074 076`) | 5 |
| `0x14079BBE0` the gate | 6/6 | 8/12 | 6 (`seq049 073 117 137 216 243`) | none |
All 0 mismatches; coverage recorded, not a gate. Still SYNTHETIC (never fired): `0x140787EC0` SLIDING, `0x140744090` KP_DROP, `0x1407C6F40` DRIBBLE_SIDE_BACK, `0x1407E8D20` demo contact. `0x140724820` (the pass handlers) was not re-proven this run (it keeps TrapAdmit's 1 REAL + 5 SYNTHETIC proof; real captures `seq045 046 048` are unpacked and could be added). `0x1406B4C10` (trampoline, no hook): SYNTHETIC 8/8. New proof root `0x14079BBE0` (spec `tools/dev/diffharness/specs/trapaction/14079BBE0.json`, set `closure-trapadmit_14079BBE0.txt`); `gen_translated.py` moved its translation from `closure/` to `trapdribble/` (a recorded function now). Layer 5668 functions (3772 recorded, 1896 closure), `ledger.py` run.

## 5. Mutations (CONFIRMED runs: each built, run, reverted)
| mutation | killed by |
|---|---|
| (a) `carrier ? Dribble : Move` -> `Dribble` | `PesAnimeAction.OnlyTheHolderIsInDribbleBecauseThePesSwitch...` red |
| (b) restore the `PlayIssuedClip` trap refusal (= main's body refusal) | `PesStall.NoPesTrapStartIsRefusedByTheBody`: **28 of 75 PES trap starts refused by the body**, red |
| (c) drop the `RunTrapAdmit` call | `PesStall.NoTrapStartsFromAnActionGate4aRefuses` (new probe): **1 of 66 starts left a tackle/none/keeper action**, red |
| (d) Sliding -> MOVE | `PesAnimeAction.ABallActionIsNeverTakenForMovementOrForTheTrapItself` red |
| (e) `kPNoMotion` back to 0xBE5 | `PesAnimeAction.ABodyInAGfClipHoldsNoMotionOfPesAnimetable` red |
| (f) a changed sub of the same action keeps the previous pair | `PesActionSwitch.AnExplicitSubIsStoredAndAChangedSubMovesThePairToo` red |
| (g) a keep sub stays 0x5E | `PesActionSwitch.AnActionChangeMovesTheCurrentPairToThePreviousOne` red |
(a), (d), (e), (f), (g) were applied together in one build: each failing test is the one named, the other two of the group passed. Unmutated: `pes_receive_test` 58/58 (7 new). Unmutated stall runs on the final logic (probe build, 3 processes): `PesStall.ATrapClipIsNeverRestartedBeforeItsContactFrame`, `AHolderIsNeverHandedANewTrap`, `NoPesTrapStartIsRefusedByTheBody`, `NoTrapStartsFromAnActionGateRefuses`, `TheBreakerNeverForcesABallUnderAnArmedRestart`, `ANoManIsEverHandedNoCommandAtAClipEnd` pass; `ALiveBallNeverLiesStillLongEnoughForTheBreaker` red, 138 ticks (was 633). The red probe `NoPesTrapStartIsRefusedByTheBody` needs the gate-4a body change, so a build of `origin/main` plus the probe was not made; TeamTick7's 84 of 113 stands as main's number.

## 6. Survey, smokes, gate (CONFIRMED, final tip `5aa090fc`, tree clean)
- `GF_PES_SURVEY` (seed 7, pes-loop, 2 min): **empty** (no untranslated target reached; `survey.txt` holds only the header).
- Smokes (seed 7, 2 min; baselines on main: switch-off `a0fbe2d5cd445a52`, pes-loop `17941efc16b4a6a5`): switch-off **`3fb16a37fcc8fc30`** (27 stoppages, longest 23.2 s, 29,951 ticks), pes-loop **`f218ef0f8bc8e22c`** (20 stoppages, longest 23.7 s, 31,570 ticks). Both moved. Attribution NOT run (no ablation): the tree carries WhistleBall (Main said it moves the pes-loop hash) and this change (`kPNoMotion`, ids); the pes-loop move is expected from gate 4a and the none index, the switch-off move is unexplained (OPEN: ablate WhistleBall alone). Neither run aborts, both finish with bounded stoppages.
- **`GATE branch: RED pes_restart_delay_test pes_throwin_release_test`**; ctest 2313 of 2315 passed (8,983 s); recorded re-check selected 24/245, 7/685, 10/546, 183/2253, 16/34 sets and passed; ledger current (38,352 rows), no status regression, off-path ports 120 allow-listed.
  - `pes_restart_delay_test`: only `PesStall.ALiveBallNeverLiesStillLongEnoughForTheBreaker` (138 ticks): the contact half, TrapContact2.
  - `pes_throwin_release_test`: `PesThrowInRelease.AThrowInIsReleasedFromInsideTheLine` ("1 of 2 released throw-ins lay outside the touchline at the touch"): a seeded-match family whose first searched match changed trajectory; DRIFT for the merge-lane census (not re-staged here).

## 7. Left (numbered, exact resume)
1. **TrapContact2: the 138-tick stall** (section 1, post-fix census): A3 admitted and in the Trap action at 0.86 m, motion 0x21D, the ball untouched while the breaker fires. Log `gf-TrapAdmit-scratch/runA.log` (`grep DBGADMIT`, `[stalemate]`).
2. Merge-lane census: `pes_throwin_release_test` seeds (above), and the two smoke moves attributed by ablation (this change vs WhistleBall).
3. OPEN decodes: the writer of the command byte `cmd[1]` (MOVE vs STEP_MOVE and every non-constant action id, section 3); running the translated switch on each GF action change (needs the other action pairs' exit/enter closures).
4. `0x140724820` could take its three unpacked real captures; `0x140787EC0`, `0x140744090`, `0x1407C6F40`, `0x1407E8D20` need hooks to fire (they never did in the launch).
5. Red probe against a real `origin/main` build (optional; mutation (b) covers the ingredient).
6. Scratch `gf-TrapAdmit-scratch` (logs, `callers.py`, `pdump.py`, mutation binaries `test_mutB`, `test_mutC` 51 MB each): delete when this report is archived. No synthetic states are left; no capture was unpacked by me.
