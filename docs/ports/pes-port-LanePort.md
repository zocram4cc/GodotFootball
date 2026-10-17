# PES pass-lane core 0x14063CD90, wrapper 0x14063DC10/0x14063DE20, helpers: port (branch LanePort)

Scope: spec `docs/re/pes-re-lane-core.md` §3–5 (lane core, wrapper, helpers). Ported as a pure module with tests. Nothing is wired into the carrier. Item 3 of the brief (one-touch 0x142128C10) is NOT ported: its tables are open (§Not ported).

## Result
- Done: lane core `0x14063CD90` (`ScanLane`), wrapper `0x14063DE20`→`0x14063DC10` (`LaneFree`), helpers `0x140470D60` (`HeadingAdd`), `0x14043C410` (`IsAwaySlot`), `0x140A62690` (`KeeperSlot`), the install constants (`LoadLaneConstants`).
- Not done: one-touch `0x142128C10` (listed below). Not wired: no caller in the sim.

## Commits (branch `LanePort`, confirmed by `git ls-remote origin LanePort`)
- `6a257d8` RED: header, stub, failing tests (13 fail, 2 pass by construction on the stub).
- `b2e5ceb` GREEN: core. 15/15.
- `6eae74e` boundary tests (2) and a wrapped line. 17/17.
- `75a6b89` head: four more boundary tests. 21/21. Head = `75a6b895a04badb4e3bf0f04f95cb7680fc48758`.

## Files
- `src/sim/onthepitch/peslane.hpp`, `peslane.cpp`: new module (namespace `pes::lane`).
- `tests/sim/pes_lane_test.cpp`: new test binary (21 tests).
- `tests/sim/CMakeLists.txt`: 4 lines, the `pes_lane_test` target.

## Behaviour (per spec §3.3, float32)
- Each side-`s` player `q` (not `reference`, active, not the keeper unless `keepKeeper`) is predicted `D = P + h·V` (xz).
- Opponent path (scanned side ≠ side of reference): reject when `|wrap180(bearing(O→D) − θ)| > max(0.5c + 10, 90)`. Teammate path skips this.
- Annulus-and-cone (`AT`): `rad² < |D−P2|² < (rad+len)²` and `|wrap180(bearing(P2→D) − θ)| < 0.5c`, with `P2 = O + rad·dir(θ+180)`.
- Otherwise segment crossing (`CT`): `properly_cross(O, E, P, D)` and `|cross2(E−O, D−P)| ≥ eps`, with `E = O + len·dir(θ)`.
- Recorded players: count; nearest = least `|D−O|²` (first scanned wins a tie).
- `LaneFree` = scan of the side opposite `reference` records nobody.

## VAs (from the install; read by `LoadLaneConstants`, C-asm per spec §2)
| Constant | VA | Value | Status |
|---|---|---|---|
| sin table + scalars (360, 180, 90, quarter, one, tabScale …) | 0x142655950 and the `SPEED_SCALARS` VAs in `pes/movestep.cpp` | table | CONFIRMED (read from install by reused loader) |
| radToDeg | 0x14259BE40 | 57.29578 | CONFIRMED (spec §2) |
| twoPi (rad) | 0x1425795E4 | 6.2831855 | CONFIRMED (spec §2) |
| coneScale | 0x142565600 | 0.5 | CONFIRMED |
| coneOffset | 0x142588210 | 10 | CONFIRMED |
| coneFloor | 0x14259BE5C | 90 | CONFIRMED |
| crossEps | 0x14259BDC4 | 1.1920929e-7 | CONFIRMED |
| keeper category | (none) | 0 | INF (O1): selector is confirmed, meaning "keeper" is from the docs |

## Reuse (rung 2)
- `pes::LoadSpeedClassConstants`, `pes::TableSinDeg`, `pes::WrapHalfTurn` from `pes/movestep.hpp` (movestep owned by A3States). `offball::Vec2f` from `pesoffball.hpp`. Any change to those signatures reaches this module.

## Decisions and INF
- `Bearing` uses `std::atan2` (ponytail). PES's `0x140A92DC0` polynomial is undecoded (O3). The difference is float rounding, not measured. One place to change: `Bearing`.
- `cos(x) = sin(x + 90)` via the table (INF: PES's `0x140A8F7C0` is assumed to do the same as `FUN_14081e340`).
- `v` is per PES frame (O2, C-dec only; live not sampled).
- Dead inputs dropped, per spec §3.5: `_unused`, the `p8` shift, the unreachable `<11` cap, the `q < 22` guard (side is 0 or 1).
- `ScanLane` returns `{nearest, count}` only. The entries array is not kept: no caller needs it yet.
- Keeper is category 0 (O1). `KeeperSlot` picks the first active man with category 0 on the side.

## Tests (`pes_lane_test`, 21/21, run with `PES21_ROOT=<workdrive>/PES21`, no SKIPs)
Fixtures skip only when `PES21_ROOT` is unset. Lane values are the one-touch G11 lane (length 15, cone 60, radius 2), test input only.
- ClearLaneRecordsNothing; OpponentInsideConeIsRecorded; OpponentOutsideConeAngleIsNotRecorded
- OpponentCrossingTheSegmentIsRecordedAndBeyondItsEndIsNot; PathAlongTheLaneLineIsNotACrossing; PathThatOnlyTouchesTheLaneLineIsNotACrossing
- PredictionUsesVelocityOverTheHorizon
- NearestOfSeveralIsTheClosestPredictedPlayer; EqualDistancesKeepTheLowestScannedSlot; NearestIsMeasuredFromThePredictedPosition
- TeammateBranchSkipsTheOriginConeAndExcludesTheReference
- LaneFreeScansTheSideOppositeTheReference; IsAwaySlotIsElevenThroughTwentyOne
- KeeperIsExcludedUnlessKept; KeeperSlotIsTheFirstActiveCategoryZeroOfTheSide
- LaneFollowsItsHeadingTowardPlusX; WideConeUsesTheOffsetFormula; LaneAngleWrapsAcrossNorth; AnnulusBoundsAreExclusive; AnnulusReachesRadPlusLengthFromTheApex
- HeadingAddWrapsIntoOneTurn

RED evidence: at `6a257d8` against the stub, 13 failed and 2 passed (ClearLane and OutsideCone, passing because the stub records nothing).

## Mutation pass (one mutant at a time; edit, rebuild, run, revert; each reverted to the committed text)
All 26 mutants were caught. "by" is the number of failing tests.
1. prediction ignores velocity: 2
2. horizon halved: 2
3. crossing test removed: 1
4. crossing denominator floor removed: 1
5. nearest tie picks last (`<=`): 1
6. nearest picks farthest: 1
7. keeper never excluded: 1
8. KeeperSlot ignores `active`: 1
9. LaneFree scans the reference's own side: 2
10. teammates get the origin cone: 1
11. opponents skip the origin cone: 1
12. opponent limit `min` instead of `max`: 2
13. opponent limit without the +10 offset: 1
14. annulus uses full cone, not half: 1
15. annulus outer radius = len alone: 1
16. HeadingAdd without wrap: 1
17. IsAwaySlot admits 22: 1
18. Dir sin/cos swapped: 3
19. Bearing atan2 arguments swapped: 9
20. origin test without angle wrap: 1
21. annulus bearing without angle wrap: 1
22. properly_cross non-strict: 1
23. properly_cross one clause only: 1
24. nearest distance from current, not predicted, position: 1
25. annulus inner bound non-strict: 1
26. annulus outer bound non-strict: 1

Not a behaviour change, so not run: dropping the `!inCone` guard on the crossing test. A recorded player is recorded either way, so it is equivalent.
Not testable: the exact cone boundary `|B| == 0.5c` (no float construction reliable enough).

## Smoke (off-path, nothing wired)
- `gfsim_run --db-sql assets/fixtures/teams.sql --home 1 --away 2 --seed 7 --minutes 2 --pes21-root <workdrive>/PES21`: exit 0, `[determinism] a3d5e43a3b7b03af`, which matches the reference off `origin/main` given in the contract. I did not re-run `origin/main` myself.
- The `[balance*]` lines were not used as acceptance (smoke policy).
- Ran without the gf-sim lock, per the contract. Full ctest not run. Gate not run (Main gates).

## Not ported: one-touch predicate 0x142128C10 (spec §6)
Gates G1–G12 are decoded and asm-checked in the spec. Blocked on open tables:
- O5: owner of the NearestEntry list behind `score` (`0x140587E70`) and the 1600 threshold's semantics.
- O6: contents and builder of the curve table `X5+0x2B0` (`0x142139650`), which G12 reads.
- O16: the pass-into-lane band depends on O6.
- O11, O12, O13: smaller helpers (byte return, level writers, IsClose) are C-dec only.
Not stubbed: the brief says list it if the tables are not decoded. Its G11 is a call to `LaneFree` with (len 15, cone 60, radius 2, keep false, h 0), which this core supports.

## Open items
1. O1: confirm keeper = category 0 live.
2. O2: confirm velocity units live.
3. O3: replace `std::atan2` in `Bearing` with the decoded `0x140A92DC0` polynomial.
4. O5, O6: one-touch port (see above).
5. O14: map the lane structs of BackSpace `0x142129110`, GoalFront `0x14212ABA0`, Safety `0x14212B520`, OneTwo `0x14212AEB0`, DirectHighBall `0x142129BE0` before wiring. The teammate branch has no caller yet (INF until O14).
6. Coupling: `peslane` reuses A3States' `pes/movestep` helpers (see Reuse).
7. Note for Main: `git status` in the worktree prints "expected submodule path 'extern/godot-cpp' not to be a symbolic link" because of the brief's symlink. Use `-c status.submodule=none`.
