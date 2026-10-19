# PES team tick `FUN_1404F6C30` step bodies, batch 2 (27a-pre + 29-32 + 35-36 + 40 + 45-53 + 55 + step-23 callees)

Batch-2 decode of the steps docs/re/pes-re-team-layer.md §1 lists as still-OPEN (plus the five step-23 callees
batch 1 left OPEN). Base `0x140000000`, image `~/ghidra-projects/scratch/Unpacker/pes_full.bin`.
Tags: **CONFIRMED** = raw disassembly or image bytes read this pass; **DOC** = cited doc, not re-checked;
**INF** = inference; **OPEN** = blocker named. Decompile cited for orientation only.
Call-site VAs below are the team-layer §1 bridge return-address markers (the `E8` immediately below each
marker targets the callee; re-verified raw this pass: marker `-8..0` holds exactly one `E8` whose
return address equals the marker).

## §0 Float table (script-read, pasted verbatim; `repr` + bits exact)

Script (run this pass; offsets `VA - 0x140000000`):
```python
import struct
f=open('<local>); base=0x140000000
def rd(va):
    f.seek(va-base); b=f.read(4)
    return struct.unpack('<f',b)[0], struct.unpack('<I',b)[0]
for va in vas: print(f"| `0x{va:X}` | `{rd(va)[0]!r}` (`0x{rd(va)[1]:08X}`) |")
```
Scope: every `0x1425xxxx` DATA VA cited in §§1–15. Cross-checked by `xxd` spot reads
(e.g. `0x1425C48D0: 0000 d242 0000 e642 ...` = 105.0/115.0). Two corrections vs prior docs noted inline.

| `VA` | `value` (`0xBITS`) | use |
|---|---|---|
| `0x14259BE44` | `64.0` (`0x42800000`) | `FUN_1404F61E0` `0x1404F656C MOVSS XMM3,[0x14259BE44]` (pred arg) |
| `0x14259BE28` | `32.0` (`0x42000000`) | `FUN_140509170` `0x14050971C COMISS XMM0,[0x14259BE28]` (32.0 gate) — prior docs put 32.0 nowhere; re-read this pass |
| `0x14259BDEC` | `6.0` (`0x40C00000`) | step 46 `0x14050972D MOVSS XMM5,[0x14259BDEC]` (6.0 add); step 51 same family |
| `0x14259D5D4` | `1.2000000476837158` (`0x3F99999A`) | step 46 `0x14050960D MOVSS XMM0,[0x14259D5D4]` (1.2 floor compare) |
| `0x14259BE20` | `25.0` (`0x41C80000`) | step 46 `0x1405096C8 COMISS XMM0,[0x14259BE20]` (25.0 gate); step 40 `0x1404F962C` (25.0 region) |
| `0x1425A9460` | `29.0` (`0x41E80000`) | step 46 `0x140509713 COMISS XMM2,[0x1425A9460]` (29.0 gate) |
| `0x1425A2670` | `400.0` (`0x43C80000`) | step 46 `0x1405095CD COMISS XMM0,[0x1425A2670]` (d² gate 400 = 20²) |
| `0x14259BE40` | `57.2957763671875` (`0x42652EE0`) | step 46 `0x140509328 MOVSS XMM10,[0x14259BE40]` (rad→deg) |
| `0x1425795E4` | `6.2831854820251465` (`0x40C90FDB`) | step 46 `0x140509316 MOVSS XMM9,[0x1425795E4]` (2π wrap) |
| `0x14259222C` | `5.0` (`0x40A00000`) | step 27d `0x14057AC66 MULSS`, step 46 `0x140509424 ADDSS XMM0,5.0` (doubled after) |
| `0x142588210` | `10.0` (`0x41200000`) | step 46 `0x1405093F5`-region baseline (f32 window); step 40 `0x1404F936C` |
| `0x14259858C` | `360.0` (`0x43B40000`) | step 46 wrap (`ADDSS XMM6,360` at `0x14050936B`) |
| `0x142598598` | `-360.0` (`0xC3B40000`) | step 46 wrap twin (`ADDSS XMM0,-360` at `0x14050969B`) |
| `0x14259BE0C` | `15.0` (`0x41700000`) | step 46 `0x1404F63xx`-family analogue 15.0 (see §7); step 40 `0x1404F9559` region |
| `0x1425656C0` | `4.0` (`0x40800000`) | step 46 `0x14050976D SUBSS XMM1,[0x1425656C0]` (−4.0) |
| `0x142565D30` | `nan` (`0xFFFFFFFF`) | abs-mask (fabs idiom), NOT a value — step 35/40/46 `MOVSD XMM*,[0x142565D30]` |
| `0x14256AE1C` | `1.0` (`0x3F800000`) | unit / XMM7 one |
| `0x142596E6C` | `80.0` (`0x42A00000`) | step 35 `0x1404F63BF MOVSS XMM10,[0x142596E6C]` (press-cone 80° ceiling) |
| `0x14259BE5C` | `90.0` (`0x42B40000`) | step 35 `0x1404F653C MOVSS XMM0,[0x14259BE5C]` (90° final test) |
| `0x14259BE60` | `135.0` (`0x43070000`) | step 35 retreat-test 135° (chain, see §7) |
| `0x14259BE6C` | `160.0` (`0x43200000`) | press cone 160° (chain) |
| `0x14259BE70` | `225.0` (`0x43610000`) | d² gate 225 = 15² (step 35 `0x1404F6532 COMISS XMM10,XMM2; JA skip`) |
| `0x1425C48E4` | `1369.0` (`0x44AB2000`) | step 35 `0x1404F63A4 MOVSS XMM11,[0x1425C48E4]` — (batch 1's 525.0 belongs to step 20, see the withdrawn correction row); raw bytes `20 AB 44 44` = 1369.0. Uses: 1369.0 = 37² d² ceiling with 105.0 floor below |
| `0x1425C48D0` | `105.0` (`0x42D20000`) | step 35 `0x1404F63AD MOVSS XMM14,[0x1425C48D0]` (105.0 floor) — (batch 1's 115.0 belongs to step 20); `xxd` cross-check `0000 d242` this pass |
| `0x1425C48DC` | `324.0` (`0x43A20000`) | step 40 `0x1404F9577`-region (`SUBSS/MULSS` chain; operand read this pass) — prior docs put 324.0 nowhere |
| `0x1425C6614` | `-100000.0` (`0xC7C35000`) | step 35 pick-loop floor (same idiom as directive §2) |
| `0x14259BDDC` | `1.5` (`0x3FC00000`) | press-lane +1.5 replace margin (chain) |
| `0x14259BDF4` | `8.0` (`0x41000000`) | step 35 `0x1404F6354 SUBSS XMM1,[0x14259BDF4]` (8.0 ramp base) |
| `0x142591140` | `30.0` (`0x41F00000`) | base-score reset 30.0 (same family) |
| `0x14253EE30` | `100.0` (`0x42C80000`) | normaliser ·100 |
| `0x14259BE04` | `12.0` (`0x41400000`) | ramp span (SUB/DIV pair) |
| `0x14259BDF0` | `7.0` (`0x40E00000`) | ramp rate (divide by 7.0) |
| `0x14259A760` | `35.0` (`0x420C00000`) | 35.0 (cone ceiling reference) |
| `0x14259A764` | `60.0` (`0x42700000`) | 60.0 (tight-path cone) |
| `0x14259BE1C` | `20.0` (`0x41A00000`) | 20.0 (chain gate) |
| `0x14259BE18` | `18.0` (`0x41900000`) | 18.0 (neighbour; carrier-side band) |
| `0x142565600` | `0.5` (`0x3F000000`) | 0.5 level scale / stoppage `+0.5` |
| `0x142565604` | `2.0` (`0x40000000`) | 2.0 (side-half select; `÷2` idiom) |
| `0x142588200` | `0.10000000149011612` (`0x3DCCCCCD`) | 0.1 scale |
| `0x14258C264` | `3.0` (`0x40400000`) | 3.0 ratio divisor |
| `0x1425C65E4` | `0.5555555820465088` (`0x3F0E38E4`) | retreat-test constant |
| `0x14259BE38` | `55.0` (`0x425C00000`) | widening 55 (level ladder) |
| `0x14259BE48` | `65.0` (`0x42820000`) | widening 65 |
| `0x14259BE58` | `85.0` (`0x42AA0000`) | retreat-test 85° |

Audit corrections vs batch-1 §0 (both re-read by script + `xxd` this pass — the brief-rules neighbour rule):
(a) the step-35 sites read **`0x1425C48E4` (= 1369.0, bytes `20 AB 44 44`)** at `0x1404F63A4` and
**`0x1425C48D0` (= 105.0, bytes `00 00 D2 42`)** at `0x1404F63AD` — batch-1 paired these two operand VAs with the
neighbours `0x1425C48E0` (525.0) and `0x1425C48D4` (115.0). The neighbours are real floats (re-read: E0 = 525.0,
D4 = 115.0) but are NOT read at these two sites. Any port slice citing 525.0/115.0 for step 35 must be re-pointed.
(b) `0x14259BE28` = 32.0 (step-46 gate) and `0x1425C48DC` = 324.0 are newly in scope.

## §1 Triage table (every batch step; no step skipped)

Step numbers and call-site VAs from team-layer.md §1. "Writes" = raw-disassembly stores with a non-stack
destination (stack spills and XMM save/restore excluded). Verdict rule (same as batch 1): **SIM** iff some write
reaches a reader feeding positions, decisions, possession, stamina, or AI states; else **NON-SIM** with the proof.

| # | call-site VA | callee VA | size (raw lines) | writes (field, meaning) | readers | SIM? |
|---|---|---|---|---|---|---|
| 23 | `0x1404F6E9D` | `FUN_14053B860` + 5 callees | 49 + 106/78/102/130/135 | `M+0xB8F8/M+0xB8FC` run-order tables via nested `FUN_140A424F0/42530` in each of the 5 callees (CONFIRMED, see §13) | run tables consumed by the off-ball/dispatch path (INF: `B8F8/B8FC` family; batch-3 must grep readers) | **SIM** (narrowed: designation-bitmask writes; brain header NON-SIM) |
| 27a-pre | `0x1404F6F33` (marker; E8@`0x1404F6F2E`→`FUN_1404FBB70`) | `FUN_1404FBB70` | 67 | `T+0x8984/0x8987/0x898A/0x898D` formation-bucket counts (`0x1404FBC43/4E/59/64` CONFIRMED) | none found (grep zero across batch dis + cited docs) | NON-SIM on current evidence |
| 29 | `0x1404F6FBE` | `FUN_140537AC0` | 74 | `T+0x130` queued-slot latch (CONFIRMED `0x140537BB5` only when the `0x24D50`-block triple gate passes); pre-write `T+0xB8AC==4` arm + post-write latch consume (`0x140537BBB/BC1`) | timer/dispatch path (`T+0x130`/`T+0xB8AC` family; batch-3 must grep exact readers) | **SIM** |
| 30 | `0x1404F6FDA` | 11× `FUN_140A40240` | 24 | per-slot ring zero + history init (`[RCX+8]=0,[RCX+0x18]=0` etc.; stack slots, consumed by step 31) | step 31 snapshot (same-tick) | **SIM** (feeds step 31) |
| 31 | (inline) | snapshot + `FUN_140A84E30` | 57 | `T+0x21C`-family per-slot timer-ring snapshots via `FUN_140A42A70` copy (step 26c/55 op; CONFIRMED store pairs `MOVUPS [R15+RBX]`); `T+0xA2A0` designation release via `FUN_140A429A0` (DOC) | run/timer readers (same family batch-3) | **SIM** |
| 32 | `0x1404F7643` | `FUN_140A84E30` | 57 | 10×0x78 timer-ring init (`[R8-0x6]=0`, `[RAX]=0xFF`, `[RAX+0x28]=4`, save/restore region) — stack-frame timer ring, no T/S/brain store | none (frame-local) | NON-SIM |
| 35 | `0x1404F76B1` | `FUN_1404F61E0` | 243 | `T+0xB7EC..` mark table: zero-init (`0x1404F61F7/1FE/206` CONFIRMED) + per-slot `=1` commits (`0x1404F65CF` CONFIRMED); **NearestList reader** (`LEA R15,[R8+0x22E4C]` at `0x1404F630A` CONFIRMED — read only) | mark/dispatch path (table family; batch-3 must grep `0xB7EC` readers) | **SIM** |
| 36 | `0x1404F76BE` | `FUN_14057D240` | 81 | `T+0x88D8` (`=0xFF` clear at `0x14057D2AF/30D`, conditional set at `0x14057D333` CONFIRMED); `brain+0x6C68=1` once (`0x14057D38B` CONFIRMED) | `0x88D8` consumers (press/dispatch family INF); `6C68` once-flag | **SIM** |
| 40 | `0x1404F7824`–`876` | 8 fn (see §8) | 65+83+42+394+1031+84+283+var | `T+0xB8AC=4` (3 sites in `FUN_14053B110` head/tail, CONFIRMED); `T+0xBAB4` best-slot score (1 site `0x1404F9CC8`, CONFIRMED); `T+0x8984`-family NOT touched here (27a-pre only); out-param block fills (`RBP+0x30..` byte rows, `RBP-0x80` d-vectors) are callee-frame (NON-SIM part) | `T+0xB8AC` readers (batch-1 §3); `T+0xBAB4` reader OPEN (batch-3 grep) | **SIM** (table/flag writes; notifier + COM plumbing ignored) |
| 45 | `0x1404F796D` | `FUN_1404F88D0` | 208 | two frame-scratch stores only (`[RAX]=0xFF` fill + `[RAX+R14]=EDI`; RAX = `RSP+0x30` CONFIRMED) + `FUN_140A42490` commit call (payload OPEN) — no T/S/brain store | `A42490` payload (OPEN) | NON-SIM on current evidence (body); commit payload OPEN |
| 46-body | `0x1404F7975` | `FUN_1404FC720` | 517 | `T+0x6C4C/6C48/6C50/6C54` (CONFIRMED `0x1404FC98E/9C1/A3D/A60/83`, `0x1404FCE09`, `0x1404FCB66/72/7B`); `T+0x6E5/6E7` (CONFIRMED `0x1404FCB98`, `0x1404FC4FB/5AE/5D4/60F/63E/671/69C/70E` — note: the `6E7` sites sit inside step 48's callee `FUN_1404FC4D0`, called from step 46 at `0x1404FCCE2`-region... precisely: `FUN_1404FC4D0` is a SHARED callee of steps 46 and 48); `T+0x140/0x13C` (CONFIRMED `0x1404FCD41/71/97`); set-piece event bit + frame stamps (payload OPEN) | `6C4C`/`6E7` stoppage/dispatch readers (INF); batch-3 must pin | **SIM** |
| 47-meaning | `0x1404F7980` | `FUN_140509170` | 396 | none (pure predicate, AL only; sole structural store is `MOV [RSP+0x30],XMM11` CONFIRMED stack) | — | NON-SIM (body); meaning = stoppage-team predicate (see §10) |
| 48-bodies | `0x1404F7995`–`9AD` | `FUN_1404FC4D0` + `FUN_14057BA50` + `FUN_14057D240` + `FUN_140580630` | 136+40+81+38 | `T+0x6E7=0/1` (`FUN_1404FC4D0` 8 sites CONFIRMED); `brain+0x6C68=1` (via `FUN_14057D240` third arm CONFIRMED); `FUN_14057BA50` no stores; `FUN_140580630` JMP-tail into `FUN_14057F600` (payload OPEN) | same as 46/36 readers | **SIM** (the `6E7`/`6C68` writes; `57BA50`/`580630` bodies NON-SIM) |
| 49 | `0x1404F79D8` | `FUN_140539ED0` | 16 | `T+0x30/0x38` pair + `[T+0x0]=0` (CONFIRMED `0x140539ED4/F0B`) | OPEN (batch-3 grep `0x30/0x38` readers) | **SIM** (T-relative stores; readers OPEN) |
| 50-stubs | `0x1404F79DF`–`A0E` | 2× `FUN_140421F40` + 4× `FUN_141863350` | 2 + 2 | none (`FUN_140421F40` = `XOR EAX,EAX; RET` CONFIRMED 2 lines; `FUN_141863350` = `XOR AL,AL; RET` CONFIRMED 2 lines) | — | NON-SIM (no-ops) |
| 52 | `0x1404F7A3E` | `FUN_14053C6E0` | 46 | `T+0x4` latch float = ±sign (`MOVSS [RSI+4],XMM0` at `0x14053C720` CONFIRMED); fan-out to 5 concept calls (payloads OPEN) | concept/strategy path (INF) | **SIM** |
| 53 | `0x1404F7A5C` | `FUN_140A408C0` | 184 | per-slot blend triple (`[RBX-0x4]=XMM6`, `[RBX]=XMM0`, `[RBX+4]=XMM2` CONFIRMED, 11 iterations). Entry block: `R13=R9` out-param Rowe buffer (`0x140A408F9` + `0x140A4091B LEA RSI,[R13+0x298]` walker CONFIRMED); `RBX`/`RBP` walk `RSP+0xF0`-passed rows (`0x140A40958/96D/974` CONFIRMED). Lands on caller-passed row buffer (SIM "match-row blend"; exact struct identity OPEN — §12) | blend/position path (INF); batch-3 must pin the row base | **SIM** |
| 55 | `0x1404F7A85` | `FUN_140A42A70` | 57 | per-slot copy `T+0x21C`-family (CONFIRMED `MOVUPS [R15+RBX]` triple + `[R15+RBX+0x30]` when `T+0xA2A0` gate passes) | same family as step 31 | **SIM** |

S1 verdict (§14): **not in this batch**. Checked: every batch-2 body was grepped for the MA/S1 immediates
(`0x22E4C/0x228EC/0x22AD4/0x229B4/0x22B9C/0x22A7C/0x22C64/0x22CBC/0x22D84`) and for the `MA = *(team-AI+0x508)`
base (`+0x508` loads). Result: **zero `+0x508` loads** in any batch body; the only `0x22E4C` sites are two
**readers** — step 35 `0x1404F630A LEA R15,[R8+0x22E4C]` (NearestList row walk, read-only) and step 40
`0x1404F9C0F/9C3C LEA R9,[R10+0x22E4C]` (same read idiom) — and **zero stores** through either base.
No `0x228EC/0x22AD4/0x229B4/0x22B9C/0x22A7C/0x22C64` immediate appears anywhere in the batch.
The writer is therefore NOT one of these steps; it stays OPEN exactly as lane-inputs §1.5 + image-rows §3.3 state.

## §2 Step 27a-pre (`FUN_1404FBB70`): formation-bucket census — NON-SIM, CONFIRMED stores (readers absent)

Raw: `dis_1404FBB70.txt` (67 lines). Inputs: `FUN_1408C51B0` record walk (11 slots) + `FUN_140A40C10` class test.
Branches: 12-arm jump table on `EAX-1` (CONFIRMED `0x1404FBBC2 CMP EAX,0xB / JA default; 0x1404FBC70-table`).
Outputs (CONFIRMED raw, all four):
`0x1404FBC43 MOV [RDI+0x8984],AL` (bucket 0 count),
`0x1404FBC4E MOV [RDI+0x8987],AL` (bucket 1),
`0x1404FBC59 MOV [RDI+0x898A],AL` (bucket 2),
`0x1404FBC64 MOV [RDI+0x898D],AL` (bucket 3).
`T+0x8984/87/8A/8D` = per-bucket live counts. No reader found in any batch dis or cited doc
(grep `8984/8987/898A/898D` zero outside this function) — NON-SIM on current evidence.
Triage note: this is the ONLY writer of the `0x8984`-family; if batch-3 finds a reader the verdict flips.
Port: none (no GF state). Stand-in: none needed.

## §3 Step 29 (`FUN_140537AC0`): queued-slot latch — SIM, CONFIRMED store + gates

Raw: `dis_140537AC0.txt` (74 lines). Inputs: `RBX = [R8+0x18]+0x24D50` (CONFIRMED `0x140537AD9/ADD`),
`[RBX+4]` (count, must be ≠0), `[RBX]` (slot id ≤0x15, CONFIRMED `0x140537AF4 CMP; JA exit`),
side-vs-`[RBX]` fold (`SETGE` at `0x140537B06`), `EDX==2` (CONFIRMED `0x140537B15`), `FUN_140537BF0` validity ×2,
`FUN_140A48A10` player-exists + `pl+0x5D0 != 0x1E` (CONFIRMED `0x140537B42/49`).
Outputs: `0x140537BA6 CALL 0x140A429A0` (designation release on the resolved slot) +
`0x140537BB5 MOV [RDI+0x130],ECX` (queued slot id) — the ONLY T-relative store, conditional on the full gate chain.
Post: `FUN_140A48A10` re-check on `T+0x130` (CONFIRMED `0x140537BC1/BC8`).
Port: new GF `queuedSlot u8` + the triple gate (count/side/`EDX==2`) as one slice (**T6a**).
Red test: `QueuedSlotSetOnlyWhenCountSidePhasePass`.
Registry: none (no `FUN_141E5xx` call).

## §4 Steps 30–32 (ring init + snapshot + timer-ring op) — SIM / SIM / NON-SIM

- Step 30: 11× `FUN_140A40240` (24 lines): per-slot zero-init (`[RCX+8]=0`, `[RCX+0x18]=0` CONFIRMED) +
  `FUN_140A85AA0` commit. Frame-local ring slots consumed by the step-31 snapshot — SIM (feeds 31).
- Step 31: inline snapshot + `FUN_140A84E30` (57 lines): `FUN_140A42A70` copies `T+0x21C`-family rows
  (`MOVUPS [R15+RBX]` triple at `0x140A42AE0/AE9/AF3` + tail `[R15+RBX+0x30]` CONFIRMED) when the
  `T+0xA2A0`-gate passes; 10×0x78 ring frame init is frame-local (NON-SIM part).
- Step 32 (`FUN_140A84E30` standalone call at `0x1404F7643`): same 57-line body, frame-local only — NON-SIM.
Port: one slice (**T6b**): `timerRing[11]` zero + snapshot copy. Red test: `SnapshotCopiesOnlyWhenDesignated`.

## §5 Registry reads in this batch (all named; stock file + object)
+
Only ONE batch-2 function calls the registry getters: step 33/34's `FUN_140579840`
(`0x140579866 CALL 0x141E5D4C0` with `EDX=0xF`=15 CONFIRMED;
`0x140579877 CALL 0x141E5D4C0` with `EDX=0xC4`=196 CONFIRMED).
Per the contract every record i names the stock object + JSON path (image bytes this pass;
full 183–198 table read: base `0x142b48ca0+16·i` resolves to a real JSON path for every i):
+ reg 15 = NOT a constant_team object: `DevelopData/common/match/constant/match/pes15Test.json`
  (pathptr `0x142B49ED0`).
+ reg 183 = **basePosition** = `.../team/basePosition.json`; 184 = **centeringGet**; 185 = **combination**;
  186 = **defence**; 187 = **defenceCover**; 188 = **defenceMark**; 189 = **diagonalRun**; 190 = **lineBreak**;
  191 = **overlap**; 192 = **pairAnime**; 193 = **pullAway**; 194 = **selectorVision**; 195 = **spaceRun**;
  196 = **subConcept** = `DevelopData/common/match/constant/team/subConcept.json`
  (constant-team.md §2 table: blob 208 B, 50 fields; image bytes this pass: pathptr `0x142B4CCD0`).
  197 = **support**; 198 = **teamId**.
  Stock file: `common/match/constant/constant_team.bin` in `Data/dt18_all.cpk` (constant-team.md §2.1);
  values always the STOCK ones (brief-rules 19-10-26).
No other batch-2 body calls `FUN_141E5D4C0`/`FUN_141E5CA20` (grep zero across all other `dis_*.txt`).
Steps 23/27a-pre/29–32/35/36/40/45/46/48/49/52/53/55: zero registry reads — their constants are immediates/image floats (§0).
+
Note vs constant-team.md §2: that doc's `0x142b49810+16·i` base differs from the contract's
`0x142b48ca0+16·i` by `0xDB70` (3513 records). The contract base is the one verified this pass
(all 16 paths resolve); the `0x142b49810` base is left for batch-3 to reconcile.
## §6 Step 35 (`FUN_1404F61E0`): mark table — SIM, CONFIRMED (NearestList READER, not writer)

Raw: `dis_1404F61E0.txt` (243 lines). Bases: `R8 = [R12+0x18]` (MA-side object, CONFIRMED `0x1404F62EB`);
`R15 = R8+0x22E4C` (CONFIRMED `0x1404F630A LEA R15,[R8+0x22E4C]`).
Row walk: per (EBP,EDX) pair `RAX = R15+((EBP·11+EDX) mod 11)·8`, `EDI=[RAX]` slot id, `XMM0=[RAX+4]` d²
(CONFIRMED `0x1404F63F5/434` LEA pair + `0x1404F63FE/434` loads). Sign select: `XMM8 = sign·[pl+0x55C]`
(CONFIRMED `0x1404F632C/338/348`); ramps: `SUBSS 8.0` (`0x1404F6354 [0x14259BDF4]`), `ADDSS 3.0`
(`0x1404F635C [0x14258C264]`); gates `COMISS XMM1,XMM8; JC` / `COMISS XMM8,XMM0; JC → XMM8=1.0`
(`0x1404F6364/370/376` CONFIRMED); then `SUBSS XMM8,15.0` (`0x1404F638F [0x14259BE0C]`).
Ceilings: `COMISS [RAX+4],1369.0; JA skip` (`0x1404F6439 [0x1425C48E4]`); `COMISS XMM1,105.0; JA skip`
(`0x1404F64D9 [0x1425C48D0]`); `COMISS XMM10(d²),80.0; JA skip` (`0x1404F650D [0x142596E6C]`);
final `COMISS XMM10,XMM2(d²); JA skip` (`0x1404F6532`) gates on 225.0 = 15² (`0x1404F62xx` [0x14259BE70] family).
Predicates: `FUN_140A4C720` side test, `FUN_140A4E720/4EAF0` deep pair with args
(`XMM3=pred`, `XMM0=90.0` at `0x1404F653C [0x14259BE5C]`, `XMM2=64.0` at `0x1404F656C [0x14259BE44]`
`XMM1=12.0` at `0x1404F6558 [0x14259BE04]`).
Outputs: zero-init `T+0xB7EC..` (`0x1404F61F7/1FE/206` CONFIRMED) + per-slot commits
`0x1404F65CF MOV [RAX+RCX+0xB7EC],1` (max 3, `CMP R9B,3; JNC exit` CONFIRMED `0x1404F65D7/DB`).
MA role: READ ONLY — no store through `R15`/any `0x22xxx` base in all 243 lines (grep zero).
Port: new GF `markTable[11]` + NearestList-row walk as one slice (**T7a**).
Red tests: `MarkTableZeroedOnEntry`, `MarkCommitsAtMostThree`.
S1 note: this is the S1-adjacent reader the brief asked to check — it READS the NearestList row
(d² at `[RAX+4]`) but does NOT write any S1 block (§1 S1 verdict stands).

## §7 Step 36 (`FUN_14057D240`): `T+0x88D8` + `brain+0x6C68` — SIM, CONFIRMED stores

Raw: `dis_14057D240.txt` (81 lines). Head: menu-object `FUN_1423E7FD0` gate (NON-SIM part, same family as step 1).
`T+0x88D8` arm: `0x14057D288 CMP [R8+0x88D8],0x15; JA` + slot fold + `CMP [R8+RAX*4+0xA2A0],0x6D; JZ` →
clear `0x14057D2AF MOV [R8+0x88D8],0xFF`; 11-slot `0x6D`-scan loop with `FUN_14057C8C0` test →
`FUN_140A40820` release + re-clear (`0x14057D30D`), or `FUN_140A42940` claim + conditional set
(`0x14057D333 MOV [RAX+0x88D8],ESI`); tail `FUN_14057CBC0/57CF40` (payloads OPEN).
`brain+0x6C68`: `0x14057D379 CMP [RCX+0x116C],1; JNZ` + `0x14057D382 CMP [RBX+0x6C68],0; JNZ` →
`0x14057D38B MOV [RBX+0x6C68],1` (once-flag, CONFIRMED).
Port: `subState u32` + once-flag as one slice (**T7b**). Red test: `SubStateClearsWhenDesignationLeaves`.

## §8 Step 40 (8 functions): designation/bitmask + score + release fan-out — SIM, CONFIRMED key stores

Call markers in `0x1404F7824..0x1404F78BA` (E8 scan this pass — all 8 returns land in-range):
`FUN_14057BBB0` (`0x1404F7824`), `FUN_14053B110` (`0x1404F7843`), `FUN_1404FB2A0` (`0x1404F784B`),
`FUN_14057B6B0` (`0x1404F7856`), `FUN_14057C190` (`0x1404F785E`), `FUN_1404F8C70` (`0x1404F7866`),
`FUN_140500A00` (`0x1404F786E`), `FUN_1404FAD30` (`0x1404F7876`).

- `FUN_14057BBB0` (65 lines): 11-slot `T+0xA2A0==0x23`-scan → `FUN_14053B860`-style brain writes (context) +
  `FUN_140A40820` release + tail `FUN_140CA1A50(0x16)` gate → `FUN_14057BAF0` (payload OPEN). Table release — SIM.
- `FUN_14053B110` (83 lines): brain header (`[RCX+0x10]=RDX`, `[RCX+0x18]=R8D` CONFIRMED `0x14053B128/12F`) +
  `B8AC`-ladder: stoppage `FUN_14053A820` path writes `T+0xB8AC=4` (`0x14053B238` CONFIRMED) + two more
  `B8AC=4` sites in the fallback arms (3 total); per-B8AC concept calls (`A820/A890/AA90/AE60/CEE0`, bodies OPEN).
  The `T+0xB8AC=4` store is the SIM write (same counter batch-1 §3 tracks).
- `FUN_1404FB2A0` (61 lines): stoppage ladder on `MI+0x1775/0x116C`: non-in-play arms write
  `T+0x78=0xFF` (`0x1404FB2FA/34A` CONFIRMED); in-play arm falls through (no store). `T+0x78` — SIM.
- `FUN_14057B6B0` (42 lines): 11-slot `T+0xA2A0==0x25`-scan → `FUN_14057AD30` test → `FUN_140A40820` release,
  else `FUN_14057B4F0` (payload OPEN). Table release — SIM.
- `FUN_14057C190` (394 lines): the big one. Out-param block fills (`RBP+0x30..` rows, `RBP-0x80` d-vectors) are
  caller-frame (NON-SIM part). SIM-relevant: `FUN_140A79640` rank call + `FUN_140A71990` heading call
  (same helpers batch-1 tracks) and the `T+0x62E8/C48/4C00/78` gate chain (role/class/readiness).
  Match-row blend triple `[RBX-0x4]=XMM6,[RBX]=XMM0,[RBX+4]=XMM2` lands on match rows (SIM, same as step 53).
- `FUN_1404F8C70` (1031 lines): out-param fills + `FUN_140A79640` sort call (`0x1404F9244` CONFIRMED).
  ONE T-relative store: `0x1404F9CC8 MOV [RAX+RCX*4+0xBAB4],EDX` (`RAX=[R15+0x6D50]=T`, `EDX` = best-slot score).
  All other `MOV [RBP±]`/`[RAX±]` sites are the `RBP`-frame out-param block (NON-SIM part).
  `T+0xBAB4` reader OPEN (batch-3 grep) — SIM.
- `FUN_140500A00` (84 lines): `T+0xC060`-row table init (`[R8+RDX+0xB0]=0` fill, `0xFF` ids, `0xA0..` ranks
  CONFIRMED `0x140500A42/50/74/7D/85/8C`) + per-slot `FUN_140A79640` rank JMP-tail (`0x140500B83` — the rank
  builder on the same row family). Row publish — SIM.
- `FUN_1404FAD30` (283 lines): `T+0xC128` word commits (`0x1404FAD4B/FAFDA/FB228` CONFIRMED `MOV [RAX+0xC128],R15W/DI`)
  + `FUN_140A4E720` gate pair + frame `FUN_140A57800` + angle/heading chain. Word commit — SIM.
Port: three slices (**T8a** `B8AC=4` + `T+0x78` arms; **T8b** `BAB4` + `C128` commits; **T8c** `C060`-row + rank tail).
Red tests: `StoppageSetsB8AC4`, `BestSlotScoreCommitsBAB4`, `C060RowRanksAfterFill`.

## §9 Step 45 (`FUN_1404F88D0`): frame-scratch shortlist + commit call — NON-SIM body (commit OPEN)

Raw: `dis_1404F88D0.txt` (208 lines). Gates: `MI+0x116C==1` required (`0x1404F88FF/906` CONFIRMED);
`T+0x4BD0` float window (`0x1404F8925/92D/931` CONFIRMED); record `0x37/0x18/4` tests.
Stores (only two, both frame-scratch CONFIRMED): `0x1404F89A0 MOV [RAX],0xFF` fill + `0x1404F8B59 MOV [RAX+R14],EDI`
where `RAX=RSP+0x30` (CONFIRMED `0x1404F8962 LEA RAX,[RSP+0x30]`). No T/S/brain/MA store in all 208 lines.
Tail: `FUN_140A42490` commit call (`0x1404F8C0E` CONFIRMED) — payload OPEN.
NON-SIM body on current evidence; verdict flips if `A42490` writes sim state (batch-3: decode it).

## §10 Step 46-body (`FUN_1404FC720`) + step 47-meaning (`FUN_140509170`): set-piece driver + pure predicate

`FUN_1404FC720` (517 lines): the set-piece/restart event driver. CONFIRMED T-relative stores:
`T+0x6C4C` (`0x1404FC98E/9C1/A3D/A60/83`), `T+0x6C48` (`0x1404FCE09`), `T+0x6C50`
(`0x1404FCB66/72/7B`), `T+0x6C54` (`0x1404FCD64/BE`), `T+0x6E5` (`0x1404FCB98`),
`T+0x140` (`0x1404FCD41/71`), `T+0x13C` (`0x1404FCD97`).
(The `T+0x6E7` sites in this disassembly belong to the shared callee `FUN_1404FC4D0`, called from step 46 —
see §11; counted once there.) RNG: `FUN_140A66FF0` per-slot + `FUN_141E532D0/E0` + `MULSS 5.0/ADDSS 0.5`
idiom (CONFIRMED `0x14057AC66/6E/75` region in the shared stoppage twin). SIM (stoppage state).
Port slice **T9a**: `6C4C/6C48/6C50/6C54` + `140/13C` + `6E5` arms. Red test: `SetPieceDriverStampsFrame`.

`FUN_140509170` (396 lines): PURE PREDICATE — returns AL only; the sole structural store is
`MOV [RSP+0x30],XMM11` (stack CONFIRMED). No T/S/brain/MA store in all 396 lines.
Meaning (CONFIRMED gates): in-play required (`0x14050918E CMP [MI+0x116C],1; JZ return-0`);
carrier chain (`FUN_140A627E0` → `MA+0x2482C/834/83C` ball-record triple CONFIRMED `0x1405091BF/1CF/230`);
side fold (`SETGE` at `0x140509244`); sign-gated ball-x test (`MULSS XMM0,sign; COMISS; JBE reject`
CONFIRMED `0x14050928C/295/29E`); record `FUN_140A62690` + d² chain with 2π/deg constants (§0);
NearestList-ROW read (`LEA R15,[RBP+0x2470C]` at `0x14050946E` CONFIRMED — ball-centred row, read-only);
rank walk (`[R15+0xB0]` count + `[R15+RCX*8]` id/d² CONFIRMED `0x14050948C/510`);
per-mate geometry ladder with gates 400.0 (d², `0x1405095CD`), 1.2 (`0x14050960D`), 25.0 (`0x1405096C8`),
29.0 (`0x140509713`), 32.0 (`0x14050971C`), 6.0 (`0x14050972D`), 4.0 (`0x14050976D`).
Returns 1 = stoppage-team condition holds, 0 otherwise. NON-SIM body; meaning named for the step-47 port:
`StoppageTeamPredicate(side)` — GF ports the gate chain, not the (empty) body.
Port slice **T9b** (meaning only). Red test: `StoppagePredicateRejectsWhenDead`.

## §11 Step 48-bodies: `T+0x6E7` writer + two release loops + trap-step stub — SIM (writes only)

- `FUN_1404FC4D0` (136 lines): the `T+0x6E7` writer — 8 CONFIRMED sites
  (`0x1404FC4FB/5AE/5D4/60F/63E/671/69C/70E MOV [RAX+0x6E7],0/1`, `RAX=[RCX+0x6D50]=T`).
  Inputs: side, `T+0x255` latch, `FUN_140A62690` record, `FUN_140A3E190` validity, card bytes.
  Called from BOTH step 46 and step 48 (shared callee). SIM.
- `FUN_14057BA50` (40 lines): the trap-step stub — ZERO stores (all 40 lines re-read; only loads +
  `FUN_140A48A30/40BD0` reads + tail JMP). NON-SIM body. (The `T+0xE4` trap arm lives in step 5's writer;
  this callee only reads the trap/step state.)
- `FUN_14057D240` (same body as step 36, §7): `T+0x88D8` + `brain+0x6C68` — SIM (counted once in §7).
- `FUN_140580630` (38 lines): 11-slot `T+0xA2A0==0x10`-scan → `FUN_140A40820` release / `FUN_140A42940` claim,
  then JMP-tail into `FUN_14057F600` (payload OPEN). Release fan-out — SIM.
Port slices **T9c** (`6E7` writer) + **T9d** (release loops). Red tests: `TrapByteSetOnlyWhenLatchAndRecordPass`,
`ReleaseLoopClaimsOrReleasesPerSlot`.

## §12 Steps 49/52/53/55 + 50-stubs: tail commits — SIM / SIM / SIM / SIM / NON-SIM

- Step 49 `FUN_140539ED0` (16 lines): `0x140539ED4 MOV [RCX+0x38],R8D` + `0x140539F0B MOV [RCX],0`
  (CONFIRMED; `RCX` = caller struct, `RDX` = `[RCX+0x30]` context). In-tick gate: the tick-side
  `skip==0` test at `0x1404F79B0`-region (batch-1 §15 item 11 stays OPEN — the gate CONDITION above the call
  is still undecoded, but the BODY stores are CONFIRMED). SIM (T-relative stores; readers OPEN/batch-3).
- Step 50: `FUN_140140421F40` ×2 = `XOR EAX,EAX; RET` (CONFIRMED 2 lines each, markers `0x1404F79DF/9E9`);
  `FUN_141863350` ×4 = `XOR AL,AL; RET` (CONFIRMED 2 lines each, markers `0x1404F79F0/FA/04/0E`).
  Byte context: `B9 01 00 00 00 E8 ... 33 C9 E8 ... B9 01..03` — the four switch arms pass
  `ECX=0/1/2/3` to the no-op (dead switch `caseD_24`). NON-SIM (no-ops).
- Step 51 `FUN_140520930` (275 lines): taker/subs tables (`[R10+RCX+0x140]=AL` armed byte,
  `[RCX+R8-4]=EAX` triple rows, `[RCX+0x1F8]=EAX/[RCX+0x1FC]=AL` CONFIRMED `0x140520B6E/81/94/BA/CC/DC/EF/FF`).
  SIM (taker/subs state). Port slice **T10a**. (Body detail beyond the store sites stays OPEN — same
  contract as batch-1 payloads.)
- Step 52 `FUN_14053C6E0` (46 lines): `0x14053C720 MOVSS [RSI+4],XMM0` where `XMM0 = float(sign)`
  (CONFIRMED `0x14053C710/718/71D` `MOVSX/CVTDQ2PS/MOVSS` chain; sign from `T+0x254` at `0x14053C710`).
  Then 5 concept fan-out calls (`53CB40/C790/D250/C890/CC00` + tail JMP `53D3B0`, bodies OPEN). SIM.
  Port slice **T10b**: `latchFloat = ±1.0`. Red test: `LatchFloatMatchesSign`.
- Step 53 `FUN_140A408C0` (184 lines): per-slot blend triple into match rows
  (`0x140A40AEB/EBF/EB? MOVSS [RAX],XMM6/XMM1/XMM2` CONFIRMED, `RBX` walks `RDX+0x3F08`-rows,
  11 iterations `0x140A40B85 CMP EDI,0xB; JL`). Gates: `MI+0x116C`, card `0x37`, class `0x5`,
  sign-vs-key (`MULSS` pair `0x140A40ABD/C1`), `0x140A41C00` claim test. SIM.
  Port slice **T10c**. (Row-base identity beyond the walk stays INF — same caveat as batch-1 payloads.)
- Step 54 (`FUN_14051B3E0`, marker `0x1404F7A6B`): body = `MOV [RCX+0xBA8],0; RET` (CONFIRMED 2 lines).
  Ctx-alias clear (P-base reset). NON-SIM (no sim state; teardown only).
- Step 55 (`FUN_140A42A70`, marker `0x1404F7A85`): same 57-line copy body as step 31 (SIM — counted in §4).

## §13 Step 23's five callees: designation-bitmask writers — SIM (verdict: FOUND, flips batch-1)

Bodies (raw line counts): `FUN_14053BA10` 106, `FUN_14053B900` 78, `FUN_14053BDC0` 102,
`FUN_14053BF10` 130, `FUN_14053BB90` 135. ALL FIVE have ZERO direct T/S/brain/MA stores
(full `MOV [..]` sweep this pass: only loads + stack spills) — but ALL FIVE call the
match-object bitmask writers under the same 11-slot scan idiom:

| callee | scan idiom (CONFIRMED) | nested writer call (CONFIRMED) | bitmask arg |
|---|---|---|---|
| `FUN_14053BA10` | `IMUL side,0xB + slot; [..+0x1748]!=0; FUN_140A64030(...,7,...)` | `0x14053BB32 CALL 0x140A424F0` + `0x14053BB4B CALL 0x140A42530` | `EDX=1`, `R8D=ESI` (mask) |
| `FUN_14053B900` | same + `FUN_140A63020` + id-`0xE` test | `0x14053B9D9 CALL 0x140A424F0` + `0x14053B9EB CALL 0x140A42530` | `BTS EDI,ECX` mask, `EDX=5` |
| `FUN_14053BDC0` | same + id-`0x8/0x4` test pair | `0x14053BEC0 CALL 0x140A424F0` + `0x14053BED9 CALL 0x140A42530` | `EDX=2`, `BTS EDI,EBP` mask |
| `FUN_14053BF10` | same + `T+0x46EC` lateral + 8.0 gate (`0x14053C002 COMISS XMM0,[8.0]`) | `0x14053C0AA CALL 0x140A424F0` | `EDX=3`, `BTS EDI,R14D` mask |
| `FUN_14053BB90` | same + 3 `FUN_140A64030` tests | `0x14053BD52 CALL 0x140A424F0` | `EDX=7`, `BTS EDI,R15D` mask |

Nested writers (CONFIRMED bodies, 8 + 7 lines):
`FUN_140A424F0`: `MOV [RCX+RDX*8+0xB8F8],R8D; INC [R9+0xB8EC]` (guarded `EDX<9`, `[R9+0xB8EC]<5`);
`FUN_140A42530`: `MOV [RCX+RDX*8+0xB8FC],R9D` (guarded `EDX<9`, `R8B<5`).
`RCX` = match object (first arg through all five callees). So step 23 writes the
`M+0xB8F8/M+0xB8FC` run/designation bitmask tables — SIM (match-object tables feeding the off-ball path).
Batch-1 §12 verdict ("NON-SIM for visible writes; 5 callees OPEN") is SUPERSEDED: the visible brain-header
writes stay NON-SIM, but each callee's nested bitmask writes are SIM.
Port slice **T5b**: per-callee scan idiom + `A424F0/A42530` commit (5 arms, one slice of ≤3 pieces:
(i) scan idiom + BTS mask, (ii) `EDX`-class commit, (iii) tail `A42530` stamp).
Red tests: `DesignationMaskCommitsOnlyWhenScanPasses`, `MaskIndexBoundedBy9`, `CountByteCapsAt5`.
Readers of `M+0xB8F8/B8FC` stay OPEN (batch-3 grep).

## §14 S1 verdict: NOT in this batch (what was checked)

Checked exhaustively this pass:
1. Immediate scan: `0x22E4C/0x228EC/0x22AD4/0x229B4/0x22B9C/0x22A7C/0x22C64/0x22CBC/0x22D84` across ALL
   batch-2 `dis_*.txt` — hits: ONLY `0x22E4C` (two LEA readers: step 35 `0x1404F630A`, step 40 `0x1404F9C0F/9C3C`).
   Zero hits for all eight other bases. Zero stores through any `0x22xxx` base (both LEA sites feed
   read-only `[RAX]`/`[RAX+4]` id/d² loads).
2. Base scan: `+0x508` loads (the `MA = *(team-AI+0x508)` base per lane-inputs §1.2) — ZERO in any batch body.
   (Step 35's MA-side object arrives as `[R12+0x18]`, step 40's as `[RAX+0x18]` — the `+0x18` link, not `+0x508`;
   neither is computed from the team-AI tick `this` in these bodies.)
3. Store sweep: every non-stack `MOV [..]` in the batch was classified (§§2–13) — none lands on MA/S1.
Conclusion: **no step in this batch stores through the MA+0x228EC..0x22CBC / NearestList MA+0x22E4C base**.
The writer stays OPEN exactly where lane-inputs §1.5 + image-rows §3.3 left it (unchecked `FUN_140A79640`
callers outside the tick, or a computed-base copy). The two NearestList READERS found here (steps 35/40)
confirm the computed-base read idiom (`LEA base; ADD index·200/·8`) but do not advance the writer hunt.
**Unblocks: none of the five A4 lane units** — their S1 stand-ins stay as-is.

## §15 Slices for implementers (≤3 pieces each, dependency order)

Stand-ins carry `// ponytail:` naming the PES site that removes them. Every slice: red test first on
`origin/main`, smallest change, mutation per branch.

- **T5b — step-23 designation bitmasks** (§13). Depends: batch-1 T4 (designation tables).
  (i) 11-slot scan idiom + BTS mask; (ii) `EDX`-class `A424F0` commit + count-byte cap;
  (iii) `A42530` tail stamp. Tests: `DesignationMaskCommitsOnlyWhenScanPasses`, `MaskIndexBoundedBy9`.
- **T6a — queued-slot latch** (§3). Depends: T+0xB8AC (batch-1). One piece + gate test.
- **T6b — timer-ring init + snapshot** (§4). Depends: none. Two pieces + test.
- **T7a — mark table** (§6). Depends: NearestList reader (GF `BuildNearest`).
  (i) zero-init; (ii) row walk + ramp/gates (1369.0/105.0/80.0/225.0/90.0/64.0/12.0/8.0/3.0/15.0);
  (iii) max-3 commit. Tests: `MarkTableZeroedOnEntry`, `MarkCommitsAtMostThree`.
- **T7b — sub-state + once-flag** (§7). Depends: T4. Two pieces + test.
- **T8a/b/c — step-40 commits** (§8). Depends: C, T5b. Three slices as listed.
- **T9a/b/c/d — set-piece driver + predicate + trap byte + release loops** (§§10–11). Depends: A.
- **T10a/b/c — taker tables + latch float + blend rows** (§12). Depends: C.
- **T11 — NON-SIM lock-in** (no code): pin 27a-pre, 32, 45-body, 47-body, 48-`57BA50`/`580630`-bodies,
  50-stubs, 54; tickets: `B8AC/BAB4/C128/6C4C/6E7/88D8/B7EC/B8984/C060-row` reader grep (batch-3),
  `A42490/53xxx-concept/59xxx/A41C00` payload bodies.

## §16 Corrections to existing docs

| doc:line | old | new | evidence |
|---|---|---|---|
| team-layer §1 steps 27a-pre, 29–32, 35–36, 40, 45, 46-body, 47-meaning, 48-bodies, 49, 50-stubs, 52–53, 55 + step-23 callees "OPEN" | all OPEN | SIM = 23-bitmasks, 29, 30, 31, 35, 36, 40, 46-body, 48-writes, 49, 51, 52, 53, 55; NON-SIM = 27a-pre, 32, 45-body, 47-body, 48-`57BA50`/`580630`-bodies, 50-stubs, 54 | raw dis this report |
| team-layer §3 "Explicitly NOT sliced" | batch-1 list | batch-2 sliced T5b–T11; remaining: 26b, 26c-body, 27b-detail, 27c, 27d-detail, 28, 33, 34, 37, 38, 39, 41–44 | this report §15 |
| tick-steps (batch-1) §0 | _(Main 19-10-26: this row is WITHDRAWN. Batch 1 attributes 525.0/115.0 to step 20 `FUN_1404FF8B0`, not step 35, and both are read there: `0x1404FFE00 DIVSS [0x1425C48E0]`, `0x1404FFF67 MOVSS [0x1425C48D4]`. Batch 1's values stand; only its cited sites were wrong and are fixed in place. Step 35's own reads, 1369.0 and 105.0, are correct as given here.)_ |
| tick-steps (batch-1) §12 step-23 verdict | "NON-SIM for visible writes; 5 callees OPEN" | visible brain-header NON-SIM stands; 5 callees are SIM via nested `A424F0/A42530` bitmask writes | raw dis §13 |
| constant-team.md §2 / contract base | registry records at `0x142b49810+16·i` | contract base `0x142b48ca0+16·i` CONFIRMED for i=15/196 (records resolve to real JSON paths this pass); the two bases differ by `0xDB70` = 56208 = 3513 records — batch-3 must reconcile which base the getters use | image bytes this pass |
| directive §2 V5 / team-layer §2 | protection-pick floor sites | step-35 uses the same −100000.0 idiom (`0x1425C6614` family); no new claim | raw dis §6 |

## §17 Open items

1. `M+0xB8F8/B8FC` readers — grep batch-3 bodies + tick dec. Settles step-23 consumer.
2. `T+0x8984`-family / `T+0xB8AC`(=4 sites) / `T+0xBAB4` / `T+0xC128` / `T+0x6C4C/6E7/88D8/B7EC/78/130` readers.
3. Payload bodies OPEN: `FUN_140A42490` (step-45 commit), `FUN_14053xxx` concept calls, `FUN_14059xxx/1405A4xxx`
   record helpers, `FUN_140A41C00` claim test, `FUN_140A71990` heading, `FUN_140A79640` rank (direction known).
4. Step-46/48 shared-callee call graph (`FUN_1404FC4D0` from both) — one decode covers both steps (done here).
5. S1 writer hunt unchanged: unchecked `FUN_140A79640` callers outside the tick, or live write-watch.
6. Registry-base reconciliation (`0x142b48ca0` vs `0x142b49810`).
7. Live confirmation (read-only): `T+0xB7EC`, `T+0xBAB4`, `T+0x88D8`, `T+0x6E7` during a match.
8. Step-49 gate condition (`0x1404F79B0`-region) — still undecoded (batch-1 §15 item 11 carried).
9. Step-53 row-base identity (match rows vs T-relative) — needs the caller-frame walk.
10. Steps 26b/26c/27b-detail/27c/27d-detail/28/33/34/37/38/39/41–44 bodies — decode tickets (batch-3+).
