# PES record layout, stage 1: team record T and its sheet S = T+0x8e08 (RecordLayout, 25-10-26)

Branch `RecordLayout` at 3103567 (red test 570cf06 first, then the implementation). Based on 69e63a5. Origin head verified with `git ls-remote`.

## 1. The pick (evidence, not argument)

Inputs: `tools/dev/translate/fieldmap.json` bindings, `results.json`, pilot specs `tools/dev/diffharness/specs/pilot/*.json`.

- 252 pilot functions: 216 diffed bit-exact, 33 live state, 3 struct-map gap.
- A function unlocks with record R only if every record it reads is R (closure). The fieldmap root `A0` is "slot record or team (per-function; provisional)", so root-name counts are not closures.
- Reclassified the 216 clean functions by the A0 offsets their pilot specs read:
  - G-table fallbacks (+0x19C8, +0x1928, +0x1978, +0x1DD8, +0x25F8; docs/re/pes-re-record-layouts.md §2): 5 (`140A48910 140A489D0 140A489F0 140A48A10 140A48A30`).
  - player-only (+0x554/+0x574/+0x57C/+0x5B0): 4 (`140458740 1404AB8E0 140A6C6D0 140A6C990`, the MoverRecord family; tests only, no production caller).
  - team-only (sheet and T words): 16 (the TeamSheet family).
  - no A0 read: 92. Other (small offsets; MI words +0x116C/+0x1554): 99.
- The first-pass "slot" count of 83 pure readers was inflated by team-root functions (+0x8E08..+0x9D6C, +0x21C, +0xA2A0). The team record unlocks more than the player record. The fieldmap root typing is a defect for Main.
- The team sheet is PES's byte layout, and GF already holds it per team. `pesteamconcepts.hpp:42`: "PES 2021's team sheet S = T+0x8e08". `TeamSheet::bytes_` is `kSheetBytes = 0x1488`, offsets named after PES functions. Spec offsets agree: captain +0x1438, designated +0x143C, taker +0x1454 (sheet frame); instruction records at sheet +0xef8 = T+0x9d00 (spec `140A42430` reads 0x9d00..0x9d6c). `teamAIcontroller.cpp:1963` gives designated at T+0xa244 = 0x8e08 + 0x143c. CONFIRMED.
- Size: the furthest T read in the specs is T+0xC134 (u32), end 0xC138. The fieldmap's 49468 is 0xC13C; the extra 4 bytes are not justified by any read. Used 0xC138. The real PES allocation size is not in the docs: OPEN.

## 2. The record and its owner

- `PesTeamRecord` (`src/sim/onthepitch/pesteamconcepts.hpp`): reserved `[0, 0x8e08)`, `TeamSheet sheet` at 0x8e08, reserved tail to 0xC138. `static_assert`s on the sheet offset and the size. Fields GF does not carry are zero-initialised reserved space (section 5).
- Owner: `TeamAIController::teamRecord`, one per side. Replaces `teamSheet`; 28 call sites migrated in `teamAIcontroller.cpp`; `GetTeamSheet()` returns `teamRecord.sheet`. The name `team` is already a `Team*` member, hence `teamRecord`.
- Window: `PesMem::window` (`tools/dev/translate/pes_state.hpp`): non-owning, addressed by VA, read-only or writable. Translated code reads S in place: no copy, no shim.

## 3. Switch-overs (PES build: rcx = window VA of S, rdx = k, r8 = plan; the hand port `sheet_port::` stays for no-image builds)

| VA | GF method | Live GF caller (src/) | Agreement test (same random sheets) |
|---|---|---|---|
| 1408C5080 | `TeamSheet::Designated(k)` | teamAIcontroller.cpp:1965 | PASS, 200 sheets, k = -1..6 |
| 1408C5030 | `TeamSheet::Toggle(k, plan)` | teamAIcontroller.cpp:491, 499, 2286, 2649, 3229, 3599; elizacontroller.cpp:1079 | PASS, 200 sheets, plan 0..3, k = -1..7 |
| 1408C52E0 | `TeamSheet::Slider(k, plan)` | teamAIcontroller.cpp:62-67, 492-493, 3228 | PASS, 200 sheets, plan 0..3, k = -1..5 |
| 1408C5060 | `TeamSheet::Taker(k)` | none in src/ (only `FreeKickTaker`, `GetPieceTaker`, unrelated) | PASS after the port fix below |

Findings:
- **Taker, k < 0: PES disagrees with the first GF port.** PES's `FUN_1408c5060` refuses k > 2 only; for k = -1 it reads S+0x1450. `Designated` refuses k < 0 in PES. The first GF port refused every k < 0. The port now matches PES, guarded to stay inside S. No GF caller passes a negative k (grep). The divergence was latent in GF.
- **ABI.** An int argument in `edx` zero-extends into `rdx`. The first test sign-extended it and reported a false mismatch at k = -1. The helper and the test now zero-extend (LaneZ does the same).
- **Taker has no live GF caller.** It is translated and proven, but no reachable path exercises the switch. Reported, not claimed as live.

## 4. Build, tests, repros

- Build (configured with PES_EXE): pespilot generated for 1408C5030, 1408C5060, 1408C5080, 1408C52E0 and 140A71990 (LaneZ). `gfsim_run` and `pes_team_sheet_test` link. Errors 0. Build exit 0.
- Red first: 570cf06 failed to compile on every missing name, observed.
- Test `tests/pilot/pes_team_sheet_test.cpp` (via `tools/dev/simslot.sh`): 5/5 PASSED on the image build. `TeamRecord.SheetSitsAtPesOffsetInsideTheTeamRecord` plus four agreement tests (table above). The Taker agreement failed at k = -1 before the port fix and passes after it.
- Repro, seed 7, 5 minutes, `--pes21-root <workdrive>/PES21`, switch-on build: GF mode determinism `7ca3d08f68567016`; `--pes-loop` determinism `67d115b984bab251`. Both exit 0.
- NOT RUN: switch-off build (no PES_EXE) and main reference runs. The switch-off configure died when its temporary shell ended, and the budget ran out. "Switch-off equals main" is therefore UNVERIFIED. The PES-loop hash has no verified cause yet: it is an unexplained new value, not a comparison.
- Mutation checks: not run (budget).
- Gate: not run (Main gates).

## 5. Not done in this stage (numbered)

1. Captain, SlotTarget, RegisteredRole, SlotRecordA/B, the setters, record base 1408C6430, and the instruction readers `140A42430` and `140A42880` (team-frame window; callee 1408C6430 must join the pilot set). Several specs cover only A0_0, so agreement is not proven over the whole sheet domain: spec inputs need widening first.
2. T words outside S: 0x21C, 0x250/0x254 (facing, sign), 0x3A9C..0x3FB8, 0x4C00, 0x7D8C..0x7D98, 0xA294..0xA2A0, 0xB8A2/0xB8A4 (GF `PesTeamTick.b8a4` is a per-tick copy, not the record), 0xB8F8, 0xC134. Reserved for now. Next: move `PesTeamTick` into the record.
3. Global G (`0x1436F3DC8`): 10 clean readers need G. Not in this stage.
4. MI, MA, A1, A2, A3: fieldmap roots not resolved by offsets alone.
5. Player (slot) record: GF has no persistent per-player PES state at slot offsets. MoverRecord, BackSpaceMate and PressPickInput are per-call snapshots; MoverRecord's readers have no production caller.
6. Fieldmap root typing: `A0` must be split per function (slot, team, MI, G). Defect for Main.
7. `fieldmap.json` gf set for the two sheet fields: T+0x8e08 (plan) and IDX+0xA25C (taker).
8. Switch-off vs main reference, mutation checks, and the PES-loop hash cause (item 4 of section 4).

## 6. Per-switch-over cost (requests, approximate)

| Item | Requests | Note |
|---|---|---|
| Evidence: closures, spec reclassification, sheet offset check | ~45 | the bulk; largest single cost |
| Record, window, owner migration (28 sites), pilot lib, CMake | ~25 | shared by all four |
| Four agreement tests and wiring | ~6 | shared |
| Taker ABI and port fix (3 failing runs) | ~6 | Taker only |
| Build, test and repro cycles | ~8 | shared |

Per switch-over marginal: Designated ~1, Toggle ~1, Slider ~1, Taker ~2 (the fix). Pilot-lib generation adds one translated TU per reader at build time.

## Main's verification on the merge with main (batch Z)

- Rebuilt in the same command: 5-min seed-7 switch-off `7ca3d08f68567016`, `--pes-loop` `67d115b984bab251`: both equal
  main's, as they should when the translated readers agree exactly with the hand ports they replace.
- `pes_team_sheet_test` (tests/pilot): 5/5. Mutation: the hand-port taker read shifted by one word turns exactly
  `TakerAgreesWithHandPortOnRandomSheets` red; restored, 5/5.
