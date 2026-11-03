# DribbleArms: the other arms of 0x142130EE0 proven, the live +0x150 writer found and ported, the COM dribble knobs deleted with a census, the style enum and the COM/style inference deleted (DribbleArms, 03-11-26)

Branch `DribbleProposal` (worktree `gf-DribbleProposal`, scratch `gf-DribbleProposal-scratch/arm`, PES-derived, never committed). Image `pes_full.bin` (VA - 0x140000000). CONFIRMED = decompile (bridge), asm, script read of the image or a test run this session; INF = inference; OPEN = not settled. Continues `pes-port-DribbleRow.md` Left 1-3, `pes-port-StyleCutover2.md` Left 2-3 and `pes-merge-AT/AU` Open.

## 0. Verdict

| Step | State |
|---|---|
| 1 merge `origin/main`, push | DONE: main `8429cfbc` was a fast-forward of the branch; later merged again (`75ff13ef`); pushed (`ls-remote` confirmed). |
| 2 `0x142130EE0` arms | DONE: 53 synthetic cases (13 before + 40), **53/53 bit-exact, own-block 107/111 = 0.964** (was 29/111). The 4 blocks left are infeasible (section 1). |
| 3 writer of live `+0x150` | FOUND and PORTED: `FUN_1414F8420` (the Player.bin -> live loader) stores `0x13` there when Player.bin record dword `+0x3C` bit 18 is set; row byte `0x3C` = `FUN_141FC0C40(0x13)` = `0x16`. GF writes it in `PlayerDatabase::ApplyEditSave`. |
| 4 COM dribble knobs | DELETED, with a census of every PES COM-mask reader (none blends drive/repel/centre/velocity). |
| 5 style enum | DELETED: `PlayingStyles::Player`, `ParsePlayer/InferPlayer/SuitsRole/Serialize`, `InferCom`, `PlayerData::playingStyle`. `pes::style::Bit` names the row's bits; a player without a save row takes `pes::kDefaultStyleWord` (0). |
| 6 smokes, gate | section 5: smokes rc 0; **GATE fast: RED** (two seeded tests need census re-staging, not done). |

## 1. `0x142130EE0` (CONFIRMED run, SYNTHETIC)

Every case is labelled `"synthetic": true, "synthetic_cases": 53` in `results_dribble.json`. `synth_members.py::arm_cases` derives every record address from the forward-emulated state's own pointers and chooses the values; the state is the kind step `0x14212FE70`'s entry state (`pid1981120/seq4891`). Records (CONFIRMED by decompile + asm of the function and its callees):

- match-info pair: `FUN_140A3E190(*(ctx+0xE0), 0/1)` = `mi+0x180698` / `mi+idx*800+20`; the three gate tests `[+0]==6`, `[+0x38]==6`, `[+0x40]==0x11` (`0x142130FA6..FBF`), `FUN_140A60300` on `+4`, then the three equality tests (`0x142130FD7..FEF`).
- speed records: `e0 = *(ctx+0x18)`, `e0+0x24710 + (byte[e0+0x247BE+slot] + (slot>=11 ? 11 : 0))*8`; the man's must be `<= 6.25` (`0x1425A9BE4`) and `<=` the compared man's (`0x142131277 COMISS`), else the default arm `0x14213176D`.
- geometry: `FUN_140A6CF70(*(G+0x40), frame)` = `BI + (frame*5+0x11)*8`; distance test `0x1421311BD COMISS [0x14259BE10]` = 16.0; ball segment `FUN_140A8F7C0(ball0, &angle, 5.0)` (`0x14259222C`) ending at `ball0 + (F(a,5), F(a+90,5))`, `F` = `FUN_140A92C80` (table sine at `0x142655950`, replayed in float32 by the case builder); the man's path `pos + {0, 27}*vel` (27 = `int(54.0*0.5+0.5)`); the crossing test `FUN_140A900E0`; window `4.0 < d^2 < 25.0` (`0x1425656C0`, `0x14259BE20`); angle clamps `0x14259BE5C`=90, `0x14259A764`=60.
- skill word: `FUN_14063E960(.., slot, 0x25)` = bit 37 of the u64 at `T + idx*0x128 + 0xA8 + 0x30 + 0x48` (the row's `+0x40` SKILL mask, not the style or COM word).

Cases: `a_*` (8: each gate test failing, the vector compare failing, every equality failing, all equal), `rec_*` (2), `slot_over/slot_left/own_still` (3), `still±_*` (8: heading side x the two clamped differences over/under 90), `cross±*` (8: from each side, direction argument below 0 / in range / past a turn (PES's `F` misplaces the segment there, replayed exactly), crossing 1 m out), `skill*` (5), `end_*` (6: man's `+0x4C`, kind 3/7, heading difference 10 / 100, the all-pass arm). The 13 earlier cases stay (they cover the `rcx+0x264 > 0x15` early arm).

Infeasible blocks (4 of 111; INF by reading the code, not run): `0x14213122B`, `0x142131258`, `0x14213128D` are the negative-slot arms behind `CMP EDX,0x15; JBE` (an unsigned compare of a sign-extended dword: a negative never gets there); `0x1421314C2` is the `JZ` of `CMP RCX,RAX` where `RAX` is the out pointer `FUN_140A90D30` returns (`rbp-0x70`) and `RCX` is `rsp+0x60`: never equal.

`synth_members.py --chunk N` (new): each poked case is a ~2 GB sparse copy of the state's memory; the runs' replays and mismatches add up, coverage is the union of the harness's executed addresses. The 53 ran as four chunks of 14 (peak ~28 GB).

## 2. The writer of live `+0x150` (CONFIRMED)

- `FUN_1414F8420(ctx, live, rec)` (called by `FUN_1414F4500` at `0x1414F457D`, which `0x1414F6CE6` calls): the Player.bin record (312 bytes, id at `+8`) -> live record: names, nationality (`rec[7]>>9 & 0x1FF`), dates, `FUN_1414C78E0(live, rec, 0)` for the abilities. Bytes `0x1414F85E9`: `8B 47 3C  C1 E8 12  A8 01  74 07  C6 83 50 01 00 00 13` = `mov eax,[rdi+0x3C]; shr eax,0x12; test al,1; jz; mov byte [rbx+0x150],0x13`.
- `FUN_141FC0C40`'s jump table (`0x141FC0C55` `mov ecx,[r8+rax*4+0x1FC0D14]`): case `0x13` -> `mov edx,0x16`; `FUN_141FE9F40` (the row builder's mask step) writes `FUN_141FC0C40(*(live+0x150))` to row byte `0x3C` through `FUN_1408CB290`.
- Why the 34 captured rows show 0: their Player.bin records in the owner's install are 4cc placeholders (flat); the flag is clear. The captured-rows test still passes with the byte now taken from Player.bin (no contradiction, and no discrimination either).
- INF: a saved player's live record is loaded from Player.bin first and the save overlays it, as GF's `ApplyEditSave` already assumed for the six bytes GF reads ("a field the save holds out of range keeps Player.bin's value"); no save field writes `+0x150`, so his byte is his Player.bin record's. A captured row of a saved player whose Player.bin record has the flag set would settle it.
- Port: `playerdb.hpp` `kLiveFlagBit = 498`, `kLiveFlagValue = 0x13`, `kRowByteOfLiveFlag = 0x16`, `PlayerTraits::live150`; `Parse` sets it from the record, `ApplyEditSave` writes `row[0x3C]`.
- OPEN: the player WITHOUT a save row still has no row at all (his Player.bin -> row path, `FUN_1414C78E0` + the builder, is not decoded), so his `0x3C` byte, style word and COM word have no source (`ponytail:` at `Match::StyleWordOf`, `PlayerData::GetComStyles`).

## 3. COM dribble knobs: PES has none (CONFIRMED census)

Readers of the COM flag word (live `+0x4C`, roster `+0x54`), from `FUN_14063E830` xrefs and the direct `[x+0x4C]` reads of `pes-re-com-styles.md` 2.4:

| Reader | What it does |
|---|---|
| `FUN_14211AB10` (`0x14211AECE`), `FUN_14211AF20` (`0x14211B19D`), `0x14211B600` | the carrier's image evaluators (Cross, Long, MiddleShot, Breakthrough grades): ported in `pescarrier` / `elizacontroller` |
| `FUN_14211FE20` (`0x142120787/7B9/87E`), `FUN_142123750`, `FUN_14212A2D0`, `FUN_14212CC20` (Mazing), `FUN_14212FE70` (`0x142130719`) | the dribble proposal kinds / feint payloads (translated chain) |
| `0x1407DDFDE` in `FUN_1407DDD10`, `0x1408527AF` in `0x140852770` | a motion-kind pick (`kind 0x1A/0x5B..`: Trickster gates animation 9/0x11) and a virtual `IsTrickster` accessor |

None of them blends an attraction, a repulsion, a centre pull and a carry velocity: those four weights are GF's own force field (`AI_GetBestDribbleMovement`), so `GetDribbleDrive/OpponentRepel/CenterPull/Velocity` (invented factors 1.3, 1.15, 0.6, 0.75, 0.5-blend, 1.25) are deleted and `AIfunctions.cpp` takes the bases directly. The scan of direct `[x+0x4C]` readers beyond the doc's list was not redone (INF that the list is complete).

## 4. The style enum (CONFIRMED by tests)

- Deleted: `PlayingStyles::Player` and its `ParsePlayer/Serialize/SuitsRole/InferPlayer`, `InferCom` (+ `ComStyleBonus`, the affinity tables, hash), `PlayerData::playingStyle`/`GetPlayingStyle` and the `playing_style` tag handling, the COM inference (`com_styles` tag stays: data).
- `pes::style::Bit` (pesstyle.hpp) names the 21 bits of the style word (same numbers as the old `EngineBit`); `EngineBit/StyleWord(Bit)` are `constexpr`. `Match::StyleHas(data, Bit)`. Callers: keeper (bits 19/20), Destroyer (12), Cross Specialist (7), `StyleLb`, `StyleRunDepth`.
- `Match::StyleWordOf` for a player without a save row returns `pes::kDefaultStyleWord` (= 0): `FUN_140AA1700` zeroes the live record, style value 0 maps to no bit (`kEditStyleMaskBit[0] = -1`, `static_assert`ed; `FUN_141FC0AD0`'s default arm).
- Tests (run directly, `PES21_ROOT`, `PES21_EDIT_SAVE` = the Documents 2021 save, `GF_PES_ROSTER_ROWS` = the 34-row file): `pes_style_gate_test` 5/5 (`EachStyleBitIsOneMaskBitOfASavedStyleValue`), `pes_offball_test` 81/81, `pes_playerdb_wiring_test` 11/11 (`APlayerWithoutARowTakesPesDefaultWordAndStandsWhereANoStyleRowStands` replaces the two enum tests), `pes_playerdb_test` 17/17 (new `TheRowsByte0x3CFollowsTheRecordsLiveFlagBit`, exe test `LiveFlagConstantsEqualTheLoaderAndTheRowSwitch`), `pes_editsave_test` 19/19.
- Red-first: the new tests do not compile on `origin/main` (`pes::style::Bit`, `kLiveFlagBit`, `kDefaultStyleWord` absent).
- Mutations (applied together in one build, run, reverted): M1 `Parse` reads bit 499 instead of 498: RED `TheRowsByte0x3CFollowsTheRecordsLiveFlagBit`; M2 `StyleWordOf` default word `1<<2` (a Fox bit): RED `APlayerWithoutARow...`. Not mutated: the `AIfunctions.cpp` knob deletion (no test reaches the force field with a COM mask: `GetComStyles` is none for every fixture player now).

## 5. Smokes and gate

2 min, seed 7, `simslot.sh`, clean build (a mutation restored by `mv` keeps its old mtime and ninja does not rebuild it: the first gate ran on mutated objects and its `TheRowsByte0x3C...` failure was that; touched, rebuilt, the test is OK):

| Path | main baseline (brief, after batch AV) | this branch (after merging origin/main, `75ff13ef`) | first run, before that merge |
|---|---|---|---|
| switch-off | `3b73c968b4a377e0` | `7e371da577eeb18d` (rc 0; 15 stoppages, 22765 ticks, idle 19 / stalls 13) | `8c1d6afb17f2bcbe` |
| pes-loop | `0e480b158cf80bd4` | `827a3deecedd8d93` (rc 0; 18 stoppages, 31829 ticks, idle 1 / stalls 1) | `341f1d29802fb7e8` |

Both moved. Ablation: NOT separable in this tree. The three coupled deletions (COM knobs, `InferCom`, the style inference) cannot be switched one at a time without an old-tree build: the fixture profiles carry no `playing_style` / `com_styles` tag, so before this change every fixture player had an INFERRED style and COM mask and, after it, every one has none (style word 0, COM mask 0). With an empty mask the four knob functions returned their base values, so the knob deletion alone is neutral; the whole move comes from the two inference deletions (style word -> keeper / Destroyer / cross / run-depth gates; COM mask -> the carrier planners and `PlayerSkills::Unlocks`). Not run: the old tree with one source forced off. The merge of main between the two columns also moved the hashes: it is not separated either. Tripwire: no crash or hang, rc 0 both, the gate's smoke and this run give identical hashes, shots and goals non-zero. Not aimed at any `[balance*]` line.

Gate (`GATE_TIER=fast`, slots 3 2, load ~30, 4249 s): **GATE fast: RED** on three tests:

- `PlayerDatabaseParse.TheRowsByte0x3CFollowsTheRecordsLiveFlagBit`: the stale mutated objects above; OK on the clean rebuild (run directly).
- `PesForfeitMatch.ARedCardRemovesTheOffenderAndTheSideIsNotForfeited` (`pes_forfeit_test.cpp:318`, `ASSERT_TRUE(match->IsGameOver())`: the seed-7 red-card match no longer reaches full time inside the 15 simulated-minute window) and `PesThrowInRelease.ACeremonyThrowInIsReleasedFromTheHandOverTheSpot` (`pes_throwin_release_test.cpp:191`, no ceremony throw-in in the first two minutes of seeds 5, 7, 9): both fail on the clean tree (run directly). They are seeded matches the deletions re-roll (styles and COM masks were part of the old rolls): per the rules they are RE-STAGED BY CENSUS, never weakened. NOT DONE (budget): a serial 1..40 seed census through `simslot.sh` for each (`kCeremonySeeds`; the forfeit test's `SEED`), then a gate re-run.
- The recorded re-check selected 0 rows in all five sets, no status regression (18595 rows), off-path ports 125 allow-listed, `LEDGER.md` current (38076 rows); `gen_translated.py --check` was not run separately (no translated source changed).

## 6. Left, numbered

1. Re-stage the two seeded tests by census (section 5), then re-run `GATE fast`.
2. The Player.bin -> row path for a player the save does not hold (`FUN_1414F8420`/`FUN_1414C78E0` + the builder): his style word, COM word and `0x3C` byte; then `kDefaultStyleWord` and the COM `ponytail:` go.
3. `FUN_140922DD0`, `Q+0x64`, `FUN_140A592B0` (the AI-record copy's neighbours); a captured-AI-record comparison of GF's ability block.
4. `0x142130EE0`: real-call cases for the gate arms (all 53 are synthetic: a real capture reaching them needs a hook predicate on `ctx+0x264` / the match-info kind).
5. COM source for the carrier planners (`GetComStyles` is the profile tag; PES reads the row's `+0x4C` in PES's mask order, GF's `Com` enum has another bit order).
6. The `gfsim_run --edit-save` full-time smoke; the ablation of section 5.
