# EditSaveFields: the EDIT save record -> roster row map, decoded from PES's code (EditSaveFields, 31-10-26)

Branch `DribbleProposal` (worktree `gf-DribbleProposal`, scratch `gf-EditSaveFields-scratch`, never committed). Image `pes_full.bin` (VA - 0x140000000). Continues `pes-port-DribbleProposal7.md` (Left 1, 2, 4, 5) and `pes-re-condition-import.md`. CONFIRMED = decompile, asm or script read of the image this session, or a test run; INF = inference; OPEN = not settled.

## 0. Verdict

| Step | State |
|---|---|
| 1 `git merge origin/main`, push | DONE (`43954349`). |
| 2 decode the save record -> row map from code | **DONE for bytes 0x00..0x3B and the skill mask +0x40.** The fitted map of DribbleProposal7 is replaced. Two layers of code, both located (section 1). |
| 3 production + tests | DONE: `DecodeEditRow` (all 58 row bytes the builder writes + 41 skill flags), `ApplyEditSave` takes Speed, Jump, Height, foot, **injury tier and Form** from it; 34/34 captured rows equal; hand-built non-flat record per field; table re-derived from the exe in a test; 4 mutation runs (section 4). |
| 4 `gfsim_run --edit-save PATH` | DONE (same `ReadEditSave` + `ApplyEditSave` as `PesLibrary::load_edit_save`). Smoke: section 5. |
| 5 gate | Section 5: interim line. |

**Corrections to DribbleProposal7 (CONFIRMED, section 2):** Speed is save id 0x1C = **bit 174**, Jump is save id 0x1F = **bit 192**. The fitted bits 256 / 358 are GK Reach (id 0x2A, row byte 0x25) and GK Reflexes (id 0x29, row byte 0x24). The fit could not see it: every ability is flat in this save and in all 34 captured rows. Form is save id 0x2D = bit 252 (not Player.bin's 438); the injury tier is id 0x2E = bit 327 (not 458). Both were wrong or absent for edit players before.

## 1. The two layers of PES code (CONFIRMED)

**Layer A, the save record's own accessors.** The descriptor table `0x1434F2B20` (0x7A entries of 0x30 bytes: `{u32 id, getter, setter, min | default<<32, max | bits<<32, 0}`) is looked up by `FUN_141402570` (its only code reference to the table is `0x141402578`, a rip-relative load; every other consumer goes through the lookup). Each getter is a leaf, `[movzx/mov eax,[rcx+disp8]] [shr eax,n] [and eax,m] mov [rdx],eax; mov al,1; ret`, with `rcx` = the 0x138-byte save record (id at +0). Example, script-read: `0x141402450` (id 0x10) `0F B6 41 0A` = `movzx eax,byte [rcx+0xA]` -> Height is the save byte +0x0A; `0x141402a20` (id 0x1C) `8B 41 14 / C1 E8 0E / 83 E0 7F` -> bit 0x14*8 + 14 = 174, 7 bits. The ids are the SAME ids as `pes-re-matchlogic.md` 1.2 (Form 0x2D, injury 0x2E, foot 0x0F, Height 0x10, Speed 0x1C, Jump 0x1F); only the bit positions differ from Player.bin. The test (`EditRowInstall.TheTableEqualsTheExes...`) decodes every getter from the exe with opcode checks.

**Layer B, save -> live 0x17C record -> row.**
- `FUN_141F10CE0(_, live record, save record, id, _)` is a switch on the save id that copies one save field into the live 0x17C record (setter `FUN_1414C6E90`). A value outside the descriptor's `[min, max]` (descriptor `+0x18`, `+0x20`) is NOT copied. Cases (decompile + asm): `0x0D` -> field 0x2D (age); `0x0E` -> field 0 (position); `0x0F` -> field 0x2A (foot); `0x10`/`0x11` -> `FUN_1414C01A0(0/1)` (Height 0x2E / Weight 0x2F); ids `0x12 + k`, k < 0x1D (`0x141F1118E lea r15d,[r9-0x12]`, `0x141F11192 cmp r15d,0x1C`, call `0x141F111F5` -> `FUN_141F114B0` -> `FUN_1414C0180(k)`, key -> field table `0x142978FD0`); ids `0x33 + k`, k < 0xD (`cmp r15d,0xD` `0x141F11128`, call `0x141F11184` -> `FUN_1412DC920` -> `FUN_1414C0220`); ids `0x2F + k`, k < 4 (`0x141F1135C lea r15d,[r9-0x2F]`, call `0x141F113AF` -> `FUN_141F11540` -> `FUN_1414C0240`); ids `0x71 + k`, k < 9 (`0x141F112FA lea r15d,[r9-0x71]`, call `0x141F11355` -> `FUN_141F114E0` -> `FUN_1414C01C0`; k = 8 goes to `rec+0x148` bits 20-27); skill flags ids `0x48 + k` (`lea edi,[rcx-0x48]` `0x141F11200`, `cmp r15d,0x28` `0x141F11203`, `FUN_1414C0260`).
- `FUN_141FE9B00` (called from the match-prep `FUN_141FE3830` at `0x141FE40F3`) builds the row from the live record: `FUN_141FE9BA0` (table `0x14352AB10`, 29 `{row, key}` pairs, key -> field through the SAME `FUN_1414C0180`; plus foot field 0x2A -> row 0x34 and field 0x32 -> row 0x10), `FUN_141FEA170` (`0x14352ABF8`, 2 pairs), `FUN_141FEA000` (position row 0 from field 0, and `0x14352ADC0`, 13 pairs, familiarity), `FUN_141FEA250` (skill mask), `FUN_141FE9F40` (style masks), `FUN_141FE9E50` (`0x14352AE30`, 9 pairs), `FUN_141FE9D00` (Height 0x31, Weight 0x32, age 0x30 with default 0x14). Every byte store is clamped by `FUN_1408CAF60` (table `0x142613C60`, 60 x `{min,max}`); the live-record field side is a bit-packed struct read by `FUN_1414C66C0` (switch).
- Because stage A and stage B apply the same key -> field function, the composed map is row <- id with `id = 0x12 + key` (`0x14352AB10`), `0x33 + key` (`0x14352ADC0`), `0x71 + key` (`0x14352AE30`), `0x2F + key` (`0x14352ABF8`), plus the single-field rows. That composition is what the test recomputes from the exe's tables.

## 2. The map (script output, `rowmap.py`; all 34 captured players, 0 mismatches on every row byte below)

Pasted per row: row byte <- save id, bit, width, descriptor gate [min,max], row clamp [min,max]. Speed and Jump are the two the port reads.

```
row 0x27 Speed    <- id 0x1C bit 174 w7 gate[40,99]   row 0x2C Jump    <- id 0x1F bit 192 w7 gate[40,99]
row 0x31 Height   <- id 0x10 bit  80 w8 gate[100,250] clamp[132,220]
row 0x32 Weight   <- id 0x11 bit  88 w8 gate[30,150]  clamp[30,129]
row 0x30 Age      <- id 0x0D bit 263 w6 gate[15,50]   clamp[15,63]
row 0x34 foot     <- id 0x0F bit 381 w1               row 0x2E Form <- id 0x2D bit 252 w3 [0,7]
row 0x2F injury   <- id 0x2E bit 327 w2 [0,2]
row 0x14 OA 112, 0x15 DA 288, 0x16 GK Aw 238, 0x17 Drib 320, 0x18 BC 119, 0x19 TP 128, 0x1A Fin 149, 0x1B LP 135, 0x1C LoP 142,
0x1D Head 302, 0x1E BW 224, 0x1F Aggr 231, 0x20 PK 160, 0x21 Curl 167, 0x22 GKC 245, 0x23 GK Clr 295, 0x24 GK Refl 358, 0x25 GK Reach 256,
0x28 PC 199, 0x29 Bal 206, 0x2A KP 365, 0x2B Acc 181, 0x2D Stamina 213    (7 bits each, saved raw, no bias; gate [40,99])
row 0x00 position <- id 0x0E bit 269 w5 (live field 4 bits, > 12 -> 1, clamp [0,12])
rows 0x01..0x0D familiarity <- ids 0x33..0x3F bits 333 335 337 339 341 343 345 347 349 372 352 354 356 (2 bits)
rows 0x0E 0x0F 0x10 0x11 0x12 0x13 <- ids 0x76 0x77 0x32 0x75 0x78 0x79; 0x26 <- 0x2C bit 318; 0x35 <- 0x2B bit 126;
0x36 <- 0x30 bit 329; 0x37 <- 0x2F bit 284; 0x38..0x3B <- 0x71..0x74 bits 309 156 312 188
skills: u64 at row +0x40, key k (ids 0x48 + k, bit 390 + k) sets skill bit kEditSkillBit[k] (table 0x14352AC70, 41 pairs)
```

The names (OA, DA, ...) are `pes-re-matchlogic.md` 1.3's Aoba labels; the id-to-row pairing behind them is now the code's (each save id's Player.bin name from matchlogic 1.2 lands on the Aoba row, e.g. id 0x1C Speed -> row 0x27, id 0x23 Defensive Awareness -> row 0x15, id 0x26 GK Awareness -> row 0x16), which also closes `pes-re-matchlogic.md` 1.3's "Speed <-> Jump and KP <-> Acc INF" pairs.

Data check (CONFIRMED run): `rowmap.py` over the 34 captured rows (`roster_rows.py` output) and the rig's save: rows 0x00..0x3B (58 decoded bytes) and the skill mask all equal. Abilities are flat in every row and in the save (4830 players), so the ability pairs rest on the code, not on the data; the non-flat fields (height, foot, Form, injury, position, familiarity, weak-foot, motion bytes, skills) vary and match.

## 3. Production

- `src/pes/editsave.hpp/.cpp`: `kEditRowFields` (59 entries: row, save id, bit, width, gate, clamp), `kEditSkillBit`, `EditRow DecodeEditRow(rec)` (gate, then the position rule, then clamp; `set` marks written bytes). The fitted constants (`kEditSpeedBit`, `kEditJumpBit`, `kEditHeightOffset`, `kEditStrongerFootBit`, `kEditAbilityBits`) are deleted.
- `PlayerDatabase::ApplyEditSave` takes Speed, Jump, Height, foot, **injury tier and Form** from `DecodeEditRow`; a field the save holds out of range keeps Player.bin's value. A save id with no Player.bin record still adds nothing (the condition byte has no source for it).
- `apps/gfsim_run.cpp`: `--edit-save FILE`, the same two calls as `PesLibrary::load_edit_save`.
- `tools/dev/rendezvous/roster_rows.py`: writes `id hex(0x50 row bytes)` per player.
- Clean cutover: no fitted map left; no Player.bin fallback for a field the save holds in range.

## 4. Tests and mutations (CONFIRMED run)

- `pes_editsave_test` 15/15 (`PES21_ROOT`, `PES21_EDIT_SAVE`): new `EditRow.EveryRowByteTakesItsOwnFieldWhenNoTwoFieldsAgree` (hand-built record, a distinct value in every field), `...SpeedJumpHeightFootInjuryAndFormComeFromTheirDecodedBits` (literals, with GK Reach/Reflexes at the old fitted bits holding other values), `...AFieldOutsideItsDescriptorRangeIsNotApplied`, `...AnAcceptedValueIsClampedToTheBlockBounds`, `...PositionKeepsFourBitsAndAboveTwelveBecomesOne`, `...EachSkillFlagSetsItsOwnSkillBit`, and `EditRowInstall.TheTableEqualsTheExesDescriptorGettersAndBuilderTables` (every getter opcode-decoded from the exe, the four pair tables, the six single-field rows, the clamp table and the skill table recomputed and compared with the production table, both directions).
- `pes_playerdb_test` 15/15 (`GF_PES_ROSTER_ROWS` = the 34-player whole-row file): `EveryCapturedRosterRowEqualsTheSavesDecodeAndTheDatabaseWithTheSaveApplied` (every decoded byte, the skill mask, and Speed, Jump, Height, foot, injury, Form of the database equal PES's row, 34 players), `TheSavesRecordOverridesSpeedJumpHeightFootFormAndInjuryTierOfItsIdOnly`, `AFieldOutsideItsDescriptorRangeKeepsPlayerBinsValue`. Red-first: the new tests do not compile on `origin/main` (`DecodeEditRow`, `kEditRowFields` absent), and the old fitted reader gives Speed from bit 256 (mutation 1 reproduces it).
- `pes_playerdb_wiring_test` 5/5 (54 s).
- Mutations, each built, run, reverted (`git checkout` of the .cpp): (1) Speed read from the old bit 256: RED `EveryRowByteTakesItsOwnField...`, `SpeedJumpHeightFoot...`, `TheSavesRecordOverrides...` (3 of 3 expected; the install test stays green on flat abilities, as the fit did: the hand-built tests catch it). (2) gate removed: RED `AFieldOutsideItsDescriptorRangeIsNotApplied`, `...KeepsPlayerBinsValue`. (3) clamp removed: RED `AnAcceptedValueIsClampedToTheBlockBounds`. (4) position 4-bit truncation removed: RED `PositionKeepsFourBitsAndAboveTwelveBecomesOne`. Not mutated: the exe test's tables (it compares both directions; a changed production constant goes RED there too, which mutation 1 would also trip if the header were edited: not run, a header edit rebuilds the sim).

## 5. Smoke and gate

- `gfsim_run --edit-save` smoke: see the final message (the run waits for a sim slot; result recorded there). A missing file prints `EDIT save PATH: ...` and exits 1 (path guarded by `ReadEditSave` on an empty buffer).
- `GATE fast`: not run (Main gates). Test binaries run: the three above, plus the build of `gfsim_run`.

## 6. Left, numbered

1. Playing-style masks, row `+0x48` (u32) and `+0x4C` (7 flags): `FUN_141FE9F40`, tables `0x14352AC10` (7 triples), `0x142979060` (byte map, save id 0x40 -> live field 0x3B via `FUN_1414C04A0`), `0x142979080` (via `FUN_1414C0300`), helpers `FUN_141FC0AD0`, `FUN_141FC0C40`, `FUN_1408CB290`. Captured rows show the mask (e.g. save style 20 -> bit 19, 12 -> bit 12, 5 -> bit 4); not ported (GF reads no style mask from the row).
2. Row bytes 0x3C..0x3F and 0x40-0x4F beyond the skill mask: rebuilt or set elsewhere (constant on 34 players).
3. What a field that fails its gate leaves in the live record: the record's initial value (`FUN_140A9FD00` / `FUN_140AA1700`), not Player.bin's; GF keeps Player.bin's. Irrelevant for the rig's save (every value in range).
4. `FUN_141FEA000`'s fallback when no familiarity byte was written (marks the registered position 1 / byte 2 = 1): not ported; the rig's save has all 13.
5. The copy of the row into the AI record `+0x70` (DribbleProposal6 OPEN) is still unlocated; the row itself is now exact.
6. Weight (row 0x32), age (0x30) and the rest are decoded but GF reads none of them yet.
