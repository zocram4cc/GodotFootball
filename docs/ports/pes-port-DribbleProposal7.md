# Dribble proposal 7: slot ability bases from the EDIT save's own record layout (not Player.bin's), the 12 chain members proven (synthetic, labelled), oracle dirs green (DribbleProposal7, 31-10-26)

Branch `DribbleProposal` (worktree `gf-DribbleProposal`, scratch `gf-DribbleProposal-scratch`, PES-derived, never committed). Image `pes_full.bin`. CONFIRMED = read or run in tool output this session; INF = inference; OPEN = not settled. Continues `pes-port-DribbleProposal6.md`.

## 0. Verdict

| Step | State |
|---|---|
| 1 `git merge origin/main`, push | DONE (`a7669bc7`). |
| 2 ability source | **DONE.** The source is the EDIT save's player record, in a layout that is NOT Player.bin's. `PlayerDatabase::ApplyEditSave` patches Speed, Jump, Height and the foot per id; equal to PES's roster row on all 34 captured players. |
| 3 row -> block copy, `+0xA4` | `+0xA4` = the foot: CONFIRMED against the source bytes (save bit 381, 34/34). The copy writer 0x00..0x4F: not traced; a fitted save -> row byte map is in section 2. |
| 4 synthetic call-state tool, 12 members | **DONE.** `synth_call.py` + `synth_members.py`; 12 of 12 proven bit-exact (11 synthetic, `0x14063D2F0` on 12 real calls). `results_dribble.json` records carry `"synthetic": true`. |
| 5 two stale oracle dirs | DONE: regenerated, `pes_dribble_oracle_test` 5/5. |
| 6 fast gate | Section 6. |

## 1. Ability source (CONFIRMED)

- DribbleProposal6's guess was right about WHERE (the save wins over the install's placeholder Player.bin, `pes-re-condition-import.md` 1) and wrong about the LAYOUT ("same 312 B layout" in that doc, corrected there): the save's 0x138-byte record has its id at +0 (Player.bin: +8), plain 7-bit abilities (Player.bin: 6 bits +40), and so on. Reading it with Player.bin's bit offsets gave 52/67/109 for 72101 (a throwaway dump), PES's row has 77/77/199.
- Method: `roster_rows.py` (new, `tools/dev/rendezvous/`) reads PES's roster row (G=[0x1436F3DC8], team roster `G+(0xA3+t)*0x28`, row `roster+0xA8+e*0x128+0x38`) from every full capture: 34 distinct players (72101-72113, 74902-74921, 71801-71823 subset), then a bit-field fit of every row byte against the player's save record (`fit4`, scratch).
- Fitted, 34/34 players each: Height = save byte +0x0A (also `pes-re-physique.md` 1.1) = row 0x31; foot = save bit 381 = row 0x34 (the two values both occur); abilities = 25 plain 7-bit fields at bits 112 119 | 128 135 142 149 | 160 167 174 181 | 192 199 206 213 | 224 231 238 245 | 256 | 288 295 302 | 320 | 358 365 = row bytes 0x14..0x25 (18) and 0x27..0x2D (7). The 18|7 split falls exactly at 245|256, so the fields run in bit order (INF). Rows 0x12/0x13 = save bytes +0x0C/+0x0D.
- **Flat abilities (CONFIRMED):** all 25 ability fields are equal in every one of the save's 4830 players (`flat`, scratch) and in every captured row. So Speed (row 0x27 <- bit 256) and Jump (row 0x2C <- bit 358) cannot be told apart from any other field by data: their identity inside the 25 is INF; the value is right for every player of this save whichever field is read. A save with a non-flat player would expose a wrong order. Open: Speed/Jump field identity (needs a non-flat player's row, or the converter code: the save -> live 0x17C record loader was not located; the 0x138-stride walkers at 0x141404A30 and 0x141406500 are the editor's array ops, not it).
- Production (clean cutover, no Player.bin fallback for a player the save holds): `EditSave.players` (raw records, `editsave.hpp` constants), `PlayerDatabase::ApplyEditSave` (id at +0; ids with no Player.bin record add nothing; injury tier and Form stay Player.bin's, which is WRONG for edit players, noted in the doc: their save positions are not decoded), `PesLibrary::load_edit_save` calls it, `WriteSlotAbilities` unchanged except exported (`elizacontroller.hpp`) so a test reaches it.
- Not wired: `gfsim_run` has no `--edit-save` flag (the gate's smokes use the install's Player.bin as before). Add it when a sim test needs the save.

### Fitted save -> row byte map beyond the abilities (INF low: 2-4 bit fields on 34 players)

0x00 <- bit 269 (4-5 bits); 0x01 <- bit 333 or 421 (3 bits); 0x02..0x09 <- bits 335, 337 ... 349 (2 bits each, every second bit); 0x0C <- 354; 0x0D <- 356; 0x0F <- 315; 0x11 <- 220; 0x26 and 0x35 <- bit 126 or 318 (2 bits); 0x2E <- 252 (3); 0x2F <- 327 (up to 7); 0x30 <- 263 (6); 0x37 <- 284 (3); 0x38 <- 309; 0x39 <- 156 or 188; 0x3A <- 312; 0x3B <- 188; 0x45 <- 410 (1). Constant on all 34: 0x0E, 0x10, 0x32 (0x50), 0x33, 0x36, 0x3C, 0x46, 0x47, 0x4B, 0x4D-0x4F. No fit (rebuilt or derived): 0x3D-0x44, 0x48-0x4A, 0x4C. Not used by GF.

## 2. Tests (CONFIRMED run)

- `pes_playerdb_test` 14/14 (`PES21_ROOT`, `PES21_EDIT_SAVE`, `GF_PES_ROSTER_ROWS` = the 34-player file `roster_rows.py` wrote): `PlayerDatabaseInstall.EveryCapturedRosterRowEqualsTheDatabaseWithTheSaveApplied` (RED before `ApplyEditSave` existed: height 180 vs 189 etc. on origin/main's path; green: 34 players), `PlayerDatabaseEditSave.TheSavesRecordOverridesSpeedJumpHeightAndFootOfItsIdOnly` (neighbour bits and bytes set, Speed 77 / Jump 88 distinct), `...ASaveIdWithoutAPlayerBinRecordAddsNothing`, `...ARightFootedRecordClearsAPlaceholderLeftFoot`.
- `pes_editsave_test` 8/8 (run with `PES21_EDIT_SAVE`): the synthetic save's `players` come through whole and in order (not mutated).
- `pes_playerdb_wiring_test` 5/5: new `SlotAbilityBasesComeFromThePesRecordOfAnImportedPlayerAndGfStatsOtherwise` (the PES-id vs GF-stat branch through the exported `WriteSlotAbilities`: Height and foot exact, Speed/Jump within the store step's cells, synthetic player takes GF's height/stat/right foot).
- Mutations, each built, run, reverted: (a) `kEditHeightOffset` 0x0A -> 0x0B, `kEditStrongerFootBit` 381 -> 380, `kEditJumpBit` 358 -> 359 together: the install test, the override test and the right-foot test RED (3 of the 4 new tests; jump is caught by the override test since the install rows are flat); (b) `ApplyEditSave` `find` -> `try_emplace`: `ASaveIdWithoutAPlayerBinRecordAddsNothing` RED; (c) `WriteSlotAbilities` `bin = nullopt`: the wiring test RED on height (180 vs 197), Speed and Jump. Not mutated: the speed-bit constant (the same value as the other 24 fields on flat data; the override test pins it by an off-by-one neighbour check only).

## 3. The synthetic call-state tool (CONFIRMED run)

- `tools/dev/translate/synth_call.py STATE OUT --entry VA --reg rcx=.. --reg edx=.. --poke ADDR=HEX`: keeps a state's memory (link, or a sparse copy when bytes are poked) and its stack, writes `call.bin` with the chosen registers and `index.json` with `rv.entry_va` = the member. `frame_trace.py recorded` and `snapdiff.py --bounded` replay it like a capture.
- `tools/dev/translate/synth_members.py STATE WORK [--merge] [--real VA=CAPTURE]`: the case table, the trace/snapdiff runs, the record merge (`results_dribble.json`: `synthetic`, `synthetic_cases`, note starts `SYNTHETIC:`; a `--real` record says REAL). State: `synth_walk.py --entry 0x14212FE70 --hook-name danger_builder_2119390` from `live2/pid1981120/seq4891`. Scratch is small (23 MB of cases and traces, the sparse state).
- Result (one run, 24 `proven=True` lines = 12 members x 2 prints; `--merge` run identical): all calls replayed, 0 mismatches, 0 excluded:

| Member | Cases | Own-block coverage | Kind |
|---|---|---|---|
| `0x14063E830` (slot bit test) | 7 (slots 0, 3, 10, 11, 14, 21, 22) | 1.0 | synthetic |
| `0x14063E9F0` (candidate list sort) | 4 lists (empty, 1, 5, sorted 3), bytes poked | 1.0 | synthetic |
| `0x142119D90`, `0x142119E90` (record fillers) | 2 each (dst 0xFF / 0) | 1.0 | synthetic |
| `0x142119E00` | 2 sources | 1.0 | synthetic |
| `0x142119E60`, `0x142119EB0` | 2 sources each | 1.0 | synthetic |
| `0x14211CA50` (sign test) | 4 words (+1.0, -1.0, 0, -0.0) | 1.0 | synthetic |
| `0x14211CB00` (kind switch) | 14 (0..9, 11, and 10 with +0xA0 = 4, 1, 10) | 1.0 | synthetic |
| `0x140A57450` | 5 (slots 0 and 5, n = 0, 1, 3, 249, 250) | 1.0 | synthetic |
| `0x142130EE0` (the big kind-step branch) | 1 (the kind step's own argument objects) | **0.2252** | synthetic |
| `0x14063D2F0` | 12 | 1.0 | **REAL** (`live2/pid407792/seq021`, the corridor-angle capture) |

- `0x142130EE0` is proven on its one case, 22.5% of its blocks: a faithful claim for what that case enters, not for the other branches. More cases (poke `kind+0x38 -> +0xA0/+0xC4` to reach the gated arms) are the next step.
- The tool cannot say a branch is semantically reachable in play: the registers are chosen. That is why every record is labelled. No translated body was mutated to prove the comparison bites (the comparison is `snapdiff`'s, proven on the roots); say so.
- `gen_translated.py --image pes_full.bin`: wrote 4456 functions (3705 recorded bit-exact, 751 closure); `--check` after: byte-identical. Eight members moved from `closure/` to `dribble/`; no function body changed.

## 4. Oracle dirs (CONFIRMED run)

- The two stale dirs were `oracle_d710/pid1981120_seq4891` and `..._seq6161`: their `mem.bin` link pointed at a synthesized state that DribbleProposal6's own `synth_walk` runs rewrote (mtime 31-10 01:12) while the manifest was older; `danger_oracle.py` on the current states failed "stack at rsp not in the snapshot".
- Regenerated: `synth_walk.py --entry 0x14211D710 --hook-name danger_builder_2119390` from each capture (`syn/d710r`), `frame_trace recorded`, `snapdiff --only 0x14211D710 --bounded` (1/1 bit-exact, own-block 0.83 / 0.75), `danger_oracle.py` -> `oracle_d710r/`; seq4891's `walkref.tsv` regenerated (`synth_walk` default entry + `walkref.py`). With the other eight dirs from `oracle2_d710`: `pes_dribble_oracle_test` **5/5** (`TurnReferenceOracle.EveryCapturedEntry...`, `...TheProposalChain...`, `CandidateScores...`, `AbilityStore`, `CarrierConstruct`). Scratch holds the sparse states; delete `syn/` and `oracle_d710r/` states with the scratch dir.

## 5. Left, numbered

1. Speed/Jump identity among the save's 25 flat ability fields (needs a non-flat player or the converter code).
2. Injury tier and Form for edit players: the save's positions are not decoded; GF still reads Player.bin's placeholder bits for them (`pes-re-condition-import.md` note). A flat 77 team dealing "Normal" injuries is probably right by accident.
3. `0x142130EE0`: more synthetic cases for the gated arms (coverage 0.2252).
4. The row bytes 0x00..0x4F writer into the AI record (section 1 map is a fit, not the code).
5. `gfsim_run --edit-save`.
6. Kind 0x13 and flag-class captures, A4 super-sub cells, breath-penalty ids (DribbleProposal5/6).

## 6. Checks run

- Test binaries: `pes_playerdb_test` 14/14, `pes_editsave_test` 8/8, `pes_playerdb_wiring_test` 5/5, `pes_dribble_oracle_test` 5/5 (`GF_PES_DRIBBLE_ORACLE` = the mixed dir above), `gen_translated.py --check` clean.
- Commits pushed: `25a34956`, `7f0e9e9c` (`git ls-remote origin DribbleProposal` confirms `7f0e9e9c`).
- `GATE fast`: **no verdict (interim).** `GATE_TIER=fast GATE_SLOTS="3 2"`, absolute Release dir `<workdrive>/gf-DribbleProposal/build`, full gate env, tree `7f0e9e9c`, was started and killed by me at build step 3519/4636 (the regenerated translated layer rebuilds ~4600 objects) under load average 42; it could not reach ctest in the time left. Nothing in it ran. Main's batch gate covers it.
