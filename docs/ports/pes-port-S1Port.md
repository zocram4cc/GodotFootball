# PES S1 score rows and K+0x6E ported into the lane units (S1Port)

> **Main audit 19-10-26:** `0x14063AA79` `movzx ecx,byte [rax+0x31]`, then `divss [0x14253EE30]` (100.0) and `comiss` against `movss xmm0,[rdi+0x44]` at `0x14063AA74`: matches (P+0xA1 given RAX = P+0x70, the agent's reading). Correction: OneTwo is not held by G3 (ported since A4LaneInputs); per A4Level's probe it dies at G12.

Branch `S1Port` (pushed): base `origin/A4Level` `b64d2b7`; commits `842a311` (the port), `9b4f4e2` (the team-tick fix), `c34bd79` (comments).
`git ls-remote origin S1Port` (verbatim): `c34bd79671197b0556290c5260b2ee98f975cc0e	refs/heads/S1Port`
Tags: CONFIRMED = decompile, asm or live read, with the VA named; INF = inference; OPEN = blocker named.

## 1. What changed

- `src/sim/onthepitch/pesscorerows.{hpp,cpp}` (new): writer FUN_14045B420. Blocks: score0/1 (MA+0x228EC, MA+0x22AD4), list0/1 (MA+0x229B4, MA+0x22B9C), pair0/1 (MA+0x22CBC, MA+0x22D84), ang0/1 (MA+0x22A7C, MA+0x22C64), NearestList (MA+0x22E4C + 200·i). Each team-split, sorted ascending, with ranks. The mask rotation (MI+0x1198 & 3), the 0xFF flag refresh and the TA+0x11 latch are in `ScoreRowWriter::Tick`. A_tab (MA+0x22F7C) is not ported (S1 doc §5).
- Each team AI owns its writer: the lane space is the team's own (0..10 own, 11..21 opponents, as the carrier controller's `fillSide` builds it). `Team::Process()` ticks it (team.cpp, before the in-play gate), so the stoppage flag can latch it.
- Consumers now read the rows (elizacontroller, pescarrier):
  - OneTwo G5 (`touch.score`) and the DHB score (`dm.score`): the attacked goal's row entry. `attackRow = attackSign > 0 ? 1 : 0`.
  - Safety gate length (`sf.carrierOwnGoalD2_m2`): `len = sqrt(d2)`, the carrier's d2 in the row of the goal he defends (`1 − attackRow`). The old stand-in was nearest opponent + radius.
  - BackSpace rank 0 (`bs.nearestOpponent`): NearestList rank 0 of the opposite side.
  - Mate orders (Safety row, OneTwo and DHB receiver lists): the carrier's NearestList order. The three distance sorts are gone.
- K+0x6E: `pes::VolatileBall` feeds `dhb.volatileBall` and `sf.receivingFlag`. `sf.ctx44` is the ball height (DHB's `ballHeight_m`, the same K+0x44 term). The `0` stand-ins are gone.
- Stand-ins removed: `scoreStandIn`, the rank-0 and gate-length `NearestSideSlot` stand-ins, the three distance sorts, `volatileBall = false`, `receivingFlag = false`, `ctx44 = 0`.
- Stand-ins kept (named in code): K+0x6C `humanCarrier` (GF's carriers are AI), B+0x4C `ballTrackSet` (the ball-track arm), A_tab and M (O9 bearings), angle table S10, and the others in `docs/ports/pes-port-A4Lanes3.md` §4.

## 2. Decode (VAs)

Writer and cadence:
- CONFIRMED `FUN_14045B420` (0x14045B420): mask bit 0x01 writes score0, list0, ang0; 0x02 writes score1, list1, ang1; 0x04 writes pairA = |yg − z| and pairB = |yg + z|. The NearestList is rebuilt every call.
- CONFIRMED `FUN_140457070` (tick, called from `FUN_140458880`): `TA+0x11` is set by a non-zero flag and gates the call. Flag ≠ 0 → mask 0xFF. Flag = 0 → mask from `MI+0x1198 & 3`: 0 → 0x01, 1 → 0x02, 2 → 0x24, 3 → 0x10 (0x140457097..0x1404570C4).
- CONFIRMED `FUN_140458880`: the call is made only for `MI+0x11B0` ∈ {8, 0xB, 0xE, 0x12} (flag 1) or {0x14, 0x15, 0x16} (flag 0). Other stages do not call it. GF mapping INF: flag = `!IsInPlay()`, and the tick runs every team tick.
- CONFIRMED helpers: `FUN_1404587F0` = A_tab row (+0x16A8 + 0x58·i), `FUN_140458810` = NearestList row (+0x578 + 200·i), `FUN_140A714A0(T, s)` = score1 when s = 1, else score0.
- CONFIRMED NearestList on-pitch test: asm `CMP byte [R+R*1+0x1748]` at 0x14045B5E9 and 0x14045B829, and `CMP …, BPL` at 0x14045BA3A (MI+0x1748).
- CONFIRMED angle constants: 0x14259BE40 = 180/π (float 57.2957763671875), 0x1425795E4 = 2π (float 6.2831854820251465). Read from the install in `LoadCarrierConstants` (`s1RadToDeg`, `s1TwoPi`).

K+0x6E (CONFIRMED: `dec_0x14063A490.c` lines 265–288, asm `0x14063AA5E`–`0x14063AB06`):
- `0x14063AA5E` `mov [rdi+0x6E], r13b` with `r13b = 0`: the reset.
- `0x14063AA62` `CMP byte [RDI+0x6C], R13B`: K+0x6C ≠ 0 skips the flag (the "ball at feet" gate).
- `FUN_14124DAD0(P) = P+0x70` (`lea +0x70`). **The byte read is `MOVZX ECX, byte [RAX+0x31]` at `0x14063AA79`, so it is P+0x70+0x31 = P+0xA1, not P+0x71** as the brief said. The decompile agrees (`(float)*(byte*)(lVar16+0x31)`).
- `0x14063AA84` `DIVSS XMM1, [0x14253EE30]`: the divisor is 100.0. `0x14063AA8C` `COMISS`, `0x14063AA91` `movb $1, [rdi+0x6E]`: set when K+0x44 > byte/100.
- B arm `0x14063AB01`–`0x14063AB06`: `byte(B+0x4C) != 0 && r14 == 0` sets the flag, where r14 = (B+0x80 ≠ 0 ? nonzero : d1 < d2) and d1 = |ball − (B+0x1C)|, d2 = |ball − (K+0x40)|. Not ported (below).

GF terms:
- **K+0x44** = the height of the ctx4 point K+0x40..0x48. FUN_14063A490 writes that point at decompile lines 166–232: the default is T+0x2482C (`local_f0 + 0x2482c`), overridden on the branches by other points (`local_118+0x155c`, `+0x1414`, the ball). GF already maps K+0x44 to `ballHeight_m` in the DHB (`dhb.ballHeight_m`), so the flag and `sf.ctx44` take the same term. **INF** for the default branch.
- **P+0xA1 / 100** = the carrier's height in metres. Stat 0x31 is GF's `physical_height` (the shoot unit reads it × 100 as cm, `shoot.height_m`). **INF** on the byte-vs-float identity.
- **B+0x4C** = `FUN_140A6D0D0`, GF's `ballTrackSet`. GF has no such state: **OPEN**. The arm is off (named ponytail), and its B+0x80/B+0x1C inputs are not ported.

## 3. Test results

Red first: the two behaviour suites failed on the stubs (NaN, 255, −1, false) before the implementation.

- `pes_scorerows_test` (new, 6 tests): `[  PASSED  ] 6 tests.`
- `pes_carrier_test` (158 tests, incl. `CarrierVolatileBall.IsSetWhenTheBallIsAboveTheCarriersHeight`): `[  PASSED  ] 158 tests.`
  - The Safety fixture failed 3 tests on the first run (the old stand-in length was gone). The fixture now sets `carrierOwnGoalD2_m2 = 52.5²` for the carrier at (0, 0) with his goal at (−52.5, 0), and the comment says so.
- `pes_lane_test` (21 tests): `[  PASSED  ] 21 tests.`
- Gate (`ctest`, the full suite) was not run: Main gates.

Mutations (each applied, rebuilt, run, reverted; the tree was clean afterwards):
1. Score row drops the z term (`score.Add(i, dx*dx)`): `ScoreRows.GoalDistanceIsTheSquareFromEachGoalCentre` FAILS, 5 pass.
2. Cadence: every block refreshed every call (`mask = kMaskAll`): `ScoreRows.EachCallRefreshesItsMaskAndTheBlocksAgeUpToFourFrames` FAILS, 5 pass.
3. NearestList order: the sort comparator reversed: `ScoreRows.NearestListIsSortedByDistanceWithTheTeamSplitAtSlot11` FAILS, 5 pass.
4. Flag boundary (`>` → `>=`): `CarrierVolatileBall.IsSetWhenTheBallIsAboveTheCarriersHeight` FAILS.

## 4. Smokes (2 min, seed 7, teams 1 vs 2, `--pes21-root <workdrive>/PES21`)

Baseline: `b64d2b7`, clean worktree build (`<workdrive>/gf-S1Base`). Its `gf-A4Level` build tree has uncommitted edits, so it was not used.

| Run | Exit | Determinism |
|---|---|---|
| b64d2b7, switch-off (no `--pes-loop`) | 0 | `[determinism] 75a11cdb2b3c8ec5` |
| b64d2b7, `--pes-loop` | 0 | `[determinism] 9fc0b08314812d8f`, `37735 sim ticks to full time` |
| S1Port `9b4f4e2` (= `c34bd79` code), switch-off | 0 | `[determinism] 75a11cdb2b3c8ec5` (**unchanged**) |
| S1Port `9b4f4e2`, `--pes-loop` | 0 | `[determinism] 9fc0b08314812d8f` (**unchanged**), `37735 sim ticks to full time` |

The switch-off hash does not move. The rows tick in both modes, and this window shows no decision change in either. The `--pes-loop` hash does not move: the rows are live (below), but no unit answers in this window, so no walk changes. I did not trace in code which modes reach the lane units.

## 5. Silence probe (temporary probe, reverted; `--pes-loop`, 2 min)

Probe: one line per rethink in `elizacontroller` after the DHB unit (`bs`, `sf`, `one`, `dhb` answer indices, `vol` = K+0x6E, `len2` = Safety own-goal d2, `g5` = OneTouch score). Reverted (`CUT`), tree clean.

- Run A (before the team-tick fix): `rethinks: 24`, `bs 0 / sf 0 / one 0 / dhb 0` answers, `vol=1: 0`, **`len2=nan` 24/24, `g5=nan` 24/24**. The rows were never latched: `Team::Process()` only ticked the controller in play, and the stoppage flag never ran. Found by this probe and fixed in `9b4f4e2`.
- Run B (after the fix): `rethinks: 24`, answers `bs 0 / sf 0 / one 0 / dhb 0`, `vol=1: 0`, `nan_len 0`, `nan_g5 0`. Samples: `[probe] bs=-1 sf=-1 one=-1 dhb=-1 vol=0 len2=3350.497 g5=3122.231`, `len2=3170.889 g5=3298.667`, `len2=3186.911 g5=3290.287`. The values are consistent with a carrier near the centre circle.

Answers, per unit: **Safety 0/24**, **DHB 0/24** (K+0x6E = 0 in 24/24, so the DHB's own volatile gate stays shut), **G5/OneTwo 0/24** (G5 is now a live number. The first closing gate in the A4Lanes3 funnel was G3 team level, a stand-in this run did not touch, so OneTwo is still silent. INF for this run, not re-split). The per-mate gates inside Safety and the DHB low-ball block were not split this run.

## 6. Open items (numbered)

1. **K+0x6E ball-track arm**: B+0x4C (`FUN_140A6D0D0`, GF `ballTrackSet`), B+0x80 and B+0x1C (0x14063AB01–06). GF has no ball-track state. The arm is off (ponytail in `pescarrier.hpp`).
2. **K+0x44 branch points** (decompile lines 166–232): GF uses the ball height; the carrier-branch points are not decoded.
3. **K+0x6C** ("ball at feet", 0x14063A5A9 / 0x14063A610): GF's carriers are AI, so it stays false.
4. **Safety gate row sign** (`1 − attackRow`): the bridge returned no decompile for `FUN_14212B520`, so the own-goal choice is INF from the code comment ("the row for the sign !k").
5. **Stage byte MI+0x11B0**: no GF state. The writer's call gate is the GF tick every team tick with flag = `!IsInPlay()` (INF).
6. **Other NearestList consumers**: pesoffball (`NearestList` built from the snapshot), pesmovestep (`NearestOpponent`), peskeeper. Outside the lane units; they still stand in for MA+0x22E4C.
7. **A_tab (O9) and M (T18+0x23F7C)** bearing stand-ins stay (A_tab is dead per S1 doc §5).
8. **Silence**: no unit answers live in a 2-minute window. DHB is shut by its K+0x6E input (B-arm open, item 1). OneTwo needs a G3 (team level, O12) split. Safety's per-mate gates need a split.
9. Tick-wiring test: the Team-level tick is covered by the probe only, not by a unit test (a Match-level fixture is needed).
