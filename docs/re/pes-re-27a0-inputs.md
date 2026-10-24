# PES 27A0 inputs (FUN_1405027A0): unbound items decoded (Inputs27A0RE, 24-10-26)

Read-only RE. Image `~/ghidra-projects/scratch/Unpacker/pes_full.bin` (base 0x140000000,
file offset = VA − 0x140000000). Bridge: one request at a time; constants re-read from the
image, never typed. Tags: **CONFIRMED** = bytes/decompile/live read this session;
**INF** = inference; **OPEN** = blocker named. Checker: `check_27a0.py` (this scratch dir),
GREEN 24-10-26; mutation (`41B80B`→`41B80C` at 0x140502F26) drives `flg.b` RED, reverted.

## §0 Script-read constants (checker output, pasted)

```
rate.cmp80 80bd8000        (0x140502C60 CMP byte [RBP+0x80],0)
rate.jz15 7406              (0x140502C67 JZ -> MOVZX [RAX+0x15])
rate.mov14 0fb64814        (0x140502C69 MOVZX ECX,[RAX+0x14])
rate.mov15 0fb64815        (0x140502C6F MOVZX ECX,[RAX+0x15])
keep.mov40 488b4840        (0x140503020 MOV RCX,[RAX+0x40])
keep.shr24 48c1e924        (0x140503024 SHR RCX,0x24)
keep.test1 f6c101          (0x140503028 TEST CL,1)
card.shr11 8b480848c1e90b (0x140503CF5 MOV ECX,[RAX+8]; SHR RCX,0xB)
anch.lea 8dbd1414000048    (0x1405036D5 LEA R15,[R13+0x1414])
rate.tabrow 01000000       (0x142645E40: row 0x20 flag dword = 1)
card.call6e1c0 e860a95600  (0x14050385B CALL 0x140a6e1c0, bit 4)
flg.b 41b80b000000         (0x140502F26 MOV R8D,0xB)
flg.c 41b80c000000         (0x140502F60 MOV R8D,0xC)
lvl.6 ba06000000           (0x140503203 MOV EDX,6)
lvl.5 ba05000000           (0x140503220 MOV EDX,5)
c.50 50.0                 (0x14259BE34)
c.99 99.0                 (0x1425C48CC)
c.49 49.0                 (0x14259D620)
```

Every instruction→operand pair above was re-checked against the disassembly text
(`dis27A0.txt`, `disB.txt` in scratch). Neighbouring constants differ by 4 bytes;
the pairs are the exact ones.

## §1 Rating bytes PMS+0x84/+0x85 = stat ids 0x14/0x15 — CLOSED (CONFIRMED bytes, INF-high labels)

Site (27A0 body, `0x140502C60`–`0x140502C73`):
```
0x140502C60 CMP byte [RBP+0x80],0      ; [RBP+0x80] = p5 copy (SIL at 0x140502992/0x140502A0F)
0x140502C67 JZ 0x140502C6F
0x140502C69 MOVZX ECX,byte [RAX+0x14]  ; p5 set: stat id 0x14
0x140502C6F MOVZX ECX,byte [RAX+0x15]  ; p5 clear: stat id 0x15
```
`RAX = FUN_14124DAD0(FUN_140A489F0(G,slot))` = AI record + 0x70 (the ability block
`ab`, contact §notation). So the bytes are **ab+0x14 / ab+0x15** — the in-match stat
block of matchlogic §1.3 (0x128 B/player, indices +0x00..+0x3B), NOT Player.bin ids.

Labels (matchlogic §1.3 group table + Aoba order, INF-high — the two tables can never
separate the pairs, Aoba's live labels are the only evidence):
- in-match **0x14 = Offensive Awareness**, **0x15 = Defensive Awareness**.
- The pair selector is the same boolean as anim-select-2 §1.4 (`0x140502C5B`'s
  `+0x14`/`+0x15` switch on the possession/attacking flag): p5 set reads OA, else DA.
- Value rule in 27A0 (`0x140502C73`–`0x140502CDE`): `b1 = byte; lo = max(b1,50)`;
  `hi = min(lo, 99)` via `fVar27 = max(50.0, b1)` / `fVar26 = min(hi, 99.0)`;
  adds `(99 − hi) / (49 / (int)(rng·0.2))` to the score (49.0 = `0x14259D620`).
  So a higher awareness byte shrinks the added term; floor 50, cap 99 (CONFIRMED asm).

Player.bin cross-check (VFS, stock `Data/dt18_all.cpk` winner via `pesfmt_dump cat
common/etc/pesdb/Player.bin`, 4830 records): record 0 (id 70101) Dribbling 77 /
Tight Possession 77 at matchlogic-§1.2 bits 352/416. **Player.bin ids 0x14/0x15 are
Dribbling / Tight Possession — different index space** (matchlogic §1.2 vs §1.3,
corrected 19-10-26 per community-qa C4). The parked doc's "`Player.bin`" phrasing is
wrong; the block is the in-match byte space.

Writer of the in-match bytes: the packed→block setter `0x1412E4C60` via field setter
`0x1414C6E90(obj, id, value)`, ids 0x0E..0x27 = block index − 6 (matchlogic §1.3,
OBS). Source of the packed struct is OPEN (not Player.bin directly).

GF carriage: **NO 1:1**. GF stats are `PlayerData::GetStat("technical_*")` 0..1 floats
(`src/sim/data/playerdata.cpp`), not PES bytes; `PlayerDatabase` (`src/pes/playerdb.*`)
carries only injury tier + stronger foot. Port spec: new per-player bytes
`stat14`, `stat15` (40..99, init from the VFS block when the packed-source import
lands; the stock archive, never the mod, per dt18 rule) + the §1.4 selector boolean;
until then `// ponytail: 0x140502C60/C69/C6F` with a constant stand-in is NOT
behaviour-safe (the term scales with the byte) — leave the 27A0 port unwired.

Status: **CONFIRMED** (bytes, selector, value rule, index-space correction);
labels **INF-high**; packed-source writer **OPEN**.

## §2 Block-9 keeper bit P+0x40 (0x24) — CLOSED (CONFIRMED)

Site (27A0 body, `0x140503020`–`0x14050303C`):
```
0x140503020 MOV RCX,[RAX+0x40]   ; RAX = AI record of the candidate slot
0x140503024 SHR RCX,0x24         ; bit 36 -> bit 0 (byte P+0x44... precisely (P+0x40)>>36&1)
0x140503028 TEST CL,1
0x14050302B JNZ 0x14050303C      ; set: skip the *0.5, take it
0x14050302D LEA RCX,[RSP+0x20]   ; clear: CLS = FUN_140A60340(out); CMP EAX,4
0x140503032 CALL 0x140a60340
0x140503037 CMP EAX,0x4
0x14050303A JL 0x140503041      ; CLS < 4: skip too; else take *0.5
0x14050303C MULSS XMM7,XMM10    ; *0.5
```
Gate (full, `0x140502FF7`–`0x140503041`): slot < 0x16, candidate == slot,
p5 set, holder-class test `FUN_140A60340 >= 4` OR keeper-bit set → score *= 0.5.

Meaning: `(P+0x40 >> 0x24) & 1` is the low bit of the published anime-state record's
second dword — the kind-family flag at PA+0x44 bit 4 (duel-sim: `hu+1` action,
`hu+3` second action; contact: `human+8` bit 15 = sliding, bit 16 = tackling;
a3helpers: bit 27 = jostle-style). Which family bit 36 names is **INF** (not decoded
which action family sets it); the read and the branch effect are CONFIRMED.

Writer: the anime-state publisher bulk copy (`FUN_1406E1CE0` / `FUN_140A47290` per
duel-sim §3; a3helpers §3: writer of `hu+8` itself OPEN). So the writer is OPEN by
inheritance — same publisher as the card record (§6).

GF carriage: **NO 1:1**. GF has no PA record (`pesreactionblock.hpp` `CardFacts` has
`bit11AtEight` only). Port spec: add `keeperBit36` (bool) beside `bit11AtEight`,
sourced from the publisher when it lands; until then `// ponytail: 0x140503020/024/028`.

Status: **CONFIRMED** (address, shift, branch); family label **INF**; writer **OPEN**
(publisher).

## §3 Anchor P+0x1414 (+0x141C) — CLOSED as address/role (CONFIRMED), writer OPEN (narrowed)

Site (block B, `0x1405036D5`–`0x14050375E`):
```
0x1405036D5 LEA R15,[R13+0x1414]   ; R13 = AI record of the slot (lVar7 = FUN_140A489F0(G,slot))
... FUN_140A91120(..., anchor) ... ; closest-t of the displaced segment to the anchor
0x140503736 SUBSS XMM3,[RBX+0x8]   ; ... then the reach test:
anchor.x/z vs the displaced point; |d| <= 0.3 -> block[0] = 1 path (see TeamT2 §block[0])
```
`+0x141C` is the z lane of the same point (x at +0x1414, y at +0x1418, z at +0x141C;
first-touch §: "A = the presser's base (`AI(presser)+0x1414/+0x1418/+0x141C`)").

Role (CONFIRMED): the slot's **base point** — block[0] is the distance ramp of this
anchor to the closest point of the motion-displaced segment (1 when degenerate;
TeamT2 §"block[0]"). First-touch confirms the semantics: `target = B` (ball now)
if `|S_k − B|² <= |S_k − A|²` else `A` — A is where the man starts from.

Writer: **OPEN, narrowed**. No `MOVSS [reg+0x1414]` store exists image-wide
(brute-force `F3 0F 11` + disp32 scan: zero hits for 0x1414/0x141C). The only
0x1414 traffic in 30 LEA sites is the bulk AI-record copy `FUN_140431800`
(`0x140431B0F`/`0x140431B16`: `LEA RCX,[RDI+0x1414]; LEA RDX,[RBX+0x1414]` +
12-byte copy) — a copier, not the origin writer. The origin writer uses a computed
base (S1/S2-blind: mod-0 SIB or rep-movs) or lives in the mover block the AI record
inherits. Candidate family: the `0x1405FBCB0/5F9CD0/5FBE10`-via-`5FA990` shape-builder
family (lane-post §6 writers of the S+0x7Bxx band rows under `T+0xC48`) — not checked
for +0x1414 stores. Next step: scan that family for +0x1414 stores, or live-watch
`A+0x1414` per carrier (foundation-widen W-A2 already prescribes the read).

GF carriage: **PARTLY**. `Block9Inputs::anchor` (`Vec3`, `pesreactionblock.hpp`)
carries the point 1:1 as a value; no GF producer fills it. Port spec: source from
the man's ground position when the writer lands (that is what the first-touch "base"
is); until then caller-supplied, `// ponytail: 0x1405036D5`.

Status: **CONFIRMED** (address, role as base point); writer **OPEN** (copier found,
origin narrowed to computed-base/shape-builder family).

## §4 B+0x80 flag — CLOSED as read/role (CONFIRMED), writer OPEN

Readers (CONFIRMED):
- Volatile-ball gate `FUN_14063A490`: `0x14063AA9A MOVZX R14D,[R12+0x80]`;
  `0x14063AAA3 TEST; JNZ skip` — arm B needs `B+0x80 == 0` (with `B+0x4C != 0` and
  `!(d2 > d1)`; ball-track audit 19-10-26: AND, not OR).
- 27A0 does NOT read B+0x80 directly. The parked item's "B+0x80" is the block-B
  ball input `FUN_140A6CF30(B, k)` / `FUN_140A6CF70` sample chain (decompile
  `f_140503430.c` head: `local_120 = *(G+0x40)` = the ball object; the
  `FUN_140A6CF30`/`FUN_140A57800` calls select the predicted sample). The flag that
  gates the volatile arm is the same byte the DHB think reads.

Writer: **OPEN**. Byte-store scans are unattributable (disp 0x80 too common:
`C6 40 80` 25 hits, `C6 41 80` 20 hits, all in unrelated code ranges). No scan with
ball-base anchoring has been run. Next step: anchor from the `FUN_140A6CF30`
callers (only two: `0x14063AAB9` in the volatile gate and the block-B sample path)
or live-watch B+0x80 across a kick.

GF carriage: **NONE** (GF has no volatile-ball byte; carrier-port §3.7 stays clear
by design). Port spec: add `ballVolatileSuppressed` (bool, `B+0x80 != 0`) when the
writer lands; until then `false` with `// ponytail: 0x14063AA9A`.

Status: **CONFIRMED** (read, AND-role); writer **OPEN**.

## §5 B+0x2960 (V1) — CLOSED (CONFIRMED)

Accessors (CONFIRMED decompiles, 2 lines each):
- `FUN_140A57730(B) = B + 0x2960` (V1); `FUN_140A57720(B) = B + 0x2948` (V2, m/s live).
- Block B reads both (`f_140503430.c` tail: `local_120 = *(G+0x40)` ball;
  `FUN_140A57730` → V1, `FUN_140A57720` → V2; `a1 = AngleOf(V1)`, `a2 = AngleOf(V2)`,
  `x6 = a2`, the card-angle path when flag80/R13).

Writer (CONFIRMED): the per-frame ball block copy `FUN_140430C30` region
(`0x140430DE8 ADD RDI,0x2960; 0x140430DEF LEA RCX,[RBX+0x2960]` + 12-byte copy;
`disCopy.txt`). It mirrors N+0x2960 → B+0x2960 every frame (same function copies
+0x2948 at `0x140430DA2/DA9`). V1 is the ball's **alternate velocity sample**
(V2's mirror-slot: predicted vs current — which is which is INF; defshape uses
+0x2948 as m/s OBS live). Ultimate engine writer is the sim step (foundation F2:
"writer: engine").

GF carriage: **1:1 as values**. `Block9Inputs::{ballV1, ballV2}` (`Vec3`,
`pesreactionblock.hpp`) + `Ball::GetMovement()` for V2 (foundation F2 faithful).
Port spec: fill V1 from the predicted velocity (`Predict(1) − Predict(0)` per tick)
when the block wires in; the copy itself needs no port (GF has one ball, not B/N).

Status: **CONFIRMED** (accessors, copy writer, GF homes).

## §6 Card words PA+0x1A/0x1C/0x38/0x40/0x60 — CLOSED (CONFIRMED reads/roles)

`PA = FUN_140A48910(G,id)` = the published anime-state record (duel-sim §3,
a3helpers §3: `hu(i) = *(G+(i+0xA5)*0x28)`, bulk copy of `Player+0xC8`).
Reads in block B (CONFIRMED asm + `f_140503430.c:330-365`):

| word | site | role in 27A0 |
|---|---|---|
| `+0x1A` u16 | `0x140503D05 CMP [RAX+0x1A],CX` (vs `+0x1C`) | clip frame (duel-sim §3: `hu+0x1a` clip frame, `hu+0x1c` contact frame) |
| `+0x1C` u16 | `0x140503D01 MOVZX ECX,[RAX+0x1C]` | contact frame; `w1c < w1a` takes the `f11c` angle arm |
| `+0x38` byte | `0x14050387A MOVZX EAX,[RBX+0x38]`; `(b−3) <= 1` | R13 = class bit 5 with `b38 ∈ {3,4}` (TeamT2 correction) |
| `+0x40` byte | `0x140503DAE CMP [RBX+0x40],5` | class-5 zero test: holder class bit 5 with `b40 == 5` zeroes block[3] |
| `+0x60` f32 | `0x140503D0B MOVSS XMM6,[RAX+0x60]` | card angle candidate (vs the computed f60 term) |
| `+0x11c` f32 | `0x140503D21 MOVSS XMM6,[RAX+0x11C]` | card angle override (taken when no-card-flag, bit 11 set, `w1c<w1a`, or f60-term exceeds) |
| `+0x8` bit 11 | `0x140503CF5 MOV ECX,[RAX+8]; SHR RCX,0xB; TEST CL,1` | flag80 = class bit 4 path uses the stored angle |

Writer: **OPEN by inheritance** — the anime-state publisher bulk copy
(`FUN_1406E1CE0`/`FUN_140A47290`, duel-sim §3; a3helpers §3). Same publisher as §2.

GF carriage: **1:1 as fields**. `CardFacts::{w1a, w1c, b38, b40, f60, f11c,
bit11AtEight}` (`pesreactionblock.hpp:115-124`) names every word. No GF producer
fills them (caller-supplied in `Block9Inputs::{cardHolder, cardSub}`). Port spec:
source from the anime-state publisher when it lands; until then caller-supplied
with `// ponytail:` naming `0x140503CF5/D01/D05/D0B/D21/DAE`.

Status: **CONFIRMED** (all six addresses, roles, GF field map); writers **OPEN**
(publisher); semantics of `+0x60` vs `+0x11c` (which angle is which) **INF**.

## §7 TM+0x25CF8/+0x25D38 (flags 13/14/15) as 27A0 uses them — CLOSED (CONFIRMED)

Reader (CONFIRMED, re-read this session): `FUN_140A63950(team, side, id, slot)` =
1 iff ∃ j<4 with `MA+0x25CF8+4·(4·side+j) == id` and partner
`MA+0x25D38+… == slot` (side < 2, id < 0x11). 27A0 call sites (CONFIRMED disasm):

| site | call | gate | effect |
|---|---|---|---|
| `0x140502F32` | `a63950(T,side,0xB,0xFF)` + newest kind == 3, sub-3 ≤ 1 | CAUTION_CROSS team-wide | score *= 0.5 (`fVar7` = 0.3, `0x142590D98`) |
| `0x140502F6C` | `a63950(T,side,0xC,0xFF)` + newest kind == 2 | CAUTION_THROUGH_PASS team-wide | score *= 0.5 |
| `0x1405030C7` | `a63950(T,side,0xE,slot)` (R9D = ESI, the candidate) | MARK_HARD on this slot | score *= 0.2 (`fVar8` = 0.2, `0x142598578`) when p5 |
| `0x1405030F9` | `a63950(T,side,0xD,0xFF)` + p5 | GEGEN_PRESS team-wide | stamina ramp: `(max−cur)·0.2/0.8` band (max `0x140A58450`, cur `0x140A58350`) |

Ids (tactics §6.3, OBS census): 0xB CAUTION_CROSS (Swarm the Box), 0xC
CAUTION_THROUGH_PASS (Deep Defensive Line), 0xD GEGEN_PRESS, 0xE MARK_HARD
(Tight Marking, per-player), 0xF READY_COUNTER (Counter Target, per-player).
27A0 uses 0xB/0xC/0xD team-wide (`pid 0xFF`) and 0xE slot-specific — consistent
with the "exactly 8/0xA/0xE/0xF take a slot" rule (0xB/0xC/0xD are team-level here).

Writer: **OPEN (static)** — foundation-teamplayer T-INST: "S1 0 hits; no
lea/memcpy setup refs the base"; directive §2.3: live only the two attacking
records armed, GEGEN_PRESS episodes switch on/off in play (tactics §7.1). The
mark-table doc's writer (`FUN_14045B420` via `FUN_1404587F0`, split displacement
`0x228D4+0x16A8 = 0x23F7C`) is the PAIR table, not this one — do not confuse them.

GF carriage: **NONE wired** (GF instruction check is a stand-in false:
lane-post §8; `A63950` in `pesoffball.hpp`). Port spec: the existing
instruction-active input (`teaminstructions.cpp` mask → 4-record/plan structure
per tactics §6) feeds these four tests when the table import lands; until then
false with `// ponytail: 0x140502F32/F6C/0C7/0F9`.

Status: **CONFIRMED** (reader, all four sites, gates, effects, id labels);
writer **OPEN** (static; live behaviour known).

## §8 27A0 bound/named/unbound table (updated)

Routine args (CONFIRMED): `FUN_1405027A0(T, side, side, 0, p5, p6, 0xFF)`;
both step-9 call sites (`0x14050244F` Block-B zero path, `0x140502610` kind-6)
pass `p5 = p6 = 0, p7 = 0xFF` (SIL = 0 at both; TeamT2 OPEN-3 note on the wiring
stands).

Bound (GF carries 1:1 — unchanged + §5):
- Event store (kind, frame, actor key) + second key E+0x180700: `EventStore`, `EventAt`, `NewestSecondKey`.
- MI+0x116C / +0x1748 / +0x1198: `pessetpieceteam.hpp`.
- Ball sample 0 (B+0x88), height (B+4), velocity V2 (B+0x2948): `Ball::Predict`, `GetMovement`; V1 (B+0x2960): `Block9Inputs::ballV1` (§5).
- Ball owner TM+0x24838: `GetBallRetainer()`.
- Latch T+0x255: `lastTickPossession`.
- Role class T+0x4C00[k]: `SideRoles::b`.
- Reaction delay T+0x6A4[k]: `reactionDelay` (+ mirror `reactionDelayMirror`).
- Stamina cur/max (PMS+0xC8/+0xE8): `pes::StaminaState::{cur, max}`.
- Live player (+0x5D0/+0x55C/+0x554/+0x574): player position/facing/velocity/action.
- Rating table + keeper index: `pesattrs` (row 0x20, `FUN_140A62610/2690`).
- Card words + keeper bit: `CardFacts` fields (§6) + `keeperBit36` (§2, to add).
- Anchor/ball points: `Block9Inputs::{anchor, ballSample0, ballV1, ballV2}` (§3/§5).

Named (meaning decoded, new GF state needed; 7 items — unchanged):
1. T+0x558[k] starter slot id. 2. T+0xB8D0 target player id. 3. T+0x620[k] PCC lane id.
4. Live +0x6B8 frames-held counter. 5. TM+0x24710/0x247BE NearestEntry table.
6. TM+0x228D4/0x228D8 goal geometry. 7. Roster G+0x180 (predicates ported).

Unbound → resolved:
1. ~~Rating bytes PMS+0x84/+0x85~~ → CLOSED §1 (ab+0x14 OA / ab+0x15 DA; INF-high labels; writer OPEN).
2. ~~Block-9 keeper bit P+0x40 (0x24)~~ → CLOSED §2 (CONFIRMED read/branch; writer OPEN publisher).
3. ~~Anchor P+0x1414 (+0x141C)~~ → CLOSED-as-address/role §3 (base point; writer OPEN narrowed).
4. ~~B+0x80 flag~~ → CLOSED-as-read §4 (volatile AND-gate; writer OPEN).
5. ~~B+0x2960 (V1)~~ → CLOSED §5 (accessor + per-frame copy writer; GF 1:1).
6. ~~Card words PA+0x1A/0x1C/0x38/0x40/0x60~~ → CLOSED §6 (all reads/roles; writers OPEN publisher).
7. ~~TM+0x23F7C~~ → CLOSED by mark-table §1 (`FUN_14045B420` via `FUN_1404587F0`; not this ticket's work, cited).
8. ~~TM+0x25CF8/+0x25D38 (flags 13/14/15)~~ → CLOSED-as-used §7 (four sites, gates, effects; static writer OPEN).
9. ~~Press level T+0xA294/A29C~~ → CLOSED by team-status (encoder `FUN_140A602B0`, setters §2, GF `teamLevelRaw`).

Remaining OPEN (all writer-side): in-match stat packed source (§1); anime-state publisher (§2, §6);
anchor origin writer (§3, narrowed); B+0x80 writer (§4); instruction static writer (§7);
`+0x60` vs `+0x11c` angle semantics (INF, §6).

Verdict: every 27A0 input is now bound (value + meaning + GF home) or named with its
exact GF gap. The port is unblocked on decode; it waits on slice-D state + the five
writer imports above.

## §9 GF port specs (exact, per item)

1. Stats: add `uint8_t statOA, statDA` (40..99) per player; init from the VFS in-match
   block when the packed-source import lands (stock archive only). Selector: p5.
   Formula §1. Until then: 27A0 stays unwired (no safe constant — the term scales).
2. Keeper bit: add `bool keeperBit36` beside `CardFacts::bit11AtEight`; publisher sources it.
3. Anchor: fill `Block9Inputs::anchor` from the man's ground position (first-touch "base").
4. Volatile: add `bool ballVolatileSuppressed` (B+0x80 != 0), default false.
5. V1: fill `ballV1` from `Predict(1) − Predict(0)` per tick.
6. Card: fill all six `CardFacts` words from the anime-state publisher when it lands;
   caller-supplied until then.
7. Flags: feed `a63950(side, 0xB/0xC/0xD/0xFF, 0xE/slot)` from the instruction-active
   input (existing mask → 4-record structure); false until the import lands.

## Evidence ledger

| ID | Claim | Conf | Evidence |
|---|---|---|---|
| 27A0-01 | ab+0x14/0x15 selector + value rule | CONFIRMED | 0x140502C60/C67/C69/C6F (checker); 0x140502C73–CDE (decompile) |
| 27A0-02 | 0x14 = OA / 0x15 = DA (in-match space) | INF-high | matchlogic §1.3 groups + Aoba order; anim-select-2 §1.4 pair switch |
| 27A0-03 | Player.bin 0x14/0x15 = Dribbling/Tight Possession (different space) | CONFIRMED | stock Player.bin 4830 recs, rec 0 id 70101 bytes 77/77 (VFS dump) |
| 27A0-04 | keeper bit (P+0x40>>0x24)&1 + *0.5 branch | CONFIRMED | 0x140503020/024/028/032/037/03C (checker) |
| 27A0-05 | anchor = AI+0x1414 base point, block[0] role | CONFIRMED | 0x1405036D5 (checker); first-touch § (base); TeamT2 block[0] |
| 27A0-06 | no MOVSS [.+0x1414] store image-wide; copier 431800 | CONFIRMED | brute-force F3 0F 11 scan (0 hits); 0x140431B0F/B16 |
| 27A0-07 | B+0x80 volatile AND-gate | CONFIRMED | 0x14063AA9A/A3 (disVol); ball-track audit 19-10-26 |
| 27A0-08 | V1 = B+0x2960, copy writer 430C30 | CONFIRMED | 0x140A57730 decompile; 0x140430DE8/DEF (disCopy) |
| 27A0-09 | card words addrs + roles | CONFIRMED | 0x140503CF5/D01/D05/D0B/D21/DAE + 0x14050385B/871/87A (disB) |
| 27A0-10 | flags 0xB/0xC/0xD/0xE sites+gates+effects | CONFIRMED | 0x140502F32/F6C, 0x1405030C7/0F9 (checker flg.b/c); tactics §6.3 |
| 27A0-11 | GF homes (§8 bound list) | CONFIRMED | grep `src/sim` 24-10-26: pesreactionblock.hpp, playerdb, match, pespress, pesdefshape |
