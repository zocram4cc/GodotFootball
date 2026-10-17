# PES playing-style effects on the off-ball run hosts (StylePort2): one effect placed, the rest listed with reasons

Branch `StylePort2` (pushed). Base `origin/StylePort` `20c51c7` (the `pes::style` gate exists only there, not on main). Commits: `c84e4ba` (the port and tests), `3e36df0` (comment width only). Worktree `<workdrive>/gf-StylePort2`, build `build/`.

## Summary

- **Placed (pes_loop only):** the Goal Poacher (bit 0) and Fox in the Box (bit 2) forward-run depth term of `FUN_1405D8B70` L650-657, in roles 7-9, for a player who is neither carrying the ball nor in a run action, with the style listed in his compat row. Value 5 m from VA `0x14259222C`, read from the install.
- **Not placed:** every other effect on the three named hosts. Each one is blocked on PES state GF does not carry, or has no GF counterpart. Details in section 4.
- **Already placed in GF before this pass (no change):** Destroyer tackle-from-behind level and tolerance table; keeper style caps and rush factors.

## 1. Decoded (CONFIRMED unless tagged)

### 1.1 `FUN_1405D8B70` run-depth term (decompile read this pass)

- Per-slot loop. `uVar12` = the slot's player; `local_1cc` = `FUN_140a627e0(team,0)`, the ball owner (INF: GF's designated possession player stands in).
- `local_1d5` = `team+0x254`, the attack sign `s` (INF on the name, same as the duels doc).
- `uVar33` = `P+0x3FB8[k]`, the slot's kind (INF: the PES role class `FUN_1408c51f0` numbering, GK0 CB1 SB2 DMF3 CMF4 SMF5 AMF6 SS7 WF8 CF9; the same numbering GF already uses for the compat rows).
- Branch: `uVar12 != local_1cc` (not the carrier) → `cVar10 = FUN_1405a4790(param_2[0x1f], action, 0, 1)` must be 0 (not running) → `uVar33 > 4`. For `uVar33 < 7` the style term needs `local_1d6 < 2`, with `local_1d6 = team+0x7b84` (**OPEN**, not decoded). For `7 ≤ uVar33 < 10` it always applies.
- Style term: bit 0 `fVar36 += s·5` (`0x14259222C` = 5.0, CONFIRMED-const, read by the install test); bit 2 `fVar36 += s·5`; bit 4 `fVar36 −= s·T[slot]` (**OPEN**: `T = param_3+0x3A9C+slot·12`).
- `FUN_1405A4790` (decompile read this pass): run actions 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x24, 0x25, 0x2C return true; 0x27 needs `param_4`; 0x2F needs `param_5 ∈ {6,7}`. With `(0,1)` this is exactly GF's `IsRun` set (`pesoffball.cpp`, the anonymous namespace at `IsRun`). The GF run state (`TeamAIController::GetRunState`) stands in for the PES action `+0x5d0` (INF).

### 1.2 Style gate

- Bit 0 Goal Poacher, bit 2 Fox in the Box, through `pes::style::Gate` (the `FUN_1408CB0B0` gate: style word AND compat row), as the host calls `FUN_140a64030`. The host does not check the possession state; it has no `teamHasPossession` term.

## 2. Changed

- `src/sim/onthepitch/pesoffball.hpp/.cpp`: `OffBallConstants::styleRunDepth_m` (loaded from `0x14259222c` in `LoadOffBallConstants`); `StyleRunDepth(k, roleClass, styleWord, runState, carrier)` placed after `IsRun` in the `pes::offball` namespace.
- `src/sim/onthepitch/teamAIcontroller.cpp` `GetAdaptedFormationPosition` (the GF counterpart of the slot target; the depth term of `GetDepthOffset`): under `match->PesLoop()`, for `GoalPoacher` and `FoxInTheBox` only, the depth is `StyleRunDepth(...)` instead of `GetDepthOffset(...)`. Other styles keep the GF stand-in. Sign and frame: the expression stays `-team->GetSide() * (depth + trackBack)`, the same attack-signed form as the GF stand-ins (INF that `-GetSide()` = PES `s`).
- `tests/sim/pes_offball_test.cpp`: 8 new tests (`OffBallStyleRunDepth.*`) and one install assertion (`styleRunDepth_m == 5.0f` from the exe) in `OffBallInstall.CapsAndLimitFromTheInstall`.

### Behaviour change on pes_loop (intended by the decode)

- Goal Poacher: GF's depth 0 becomes +5 m in the attack direction for roles 7-9 when not running and not carrying.
- Fox in the Box: GF's +3 m with possession (0 without) becomes +5 m in the same cases, with or without possession. The PES host has no possession term.
- Roles 5-6 and the other roles: no term (see section 4, item a).

## 3. Verification

- **Red:** the new tests did not compile against `origin/StylePort` (no `StyleRunDepth`, no `styleRunDepth_m`). That is a compile red, not a behaviour red. No stub was written.
- **Green:** `pes_offball_test` 62/62 (54 existing + 8 new; install test included). `sim_match_test` 10/10 on the final tree (links the changed `teamAIcontroller.cpp`). Not run: other gfsim-linked test binaries (Main's gate); `pes_style_gate_test` (its sources are unchanged).
- **Mutations** (one at a time, rebuilt, run, reverted; all caught):
  - M1 carrier exclusion dropped: `CarrierGetsNoRunTerm`.
  - M2 run gate dropped: `ARunningPlayerGetsNoRunTerm`.
  - M3 CF (role 9) dropped from the window: `GoalPoacherInACentreForwardRoleGetsFiveMetres`, `FoxInTheBoxInACentreForwardRoleGetsFiveMetres`, `AStandingPlayerOfTheSameStyleStillGetsIt`.
  - M4 roles 5-6 included: `RolesFiveAndSixWaitForTheUndecodedTeamByte`.
  - M5 magnitude 5 → 6: four tests including `GoalPoacherInACentreForwardRoleGetsFiveMetres`.
  - M6 Poacher compat row ignored: `CompatRowDecidesWhichForwardRoleKeepsTheStyle`.
  - M7 Fox gate uses the Poacher bit: `FoxInTheBoxInACentreForwardRoleGetsFiveMetres`.
  - M8 any non-zero run state counts as running: `AStandingPlayerOfTheSameStyleStillGetsIt`.
  - M9 (wiring, `teamAIcontroller.cpp:729`) `PesLoop()` guard dropped: off-path smoke hash moves to `1a01841617c0b1ad`, caught by the off-path smoke.
- **Smoke** (`gfsim_run --db-sql assets/fixtures/teams.sql --home 1 --away 2 --seed 7 --minutes 2 --pes21-root <workdrive>/PES21`, run directly, no `gf-sim.lock`, per the assignment; the baseline binary was built from unmodified `20c51c7`):

| run | baseline `20c51c7` | after `3e36df0` |
|---|---|---|
| off path | `a3d5e43a3b7b03af` | `a3d5e43a3b7b03af` (unchanged) |
| `--pes-loop` | `011518e07fbeabc4` | `079b211592495284` (two reruns identical) |

- Tripwire on the new `--pes-loop` run: exit 0, no hang, passes 14-11, shots 3-0, `[balance] possession 61%-39%`. Deterministic across two runs.
- Gate: not run (Main gates).

## 4. Effects not placed, and why

Named hosts first (the brief's list), then the rest of the 228-site census that the StylePort doc lists as open.

a. **Forward-run term, roles 5-6** (`1405D8B70` L650-653): gated on `team+0x7b84` (`local_1d6`), not decoded. GF has no equivalent field. Remove the ponytail when decoded.

b. **Forward-run term, bit 4 Creative Playmaker** (`1405D8B70` L659-661): `−s·T[slot]`, `T = param_3+0x3A9C+slot·12` (OPEN, per-slot team table).

c. **Goal Poacher run threshold 30→15 and close radius 11→16 m** (`140595AB0`; VA `0x142591140`=30, `0x14259BE0C`=15, `0x1425C77D8`=121, `0x1425CA314`=256; style gate `FUN_140a64030(param_2, 0)`). The host is the Chance accept test. GF has its counterpart site in `pesoffball.cpp` (the `commits` lambda, ponytail at ~421), which does not run this test. Blocked: the first gate, `FUN_140a50250(ctx, team, k)` (`k = 0,1,2`), reads the ball holder's in-match dwords (`rec+0xd4+4k`). GF does not carry that record. With a stand-in of 0, the host returns 1 (reject) on every call, which would switch off every Chance run. The caller's `param_4` (the compared value) is not decoded either.

d. **Target Man −6·s, Orchestrator −6·s, Defensive FB −3·s** (`1405E6E80` L429-433, L442-446, L447-450; constants `0x14259BDEC`=6, `0x14258C264`=3). The host is the off-ball team-tick slot-target builder (`pes-re-off-ball.md:249`), not the carrier path. Its gates are per-slot team words, not GF state: `local_488` comes from `+0x80A0` and `+0x80E0[k]` and the `+0x7ED4[k]` flags (L292-342); Target Man also needs `param_6 != 0` and `+0x7ED4[k] != 0`. GF has none of these. Sign to resolve on port: GF's `GetDepthOffset(TargetMan)` is +2 with possession, PES says −6·s.

e. **Fox/Poacher/Target Man lateral terms** (`1405E6E80` L527-530, L552-556, L578-582, L590-594; `fVar47`): replacements and multipliers gated on the sign of `fVar48` and on `fVar42` (both undecoded). GF's `GetWidthOffset` values are not the same quantity; no faithful stand-in.

f. **Extra Frontman +0.2 and the designated aerial attacker** (`1405FC860` L62-65, writer of `P+0x3EF8`; `1405FC500` Target Man +30, same set-piece family). GF has no aerial-attacker pick (a grep of `src/sim` finds no set-piece header or designated-attacker selection), so per the brief this is not ported. The Extra Frontman doc's own finding is that the game reverts the bit within seconds on the live run.

g. **Hole Player** (key 0x1D +14, key 0x1F ×1.3; `140612660`, `14060A8D0`) and **Hole / Box-to-Box +5** (`1405DEF90` L88-92, `1405DF1F0` L72-74): candidate-score terms. No GF candidate score was located for them (not exhaustively checked).

h. **Poacher target floor 25** (`140662A10`, VA `0x14259BE20`): a floor on a reach value inside a distance test. No GF site located. **Poacher `5a5b20` floor 26** is a second Poacher effect whose GF site exists (`pesoffball.cpp` near the reach chain, ~1754-1756, stubbed off). Its host needs the state decode (states 0x1D-0x2C, timer ring); not decoded here.

i. **`1406087A0` −12** (VA `0x14259BE04`): meaning INF in the style doc; not evaluated.

j. **Full-back blend** (`1405DF5A0` L105-121: base `(0.5a+b)/1.5` with 1.5, bit 16 `(a+b)/2` with 2.0, bit 17 `b`) and **set-piece clamps** (`140680980` L293-303: [0, 0.1] and [0, 0.5]): no GF counterpart located.

k. **Dummy Runner** (bit 1, key 0x21, record 0xBD) and **Box-to-Box** profile (key 0x3A, record 0xBC): dt18 record values are not in the exe (OPEN). **Dispatcher** `0x1429530D8`: OPEN.

l. **Carrier-path effects** (`140640CC0`, `140641690`, `140608AC0`): A4Cont's; not touched.

m. **Already placed, no change:**
   - Destroyer (bit 12) tackle level: `elizacontroller.cpp:820-826` (`_TackleAggressionLevel`, +1 for the style, +1 for the team defensive mode) and the tolerance table `pesdefenderdecisions.hpp:64`. The doc's 0° VA `0x14259BE20` is 25.0; the 0° is a register-zeroed literal (`pesdefenderdecisions.cpp:91`).
   - Keeper caps and rush factors (bits 19/20): `peskeeper.cpp` takes the `offensiveStyle`/`defensiveStyle` flags from the same style bit (`GetPlayingStyle()`), so the "rewire" in the StylePort report is a source change with no behaviour change.

## 5. Corrections to existing docs

1. `pes-port-StylePort.md` (carrier list): `1405E6E80` is the off-ball team-tick slot-target builder, not the carrier path.
2. `pes-port-StylePort.md` (Open, Goal Poacher): the GF site is `pesoffball.cpp:421`, not `:418`.
3. `pes-port-StylePort.md` (Open, Keepers): stale; see 4m.
4. `pes-re-style-effects.md` §1 row 0 and §6: `140662A10` is a distance floor inside a reach test, not a target-x floor.
5. `pes-re-style-effects.md` §2/§6 and the Frontman doc: the `1405D8B70` `+5` terms are gated on roles 5-9 and on the run predicate (`FUN_1405A4790`, decoded here), not on slots 5-9 by position.

## 6. Open items

1. `team+0x7b84` (`local_1d6`): roles 5-6 forward-run term (4a).
2. `T[slot]` at `param_3+0x3A9C` (4b).
3. Holder in-match dwords `rec+0xd4+4k` (`FUN_140a50250`): the blocker for the Poacher Chance test (4c).
4. `local_488` team words `+0x7ED4[k]`, `+0x80A0`, `+0x80E0[k]`, and `param_6` (4d).
5. The sign and frame check for the Target Man / Orchestrator / Defensive FB terms (4d).
6. Role-class ↔ kind mapping (`P+0x3FB8`): INF, used as-is by the gate.
7. Run state ↔ action `+0x5d0` and carrier ↔ `FUN_140a627e0(team,0)`: stand-ins, INF on equivalence.
8. Lateral `fVar42`/`fVar48` (4e). Set-piece clamps and full-back blend (4j).
