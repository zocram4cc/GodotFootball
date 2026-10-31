# Dribble proposal 5: every slot's ability bytes are written from GF's player data through PES's store step (0x14092AF10), candidate scores equal PES, unit oracle regenerated and green (DribbleProposal5, 31-10-26)

Branch `DribbleProposal` (worktree `gf-DribbleProposal`, scratch `gf-DribbleProposal-scratch`, PES-derived, never committed). Image `pes_full.bin`. CONFIRMED = read in tool output this session; INF = inference; OPEN = not settled. Continues `pes-port-DribbleProposal4.md`.

## 0. Verdict

| Step | State |
|---|---|
| 1 `git merge origin/main`, push | DONE (`8b108568`; main had only the TrapTick4 report and auto_slim). Later pushes: `4f0b71ac`, `e833b4b1`. |
| 2 decode which stat bytes the manager reads and their writer; write them for every slot | DONE (section 1, 2). Red-first test green (section 3). |
| 3 regenerate unit-oracle dirs, run `GF_PES_UNIT_ORACLE`; replay proofs of the DribbleProposal3/4 closure members | Unit oracle DONE: 12/12 regenerated whole dirs green (section 4). The closure-member replay proofs are NOT done (section 6, item 1). |
| 4 kind 0x13 / flag-class | No captured state exercises them; the situations and a `--when` are named (section 5). |
| 5 mutations, smoke | Mutations DONE (section 3). Gate and smoke: section 7. |

## 1. What the manager reads (CONFIRMED)

- `0x14211CBD0` (the carrier manager's kind-6 candidate scorer; decompile lines 91-176 in `cbd0.txt`) loops candidates 0x10..0x15 (`uVar13 = ((uint*)param_2[1])[2]*0xb + iVar12`, `< 0x16`), and for each reads `FUN_14124DAD0(FUN_140A489F0(G, slot))[id]` = `record + 0x70 + id` for id **0x31** (`(f - fVar3)/_DAT_14259be0c`), **0x2C** (`(f - fVar5)/_DAT_1425a9460`) in the case-4..9 arm and **0x27**, **0x31** in the other arm, and `0x140A489F0(G,k) = *(G + (k + 0xF3) * 0x28)` (`k < 0x16`). `0x14124DAD0 = param + 0x70` (decompile). `0x14063E9D0` inserts the (id, score) entries, `0x14063EA30` sorts.
- Byte labels (INF high, `pes-re-matchlogic.md` 1.3, `pes-re-anim-select-2.md` 1): 0x27 Speed, 0x2C aerial (labelled Jump; the Speed/Jump pair cannot be told apart by the modifier tables), 0x31 height in cm.
- Before: production wrote only the carrier's `+0xA1` (= `0x70 + 0x31`) in `SetCarrierProfile`; every other slot read zero (DribbleProposal3 saw `0x15` score 1.0 where PES had 1.5391).

## 2. The writer (CONFIRMED on 5 whole captures, 2530 slot/id rows)

- The record's `+0x70` block is written by the stat rebuild **`0x14092AF10(P, Q)`** with `Q` = the AI record = `0x140A489F0(G, slot)` (decompile lines 429-511: `FUN_14124DAD0(Q)` is the block). Its store step: product of the player object's five float cells `P+0xC888 (A2) * +0xC798 (A1) * +0xC978 (A3) * +0xCA68 (A4) * +0xCB58 (A5)` at `4*id` (`0x14092B849-0x14092B86D`, id < 0x3C), `MULSS` with `(float)base[id]` (`0x14092B890`), `CVTTSS2SI` (`0x14092B894`), the breath penalty for ids 0x1A..0x1C and 0x2A (`0x14092B8A8`), the compare with the block's current byte (`0x14092B8B7`), the clamp bounds call `0x1408CAF60(&lo, &hi, id)` (`0x14092B8CF`; table `0x142613C60`, two bytes per id) and the store `0x14092B904`: above the upper bound the bound, below the lower the lower, else the byte.
- `base[id]` = the roster row `roster + e*0x128 + 0xA8 + 0x38 + id` (`e = roster[4 + 4*(slot%11)]`, `0x14092AF10` locals `local_1b8 = record + 0x38`), and the block's other bytes start as that row (checked: height `0x31` equals the row in every slot).
- Cells for the ids GF writes, read from the player objects in the captures: A2 = the condition arrow (0.84/0.88 on the speed and aerial ids for arrowed players), A5 = the fatigue factor on `{0x17,0x18,0x1A,0x1B,0x1C,0x28..0x2C}` only (so on the aerial byte 0x2C, not on Speed 0x27; slot 1 of `pid483669/seq11497` carries 0.9 on 0x2C), A1/A3/A4 = 1.0 in these captures.
- Formula check outside GF (script `scr/abil.py` in scratch, python, reading PES's own memory: roster row, P arrays through the 60-float runs, clamp table from the image): 5 captures (`pid483669` seq11497, seq13209; `pid1330921` seq22186 (No Mod launch); `pid2122415` seq19912, seq34219), 22 slots x 23 ids (ids 0x14..0x2D minus the 4 penalty ids, plus 0x31): **0 mismatches in 2530 rows**.
- Player-object finding: `P` objects are not at a fixed stride (slot 0 at `0x336061C0`, slot 1 `0x336207B0` in seq11497); found by the 60-float 1.0 run at `P+0xC798` with `P+0x18 == slot`.

## 3. GF code and tests (CONFIRMED run)

- `PesWorld::StoreLiveAbility(slot, id, base, AbilityCells)` (`pesworldrecord.cpp`): the product in PES's float order, the truncation, the unchanged-byte skip, the clamp through the **translated `0x1408CAF60`** run on the window (baked table window, no image table needed). `pes::AbilityCells` holds the five cells. `SetCarrierProfile` lost its `heightStatCm` parameter and the `+0xA1` write (clean cutover; callers and tests updated).
- `ElizaController`: `WriteSlotAbilities` writes Speed, aerial and height for every active player of both squads before `RunDribbleProposal` (all 22 slots): base = the player's own stat (`PlayerData::GetStat`, not `Player::GetStat`, so GF's difficulty and clutch multipliers stay out) over `kAbilityBytesPerStat = 100` (GF stores PES's 40..99 as 0.40..0.99, the convention of `pesstamina.cpp`), A2 = `FormState::ArrowMultiplierFor(conditionArrow)`, A3 = `FamiliarityMultiplierFor(deployed familiarity)`, A5 = `GetFatigueFactor()` on the aerial id only, height = `PlayerData::GetHeight()*100`.
- `ponytail:` stand-ins named in the code: A4 (super-sub pair table `0x142632F74`, `0x14092B7FC`) is 1.0; the base byte comes from GF's stat where PES reads the roster row (Player.bin ability + 40; the roster row's writer and its copy into the AI record are unlocated, `pes-re-publisher-link.md` 2); the breath penalty of the four ids is not applied (no GF caller reads them).
- Tests (`tests/pes_dribble_oracle_test.cpp`, `tests/pes_walk_oracle_test.cpp`):
  - `TurnReferenceOracle.CandidateScoresEqualPesWhenEverySlotsAbilitiesAreWrittenFromPlayerData` (`GF_PES_DRIBBLE_ORACLE` = the 10 d710 dirs): every slot's three bytes zeroed, then written through `StoreLiveAbility` (GF's player data := the capture's own ability bytes at cells 1.0), `RunTurnReference` on the window; the set of (id, score bits) the insert `0x14063E9D0` stores (read from the write log, PES's from the oracle's `corpus.jsonl` log) equals PES's on every state that runs the kind-6 step. **Red-first:** on `origin/main` the API does not exist; and the same states with only the carrier's height written (the old production) are asserted to differ from PES on at least one state (`carrierOnlyWrong > 0` holds).
  - `AbilityStore.LeavesTheLiveByteOfEveryCapturedSlotAndId` (`GF_PES_ABILITY_ORACLE` = 5 tsv, 2530 rows): equal; some rows have `live != base` (asserted), so the multipliers and clamp are exercised.
  - `AbilityStore.ClampsToTheInstallsBoundsOfTheId`: bounds read from `PES2021.exe` (`0x142613C60`), above, below and unscaled for ids 0x27, 0x2C, 0x31.
  - `CarrierProfile.PesReadersSeeTheCarriersMaskAndEverySlotsHeight` (replaces the carrier-only test): PES's reader `0x14063D8A0(ctx, slot, 0x31)` returns a distinct height for each of the 22 slots.
- Mutations (each run, rebuilt, reverted by `git checkout`): drop A5 from the product -> `AbilityStore.Leaves...` RED; no clamp (`block[id] = value`) -> `AbilityStore.Leaves...` and `ClampsTo...` RED; write the neighbour slot (`Block((slot+1)%22)`) -> all three RED (candidate test included). Not run: truncation vs rounding, the unchanged-byte skip.

## 4. Unit oracle regenerated (CONFIRMED)

- `walkunit.sh` (copy of `walkdir.sh`; whole synthesized states under `syn/whole`, oracle dirs symlink their `mem.bin`) for the 12 `pid483669` dirs (seq11497, 8889, 13209, 14088, 15049, 17213, 18365, 20019, 20021, 20023, 20026, 20029); `unitall.sh` ran `GF_PES_UNIT_ORACLE` on each alone and deleted its 2 GB state afterwards (scratch holds the 12 slim oracle dirs only).
- Result: `UnitOracle.UnitInfoAndStateFromTheWindowEqualPesOnEveryCapture` and `UnitOracle.DangerBuilderOnTheWindowWritesTheWalksDangerByteAndZone` **PASS on all 12** (2/2 each).

## 5. Kind 0x13 and the flag-class return (OPEN: no captured state)

- Kind 0x13: `0x140A638C0(ctx, slot)` (decompile): true when `slot` is the taker `FUN_140A62610(side, MI, side record)` names for its side: the carrier is the restart taker. The unit driver only runs the proposal chain for a carrier, so the state needed is a restart (set piece / kick-off) where the unit step runs for the designated taker. Candidate `--when` for a hook on the carrier unit (INF, never run): on `carrier_clear_2109EB0` (`r8` = the unit object `{unit info VA, ctx VA}`; unit info +0 slot, +8 side): `d(q(q(0x1436F3DC8)+0x1928+d(q(r8)+8)*0x28)+0x7C)==d(q(r8))` (side record = `G+0x1928+side*0x28`, taker at `+0x7C`, `kSideKickerOffset`).
- Flag-class return (`0x14210DD60`, untranslated): the carrier's anime-class byte (`anime record +1`) or the slot record's word `+0x14` indexes the class table `0x1426484F0` with `& 0x30000` (`0x140A6E320`). Script-read from the image: the flagged classes are 0x8-0xA (bit 16) and 0x19, 0x1B, 0x1C, 0x1F-0x21, 0x24-0x27, 0x29-0x2B, 0x32, 0x33, 0x36, 0x38-0x3D, 0x3F-0x42, 0x44 and later (bit 17): a carrier who is mid kick/shot/pass-type action. `0x14210DD60` writes the "Kick Action Decided" state (`BP+0x4179..0x41D2`) from `0x142110840`. A capture would be a carrier-unit call with the carrier's anime class in that list; `--when` needs the anime table offsets (`kAnimeTableOffset`), not drafted.

## 6. Left, numbered

1. Replay proofs (bounded snapdiff on the entry states) for the 8 closure members of DribbleProposal3 and the 4 seeds of DribbleProposal4: NOT run (budget). The functions ran bit-equal inside the chain tests (`TheProposalChain...` 7 states) but have no per-function proof.
2. Kind 0x13 and flag-class captures (section 5); `0x14210DD60` translation.
3. A4 super-sub cells, the roster row's writer, the four breath-penalty ids; also the corridor angle's second slot byte `+0xA4` (= block `+0x34`, `pes_angle_oracle_test` reads it) has no production writer yet.
4. The slot stats' base is GF's stat; the Player.bin ability path (`PlayerDatabase` + the id map Speed 0x1C -> byte 0x27, Jump 0x1F -> 0x2C, Height 0x10 -> 0x31, `pes-re-matchlogic.md` 1.2/1.3) for players with a PES id.
5. 20-minute smoke (Main's batch).

## 7. Checks run

- Tests: `pes_walk_oracle_test` (`CarrierProfile.*` 2/2; `UnitOracle.*` 2/2 on each of 12 regenerated dirs), `pes_dribble_oracle_test` (`AbilityStore.*` 2/2, `*Candidate*` 1/1), `pes_danger_oracle_test` 2/2 (links the changed `pesworldrecord.cpp`), `pes_angle_oracle_test` (builds; 0 tests ran, its oracle env unset).
- `GATE fast`: NOT RUN TO A VERDICT. `GATE_TIER=fast GATE_SLOTS="3 2" tools/dev/gate.sh` sat about 2 hours on the sim-slot flock (slots 3 and 2 held by other branches' gates; only `select_recorded` printed); I killed my own queued gate (pid 2531205 and its flock child). `gen_translated.py --check` not re-run: no translated or generator file changed this session (only `pesworldrecord.*`, `elizacontroller.cpp`, two tests).
- Smoke: not run (needs the sim slots). INF only: the switch-off path does not run the carrier writer, so main's `e663d1e917d6c4f4` is expected; the `--pes-loop` hash `980c54e27f93ced2` may move now that non-carrier slots hold real stats (the manager's scores change). Main's batch smoke decides.
