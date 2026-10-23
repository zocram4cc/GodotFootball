# PES condition arrows: the writer chain (ConditionRE, 23-10-26)

> **Main audit 23-10-26 (bytes):** `0x14092B4EF cmp eax, 3`, `0x14092B580 inc ebx` (the opponent's one-step-worse shift), `0x140922D00 mov [rdi+0x64], eax` (the record store): match. The agent's own checker was specified but not written (budget); the band edges in §4 are decompile-only and must be script-read before a port.


## Objective/scope
Find the writer of record+0x64 (arrow 0..4), its caller chain, inputs, band thresholds, RNG stream/seed; rule on `FUN_14089BAE0`; confirm the apply-time team shift at the bytes; locate form/randomness inputs in install data; give the GF port spec. Read-only; image `pes_full.bin` (VA−0x140000000=file off); bridge one-request-at-a-time; scripts in scratch (paths below are scratch-side).

## Findings

### 1. Consumer + team shift — CONFIRMED at the bytes
`FUN_14092AF10`, param_2 = record (`0x14092AF5F mov [rsp+0x28],rdx`):
- `0x14092B4D6 mov ebx,[rax+0x64]` (rax=[rsp+0x28]). Single plain-dword read.
- `0x14092B4D9 call 0x140A40BC0` (=`return *(param+0xA298)`); `0x14092B4EF cmp eax,3`; `0x14092B4F2 je 0x14092B584`; `0x14092B584 mov ebx,r14d` (r14d=0, xored at entry). Own-team state==3 forces arrow index **0** (table row head = 1.12 = Top).
- Opponent path: `0x14092B536 call 0x140A40BC0`; `0x14092B53B cmp eax,3`; `0x14092B53E je 0x14092B57B`; `0x14092B57B cmp ebx,4`; `0x14092B57E jge`; `0x14092B580 inc ebx`. Opponent shifts **one step worse, capped at 4**.
- Store `0x14092B595 mov [rdi+0xC788],ebx`; skips `0x14092B59B cmp ebx,5;jge` and `0x14092B5A0 cmp ebx,2;je` (green ×1.0 writes nothing).
- Table `0x14092B5D4 mov eax,[rbx+rcx*4+0x2632D70]` → base `0x142632D70`; index `5·tier+arrow` (`0x14092B5C7 lea rax,[r11+rcx*4]; add rcx,rax`); tier/stat list lea `0x14092B5A8 [rip+0x1D07805]` → `0x142632DB4`. Matches prior doc; rows re-verified there.

### 2. `FUN_14089BAE0` is NOT the arrow roll — killed (CONFIRMED)
Decompile: writes only `+0x328/+0x32C/+0x330` (from bases `+0x20/+0x24/+0x28`), randomness byte `+0x44`, RNG `FUN_140A924E0` stream 2, `value=base·(100−r+RNG(100)·r·0.02)/100`. Never writes `+0x64` or any band. Sole caller `FUN_14089D690` (player pipeline init; its callers are match-pipeline, e.g. `0x142176970` family) — disjoint from the `0x140925470→0x14092AF10` consumer chain, which never calls it. It is per-frame stat jitter/init, not the arrow.

### 3. Writer chain — CONFIRMED (record ← provider ← player ← edit loop)
Per-player chain (param_1+0x18 = squad index; `0x140925750` iterator bumps `[rcx+0x10]` then calls `0x140925470`):
- `FUN_1404239F0` loop: `uVar1=*(lVar5+100)` (player struct from `FUN_140A489F0`, +0x64) → `FUN_1408C7710(uVar6,uVar1)`; `FUN_1408C7710` = `*(param_1+0x58)=param_2` (provider arrow store, 3-line setter).
- `FUN_140922BC0`: copies provider bytes `+0x30..+0x120` to stack (`movups` run `0x140922C63–0x140922CD8` = 0xF0 bytes from `FUN_141B59E20`=`a3af60+0x30` result), then `0x140922CF1 mov eax,[rsp+0x78]`; `0x140922D00 mov [rdi+0x64],eax`. So record+0x64 = *(provider+0x30+0x58) = **provider+0x88**.
- Root: loop in `FUN_141FE3830` (two sites: `~0x141FE3984`, `~0x141FE4113`) does per (side 0..1, slot 0..0x27): `band=[editrecord+0x147]&7` → `FUN_141FC1380(provider,side,band)` → provider+0x58. Edit record = `FUN_1414C3540`→`FUN_1414C2170(base+0x35960,side,slot)` = `base+((slot+side·0x28)·0x188)+0x1308`. Provider = `FUN_1408C6560(base,side,slot)` = `base+side·0x3A10+slot·0xF0+0x68` (so +0x58 lands at base+side·0x3A10+slot·0xF0+0xC0).
- `FUN_141FC1380` inverse map (decompile, CONFIRMED): `0→4, 1→3, 2→2, 3→1, 4→0, else→2`.

### 4. Band thresholds — INF (decompile only, NOT byte-verified)
`FUN_140AF2DA0`: condition byte b (`FUN_1414E2120`, field 0xD, clamped 0x28..0x6D) → `≤0x40→0; <0x46→1; <0x4B→2; <0x50→3; <0x55→4; <0x5A→5; <0x5F→6; else 7`, written to edit field 0x30 (`FUN_1414C6E90` case: `param_1[6]=(v&7)<<0x1C`). Do NOT port edges until the `cmp` immediates are script-read (open byte check).

### 5. RNG/stream/seed — INF + one CONFIRMED negative
`FUN_141FE3830` contains **zero** `FUN_140A924E0` calls (grep count 0): the match-prep arrow write is deterministic given the edit record. The roll/randomness enters upstream (`+0x147` byte origin = edit-data creation path; all 4 image writers found are struct copies). Seed/stream: OPEN. (`FUN_140CA1A50` in the adjacent `92FAD0` path is a stub returning 1 — that checksummed `a592c0` path always takes the `iVar15=0-or-iVar7` branch; it never touches plain +0x64.)

### 6. Form/randomness inputs in install data — INF
- Player.bin field 0x2D (Form, UI raw+1): accessor `0x141F0FC60` = `[rcx+0x34]>>0x16&7`; generic dispatch `FUN_1414C66C0` case `0x2D` = `param_1[7]&0x3F`; setter case `param_1[7]=(v&0x3F)`. Accessor-table index 97 (id 0x2D at 96, fn at 97).
- But **no** in-match-engine (0x1408xxxx–0x140Axxxx) caller reads field 0x2D at match prep (147 `edx=0x2D` dispatch sites image-wide; only `0x1408E12F6` UI wrapper and `0x140AF2F1B` edit UI are near). The arrow path reads the **edit record** (field 0xD byte via `0x1414E2120`, band field 0x30, mirror bytes +0x178/+0x179), not Player.bin directly. Edit→Player.bin linkage: OPEN.

### §0 Script-read constants (pasted tool output)
- `0x14259BE5C` = 90.0 (`0000B442`): operand of `movss xmm1,[rip+0x1C7915C]` at `0x140922CF8` (next `0x140922D00` + disp). NOTE: my first computation used VA−1 (`…B5B`); corrected — the `…B5B` bytes (`420000B4`, ≈−1.19e−7) were my misalignment artifact, not a constant.
- `0x14253EE30` = 100.0: divisor of `divss` at `0x1420ED9D1` (condition-normalize in `0x1420ED7C0`: `byte[provider+0x5D]/100.0`).
- `FUN_140922DD0` stamina-formula floats: `0x1425CB004`=2000.0, `0x1425CB498`=4000.0, `0x1425CB4A0`=8000.0, `0x14260B390`=6000.0, `0x14259D638`=16000.0 (adjacent stamina use of the same provider bytes +0x35/+0x58/+0x5d — confirms byte roles: dl=[rax+0x35] form-ish, edi=max([rax+0x5d],10) condition, r8d=[rax+0x58] arrow; bands `cmp dl,5/0x46/0x50/0x5A/0x5F` with `imul ×0x6E/0x78/0x82/0x8C /100`).

## Spec/interface defined
Arrow dataflow (all VAs above): `editrecord+0x147&7 → band(7 edges) → inverse map → provider+0x58 → player+0x64 → record+0x64 → team+0xC788 → table[5·tier+arrow] → stats+0xC888`. PES arrow index 0=Top…4=Terrible; GF `Arrow` is 0=Terrible…4=Top, so `gf = 4 − pes`; band→GF map = `{0,1,2,3,4,2,2,2}`.

## Code changed
None (read-only decode task; no repo writes).

## Tests run
None (no code). Byte spot-checks done via local disassembly scripts only.

## Port spec (which GF fields)
1. `src/pes/playerdb.*`: add Form field 0x2D (bit 438, 3 bits, raw; UI=raw+1) alongside 0x2E/0x0F — same `Field()` helper.
2. `src/sim/data/formstate.*`: replace `ArrowForPlayer` splitmix draw with `pes_arrow = kInvMap[Band(condByte)]`, `Band` = 7-edge ladder (edges INF — verify at bytes first), `kInvMap={4,3,2,1,0,2,2,2}`; `// ponytail:` naming the PES sites (`0x141FC1380`, `0x140AF2DA0`) until inputs arrive through the VFS. GF-space equivalent: band→`{0,1,2,3,4,2,2,2}`.
3. `Team::InitPlayers` + apply path (`match.cpp`): store per-player arrow; replicate shift — own-team state==3 → index 0, opponent `min(arrow+1,4)` (`// ponytail:` until `+0xA298`-state-3 semantics decoded).
4. Keep GF `kArrowTable`; note PES index = `4 − GF_Arrow` when reading `0x142632D70` at runtime.

## Checker (spec — file not written, tools cut)
`check_condition_writer.py`: assert `pes_full.bin` bytes: `8B 5C 24 28 / 8B 63 64` at `0x14092B4D6` region; `E8 …(→0x140A40BC0)` at `0x14092B4D9`; `83 F8 03` at `0x14092B4EF`; `FF C3` at `0x14092B580`; `C7 41 64`-free `89 47 64` (`mov [rdi+0x64],eax` = `89 47 64`) at `0x140922D00`; `F3 0F 10 0D 5C 91 C7 01` at `0x140922CF8`; float words above; `BA 2D 000000` absent within ±0x800 of `0x140925470/0x14092AF10` (no Player.bin-form read at prep). Mutations: flip each `cmp` edge / map entry → checker red.

## Open/incomplete (numbered)
1. Caller of `FUN_140922BC0` and callers above `FUN_141FE3830` (`0x141FE2208` in `FUN_141FE20A0`; higher chain untraced).
2. Band edges not byte-verified (`cmp` immediates in `0x140AF2DA0`/`0x140B58xxx`); the in-memory `{5F,5A,50,4B,46(,3C)}` table's role vs immediate edges unresolved.
3. `+0x147` byte origin + RNG seed/stream (writer deterministic; randomness upstream in edit-data path).
4. `+0xA298` state-3 and `a40bd0/a602e0` predicate semantics.
5. `FUN_14089BAE0`'s actual consumer role (stamina/jitter pipeline) — only killed as arrow source.