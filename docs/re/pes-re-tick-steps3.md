# PES team tick `FUN_1404F6C30` step bodies, batch 3 (26b/c, 28, 33/34, 37-39, 41-44)

Batch-3 decode of the steps team-layer.md §1 lists as OPEN that batches 1+2 left
(step 27c is directive.md §1; step 41's manager is off-ball.md §1; step 44's
dispatcher is defshape §1 + press-life). Base `0x140000000`, image
`~/ghidra-projects/scratch/Unpacker/pes_full.bin`. Disassembly files in
`<workdrive>/gf-TickSteps3-scratch/dis_*.txt` (bridge `/disassemble_function`).
Tags: **CONFIRMED** = raw disassembly or image bytes this pass; **DOC** = cited
doc, not re-checked; **INF** = inference; **OPEN** = blocker named. Decompile
cited for orientation only.

## §0 Float table (script-read, pasted verbatim; `repr` + bits exact)

Script (run this pass; offsets `VA - 0x140000000`; VAs = every `0x142xxxxxx`
operand VA appearing in this batch's `dis_*.txt` float loads):

```python
import struct
f=open('<local>); base=0x140000000
for line in open('vas.txt'):
    va=int(line.strip(),16)
    f.seek(va-base); b=f.read(4)
    v,i=struct.unpack('<f',b)[0],struct.unpack('<I',b)[0]
    print(f"| `0x{va:X}` | `{v!r}` (`0x{i:08X}`) |")
```

| `VA` | `value` (`0xBITS`) | use |
|---|---|---|
| `0x14253EE30` | `100.0` (`0x42C80000`) | step 37 `0x1404FA56E` score scale (×100/÷100 pair) |
| `0x142565600` | `0.5` (`0x3F000000`) | steps 27d `0x14057AC75` / 33 `0x1405798F8,0x140579B1C,0x1405381C0` (+0.5 arm) |
| `0x142565604` | `2.0` (`0x40000000`) | step 41 `0x140525629` (per-tick cap scale) |
| `0x142565D30` | `nan` (`0xFFFFFFFF`) | abs-mask (fabs idiom), NOT a value — steps 33 `0x1405799E4`, 42 `0x14053832F` |
| `0x14256AE1C` | `1.0` (`0x3F800000`) | unit — steps 37 `0x1404FA556`, 42 `0x14053829E` |
| `0x142572348` | `0.800000011920929` (`0x3F4CCCCD`) | step 37 `0x1404FA54A` advance scale (×0.8) |
| `0x14257A190` | `0.6000000238418579` (`0x3F19999A`) | step 37 `0x1404FA565` divisor (÷0.6 rate) |
| `0x142588200` | `0.10000000149011612` (`0x3DCCCCCD`) | step 33 `0x1405799F0` (×0.1 rate) |
| `0x142588218` | `-1.0` (`0xBF800000`) | step 42 `0x1405382AA` sign leg |
| `0x142590D98` | `0.30000001192092896` (`0x3E99999A`) | step 41 `0x140525643` (0.3 floor) |
| `0x14259222C` | `5.0` (`0x40A00000`) | step 27d `0x14057AC66` (×5.0 rate) |
| `0x142598578` | `0.20000000298023224` (`0x3E4CCCCD`) | step 37 `0x1404FA53E` floor (0.2) |
| `0x14259D664` | `-12.0` (`0xC1400000`) | step 42 `0x1405382EB` (gate −12.0) |

Operand-VA pairs (brief-rules 19-10-26; each float cited with the instruction
that reads it): step 27d `0x14057AC66 MULSS XMM0,[0x14259222C]` = 5.0,
`0x14057AC75 ADDSS XMM0,[0x142565600]` = 0.5; step 33
`0x1405799E4 MOVSD XMM6,[0x142565D30]` = abs-mask,
`0x1405799F0 MOVSS XMM7,[0x142588200]` = 0.1,
`0x1405799F8 MOVSS XMM8,[0x142565600]` = 0.5,
`0x140579B18 MULSS XMM0,XMM7` (×0.1), `0x140579B1E ADDSS XMM0,XMM8` (+0.5);
step 37 `0x1404FA53E MOVSS XMM8,[0x142598578]` = 0.2,
`0x1404FA54A MOVSS XMM9,[0x142572348]` = 0.8,
`0x1404FA556 MOVSS XMM10,[0x14256AE1C]` = 1.0,
`0x1404FA565 MOVSS XMM11,[0x14257A190]` = 0.6,
`0x1404FA56E MOVSS XMM12,[0x14253EE30]` = 100.0; step 41
`0x140525629 MOVSS XMM7,[0x142565604]` = 2.0,
`0x140525643 MOVSS XMM8,[0x142590D98]` = 0.3; step 42
`0x14053829E MOVSS XMM8,[0x14256AE1C]` = 1.0,
`0x1405382AA MOVSS XMM9,[0x142588218]` = −1.0,
`0x1405382EB MOVSS XMM10,[0x14259D664]` = −12.0,
`0x14053832F ANDPS XMM1,XMM6` with `XMM6` from `0x142565D30` abs-mask,
`0x1405381C0 ADDSS XMM0,[0x142565600]` = +0.5.

## §1 Triage table (every batch step; no step skipped)

Step numbers and call-site VAs from team-layer.md §1. "Writes" = raw-disassembly
stores with a non-stack destination. Verdict rule (same as batches 1+2): **SIM**
iff some write reaches a reader feeding positions, decisions, possession,
stamina, or AI states; else **NON-SIM** with the proof.

| # | call-site VA | callee VA | size (raw lines) | writes (field, meaning) | readers | SIM? |
|---|---|---|---|---|---|---|
| 26b | `0x1404F6F04` | `FUN_14051B3E0` | 2 | `[ctx+0xBA8]=0` (`0x14051B3E0` CONFIRMED) — teardown of the tick-init alias (role-writer §4: init `0x14051C84B` aliases P to `T+0xC48` at `0x1404F6D02`) | none (null, end of restart path) | NON-SIM (teardown only; cf. step 54 same body) |
| 26c | `0x1404F6F13` | `FUN_140A42A70` | 57 | `T+0x21C`-family row copy (`MOVUPS [R15+RBX]` triple `0x140A42AE0/AE9/AF3` + tail `+0x30` CONFIRMED) gated on per-slot `T+0xA2A0==0x25` skip + `FUN_140A401C0/1F0` match pair | same timer-ring family as steps 31/55 (batch-2 §4) | **SIM** |
| 27c | `0x1404F6F47` | `FUN_140581980` | 49 | `brain+0x6CF0` countdown + `T+0xA508` (=0 clear / =2 armed, `0x1405819C0/2C` CONFIRMED) | `T+0xA508` read by off-ball Chance mask (`T+0xa508 != 0`, DOC off-ball §2.1) | **SIM** (counter arm; directive.md's "27c" port-slot label names the below-tick `FUN_1405DB800` block, a different site — no overlap) |
| 27d | `0x1404F6F4F` | `FUN_14057AC20` | 55 | `T+0xB6DF` (0/1, `0x14057AC98/BB/AD1C` CONFIRMED), `T+0xB6E0` (0/1, `0x14057ACEC/AD05` CONFIRMED) | `B6DF/B6E0` stoppage-pair readers OPEN (no reader in any batch dis or cited doc; grep zero) | **SIM** (T-relative state writes; readers OPEN) |
| 28 | `0x1404F6F91` | inline | ~15 | `T+0x80=0xFF` (`0x1404F6F74` CONFIRMED, gated `116C==1 && latch` skip at `0x1404F6F5D-69`); `T+0x6E6=0` (`0x1404F6F9D` CONFIRMED, gated `A3E190`-record test at `0x1404F6F7E-91`) | `T+0x80` restart-word readers OPEN; `T+0x6E6` readers OPEN | **SIM** (T-relative stores; readers OPEN) |
| 33 | `0x1404F7678` | `FUN_140579840` | 248 | `T+0xB6E8[k]=1` arming (`0x140579AC4` CONFIRMED); `T+0xB718[k]` end-stamp (`0x140579B4C` CONFIRMED); `FUN_140A40820` releases (`0x14057994C/AFE` CONFIRMED); per-concept start/stop virtual calls (payloads OPEN) | `T+0xB6E8` read by step 34 executor (`0x140579C65` CONFIRMED — same-batch reader) | **SIM** |
| 34 | `0x1404F769D` | `FUN_140579BF0` | 149 | per-slot `FUN_140A429A0/42A40/42A30/42B80/42B50` commits (`0x140579D6C-DA4` CONFIRMED) for running concepts with queued moves; `FUN_14057A3E0` stop calls (payload OPEN) | run/concept states (same `A2A0/28C/278` family as steps 22/41) | **SIM** |
| 37 | `0x1404F76D1` | `FUN_1404F9DC0` | 523 | head: `FUN_140A429A0` per-slot releases (`0x1404F9E68` CONFIRMED); mid/tail: `FUN_14051D9E0` role-rebuild calls (3 sites `0x1404F9F24/A237/ADDB` CONFIRMED); `brain+0xBB8`-object point copies (`MOVSS [RAX],XMM6` family, ~20 sites CONFIRMED); tail: `FUN_140A408C0` blend call (`0x1404FA6F9` CONFIRMED) | role/shape path (DOC defshape §14.0); blend rows (same family as step 53) | **SIM** |
| 38 | `0x1404F773E` | inline 11-slot loop + `FUN_140A429A0` | ~70 (tick-inline `0x1404F76D9-807`) | `T+0xA2A0[s]=1` via `FUN_140A429A0(T,s,1,rec)` (`0x1404F77F2` CONFIRMED) gated on concept-slot `T+0xB748` + `T+0x8E00/8D4` + `T+0x7FD7` + prio gate `FUN_140A40050` | designated-supporter state (DOC off-ball §3.1 item 6: forced back to BASE_POSITION so it can never be a runner) | **SIM** |
| 39 | `0x1404F781C` | `FUN_140501160` (cond latch `0x1404F780D/812` CONFIRMED) | 62 | `T+0xB82C[2]` pair (`0x1405011F6` CONFIRMED); `T+0xB828=1` armed (`0x14050122A` CONFIRMED); fallback `B828=1,B830=-1,B82C=-1` (`0x14050123E-24F` CONFIRMED) | `B828/B82C` readers OPEN | **SIM** (writes only; DOC team-layer §2 V4 for the 2-CF meaning) |
| 41 | `0x1404F78BA` | `FUN_140525530` | 244 | run-state writes via `FUN_140A40820` releases (`0x14052580B` CONFIRMED) + selector `vt+0x10` takes (payload OPEN); per-slot timer arms (`FUN_1405A4830`, `FUN_1405969B0` zeroing CONFIRMED call sites) | run states (DOC off-ball §1.2: caps/priorities/timers/stop+select) | **SIM** (DOC off-ball §1–2 for the body) |
| 42 | `0x1404F78D4` | `FUN_140538050` | 280 | head: brain header (`[RCX+8]=RDX,[RCX+0x10]=R8D` CONFIRMED); mode-4 arm: `FUN_140A429A0(T,slot,0x1E)` SPACE_RUN seeding (`0x1405384F5` CONFIRMED) + `FUN_140A42A40` (`0x140538504` CONFIRMED) + `[brain]=1` (`0x14053850C` CONFIRMED) | run states incl. SPACE_RUN 0x1E (DOC teamtick §2.3) | **SIM** (the mode-4 writes; modes 1-3/5-6 callees OPEN — see §7) |
| 43 | `0x1404F78FE` | 11× `FUN_140A40820` (inline loop `0x1404F78E4-903`) | 33 (callee) | `T+0xA2A0[s]=0` + history-ring + target-block zero (`0x140A40842/75/96` CONFIRMED) gated `T+0x218==0` (`0x1404F78DB` CONFIRMED) | run states released | **SIM** |
| 44 | `0x1404F795A` | `FUN_140535C60` | 296 | `T+0x6E6=0` (`0x140535CC3` CONFIRMED); full defence pass calls (MARK `0x140535DDE`, PCC `0x140535E06`, PRESS/DELAY `0x140536002`, dispatcher-loop `A42940` `0x140536059` — CONFIRMED sites); q-gated passes (`536170/535A10/6377F0/6315B0/379C0/369E0/360D0` — CONFIRMED sites) | defence states (DOC defshape §1 + press-life §§1–4) | **SIM** (DOC defshape §1 + press-life for the bodies) |

Triage notes:
- Step 26b's callee `FUN_14051B3E0` is byte-identical in role to step 54's
  (`0x1404F7A6B`, batch-2 §12: `MOV [RCX+0xBA8],0; RET` NON-SIM). Two markers,
  one body, one verdict. Step 26b sits at the end of the restart path
  (`0x1404F6EED` → `FUN_14051D9E0` → `0x1404F6EFF CALL 0x14051B3E0`); step 54 is
  the unconditional tail (`0x1404F7A80`). Both clear the same alias.
- Step 26c's callee `FUN_140A42A70` is the same 57-line body as steps 31/55
  (batch-2 §4). The tick calls it twice: restart-path `0x1404F6F0E` (26c) and
  tail `0x1404F7A8F` (55). One body, one SIM verdict, counted once.
- Step 28's `T+0x80=0xFF` gate re-verified raw this pass: `0x1404F6F5D CMP
  [MI+0x116C],1; JNZ write` + `0x1404F6F66 TEST SIL,latch; JZ skip-write` — the
  write happens when NOT in play or NOT in possession (a restart word).
  `T+0x6E6=0` additionally needs the `FUN_140A3E190`-record test to pass
  (`0x1404F6F8C CALL; 0x1404F6F91 CMP [RAX],5; JZ skip`).
- Step 37's three `FUN_14051D9E0` call sites: `0x1404F9F24` (odd-frame arm,
  `TEST [MI+0x1198],1; JZ even`), `0x1404FA237` (even-frame arm), `0x1404FA6DB`
  (R12B==0 arm). DOC defshape §14.0 for the phase meaning; the CALL sites are
  CONFIRMED here, the phase semantics stay DOC.
- Step 41's gate re-verified raw: `0x1404F7876 MOVZX R14D,[RSP+0x30]` (skip
  byte) / `0x1404F787F JNZ skip-41/42` + `0x1404F7890 AND frame,1; 0x1404F7893
  CMP frame,side; JNZ skip-41` — parity + skip gates CONFIRMED (batch-1 §12
  already named the skip gate at `0x1404F7876`; the parity gate is added here).
- Step 43's gate re-verified raw: `0x1404F78DB CMP [T+0x218],0; JNZ skip` —
  the 11× release runs only when the support-weight counter is zero (links the
  step-11 weight block to the run release).

## §2 Step 26b (`FUN_14051B3E0`): ctx-alias teardown — NON-SIM, CONFIRMED

Raw: 2 lines. `0x14051B3E0 MOV [RCX+0xBA8],0; 0x14051B3EB RET`. `RCX` = brain
(`[RDI+0x6d50]`-adjacent ctx per the `0x1404F6EFF`/`0x1404F7A80` call sites
passing `RDX=T+0xC48`-derived values — INF on the exact object; the STORE
itself is CONFIRMED). Clears the `[ctx+0xBA8]` alias that `FUN_14051C840`
installed at tick entry (`0x1404F6D02`, DOC role-writer §4). No sim state
written (null to a context pointer). Port: none (GF has no brain/ctx struct).
No stand-in needed.

## §3 Step 26c (`FUN_140A42A70`): timer-ring snapshot copy — SIM, CONFIRMED

Raw: 57 lines (same body as steps 31/55, batch-2 §4 — no new decode; triage
only). Per-slot (11×): skip when `T+0xA2A0[s]==0x25`
(`0x140A42ABC CMP [RDI],0x25; JZ skip` CONFIRMED); else
`FUN_140A401C0`-pair match test (`0x140A42AC6` + `0x140A42B08` CONFIRMED);
on match, 52-byte row copy `T+0xA2CC+0x34·s ← src+0xA2CC+0x34·s`
(`MOVUPS` triple + tail dword CONFIRMED). `RCX` = T (dst), `RDX` = src block.
Port: folds into batch-2 slice T6b (`timerRing[11]` snapshot). No new slice.

## §4 Step 27d (`FUN_14057AC20`): stoppage pair `T+0xB6DF/B6E0` — SIM, CONFIRMED stores

Raw: 55 lines. `T` = `[RBX+0x6D50]` (brain-relative, CONFIRMED `0x14057AC26`).
- `B6DF` clear arm: set (`[RAX+0xB6DF]=1` at `0x14057ACBB`) only via
  `FUN_14057AA70` (`0x14057ACAB CALL; TEST AL; JZ exit` CONFIRMED); cleared
  (`=0` at `0x14057AC98`) when already set and (`MI+0x116C!=1`
  `0x14057AC47`, or own-latch clear `0x14057AC58`, or possession-age
  `fps·5.0+0.5 vs MI+0x11A0` `0x14057AC61-8F` — CONFIRMED chain with
  `0x14057AC66 MULSS XMM0,[0x14259222C]` = ×5.0 and
  `0x14057AC75 ADDSS XMM0,[0x142565600]` = +0.5).
- `B6E0` arm: cleared (`=0` at `0x14057ACEC`) when set and `FUN_14057AB30`
  true (`0x14057ACDC/ E1` CONFIRMED); set (`=1` at `0x14057AD05`) when clear
  and `FUN_14057A580` true (`0x14057ACF5/FA` CONFIRMED).
- Tail link (`0x14057AD13-1C` CONFIRMED): when `B6E0` set, `B6DF=1` forced.
Port: new GF `stoppagePair {b6df,b6e0}` + the B6DF clear chain (in-play +
latch + age `fps·5+0.5`) as one slice (**T12a**). Red tests:
`B6DFClearsWhenStoppedOrLatchLost`, `B6E0FollowsItsPredicatePair`.
`FUN_14057AA70/A580/B30` predicate bodies OPEN (payloads, same contract as
batch-1/2 payloads). Readers of `B6DF/B6E0` OPEN (item 1).

## §5 Steps 33/34 (`FUN_140579840` selector + `FUN_140579BF0` executor) — SIM, CONFIRMED

### 33 — selector (248 lines)
Inputs: side + latch (`MOVZX [RSP+0xB8],[T+0x255]` `0x140579898-8A6`
CONFIRMED); registry: `0x140579866 CALL 0x141E5D4C0` with `EDX=0xF`=15
CONFIRMED + `0x140579877 CALL 0x141E5D4C0` with `EDX=0xC4`=196 CONFIRMED
(reg 15 = `.../match/pes15Test.json`, pathptr `0x142B49ED0`; reg 196 =
`.../team/subConcept.json`, pathptr `0x142B4CCD0` — image bytes this pass;
same pair batch-2 §5 named). Gates: `FUN_141B92460 < 5` return
(`0x1405798AF-8B7` CONFIRMED; stub returns 5 so never — same dead-gate idiom
as press-life §3.1); `subConcept[0x68]!=0` return (`0x1405798BD-C1`
CONFIRMED); `FUN_140A604D0(level,0x21)` required (`0x1405798D2-8E6`
CONFIRMED).
Body: head release sweep — per slot with `T+0xA240[slot]==0x22` and
`T+0xB6E8[slot]==0x30`: `FUN_140A40820` (`0x140579930-95C` CONFIRMED);
stoppage arm (`MI+0x116C==2 && MI+0x11A8<=1`: all-48 `FUN_14057A3E0` stop
calls `0x140579961-998` CONFIRMED); main 48-concept loop
(`EDI`=0..0x2F, `0x140579993/9BE` CONFIRMED): per concept
`FUN_140579E10` table lookup (`0x140579A0B` CONFIRMED; null/empty → skip),
`T+0x255`-gated start rules (`0x140579A25-AA8` CONFIRMED shape; helper
semantics OPEN), `T+0xB6E8[concept]=1` arm (`0x140579AC4` CONFIRMED) with the
latch-gated `FUN_14057A1F0` start call (`0x140579AD3-ADF` CONFIRMED), end-test
`FUN_14057A160` (`0x140579A82` CONFIRMED), per-concept stop `FUN_14057A3E0`
(`0x140579AFE` CONFIRMED), running-time hysteresis
(`fps·0.1+0.5` vs frame−stamp `0x140579B08-B3F` CONFIRMED with
`0x1405799F0 [0x142588200]` = 0.1, `0x1405799F8 [0x142565600]` = 0.5).
Outputs: `T+0xB6E8` running flags + `T+0xB718` stamps + releases.
Port slice **T12b** (selector skeleton: gates + 48-loop + B6E8 arms;
`579E10/5A160/5A1F0/5A3E0` payloads OPEN). Red test:
`SelectorArmsB6E8OnlyWhenLevelGatePasses`.

### 34 — executor (149 lines)
Per running concept (`T+0xB6E8[c]!=0`, 48×, `0x140579C65` CONFIRMED):
collect queued-move slots (`T+0xB748==c && state∈{0x25,0x2C}`
`0x140579CA7-CB8` CONFIRMED) → end-test `FUN_14057A070`
(`0x140579CE1` CONFIRMED) → stop `FUN_14057A3E0` when ended-or-queued
(`0x140579CEF-FB` CONFIRMED) → else per queued slot the 5-commit chain
`FUN_140A429A0` (state, `0x140579D6C`) + `FUN_140A42A40` (point,
`0x140579D79`) + `FUN_140A42A30` (byte, `0x140579D88`) +
`FUN_140A42B80` (queued state, `0x140579D96`) + `FUN_140A42B50` (point,
`0x140579DA4`) — all CONFIRMED sites. The commit source rows are
`[RBX+0x20]`-family blocks (`R8D=[RBX+0xC]`, queued `[RBX+0x10]`
CONFIRMED). Port slice **T12c** (executor commit chain). Red test:
`ExecutorCommitsQueuedMovesForRunningConcepts`.

## §6 Step 37 (`FUN_1404F9DC0`): shape-dispatch inner — SIM, CONFIRMED sites

Raw: 523 lines. Three regions:
1. Head release sweep (`0x1404F9E10-9E79` CONFIRMED): per slot with
   `T+0xB748==0x30`-family state and concept mismatch vs `T+0xB8D4`:
   `FUN_140A429A0` release (`0x1404F9E68` CONFIRMED).
2. Registry + shape rebuild (`0x1404F9E7B-EBB` + 3 `51D9E0` arms CONFIRMED):
   `0x1404F9E87 CALL 0x141E5D4C0` with `EDX=0xB7`=183 (basePosition —
   CONFIRMED immediate); `RDX=[RBX+0x6D50]` = T; `R9B`=latch-derived
   (`SETNZ R8B` at `0x1404F9EAC` CONFIRMED); `R12`=tick DL byte. Odd-frame
   `0x1404F9F24 CALL 0x14051D9E0`, even-frame `0x1404FA237 CALL 0x14051D9E0`,
   fallback `0x1404FA6DB CALL 0x14051D9E0` (all CONFIRMED). Between/after:
   `brain+0xBB8`-object float-triple copies (`MOVSS [RAX],XMM6` family —
   ~20 store sites CONFIRMED; object identity OPEN, same caveat as
   batch-2 payloads).
3. Blend tail (`0x1404FA53E-6F9` CONFIRMED): float chain with
   `0x1404FA53E [0x142598578]` = 0.2 floor, `0x1404FA54A [0x142572348]` =
   0.8 scale, `0x1404FA556 [0x14256AE1C]` = 1.0, `0x1404FA565 [0x14257A190]`
   = 0.6 divisor, `0x1404FA56E [0x14253EE30]` = 100.0 (`SUBSS/DIVSS/MULSS`
   chain `0x1404FA600-652` CONFIRMED); per-slot `FUN_140A48A10` resolve
   (`0x1404FA656` CONFIRMED) + blend-triple commits (`0x1404FA676-67F`
   CONFIRMED); final `0x1404FA6F9 CALL 0x140A408C0` (blend rows — same
   family as step 53, batch-2 §12).
Port: DOC defshape §14.0 (phase dispatch) + batch-2 T10c (blend rows); no new
slice — the CONFIRMED sites here are the call skeleton, the bodies stay DOC.

## §7 Step 42 (`FUN_140538050`): mode dispatcher — SIM (mode-4 writes CONFIRMED)

Raw: 280 lines. Head: brain header writes (`0x14053805D/061` CONFIRMED) +
latch gate (`0x14053806B JZ exit` CONFIRMED — team must hold the latch) +
`B8B4`-mode ladder (`SUB EBX,1/JZ, SUB 2/JZ, SUB 1/JZ ×2, CMP 1`
`0x1405380BF-D6` CONFIRMED → modes 1..6 arms at
`0x140538140/126/10C/0F2/0D8`-targets).
- Mode 4 (`0x140538160`, CONFIRMED detail this pass): `FUN_140538550`
  receiver-search call (`0x140538188` CONFIRMED) → `[brain]==0` required
  (`0x14053818D` CONFIRMED) → possession-age gate
  (`MI+0x119C vs fps+0.5`, `0x1405381B4-1CF` CONFIRMED with
  `0x1405381C0 [0x142565600]` = 0.5) → carrier-record chain
  (`A3E190/A60530/A62EA0`, `0x1405381DB-22E` CONFIRMED) → sign-gated
  ball-x test (`MULSS`-via-`A4C720(-12.0)`, `0x140538367-385` CONFIRMED with
  `0x1405382EB [0x14259D664]` = −12.0) → 11-slot MA-order scan with
  `A79640` rank (`0x140538300-40B` CONFIRMED) → up-to-3 takes:
  `FUN_140537C60` build (`0x1405384D1` CONFIRMED) +
  `FUN_140A429A0(T,slot,0x1E)` (`0x1405384F5` CONFIRMED, `R8D=0x1E`) +
  `FUN_140A42A40` (`0x140538504` CONFIRMED) + `[brain]=1`
  (`0x14053850C` CONFIRMED).
- Modes 1/2/3/5/6 callees (`FUN_1405387A0/39860/38160/39AF0/39260`):
  call SITES confirmed (`0x140538146/136/121/107/0ED`); BODIES open (not
  fetched this pass — payloads, same contract).
Port slice **T12d** (mode-4 arm: gates + 3-take + 0x1E seed; other modes
OPEN). Red test: `Mode4SeedsSpaceRunForUpToThree`. DOC teamtick §2.3 for the
0x1E meaning (SPACE_RUN seeding).

## §8 Steps 38/39/43 + 28-inline: small writers — SIM, CONFIRMED

- Step 38 (tick-inline `0x1404F76D9-807`): 11-slot loop over concept slots
  (`R15=0xB748` base CONFIRMED `0x1404F76D9`); per slot: state-filter
  (`0x1404F76E7-700` CONFIRMED), `T+0x8E00` id-match (`0x1404F76FA`
  CONFIRMED), `T+0xB8D4` mismatch (`0x1404F7768` CONFIRMED), kind≠0x2A
  (`0x1404F7774` CONFIRMED), record/predicate chain (`0x1404F7786-7BB`
  CONFIRMED incl. `FUN_140A3E190` + `[RAX]==3` test), then
  `0x1404F77F2 CALL 0x140A429A0` with `R8D=1` (`0x1404F77C9` CONFIRMED) +
  16-byte record copy (`MOVUPS` pair `0x1404F77D1-E8` CONFIRMED). DOC
  off-ball §3.1 item 6 for the meaning (supporter → BASE_POSITION).
- Step 39 (`FUN_140501160`, 62 lines): `87CD==2` gate (`0x14050117B`
  CONFIRMED); 11-slot scan with on-pitch (`+0x1748`, `0x1405011C2`
  CONFIRMED) + class `{7,9}`-family test (`ADD EAX,-7; TEST ~3`
  `0x1405011DE-1E6` CONFIRMED — classes 7/9 = SS/CF per role-writer §5) →
  first-two commits `T+0xB82C[2]` (`0x1405011F6` CONFIRMED, `R11B<2` cap
  `0x1405011FD-201`); exactly-2 → `T+0xB828=1` (`0x14050121D-22A`
  CONFIRMED); else fallback triple (`0x140501237-24F` CONFIRMED). DOC
  team-layer §2 V4 for the 2-CF meaning; sites CONFIRMED here.
- Step 43 (tick-inline `0x1404F78E4-903`): `T+0x218==0` gate
  (`0x1404F78DB` CONFIRMED) → 11× `FUN_140A40820(T,s)`
  (`0x1404F78F9` CONFIRMED). Callee `FUN_140A40820` (33 lines, CONFIRMED):
  `T+0xA2A0[s]=0` (`0x140A40842`) + history-ring `FUN_140A40200`
  (`0x140A4085A`) + target-block zero `FUN_140A85AA0` (`0x140A40870`,
  `0x140A408B9` tail-JMP) + point/qword clears (`0x140A40875-8A5`
  CONFIRMED). Port slice **T12e** (release sweep on `218==0`). Red test:
  `ReleaseSweepRunsOnlyWhenWeightCounterZero`.
- Step 28-inline: covered in §1 triage row (no callee to decode).

## §9 Batch-2 open-field readers found in this batch

Searched all batch-3 `dis_*.txt` for the batch-2 open fields
(`M+0xB8F8/B8FC`, `T+0xBAB4/C128/6C4C/6E7/88D8/B7EC/8984/C060`,
`T+0xB8AC`, `T+0x78`, `T+0x130`, `T+0x21C`, `T+0xB828/B82C`,
`T+0xB6DE/B6DF/B6E0/B6E8`, `T+0xA508`, MA `+0x508/0x22E4C/0x228EC`-family):

| field | reader found in this batch? | evidence |
|---|---|---|
| `M+0xB8F8/B8FC` | NO | zero hits in all batch-3 dis; stays OPEN (batch-2 §17 item 1 carried) |
| `T+0xB8AC` | YES (1 site) | step 41 `0x1405255DA/5F4` reads `T+0xB8AC` (level ladder for caps — off-ball §1.4 DOC; site CONFIRMED here) |
| `T+0xBAB4` | NO | zero hits; stays OPEN |
| `T+0xC128` | NO | zero hits; stays OPEN |
| `T+0x6C4C` | NO | zero hits (`6C48/6C54` also zero); stays OPEN |
| `T+0x6E7` | NO | zero hits; stays OPEN |
| `T+0x88D8` | NO | zero hits; stays OPEN |
| `T+0xB7EC` | NO | zero hits; stays OPEN |
| `T+0x8984`-family | NO | zero hits; stays OPEN (sole writer stays step 27a-pre) |
| `T+0xC060`-row | NO | zero hits; stays OPEN |
| `T+0x78` | YES (1 site) | step 41 `0x1405255B0 MOV EBX,[RAX+0x78]` (ref-player fallback — teamtick §5.2 DOC; site CONFIRMED here) |
| `T+0x130` | NO | zero hits; stays OPEN |
| `T+0x21C` | YES (callee-internal) | `FUN_140A40820 0x140A40850` + `FUN_140A42A70 0x140A42A90` (stamp/ring base — press-life §1 DOC; sites CONFIRMED here) |
| `T+0xB828/B82C` | YES (self-contained) | step 39 writes + step-39 gate reads only; no external reader found |
| `T+0xB6DE` | YES (tick-inline writer) | `0x1404F7987 MOV [RCX+0xB6DE],AL` (AL = `FUN_140509170` return — step-47 writer, CONFIRMED site); no reader found |
| `T+0xB6DF/B6E0` | YES (self-contained) | step 27d reads-before-writes both (`0x14057AC30/ACD3/AD13` CONFIRMED); no external reader found |
| `T+0xB6E8` | YES (same-batch pair) | step 33 writes (`0x140579AC4`), step 34 reads (`0x140579C65`) — CONFIRMED pair |
| `T+0xA508` | YES (writer only) | step 27c writes (`0x1405819C0/A2C`); reader = off-ball Chance mask (DOC, not re-checked) |

## §10 S1 verdict: NOT in this batch (what was checked)

Checked exhaustively this pass:
1. Immediate scan: `0x22E4C/0x228EC/0x22AD4/0x229B4/0x22B9C/0x22A7C/0x22C64/0x22CBC/0x22D84`
   across ALL batch-3 `dis_*.txt` — **zero hits** for every one (no reader,
   no writer). Unlike batches 1–2, no batch-3 body even reads the NearestList.
2. Base scan: `+0x508` loads (the `MA = *(team-AI+0x508)` base per lane-inputs
   §1.2) — **zero** in any batch body. Step 41's MA-side object arrives via the
   manager-tick args (M/T/ctx/reg per off-ball §1.2), step 44's via
   `brain+0x4200` (defshape §1 DOC); neither is computed from the team-AI tick
   `this` in these bodies.
3. Store sweep: every non-stack `MOV [..]` in the batch was classified
   (§§2–9) — none lands on MA/S1. The only MA-adjacent sites are step-37
   `brain+0xBB8`-object float copies (a different object, identity OPEN).
Conclusion: **no step in this batch computes the S1 base or stores through
it**. The writer stays OPEN exactly where lane-inputs §1.5 + image-rows §3.3
left it. **Unblocks: none of the five A4 lane units** — their S1 stand-ins
stay as-is.

## §11 Registry-base note (batch-2 §16 item raised; CLOSED here)

Batch-2 §16 row: "contract base `0x142b48ca0+16·i` CONFIRMED for i=15/196;
the two bases differ by `0xDB70` = 56208 = 3513 records — batch-3 must
reconcile which base the getters use."

Reconciliation (image bytes this pass):
`0x142b49810 − 0x142b48ca0 = 0xB70 = 2928 = 183 records` (NOT `0xDB70`/3513 —
batch-2's subtraction was wrong; `0xDB70` is 10× too large and matches no
base pair in either doc). So `0x142b49810 = 0x142b48ca0 + 16·183`: the two
bases are the SAME table. `0x142b48ca0+16·i` indexes by absolute registry id
(contract usage: reg 15 = `.../match/pes15Test.json` pathptr `0x142B49ED0`,
reg 196 = `.../team/subConcept.json` pathptr `0x142B4CCD0` — both re-read
this pass); `0x142b49810+16·i` indexes by team-block offset
(constant-team.md §2.1 usage: index `183+i`, i.e. reg 183 = basePosition
pathptr `0x142B4C9D8` — re-read this pass, equals contract-base[183]).
Getters (`FUN_141E5D4C0`) take the absolute id (EDX=15/196/183 observed raw).
Correction row in §14.

## §12 Slices for implementers (≤3 pieces each, dependency order)

Stand-ins carry `// ponytail:` naming the PES site that removes them. Every
slice: red test first on `origin/main`, smallest change, mutation per branch.
No GF code changed by this report (decode-only assignment; slices are tickets).

- **T12a — stoppage pair** (§4). Depends: latch/mi. (i) `B6DF` clear chain
  (in-play + latch + `fps·5+0.5` age); (ii) `B6E0` predicate pair + tail link.
  Tests: `B6DFClearsWhenStoppedOrLatchLost`, `B6E0FollowsItsPredicatePair`.
- **T12b — concept selector skeleton** (§5). Depends: T12a + subConcept table.
  (i) head gates (latch, dead-gate, `0x68`, level `0x21`); (ii) 48-loop +
  `B6E8=1` arms + `B718` stamps; (iii) release sweep + stoppage arm.
  Test: `SelectorArmsB6E8OnlyWhenLevelGatePasses`.
- **T12c — concept executor commits** (§5). Depends: T12b. (i) queued-move
  collect (`B748==c` + `{0x25,0x2C}`); (ii) 5-commit chain per slot.
  Test: `ExecutorCommitsQueuedMovesForRunningConcepts`.
- **T12d — mode-4 seeding** (§7). Depends: T12b (B8B4 modes). (i) latch +
  age + carrier gates; (ii) MA-order scan + rank; (iii) up-to-3 takes with
  0x1E seed. Test: `Mode4SeedsSpaceRunForUpToThree`.
- **T12e — release sweep** (§8). Depends: step-11 counter. `T+0x218==0` gate
  + 11× `A40820`. Test: `ReleaseSweepRunsOnlyWhenWeightCounterZero`.
- **T13 — NON-SIM lock-in** (no code): pin 26b (alias teardown). Tickets:
  `B8F8/B8FC/BAB4/C128/6C4C/6E7/88D8/B7EC/8984/C060-row/B6DF/B6E0/B828/B82C`
  reader grep in any future bodies; `579E10/5A070/5A160/5A1F0/5A3E0/57AA70/
  57A580/57AB30/538xxx-mode-bodies/51D9E0-subcalls` payload bodies.

## §13 Corrections to existing docs

| doc:line | old | new | evidence |
|---|---|---|---|
| team-layer §1 steps 26b, 26c, 27c, 27d, 28, 33, 34, 37–39, 41–44 "OPEN" | all OPEN | SIM = 26c, 27c, 27d, 28, 33, 34, 37, 38, 39, 41, 42, 43, 44; NON-SIM = 26b | raw dis this report |
| team-layer §3 "Explicitly NOT sliced" | batch-2 list (26b, 26c-body, 27b-detail, 27c, 27d-detail, 28, 33, 34, 37, 38, 39, 41–44) | batch-3 sliced T12a–T13; remaining: NONE from the tick — the full 56-step table is now triaged (see §15) | this report §12 |
| tick-steps2 §16 registry row | "the two bases differ by `0xDB70` = 56208 = 3513 records — batch-3 must reconcile" | differ by `0xB70` = 2928 = 183 records; same table (`0x142b49810` = contract-base + 16·183); getters take absolute ids | image bytes §11 |
| constant-team.md §2.1 | "records at `0x142b49810 + 16·i`, registry index `183 + i`" (unqualified) | add: absolute base is `0x142b48ca0 + 16·id`; `0x142b49810` is that base + 16·183 (team-block view). Both usages resolve to the same records (verified: basePosition pathptr `0x142B4C9D8` at both) | image bytes §11 |
| tick-steps (batch-1) §0 scope note | step-35 `0x1425C48E4`=1369.0 / `0x1425C48D0`=105.0 (neighbours corrected) | unchanged (no batch-3 float collides with those sites); batch-3 adds 13 new VAs (§0), incl. `0x142590D98`=0.3 (step-41 floor, newly in scope) | script §0 |

## §14 Open items

1. `M+0xB8F8/B8FC`, `T+0xBAB4/C128/6C4C/6E7/88D8/B7EC/8984/C060-row/B6DF/B6E0/
   B828/B82C/130` readers — no reader in any of the three batches' bodies or
   cited docs; needs bodies outside the tick (dispatch/concept/match paths) or
   live watch.
2. Payload bodies OPEN: `FUN_140579E10` (concept table), `FUN_14057A070/5A160/
   5A1F0/5A3E0` (concept tests), `FUN_14057AA70/5A580/5AB30` (stoppage preds),
   `FUN_1405387A0/39860/38160/39AF0/39260` (modes 1/2/3/5/6),
   `FUN_140537C60/38550` (mode-4 search), `FUN_14051D9E0` subcalls
   (`51CF70/51B8B0/51D100/5F8430/5EC600/51B150/51D510/A41B10`).
3. Step-37 `brain+0xBB8`-object identity (the float-triple copy target).
4. S1 writer hunt unchanged: unchecked `FUN_140A79640` callers outside the
   tick, or live write-watch.
5. Live confirmation (read-only): `T+0xB6E8/B6DF/B6E0/B828`, `T+0x80`,
   `T+0xA508` during a match.
6. Step-49 gate condition (`0x1404F79B0`-region) — still undecoded (batch-1
   §15 item 11 carried; the gate CALL `0x140539ED0` fires when the skip byte
   `R14B==0`, but what sets `R14B` above `0x1404F79AD` besides the skip bit is
   not closed).
7. Step-53 row-base identity — still OPEN (batch-2 §17 item 9 carried).

## §15 Complete team-tick step list with final status

After this batch every step in team-layer.md §1 is triaged. DOC = body decoded
in the cited doc (not re-read here); CONFIRMED = raw disassembly in the named
batch report. Steps 0/3/4/7/10/12/13/18/26a/27a/27b/54/56 are tick-inline or
covered outside the three batches (status from team-layer.md + role-writer +
cited docs, marked accordingly).

| # | callee / action | status | evidence |
|---|---|---|---|
| 0 | tick entry `brain+0x6D48/6D50` | NON-SIM (context) | teamtick §2.2 (DOC) |
| 1 | `FUN_141E5DC40` + `FUN_1423E7FD0` (skip bit) | body NON-SIM, output bit SIM-relevant | batch-1 §12 |
| 2 | `FUN_140A0C3E0` | NON-SIM (no writes) | batch-1 §12 |
| 3 | `FUN_14051C840` (ctx alias init) | SIM (alias init) | role-writer §4 (DOC) |
| 4 | `FUN_140501260` + `T+0x8C=1` | SIM (flag) | team-layer §1 (DOC writer named) |
| 5 | `FUN_1404F6720` (trap arm) | ported/sliced T1 | batch-1 §§2,13 |
| 6 | `FUN_1404F7AD0` (counters) | sliced T1 | batch-1 §§3,13 |
| 7 | `FUN_140A42580` (frame stamp) | SIM (stamp) | teamorder §2.3 (DOC) |
| 8 | `FUN_1405008E0` (event select) | sliced T2a | batch-1 §§4,13 |
| 9 | `FUN_140502190` (timer table) | sliced T2b | batch-1 §§5,13 |
| 10 | `FUN_1404FE9C0` (possession latch) | ported (PossessionLatch) | possession-latch doc (DOC) |
| 11 | `FUN_1404FEDC0` (support weights) | sliced T2c | batch-1 §§6,13 |
| 12 | `FUN_14051E910` + `T+0x134` | ported (UpdateStrategy) | tactics §3 (DOC) |
| 13 | `FUN_1404F8350` (attack-level drift) | ported (mentality half) | tactics §3 (DOC) |
| 14 | `FUN_1404F6680` | NON-SIM (no reader) | batch-1 §12 |
| 15 | `FUN_1404FA840` (counters) | sliced T1 | batch-1 §§3,13 |
| 16 | `FUN_1404FA720` | NON-SIM | batch-1 §12 |
| 17 | `FUN_1404FBCA0` (mode word) | sliced T1 | batch-1 §§3,13 |
| 18 | `T+0xB8A2/B8A3` inline flags | ported (readers) | tactics §3 (DOC) |
| 19 | `FUN_140500200` (eligibility mask) | sliced T3a | batch-1 §§7,13 |
| 20 | `FUN_1404FF8B0` (slot tables) | sliced T3b | batch-1 §§8,13 |
| 21 | `FUN_1404F7BA0` (best slot) | sliced T3c | batch-1 §§9,13 |
| 22 | `FUN_1404FD4B0` (designation op) | sliced T4a/b | batch-1 §§10,13 |
| 23 | `FUN_14053B860` + 5 callees (bitmasks) | sliced T5b | batch-2 §13 |
| 24 | `FUN_1404F87D0` | NON-SIM | batch-1 §12 |
| 25 | `FUN_140500B90` (sheet publish) | sliced T4c | batch-1 §§11,13 |
| 26a | `FUN_14051D9E0` restart branch | ported (role rebuild part) | role-writer §1 + defshape §6–7 (DOC) |
| 26b | `FUN_14051B3E0` (alias teardown) | NON-SIM | batch-3 §2 |
| 26c | `FUN_140A42A70` (snapshot copy) | sliced T6b | batch-3 §3 (body = batch-2 §4) |
| 27a | `FUN_1404FBB70` (bucket census) | NON-SIM (no reader) | batch-2 §2 |
| 27a-pre | `FUN_1404FBB70` cond gate | NON-SIM (same body) | batch-2 §2 |
| 27b | `FUN_14051E350` (designation publisher) | sliced C | team-layer §2 V4 (DOC) |
| 27c | `FUN_140581980` (counter arm) | sliced (counter part) | batch-3 §1 + directive §1.6 + counter-trigger (DOC) |
| 27d | `FUN_14057AC20` (stoppage pair) | sliced T12a | batch-3 §4 |
| 28 | `T+0x80=0xFF` + `T+0x6E6=0` inline | **SIM** (stores; readers OPEN) | batch-3 §1 |
| 29 | `FUN_140537AC0` (queued slot) | sliced T6a | batch-2 §3 |
| 30 | 11× `FUN_140A40240` (ring init) | sliced T6b | batch-2 §4 |
| 31 | inline snapshot + `FUN_140A84E30` | sliced T6b | batch-2 §4 |
| 32 | `FUN_140A84E30` | NON-SIM (frame-local) | batch-2 §4 |
| 33 | `FUN_140579840` (concept selector) | sliced T12b | batch-3 §5 |
| 34 | `FUN_140579BF0` (concept executor) | sliced T12c | batch-3 §5 |
| 35 | `FUN_1404F61E0` (mark table) | sliced T7a | batch-2 §6 |
| 36 | `FUN_14057D240` (sub-state) | sliced T7b | batch-2 §7 |
| 37 | `FUN_1404F9DC0` (shape dispatch) | **SIM** (sites; bodies DOC) | batch-3 §6 + defshape §14.0 |
| 38 | inline supporter reset | **SIM** (writes; meaning DOC) | batch-3 §8 + off-ball §3.1 |
| 39 | `FUN_140501160` (2-CF designator) | **SIM** (writes; meaning DOC) | batch-3 §8 + team-layer §2 V4 |
| 40 | 8 fn (designation/score/release) | sliced T8a/b/c | batch-2 §8 |
| 41 | `FUN_140525530` (run manager) | ported/sliced (manager DOC) | batch-3 §1 + off-ball §1–2 |
| 42 | `FUN_140538050` (mode dispatcher) | sliced T12d (mode-4; other modes OPEN) | batch-3 §7 + teamtick §2.3 |
| 43 | 11× `FUN_140A40820` (release sweep) | sliced T12e | batch-3 §8 |
| 44 | `FUN_140535C60` (defence dispatcher) | ported/sliced (passes DOC) | batch-3 §1 + defshape §1 + press-life |
| 45 | `FUN_1404F88D0` | NON-SIM body (commit OPEN) | batch-2 §9 |
| 46-body | `FUN_1404FC720` (set-piece driver) | sliced T9a | batch-2 §10 |
| 47-meaning | `FUN_140509170` (predicate) | NON-SIM body (meaning ported T9b) | batch-2 §10 |
| 48-bodies | `FC4D0+57BA50+57D240+580630` | sliced T9c/d (writes; stubs NON-SIM) | batch-2 §11 |
| 49 | `FUN_140539ED0` | **SIM** (stores; readers + gate OPEN) | batch-2 §12 |
| 50-stubs | `421F40`×2 + `1863350`×4 | NON-SIM (no-ops) | batch-2 §12 |
| 51 | `FUN_140520930` (takers/subs) | sliced T10a | batch-2 §12 |
| 52 | `FUN_14053C6E0` (latch float) | sliced T10b | batch-2 §12 |
| 53 | `FUN_140A408C0` (blend rows) | sliced T10c | batch-2 §12 |
| 54 | `FUN_14051B3E0` (alias teardown) | NON-SIM (same body as 26b) | batch-2 §12 + batch-3 §2 |
| 55 | `FUN_140A42A70` (snapshot copy) | sliced T6b (same body as 26c/31) | batch-2 §4 + batch-3 §3 |
| 56 | `brain+0x6D50=0` | NON-SIM (context) | teamtick §2.2 (DOC) |
