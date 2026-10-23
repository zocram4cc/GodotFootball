# PES condition bands: byte-verified edges, writer chain, port spec (ConditionBandsRE, 23-10-26)

> **Main audit 23-10-26 (bytes):** `FUN_140AF2DA0`'s seven `cmp eax` immediates read from the image at 0x140AF2EAC-0x140AF2EED: 0x41, 0x46, 0x4B, 0x50, 0x55, 0x5A, 0x5F: match.

Parent: `docs/re/pes-re-condition-writer.md` (the writer chain) +
`docs/re/pes-re-condition-arrows.md` (consumer/table). This file closes open
item 2 (band edges) and narrows items 3–4 (+0x147 origin/RNG, edit↔install
link); +0xA298 state 3 is decoded to its writer.

Image: `pes_full.bin`, file offset = VA − 0x140000000. Bridge one request at a
time; capstone 5.0.7 for local decode. Checker:
`<workdrive>/gf-ConditionBandsRE-scratch/check_condition_bands.py` — 48
byte/float asserts, GREEN 23-10-26; one mutation run (0x46→0x47 at
0x140AF2EB3) goes RED on `band.lt70`, then reverted.

## 1. Band edges — CONFIRMED at the bytes

`FUN_140AF2DA0` ladder (disasm `af2da0.asm` lines 67–89; bytes script-read):

| VA | bytes | asm | band |
|---|---|---|---|
| 0x140AF2EAC | `83 F8 41` | `CMP EAX,0x41; JC →0` | b ≤ 0x40 → 0 |
| 0x140AF2EB1 | `83 F8 46` | `CMP EAX,0x46; JNC fwd` else 1 | b < 0x46 → 1 |
| 0x140AF2EBD | `83 F8 4B` | `CMP EAX,0x4B` … else 2 | b < 0x4B → 2 |
| 0x140AF2EC9 | `83 F8 50` | else 3 | b < 0x50 → 3 |
| 0x140AF2ED5 | `83 F8 55` | else 4 | b < 0x55 → 4 |
| 0x140AF2EE1 | `83 F8 5A` | else 5 | b < 0x5A → 5 |
| 0x140AF2EED | `83 F8 5F` | `CMP EAX,0x5F` | b < 0x5F → 6 |
| 0x140AF2EF0 | `1B DB / 83 C3 07` | `SBB EBX,EBX; ADD EBX,7` | else 7 |

So `band(b) = 0 if b≤0x40; 1..6 on <0x46/4B/50/55/5A/5F; else 7`.
Correction to the parent doc: the first edge is **`CMP 0x41 + JC`** (≤0x40→0),
not `CMP 0x40`. The remaining edges are `<imm>→k` with descending fallthrough.
`FUN_1414C6E90` case 0x30 stores it: `0x1414C7144 AND R8D,7 (41 83 E0 07)`,
`0x1414C7148 SHL R8D,0x1C`, `0x1414C714E OR [RCX+0x18],R8D` — i.e.
`param_1[6] = (param_1[6] & 0x8FFFFFFF) | ((v&7)<<0x1C)`. CONFIRMED.

The `{5F,5A,50,4B,46(,3C)}` table question is answered negatively: there is **no
in-memory table** — both ladders use immediates. `FUN_140922DD0` (stamina) uses
`CMP DL,imm` (`80 FA 05/5F/5A/50/46` at 0x140922E8C/E95/EA5/EB5/EC2,
script-read): `dl<5→clamp 5; <0x46→×0x6E; <0x50→×0x78; <0x5A→×0x82;
<0x5F→×0x8C; else ×100`-then-`/100`. The stamina edges {46,50,5A,5F} are a
subset of the band edges (minus 0x41/0x55, plus the <5 clamp); the prior doc's
"table" was the decompiler's rendering of these immediates. CONFIRMED (no table
exists; byte scan of .rdata for 5F 5A 50 4B 46 / reversed / with 3C: zero hits).

## 2. Inverse map + provider store — CONFIRMED at the bytes

`FUN_141FC1380` (`dis_141FC1380.asm`, 41 lines): default `MOV EBX,2`
(`0x141FC1386 BB 02…`); band 0 → `MOV EBX,4` (`0x141FC13E1`); 1 → 3
(`0x141FC13CD`); 3 → 1 (`0x141FC13B9`); 4 → `XOR EBX,EBX` (`0x141FC13A8 33 DB`);
2/5/6/7 fall to default 2. So `0→4, 1→3, 2→2, 3→1, 4→0, else→2`. CONFIRMED.
`FUN_1408C7710` = `MOV [RCX+0x58],EDX (89 51 58); RET`. CONFIRMED.
Only callers of `FUN_141FC1380` image-wide: `0x141FE3984`, `0x141FE4113` (both in
`FUN_141FE3830`). CONFIRMED.

## 3. Match-prep reads — CONFIRMED at the bytes

Site 1: `0x141FE3973 MOVZX R8D,[RAX+0x147] (44 0F B6 80 47 01 00 00)`,
`0x141FE397B AND R8D,7 (41 83 E0 07)`, `0x141FE3984 CALL 0x141FC1380`.
Site 2: `0x141FE4100 MOVZX R8D,[RSI+0x147]`, `0x141FE4108 AND R8D,7`,
`0x141FE4113 CALL 0x141FC1380`. CONFIRMED. `FUN_141FE3830` contains zero RNG
calls (no `140A924E0` in 795-line disasm — re-CONFIRMED): deterministic given
the edit record.

Slot indexer `FUN_1414C2170` byte-verified: `CMP EAX,2 (83 F8 02)`,
`LEA RAX,[RCX+0x1308]`, `CMP EDX,0x28 (83 FA 28)`,
`IMUL RAX,RCX,0x188 (48 69 C1 88 01 00 00)`, `ADD RAX,0x1308 (48 05 …)` —
`((slot+side·0x28)·0x188)+0x1308+base`, out-of-range → base+0x1308. CONFIRMED.

## 4. Consumer + team shift — CONFIRMED at the bytes (extends parent §1)

`0x14092B4D6 8B 58 64` (read record+0x64); `0x14092B4D9 E8 E2 56 11 00`
(call `FUN_140A40BC0`); `0x14092B4EF 83 F8 03`, `JZ 0x14092B584` (own-team
state==3 → `MOV EBX,R14D` = 0); second `CMP EAX,3` at `0x14092B53B` + `JZ`;
`0x14092B57B CMP EBX,4 (83 FB 04)`, `JGE`, `0x14092B580 INC EBX (FF C3)`;
`0x14092B595 MOV [RDI+0xC788],EBX`; `CMP EBX,5 / CMP EBX,2` skips (green ×1.0
writes nothing). New detail vs parent: the own-team gate ALSO requires
`FUN_140A40BD0`→`FUN_140A602E0(…,6)` false and reads side via `R12D^1`
(`0x14092B516 CMP R12D,2; JC …; XOR EDX,1`): the `edx=6` argument to
`FUN_140A602E0` (`*p/100==6`) is a per-team predicate, not a constant.
`FUN_140A602E0` = `return *param_1/100 == param_2` (decompile). INF: what the
`/100==6` code means (a team-status code whose 100s digit is 6).
`FUN_140A40BD0` = `+0xA29C ? zero : *(+0xA294)` into the predicate slot. CONFIRMED.

### +0xA298 state 3 — traced to its writer (was OPEN item 4)

- Predicate: `FUN_140A40BC0` = `MOV EAX,[RCX+0xA298] (8B 81 98 A2 00 00)`. CONFIRMED.
- Setter: `FUN_140A40BD0`→ next function `FUN_140A424D0` = `MOV [RCX+0xA298],EDX
  (89 91 98 A2 00 00)`. CONFIRMED. Sole caller: `0x141FBA4C5` in `FUN_141FB9FB0`.
- Value: `FUN_141FB9FB0` computes `uVar11 = FUN_141FC07B0(FUN_141030B40(lVar1))`
  where `FUN_141030B40` = `*(p+0x20)` and `FUN_141FC07B0` = identity map
  0→0,1→1,2→2,3→3 else 0 (decompile), unless `FUN_141FC1190` true (→0); then
  `FUN_140A424D0(lVar16,uVar11)`. So **state 3 ⇔ *(edit+0x20)==3** at that call
  (modulo the `141FC1190` veto). CONFIRMED (decompile; the two thunks are 1-line).
- Init default: `FUN_140A415A0` sets word `0x28A6` (=0xA298) to **4**. CONFIRMED (decompile).
- OPEN: what edit+0x20 enumerates (which menu/setup state is 3), and
  `FUN_141FC1190`'s veto. Next: decompile `FUN_141FC1190` + find writers of
  edit+0x20 value 3.

## 5. RNG function, stream, seed — CONFIRMED (mechanism); use OPEN

`FUN_140A924E0(obj,range,stream)` = `x = *(obj+8+4·stream)·(−0x2D22B2E3)+0x4513
& 0x7FFFFFFF; *(…) = x; return x·(1/2³¹)·range` with `1/2³¹` float at
`0x1426550D8` (`00000030` = 4.6566e-10, script-read). CONFIRMED.
Seed: `FUN_140A923F0` inits state (streams to 0x2F3D0..3, i.e. consecutive);
`FUN_141F51FD0` reseeds stream 2 from global `DAT_1437F5D88` (`FUN_140A928A0`
= `*(p+8+4·stream)=seed`). `FUN_141F7A100(obj,n)` = rolling accumulator
(`RNG(0x2AC)` into `obj+0x20`, returns `(acc·n)>>10`). CONFIRMED (decompile).
All 30 image-wide RNG-caller functions listed in scratch
(`rng_callers.txt`); **zero** also call the slot indexer `FUN_1414C2170`
(empty intersection), and no edit-region (0x1415xxxx) function calls RNG —
so the +0x147 roll is NOT a direct RNG→slot write. See §6.

## 6. The +0x147 byte origin — narrowed, still OPEN

Established (all CONFIRMED unless noted):
- `FUN_140AF2DA0` (edit-player build, 0x17C-stride array via `IMUL R8,0x17C`)
  reads condition byte b = `FUN_1414E2120(rec, 0xD, 2)` (thunk `0x1415130F0`),
  clamps 0x28..0x6D (`0x1414E22FC CMP R8B,0x28 (41 80 F8 28)`; `0x1414E2302 MOV
  R8B,0x28`; `0x1414E2320 MOV AL,0x46` default; `0x1414C7144…` band store),
  writes band field 0x30, then `FUN_1414C66C0(rec,0x2D)` → `+0x178`,
  cond byte → `+0x179`, then `FUN_14150EC40` overall → `+0x13C` bits, then
  `FUN_140AF06D0` copies the 0x17C record into the array. Two callers:
  `FUN_140AF1400` (0x17C-array loop) and `FUN_140AF0E50` (gated by player-id
  checksum `[0x14351AE30/34]` match; else `FUN_140AF2DA0` path). CONFIRMED.
- The condition byte itself is **deterministic**: mixer `FUN_141E5B540`
  (ability-weighted sum, `(Σw·v+0x32)/100`, clamp 0x28..0x6D, default 0x46) —
  no RNG inside. Its input-id table at `0x142B47AD0` (script-read: 13 ability
  ids 0xC..0x0 interleaved + 0xD twice, etc.) contains **no 0x2D (Form)**:
  Form does NOT enter the condition byte via the mixer. CONFIRMED (bytes for
  the table, decompile for the mixer).
- Form (Player.bin bit 438, 3 bits — verified on vanilla: 17,782 records,
  distro {0:1,1:79,2:931,3:7606,4:6000,5:2559,6:512,7:94}, peak 3–4) reaches the
  0x17C record only as the `+0x178` mirror via `FUN_1414C66C0` case 0x2D
  (`*p = param_1[7]&0x3F`, decompile). Its in-match reader is unfound (no
  0x17C-`+0x178` reader in the prep chain). INF: Form shapes the arrow only
  through paths not yet traced (possibly the `FUN_14150EC40` overall, which
  takes the cond byte — not Form — plus band; or UI-only).
- The 0x188 slot record's `+0x147` (low 3 bits = band) has **no direct writer
  found**: `FUN_140B158D0` (generic struct copy, 23 callers) carries +0x145..48
  bytes but is a copier, not a roller; the `0x1418E2246 MOV [RBP+0x147],AL`
  site zeroes (EAX=0) in an init block, not a roll. The band field 0x30 lives
  at byte 0x1B of the 0x17C record, so +0x147 is a *different* byte, written
  during team-slot build (0x141FCxxxx family is RNG-clean) or carried from
  save data.
- NEXT (narrowed): (a) decompile `FUN_141FC5500`'s `0x1414C2170`-slot uses
  (slot-record fill sites `0x141FC55CB/5823`); (b) scan stores to disp 0x147
  whose base traces to a `1414C2170` result — candidate pattern
  `88 85 47 01 00 00` has exactly 3 image hits (`0x1418E2246` init-zero +
  `0x151DFDE37, 0x154C247AB` unchecked); (c) check edit-save blobs for a
  persisted per-slot condition byte.

## 7. Edit record ↔ Player.bin link — CONFIRMED (mechanics), INF (flow)

- Dispatch `FUN_1414C66C0` case 0x2D = `param_1[7]&0x3F` (edit field 0x2D =
  low 6 bits of word 7); case 0x38 = `param_1[7]>>0x16&7` (a *different*
  3-bit Form-like view of the same word); cases 0x2E/0x2F = bytes 0/1.
  CONFIRMED (decompile). Accessor-table index 97 = thunk at `0x141F0FC60`
  (`85 D2 75 0A 8B 41 34 C1 E8 16 83 E0 07 …`, byte-verified) = Form view.
- Player.bin→edit flow: the 0x17C player-edit array is filled from Player.bin
  at edit-data build (`FUN_140AF0E50` region); the exact blit (Player.bin bit
  438 → edit word 7 bits) is NOT yet traced. INF: the blit site.
  GF reads Player.bin bit 438 today only for the port's Form input (new).

## 8. Exact port spec (GF fields)

PES arrow index p: 0=Top…4=Terrible; GF `Arrow`: 0=Terrible…4=Top ⇒ `g = 4−p`.
Band→PES map `kInvMap={4,3,2,1,0,2,2,2}`; band→GF map `{0,1,2,3,4,2,2,2}`.

1. `src/pes/playerdb.*`: add Form = `Field(rec, 438, 3)` (Player.bin id 0x2D,
   accessor `0x141F0FC60`, bit 438, 3 bits, raw 0..7; UI = raw+1 per
   matchlogic §1.2, fit 32/32) alongside 0x2E/0x0F — same `Field()` helper.
   Extend `PlayerTraits{…, int form}` (raw). No table transcription: read at
   runtime through the VFS (`build/apps/pesfmt_dump`, `src/pes/vfs*`).
2. `src/sim/data/formstate.*`: replace `ArrowForPlayer` splitmix draw with
   `pes = kInvMap[Band(condByte)]`, `Band(b) = b≤0x40?0 : b<0x46?1 : b<0x4B?2
   : b<0x50?3 : b<0x55?4 : b<0x5A?5 : b<0x5F?6 : 7` (edges §1, `// ponytail:`
   naming `0x140AF2DA0`/`0x141FC1380` until inputs arrive through the VFS);
   GF-space: `band→{0,1,2,3,4,2,2,2}`. `condByte` input: the mixer output
   (§6) — until the +0x147/blt path lands, feed it from Form via the vanilla
   distro (§6) behind the same `// ponytail:`.
3. `Team::InitPlayers` + apply path (`match.cpp`): store per-player arrow;
   replicate the shift — own-team `+0xA298==3 → index 0` (predicate
   `0x140A40BC0`; setter `0x140A424D0` from `0x141FB9FB0`; `// ponytail:` until
   edit+0x20==3 semantics land), opponent `min(arrow+1,4)` (`0x14092B57B/580`
   `CMP EBX,4;JGE;INC EBX`); skip stat writes for index 2 and ≥5
   (`0x14092B59B/5A0`); table base `0x142632D70`, index `5·tier+arrow`, note
   PES index = `4 − GF_Arrow` when reading it at runtime.
4. Keep GF `kArrowTable` (matches rows re-verified in the parent doc; tier-1
   1.12/…/0.84 etc.).

## 9. Status ledger

- CONFIRMED: band edges (§1); no-table verdict; inverse map; provider+0x58
  store; both +0x147 read sites + `&7`; slot indexer math; consumer gates incl.
  `+0x64` read / `+0xC788` store / green+OOR skips / `inc` cap; `+0xA298`
  predicate/setter/init-4/state-3-via-edit+0x20 chain; RNG mechanism + seed
  path; mixer determinism + no-Form-in-mixer; Form bit 438 + vanilla distro;
  dispatch cases 0x2D/0x38/0x2E/0x2F; thunk bytes; floats 90/100/2⁻³¹.
- INF: `/100==6` team predicate; `+0x178` (Form mirror) in-match reader;
  Player.bin→edit-word-7 blit; `FUN_141FC1190` veto.
- OPEN: (1) +0x147 byte writer + RNG stream/seed into it (narrowed §6);
  (2) edit+0x20==3 meaning; (3) `FUN_14089BAE0`'s actual pipeline role
  (killed as arrow source only).
