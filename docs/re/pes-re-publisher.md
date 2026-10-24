# PES publisher RE: 27A0 writer-side opens (PublisherRE, 24-10-26)

Read-only RE. Image `~/ghidra-projects/scratch/Unpacker/pes_full.bin` (base 0x140000000,
file offset = VA − 0x140000000). Bridge one request at a time; `dec.sh` for undefined code.
Read-first: `pes-re-27a0-inputs.md` (the read side; §1–§7 CLOSED there).
Tags: **CONFIRMED** = bytes/decompile this session; **INF** = inference; **OPEN** = blocker named.
Checker: `check_publisher.py` (this scratch dir), ALL GREEN 24-10-26; mutation (flag80.init0
imm `00`→`01`) drives exactly that group RED, reverted (mut file removed).

## §0 Script-read constants (checker output, pasted)

```
flag80.init0 c6858000000000     (0x140503803 MOV byte [RBP+0x80],0)
flag80.set1  c6858000000001     (0x140503864 MOV byte [RBP+0x80],1)
flag80.read  0fb69d80000000     (0x140503CCF MOVZX EBX,[RBP+0x80])
card.cls     0fbe4801           (0x140503857 MOVSX ECX,[RAX+1])
card.call6e1c0 e860a95600       (0x14050385B CALL 0x140a6e1c0, bit 4)
card.b38     0fb64338           (0x14050387A MOVZX EAX,[RBX+0x38])
card.bit11   8b4808             (0x140503CF5 MOV ECX,[RAX+8])
card.w1c     0fb7481c           (0x140503D01 MOVZX ECX,[RAX+0x1C])
card.w1a     6639481a           (0x140503D05 CMP [RAX+0x1A],CX)
card.f60     f30f107060         (0x140503D0B MOVSS XMM6,[RAX+0x60])
card.f11c    f30f10b01c010000   (0x140503D21 MOVSS XMM6,[RAX+0x11C])
card.b40     807b4005           (0x140503DAE CMP byte [RBX+0x40],5)
keep.mov     488b4840           (0x140503020 MOV RCX,[RAX+0x40])
keep.shr     48c1e924           (0x140503024 SHR RCX,0x24)
keep.test    f6c101             (0x140503028 TEST CL,1)
rate.cmp     80bd80000000       (0x140502C60 CMP byte [RBP+0x80],0)
rate.stat14  0fb64814           (0x140502C69 MOVZX ECX,[RAX+0x14])
rate.stat15  0fb64815           (0x140502C6F MOVZX ECX,[RAX+0x15])
flg.0xB      41b80b000000       (0x140502F26 MOV R8D,0xB)
flg.0xC      41b80c000000       (0x140502F60 MOV R8D,0xC)
anch.lea     8dbd14140000       (0x1405036D5 LEA R15,[R13+0x1414])
vol.read     450fb6b42480000000 (0x14063AA9A MOVZX R14D,[R12+0x80])
vol.test     4584f6             (0x14063AAA3 TEST R14B,R14B)
copy.w1a     6689411a           (0x140430548 MOV [RCX+0x1A],AX)
copy.dst38   884138             (0x1404305C7 MOV [RCX+0x38],AL)
pub.slot4    488b89d8070000     (0x1409299A0 MOV RCX,[RCX+0x7D8])
```

Operand-VA pairs re-checked against disassembly text this session (§constants rule).

## §1 The anime publisher (card words + keeper bit) — CHAIN CONFIRMED, one link INF

Publisher chain (CONFIRMED, dec.sh + bridge decompiles + image bytes):

```
match::Player vtable slot 4 = FUN_1409299A0            (vtable 0x1426329A8 + 0x20;
                                                         also in 0x142641E98/0x142642110/0x142642368)
  lVar3 = *(PM + 0x7D8) = P (AnimePlayer)               (pub.slot4 bytes above)
  dst = FUN_1406EDC70(*(P + 0x3AD0))                    (lazy cached pointer:
                                                         *(handle+0x10) + 0x180, +0x2970 variant)
  FUN_140430500(dst, P + 0xAC8)                         (sole caller: FUN_1406FA780(P);
                                                         thunk at 0x1404335AB is a JMP, E9 50CFFFFF)
```

`FUN_140430500` is a 0x180-byte struct copy covering **every 27A0 card word**:
+0x01 (class), +0x08 (bit-11 dword), +0x1A/+0x1C (clip/contact frame, copy.w1a),
+0x38/+0x40..0x47 (class bytes incl. keeper qword, copy.dst38), +0x60, +0x11C.
Source base `P+0xAC8` = the anime-action record (a3helpers: `FUN_1406Eebb0` sets
action id at +0xAC8/+0xAC9). Motion-issue family `FUN_140753FC0` (vtable slot shared
by 7 anime-action vtables: 0x1425E73C8/0x1425E7B20/0x1425E7E30/0x1425E80F0/0x1425E83B0/
0x1425E8670/0x1425E8D00) fills the source words (+0x18/+0x1A/+0x1C seen in decompile).

Frame timing: slot 4 is a virtual dispatched per-frame (`FF 50 20` sites cluster in
0x14090xxxx–0x1409Fxxxx player-tick code; 19 sites). Exact caller order INF.

One link INF: `dst == PA (*(G+(id+0xA5)*0x28))` is not proven end-to-end (the
`P+0x3AD0`-handle → PA-pointer-table link is one dereference short). What IS proven:
the copy moves all six card words + keeper qword from motion state to a published
slot every frame, and 27A0 reads them from PA.

GF carriage:
- Card words: `CardFacts::{cls,b38,b40,w1a,w1c,f60,f11c,bit11AtEight}`
  (`src/sim/onthepitch/pesreactionblock.hpp:115-124`) carries all six 1:1 as fields.
  No GF producer fills them. Port spec: fill from `HumanoidBase::currentAnim`
  (`humanoidbase.hpp:360`, with `GetFrameNum()`/`GetFrameCount()`/`GetCurrentAnim()`)
  + `pes::AnimetableCell::motionNo` (`src/pes/animetable.hpp:37`) when the publisher
  lands: w1a = current frame, w1c = touch/contact frame, b38/b40/cls from the motion's
  class row, f60/f11c from the motion record. Until then caller-supplied with
  `// ponytail:` naming `0x140503CF5/D01/D05/D0B/D21/DAE`.
- Keeper bit: **NO 1:1** (grep: no `keeperBit` in `src/`). Port spec: add
  `bool keeperBit36` beside `bit11AtEight`; source from the publisher (bit 36 of the
  published +0x40 qword). Keeper-motion classes 0x38/0x3D (duel-sim §3) are the
  family hint. Until then `// ponytail: 0x140503020/024/028`.
- GF already carries the motion source: `currentAnim` + `motionNo`. No new GF state
  needed on the source side; only the six destination fields (exist) + keeper bit (new).

Status: chain **CONFIRMED**, dst==PA **INF** (one link), timing **INF** (slot-4
per-frame virtual, exact order OPEN), GF map **CONFIRMED** (grep 24-10-26).

## §2 B+0x80's writer — disambiguated (CONFIRMED), ball writer OPEN

Two different +0x80s (CONFIRMED disassembly):

- (a) **Ball B+0x80** (the ticket's item): volatile AND-gate reader
  `0x14063AA9A MOVZX R14D,[R12+0x80]` (R12 = ball `*(G+0x40)`), `0x14063AAA3 TEST`.
  Full gate (`FUN_14063A490` decompile): rate test
  (`*(FUN_14124DAD0+0x31)/c < param_1[0x11]`) AND `B+0x80 == 0` AND co-gate
  `FUN_140A6D0D0(B) = B+0x4C != 0` AND `!(d2 > d1)` → else arm `param_1+0x6E = 1`.
  Writer: **OPEN**. Eliminated: `C6 40 80` (all imm 0xFE = other struct),
  `C6 45 80`/`C7 45 80` (rbp stack locals), `88 xx 80` hits (verified misaligned,
  e.g. `0x140458E6C` sits inside `80 BC 04 …` CMP), `FUN_140430C30` ball copier
  (copies only +0x2798..0x29xx, full disasm read). Remaining: computed-base /
  register-copy writer in ball-sim-step family. Next: anchor from ball-tick stores
  to +0x4C (co-gate byte, same struct) or live-watch B+0x80 across a kick.
- (b) **Block-B local RBP+0x80** (flag80, NOT the ticket's item but closed here):
  init 0 at `0x140503803`, set 1 at `0x140503864` (when PA+1 bit-4 class test
  `FUN_140A6E1C0` passes and bit-5 `FUN_140A6E5E0` passes with b38∈{3,4}), read at
  `0x140503CCF`. Slot reuse: dword/float spill at 0x140503665–6BA, then byte flag.

GF carriage: **NONE** (no volatile-ball byte; grep confirms no `ballVolatile` in
`src/`). Port spec: add `bool ballVolatileSuppressed` (`B+0x80 != 0`), default false
with `// ponytail: 0x14063AA9A`, when the writer lands.

Status: read + AND-role **CONFIRMED**; ball writer **OPEN** (narrowed: not a plain
disp8/disp32 imm store, not the 430C30 copier).

## §3 Packed stat source for ab+0x14/0x15 — FORMULA + SOURCE CONFIRMED, ab-fill OPEN

Chain (CONFIRMED decompiles + VFS dump):

1. `ab = AI_record + 0x70` (`FUN_14124DAD0` = `LEA RAX,[RCX+0x70]`;
   `AI = FUN_140A489F0(G,slot)` = `*(G+(slot+0xF3)*0x28)`, slot < 0x16).
2. Squad builder `FUN_1412E5EB0` iterates players, packed source
   `puVar10 * 0x9C + lVar15` (0x9C stride into the loaded player array
   `DAT_143700880`), calls `FUN_1412E4C60(tmp, packed, …)`, result copied to the
   0x17C EDIT record via `FUN_140B75B00`.
3. `FUN_1412E4C60` stat formula (CONFIRMED): stat ids 0x0E..0x27 =
   `(packed6 & 0x3F) + 0x28`, i.e. **raw6 + 40, range 40..103**. Per-id sources:
   0x0E←p[0]bits0-5, 0x0F←p[0]bits6-11, 0x10←p[0]bits12-17, 0x11←p[0]bits18-23,
   0x12←byte3, 0x13←byte0x13, 0x14←p[1]bits0-5 … (getter `FUN_1414C66C0` mirrors:
   0x0E→byte3-of-dword0, 0x0F→dword1 bits0-6, … sequential 7-bit fields from bit 24).
4. Player.bin cross-check (VFS stock `Data/dt18_all.cpk` winner, 4830 × 312B records):
   rec 0 (id 70101) +36..48 hold 6-bit 77s (`0x25965965`): Dribbling/Tight Possession
   77 CONFIRMED in raw bytes. In-match index map (matchlogic §1.3 + §3.2, INF-high):
   block 0x14 = OA ← packed id 0x0E ← Player.bin id 0x12 (Offensive Awareness,
   accessor bit 370); block 0x15 = DA ← packed id 0x0F ← Player.bin id 0x23
   (Defensive Awareness, accessor bit 275). Labels INF-high (tables never separate
   the pairs; Aoba order only evidence). No condition/form applied at build:
   modifiers (condition arrows @0x142632D70 × tier map @0x142632CC0, familiarity,
   super-sub, clamp 40..99 @0x142613C60) apply at CONSUMPTION, not in ab
   (matchlogic §3.2). So ab holds base stats; 27A0's own 50/99 clamp applies at use.
5. ab-fill (packed struct → AI+0x70 byte block): **OPEN**. Proven NOT in-place:
   packed byte 0x14 would mix ids 0x1F/0x21 (7-bit packing), but 27A0 reads clean
   stat bytes — ab is a decoded view filled by an unlocated loop (getter→byte
   stores). `LEA [r+0x70]` scan (2553 image-wide) too diffuse; AI-table publisher
   (`ADD r,0xF3` single hit 0x141579B24) is edit-data, not the AI fill.

GF carriage: **NONE 1:1**. `PlayerData` stats are 0..1 floats (`playerdata.cpp`);
`PlayerDatabase` (`src/pes/playerdb.*`) carries only injury tier + stronger foot.
Port spec: extend `PlayerDatabase::PlayerTraits` (or a new per-player bytes struct)
with raw6 OA/DA read from the VFS Player.bin record at stock offsets
(bit 370/6 and bit 275/6 per matchlogic §1.2 accessor table — read via the exe's
accessor semantics, never transcribed tables), value = raw6 + 40; condition arrows
stay OUT (consumption-side per §3.2). Until then 27A0 stays unwired
(`// ponytail: 0x140502C60/C69/C6F`).

Status: formula + Player.bin source **CONFIRMED**; OA/DA index labels **INF-high**;
AI-block fill loop **OPEN**.

## §4 Anchor AI+0x1414 + instruction static writer (budget items)

- Anchor: shape-builder family (`0x1405FA990` 497 insns, `0x1405FBCB0`,
  `0x1405FBE10`) contains **zero** 0x1414 references (CONFIRMED disasm) — family
  eliminated. Anchor copier `FUN_140431800` has exactly one caller, thunk
  `0x1404335CB` (JMP, E9 30E2FFFF), in the same unanalyzed publisher block as the
  anime-copy thunk — consistent with per-frame AI-record publish, origin still
  computed-base. GF: `Block9Inputs::anchor` carries the value 1:1, no producer.
  Status: writer **OPEN** (one family eliminated, copier confirmed).
- Instruction table: `FUN_14045B420` writes ONLY the 0x228xx pair region
  (CONFIRMED full decompile: 0x228D4/0x228EC/0x2299C… fills, no 0x25xxx) — pair
  writer ≠ instruction writer. No disp32 writer to 0x25CF8/0x25D38 in .text
  (only CMP reads + one SIB MOV read at 0x142160BD9). Static writer lives on the
  tactics/formation-load path (VFS GamePlan/Formation side). GF: `teaminstructions.cpp`
  mask + `pesoffball.hpp:355` stub. Status: writer **OPEN** (narrowed to load path).

## §5 Port spec (exact, per item)

1. Card: fill `CardFacts` six fields from `currentAnim` + `motionNo`; keeper bit:
   add `bool keeperBit36`, publish-sourced. (`// ponytail: 0x14050385B/…/0x140503020`)
2. Volatile: add `bool ballVolatileSuppressed`, default false. (`// ponytail: 0x14063AA9A`)
3. Stats: add raw6 OA/DA bytes (VFS Player.bin, stock archive), value = raw6 + 40;
   no condition at build; 27A0 unwired until then. (`// ponytail: 0x140502C60`)
4. Anchor: fill `Block9Inputs::anchor` from ground position. (`// ponytail: 0x1405036D5`)
5. Flags: feed `a63950(0xB/0xC/0xD team, 0xE slot)` from the instruction import; false
   until then. (`// ponytail: 0x140502F32/F6C/0C7/0F9`)

## Evidence ledger

| ID | Claim | Conf | Evidence |
|---|---|---|---|
| PUB-01 | anime publish = slot4 1409299A0 → 140430500(dst, P+0xAC8) | CONFIRMED | dec.sh FUN_1409299A0; xrefs (slot4 in 4 vtables); thunk E9 bytes |
| PUB-02 | copy covers all card words + keeper qword | CONFIRMED | FUN_140430500 decompile; copy.w1a/dst38 bytes |
| PUB-03 | dst == PA | INF | one handle dereference short (P+0x3AD0 chain) |
| PUB-04 | flag80 writers 803/864 + reader CCF | CONFIRMED | checker bytes + disasm |
| PUB-05 | ball B+0x80 AND-gate + reader | CONFIRMED | 14063A490 decompile; vol.read/test bytes |
| PUB-06 | ball B+0x80 writer open (eliminations) | CONFIRMED (neg) | scans §2; 430C30 full disasm |
| PUB-07 | stat formula raw6+40, ids 0x0E..0x27 | CONFIRMED | 1412E4C60 decompile; getter mirror |
| PUB-08 | packed source = 0x9C player array; Player.bin 77s | CONFIRMED | 1412E5EB0 decompile; VFS dump rec0 |
| PUB-09 | OA/DA index labels | INF-high | matchlogic §1.3/§3.2 + Aoba order |
| PUB-10 | modifiers at consumption, not build | CONFIRMED | matchlogic §3.2 table addrs |
| PUB-11 | anchor shape-builder eliminated; copier single-caller | CONFIRMED | 3 disasms; xrefs 431800 |
| PUB-12 | 45B420 ≠ instruction writer; no disp32 writer | CONFIRMED (neg) | full decompile; byte scans |
| PUB-13 | GF homes/gaps | CONFIRMED | grep src/sim + src/pes 24-10-26 |

## Unresolved / next steps

1. PA-identity: resolve `*(P+0x3AD0)` handle → PA table entry (one `dec.sh` on 1406EDC70 callers).
2. B+0x80 writer: ball-tick +0x4C-anchored scan or live-watch across a kick.
3. ab-fill loop: getter→byte-store loop hunt from AI-record constructor.
4. Instruction static writer: tactics/formation VFS load path.
5. Slot-4 frame order: attribute the 19 `FF 50 20` dispatch sites.

Requests used: ~85 of 150. Scratch: `<workdrive>/gf-PublisherRE-scratch/`
(`check_publisher.py`, `dec_*.txt`, `playerbin_raw.bin`).
