# S1Port round 2: Safety gates split and fixed, NearestList consumers cut over, K+0x6E track arm ported

> **Main audit 19-10-26:** `0x14063AA79` `movzx ecx,byte [rax+0x31]`, then `divss [0x14253EE30]` (100.0) and `comiss` against `movss xmm0,[rdi+0x44]` at `0x14063AA74`: matches (P+0xA1 given RAX = P+0x70, the agent's reading). Correction: OneTwo is not held by G3 (ported since A4LaneInputs); per A4Level's probe it dies at G12.
> **Main audit 19-10-26 (round 2, `90c94a3`):** `FUN_140587E70` calls `0x140A714C0` (Safety gate 4 reads the score row): confirmed. Safety now dies at gate 5 (backward mates; PES candidate set not compared). B+0x4C writer open (handed to RE for a live watch).

Branch `S1Port`, head `90c94a3` (`git ls-remote origin S1Port` = `90c94a3db78db8d09e06373e2640b171093449ad`). History: `842a311` (rows, flag), `9b4f4e2` (writer ticks in every team tick), `c34bd79` (comments), `756a6ab` (item 3), `90c94a3` (items 1 and 2). Base `b64d2b7`.
Tags: CONFIRMED = decompile, asm or live read, with the VA; INF = inference; OPEN = blocker named.

## 1. Item 3: Safety per-mate gates

Live split (2 min, seed 7, 1 vs 2, `--pes-loop`), before any fix: gate 4 (`sfOppD2Min` 225 <= d2) killed 71, gate 5 (scored limit) killed 11, and no mate reached a later gate. The 11 gate-5 mates are the ones that passed gate 4.

Fixes, each red first (tests in `pes_carrier_test`, `pes_scorerows_test`):

1. **Gate 4 read the wrong quantity (GF input error).** GF used the mate's nearest-opponent d^2. PES reads `0x140587E70(T18,k,q)`, which returns the S1 score row entry (CONFIRMED): it calls `0x140A714C0(T18,k)`, which returns `T+0x228EC` (score0) for `k == 1` and `T+0x22AD4` (score1) otherwise. A4G12 confirmed the chain from raw asm (`0x142128D70 CVTTSS2SI EDX,[RAX+0x14]`, `0x140A714C0: CMP DL,1; JZ`). `SafetyMate::score` now carries it. `nearestOppD2` is kept for G1's 35 m test and the tiebreak. Gate 4 kills went 71 -> 1.
2. **Row sign inverted in my previous commits.** G5, the DHB score and the Safety gate length were using the opposite row. Now `ScoreRowForSign(rows, k)` (score0 for k == 1) feeds G5, the DHB and gate 4; `GateRowForSign(rows, k)` (`0x140A714A0`, the opposite row) feeds the Safety gate length. `k` = `int(C[5])` = the attack sign (+1 = attacking +x, consistent with `AttackAngleDeg(+1) = 90`; A4G12's asm reading agrees).
3. **Mates were indexed by `GetID()`, which is the match's global `AllocatePlayerID()` counter, not a lane slot.** Every lane unit used it as one. `laneSlotOf` now returns the index in the team's active list (own 0..10, opponents 11..21), the same numbering `fillSide` uses. Applied to all mate records (GoalFront, BackSpace, Safety, OneTwo/DHB receivers). Not unit-tested: verified by the probe only.
4. **`ShortLimit` followed no PES rule for two cases** (rule from `docs/re/pes-re-carrier-helpers.md`, Safety limit, `0x1421271B7-1D2`, marked CONFIRMED there; I did not re-decompile it this turn). A keeper keeps the default need 135, so lim = max(135, 45); a non-keeper with need > 135 gets lim 45, not the cap. Red tests: `AScoredNeedOverTheCapIsHeldToFortyFiveDegrees` (expected reject, got accept), `AKeeperKeepsTheDefaultNeedAndIsHeldToItNotTheFloor` (expected accept, got reject).

Gate tallies after the fixes (same smoke, instrumented, reverted before commit):

| Stage | gate 4 | gate 5 | gate 6 | gate 7 | gate 8 | passes |
|---|---|---|---|---|---|---|
| before any fix | 71 | 11 | 0 | 0 | 0 | 0 |
| after gate 4 fix | 1 | 79 | 2 | 2 | 9 | 0 |
| after limit fix | 1 | 82 | 1 | 2 | 7 | 0 |

**What still kills the Safety unit (gate 5, 82 rejects).** Value probe on the 82: turn (`recAVsFacing`, |bearing(pass) − carrier facing|) mean 147.6 deg, limit mean 101.4, distance mean 21.8 m, and turn > limit in all 82. These are backward passes. The gate itself matches the rule. The open question is GF's candidate set: `rethinkCandidates` admits teammates behind the carrier, and PES's Safety candidate list is not compared yet (OPEN, item 3 below). I did not change the candidate set. My class and keeper fields in that probe were printed with a mismatched format and are not cited.

Verdict: Safety is silent for GF's candidate set, but no gate is wrong by decode now. Gate 4 and the limit rule were the two real input and port errors.

## 2. Item 2: the other NearestList consumers cut over

- `pesoffball`: `BuildNearest` and `NearestEntry` are gone. `RunWorld::nearestRows` points at the two team AIs' S1 rows and is read live. `NearestOpponents` maps a row's lanes to run-world ids (own team 0..10, other 11..21) and returns the opponents' block, nearest first. Rows are the writer's (`BuildScoreRows`), so the owner is excluded from his own block and inactive slots are absent, as before.
- `pesmovestep` `NearestOpponent`: goes through `NearestOpponents` (live rows). The remaining stand-in is named: a substitute has no run slot until the next in-play build (RunSlotOf's roster).
- `peskeeper`: the keeper's teammate pick is the first team-mate in his own NearestList (block 0, nearest first) with d^2 under 100.0. The threshold comes from `0x14253EE30` (the value of `sfCeilOpp_m`). The carry point is still computed for the walk target. The nearest-player stand-in is gone.
- Include cycle found and broken: `pesoffball.hpp` -> `pesscorerows.hpp` -> `peslane.hpp` -> `pesoffball.hpp`. The lane constants now live in `pesscorerows.hpp`, and `peslane.hpp` includes it.

INF: the keeper pick (`docs/re/pes-re-keeper-port.md` 2.2) reads "the first whose value < 100.0" in block 0. Its block index and what the value is are INF. The GF comment said "10 m of space", which I did not implement.

TDD: no red-first test for the cutover. The existing NearestList tests were ported to the rows and stayed green (offball 62/62). The cutover is verified by the probe.

## 3. Item 1: K+0x6E ball-track arm

CONFIRMED:
- The arm (`0x14063AB01..0x14063AB06`): K+0x6E is set when `byte(B+0x4C) != 0` and `B+0x80 == 0` and `r14 == 0`. Here `B = *(G+0x40)`, `FUN_140A6D0D0` = `B+0x4C`.
- `r14` = `d1 < d2`, where `d1` = `FUN_140A8F890(rec0, B+0x1C)` and `d2` = `FUN_140A8F890(rec0, K+0x40)`. `FUN_140A8F890` is the squared 2-D distance `(p1[2]-p2[2])^2 + (p1[0]-p2[0])^2`. `rec0` = `FUN_140A57800(B)` = the track record 0 (B+0x88).
- **Correction to the GF input comment:** K+0x40 is not the ball. Decompile lines 166–232 of `FUN_14063A490` write K+0x40 from several branches: the default is `MA+0x2482C`, the others are the track record, the carrier, or image-A points. `dhb.ball` (GF) is an assumption.
- B+0x4C is a runtime 0/1 flag: live (first sampler, 29 valid samples, pid of the running match) it was 1 in 5 samples and 0 in 24. B+0x80 was 1 in 12 and 0 in 17, and was 0 in all five samples where B+0x4C was 1.

OPEN:
- **The B+0x4C writer is not found.** Static search: no store to +0x4C or +0x80 in the nine prediction functions (`0x140865f20`, `0x140866b60`, `0x140868500`, `0x140868ad0`, `0x140869500`, `0x140869d10`, `0x14086a3f0`, `0x14086ab90`, `0x14086b690`), nor in their dispatcher `FUN_140865d70` or its four callers (`FUN_140425010`, `FUN_1406e2f4b`, `FUN_140810155`, `FUN_1408182d8`). The flag is probably written by a block copy, which a store search cannot see. The integrator's `[rdi+0x4c]` (`0x140876a86`) is a different object.
- B+0x1C (`FUN_140A6CF30(B,0)`) has no GF state; K+0x40 needs the branch logic at lines 166–232.

Port: `VolatileBall` implements A ∨ (flag ∧ ¬b80 ∧ ¬r14), with squared 2-D distances. The flag is false in GF (ponytail naming `0x140A6D0D0`'s writer as OPEN), so the arm is off. Test `TheTrackArmSetsItWhenTheBallIsNotNearerItsAnchor` was red (value, at its second assertion) before the arm, and green after.

## 4. Silence, final probe (`--pes-loop`, 2 min, 24 rethinks, probe line removed before commit)

| Unit | answers | notes |
|---|---|---|
| Safety (6) | 0/24 | gate 5 kills every candidate (section 1) |
| OneTwo (10) | 0/24 | G5 is live: value 3122–3351 (own-goal row); G12 is A4Level's |
| BackSpace (3) | 0/24 | |
| DHB (11) | 0/24 | K+0x6E = 0 in 24/24 (arm A rare; arm B's flag is OPEN) |

No NaN in any row value. G5 values are finite.

## 5. Smokes (2 min, seed 7, 1 vs 2)

- Clean baseline `b64d2b7`: switch-off `75a11cdb2b3c8ec5` (exit 0), `--pes-loop` `9fc0b08314812d8f` (exit 0, 37735 ticks).
- `90c94a3`: switch-off `75a11cdb2b3c8ec5` (exit 0, unchanged), `--pes-loop` `9fc0b08314812d8f` (exit 0, unchanged). The walk did not change in this window: no unit answered.

## 6. Tests

- `pes_carrier_test` 162/162 (new: `TheShortGateReadsTheScoreRowNotTheNearestOpponent`, `AScoredNeedOverTheCapIsHeldToFortyFiveDegrees`, `AKeeperKeepsTheDefaultNeedAndIsHeldToItNotTheFloor`, `TheTrackArmSetsItWhenTheBallIsNotNearerItsAnchor`).
- `pes_scorerows_test` 7/7 (new: `TheRowOfAnAttackSignIsTheGoalBehindTheAttack`; its stub was red by value).
- `pes_offball_test` 62/62 (NearestList tests ported to the rows; no red-first, see section 2).
- `pes_lane_test` 21/21.
- Mutations: the pre-fix runs of the gate-4 and limit tests are the mutations (the old input and the old cap were the mutated code, both red). The sign selector was checked by its stub (red by value). No further mutation runs this round (budget).
- Gate (full ctest): not run, Main gates.

## 7. Open items

1. **B+0x4C writer** (item 1). Suggested next: find the block copy into the BallInfo object (`movups` of 16 bytes at +0x40..+0x4F) in the prediction loop's caller, or a live write watch when a match is running (the sampler found the pointer valid only intermittently).
2. **B+0x1C and K+0x40** (item 1): meaning and the branch logic at decompile lines 166–232 of `FUN_14063A490`.
3. **Safety candidate set**: GF's `rethinkCandidates` admits backward mates (turn mean 148 deg). Compare PES's Safety candidate builder before touching the gate.
4. **Keeper pick**: block index and value semantics (`pes-re-keeper-port.md` 2.2); decompile `0x1409E8880` (the bridge found no function at that address).
5. **Substitutes**: RunSlotOf's roster vs the writer's active-list lanes (ponytail in `pesmovestep.cpp`).
6. **G12 / OneTwo**: A4Level owns it (A4G12 is working it on the merged tree).
7. A4G12 has merged `756a6ab` into A4Level (`d442365`); re-merge `90c94a3`.
