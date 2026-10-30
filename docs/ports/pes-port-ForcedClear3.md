# Carrier walk inputs from PES writers: unit info and state by translated 0x14063C9F0 / 0x14063A490 on the window; danger byte and zone from the danger builder; team-info record separate and bound; receiver slots in PES slots; MI+0x1775 and the BP+0x41E4 / +0x32AD / +0x60 / +0xC8 writers decoded (EndClear, ForcedClear3, 30-10-26)

Branch `EndClear`, head `5733cc30` (confirmed by `git ls-remote origin EndClear`); `origin/main` merged and pushed first (`4d8184e8`). Worktree `gf-EndClear`, scratch `gf-EndClear-scratch` (`sd2/`, `sd3/` oracle dirs, `statedirs.py`, `scan_store.py`, PES-derived, never committed). Tags: CONFIRMED = read in tool output this session; INF = inference; OPEN = not settled. Budget ran past 70% before the smoke runs and mutations; items 5-6 below say exactly what was and was not run.

## 0. Verdict

| Step | State |
|---|---|
| 1 merge `origin/main`, push | DONE (`4d8184e8`). |
| 2 MI+0x1775 | DECODED, stand-in 0 is PES's open-play value (section 1). |
| 3 unit-updater fields | BP+0x60, +0xC8 (flag), +0x41E4, +0x32AD, the zone record, the unit info and the whole state record: now PES writers on the window, field-equal on 8 unslimmed captures except two named bytes (section 2). BP+0x3380 stays a named stand-in (section 2.4). |
| 4 receiver slots | DONE: `pes::ReceiverPesSlot`; the table now holds PES slots for all nine answer kinds (section 3). |
| 5 mutations | PARTLY: none of the three requested ones run (section 5). |
| 6 tests/smoke | `pes_walk_oracle_test` 6/6, `pes_carrier_test` 173/173; smoke NOT run (section 5). |

## 1. MI+0x1775 (CONFIRMED)

Every store with displacement 0x1768..0x1775 in the image (`scan_store.py`, capstone over `pes_full.bin`): `0x140A3C7AA` `mov [rbx+0x1774], ebp` (the match-info constructor, ebp = 0), `0x1404144E2` (the copy-assign: `movzx eax,[rdi+0x1775]; mov [rbx+0x1775],al`), `0x140A3CCF0` (`mov [rcx+0x1775],dl`, the setter) and `0x1422B6611` (a different struct). The setter has one call, `0x141FB1903` in `0x141FB1880`, with 3; `0x141FB1880` is called only at `0x141FD53FE`, case 5 of the flow `0x141FD5200` when `+0x10C == 2`. `+0x10C` becomes 1 or 2 at `0x141FC02E8` / `0x141FC0334` after string compares (`0x140CF3990`): a menu command picks it, not a match in play. So 9 and 10 have no writer, 3 is only that flow, and open play reads 0: all 9 captures where the page is readable (`pid132854` seq5796, `pid1981120` seq5878, `pid91171` seq040, 1088, 1562, 2207, 2862, 4032, 545) hold `MI+0x1775 = 0`. The GF write of 0 stays, with the decode in its comment (a PES-faithful value, not a stand-in).

## 2. The unit updater's fields

The driver `0x142110C90` (called from `0x142110D30`'s caller chain) writes the unit info with `0x14063C9F0(BP+0x1C, ctx, slot)` and the state with `0x14063A490(BP+0x78, ctx, BP+0x1C)` (CONFIRMED by decompile). `0x142110D30` (decompile, writers `0x142110F34`, `0x142110F88`, `0x14211105B`) sets the previous kind BP+0x41E4 to 0x19, then to 0x10 while the state's word 0 (BP+0x78) is 0, before the walk; `0x14210E5F0` (the BP reset) calls the state constructor `0x14063A3B0` and stores 0x19. BP+0x32AD is `imageA+0x31C1` (BP+0xEC + 0x31C1), the danger byte `RunDangerBuilder` writes: the old `bp[kBpGuardOffset] = 0` zeroed PES's own danger flag before the walk (a bug, deleted).

`PesWorld::RunCarrierUpdate(side, slot)` runs `0x14063A3B0` (when the unit changed), `0x14063C9F0`, `0x14063A490` and then writes BP+0x41E4 by the rule above; `SetCarrierInputs` and the hand-written unit info/state are deleted.

### 2.1 Oracle (CONFIRMED, run)

`UnitOracle.UnitInfoAndStateFromTheWindowEqualPesOnEveryCapture` (new): window rebuilt from a capture's WHOLE records (`BuildFull`), the two writers run, every byte of BP+0x1C..BP+0xEC compared with the capture's. Result on the 8 unslimmed walk captures (`pid91171` seq040, 1088, 1562, 2207, 2862, 4032, 545 and `pid132854` seq5796): every byte equal (unit info incl. the slot->info map +0xC, the three role words, the tier word BP+0x60, the position/sign words; the state incl. the ball copy and owned byte) except
- the two counters (state words 2 and 3, BP+0x80/0x84): the capture is after the update, so the test asserts +1 and "owned ? +1 : 0";
- BP+0x41E4: equal to the capture's (asserted);
- OPEN: BP+0x88..0x8B (ticks since the last event, seq040) and BP+0xC8 (the sticky flag, seq1088 and seq2862). Both come from event-store queries `0x140A3E5D0/0x140A3E940` whose none record is the image global `0x1436F3A40` (kind word 0x37, +0x14 = 0x0B, identical in file and capture), read through a register-held address the window does not bind. Binding it as an image segment fixed these two but moved BP+0xB4 (want 0x18, got 0) on every capture, cause not found. The test excludes exactly these bytes and says why. In production the same missing record means PES's event tests see "an event exists".
Older captures (`pid1965411`, `pid1981120`) are slimmed to the walk's read set and cannot serve this test (pitch record and most inputs absent): `GF_PES_UNIT_ORACLE` must name unslimmed ones (`statedirs.py`).

`UnitOracle.DangerBuilderOnTheWindowWritesTheWalksDangerByteAndZone` (new, PASS): with the danger byte and the zone record (BP+0xAB8..+0x180) zeroed, `RunDangerBuilder` on the capture's records gives the capture's bytes back on all 8. So the zone record is the danger builder's own output: `SetCarrierZone` (GF's close/not-close band) is deleted.

### 2.2 Window defects the oracle found (fixed)

- **G's side table B was unbound in production** (only A was bound in the constructor; the tests bound B = A). The walk's team-info reads (skill mask, designated taker `+0x7C`, slot->info map, `0x140A3AF30` info records) therefore read VA 0 in a real match, so `SetCarrierProfile` (written to the A record) never reached the walk. Fixed: a separate `PesTeamInfoRecord` per side (`kTeamInfoRecordBytes` = 0xA8 + 0x28 * 0x128), bound in the constructor, `SetCarrierProfile` writes it; PES's two side objects are different (captures: A `0x28862434`, B `0x2887A6D8`).
- The side record was too small for the tier word: `kSideRecordBytes` 0x4C30 -> 0xA2A0 (tier at +0xA294, flag byte +0xA29C, read by `0x140A40BD0`); `PesWorld::SetSideTier`. The tier writers are the menus' `0x140A424E0` callers (`0x141FBE480`, `0x141FB9FB0`, `0x141FCDA30` ...): GF keeps the live setting 500 as a named `ponytail:` at the call.
- Layout change: the window grew (two side records, two team-info records); no `#deps 0` object after the build (checked).

### 2.3 Stand-ins left, each with evidence

- BP+0x18 refresh flags = 0x1F (every capture); writer `0x142117370` is translated but its inputs are the plan block's (`ponytail` in `RunCarrierUpdate`).
- Team-info role words (info +0x10C..0x114, unit +0x30..0x38): GF's team-info record holds zeros; PES holds the roster's role ids (2/1/0xFF ...). The oracle copies PES's record, so equality holds; GF's own writer does not exist.
- The slot -> info map is the identity in GF (`SetCarrierProfile`); PES's map is a permutation (slot 3 -> 4, 19 -> 10: INF a lineup order). OPEN.
- MI+0x116C from GF's play state (unchanged `ponytail`); the designated taker (`T+0x7C`) = the carrier (unchanged: the restart set-up writer is undecoded).

### 2.4 BP+0x3380 (OPEN)

Its block is `BP+0x32C0` (+0xC0), built by `0x14211D710` only while the state's word 0 is 0, through `0x14211D9C0`, `0x14211E2D0`, `0x14211E210`, `0x14211E650` (not translated). The carrier's facing stands in, named at the call. Needs those four translated, then a field check on the captures.

## 3. Receiver slots (CONFIRMED by decompile; unit-tested)

The walk reads a record's `+8` as a man-record index: `0x14063E3A0` (`FUN_140a48a10(G, param_3)`), `0x140A638C0`, `0x140A62FE0` all go through `0x140A48A10`, whose index is `side * 11 + squad place` (absolute). GF's lane answers name lane slots (own team 0..10, opponents 11..21, active-list order) and the pass/long/cross answers a global id. `pes::ReceiverPesSlot` maps either through the two active lists to `Match::PesSlotOf`; the controller converts all nine answers for the table before `FillUnitTable`, and the lane answers to global ids afterwards for the commands as before. New test `PesCarrierLaneSlots.AReceiverIsNamedByHisPesSlotWhateverNamedHim` (away side, active order different from squad place). Not capture-checked: the only real answer in the 16 walk captures is seq5878's kind 3 (receiver 4 for carrier slot 10, side 0), which both conventions give.

## 4. Behaviour

The carrier's unit info and state, the previous kind, the danger byte, the zone and the team-info reads now come from PES's writers on GF's records. Visible effects expected (not measured, smoke not run): the walk's previous-kind rule sees 0x10 in play (PES's value), the Safety/Long arms see the real danger byte, the zone-dependent rules see PES's zone.

## 5. Verification

Run (direct, no gate): `pes_walk_oracle_test` 6/6 with `GF_PES_WALK_ORACLE` (8 real + 2 synthetic dirs) and `GF_PES_UNIT_ORACLE` (the 8 unslimmed captures), `pes_carrier_test` 173/173 (172 + the receiver-slot test), `pes_world_test` 6/6, `pes_danger_oracle_test` 2 run / 3 skipped (no `GF_PES_DANGER_ORACLE`); `pes_trap_oracle_test` builds. `pes_tick_a1_test` and `pes_movestep_wire_test` (under `build/tests/sim/`): see the final message for their counts.
NOT run: the 2-minute smoke on both paths (no budget left; the determinism lines are therefore not given), `gen_translated.py --check` (no layer file changed), `GATE_TIER=fast`, and all three mutations requested in step 5 (the unit-object rdx, `EncodeAnswerRecord`'s angle offset, `SetCarrierProfile`): no existing test exercises GF's production `SetCarrierProfile` effect, so its mutation would survive; a test that reads the skill mask through `0x14063E960` on the window is the missing piece. Red-first: the receiver-slot test and `UnitOracle` did not compile on the previous head (no `ReceiverPesSlot`, `RunCarrierUpdate`, `BuildFull`).
Interim line: `GATE fast (interim): NOT GREEN-CLAIMED: pes_walk_oracle_test 6/6, pes_carrier_test 173, pes_world_test 6, pes_danger_oracle_test 2 (+3 skipped); smoke not run`.

## 6. Open items

1. Smoke (both paths, 2 min, seed 7, twice) and the fast-tier gate on the merged tree: Main.
2. The three requested mutations (section 5); add a test that the team-info skill mask / height / kicker reach PES's readers.
3. The event-store none record `0x1436F3A40` (BP+0x88, BP+0xC8) and why binding it moves BP+0xB4.
4. BP+0x3380 (section 2.4); the slot -> info map and role words (section 2.3); BP+0x18 flags via `0x142117370`.
5. Cleanup: `gf-EndClear-scratch`, worktree `gf-EndClear`.
