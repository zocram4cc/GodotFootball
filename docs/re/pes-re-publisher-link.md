# PES publisher link: 27A0 writer-side closes (PublisherLinkRE, 24-10-26)

Read-only RE. Image `~/ghidra-projects/scratch/Unpacker/pes_full.bin` (base 0x140000000,
file offset = VA − 0x140000000). Bridge one request at a time
(`decompile_function` + `xrefs_to` + `disassemble_function`); `dec.sh` not needed
(all targets had symbols). Read-first: `pes-re-publisher.md` (the handover,
§0–§5) and `pes-re-27a0-inputs.md` (the read side, §1–§7 CLOSED there).
Tags: **CONFIRMED** = bytes/decompile this session; **INF** = inference; **OPEN** = blocker named.
Checker: `check_publisher_link.py` (this scratch dir), ALL GREEN 24-10-26 (extends
`check_publisher.py`: all prior groups re-run GREEN here); mutation (`ballw.store`
`...00` → `...01`) drives exactly that group RED, reverted (backup copy removed).

## §0 Script-read constants (checker output, pasted)

```
link.call6edc70 e8db34ffff     (0x1406FA790 CALL 0x1406edc70, sole xrefs_to hit)
anch.copy  488d8f14140000      (0x140431B0F LEA RCX,[RDI+0x1414])
anch.lea27a0 8dbd1414          (0x1405036D5 LEA R15,[R13+0x1414], head bytes)
ballw.store 889e80000000       (0x14087197B MOV [RSI+0x80],BL)
ballw.gate 80be8000000000      (0x14087194C CMP byte [RSI+0x80],0)
ballw.blsrc 440fb6c0           (0x140424C33 MOVZX R8D,AL)
ballw.f108 bb96de3d            (0x142598958: 0.10868593f)
cond.get14 ba14000000          (0x140D5BC22 MOV EDX,0x14)
cond.copy6c e84e48faff         (0x14152263D CALL 0x1414c6e90, packed copy loop)
inst.cmpB 45399490f85c0200     (0x140A63976 CMP [R8+RDX*4+0x25CF8],R10D)
inst.cmpS 45398c90385d0200     (0x140A63980 CMP [R8+RDX*4+0x25D38],R9D)
```

Operand-VA pairs re-checked against disassembly text this session (§constants rule).
Prior groups (slot4, copy.w1a/dst38, flag80, card, keeper, rate, vol.read/test)
re-run GREEN unchanged — see checker file.

## §1 The PA identity link: chain CONFIRMED linear, dst==PA stays INF (strengthened)

Chain (CONFIRMED decompiles + `xrefs_to`, each link single):

```
match::Player vtable slot 4 = FUN_1409299A0   (4 vtables, prior doc)
  lVar3 = *(PM + 0x7D8) = P                    (pub.slot4 bytes, re-run GREEN)
  sole caller of FUN_1406FA780 is 0x1409299A7  (xrefs_to 1406FA780: 1 hit)
  FUN_1406FA780(P): dst=FUN_1406EDC70(*(P+0x3AD0)); FUN_140430500(dst,P+0xAC8);
                    FUN_1406E4690(*(P+0x3AD0))   (decompile, 3 lines)
  sole caller of FUN_1406EDC70 is 0x1406FA790  (xrefs_to 1406EDC70: 1 hit;
                                                link.call6edc70 bytes above)
  sole code caller of FUN_140430500 is 0x1406FA79F (xrefs_to 140430500: 2 hits =
                                                6FA79F + thunk 0x1404335AB)
```

`FUN_1406EDC70` (CONFIRMED decompile): lazy handle — if `param_1[1]==0` run the two
vtable inits (`**(param+8)`, `**(param+0x10)` via `FUN_140433870`); if `param_1[4]==0`
set `param_1[4] = *(param_1[2]+0x10)+0x180` and return it. So
**dst = \*(\*(handle+2)+0x10)+0x180**, cached in handle slot 4, `+0x20` = dirty flag
(cleared by `FUN_1406E4690`, same block shape as slot4's tail — the publish handshake).

Handle origin (traced, ends at a virtual call — the remaining gap):
`P+0x3AD0` is written only in `FUN_1406E1610` (`MOV [RDI+0x3AD0],RSI`, with sibling
slots `+0x3AC8/+0xAA0/+0xB28`; vtables set at `P+0/+0xAB8/+0xAC0`), whose only code
caller is `FUN_1406E2100` (`0x1406E210F`), which passes its own incoming RSI through
(`MOV RDI,RCX; CALL 6E1610` with RSI untouched) — and `FUN_1406E2100` itself lives in
vtable `0x1425E34D8` (DATA xref), so the handle value arrives via virtual dispatch:
**not statically traceable**. The sibling resolver `FUN_1404335E0` (same lazy shape,
offset `+0x2970`) resolves `P+0x4E0`-family handles to the ball; the `+0x180` /
`+0x2970` pair reads as two views off one handle base (`param[2]+0x10`).

Why dst==PA still INF but now tightly boxed (three-sided):
(a) the chain is linear — no fan-out anywhere (each `xrefs_to` = 1);
(b) layout containment — `FUN_140430500` (CONFIRMED full decompile, 0x180 bytes)
copies every 27A0-read word: `+0x01`, `+0x08` (bit 11), `+0x1A/+0x1C`, `+0x38`,
`+0x40..0x47` (keeper qword, bit 36), `+0x60`, `+0x11C`;
(c) size consistency — PA entries are 8-byte pointers (`*(G+(id+0xA5)*0x28)`,
`FUN_140A48910`, slot bound `< 0x1A`, else `G+0x19C8`; AI twin `FUN_140A489F0`
bound `< 0x16`), so the ≥0x120-byte anime record fits the 0x180-byte dst.
The live read that closes it: break on `FUN_140430500`, compare `dst` against
`FUN_140A48910(G,id)` for the player's id (both available at the breakpoint).

GF carriage: unchanged from handover §1 — `CardFacts` six fields exist 1:1
(`src/sim/onthepitch/pesreactionblock.hpp:115-124`), no producer; keeper bit has
NO 1:1 (grep: no `keeperBit` in `src/`). Port spec: fill the six from
`HumanoidBase::currentAnim` (`humanoidbase.hpp:360`, `GetFrameNum()`/`:195`,
`GetFrameCount()`/`:196`, `GetCurrentAnim()`/`:249`) + `AnimetableCell::motionNo`
(`src/pes/animetable.hpp:37`); add `bool keeperBit36` beside `bit11AtEight`.
Until then `// ponytail: 0x140503CF5/D01/D05/D0B/D21/DAE` + `0x140503020/024/028`.

Status: chain **CONFIRMED** linear; dst==PA **INF** (one virtual-dispatch gap);
GF map **CONFIRMED** (grep 24-10-26).

## §2 The in-match ability block's fill: formula+source CONFIRMED, gather loop OPEN, modifiers CONFIRMED-packed-side

Pipeline (CONFIRMED decompiles, all this session):

1. Squad load `FUN_1412E5EB0`: packed source `puVar10 * 0x9C + lVar15` (0x9C stride
   into `DAT_143700880`) → `FUN_1412E4C60(tmp, packed, …)` → `FUN_140B75B00`
   into the 0x17C EDIT record. Three extra setter calls set packed ids
   `0x2E/0x2F/0x2A` (condition/familiarity gate inputs) before the formula call.
2. Formula `FUN_1412E4C60` (CONFIRMED): stat ids `0x0E..0x27` =
   `(packedN & 0x3F) + 0x28` (**raw6 + 40**, range 40..103); sources interleave
   dwords (`0x0E←p[0]bits0-5 … 0x12←byte3, 0x13←byte0x13, 0x14←p[1]bits0-5 …`;
   note id `0x20` breaks the run: `param_1[8]` 2-bit field, then `0x21..` resume
   in `param_1[5]`). Full per-id table in `dec_12e4c60.txt`. The setter
   `FUN_1414C6E90` (CONFIRMED) writes 7-bit stat fields as
   `param_1[d] = (param_1[d] & mask) | (v & 0x7F) << shift`, sequential 7-bit slots
   from dword 1 bit 28 (`0x0E` = byte 3 of dword 0). Getter `FUN_1414C66C0`
   (CONFIRMED) mirrors exactly.
3. Per-frame-ish publish `FUN_141503DE0`: for each of 0x28 slots, resolve
   (`FUN_1414BB2A0`/`FUN_1414C21B0`), copy the 0x17C EDIT (`FUN_140AF06D0`),
   condition-gate (`FUN_14151D610`), then `FUN_1415225C0(AIpackedslot, EDIT+8)`
   where `AIpackedslot = param_2+0x88+slot*0x2C` — a whole-packed copy
   (ids `0..0x6B` getter→setter, CONFIRMED 6-line loop, cond.copy6c bytes).
   So the AI record carries a **packed** struct (0x2C stride) at `+0x88`.
4. Condition/familiarity/super-sub (CONFIRMED packed-side): `FUN_140D5B970`
   (+ caller `FUN_140D5AED0`, team array stride **0x648**, elem `+0x630`) runs
   getter→add→setter per stat id (`0x0E,0x12,0x11,0x13,0x15,0x16,0x14,0x17,0x0F,
   0x18,0x19,0x24,0x21,0x25,0x23,0x22,0x26…`, cond.get14 bytes) — i.e. modifiers
   apply to the **packed struct**, never to `ab` directly. No 50/99 clamp at build
   (the clamp lives in 27A0 at use: `max(b,50)`/`min(hi,99)`).
5. The byte block `ab = AI+0x70` (`FUN_14124DAD0` = `param+0x70`, CONFIRMED 1-line):
   27A0 reads `ab+0x14/ab+0x15` as clean bytes. The packed→byte **gather loop**
   (getter→byte-store per block index) is **OPEN** — `LEA [r+0x70]` scan is
   diffuse (60+ `FUN_14124DAD0` readers, all consumers: `140502C5B/14050301B`
   in 27A0, `1404FEDC0` reads `ab+0x15` + `ab+0x40` bit 23, etc.). Narrowed:
   block byte `b` = `getter(AIpacked, b+6 or b)` per matchlogic §1.3's
   "ids 0x0E..0x27 = block index − 6" (OBS); the store side is a byte-store loop
   off the AI base that no disp scan isolates (computed base). Consumers that
   prove the semantics: `FUN_141333080` (reads packed `0x14/0x15/0x16`, sums
   `0x14+0x15` — ability aggregation), `FUN_14156D329` (reads packed `0x14`).

GF carriage: **NONE 1:1** — `PlayerData` stats are 0..1 floats (`playerdata.cpp`),
`PlayerDatabase` (`src/pes/playerdb.hpp:29-56`) carries only injury tier
(`kInjuryResistanceBit 458`) + stronger foot (`kStrongerFootBit 514`).
Port spec: extend `PlayerTraits` (or a new per-player bytes struct) with raw6 OA/DA
read from the VFS `Player.bin` winner at the stock offsets via the exe's accessor
semantics (never transcribed tables), value = raw6 + 40; condition arrows stay
OUT (consumption-side per §2.4); 27A0 stays unwired until then
(`// ponytail: 0x140502C60/C69/C6F`).

Status: formula + packed source + EDIT/AI-packed path **CONFIRMED**; OA/DA index
labels **INF-high**; modifiers-packed-side **CONFIRMED**; byte-block gather loop
**OPEN** (narrowed to getter→byte stores off the AI base).

## §3 The ball B+0x80 writer: FOUND and CONFIRMED (was OPEN)

Disambiguation (this session): there are TWO `+0x80` flag families, and the
handover's elimination list had them tangled:

- (a) **Ball B+0x80** — reader `0x14063AA9A MOVZX R14D,[R12+0x80]` /
  `0x14063AAA3 TEST` (vol.read/test, re-run GREEN). `R12` is **ball B**:
  `dis_vol.txt:14063A520 MOV RAX,[RCX+0x40]` (RCX=G) → `[RSP+0x70]` →
  reloaded `14063AA95 MOV R12,[RSP+0x70]`. Co-gate `FUN_140A6D0D0(B) = B+0x4C`
  (CONFIRMED 1-line), constructor `FUN_140A572F0` zeroes both `+0x4C` and `+0x80`
  (`dis_a573a0.txt:140A573FD/140A57435`, BPL=0).
- (b) **Action-elem +0x80** — `FUN_1406EFFB0` writes `([P+0xA08]+0x110)+0x80`
  (`0x1406F00AE/0x1406F02B3 MOV [RCX+0x80],R10B`, CONFIRMED decompile: param_2
  selects `+0x3754/+0x3755/+0x3756`, zeroes the sibling sample bank, then
  `+0x80 = 0` when all three clear else `= 1`), and `FUN_1406FA930` writes
  `[action-elem+0x80] = 0` after its vtable slot-0x20 call
  (`0x1406FA9B9 C6 83 80 00 00 00 00`, CONFIRMED disasm). These are NOT the
  ball flag (different base chain: `P+0xA08`, not `G+0x40`). The handover's
  "eliminated: C6 imm" scans were hitting this family.

Writer (CONFIRMED): **`0x14087197B MOV [RSI+0x80],BL` in `FUN_140870E90`**
(ballw.store/gate bytes). RSI is the ball: `RSI = FUN_1404335E0(P+0x4E0)`
(the 4335E0 lazy resolver, same shape as 6EDC70, offset `+0x2970`) and RSI feeds
ball samplers `FUN_140A6CF30/140A57800/140A6CF70/140A6CE90/140A6CEC0` throughout
the function. Rule (CONFIRMED disasm `dis_ballw2.txt:14087194C-140871981`):

```
if ([RSI+0x80] == 0 && XMM6 > [RBP+0x24] && [R15+0x74] > XMM6) [RSI+0x80] = BL
elif ([R15+0x68] != 0.10868593f)                                [RSI+0x80] = BL
```

`BL` = incoming `param_3` (`MOVZX EBX,R8B` at `0x140870F10`); sole caller
`0x140424C3B` passes `R8B = FUN_140A60530(...)` byte (ballw.blsrc bytes:
`0x140424C33 MOVZX R8D,AL`). `R15 = param_1` (the P-side record:
`+0x1C/+0x20/+0x68/+0x74` floats). `[RBP+0x24]` = incoming float threshold
(stack-arg; exact caller slot OPEN — named, not needed for the port).
Floats script-read, never typed: `0.5 @0x142565600`, `1.19e-7 @0x14259BDC4`,
`π @0x142578480`, `0.10868593 @0x142598958` (ballw.f108 bytes).

Eliminated as ball writers (CONFIRMED other-struct): `FUN_140A4B240`'s
`[RBX+0x80]` + siblings (`1404BB136/140DBE6DB/140E26EC3/140E38145/140E3E899`)
are refcount-flag copies beside pointer fields (`+0x88/+0xB0`…); `FUN_140A45060`'s
`[RBX+0x80]` is a constructor zero; the 40 `1402Exxxxx [RBP/RSP+0x80]` hits are
stack locals; image-wide only 5 register-base writers touch heap `+0x80` outside
the action family and 4 are the refcount/constructor cases above.

GF carriage: **NONE** (no volatile-ball byte; grep confirms no `ballVolatile` in
`src/`). Port spec: add `bool ballVolatileSuppressed` (`B+0x80 != 0`), default
false; set from the writer rule (gate above, threshold as caller-supplied float
until `[RBP+0x24]`'s slot is named); `// ponytail: 0x14087197B`.

Status: writer **CONFIRMED** (VA, rule, BL source, constants); threshold-slot
naming **OPEN** (not port-blocking).

## §4 Budget items

- Instruction table: reader `FUN_140A63950` (CONFIRMED decompile) is an
  SIB-indexed search: `MA=*(TM+0x18)`, `j<4`, `MA+0x25CF8+side*16+j*4 == id &&
  MA+0x25D38+side*16+j*4 == slot` (inst.cmpB/cmpS bytes; note the disp bytes are
  `F8 5C 02 00` / `38 5D 02 00` — a prior scan used nibble-swapped patterns and
  reported zero hits, corrected this session: **9 + 9 sites, ALL reads**
  — id compares against 3/9/0x10/0xF/0xE, slot against 0xFF/ESI/EDI; plus
  `0x142160BD6 MOV EDX,[RBP+RCX*4+0x25CF8]` in a formation-checksum summer
  `FUN_142160A60`). **Zero WRITES image-wide** → the static writer does not emit
  per-field stores: the table is bulk-filled (memcpy) or load-mapped on the
  tactics/formation VFS path. GF: `TeamInstructions::State`
  (`src/sim/onthepitch/teaminstructions.hpp`, mentality only — no advanced
  instructions) + `pesoffball` stand-ins reference `a63950` only in `ponytail:`
  comments. Status: reads **CONFIRMED**, static writer **OPEN** (narrowed to
  bulk-fill/load path; no per-field store exists to find).
- Anchor `AI+0x1414`: shape-builder family (`0x1405FA990` etc.) still eliminated
  (prior doc, stands). New this session: image-wide 0x1414 census (108 sites) —
  **zero `MOVSS [r+0x1414]` stores** (37 797 `F3 0F 11` scanned, none targets
  0x1414; all 0x1414 traffic is `MULSS/SUBSS/SUBPS/LEA/ADD` reads + the
  `FUN_140431800` copier `LEA RCX,[RDI+0x1414]; LEA RDX,[RBX+0x1414]` at
  `0x140431B0F`, anch.copy bytes). Two computed-base stores exist but are
  elsewhere: `0x1421ADD16 MOV [RBX+0x1414],RSI` (network-matchplan sync record
  init, `FUN_1421ADC70` — different struct, zero-init) and
  `0x14113909B MOV [RBP+RCX*8+0x1414],EAX` (match-plan array copy in
  `FUN_141138950`, formation indices `+0x1920/+0x1924` → `+0x1410/+0x1414`).
  Neither is the AI-record origin (different base chains). GF:
  `Block9Inputs::anchor` carries the value 1:1, no producer. Status: writer
  **OPEN** (copier CONFIRMED, two look-alikes eliminated, origin = computed-base
  or mover-block inheritance).

## §5 Port spec (exact, per item)

1. Card: fill `CardFacts` six fields from `currentAnim` + `motionNo`; keeper bit:
   add `bool keeperBit36` beside `bit11AtEight`, publish-sourced.
   (`// ponytail: 0x14050385B/…/0x140503020`)
2. Stats: add raw6 OA/DA bytes (VFS `Player.bin` winner, stock archive), value =
   raw6 + 40; no condition at build; 27A0 unwired until the gather loop lands.
   (`// ponytail: 0x140502C60`)
3. Volatile: add `bool ballVolatileSuppressed` (`B+0x80 != 0`), default false; set
   per the §3 gate (threshold caller-supplied). (`// ponytail: 0x14087197B`)
4. Anchor: fill `Block9Inputs::anchor` from ground position.
   (`// ponytail: 0x1405036D5`)
5. Flags: feed `a63950(0xB/0xC/0xD team, 0xE slot)` from the instruction import;
   false until then. (`// ponytail: 0x140502F32/F6C/0C7/0F9`)

## Evidence ledger (new/changed; PUB-01–13 stand)

| ID | Claim | Conf | Evidence |
|---|---|---|---|
| LINK-01 | slot4→6FA780→6EDC70 single-caller linear chain | CONFIRMED | xrefs_to 6FA780=1 (9299A7), 6EDC70=1 (6FA790); link.call6edc70 bytes |
| LINK-02 | 6EDC70 returns \*(handle+2)[0x10]+0x180, handle via virtual dispatch | CONFIRMED | decompile 6EDC70; setter 6E1610←6E2100←vtable 1425E34D8 |
| LINK-03 | dst==PA stays INF (boxed: linear + containment + size) | INF | 430500 decompile (0x180); 48910/489F0 bounds; live-read named |
| LINK-04 | ball B+0x80 writer = 0x14087197B, gate + BL + floats | CONFIRMED | dis_ballw2.txt 87194C-871981; ballw.* bytes; dec_ballw3.txt |
| LINK-05 | action +0x80 family (6EFFB0/6FA930) ≠ ball flag | CONFIRMED | dec_ballw.txt; dis_a930.txt (P+0xA08 chain vs G+0x40) |
| LINK-06 | refcount/constructor +0x80 stores eliminated | CONFIRMED | dis_4b2c0/45080/a573a0.txt |
| LINK-07 | stat formula raw6+40 + packed→EDIT→AI-packed path | CONFIRMED | dec_12e4c60/25eb0/2260/03de0.txt |
| LINK-08 | modifiers (cond/fam/sub) apply packed-side | CONFIRMED | dec_cond.txt; dis_5aed0.txt (0x648 stride, +0x630) |
| LINK-09 | byte-block gather loop OPEN (getter→byte stores) | OPEN | 60+ 24DAD0 readers all consume; noted stores none |
| LINK-10 | inst reads 9+9, zero writes (bulk-fill/load path) | CONFIRMED (neg) | corrected disp bytes; inst.cmpB/cmpS |
| LINK-11 | anchor: zero MOVSS stores image-wide; 2 look-alikes out | CONFIRMED (neg) | 37 797-opcode scan; dec_anchor*.txt; anch.copy bytes |
| LINK-12 | GF homes/gaps re-verified | CONFIRMED | grep src/sim+src/pes 24-10-26 (paths in §5) |

## Unresolved / next steps

1. PA-identity: live-break `FUN_140430500`, compare `dst` vs `FUN_140A48910(G,id)`.
2. ab gather: live-watch `AI+0x84/0x85` across a condition-arrow change; packed
   (`AI+0x88`…) vs byte block diverge → loop found.
3. Ball threshold: name `[RBP+0x24]`'s slot in `FUN_140870E90` from callers above
   `0x140424BB0` (2nd float arg chain).
4. Instruction bulk-fill: formation/tactics VFS load path (`GamePlan`/`Formation`
   winners) — checksum summer `FUN_142160A60` as entry point.

Scratch: `<workdrive>/gf-PublisherLinkRE-scratch/`
(`check_publisher_link.py`, `dec_*.txt`, `dis_*.txt`, `scans.txt`).
Prior requests ~85 + this session ~60 (bridge calls are one-at-a-time; two
image-wide capstone scans ran locally, nothing in /tmp, peak < 2 GB).
