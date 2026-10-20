# PES STEP family: per-handler arm maps, exits and `req[0x39]` next-state writes

> **Main audit 20-10-26:** STOP exits call the shared requester 0x1406D5D60 at `0x1406C7F12` (state 15) and `0x1406C6C21` (state 5) directly, and for state 14 at `0x1406C1491` (`0x1406C148B` is its argument setup; the report now cites the call); Finding 11 loads `0x1406C3BF8` `movss xmm1,[rdi+0x10]` / `0x1406C3BFD` `movss xmm0,[rdi+0x8]`; `0x1406C3C24` is `call [rax+0x18]` (vtable, not FUN_14043A0A0): all match.
> **Main audit, round 2:** STEP_TURN_STEP exits `0x1406C9416` `movl $0xf,0xe4(%rsi)` and `0x1406C9DC4` `movl $0x4,0xe4(%rsi)` (+0xE4 = req word 0x39): match. The old "§7 Method" is now §12.

Read-only RE. No GF code changed, no PES written. Scratch: `<workdrive>/gf-TeamDecode-scratch/dec/`.
Base `0x140000000`, image `~/ghidra-projects/scratch/Unpacker/pes_full.bin`.
Tags: **CONFIRMED** = decompile and/or raw dis this pass; **INF** = inference; **OPEN** = blocker named.

Companion to `pes-re-runloop-arms.md` (the state dispatch table and the full `req[0x39]`
writer inventory). This report adds the **per-handler arm map** for the STEP family.

`req` is a `uint *`, so `req[N]` is word N and the byte offset is `4*N`.
**`req[0x39]` = byte offset `0xE4`** — the next-state request.
**`req+0x38` = byte offset `0xE0`** — the "a state change is requested" flag (`kChangeState`).
PES sets `req+0x38 = 1` **only** in the arms that also write `req[0x39]`; the two are always paired.

**The port's word numbering** (`<workdrive>/gf-ClipWords/src/pes/movestep.hpp:133` `namespace moveword`)
matches PES exactly: `kChangeState = 0x38`, `kNextState = 0x39`, `kClassWord = 0x1f` (byte `0x7c`),
`kDwellElapsed = 0x1a`, `kDwellLimit = 0x1b`, `kDwellElapsed2 = 0x2d`, `kDwellLimit2 = 0x2e`,
`kRate = 0x4a`, `kDistToTarget = 0x11`, `kDistToLook = 0x12`, `kTurn = 0x06`, `kKind = 0x00`,
`kCommandSpeedClass = 0x09`, `kMoveBucket = 0x08`, `kPrecise = 0x0e`, `kCommandId = 0x16`.

## §0 The state dispatch table (the numbering anchor)

PES dispatches through a jump table at **`0x1406BB50C`**, indexed by the state byte:
`0x1406BA9EB MOV EAX,dword ptr [RCX+RAX*0x4+0x6bb50c]` / `0x1406BA9F2 ADD RAX,RCX` / `0x1406BA9F5 JMP RAX`.

| state | port name | handler |
|---|---|---|
| 4 | `kStepLoop` | `FUN_1406C4C10` |
| 5 | `kStepLoopChange` | `FUN_1406C6480` |
| 13 | `kStepToRun` | `FUN_1406C6F80` |
| 14 | `kRunToStep` | `FUN_1406C1070` |
| 15 | `kStepTurnRun` | `FUN_1406C7400` |
| 16 | `kRunTurnStep` | `FUN_1406C3470` |
| 19 | `kStepTurnStep` | `FUN_1406C8D80` |
| 22 | `kTimingStep` | `FUN_1406CA6B0` → `FUN_1406D6290` |

## §0b Float table (script-read from `pes_full.bin`; `repr` exact, bits exact)

Every constant below is read from the image by script (`<workdrive>/gf-TeamDecode-scratch/readf.py`),
never typed. The `use` column names the instruction that consumes it.

| `VA` | `value` (`0xBITS`) | use |
|---|---|---|
| `0x14259BDD4` | `0.75` (`0x3F400000`) | `0x1406C1360 MULSS XMM0,[0x14259BDD4]` — the dwell 3/4 gate (step 14) |
| `0x1425DDFD4` | `1139.0625` (`0x448E6200`) | `0x1406C1375 COMISS XMM7,[0x1425DDFD4]` — the angle² cap (step 14) |
| `0x142590D98` | `0.3` (`0x3E99999A`) | `0x1406C6BFB MULSS XMM7,[0x142590D98]` — the class scale (step 5) |
| `0x14259BDFC` | `11.0` (`0x41300000`) | `0x1406C7EAF MOVSS XMM0,[0x14259BDFC]` — the class-1 gate (step 15) |
| `0x14259222C` | `5.0` (`0x40A00000`) | `0x1406C7DEA MOVSS XMM0,[0x14259222C]` (step 15) |
| `0x14259D618` | `22.5` (`0x41B40000`) | the STEP_LOOP speed window |
| `0x142592228` | `0.25` (`0x3E800000`) | `0x1406C57A1 MULSS XMM10,[0x142592228]` — the dwell scale (step 4) |
| `0x1425A8A28` | `0.05000000074505806` (`0x3D4CCCCD`) | the rate clamp step (steps 14, 4) |
| `0x14257A194` | `0.699999988079071` (`0x3F333333`) | the dwell scale (step 14) |
| `0x14259858C` | `360.0` (`0x43B40000`) | the wrap |
| `0x142598598` | `-360.0` (`0xC3B40000`) | the wrap |
| `0x142598580` | `180.0` (`0x43340000`) | the wrap half |
| `0x14256AE1C` | `1.0` (`0x3F800000`) | the scale |
| `0x142588218` | `-1.0` (`0xBF800000`) | the sign |
| `0x142565604` | `2.0` (`0x40000000`) | the distance band |
| `0x142565600` | `0.5` (`0x3F000000`) | the half |
| `0x14259BE5C` | `90.0` (`0x42B40000`) | the angle |
| `0x14259BE60` | `135.0` (`0x43070000`) | the angle |
| `0x14259BE10` | `16.0` (`0x41800000`) | the rate band ceiling |
| `0x1425DDF98` | `31.640625` (`0x41FD2000`) | the dwell gate (step 14) |
| `0x1425DDFD8` | `8100.0` (`0x45FD2000`) | the angle² cap (step 14) |
| `0x14259BDF0` | `7.0` (`0x40E00000`) | the band |
| `0x14259BE1C` | `20.0` (`0x41A00000`) | the distance |
| `0x14259BE04` | `12.0` (`0x41400000`) | the band |
| `0x1425C65E8` | `19.0` (`0x41980000`) | `0x1406C7E69 MOVSS XMM0,[0x1425C65E8]` (step 15) |
| `0x1425CC680` | `11.25` (`0x41340000`) | the angle band (step 19) |
| `0x1425CB30C` | `112.5` (`0x42E10000`) | the angle gate (step 16) |
| `0x1425CCA34` | `67.5` (`0x42870000`) | the distance floor |
| `0x14259BE30` | `45.0` (`0x42340000`) | the angle cap |
| `0x1425DDFF4` | `-110.0` (`0xC2DC0000`) | the rate clamp |
| `0x14259BDE0` | `1.7999999523162842` (`0x3FE66666`) | §8.7 T7 — the `XMM6 - 1.8` window |
| `0x1425CB30C` | `112.5` (`0x42E10000`) | §10.1 — the 0xD gate `|kTurn + fVar38| < 112.5` |
| `0x1425CBDBC` | `-0.0` (`0x80000000`) | §10.1 — the 0xD write `req[0x5C]` floor |
| `0x1425DDF60` | `0.3500000238418579` (`0x3EB33334`) | §9.2 S2 — the residual-trim threshold |
| `0x1425DDFBC` | `146.25` (`0x43124000`) | §9.2/§9.7 — the angle band |
| `0x1425DDFB8` | `140.625` (`0x430CA000`) | §9.2 — the angle band (the other side) |
| `0x1425DDFCC` | `168.75` (`0x4328C000`) | §9.2 — the angle band (the other side) |
| `0x1425CC688` | `33.75` (`0x42070000`) | §9.2 — the angle band |
| `0x1425DDF50` | `0.1666666716337204` (`0x3E2AAAAB`) | §9.9 — the blend duration |

**The immediates in the disassembly are not memory constants** — they are inline
`0x40A00000` = `5.0`, `0x41300000` = `11.0`, `0x41800000` = `16.0`,
`0x3E888889` = `0.26666668`, `0x3E4CCCCD` = `0.2`, `0x3F800000` = `1.0`,
`0x3EB33333` = `0.35`, `0x3DCCCCCD` = `0.05`. These are read from the instruction
bytes by the checker.

## §1 The shared STOP / IDLE requester `FUN_1406D5D60` — the census's mechanism

Every STEP handler's STOP exit goes through this one function. It is the **only** place the
`kStop` (8) request is produced from the STEP family.

```
1406D5DA6 MOV EAX,dword ptr [RDX+0x7c]     ; the class word (req word 0x1f, byte 0x7c)
1406D5DA9 INC EAX
1406D5DAB CMP EAX,0x1
1406D5DAE JBE 0x1406d60b1                    ; class == 0 -> the IDLE branch
```
- **class == 0** → the IDLE branch → **`req[0x39] = 1`** at `0x1406D6243`
  (with `req[0x1B] = 0x3DCCCCCD`, `req[0x2E] = 0x3EB33333`, `req+0x38 = 1`).
- **class >= 1** → the STOP branch → **`req[0x39] = 8`** at `0x1406D608A`
  (with `req[0x78] = req[0]`, `req+0x38 = 1`, `req[0x5C] = R12`, `req[0xF0] = R12B`).

**So a STEP handler's STOP exit is conditional on the class word (`req` word `0x1F`) being >= 1.**
That is the switch the live census is seeing.

The other two shared exit helpers, for completeness:
- **`FUN_1406D2590`** — the "no target / give up" requester; it writes `req[0x39] = 6` (`kTurnStop`) at
  `0x1406D2D84`.
- **`FUN_1406D6780`** — the third exit; **OPEN** which value it writes (it is not in the
  `req[0x39]` immediate list of `pes-re-runloop-arms.md` §5, so it likely writes a register form or
  does not write `req[0x39]` at all).

## §2 Per-handler arm maps

### 2.1 `FUN_1406C1070` — `kRunToStep` (state 14) — the STEP family's gateway

**Arm 14.1 — the rate/ramp setup (decompile 22-56).**
- `req[0x4A]` is clamped toward the current rate by `DAT_1425A8A28` = `0.05` (`0x1406C1070`).
- The **dwell** words are read: `req[0x2D]` (`kDwellElapsed2`) and `req[0x2E]` (`kDwellLimit2`).

**Arm 14.2 — the "close enough" test and the three-way exit (decompile 96-119; raw `0x1406C1343`–`0x1406C1496`).**
```
1406C1343 MOVSS XMM1,dword ptr [RBX+0xb4]     ; req[0x2D] as a float
1406C134B MOVSS XMM0,dword ptr [RBX+0xb8]     ; req[0x2E] as a float
1406C1353 COMISS XMM1,XMM0
1406C135E JNC 0x1406c139d                     ; req[0x2D] >= req[0x2E] -> take the STEP_LOOP exit
1406C1360 MULSS XMM0,dword ptr [0x14259bdd4]  ; req[0x2E] * 0.75
1406C1368 COMISS XMM1,XMM0
1406C136b JC 0x1406c1496                      ; req[0x2D] < req[0x2E]*0.75 -> the "not yet" path
1406C1371 MULSS XMM7,XMM7
1406C1375 COMISS XMM7,dword ptr [0x1425ddfd4]  ; the angle^2 vs 1139.0625
1406C137c JA 0x1406c139d
1406C137e COMISS XMM1,XMM0
1406C1381 JC 0x1406c1496
1406C1387 CMP dword ptr [RBX+0x38],0x1        ; req+0x38 (the change-state flag) == 1
1406C138b JNZ 0x1406c1496
1406C1391 MOV EAX,dword ptr [RBX+0x7c]        ; the class word
1406C1394 CMP dword ptr [RBX+0x24],EAX        ; req[0x24] (kPrevClassWord) vs the class word
1406C1397 JGE 0x1406c1496                     ; kPrevClassWord >= class -> the "not yet" path
```
**=> The `req[0x39] = 4` (`kStepLoop`) exit at `0x1406C13BD` is taken when:**
```
req[0x2D] >= req[0x2E]                                  (the dwell has elapsed)
OR ( req[0x2D] >= req[0x2E] * 0.75
     AND angle^2 <= 1139.0625
     AND req[0x2D] >= req[0x2E] * 0.75
     AND req+0x38 == 1
     AND req[0x24] < req[0x1F] )
```
(0.75 is `DAT_14259BDD4` at `0x14259BDD4`; 1139.0625 is `DAT_1425DDFD4` at `0x1425DDFD4`.)

The writes at the exit (`0x1406C139D`–`0x1406C13BD`):
```
req[0x68]  = 0            ; 0x1406C13A6
req[0xB4]  = 0            ; 0x1406C13AA
req[0x78]  = req[0]       ; 0x1406C13B3
req+0x38   = 1            ; 0x1406C13B6
req[0x39]  = 4            ; 0x1406C13BD  <-- kStepLoop
```
(the decompile also shows `req[0x1A] = 0`, `req[0x1B] = 0`, `req[0x2D] = 0`, `req[0x2E] = 0`,
`req[0x1E] = req[0]`, `req+0x3C] = 0` — these are the same words as `req[0x68]`/`req[0xB4]`/
`req[0xB8]`/`req[0x78]` seen as floats/ints at a different granularity; see §5.)

**Arm 14.3 — the "not yet" path, the three-way exit (raw `0x1406C1496` onward, decompile 105-119).**
```
0x1406C145E CMP dword ptr [RBX+0x24],-0x1     ; req[0x24] == -1 ?
0x1406C1462 JZ  0x1406c148b                    ; -> FUN_1406D5D60  (IDLE or STOP)
0x1406C1464 CMP dword ptr [RBX+0x24],-0x1
0x1406C1468 JZ  0x1406c147e                    ; -> FUN_1406D2590
0x1406C146A COMISS XMM9,dword ptr [RBX+0x44]   ; req[0x11] (kDistToTarget) vs the rate
0x1406C146F JA  0x1406c147e                    ; -> FUN_1406D2590
0x1406C1471 ... CALL 0x1406d6780               ; else -> FUN_1406D6780
```
**So from `kRunToStep` the STOP/IDLE exit is taken when `req[0x24] == -1`.**
That is the **STOP predecessor 14** the live census reports.

### 2.2 `FUN_1406C4C10` — `kStepLoop` (state 4)

This is the largest STEP handler (901 decompile lines, 1509 dis lines). Its three
`req[0x39]` writes:

| writer VA | value | next state | writes before it |
|---|---|---|---|
| `0x1406C592A` | `0xD` | `kStepToRun` | `req[0xDC] = r8`, `req[0x140] = r8b`, `req[0x68] = 0`, `req[0x6C] = 0x3E888889`, `req[0xB4] = 0`, `req[0xB8] = 0x3E888889`, `req[0x24] = clamp(req[0x24],0,3)`, `req[9] = clamp(req[0x24],0,3)`, `req[0x30] = DAT_1425DDCF0[req[9] + uVar12*4]`, `req+0x31] = the mirror byte`, `req[0x35] = 0x3F800000`, `req[0x36] = 0`, `req[0x32] = 0x3E888889`, `req[0x33] = 0`, `req[0x1E] = req[0]`, `req[0x20] = 0` |
| `0x1406C5D30` | `0xF` | `kStepTurnRun` | `req[0x24] = req[0x1F]`, `req[0x22] = req[0x1D]`, `req[0x25] = req[0x20]`, `req[0x20] = 0`, `req[0x1D] = fVar22`, `req[0x1F] = iVar29`, `req[0x1E] = req[0]` |
| `0x1406C62C1` | `0x5` | `kStepLoopChange` | `req[0x24] = req[0x1F]`, `req[0x22] = req[0x1D]`, `req[0x25] = req[0x20]`, `req[0x20] = local_114`, `req[0x1D] = fVar28`, `req[0x1F] = iVar31`, `req[0x1E] = iVar10` |

**The three-way selection (decompile 505-560).** The handler reaches the block at decompile 505:
```
if ((!bVar20) && (local_res8[0]._0_1_ == 0 && !bVar19)) goto LAB_1406c5eba;   // the 0x5 arm
...
if ( ((req[0x1F] < 0)
      || (((iVar31 < 3) && (((iVar31 < 2) || (req[0] == 3)) || (req[0] == 6))) || (iVar29 != 0)))
     || (((1 < req[0x20]-3U) && (1 < req[0x20]-8U))
         || ((fVar38 <= fVar28) || (fVar24 * DAT_14259D618 + fVar21 <= fVar26))) ) ) {
    if ((float)req[0x1A] < (float)req[0x1B] * DAT_142592228) goto LAB_1406c5eba;  // the 0x5 arm
    ...
```
**=> The `req[0x39] = 5` (`kStepLoopChange`) exit is the FALL-THROUGH default** — it is reached
whenever neither of the two "action" conditions holds. The `req[0x39] = 0xD` and `0xF` exits
are the two *action* branches inside the `if` body.

**The `req[0x39] = 0xD` (`kStepToRun`) exit.** Reached after the `FUN_1406B9FF0` row pick and
`FUN_1406BA2F0`; the block at raw `0x1406C5843`–`0x1406C592A` computes the class clamp, the
representative motion `DAT_1425DDCF0[...]` and the mirror, then writes. The branch that selects it
is the one at decompile 393 (`if ((iVar29 == 0) && …)`) and the inner test at decompile 517.

**The `req[0x39] = 0xF` (`kStepTurnRun`) exit.** The block at decompile ~880–892: it recomputes
`iVar29 = clamp(req[0x1F], 0, 2)`, calls `FUN_1406B9FF0(param_1, req[0xB], iVar29, fVar22, req[0])`
and stores the result in `req[0x4A]`, then writes the six state words and `req[0x39] = 0xF`.

**The `FUN_1406D6290` (TIMING_STEP) call at `0x1406C4E51`.**
`FUN_1406D6290` is a helper whose single caller is `0x1406C4E51`, inside `FUN_1406C4C10`.
**=> `kTimingStep` is entered from `kStepLoop`.** It writes `req[0x39] = 0x16` at `0x1406D66F8`.

**The STOP exit.** One `FUN_1406D5D60` call, at `0x1406C4F36`:
```
0x1406C4F10 CALL 0x1406b6ee0
0x1406C4F15 TEST AL,AL
0x1406C4F17 JZ 0x1406c4f40              ; -> the "not yet" path
0x1406C4F19 CMP dword ptr [RDI+0x7c],R13D
0x1406C4F1D JNZ 0x1406c4f30
0x1406C4F1F COMISS XMM14,XMM10
0x1406C4F23 JBE 0x1406c4f30
0x1406C4F25 MOV dword ptr [RDI+0x7c],R15D
0x1406C4F29 MOV dword ptr [RDI+0x74],0x40a00000
0x1406C4F30 ... CALL 0x1406d5d60         ; <-- the STOP/IDLE exit
```
**=> `kStepLoop` reaches STOP when `FUN_1406B6EE0` returns true** (and it first raises the class
word and drops `req[0x74]` to `0x40A00000` = `5.0` when the class equals `R13D` and the rate is
above `XMM10`).

### 2.3 `FUN_1406C6480` — `kStepLoopChange` (state 5)

- **The STOP exit**: one `FUN_1406D5D60` call, at `0x1406C6C21`:
```
0x1406C6BF0 MOV EAX,dword ptr [RDI+0x90]     ; req[0x24] (kPrevClassWord) as an int
0x1406C6BF6 CMP dword ptr [RDI+0x7c],EAX     ; the class word vs kPrevClassWord
0x1406C6BF9 JGE 0x1406c6c05
0x1406C6BFB MULSS XMM7,dword ptr [0x142590d98] ; * 0.3
0x1406C6C03 JMP 0x1406c6c0a
0x1406C6C05 MULSS XMM7,XMM11                  ; * 1.0
0x1406C6C0A COMISS XMM7,XMM1
0x1406C6C0D JBE 0x1406c6c1b
0x1406C6C0F MOV dword ptr [RDI+0x7c],EAX      ; raise the class word
0x1406C6C12 MOV EAX,dword ptr [RDI+0x88]
0x1406C6C18 MOV dword ptr [RDI+0x74],EAX
0x1406C6C1B ... CALL 0x1406d5d60              ; <-- the STOP/IDLE exit
```
  (0.3 is `DAT_142590D98` = `0.3` at `0x142590D98`; 1.0 is `DAT_14256AE1C`.)
  **=> `kStepLoopChange` reaches STOP unconditionally on this path** — the handler's exit is the
  `FUN_1406D5D60` call, so the class word decides IDLE vs STOP.
- **The `req[0x39] = 4` (`kStepLoop`) exit** at `0x1406C6F27`.

### 2.4 `FUN_1406C6F80` — `kStepToRun` (state 13)

**PES VA for the `req[0x39] = 3` (`kRunLoop`) write: `0x1406C73A3`.**
- **The STOP exit**: one `FUN_1406D5D60` call, at `0x1406C71BB`:
```
0x1406C718A CALL 0x1406d2f60
0x1406C718F TEST AL,AL
0x1406C7191 JNZ 0x1406c73c1          ; -> the RUN_LOOP exit
0x1406C7197 MOVSS XMM0,dword ptr [RBX+0x6c]
0x1406C719C SUBSS XMM0,dword ptr [RBX+0x68]
0x1406C71A1 MOVSS dword ptr [RBX+0x5c],XMM9
0x1406C71A7 COMISS XMM10,XMM0
0x1406C71AB MOVSS dword ptr [RBX+0x60],XMM0
0x1406C71B0 JC 0x1406c71b5
0x1406C71B2 MOV dword ptr [RBX+0x60],ESI
0x1406C71B5 ... CALL 0x1406d5d60      ; <-- the STOP/IDLE exit
```
  **=> `kStepToRun` reaches STOP when `FUN_1406D2F60` returns false** (i.e. the RUN_TURN_STEP
  pre-empt did not fire).
- **The `req[0x39] = 3` (`kRunLoop`) exit** at `0x1406C73A3`.

### 2.5 `FUN_1406C7400` — `kStepTurnRun` (state 15)

- **The STOP exit (1 of 2)**: `FUN_1406D5D60` at `0x1406C7F12`:
```
0x1406C7EE0 MOV EAX,dword ptr [R14+0x80]
0x1406C7EE7 LEA RCX,[R15+0xac0]
0x1406C7EEE XOR R9D,R9D
0x1406C7EF1 MOV dword ptr [R14+0x94],EAX
0x1406C7EF8 MOV RAX,qword ptr [RCX]
0x1406C7EFB MOVAPS XMM1,XMM3
0x1406C7EFE LEA R8D,[R9+0xa]
0x1406C7F02 CALL qword ptr [RAX+0x60]
0x1406C7F05 MOV RDX,R14
0x1406C7F08 MOV RCX,R15
0x1406C7F0B MOV dword ptr [R14+0x80],EAX
0x1406C7F12 CALL 0x1406d5d60       ; <-- the STOP/IDLE exit
```
  The block is entered from the branch ladder at `0x1406C7DDF`–`0x1406C7DF4`:
```
0x1406C7DCA CMP EBX,0x1
0x1406C7DCD JZ 0x1406c7de1
0x1406C7DCF MOVSS XMM0,dword ptr [RSP+0x158]
0x1406C7DD8 COMISS XMM0,XMM6
0x1406C7DDB JBE 0x1406c8364
0x1406C7DE1 CMP ECX,-0x1
0x1406C7DE4 JNZ 0x1406c7f1c
0x1406C7DEA MOVSS XMM0,dword ptr [0x14259222c]     ; 5.0
0x1406C7DF2 TEST AL,AL
0x1406C7DF4 JZ 0x1406c7e97
```
  and the three sub-arms that set the class word and `req[0x74]`:
```
0x1406C7E97 COMISS XMM0,XMM9
0x1406C7E9B JBE 0x1406c7eaf
0x1406C7E9D MOV dword ptr [R14+0x7c],0x0        ; class 0
0x1406C7EA5 MOV dword ptr [R14+0x74],0x40a00000 ; 5.0
0x1406C7EAF MOVSS XMM0,dword ptr [0x14259bdfc]    ; 11.0
0x1406C7EB7 COMISS XMM0,XMM9
0x1406C7EBB JBE 0x1406c7ed0
0x1406C7EBD MOV EAX,0x1
0x1406C7EC2 MOV dword ptr [R14+0x74],0x41300000 ; 11.0
0x1406C7ECA MOV dword ptr [R14+0x7c],EAX        ; class 1
0x1406C7ED0 MOV dword ptr [R14+0x7c],0x2        ; class 2
0x1406C7ED8 MOV dword ptr [R14+0x74],0x41800000 ; 16.0
```
  **=> `kStepTurnRun` reaches STOP with the class word set to 0, 1 or 2 by the branch above.**
  Note class 0 → **IDLE**, class 1 and 2 → **STOP**.
- **The `req[0x39] = 4` (`kStepLoop`) exit** at `0x1406C7B19`.
- The second `FUN_1406D5D60` call site in this handler is on a second path.

### 2.6 `FUN_1406C3470` — `kRunTurnStep` (state 16)

- **Five `FUN_1406D5D60` call sites** — the handler has the most STOP exits.
- The first (`0x1406C3D24`):
```
0x1406C3CF4 MOV EAX,dword ptr [RDI+0x80]
0x1406C3CFA LEA RCX,[RSI+0xac0]
0x1406C3D01 XOR R9D,R9D
0x1406C3D04 MOV dword ptr [RDI+0x94],EAX
0x1406C3D0A MOV RAX,qword ptr [RCX]
0x1406C3D0D MOVAPS XMM1,XMM12
0x1406C3D11 LEA R8D,[R9+0xa]
0x1406C3D15 CALL qword ptr [RAX+0x60]
0x1406C3D18 MOV RDX,RDI
0x1406C3D1B MOV RCX,RSI
0x1406C3D1E MOV dword ptr [RDI+0x80],EAX
0x1406C3D24 CALL 0x1406d5d60       ; <-- a STOP/IDLE exit
```
  and just before it, the sibling exit:
```
0x1406C3CDD MOV dword ptr [RDI+0x74],0x41800000   ; 16.0
0x1406C3CE4 MOV RDX,RDI
0x1406C3CE7 MOV RCX,RSI
0x1406C3CEA CALL 0x1406d2590                        ; the other shared exit
```
- **The `req[0x39] = 4` (`kStepLoop`) exit** at `0x1406C4BA8`.

### 2.7 `FUN_1406C8D80` — `kStepTurnStep` (state 19)

- **The STOP exit (1 of 2)**: `FUN_1406D5D60` at `0x1406C9328`:
```
0x1406C92EB CALL 0x1406d38d0        ; the shared STEP_TURN helper
0x1406C92F0 TEST AL,AL
0x1406C92F2 JNZ 0x1406c9dce         ; -> the 0xF / 0x4 exits
0x1406C92F8 MOV EAX,dword ptr [RSI+0x80]
0x1406C92FE LEA RCX,[RBP+0xac0]
0x1406C9305 XOR R9D,R9D
0x1406C9308 MOV dword ptr [RSI+0x94],EAX
0x1406C930E MOV RAX,qword ptr [RCX]
0x1406C9311 MOVAPS XMM1,XMM9
0x1406C9315 LEA R8D,[R9+0xa]
0x1406C9319 CALL qword ptr [RAX+0x60]
0x1406C931C MOV RDX,RSI
0x1406C931F MOV RCX,RBP
0x1406C9322 MOV dword ptr [RSI+0x80],EAX
0x1406C9328 CALL 0x1406d5d60         ; <-- the STOP/IDLE exit
```
  **=> `kStepTurnStep` reaches STOP when the shared helper `FUN_1406D38D0` returns false.**
- The `req[0x39] = 0xF` (`kStepTurnRun`) exit at `0x1406C9416` and the
  `req[0x39] = 4` (`kStepLoop`) exit at `0x1406C9DC4` are on the other side of that test
  (`0x1406C92F2 JNZ 0x1406c9dce`).

### 2.8 `FUN_1406D6290` — the `kTimingStep` helper (state 22)

- **Its `req[0x39] = 0x16` write is at `0x1406D66F8`.** Its only caller is `0x1406C4E51`, inside
  `FUN_1406C4C10` (`kStepLoop`) — so **`kTimingStep` is entered from `kStepLoop`**.
- The function has **no** `FUN_1406D5D60` call — it never requests STOP itself.

## §3 The STOP predecessors, per the live census (5, 15, 14)

| census predecessor | PES state | handler | the STOP exit |
|---|---|---|---|
| **14** `kRunToStep` | 14 | `FUN_1406C1070` | `0x1406C1491` — reached when `req[0x24] == -1` (raw `0x1406C145E`/`0x1406C1462`) |
| **15** `kStepTurnRun` | 15 | `FUN_1406C7400` | `0x1406C7F12` — the branch ladder at `0x1406C7DCF`–`0x1406C7DF4`; the class word becomes 0, 1 or 2 |
| **5** `kStepLoopChange` | 5 | `FUN_1406C6480` | `0x1406C6C21` — the handler's own exit, unconditional on that path |

**The common switch in all three is the class word (`req` word `0x1F`, byte `0x7c`):**
`FUN_1406D5D60` writes IDLE (1) when it is 0 and STOP (8) when it is >= 1.

## §4 `FUN_1406BA900` — the shared INIT / row-pick helper

`FUN_1406BA900` is **not** a state handler: it is the shared helper the state machine calls for
the clip row pick. It owns one `req[0x39]` write:
- `0x1406BB0C8 MOV dword ptr [RSI+0xe4],0x4` — **`req[0x39] = 4` (`kStepLoop`)**, CONFIRMED raw dis.
- and one register-form write at `0x1406BB112 MOV dword ptr [RSI+0xe4],EDI` where
  `EDI = MOVSX EDI,AX` from `0x1406BB005` — the **return value of `FUN_1406ABB30`**, the clip
  picker. So this site requests whatever state the picked clip's row carries (data-driven).

The `req[0x39] = 4` write at `0x1406BB0C8` is in the block that also writes
`req[0x78] = R12D`, `req+0xE0] = 1`, `req[0x68] = R15`, `req[0x6C] = 0x3E888889` — i.e. it is the
"issued the STEP_LOOP clip" exit. Its arm conditions are **OPEN** (the block is entered from the
`0x1406B9FF0` row pick; the exact gate is in `FUN_1406BA900` decompile lines 250-300).

## §5 `FUN_1406D38D0` / `FUN_1406D2F60` — which state each call site belongs to

`FUN_1406D38D0` (20 call sites) writes `req[0x39] = 0xF` (twice: `0x1406D4395`, `0x1406D541E`) and
`0x13` (twice: `0x1406D4710`, `0x1406D5D29`) — it is the shared **STEP_TURN** helper.
`FUN_1406D2F60` (13 call sites) writes `req[0x39] = 0x10` at `0x1406D3837` — the shared
**RUN_TURN_STEP** helper.

The call sites and the state each belongs to (the owner is resolved by the containing function,
and the state by the §0 dispatch table):

| call site | owner function | state |
|---|---|---|
| `0x1406BAE7F` | `FUN_1406BA900` | (the shared INIT/row helper) |
| `0x1406BC048`, `0x1406BDA43`, `0x1406BEDD0` | `FUN_1406BA900` / `FUN_1406BDD70` (kDemo) / `FUN_1406BDF10` (kGotoRun) | mixed — see below |
| `0x1406C2400`, `0x1406C2B29` | `FUN_1406C14C0` | **12 `kRunTurnRun`** |
| `0x1406C40F4` | `FUN_1406C3470` | **16 `kRunTurnStep`** |
| `0x1406C4E3E` | `FUN_1406C4C10` | **4 `kStepLoop`** |
| `0x1406C685B`, `0x1406C69CD` | `FUN_1406C6480` | **5 `kStepLoopChange`** |
| `0x1406C7CFC`, `0x1406C82EF` | `FUN_1406C7400` | **15 `kStepTurnRun`** |
| `0x1406C8A05` | `FUN_1406C8D80` | **19 `kStepTurnStep`** |
| `0x1406C92EB` | `FUN_1406C8D80` | **19 `kStepTurnStep`** |
| `0x1406C962B` | `FUN_1406C8D80` | **19 `kStepTurnStep`** |
| `0x1406CA01A`, `0x1406CA1AB` | `FUN_1406C9E20` | **8 `kStop`** |
| `0x1406CA9A6` | `FUN_1406CA6B0` | **22 `kTimingStep`** |
| `0x1406CB100`, `0x1406CB65B` | `FUN_1406CAB20` | **6 `kTurnStop`** |

`FUN_1406D2F60` (13 sites): `0x1406BAC83` (the INIT path), `0x1406BC1C2`, `0x1406BDB6C`,
`0x1406BEF69`, `0x1406C03E8`, `0x1406C2537`, `0x1406C3FEE` (→ `FUN_1406C3470`, 16),
`0x1406C67D5`, `0x1406C718A` (→ `FUN_1406C6F80`, 13), `0x1406C80C5` (→ `FUN_1406C7400`, 15),
`0x1406CA17F`, `0x1406CB0A7`, `0x1406CB60C`.

**Note:** `FUN_1406D38D0` and `FUN_1406D2F60` are called from *several* states, so the
`req[0x39]` they write (0xF / 0x13 / 0x10) is **not** unique to one predecessor.

## §5b The STEP family's `req[0x39]` writers, in one table (CONFIRMED)

| writer VA | value | next state | handler (state) |
|---|---|---|---|
| `0x1406C13BD` | `0x4` | `kStepLoop` | `FUN_1406C1070` (14 `kRunToStep`) |
| `0x1406C592A` | `0xD` | `kStepToRun` | `FUN_1406C4C10` (4 `kStepLoop`) |
| `0x1406C5D30` | `0xF` | `kStepTurnRun` | `FUN_1406C4C10` (4 `kStepLoop`) |
| `0x1406C62C1` | `0x5` | `kStepLoopChange` | `FUN_1406C4C10` (4 `kStepLoop`) |
| `0x1406C6F27` | `0x4` | `kStepLoop` | `FUN_1406C6480` (5 `kStepLoopChange`) |
| `0x1406C73A3` | `0x3` | `kRunLoop` | `FUN_1406C6F80` (13 `kStepToRun`) |
| `0x1406C7B19` | `0x4` | `kStepLoop` | `FUN_1406C7400` (15 `kStepTurnRun`) |
| `0x1406C4BA8` | `0x4` | `kStepLoop` | `FUN_1406C3470` (16 `kRunTurnStep`) |
| `0x1406C9416` | `0xF` | `kStepTurnRun` | `FUN_1406C8D80` (19 `kStepTurnStep`) |
| `0x1406C9DC4` | `0x4` | `kStepLoop` | `FUN_1406C8D80` (19 `kStepTurnStep`) |
| `0x1406D66F8` | `0x16` | `kTimingStep` | `FUN_1406D6290` (the helper; called only from `FUN_1406C4C10`) |

All are immediate-form `C7 /0` stores to displacement `0xE4`; each is preceded by
`MOV byte ptr [reg+0xE0],0x1` (the `req+0x38` change-state flag), so the two are paired.

**The two `FUN_1406C8D80` writers, re-confirmed in round 2:**
- `0x1406C9416` = `0xF` (kStepTurnRun) — `0x1406C940F MOV byte ptr [RSI+0xe0],0x1` immediately before;
- `0x1406C9DC4` = `0x4` (kStepLoop) — `0x1406C9DBD MOV byte ptr [RSI+0xe0],0x1` immediately before.

**The `FUN_1406C4C10` 0xD writer:** `0x1406C592A` = `0xD` (kStepToRun).
**The `FUN_1406C4C10` 0xF writer:** `0x1406C5D30` = `0xF` (kStepTurnRun).

## §6 Finding 11 — the dropped `FUN_14043A0A0` operand pairs in `FUN_1406C3470` (recovered)

**`wrap180` arg order, confirmed once more in this pass:** `FUN_14043A0A0(XMM0 = a, XMM1 = b)`
returns **`wrap180(b - a)`**. The **FIRST** operand is `a`. Every site below is read off the raw
disassembly, not the decompile.



**The signature.** `float FUN_14043A0A0(float a, float b)` — `XMM0` = `a`, `XMM1` = `b` — and the body is
```
param_1 = wrap180(param_1)   ; a into [0,360)
param_2 = wrap180(param_2)   ; b into [0,360)
param_2 = param_2 - param_1
if (180.0 < param_2) param_2 -= 360.0 * sign(param_2)   ; into (-180, 180]
return param_2
```
So it returns **`wrap180(b - a)`** — the signed angular difference from `a` to `b`.

**The base register.** `0x1406C348E MOV RDI,RDX` — `RDI` is the request block (`param_2` of
`FUN_1406C3470`, which the tick passes as `RDX = RSI`). So:

| address | req word |
|---|---|
| `RDI+0x08` | word `0x02` — `kFacing` |
| `RDI+0x10` | word `0x04` — `kBodyDirection` |
| `RDI+0x18` | word `0x06` — `kTurn` |
| `RDI+0x1C` | word `0x07` — `kFacingError` |
| `RDI+0x80` | word `0x20` |

**The recovered pairs.** Every `CALL 0x14043a0a0` in `FUN_1406C3470`, with the two instructions that
set `XMM0`/`XMM1` immediately before it (CONFIRMED raw dis, `dec/dis_1406C3470.txt`):

| call VA | XMM0 (`a`) | XMM1 (`b`) | the call computes |
|---|---|---|---|
| `0x1406C3889` | `XMM11` (the current facing, carried) | `XMM6` (`0x1406C3879 MOVAPS XMM1,XMM6`) | `wrap180(XMM6 - XMM11)` |
| `0x1406C390C` | `req[0x02]` (`kFacing`, `0x1406C3903 MOVSS XMM0,[RDI+0x8]`) | `XMM11` (the current facing) | `wrap180(XMM11 - req[0x02])` |
| `0x1406C3A12` | `req[0x02]` (`0x1406C3A0D MOVSS XMM0,[RDI+0x8]`) | `req[0x04]` (`kBodyDirection`, `0x1406C3A08 MOVSS XMM1,[RDI+0x10]`) | **`wrap180(req[0x04] - req[0x02])`** |
| `0x1406C3B4C` | `req[0x18]` (`0x1406C3B47 MOVSS XMM0,[RDI+0x60]`, byte `0x60` = word `0x18`; unnamed in the port's `moveword`) | `req[0x01]` (`kHeading`, `0x1406C3B42 MOVSS XMM1,[RDI+0x4]`, byte `0x04` = word `0x01`) | `wrap180(req[0x01] - req[0x18])` |
| **`0x1406C3C02`** | **`req[0x02]` (`kFacing`, `0x1406C3BFD MOVSS XMM0,[RDI+0x8]`)** | **`req[0x04]` (`kBodyDirection`, `0x1406C3BF8 MOVSS XMM1,[RDI+0x10]`)** | **`wrap180(req[0x04] - req[0x02])`** |
| `0x1406C3FAB` | `req[0x03]` (`kLookHeading`, `0x1406C3FA6 MOVSS XMM0,[RDI+0xc]`) | `req[0x01]` (`0x1406C3FA1 MOVSS XMM1,[RDI+0x4]`) | `wrap180(req[0x01] - req[0x03])` — guarded by `req+0x38 == 1` at `0x1406C3F9B` |
| `0x1406C4152` | `req[0x02]` (`0x1406C4148 MOVAPS XMM0,XMM7` where `XMM7 = req[0x02]` at `0x1406C413E`) | `req[0x04]` (`0x1406C4143 MOVSS XMM1,[RDI+0x10]`) | `wrap180(req[0x04] - req[0x02])` |
| `0x1406C41C3` | `XMM7` (`0x1406C41C0 MOVAPS XMM0,XMM7`) | `XMM7 + DAT_1425DD660[rax]` (`0x1406C41B7 ADDSS XMM1,[RCX+RAX*0x4+0x25dd660]`) | `wrap180(XMM7 + DAT_1425DD660[rax] - XMM7)` |
| `0x1406C4716` | `req[0x18]` (`0x1406C4711 MOVSS XMM0,[RDI+0x60]`) | `req[0x04]` (`0x1406C470C MOVSS XMM1,[RDI+0x10]`) | `wrap180(req[0x04] - req[0x18])` — guarded by `req[0x24] < 1` at `0x1406C4706` |
| `0x1406C47AB` | `req[0x02]` (`0x1406C47A1 MOVAPS XMM0,XMM6` where `XMM6 = req[0x02]` at `0x1406C4797`) | `req[0x04]` (`0x1406C479C MOVSS XMM1,[RDI+0x10]`) | `wrap180(req[0x04] - req[0x02])` |
| `0x1406C47FC` | `XMM6` (`0x1406C47EF MOVAPS XMM0,XMM6`) | `XMM6 + DAT_1425DD660[rax]` (`0x1406C47F2 ADDSS XMM1,[R12+RAX*0x4+0x25dd660]`) | `wrap180(XMM6 + DAT_1425DD660[rax] - XMM6)` |

**The two the brief names.**
- **`0x1406C3C02`** — the pair is **`(req[0x02], req[0x04])`**, i.e.
  `FUN_14043A0A0(req[0x02], req[0x04])` = `wrap180(kBodyDirection - kFacing)`.
  CONFIRMED: `0x1406C3BF8 MOVSS XMM1,dword ptr [RDI+0x10]` and
  `0x1406C3BFD MOVSS XMM0,dword ptr [RDI+0x8]`, then `0x1406C3C02 CALL 0x14043a0a0`.
  The result lands in `XMM0` and is copied to `XMM12` at `0x1406C3C0D MOVAPS XMM12,XMM0`.
- **`0x1406C3C20`** — this is **not** a `FUN_14043A0A0` call; it is
  `0x1406C3C24 CALL qword ptr [RAX+0x18]` — a **vtable call on the `+0xAC0` interface object**
  (`RAX = [R14]`, the interface vtable). Its argument is the single float `XMM0`, built from
  `XMM12` (the `0x1406C3C02` result) via `0x1406C3C14 CVTSS2SD XMM1,XMM12` /
  `0x1406C3C1B ANDPS XMM1,XMM8` / `0x1406C3C1F CVTPD2PS XMM13,XMM1`.
  **The `FUN_14043A0A0` call the brief may mean at `0x1406C3C20` is really at `0x1406C3C02`**;
  the neighbouring `FUN_14043A0A0` at `0x1406C3B4C` is a different pair.

**A note on the decompile defect.** The decompile drops the args because the two floats are loaded
into `XMM0`/`XMM1` immediately before the call and Ghidra's argument recovery loses them when the
same registers are reused in the surrounding block. **The raw disassembly is authoritative**: read
the two `MOVSS XMMn, [RDI+off]` instructions immediately before each `CALL 0x14043a0a0`.



1. `FUN_1406D6780` — the third shared exit helper; which `req[0x39]` value it writes is **OPEN**.
2. `FUN_1406BA900`'s arm conditions for the `req[0x39] = 4` write at `0x1406BB0C8` — **OPEN**.
3. The full reduction of `FUN_1406C4C10` (901 decompile lines) — the three exit conditions are given
   in §2.2 but the *inner* sub-conditions of the 0xD and 0xF branches are only partly read.
4. The same for `FUN_1406C7400` and `FUN_1406C3470` (both ~800 decompile lines) — the arm maps give
   the exit sites and their immediate guards, not every sub-branch.
5. The live confirmation of the census is **OPEN**; this report is static only.

## §7 Index — every exit of every STEP-family handler, in one place

### §7.1 The `req[0x39]` next-state writes

| writer VA | value | next state | handler (state) | section |
|---|---|---|---|---|
| `0x1406C13BD` | `0x4` | `kStepLoop` | `FUN_1406C1070` (14 `kRunToStep`) | §2.1 |
| `0x1406C592A` | `0xD` | `kStepToRun` | `FUN_1406C4C10` (4 `kStepLoop`) | §10.1 |
| `0x1406C5D30` | `0xF` | `kStepTurnRun` | `FUN_1406C4C10` (4 `kStepLoop`) | §10.2 |
| `0x1406C62C1` | `0x5` | `kStepLoopChange` | `FUN_1406C4C10` (4 `kStepLoop`) | §10.3 |
| `0x1406C6F27` | `0x4` | `kStepLoop` | `FUN_1406C6480` (5 `kStepLoopChange`) | §2.3 |
| `0x1406C73A3` | `0x3` | `kRunLoop` | `FUN_1406C6F80` (13 `kStepToRun`) | §2.4 |
| `0x1406C7B19` | `0x4` | `kStepLoop` | `FUN_1406C7400` (15 `kStepTurnRun`) | §2.5 |
| `0x1406C4BA8` | `0x4` | `kStepLoop` | `FUN_1406C3470` (16 `kRunTurnStep`) | §2.6 |
| `0x1406C9416` | `0xF` | `kStepTurnRun` | `FUN_1406C8D80` (19 `kStepTurnStep`) | §9.5 |
| `0x1406C9DC4` | `0x4` | `kStepLoop` | `FUN_1406C8D80` (19 `kStepTurnStep`) | §9.9 |
| `0x1406D66F8` | `0x16` | `kTimingStep` | `FUN_1406D6290` (the helper; called only from `FUN_1406C4C10` at `0x1406C4E51`) | §8.9 |

### §7.2 Every `FUN_1406D5D60` (the shared STOP / IDLE requester) call site in the STEP family

Each of these is a **STOP-or-IDLE** exit: the class word (`req` word `0x1F`, byte `0x7C`) decides
which — 0 gives IDLE (`req[0x39] = 1` at `0x1406D6243`), >= 1 gives STOP (`req[0x39] = 8` at
`0x1406D608A`). See §1.

| call VA | handler (state) | the gate that reaches it | section |
|---|---|---|---|
| `0x1406C1491` | `FUN_1406C1070` (14 `kRunToStep`) | `req[0x24] == -1` | §2.1 |
| `0x1406C4F36` | `FUN_1406C4C10` (4 `kStepLoop`) | `FUN_1406B6EE0` returned true | §2.2 |
| `0x1406C6C21` | `FUN_1406C6480` (5 `kStepLoopChange`) | the handler's own exit | §2.3 |
| `0x1406C71BB` | `FUN_1406C6F80` (13 `kStepToRun`) | `FUN_1406D2F60` returned false | §2.4 |
| `0x1406C7F12` | `FUN_1406C7400` (15 `kStepTurnRun`) | the branch ladder `0x1406C7DCF`-`0x1406C7DF4` | §2.5 |
| `0x1406C835A` | `FUN_1406C7400` (15 `kStepTurnRun`) | the second site | §2.5 |
| `0x1406C3D24` | `FUN_1406C3470` (16 `kRunTurnStep`) | the first of its five | §2.6 |
| `0x1406C9328` | `FUN_1406C8D80` (19 `kStepTurnStep`) | `FUN_1406D38D0` returned false | §9.6 |
| `0x1406C96A0` | `FUN_1406C8D80` (19 `kStepTurnStep`) | the `LAB_1406C9645` block, when `|kFacingError| < 67.5` | §9.7 |

**The complete `CALL 0x1406D5D60` site list, from the disassembly:**
`FUN_1406C1070` — `0x1406C1491` (1);
`FUN_1406C4C10` — `0x1406C4F36` (1);
`FUN_1406C6480` — `0x1406C6C21` (1);
`FUN_1406C6F80` — `0x1406C71BB` (1);
`FUN_1406C7400` — `0x1406C7F12`, `0x1406C835A` (2);
`FUN_1406C3470` — `0x1406C3D24`, `0x1406C3ED1`, `0x1406C3EFD`, `0x1406C3F1B`, `0x1406C4510` (5);
`FUN_1406C8D80` — `0x1406C9328`, `0x1406C96A0` (2).
`FUN_1406D6290` (TIMING_STEP) has **none** — it never requests STOP itself.

### §7.3 The other shared exits

| callee | what it is | the STEP-family call sites |
|---|---|---|
| `FUN_1406D2590` | the "no target / give up" requester; it writes `req[0x39] = 6` (`kTurnStop`) at `0x1406D2D84` | `0x1406C1484` (§2.1), `0x1406C3CEA` (§2.6) |
| `FUN_1406D6780` | the third shared exit helper — **which `req[0x39]` value it writes is OPEN** | `0x1406C1477` (§2.1), `0x1406C5673` (§10.1) |
| `FUN_1406CE820` | the re-plan; it writes `req[0x39] = 7` (`kGotoRun`) at `0x1406CFD31` | `0x1406C4E2E` (§8.4), `0x1406C95FE` (§9.7) |
| `FUN_1406D38D0` | the shared STEP_TURN helper; it writes `0xF` (`0x1406D4395`, `0x1406D541E`) and `0x13` (`0x1406D4710`, `0x1406D5D29`) | 20 call sites — see §5 |
| `FUN_1406D2F60` | the shared RUN_TURN_STEP helper; it writes `0x10` at `0x1406D3837` | 13 call sites — see §5 |

## §8 TIMING_STEP `FUN_1406D6290` — the full rules (CONFIRMED)

`FUN_1406D6290(param_1, param_2)` — `param_1` = the playback host, `param_2` = the request block.
In the asm: `RSI` = the request, `RBP` = the host, `RBX` = the `+0xAB8` interface,
`R15` = the `+0xAC0` interface, `RBP+0x4D0` = the callback object.

### §8.1 The two entry gates

**Gate T1 — the command id (asm `0x1406D62A2`):**
```
1406D62A2 MOV EDX,dword ptr [RDX + 0x58]   ; req+0x58 = req word 0x16 (kCommandId)
1406D62A5 SUB EDX,0x1 / 1406D62A8 JZ 0x1406d6772      ; kCommandId == 1     -> return 0
1406D62AE SUB EDX,0x4 / 1406D62B1 JZ 0x1406d6772      ; kCommandId == 5     -> return 0
1406D62B7 SUB EDX,0x31 / 1406D62BA JZ 0x1406d6772     ; kCommandId == 0x36  -> return 0
1406D62C0 CMP EDX,0x7 / 1406D62C3 JZ 0x1406d6772      ; kCommandId == 0x3D  -> return 0
```
**=> `kCommandId` must NOT be 1, 5, 54 (`0x36`) or 61 (`0x3D`).**

**Gate T2 — the ball side must have possession (asm `0x1406D62E1`–`0x1406D6314`):**
```
1406D62E1 XOR EDI,EDI
1406D62E3 TEST EAX,EAX / 1406D62E7 CMP EAX,0xb / 1406D62EA JL / 1406D62EC CMP EAX,0x16 / 1406D62EF SETL DIL
   ; EDI = (the player id > 10) -> the side
1406D6304 CALL 0x140a48a30                    ; that side's team record
1406D6309 CMP byte ptr [RAX + 0x255],0x0      ; T+0x255 (the possession latch)
1406D6310 JNZ 0x1406d6319                     ; the latch is set -> continue
1406D6312 XOR AL,AL / 1406D6314 JMP 0x1406d6754  ; else return 0
```

### §8.2 The rule chain

**T3 — the playback rate and the ball distance (asm `0x1406D63A3`–`0x1406D6408`):**
```
1406D63A3 CALL qword ptr [RAX + 0x30]        ; the +0xAC0 interface, slot 0x30
1406D63AF CALL qword ptr [RAX + 0x50]        ; the +0xAC0 interface, slot 0x50 -> the rate
1406D63C0 MOVSS dword ptr [RSI + 0xd4],XMM0 ; req+0xD4 = req word 0x35 (kPlayback) = the rate
1406D63C8 CALL 0x141eb50b0                  ; the ball position
1406D63CD..0x1406D63EB                      ; the squared magnitude
1406D63EF CALL 0x1415a1780                  ; the magnitude
1406D63F4 CALL 0x14042d470                  ; the current angle -> XMM6
1406D6402 CALL qword ptr [RAX + 0x8]         ; the +0xAB8 interface, slot 0x8
1406D6405 CMP EAX,0x15 / 1406D6408 JA 0x1406d676e   ; the player id must be < 22
```

**T4 — the class and command gates (asm `0x1406D640E`–`0x1406D6452`):**
```
1406D640E MOV EAX,dword ptr [RSI + 0x7c]               ; the class word (req word 0x1F)
1406D6411 MOVSS XMM2,dword ptr [0x14259be10]           ; 16.0
1406D6419 MOVD XMM1,EAX / 1406D641D CVTDQ2PS XMM1,XMM1
1406D6420 COMISS XMM2,XMM1 / 1406D6423 JC 0x1406d676e   ; 16.0 < class -> return 0
1406D6429 CMP EAX,0x3 / 1406D642C JZ 0x1406d676e        ; class == 3 -> return 0
1406D6432 CMP dword ptr [RSI + 0x24],0x0                ; req word 0x09 (kCommandSpeedClass)
1406D6436 JNZ 0x1406d676e                               ; != 0 -> return 0
1406D643C CMP dword ptr [RSI + 0x38],0x1                ; req word 0x0E (kPrecise)
1406D6440 JNZ 0x1406d676e                               ; != 1 -> return 0
1406D6446 CMP dword ptr [RSI],0x2                      ; req[0] (kKind)
1406D6449 JA 0x1406d676e                               ; kKind > 2 -> return 0
1406D644F COMISS XMM2,XMM6 / 1406D6452 JBE 0x1406d676e  ; 16.0 <= the angle -> return 0
```

**T5 — the two angle windows (asm `0x1406D6458`–`0x1406D64A2`):**
```
1406D6458 MOVSS XMM0,dword ptr [RSI + 0x18]    ; req word 0x06 (kTurn)
1406D6466 MOVSS XMM2,dword ptr [0x14259d618]   ; 22.5
1406D646E..0x1406D6475                        ; |kTurn|
1406D6479 COMISS XMM2,XMM1 / 1406D647C JA 0x1406d648f   ; 22.5 > |kTurn| -> skip the 2.5 test
1406D647E MOVSS XMM0,dword ptr [0x14259bde4]   ; 2.5
1406D6486 COMISS XMM0,XMM6 / 1406D6489 JBE 0x1406d676e  ; 2.5 <= the angle -> return 0
1406D648F MOVSS XMM0,dword ptr [RSI + 0x1c]    ; req word 0x07 (kFacingError)
1406D6494..0x1406D649B                        ; |kFacingError|
1406D649F COMISS XMM2,XMM1 / 1406D64A2 JBE 0x1406d676e  ; 22.5 <= |kFacingError| -> return 0
```
**=> `|kTurn| < 22.5`, OR (`|kTurn| >= 22.5` AND the current angle `< 2.5`); and `|kFacingError| < 22.5`.**

**T6 — the opponent's side (asm `0x1406D64A8`–`0x1406D64F5`):**
```
1406D64A8 MOVSX EAX,byte ptr [RBP + 0x2458]  ; P+0x2458, a signed byte
1406D64B1..0x1406D64BD                       ; DIL = (0 < byte < 22) -> that player's side
1406D64D2 CALL 0x140a48a30                   ; that side's team record
1406D64E2 MOVZX R12D,byte ptr [RDI + 0x254]  ; T+0x254 (the side sign)
1406D64F5 CALL 0x140a71520                   ; the home angle for that side -> XMM10
```

**T7 — the two distance measures (asm `0x1406D6520`–`0x1406D6599`):**
```
1406D6520 CMP byte ptr [RDI + 0x255],0x0     ; the OPPONENT's possession latch
1406D6527 MOVSX EAX,R12B / 1406D652F CVTDQ2PS XMM7,XMM7
1406D6535 MULSS XMM7,dword ptr [RBP + 0x8]   ; XMM7 = |sign| * P+0x8
1406D653A MULSS XMM6,dword ptr [RCX + R14*0x1]; XMM6 = |sign| * homeX   (RCX = 0x54 or 0x40)
1406D6540 JZ 0x1406d676e                      ; the opponent HAS the ball -> return 0
1406D6546 MOVSS XMM0,dword ptr [RSI + 0x4]    ; req word 0x01 (kHeading)
1406D654B MOVAPS XMM1,XMM10                   ; the home angle
1406D654F CALL 0x14043a0a0                   ; wrap180(homeAngle - kHeading)   (a = kHeading)
1406D655B MOVSS XMM0,dword ptr [0x1425cca34]  ; 67.5
1406D656B COMISS XMM0,XMM2 / 1406D656E JBE 0x1406d676e   ; 67.5 <= |wrap180| -> return 0
1406D6574 MOVAPS XMM0,XMM6 / 1406D6577 SUBSS XMM0,[0x14259bde0]  ; XMM6 - 1.8
1406D657F COMISS XMM7,XMM0 / 1406D6582 JBE 0x1406d676e    ; XMM7 <= (XMM6 - 1.8) -> return 0
1406D6588 MOVSS XMM8,[0x14256ae1c] / 1406D6591 ADDSS XMM6,XMM8 ; XMM6 + 1.0
1406D6596 COMISS XMM6,XMM7 / 1406D6599 JBE 0x1406d676e    ; (XMM6 + 1.0) <= XMM7 -> return 0
```
**=> `XMM6 - 1.8 < XMM7 < XMM6 + 1.0`, and `|wrap180(homeAngle - kHeading)| < 67.5`.**

**T8 — the look heading and the row (asm `0x1406D659F`–`0x1406D65E6`):**
```
1406D659F MOVSS XMM1,dword ptr [RSI + 0x4]    ; kHeading
1406D65A4 MOVSS XMM0,dword ptr [RSI + 0xc]    ; req word 0x03 (kLookHeading)
1406D65A9 CALL 0x14043a0a0                   ; wrap180(kLookHeading - kHeading)  (a = kHeading)
1406D65BE CALL qword ptr [RAX + 0x60]         ; the +0xAC0 interface, slot 0x60 -> the row id
1406D65CB MOV R14D,dword ptr [RAX + R12*0x4 + 0x25dd988]  ; DAT_1425DD988[row]
1406D65D3 CMP R14D,0xf / 1406D65D7 JA 0x1406d676e         ; row > 15 -> return 0
1406D65DD MOV EAX,0xab54 / 1406D65E2 BT EAX,R14D / 1406D65E6 JNC 0x1406d676e
```
**=> the row's kind must be in the bitmask `0xAB54`** (bits 2, 4, 5, 6, 9, 11, 13, 15).

**T9 — the clip pick and the writes (asm `0x1406D65EC`–`0x1406D6716`):**
```
1406D65EC CALL 0x1406abba0
1406D65F4 MOV dword ptr [RSP + 0xd8],R14D / 1406D6604 MOV dword ptr [RSP + 0xe0],0x536
1406D661D CALL 0x1406abb30                   ; the clip pick (the 0x536 family)
1406D664B CALL 0x1406abd60                   ; the mirror
1406D665A MOVZX ECX,byte ptr [RCX + R12*0x1 + 0x25dd950]  ; DAT_1425DD950[row]
1406D6666 MOV byte ptr [RSI + 0x98],CL       ; req+0x98 = req word 0x26 (the foot flag)
1406D6670 MOV ECX,dword ptr [RSI + 0x7c] / 1406D6679 MOV dword ptr [RSI + 0x90],ECX  ; req word 0x24
1406D6673 MOV EDX,dword ptr [RSI + 0x80] / 1406D6699 MOV dword ptr [RSI + 0x94],EDX  ; req word 0x25
1406D667F MOV ECX,dword ptr [RSI + 0x28] / 1406D6690 MOV dword ptr [RSI + 0x88],ECX  ; req word 0x22
1406D6688 MOVSS XMM0,dword ptr [0x1425c48a0] ; 0.15
1406D66A2 MOV dword ptr [RSI + 0x80],R12D    ; req word 0x20 = the row id
1406D66A9 MOV dword ptr [RSI + 0xd4],0x3f800000 ; req word 0x35 (kPlayback) = 1.0
1406D66B6 MOVSS dword ptr [RSP + 0x30],XMM8 / 1406D66BD MOVSS dword ptr [RSP + 0x28],XMM0
1406D66C3 MOV dword ptr [RSP + 0x20],0x0
1406D66CB CALL qword ptr [R10 + 0x10]         ; the issue
1406D66D8 MOV dword ptr [RSI + 0x68],0x0      ; req word 0x1A (kDwellElapsed) = 0
1406D66DF MOV dword ptr [RSI + 0x6c],0x3e19999a ; req word 0x1B (kDwellLimit) = 0.15
1406D66E6 MOV dword ptr [RSI + 0x78],EAX     ; req word 0x1E = req[0]
1406D66E9 MOV EAX,dword ptr [RSI + 0x4] / 1406D66EC MOV dword ptr [RSI + 0x60],EAX ; req word 0x18
1406D66F1 MOV byte ptr [RSI + 0xe0],0x1      ; req+0x38 (kChangeState) = 1
1406D66F8 MOV dword ptr [RSI + 0xe4],0x16     ; req[0x39] = 0x16 (kTimingStep)  <-- the write
1406D6702 MOV dword ptr [RSI + 0x78],EAX / 1406D6705 MOV byte ptr [RSI + 0xf0],0x0
1406D6716 CALL qword ptr [RAX + 0x8]           ; the +0x4D0 callback
1406D6719 MOV AL,0x1                          ; return 1
```

### §8.3 The complete rule set

`FUN_1406D6290` requests `req[0x39] = 0x16` (kTimingStep) and returns 1 **iff all of**:
1. `kCommandId` (req word `0x16`) is **not** 1, 5, 54 or 61;
2. the side owning the ball has the possession latch (`T+0x255 != 0`);
3. the player id is `< 22`;
4. the class word (req word `0x1F`) is `<= 16` and **not** 3;
5. `kCommandSpeedClass` (req word `0x09`) == 0;
6. `kPrecise` (req word `0x0E`) == 1;
7. `kKind` (req[0]) `<= 2`;
8. the current angle `< 16.0`;
9. `|kTurn| < 22.5`, or (`|kTurn| >= 22.5` and the current angle `< 2.5`);
10. `|kFacingError| < 22.5`;
11. the **opponent** does **not** have the ball;
12. `|wrap180(homeAngle - kHeading)| < 67.5`;
13. `XMM6 - 1.8 < XMM7 < XMM6 + 1.0` with `XMM7 = |sign| * P+0x8`, `XMM6 = |sign| * homeX`;
14. the row's kind is in the bitmask `0xAB54`.

Otherwise it returns 0 and writes nothing.

**The writes when it fires:** `req[0x1A]=0`, `req[0x1B]=0.15`, `req[0x18]=kHeading`, `req[0x1E]=req[0]`,
`req[0x20]=the row id`, `req[0x22]=kDesiredSpeedKmh`, `req[0x24]=the class word`,
`req[0x25]=the old req word 0x20`, `req+0x26]=DAT_1425DD950[row]`, `req[0x35]=1.0`,
`req+0x38]=1`, **`req[0x39]=0x16`**, `req+0x3C]=0`, `req+0xF0]=0`.

### §8.4 The entry from `0x1406C4E51` (in `FUN_1406C4C10`, state 4 `kStepLoop`)

```
1406C4E10 CMP byte ptr [RDI + 0x3f],R15B    ; req+0x3F = req word 0x0F
1406C4E14 JZ 0x1406c4e38
1406C4E16 CMP EAX,0x3
1406C4E19 JGE 0x1406c4e38
1406C4E1B TEST ESI,ESI
1406C4E1D JNZ 0x1406c4e38
1406C4E1F COMISS XMM1,XMM10
1406C4E23 JBE 0x1406c4e38
1406C4E25 XOR R8D,R8D
1406C4E28 MOV RDX,RDI / 1406C4E2B MOV RCX,R14
1406C4E2E CALL 0x1406ce820                   ; the re-plan (req[0x39] = 7)
1406C4E33 JMP 0x1406c6438
1406C4E38 MOV RDX,RDI / 1406C4E3B MOV RCX,R14
1406C4E3E CALL 0x1406d38d0                   ; the shared STEP_TURN helper
1406C4E43 TEST AL,AL / 1406C4E45 JNZ 0x1406c6438   ; it fired -> leave
1406C4E4B MOV RDX,RDI / 1406C4E4E MOV RCX,R14
1406C4E51 CALL 0x1406d6290                   ; <-- TIMING_STEP
1406C4E56 TEST AL,AL / 1406C4E58 JNZ 0x1406c6438   ; it fired -> leave
```
**=> `kTimingStep` is requested from `kStepLoop` only when**
(a) the re-plan condition at `0x1406C4E10`–`0x1406C4E23` does **not** hold
(`req+0x3F != 0` **or** `EAX >= 3` **or** `ESI != 0` **or** `XMM1 <= XMM10`), **and**
(b) `FUN_1406D38D0` returned **false**, **and**
(c) all 14 of §8.3's rules hold.

When (c) holds, `FUN_1406D6290` writes `req[0x39] = 0x16` itself — the caller does **not** write it.

## §9 STEP_TURN_STEP `FUN_1406C8D80` — every branch and gate (CONFIRMED)

`FUN_1406C8D80(param_1, param_2)` — `param_1` = the playback host, `param_2` = the request block.
In the asm: `RSI` = the request, `RBP` = the host, `R14` = the `+0xAB8` interface,
`R15` = the `+0xAC0` interface, `RBP+0x4D0` = the callback object.

### §9.1 Block S1 — the setup (asm `0x1406C8D80`–`0x1406C8F70`)

The head reads the live state through the `+0xAB8` interface:
```
1406C8D8B MOV iVar9,[plVar1 + 0x38]              ; the interface slot 0x38 -> iVar9 (the frame count)
1406C8D9B FUN_1406B1F00(uVar16,*puVar17,1.0,0)  -> iVar10
1406C8DBA FUN_1406B1D00(uVar16,*puVar17,1.0,0)  -> fVar11
1406C8DC9 FUN_1406B1C70(uVar16,*puVar17,uVar6,0) -> fVar24
1406C8DDA FUN_1406AF620(uVar16,*puVar17,1.0)     -> local_res8[0]
1406C8DE5 FUN_1406AFBC0(uVar16,*puVar17,5)       -> iVar12
   if (0 < iVar12) { FUN_1406AF8E0(uVar16,*puVar17,1.0,iVar12-1);
                      local_res8[0] = FUN_1406AF8E0(uVar16,*puVar17,1.0,0); }
```
`fVar11` is then capped by the clip length:
```
if ((int)local_res10 < (int)fVar11) fVar11 = local_res10;
```
**=> `fVar11` is the number of frames the clip still has.**

### §9.2 Block S2 — the "the turn is finished" block (asm `0x1406C8F70`–`0x1406C92F7`)

The gate:
```
cVar7 = FUN_141EB3320(plVar2)                 ; the +0x4D0 callback object's flag
if (cVar7 != 0) -> skip S2
if (iVar10 > iVar9) -> skip S2
iVar10 = iVar10 + (int)fVar11 * 2
if (iVar9 < iVar10 / 3 + (iVar10 >> 31) + ...) -> skip S2
```
**=> S2 runs when the callback flag is clear, `iVar10 <= iVar9`, and
`iVar9 < (iVar10 + 2*fVar11)/3`.**

Inside S2:
```
fVar26 = (float)param_2[0x17]                 ; req word 0x17 (kRecordStampA)
uVar20 = (uint)((float)((int)fVar11 - iVar9) / fVar28 + 0.5)   ; fVar28 = P+0x2468
fVar25 = FUN_14043A0A0(...)                   ; the wrap180 pair
if (|fVar25| < 90.0) fVar26 = FUN_140A92950() ; 0x14259BE5C = 90.0
fVar31 = fVar26 + DAT_1425DD660[param_2[0x20]]
fVar31 = wrap360(fVar31)
fVar27 = FUN_14043A0A0(...)                   ; the second wrap180 pair
if (|fVar27| / (float)(int)uVar20 < 45.0) {   ; 0x14259BE30 = 45.0
    param_2[0x3D] = (uint)fVar26;             ; req word 0x0F
    *(byte*)(param_2 + 0x3C) = 1;             ; req byte 0x3C
    param_2[0x3E] = uVar20;                   ; req word 0x0F + 1
    *(byte*)(param_2 + 0x3F) = 0;             ; req byte 0x3F
    param_2[0x18] = (uint)fVar31;              ; req word 0x06 (kTurn)
    /* the mirror fix-up */
    if (90.0 < |fVar24|) {
        s1 = sign(fVar24); s2 = sign(fVar27);
        if (s1 != s2) { uVar6 = param_2[0x3F];
            if (22.5 < |fVar27|) uVar6 = 1;    ; 0x14259D618 = 22.5
            *(byte*)(param_2 + 0x3F) = uVar6; }
    }
}
```

### §9.3 Block S3/S4 — the wrap180 pairs and the rate adjustment

The first pair (asm `0x1406C8F80`–`0x1406C9100`):
```
fVar24 = (float)param_2[0x18]   ; req word 0x06 (kTurn)
fVar28 = (float)param_2[1]      ; req word 0x01 (kHeading)
fVar24 = wrap360(fVar24); fVar28 = wrap360(fVar28)
fVar24 = fVar28 - fVar24        ; then the +-180 normalisation
```
The second pair uses `XMM0` = req word `0x01` and `XMM1` = req word `0x03` (kLookHeading).

The rate adjustment (asm `0x1406C9100`–`0x1406C91C0`):
```
if (param_2[0xE] == 0 && 0 < (int)param_2[0x1F]) {      ; kPrecise == 0 and the class > 0
    fVar24 = (float)param_2[0x11]                        ; req word 0x11 (kDistToTarget)
    fVar25 = (2.0 < fVar24) ? ((fVar24 - 2.0) * 0.5) : 0.0;  /* 0x142565604=2.0, 0x1425656C0=4.0, 0x142565600=0.5 */
    fVar32 = fVar25 * 22.5 + 45.0;                       /* 0x14259D618=22.5, 0x14259BE30=45.0 */
}
```

### §9.4 Block S5 — the three-way branch (asm `0x1406C91C0`–`0x1406C9439`)

```
if (param_2[0x16] != 0x11) {              ; kCommandId != 0x11
    fVar25 = rand();
    if (iVar9 < (int)((fVar25 * 6.0) / 60.0 + 0.5)) {   ; 0x14259BDEC=6.0, 0x14259A764=60.0
        /* S5a */
        if (param_2[9] == 0xFFFFFFFF) goto LAB_1406C92F8;      ; -> the STOP/IDLE exit
        if (56.25 <= fVar28) {                                 ; 0x14259BE3C = 56.25
            if (13.5 <= fVar28) { param_2[0x1F] = 2; param_2[0x1D] = 0x41800000; }  /* 16.0 */
            else               { param_2[0x1F] = 1; param_2[0x1D] = 0x41300000; }  /* 11.0 */
        } else {
            param_2[0x1F] = 0; param_2[0x1D] = 0x40A00000;    /* 5.0 */
        }
        if (FUN_1406D38D0(param_1, param_2) != 0) return;     /* the shared helper claimed it */
        goto LAB_1406C92F8;                                    /* -> the STOP/IDLE exit */
    }
    if ((fVar28 < 11.25) && ((int)param_2[0x1F] < 3) && (param_2[9] == 3)) {
        /* S5b: the three special kinds, on the interface's kind word */
        if (*(short*)(interface slot 0x18) == 0x35)  { ... }  /* 0x1406C9363 */
        else if (*(short*)(interface slot 0x18) == 0x39)  { ... }  /* 0x1406C943F */
        else if (*(short*)(interface slot 0x18) == 0xC51) { ... }  /* 0x1406C9487 */
    }
}
```
**=> S5a is gated by `kCommandId != 0x11` AND a 10% random roll
(`iVar9 < (int)(rand*6.0/60.0 + 0.5)`).**
**=> S5b is gated by `fVar28 < 11.25` (`0x1425CC680`), the class word `< 3`, and
`kCommandSpeedClass` (req word `0x09`) == 3**, then on the interface's kind word.

### §9.5 S5b's writes — the `req[0x39] = 0xF` exit (asm `0x1406C93A9`–`0x1406C9416`)

The per-kind setup before the issue:
- kind `0x35` (asm `0x1406C9370`): `fVar28 = (float)(iVar9 + 1) / rand();`, `uVar16 = 0x3F`,
  `uVar29 = DAT_142588200` (0.1);
- kind `0x39` (asm `0x1406C943F`): the same shape, `uVar16 = 0x4E`, `uVar29 = DAT_142598578` (0.2);
- kind `0xC51` (asm `0x1406C9487`): `uVar16 = 0xC6D`, `uVar29 = DAT_142598578` (0.2).
`uVar6` is the mirror: `uVar6 = (interface slot 0x30) == 0` (`SETZ` at `0x1406C93A5`).

The issue and the write:
```
LAB_1406C93A9:
  (**(code **)(lVar23 + 0x10))(param_1 + 0xac0, param_2, uVar16, uVar6, 0, DAT_142588200, param_2[0x35], fVar28);
  param_2[0x24] = param_2[0x1F];
  param_2[0x22] = param_2[0x1D];
  param_2[0x1E] = *param_2;
  param_2[0x18] = param_2[1];
  param_2[0x20] = param_2[8];
  param_2[0x1F] = 3;
  param_2[0x1D] = 0x41E00000;      ; 30.0
  *(byte*)(param_2 + 0x38) = 1;    ; req+0x38 (kChangeState)
1406C9416: MOV dword ptr [RSI + 0xe4],0xf ;   ; req[0x39] = 0xF (kStepTurnRun)
```

### §9.6 Block S6 — the STOP/IDLE exit `LAB_1406C92F8` (asm `0x1406C92F8`–`0x1406C932D`)
```
1406C92F8: param_2[0x25] = param_2[0x20];
           uVar13 = (**(code **)(*(param_1 + 0xac0) + 0x60))();   ; the row id
           param_2[0x20] = uVar13;
1406C9328: CALL 0x1406d5d60                     ; the shared STOP/IDLE requester
1406C932D: JMP 0x1406c9dce
```
**=> this is where `kStepTurnStep` reaches STOP** — the class word decides IDLE vs STOP.

### §9.7 Block S7 — the "still turning" path (asm `0x1406C94D3`–`0x1406C9645`)

```
if (iVar9 < (int)local_res8[0]) return;        ; the clip is not long enough -> bail
iVar10 = (interface slot 0x28)();
if (iVar10 != 0x4A &&
    (((param_2[9] == 0xFFFFFFFF && param_2[0xE] == 1) || (fVar32 < fVar28)))) {
    if ((param_2[9] != 0xFFFFFFFF) && (param_2[0xE] == 0) && ((int)param_2[0x1F] < 1))
        goto LAB_1406C95D6;                    ; -> the re-plan
    FUN_141E532D0();
    if ((param_2[9] != 0xFFFFFFFF) && (0 < (int)param_2[0x1F])) {
        fVar28 = FUN_14042D470();              ; the current angle
        if (*(short*)(interface slot 0x18) == 0x4D || 2.5 <= fVar28) {   /* 0x14259BDE4 = 2.5 */
            if (|param_2[6]| <= 135.0) {       /* 0x14259BE60 = 135.0 */
                if (8.0 <= fVar28) goto LAB_1406C95FE;   /* 0x14259BDF4 = 8.0 */
                param_2[0x1F] = 0; param_2[0x1D] = 0x40A00000;   /* 5.0 */
            } else {
                if (fVar28 < 8.0) goto LAB_1406C95C8;
LAB_1406C95FE:
                if (13.5 <= fVar28) { param_2[0x1F] = 2; param_2[0x1D] = 0x41800000; }  /* 16.0 */
                else               { param_2[0x1F] = 1; param_2[0x1D] = 0x41300000; }  /* 11.0 */
            }
            if (FUN_1406D38D0(param_1, param_2) != 0) return;
            if (5.5 <= fVar28) goto LAB_1406C9645;    /* 0x1425C8378 = 5.5 */
        }
LAB_1406C95C8:
        param_2[0x1F] = 0; param_2[0x1D] = 0x40A00000;    /* 5.0 */
LAB_1406C95D6:
        FUN_1406CE820(param_1, param_2, 0);    ; the re-plan (req[0x39] = 7)
        return;
LAB_1406C9645:
    if (|param_2[7]| < 67.5) {              /* 0x1425CCA34 = 67.5 */
        FUN_14043A0A0();
        param_2[0x25] = param_2[0x20];
        uVar13 = (**(code **)(*(param_1 + 0xac0) + 0x60))();
        param_2[0x20] = uVar13;
        FUN_1406D5D60(param_1, param_2);      ; <-- the 2nd STOP/IDLE exit
        return;
    }
}
```

### §9.8 Block S8 — the root-facing heading write (asm `0x1406C9645`–`0x1406C9700`)

```
if ((int)fVar11 <= iVar9) {
    *(byte*)(param_2 + 0x3C) = 0;
    (**(code **)(*plVar2 + 8))(plVar2, *(param_1 + 0xA08));
    fVar24 = (float)param_2[1];             ; kHeading
    if (param_2[0x20] == 7)      fVar24 = fVar24 + 90.0;   /* 0x14259BE5C */
    else if (param_2[0x20] == 2) fVar24 = fVar24 - 90.0;
    else                         fVar24 = fVar24 - 180.0;  /* 0x142598580 */
    fVar24 = wrap360(fVar24);
    fVar34 = fVar24 - P+0x20;              /* the host's facing */
    fVar34 = wrap180(fVar34);
    fVar32 = (float)param_2[3];            /* kLookHeading */
    fVar32 = wrap360(fVar32);
    fVar24 = fVar24 - fVar32;              /* then the +-180 normalisation */
    fVar33 = (fVar24 > 22.5) ? ((fVar24 - 22.5) / 22.5) : 0.0;   /* 0x14259D618 */
    fVar33 = fVar33 * 90.0 + 90.0;         /* 0x14259BE5C */
    fVar24 = (float)FUN_141E532D0();
    fVar33 = fVar33 / fVar24;              /* a random fraction */
    fVar24 = (fVar34 <= fVar33) ? max(fVar33, -fVar33) : fVar34;
    fVar24 = fVar24 + P+0x20;
    fVar24 = wrap360(fVar24);
    *(byte*)(param_2 + 0x3A) = 1;          ; req byte 0x3A (kRootFacing)
    if ((float*)(param_2 + 0x3B) != local_res8) param_2[0x3B] = (uint)fVar24;  /* kRootFacingAngle */
}
```
**=> the root-facing angle is `P+0x20 + a random fraction of (90 + 90*t)`, where `t` is the
normalised residual turn, and the bucket offset is `+90` for the bucket 7, `-90` for the bucket 2,
`-180` otherwise.**

### §9.9 Block S9 — the final gate and the `req[0x39] = 4` exit (asm `0x1406C9700`–`0x1406C9DC4`)

The five-way gate:
```
bVar3 = (uVar20 == 0 && param_2[9] == 3)                                     /* kPrecise == 0 and class == 3 */
     || ((int)param_2[9] < 1 && |wrap180(...)| < fVar28)
     || (45.0 < |param_2[6]| && |wrap180(...)| < fVar24)
     || (uVar20 == 1 && fVar24 < |param_2[7]| && (int)fVar11 <= iVar9)
     || (uVar20 == 0 && 90.0 < |param_2[5]| && (int)fVar11 <= iVar9);
if (iVar9 < (int)local_res10 && !bVar3) return;
```
(`uVar20 = param_2[0xE]` = `kPrecise`; `param_2[5]` = req word `0x05` = `kLookDelta`;
`param_2[6]` = word `0x06` = `kTurn`; `param_2[7]` = word `0x07` = `kFacingError`.)

Then the final block:
```
(**(code **)(*plVar2 + 8))(plVar2, *(param_1 + 0xA08));
fVar34 = FUN_14043A0A0();                  ; the wrap180 pair
if ((int)fVar11 <= iVar9) {
    uVar20 = param_2[0x20];
    if ((((uVar20 != 9) && (uVar20 != 4)) || (|fVar34| <= 135.0)) && (uVar20 == 7 || uVar20 == 2)) &&
        (fVar24 < |fVar34| && |fVar34| < 135.0)) {
        FUN_14043A0A0(0x140000000, fVar28 + (float)DAT_1425DD660[uVar20]);   /* the extra wrap */
    }
}
uVar14 = (**(code **)(*(param_1 + 0xac0) + 0x60))();    /* the row id */
uVar20 = param_2[0x1F];                                /* the class */
fVar25 = (int)uVar20 < 2 ? 5.0 : 11.0;                 /* 0x14259222C / 0x14259BDFC */
uVar21 = param_2[9];
if ((int)uVar21 < 3) uVar21 = max(uVar21, (uVar20 > 1) ? 1 : 0); else uVar21 = 2;
iVar9 = (int)(float)param_2[10];                      /* kDesiredSpeedKmh */
iVar9 = (iVar9 < 0x11) ? max(iVar9, (int)fVar25) : 0x10;
uVar20 = *param_2;
if (*param_2 < 2) { uVar20 = 2; local_res10 = 2.0; }
else if (*param_2 == 4) { uVar20 = 5; local_res10 = 5.0; }
fVar28 = (float)DAT_1425DD988[uVar14];
fVar11 = *(float *)(&DAT_1425DD9B0 + (uVar20 + uVar21*8)*4);
/* the clip pick + mirror */
uVar8 = FUN_1406ABB30(uVar16, local_res18, local_res8, 0);
cVar7 = FUN_1406ABD60(uVar16, local_res20, local_res18);
*(byte*)(param_2 + 0x26) = DAT_1425DD950[uVar14];
param_2[0x24] = param_2[0x1F];
param_2[0x22] = param_2[0x1D];
param_2[0x25] = param_2[0x20];
param_2[0x1D] = (uint)fVar25;
param_2[0x1F] = uVar21;
param_2[0x20] = uVar14;
param_2[0x35] = FUN_1406BA070(param_1);
fVar11 = (float)FUN_1406B97D0(local_res8[0]._0_2_);
/* the kind-0/1 remap block (FUN_1406AFE20 with the 0->2, 1->3, 2->0, 3->1 map) */
(**(code **)(*(param_1 + 0xac0) + 0x10))(param_1 + 0xac0, param_2, (int)sVar5, cVar7, 0, DAT_1425DDF50, param_2[0x35], fVar11);
param_2[0x1E] = (uint)local_res10;
param_2[0x1A] = 0;
param_2[0x1B] = 0;
*(byte*)(param_2 + 0x38) = 1;            ; req+0x38 (kChangeState)
1406C9DC4: MOV dword ptr [RSI + 0xe4],0x4 ;   ; req[0x39] = 0x4 (kStepLoop)
```
The final class ladder (asm `0x1406C9B20`–`0x1406C9B50`):
```
if (|fVar34| <= fVar24) {
    if (fVar35 < 8.0 && 1 < (int)uVar21) { uVar21 = 1; fVar25 = 11.0; }   /* LAB_1406C9B50 */
} else if (8.0 <= fVar35) {
    if (fVar35 < 13.5) goto LAB_1406C9B50;
    uVar21 = 2; fVar25 = 16.0;
} else { uVar21 = 0; fVar25 = 5.0; }
```
where `fVar35 = (rand*60*60/1000) * |ball|` — a distance-scaled random.

### §9.10 The STEP_TURN_STEP branch map, in order

1. **S1** the setup — the clip length `fVar11`, the angle `fVar24`, the interface counters.
2. **S2** the "the turn is finished" block — gated on the callback flag, `iVar10 <= iVar9` and
   `iVar9 < (iVar10 + 2*fVar11)/3`; writes `req[0x0F]`, `req+0x3C]`, `req+0x3F]`, `req[0x06]`.
3. **S3/S4** the wrap180 pairs and the rate adjustment.
4. **S5a** the random 10% branch → the class ladder → `FUN_1406D38D0` → the STOP/IDLE exit
   (`0x1406C9328`).
5. **S5b** the three special kinds (`0x35`, `0x39`, `0xC51`) → **`req[0x39] = 0xF`** (`0x1406C9416`).
6. **S7** the "still turning" path → the class ladder → `FUN_1406D38D0` → the re-plan, or the
   **STOP/IDLE exit** (the 2nd `FUN_1406D5D60`, in the `LAB_1406C9645` block).
7. **S8** the root-facing heading write (`req+0x3A`, `req[0x0E]`).
8. **S9** the final gate → the clip pick → **`req[0x39] = 4`** (`0x1406C9DC4`).

**The three exits of STEP_TURN_STEP:** `0xF` (kStepTurnRun) at `0x1406C9416`; `4` (kStepLoop) at
`0x1406C9DC4`; and the shared STOP/IDLE requester at `0x1406C9328` and in the `LAB_1406C9645` block.

## §10 `FUN_1406C4C10` — the inner sub-conditions of the 0xD and 0xF branches (CONFIRMED)

### §10.1 The `req[0x39] = 0xD` (kStepToRun) branch

**The entry gate (asm `0x1406C55CA`–`0x1406C5638`).** The branch is reached with the class word 0:
```
1406C55D2 MOVSS XMM6,dword ptr [0x1425cb30c]   ; 112.5
1406C55DA JNZ 0x1406c5940                      ; the class word != 0 -> the other branch
1406C55E0 MOVSS XMM5,dword ptr [RDI + 0x18]    ; req word 0x06 (kTurn)
1406C55E5 MOVAPS XMM1,XMM5 / 1406C55E8 ADDSS XMM1,XMM4
1406C55EC..0x1406C55F3                        ; |kTurn + fVar38|
1406C55F7 COMISS XMM6,XMM3 / 1406C55FA JNC 0x1406c5638   ; 112.5 <= |kTurn + fVar38| -> the other branch
1406C55FC..0x1406C5603                        ; |kTurn|
1406C5607 MOVSS XMM0,dword ptr [0x1425cca34]   ; 67.5
1406C560F COMISS XMM0,XMM1 / 1406C5612 JBE 0x1406c5940   ; 67.5 <= |kTurn| -> the other branch
1406C5618 MOVSS XMM2,dword ptr [0x14259be5c]   ; 90.0
1406C5620..0x1406C562B                        ; |fVar38|
1406C562F COMISS XMM2,XMM1 / 1406C5632 JC 0x1406c5940    ; 90.0 < |fVar38| -> the other branch
```
**=> the 0xD branch needs: the class word == 0, `|kTurn + fVar38| < 112.5`, `|kTurn| < 67.5`,
and `|fVar38| <= 90.0`.**

**The two wrap180 pairs (asm `0x1406C559F`–`0x1406C55C5`):**
```
1406C559F MOVSS XMM12,dword ptr [RDI + 0x4]   ; req word 0x01 (kHeading)
1406C55A5 MOVSS XMM6,dword ptr [RDI + 0x8]    ; req word 0x02 (kFacing)
1406C55AA MOVAPS XMM1,XMM12 / 1406C55AE MOVAPS XMM0,XMM6
1406C55B1 CALL 0x14043a0a0                    ; wrap180(kHeading - kFacing)   (a = kFacing, b = kHeading)
1406C55B6 MOVSS XMM7,dword ptr [RDI + 0x10]   ; req word 0x04 (kBodyDirection)
1406C55BB MOVAPS XMM11,XMM0
1406C55BF MOVAPS XMM1,XMM7 / 1406C55C2 MOVAPS XMM0,XMM6
1406C55C5 CALL 0x14043a0a0                    ; wrap180(kBodyDirection - kFacing)  (a = kFacing, b = kBodyDirection)
```
**=> `fVar23 = wrap180(kHeading - kFacing)` and `fVar38 = wrap180(kBodyDirection - kFacing)`.**
(The decompile's `FUN_14043a0a0(iVar3, iVar10)` and `FUN_14043a0a0(iVar3, iVar4)` agree, with
`iVar3 = param_2[2]` = kFacing, `iVar10 = param_2[1]` = kHeading, `iVar4 = param_2[4]` =
kBodyDirection — the two calls differ only in the **second** argument, so the decompile's argument
order is right here.)

**The re-plan escape (asm `0x1406C5638`–`0x1406C5678`):**
```
1406C5638..0x1406C5643                        ; |fVar23|
1406C5647 COMISS XMM8,XMM1 / 1406C564B JBE 0x1406c567d   ; XMM8 <= |fVar23| -> skip
1406C564D..0x1406C5654                        ; |kTurn|
1406C5658 COMISS XMM0,XMM8 / 1406C565C JBE 0x1406c567d   ; |kTurn| <= XMM8 -> skip
1406C565E MOV RDX,RDI / 1406C5661 MOV RCX,R14
1406C5664 CALL 0x1406b70d0
1406C5669 TEST AL,AL / 1406C566B JZ 0x1406c567d         ; not set -> skip
1406C566D MOV RDX,RDI / 1406C5670 MOV RCX,R14
1406C5673 CALL 0x1406d6780                    ; the third shared exit
1406C5678 JMP 0x1406c6415
```
**=> the 0xD branch bails to `FUN_1406D6780` when `|fVar23| > XMM8` AND `|kTurn| > XMM8` AND
`FUN_1406B70D0` returns true.** (`XMM8` is the 45.0-scale angle of §10.3.)

**The writes (asm `0x1406C567D`–`0x1406C592A`):**
```
1406C567D COMISS XMM11,XMM6 / 1406C5681 MOVSS dword ptr [RDI + 0x5c],XMM11
1406C5689 MOVSS XMM0,dword ptr [0x1425dbdbc]   ; -0.0
1406C5691 MAXSS XMM0,XMM11 / 1406C5696 MOVAPS XMM6,XMM0
1406C5699 CMP dword ptr [RDI + 0x24],0x0        ; req word 0x09 (kCommandSpeedClass)
1406C569D MOVSS dword ptr [RDI + 0x5c],XMM6
1406C56A2 ADDSS XMM6,dword ptr [RDI + 0x8]      ; + kFacing
1406C56A7 MOVSS dword ptr [RDI + 0x60],XMM6    ; req word 0x18
1406C56AC MOVZX ECX,byte ptr [RDI + 0x7c]      ; the class word
1406C56B0 JLE 0x1406c56c3
1406C56B2 MOVZX EAX,CL / 1406C56B5 CMP CL,0x1 / 1406C56B8 MOV ECX,0x1 / 1406C56BD CMOVL EAX,ECX
1406C56C0 MOVZX ECX,AL                        ; the class, clamped to >= 1
1406C56C3 MOVSS XMM0,dword ptr [RDI + 0x28]    ; req word 0x0A (kDesiredSpeedKmh)
1406C56C8 COMISS XMM0,dword ptr [0x14259222c]  ; 5.0
1406C56CF MOVSS XMM3,dword ptr [RDI + 0x74]    ; req word 0x1D
1406C56D4 JBE 0x1406c56e5
1406C56D6 MOVSS XMM0,dword ptr [0x14259bdfc]   ; 11.0
1406C56DE MAXSS XMM0,XMM3 / 1406C56E2 MOVAPS XMM3,XMM0
1406C56E5 MOV EAX,dword ptr [RDI] / 1406C56E7 MOVZX EDX,byte ptr [RDI + 0x2c]
1406C56EB MOVSX R8D,CL
1406C56F2 MOV dword ptr [RSP + 0x20],EAX
1406C56F6 CALL 0x1406b9ff0                    ; the row pick
1406C56FB MOVSS XMM1,dword ptr [RDI + 0x18] / 1406C5700 MOVSS dword ptr [RDI + 0x128],XMM0
```
then the rate trim and the final block (asm `0x1406C5835`–`0x1406C592A`):
```
1406C5835 MOV byte ptr [RSP + 0x40],0x0
1406C5843 MOV EDX,EAX
1406C5845 MOVZX EAX,byte ptr [RDI + 0x98]    ; req+0x98 = req word 0x26
1406C5860 CALL 0x1406ba2f0
1406C5865 MOV ESI,dword ptr [RDI + 0x24]      ; req word 0x09
1406C586E MOV dword ptr [RDI + 0xdc],R8D / 1406C5875 MOV byte ptr [RDI + 0x140],R8B
1406C587C MOV dword ptr [RDI + 0x68],R9D     ; req word 0x1A
1406C5880 MOV dword ptr [RDI + 0x6c],0x3e888889 ; req word 0x1B = 0.26666668
1406C5887 MOV dword ptr [RDI + 0xb4],R9D     ; req word 0x2D
1406C588E MOV dword ptr [RDI + 0xb8],0x3e888889 ; req word 0x2E = 0.26666668
1406C5898 CMP ESI,0x3 / 1406C589B JLE 0x1406c58a3
1406C589D LEA ESI,[R9 + 0x3] / 1406C58A1 JMP 0x1406c58a9
1406C58A3 TEST ESI,ESI / 1406C58A5 CMOVS ESI,R9D
1406C58A9 MOVSXD RAX,ESI / 1406C58AC MOV dword ptr [RDI + 0x24],ESI   ; req word 0x09 = clamp(0..3)
1406C58AF LEA RDX,[RAX + R8*0x4] / 1406C58B3 LEA RAX,[0x140000000]
1406C58BA MOV EAX,dword ptr [RAX + RDX*0x4 + 0x25ddcf0]               ; DAT_1425DDCF0[...]
1406C58C1 MOV dword ptr [RDI + 0xc0],EAX    ; req word 0x18
1406C58C7 CMP R8D,0xc / 1406C58CB JA 0x1406c58e1
1406C58CD MOV EAX,0x1028 / 1406C58D2 BT EAX,R8D / 1406C58D6 JNC 0x1406c58e1
1406C58D8 MOV byte ptr [RDI + 0xc4],0x1     ; req word 0x19
1406C58DF JMP 0x1406c5908
1406C58E1 CMP R8D,0xb / 1406C58E5 JA 0x1406c58fb
1406C58E7 MOV EAX,0x814 / 1406C58EC BT EAX,R8D / 1406C58F0 JNC 0x1406c58fb
1406C58F2 MOV byte ptr [RDI + 0xc4],R9B
1406C58F9 JMP 0x1406c5908
1406C58FB MOVZX EAX,byte ptr [RDI + 0x98] / 1406C5902 MOV byte ptr [RDI + 0xc4],AL
1406C5908 MOV EAX,dword ptr [RDI] / 1406C590A MOV qword ptr [RDI + 0xd4],0x3f800000
1406C5915 MOV qword ptr [RDI + 0xc8],0x3e888889
1406C5920 MOV dword ptr [RDI + 0x78],EAX / 1406C5923 MOV dword ptr [RDI + 0x80],R9D
1406C592A: MOV dword ptr [RDI + 0xe4],0xd   ; req[0x39] = 0xD (kStepToRun)
```
**=> the mirror byte `req[0x19]` is `1` when the style is in the `0x1028` bitmask (styles 0..12),
`0` when it is in the `0x814` mask (styles 0..11), and otherwise the old `req+0x98]` byte.**

### §10.2 The `req[0x39] = 0xF` (kStepTurnRun) branch

**The entry (asm `0x1406C5C3E`–`0x1406C5D30`).** The 0xF branch is the tail of the same `if` body
as the 0xD branch — it is reached when the 0xD branch's class ladder did **not** claim the request
(the `FUN_1406D38D0` helper returning true diverts to the 0xD path, and the other path reaches the
0xF block). The decompile's 0xF block (lines 880-892) is:
```
iVar29 = clamp(param_2[9], 0, 2)                  ; the class, clamped to 0..2
fVar22 = (float)param_2[10];                       ; kDesiredSpeedKmh
if (fVar22 <= 16.0) fVar22 = 5.0; else fVar22 = max(fVar22, 11.0);
   ; 0x14259BE10 = 16.0, 0x14259222C = 5.0, 0x14259BDFC = 11.0
iVar31 = FUN_1406B9FF0(param_1, (char)param_2[0xB], iVar29, fVar22, *param_2);
param_2[0x4A] = iVar31;
fVar23 = (float)(**(code **)(*plVar1 + 0x18))(plVar1);
fVar28 = (float)param_2[0x4A];
if (fVar28 <= fVar23 && 0.0 <= fVar28) fVar23 = fVar28;
param_2[0x4A] = (int)fVar23;
param_2[0x24] = param_2[0x1F];
param_2[0x22] = param_2[0x1D];
param_2[0x25] = param_2[0x20];
param_2[0x20] = 0;
param_2[0x1D] = (int)fVar22;
param_2[0x1F] = iVar29;
param_2[0x1E] = *param_2;
1406C5D30: MOV dword ptr [RDI + 0xe4],0xf   ; req[0x39] = 0xF (kStepTurnRun)
```
**=> the 0xF branch: the class is `clamp(req word 0x09, 0, 2)`, the speed band is
`kDesiredSpeedKmh` clamped to `[5.0, 11.0]` unless it is already `<= 16.0` (then it becomes `5.0`),
the row is picked by `FUN_1406B9FF0`, and `req[0x4A]` is then clamped to the interface's speed cap.**

### §10.3 The `req[0x39] = 5` (kStepLoopChange) branch — the fall-through default

The 0x5 branch is taken when the action conditions fail. Its gate (decompile 505-525, asm
`0x1406C5638` onward): the 0xD/0xF bodies are guarded by
```
if ( ((req[0x1F] < 0)
      || (((iVar31 < 3) && (((iVar31 < 2) || (req[0] == 3)) || (req[0] == 6))) || (iVar29 != 0)))
     || (((1 < req[0x20]-3U) && (1 < req[0x20]-8U))
         || ((fVar38 <= fVar28) || (fVar24 * 22.5 + fVar21 <= fVar26))) ) ) {
    if ((float)req[0x1A] < (float)req[0x1B] * 0.25) goto LAB_1406c5eba;   /* -> the 0x5 arm */
```
**=> the 0x5 arm is the default: it is taken whenever the request is "not in an action state"
(the class word is 0 and the kind is 3 or 6, or the class is out of the 0..2 range, or the bucket
is outside {3,4,8,9}, or the speed/angle test fails) — and the dwell is not yet up.**

## §11 Open items

1. `FUN_1406D6780` — the third shared exit helper; which `req[0x39]` value it writes is **OPEN**.
2. `FUN_1406BA900`'s arm conditions for the `req[0x39] = 4` write at `0x1406BB0C8` — **OPEN**.
3. The live confirmation of the census is **OPEN**; this report is static only.
4. `FUN_1406C4C10`'s **0x5 (kStepLoopChange)** branch body — the gate is given in §10.3 but the
   branch's own writes (the `FUN_1406ABB30` / `FUN_1406ABD60` clip pick and the six state words) are
   **not** reduced here; only the 0xD and 0xF branches were in scope for this pass.
5. `FUN_1406C8D80`'s S2 block's two `wrap180` pairs — the **operand pairs** are not named in §9.2
   (the decompile drops them the same way it does in `FUN_1406C3470`); the gate and the writes are
   given, the pairs are **OPEN**.

## §12 Method

- Bridge decompiles (`dec/FUN_*.c`) and disassembly (`dec/dis_*.txt`) for every handler named here;
  ~30 fetches (round 1) + ~25 more (round 2) against `http://127.0.0.1:8080`.
- Every constant is read by script from `~/ghidra-projects/scratch/Unpacker/pes_full.bin`
  (`readf.py`) and is paired with the instruction VA and the operand VA the disassembly shows.
- The state→handler map comes from the jump table at `0x1406BB50C` (see
  `pes-re-runloop-arms.md` §4).
- The `req[0x39]` writer sites come from the disp32 scan `w39.py` (see
  `pes-re-runloop-arms.md` §5).
- No GF code changed, no GF build/test run, no PES write/pin/input. The port was read only.

### Self-check

`<workdrive>/gf-TeamDecode-scratch/check_step.py`, run against this file:
```
operand-pair rows in the report: 8
  -> operand pairs checked
  -> 38 constant rows checked
req[0x39] writer rows: 25
  -> writer rows checked
cited code VAs not inside a disassembled instruction: 0
  -> 19 branch pairs checked
  -> 9 STOP-exit call sites checked

TOTAL mismatches: 0
```
The checker verifies, against the **image bytes and the disassembly** (never against the report's
own text):
1. every `FUN_14043A0A0` operand pair — the cited instruction VA must be a real
   `MOVSS <reg>,[RDI+off]`, the register and offset must match what the report names, and the
   register must not be reloaded between the load and the call;
2. every constant row in §0b — the bits and the decimal must both match the image;
3. every `req[0x39]` writer row (§5b and §7.1) — the site must really be a `C7 /0` store to
   displacement `0xE4` carrying the quoted immediate;
4. every quoted code VA must lie inside a disassembled instruction;
5. every quoted **branch pair** — the instruction at the source VA must really be a branch, and it
   must target the quoted destination;
6. every **STOP-exit call site** in §7.2 — the instruction at the quoted VA must really be a
   `CALL 0x1406D5D60`.

**Mutation-tested — all ten fire:**
- a constant value changed (`0.75` → `0.76`) → `BAD 0x14259bdd4: value claimed 0.76 actual 0.75 (0x3F400000)`;
- a writer value changed (`0x4` → `0x5` at `0x1406C13BD`) → `BAD 0x1406c13bd: report says 0x5, image says 0x4`;
- an operand offset changed (`[RDI+0x8]` → `[RDI+0x18]`) → `BAD 0x1406c3c02: the cited load 0x1406c3bfd uses offset 0x8, the report says 0x18`;
- an operand instruction VA changed (`0x1406C3BFD` → `0x1406C3BFE`) → `BAD 0x1406c3c02: the cited load 0x1406c3bfe is not a 'MOVSS <reg>,[RDI+..]'`;
- an operand register changed (`XMM1` → `XMM2`) → `BAD 0x1406c3c02: the report's text says XMM2 but the instruction 0x1406c3bf8 targets XMM1`;
- a branch target changed → `BAD branch 0x1406c1462 does not target 0x1406c148c (dis: JZ 0x1406c148b)`;
- a branch source changed → `BAD branch 0x1406c1463 -> 0x1406c148b: the instruction is not in any dis file`;
- a round-2 constant changed (`112.5` → `112.6` at `0x1425CB30C`) → `BAD 0x1425cb30c: value claimed 112.6 actual 112.5 (0x42E10000)`;
- a STOP-exit site moved to a non-call VA → `BAD STOP-exit site 0x1406c148b does not call 0x1406d5d60 (dis: MOV RDX,RBX)`;
- a writer value changed in the §7.1 index → `BAD 0x1406c13bd: report says 0x9, image says 0x4`.

The file was restored after each mutation and re-verified clean. (Four checker defects were found
and fixed across the passes: the VA-existence check had been matching 8-hex-digit prefixes of
9-digit VAs; the operand-pair check was originally reading the offset from the report's own text
— which made it self-consistently pass a wrong value; the branch-pair check was skipping a
source VA that is in no disassembly file instead of failing on it; and the STOP-exit check was
using a hard-coded VA list instead of reading the §7.2 table. All four were re-written and
re-mutation-tested.)
