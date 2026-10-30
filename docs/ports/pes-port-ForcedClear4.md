# Carrier window verification gaps closed: event none record built from PES's constructor (unit oracle field-equal on 8 captures, no exclusions); the designated taker was written to the wrong record (fixed); profile read back through PES's readers; three mutations killed (EndClear, ForcedClear4, 30-10-26)

Branch `EndClear` (worktree `gf-EndClear`, scratch `gf-EndClear-scratch`, PES-derived, never committed). Tags: CONFIRMED = read in tool output this session; INF = inference; OPEN = not settled. Continues `pes-port-ForcedClear3.md` (Left open 1-4).

## 0. Verdict

| Step | State |
|---|---|
| 1 merge `origin/main`, push | DONE, twice: first docs-only (`5c44a2ce`), last `f5253535` (main `5d0e82fa`, one conflict in `tools/dev/translate/driver.py`, resolved to main's `reached_roots`; the branch had no edit of its own there). |
| 2 profile test + three mutations | DONE (section 2). The test found a real bug: the taker was written to a record the walk never reads. |
| 3 none record, BP+0xB4 | DONE: root cause found, all fields equal, exclusions deleted (section 1). |
| 4 BP+0x3380 writers, slot->info map, role words | NOT DONE (section 4). |
| 5 smoke, `gen_translated --check`, fast tier | smoke deterministic on both paths; check green; fast gate: see section 5. |

## 1. The event none record and BP+0xB4 (CONFIRMED)

- The none record is `0x1436F3A40`, 0x320 bytes (`IMUL RAX,RAX,0x320` `0x140A3E971`; returned by `0x140A3E940` at `0x140A3E97C`, by `0x140A3E5D0`'s tail and `0x140A3E190`'s out-of-range arm). It lies past `.data`'s raw bytes in the install exe (`.data` VA `0x14339B000`, raw size `0x334800`, so raw ends at `0x1436CF800`): `PeImage::ReadVa` cannot read it. PES builds it at start-up: the static initialiser `0x1401C2944` does `LEA RCX,[0x1436F3A40]; CALL 0x140A3CDD0`, and the constructor (decompile and asm) zeroes 0x320 bytes (`MOV R8D,0x320` `0x140A3CDD8`), stores `[RBX+0x10] = 0xB` (`MOV dword ptr [RBX+0x10],0xB` `0x140A3CDE8`) and `[RBX] = 0x37` (`MOV dword ptr [RBX],0x37` `0x140A3CDF9`). ForcedClear3's note said "+0x14"; the word is at +0x10 (`pes_full.bin` bytes at the VA: `37000000 00000000 00000000 00000000 0b000000`).
- Why binding it "moved BP+0xB4": the load failed (`image table unreadable at VA 5426330176`, my first attempt, reproduced), `InstallTables()` in the test returned an EMPTY table set after its `EXPECT_TRUE`, so the frame-rate cells `0x14351AE50` were absent and BP+0xB4 (state word 15 = `(int)(FUN_141E532D0() * DAT_1425903B4 + DAT_142565600)`, `0x14063A490`) read 0 instead of 0x12. The none record itself never moved it.
- Fix (`pesimagetables.cpp/.hpp`): `LoadPesImageTables` builds the record the constructor leaves, reading the two stores' immediates from the exe with their opcodes checked (`C7 03`, `C7 43 10`); nothing typed. The frame-cell check addresses its segment by index (it used `.back()`).
- Test: `UnitOracle.UnitInfoAndStateFromTheWindowEqualPesOnEveryCapture` with the exclusions (BP+0x88..0x8B, BP+0xC8) deleted. Red first (CONFIRMED, run): BP+0x88 on `pid91171_seq040` (got 0x70, want 0) and BP+0xB4 on both; green after the fix: every byte of BP+0x1C..BP+0xEC equal on the 8 unslimmed captures except the two documented counters (ticks +1, owned +1/0), asserted separately.

## 2. Profile through PES's readers; the taker bug; mutations

New (`tests/pes_walk_oracle_test.cpp`, no capture env needed for the first):
- `CarrierProfile.PesReadersSeeTheCarriersMaskAndHeightAndNobodyElses`: `SetCarrierProfile` for both sides (different masks and heights), then PES's own readers `0x14063E960(slot, bit)` (skill, through the slot->info map `0x140A3AF90`) and `0x14063D8A0(slot, 0x31)` (height byte) run on the window for all 22 slots: the carrier's slot reads its mask bits and height, every other slot reads 0.
- `CarrierProfile.TheWalkReadsTheTakerFromTheSideRecordAndTheCarrierFromTheUnitObject`: play state 3, one listed kind with a dribble answer (record type 3). The carrier equal to the side record's `+0x7C` is skipped (`kWalkNoKind`); a different carrier slot in the unit info is handed the listed kind (`CMP EDX,[RAX+0x7C]` `0x1421101C7`, `JNZ 0x142110235`; `0x14210F1DB..1E6` loads the record from `0x140A48A30(G, unit info side)`).
- `PesA1Sim.TheClearRecordGfListsEqualsTheRecordPesClearUnitWrites` (`tests/sim/pes_tick_a1_test.cpp`): for each of side 0's 11 carrier slots in a real match window, `EncodeAnswerRecord({Clear, angle})` equals PES's Clear unit's own record at the type word and at the angle word (`kAnswerAngleOffset`), with at least one nonzero angle so the offset matters.

**Bug found (red first, CONFIRMED):** the walk's taker test reads `+0x7C` of G's FIRST side table (`0x140A48A30` -> the side record), but `SetCarrierProfile` wrote the taker into the team-info table (`0x140A489D0`'s object, the skill reads' table). The oracle never saw it because it copies PES's side record. In a real match the side record's `+0x7C` stayed 0, so in a stopped play the carrier never equalled the taker. Fixed: `SetCarrierProfile` writes `SideRecord(side)+0x7C` (the `ponytail:` that the taker is the carrier stays, naming the undecoded restart set-up writer). Red run before the fix: both sides "the taker himself" got 3, wanted 25.

Mutations, each applied, rebuilt, run, reverted (CONFIRMED):

| # | Mutation | Killed by |
|---|---|---|
| 1 | `RunWalk` rdx = the other side's unit object | `WalkOracle.EveryCapturedWalkCallOnGfsWindowReturnsPesKind`, `WalkOracle.TheListGfWritesReproducesPesKindOnEveryCapture`, `CarrierProfile.TheWalkReadsTheTaker...` (3 failed) |
| 2 | `EncodeAnswerRecord` Clear angle at +0xC instead of +8 | `PesCarrierTable.TheAnswerRecordCarriesTheFieldsTheWalkReads` and `PesA1Sim.TheClearRecordGfListsEqualsTheRecordPesClearUnitWrites` |
| 3a | taker written to the team-info record (the old code) | `CarrierProfile.TheWalkReadsTheTaker...` |
| 3b | `SetCarrierProfile` mask written at info index `(slot+1)%11` | `CarrierProfile.PesReadersSeeTheCarriersMaskAndHeight...` |
| 3c | slot->info map shifted (`(k+1)%11`) | `CarrierProfile.PesReadersSeeTheCarriersMaskAndHeight...` |

All reverted; `pes_walk_oracle_test` 8/8 and `pes_carrier_test` 173/173 rebuilt and green after the reverts.

## 3. Walk inputs

Field-equal on all 8 captures with no exclusions (`UnitOracle`), plus the danger byte and zone (`DangerBuilderOnTheWindowWritesTheWalksDangerByteAndZone`). The walk oracle (`WalkOracle.*`, 8 real + 2 synthetic) passes.

## 4. Not done

1. BP+0x3380 writers `0x14211E2D0` / `0x14211E210` / `0x14211E650`: not translated, not called. The translated layer is committed only for functions with a bit-exact diff record; these three have no capture hook (carrier_walk's captures never reached them as separate calls), so translating them needs a new hook or a snapdiff of `0x14211D710`, Main's call. The carrier's facing still stands in (named `ponytail:` at the call).
2. Slot->info map (PES: a permutation, e.g. slot 3 -> 4, 19 -> 10, INF a lineup order; GF: identity) and the team-info role words: the PES writer is undecoded; the profile test now pins the identity map as GF's production behaviour, so a change to it must update the test.
3. BP+0x18 refresh flags via `0x142117370`; MI+0x116C; tier word 500: unchanged stand-ins.

## 5. Verification

Run (direct): `pes_walk_oracle_test` 8/8 (`GF_PES_WALK_ORACLE` 8 real + 2 synthetic, `GF_PES_UNIT_ORACLE` the 8 unslimmed), `pes_carrier_test` 173, `pes_tick_a1_test` 27, `pes_movestep_wire_test` 28, `pes_involvement_test` 6, `pes_mark_table_test` 19, `pes_match_info_test` 2, `pes_offball_test` 76, `pes_scorerows_test` 12, `pes_team_tick_test` 26, `pes_tick_order_test` 4, `pes_danger_oracle_test` 2 run / 3 skipped (no oracle env), `pes_angle_oracle_test` and `pes_trap_oracle_test` skipped (no env set). No seeded test moved: `pes_tick_a1_test` and `pes_movestep_wire_test` pass unchanged, nothing re-staged.

`gen_translated.py --check` (venv python, `pes_full.bin`): `4346 functions (3694 recorded bit-exact, 652 closure) regenerate byte-identical`. No layer file changed.

Smoke, 2 min, seed 7, teams 1 v 2, `--pes21-root`, each run twice (`gfsim_run`, slots 2-3):

| Path | Run 1 vs run 2 | md5 of the log minus timing lines | Result lines |
|---|---|---|---|
| default | identical | `e2ffa0ab75b5fe5a91d67649a80c598f` | `[ticks] 27703`, 21 stoppages, passes 13-10, shots 0-6, clearances 2-1, idle 18 episodes / stalls 8 |
| `--pes-loop` | identical | `f777b664308fd77b3d09d90ae1142af9` | `[ticks] 26170`, 11 stoppages, passes 35-29, shots 2-0, clearances 0-0, idle 3 / 3 |

Moved vs ForcedClear2's pes-loop line (passes 70-28, shots 2-4, clearances 0-1): yes, as ForcedClear3's real unit info/state/danger/zone inputs change the carrier's walk; these are tripwire numbers, not tuning targets. No crash, hang or zero-pass run. No baseline on current main was run for the default path.

`GATE_TIER=fast GATE_SLOTS="3 2" tools/dev/gate.sh` on the merged tree (`f5253535`) was started and did NOT finish: under load 25-30 it passed its selection step (488 changed paths, all recorded functions selected: 245/245, 685/685, 546/546, 2253/2253, 34/34) and both gate smokes (`smoke-off rc=0`, `smoke-pesloop rc=0`), then hit my 3600 s cap inside the ctest phase with no result line. Final line therefore NOT GREEN-CLAIMED: `GATE fast (interim): pes_walk_oracle_test 8/8, pes_carrier_test 173, pes_tick_a1_test 27, pes_movestep_wire_test 28, pes_world-linked tests as listed, gen_translated --check identical, smoke both paths deterministic; gate itself timed out in ctest (log in the scratch dir)`. Main: rerun the fast tier when load allows.

## 6. Open items

1. BP+0x3380 writers (section 4.1), needs a capture hook or Main's decision.
2. Slot->info map and role words from GF state (section 4.2).
3. Cleanup: `gf-EndClear-scratch` (`sd2/ sd3/ wo/ smoke/ gate_fast.log`), worktree `gf-EndClear`.
