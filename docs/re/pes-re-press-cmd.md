# PES 2021 press command: how a presser at rest is commanded to run

Read-only RE. Scratch: `<workdrive>/gf-PressCmd-scratch/` (`d_140962440.c`, `think_140929a00.c`,
`fn_14092a340.c`, `verify.py`, store-scan scripts). No GF code changed; no PES written; no live
read relied on (PES was stopped during this pass).

Base `0x140000000`, image `~/ghidra-projects/scratch/Unpacker/pes_full.bin` (file offset == VA − base
for the code/rdata). Tags: **CONFIRMED** = read in a decompile or the raw image bytes this pass
(or a prior doc whose VAs I re-verified). **INF** = inference with the reason. **OPEN** = blocker.

## 0. The one-line answer

PES's `ActionPress` (`FUN_140962440`) does **not** build the MOVE command. It only computes and
returns the **target point** (the ball sample or the presser's own base) and feeds the presser's own
pace class to the **ETA planner**. The MOVE command's class/speed/rate is written by a separate
**emitter** (the AI think's movement branch), and the rate that keeps the man from cycling
IDLE↔RUN_LOOP is the **class-derived band rate** (the `useClassRate` path, `cmd+0x5c5`), not a
speed-derived `SpeedToRate`. GF's stall is that it sends a jog-class press with **no** `useClassRate`
writer, so the rate falls to `SpeedToRate(12.6 km/h) = 0.182 < 0.3` and RUN_LOOP hands to IDLE.

---

## 1. `ActionPress` `FUN_140962440` — the command it "builds" (CONFIRMED decompile `d_140962440.c`)

Signature: `float * (action, out3, ctx)`. `param_3+4` = slot id, `+0x38` = ctx, `+0x40` = planner owner.
Dispatched only on PRESS state `0x34`; zero `E8` callers (action-table dispatch, INF).

It returns `param_2` = a **3-float target point** (x, z, w). It writes **no** command-record word
(class / speed / rate / `0x5c5`). What it does, in order:

1. `target = FUN_140639100(&fStack_e8, ctx, slot, mustPress = carrierInvalid, …)` — the press target
   (the ball-now or the presser's own base; `pes-re-press-life.md` §7, CONFIRMED).
2. Formation-shift arm (`FUN_14234E8E0 == 0x19`, mode 4): if the bearing (man→target) vs (man→anchor)
   differs by `> 10°` (`0x142588210`), shift target `0.7·(anchor − man)` (`0x14257A194`).
3. `action+0x74 = 0`; if `action+0x58 == 0`, the stored point `action+0x68/6c/70` takes `target`.
4. If `action+0x5C != 0` (INF: has-base flag): `target = AI(man)+0x1420/24/28` (the presser's base).
5. Touch-frame arm (`MI.frame − newest.frame < 1`): stored point = man pos advanced 5 m
   (`0x14259222c`) along man→target; return.
6. **The planner call (this is the speed/class-relevant part):**
   - bearing man→target (`FUN_140439E50`);
   - `local_18 = (FUN_140A75B20(pl) == 2)` (a stamina-adjacent flag, OPEN);
   - `eta = FUN_140A4D350(BallInfo, target)` (ball ETA to target, ticks);
   - **`cls = FUN_140A75BE0(pl)`** — the **presser's own pace class 0..4**;
   - `n = FUN_140678950(plannerOwner, pl, target, cls, local_18, 2, bearing, 0, 1, 0)` (man ETA, ticks);
   - `action+0x30 = (n + 10 < eta)` (INF: re-plan flag).
7. Stop/arrival arm (`action+0x48 < 0x16`, a marked peer exists): may rewrite the target in place via
   `FUN_1406378F0` and converge it onto the man; sets `action+0x78`.
8. Write-back stored→`action+0x68`; return `out`.

**The pace class `FUN_140A75BE0(pl)` (CONFIRMED decompile; cuts re-verified against the image this pass):**
```
if pl.u8[+0x3F4] == 0: return 0
v = pl.f32[+0x3FC + 16·pl.u8[+0x108]]  (km/h, the man's own top speed)
v ≤ 0.05 → 0; ≤ 8 → 1; ≤ 13.5 → 2; ≤ 22 → 3; else 4
(0.05 @0x1425A8A28, 8 @0x14259BDF4, 13.5 @0x1425A1FFC, 22 @0x1425C48C0 — all OK in verify.py)
```
A typical outfielder (top speed 24–30 km/h) is **class 4**. A slow/late presser is class 2 or 3.
**This class is passed to the ETA planner only — it is the "how fast will he get there" class, used to
pace the route, not (by itself) the MOVE command's class word.** (INF, high: the planner is a
passthrough `FUN_1406788B0 → FUN_1406B9270` movement solver returning `max(1, int(fps·dist))` ticks.)

So `ActionPress` answers: **target = ball-now or own base; the man's own pace class (0..4) drives the
ETA; the actual MOVE command class/speed/rate is NOT set here.**

---

## 2. The command record `cmd`, its words, and the `cmd+0x5c5` writer

The command record is a **separate object** from the action. The MOVESTEP filler
`FUN_1406B7560` (`docs/re/pes-re-decision-read.md` §2, CONFIRMED) reads from it:

| `req` word | source in `cmd` | meaning |
|---|---|---|
| `req[0]` | `cmd[0x16]` (8→2) | move kind |
| `req[0xE]` | `cmd[0x15]` (2→1) | precise flag |
| `req[9]` | `cmd[0x17]` = **`cmd+0x5C`** | **command speed class** (mapped 0..3 by `FUN_1406B8DE0`, else −1) |
| `req[0xA]` | `cmd[0x172]` = **`cmd+0x5C8`** | **desired speed, km/h** |
| `req[0xB]` | **`cmd+0x5C5`** | **`useClassRate` byte** — "use the class rate" |
| target | `cmd+0x20` | move target (x, z) |
| look | `cmd+0x1A` | look-at point |

(`cmd` is `uint`-indexed: `cmd[0x172]` = byte `0x172·4 = 0x5C8`; `cmd[0x17]` = byte `0x5C`.)

**Who writes `cmd+0x5C5` / `cmd+0x5C` / `cmd+0x5C8` — the emitter (INF, high).** The AI think
`FUN_140929A00` (CONFIRMED decompile `think_140929a00.c`) writes only the **decision code**
`A+0x8D4` (codes 0x13/0x14/0x19/0x1B/0x1C/0x1D). Its movement branch `FUN_14092A340`
(CONFIRMED decompile `fn_14092a340.c`) then **copies the AI's ball-request sub-record `A+0x874`
(0x874..0x8C4, written by `FUN_1409257B0`) into the command record** (`param_1+0xa390`), word by word
including the class word at `+0x17`. So the command's class/speed come from the **ball-request /
movement-intent sub-record `A+0x874`**, not from `ActionPress`.

- The **direct byte writers** of `cmd+0x5C8` (the speed float) are **register-relative stores invisible
  to a disp32 scan** — the walk-speed doc (`pes-re-walk-speed.md` §8) already established this: only
  6 float stores to `cmd+0x5C8` exist, all in non-MOVE functions, and the AI emitter's own write is
  through a register helper. My SSE store-scan (`ssestore.py`) confirms the 9 `cmd+0x5C8` stores are
  scattered (the filler's own copy at `0x1406B75B5` plus non-AI sites); **none is a single identified
  "press emitter" function.** The emitter's speed write is OPEN to pin byte-for-byte.
- The `cmd+0x5C5` "writer" my earlier disp32 scan flagged (`0x1407C25AB`) is a **false positive**:
  `0x5C5` is not 4-byte aligned, so a `C7 /0 imm32` to `+0x5C5` is not a real store; the bytes are a
  data float. **CONFIRMED: there is no simple immediate store to `cmd+0x5C5` in the image.** It is
  written as part of the sub-record copy (INF) or left at its default.

**The meaning of `cmd+0x5C5` (CONFIRMED from the port's read, `movestep.hpp:154`):** it is the
`useClassRate` flag. When set, the requested rate is the **class's band rate** (`FUN_1406B9E50` table
`UNK_1425DDE10`), **not** the speed-derived `SpeedToRate` (`FUN_1406B9E80`). This is the lever that
decides the man's rate at rest.

---

## 3. The MOVESTEP path from rest under the press command

### 3.1 The rate the man runs at — `RequestedRate` (port `movestep_run.cpp:177`, PES `FUN_1406B9FF0`)
```
useClass = req[0xB]   // the cmd+0x5C5 flag
cls      = req[9]     // command speed class 0..3 (or -1)
if (useClass)  return BandRateFor(cls, kind);          // FUN_1406B9E50: class -> band rate
return SpeedToRate(desiredKmh);                        // FUN_1406B9E80: speed -> rate
```
`BandRateFor(cls, kind)` (port `movestep_run.cpp:315`, PES `FUN_1406B9E50`):
- `cls == 0 && kind ∈ {3,6}` → **0.0** (band 0, run kind: no rate);
- else `bandRate[clamp(cls,0,3)]` from `UNK_1425DDE10` (a 4-float table, class 0..3).
`SpeedToRate` (port `movestep_common.cpp:356`, PES `FUN_1406B9E80`): piecewise-linear on
`(kmh / 3.6) / physiqueG` against the speed-rate table.

**This is the crux.** At a speed of 12.6 km/h the speed-derived rate is **0.182** (CONFIRMED by the
LooseBall trace; `SpeedToRate(12.6)` in the port). At 13.5 km/h it is only marginally higher —
**still well below 0.3.** So **no presser at ≤ ~13.5 km/h can hold RUN_LOOP on the speed path.**
The only way the rate reaches ≥ 0.3 is the **`useClassRate` path with a high enough class**
(`bandRate[cls] ≥ 0.3`, i.e. the run/sprint class), **or** commanding a genuinely high speed.

### 3.2 Which state takes the presser from rest
- **ACCELERATE** (state 9, `FUN_1406CCD00`, port `EnterAccelerate` `movestep_run.cpp:2510`) — arm 1
  refuses unless **`cls == 3`** (the top mapped class, = dash/sprint command class 4), plus:
  band word ≥ threshold, precise clear, rate < 0.65 (`0x14259D5D0`), ground speed > 11 km/h
  (or > 0.5 when the turn is under 67.5°), look distance > 2 m. **A jog/walk-class press (cls < 3)
  never reaches ACCELERATE.** CONFIRMED (port + PES decompile comment).
- So a presser at a non-sprint class goes **IDLE → GOTO_RUN → RUN_LOOP**.
- **RUN_LOOP's keep test** (port `movestep_run.cpp:562`, PES `FUN_1406D5D60` the IDLE requester;
  `docs/re/pes-re-runloop-arms.md` §2 E3): **`req[0x4A] (rate) < 0.3` (`DAT_142590D98`) → IDLE.**
  CONFIRMED.

### 3.3 How PES avoids the IDLE/RUN cycle (INF, high — the mechanism)
Because the speed path can't produce ≥ 0.3 for a ≤ 13.5 km/h presser, PES keeps the presser moving by
one of (both point the same way):
1. **the presser command carries `useClassRate` (`cmd+0x5c5`) set and a high command class**, so the
   rate is `bandRate[cls]` (high, ≥ 0.3) and RUN_LOOP holds / ACCELERATE is reachable; **or**
2. **the presser is routed at the walk/low class to the STEP family**, which does not gate on the
   RUN_LOOP 0.3 test (the LooseBall doc open item #1 notes "PES's press sends the presser's own pace
   class `FUN_140A75BE0`, 1 = walk, which would route to the walk/STEP family").

Either way the press rate is **not** `SpeedToRate(12.6) = 0.182`. **This is the answer to "or whether
its press rate is never that low": in PES the rate the man runs at is class-derived (or the man is in
the STEP family), so it is never the sub-0.3 speed-derived value.**

### 3.4 What "class 3" is
In the port's mapping (`CommandSpeedClassOf` `pesmovestep.cpp:46`): none=0, jog=2, run=3, dash=4
(GF velocity bands). `FUN_1406B8DE0` (`CommandClassFor`, port `movestep_run.cpp:308`, table
`UNK_1425DDCD8`) maps the 1..5 command class into the 0..3 **class word** `req[9]`. So **class 3 = the
top band (dash/sprint)**. `EnterAccelerate`'s `cls == 3` therefore means "commanded at the top speed
class." (The exact 1..5→0..3 table values are install reads; the mapping shape is CONFIRMED from the
decompile, the table bytes are OPEN to cite individually.)

---

## 4. `SpeedToRate` and the press speed quantisation

- **`SpeedToRate` (PES `FUN_1406B9E80`, port `movestep_common.cpp:356`):** `(kmh/3.6)/physiqueG`
  through a piecewise-linear speed→rate table, clamped to `[0, 1]`. **13.5 km/h → rate just above
  0.182, still < 0.3.** So a 13.5 km/h press cannot hold RUN_LOOP on the speed path. CONFIRMED
  (table shape + the 12.6→0.182 trace point).
- **The 13.5 → 12.6 snap is a GF stand-in, not PES.** `elizacontroller.cpp:918` runs `RangeVelocity`
  (`velocitystate.hpp:27`) on every PES-loop movement command, "emulate controller quantization"
  (a named ponytail). `PressCommandSpeed_ms` (`pespress.cpp:603`) caps a class-1 presser's command at
  `speedCut2/3.6 = 13.5/3.6 = 3.75 m/s`; `RangeVelocity(3.75) = 3.5 m/s = 12.6 km/h`. **PES's own
  press speed is the uncapped `PressCommandSpeed_ms` value (≤ 13.5 km/h for a class-1 presser), and
  there is no evidence PES quantises AI speeds to 0.5 m/s steps.** (INF, strong — LooseBall open #3.)
- **What rate does PES produce for the press?** On the speed path: ~0.19 (sub-0.3, would stall).
  **On the class path (the one that keeps him moving): `bandRate[cls]`, which for a run/sprint-class
  presser is ≥ 0.3.** The presser in PES is commanded such that his **effective rate ≥ 0.3** — that is
  the observable difference from GF.

---

## 5. Which of these are translated and proven (so the fix can switch to them)

Checked every `tools/dev/translate/results*.json` (`results`, `results_core`, `results_cutscene`,
`results_impure`, `results_presentation`, `results_snapdiff`) for the key VAs:

| function | VA | in results? | proven? |
|---|---|---|---|
| **ActionPress** | `0x140962440` | **no** | not translated |
| press target `FUN_140639100` | `0x140639100` | yes (impure + snapdiff) | **`translated=true`, NOT proven**: impure has 101 cases, **1 mismatch** (case 6, `w_A0_0_d0`), `diffed=false`; snapdiff "no replayable call (0 cases)" |
| pace class `FUN_140A75BE0` | `0x140A75BE0` | **no** | not translated |
| ETA planner `FUN_140678950`/`8B0` | `0x140678950`/`8B0` | **no** | not translated |
| movement solver `FUN_1406B9270` | `0x1406B9270` | **no** | not translated |
| filler `FUN_1406B7560` | `0x1406B7560` | **no** | not translated |
| `SpeedToRate` `FUN_1406B9E80` | `0x1406B9E80` | **no** | not translated |
| `EnterAccelerate` `FUN_1406CCD00` | `0x1406CCD00` | **no** | not translated |
| RUN_LOOP `FUN_1406C0170` | `0x1406C0170` | **no** | not translated |
| class map `FUN_1406B8DE0` | `0x1406B8DE0` | **no** | not translated |
| think `FUN_140929A00` | `0x140929A00` | **no** | not translated |

**Bottom line:** the presser's command path is **not** in the proven/translated set. The only one in
scope is `FUN_140639100` (the target), and even that is translated-but-not-proven. **The fix cannot
"switch to" a proven translation — it must be ported from the decompile/spec, TDD-style, with the
PES command words as the target.** (This matches the house rule: proven = every case bit-exact;
coverage is reported, not a gate.)

---

## 6. Fix recipe for GF (what differs from PES, with VAs)

The stall is a **rate problem**, not a quantisation problem (LooseBall E1: bypassing `RangeVelocity`
made it *worse*, 477 ticks; E2: setting `useClassRate` alone gave no gain, 376 ticks). The presser's
**effective rate** is `SpeedToRate(12.6) = 0.182 < 0.3` because GF (a) sends a low command class and
(b) **never sets `useClassRate`**, so the class-derived band rate is never used.

### Divergences (GF → PES)
1. **No `useClassRate` writer.** GF `FillCommandWords` (`pesmovestep.cpp:197`) never writes
   `kUseClassRate` (`req[0xB]` / `cmd+0x5C5`). → `RequestedRate` (`movestep_run.cpp:177`) always
   takes the speed path. **PES writes it** (INF, high — it is the `cmd+0x5C5` word the filler reads).
2. **Command class from speed, not from the man's pace.** GF sets
   `in.commandSpeedClass = CommandSpeedClassOf(speed)` (`pesmovestep.cpp:222`) → a jog class for a
   12.6 km/h press. **PES's presser command carries the presser's own pace class**
   (`FUN_140A75BE0`, `0x140A75BE0`) so the run decision sees the right class (INF, high —
   `ActionPress` step 6 passes it to the planner; LooseBall open #1).
3. **`RangeVelocity` on the PES path** (`elizacontroller.cpp:918`) snaps 13.5 → 12.6 km/h. A GF
   stand-in; PES AI speeds are continuous (INF).
4. Downstream (both already ported faithfully, **do not change**): RUN_LOOP keep test
   `req[0x4A] < 0.3 → IDLE` (`movestep_run.cpp:562`, `DAT_142590D98`); `EnterAccelerate`
   `cls != 3 → refuse` (`movestep_run.cpp:2533-2536`, `FUN_1406CCD00`).

### The change (smallest PES-faithful form)
Two things are required **together**, and the two LooseBall experiments explain why neither alone worked:
- **`useClassRate` alone (E2) fails because the class is still the speed-derived jog class.** With
  `cls = 0`/jog, `BandRateFor(0)` is the band-0 rate (and `0` for kind 3/6) — still sub-0.3. The flag
  only helps once the **class** is the man's pace class.
- **Class alone (E1) fails because without `useClassRate` the class is ignored by `RequestedRate`**
  and the rate stays `SpeedToRate(speed)`.
So the press command must carry **(i) the presser's own pace class as the command speed class** AND
**(ii) `useClassRate` set**, so `RequestedRate` returns `bandRate[cls]` (≥ 0.3 for a run/sprint-class
man) instead of `SpeedToRate(12.6)`. Concretely:
- `elizacontroller.cpp` press block (≈`:897-916`): tag the press command with the presser's
  `PressSpeedClass` (already computed via `pes::PressSpeedClass`, the GF mirror of `FUN_140A75BE0`)
  and a "use class rate" intent.
- `pesmovestep.cpp FillCommandWords`: for a press command, set
  `in.commandSpeedClass = <presser pace class>` and set `kUseClassRate` in `req` (the new writer).
- Keep `desiredKmh` at the (unquantised) `PressCommandSpeed_ms` value so the man's actual speed is
  still ~≤ 13.5 km/h; only the **rate** becomes class-driven.

**Caveat (why this is still a hypothesis, not a proven fix):** the class that `BandRateFor` must
read is `req[9]` (the mapped 0..3 command class). `bandRate[3] ≥ 0.3` is the assumption the fix rides
on — it is **OPEN** until `UNK_1425DDE10` is read (§8.2). If the band-0/1 rates are what a
jog-class presser maps to, the fix must instead route the presser to the **STEP family** (§3.3 alt. 2),
which is the other documented possibility. The red test in §6's "why this is the root-cause fix"
decides which; do not land until the live `cmd` words (§8.1) are CONFIRMED.
- (Optional, higher fidelity) drop the `RangeVelocity` snap for the press command so the speed is the
  true 13.5 km/h — but this alone is not the fix (E1).

**TDD red test:** a loose-ball presser standing at rest must, over N ticks, reach RUN and keep it
(no IDLE↔GOTO_RUN cycle) and arrive within the LooseBall budget (seed 7 ≤ 270, seed 3 ≤ 270).
Mutation: clear the `useClassRate` writer (or force class 0) → the IDLE cycle returns → red.

### Why this is the root-cause fix (not a band-aid)
It changes the **rate source** to the class-derived band rate — the exact PES mechanism — at the one
shared site (`RequestedRate`) that all locomotion routes through. It does not special-case the press
in the FSM, does not lower the 0.3 keep test, and does not weaken `EnterAccelerate`. The 0.3
(`DAT_142590D98`) and the `cls==3` gate are PES's and stay.

---

## 7. Self-check / constants verified this pass (`verify.py`, against `pes_full.bin`)
All **OK**: keep-rate `0x142590D98`=0.3; class cuts `0x1425A8A28`=0.05, `0x14259BDF4`=8,
`0x1425A1FFC`=13.5, `0x1425C48C0`=22; walk-down rates `0x14259BDFC`=11, `0x14259222C`=5,
`0x14259BE10`=16, `0x1425A2004`=28; target tick `0x142588200`=0.1, `0x142565600`=0.5; fps
`0x14351AE54`=54; press slope `0x142588210`=10.

## 8. OPEN items (blockers for a fully CONFIRMED fix)
1. **The live presser command words** — the exact `cmd[0x17]` (class), `cmd+0x5C8` (km/h) and
   `cmd+0x5C5` (`useClassRate`) a real PRESS man carries. Needs one rendezvous capture of a
   presser's `cmd` (or a decode of `FUN_1409257B0`'s class/speed write for a press). This would
   promote the §3.3 / §6 mechanism from INF to CONFIRMED.
2. **`UNK_1425DDE10`** (the `BandRateFor` table) and **`UNK_1425DDCD8`** (the `FUN_1406B8DE0`
   class map) — read the 4/5 floats and cite them so the fix's "≥ 0.3" threshold is a named install
   value, not an assertion.
3. Whether the presser is instead routed to the **STEP family** (the §3.3 alternative) — decode the
   class→family routing the filler/handlers use.
4. `FUN_140A75B20` (the `local_18` stamina flag) semantics.

## 9. Recommended next step
Capture one presser's `cmd` words live (or decode `FUN_1409257B0`) to CONFIRM `cmd[0x17]` and
`cmd+0x5C5` for a PRESS man; then implement the §6 change with the red test. Highest value because
it closes the only remaining INF on the causal chain and unblocks re-enabling the disabled
loose-ball test.
