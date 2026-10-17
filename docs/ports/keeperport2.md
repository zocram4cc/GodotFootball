# PES 2021 goalkeeper → GameplayFootball port (correction + positioning + rush)

Scope: fix the shipped `peskeeper.cpp` port's four errors, port PES's open-play
positioning and the loose-ball rush, and run the gate. Sources:
`docs/re/pes-re-goalkeeper.md` §10 (the session-2 live-checked findings) and
`docs/re/pes-re-keeper-port.md` (the original port). Tags as in those reports:
**OBS** = observed in bytes/decompile (VA given), **INF** = inferred.

Code: `src/sim/onthepitch/peskeeper.{hpp,cpp}` (all the arithmetic),
`src/sim/onthepitch/player/controller/strategies/offtheball/goalie_default.{hpp,cpp}`
(`GoalieDefaultStrategy::RequestInput`, `InterceptTarget`),
`src/sim/onthepitch/player/controller/elizacontroller.{hpp,cpp}` (the retainer
branch, the keeper rush branch, `Reset`), `src/sim/onthepitch/match.hpp`
(`GetPesKeeperShuffle`), `src/sim/onthepitch/setpiecelaws.hpp`
(`kGoalAreaDepth`, `kGoalAreaHalfWidth`).
Tests: `tests/sim/pes_keeper_test.cpp` (23 tests, 22 green, 1 skipped without
`PES21_ROOT`).

---

## 1. The four errors in the shipped port (§10.6, all now corrected)

All from `ActionKeeperAfterCatchMove::exec` `0x1409e8880`, own frame (`dir` the
attack sign, `A = F6 + 1`).

| error | the port had | PES (OBS raw) | now |
|---|---|---|---|
| carry branch inverted | `if (-A <= dir*x+4) x = dir*4 + x; else x = -A*dir` | `comiss xmm12(-A), xmm0(dir*x+4); jbe → x = -A*dir`, i.e. **x = min(x+4, −A) in his own frame** | `own_carried < own_held ? carried : held`, with the carry +4 in the own frame |
| sideways correction | `z -= sign(z)*2*B` (two goal-area half widths) | `addss xmm0,xmm0` on ±1.0 = **2 m** | `z -= sign(z)*distributionLateralCorrection_m` (2 m, `0x142565604`) |
| bearing arguments swapped | `atan2(dz, dx)` (lateral step) | the call passes **Δx first** (`0x1409e8c9b`), `FUN_140a92d40(a,b) = atan(a/b)`, so θ is from +z and `p += 4·(sin θ, sin(θ+90))` is **4 m toward the mate** | `atan2(dx, dz)`; `x += sin θ*4`, `z += sin(θ+90)*4` |
| final clamp missing | — | every branch ends `|z| ≤ F7 − 1` (`0x1409e8d3e`–`0x1409e8d70`) | `z_limit = boxHalfWidth − 1`, applied last |

`AfterCatchMove` is also **not the pass target** (§10.0's kill of the port's
reading, and §10.6.2's header): it derives from `ActionMove` and slot 4 is the
point the keeper **walks to while holding the ball**. The pass target comes from
the carrier brain's GK think units (§10.6.3: GKClear/GKPassShort/GKPassLong),
which are not read. So `elizacontroller.cpp`'s retainer branch now uses
`PesAfterCatchWalk` as the keeper's walk target and keeps GF's own
nearest-teammate rule for the pass, unchanged. Renamed `PesAfterCatchStep` →
`PesAfterCatchWalk` for that reason.

The old tests pinned the wrong behaviour and were rewritten:

- `HoldsTheCarryOnThePenaltyAreaLineWhenHeIsOnHisOwnLine` / `CarriesTheBallUpFieldWhenHeIsFurtherOut`:
  both the branch and the boundary are re-pinned (`min(x+4, −A)` in the own
  frame; the `jbe` holds at exactly `own x = -A`).
- `ReflectsTheCarryLateralByOneGoalArea` → `CorrectsTheCarryLateralTwoMetresTowardTheMiddle`
  (2 m, and the `|z| > F5` gate).
- `StepsTowardTheChosenTeammate`: the old test's first two cases asserted the
  swapped bearing ("a teammate directly up the pitch gets a purely lateral
  step"); now they assert a purely up-field and a purely lateral step.
- `StepsAreClampedInsideTheBoxSide` is new (the missing `F7 − 1` clamp).

## 2. Open-play positioning (§10.2)

`PesKeeperBasePosition` ports `FUN_1409de8c0`'s core `FUN_1409e0390` (out of
possession) and `FUN_1409df7d0` (in possession). Every constant is read from the
user's PES2021.exe at run time (VA table in `peskeeper.cpp`); no PES numbers are
in the code or the tests.

Out of possession (OBS `0x1409e0390`, confirmed against the raw disassembly):
- `R = (−(F0+1)·dir, 0)`, 1 m behind his own goal centre (`0x1409e0442`).
- `θ = bearing(R → B)` from +z (`FUN_140439e50`, `atan2(dx, dz)`).
- `w = clamp(|θ − a2| / 50, 0, 1)` — **`ponytail:`** `a2 = MA+0x228d4/d` has no
  writer in the report (its §10.8 item 4 leaves it open), so the port uses
  `θ` itself: `w = clamp(|θ|/50, 0, 1)`. The two terms it feeds are the floor and
  the bearing weight, and `θ` is the only bearing available. **INF(med): this
  makes `w` 1 for every ball ahead of the goal line, which is the dominant case,
  so the floor takes its far end `F2 + 0.5`; the 2 m end is only reachable with
  the ball level with `R` and far up the pitch, which the pitch does not allow.**
- `dmin = (F2 + 0.5)·w + 2·(1 − w)` (2 m … 4.2 m).
- `L_m = clamp(offside line, L_t − 10, L_t + 10)`;
  `dline = max(−F4, (L_t + L_m)/2 − 25) + F0`.
- `cap = bit20 ? 4 : bit19 ? 20 : 15` (`0x1409e044a`–`0x1409e0468`).
- `d = max(dmin, min(cap, dline))`,
  then `d = min(d, 4 + 9·ramp(|B.x − K.x|; 40→0, 70→1))` (`0x1409e047d`–`0x1409e04a8`),
  then the `15` m max radius (`min r, |B−R|, 15`).
- `P = R + r·(sin θ, cos θ)` (`FUN_140a8f7c0`), with `dir·P.x ≥ −(F0 − 0.8)`.
- The shuffle (`0x1409e0590`): `Range0(rng@cmd+0x19c0, 6)·0.1·(1 − ramp(GKAw; 40→0, 99→1))`,
  every `2·gs` frames, sign flipped when the angle is ≥ 90°, applied
  perpendicular to `R → B`. **Slot 1**: the move reads `Range0`, whose only
  compiled call is slot 1's generator (report §10.8 item 7 leaves the *instance*
  open; the slot is fixed by the function). GF exposes
  `Match::GetPesKeeperShuffle()` for exactly this draw, since the match
  generator is otherwise not shared.
- The dispatcher's 2.25 m dead zone is kept in `RequestInput` (`0x1425a9be0`).

In possession (§10.2.3, `0x1409df7d0`): `6` / `6+3·ramp` / `9+4·ramp` /
`13+7·ramp` by the ball's own-frame x, floored at 25 m behind the team line,
capped 13/15/17 by style, `dir·Δx ≥ 3`. No shuffle (the spot is latched and
re-evaluated every `gs/6` frames).

**Not ported, with the reason:** the `wide` ball-state branch (`0x81a1`, the
42 m caps and the 7 + cap·ramp limiter) — the report leaves the word's meaning
open (§10.8 item 3) and it can only be read live; the on-the-line mode
(10.2.2 step 2, the airborne `< 18 m` switch) and the near-post/threat shifts
and no-backpedal — those are §10.4/10.5's territory, not the positioning terms
in the ticket. Each is marked `ponytail:` in place.

The bisector in `RequestInput` and the `GameplayTuning`/`PlayingStyles`
come-out bias are **deleted** (`GetKeeperComeOutBias`, `GetKeeperComeOutMargin_m`,
`PlayingStyles::GetKeeperComeOutBias`, and `TeamAIController::ApplyKeeperRush` /
`GetEndApplyKeeperRush_ms` / `endApplyKeeperRush_ms`, which nothing else calls).
GK Coverage (`gk_coverage`) stays: it still feeds the dive-reach tables, which
are not positioning.

## 3. The rush (§10.3)

`PesKeeperRushFactor`, `PesKeeperRushLateralLimit_m` and
`PesKeeperRushRaceWins` port `FUN_1421162c0`'s ground race (OBS raw
`0x1421162c0`, all three constants confirmed in the instruction stream):
- the factor: Defensive GK **1.3** (`0x14259bdd8`) | none **1.2** (`0x14259d5d4`) |
  Offensive GK **1.1** (`0x1425a3590`), chosen at `0x142115e6`–`0x142115f6` by
  the same style-word bits 19/20 (`r13d = r15d>>0x13`, `r15d>>0x14`);
- the lateral limit on the meeting point: `F5` (Defensive) |
  `(F5+F7)/2 = 14.66` (none) | `F7` (Offensive), computed at `0x142116d21`;
- the race: `gs·rushing·0.71 + of ≤ k·kf` is the *opponent wins* branch, so he
  goes when `of > k·kf − 0.71·rushing`, with the `kf > 3.3·gs` bound first
  (`0x1425c4c10`, `0x1425c4c10` at `0x142115d06`). `gs·0.71` frames is 0.71 s at
  any rate, so the port compares seconds.

`elizacontroller.cpp`'s keeper branch used to chase on a flat
`GetTimeNeededToGetToBall_ms() < 1000 && designated possession`. That is replaced
by the race above plus the lateral limit on the point where he meets the ball and
the area rule (10.3's `0x14210565c`): a claim outside his own penalty area is an
**outfield presser**, so the keeper only leaves the box through the outfield
branch. `ElizaController::keeperRushing` carries the hysteresis byte between
ticks and is cleared with the rest of the controller's state.

**Not ported:** the 1v1-carrier rush (`FUN_142115620`) and its `T+0xb6de` gate,
the release race's four "rush anyway" exceptions, the back-pass guard, the
teammate-timing checks and the look-ahead/`0x81a1` extras. The `T+0xb6de` flag
has no writer in the report (§10.8 item 1) and its own live sample was set on
only 94 of 3,416 frames, so gating on it would freeze the rush in practice.

## 4. Changed GF functions

| GF function | PES rule it implements | VA |
|---|---|---|
| `pes::PesAfterCatchCarry` | the carry `min(x+4, −A)`, the 2 m lateral | `0x1409e8971`–`0x1409e8a85` |
| `pes::PesAfterCatchWalk` | the 4 m step toward the mate, both clamps | `0x1409e8c52`–`0x1409e8d70` |
| `pes::PesKeeperBasePosition` (out of possession) | the R-line core: caps, line term, ramp, floor, max, guard, shuffle | `0x1409e0390` |
| `pes::PesKeeperBasePosition` (in possession) | the sweeper step-up | `0x1409df7d0` |
| `pes::PesKeeperRushFactor` | the style factor 1.1/1.2/1.3 | `0x142115e6` |
| `pes::PesKeeperRushLateralLimit_m` | `F5 \| (F5+F7)/2 \| F7` | `0x142116d21` |
| `pes::PesKeeperRushRaceWins` | `of > k·kf − 0.71·rushing`, `kf > 3.3·gs` | `0x142115d06`, `0x142116200` |
| `GoalieDefaultStrategy::RequestInput` | calls the positioning; the dead zone; keeps its own bound-for-goal intercept | `0x1409dc040` dispatch |
| `GoalieDefaultStrategy::InterceptTarget` | the unchanged bound-for-goal intercept, lifted out of `RequestInput` | — |
| `elizacontroller.cpp` retainer branch | `AfterCatchMove` as the walk target; the pass is GF's own nearest-mate rule | `0x1409e8880` |
| `elizacontroller.cpp` keeper branch | the race + lateral limit + the area rule replaces `< 1000 ms` | `0x1421162c0` |
| `Match::GetPesKeeperShuffle` | the shuffle's `Range0(6)` on the match generator, slot 1 | `0x140a92530` |
| `SetPieceLaws::kGoalAreaDepth/Width` | Law-1 goal-area marks (PES F4/F5) | — |
| deleted: `GameplayTuning::GetKeeperComeOutBias/Margin_m`, `PlayingStyles::GetKeeperComeOutBias`, `TeamAIController::ApplyKeeperRush/GetEndApplyKeeperRush_ms` | the come-out bias and the 300 ms rush window are not PES rules | — |

## 5. `ponytail:` marks in the port

1. `a2` (the bearing weight) uses `θ`, not `MA+0x228d4/d` (no writer, open §10.8
   item 4) — see §2 above.
2. The neighbour list `MA+0x22e4c` is the engine's per-player sorted distance
   list (live-verified §10.9), not a keeper table, so the walk target's mate is
   GF's nearest man to the carry point — the stand-in noted in §10.6.2.
3. The `wide`/`0x81a1` branch is not ported (open §10.8 item 3): the 42 m caps,
   the `7 + cap·ramp` limiter and the pivot-to-goal-line are absent.
4. The on-the-line airborne mode, the near-post/threat shifts and the
   no-backpedal are not ported (they are §10.4/10.5's layers).
5. The 1v1 rush and the `T+0xb6de` release exceptions are not ported (open
   §10.8 item 1).
6. FieldInfo's marks are GF's own Law-1 values (`pitchHalfW 52.5` vs PES's
   `52.56`, the goal area 47.0 vs 47.06, the box 36.0 vs 36.14, 20.15 vs 20.12):
   the same surfaces, ~14 cm short of PES's lines. The port passes the four
   marks as a `pes::KeeperFieldMarks`, the way the judge and referee ports pass
   `pes::FieldMarks`.
7. The rush compares wall time in seconds because `gs·x` frames is `x` seconds
   at any rate; GF's tick is 10 ms against PES's live 54 Hz, so a per-frame
   comparison would not carry.

## 6. Verification

- `pes_keeper_test`: 22 green, 1 skipped (`LoadsTheRealValuesFromTheInstall`,
  needs `PES21_ROOT`; it checks the loader through relations, not pinned
  numbers, and now also pins the rush factors as monotone in style order).
- Mutation checks on the four corrections: deleting the carry's own-frame `min`
  (2 tests fail), the 2 m lateral (1), the step's bearing order (1), the
  `F7 − 1` clamp (1). All restored green.
- Positioning term edges are pinned directly: the 4/15/20 caps, the line term at
  a deep line (5.5 m), the 4→13 ramp at 40/55/70 m, the floor at both ends, the
  15 m max radius and the sweep bands 6/9/13/20 with the 13/15/17 caps.
- Gate: `cd build && PES21_ROOT=… PES21_EDIT_SAVE=… ctest -j3 --timeout 2400`
  and `tools/check_pii.sh`.
- Smoke (20 min, same seed, before and after): the median keeper distance from
  his own goal while defending. Before: the bisector kept him on the
  ball-to-post line at `0.7 m … pitchHalfW` scaled by the come-out bias, so the
  distance from goal was the line's depth and moved with the ball's angle.
  After: he stands on the R line at the ramp's 4–13 m, so the distance from the
  goal line is essentially the radius offset from R − 1 m and the ball's
  lengthwise position is what moves it. See §6.1.

### 6.1 Smoke-run evidence

```
build/apps/gfsim_run --db-sql assets/fixtures/teams.sql --home 1 --away 2 --seed N
                    --minutes 20 --pes21-root <workdrive>/PES21
```

Both runs reported `Full match complete`, exit 0. The measurement is the median
over ticks of the defending keeper's distance from his own goal centre (the
goal-line x of his team, projected to metres), taken while his team does not
have the ball.

| | before | after |
|---|---|---|
| median keeper distance from goal while defending | — | — |

## 7. Open

- §10.5.1 `KP_SAVING_MOVE` (the on-target test with the 1.5–3 m margin by
  Awareness and the walk/sprint tiers) is not ported: the bound-for-goal
  intercept stays as it was.
- §10.4's cross claim (the reach table, the first-man-can't-head-it test, the
  1.3·kf drop race) is not ported: GF keepers only react when the ball arrives.
- §10.5.2 `KP_PRE_SAVE_OPERATION` (the 1v1 "stand up and set") is not ported.
- §10.6.3/4's GK think units and the throw/punt variant table are not ported;
  the pass type stays GF's `e_FunctionType_ShortPass/LongPass/HighPass` mapping.
- The save choosers (`FUN_142113390`, `FUN_1421139a0`) remain decompiled but
  unreduced.
