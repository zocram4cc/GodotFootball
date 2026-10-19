# A4Lanes3: OneTwo (kind 10) and DirectHighBall (kind 11) ported and wired behind pes_loop; Safety length decoded (stays a stand-in)

Branch `A4Lanes3`, worktree `<workdrive>/gf-A4Lanes3`, base `origin/A4Lanes2` `7cac846`. Pushed; `git ls-remote origin A4Lanes3` = `43c16ae`. Probe reverted; tree clean at `43c16ae`.
Spec: `pes-re-lane-units2.md` (§0 floats, §2 shared callees, §3 OneTwo, §4 DHB, §5/§6 port specs, §7 stand-ins, §8 corrections). Image rows: `pes-re-image-rows.md`. Lane channels: `pes-re-lane-structs.md`.
Files changed: `src/sim/onthepitch/pescarrier.{hpp,cpp}`, `src/sim/onthepitch/player/controller/elizacontroller.cpp` (carrier path only), `tests/sim/pes_carrier_test.cpp`. Not touched: `peslane.*` (ScanLane already implements the teammate branch), referee, pessequencer, pesstoppage, ball, match.hpp, humanoid.cpp, pesoffball, teamAIcontroller, pesstyle.

## 0. Verdict

| Unit (walk kind) | Gate VA | Status |
|---|---|---|
| OneTwo (10) | `0x14212AEB0`; picker `0x142126A80`; one-touch `0x142128C10`; receiver `0x142124950` | **Ported, tested, wired** (`1cd4281`, `4de9ffd`). Live: silent, closed by named stand-ins (§4, §5) |
| DirectHighBall (11) | `0x142129BE0` | **Ported, tested, wired** (`39abe7e`, `4de9ffd`). Live: silent, closed by one named stand-in (§4, §5) |
| Safety length (6) | `0x14212B71D`-`0x14212B76F` | **Decoded** (§3). Exact value needs the score row's writer (OPEN, S1). Stand-in kept, fixture check added (`43c16ae`) |

Acceptance:
- `pes_carrier_test` **136/136** (base 113: +10 OneTwo, +12 DHB, +1 Safety fixture check). `pes_lane_test` **21/21**. Re-run on the reverted tree.
- Switch-off 2-min hash **`a3d5e43a3b7b03af`** (unchanged from base).
- `--pes-loop` 2-min: exit 0, hash **`a817a13303958f90`** (= base `72f5977`, = A4Lanes, = A4Lanes2), shots 0-0 (= base), **passes 10-10** (`[balance-passing]`, = A4Lanes).
- None of the five lane units answered in the probed run (0 answers in 29 rethinks; §5).

## 1. Commits and mutations

| SHA | Content | Tests |
|---|---|---|
| `11d9289` | RED: inputs, install constants (lane-units2 §0 VAs), stub units | 13 fail on behaviour, 122 pass |
| `1cd4281` | OneTwo port: closing sweep, one-touch G1-G12, receiver predicate, picker | OneTwo 10/10 |
| `39abe7e` | DirectHighBall port: low-ball block, mate loop, branch A, 9/27-frame gates, `|X|`, teammate lane, f10 | DHB 12/12; carrier 135/135 |
| `4de9ffd` | Wiring: controller builds the inputs from the rethink records; kinds 10 and 11 dispatched | carrier 135/135; gfsim_run links and runs |
| `43c16ae` | Safety length comment (decode and verdict); PES-shaped fixture check | carrier 136/136; lane 21/21 |

Mutations (each run, rebuilt, reverted, re-run green):
1. OneTwo angle gate `|d| <= 90` made strict (`<`): **caught** by `OneTwoUnitTest.TheAngleGateIsInclusiveAtNinetyDegrees` (1 failure). Reverted; 10/10.
2. DHB score floor `score < 225` made inclusive (`<=`): **caught** by `DirectHighBallUnitTest.TheScoreFloorIs225ExactlyKept` (1 failure). Reverted; 12/12.

## 2. Decoded facts used (VAs; CONFIRMED = asm read, INF = named inference)

- **Closing sweep** `0x140A4E720` → `0x140A4E3F0` (CONFIRMED, spec §2.1): frames `int(54·0.3+0.5) = 16` (`0x142590D98`); outer 6 m (`0x14259BDEC`); inner 0; bearing threshold half of 180 (`0x142598580`). Counts opponents of the reference only.
- **One-touch G1-G12** `0x142128C10` (spec §2.2; G1-G4, G11, G12 asm-checked there): G3 `level/100 >= 2`; G4 mode 3 or 4, or category ≥ 4; G5 score ≥ 1600 (`0x14259CE40`); G8 cap `min(2, base + inc)`; G10 speed 15 (`0x14259BE0C`) and heading 100 (`0x14253EE30`); G11 lane 15 m / cone 60 / radius 2 with `h = 0` (`0x142129025`-`0x142129076`); G12 `-10 <= k·(x - curve) < 0` (`0x14259BE84`).
- **Receiver predicate** `0x142124950` (spec §2.6, lane-core §8): P0 `r ≤ 21`, `r ≠ c`; P1 b6c status 0xB; **P2 ψ = snapshot(r)[0xD] ≠ 0 is a gate** (CONFIRMED asm `0x142124A83` `CMP [RSP+0x54],0` and `0x142124A8B` `JZ 0x142124AD8`: a zero exits). P3 `player(r)+0x5D0 != 0x25`. Band 1: difficulty ≤ 1.0 beyond 15 m or past 110°, else ≤ 2.0. Angle block (`K+0x6C`, mode 3/4, `C[0xF]`, `NL(c,r) >= 36`). Window (base 10 or 5 by `A+0xE4 < 120`; ramp by `A+0xE8`; `3 <= dist <= max(u, 3)`). M tests (`< 120`, `> 20`) and openness class > 2.
- **OneTwo picker** `0x142126A80` (CONFIRMED, spec §2.3): the first listed teammate the receiver predicate takes. Answer `(2, t, b = 0, f10 = 1)`.
- **DHB** `0x142129BE0` (CONFIRMED, spec §4, `asm_dhb_full.txt`): `K+0x6E == 0` → none (`0x142129CC8`). Low-ball block when `K+0x44 <= byte/100 + 0.1` (`0x142588200`): the nearest opponent of the ball within 5 m (`0x14259BE20`, d² 25) and not mid. Mates: not keeper, b6c valid, class ≥ 1; score floor 225 (`0x14259BE70`); branch A below 900 (`0x1425C65F4`) skips a mate whose nearest opponent is under 10 m (`0x14253EE30`, kept at exactly 100); 9-frame gate (`0x14259D60C`); best by the 27-frame distance (`0x142565600`, 0.5). `|X| > 80` (`0x142596E6C`) leaves b = 0 with f10 unwritten. Free when mark count 4, or mark count not 0/2 with speed > 15 (`0x14259BE0C`); otherwise the teammate lane: origin 4 m (`0x1425656C0`) along `wrap360(table + 180)`, `ScanLane(side = IsAway(c), ref = c, 15 m / 60° / 5 m, h = 13)`. `f10 = one-touch AND receiver(best)`.
- **The teammate ScanLane branch is live** (lane-structs §1). `peslane.cpp` already implements `opponent = side != isAway(reference)`, so DHB needs no core change.

## 3. Safety gate length `0x14212B71D`-`0x14212B76F` (decoded; not resolved exactly)

CONFIRMED (`objdump` of `pes_full.bin`, `0x14212B6F0`-`0x14212B780`):
- `edx = int(C[5])` (attack sign), `ebx = C[0]` (carrier slot), `call 0x140A714A0(rcx = T18, edx = k)`.
- `0x140A714A0` (CONFIRMED asm `0x140A714A0`-`0x140A714B3`): `rax = T18+0x22AD4` if `k == 1`, else `T18+0x228EC`. That is the **opposite-sign** score row. `0x140A714C0` (the score reader, `0x140587E70`) is the same pair with the sign the other way.
- Rank: `ecx = byte [row + 0xB2 + c]`, plus 11 when `c >= 11` (`cmovl rdx, rsi`; `rsi = 0` from `xor esi,esi` at `0x14212B535`).
- Length: `xmm8 = float [row + 8·(rank + 11·(c>=11)) + 4]`, the **d² of the carrier's own entry**, then `len = sqrt(xmm8)` (`call 0x1415A1780`).
- So the decoded quantity is the carrier's own d² in the opposite-sign score row, **not** the nearest opponent. The lane-structs doc §3.3 said "INF: distance to the nearest opponent"; the asm contradicts it.
- The row's writer is OPEN (S1; image rows §3.3). Without it the length cannot be exact, so **the stand-in stays**: the nearest opponent's distance plus the radius (`k.sfLaneRad_m`). The comment names the decode and the removal site. The same S1 row feeds the one-touch G5 score and the DHB score.
- Why the radius: with the length at exactly the nearest distance, the strict outer bound (apex to rim) excludes him; the radius keeps him on the lane.
- **Fixture check.** The blocked-lane tests put the blocker 5 m out on the lane's axis, which the stand-in records by construction. Added `AnOpponentOffTheGateLaneLeavesTheLaneFree`: the same nearest opponent at 9.4 m, 45° off heading (past the 40° half cone), leaves the lane free. The cone decides; the length alone does not.

## 4. Stand-ins that close OneTwo and DHB live (named; none tuned)

- **OneTwo G3** (team level / 100 ≥ 2; `T+0xA294/A29C`; writer OPEN, O12): `teamLevel = 0` → G3 closes every one-touch.
- **OneTwo receiver P2** (`ψ = T(side)+0x4BD4[r%11] ≠ 0`; writer OPEN, S6): `psi = false` → every receiver closes.
- **DHB K+0x6E** (volatile / volley-header flag; no GF state): `volatileBall = false` → DHB closes at its first gate.
- Also stand-ins (they do not close a gate live; each changes outcomes once a gate opens): team mode 2 (S4); score S1 = squared distance to the attacked goal's centre; curve S2 = the most advanced opponent's x; A_tab and M tables = bearings (O9); angle table S10 = facing; mark count S11 = 0; first-time bit S12 = false; height S13 = carrier stat 0x31; one-two run S7 = none; C[0xF] S18 = false; K+0x34 S9 = facing (INF); history S17 = current; team line S14 = own goal line; pitch bound S8 = half length; list row S3 = carrier's nearest teammates.

## 5. Smokes and the silence probe

**Smokes** (2 min, seed 7, `teams.sql` 1 vs 2, `--pes21-root <workdrive>/PES21`):

| Run | Hash | Passes | Shots | Exit |
|---|---|---|---|---|
| switch-off (`43c16ae`) | `a3d5e43a3b7b03af` | (GF path) | (GF path) | 0 |
| `--pes-loop` (`43c16ae`) | `a817a13303958f90` | 10-10 | 0-0 | 0 |
| `--pes-loop` (base `72f5977`, A4Lanes) | `a817a13303958f90` | 10-10 | 0-0 | 0 |

**Probe** (temporary `PROBE` counters, insert-only, two runs; reverted; not committed). 29 rethinks and 1496 walks in both runs (A4Lanes2 counted 1496 walks too). The determinism hash `a817a13303958f90` is unchanged under the probe.
- Answers: `ans:*` 0 for all five units in both runs. Walk outcomes: 1473 no answer, 21 shot (kind 0), 2 cross (kind 7). The probe does not change the answers.

Per-unit funnel (reached → rejected at the next gate; counts are calls unless marked):

| Unit | Reached | Where it stops (count) | Gate named |
|---|---|---|---|
| **OneTwo (10)** | 29 | angle gate closed 6 (|d| > 90 and no closing opponent); one-touch 23 → all 23 rejected; picker/receiver 0 | one-touch first failing gate **G3** (team level stand-in 0). G1-G2 pass for on-pitch carriers (INF, not split). Receiver predicate **never reached** |
| **DirectHighBall (11)** | 29 | all 29 at `K+0x6E == 0` (volatile stand-in false) | `0x142129CC8`. Low-ball block, mates, lane never reached |
| **BackSpace (3)** | 29 | lane branch entered 0 (carrier not mid, or lane blocked in all 29); block 2 passed 29/29; plan 4 active 0/29; cut-back picker entered 29 | picker: **163 mate tests → 146 fail the PassThrough validator, 17 pass; of the 17, 17 fail the corner/curve gate** (`curveGate` stand-in false). Survivors 0. The validator's 12 internal gates are not split |
| **Safety (6)** | 29 | gate lane free 19 (length stand-in = nearest opponent + radius); gate blocked 10 → goal-mouth off → short picker: **52 mate tests, 52 fail `ShortMateSurvives`**; through picker called 10 (kk ≠ 0), none 10 | the short picker's per-mate gates (not split); through picker none |
| **GoalFront (2)** | 29 | carrier central 1; lane free 14 (no picker); lane blocked 14 → picker entered 14 → no answer | picker's per-mate gates (not split) |

The per-mate gates inside BackSpace's validator, Safety's `ShortMateSurvives`, and GoalFront's picker are not split (budget). The next probe is those three inner gates. The answer is "no unit answers live", and the counts above name the gate at each level.

## 6. Spec and doc corrections (doc:line, old, new, evidence)

1. `pes-re-lane-units2.md` §8 / §2.6: P2 ψ is a **gate**, not only a definition. Evidence: `0x142124A83` `CMP [RSP+0x54],0`; `0x142124A8B` `JZ 0x142124AD8`.
2. `pes-re-lane-structs.md` §3.3 and the Safety text in `pes-port-A4Lanes2.md` §4 item 8: "len = distance to the nearest opponent (INF)". New: `sqrt` of the carrier's own d² in the opposite-sign score row (rank `c`, row `0x140A714A0`). Evidence: `0x14212B708`-`0x14212B76F`, `0x140A714A0`.
3. `pes-re-lane-units2.md` §2.2 / C14: `K+0x34` "the ball/carrier angle (INF)". A4Carrier §0 says "facing within 90° of attack". Used with the facing as stand-in (S9, INF).

## 7. Open items (numbered; each names the blocker)

1. **Score row S1** (`T18+0x228EC/0x22AD4`; writer OPEN; image rows §3.3). Unlocks the one-touch G5 score, the DHB score, and the exact Safety length. The largest single item.
2. **Team level** O12 (`T+0xA294/A29C`) and **team mode** S4. Closes OneTwo at G3 live. Decode the level writer.
3. **ψ** S6 (`T(side)+0x4BD4[r%11]`). Closes every OneTwo receiver live. Decode its writer and meaning.
4. **K+0x6E** (volatile / volley-header flag). Closes DHB live. Decode its writer.
5. **Angle table** S10 (`T18+0x22C64/0x22A7C`, `0x140A71260`). DHB's lane heading uses the facing until then.
6. **A_tab and M tables** (`T18+0x22F7C`, `T18+0x23F7C`, O9). The receiver's bearings are stand-ins.
7. **List row sort key** S3 (`0x140A714E0`; writer OPEN). The picker and DHB take the carrier's nearest teammates.
8. **Inner gates, not split** (§5): BackSpace validator (146/163 rejected), Safety `ShortMateSurvives` (52/52), GoalFront picker mates.
9. **ClosingCount active check**: PES's flag-1 path skips it; GF keeps it (documented ponytail).
10. **Mark count** S11, **one-two run** S7, **C[0xF]** S18, **history** S17, **K+0x34** S9: named stand-ins.
