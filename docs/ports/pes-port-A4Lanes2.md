# A4Lanes2: BackSpace (kind 3) and Safety (kind 6) carrier units ported and wired

Branch `A4Lanes2` (worktree `<workdrive>/gf-A4Lanes2`), base `origin/A4Lanes` `d5e8417`.
Head pushed: `7cac846` (`git ls-remote origin A4Lanes2` = `7cac846`). Earlier commits: RED `c50e96f`,
BackSpace port `63b12d2`, BackSpace wiring `a71bcd1`, Safety RED `cc2a7b9`.
Files: `src/sim/onthepitch/pescarrier.{hpp,cpp}`, `src/sim/onthepitch/player/controller/elizacontroller.cpp`
(lane-unit block, pes_loop path only), `tests/sim/pes_carrier_test.cpp`. Not touched: referee.*,
pessequencer.*, pesstoppage.*, ball.*, match.hpp, humanoid.cpp. Tree clean at the head.

Spec: `docs/re/pes-re-carrier-helpers.md` (float audit §0 used as given), lane channels
`docs/re/pes-re-lane-structs.md` §3.1/§3.3/§6, roles `docs/re/pes-re-role-writer.md`.
No messages from RoleArrays arrived in this round, so no elizacontroller carrier-path reads changed.

## 1. Tests (PES21_ROOT=<workdrive>/PES21, Release build, `--target pes_carrier_test pes_lane_test`)

- `pes_carrier_test`: **113/113** at `7cac846` (106 before Safety; Safety adds 7).
- `pes_lane_test`: **21/21**.
- RED (behaviour, not compile): BackSpace 9 tests, 5 failed on the stub; Safety 7 tests, 3 failed on
  the stub (`cc2a7b9`). Each turned green on its port.
- Mutations (each run, rebuilt, reverted, green again after the revert):
  1. BackSpace validator gate 8, `<` to `<=` (both comparisons): the first run was **not caught**
     (the fixture never hit the boundary). Boundary asserts were added (need 8 m, heading 45 deg), then
     it was **caught** by `BackSpaceUnitTest.TheValidatorRejectsAlignedHeadings`; reverted.
  2. Safety scored limit (distance ramp multiplies by the -20 span instead of dividing): **caught** by
     `SafetyUnitTest.TheScoredLimitRejectsATurnedMate` (112 pass, that one fails); reverted.

## 2. Smokes (2 min, seed 7, teams.sql 1 vs 2, `--pes21-root <workdrive>/PES21`, Release)

| Run | Hash | Passes | Shots | Exit |
|---|---|---|---|---|
| Switch-off (no `--pes-loop`) | `a3d5e43a3b7b03af` | (GF path) | (GF path) | 0 |
| `--pes-loop` | `a817a13303958f90` | 10-10 | 0-0 | 0 |

- Switch-off hash is `a3d5e43a3b7b03af`, unchanged. The kind-2 dispatch fix I made in passing lives
  after the `if (!pesPath) {...; return;}` early return in `GetOnTheBallCommands`, so it runs only on
  pes_loop. The switch-off path does not reach it.
- `--pes-loop` completes and its hash equals the base (`72f5977`) and A4Lanes (`d5e8417`) hash, with
  passes 10-10 and shots 0-0, the same as the A4Lanes record.
- Probe (temporary `stderr` line at the walk, reverted, not committed): the walk ran **1496 times**;
  BackSpace and Safety returned **no answer in any of them** (`bs=-1 sf=-1` every time); the walk won
  shot 21 times and cross 2 times. So the unchanged hash means the two units are live but silent in
  this seed, not that they agree with the old path. Why they are silent is open (see §6).
- The first switch-off run used a Debug configuration (asserts on) and aborted at `humanoid.cpp:2070`
  (`assert(command.desiredLookAt.coords[2] == 0.0f)`). That assert exists in the base. The build was
  reconfigured to Release, the team's standard, and the smokes were rerun there. `ninja -t deps`
  showed no `#deps 0` objects after the layout change.

## 3. What was ported (VAs, all from the decode; constants loaded from the install)

BackSpace (kind 3), gate `0x142129110`: lane branch (IsMid, LaneFree on the 20/80/1 lane from the
carrier; the nearest opponent's 9-frame prediction past 6 m gives no answer), block 2 (receiving flag,
ball height 1.0), plan 4 through the validator `0x1421226C0` (12 gates, lane 1 direct core with the
`h = target slot` quirk ported as it is, lane 2 as a packer-B replica through per-entry scans; the curve
arm `0x1421225E0` is an input), other plans through the cut-back picker `0x142125140` (crowd count
`0x140A4F720`, lanes A and B, box branch, advance branch, candidate tiebreak). Answers are through balls
(b = 3), dropped to short (b = 0) when close, pressure and class all exceed 1.

Safety (kind 6), gate `0x14212B520`: k from the pressure counts (`+0x30/+0x34/+0x38`), a facing-vs-plan
turn over 135 adds one, danger gives k = 3, a FREE gate lane gives no answer, the goal-mouth flag gives
no answer. Short picker `0x142126B70`: pre-gate (keeper arm / field arm), scored limit (keeper 45;
field max(need, 45) while need <= 135, else 135), over-110 arm, distance window and ceiling cascade,
mate-speed arm, G1 opponent tests (lim-check and second test), pressure gate, class floor, one-touch lane
(wrapper D), keeper lane (`0x140A4FEA0`, keep = 1), tiebreak. Through picker `0x142128490`: fast path
(162 < press+0x20 and pair < 9) or the 40 m gate, teammates in distance order (row end at 900), then
aim, band, validator `0x1421226C0`, and the final lane (10/60/5, h = 54).

## 4. Corrections to `docs/re/pes-re-carrier-helpers.md` (doc:line, old, new, evidence)

1. §2.2 "Limit" (doc ~285-287): old "keeper -> lim = max(need, 45.0); non-keeper with need > 135 ->
   lim = 45.0". New: keeper lim = **45** (the need is not read on the keeper branch: fVar32 = fVar30 = 45);
   non-keeper need <= 135 -> max(need, 45); need > 135 -> **135** (the loop-top value of fVar31 stays; the
   `&&` short-circuits without assigning 45). Evidence: `d_142126B70.c` L225-233, `fVar31 = DAT_14259be60`
   at the loop top.
2. §2.2 G1 (doc ~300-306, opponent-near-mate): old "turnA >= 80 AND mid false" as the gate. New: the
   opponent block runs only when `!b28 || turnA >= 50 || dist <= 15` (turnA = record A +0xE4, not
   vsFacing). Arm A (keeper or oppD < 35): clamp(0.4d, 10, 20). Arm B (turnA >= 80 and !mid): clamp(0.4d,
   3, 10). The pressure gate sits inside G1 and runs only when turnA >= 80. Evidence: `d_142126B70.c`
   L280-284 (`piVar20[0x39]` = +0xE4 against `DAT_14259be34` = 50.0).
3. §2.2 second opponent test (doc ~310-312): old "d2(matePred, oppPred_by_same_fr) < (sqrt(dCarrierOpp2)+pad)^2".
   New: oppPred is **the mate's** nearest opponent predicted by fr (the carrier's is not used).
   dCarrierOpp2 = d2(carrierPred27, nearest-opponent-of-carrierPred27 predicted 27). Pad = (k*mate.x <= 0)
   ? (keeper ? 5 : 3) : (over110 ? 2 : 0). Evidence: L~300-330 (`FUN_140a4ee40(..., &local_170, 0)`), head L~150-175.
4. §2.2 ceiling (doc ~293-297): old "over110: vsFacing > 100 AND class <= 3 -> 15; elif k*ball.x > 0 -> 15;
   elif keeper AND class >= 5 -> 60". New: default 40. Over110 with vsFacing > 100 sets 15 and x = class > 3.
   With x: 15, then if k*ball.x <= 0 -> 40, and 60 when keeper and class > 4. teamMode > 2 -> 15.
   Evidence: L~243-262, the `&&` chain with an assignment at each step.
5. §2.2 tiebreak (doc ~327-337): old "if best.turnA <= q.turnA keep best unless best is the keeper". New: the
   own-half branch takes q only when best is the keeper and best.vsFacing <= q.vsFacing. A non-keeper q
   replaces a keeper best. Two non-keepers: a corner d2 >= 1600 takes the more advanced; else equal class
   and dOpp(best) > dOpp(q) takes **q** (the comma assignment in the `&&` chain leaves q); else equal class
   with |dOpp diff| < 5 and q more advanced takes q. Evidence: L~400-440 (`uVar13 = uVar25` inside the chain).
6. §2.2 one-touch lane origin (doc ~318-322): old "origin = ball advanced dist along recA+0xE0". New: the
   picker first **writes** recA+0xE0 with the mate's velocity heading (`0x140470D60(recA+0xE0, heading, 360)`),
   so the lane angle is the heading. Evidence: L~255-262 (`FUN_140470d60(piVar20 + 0x38,local_110,DAT_142598580)`).
7. §2.3 through pre-gate 1 (doc ~346-349): old "proceed when pressTable[oppSide]+4 > 9.0". New: the fast path
   skips gate 2 when **162 < press+0x20 and pair < 9**; the JA at `0x1421285BC` jumps on `9 > pair`.
   Evidence: `asm_142128490.txt:73-74` (`COMISS` against `[RDX+4]`, `JA 0x14212864f`), `d_142128490.c` L11-17.
8. §4.2 Safety gate length (doc ~487-489): old "len = sqrt(nearOppD2)". New: the decoded length is a
   **rank-indexed** NearestEntry read (`0x14212B71D-76F`; the rank byte's meaning is undecoded). With
   len = nearest distance, the strict annulus measured from the apex one radius behind the origin never
   records the nearest opponent (`pes-re-lane-core.md` §5.2). The port uses nearest distance + radius as
   a named stand-in (open item 1).
9. §1.1 gate 10 (doc ~91-94): the human flag is ctx4+0x6C and is separate from the possession flag. The
   BackSpace port had conflated them (`owned`); it now has its own `humanCarrier` input.
10. §1.1 gate 11 lane 1 `h` (doc ~97-101): unchanged (`h = target slot`, verbatim).
11. §2.2 keeper lane (doc ~323-326): the scan is on the opponent side with keep = 1. The decode passes no
    horizon to `0x140A4FBC0`; the port uses current positions (INF).
12. §2.1 gate lane heading (doc ~233-237): wrap360(table + 180) is one wrap after the two wraps net out. No change.
13. §2.2 turn fields: `+0xE4`, `+0xE8`, `+0xEC`, `+0xF0` are compared unsigned (`110 < turnA`, `lim < vsFacing`,
    `vsFacing <= 100`), so the controller stores their magnitudes. INF: PES stores magnitudes; the writer
    `0x1421174F0` was not read.

## 5. Stand-ins (`// ponytail:` in code; each names its removing site)

- Pressure counts `+0x30/+0x34/+0x38/+0x20` and entry `+0x10` (writer `0x14069A500`). Stand-in: the
  carrier's class 0 sets press30 = 1; the rest are 0.
- Angle table `0x140A71260` (writer undecoded): lane heading and the angles use facing + 180.
- Team table (`0x140A714A0/C0`, `0x140439F90`, `0x1404C8E00`, `0x14061E7C0`, `T18+0x2470C` pair): the mate's
  nearest-opponent distance stands in for the need; line values stand in with the goal lines; the
  `T18+0x2470C` pair is 0 (passes its `<= 9` tests).
- Ball-track flag `0x140A6D0D0`; ctx4 `+0x6C/+0x6E/+0x6F/+0x44` (not decoded to GF state).
- Per-slot stats `imageA+0x898`; box scalars `+0xB44/+0xB48/+0xB3C/+0xB40` (zeros); danger `0x140699960`
  (own-third pressure stands in); goal mouth `+0x31C0` (false).
- Team mode `0x141052E30` (neutral 2); plan heading `+0xC0` (attack direction); plan record slot 10
  (`0x14211CB00`, none); curve table `+0x2B0` (off); zone builders `0x140698340/830` (the ported zone stands in).
- B-array role class (`0x140A42590`): GK = 0, other = 1.
- Receiver records: record B target and angles are GF's one target and pass bearing (no separate B record).

## 6. Open items

1. **Safety gate length** (correction 8): decode the NearestEntry rank read (`0x14212B71D-76F`; writer
   `0x140A714C0`) and remove the `+ sfLaneRad_m` stand-in in `SafetyUnit`. Until then the gate is blocked
   by the nearest opponent, which is a stand-in and not PES.
2. **Units are silent in the 2-min seed**: the probe shows 0 BackSpace and 0 Safety answers in 1496 walks.
   The cause is open. Candidates: the plan-4 arm is silent (no slot-10 record in GF); the cut-back's
   branches need b true; stand-in values (pressure, team table, box scalars) may keep the gates shut. Not
   diagnosed in this round.
3. **f10 / f12 not carried**: the one-touch predicates `0x142128C10` and `0x142124950` (specified in
   lane-core §6-§9, not ported) and the through answer's `+0x12 = 1`. GF's `PassAnswer` has no f12 field;
   f10 is always false.
4. `0x140A4FBC0` horizon: not read; current positions are used (INF).
5. Cleanup debt: the BackSpace validator's difficulty limit (`0x14256AE1C` = 1.0) reads `bsBallHeight_m`;
   it should read `gfDifficultyMax`. Left as is in this branch.
6. Live check: none. All CONFIRMED tags are static. The team-table lines, need reads and pressure
   counts are stand-ins.
