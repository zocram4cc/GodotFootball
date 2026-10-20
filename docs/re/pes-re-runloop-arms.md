# PES RUN_LOOP (`FUN_1406C0170`) arms and the STEP-family `req[0x39]` writers

> **Main audit 20-10-26:** `0x1406C0AEB` `movb [rsi+0xe0],1` + `0x1406C0AF2` `movl [rsi+0xe4],0xe` (RUN_LOOP style arm requests RUN_TO_STEP); `0x1406D5DA6/DAB` class word [rdx+0x7c] compare; `0x1406D608A` `movl [rdi+0xe4],8` (STOP) and `0x1406D6243` `movl [rdi+0xe4],1` (IDLE): all match.

Read-only RE. No GF code changed, no PES written. Scratch: `<workdrive>/gf-TeamDecode-scratch/dec/`.
Base `0x140000000`, image `~/ghidra-projects/scratch/Unpacker/pes_full.bin`.
Tags: **CONFIRMED** = decompile and/or raw dis this pass; **INF** = inference; **OPEN** = blocker named.

The port under comparison is `<workdrive>/gf-ClipWords/src/pes/movestep_run.cpp` (`RunLoop`).
The port's state numbering is `src/pes/movestep.hpp:32` `enum class MoveState` — used throughout below.

## §0 The request block and `req[0x39]`

`FUN_1406C0170(longlong param_1, uint *param_2)` — `param_1` is the playback/host object, `param_2` is the
**request block** (`req`). `param_2` is a `uint *`, so `req[N]` is word `N` and the byte offset is
`4*N`. The next-state request is `req[0x39]` = **byte offset `0xE4`**.

CONFIRMED raw dis: `0x1406C0AF2 MOV dword ptr [RSI + 0xe4],0xe` (the RUN_TO_STEP write),
where `RSI` is `param_2`.

**How the writers were found.** A disp32 memory-operand scan of the `.trace` executable section
(`<workdrive>/gf-TeamDecode-scratch/w39.py`, built on `patscan3.py`) enumerated every store whose
displacement is `0xE4`; the `C7 /0 imm32` form gives the stored value directly. 332 sites in the
whole image, of which the MOVESTEP family is the `0x1406B…`–`0x1406D…` range.

## §1 The state numbering (from the port's enum, `movestep.hpp:32`)

| value | port name | STEP family? |
|---|---|---|
| 0 | `kInit` | no |
| 1 | `kIdle` | no |
| 2 | `kIdleTurn` | no |
| 3 | `kRunLoop` | no (this handler) |
| 4 | `kStepLoop` | **yes** |
| 5 | `kStepLoopChange` | **yes** |
| 6 | `kTurnStop` | no |
| 7 | `kGotoRun` | no |
| 8 | `kStop` | no (the target state) |
| 9 | `kAccelerate` | no |
| 10 | `kDecelerate` | no |
| 11 | `kGotoStep` | (never dispatched) |
| 12 | `kRunTurnRun` | **yes** |
| 13 | `kStepToRun` | **yes** |
| 14 | `kRunToStep` | **yes** |
| 15 | `kStepTurnRun` | **yes** |
| 16 | `kRunTurnStep` | **yes** |
| 17 | `kIdleOther` | no |
| 18 | `kRunTurnRunSlipdown` | (not in the brief's list) |
| 19 | `kStepTurnStep` | **yes** |

## §2 PART 1 — the control-flow map of `FUN_1406C0170` (decompile lines ~330-466)

The decompile is 468 lines; lines 330-466 are the **tail** of the handler. The whole function is a
**chain of guarded arms**, each ending in `return`. The arms below are in PES order.

### Arm A — "no target / nothing to do" (decompile 57-61) — `FUN_1406D2590`
- **Condition:** `req[9] == 0xFFFFFFFF` **AND** (`req[0xE] == 1` **OR** `MI+0x116C != 1`).
- **Writes:** none directly (the callee writes).
- **req[0x39]:** written by `FUN_1406D2590` (see §3).

### Arm B — "external interrupt" (decompile 62-69) — `FUN_1406CE820(param_1, param_2, 0)`
- **Condition:** (any of `req[0x3D]`, `req[0x3E]`, `req[0x3F]` non-zero) **AND**
  (`req[9] > 1` **OR** `req[0x3F] != 0`) **AND** `req[0x4A] <= 0.6` **AND** `req[0xE] == 0`.
- **Writes:** none directly.
- **req[0x39]:** written by `FUN_1406CE820`.

### Arm C — early sub-handlers (decompile 70-77)
- `FUN_1406CCD00(param_1, param_2)` returns non-zero → **return** (no further arms).
- `FUN_1406CDAF0(param_1, param_2)` returns non-zero → **return** (no further arms).

### Arm D — "still too far" (decompile 90-102) — `FUN_1406CE820(param_1, param_2, 0)`
- **Condition:** (`req[0xE] == 0` **AND** `req[9] > 2` **AND**
  ((`req[0x4A] <= DAT_1425DDF54` **AND** `DAT_1425DDF54 != req[0x4A]`) **OR**
   (`rand_term * dist < 11.0` **AND** `45.0 < req[6]`))) **AND**
  `3.0 < req[0x12]` **AND** `DAT_14259BE3C < req[6]`.
- **req[0x39]:** written by `FUN_1406CE820`.

### Arm E — the **swing / bucket** arm (decompile 114-263, label `LAB_1406C0B01`)

This is the arm the port calls "the swing arm". It is reached when
`((req[8] == 0 || req[8] == 5) && 0.15 < req[0x4A])` **OR**
`FUN_1406B70D0(param_1, param_2) == 0`.

**E0 — early return inside the swing arm (decompile 120-122).**
- **Condition:** `req[0x1A] < req[0x1B]` (the drift term still below its ceiling).
- **Effect:** `return` — **no** `req[0x39]` write. The tick ends with the request unchanged.
- CONFIRMED raw dis: `0x1406C0B01 MOVSS XMM0,[RSI+0x68] / 0x1406C0B06 COMISS XMM0,[RSI+0x6c] / 0x1406C0B0A JC 0x1406C0FFC`.

**E1 — the swing-arm body (decompile 123-262).**
It computes the new **style** (`uVar9`) and the mirror, then writes the block:
- `req[0x37] = uVar9` (the new style)
- `req+0x2F = 1` (motion pending byte)
- `req+0x50 = (char)uVar9` (the style byte)
- `req[9] = clamp(req[9], 0, 3)`
- `req[0x30] = DAT_1425DDCF0[req[9] + uVar9*4]` (the representative motion)
- `req+0x31` = the mirror byte
- `req[0x32] = fVar24` (`DAT_1425DDF4C` or `DAT_142598578` when the style is 0 and the old style was not)
- `req[0x35] = 0x3F800000` (1.0f)
- `req[0x33] = 0`
- `req[0x1A] = 0`, `req[0x1B] = 0.2f` (`0x3E4CCCCD`)
- `req[0x2D] = 0`, `req[0x2E] = 0.2f`
- `req[0x20] = 0`
- `req[0x1E] = req[0]`
- **and `return`** — **no `req[0x39]` write in this arm.**

**This is the key divergence.** The port's swing arm ends with
`RequestState(req, MoveState::kTurnStop)` (`movestep_run.cpp`, "RequestState(req, MoveState::kTurnStop);"
just after the `host.Issue(...)` in the block quoted at line ~2385). **PES's swing arm writes no
`req[0x39]` at all** — it leaves the next state as it was. The port's `kTurnStop` request is a
**stand-in** for "no state change".

**E2 — the "not far enough yet" fall-through (decompile 265-266).**
- **Condition:** `(speed - fVar23) * fVar20 + fVar23 < req[0x4A]`.
- **Effect:** `goto LAB_1406C0B01` → the swing arm (E0/E1).

**E3 — the "far enough" ladder (decompile 267-286, label `LAB_1406C06C9`).**
- `req[8]` is the ladder selector: `0` and `1` fall into the ladder, `5` jumps to `LAB_1406C0708`,
  `6` jumps to `LAB_1406C06C9`, anything else → `LAB_1406C06C9`.
- `LAB_1406C06C9` (decompile 274-284):
  - if `req[0x4A] < DAT_142590D98` → `FUN_1406D5D60` (**the IDLE requester**, `req[0x39] = 1`),
  - else if `req[0x12] <= 2.0` **AND** `2.0 != req[0x12]` → `FUN_1406D2590`,
  - else → `FUN_1406D6780`.

### Arm F — the **style** arm (decompile 287-406, label `LAB_1406C0708`)

Reached from E3 when `req[8] == 5`, and from the `req[8] == 0/1` path when the "far enough" test
passes. This is the block that writes **`req[0x39] = 0xE`**.

- **F0 (decompile 288-314):** the style ladder — `FUN_14043A0A0`, `FUN_1405AEF10`, the angle clamps.
- **F1 (decompile 315-332):** the playback rate and the `FUN_1406ABB30` / `FUN_1406ABD60` clip pick.
- **F2 (decompile 333-397):** the state-word bookkeeping —
  `req[0x25] = req[0x20]`, `req[0x20] = vtable+0x60 result`, `req[0x17]`, `req[0x18]`,
  the `req[0x1F]` class ladder, `req[0x24] = req[0x1F]`, `req[0x22] = req[0x1D]`,
  `req[0x1D] = fVar29`, `req[0x1F] = uVar8`, `req[0x1E] = uVar26`.
- **F3 — the issue (decompile 398):** `(**(code **)(lVar14 + 0x10))(plVar2, param_2, sVar7, cVar4, uVar30, DAT_142598578, uVar8, uVar27)`.
- **F4 (decompile 399-405) — the state request:**
  ```
  req[0x1A] = 0;
  req[0x1B] = 0.2f;          // 0x3E4CCCCD
  req[0x2D] = 0;
  req[0x2E] = 0.2f;          // 0x3E4CCCCD
  req[0x1E] = uVar26;
  req+0x38 = 1;              // 0xE0
  req[0x39] = 0xE;           // 0xE4  <-- RUN_TO_STEP
  return;
  ```
  CONFIRMED raw dis: `0x1406C0ACB MOV dword ptr [RSI+0x68],R15D`,
  `0x1406C0ACF MOV dword ptr [RSI+0x6c],0x3e4ccccd`,
  `0x1406C0AD6 MOV dword ptr [RSI+0xb4],R15D`,
  `0x1406C0ADD MOV dword ptr [RSI+0xb8],0x3e4ccccd`,
  `0x1406C0AE7 MOV dword ptr [RSI+0x78],R13D`,
  `0x1406C0AEB MOV byte ptr [RSI+0xe0],0x1`,
  `0x1406C0AF2 MOV dword ptr [RSI+0xe4],0xe`.

**So `req[0x39] = 0xE` (RUN_TO_STEP) is written by the STYLE arm (F), not by the swing arm (E).**

### Arm G — the "no class" tail (decompile 408-466)

- **G0 (decompile 410-422):** the deceleration ramp (`DAT_1425CCA34`, `DAT_14259BE60`, `DAT_142590D98`).
- **G1 (decompile 424-447):**
  - **Condition to skip:** `req[0xE] == 1` **AND** `req[0x1F] <= 2` **AND** `fVar23 >= req[0x4A]`.
  - If **not** skipped: `FUN_1406D6780(param_1, param_2)` (decompile 443) **or**
    `FUN_1406D2590(param_1, param_2)` (decompile 446) — one of the two, by the
    `fVar23 < req[0x12]` test and the `FUN_14043A0A0` compare.
- **G2 (decompile 449-466):** the `req[0x1F]` / `req[0x1D]` update, then
  `FUN_1406D5D60(param_1, param_2)` — the **IDLE requester** (`req[0x39] = 1`).

## §3 PART 2 — every PES writer of `req[0x39]` (the next-state request)

From the disp32 scan (`w39.py`). Only the MOVESTEP-family handlers are listed; the `0x1400…`–`0x1405…`
hits are unrelated structures that also use offset `0xE4`.

| writer VA | function | value | next state (port name) |
|---|---|---|---|
| `0x1406BB0C8` | `FUN_1406BA900` | `0x4` | **STEP_LOOP** |
| `0x1406C0AF2` | **`FUN_1406C0170` (RUN_LOOP)** | `0xE` | **RUN_TO_STEP** |
| `0x1406C13BD` | `FUN_1406C1070` | `0x4` | **STEP_LOOP** |
| `0x1406C4BA8` | `FUN_1406C3470` | `0x4` | **STEP_LOOP** |
| `0x1406C592A` | `FUN_1406C4C10` | `0xD` | **STEP_TO_RUN** |
| `0x1406C5D30` | `FUN_1406C4C10` | `0xF` | **STEP_TURN_RUN** |
| `0x1406C62C1` | `FUN_1406C4C10` | `0x5` | **STEP_LOOP_CHANGE** |
| `0x1406C6F27` | `FUN_1406C6480` | `0x4` | **STEP_LOOP** |
| `0x1406C7B19` | `FUN_1406C7400` | `0x4` | **STEP_LOOP** |
| `0x1406C9416` | `FUN_1406C8D80` | `0xF` | **STEP_TURN_RUN** |
| `0x1406C9DC4` | `FUN_1406C8D80` | `0x4` | **STEP_LOOP** |
| `0x1406CB47F` | `FUN_1406CAB20` | `0x4` | **STEP_LOOP** |
| `0x1406D3837` | `FUN_1406D2F60` | `0x10` (16) | **RUN_TURN_STEP** |
| `0x1406D4395` | `FUN_1406D38D0` | `0xF` | **STEP_TURN_RUN** |
| `0x1406D4710` | `FUN_1406D38D0` | `0x13` (19) | **STEP_TURN_STEP** |
| `0x1406D541E` | `FUN_1406D38D0` | `0xF` | **STEP_TURN_RUN** |
| `0x1406D5D29` | `FUN_1406D38D0` | `0x13` (19) | **STEP_TURN_STEP** |

The non-STEP values written by the same handlers (for completeness, from the same scan):
`0x1` (IDLE) at `0x1406BDEF2`, `0x1406BFCF4`, `0x1406BFE54`, `0x1406C0142`, `0x1406CA645`,
`0x1406CA9E2`, `0x1406CB9EC`, `0x1406D6243`; `0x3` (RUN_LOOP) at `0x1406C73A3`, `0x1406D2354`;
`0x6` (TURN_STOP) at `0x1406D2D84`; `0x7` (GOTO_RUN) at `0x1406CFD31`, `0x1406D111E`;
`0x9` (ACCELERATE) at `0x1406CDA43`; `0xA` (DECELERATE) at `0x1406CE47B`;
`0x2` (IDLE_TURN) at `0x1406D171F`; `0x8` (STOP) at `0x1406D608A`;
`0x11` (IDLE_OTHER) at `0x1406D134C`; `0x16` (TIMING_STEP) at `0x1406D66F8`;
`0x17` (DEMO) at `0x1406CE77B`.

**No writer of `0x0C` (12, `kRunTurnRun`) exists in the image.** State 12's handler
(`FUN_1406C14C0`) never writes `req[0x39]`.

**The full table, with the owning state for every site, is in §5.**

## §4 The state dispatch table (CONFIRMED) — the numbering anchor

PES dispatches the MOVESTEP handlers through a **jump table at `0x1406BB50C`**, indexed by the state
byte. CONFIRMED raw dis:

```
0x1406BA9D9 CMP ECX,0x17
0x1406BA9DC JA 0x1406bb314
0x1406BA9E2 MOV EAX,ECX
0x1406BA9E4 LEA RCX,[0x140000000]
0x1406BA9EB MOV EAX,dword ptr [RCX + RAX*0x4 + 0x6bb50c]
0x1406BA9F2 ADD RAX,RCX
0x1406BA9F5 JMP RAX
```

| state | port name | dispatch arm | handler |
|---|---|---|---|
| 0 | `kInit` | `0x1406BAA90` | (inline in `FUN_1406BA900` — no separate call) |
| 1 | `kIdle` | `0x1406BB33B` | `FUN_1406BF0C0` |
| 2 | `kIdleTurn` | `0x1406BB356` | `FUN_1406BFE70` |
| 3 | **`kRunLoop`** | `0x1406BB371` | **`FUN_1406C0170`** |
| 4 | `kStepLoop` | `0x1406BB38B` | `FUN_1406C4C10` |
| 5 | `kStepLoopChange` | `0x1406BB3A8` | `FUN_1406C6480` |
| 6 | `kTurnStop` | `0x1406BB3C5` | `FUN_1406CAB20` |
| 7 | `kGotoRun` | `0x1406BB3D5` | `FUN_1406BDF10` |
| 8 | `kStop` | `0x1406BB3E5` | `FUN_1406C9E20` |
| 9 | `kAccelerate` | `0x1406BB3F5` | `FUN_1406BB570` |
| 10 | `kDecelerate` | `0x1406BB405` | `FUN_1406BCD20` |
| 11 | `kGotoStep` | `0x1406BB314` | (the **function epilogue** — `0x1406BB314 MOVAPS XMM6,… / 0x1406BB32B RET` — so `kGotoStep` is **never dispatched**, as the port already notes) |
| 12 | `kRunTurnRun` | `0x1406BB415` | `FUN_1406C14C0` |
| 13 | `kStepToRun` | `0x1406BB425` | `FUN_1406C6F80` |
| 14 | `kRunToStep` | `0x1406BB435` | `FUN_1406C1070` |
| 15 | `kStepTurnRun` | `0x1406BB445` | `FUN_1406C7400` |
| 16 | `kRunTurnStep` | `0x1406BB455` | `FUN_1406C3470` |
| 17 | `kIdleOther` | `0x1406BB465` | `FUN_1406BF7F0` |
| 18 | `kRunTurnRunSlipdown` | `0x1406BB475` | `FUN_1406C2F80` |
| 19 | `kStepTurnStep` | `0x1406BB485` | `FUN_1406C8D80` |
| 20 | `kIdlePkPlayer` | `0x1406BB495` | `FUN_1406BFD10` |
| 21 | `kIdlePkGk` | `0x1406BB4B3` | `FUN_1406BFB70` |
| 22 | `kTimingStep` | `0x1406BB4D1` | `FUN_1406CA6B0` |
| 23 | `kDemo` | `0x1406BB4EE` | `FUN_1406BDD70` |

**The port's `MoveState` enum (`movestep.hpp:32`) matches this table exactly, value for value.**
That is the anchor for every name in this report.

## §5 The full `req[0x39]` transition table (CONFIRMED)

From the disp32 scan (`w39.py`) plus the dispatch table of §4. "from" is the state whose handler
owns the writing site; "(helper)" means the writer is in a function called from several handlers.

| writer VA | value | next state | written from |
|---|---|---|---|
| `0x1406BB0C8` | `0x4` | `kStepLoop` | helper `FUN_1406BA900` |
| `0x1406BDEF2` | `0x1` | `kIdle` | 23 `kDemo` (`FUN_1406BDD70`) |
| `0x1406BFCF4` | `0x1` | `kIdle` | 21 `kIdlePkGk` (`FUN_1406BFB70`) |
| `0x1406BFE54` | `0x1` | `kIdle` | 20 `kIdlePkPlayer` (`FUN_1406BFD10`) |
| `0x1406C0142` | `0x1` | `kIdle` | 2 `kIdleTurn` (`FUN_1406BFE70`) |
| **`0x1406C0AF2`** | **`0xE`** | **`kRunToStep`** | **3 `kRunLoop` (`FUN_1406C0170`)** |
| `0x1406C13BD` | `0x4` | `kStepLoop` | 14 `kRunToStep` (`FUN_1406C1070`) |
| `0x1406C4BA8` | `0x4` | `kStepLoop` | 16 `kRunTurnStep` (`FUN_1406C3470`) |
| `0x1406C592A` | `0xD` | `kStepToRun` | 4 `kStepLoop` (`FUN_1406C4C10`) |
| `0x1406C5D30` | `0xF` | `kStepTurnRun` | 4 `kStepLoop` (`FUN_1406C4C10`) |
| `0x1406C62C1` | `0x5` | `kStepLoopChange` | 4 `kStepLoop` (`FUN_1406C4C10`) |
| `0x1406C6F27` | `0x4` | `kStepLoop` | 5 `kStepLoopChange` (`FUN_1406C6480`) |
| `0x1406C73A3` | `0x3` | `kRunLoop` | 13 `kStepToRun` (`FUN_1406C6F80`) |
| `0x1406C7B19` | `0x4` | `kStepLoop` | 15 `kStepTurnRun` (`FUN_1406C7400`) |
| `0x1406C9416` | `0xF` | `kStepTurnRun` | 19 `kStepTurnStep` (`FUN_1406C8D80`) |
| `0x1406C9DC4` | `0x4` | `kStepLoop` | 19 `kStepTurnStep` (`FUN_1406C8D80`) |
| `0x1406CA645` | `0x1` | `kIdle` | 8 `kStop` (`FUN_1406C9E20`) |
| `0x1406CA9E2` | `0x1` | `kIdle` | 22 `kTimingStep` (`FUN_1406CA6B0`) |
| `0x1406CB47F` | `0x4` | `kStepLoop` | 6 `kTurnStop` (`FUN_1406CAB20`) |
| `0x1406CB9EC` | `0x1` | `kIdle` | 6 `kTurnStop` (`FUN_1406CAB20`) |
| `0x1406CDA43` | `0x9` | `kAccelerate` | helper `FUN_1406CCD00` |
| `0x1406CE47B` | `0xA` | `kDecelerate` | helper `FUN_1406CDAF0` |
| `0x1406CE77B` | `0x17` | `kDemo` | helper `FUN_1406CE550` |
| `0x1406CFD31` | `0x7` | `kGotoRun` | helper `FUN_1406CE820` |
| `0x1406D111E` | `0x7` | `kGotoRun` | helper `FUN_1406D01D0` |
| `0x1406D134C` | `0x11` | `kIdleOther` | helper `FUN_1406D1170` |
| `0x1406D171F` | `0x2` | `kIdleTurn` | helper `FUN_1406D1390` |
| `0x1406D2354` | `0x3` | `kRunLoop` | helper `FUN_1406D2030` |
| `0x1406D2D84` | `0x6` | `kTurnStop` | helper `FUN_1406D2590` |
| `0x1406D3837` | `0x10` | `kRunTurnStep` | helper `FUN_1406D2F60` |
| `0x1406D4395` | `0xF` | `kStepTurnRun` | helper `FUN_1406D38D0` |
| `0x1406D4710` | `0x13` | `kStepTurnStep` | helper `FUN_1406D38D0` |
| `0x1406D541E` | `0xF` | `kStepTurnRun` | helper `FUN_1406D38D0` |
| `0x1406D5D29` | `0x13` | `kStepTurnStep` | helper `FUN_1406D38D0` |
| **`0x1406D608A`** | **`0x8`** | **`kStop`** | helper `FUN_1406D5D60` |
| `0x1406D6243` | `0x1` | `kIdle` | helper `FUN_1406D5D60` |
| `0x1406D66F8` | `0x16` | `kTimingStep` | helper `FUN_1406D6290` |

### The register-form writers (value not a constant)

Six more sites store to `req[0x39]` with a **register**, so the value is computed, not an immediate.
They are listed here rather than in the table above because their value is not a constant:

| writer VA | value | owning function | notes |
|---|---|---|---|
| `0x1406BB112` | `EDI` = the return of `FUN_1406ABB30` (the clip picker), sign-extended at `0x1406BB005 MOVSX EDI,AX` | `FUN_1406BA900` (the INIT / shared helper) | the next state is **whatever the picked clip's row says** — a data-driven state, not a constant |
| `0x1406BF4A4` | `R15D` = `SETNZ` + `0x14` → **0x14 (20 `kIdlePkPlayer`) or 0x15 (21 `kIdlePkGk`)** | `FUN_1406BF0C0` (`kIdle`, also the `kGotoStep` arm) | `0x1406BF48B SETNZ R15B / 0x1406BF499 ADD R15D,0x14` |
| `0x1406BF697` | same function, the second site | `FUN_1406BF0C0` | as above |
| `0x1406BFA8B` | `R13D` | `FUN_1406BF7F0` (`kIdleOther`) | the state is computed earlier in the function |
| `0x1406B69F1` | `ESI` | `FUN_1406B68D0` (a helper) | the state is computed earlier in the function |
| `0x1406D0135` | a register | `FUN_1406CE820` (the re-plan helper) | the state is computed earlier in the function |

**None of the register-form sites writes a STEP-family value** — the only STEP-family writer is the
immediate-form `0x1406C0AF2` (`0xE`, RUN_TO_STEP) in `FUN_1406C0170`. **This is what makes the
`kRunLoop` → `kRunToStep` edge the single entry point into the STEP family from a constant.**



| STEP state | entered from | by which writer |
|---|---|---|
| **4 `kStepLoop`** | 14 `kRunToStep`; 16 `kRunTurnStep`; 19 `kStepTurnStep`; 6 `kTurnStop`; 5 `kStepLoopChange`; 15 `kStepTurnRun`; and the helper `FUN_1406BA900` | `0x1406C13BD`, `0x1406C4BA8`, `0x1406C9DC4`, `0x1406CB47F`, `0x1406C6F27`, `0x1406C7B19`, `0x1406BB0C8` |
| **5 `kStepLoopChange`** | **4 `kStepLoop` only** | `0x1406C62C1` |
| **12 `kRunTurnRun`** | **NOTHING** — no writer of `0x0C` exists in the image | — |
| **13 `kStepToRun`** | **4 `kStepLoop` only** | `0x1406C592A` |
| **14 `kRunToStep`** | **3 `kRunLoop` only** | `0x1406C0AF2` |
| **15 `kStepTurnRun`** | **4 `kStepLoop`; 19 `kStepTurnStep`** | `0x1406C5D30`, `0x1406C9416`, `0x1406D4395`, `0x1406D541E` |
| **16 `kRunTurnStep`** | the helper `FUN_1406D2F60` (called from 13 sites, incl. `0x1406BAC83` in the INIT path) | `0x1406D3837` |
| **19 `kStepTurnStep`** | the helper `FUN_1406D38D0` (20 call sites) | `0x1406D4710`, `0x1406D5D29` |
| 22 `kTimingStep` (also a step) | the helper `FUN_1406D6290` | `0x1406D66F8` |

**The key structural fact for the port:** the STEP family is reached from exactly three places —
the `kRunLoop` handler (→ 14), the `kStepLoop` handler (→ 5, 13, 15) and the `kStepTurnStep`
handler (→ 4, 15) — plus the two shared helpers `FUN_1406D2F60` (→ 16) and `FUN_1406D38D0` (→ 15, 19).
**A port that never visits states 4, 5, 12-16, 19 cannot reach the STEP family at all**, and since
live PES enters STOP (8) mostly from them, the port's STOP entries come from a different place.

### The STOP path (`FUN_1406D5D60`) — the census's "from 5, 3, 15, 14, 0, 7"

`FUN_1406D5D60` is the shared **STOP / IDLE requester**:
```
1406D5DA6 MOV EAX,dword ptr [RDX+0x7c]     ; the class word (req byte 0x1F)
1406D5DA9 INC EAX
1406D5DAB CMP EAX,0x1
1406D5DAE JBE 0x1406d60b1                    ; class == 0 -> the IDLE branch
```
- **class == 0** → the IDLE branch → `req[0x39] = 1` at `0x1406D6243`
  (with `req[0x1B] = 0x3DCCCCCD`, `req[0x2E] = 0x3EB33333`, `req[0x38] = 1`).
- **class >= 1** → the STOP branch → **`req[0x39] = 8`** at `0x1406D608A`
  (with `req[0x78] = req[0]`, `req[0x38] = 1`, `req[0x5C] = R12`, `req[0xF0] = R12B`).

`FUN_1406D5D60` has **24 call sites** — from `FUN_1406C0170` itself (`0x1406C0573`, `0x1406C06DC`),
from `FUN_1406D2590`, `FUN_1406D2F60`, `FUN_1406CE820` and the STEP handlers. That is the
mechanism by which "STOP is entered from 5, 3, 15, 14, 0, 7": those are the states whose handlers
call `FUN_1406D5D60` with a non-zero class word.

## §6 PART 3 — the port's `RunLoop` arms vs PES's arms

The port is `<workdrive>/gf-ClipWords/src/pes/movestep_run.cpp`, `RunLoop` at line 2120.

| port arm (line) | PES arm | agreement |
|---|---|---|
| `!running` no-class block (2129-2133) | — | **the port's comment says "its state request is TURN_STOP"; PES's no-class path in this handler writes no `req[0x39]` in the swing arm — the ladder `LAB_1406C06C9` calls `FUN_1406D5D60` (IDLE/STOP), `FUN_1406D2590` or `FUN_1406D6780`.** |
| Arm 1 turn-lock re-plan (2135-2143) | **B** `FUN_1406CE820` (decompile 62-69) | matches — `FUN_1406CE820` writes `req[0x39] = 7` (`kGotoRun`) at `0x1406CFD31` |
| Arm 2 ACCELERATE / DECELERATE pre-empts (2145-2147) | helpers `FUN_1406CCD00` / `FUN_1406CDAF0` | matches — `0x9` at `0x1406CDA43`, `0xA` at `0x1406CE47B` |
| Arm 3 re-plan term (2149-2159) | **D** `FUN_1406CE820` (decompile 90-102) | matches |
| Arm 4 RUN_TURN_STEP pre-empt (2161-2163) | helper `FUN_1406D2F60` | matches — `req[0x39] = 0x10` at `0x1406D3837` |
| Arm 4 keeper-kind / run-turn block (2165-2189), incl. `RequestState(req, MoveState::kTurnStop)` at **2185** | **E3** ladder `LAB_1406C06C9` (decompile 274-284) | **diverges** — PES calls `FUN_1406D5D60` (IDLE/STOP) / `FUN_1406D2590` / `FUN_1406D6780`, never a TURN_STOP request |
| the `swing_write:` block, `RequestState(req, MoveState::kTurnStop)` at **2381** | **E1** swing body (decompile 123-262) | **diverges — PES writes NO `req[0x39]` in the swing arm.** The port's `kTurnStop` is a stand-in for "leave the next state alone". |
| Arm 5 the style block (~2401 onward) | **F** style arm (decompile 287-406) | **the port's style arm must request `kRunToStep` (14)** — PES writes `req[0x39] = 0xE` at `0x1406C0AF2`, plus `req+0x38 = 1` at `0x1406C0AEB` and the four blend words (`0x1406C0ACB`/`0x1406C0ACF`/`0x1406C0AD6`/`0x1406C0ADD`). |

### The two concrete divergences to fix in the port

1. **The swing arm must not request a state.** PES's swing arm writes nothing to `req[0x39]`;
   the port's `RequestState(req, MoveState::kTurnStop)` at line 2381 (and the one at 2185) is a
   stand-in. Removing it means the state stays whatever it was — which is how PES can stay in
   `kRunLoop` and later request `kRunToStep`.
   - Note: `RequestState` (`movestep.hpp:211`) sets `kChangeState` (`req[0x38]`) **and**
     `kNextState` (`req[0x39]`). PES sets `req+0x38 = 1` **only** in the arms that also write
     `req[0x39]` (e.g. `0x1406C0AEB` / `0x1406C0AF2` are adjacent). The two must stay paired:
     a `kChangeState` without a `kNextState` would be a state change with no target.
2. **The style arm must request `kRunToStep` (14).** This is the only way the port can enter the
   STEP family at all, and it is exactly the state live PES is in when it later enters STOP.

## §7 Open items

1. The **conditions** inside the swing arm (E1) that choose the style `uVar9` (`0x10`, `0x7`, `0x6`
   at decompile 197-211) are not reduced to a table here — the port's own comments carry them.
   Not blocking: the port already ports the swing body.
2. `FUN_1406BA900` (the writer of `req[0x39] = 4` at `0x1406BB0C8`) is a **shared helper**, not a
   state handler — its callers and its arm conditions are **OPEN**.
3. The two shared helpers `FUN_1406D2F60` (→ 16) and `FUN_1406D38D0` (→ 15, 19) have 13 and 20
   call sites respectively; which state each call site belongs to is **OPEN** beyond the two
   state-owned writers listed in §5.
4. `FUN_1406D6290` (→ 22 `kTimingStep`) has a single caller, `0x1406C4E51` — inside
   `FUN_1406C4C10` (`kStepLoop`). So **`kTimingStep` is entered from `kStepLoop`**.
5. The live confirmation of the census (which states really enter STOP) is **OPEN** — this report
   gives the static mechanism only.

## §8 Method

- Bridge decompiles (`dec/FUN_*.c`) and disassembly (`dec/dis_*.txt`) for `FUN_1406C0170` and the
  handlers; ~45 fetches against `http://127.0.0.1:8080`.
- The `req[0x39]` writer inventory comes from `<workdrive>/gf-TeamDecode-scratch/w39.py`, a
  disp32 memory-operand scan of the `.trace` executable section for stores at displacement `0xE4`,
  reading the immediate out of the `C7 /0 imm32` encoding. The function that owns each site was
  resolved with `fnof.py` (the bridge's "at or containing" disassembly endpoint).
- The state→handler map comes from reading the jump table at `0x1406BB50C` out of the image bytes
  and the dispatch arms in `dec/dis_1406BB300.txt`.
- No GF code changed, no GF build/test run, no PES write/pin/input. The port under comparison was
  read only (`<workdrive>/gf-ClipWords/src/pes/movestep_run.cpp`, `movestep.hpp`).

### Self-check

`<workdrive>/gf-TeamDecode-scratch/check_runloop.py`, run against this file:
```
req[0x39] writer rows in the report: 37
  -> 37 writer rows checked, 0 mismatches so far
dispatch-table rows in the report: 22
  -> 22 dispatch rows checked
cited code VAs not inside a disassembled instruction: 0

TOTAL mismatches: 0
```
The checker re-derives every `req[0x39]` writer from the image bytes (it finds the `C7 /0 imm32`
store at displacement `0xE4` and reads the immediate back), re-reads the state jump table at
`0x1406BB50C` and compares each state's dispatch arm and handler against the disassembly, and
confirms every quoted code VA lies inside a disassembled instruction.

**Mutation-tested — all three checks fire:**
- a writer's value changed (`0xE` → `0xD` at `0x1406C0AF2`) → `BAD 0x1406c0af2: report says 0xd, image says 0xe`;
- a dispatch arm changed (`0x1406BB38B` → `0x1406BB38C`) → `BAD state 4: report arm 1406BB38C, jump table says 0x1406bb38b`;
- a handler name changed (`FUN_1406C0170` → `FUN_1406C0171`) → `BAD state 3: report handler 1406C0171, dis says 0x1406c0170`.

The file was restored after each mutation and re-verified clean. The "22 dispatch rows" is because
states 0 and 11 have no `FUN_` in the handler column (0 is inline in `FUN_1406BA900`, 11 is the
function epilogue) — the checker treats those two specially and asserts they have **no** handler call.

