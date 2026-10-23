# PES team sheet: the sheet triple, its install source, the plan, and the grid (SheetTriple, 23-10-26, part 2)

Image `pes_full.bin` (file offset = VA − 0x140000000). Sheet `S` = `T+0x8E08`. Labels: **CONFIRMED** = read from bytes or a decompile and checked; **INF** = inference; **OPEN** = not decoded, blocker named. Checker: `gf-SheetTriple-scratch/checker_sheet_triple.py` (17 byte anchors, layout identities, 1326 install groups, 10 constants: RESULT OK; selftest OK).

## 0. Status

- M1 (which tactics record fills a plan): **answered** for the record-to-plan rule (§6). The side's current plan index is **OPEN** (§6c).
- M2 (group entry → plan table): **shown** from the decompile and the accessor layouts (§7).
- M3 (grid formula): **written out** (§8).
- Port: pure module `src/pes/formation.{hpp,cpp}` with 11 tests; wired on the PES loop (`WriteRoleArrays` → `WriteSlotPoints`; `UpdatePesDesignation` key = slot-point f2). Branch `SheetTriple`, head `f8c9c68`.
- Repro: 5-min seed-7 `--pes-loop` hash **a5fe3f7b1a6f7faa**, unchanged by the port (§10). Not a verified cause.

## 1. Sheet layout (CONFIRMED)

| offset | content | accessor |
|---|---|---|
| +0x00 | plan u32 (0..2; `3` = current) | `FUN_1408C50E0` |
| +0x28 | 99 × u32 registered role | `FUN_1408C51F0` / `FUN_1408C5920` |
| +0x1B4 / +0x268 | 90 × u16 outfield x / y | `FUN_1408C5130/5170` / `FUN_1408C5D50` |
| **+0x31C** | **99 × 12 B: the sheet triple (record A)** | `FUN_1408C50E0` reads; `FUN_1408C5920` writes (copy of B) |
| +0x844 / +0x8C8 | slot targets / role table | `FUN_1408C50A0` / `FUN_1408C5310` |
| **+0xA54** | **99 × 12 B: record B** | `FUN_1408C5E30` (only caller: setup builder `FUN_141FC1590`) |

Identities: `0x28+4·99=0x1B4`, `+2·90=0x268`, `+2·90=0x31C`, `+12·99=0x7C0`, `+4·33=0x844`, `+4·33=0x8C8`, `+4·99=0xA54`, `+12·99=0xEF8`.

## 2. Triple accessor and rows writer (CONFIRMED by bytes)

`FUN_1408C50E0(S, slot, f, 3)`: `fam = S[0]`, valid iff `fam ≤ 2`, `slot < 11`, `f < 3`; returns `S+0x31C + 12·(3·(11·fam+slot)+f)`; invalid returns `S+0x31C` (VA 0x1408C50E0–0x1408C5124).

`FUN_1404F5F10(P, S, MI, F)` (VA 0x1404F5F10): `f = 2 − (F≠0)`; `f = 0` when `MI+0x116C ≠ 1 && MI+0x1554 == 1` (VA 0x1404F5F38–0x1404F5F4A). For each slot `k`: `P+0x3A9C+12k ← t`, `P+0x3B20+12k ← t` where `t = FUN_1408C50E0(S, k, f, 3)` = (f0, f1, f2). Restart variant `FUN_1404A8620` (corner chooser, VA 0x1404A87F6 and 0x1404A8825): `f = 0` as a literal.

## 3. The install source (CONFIRMED structure, all 1326 groups)

`common/etc/pesdb/Tactics.bin` = 1326 × `{u32 tacticsId, u32 teamId, u32 packed}`; `TacticsFormation.bin` = 1326 × 396 B groups, group `g` has the id of record `g`, 33 entries `{u32 id, u32 role, u32 packed}`, `packed = x | y<<8 | slot<<16 | phase<<20` (x < 49, y < 105, slot < 11, phase < 3, role ≤ 12, bits ≥ 24 zero). 663 teams, two records each. Fixture teams 1 and 2 are present. Team.bin ids (701–910, 210 records) do not overlap Tactics team ids, so Tactics ids are keyed on `TeamData::GetDatabaseID()`.

## 4. Earlier correction

`Tactics.bin`'s third word is not a hash: it is the plan flag word (bit 24 = plan) with the team's slider and toggle values in the other bits (FUN_1414F5120 l.857–893). `TacticsFormation.bin` is cells, not an index (§7). Corrects pes-re-setpiece-referee §1.11 and the earlier §3 of this file.

## 5. Setup builder and keeper (CONFIRMED)

`FUN_141FC1590` reads the side blocks (`setup+0x14+side·0x248`, plan at `setup+0xB34+side·0xA8` via `FUN_1408E16D0`) and fills record B. For table slot 0 (the keeper) it does not use the table cell: `mov dword [rbp-0x78], 0xC2480000` (bytes `c7 45 88` at VA 0x141FC1954, immediate at 0x141FC1957) stores the record `(−50.0, 0, 0)` in every phase (`if (uVar16 == 0)` at 0x141FC1950).

## 6. M1: which tactics record fills a plan

**6a (CONFIRMED).** `FUN_1414F5120` (VA 0x1414F5120, 1042 lines) decodes each Tactics record into a plan block. `uVar27 = bit 24` of `packed` (l.857–893) selects the destination: `ppuVar25 + (bit24·0xA0 + 0x40)` (plan block) and `puVar30 + (bit24·0xA0 + 0x460)`. So **record bit24 = 0 fills plan 0, bit24 = 1 fills plan 1**. Install data: team 191 records 0x1AF6 (bit24 0) and 0x1AF7 (bit24 1); team 1 records 0x1B50, 0x1B51.

**6b (CONFIRMED).** Each record's formation group is the table-24 group with the same id (l.386–491: entries sorted by key `(id, tag, slot)`; l.938–985 match each record's id). The group fills that plan's table (§7).

**6c (OPEN).** Which plan index a side uses. `FUN_1408E16D0` reads `setup+0xB34+side·0xA8` (checked: its only readers are 17 functions listed in the decompile xrefs; the writer is not among the setup callers searched: `FUN_141FD6190` l.240–279 copies a 94 × 0x80 state block, `FUN_141FBAC10` l.100–140 reads the side blocks' roster at `+0x1E4` and writes the team's player table). In play, `FUN_140500B90` (VA 0x140500B90) sets the team sheet plan: `FUN_1421B7D40` by default; `param_1+0xBCC` < 2 → 1; == 2 → `team+0xB8A3 ≠ 0 ? 1 : 0`; 3–4 → 0; and plan 2 in a special state (l.291–320). The mapping of GF's attack level to `+0xBCC` is OPEN.

## 7. M2: group entry → plan table (CONFIRMED by decompile and accessors)

For each group entry `e` (id = record id, `tag = (packed>>20)&3` = phase, `slot = (packed>>16)&0xF`, `role`, `x = packed&0xFF`, `y = (packed>>8)&0xFF`):
- `role == 0`: `(x, y) := (3, 52)` (l.966, immediate 0x3403);
- phase block `tag` (33 bytes): role byte at `+slot`; coordinates at `+0x0B+2·slot` (x) and `+0x0C+2·slot` (y) for tag 0; the same at `+0x21`, `+0x2C` for tag 1 and `+0x42`, `+0x4D` for tag 2 (`FUN_1414C6180/61D0` read and `FUN_1414C65E0/6650` write: CONFIRMED layouts).
- Three 33-byte blocks = the 99-byte table; the sheet's record B reads it through `FUN_1414C6180/61D0/6220`.

## 8. M3: the grid (CONFIRMED by decompile and ops; constants by bytes)

`FUN_14051B3F0(out, F, (x, y))` writes `out = (X, 0, Z)`: `X` from the x byte over F0 (length), `Z` from the y byte over F1 (width). Per axis with half-size `h`:

    cell  = (h + h) / 7.0            (DAT_14259bdf0, VA 0x14259BDF0)
    step  = cell / 15.0              (DAT_1425c5cd0, VA 0x1425C5CD0)
    centre = step · 0.5              (DAT_142565600, VA 0x142565600)
    j = v / 15,  k = v % 15
    axis(v) = (cell · j − h) + ((cell · k) / 15.0 + centre)

Equivalently `(v + 0.5)·2h/105 − h`. The sub-table entries `local_128[k]` and the cell entries `local_c8[j]`, `local_e8[j]` are the same expressions (decompile l.25–110, asm 0x14051B3F0–0x14051B856). Constants are read by the port from the image; F0 = 52.56 and F1 = 34.06 are GF's FieldInfo constants (`pesFieldHalfLength`, `pesFieldHalfWidth`).

## 9. The port (what the code does)

- `src/pes/formation.{hpp,cpp}`: `ParseTactics`, `BuildPlan` (M2), `GridAxis` / `SheetTriple` (M3), `SlotPointRows` (slot 0 = keeper row, slots 1–10 = triple of cell `[slot][phase]`), `FormationTables::Load/PlanOf` (M1: plan = bit 24), `ReadSheetGrid`, `ReadKeeperRow`, `ReadSheetConstants` (exe constants with the addresses above, opcode-checked for the keeper).
- `TeamAIController::WriteRoleArrays` calls `WriteSlotPoints(phase)` with `phase = WritePhase(!head && possession)` (= `2 − (flag≠0)`, the same rule as `FUN_1404F5F10`). `WriteSlotPoints` takes the team's plan 0 (stand-in, §10 item 1) by DB id.
- `UpdatePesDesignation`: lateral key = slot point f2, raw (FUN_140A40CA0, CONFIRMED in pes-re-team-layer2). The GF formation-point stand-in is removed from this path.
- Plumbing: `MatchSetup::pesFormations / pesSheetConstants`, `Match` copies and exposes them; `gfsim_run` loads them.

## 10. The repro (5 min, seed 7, `--pes-loop`, `tools/dev/simslot.sh`)

| run | build | `[determinism]` |
|---|---|---|
| pre-change (binary built from origin/main) | yes | a5fe3f7b1a6f7faa |
| wired (this port) | rebuilt | a5fe3f7b1a6f7faa |
| wired, designation key forced to 0.0 (probe) | rebuilt | a5fe3f7b1a6f7faa |

`UpdatePesDesignation()` runs on every PES-loop team tick (`team.cpp:548`), but its key does not reach this run's trajectory: the constant-key probe leaves the hash unchanged. So the hash did not move, and there is no hash-moving cause to verify. Not verified: longer runs, set-piece scenarios, and any consumer of the designation list (`PesBoxMen`), which this run does not reach.

## 11. Tests

`pes_formation_test` (11): synthetic tests of the plan rule, the group transform and keeper case, the grid axis, the triple, and the slot rows; install tests (skip without `PES21_ROOT`) for the table structure, plan pairs and the exe constants. All 11 pass. Three rule mutations (grid centre term, phase map, plan bit) were applied at once: each failed only its own tests (4 failing, mapped by name), then reverted and green. The red was a compile failure on the missing module (written first, not run separately).

## 12. Open items

1. The side plan index writer (`setup+0xB34`) and the in-play rule `FUN_140500B90` → GF attack level (`+0xBCC`). Until then the rows use plan 0 (ponytail in `WriteSlotPoints`).
2. Restart override `f = 0` (`MI+0x116C`, `MI+0x1554`; GF has no restart-kind mapping), the corner chooser rows `FUN_1404A8620` and the set-piece rows `FUN_14051CF70`: not wired.
3. Lineup → sheet slot: assumed identity (INF); the roster lookup `FUN_140A3B000` is the PES mapping.
4. Phase = tag bits 20–23 (INF from `FUN_1408C5E30` call order).
5. `TeamFrameLateral` is unused on the PES path but tested (`pes_designation_test.cpp:147–150`): delete with its test (clean cutover).
6. **Landing blocker:** the Godot path (`pes_library` and `gf_match.cpp:81`) does not load the tables or sheet constants. A `--pes-loop` game run would hit the assert in `WriteSlotPoints`. Wire `setup.pesFormations / pesSheetConstants` there before merge.
7. M4 (plan-flag writer `S+0x1469+p`) and M5 (live read of one team) not done.
