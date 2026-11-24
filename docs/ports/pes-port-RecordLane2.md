# PressLane (RecordLane2): press tick 0x140928FC0 enters the translated layer (RO-table holds), 24-11-26

Scenario claimed: **M5** (translation record). Input declarations only (`tools/dev/translate/ro_tables.json`), tool-regenerated layer; no hand edits to generated code; no tooling changed, so **M6 is not claimed**.

Branch `PressLane`, worktree `<workdrive>/gf-PressLane`. Merged `origin/Integrate` c7fa9d01d first (fast-forward; PressLane 9fb64855 was its ancestor). Main's planner patch `ro_tables_planner_window.patch` applied unchanged.

Commit: **d4fdb7f5ff** (`PressLane: declare the press tick's RO tables; 0x140928FC0 enters the layer (M5)`), pushed to `origin/PressLane` (ls-remote confirmation in the final message).

## Result

- **Live RO reads of the 11 tick-closure functions (LIVE_RO scan, `xlate.translate_one` per function): 31 at start, 0 now.** Scan script: `<workdrive>/gf-PressLane-scratch/ro/scan.py`.
- **0x140928FC0 is no longer held out.** It is committed at `src/pes/translated/gameplay/pes_fn_140928FC0.cpp`.
- Layer: 10,999 -> 11,191 functions (4,161 recorded bit-exact, 7,030 closure). The +192 are new closure members that the tick now reaches; they are untracked before the commit and committed now.
- Drift in tracked committed code (every changed committed file; recorded status unchanged, text changed only where noted):
  - `src/pes/translated/trapchain/pes_fn_1406E7BC0.cpp`: window `0x1425E3540` grows 1024 -> 4912 bytes (merged with the planner window). Same reads, same VAs; only the baked extent changed.
  - `src/pes/translated/trapchain/pes_fn_1406EA840.cpp`: window `0x1425E4700` (708 bytes) replaced by `0x1425E3790` (4660 bytes, the planner window). Same VAs, same bytes.
  - `src/pes/translated/pes_dispatch_gen.cpp` (+384) and `translated.hpp` (+192): index files, new entries for the new closure members.
- Recorded functions: the regen re-translated every recorded function; the regenerated text of every recorded function is committed. Only the two trapchain functions above changed text among committed functions. Main measures the ratchet on Integrate.

## Declarations (VA cites)

Each record gives: table, extent, read sites, index source, bound. Evidence strings are in the JSON.

| Table | Bytes | Read sites | Index source | Bound (CONFIRMED) |
|---|---|---|---|---|
| 0x1425DD460 | 0x400 | 0x1406B7CEA (B7560) | `MOVSX rax, al` of a call result | signed byte, -128..127 (floats at DD660 + 4s). Also covers C0170 |
| 0x1425DD950 | 0xA | 0x1406BB037 (BA900) | `[rbp+0xC0]`: vt+0x60 result (0x1406BAEAC), 9 (0x1406BB148), 8 + SETA (0x1406BB1B7) | 0..9 |
| 0x1425EE990 | 0x90 | 0x14080A326, 35E, 37F, 388, 39E (0x140809070) | `0x1406E6BF0(rcx,100)` mod 12 | 0..11 (12-byte rows) |
| 0x1425EEA20 | 0x18 | 0x14080A3D0, 3D9, 3E7 | `0x1406E6BF0(rcx,2)` | 0..1 |
| 0x1425EEA38 | 0x24 | 0x14080A452, 45B, 469 | `0x1406E6BF0(rcx,3)` mod 3, or `CMOVAE ebx, r12d` (0x14080A447) with r12d = 0 | 0..2 |
| 0x1425EEA68 | 0x24 | 0x14080A625, 6DD, 6EC | `0x1406E6BF0(rcx,100)` mod 3 | 0..2 |
| 0x1425EEC80 | 0x8 | 0x14080BBD4 | `0x1406E6BF0(rcx,0x3E8) & 3` | 0..3 (words) |
| 0x1425E91E0 | 0x34 | 0x140781C2F, 0x140781ED7 | `0x1406E6BF0(rcx,2)` / `(rcx,0xB)` | 0..1 at E91E0, 0..10 at E91E8 |
| 0x1425EFBE0 | 0x10 | 0x140842C6D (0x140842BE0) | `r8d` from 4 callers: ebx < edi (edi 0..2), 0, ebx < 3, 3 | 0..3 |
| 0x1425EFC18 | 0x20 | 0x140844E00 (0x140844CB0) | `min(0x140842D20 result, r14d)`, r14d = 7 | 0..7 |
| 0x14263EC50 | 0x1A0 | 0x140A06F4B, F5A, F6A (0x140A06DD0) | three row tables EC60 (ebx 0, 17 rows), ED70 (ebx 1), EDB0 (ebx 3); ebx from 0x140A06FD0, 2 excluded | reads in EC60..EDF0; declared wider inside .rdata |

Helper facts (CONFIRMED by decode of the image bytes):
- `0x1406E6BF0(rcx, n)` returns `|draw| mod n` (BTR 31 at 0x1406E6C27 makes it non-negative; `n = 0` divides by 1), i.e. `0..n-1`.
- vt+0x60 of the `+0xac0` anime table (vptr 0x1425E35B0; slot VA 0x1425E3610 = 0x1406B9B20) returns 0 or a value of `[0x1425DDF10 + 4(ecx-1)]` (values `9,8,7,6,5,0,1,2,3,4`), so 0..9. It is called with r8d = 10 at 0x1406C0170 and 0x1406BA900.
- `r12d` at 0x14080A447 (EEA38): zeroed at 0x14080908B. The only path to the case block 0x14080A046 is entry 35 of the dispatch at 0x1408091AE (dword 0x80CE48 in table 0x80CDBC; no other dword or qword in the image reaches 0x14080A046). No jump enters 0x140809070..0x1408091AE. No `r12` write lies on any path from 0x14080A046 to 0x14080A447. So `r12d = 0` there.

## Verification

- `gen_translated.py --image pes_full.bin` (regen): exit 0, 11,191 functions written.
- `gen_translated.py --check` before the commit: 192 drifts, all untracked closure members the commit adds. On the pushed tip d4fdb7f5ff (after the commit): `11191 functions (4161 recorded bit-exact, 7030 closure) regenerate byte-identical`, EXIT 0.
- PII: grep of every changed or new file for `<workdrive>` and `<home>`: no match. `tools/check_pii.sh` not run (the brief says run it from the main checkout; left for Main).
- Build and test: none (per the verification split). Gate: not run. ALiveBall and PesStallQuick: not measured by me; Main measures on Integrate.

## Open items

1. **CD90 spec gap (not landed; M6 scope, needs a tooling change).** My first reading was wrong: CD90 does read `rdx`, `r8d`, `r9d` at entry (0x14063CD93 `MOV [rax+0x20], r9d`, 0x14063CD97 `MOV [rax+0x18], r8d`, 0x14063CD9B `MOV [rax+0x10], rdx`, 0x14063CE36 `MOVSXD rdi, r9d`). The spec already has `scalar_rdx`, `scalar_r8`, `scalar_r9` inputs (`tools/dev/diffharness/specs/core/14063CD90.json`). Real gap: `args` lists only `rcx -> A0`; `rdx` is a record pointer (stored, then dereferenced) and needs a region root (`A1`), while `r8d` and `r9d` are signed int32 (`MOVSXD`, dword stores) but their schema is generic `u8 0..255`. Fix: specgen's `ROOT_INPUT_DOMAINS` / args for CD90, then re-diff CD90 alone. Not done.
2. The tick is in the layer, but its closure's diff status is the harness's. Main must check the tick's record (`gen --check` on the tip and the diff of 0x140928FC0's closure) before the M5 landing.

## Scratch

`<workdrive>/gf-PressLane-scratch/ro/`: `scan.py` (LIVE_RO per function), `dump.py` (disassembly with rip operands resolved), `callers.py` (direct E8/E9 callers), `0x*.asm` dumps, `regen1.log` (cancelled first regen), `regen2.log` (final regen), `check1.log` (pre-commit check: untracked drift only), `check2.log` (post-commit check).
