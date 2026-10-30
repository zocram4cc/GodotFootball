# Dribble proposal 3: SwitchTables bake replaces the hand-bound jump tables, the BP+0x3328 red was the oracle manifest (slot 21 filed under slot 20), the proposal chain runs on GF's window but its heading differs from PES; controller NOT wired, walk 22+6 oracle NOT done (DribbleProposal3, 30-10-26)

Branch `DribbleProposal` (worktree `gf-DribbleProposal`, scratch `gf-DribbleProposal-scratch`, PES-derived, never committed). Image `pes_full.bin`. CONFIRMED = read in tool output this session; INF = inference; OPEN = not settled. Continues `pes-port-DribbleProposal2.md`.

## 0. Verdict

| Step | State |
|---|---|
| 1 merge `origin/SwitchTables` (+ `origin/main` docs), regenerate, delete the hand bindings, dribble oracle, push | **DONE, CONFIRMED** (section 1). Pushed `53c5af3a` |
| 2 BP+0x3328 one-byte red (GF 0x11, PES 0x15) on seq4891, seq6161 | **DONE, CONFIRMED** (section 2). Dribble oracle `TurnReferenceOracle` green on all captured states (9 comparable, seq033 writes nothing). Pushed `ca330252` |
| 3 wire `RunCarrierConstruct`, `RunTurnReference`, `0x14211ED10`, `0x1421102D0` at the controller in PES's order; delete the BP+0x3380 stand-in; BP+0x4204 from the translated writer | **PARTLY** (section 3): `PesWorld::RunDribbleProposal` runs the whole chain on GF's window with translated code (8 more closure members were needed); a new oracle test is RED on the writer's heading float on six of seven states; the controller and `RunCarrierUpdate` are therefore left as main (stand-in kept, `ponytail:` text updated) |
| 4 walk oracle 22 + 6, red-first stand-down test, mutations, 2-min smoke determinism | **NOT DONE** (section 4) |

## 1. SwitchTables bake covers the three hand-bound tables (CONFIRMED)

- `git merge origin/SwitchTables` (conflict-free after `rm extern/godot-cpp; mkdir; merge; rmdir; ln -s`), then `origin/main` (docs only).
- `gen_translated.py --check` after the merge: `3 drift(s)`: `closure/pes_fn_14063D3C0.cpp`, `pes_fn_14211E2D0.cpp`, `pes_fn_14211E650.cpp`. The SwitchTables layer had not been regenerated for the dribble closure. `gen_translated.py` (no `--check`) rewrote exactly those three: each now reads its table from a baked window, `ro_rd(k_t14211E5A8_28, 40, idx*4, 4)` (and `0x14211E5D0`, `0x14211E5F8`, `0x14211E620`, 40 bytes each), `k_t14211EC50_20` (32 bytes), `k_t14063D884_1C` (28 bytes). Sizes equal the hand windows (`4 x 10 u32`, `8 u32`, `7 u32`).
- Deleted `kManagerJumpTablesVa/Bytes`, `kTurnAngleJumpTableVa/Bytes`, `kStateJumpTableVa/Bytes` and their three `Read` calls from `pesimagetables.{hpp,cpp}`: one mechanism (the translator's bake), not two.
- Oracle after the deletion: unchanged (8 of 9, then the two states of section 2). Pushed as `53c5af3a`.

## 2. The BP+0x3328 red: GF's input, not GF's code (CONFIRMED)

- `0x14211CBD0` (the manager's kind-6 step; decompile this session) scores the candidates 0x10..0x15 (`uVar13 = side*11 + i`) into a list `RSP+0x50` and sorts it (`0x14063EA30` -> `0x141E53540`); the head id goes to BP+0x3328. The PES replay's insert stores (`0x14063E9DD`) give (id, score): 0x10 0.3552, 0x11 1.3241, 0x12 1.1115, 0x13 0.8828, 0x14 0.3414, **0x15 1.5391**; the window's: same, except **0x15 = 1.0** (the base term `_DAT_14256AE1C` alone). So the window's 0x15 had none of the stat bonuses: they come from the player record of slot 21 (`0x140A489F0(G, 21)` -> `+0x70`, bytes `+0x31`, `+0x2c`, `+0x27` = record bytes `0xA1`, `0x9C`, `0x97`).
- Cause: `danger_oracle.py` (builder mode) classifies each live-in byte into the first record range that contains it. Slot records are typed `0x2E50` long but slot 20 and slot 21 sit `0x2B90` apart (`0x30FD0018`, `0x30FD2BA8` in seq4891, read from the capture), so slot 21's `+0x9C` was filed as `slot 20 +0x2C2C` and the window's slot 21 never got its six bytes. The manifest of seq4891 listed `slot 16..20 0x9C 6` and `slot 20 0x2C2C 6`.
- Fix (`tools/dev/translate/danger_oracle.py`, `which()`): among the ranges of the first match's kind, the nearest base below the byte. Manifests regenerated for the ten states. `TurnReferenceOracle.EveryCapturedEntryLeavesPesManagerBytesAndTurnReference`: **green** (it was RED on seq4891 and seq6161, BP+0x3328 GF 0x11 / PES 0x15). Mutation (run before the fix, the red itself): old `which()` -> red. `trap_oracle.py` has the same `which()`; not touched (its oracles are not in this ticket): OPEN 5.
- Production note (OPEN 3): GF's production window writes only the carrier's `+0xA1` (`SetCarrierProfile`), so the candidate scores of the manager's kind-6 step read zero stats for every other slot there. Not decoded to GF state.

## 3. The proposal chain on GF's window (CONFIRMED run; heading RED)

- `PesWorld::RunDribbleProposal(side, slot)`, in `0x142110D30`'s order: `RunTurnReference` (`0x14211D710`); `0x14211ED10` builds the 72-byte step object from the step input S (S at the top of the window's stack: `+4` = slot, `+0x30` = BP+0x4258, `+0x38` = ctx; CONFIRMED layout from seq16184 in DribbleProposal2); the two flag-class gates (`0x140A6E320` of the anime class byte and of slot record `+0x14`: both false on all captured slots; the flagged carrier's `0x14210DD60` is untranslated, `ponytail:` named); `BP+0x41E4 = 0x19` and `0x142119D20(BP+0x41E8)`; while the state's word 0 is 0: kind 0x10, or 0x13 when `0x140A638C0(ctx, slot)` holds, `0x1421102D0(BP, kind, step object, BP+0x4204)`, the copy `BP+0x41E8 <- BP+0x4204` (0x1C bytes) and `BP+0x41E4 = 0x10`.
- First run on a window whose kind objects were never built: record all zero (the kind object's `+0x38` flag read 0; PES's is 1). `Build(..., construct = true)` (new parameter of `pes_oracle_world.hpp`) runs `RunCarrierConstruct` before the capture's bytes are overlaid. Then the first abort: `PES_DISPATCH_UNTRANSLATED: 0x142119e60` (a record filler, `type 3, id 6, target = *p2`, called from the kind step `0x14212FE70` in a branch no replay entered). Static closure of `0x14212FE70` (`closure.py --dry-run`): 8 direct callees not in the layer: `0x14063E830, 0x14063E9F0, 0x142119D90, 0x142119E00, 0x142119E60, 0x142119E90, 0x142119EB0, 0x142130EE0`. Added as closure members (`closure.py ... --tag dribble` for those 8 roots; its output set file REPLACES the old one, so the 15 new lines (8 roots + 7 callees) were merged into the committed `closure-dribble.txt` by hand: 248 -> 263 lines). `gen_translated.py` wrote 4456 functions (3697 recorded, 759 closure; +8 closure). These 8 are translated but NOT proven by a replay (coverage item).
- Oracle: `walkref.tsv` per oracle dir = PES's bytes at the carrier walk's entry of the same unit step (`synth_walk.py`, default entry, no `--patch-clear`: BP+0x3380, BP+0x41E4, BP+0x41E8..+0x4204, BP+0x4204..+0x4220, made by `tools/walkref.py` in scratch). `synth_walk` reached the walk from 7 of the 10 builder captures (seq5314, seq5769, seq6161 do not: the known unmodelled imports). New test `TurnReferenceOracle.TheProposalChainLeavesPesTurnReferenceAndProposalRecord` (capture bytes and production window): **RED, not weakened**. seq033 (type 0, id 0x19: the none record) equals PES. The six others: BP+0x3380 equals PES, the record's type word, id and the second word equal PES, **the heading float BP+0x4210 differs** (seq4891: GF `0x43910000` = 290.0, PES `0x43660000` = 230.0; seq4444 the same; seq4046, seq495, seq16184, seq16456 differ in the low bytes of the same float and, on 16184, of `BP+0x420C`).
- INF cause: an input of the kind-0x10 step `0x14212FE70` that GF's fresh window lacks (the kind objects are freshly constructed, PES's evolved over earlier frames; the step's output record BP+0x4258, `1,0,0,1,7,0x162E6` in the capture, is zero in the window). Not verified; next step in section 5.
- Controller: I wired `RunDribbleProposal` in place of the BP+0x3380 stand-in and made `RunCarrierUpdate` construct and reset with `0x14210E5F0`. The 2-minute smoke (`--seed 7 --minutes 2`, switch-off path, `simslot.sh timeout 1500`) did NOT finish on that tree (log empty). **Reverted both edits** (production is main's again; the stand-in comment names this report) and re-ran the same smoke on the reverted tree: it did NOT finish in 1200 s either, so the non-finish is not caused by the wiring (CONFIRMED by the second run; cause not diagnosed: machine load ~35, or the SwitchTables layer on this machine; main's baselines `e663d1e917d6c4f4` / `980c54e27f93ced2` were NOT reproduced here). No smoke hash from me: item 4 of the ticket is open. `RunDribbleProposal` and `RunCarrierConstruct` stay as test-only API.

## 4. Not done

- 22 walk dirs + 6 stand-down states, the red-first stand-down test (PES answers 6, 0x19 x4, 0xB), mutations, both 2-min smoke hashes vs main (`e663d1e917d6c4f4` switch-off, `980c54e27f93ced2` pes-loop). The walk states of the earlier reports are gone from scratch: regeneration is `synth_walk.py` (12 `pid483669` carrier_clear captures, 30-60 s each) + `frame_trace.py recorded` + `snapdiff.py --only 0x14210F110` + `danger_oracle.py ... walk`. 7 builder-source walk states now exist (`syn/walk`).
- Whether the writers re-run on a post-writer state give the same outputs (the stand-down red-first design): not measured.

## 5. What is left, numbered

1. The heading BP+0x4210 (section 3): trace the kind step `0x14212FE70`'s first reads on the PES replay (`readorder.py` on `syn/p2d0/...`, made by `synth.sh pid1981120/seq4891 0x1421102D0 danger_builder_2119390 p2d0`: 17508 first reads) against the window's, as DribbleProposal2 did for the manager; the first suspects are the kind object's state and BP+0x4258.
2. Then wire `RunDribbleProposal` at the controller and restore the full block reset in `RunCarrierUpdate`; run both smokes on a quieter machine (this tree's smoke did not finish in 20 min even with the production path reverted to main's).
3. Production slot stats (`0xA1`, `0x9C`, `0x97` of every slot record) for the kind-6 candidate scores (section 2).
4. Walk oracle 22 + 6 and the stand-down test (section 4).
5. `trap_oracle.py which()` has the slot-overlap defect too.
6. Coverage: the 8 new closure members and the kinds other than 0x10/0x13.

## 6. Checks run

- `gen_translated.py --check` before the regeneration (3 drifts, section 1); `gen_translated.py` after both regenerations ("wrote 4448", then "wrote 4456"); `--check` after the last regeneration: see the final message.
- `pes_dribble_oracle_test` (`GF_PES_DRIBBLE_ORACLE` = the ten regenerated dirs, `PES21_ROOT` set): `EveryCapturedEntry...` green; `CarrierConstruct` green; `TheProposalChain...` RED (section 3).
- `GATE fast: NOT RUN` (interim: production behaviour is main's; layer +8 closure members, `pesimagetables` three bindings deleted, tooling and tests).
