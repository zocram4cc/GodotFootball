# PES 2021 Wave 1: T-SHEET (team tactics sheet, T+0x8e08)

Worktree `<workdrive>/gf-Wave1TSheet` (detached at origin/main 453640f, branch `Wave1TSheet`).
Evidence: Ghidra bridge (decompile and disassembly by address), install exe `PES2021.exe`
(opcode-checked reads at run time), and the decompiles in `<workdrive>/gf-Wave1TSheet-scratch/dec`.
No live reads.

## 1. The block (S = T+0x8e08, 0x1488 bytes), field by field

Accessors (GF names) and the PES function each one ports. "reader open" means no PES reader was
decoded; "writer open" means no writer was decoded.

| Offset (in S) | Shape | Reader (PES) | Writer (PES) | GF accessor | State |
|---|---|---|---|---|---|
| +0x000 | u32 plan | `*S` (plan 3) | FUN_1408c5e20 (plan < 3) | `Plan`, `SetPlan` | done |
| +0x004 + 5p + k | u8 sliders [3][5] | FUN_1408c52e0 (0 when refused) | FUN_1408c5df0 | `Slider`, `SetSlider` | done; k0 SR, k2 DL, k3 Compactness read by AI; k1, k4 inert |
| +0x013 + 7p + k | u8 toggles [3][7] | FUN_1408c5030 | FUN_1408c5c70 (keeps 0/1, else 1) | `Toggle`, `SetToggle` | done |
| +0x028 + 4·slot-phase | u32 registered role [3][11][3] | FUN_1408c51b0 (default 0xc) | open | `RegisteredRole` | reader done, writer open |
| +0x1b4 / +0x268 + 2·(10p+i)·3+f | u16 x / y coord | FUN_1408c5130 / FUN_1408c5170 | FUN_1408c5d50 (x < 49, y < 105) | `CoordX`, `CoordY`, `SetCoord` | done |
| +0x31c + 12·slot-phase | 12 B record A [3][11][3] | FUN_1408c50e0 (refused: record 0) | open | `SlotRecordA` | reader done, writer open |
| +0x844 + 4(11p+slot) | u32 slot target | FUN_1408c50a0 (default 0xff) | FUN_1408c5d10 (slot < 11 or 0xff) | `SlotTarget`, `SetSlotTarget` | done |
| +0x8c8 + 4·slot-phase | u32 role [3][11][3] | FUN_1408c5310 (default 0xc) | FUN_1408c5e90 (role < 13) | `Role`, `SetRole` | done |
| +0xa54 + 12·slot-phase | 12 B record B [3][11][3] | open | FUN_1408c5e30 (copy) | `SlotRecordB`, `SetSlotRecordB` | writer done, reader open (accessor reads back what the writer wrote) |
| +0xef8 + 0x70(i + 4p) | 0x70 B instruction record, 4 per plan | FUN_1408c6430 (refused: record 0) | FUN_140a42600 (`SetInstruction`), FUN_140a42880 (`SetInstructionByte30`), FUN_140a42430 (`ResetInstruction`) | `Instruction*` | done (T-INST owns the meaning of the fields) |
| +0x1438 | u32 captain (roster index or 0xff) | FUN_1408c5020 | FUN_1408c5c50 (< 0x28 or 0xff) | `Captain`, `SetCaptain` | done |
| +0x143c + 4k, k < 6 | u32 designated (roster or 0xff) | FUN_1408c5080 (0xff when refused) | FUN_1408c5cf0 | `Designated`, `SetDesignated` | done |
| +0x1454 + 4k, k < 3 | u32 taker (roster or 0xff) | FUN_1408c5060 (0xff when refused) | FUN_1408c5cb0 | `Taker`, `SetTaker` | done |
| +0x1460 | u32 (< 4) | open | FUN_1408c5c20 | not modelled | writer done, reader open |
| +0x1464, +0x1465, +0x1466, +0x1468 | u8 flags | open | FUN_1408c5c30, FUN_1408c5c40, FUN_1408c5c10, FUN_1408c5ee0 | not modelled | writers done, readers open |
| +0x1469 + k, k < 3 | u8 flags | FUN_1408c58e0 (k < 3) | FUN_1408c5ef0 | not modelled | writer done, reader known, not ported |
| +0x146c + k, k < 3 | u8 flags | open | FUN_1408c5cd0 | not modelled | writer done, reader open |
| +0x1470, +0x1474, +0x1478 | u32 (ctor: `DAT_14351ae44`, +0x80) | open | ctor 0x1408c5390 | not modelled | open |
| +0x147c, +0x1480, +0x1484, +0x1486 | -1 / 0xffff / 0xff | open | ctor 0x1408c5390 | not modelled | open |
| +0x7c0 .. +0x843 | 33 u32 (between the record-A block and slot targets) | open | open | not modelled | open |
| +0x8c8 .. (sheet tail) | covered above | | | | |

The constructor FUN_1408c5390 (VAs in `pesteamconcepts.cpp`, opcode-checked) supplies these defaults:
sliders k0 `1408c5418`, k1 `1408c5438`, k2 `1408c5454`, k3 `1408c5471`, k4 `1408c548e` (plan 0 only, the
current plan at construction); captain `1408c5838` (0x0a); designated loop `1408c5858` (0x0a, six
slots); takers `1408c586b`, `1408c587a`, `1408c5885` (0xff); slot-target loop `1408c5671` (0xff). The
rest of the ctor's stores are zeros (toggles, roles, coordinates, record A and B, flags).

## 2. Setters (the foundation's named writers)

- FUN_140a42600 `SetInstruction(i, id, arg, param, plan, flag)`: refuses i > 3 and id 0x11. Writes id (+0),
  param (+0x34), clears +4..+0x2b, sets +0x2c = 0xff, stores arg at +0x2c for id 0xe (arg < 0x16), marks
  slot arg%11 at +4 for ids 0x8, 0xa, 0xf, and writes flag at +0x6c for every id except 0. It also calls
  FUN_140ca1a50, which is `return 1` (no effect, not ported).
- FUN_140a42880 `SetInstructionByte30`: writes the byte at +0x30.
- FUN_140a42430 `ResetInstruction`: id 0, clears +4..+0x2b, +0x2c = 0xff, +0x30 = 0, +0x34 = 0, +0x38 = 0xff,
  +0x6c = 0.
- Quirk ported: FUN_1408c6430 returns the base (record 0) for an out-of-range record index or plan.

Callers of the setters (Ghidra xrefs): FUN_1404fabd0 (reset and byte), FUN_1404fd4b0 (byte), FUN_1404ff7d0
(reset), FUN_141fc1590 (record writer, team setup). The first three run in match (team tick), so
instruction records change during play; their writer is therefore not only setup.

## 3. Install defaults and the loader

`LoadTeamConceptConstants` reads the ten stores above as opcode-checked immediates (dword reads added as
`ReadDwordImm`). `DefaultTeamSheet(k)` applies them to a zero image and resets the instruction records. The
install relations test (PES21_ROOT) loads them: 16/16 pass, including the new reads.

## 4. What the sheet is fed from (open)

- Writer of the team's sheet at match setup: FUN_141fc1590 (21 KB, team setup builder). It calls
  FUN_1408c5e20 (plan), FUN_1408c5df0/5c70 (sliders/toggles), FUN_1408c5e90 (roles), FUN_1408c5d50 (coords),
  FUN_1408c5e30 (record B), FUN_1408c5d10 (targets), FUN_1408c5c50/5cf0/5cb0 (captain, designated, takers),
  FUN_1408c5ef0/5cd0 (flags), with inputs from the per-side setup block `param_5 + 0x14 + side*0x248` (lVar10)
  and its helpers FUN_1408e16d0/1e19b0/1e1c30/1414c6220/1414c61d0. The input's source (DB table, EDIT save,
  or menu) is not established.
- GF has no importer of PES sheet data. `tactics_xml` (teamdata.cpp) carries GF tactic keys only; the fixture
  teams carry no sheet keys. So every GF team runs on the install defaults plus whatever sliders and toggles
  its tactics set (as before).
- Team-struct copies (T+0x4b4c, T+0x4b40 outlet, T+0x4c00, T+0x4bd4, T+0x558): FUN_141fc1590 does not write
  them, so they are copied elsewhere (not traced). T+0x4b40 is not in S, so "sheet-fed outlet" is open.

## 5. Stand-ins left, and why

- Taker waterfall (FUN_1404a8ad0): the designated index used there is local_f4 in {2, 1, 0, 6} (6 = none)
  for FUN_1408c5080; the taker loop (+0x1454) iterates k < 3 in the set-piece code (AnimLeftovers st2). The
  map from index to kick kind is not decoded, so no GF stand-in was rewired to Taker/Designated. The comments
  in `pessetpieceteam.*` and `teamAIcontroller.cpp` now say that the record exists and is not yet read.
- Roles, coordinates and slot records have readers in T-SHAPE and T-CONC, which are later waves. No stand-in
  was rewired to them here.

## 6. Tests and mutation checks

- `tests/sim/pes_team_concepts_test.cpp`: 14 `PesTeamSheet` tests and 1 `PesTeamSheetDefaults` test
  (synthetic constants), plus the install relations test (`PesTeamConceptsInstall`, needs PES21_ROOT).
- Run: no PES21_ROOT: 30 passed, 1 skipped. With PES21_ROOT: 16 passed (sheet and install).
- Mutation check (six guarded lines mutated at once, each to be caught by one test): exactly those six tests
  failed, the other nine passed:
  - taker defaults loop skipped -> `DefaultsAreTheConstructorStoresFromTheInstall`
  - id 0 clears the flag path -> `IdZeroLeavesTheFlagByteAlone`
  - refused record index to record 1 -> `OutOfRangeRecordIndexWritesRecordZero`
  - coordinate x bound off by one -> `CoordinatesStoreBothAxesWithinPesBounds`
  - role bound off by one -> `RolesAcceptBelowThirteenAndReadTwelveOutOfRange`
  - toggle clamp removed -> `TogglesStoreOneForAnyValueAboveOne`
  All six lines were reverted; the sheet suites were re-run green.
- Not run as RED: the test and implementation were written in one pass before the first compile (the
  compile-RED was not observed). The mutation check is the evidence that the tests bite.

## 7. Next steps (not done here)

1. Port the taker waterfall's index-to-kick map (FUN_1404a8ad0 local_f4 and the taker loop in AnimLeftovers
   st2), then rewire `FreeKickTaker` and the set-piece taker choice to `Designated`/`Taker`.
2. Decode the writers and readers of +0x28, +0x31c, +0x1460..+0x146e, +0x1470..+0x1486 and the 33-dword block
   at +0x7c0; then the T+0x4b40 outlet (the copy from S into the team struct, FUN_141fc1590 callers).
3. Identify the sheet's input source at match setup (what FUN_141fc1590's param_5 is loaded from) before any
   importer is written.

## 8. Gate, smoke and commit

- Commit `53b9742` on `origin/Wave1TSheet` (base `453640f`). Eight files; `extern/godot-cpp` not staged.
- Gate (owner's ctest command, fleet lock): 723/723 pass. Standing skips only: `VfsInstall.NestedOriginalsSurviveWithoutDownloads`, `EditSaveInstall.DecryptsAndMatchesTheRunningGame`. `repo_pii_scan` passes.
- Seed-7 smoke (20 min): byte-identical to a `gfsim_run` built from `453640f` (46,841 lines; determinism `45b5c9894844c576`).
- Gates the structure unblocks (T3, T5, T7, DR1): the structure exists; the gate ports are separate work and are not done here.
