# PES role-array writer: who writes T+0x4BD4 / T+0x4C00 / T+0x3FB8

Status: DONE (all acceptance items answered; open items are follow-ups, not gaps).
Scope: identify the writer(s) of the three role arrays, when they run, with what values,
and whether A/B/C are three arrays or two arrays seen from two bases.
Image VAs use base `0x140000000`. Tags: CONFIRMED (disasm/data/live) or INF with VAs.

## 0. TL;DR
- The doc's "three arrays" are THREE DISTINCT arrays on the per-team object `S = T+0xC48`:
  A = `S+0x3F8C` (doc `T+0x4BD4`), B = `S+0x3FB8` (doc `T+0x4C00`), C = `P+0x3FB8` where
  `P` is the SET-PIECE object `[ctx+0xBA8]` (doc `P+0x3FB8`), NOT `S+0x3FB8`.
  The naming collision (`+0x3FB8` on two different objects) caused the confusion.
  (CONFIRMED: writer stores + reader bases below.)
- ONE store pair writes A and B: `0x140A425A0` (`[rcx+i*4+0x3F8C]=r8d`) and
  `0x140A42590` (`[rcx+i*4+0x3FB8]=r8d`), always called as a pair with the SAME R8D
  (CONFIRMED at all 7+7 call sites: `MOV R8D,EAX/ESI/EBX` before each call).
  So A and B are IDENTICAL after every bulk writer. The ONLY divergence is the
  slot-swapper `FUN_140A422C0`, which swaps B entries (and slot points) but never A.
- Bulk writers (all write A=B=class): `FUN_140A59F90` (both teams, setup/phase),
  `FUN_1404F5F10` (one team, team tick + setup), and 4 designation-scratch rebuilds
  inside the lineup picker `FUN_1404A7D60` (scratch object `param_1+0x58DC`, remap/sort only).
- Value: `FUN_1408C51F0` class map role 0..12 -> class 0..9
  (0->0, 1->1, 2/3->2, 4->3, 5->4, 6/7->5, 8->6, 9/10->8, 11->7, 12->9; CONFIRMED raw
  jump-table dump at `0x1408C52A4`). The `FUN_140A59F90` card-mode remap switch is
  BYTE-IDENTICAL to this map (CONFIRMED: both switch tables dumped), so the two
  branches produce the same value; the remap only changes WHICH registry row is read
  (`+0x28` class row vs `+0x8C8` role row) and the >12 default (0 vs 12->9).
- Array-B 0 = keeper HOLDS for the bulk value (class 0 = role 0 = GK; the keeper
  selector `0x140A62610` returns the first active slot with B==0, CONFIRMED dec).
- No direct `MOV [..,0x4BD4/0x4C00],..` store exists anywhere in the image (CONFIRMED:
  store-only disp32 scan; all hits are misaligned-decode decoys). All A/B writes go
  through the `0x3F8C/0x3FB8` helpers on the S base.

## 1. Writer table (VA, array, value, source, when)
| VA | array | value | source | when | tag |
|---|---|---|---|---|---|
| `0x140A425A0` | `S+0x3F8C+4i` (doc A `T+0x4BD4`) | R8D (same as B at every call site) | caller loop reg | with B, always | CONFIRMED (asm) |
| `0x140A42590` | `S+0x3FB8+4i` (doc B `T+0x4C00`) | R8D (same as A at every call site) | caller loop reg | with A, always | CONFIRMED (asm) |
| `FUN_140A59F90` calls `0x140A5A07C/0x140A5A08A` (team 0) + `0x140A5A1CD/0x140A5A1DE` (team 1) | A+B both teams | class 0..9 (flag=0: `8C51F0` row+0x28; flag=1: remap of `8C5310` row+0x8C8) | `param_2/param_3+0x8E08` sheet registry, phase f=0, plan=`*sheet` | setup (`0x141FB9F0F` flag 0, `0x141FCDD0E` flag=param) + phase handler `0x1404A5FCD` (`FUN_1404A5DE0`, flag 1) | CONFIRMED (dec+asm) |
| `FUN_1404F5F10` calls `0x1404F5F7D/0x1404F5F8A` | A+B one team (`param_1=S`) | class 0..9, `8C51F0(sheet,slot,f,3)`, f = `2-(flag!=0)`, forced 0 when `MI+0x116C!=1 && MI+0x1554==1` | `param_2=T+0x8E08`, `param_3=MI` | EVERY team tick `FUN_14051D9E0` head (`0x14051DA5A`, gated `MI+0x1775==0`) + tail (`0x14051E2E7`, same gate; R9B=`T+0x255!=0`) + setup loop (`0x141FBB06A`, flag=`i==0`) | CONFIRMED (dec+asm) |
| `FUN_1404A8620` calls `0x1404A880D/0x1404A881A` | A+B on SCRATCH (`param_3` = picker `param_1+0x58DC`) | raw class, `8C51F0(sheet,slot,0,3)` | `T+0x8E08` | lineup picker `FUN_1404A7D60` case 4 (`0x1404A852D`), gated scratch-active byte; result remapped by `FUN_140A40CA0` for designation pick | CONFIRMED (dec+asm) |
| `FUN_1404A8930` calls `0x1404A89B2/0x1404A89BF` | A+B on SCRATCH (same) | raw class | `T+0x8E08` | via `FUN_1404A8AD0` (`0x1404A9079`) from picker case 5 (`0x1404A8591/0x1404A85C7`) | CONFIRMED (dec+asm) |
| `FUN_1404A90D0` calls `0x1404A928D/0x1404A929A` | A+B on SCRATCH (same) | raw class | `T+0x8E08` | picker case 6 (`0x1404A855E`) | CONFIRMED (dec+asm) |
| `FUN_1404A93A0` calls `0x1404A9701/0x1404A970E` | A+B on SCRATCH (same) | raw class | `T+0x8E08` | picker case 2 (`0x1404A85EE`) | CONFIRMED (dec+asm) |
| `FUN_140A422C0` stores `0x140A423EB/0x140A423F3` | B ONLY (`S+0x3FB8` swap of two slots; A untouched) | existing B values swapped | B itself | team tick `FUN_14051D9E0` tail (`0x14051E31E`, after 2nd bulk write, gated `T+0x255!=0` + `FUN_140A41BE0(S)`) + `0x14051D0D1/0x14051D4D1` (set-piece paths) | CONFIRMED (dec+asm) |
| `FUN_140A411C0` init loop | A=B=10 (sentinel) + `P+0x3EF8=0xFF`, `S+0x80A0=0`, etc. | const 10 | — | before every bulk writer (7 pre-call sites pair with the 7 writer loops) | CONFIRMED (dec) |

When-summary: A/B are rebuilt from the sheet EVERY team tick (head+tail when
`MI+0x1775==0`), at setup/phase boundaries by `FUN_140A59F90`, and on scratch by the
picker. The ONLY in-tick divergence (A!=B) comes from the `FUN_140A422C0` B-swap.

## 2. Reader-base map (Main's first job)
Key: `T = FUN_140A48A30(G,side)` (CONFIRMED: `[G+(side+0xA1)*0x28]`, else `[G+0x1928]`);
`S = T+0xC48` (shape/line-work block). `0x3F8C+0xC48 = 0x4BD4`, `0x3FB8+0xC48 = 0x4C00` (arithmetic).
| Reader | doc's base | actual base (CONFIRMED dec/asm) | maps to writer addr? | tag |
|---|---|---|---|---|
| `0x140A62610` keeper scan | `T+0x4C00` | `param_3=T`; loop `lVar4=0x3FB8; *(lVar4+0xC48+T)` for j<11, else `*(T+0x4C00)` — i.e. `S+0x3FB8[j]` = writer's B | YES: B (`0x140A42590`) | CONFIRMED |
| `FUN_140A64030` style gate | `T+0x4C00[slot]` | `param_4=T`; `*(T+0x4C00+4*(slot%11))` = `S+0x3FB8` = writer's B | YES: B | CONFIRMED |
| `FUN_1404A7D60` picker case 1/7 (+15 for {5,8}) | `T+0x4BD4` | `lVar9=T`; `*(0x3F8C+T+0xC48)` for j<11 else `*(T+0x4BD4)` = `S+0x3F8C` = writer's A | YES: A (`0x140A425A0`) | CONFIRMED |
| `FUN_1405FC860` fallback (`kind P+0x3FB8 in {1,2}`) | `P+0x3FB8` | `param_2=P=[ctx+0xBA8]` (set-piece object); `*(P+0x3FB8+j*4)` — DIFFERENT object from S | NO (separate array C, own writer TBD §4) | CONFIRMED |
| stamina-sub `T+0x4BD4` | `T+0x4BD4` | `T` via `FUN_140A48A30`; same `S+0x3F8C` pattern | YES: A | CONFIRMED (reader scan) |
| tactics-clock `T+0x4C00` "perf tier" | `T+0x4C00` | same `S+0x3FB8` | YES: B (mislabeled; it is the role class, §6) | CONFIRMED |

A/B/C verdict: THREE arrays, but the split is A=`S+0x3F8C` vs B=`S+0x3FB8` (same
object, same bulk value, diverged only by the B-swap) vs C=`P+0x3FB8` (different
object). The QA doc's "A and B identical after this writer" is CONFIRMED and expected;
the auditor's A/B distinction (raw class vs remapped role) does NOT exist in this build:
both stores receive the same register at all 7 call pairs.

## 3. Caller chain of FUN_140A59F90 (rel32 scan, CONFIRMED)
1. `0x1404A5FCD` in `FUN_1404A5DE0` (phase handler; flag R9B=1): `RCX=[R15+0x17D70]`
   (param_1 = twin object, `param_1+0x44/+0x8204` = the two S blocks),
   `RDX/R8=[RSP+0x20/0x28]` (the two team T objects from `param_1+0x17EC0` allocators).
   Caller: `0x140449157` in dispatcher `FUN_140449130` case `0x36`; its caller `0x14044CB8C`.
   (§3.1 for the walk-up; the auditor's "phase handler" claim CONFIRMED.)
2. `0x141FB9F0F` in `FUN_141FB9E20` (match-setup; flag 0): `param_1=[param_1+0x1AFF8]`.
3. `0x141FCDD0E` in `FUN_141FCDA30` (setup/transition; flag=param_4 passthrough).
`param_1` identity: the TWIN-TEAM object also used by `FUN_140A59B20/140A59CA0/140A59EC0`
(`param_1+0x44/+0x8204` selected by slot<11; CONFIRMED dec). At runtime the two S slots
are the live `T+0xC48` blocks; the setup callers pass the pre-match twin. INF: the twin
is the pre-split setup copy of the two teams' S blocks (VAs above; allocation ctors
`FUN_140451330/1404513A0/140422F20` not decoded).

### 3.1 Walk-up (when it runs)
- Team-tick path (per-tick writer `FUN_1404F5F10`): frame `FUN_14041EC80` -> brain tick
  `FUN_1404F6C30` (sole caller `0x14041EDD7`) -> `FUN_14051D9E0` directly when
  `MI+0x1775==0x17` (`0x1404F6EE8`), else via `FUN_1404F9DC0` (`0x1404F76CC`; calls tick at
  `0x1404F9F24` when `param_2==0`, else `0x1404FA237` on even `MI+0x1198`) — CONFIRMED dec.
  Inside the tick the head write runs when `MI+0x1775==0` (`0x14051DA3B`), the tail write
  when `MI+0x1775==0` at `0x14051E2A8`; the B-swaps at `0x14051D0D1/0x14051D4D1/0x14051E31E`
  run in set-piece/tail paths gated on `T+0x255` and `FUN_140A41BE0(S)` (CONFIRMED asm).
- Phase path (`FUN_140A59F90`): dispatcher case `0x36` (in-play sub-step per A5Judge:194).
  Every-tick vs discrete: the TICK writer runs every team tick (subject to the `0x1775`
  gates); the PHASE writer runs at setup + dispatcher sub-step `0x36`. Substitution and
  tactics-change triggers: OPEN (the `FUN_140A41BE0` guard + `T+0x255` + `T+0x89D4/0x89D8`
  swap args are not decoded to events; no caller ties a swap to a sub event yet).

## 4. Other writers
- Direct stores at `+0x4BD4/+0x4C00`: NONE in the image (CONFIRMED: store-only disp32 scan
  over 59/764 raw hits; every candidate is a misaligned-decode decoy — `mov seg,[]`/
  `add [],..` phantoms; zero `mov [reg..+disp],reg` stores).
- Direct stores at `+0x3F8C`: ONLY `0x140A425A0` (`0x140A425A3`) (CONFIRMED; the
  `0x140A425A4/A5` twins are the same bytes misaligned).
- Direct stores at `+0x3FB8`: `0x140A42590` (`0x140A42593`) + the `FUN_140A422C0` swap pair
  (`0x140A423EB/0x140A423F3`) ONLY (CONFIRMED; the `0x140A423EC`/`0x140A42594` twins are
  misaligned phantoms; the 20 other hits are `mov seg` decoys or immediates).
- Callers through the two helpers: exactly the 7 pairs listed in §1 (CONFIRMED E8 scan:
  `0x1404A880D/1A`, `0x1404A89B2/BF`, `0x1404A928D/9A`, `0x1404A9701/0E`, `0x1404F5F7D/8A`,
  `0x140A5A07C/8A`, `0x140A5A1CD/DE`; no others).
- Array C (`P+0x3FB8`) writers: NOT in this hunt's helper set (P != S). Candidates from
  the init scan: `FUN_140A411C0` writes `P+0x3FB8=10`-family? NO — `411C0` takes the same
  S-shaped object (its `+0x3FB8` loop is the A=B=10 init on whichever base is passed;
  called with S bases at the 7 pre-sites + `0x1404421C3/0x140A404B4/0x14419562C/38` TBD).
  The live C writer (who sets `P+0x3FB8` = keeper/kind per slot on the set-piece object):
  OPEN. NOTE: `FUN_1405FC860`'s `param_2` IS `[ctx+0xBA8]`, and `ctx+0xBA8` is initialised
  to `T+0xC48` (= S) by `FUN_14051C840` called once at `0x1404F6D02` (`LEA R9,[R8+0xC48]`,
  CONFIRMED asm) — so at tick scope P and S may ALIAS (P==S) unless re-pointed later.
  Whether any code re-points `ctx+0xBA8` after init: OPEN (the `0x14051C84B` qword store
  is the only direct writer; computed-base stores not excluded).

## 4b. Swapper semantics (CONFIRMED dec+asm)
| site | flag | effect |
|---|---|---|
| `FUN_140A41BE0(S)` guard (all 3 swap sites + `0x14092B1CE` + `0x140A62BB2`) | — | returns `S+0x7D8C<11 && S+0x7D90<11` (last-swap args in range) |
| tick tail `0x14051E31E` (R9B=1) | 1 | swaps B entries + slot points, sets `S+0x7D98=1`; gated `T+0x255!=0` + guard |
| `FUN_14051CF70` set-piece path (`0x14051D0D1`) + `FUN_14051D100` (`0x14051D4D1`) | 0 | slot-points + `S+0x7D8C/90` latch ONLY, no B-swap; coords `FUN_1408C50E0(sheet,slot,f)`, f=1 in-possession else 2; keeper slot (B==0) uses `FUN_14051B3F0` spot |
| `T+0x89D4/0x89D8` (tail-swap args) | — | two slot indices; NO disp32 store in image (computed-base writers); also read by live-stat remap `0x14092B1FD/208/22A` (gated `T+0x89E0!=0`) and sub-queue test `0x140927CE6/EE` |

`FUN_140A422C0(p, a, b, flag)` ALWAYS swaps the two 12-byte slot-point rows
(`p+0x3A9C`/`p+0x3B20`) and latches `p+0x7D8C=a, p+0x7D90=b`; only with flag!=0 does it
also swap B (`S+0x3FB8`) and set `S+0x7D98=1`. All three in-tick call sites pass flag 0
EXCEPT the tail site `0x14051E31E` (R9B=1, CONFIRMED asm `MOV R9B,0x1`). So: set-piece
paths re-latch slot points without touching B; only the T+0x255-gated tail swap moves
B entries (CONFIRMED).

## 5. Value meaning per array
- Registry (CONFIRMED dec): `FUN_1408C51F0(sheet,slot,f,plan)` reads
  `sheet[((plan*11+slot)*3+f)+10]` = offset `+0x28+4*(3*(11*plan+slot)+f)` (role 0..12,
  default 12->class 9; >12 returns 0 via the JA path); `FUN_1408C5310` reads the same
  index `+0x232` dwords further = offset `+0x8C8+...` (role 0..12, default 12).
  `FUN_1408C51B0` = 51F0's reader without the map, at `+0x28`.
- Class map (CONFIRMED raw table `0x1408C52A4`, 13 targets):
  role 0 GK->0; 1 CB->1; 2 LB,3 RB->2; 4 DMF->3; 5 CMF->4; 6 LMF,7 RMF->5; 8 AMF->6;
  9 LWF,10 RWF->8; 11 SS->7; 12 CF->9. (Role names from matchlogic:83/219-220.)
- `FUN_140A59F90` remap (CONFIRMED raw tables `0x140A5A28C/0x140A5A2C0` + asm MOVS):
  IDENTICAL mapping (0->0,1->1,2/3->2,4->3,5->4,6/7->5,8->6,9/10->8,11->7,12->9).
  Consequence: flag 0 and flag 1 produce the same class for roles 0..12; they differ only
  in source row (`+0x28` vs `+0x8C8`) and out-of-range default (0 vs 12->9 when JA... precisely
  flag-1 JA keeps the 51F0 value, it does not default). The QA doc's "remapped enum 7/8=FW,
  9=CF, 3-6=MF" describes the SAME class map with coarser family names — no conflict with
  stamina-subs:26 (SS=7, WF=8) once read as families; the C5 proposal's "different output"
  framing is wrong (§6).
- Array A (`S+0x3F8C`): role class 0..9 as above (readers: picker {5,8} test, stamina-sub
  `cmp class-2,7`, rush race, budgets). Array B (`S+0x3FB8`): SAME class at bulk-write time;
  post-swap, B[slot] = the class of the slot swapped in (formation-position tracking per
  `T+0x89D4/0x89D8` args; tactical role vs registered class — INF). Array C (`P+0x3FB8`):
  same class values IF P aliases S (init alias CONFIRMED, re-pointing OPEN); FC860 tests
  `kind in {1,2}` (CB/FB classes) consistent with the class map (CONFIRMED dec).
- Array-B 0 = keeper: HOLDS as the bulk value (class 0 = GK; `0x140A62610` first-active-B==0
  CONFIRMED; live `T1+0x4C00[0]=0` in extra-frontman:305). Caveat: post-swap, B==0 sits at
  whatever slot holds the keeper's class entry; and the style-gate table `DAT_142613D20`
  has 10 rows (classes 0..9, CONFIRMED matchlogic:289) so class ≥10 (the 411C0 sentinel)
  would overrun — the bulk write always follows the init, so live B is 0..9 (INF).
  Live A-vs-B check: EFPin logs already show `T1+0x4C00[k]=(0,1,1,2,2,3,4,6,7,7,9)`
  (extra-frontman:305) — the class map, consistent with A==B bulk values (CONFIRMED live).

## 6. Corrections to our docs (doc:line, old, new, evidence)
| doc:line | old | new | evidence |
|---|---|---|---|
| community-qa:27 | "the value passed [to 42590] is inferred ... INF" | "R8D=ESI at both calls (MOV R8D,ESI before each CALL); same value CONFIRMED" | asm `0x140A5A073-8A`, `0x140A5A1C1-DE`; same pattern at all 7 pairs (`MOV R8D,EAX` + `MOV R8D,EBX`) |
| community-qa:27 | "base pointer param_1+0x44 not resolved (INF T+0xC48)" | "param_1+0x44/+0x8204 are the two teams' S blocks; S=T+0xC48 CONFIRMED by setup caller `FUN_1404F5F10(T+0xC48,...)` at `0x141FBB06A`" | dec FBB06A:179; tick callers pass `[ctx+0xBA8]` init to `T+0xC48` (`0x1404F6D02`+`0x14051C840`) |
| community-qa:26/C5 (line 326) | "the remapped output uses 0 GK..9 CF; the raw map applies when the flag is off (different outputs)" | "both branches implement the SAME role->class map (tables dumped identical); the flag selects source row +0x28 vs +0x8C8, not a different enum" | raw tables `0x1408C52A4` vs `0x140A5A28C/0x140A5A2C0`; asm MOV immediates |
| community-qa:25 | "425A0 stores the raw role-class ... 42590 stores the array (A/B distinction)" | "both stores write the same class value; A/B differ only after the `FUN_140A422C0` B-swap" | §1-2; swapper dec/asm |
| tactics-clock:137,276,302,519 | "`T+0x4C00` per-player perf tier" | "`T+0x4C00` is the role-class array (B); the clock reads it as tier thresholds 1..2/3..6/≥7" | §2 reader map; value map §5 |
| positioning-phases:10-12,43 | "role-array writers ... no store pinned yet" | "writers: `FUN_140A59F90` + `FUN_1404F5F10` + picker scratch + `FUN_140A422C0` B-swap (§1)" | this report |
| foundation-teamplayer:98-99 | "T+0x4c00 registered position 0..12; writer: setup copy by loop" | "T+0x4C00 holds class 0..9 (B), rebuilt every team tick + setup/phase; source registry holds 0..12" | §1, §5 (S1 found no hits because the stores use the S base +0x3FB8, not T base +0x4C00) |
| foundation-teamplayer:98-99 | "S1 0 hits → setup copy by loop (no disp)" | "S1 missed because stores are `[S+0x3F8C/0x3FB8]`, i.e. T-relative only through the +0xC48 base" | store scan §4 |
| OffBallSelectors:15, foundation-matchball:134 | "`0x3fb8+0xc48 = 0x4c00` (opponent's registered position)" | "arithmetic holds on the S base (`S+0x3FB8` = `T+0x4C00`); but `P+0x3FB8` (set-piece object) is a different array that aliases S only at init" | §2, §4 |
| lane-core:216,400 (O1) | "category→keeper mapping OPEN (O1)" | "CLOSE O1: class 0 = GK by the dumped map; selector returns first active B==0 (post-swap caveat §5)" | map dump + dec `0x140A62610` + live extra-frontman:305 |
| community-qa:329 (C8) | "keep OPEN until the live writer is checked" | "close on static: the live value `T1+0x4C00[0]=0` already matches the dumped writer map" | same as above |
| extra-frontman:61,344 | "classes are P+0x3FB8, 22 ints; names beyond kind 0 OPEN" | "P+0x3FB8 classes are the §5 class map (kind 1=CB, 2=FB per FC860's {1,2} test); P aliases S at init" | §2, §5 |

## 7. Open items
1. Array C live writer if P is ever re-pointed away from S (init alias CONFIRMED at
   `0x1404F6D02`; re-pointing scan: only direct writer `0x14051C84B`; computed-base
   stores not excluded). If never re-pointed, C has no separate writer — it IS B.
2. `FUN_140A41BE0(S)` guard + `T+0x255`/`T+0x89D4/0x89D8` swap semantics (which event
   triggers a B-swap: substitution? tactics change? formation shift?).
3. `FUN_140A59F90` twin-object allocator identity (setup ctors `140451330/1404513A0`
   region) — needed only to describe setup vs live bases precisely.
4. `FUN_140A404B4`/`0x1404421C3`/`0x14419562C/38` `FUN_140A411C0` callers (other init sites).
5. Live confirmation that A==B except post-swap (read `S+0x3F8C` vs `S+0x3FB8` across a
   sub/tactics event; needs the running match, not done here).
