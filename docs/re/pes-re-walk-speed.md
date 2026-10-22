# PES 2021 set-piece walker command speed - the writer (RE report, WalkSpeedRE)

> **Main audit 22-10-26 (bytes):** `DAT_14259bdfc` = 11.0; `0x1406b77d6 call 0x140a76070` with `ecx = r15d`, result stored at `0x1406b77db movss [rsp+0x40], xmm0` (the desired speed), on the branch where `0x1406b77cd cmp [rsp+0x74], eax; jbe` falls through. Matches the report. Port: the cap with its gate named as a ponytail (the gate inputs are undecoded).

Read-only RE. Sources: the mid-match image `pes_full.bin` (base `0x140000000`, file offset = VA − base),
the headless decompiles in `gf-WalkSpeedRE-scratch/dec/` (`1406b7560.c`, `140a76070.c`), and capstone over the
raw bytes (via `gf-MissingRE-scratch/dispfind.py`). No GF code was edited; no live process was used.

Tags: **OBS** = read in raw bytes / disassembly / decompile at the named address. **INF(conf)** = inference.

## 1. The finding in one line

PES's set-piece walker does **not** use a gap-proportional approach speed. The MOVESTEP command filler
`FUN_1406b7560` (the "code-to-req" bridge that every MOVE-family handler reaches) carries a **walk-down
branch** that, when a run/sprint-class taker approaches a set-piece target from a distance, **overwrites the
applied desired speed (`req[0xa]`, byte `0x28`) with a fixed 11 km/h** (`FUN_140a76070(2)` → `DAT_14259bdfc`).
That is the writer the GF stand-in `2 × gap` is missing.

## 2. The chain (who reaches the writer)

```
MOVE handlers: 0x1407824A0, 0x140789DD0, 0x1407D1E60   (all call FUN_1406b8340)
        └─> FUN_1406b8340  (MOVE entry)
                └─> FUN_1406b7560  (command -> req filler; copies cmd+0x5c8 -> req[0xa])
                        └─> walk-down branch @0x1406b77d3  <── THE WRITER
                                └─> FUN_140a76070(2)  ->  DAT_14259bdfc  (11.0 km/h)
```

- The three MOVE handlers each call `FUN_1406b8340` once (OBS, decompile `dec/1407824A0.c`,
  `dec/140789DD0.c`, `dec/1407D1E60.c`: one `FUN_1406b8340` reference each).
- `FUN_1406b8340` calls the filler `FUN_1406b7560` (OBS, `dec/1406b8340.c` references it).
- The **throw-in sequencer `FUN_140791EC0` writes no command speed at all** (OBS: `dec/140791EC0.c`
  contains no `0x172`/`0x17`/`0x5c8`/speed store; it plays clips and warps the root). So the carry's
  approach is root-warped, not a MOVE command - the GF MOVE-command walk branch stands in for the general
  set-piece approach, and the walk-down is its speed writer.

## 3. The walk-down branch (OBS, `FUN_1406b7560`)

Decompiles `dec/1406b7560.c` (the command is `param_3`, uint-indexed; the req block is `param_2`):

- L53: `local_118 = param_3[0x172]` - the command's desired km/h, byte `cmd+0x5c8`.
- L51: `fVar16 = (float)param_3[0x17]` - the command's class, byte `cmd+0x5c`.
- L109: `local_118 = FUN_140a76070(2)` - **the walk-down override: a fixed rate.**
- L303: `param_2[10] = local_118` - req word 0xa (byte `0x28`) = the applied desired km/h
  (the command's, or the walk-down's). This is the word ClipWords §`[0xa]` already named
  (`cmd[0x172] desired km/h | cmd+0x5c8`).

### 3.1 The call site (OBS, disassembly)

```
0x1406b75a5: mov     r15d, 2                    ; the rate-table index: 2 (set, then NOT changed)
0x1406b77d1: jbe     0x1406b77e3                ; frame-budget gate (see 3.3)
0x1406b77d3: mov     ecx, r15d                  ; arg = 2  (r15d still 2; the only r15d write after ..5a5 is
                                                 ;   the cmove eax,r15d at ..603, which targets eax)
0x1406b77d6: call    0x140a76070                ; FUN_140a76070(2)
0x1406b77db: movss   dword ptr [rsp+0x40], xmm0 ; stage the override where cmd+0x5c8 was staged
```

The argument is **always 2** for the walk-down: `r15d` is loaded with `2` at `0x1406b75a5` and the only
`r15d`-touching instruction between there and the call is `cmove eax, r15d` at `0x1406b7603`, which writes
`eax`, not `r15d`. (The `cmove r15d, 3` at `0x1406b77f1` is *after* the call and is the separate
class-band remap of `req[0x17]`, not the rate index.) **OBS.**

### 3.2 `FUN_140a76070` - the fixed per-index rate table (OBS, script-read)

`dec/140a76070.c`:

```
if (param_1 == 1) return DAT_14259222c;   // 5.0
if (param_1 == 2) return DAT_14259bdfc;   // 11.0  <── the walk-down's rate
if (param_1 == 3) return DAT_14259be10;   // 16.0
if (param_1 == 4) return DAT_1425a2004;   // 28.0
return 0;
```

Each branch's read instruction, script-read from the image (rip-relative target of the `movss`):

| index | branch instr            | read instr                          | VA          | value (km/h) |
|-------|-------------------------|-------------------------------------|-------------|--------------|
| 1     | `0x140a76073: je ..a3`  | `0x140a760a3: movss [rip+0x1b1c181]`| `0x14259222c` | 5.0        |
| **2** | `0x140a76078: je ..9a`  | `0x140a7609a: movss [rip+0x1b25d5a]`| `0x14259bdfc` | **11.0**   |
| 3     | `0x140a7607d: je ..91`  | `0x140a76091: movss [rip+0x1b25d77]`| `0x14259be10` | 16.0       |
| 4     | fall-through (`..84`)   | `0x140a76084: movss [rip+0x1b2bf78]`| `0x1425a2004` | 28.0       |

These four values are **scattered constants, not a contiguous table** (script-read: `0x14259bdfc` = `00 00 30 41`,
its neighbours are 8.0 / 9.5 / 11.5 / 12.0). The walk-down uses index **2 = 11.0 km/h** = `3.0556 m/s`.

### 3.3 Gate: when the walk-down fires (INF high, decompile L70-108)

The override runs only when **all** hold (else the command's own `cmd+0x5c8` is used as-is):

1. action ≠ `0x4a` and match is "in play" (`match+0x116c == 1`);          (L70, `0x1406b766f`)
2. the route flag is set (`playerRec+0x25930 ≠ 0`) **and the class ≥ 3** (run/sprint); (L79, `0x1406b768d`)
3. a route/collision check passes (`FUN_140479470(...) == 0`);             (L83)
4. the taker is **far from the target**: distance-in-frames `local_e4` > frame budget `uVar18`. The distance is
   `match+0x1198 - playerRec+0x10` (`0x1406b76ef..0x1406b76f5`), the budget is `int(frames*0x14259be30/
   0x14259a764 + half)` with `0x14259be30 = 45.0` and `0x14259a764 = 60.0` (script-read), and the compare is
   `cmp [rsp+0x74], eax; jbe skip` at `0x1406b77cd`.

For a set-piece taker running to his spot from a distance under a run/sprint class, all four hold, so the
applied speed is capped to 11 km/h. This is why a run-class approach "reads as a walk" in PES, and why GF's
gap-proportional `2 × gap` (sprint class at distance) is the exact case the walk-down exists to cap.

## 4. Which set-piece roles use it, and the class it maps to

- **Shared by every MOVE-command walker**, not throw-in-specific: the walk-down sits in the general filler
  `FUN_1406b7560` that all three MOVE handlers reach. So it covers the **free-kick taker's run** (ClipWords
  §8.2 "free-kick run now sets useDesiredTarget"), the **goal-kick keeper's approach**, and any run/sprint-class
  approach to a set-piece point. The **seamless throw-in carry** does *not* go through it (root-warped, §2).
- **The class it maps to:** the override is the **class-2 rate** (`FUN_140a76070(2)`), i.e. the walk/jog band.
  The walk-down also resets the applied class to the low band (decompile L110, `fVar16 = 2.8026e-45` = float
  `1.0`; INF high - the decompiler is unreliable on the float/int here, so I do not pin the exact req word).
  Net effect: a run/sprint request is demoted to a **11 km/h walk**, which is **on the movement table**
  (table top is 28 km/h = class 4).

## 5. Why GF's stand-in goes off-table (the bug, stated precisely)

`elizacontroller.cpp:183-185` sets `command.desiredVelocityFloat = FloorVelocity(pes::kPieceTakerApproachGain
* gap)` with `kPieceTakerApproachGain = 2.0f`. `FloorVelocity` maps to the nearest level at-or-above, so for
`gap > 2.5 m` it returns sprint (`8.0 m/s = 28.8 km/h`). `FillCommandWords` turns that into `in.desiredKmh =
28.8`, **above the movement table's 28 km/h top**, so MOVESTEP reads it as no class (TURN_STOP). PES avoids
this by capping the applied speed to 11 km/h (the walk-down). GF has no walk-down, so the raw sprint velocity
is applied and goes off-table.

## 6. The exact GF port (for the TDD side - no GF code changed here)

**File:** `src/sim/onthepitch/player/controller/elizacontroller.cpp`, the set-piece walk branch
(`IsPieceTakerWalking`, `181-186`), and the same rule at `:522`.

**Change:** replace the gap-proportional velocity with the fixed PES walk rate. The applied speed becomes
`11 km/h = 11 / 3.6 ≈ 3.0556 m/s`; GF's existing `CommandSpeedClassOf` then derives a jog class (on-table), and
`FillCommandWords` writes `in.desiredKmh = 3.0556 * 3.6 = 11.0 km/h` exactly.

```cpp
// Before:
command.desiredVelocityFloat = team->GetController()->PieceTakerArrived()
                                   ? idleVelocity
                                   : FloorVelocity(pes::kPieceTakerApproachGain * gap);
// After (PES's applied walk rate; the walk-down FUN_1406b7560 @0x1406b77d3 caps the approach to FUN_140a76070(2)):
command.desiredVelocityFloat = team->GetController()->PieceTakerArrived()
                                   ? idleVelocity
                                   : pes::kPieceTakerWalkKmh / 3.6f;
```

- `pes::kPieceTakerWalkKmh` must be **read from the install at `DAT_14259bdfc`** (= 11.0) via `PeImage::ReadVa`
  (or the install's own table) per the rules - **no magic number in code or test.** `3.6f` is the km/h-per-m/s
  unit factor (`kKmhPerMps`, already in `pesmovestep.cpp:44`).
- `gap` is still used for `desiredDirection` (`toSpot.GetNormalized`); only the velocity stops being
  gap-proportional.
- The class is left to GF's `CommandSpeedClassOf(velocity)` (jog at 3.0556 m/s), matching the walk-down's
  low-band demotion without porting the full gate (which needs `match+0x116c`, `playerRec+0x25930` and
  `FUN_140479470`, none decoded).
- **TDD red test:** a throw-in/free-kick taker at a large gap (e.g. 10 m) must approach at `11/3.6 m/s`,
  NOT `FloorVelocity(2*10)`; assert `desiredVelocityFloat == 11.0f/3.6f`. Mutation: restore `2 × gap` → red.
- **Do not** port the walk-down gate itself (lazy: the fixed-rate correction is the observable behavior;
  the gate's three undecoded inputs are an upgrade path only if a set-piece role is shown to need the
  class≥3 demotion explicitly).

## 7. Self-checking checker

`gf-WalkSpeedRE-scratch/check_walkspeed.py` verifies every cited VA→value and every cited instruction
against `pes_full.bin`, then runs a mutation battery (each mutant must be caught). It parses the report's
own claims (rate-table rows, call-site asm lines, required function/word citations, gate VAs and the
frame-budget constants) and cross-checks each against the image, so a wrong value, wrong read-instr, wrong
call arg, or dropped citation in the report fires the corresponding check. Run:

```
python3 gf-WalkSpeedRE-scratch/check_walkspeed.py gf-WalkSpeedRE-scratch/pes-re-walk-speed.md
```

Result on the current report: clean pass, `RESULT: PASS`, and all 10 battery mutants caught.

## 8. Open items

- The **AI emitter's own write of `cmd+0x5c8`** (the base desired km/h before the walk-down) is not decoded -
  it is written through a register helper invisible to the disp32 scan (only 6 float stores to `cmd+0x5c8`
  exist, all in non-MOVE functions). For a walk/jog-class taker (class < 3) the walk-down does not fire, so
  the emitter's value is the applied speed there. Not needed for the port (the fixed-rate correction covers
  the run/sprint case that goes off-table), recorded here for completeness.
- `match+0x116c` (in-play), `playerRec+0x25930` (route flag) and `FUN_140479470` (route/collision) are the
  walk-down's undecoded gate inputs (§3.3, INF high).
