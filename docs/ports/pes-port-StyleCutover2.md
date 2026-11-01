# StyleCutover2: bit 4 and the roles 5-6 gate at the run-depth site, the width / poacher / dribble style stand-ins deleted, the keeper / Destroyer / cross sites on the style word (StyleCutover2, 01-11-26)

Branch `DribbleProposal` (worktree `gf-DribbleProposal`), pushed `1be2ba8c` (`git ls-remote` confirmed; earlier push of this task `22898c58`, merge of `origin/main` `a135736c`). Continues `pes-port-StyleCutover.md` Left 1-3 and `pes-port-DribbleRow.md`. CONFIRMED = decompile (bridge), asm, live read or test run this session; INF = inference; OPEN = not settled. Image `pes_full.bin` (VA - 0x140000000). Constants below were read from the image by script: `0x14259222C` = 5.0, `0x142588210` = 10.0.

## 0. Verdict

| Step | State |
|---|---|
| 1 merge `origin/main`, push | DONE (`a135736c`). |
| 2 bit 4 + roles 5-6 gate | DONE, and the premise was wrong in one half: the bit-4 term is NOT a team-record x (section 1). The roles 5-6 gate is the team record's phase-1 forward-line count, written by `FUN_1405F9CD0`; carried in GF's PES-layout team record and applied. |
| 3 remaining enum consumers | `GetWidthOffset`, `GetPoacherTargetX`, the four dribble knobs' style terms: DELETED. Keeper (bits 19/20), Destroyer (bit 12), Cross Specialist (bit 7) sites: cut over to the style word (`Match::StyleHas`). The enum is NOT yet deletable (section 4). |
| 4 controller-level carrier / run-action tests | NOT DONE (section 5). Function-level pins exist. |
| 5 smokes | Both moved; the new PES terms moved neither (ablation, section 6). |

## 1. Bit 4 (Creative Playmaker): the subtrahend is the constant 10 m, not `team+0x3A9C` (CONFIRMED)

- `FUN_1405D8B70` decompile L196 sets `fVar30 = s * team[+0x3A9C + slot*12]`, but L482 (`fVar30 = DAT_142588210`) overwrites it on every path to L650-662. Asm: `0x1405D8F7D MULSS XMM7,[R13+RBX*4+0x3A9C]` (the L196 product) and `0x1405D9841 MOVSS XMM7,[0x142588210]` (L482 reload); the bit-4 subtract is `0x1405D9F80 MULSS XMM0,XMM7` then `SUBSS XMM13,XMM0`. A reaching-definition pass over the whole function's disassembly (jump table `0x1405DA370` resolved; script `rd7.py`, scratch) finds exactly one XMM7 definition reaching `0x1405D9F80`: `0x1405D9841`. So `fVar36 -= s * 10.0` (`DAT_142588210`).
- This also reverses `pes-re-style-effects.md` §5's "correction" ("`fVar30` is reassigned in the slot loop ... not the constant 10"): it is reassigned at L196 and reassigned back to the constant at L482. The earlier `-10*s` reading was right. Both StyleCutover's `ponytail:` ("subtracts a per-slot team-record x") and Left 1 are retired. No `team+0x3A9C` writer search was needed for this site (GF already carries the slot-point rows as `TeamAIController::slotPoints`; their other readers in this function, L242/276/308/480, are not ported).
- GF: `StyleRunDepth` now adds bit 0 and bit 2 (+5) and subtracts bit 4 (`styleBackDepth_m`, read from `0x142588210`), three separate statements, each behind the compat gate.

## 2. Roles 5-6 gate: `local_1d6 = team+0x7B84` (CONFIRMED)

- The kinds-5-6 arm is `0x1405D9EFF CMP byte [RSP+0x42],1; JA` (bytes `80 7C 24 42 01 0F 87`, read from the image): the term applies while `local_1d6 <= 1`. `local_1d6` is `0x1405D8C69 MOVZX EAX,byte [R13+0x7B84]`. In `FUN_1405D8B70` the "team" (`param_3`) is **P = T+0xC48**, not T: `FUN_140A63680` reads the same byte as `T+0x87CC` (decompile `lVar4 + 0x87cc`, `lVar4 = FUN_140A48A30`), and `P+0x3FB8` is the B array (`pesroles.hpp`).
- Exact-boundary scans (`.pdata` function starts + capstone, scripts in scratch) found no direct-displacement writer of `P+0x7B84`: the three hits are readers (`0x1405D8C69`, `0x1405DCAF8`, `0x140A6373B`). The byte is row-5 byte `+0x10` of the six band rows (`P+0x7B10`, 0x14 bytes each; the constructor `FUN_140A40290` zeroes them as `u16` at row `+0x10`).
- **Writer: `FUN_1405F9CD0`** (called from `FUN_1405EC600` at `0x1405ECAF7` region; decompile l.71-81): `*(P + 0x7B20 + line*0x14 + phase*0x3C) = men`, `*(P + 0x7B21 + ...) = active`, where `phase = ctx+0x10C` and `line` = the class's reclassify line (3-6 -> 1, 7-9 -> 2, else 0), over the 11 B classes (`ctx+0x128` = `local_330` = `&local_138` = copy of `P+0x3FB8`, CONFIRMED by stack layout: `0x458 - 0x330 = 0x128`); `men` counts classes != 0, `active` those with `MI+0x1748[side*11+k] != 0`. `P+0x7B84` = phase 1, line 2, first byte. The out-of-possession build also runs `FUN_140682320` (via `FUN_140681FF0`, only phase 0) which rewrites phase 0's pair with active-only counts: not carried (no reader in GF).
- `ctx+0x10C` is the shape phase being built, not "in possession" (`pes-re-defensive-shape.md` §14.0): `FUN_1404F9DC0` calls `FUN_14051D9E0` with `r8b = latch` on odd frames (`0x1404F9F0D TEST byte [MI+0x1198],1`) and `!latch` on even frames; so each phase refreshes every second frame.
- LIVE read (PES pid 3741448, 01-11-26, read-only `dd` of `/proc/pid/mem`; `T = *(*(0x1436F3DC8) + (side+0xA1)*0x28)`): at `T+0xC48+0x7B10`, team 0 rows `(men,active)` per line, phase 0: (4,4) (4,4) (2,2); phase 1 identical; team 1: (3,3) (5,5) (2,2) in both phases; `T+0x255` latch 1 / 0. So `T+0x87CC` = 2 on both teams (4-4-2 and 3-5-2 shapes). Stable over 3 samples 2 s apart.
- GF: `pes::roles::WriteBandRows/BandRowMen/BandRowActive/BandRowOffset/BuiltPhase` (`pesroles.*`); the bytes live in `teamRecord.reserved_before_sheet` at the exact T offsets (`BandRowOffset(1,2) == 0x87CC`, pinned). `TeamAIController::WriteRoleArrays(head)` writes the phase `BuiltPhase(GetSimFrame(), possession)` right after the head role write (the shape build follows FUN_14051D9E0's head write), `ForwardLineMen()` reads phase 1 line 2, `StyleRunDepth` takes it as `forwardsInLine`; roles 5-6 need `forwardsInLine <= styleFlankMaxForwards` (the immediate, read from `0x1405D9EFF` with its opcode checked).
- ponytails (named at the code): the build runs every GF tick; PES builds only in some `MI+0x116C` states (state 3 every fifth frame `0x14051DA80`, `MI+0x1775` arms) and its third call site (`0x1404FA6DB`, `FUN_1404F9DC0`'s `R12B == 0`) builds the latch's phase every frame (no GF carrier for that byte).

## 3. The enum consumers

| GF knob | PES | Verdict |
|---|---|---|
| `GetWidthOffset` | style bits read in `FUN_1405E6E80` L527-582: they edit the lateral BAND edges `fVar42`/`fVar47` (anchor `fVar48 = param_5[slot*12]`): bit 7 `fVar47 = fVar42 + 1.0`, bit 6 `+15`, bits 5/6 `fVar47 *= 0.5`, bit 18 `+15` (constants `0x14256AE1C`, `0x14259BE0C`, `0x142565600`). GF's knob shifted a formation POINT by +4/+5/-4/-5/-3/-2 m per enum value: no PES counterpart (different quantity, different numbers). | DELETED (header, source, call site). The real PES terms need `FUN_1405E6E80`'s band pipeline (1618 decompile lines, `FUN_1405DB800` caller): OPEN, Left 1. |
| `GetPoacherTargetX` | no PES site places the Poacher at the offside line (style-effects §6). The real Poacher effects are `FUN_140595AB0` run threshold 30 -> 15, `FUN_140662A10` floor 25, `FUN_1406087A0` (-12), all unported. | DELETED with its call site and `poacherOffsideCushion`. |
| `GetDribbleDrive/OpponentRepel/CenterPull/Velocity` | no style bit in any dribble code (228-site census). | Style terms DELETED (signatures now take the COM mask only). COM terms kept, `ponytail:`-marked: no PES dribble site for the `+0x4C` COM flag word was searched (Left 2). |
| keeper rush (`elizacontroller.cpp`), goalie off-ball shuffle (`goalie_default.cpp`) | bits 19/20 read directly (`FUN_1409DD470` `>>0x14`/`>>0x13`) | `Match::StyleHas` (word bit test, no compat row), not the enum. |
| Destroyer (`elizacontroller.cpp` x2), Cross start | bit 12 `>>0xC&1` (`FUN_14078E5A0`, `FUN_140971BA0`); bit 7 `shr 7` (`0x14211AC8D`) | `Match::StyleHas`. |

`Match::StyleHas` = `StyleWordOf(data) & StyleWord(style)`: for a save-row player it is PES's row word; for a Player.bin-only player it falls back to the enum's word (the existing `ponytail:` in `StyleWordOf`), so behaviour for players with no row is unchanged.

## 4. Why the enum is not deleted

`PlayingStyles::Player` still has users that are DATA, not behaviour: `PlayerData` parses/serialises the `playing_style` tag and infers a style for rosters without one (`ParsePlayer`, `Serialize`, `InferPlayer`, `SuitsRole`), `pes::style::EngineBit/StyleWord` map it to bit numbers, `Match::StyleWordOf` falls back to it for a player without a save row, `StyleHas` names bits by it, and three test files use it. Deleting it means `PlayerData` carrying a style word and the inference/parse returning words: a separate refactor (Left 3). No behaviour reads the enum any more except through the word.

## 5. Tests (run directly; `PES21_ROOT` set; no sim slot, no gate)

- `pes_roles_test` **11/11** (6 new `PesBandRows.*`: offset `0x87CC`, line counts and keeper excluded, class past 9 takes the back line, inactive man counts as man not active, a build writes only its own phase, phase alternation with the frame parity).
- `pes_offball_test` **81/81** (new: install pins `styleBackDepth_m == 10`, `styleFlankMaxForwards == 1`; bit 4 subtracts, adds to bit 0 in one word, needs the compat row, not taken by carrier / runner; roles 5-6 take the terms only with few forwards, roles 7-9 never look at the count; `RolesFiveAndSixWaitForTheUndecodedTeamByte` replaced).
- `pes_playerdb_wiring_test` **12/12** (new controller-level: `CreativePlaymakerWordPullsTheForwardBackWhereHisRowListsIt` (expectation from the install's compat row), `TheControllersForwardLineCountIsTheClassesOfItsSlots` (after two ticks the controller's count equals a recount of the slots' B classes, both loop paths)).
- `pes_style_gate_test` **5/5**.
- Mutations (all applied in ONE build, run, reverted by `git checkout`; each is red in its own tests): M1 bit-4 `-=` -> `+=`: RED `CreativePlaymakerPullsTheRunTargetBack...`, `PlaymakerTermAddsToThePoacherTerm...`, `PlaymakerTermNeedsTheCompatRow...`; M2 roles 5-6 `<=` -> `<`: RED `RolesFiveAndSixTakeTheTermsOnlyWithFewForwards`; M3 `BuiltPhase` `==` -> `!=`: RED `TheBuiltPhaseAlternatesWithTheFrameParity`; M4 keeper class 0 counted (`== 0` guard -> `< 0`): RED `EachLineCountsItsClasses...`, `ADeparturePastTheClasses...`. Not mutated: the `forwardsInLine` argument wiring at the site (pinned by the controller-level count test, not by a spot test: the fixture side has 2 forwards so the gate is closed for roles 5-6 there).
- Red-first: the new function-level tests do not compile on `origin/main` (`styleBackDepth_m`, band-row API absent); the old `RolesFiveAndSixWait...` pinned `0` where the port gives `5` with few forwards.
- NOT DONE: controller-level tests for the carrier and run-action exclusions. Blocker: the carrier is `Match::GetDesignatedPossessionPlayer()`, recomputed inside `Match::Process` from `ballRetainer`, which `Humanoid::Process` clears (`humanoid.cpp:1437`); a first attempt (`SetBallRetainer(forward)` before `Process`) left a different designated man (test removed, not committed). A run state needs the Chance select pass to pick a specific man. Both need a seam that sets the designated man / a run slot (or a scripted ball touch). The function-level pins (`CarrierGetsNoRunTerm`, `ARunningPlayerGetsNoRunTerm`, and the Playmaker twins) stand.
- Gate: `GATE fast: NOT RUN (Main gates the batch)`.

## 6. Smokes (2 min, seed 7, run directly; sim slots not taken)

| Path | StyleCutover (`e5002c16`) | this branch `1be2ba8c` | ablation A (bit 4 + roles 5-6 arm off, deletions kept) |
|---|---|---|---|
| switch-off | `4a5f9921d7ce62ef` | `3b73c968b4a377e0` (rc 0; 26 stoppages, 30186 ticks, idle 21 / stalls 7) | `3b73c968b4a377e0` |
| pes-loop | `f4b283a59fce8403` | `0e480b158cf80bd4` (rc 0; 21 stoppages, 43824 ticks, idle 3 / stalls 3) | `0e480b158cf80bd4` |

Both hashes moved, and ablation A equals the final hash on both paths: the new PES terms (bit 4, roles 5-6) moved nothing in this fixture (no fixture player carries the Playmaker word, and the roles 5-6 arm needs a Poacher/Fox/Playmaker word on a role-5/6 man with at most one forward). The whole move comes from the deletions (width tuck, poacher spot, dribble style terms; the keeper/Destroyer/cross word cutover is value-neutral for players without a row). The deletions were not separated one by one (one ablation only). Tripwire status (crash, hang, determinism, zero passes/shots): no crash or hang, rc 0 both; same-hash-on-rerun not rerun; pass/shot counts not extracted. Not aimed at any `[balance*]` line.

## 7. Left, numbered

1. `FUN_1405E6E80` L420-600 (the style-dependent lateral band edges: bits 5, 6, 7, 18 and the depth-window terms of bits 3, 8-11, 13, 16, 17): the real consumer of what `GetWidthOffset` approximated; needs the band pipeline GF lacks.
2. COM flag word (`+0x4C`): whether PES reads it in dribble code; the COM terms of the four dribble knobs stay as `ponytail:` stand-ins.
3. Delete `PlayingStyles::Player`: `PlayerData` carries a style word; `ParsePlayer`/`InferPlayer`/`Serialize`/`SuitsRole`, `EngineBit`/`StyleWord(enum)` and the `StyleWordOf` fallback go with the Player.bin row loader.
4. Controller-level carrier and run-action tests (seam named in section 5).
5. Unported PES style consumers listed in `pes-re-style-effects.md` §6: Poacher run threshold (`FUN_140595AB0`), Target Man candidate score (`FUN_1405FC500`), Extra Frontman (`FUN_1405FC860`), full-back blend (`FUN_1405DF5A0`), set-piece clamps (`FUN_140680980`).
6. Phase 0 row bytes (`FUN_140682320`) and the build's state gating (section 2 ponytails).
7. Same-hash-on-rerun and pass/shot counts for the two smokes.
