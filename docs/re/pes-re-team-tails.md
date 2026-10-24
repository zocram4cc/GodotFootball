# PES team-tick tails: T6b source (step-26c R8), T9/T10 open calls (TeamTailsRE)

> **Correction (Main, 23-10-26):** `TM+0x23F7C`'s writer is **not** unbound: it is `FUN_14045B420` through the split displacement 0x228D4 + 0x16A8 (`FUN_1404587F0`), decoded in `pes-re-mark-table.md` and ported in GF's `ScoreRows` (the pair-bearing table). Its consumers in T9/T10 read GF's table.

Status: decode-only. No GF code changed. Scratch: `<workdrive>/gf-TeamTailsRE-scratch/`
(`dis_tick.txt` = 772-line `FUN_1404F6C30` disassembly, `dis_*.txt` = callee bodies,
`dec_*.txt` = decompiles, `checker_team_tails.py` = self-checking checker with mutations).
Base `0x140000000`, image `~/ghidra-projects/scratch/Unpacker/pes_full.bin`
(file offset = VA − base). Tags: **CONFIRMED** = raw disassembly or image bytes
read this pass (opcode bytes quoted); **DOC** = cited doc, not re-checked;
**INF** = inference; **OPEN** = blocker named. Decompile cited for orientation only.

## §0 Float table (script-read from the image, `repr` + bits exact, pasted verbatim)

Script: `struct.unpack('<f'/`<I')` over every `0x142xxxxxx` DATA VA cited in §§1–7
plus the report's audit rows. The checker (`checker_team_tails.py`) re-asserts
every row below plus the opcode bytes in §1/§4; all pass (`FAILURES: none`).
One audit correction found by the checker this pass: `0x14259BE70` bits are
`0x43610000` (225.0 stays; bits fixed — the checker caught `0x43200000`).

| `VA` | `value` (`0xBITS`) | use |
|---|---|---|
| `0x14253EE30` | `100.0` (`0x42C80000`) | blend rate normaliser (53 `0x140A40AF6/40B36`); 57F720 head gate `0x14057F83E`-region divisor base |
| `0x14259BE04` | `12.0` (`0x41400000`) | blend class-load scale `0x140A408F0`-region (`MULSS ...,[0x14259BE04]` at `0x140A40906`-adjacent; `0x14053C820 MULSS XMM0,[0x14259BE04]`) |
| `0x142565600` | `0.5` (`0x3F000000`) | `0x14053C828 ADDSS +0.5`; 57F600 `SUB ECX,5`/window; 57FDE0/57F720 `+0.5` arms |
| `0x14259222C` | `5.0` (`0x40A00000`) | `0x14053D463 MULSS ×5.0` + `0x14053D46B ADDSS +0.5` (53D250 window); 57FDE0/57F720 `×5.0` arms |
| `0x14258C264` | `3.0` (`0x40400000`) | `0x14053D2CE DIVSS /3.0` |
| `0x142565D30` | `nan` (`0xFFFFFFFF`) | abs-mask (fabs idiom), NOT a value — 53C790/53CC00/53D3B0 `MOVSD/MOVAPS XMM*,[0x142565D30]` |
| `0x14259D634` | `10000.0` (`0x461C4000`) | `0x14053D4C6 MOVSS XMM8,[0x14259D634]` (53D3B0 initial best) |
| `0x14256AE34` | `1000.0` (`0x447A0000`) | `0x14053D1A7 DIVSS /1000.0` (53CC00 tail RNG scale) |
| `0x14259A764` | `60.0` (`0x42700000`) | 60° cone ceiling (57FDE0 `0x140580...` + 57F720 `COMISS ...,[0x14259A764]`) |
| `0x1425CB000` | `1.6399999856948853` (`0x3FD1EB85`) | 57FDE0 sub-threshold span (`iVar19==4` arm) |
| `0x142572348` | `0.800000011920929` (`0x3F4CCCCD`) | 53CC00 tail `0x14053D161 MOVSS XMM1,0.8` (pass arm) |
| `0x142598578` | `0.20000000298023224` (`0x3E4CCCCD`) | 53CC00 tail `0x14053D16B MOVSS XMM1,0.2` (fail arm) |
| `0x142598590` | `3600.0` (`0x45610000`) | 57F720 head gate divisor (`fVar25 < 2000.0/(rnd*3600.0) → return`) |
| `0x1425CB004` | `2000.0` (`0x44FA0000`) | 57F720 head gate numerator |
| `0x14256AE30` | `120.0` (`0x42F00000`) | 57Fxxx heading gate (`<=120.0`) |
| `0x1425656D0` | `-0.0` (`0x80000000`) | sign-flip xor (57FDE0/57F720 `XORPS XMM0,XMM14`) |
| `0x14259BE20` | `25.0` (`0x41C80000`) | 25.0 distance gate (57FDE0 `fVar8`, 57F720 `fVar12`) |
| `0x14259BE30` | `45.0` (`0x42340000`) | 45.0 gate (57FDE0 `fVar9`, 57FDE0 `COMISS ... <=45.0`) |
| `0x142588218` | `-1.0` (`0xBF800000`) | −1.0 leg (57F720 `fVar10`; latch-float `XMM9` leg in 53CC00 head) |
| `0x142588210` | `10.0` (`0x41200000`) | 10.0 floor (`0x14053D05D COMISS XMM0,10.0`) |
| `0x142598580` | `180.0` (`0x43340000`) | 180° wrap gate (57FDE0/57F720 heading normalise) |
| `0x14259BE84` | `-10.0` (`0xC1200000`) | carrier-slot penalty (57FDE0 `FUN_140A4C720(...,−10.0)`) |
| `0x14259BE68` | `150.0` (`0x43160000`) | 150° band ceiling (53CC00 `0x14053CD03 MOVSS XMM12,150.0`) |
| `0x14259BE28` | `32.0` (`0x42000000`) | step-46 gate family (kept for cross-check; not read in §§2–6) |
| `0x14259BDEC` | `6.0` (`0x40C00000`) | step-46 family (kept; not read in §§2–6) |
| `0x14259D5D4` | `1.2000000476837158` (`0x3F99999A`) | step-46 family (kept; not read in §§2–6) |
| `0x1425A9460` | `29.0` (`0x41E80000`) | step-46 family (kept; not read in §§2–6) |
| `0x1425A2670` | `400.0` (`0x43C80000`) | 400.0 d² gate family (57F720-side geometry uses the same idiom) |
| `0x14259BE40` | `57.2957763671875` (`0x42652EE0`) | rad→deg (kept; not read in §§2–6) |
| `0x1425795E4` | `6.2831854820251465` (`0x40C90FDB`) | 2π wrap (kept; not read in §§2–6) |
| `0x14259858C` | `360.0` (`0x43B40000`) | 360 wrap (57FDE0/57F720 heading wrap) |
| `0x142598598` | `-360.0` (`0xC3B40000`) | −360 wrap twin (kept) |
| `0x1425656C0` | `4.0` (`0x40800000`) | 4.0 (kept; not read in §§2–6) |
| `0x14256AE1C` | `1.0` (`0x3F800000`) | unit / XMM9 one-leg |
| `0x142596E6C` | `80.0` (`0x42A00000`) | step-35 family (kept; not read in §§2–6) |
| `0x14259BE5C` | `90.0` (`0x42B40000`) | 90° final test (57FDE0-side geometry `COMISS 90.0,[0x14259BE5C]`) |
| `0x14259BE60` | `135.0` (`0x43070000`) | retreat-test 135° (kept) |
| `0x14259BE6C` | `160.0` (`0x43200000`) | 160.0 (kept) |
| `0x14259BE70` | `225.0` (`0x43610000`) | d² gate 225 = 15² (bits corrected this pass: `0x43610000`, NOT `0x43200000`) |
| `0x1425C48E4` | `1369.0` (`0x44AB2000`) | 37² d² ceiling family (kept; not read in §§2–6) |
| `0x1425C48D0` | `105.0` (`0x42D20000`) | 105.0 floor family (kept; not read in §§2–6) |
| `0x1425C48DC` | `324.0` (`0x43A20000`) | step-40 family (kept; not read in §§2–6) |
| `0x1425C6614` | `-100000.0` (`0xC7C35000`) | pick-loop floor idiom (kept) |
| `0x14259BDDC` | `1.5` (`0x3FC00000`) | press-lane margin family (kept) |
| `0x14259BDF4` | `8.0` (`0x41000000`) | 8.0 ramp base (kept) |
| `0x142591140` | `30.0` (`0x41F00000`) | base-score reset (kept) |
| `0x14259BE0C` | `15.0` (`0x41700000`) | 15.0 scale (kept) |
| `0x14259BDF0` | `7.0` (`0x40E00000`) | 7.0 ramp rate (kept) |
| `0x14259A760` | `35.0` (`0x420C0000`) | 35.0 cone reference (kept) |
| `0x14259BE1C` | `20.0` (`0x41A00000`) | 20.0 chain gate (kept) |
| `0x14259BE18` | `18.0` (`0x41900000`) | 18.0 neighbour (kept) |
| `0x142565604` | `2.0` (`0x40000000`) | 2.0 (kept) |
| `0x142588200` | `0.10000000149011612` (`0x3DCCCCCD`) | 0.1 rate (kept) |
| `0x1425C65E4` | `0.5555555820465088` (`0x3F0E38E4`) | retreat-test constant (kept) |
| `0x14259BE38` | `55.0` (`0x425C00000`) | widening 55 (kept) |
| `0x14259BE48` | `65.0` (`0x42820000`) | widening 65 (kept) |
| `0x14259BE58` | `85.0` (`0x42AA0000`) | retreat-test 85° (kept) |
| `0x142590D98` | `0.30000001192092896` (`0x3E99999A`) | 0.3 floor (kept) |
| `0x14257A190` | `0.6000000238418579` (`0x3F19999A`) | 0.6 divisor (kept) |
| `0x14259D664` | `-12.0` (`0xC1400000`) | −12.0 gate (kept) |
| `0x14257A194` | `0.699999988079071` (`0x3F333333`) | support-score weight ×0.7 (kept) |
| `0x14259D5D0` | `0.6499999761581421` (`0x3F266666`) | non-protected scale ×0.65 (kept) |
| `0x14259D5D8` | `1.399999976158142` (`0x3FB33333`) | pick blend base (kept) |
| `0x14259BE88` | `-30.0` (`0xC1F00000`) | carrier-slot penalty edge (kept) |
| `0x14259D5C8` | `0.3499999940395355` (`0x3EB33333`) | jitter amplitude ×0.35 (kept) |
| `0x14259D668` | `-15.0` (`0xC1700000`) | lower score floor (kept) |
| `0x14259D654` | `-3.0` (`0xC0400000`) | support-score add (kept) |
| `0x1425A9464` | `36.0` (`0x42100000`) | widening 36 m gate (kept) |
| `0x14259BE2C` | `40.0` (`0x42200000`) | chip ramp low (kept) |

Operand-VA pairs (brief-rules 19-10-26; each float with the instruction that reads it):
step 52 `0x14053C720 MOVSS [RSI+4],XMM0` (latch float; XMM0 built by
`0x14053C710/718/71D MOVSX/CVTDQ2PS/MOVSS` from `T+0x254`);
53CB40 `0x14053C820 MULSS XMM0,[0x14259BE04]` = 12.0,
`0x14053C828 ADDSS XMM0,[0x142565600]` = +0.5;
53D250 `0x14053D2CE DIVSS XMM0,[0x14258C264]` = /3.0,
`0x14053D463 MULSS XMM0,[0x14259222C]` = ×5.0, `0x14053D46B ADDSS XMM0,[0x142565600]` = +0.5,
`0x14053D4C6 MOVSS XMM8,[0x14259D634]` = 10000.0;
53CC00-head `0x14053CCBB MOVSD XMM8,[0x142565D30]` = abs-mask,
`0x14053CCCD MOVSS XMM9,[0x142588218]` = −1.0, `0x14053CCDF MOVSS XMM10,[0x14256AE1C]` = 1.0,
`0x14053CCF1 MOVSS XMM11,[0x14259A764]` = 60.0, `0x14053CD03 MOVSS XMM12,[0x14259BE68]` = 150.0,
`0x14053CD15 MOVSS XMM13,[0x14258C264]` = 3.0, `0x14053CD27 MOVSS XMM14,[0x1425656D0]` = −0.0;
53D3B0-head `0x14053D4B8 MOVSS XMM8,[0x14259D634]` = 10000.0,
`0x14053D1A7 DIVSS XMM0,[0x14256AE34]` = /1000.0;
blend `0x140A40906`-region `MULSS ...,[0x14259BE04]`-family + `0x140A40AF6/40B36` rate pair
`MULSS XMM0,[0x14253EE30]` = ×100.0 (bytes `F3 0F 59 ...` at `0x140A40AF6`/`0x140A40B36` CONFIRMED).

## §1 T6b (step 55 / step 26c): the R8 source at `0x1404F6C7B` — RESOLVED (CONFIRMED)

### The register trace (bytes, no decompile needed)

Tick prologue (`dis_tick.txt` lines 14–19), opcode-checked against the image:
- `0x1404F6C60 MOV [RCX+0x6D50],RDX` (`48 89 54 ...` region) — brain+0x6D50 := RDX.
- `0x1404F6C6A MOV [RCX+0x6D50],R8` — brain+0x6D50 := **R8** (second store wins).
- `0x1404F6C7B MOV R13,R8` — bytes `4D 8B E8` CONFIRMED (R13 := R8, the tick's 3rd parameter).

So at entry, in the System V / MS-x64 convention the bridge disassembly shows
(`RCX=brain, RDX=?, R8=?`), **R8 is the caller's team-side pointer RDX-chain**:
`[RDI+0x6D50]` (= `brain+0x6D50` = T, the tick's team object) is stored FROM R8.

Step-26c call site (`0x1404F6F0B/0x1404F6F0E`):
- `0x1404F6F0B MOV RDX,R13` — bytes `49 8B D5` CONFIRMED.
- `0x1404F6F0E CALL 0x140A42A70` — `E8 5D BB 54 00` CONFIRMED (rel32 → `FUN_140A42A70`).

`FUN_140A42A70(RCX=T(dst), RDX=src)` is the 57-line row copy (tick-steps3 §3, DOC;
re-read this pass, `dis_140A42A70.txt`): `RCX` = dst team, `RDX` = src block,
copies `src+0xA2CC+0x34·s → T+0xA2CC+0x34·s` gated on the `T+0xA2A0==0x25` skip +
`FUN_140A401C0/1F0` match pair, then the per-slot touch
`FUN_140A401F0(ring, [T+0x21C])`.

Step-26c therefore passes **src = R13 = R8 = the tick's incoming 3rd parameter**.
The restart-path branch (`0x1404F6ED2 JNZ 0x1404F6F18` on `MI+0x1775==0x17`)
falls through to the teardown pair (`0x1404F6EFF CALL 0x14051B3E0`,
`0x1404F6F0E CALL 0x140A42A70`) and then `0x1404F6F13 JMP 0x1404F7A9F` (tick
exit) — so on the restart path the copy's src is the entry R8.

Step-55 call site (`0x1404F7A66–0x1404F7A8F`):
- `0x1404F7A66 CALL 0x140A48A30` — `E8 C5 0F 55 00` CONFIRMED.
- `0x1404F7A7D MOV RBX,RAX` — bytes `48 8B D8` CONFIRMED (X := return value).
- `0x1404F7A8F CALL 0x140A42A70` — `E8 DC AF 54 00` CONFIRMED.

`FUN_140A48A30(G, side)` is 9 lines (`dis_A48A30.txt`, CONFIRMED):
`CMP EDX,2; JNC take-[RCX+0x1928]` else `RAX=[RCX+RAX*8]` with
`RAX=side+0xA1, ×5` — i.e. **X = T(side)**, the side's team object (run-8 already
decoded this: "X = FUN_140A48A30(G, side) = T(side), so the copy is a self-copy";
re-verified raw this pass — the body is exactly the side-indexed team-pointer
table with the `+0x1928` fallback).

### Verdict

- Step-26c src: **R8 at `0x1404F6C7B` = the tick's entry 3rd parameter** (the
  caller-side team pointer saved to `R13`), status **CONFIRMED** (bytes +
  full prologue/tail trace above). It is NOT a fresh allocation: no creator
  call sits between `0x1404F6C7B` and `0x1404F6F0B` on the restart path
  (only `FUN_14051C840/01260/4F6720/4F7AD0/42580/5008E0/502190/4FE9C0/FEDC0/
  51E910/4F8350/4F6680(×cond)/4FA840/4FA720/4FBCA0` + the inline B8A2/A3
  stores + `FUN_140500200/4FF8B0/4F7BA0/4FD4B0/53B860/4F87D0/500B90` + the latch
  byte + `FUN_1404FBB70`-cond/`51E350/51980/57AC20` + the step-28 inline stores —
  none writes R13; `R13` is callee-saved and untouched between the two sites).
- Step-55 src: **RAX left by `FUN_140A48A30` = T(side)**, status **CONFIRMED**
  (9-line body + `MOV RBX,RAX` + `MOV RDX,RBX` at `0x1404F7A8C`, opcode-checked).
  Since the tick runs per side with `RCX=T` = `T(side)`, the step-55 copy is a
  **self-copy** (DOC run-8, re-confirmed).
- What step-26c does with a non-self src on the restart path: the same
  `FUN_140A42A70` body — per-slot gated copy + `FUN_140A401F0(ring, [T+0x21C])`
  touch. Whether src ≠ dst there (i.e. whether the caller passes an alias X
  distinct from T) is a property of the tick's CALLER (`FUN_14041ECF0` →
  `FUN_1404F6C30(brain, S, T_s)` per team-layer §1 line 6; the H-frame binds
  S/T per side — DOC team-layer §1, not re-checked). OPEN: the exact identity
  the H-frame passes as the 3rd arg on the restart arm (one live/caller decode).

### What `FUN_140A401F0(ring, [T+0x21C])` does in GF terms (CONFIRMED)

`FUN_140A401F0` is 4 lines (`dis_A401F0.txt`):
`MOVSXD RAX,[RCX+0x30]; LEA R8,[RAX+RAX*2]; MOV [RCX+R8*4+8],EDX; RET` —
writes EDX into the **head entry's seen-stamp** (`ring+8+12·(index mod 4)`,
with `index=[ring+0x30]`). `EDX=[R12]` with `R12=RCX+0x21C` at entry
(`0x140A42A90 LEA R12,[RCX+0x21C]`, `0x140A42B01 MOV EDX,[R12]`
— `41 8B 14 24` CONFIRMED), and `T+0x21C` is the frame stamp step 7 writes
(`FUN_140A42580`, DOC teamorder §2.3). GF already ports this 1:1 as
`pes::run::Touch(Ring*, stamp)` (`pesrunslots.hpp:46-47`; `Touch` sets the
head's `seenAt`). **No new GF state is needed for the touch itself** — the
per-slot `Ring` + the `T+0x21C` stamp (GF `GetPesFrame`, `pes::PesFrameOf`)
carry it 1:1. What is NOT yet wired: calling the step-55/26c `FUN_140A42A70`
copy itself (GF `pesrunslots` has no 52-byte row-copy entry point; the rows
`T+0xA2CC+0x34s` have no GF counterpart — see §7 port spec T6b).

## §2 T9: the step-46/48 tail `FUN_14057F600` — DECODED (CONFIRMED body, payloads OPEN)

67 lines (`dis_57F600.txt`). Signature: `FUN_14057F600(brain, side)` —
`RDI=RCX` (brain) saved at `0x14057F605`, `RBP` = side (`[RCX+0x8]`,
`0x14057F62A MOV EDX,[RCX+0x8]` — bytes `8B 51 08 48 8B 89 48 6D` CONFIRMED).

Writes: **none to T/S/brain/MA directly** — every arm ends in a CALL
(`FUN_14057FDE0` / `FUN_14057F720`) or falls to the epilogue. It is a
**release/claim dispatcher**, not a writer. Status: body CONFIRMED,
the two payloads OPEN (decoded one level deeper this pass, §§2.1–2.2).

Gate chain (CONFIRMED sites, opcode-checked):
- `0x14057F61B CMP [RSI+0x116C],2; JZ exit` (`83 F8 02 0F 84` CONFIRMED) —
  returns immediately when `MI+0x116C==2`.
- `0x14057F63E CMP EAX,3; JNZ 0x14057F694` (`83 F8 03 75 51` CONFIRMED):
  - `==3` arm: `XOR R8D,R8D; CALL 0x140642670` (`45 33 C0 E8...` CONFIRMED).
    `TEST AL,AL; JZ exit`. Else the 2-slot scan (`BL=0..1`,
    `MOV RBP,0xC00E380000000` — bytes `48 BD 00 00 00 80 E3 00 0C 00` CONFIRMED):
    per global slot `EDX=[RSI+RAX*4+0x173C]` (`8B 94 86 3C 17 00 00` CONFIRMED),
    `CMP EDX,0x15; JA skip`; `CALL 0x140A48910` (`E8 95 92 4C 00` CONFIRMED);
    `MOVSX ECX,[RAX+0x1]; CMP ECX,0x33; JA skip; BT RBP,RCX; JC dispatch`.
    On dispatch fall-through (`0x14057F6C3`): latch test
    (`CMP [RDX+0x255],0` at `0x14057F6CA` — `80 BA 55 02 00 00 00` CONFIRMED):
    latch set → `FUN_14057FDE0(RDI-side...)` (`E8 FD 06 00 00` CONFIRMED);
    latch clear + `MI+0x116C==1` → `FUN_14057F720` (`E8 ...` at `0x14057F6F9`).
  - `!=3` arm (`0x14057F694 CALL 0x14057F590; TEST AL,AL; JZ exit`): then the
    carrier-record gate (`CALL 0x140A3E190` at `0x14057F6AD` —
    `33 D2 48 8B` region CONFIRMED) + the frame window
    (`ECX=[RSI+0x1198]−[RAX+0x10]−5; CMP ECX,5; JA exit` at
    `0x14057F6B2–6BE` — `8B 8E 98 11 00 00 2B 48` CONFIRMED), then the same
    latch dispatch (`0x14057F6CA` → `57FDE0`/`57F720`).

Inputs: side (`[brain+0x8]`), `MI+0x116C`, `MI+0x1554`-adjacent stoppage code
(via `FUN_140642670`/`FUN_14057F590`), the global-slot table `MI+0x173C`,
player records (`FUN_140A48910`), the latch `T+0x255`, the carrier record
(`FUN_140A3E190`), `MI+0x1198` frame. Outputs: none directly — routes to the
two release loops. GF 1:1: side, play state, latch, frame all bound
(`TeamTick`/controller state); `MI+0x173C` global-slot table, `FUN_140642670`,
`FUN_14057F590`, and both payloads OPEN (payload details §§2.1–2.2 name the
next decode, not new GF state).

### §2.1 `FUN_14057FDE0` (latch-set release loop, 245-line decompile DEC — INF detail, CONFIRMED skeleton)

`dec_57FDE0.txt` (decompile orientation; register trace INF):
per global slot `uVar21` (11): on-pitch (`[..+0x1748]`), stoppage arms
(`116C==3 && 1554==5`, `local_150==2` branch with `FUN_140A42940(T,...,0x10)` take +
`FUN_140A42A40` point commit), else the guard ladder
(`FUN_140A4C9A0`, `FUN_140A40000(...,0x10,...)`, `FUN_14043A020 <= 60.0`
[`DAT_14259A764`], `FUN_1404C8E00 <= 25.0` [`DAT_14259BE20`], `FUN_140439E50` +
180° wrap [`DAT_142598580`]/360 [`DAT_14259858C`]/120.0 [`DAT_14256AE30`],
`FUN_14057F230` + `FUN_14057F3D0` refusal pair) ending in
`FUN_140A42940(T, slot, 0x10)` take + `FUN_140A42A40` point commit.
State written: `T+0xA2A0[s]=0x10` via the take + the 32-byte target block
(`FUN_140A42A40`, same family as step-34 commits — DOC tick-steps3 §5).
Status: skeleton CONFIRMED (call sites + constants `60.0/25.0/45.0/180.0/360.0/
120.0/10.0/−10.0/−0.0/+0.5/×5.0/0.4/200.0/1.64` all script-read §0);
per-gate dataflow INF (which value feeds which call needs the register trace).

### §2.2 `FUN_14057F720` (latch-clear release loop, 212-line decompile DEC — INF detail, CONFIRMED skeleton)

`dec_57F720.txt`: head RNG gate
(`fVar25 < 2000.0/(rnd·3600.0) → return`, `DAT_1425CB004`/`DAT_142598590`),
role-class gate (`T+0x4C00`-family / `0x6D/0x36` state test),
`FUN_140A40000(...,0x10,...)` + `FUN_14043A020 <= 60.0` + `FUN_14057D930 <= 25.0`
gates, `FUN_140A42940(T, slot, 0x10)` take on pass.
Status: same as §2.1 (skeleton CONFIRMED, dataflow INF).

## §3 T9: the five concept fan-outs (step 52 `FUN_14053C6E0` callees) — DECODED (CONFIRMED)

`FUN_14053C6E0` (46 lines, `dis_14053C6E0.txt`): inputs `RCX`=row-block,
`RDX`=S-team, `R8`=side. Store: `T+0x4 latch float = ±1.0`
(`0x14053C710 MOVSX R9D,[RAX+0x254]` from `FUN_140A48A30(G,side)`;
`0x14053C718/71D CVT`; `0x14053C720 MOVSS [RSI+4],XMM0` —
`F3 0F 11 46 04` CONFIRMED). Then five concept calls in order
(R8=side, RDX=S-team, RCX=row-block; each opcode-checked `E8`):
1. `0x14053C733 CALL 0x14053CB40` — **counter gate**: resolves T via
   `FUN_140A48A30` (`0x14053C701`, `E8 2A C3 50 00` CONFIRMED), validates the
   record pair (`FUN_140A3E7D0` + `FUN_140A60530`), then: record-match →
   `[RBX]=0`; else latch-clear + `T+0x78>0x15` + record-valid + id-match →
   fall through; finally `MI+0x116C==1` → `INC [RBX]` (the concept row's
   counter word). Writes: `[RBX]` (row counter, 0 or ++1). Inputs: side, T,
   `T+0x255`, `T+0x78`, `MI+0x116C`. GF: latch + `noBallCounter`-adjacent
   counter word — needs a new GF row-counter word (see §7 T10b).
2. `0x14053C741 CALL 0x14053C790` — **level-map + RNG window**: `RBP=[RDX+0x18]`
   (MA-side object); writes `[RBX]`/`[RBX+4]` from the `T+0xB8A4` ladder
   (0→0, 1→0, 2→1, 3→2, 4→3, else 3) then the RNG arm
   (`CALL 0x141E532D0; MULSS ×12.0 [0x14259BE04]; ADDSS +0.5 [0x142565600];
   CVTTSS2SI; CMP [RBX],EAX; JA...` at `0x14053C81B-835` CONFIRMED) and the
   sign-gated ball-x test (`MULSS sign·[RBP+0x2482C]; COMISS 0` at
   `0x14053C847-852`) possibly forcing `[RBX+4]=3`, plus the `T+0xB8A2` arm
   forcing `[RBX+4]=3`. Writes: row words `[RBX]`,`[RBX+4]` (level + window).
   Inputs: `T+0xB8A4`, RNG cell, `MA+0x2482C` ball-x, `T+0x254` sign,
   `T+0xB8A2`, `MI+0x116C`. GF: `b8a4/b8a2` bound (`TeamTick`); RNG cell,
   `MA+0x2482C`, row words need new state (see §7).
3. `0x14053C74F CALL 0x14053D250` — **six-lane max + hysteresis**: `R14=G+0x2AA8`
   geometry; per lane writes `[RDI+0x20]` (0/1/2/4/5 across the
   `FUN_140A57800`-gated tests with `/3.0`, sign-gated ball-x, fabs idiom
   `ANDPS [0x142565D30]`), keeps the max in `[RDI+0x24]`, then the 6-entry
   `[RDI+0x28..]` histogram (`INC` the winner or zero it). Writes: row words
   `+0x20/+0x24/+0x28[6]`. Inputs: side, T, `T+0x254` sign, `G+0x2AA8`,
   `G+0x40` player table. GF: needs a new GF 8-word row (see §7).
4. `0x14053C75D CALL 0x14053C890` — **per-slot aim-point fill**: 11-slot
   on-pitch scan (`side·11+k`, `[..+0x1748]`, `[..+0x175E]`), per passing slot
   the 5-call aim chain (`53C250/63A3A0/53C150/53C510/53C3D0`) filling
   `[RDI−8..+8]` (20-byte row), else the fallback ladder
   (`0xBA2E8BA3` magic-divide = frame/11, `FUN_140A3AF60/1B59E20/8C75C0` rank
   chain); then the two `FUN_14063CD80`-loop reduction passes + the
   `FUN_1401421B7D40/63EA30` commit ladder writing `[RSI]=slot-or-0xFF`.
   Writes: caller-passed row buffer (match rows, same family as step 53).
   Inputs: side, record table, rank helpers (all OPEN bodies). GF: row buffer
   = caller frame state (see §7 T10c — same destination family as the blend).
5. `0x14053C76B CALL 0x14053CC00` — **concept-state classifier**: per-entry
   (`FUN_140A3E8E0` walk) switch on `[RAX]` (0/7/12/0x18/else): the `0x18` arm
   (`FUN_140A73E20` + id-match + `FUN_1421B7D40==3` + `FUN_140A3CE70/3CE10`
   + 60.0/3.0/abs gates → `INC [RDI+0x48]`); the `0xC` arm
   (`[RDI+0x8]=3, R13B=1` or per-card geometry → `[RDI+0x8]∈{1,2}`);
   the `7` arm (card-pair geometry with 60.0/3.0/90.0/225.0/10.0 gates →
   `[RDI+0x8]∈{1,2}` or the 150°/60° band → `[RDI+0x8]=EAX∈{1,2}`);
   tail RNG scale (`frame/1000.0`, 200.0/3600.0-family constants `0x14256AE34`/
   `0x142598590` region) → `[RDI+0x8]∈{0,1,2}`.
   Writes: `[RDI+0x8]` (concept state), `[RDI+0x48]` (counter), `R13B` latch flag.
   Inputs: side, T, `T+0x254`, card records, `T+0x250`, frame. GF: needs new
   GF concept-state words (see §7).
- Finale `0x14053C788 JMP 0x14053D3B0` (`E9 23 0C 00 00` CONFIRMED):
  `FUN_14053D3B0` (141 lines) — **take-or-timeout commit**: `[R14+0x14]++`
  with the `0x40`-word state machine (`[RBP+0x40]` 0/1/2 arms, `BB`-record
  + `A3E190` + `A62EA0` + `×5.0/+0.5` window at `0x14053D463/46B` CONFIRMED);
  then the kick-off arm (`116C`/`1554==1`/`1558`-id match → `[RBP+0x44]=0xFF`,
  per-slot nearest-pick loop with `[..+0x1748]` + `FUN_140A638C0` refusal +
  `FUN_140A8F890` aim → `[RBP+0x44]=slot`, `[RBP+0x40]=1`).
  Writes: `[R14+0x14]` (counter), `[RBP+0x40]/[RBP+0x44]` (concept words).
  Status: body CONFIRMED, helper payloads OPEN.

## §4 T10: the 184-line blend body `FUN_140A408C0` — DECODED (CONFIRMED)

184 lines (`dis_140A408C0.txt`). Signature from the call site
(`0x140A408C0(RCX=ctx-obj, RDX=S-team, R8=side, R9=row-buffer)`):
`R12D=R8D` (side), `R13=R9` (out-param row/walker buffer),
`RSI=[R13+0x298]` walker, `RBP/RBX` walk `RDX+0x7DA0`/`RDX+0x3F08` rows
(`0x140A4096D/974 LEA RBP/RBX` — `48 8D AA A0 7D 00 00` / `48 8D 9A 08 3F 00 00`
CONFIRMED), 11 iterations (`0x140A40980 CMP R14,0xB`).

Per-slot (CONFIRMED sites, opcode-checked):
- Gate A (`0x140A40986 CMP [RAX+RBP+0x2500],1; JNZ skip` —
  `83 BC 28 00 25 00 00 01` CONFIRMED): row enabled.
- Gate B (`0x140A40990 CMP [RSI−0xC],1; JNZ skip` + `0x140A4099A JZ blend`
  on `[RSI+0x5]`): concept-row armed check.
- Blend lane (`0x140A409D9`): `XMM6=[RBX−0x4]`, `XMM0=[RBX]`, `XMM2=[RBX+0x4]`;
  store triple `[RAX]=XMM6, [RBX]=XMM0, [RBX+4]=XMM2`
  (`0x140A40B62/66/6B MOVSS [RAX]/[RBX]/[RBX+4]` — `F3 0F 11/10` family
  CONFIRMED) onto the **caller-passed match-row buffer** (SIM "match-row
  blend"; exact struct identity OPEN — same caveat as tick-steps2 §12).
- Copy lane (`0x140A409F4`): `XMM0=[RBX−0x364..]` pair + `EAX=[RBX−0x35C]`
  staged to `[RSP+0x48/50]`; if `R9(row-count)==1 && RCX(match-frame)==R9 &&
  EDI<11` the fallback row (`[RBP−0x3DE8]` or `[RDX+0x3FB8]`) class gate
  (`CMP EAX,5; JL copy`) selects it; the `T+0x8E00` id-match gate
  (`0x140A40A65 IMUL EAX,R12D,0xB; ADD EAX,EDI; CMP [R13+0x8E00],EAX; JZ copy` —
  `41 6B C4 0B 03 C7 41 39 85 00` CONFIRMED) routes to the copy; else the
  claim-test ladder: card refusal (`0x140A40A8A CMP [R15],0x37; JZ blend` —
  `41 83 3F 37` CONFIRMED), class window (`[R15+0x18]−3 & ~0xFD`, i.e.
  classes 3..5 — `41 8B 4F 18 8D 41 FD A9` CONFIRMED), side-key match
  (`[R15+0x14]==R12D`), sign-vs-key
  (`0x140A40ABD MULSS XMM1,[RAX]; 0x140A40AC1 MULSS XMM0,XMM6;
  0x140A40AC5 COMISS XMM0,XMM1; JBE blend` — `F3 0F 59 08 F3 0F 59 C6`
  CONFIRMED), class-5 claim test (`0x140A40AE2 CALL 0x140A41C00` —
  `E8 16 ...` CONFIRMED; `TEST AL; JZ blend`); on pass-through the rate pair
  blends: `XMM1−=[RBX−0x2E0]; XMM0=XMM8(rate); XMM1=XMM0·XMM7(100.0);
  XMM1/=100.0; XMM6+=XMM1` (`0x140A40AF6/40B36` pair CONFIRMED) and the same
  for `XMM2` (`0x140A40B14-42`), then the triple store.

Inputs: side (`R12D`), match frame (`[RSP+0xF0]`-passed row count + frame),
`T+0x8E00` (designation id), `T+0x254` sign (`0x140A40AA4 MOVSX EAX,[R13+0x254]` —
`41 0F BE 85 54 02 00 00` CONFIRMED), card `0x37`, class `[R15+0x18]`,
side-key `[R15+0x14]`, rate `XMM8=[RCX+0x3F88]` (`0x140A408F0` —
`F3 44 0F 10 81 88 3F 00 00` CONFIRMED), normaliser 100.0.
Writes: the caller-passed match-row buffer triple per slot (11×).
GF: the row buffer is caller frame state (the step-33/34 concept rows);
no GF 1:1 today — needs the row-buffer decision (see §7 T10c).

## §5 T6b per-slot touch in GF terms (answer to the brief's second question)

`FUN_140A42A70`'s per-slot tail (`0x140A42B01 MOV EDX,[R12]` / `0x140A42B08 CALL
0x140A401F0`) is GF `pes::run::Touch(&ring, stamp)` with `stamp = T+0x21C`
(GF `GetPesFrame()`), called for every slot after the gated 52-byte copy.
GF carries this 1:1 TODAY: `RunSlotOf/OwnRunSlot` map the player to the own
slot (`teamAIcontroller.cpp:2973-3000`), `runSlots[k].ring` is the ring,
`run::Touch` sets the head's `seenAt`. The port gap is NOT the touch — it is
the 52-byte copy itself (`T+0xA2CC+0x34s` rows have no GF counterpart) and the
src identity (step-26c R8 = entry 3rd param, CONFIRMED §1; step-55 src =
T(side) self-copy, CONFIRMED §1).

## §6 Inputs: GF 1:1 or exactly-what-is-needed

Bound (an existing GF state carries the quantity 1:1):
- T (the tick's team object), side, latch `T+0x255`, frame `T+0x21C`/`MI+0x1198`:
  `TeamTick` (`pesteamtick.hpp`), `RunSlotOf/OwnRunSlot`, `GetPesFrame`/`PesFrameOf`.
- `T+0xA2A0` state word, history ring, `T+0x26C/0x28C` blocks: `pes::run::Slot`
  (`pesrunslots.hpp`) — `state/ring/block`, `Push/Behind/Touch/Age/Assign/Take/Release`.
- Step-43 gate `T+0x218`: `TeamTick::noBallCounter` (`pesteamtick.hpp:50`).
- Latch float `T+0x4`: sign of `T+0x254` — GF comment names the convention
  (`teamAIcontroller.cpp:2417`); the float itself is new (see Named 1).
- Step-52 covers: side + row-block + S-team are call args (no stored state needed).

Named (meaning decoded; a new GF state is needed; 7 items):
1. `T+0x4` latch float (±1.0): one float per side (`TeamTick::latchFloat`).
2. Concept row-block words (`[RBX]` counter, `[RBX+4]` level/window, `+0x20/+0x24/+0x28[6]`,
   `[RDI+0x8]` concept state, `[RDI+0x48]` counter, `[RBP+0x40]/[RBP+0x44]` commit words,
   `[R14+0x14]` counter): one small struct per concept row (the step-33/34 row buffer).
3. `T+0xA2CC+0x34s` 52-byte history rows (the `FUN_140A42A70` copy payload).
4. `T+0x8E00` designation id (read at `0x140A40A65`): the designated-supporter id word.
5. `T+0x78` ref-player fallback (read in 57FDE0/57F720-adjacent gates): one slot id.
6. `MI+0x173C` global-slot table + `MI+0x1748/0x175E` on-pitch bytes: roster presence table.
7. `G+0x2AA8` geometry + `G+0x40` player table + `MA+0x2482C` ball-x: match-geometry inputs.

Unbound (meaning undecoded or OPEN; 9 items):
1. `FUN_140642670` (57F600 `==3`-arm stoppage test): body OPEN.
2. `FUN_14057F590` (57F600 `!=3`-arm test): body OPEN (22 lines fetched, not classified).
3. `FUN_14057FDE0` per-gate dataflow (which value feeds which call): register trace OPEN.
4. `FUN_14057F720` per-gate dataflow: same, OPEN.
5. `FUN_140A57800/140A3E190/140A62EA0/140A48A10/140A48910` record helpers: payloads OPEN.
6. `FUN_140A3AF60/1B59E20/8C75C0` rank chain (53C890 fallback): bodies OPEN.
7. `FUN_140A41C00` claim test (blend gate): body OPEN.
8. `TM+0x23F7C` pair-table writer: still OPEN — all 33 code-region disp32 hits are
   `0F 10 ...` MOVUPS reads (`pre: 0f10...` at every hit, §6.1); the near-value
   scan (±0x200) is noise (small-int immediates everywhere); no indexed-store
   writer found. The writer is NOT among disp32 hits (same conclusion as run 7).
9. Step-26c caller arg identity on the restart arm (which object the H-frame passes
   as the tick's 3rd param): one caller decode (`FUN_14041ECF0` region).

### §6.1 `TM+0x23F7C` writer hunt (split-displacement search, this pass)

- All 39 image hits for the exact disp32 `0x23F7C` enumerated (33 in code):
  every code-region hit has `pre = 0F 10 ..` (MOVUPS/MOVSS load) — all reads,
  zero stores. Last-4 data-region hits (`0x1421328D2+`) are pointers, not code.
- Split check: brute-forced every LE int32 in `.text` within ±0x200 of `0x23F7C`:
  194 distinct near-values, all small-int noise (counts track generic immediates;
  no cluster at a plausible recombined base). No `ADD reg,0x23XX` + `ADD` pair
  pattern found. Verdict: writer stays OPEN (27A0 unbound 7 carried).

## §7 Port spec per slice (ready for an implementer; ≤3 pieces each)

Conventions: red test first on `origin/main`, smallest change, mutation per branch.
Stand-ins carry `// ponytail:` naming the PES site that removes them.

- **T6b — snapshot copy + touch** (§§1,5). Depends: run-slots table (ported).
  (i) `SnapshotCopy(dst, src)`: per-slot skip when `dst.state==0x25`, else the
  `Behind`-pair match test (`FUN_140A401C0`), on match the 52-byte row copy —
  needs Named 3 (52-byte rows beside `pes::run::Ring`; the ring alone cannot
  carry the `MOVUPS` payload). Test: `SnapshotCopiesOnlyWhenPairMatches`.
  (ii) The per-slot `Touch(ring, frame)` with `frame = GetPesFrame()` —
  1:1 `pes::run::Touch`, no new state. Test: `TouchSetsHeadSeenToFrame`.
  (iii) Wire step-55 (self-copy: src = dst = own `runSlots`) and step-26c
  (src = caller alias; until Unbound 9 closes, `// ponytail: step-26c src is
  the tick entry R8 (0x1404F6C7B); alias object TBD` and pass own slots).
  Mutations: skip-word flipped to `0x26`; touch writes `enteredAt`; copy
  advances on mismatch.
- **T9 — release/claim dispatch + loops** (§2). Depends: run-slots `Take`
  (ported), Named 4/5/6.
  (i) `FUN_14057F600` dispatcher skeleton: the `116C==2` early-out, the
  `==3`/else fork, the `0xC00E380000000` 2-slot scan with the
  `MI+0x173C`/`0x15`/`0x33`-bitmask gates (constants: `0x15`, `0x33`, mask
  `0xC00E380000000`, window `−5/+5`), the latch fork to the two loops.
  Test: `ReleaseDispatchExitsWhenPlayState2`.
  (ii) Latch-set loop (≈§2.1: `0x10`-take + point commit behind the
  `60.0/25.0/45.0/120.0` + refusal-pair gates) and latch-clear loop (≈§2.2:
  head RNG gate `2000.0/(rnd·3600.0)` + class/state gates + `0x10`-take).
  Both behind `// ponytail:` for Unbound 1–5 (helper payloads).
  Tests: `LatchSetLoopTakesSlot16`, `LatchClearLoopTakesSlot16`.
  (iii) Wire: call the dispatcher where step 48 calls `FUN_14057F600`
  (`0x1404F79A8`, gated by the same skip-bit test as step 49 at `0x1404F79AD`).
  Mutations: `116C==2` gate dropped; latch fork swapped; window `−5/+5` widened.
- **T10a — taker/subs tables** (step 51, DOC tick-steps2 §12; not re-decoded —
  no new evidence this pass; contract stands as cited).
- **T10b — latch float + five fan-outs** (§3). Depends: Named 1/2/7.
  (i) `latchFloat = sign(T+0x254)` (`TeamTick::latchFloat`; test
  `LatchFloatMatchesSign` — same name as the DOC ticket).
  (ii) Counter gate (53CB40: `[RBX]=0-or-++1`) + level-map/RNG window (53C790:
  `[RBX]/[RBX+4]` from the B8A4 ladder + `rnd·12.0+0.5` window + sign-gated
  ball-x + B8A2 force-3) — needs Named 2 row words + Named 7 geometry.
  Tests: `CounterGateZeroesOnRecordMatch`, `LevelMapFollowsB8A4Ladder`.
  (iii) Six-lane max (53D250) + aim fill (53C890) + classifier (53CC00) +
  commit (53D3B0) — needs Named 2/7; helpers OPEN (Unbound 5/6) stand in as
  `// ponytail:` returning "no record".
  Mutations: ladder arm swapped (2→1); RNG window dropped; max keeps first.
- **T10c — blend rows** (§4). Depends: Named 4 + row-buffer decision.
  (i) Gate chain (row-enabled `+0x2500==1`, concept armed, `T+0x8E00`
  id-match, card `0x37` refusal, class 3..5, side-key, sign-vs-key,
  `FUN_140A41C00` claim — Unbound 7 stands in as pass-through
  `// ponytail:`). Test: `BlendSkipsWhenCardRefuses`.
  (ii) Triple store (`[row−4]=XMM6, [row]=XMM0, [row+4]=XMM2`) + rate pair
  (`−[row−0x2E0]`, `×rate`, `×100.0`, `/100.0`, constants §0) — 1:1 float ops.
  Test: `BlendAddsRateScaledDelta`.
  (iii) Wire at step 53 (`0x1404F7A57`) with the step-33/34 row buffer as the
  caller buffer (until the row identity closes, `// ponytail: caller rows are
  the concept row buffer; PES struct identity OPEN (tick-steps2 §17.9)`).
  Mutations: rate pair dropped (blend is identity); claim gate dropped.

## §8 Corrections to existing docs

| doc:line | old | new | evidence |
|---|---|---|---|
| TeamT2 run 8: "step-26c X sources: at step 26c, X = R8 of the tick ... the caller's argument; ... (OPEN)" | OPEN | RESOLVED: X = entry R8 (`MOV R13,R8` at `0x1404F6C7B`, bytes `4D 8B E8`; `MOV RDX,R13` at `0x1404F6F0B`, bytes `49 8B D5`) — the tick's 3rd parameter, callee-saved to the restart-path call | bytes + prologue trace §1 |
| TeamT2 run 8: "Open before a port: the step-26c copy source (R8 ...)" | blocks T6b | unblocked to the port spec above (§7 T6b); remaining OPEN narrows to the H-frame 3rd-arg identity (Unbound 9) | §1 |
| tick-steps2 §12 step-53 row-base identity OPEN (carried in tick-steps3 §14.7) | OPEN with no new sites | same verdict, new sites: caller buffer = `R9` at entry (`MOV R13,R9` — `4D 8B E9` at `0x140A408F9`), walker `RSI=[R13+0x298]` (`49 8D B5 98 02 00 00` at `0x140A4091B`); rows walk `RDX+0x7DA0/0x3F08` | opcode §4 |
| TeamT2 run 8 step-55 note "X = T(side), self-copy" (DOC) | DOC | CONFIRMED: `FUN_140A48A30` 9-line body + `MOV RBX,RAX` (`48 8B D8`) at `0x1404F7A7D` | §1 |
| §0 `0x14259BE70` bits | (would-be `0x43200000`) | `0x43610000` (225.0; caught by the checker) | image re-read §0 |

## §9 Open items (numbered, with what blocks each)

1. H-frame 3rd-arg identity on the restart arm (which object the caller passes
   as the tick's R8) — decode `FUN_14041ECF0` call sites. Blocks T6b(iii) exactness.
2. `FUN_140642670` / `FUN_14057F590` bodies — decode both. Blocks T9(ii).
3. `FUN_14057FDE0` / `FUN_14057F720` register-level dataflow — trace which value
   feeds which call. Blocks T9 exactness (skeleton ports without it).
4. Record helpers (`A57800/A3E190/A62EA0/A48A10/A48910`, `A3AF60/1B59E20/8C75C0`,
   `A41C00`) — decode payloads. Block T9(ii)/T10b(iii)/T10c(i).
5. `TM+0x23F7C` writer — still OPEN after the split search (§6.1). Blocks T7a +
   27A0 unbound 7 (carried, not closed here).
6. Caller row-buffer struct identity (step-53/53C890 destination) — needs the
   caller-frame walk (step-33/34 rows). Blocks T10b(iii)/T10c(iii) exactness.
7. Live confirmation (read-only): `T+0x21C` stamp cadence, `T+0xA2A0=0x10`
   takes during a match, latch-float sign flips.

## §10 What was verified, what was not

- Ran: `checker_team_tails.py` — 68 float rows + 13 opcode sites + 4 GF-state
  greps + 2 mutations, all pass (`FAILURES: none`). The checker caught one real
  error (BE70 bits) before the report was written.
- Re-read raw: tick prologue/tail (772 lines), `FUN_140A42A70/401C0/401F0/429A0/
  48A30`, `FUN_14057F600` + `53CB40/53C790/53D250/53C890/53CC00/53D3B0`,
  `FUN_140A408C0` (184), `FUN_14053C6E0` (46). Decompiles used for orientation
  only (§§2.1–2.2 marked INF detail).
- NOT run: no GF build/tests (decode-only assignment); no live PES reads;
  no bridge calls beyond disassembly/decompile reads. Bridge-request count this
  session: ~25 (10 fetches + 9 decompiles + 6 single-shot reads), well under budget.
