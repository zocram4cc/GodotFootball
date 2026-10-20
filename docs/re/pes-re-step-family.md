# PES STEP family: per-handler arm maps, exits and `req[0x39]` next-state writes

> **Main audit 20-10-26:** STOP exits call the shared requester 0x1406D5D60 at `0x1406C7F12` (state 15) and `0x1406C6C21` (state 5) directly, and for state 14 at `0x1406C1491` (`0x1406C148B` is its argument setup); Finding 11 loads `0x1406C3BF8` `movss xmm1,[rdi+0x10]` / `0x1406C3BFD` `movss xmm0,[rdi+0x8]`; `0x1406C3C24` is `call [rax+0x18]` (vtable, not FUN_14043A0A0): all match.

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
| **14** `kRunToStep` | 14 | `FUN_1406C1070` | `0x1406C148B` — reached when `req[0x24] == -1` (raw `0x1406C145E`/`0x1406C1462`) |
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

## §6 Finding 11 — the dropped `FUN_14043A0A0` operand pairs in `FUN_1406C3470` (recovered)

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

## §7 Method

- Bridge decompiles (`dec/FUN_*.c`) and disassembly (`dec/dis_*.txt`) for every handler named here;
  ~30 fetches against `http://127.0.0.1:8080`.
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
  -> 30 constant rows checked
req[0x39] writer rows: 14
  -> writer rows checked
cited code VAs not inside a disassembled instruction: 0

TOTAL mismatches: 0
```
The checker verifies, against the **image bytes and the disassembly** (never against the report's
own text):
1. every `FUN_14043A0A0` operand pair — the cited instruction VA must be a real
   `MOVSS <reg>,[RDI+off]`, the register and offset must match what the report names, and the
   register must not be reloaded between the load and the call;
2. every constant row in §0b — the bits and the decimal must both match the image;
3. every `req[0x39]` writer row in §5b — the site must really be a `C7 /0` store to displacement
   `0xE4` carrying the quoted immediate;
4. every quoted code VA must lie inside a disassembled instruction.

**Mutation-tested — all five checks fire:**
- a constant value changed (`0.75` → `0.76`) → `BAD 0x14259bdd4: value claimed 0.76 actual 0.75 (0x3F400000)`;
- a writer value changed (`0x4` → `0x5` at `0x1406C13BD`) → `BAD 0x1406c13bd: report says 0x5, image says 0x4`;
- an operand offset changed (`[RDI+0x8]` → `[RDI+0x18]`) → `BAD 0x1406c3c02: the cited load 0x1406c3bfd uses offset 0x8, the report says 0x18`;
- an operand instruction VA changed (`0x1406C3BFD` → `0x1406C3BFE`) → `BAD 0x1406c3c02: the cited load 0x1406c3bfe is not a 'MOVSS <reg>,[RDI+..]'`;
- an operand register changed (`XMM1` → `XMM2`) → `BAD 0x1406c3c02: the report's text says XMM2 but the instruction 0x1406c3bf8 targets XMM1`.

The file was restored after each mutation and re-verified clean. (Two checker defects were found
and fixed during this pass: the VA-existence check had been matching 8-hex-digit prefixes of
9-digit VAs, and the operand-pair check was originally reading the offset from the report's own
text — which made it self-consistently pass a wrong value. Both were re-written and re-mutation-tested.)
