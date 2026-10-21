# PenaltyIntegrate: PES's penalty scatter drives the shootout's aim (dice cut)

Branch `PenaltyIntegrate` @ `5e870ce` (pushed; `git ls-remote origin PenaltyIntegrate` confirms `5e870cebf2f7707a966a2ea4a82335c99a41773a`).
Commits: `6df84f2` red tests (on base `9d42629` = `origin/PenaltyScatter`) → `d2e2fec` cutover → `f2b7a29` merge of `origin/main` @ `1aa17c3` (batch 3 landed; plain merge, no conflicts) → `5e870ce` port note.

## 1. What changed

- The shootout's kick aim is PES's kick-kind-54 scatter. `PenaltyShootout::AimPenaltyKick(k, rng, taker)` returns `KickAim{lateral_m, height_m, radius_m}`: the radius is `pes::PenaltyScatterRadius` on the base point, the jitter is `pes::ApplyPenaltyJitter` drawn from the taker's own generator (`GetPesRandom()`, slot 2).
- Deleted (clean cutover): `PenaltyShootout::CalculateAim`, `IsOnTarget`, `GetSaveChance`, `ResolveKick`, the `Shooter`, `Keeper` and `Aim` structs, and their constants (`aimCentreHeight`, `minAimSpread`, `maxExtraAimSpread`, `maxSaveChance`, the height spreads, `placementSaveRelief`, `Clamp01`, `ClampSample`, `GetShooterSkill`). Controller: `plannedOutcome`, `GetPlannedOutcome()`, the keeper inputs, and the planned-outcome verdict log. `PlayerSkills::GetPenaltyReflexes` (only the dice used it).
- Verdict: unchanged path. `ObserveKick` classifies the ball and the keeper's touch. `SimMatch.ShootoutVerdictIsWhatTheBallDid` covers it.
- Constants: `pes::KickErrorConstants::penaltyScatter`, loaded inside `LoadKickErrorConstants` (so `match.cpp`, `elizacontroller.cpp`, `humanoid*.cpp` are untouched). Reached as `match->GetPesKickErrorConstants().penaltyScatter`.
- `GetAimHeight()` stores the kick's height. No strike reads it yet (see open item 1).
- Port note `docs/ports/pes-port-PenaltyScatter.md`: the draw order was wrong (it said height first). Now lateral first, height second. "Not done" replaced by the integration status.

## 2. Decoded (Ghidra bridge; decompile, CONFIRMED unless marked)

- **FUN_1406E3210 case 0x36** (jitter): `lateral += FUN_1406EAA10(modulus)/1000` first, `height += FUN_1406EAA10(modulus)/1000` second, `height = max(0, height)`. modulus = `(int)(radius*1000)`. CONFIRMED.
- **FUN_1406EAA10**: sign draw `% 100 < 50` (+), then magnitude `% modulus`; two `FUN_140A92470(slot 2)` per call, so **four `Range(2)` per jitter**. `FUN_140A92470` returns unsigned values (mask `0x7FFFFFFF`), so the `abs` in FUN_1406EAA10 is a no-op. CONFIRMED. The draw count is re-checked (test `TheKickDrawsFourTimesFromTheTakersSlotTwo`).
- **FUN_1408B7550** (action builder): branches on `*(char*)(lVar9+0x11b0)` (0x15 / 0x16); both call `FUN_1408ABC30` (base point) then `FUN_1406B4000(charge, …, 0x36)` (radius, kind 54). CONFIRMED. In the 0x15 branch `param_5` (lateral scale) is literal 0, so the base lateral is 0. CONFIRMED. Which branch a shootout kick takes: OPEN.
- **FUN_1408ABC30** (base point): `x = side*geom[+0x0]`; `height = (geom[+0xC] - (tune[+0x2C]*C + geom[+0x3C]*0.5)) * 0.5`; `lateral = -(geom[+0x8] - (tune[+0x34]*C + geom[+0x40]*0.5)) * side * scale * ramp * sign`; `tune = FUN_141E5CA20(ctx, 0x36)`; `C = DAT_142598958`. CONFIRMED structure. Tuning values and geometry +0x3C/+0x40: runtime, not read (OPEN).
- **FUN_141E5CA20(ctx, i)** = `*(*ctx + 8i) + 8` when `i < FUN_141E5DE80()` (= 0xEA). CONFIRMED. **FUN_14043A0A0** wraps an angle difference to ±180 (period 360). CONFIRMED.

### 0. Constants (script output: `pes_full.bin`, offset = VA − 0x140000000)

```
FUN_1408abc30 0.5 (half)                    VA=0x142565600 float=0.5
FUN_1408abc30 DAT_142598958 (scale C)       VA=0x142598958 float=0.1086859330534935
FUN_1408abc30 90 (quarterTurn)              VA=0x14259be5c float=90.0
FUN_1408abc30 360 (fullTurn)                VA=0x14259858c float=360.0
FUN_1408abc30 DAT_142598598                 VA=0x142598598 float=-360.0
FUN_1408abc30 1.0                           VA=0x14256ae1c float=1.0
FUN_1408abc30 -1.0                          VA=0x142588218 float=-1.0
FUN_1408b7550 DAT_142598580 (penalty angle) VA=0x142598580 float=180.0
FUN_1408abc30 DAT_14256b140                 VA=0x14256b140 float=0.44999998807907104
FUN_1408abc30 DAT_14259222c (ramp floor)    VA=0x14259222c float=5.0
FUN_1408abc30 DAT_14259be2c                 VA=0x14259be2c float=40.0
FUN_1408abc30 DAT_14259be30                 VA=0x14259be30 float=45.0
FUN_1408abc30 DAT_14259be60                 VA=0x14259be60 float=135.0
FUN_1408abc30 DAT_1425c8380                 VA=0x1425c8380 float=175.0
FUN_1408abc30 DAT_1425c6604                 VA=0x1425c6604 float=-40.0
FUN_1406e3210 1000 (mm per unit)            VA=0x14256ae34 float=1000.0
FUN_1406e3210 DAT_142598578 (0.2)           VA=0x142598578 float=0.20000000298023224
```
The scatter's own constants come from the install through `pes::LoadPenaltyScatterConstants`. Test `PesKickErrorInstall.LoadsFromTheExe` (10/10) covers the new plumbing.

## 3. Stand-ins (each carries a `ponytail:` comment in code)

| Input | Stand-in | PES site that removes it |
|---|---|---|
| Base height | bar × 0.5 | FUN_1408ABC30 height term, runtime tuning entry `tune[+0x2C]` and geometry `+0x3C` |
| Base lateral | 0 | the penalty branch passes lateral scale 0 (INF which branch a shootout kick takes) |
| Post / bar | GF `goalHalfWidth` 3.7 / `goalHeight` 2.5 (`gametypes.hpp:74-75`), per the brief | the geometry object `+8/+0xC` (port note: 3.66 / 2.44) |
| Charge (power) | `physical_shotpower` (0..1) | strike sub_140815BE0: Kicking Power through the penalty tuning table at `0x140815cc8` |
| Place Kicking | `technical_setpiece` (×100) | the PES ability at taker `+0x20`; mapping INF from `playerskills.cpp:61` (the PK Specialist card's stat) |
| Finishing | `technical_shot` (×100) | CONFIRMED by the comment at `humanoid.cpp:52` (Finishing for a shot) |
| Draw timing | once per kick, in `SetUpKick` | PES draws in the action's per-frame update (INF) |

## 4. Tests (red → green, one mutation each)

Red at `6df84f2`: `tests/sim/penalty_shootout_aim_test.cpp` fails to build with `'KickAim' in namespace 'PenaltyShootout' does not name a type` and `'AimPenaltyKick' is not a member of 'PenaltyShootout'`.

Green at `d2e2fec` and on the merged tree (`f2b7a29`/`5e870ce`):

| Binary | Result |
|---|---|
| `penalty_shootout_aim_test` | 3/3 (`APerfectTakersAimStaysInsideTheScatterRadius`, `ThePkSpecialistCardTightensTheRadius`, `TheKickDrawsFourTimesFromTheTakersSlotTwo`) |
| `pes_penalty_scatter_test` | 8/8 |
| `pes_kick_error_test` | 10/10 (incl. `PesKickErrorInstall.LoadsFromTheExe`) |
| `sim_match_test --gtest_filter=*Shootout*` | 1/1 (`SimMatch.ShootoutVerdictIsWhatTheBallDid`) |
| `pes_clock_test --gtest_filter=*PenaltyShootout*` | 1/1 |

Mutations (each applied, rebuilt, run, reverted; the reverted files match the committed content):

| # | Mutation | Test that fails | Evidence |
|---|---|---|---|
| M1 | `aim.lateral_m = 2 * jittered.lateral_m` | `APerfectTakersAimStaysInsideTheScatterRadius` | `EXPECT_LE` fails on seeds 2, 3, 4, 8, 9, 10, … |
| M2 | `base.penaltySpecialist = false` | `ThePkSpecialistCardTightensTheRadius` | `cardRadius < plainRadius`: `1.04524112 vs 1.04524112` |
| M3 | extra `rng.Range(2)` after the jitter | `TheKickDrawsFourTimesFromTheTakersSlotTwo` | `EXPECT_EQ` at line 108 fails |
| M4 | `FinishKick(PenaltyShootout::e_Outcome_Goal)` (constant verdict) | `SimMatch.ShootoutVerdictIsWhatTheBallDid` | fails at kick 1 and kick 3 |

Gate: not run (Main gates). Full build on the merged tree: `BUILD_EXIT=0`, `#deps 0` count 0.

## 5. Smokes (2 min, seed 7, sim lock held; merged code `f2b7a29`+`5e870ce`)

| Mode | Run a | Run b | Result |
|---|---|---|---|
| switch-off | `[determinism] 961c600e97796617` | same | Full match complete HOM 0 - 1 AWA; `[idle] 29 episodes, stalls 15` |
| `--pes-loop` | `[determinism] 182e4fe85687c8db` | same | Full match complete HOM 2 - 1 AWA; `[idle] 26 episodes, stalls 9` |

No crash, no hang, rerun hashes identical. The 2-minute match never reaches the shootout (0 `PenaltyShootout` lines). I did not run `origin/main` for a same-commit smoke. The change cannot touch the 2-minute path (the scatter runs only in `SetUpKick`, and the only other edit is the constants layout), so the hashes should match main's, but Main should confirm on the batch.

## 6. Open items

1. **Height carrier** (blocks the height term acting on the strike). `elizacontroller.cpp:275-277` builds the taker's touch direction with z = 0, and `GetShotVector` (`humanoid_utils.cpp:449`) takes 2D angles. Both are busy, so untouched. I messaged Main; no reply yet. Minimal fix: read `GetAimHeight()` there and carry it into the target's z. Until then `GetAimHeight()` is stored and unread.
2. **Base point**: the kind-54 tuning entry `FUN_141E5CA20(ctx, 0x36)` (+0x2C, +0x34) and geometry +0x3C/+0x40. The table's source is not found. OPEN.
3. **Penalty branch**: 0x15 or 0x16 in FUN_1408B7550 for a shootout kick. OPEN.
4. **Goal dimensions**: GF `goalHalfWidth` 3.7 / `goalHeight` 2.5 vs PES 3.66 / 2.44 (port note). The brief asked for GF's constants, so I used them. This also moves GF's own goal classifier, so it is a Main decision.
5. **Power**: the aim power is now the taker's stat, not the dice's 0.55..1.0 bands. Its distribution is unmeasured in a shootout run (no smoke reaches one). Watch conversion in the next live shootout.
6. **Aim is the goal's centre ± scatter** (INF): the keeper now saves central kicks by physics. Expect a save-heavy shootout until items 2-3 land.
7. **Draw timing** (INF): once per kick, not per frame.

Incident: two relative-path writes (an agent message file and the test file) landed in the owner's `<local>`. Both removed or moved into the worktree; no other write there.
