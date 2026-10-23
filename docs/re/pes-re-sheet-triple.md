# PES team sheet: the sheet triple, its writers, and the install source of its formation records (SheetTriple, 23-10-26)

Image `pes_full.bin` (file offset = VA − 0x140000000). Sheet `S` = `T+0x8E08`. Labels: **CONFIRMED** = read from bytes or a decompile and checked by the checker; **INF** = inference; **OPEN** = not decoded, blocker named.

**Status.** The triple accessor, the per-tick rows writer, the restart variant, both sheet writers and the install source are decoded and checked. The port is **not made**: the install formation record is not yet bound to a side's plan (M1), and the record-to-world mapping is not decoded (M3). No GF code changed, so no repro hash moves.

## 0. Layout of S (CONFIRMED, offsets chain exactly)

| offset | content | accessor (VA) | verdict |
|---|---|---|---|
| +0x00 | `plan` u32 (0..2; index 3 = "current" = `S[0]`) | `FUN_1408C50E0` (`arg==3` reads `S[0]`) | CONFIRMED |
| +0x28 | 99 × u32 registered role (key), idx = `3·(11·plan+slot)+f` | `FUN_1408C51F0` reads; `FUN_1408C5920` writes | CONFIRMED |
| +0x1B4 / +0x268 | 90 × u16 outfield x / y, `(10·plan+i)·3+f` | `FUN_1408C5130/5170` read; `FUN_1408C5D50` writes | CONFIRMED (layout), names from pes-re-tactics.md |
| **+0x31C** | **99 × 12 B: the sheet triple (record A)**, `(f0,f1,f2)` per idx | `FUN_1408C50E0` reads; `FUN_1408C5920` is the only writer | CONFIRMED |
| +0x7C0 | 33 × u32, not modelled | — | OPEN (unused by this chain) |
| +0x844 | 33 × u32 slot targets | `FUN_1408C50A0` | CONFIRMED (identity) |
| +0x8C8 | 99 × u32 role table (default 0xC) | `FUN_1408C5310` reads; `FUN_1408C5E90` writes | CONFIRMED (identity) |
| **+0xA54** | **99 × 12 B: record B, the source of record A** | `FUN_1408C5E30` is the only writer | CONFIRMED |
| +0xEF8 | instruction records (4 per plan, 0x70 each) | `FUN_1408C6430` | doc |
| +0x1469+plan | u8 plan flag (ctor 0) | read by `FUN_1408C5920`; writer INF = `FUN_1408C5EF0` | read CONFIRMED, writer INF |

Identities checked: `0x28+4·99=0x1B4`, `0x1B4+2·90=0x268`, `0x268+2·90=0x31C`, `0x31C+12·99=0x7C0`, `+4·33=0x844`, `+4·33=0x8C8`, `0x8C8+4·99=0xA54`, `0xA54+12·99=0xEF8`.

## 1. Triple accessor `FUN_1408C50E0` (VA 0x1408C50E0), CONFIRMED by bytes

`(S, slot, f, kind) → u32*`. If `kind==3`, `fam = S[0]`. Valid iff `fam ≤ 2` (unsigned, `ja` at 0x1408C50F0), `slot < 11` (`jge` at 0x1408C50F5), `f < 3` (`jge` at 0x1408C50FB). Valid: returns `S + 0x31C + 12·idx`, `idx = (3·(11·fam+slot)) + f`. **Invalid: returns `S + 0x31C` (record idx 0; PES quirk).** The record-A address is `LEA RAX,[RDX*2+0xC7]` at 0x1408C5114.

## 2. The rows writer `FUN_1404F5F10` (VA 0x1404F5F10), CONFIRMED by bytes

Args `(P, S, MI, F)`. `f = 2 − (F≠0)` (`neg r9b; sbb; add 2`). If `MI+0x116C ≠ 1 && MI+0x1554 == 1`, `f = 0` (`cmp/cmove`, 0x1404F5F38–0x1404F5F4A).
For `k = 0..10`:
- `role = FUN_1408C51F0(S, k, f, 3)`; `FUN_140A425A0(P,k,role)` → `P+0x3F8C+4k`; `FUN_140A42590(P,k,role)` → `P+0x3FB8+4k`.
- `t = FUN_1408C50E0(S, k, f, 3)`: `P+0x3A9C+12k ← (t0,t1,t2)` (`f0` at +0x3A9C, `f1` at +0x3AA0, `f2` at +0x3AA4) and `P+0x3B20+12k ← (t0,t1,t2)` (+0x3B20/+0x3B24/+0x3B28). Anchors at 0x1404F5FC4–0x1404F5FFA.

`f2` is the lateral key (the designation sort reads it, pes-re-team-layer2). `f0` is the protected-scan score (pes-re-blockA-forward-pull §3).

**Restart variant `FUN_1404A8620`** (VA 0x1404A8620, corner chooser): the same loop with `f = 0` as a literal (`xor r8d,r8d` at 0x1404A87F6 and 0x1404A8825). CONFIRMED.

Callers of `FUN_1408C50E0`: FUN_1404A8930, FUN_14051CF70, FUN_14051CE50, FUN_140A59F90 (×2), FUN_1404A90D0, FUN_1404A93A0, FUN_1404A8620, FUN_1404F5F10 (xrefs).

## 3. Record A writer `FUN_1408C5920` (VA 0x1408C5920), CONFIRMED by bytes

Per plan `p` (`S[0x1469+p]`):
- flag ≠ 0: `A[p][s][f] = B[p][s][f]` for every slot and phase; key `S+0x28[idx] = S+0x8C8[idx]` when ≤ 12.
- flag = 0: `A[p][s][f] = B[p][s][0]` for `f = 0..2`; key = role of phase 0.

Store sites: A copy at 0x1408C5A67–0x1408C5A99 (set branch) and 0x1408C5BA0–0x1408C5BC2 (phase-0 branch). Callers: FUN_1404F5DB0 (0x1404F5E6A), FUN_141FD6190 (0x141FD6715), FUN_141FBAC10 (0x141FBB04C), FUN_1408C5F10 (0x1408C6423), FUN_141FCF130 (0x141FCF790), FUN_141FDAA80 (0x141FDB010). Frequency per tick: INF (FUN_1404F5DB0 is the caller at the team-tick area; OPEN).

## 4. Record B writer `FUN_1408C5E30` (VA 0x1408C5E30), CONFIRMED

Writes 12 B to `S + 0xA54 + 12·idx` (`lea rcx,[rdx*2+0x295]` at 0x1408C5E64). Its only caller is the setup builder `FUN_141FC1590` (0x141FC19E6).

## 5. Setup builder `FUN_141FC1590` (VA 0x141FC1590, 21 KB), decompile-level (INF where noted)

- Loop over plan `p` (outer, 0..2), slot `s` (0..10), phase `f` (inner, 0..2).
- Outfield `s ≥ 1`: `(x,y)` bytes from the setup block (`FUN_1414C6180`, a u16 at the entry); `FUN_1408C5D50(S, s−1, x, y)` (coords); record B = `FUN_14051B3F0(F, {x,y})`. INF: `x` is byte +1 and `y` byte +2 of a 3-byte `(role,x,y)` entry.
- Keeper `s = 0`: record B = `(−50.0, 0.0, 0.0)`; the immediate `0xC2480000` is at VA **0x141FC1957** (bytes `c7 45 88 00 00 48 c2` at 0x141FC1954, CONFIRMED).
- Roles: `FUN_1414C61D0` → `FUN_1408C5E90` (the +0x8C8 role table).
- Inputs: setup struct `DAT_1436F38A0` (`FUN_1408E1AF0`); side block `setup+0x14+side·0x248`; plan `setup+0xB34+side·0xA8` (`FUN_1408E16D0`); each plan record is 0xA0 bytes with a 99-byte `(slot,phase)→(role,x,y)` table at +4 (`FUN_1408E19B0`). `F` = the pitch pair (INF: FieldInfo F0/F1).
- Callers: FUN_141FD6190 (0x141FD66FB), FUN_141FBAC10 (0x141FBAF73), FUN_141FCF130 (0x141FCF719), FUN_141FDAA80 (0x141FDAFF3).

## 6. `FUN_14051B3F0` (VA 0x14051B3F0): record-B mapper

Output `(x, 0, z)` with `out[1]` zeroed at entry and never written (CONFIRMED by bytes at 0x14051B462–465, stores at 0x14051B720 and 0x14051B856). `x` comes from `param_3[0]`, `z` from `param_3[1]`, scaled by `F[0]`, `F[1]`. Image constants read by the checker (float32): `0x14259BDF0 = 7.0`, `0x1425656C0 = 4.0`, `0x14259222C = 5.0`, `0x14259BDEC = 6.0`, `0x14258C264 = 3.0`, `0x14259BDFC = 11.0`, `0x14259BE04 = 12.0`, `0x1425C5948 = 13.0`, `0x1425C48B8 = 14.0`, `0x142565600 = 0.5`; vectors `0x1425C5CC0 = (7,8,9,10)`, `0x1425C5CD0 = (15,15,15,15)`. The grid arithmetic (7 cells × 15 steps, the `/15` split) is **OPEN** (M3).

**Consequence (CONFIRMED):** record B's dword 1 is always 0, so `f1 ≡ 0` and `P+0x3AA0` and `P+0x3B24` stay 0.

## 7. Install source: `common/etc/pesdb/Tactics.bin` and `TacticsFormation.bin`

Extracted with `pesfmt_dump cat common/etc/pesdb/<name>` (PES21 install; sizes 15,912 and 525,096 B).

- **Tactics.bin** = 1326 × `{u32 tacticsId, u32 teamId, u32 hash}`. Every team has exactly 2 records (663 teams). Team 1 → records 80 (id 0x1B50) and 81; team 2 → 86 and 87 (CONFIRMED, checker).
- **TacticsFormation.bin** = 1326 groups × 396 B = 33 × `{u32 id, u32 role, u32 packed}`. Group `g` belongs to Tactics record `g` (the id matches for all 1326; CONFIRMED).
  - `packed` bits 20–23 = phase tag (= segment index 0..2; all groups);
  - bits 16–19 = slot ordinal (= position 0..10; all groups);
  - bits 8–15 = `y` (< 105 for all); bits 0–7 = `x` (< 49 for all);
  - `role` ≤ 12 for all. Roles 0..12 all occur.
  - 1310 of 1326 groups have three identical phase segments; 16 differ by phase (INF: phase-dependent formations).
- This **reverses** the earlier wave's reading of these files as an index or bitmask (pes-re-setpiece-referee §1.11). The structure matches the sheet's own bounds (x < 49, y < 105, role ≤ 12) and the 33-entry = 3 phases × 11 slots shape of the sheet's 99-cell table. INF (high): each group is one tactics id's formation table; the value semantics are confirmed only by the bounds and the symmetry below.

**Real values, group 80 (tactics 0x1B50, team 1), phase 0** (phases 1 and 2 identical):

| slot | role | x (depth) | y (lateral) |
|---|---|---|---|
| 0 | 0 (GK) | 3 | 52 |
| 1 | 1 (CB) | 11 | 64 |
| 2 | 1 (CB) | 11 | 40 |
| 3 | 3 | 13 | 88 |
| 4 | 2 | 13 | 16 |
| 5 | 5 | 23 | 64 |
| 6 | 5 | 23 | 40 |
| 7 | 7 | 31 | 84 |
| 8 | 6 | 31 | 20 |
| 9 | 12 (CF) | 43 | 64 |
| 10 | 12 (CF) | 43 | 40 |

The lateral values pair symmetrically about 52 (64/40, 88/16, 84/20), which fits a real formation.

## 8. Other constants

`FieldInfo F0 = 52.5599976` (bits 0x42523D70) and `F1 = 34.0599976` (bits 0x42083D70), from pes-re-fieldinfo §1 (OBS). The checker asserts these bits. The builder's binding of `F` to FieldInfo is INF.

## 9. Open items (exactly what blocks the port)

- **M1 (blocker): which tactics record fills which side's plan.** The side plan records (`setup+0x14+side·0x248+plan·0xA0`) have no writer found yet. Decoded and excluded: `FUN_1404DD7F0` (VA 0x1404DD7F0, 139 lines) initialises six 16-byte entries per side at `setup+0x4A8+side·0x340` (defaults 0xFF via `FUN_1408E33D0`, stored by `FUN_1408E33F0`), calls `FUN_1404DDC60(side)`, and reads three menu choice objects `choice_0/1/2` (strings `0x1425B6AA0/AB8/AD0`, defaults `0xC0002`, `0x100025`, `0x6C005B` via `FUN_140EA39C0`); it does not write the 0xA0 plan records. `FUN_1404DDC60` (79 lines) reads the setup `+0x4A8` block, not the plan records. Still open: `FUN_141FD6190` lines 240–279 (0x80-stride copies into a setup block) and `FUN_141FBAC10` lines 100–140 (per-side loop, 0x68 stride, source `local_118`). The DB reads (table ids 23/24, `FUN_14125F290` / `FUN_14125F2C0`) are not yet traced into the setup.
- **M2 (blocker): the transform** from a group entry `{role, packed x, packed y}` into the side plan's 99-byte table. INF: identity on `(role,x,y)`; to be confirmed in M1's writer.
- **M3 (blocker for values): `FUN_14051B3F0` arithmetic.** The constants are read (§6). The derivation of `(x_w, z_w)` from `(x,y)` and `F` is structurally decoded, not yet written as a formula.
- **M4: plan-flag writer** `S+0x1469+plan` (INF: `FUN_1408C5EF0`) and the sync's frequency (`FUN_1404F5DB0`).
- **M5 (verification): live read** of record A/B for a running team against group 80/81 (not done; the shared process is PES2021.exe, PID 2510857, read-only).

Port order after M1–M4 (for the next ticket): an importer for Tactics/TacticsFormation through the VFS (install-gated tests); `FUN_14051B3F0` and `FUN_1408C5920` ports; the rows into GF's existing input fields (`pes::forward` `lineupScore` = f0, `y` = f2); then replace the stand-in at `teamAIcontroller.cpp` ~3116–3118 on the PES path only.

## 10. Checker

Scratch checker (not committed): 17 byte anchors (VA → raw bytes, objdump-verified), the layout identities, all 1326 groups (structure, team pairs), and the 10 image constants. `RESULT OK`. `--selftest` catches each of 5 mutation classes (anchor byte, layout offset, tag bit, role range, x range, constant). `SELFTEST OK`.
