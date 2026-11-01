# DribbleRow: playing-style masks and the whole roster row decoded, the row -> AI record copy located and ported, 0x142130EE0 coverage (DribbleRow, 01-11-26)

Branch `DribbleProposal` (worktree `gf-DribbleProposal`, scratch `gf-DribbleProposal-scratch/sty`, PES-derived, never committed). Image `pes_full.bin` (VA - 0x140000000). Continues `pes-port-EditSaveFields.md` (Left 1, 2, 5) and `pes-port-DribbleProposal7.md` (Left 3, 4). CONFIRMED = decompile, asm, script read of the image or a test run this session; INF = inference; OPEN = not settled.

## 0. Verdict

| Step | State |
|---|---|
| 1 `git merge origin/main`, push | DONE (`5a83a246`, a fast-forward-free merge; main was `5a83a246`). |
| 2 style masks `+0x48/+0x4C`, row bytes `0x3C..0x3F` | DONE: masks decoded and applied; `0x3C` decoded (always 0 for a save player); `0x3D..0x3F` are builder padding (CONFIRMED by data). Equal on all 34 captured rows, whole 0x50-byte image. |
| 3 the row -> AI record copy | **LOCATED and PORTED**: `FUN_140922BC0` -> `FUN_140A58F00`. GF's block now starts as PES's copy of the save's row for a save player. |
| 4 `0x142130EE0` synthetic cases | PARTLY: 13 cases (was 1), all bit-exact, own-block coverage 0.2252 -> 0.2613. Not the target of covering its other arms (section 4). |
| 5 gate | interim line in the final message. |

## 1. Playing-style masks and row bytes 0x3C..0x3F (CONFIRMED)

The row builder `FUN_141FE9B00` calls `FUN_141FE9F40` (decompile, bridge) which writes three things:

- **Row `+0x48` (u32), the style word** that PES's gate reads (`pesstyle.hpp`: bit = engine bit; AD0's output bit for style 1 is bit 0 = GoalPoacher, 2 = DummyRunner, ... matches `EngineBit`). Chain: save id `0x40` (descriptor `0x1434F2B20` entry: 5 bits at record bit 274, range 0..31, so every value is accepted) -> `FUN_141F10CE0` case `0x40` (`0x141F11114` CALL `0x1414C04A0`, `edx` = the value) -> `FUN_1414C04A0` (`CMP EDX,0x16; JC` : above 0x15 stores 0, else the byte table `0x142979060`) into live field `0x3B` -> `FUN_1414C0300` (dword table `0x142979080`, value above 0x15 -> 0) -> `FUN_141FC0AD0` (jump table `0x1401FC0BE4`-relative at `0x141FC0AE6`, 22 entries; entry 0 is the default arm and sets no bit, entries 1..21 `mov dword [rcx],bit`). Bits `0 1 2 3 4 5 6 7 8 9 10 12 13 11 16 18 17 14 15 19 20` for style 1..21.
- **Row `+0x4C` (u32), seven flags**: save ids `0x41 + k` (1 bit each at record bit 383 + k; `FUN_141F10CE0` default arm `param_4 - 0x41 < 7` -> `FUN_1414C0200(k)`, table `0x1429790D8` = live key 60 + k, stored 1 when the value is non-zero) and the table `0x14352AC10` (7 triples `{mask bit, key}`) turn key 60 + k into mask bit `0 2 1 4 6 3 5` for k = 0..6.
- **Row byte `+0x3C`** = `FUN_141FC0C40(live +0x150)` via `FUN_1408CB290` (`mov byte [rcx+0x3c],dl`). Live `+0x150` is zeroed by `FUN_140AA1700` (`FUN_1415A1720(rec,0,0x17C)`) and written by neither `FUN_141F10CE0` nor the field setter `FUN_1414C6E90` (decompile grep for `0x150`: none). So it is 0 for a save-decoded player, as on all 34 captured rows. OPEN: another writer of live `+0x150` (a Player.bin or team path) was not searched; byte-store scans found hundreds of `+0x150` stores in unrelated structs.
- **Row bytes `+0x3D..0x3F`**: no field of the save and no builder store. Evidence: the same player (e.g. 72101) shows `00000000` in one capture and `00d81845` in another (22 of the 34 players appear with both). INF: padding the row struct picked up from the stack. Not decoded; the test excludes them.

Capture hygiene finding (CONFIRMED): the whole-row file for these tests must come from the SAVE the game loaded, `~/Documents/KONAMI/eFootball PES 2021 SEASON UPDATE/<id>/save/EDIT00000000`. The bottles copy and `~/Downloads/EDIT00000000` are other saves: against them only 18/34 positions and 9/34 skill masks match; against the Documents one 34/34 and 34/34.

Production: `editsave.hpp/.cpp` (`kEditStyleLive`, `kEditLiveStyle`, `kEditStyleMaskBit`, `kEditFlagMaskBit`, `kRosterRowBytes`, `kRowStyleByte/Skills/StyleMask/FlagMask`; `EditRow.styleMask/flagMask/image` in `DecodeEditRow`; `kEditRowBytes` 0x3C -> 0x3D). `PlayerTraits::row` (the whole image, set by `ApplyEditSave`; the six bytes GF reads take Player.bin's value where the save failed its gate, named `ponytail:`). `Match::StyleWordOf` (the row's `+0x48`, else GF's style bit) now feeds the two style-word sites in `teamAIcontroller.cpp` (extra-frontman designation gate, the offball man's `styleWord`). NOT cut over: the run-depth branch at `teamAIcontroller.cpp:869` keys on GF's style enum (`GetDepthOffset(style)` is GF's own); its `StyleWord(style)` argument stays GF's.

## 2. The row -> AI record copy (CONFIRMED; the writer found by a store-pattern scan)

- `FUN_140A58F00(Q, row)` (`0x140A58F00..0x140A58F33`, undefined in Ghidra, one caller): `movups xmm0,[rdx]; movups [rcx+0x70],xmm0` x5 at disp `0x70,0x80,0x90,0xA0,0xB0` from `[rdx+0..0x40]`: the first **0x50 bytes of the row become the record's ability block (`Q+0x70`)**. Its neighbours are the record's setters (`0x140A58F40 mov [rcx+0xC4],dl`, `0x140A58F50 mov [rcx+0xC0],edx`).
- Its only caller `0x140922CE1` is `FUN_140922BC0(P)` (one caller: `0x14041A628` in `FUN_14041A390`): `side = P+0x18 in 11..21`, `k = slot % 11`, `entry = FUN_140A3AF60(FUN_140A489D0(G, side), k)` = `team + 0xA8 + roster[k]*0x128`, `src = FUN_141B59E20(entry)+8 = entry+0x38` (the row `roster_rows.py` reads), `Q = FUN_140922FF0(P+0x1A488)`. After the copy: `FUN_140922DD0(P,Q)` (derived record fields), `Q+0x64 = row dword +0x50` and `FUN_140A592B0(Q, DAT_14259BE5C)`: not in GF's window, not ported.
- Past the 0x50 bytes the block is NOT the row (offsets `0x50..` differ on every slot, `scr/blk2.py`), which matches a 0x50-byte copy.
- Port: `PesWorld::LoadRosterRow(slot, row)` (`pesworldrecord.cpp`: the 0x50-byte copy to `kSlotRecordOffset + kSlotAbilityOffset`), called by `WriteSlotAbilities` for a player whose `PlayerTraits::row` exists, before the store step's four ids. A Player.bin-only player has no row GF can build (PES's row loader for him is not decoded): his block starts from the store step's base, as before (named `ponytail:`).
- Not run: a comparison of GF's block against a captured AI record (needs the whole record from a capture; `blk2.py` shows block = row for the rows of three captures except the store step's ids and the +0x50.. tail).

## 3. Tests (CONFIRMED run: `PES21_ROOT`, `PES21_EDIT_SAVE` = the Documents save, `GF_PES_ROSTER_ROWS` = the 34-player whole-row file)

- `pes_editsave_test` **19/19**: new `EditRowStyle.EachSavedStyleSetsItsOwnBitAndNoneOrOutOfRangeSetsNone`, `...EachFlagSetsItsOwnBitOfTheFlagMask`, `...NeighbouringBitsAreNotTheStyleOrTheFlags`, `...TheRowImageHoldsEveryPartAtItsOffset` (hand-built non-flat records, all fields distinct), and the exe test extended: descriptors of ids 0x40..0x47, both style tables, the switch's jump table decoded from the code, the flag triples, and `DecodeEditRow` against the same chain read from the exe for every saved style 0..31 and every flag.
- `pes_playerdb_test` **15/15**: `EveryCapturedRosterRowEqualsTheSavesDecode...` now compares the whole 0x50-byte image except `0x3D..0x3F`, both masks, and `traits.row == decode`, for 34 players; `AFieldOutsideItsDescriptorRangeKeepsPlayerBinsValue` also pins the row's six bytes.
- `pes_playerdb_wiring_test` **6/6**: new `ARowOfTheSaveIsCopiedWholeIntoTheSlotsAbilityBlock` (every row byte but Speed/Jump lands in the block; the byte past the row does not; an untouched slot stays empty).
- Red-first: the new tests do not compile on `origin/main` (`styleMask`, `image`, `row`, `LoadRosterRow` absent).
- Mutations (each built, run, reverted by `git checkout` of the file): (M1+M3, `editsave.cpp`, applied together) the second style table dropped and the 5-bit value wrapped `% 22`: RED `EachSavedStyleSet...` and the exe test; (M2, same build) flag k sets bit k: RED the exe test only (the hand-built tests accept any injective map, so the function-level exe comparison is what bites); (M4) `LoadRosterRow` copies 0x40 bytes: RED the wiring test (row bytes 0x40, 0x49 differ); (M5) the six-byte patch of `ApplyEditSave` removed: RED `AFieldOutsideItsDescriptorRange...`. Not mutated: the `StyleWordOf` fallback branch and the two controller sites (no test reaches the style word through a match).

## 4. `0x142130EE0` (CONFIRMED run, SYNTHETIC)

`synth_members.py` cases: the kind step's own args, the man's slot (`*(u32*)*kind` = 0, 5, 11, 15: the side test at 0xB and the compared-man arms), `rcx+0x264` = 0x16, 0x20 (early-return arm), 0, 11, and the prediction frame (`*(kind+0x20)+0x3C` = 0, 3, 40, 255). `synth_call.py` + `snapdiff --bounded` over the state of `pid1981120/seq4891`: **13 calls, 13 replayed, 0 mismatches, own-block 0.2613** (was 1 call, 0.2252); `results_dribble.json` record `synthetic_cases: 13`, `"synthetic": true`. Only that record changed (compared per VA). The frame cases add no blocks; the slot and `+0x264` cases add the 0.036. The remaining arms need the records the function reads through pointers (`FUN_140A3E190(p,0/1)` fields `+0`, `+0x38`, `+0x40` for the first early arm; the compared man's speed record at `+0x24710`; the geometry test): their addresses must be derived from the state before poking. Not done.
- `gen_translated.py --check` and `ledger.py --check`: see the final message.

## 5. Left, numbered

1. `0x142130EE0` arms behind the `FUN_140A3E190` records and the speed-record compare (coverage 0.26).
2. Live `+0x150` writer (row byte 0x3C for non-save players) and the Player.bin row loader (so a Player.bin-only player gets a row too, and the `ponytail:` fallbacks in `WriteSlotAbilities`/`StyleWordOf` can go).
3. `FUN_140922DD0`, `Q+0x64`, `FUN_140A592B0` (the copy's neighbours), and a captured-AI-record comparison of GF's block.
4. The run-depth site at `teamAIcontroller.cpp:869` still keys on GF's style enum.
5. The `gfsim_run --edit-save` full-time smoke.
