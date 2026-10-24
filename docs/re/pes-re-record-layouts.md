# PES record layouts: the A3E190 ring, the slot record, the 6E150 class byte, the need chain (RecordLayoutRE, 23-10-26)

Read-only RE. Image `~/ghidra-projects/scratch/Unpacker/pes_full.bin`
(file offset = VA − 0x140000000). Bridge `http://127.0.0.1:8080`, one request
at a time. Scratch: `<workdrive>/gf-RecordLayoutRE-scratch/`
(`d_A3E190.c`, `d_6CA10.c`, `d_6E150.c`, `d_6E340.c` = bridge decompiles saved
this pass; `check_record_layouts.py`: RESULT OK — 11 floats + 6 opcode sites
+ 10 structural rows; `--selftest` OK: base clean, 3 mutations each caught).
Prior: `docs/re/pes-re-holder-tree.md` (HT-OPEN-01, HT-INF-01),
`docs/re/pes-re-team-helpers.md` (§6 spine), `docs/re/pes-re-mark-slot.md`
(§2 action word).

Verdicts: **CONFIRMED** = decompile + asm opcode bytes read this pass;
**INF** = inference; **OPEN** = not decoded, blocker named.
Closes holder-tree HT-OPEN-01 (layouts + writers) and corrects HT-INF-01
(the 0.35 scale is NOT read by F0E50 — §5).

## §0 Constants (script-read, never typed; checker float rows)

```
0x14257A194 f32=0.7  bits=3333333f  (F0E50 0x1404F37AC MULSS — the only 0.7 site in F0E50)
0x142565600 f32=0.5  bits=0000003f  (F0E50 lag addend / int-noise addend)
0x14259BDDC f32=1.5  bits=0000c03f  (F0E50 0x1404F37C3 MULSS-arm scale)
0x14259D5C8 f32=0.35 bits=3333b33e  (NOT read by F0E50 — §5; belongs to another subsystem)
class table u32s @ 0x1426484F0[idx*4] (§3 table)
```

## §1 The A3E190 ring (800-byte ring) — CONFIRMED

Reader (`d_A3E190.c`, bridge decompile this pass, byte-verified):

```c
int *FUN_140a3e190(longlong p, uint k) {
  if (k == 0) return (int *)(p + 0x180698);   // current record (not ring-indexed)
  if (k < *(uint *)(p + 0x1900)) {            // count (u32)
    i = *(int *)(p + 0x1904) - k;             // head (s32)
    if (i < 0) i += 8;
    e = (int *)(i * 800 + p);                 // stride 800 = 0x320
  } else e = &DAT_1436f3a40;                  // default record
  if (*e == 0x37) return &DAT_1436f3d60;      // carded-out default
  return e + 5;                               // +20: returned pointer is entry+0x14
}
```

Layout of one lane block (p = T+0xE0 lane base):

| offset | size | meaning | evidence |
|---|---|---|---|
| `+0x0 … +0x1900` | 8 × 800 B | ring entries (ring i at `p + ((head−k) mod 8)·800`) | stride `* 800` (`69 … 20 03 00 00`-family; push leaf `IMUL RAX,RAX,0x320` at `0x140A3DB02` CONFIRMED bytes) |
| `+0x1900` | u32 | count (saturates at 8) | push leaf `INC [RCX+0x1900]` (`FF 81 00 19 00 00` @ `0x140A3DAC0` CONFIRMED), clamp `CMP EAX,8 / CMOVA` @ `0x140A3DACF` |
| `+0x1904` | s32 | head index (wraps mod 8) | push leaf `INC [R8+0x1904]` + `CMP EAX,8 / CMOVNC` @ `0x140A3DAD9-F6` |
| `+0x180698` | 800 B | **current record** (k==0 shortcut; NOT a ring slot) | `LEA … [0x180698]` refs in A3E1xx range (`0x140A3E065/197/4B0/90B/9F0` CONFIRMED bytes) |

Entry fields read by F0E50/T9 (offsets relative to the **entry base**;
the reader returns entry+0x14, so the decompile's `+0x10` = entry+0x24 etc. —
table below uses **entry-base** offsets, converted):

| entry offset | size | meaning in F0E50 | instruction (opcode-checked) |
|---|---|---|---|
| `+0x0` | s32 kind | kind gates: 1,2,3,4?,5,6,7,9..14(0xE),16(0x10); `0x37` = carded-out | `*piVar18` CMP family throughout; `0x1404F1A2A MOV ECX,[RAX+0x38]` is +0x24-mapped (see note) |
| `+0x4` | u32 id/slot | `62EA0` holder-match input; compared vs `T+0x6D00`/candidates | `0x14057FE3F MOV RDX,[RAX+0x4]` (T9); `0x1404F1A66 LEA RCX,[RAX+0x4]` + `60300` (F0E50) |
| `+0x10` | s32 frame | `(frame − recFrame)` windows vs `(long)(rnd·rate+0.5)` | `0x1404F1862 SUB EBX,[RAX+0x10]`, `0x1404F1B05 CMP [RAX+0x10],EBX` |
| `+0x1C` | s32 class | gates `==2 / ==4 / ==6 / !=2` | `0x1404F130A CMP [RAX+0x1C],0x2`, `0x1404F143E …0x4`, `0x1404F1A43 …0x6` |
| `+0x20` | s32 mode | `==10` / `!=10` arms | `0x1404F136F CMP [RAX+0x20],0xA`, `0x1404F311B`, `0x1404F42A5` |
| `+0x38` | s32 field | `1..2` range gate (0xE-kind); `==3` / `==4` tests | `0x1404F1A2A MOV ECX,[RAX+0x38]` + `DEC/JA` (asm), `0x1404F2A75 CMP byte [RBX+0x38],0x3`, `0x1404F4639 CMP byte [R13+0x38],0x4` |
| `+0x38`/`+0x3C` | f32 pair | `76240` target point (aim geometry) | `0x1404F414C MOVSS XMM10,[RAX+0x38]`; `0x1404F4193 SUBSS XMM1,[RDI+0x40]`-family lerp arm |
| `+0x40`/`+0x44` | f32 pair | lerp endpoints (`out = (a−b)·t + b`) | `0x1404F335F MULSS XMM4,[RAX+RCX*4+0x40]`, `0x1404F4193/419C` SUBSS/ADDSS arm |
| `+0x45` | bool | `0x400`-bit arm flag | `0x1404F13E0 CMP byte [RAX+0x45],0x0` |
| `+0x78` | u32 slot/id | winner-compare (`rec+0x78 == candidate`) | `0x1404F1480-region` (decompile `*(uint *)(lVar16+0x78) == uVar39`); T+0x78 is the SEPARATE team word (§4) |

Note on the `+0x14` return bias: `A3E190` returns entry+0x14 (the `+5` ints),
so every decompile offset on its result = entry offset +0x14
(e.g. decompile `+0x38` on the result = entry `+0x4C`). The table above is
already converted to entry-base. The `T+0xE0 → uVar4` chain
(`param_1+0x6D48 → +0xE0`) is holder-tree §2 DOC, not re-checked.

Index: F0E50 calls `A3E190(uVar4,0)` at 40+ sites (all `XOR EDX,EDX` = k=0
CONFIRMED by disassembly sweep); the ONLY k=1 site is `0x1404F1A3E`
(`MOV EDX,EBX` with EBX=1, entry+1 class-`==6` gate). T9 loops call
`A3E190(T+0xE0,0)` + read `+0x4` (62EA0 input) and `+0x1C` (class)
(`0x14057FE37-63` CONFIRMED).

Writer (CONFIRMED this pass): the `0x140A3DAC0` leaf (undefined bytes,
byte-read):
count = min(count+1, 8); head = (head+1) mod 8; dst = base + head·0x320;
copy 800 B (`ECX=6` × 0x80 + 2 × 0x10 MOVUPS = 0x320 = 800).
Sole caller `0x140A3DFF9` in `FUN_140A3DC40` (bridge xrefs CONFIRMED).
`A3DC40` builds the record on its stack (two `A3CDD0` fills + src select),
then calls the leaf with `RCX = RBX + lane·0x1908` for lane index
`[0x142642E60+RSI*4] < 10` (`IMUL RCX,RCX,0x1908 / ADD RCX,RBX` @
`0x140A3DFEF-FF6` CONFIRMED), else the `+0xFA50` overflow lane
(`0x140A3E000 LEA RCX,[RBX+0xFA50]`).
The sibling multi-lane reader `A3E150(base,lane,k)` = lane·0x1908 + same
8×800 walk (bridge decompile this pass); 10 lanes × 0x1908 = 0xFA50 —
the overflow-lane offset. So the T+0xE0 block ≥ 0xFA50 + lanes, plus the
current-record area at +0x180698 (written by `A3DC40` only on the `ESI==7`
arm: six `MOVUPS [RBX+0x180698…]` + `[RBX+0x1806F8]` @ `0x140A3E062-0A3`,
and the `+0x1806FC…` second copy on the `60300` arm @ `0x140A3E0BA-0FB`).
A3E880/A3E890 return `rec+0x180700` / `rec+0x1806FC` (bridge decompiles).

Status: entry layout + index + count/head + writer chain CONFIRMED.
Per-field *semantics* (which game concept each kind/class value is) stay
INF — the value sets observed are listed, not interpreted.

GF mapping: NO 1:1 carrier (no 800-byte ring; `MentalImage` predictions are
the functional analogue per holder-tree §3). Exactly what's needed: per-team
record ring (8 × 800 B entries + u32 count + s32 head per lane, current
800 B record; fields `+0x0,+0x4,+0x10,+0x1C,+0x20,+0x38,+0x40/+0x44,+0x45,+0x78`).

## §2 The slot record (`+0x5D0`, `+0x1430`, `+0x1438`, …) — CONFIRMED

Tables (bridge decompiles this pass; stride form):
`A48910(G,slot) = *(G + (slot+0xA5)·0x28)` (slot<0x1A else `+0x19C8`);
`A48A10(G,slot) = *(G + (slot+0xBF)·0x28)`; `A489F0(G,slot) = *(G + (slot+0xF3)·0x28)`
(slot<0x16 else `+0x25F8`); `A489D0(G,side) = *(G + (side+0xA3)·0x28)`
(side<2 else `+0x1978`). Slots are global 0..21 (`%11` folds to within-side).

Slot-record fields read by F0E50 (sizes from opcode widths — all CONFIRMED):

| offset | size | readers (F0E50 sites) | writer |
|---|---|---|---|
| `+0x1` | u8 class byte | `6E150/340/050/4B0/580/5E0/1C0` inputs (§3) | §3 |
| `+0x8` | u32 bits | `>>0x16&1`, `>>9&1` flag arms (decompile l.~1010/1130) | OPEN (flag vocabulary) |
| `+0x1A` | u16 lo | need-index/counter ladder (`< 0x1C`, `rnd+lo < hi`) | `FUN_14092ED30` (§2.1) |
| `+0x1C` | u16 hi | same ladder | `FUN_14092ED30` |
| `+0x22` | u16 cap | `hi < lo && cap != 0 && rnd·0.1+… < cap` | `FUN_14092ED30` |
| `+0x2C` | s32 flag | `!= 0` arm @ `0x1404F379D` (`CMP [RDI+0x2C],0`) | `FUN_14092ED30`-family (record build) |
| `+0x38` | s8/char | `== 0x03` / `== 0x04` arms (`CMP byte [RBX+0x38],0x3` @ `0x1404F2A75`; decompile `+0x38 != 4` @ l.1592) | record build |
| `+0x55C`/`+0x564` | f32 x/y | man position (all geometry arms) | tick movement |
| `+0x574`/`+0x57C` | f32 vx/vy | velocity (aim extrapolation) | tick movement |
| `+0x5D0` | s32 action | `== 5` / `== 0x15` / `-3U<3` arms | `FUN_140A6CA10` (mark-slot §2 CONFIRMED: `+0x5D4=old, +0x5D0=new, +0x6B8` repeat count; sites `0x14092D607` copy `+0xB5AC→+0x5D0`, `0x140A36D6F` 1/0x72) |
| `+0x5D4` | s32 prev | (kept by 6CA10; no F0E50 read) | `FUN_140A6CA10` |
| `+0x6B8` | s32 repeat | (kept by 6CA10; no F0E50 read) | `FUN_140A6CA10` |
| `+0x1414`/`+0x141C` | f32 pair | geometry deltas (`SUBSS XMM_,[R_+0x1414]` @ `0x1404F225C/228A/33DB/340E`) | `FUN_14092ED30` block copy (`+0x1408…+0x143C` rows) |
| `+0x1430` | s32 counter A | `+ iVar11 < (int)(rnd·0.5)`; `rnd·1.5` compare; `A−B < (int)(rnd·1.25)`; `57450` input | `FUN_14092ED30` (writes `[R15+0x1430]=EAX` @ `0x14092F1CF` CONFIRMED bytes `41 89 87 30 14 00 00`); value = `local_f0` need-blend out (decompile l.~196-205) |
| `+0x1438` | s32 counter B | `A−B` (0x1404F3632 `SUB EAX,[RDX+0x1438]`); `uVar12/3` folds; `-0.3` trim; winner margins | `FUN_14092ED30` (writes `[R15+0x1438]=EAX/UVar7` @ `0x14092F204` CONFIRMED bytes `41 89 87 38 14 00 00`); value = `928120`-gated `local_f0` |
| `+0x1457` | u8 sub | `== 0x03` / `== 0x05` with action-`==5` | record build |
| `+0x1470` | bool | `!= 0` holder-present arm | record build |
| `+0x1748` (G-base) | bool | on-pitch per global slot | roster |

### §2.1 Counter writer `FUN_14092ED30` (slot, need-blend out) — CONFIRMED

Writes `[R15+0x1430]` unconditionally and `[R15+0x1438]` on the
`[RSP+0x33]==0` arm (disassembly `0x14092F1CF-204` CONFIRMED); both values
derive from `local_f0` (the `677700`-need-blend output u32), the `+0x1438`
value passing the `FUN_140928120` gate when `local_f8._3_1_ == 0`.
Preceding stores in the same function: `[R15+0x1448]=local_f4`,
`[R15+0x143C…]=RSI[0…8]` copy, `[R15+0x1414/1418/141C]` + `[R15+0x1408/140C/1410]`
aim rows, `[R15+0x142C]`, `[R15+0x1404]`, `[R15+0x1403]`.
R15 = the slot record (`param_2`); counts as the slot-record init/push path.
Deeper value vocabulary (which match states yield which counts) stays INF.

GF mapping: PARTLY (`PlayerData`/formation role + `GetTimeNeededToGetToBall_ms()`
cover need/role fragments; `pescarrier.hpp:1014 action` covers `+0x5D0`
narrowly). Exactly what's needed: per-slot `actionWord` (`+0x5D0/5D4/6B8`
triple) + two int counters (`+0x1430/+0x1438`) + the `+0x1A/1C/22` u16 triple
— all fed from the carrier/command layer (`// ponytail:` naming
`FUN_140A6CA10` + sites and `FUN_14092ED30`).

## §3 The 6E150-family class byte — CONFIRMED (byte, table, values)

Which byte: the slot record's `+0x1` byte (`*(char *)(rec + 1)` at every
F0E50 call site — decompile l.280/300/450/1004/1132 CONFIRMED; T9 loops read
the same byte through `T+0x4C00`-family class words, team-helpers §4).

Semantics: seven one-bit predicates over one shared u32 table
`0x1426484F0[idx·4]` (94 rows, idx = class byte; `> 0x5D` → false):

| fn | disassembly (CONFIRMED) | bit |
|---|---|---|
| `6E150` | `MOV EAX,[RCX+RAX*4] / SHR EAX,3 / AND AL,1` (@ `0x140A6E162`) | word bit 3 |
| `6E1C0` | `… SHR EAX,4 …` | word bit 4 |
| `6E5E0` | `… SHR EAX,5 …` | word bit 5 |
| `6E580` | `… SHR EAX,6 …` | word bit 6 |
| `6E4B0` | `… SHR EAX,7 …` | word bit 7 |
| `6E340` | `MOVZX EAX,byte [RCX+RAX*4+2] / AND AL,1` (@ `0x140A6E352`) | byte+2 bit 0 (= word bit 16) |
| `6E050` | `… & 0x34000 …` (decompile) | word bits 14/18 |

Values (script-read this pass; F0E50-observed classes decoded):

| class | word | 150 | 1C0 | 5E0 | 580 | 4B0 | 340 | 050 | F0E50 role |
|---|---|---|---|---|---|---|---|---|---|
| `0x01` | `0x00000000` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | T7a-admitted (`+0x5D0` pair member) |
| `0x03` | `0x00000005` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | – |
| `0x04` | `0x00004008` | 1 | 0 | 0 | 0 | 0 | 0 | 1 | admitted by 6E150-arm |
| `0x05` | `0x0000400C` | 1 | 0 | 0 | 0 | 0 | 0 | 1 | action-`==5` carrier class |
| `0x06` | `0x0000400C` | 1 | 0 | 0 | 0 | 0 | 0 | 1 | – |
| `0x07` | `0x00004020` | 0 | 0 | 1 | 0 | 0 | 0 | 1 | – |
| `0x09` | `0x00010000` | 0 | 0 | 0 | 0 | 0 | 1 | 1 | – |
| `0x0B` | `0x00004080` | 0 | 0 | 0 | 0 | 1 | 0 | 1 | – |
| `0x0E` | `0x00004100` | 0 | 0 | 0 | 0 | 0 | 0 | 1 | – |
| `0x10` | `0x00003002` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | F0E50 `cVar9=='R'`-arm class (switch-case `0x10/0x11/0x15/0x50/0x52/0x57`) |
| `0x11` | `0x00003000` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | same arm (`0x11/0x14/'R'/'W'` retest) |
| `0x14`/`0x15` | `0x00000002` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | same arm |
| `0x3F` | `0x00020000` | 0 | 0 | 0 | 0 | 0 | 0 | 1 | T7a-admitted (050 bit 18) |
| `0x50/52/57` | `0x08000000/…/0x0C000000` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | same switch arm |

F0E50's class logic (CONFIRMED dataflow, decompile l.449–530 + l.1004–1035):
admit-`uVar39` arm requires `6E150==0 && (6E5E0==0 || w22) && (6E340==0 || w9)`
(l.449); the `param_2`-seed arm admits classes `{0x10,0x11,0x15,0x50,0x52,0x57}`
verbatim plus `0x14/'R'/'W'` on the retest (l.330–352, all-zero predicate rows
above — i.e. these classes pass by having NO bits set); the tail retest
(l.1004) inverts: `6E150 || 6E5E0&&!w22 || 6E340&&!w9` with `1A<1C`.
The `6E050/4B0/580` triple gates the carrier-need arm
(`050 && !4B0 && !580 && lo<hi…`, l.300–311). All six predicates also gate the
T9 take loops through `T+0x4C00`-family words (team-helpers §4 CONFIRMED).

GF mapping: NO 1:1 carrier for the `+0x1` byte (the `pesroles.hpp` class map
covers formation role classes, a different vocabulary). Exactly what's needed:
per-slot class byte + the 94-u32 table (ports verbatim — 376 bytes) + the
seven one-line predicates (port verbatim — each is 3 instructions).

## §4 T+0x78 and its writers — CONFIRMED (no change from holder-tree §2)

`T+0x78` = per-side u32 holder id (0..21, `0xFF` = none). F0E50 reads it
(`0x1404F12xx-region` + `local_1e8` seed) and returns the winner; the caller
`FUN_1404FB380` stores `T+0x78 := F0E50(...)` (holder-tree §2 CONFIRMED).
F0E50 itself stores `param_1+0x6D00` (Y-side word, `0x1404F4677
MOV [RSI+0x6D00],R14D`). Writers of `T+0x78`: lane-leaves §4 chain
(`4FB2A0→4FB380→F0E50`, independently ported). GF: `kickMan/kickValid`
stand-in (`teamAIcontroller.cpp:2446-2448`) — closable with a per-side id
word + the MI play-state words (holder-tree §3 Named 5/8).

## §5 The need chain and the 0.35/0.7 scales — CORRECTED

Need value (CONFIRMED this pass): `FUN_14057D930(T,side,slot)` and
`FUN_1404C8E00(T,side,slot)` are the same 12-instruction body:
`needrec = A714C0/A714A0(side-key)`; `idx = u8(slot + needrec + 0xB2)`;
`need = f32(needrec + (idx + sideFold)·8 + 4)` with sideFold = 0/11;
result through the sqrt dispatch (`JMP 1415A1780`).
Side-keyed record selects (bridge decompiles this pass):
`A714C0 = side? +0x228EC : +0x22AD4`; `A714A0` = swapped pair;
`A71500 = side? +0x229B4 : +0x22B9C`; `A71540 = side? +0x228D4 : +0x228D8`
(u32 cells); `A71460 = side? +0x228E4 : +0x228DC` (u32 cells).
Upstream need-record layout: per-side table with a u8 index array at `+0xB2`
(≥22 entries) + f32 table at 8 B stride (float at +4 of each 8 B entry).
The u8-index writer and the float-table writer stay OPEN (one scan).

Scales — correction to holder-tree HT-INF-01 and mark-slot §0/§3:
F0E50's disassembly (3424 lines, exhaustive `14259…` census this pass)
contains **exactly one** 0.7 read — `0x1404F37AC MULSS XMM0,[0x14257A194]`
in the head need-gate arm (`(int)(rnd·0.7) >= iVar13 → set bVar40`, decompile
l.1085–1088) — and **zero** reads of `0x14259D5C8` (0.35). The `×0.35`
"need-chain scale" cited in holder-tree §1.2 / mark-slot §0 is NOT a F0E50
input: no `59D5C8`/`D5C8` reference exists anywhere in `dis_4F0E50.txt`
(case-insensitive sweep, this pass). The 0.35 word belongs to a different
subsystem's arm (present in the image, value CONFIRMED `0x3EB33333`).
HT-INF-01 is therefore SPLIT: the 0.7 arm is CONFIRMED (site + semantics);
the 0.35 arm is REMOVED from the F0E50 port spec (no site, no writer needed).
The 0.7 upstream need-record layout above is CONFIRMED (reader bodies +
selects); only the need-table *writers* stay OPEN.

GF mapping: `GetTimeNeededToGetToBall_ms()` is the need analogue
(`playercontroller.cpp:316`; holder-tree §3 PARTLY). Exactly what's needed:
per-side need table (u8 index array + f32 table, `// ponytail:` naming
`A714C0/A714A0` + `FUN_14057D930/1404C8E00` until the writers land) +
the one 0.7 gate constant (ports verbatim).

## §6 GF 1:1 or exactly-what-is-needed (read-only grep over `src/`, origin/main)

| PES input | GF carries 1:1 | Exactly what is needed |
|---|---|---|
| ring lane block (T+0xE0, §1) | NO | new: per-team lane block (10 × 0x1908 lanes × (8×800 B + u32 count + s32 head) + 800 B current record) |
| ring writer (A3DC40 + leaf) | NO | new: `RecordPush(lane, record)` (count/head update + 800 B copy; ports verbatim) |
| slot tables (A48910/48A10/489F0/489D0) | NO | new: side/slot record tables (strides `(slot+{0xA5,0xBF,0xF3})·0x28`, fallbacks `+0x19C8/+0x1DD8/+0x25F8/+0x1978`) |
| slot actionWord triple | PARTLY (`pescarrier.hpp:1014 action`) | new per-slot: s32 `+0x5D0/5D4/6B8` (fed by 6CA10-copy; `// ponytail:` names both sites) |
| slot counters `+0x1430/1438` | NO | new per-slot: two s32 (fed by `FUN_14092ED30`; `// ponytail:`) |
| slot u16 triple `+0x1A/1C/22` | NO | new per-slot: three u16 (same feed) |
| class byte `+0x1` + 94-u32 table | PARTLY (`pesroles.hpp` map = different vocabulary) | new per-slot u8 + 376 B table + 7 predicates (all verbatim) |
| need tables (side-keyed) | PARTLY (`GetTimeNeededToGetToBall_ms()`) | new: u8-index + f32 tables per side (`// ponytail:` names selects) |
| 0.7 constant | NO | one float (ports verbatim) |
| 0.35 constant | n/a | NOT NEEDED by F0E50 (§5 correction) |
| T+0x78 / T+0x6D00 words | PARTLY (`kickMan/kickValid`) | new: two per-side id words |
| T+0x254 sign | YES (`attackSign`, `pescarrier.hpp:582`) | 1:1 |
| man/aim geometry | YES (`+0x55C/564/574/57C` analogues) | 1:1 |
| MI frame/phase words | PARTLY (`GetActualTime_ms`, `e_MatchPhase`) | frame counter in PES ticks + phase-word map |

## §7 Port spec (replaces the F0E50 `// ponytail:` leaf; updates holder-tree §4)

```cpp
// Records (new state, §1/§2/§3):
//   RingLane { uint8 e[8][800]; uint32 count; int32 head; };  // +0x1900/+0x1904
//   cur[800];                                                // +0x180698 k==0
//   A3E190(lane,k): k==0 ? cur : (k<count ? e[(head-k)%8] + 20 : default)
//                   (+0x37-kind → second default). k==1 only at 0x1404F1A3E.
//   Slot { …; uint8 cls; …; int32 act,actPrev,actRep; int32 ctrA,ctrB;
//          uint16 lo,hi,cap; … };                            // §2 table
//   ClassTable[94] = {…}; // 376 B verbatim; predicates 6E150/1C0/5E0/580/4B0/340/050 verbatim (§3).
//   Need tables per side: uint8 idx[] @+0xB2; float tab[] stride 8 (float @+4) (§5).
// Score (unchanged from holder-tree §4) + the one 0.7 gate:
//   head need-gate (0x1404F37AC): bVar40-arm iff (int)(rnd*0.7) >= iVar13 (CONFIRMED; no 0.35 input).
// Take-over: strict > (ties keep incumbent), +1.0 tie-extra (holder-tree HT-07/HT-08).
// Output: T+0x6D00 := winner (F0E50); caller: T+0x78 := winner or 0xFF.
```

Order inside F0E50 (CONFIRMED): head gates → param_2-seed arm (class
`{0x10,0x11,0x15,0x50,0x52,0x57}` + `0x14/'R'/'W'` retest) → geometry preamble
→ candidate loop (early-return arms + 0.7-gated need arm + score build) →
take-over compare (`JLE` strict) → record/geometry cross-check →
`T+0x6D00 := winner` / `0xFF` (holder-tree §2 + §1.4).

## §8 Evidence ledger

| ID | Claim | Verdict | Evidence |
|---|---|---|---|
| RL-01 | ring entry 800 B, count `+0x1900`, head `+0x1904`, k==0 `+0x180698`, `+0x37` default | CONFIRMED | `d_A3E190.c` + `0x140A3DAC0 FF 81 00 19 00 00` + clamp `83 F8 08` + `IMUL …,0x320` @ `0x140A3DB02` |
| RL-02 | ring-push leaf + sole caller `0x140A3DFF9` in `A3DC40`; lane `·0x1908`, overflow `+0xFA50`, current-record `+0x180698` writers | CONFIRMED | disassembly `0x140A3DAC0-DB78` + `0x140A3DFEF-FF6` + `0x140A3E062-0FB` |
| RL-03 | F0E50 uses k==0 everywhere except `0x1404F1A3E` (k==1) | CONFIRMED | disassembly sweep: 40+ `XOR EDX,EDX` vs one `MOV EDX,EBX` (EBX=1) |
| RL-04 | slot-table strides `(slot+{A5,BF,F3})·0x28`, fallbacks | CONFIRMED | bridge decompiles `A48910/A48A10/A489F0/A489D0` |
| RL-05 | slot fields + sizes (`+0x1` u8 … `+0x1438` s32) | CONFIRMED | opcode widths @ sites in §1/§2 tables |
| RL-06 | counters written by `FUN_14092ED30` @ `0x14092F1CF/204` (`41 89 87 …`) | CONFIRMED | bytes + disassembly + decompile value chain (`local_f0` → `1430`, gated → `1438`) |
| RL-07 | action triple by `FUN_140A6CA10` (sites `0x14092D607`, `0x140A36D6F`) | CONFIRMED | `d_6CA10.c` + mark-slot §2 (carried, not re-read) |
| RL-08 | class byte = slot `+0x1`; 7 predicates = table bits (§3) | CONFIRMED | `d_6E150.c`/`d_6E340.c` + `0x140A6E162 C1 E8 03` + `0x140A6E352 0F B6 44 88 …` + 14 class words script-read |
| RL-09 | class value sets in F0E50 arms | CONFIRMED | decompile switch/retest sites + word table (§3) |
| RL-10 | need readers `57D930/4C8E00` = idx-byte `+0xB2` + `(idx+fold)·8+4` float + sqrt | CONFIRMED | disassembly `0x1404C8E00` (12 insns) + selects `A714C0/A714A0/A71500/A71540/A71460` |
| RL-11 | 0.7 read once @ `0x1404F37AC`; 0.35 NEVER read by F0E50 | CONFIRMED | exhaustive `14259…` census of `dis_4F0E50.txt` (143 float refs; 0 × `59D5C8`) |
| RL-12 | checker + mutations | CONFIRMED | `check_record_layouts.py`: RESULT OK; `--selftest` OK (3/3 caught) |
| RL-INF-01 | need-table writers (u8-index + float-table feeds) | OPEN | blocks exact need values; readers + selects ported independently |
| RL-INF-02 | slot flag vocabularies (`+0x8` bits, `+0x38/0x1457/0x1470` enums, counter value vocabulary) | OPEN | blocks exact replays; words port independently |

Corrections to prior docs: holder-tree §1.2 "0.35" row and §5 HT-INF-01
(0.35 is not a F0E50 input — §5 above); mark-slot §0/§3 "4F0E50 x0.35 arm"
(same mis-cite; the 0.35 word exists in the image but is read elsewhere).
The 0.7 half of HT-INF-01 is CLOSED (RL-10/RL-11).

## §9 Open items (for Main)

1. Need-table writers (RL-INF-01) — one scan: who stores the `+0xB2` index
   bytes and the 8-stride float tables (side-keyed `+0x228EC/+0x22AD4` family).
2. Slot flag vocabularies (RL-INF-02) — value-enumeration pass, non-blocking.
3. T+0xE0 lane count/ownership (10 lanes + `+0xFA50` overflow + `ESI==7`
   current-record arm) — needs the `A3DC40` caller frame walk to name which
   lane is the holder tree's.
4. Live confirmation (read-only): `T+0x6D00` store cadence; ring count/head
   advance during a match.

## §10 What was verified, what was not

- Ran: `check_record_layouts.py` — 11 floats + 6 opcode sites + 10 structural
  rows, all pass; 3 mutations each caught (M1 float bits, M2 push opcode,
  M3 ring stride).
- Re-read raw: `A3E190/A6CA10/6E150/6E340` decompiles (bridge, saved),
  push-leaf + `A3DC40` + `A3DA00` + `92ED30` + `4C8E00` bodies (disassembly),
  F0E50 full float-ref census (143 refs), class table 14 words, counter-store
  bytes, ring-reader `180698/1900/1904` bytes.
- NOT run: no GF build/tests (decode-only assignment); no live PES reads;
  bridge requests this session: ~30 (one at a time per the brief).
- Budget: ~110 tool requests used of ~150.
