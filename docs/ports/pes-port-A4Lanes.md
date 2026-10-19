# A4Lanes: carrier units on the pass-lane core (GoalFront ported; BackSpace and Safety stopped on undecoded helpers)

Branch `A4Lanes`, worktree `<workdrive>/gf-A4Lanes`, base `origin/main` `72f5977`. One code commit, `d5e8417`, pushed (`git ls-remote origin A4Lanes` = `d5e8417`). Files: `src/sim/onthepitch/pescarrier.{hpp,cpp}`, `src/sim/onthepitch/player/controller/elizacontroller.cpp` (carrier path, pes_loop only), `tests/sim/pes_carrier_test.cpp`. Not touched: referee.*, pessequencer.*, pesstoppage.*, ball.*, match.hpp, humanoid.cpp.

## 0. Verdict

| Unit (walk kind) | Gate VA | Status |
|---|---|---|
| GoalFront (2) | `0x14212ABA0` / picker `0x1421265C0` | **Ported, tested, wired** (`d5e8417`) |
| BackSpace (3) | `0x142129110` | **Stopped.** Gate decoded; both answer branches depend on undecoded helpers (§2) |
| Safety (6) | `0x14212B520` | **Stopped.** Gate partly decoded; answer depends on undecoded pickers `0x142126B70`, `0x142128490` (§3) |

Acceptance status: switch-off hash **unchanged** (`a3d5e43a3b7b03af`). `--pes-loop` completes with passes (10-10) but **shots 0-0**, which is **not** the acceptance state. The base `72f5977` gives the same `--pes-loop` result, so the zero shots predate this branch (§5).

Walk order for PassThrough (plan 4) is `[3, 2]` (`pescarrier.cpp:1099-1101`). With BackSpace unported, a PassThrough carrier can reach GoalFront with no BackSpace answer first. That is a known gap, not a fix (§4).

## 1. GoalFront (kind 2): ported, `d5e8417`

- Gate `0x14212ABA0`: `IsCentral(team+0x9CC)` true returns no answer. Otherwise the lane is packed (origin = carrier (+0x55C, +0x564), len 20 `0x14259BE1C`, cone 80 `0x142596E6C`, radius 1 `0x14256AE1C`, heading `wrap360(0x140A71260(team, side)[slot] + 180)`), and the unit runs only if the lane is **blocked** (`LaneFree` false), via `0x14063CD90`/`0x14063DC10`/`0x14063DE20`.
- Picker `0x1421265C0`: b6c status `0x142118D20(team+0xB6C, slot%11) == 0xB`; record A `+0xF0 <= 1` (`0x14256AE1C`); angle limit on record B `+0xE8`: 90 (`0x14259BE5C`), raised to 117 (`0x142BF07E0`) or 140 (`0x14259BE64`) by stat and flag gates; band `5 < d < 30` (`0x14259222C`/`0x142591140`); openness class ladder with 15 m (`0x14259BE0C`) for class 2; mate's zone central with `+0x170 < 15`; pressure entry `0x14069A3E0(team+0x2124, slot%11)[+0x10] != 0`. Ties keep the earlier mate (strict `>`). Answer: kind 2 (AnswerPass), b = 0 (short).
- Predicted point for the mate: `int(54*0.5 + 0.5) = 27` frames (`0x142565600` = 0.5; game speed `0x141E532D0` = 54, CONFIRMED live per `pes-re-Dribble-inventory.md` §1).
- Constants from the install (`PeImage` parse of `PES2021.exe`): `0x14259BE1C` = 20.0, `0x142596E6C` = 80.0, `0x14256AE1C` = 1.0, `0x14259BE0C` = 15.0, byte immediates `0x1421267FF` (70), `0x1421266AD` (20).
- Stand-ins (`// ponytail:` in `elizacontroller.cpp`): the per-slot angle table `0x140A71260` (writer undecoded, `pes-re-lane-structs.md` §4) is replaced by the carrier's facing vector; human flag `team+0x6C`; the `+0x21D8`/`+0x2128` pair; the b6c status byte; the pressure entry `+0x10` meaning; the mate's nearest opponent stands in for the pressure value.
- Call site: `elizacontroller.cpp` `if (rethink)` block, `pes::GoalFrontUnit(gf, carrier)` (line ~2659). Live check: a temporary probe logged **29 calls** in the seed-7 2-min `--pes-loop` run (`[gfprobe] central=…` lines; 1 with `central=1`, the rest `central=0`), reverted before commit.

### Tests (TDD record, `tests/sim/pes_carrier_test.cpp`)
- RED on the base: with `GoalFrontUnit` stubbed to a no-answer return (a behaviour failure, not a compile error): **89 passed / 7 failed**, exactly the seven GoalFront tests.
- GREEN after the port: **96/96** with `PES21_ROOT=<workdrive>/PES21` (re-run at HEAD `d5e8417`).
- Mutation: picker's strict-greater rule `bestOpenness <= openness && openness != bestOpenness` changed to `bestOpenness <= openness` (ties replace the earlier mate). Caught by `GoalFrontUnitTest.OnlyAStrictlyMoreOpenMateReplacesTheBest` (expected 5, got 7). Reverted, 96/96 again. The first version of that test missed the tie case; the mutation exposed it and the tie assertion was added.
- `pes_lane_test`: **21/21** green at HEAD.
- Install test extended with relation checks for the new loader fields (angle gates ordered, distance bands ordered, zone distance equals the class-2 distance at its one load site, lane table non-zero).

## 2. BackSpace (kind 3): stopped

### 2.1 Gate `0x142129110` (decoded; `gf-A4Lanes-scratch/a_backspace.txt`, `gf-A4Carrier-scratch/new_142129110.c`)
CONFIRMED (asm and decompile):
1. `IsMid(team+0x9CC)` via `0x1406997A0`. If true, the lane branch runs. If false, control goes to block 2.
2. Lane branch: heading = `wrap360(team+0x250)`. No `+180`, and not the per-slot table `0x140A71260` that GoalFront uses. Origin = carrier (+0x55C, +0x564). len 20 `0x14259BE1C`, cone 80 `0x142596E6C`, radius 1 `0x14256AE1C`, keeper 0, h 0, through `0x14063DE20`. If the lane is **free**:
   - opposing side = flip of `IsAway(slot)`; `opp = 0x140A4EE40 → 0x140A4EE60(players, side, ctx+0x40 (x,z), k=0)`: the nearest of that side to the point (the `ob::NearestOpponents` family).
   - if `opp < 22`: frames = `fps / 6`, `fps = 0x141E532E0` (= `DAT_14351AE50` = 54), so 9 frames. Predicted (x, z) = `opp.pos + opp.vel * 9` (vel `+0x574`, `+0x57C`). `d2 = 0x140A8F890(predicted, ctx+0x40)`.
   - **early return (no answer) if `d2 > 0x1425A9464` (= 36.0).** `0x140A8F890` returns a **squared** (x, z) distance, so 36 is 6 m.
3. Block 2 gate: `ctx+0x6F == 0 && (ctx+0x6C != 0 || ballHeight <= 1.0)`. Ball height via `0x140A57800` → `0x140A6CF70` (+4). `0x140A6CF70` internals **OPEN** (only its `+4` read is used here).
4. Plan kind `[ctx7+0xA0]` (`kPlanPassThrough` = 4 in GF, reachable, `pescarrier.cpp:1099`):
   - **plan 4**: target = `0x14211CB00(plan, 10)`, which returns the plan record at `+0x48` for plan 4 (CONFIRMED switch: case 0 `+0xC`, 2 `+0x28`, 3 `+0x38`, … 9 `+0x98`, default 0). Accept only if `target < 22`. Two plan-record bytes are tested first (`pcVar7[1]==0 && *pcVar7!=0` kills the answer even when the validator passes). Then `0x1421226C0(ctx, target, 0)` must be true (decoded: carrier-helpers §1.1, 12 gates), `d2(recB[target].target, corner) <= 225.0` (`0x14259BE70`, squared 15 m; the first point is the record-B TARGET, not the mate) OR `0x1421225E0(ctx, target)` (§2.2). Answer kind 2, target, b = 3. _Corrected 19-10-26 from pes-re-carrier-helpers.md §6 item 1: new_142129110.c L100-118 re-read; old "0x1421226C0 UNDECODED / 0x1421225E0 partly decoded / 0x140A8F890 against 225.0"._
   - **plan != 4**: cut-back picker `0x142125140(ctx, byte [param_1+0x3D])` (**UNDECODED past line 100 of 294**). If it returns not `0xFF`: kind 2, target, b = 3. Then b drops to 0 when `0x14063E270(players, target, 0)` is true **and** `0x14069A3E0(team+0x2124, slot%11)[+0x10] > 1` **and** the b6c `*0x142118C90(team+0xB6C, slot%11) > 1` (CONFIRMED decompile lines 132-140).
5. `0x142139630(team+0x898, slot)` is a pure address calc (`base + slot*0x1C`, or base if slot > 10; CONFIRMED). Its result is unused. It is not a dependency.

### 2.2 Blockers (what is missing, with VAs)
- **`0x1421226C0`** (plan-4 validator, `gf-A4Lanes-scratch/b_1421226c0.c`, 418 lines, 18.8 KB — decoded: carrier-helpers §1.1, 12 gates + tail loop). Full callee list (asm CALL scan): `0x141E532D0/E0`, `0x1421B7D40`, `0x140458740`, `0x142118D40`, `0x14043A0A0`, `0x140A8F890`, `0x140474C40`, `0x14063D2F0`, `0x14063CD90` (direct core call at `0x142122D1D`), `0x14063C560`, `0x1404C8E00`, `0x140481B00`, `0x140698CC0`, `0x140698D10`, `0x140A48A10/30`. _Corrected 19-10-26 from pes-re-carrier-helpers.md §6 item 3: old list omitted 0x14063CD90/0x14063D2F0/0x140A48A30 and had typo 0x140481B0._
- **`0x142125140`** (cut-back picker, `gf-A4Lanes-scratch/d_142125140.c`, 294 lines — decoded: carrier-helpers §1.2, 12-gate loop + |z| tiebreak). Lines 1-100: lane heading (`team+0x250`, as in the gate), lane packer `0x14063DE20` (cone `0x14256AE30` = 120, radius 1; result DISCARDED — dead), stat `0x14063E960(team, slot, 0xC)` (result discarded — dead), and a loop over the 22 players. Callees: `0x140477130`, `0x140A489F0`, `0x14124DAD0`, `0x140A8F860`, `0x14063E010`, `0x14061E7C0`, `0x140439F90`, `0x1404C8E00`, `0x14043A0A0`, `0x140458740`, `0x140699820`, `0x140A4F720`, `0x1421225E0`. _Corrected 19-10-26 from pes-re-carrier-helpers.md §6 item 4: old "loop body not read"; dead calls per asm 0x142125249/311._
- **`0x140A6CF70`** (ball accessor behind `0x140A57800`; the `+4` read is the height). Internals undecoded. Used only in block 2.
- **`0x1421225E0`** (decoded: carrier-helpers §1.4): returns false for `target > 21`. The `fVar5*0.0 + DAT_142565600` term is REAL (`MULSS XMM0,XMM7`, XMM7 = 0 at `0x142122649`): frames = 0 always, so it compares CURRENT positions (`k*mate.x >= k*curve(mate.z)`). `0x142139650` is the curve-table interp (lane-core §6.3, table runtime/OPEN). _Corrected 19-10-26 from pes-re-carrier-helpers.md §6 item 2: asm_1421225E0.txt; old "looks like a decompiler artefact, needs an asm check"._

### 2.3 Constants (install, `PeImage` parse of `PES2021.exe`)
`0x14259BE1C` = 20.0 (lane len), `0x142596E6C` = 80.0 (cone), `0x14256AE1C` = 1.0 (radius; also the ball-height limit), `0x14256AE30` = 120.0 (picker cone), `0x14259858C` = 360.0 (wrap), `0x1425A9464` = 36.0 (squared, 6 m), `0x14259BE70` = 225.0 (squared, 15 m), `0x14259BE0C` = 15.0, `0x1425656D0` = -0.0 (sign mask, the `(char)(*(float*)(*p+0x14)) != 1` flip of the x component), `DAT_14351AE50` = 54 (int fps; `0x141E532E0` returns it).

### 2.4 Corrections to `docs/re/pes-carrier-A4Cont.md` (BackSpace)
- "opponent-on-line within 36 m → no answer" is wrong. The 36.0 is a squared distance (6 m), and the early exit is when the nearest opposing player's 9-frame prediction is **more than** 6 m from the ball point (`0x140A8F890` squared).
- "gate block 1 = IsMid, lane free … → no answer" is incomplete. `IsMid` selects the lane branch. The only early exit is IsMid ∧ lane free ∧ nearest opposing player exists ∧ `d2 > 36`. A blocked lane falls through to block 2.
- The BackSpace lane heading is `wrap360(team+0x250)`, with no `+180`.

## 3. Safety (kind 6): stopped

Gate `0x14212B520` (`gf-A4Carrier-scratch/new_14212b520.c`, 141 lines; read lines 1-63). CONFIRMED so far: `ctx7` plan pointer `+0xC0` (plan heading), `team+0x31C1` flag, pressure records `0x14069A3E0(team+0x2124, 0xFF)` (+0x30, +0x34, +0x38), `0x140A71260` (the per-slot angle table: same undecoded writer as GoalFront), `0x140A489F0`, `0x14124DAD0`. Gate lines 64-113 and 114-137 not fully read. The answer comes from the two pickers:
- **`0x142126B70`** (short, `gf-A4Lanes-scratch/d_142126B70.c`, 492 lines, 21 KB): not decoded. Callees include `0x14069A3E0` (8×), `0x140A48A10` (7×), `0x142118C90` (4×), `0x141E532D0/E0`, `0x140A8F890`, `0x1415A1780`, `0x140A4EE40`, `0x14047713`, `0x140474C40`, `0x141052E30`.
- **`0x142128490`** (through, `gf-A4Lanes-scratch/d_142128490.c`, 189 lines, 6 KB): not decoded. Callees include `0x141E532D0`, `0x140A48A10`, `0x14043A0A0`, `0x14212849`, `0x1421226C0` (the same undecoded validator as BackSpace), `0x142118D40`, `0x140A8F890`, `0x140A714C0`, `0x14069A3E0`, `0x14063DE20`, `0x1405D06C`, `0x14050774`.

Safety cannot be ported without both pickers. Porting the gate alone would give a wrong answer for every state where a picker answers.

## 4. Walk wiring note
- `BuildUnitList` for PassThrough (plan 4) emits `[3, 2]`. Kind 3 (BackSpace) is unported, so the table has no answer for it. GoalFront (kind 2) is wired at its decoded place. A PassThrough carrier can therefore get a GoalFront answer that PES would only give after BackSpace declines. This state holds until BackSpace lands. It is the top open item.
- GoalFront is consulted on every rethink tick (29 calls in the 2-min run), but only plan 4 lists kind 2, so the answer only reaches the walk on PassThrough carriers.

## 5. Smokes (2 min, seed 7, teams.sql 1 vs 2, `--pes21-root <workdrive>/PES21`)

| Build | Command | Hash | Passes | Shots |
|---|---|---|---|---|
| `d5e8417` (branch) | no `--pes-loop` | `a3d5e43a3b7b03af` | (GF path) | 2-2 |
| `d5e8417` (branch) | `--pes-loop` | `a817a13303958f90` | 10-10 | **0-0** |
| `72f5977` (base) | `--pes-loop` | `a817a13303958f90` | 10-10 | **0-0** |

- The switch-off hash is unchanged. Acceptance met.
- `--pes-loop` completes (exit 0) with passes present. **Shots are absent on both base and branch**, so "shots present" is not met. Not caused by GoalFront: base and branch give the same hash and counts. The next suspect is the shot path. It is outside this task's files, so it is recorded, not fixed.
- The GoalFront probe (temporary `fprintf` in `GoalFrontUnit`, reverted before commit): 29 calls, hash unchanged. The unit is live but its answer does not reach the walk in this seed.

## 6. Open items (ordered)
1. **Zero shots on the `--pes-loop` path (base `72f5977` too).** Main's gate needs this resolved. It is the reason "shots present" is not met.
2. BackSpace: decode `0x1421226C0` (418 lines; plan-4 validator), `0x142125140` lines 100-294 with their helpers (`0x14063E960`, `0x140477130`, `0x140A489F0`, `0x14124DAD0`, `0x140A8F860`, `0x14063E010`, `0x14061E7C0`, `0x140439F90`, `0x1404C8E00`, `0x14043A0A0`, `0x140458740`, `0x140699820`, `0x140A4F720`), `0x140A6CF70` for the ball height, and the asm of `0x1421225E0` for the frames term. Then port per §2.1 and wire at kind 3 before GoalFront.
3. Safety: decode `0x142126B70` (short picker, 21 KB) and `0x142128490` (through picker, 6 KB), gate lines 64-137 of `0x14212B520`, and its helper `0x140A714C0`.
4. The per-slot angle table `0x140A71260` writer (shared by GoalFront and Safety): still undecoded. GoalFront's facing-vector stand-in is named in `elizacontroller.cpp`.
5. GoalFront stand-ins: the `team+0x6C` human flag, the `+0x21D8`/`+0x2128` pair, the b6c status byte, the pressure entry `+0x10` meaning.
6. The Cross unit's `laneOpen` stand-in in `elizacontroller.cpp` can now be replaced by the ported lane core. Out of this task's units, so left as is.
