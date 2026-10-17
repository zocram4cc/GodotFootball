# PES 2021 — open-play kick horizontal alignment (KickAlignRE)

Read-only static RE, 2026-10-14. Flat live image base `0x140000000`, decompiles via
`dec.sh`+`annot.py`, raw via `pdis.py`, call sites via a local rel32 scanner, constants read
directly from the image. Scratch: the KickAlignRE scratch dir (`ka1..ka7.c`, `calls.py`).
Tags **OBS** = observed at the named VA / from the named decompile; **INF(c)** = inferred.
`P` = `match::AnimePlayer`, `req` = per-tick move-request, `motion` = the 0x390-B motion object.

Closes `docs/re/pes-re-anim-select-2.md` item 5 ("how open-play kicks line the kicking foot up
with the ball horizontally"). Premise carried in: `FUN_1407302b0` (the open-play kick writer)
provably touches only `P+0x498` (height) and `P+0x4d0` (yaw) — **no** `P+0x428`/`AdjustTransXZ`
(AS §6.2, AL-12).

## 0. Headline

**An open-play kick does NOT line the foot up with a per-kick horizontal correction.** The
`AdjustTransXZ` root-pull that exists for *touches* is a **separate, table-dispatched
step/touch writer (`FUN_14073f13c`)** that the open-play kick path never calls. The kick's
horizontal alignment is carried by the **approach run-up to a ball-offset move target**
(`FUN_14072f670`) plus a **player-scale reach gate** (`FUN_14072f3c0`): by the time the ball is
in reach the running player's foot is already in line, so the kick writer only adds height +
yaw. **Hypothesis (b) — offset move target — is the answer; (a) — an earlier-registered
correction — is the mechanism for the dribble touch, not the kick.**

Correction to the carried-in note: the two `AdjustTransXZ` argument blocks the AS doc placed in
`FUN_14073f750` are in **`FUN_14073f13c`** (the cited VAs `0x14073f3eb`/`0x14073f666` both fall
inside `FUN_14073f13c` `[14073f13c, 14073f735]`, which is below `FUN_14073f750`). `FUN_14073f750`
has only **one** `AdjustTransXZ` (`P+0x428`) — OBS (ka1.c:179).

---

## 1. The three decompiles asked for

### 1.1 `FUN_1406b10d0` — hit frame → ticks  (ka1.c:1-34, OBS)
```
int FUN_1406b10d0(motion*, motionId, rate, index=0):
    motion = *motion_table + motionId*0x390
    rec = *(u32**)(motion + 0x90 + index*8)   // per-motion contact table, NOT the +0x20 HitData
    if (motion[4] <= index) or rec == 0:  rec = 0x1436f23d8      // default
    f = (byte)((char)(rec_u32 >> 13) + 2)                       // hit frame = bits13..22 + 2
    return (int)( (2*f / rate) * (engineRate / 60.0) + 0.5 )    // round(...)
```
- **Hit frame** = bits 13..22 of the per-motion contact record's u32, **+ 2** (the "+2 frame"
  offset). `rec` is the `+0x90` table (a contact/kick-specific record distinct from the
  `+0x20` HitData and `+0x48` SubHitData of AS §5.3) — INF(med) it is a ball-contact-specific
  table; the `+0x90` vs `+0x20` distinction is OBS from the decompile.
- **Hit ticks** = `round( 2·(hitFrame)·engineRate / (rate·60) )`. `engineRate = func_0x141e532d0`
  (≈57.0), `rate` = playback rate (clamped to ≥0→1.0). Constants: `_DAT_14259a764`=**60.0**,
  `_DAT_14256ae1c`=**1.0**, `_DAT_142565600`=**0.5** (the round+0.5) — OBS. So the "×2" and the
  engine-rate/60 scaling match AS §5.3's cancel-frame formula `round((2·f/rate)·(54/60))`, with
  the live engine rate (≈57) standing in for the nominal 54.
- `FUN_1406b03d0` (ka1.c:362+) is the twin using the `+0x30` table, frame offset `+0`, and
  `(uVar2+uVar2)+1` — the "current line" variant. Both return the same *shape* (hit-frame→ticks).

### 1.2 The two `AdjustTransXZ` argument blocks — `FUN_14073f13c` (ka2.c:103-130 & 197-224, OBS)
Both blocks are byte-identical in structure (the two foot-variant branches). Each:
```
sVar4  = func_0x1406abb30(row, col, 0)          // the clip's contact/hit frame (short)
N      = (int)sVar12 + 1                          // spread window = hitFrame + 1 ticks
tgt    = FUN_1406e85b0(hitTicks, out, row, ball, angle)   // ROOT TARGET (below)
spr    = FUN_1406e8480(out2, row, angle)                    // direction spread
spr    = FUN_1404bcad0(spr, bodyYaw)                         // rotate spread by yaw
cur    = func_0x141eb50b0(ball, out3, 1)                     // current root (P+8)
FUN_141eb39a0(P+0x428, tgt, P+8, N, ...)                     // AdjustTransXZ: spread cur->tgt over N
func_0x141eb3ae0(P+0x428)
FUN_141eb35a0(P+0x4d0, yaw, yaw+angle, N, ...)               // AdjustAngle (yaw spread, same N)
```
- `FUN_141eb39a0` (ka3.c:113-162) stores per-axis deltas `(tgt − cur)/N` into `P+0x428` and a
  per-tick counter `P+0x428+2c` — i.e. a **linear** position correction from the current root to
  the target over exactly `N` ticks (no easing). This is the `AdjustTransXZ` primitive.
- **Root target** `FUN_1406e85b0` (ka2.c:249-271, OBS): `T = ball − R(yaw)·d` where
  `d = FUN_1406e8b70(motionId, ...)` (ka3.c:5-39) = the clip's hit-frame **root→foot offset**
  (`FUN_1406aff90 − FUN_1406b05e0`), scaled by `*(P+0x3b18)` (player size). So
  **`T = ball − R(yaw)·(hitFrame rootOffset · playerScale)`** = the root position that puts the
  clip's hit-frame foot on the ball. `param_2[1]=0` → 2D (pitch plane).
- Rows selected: `0x4f5/0x4f6/0x4f7` (by `*vtable+0x28 == 0x29/0x2a/0x2b`) and `0x4f4`
  (18-bucket angle) — the **step/touch** rows, not the kick rows.

### 1.3 The open-play kick dispatch — `FUN_14072f3c0` (ka4.c:1-120, OBS)
```
v = func_0x141eb51a0(P, out, 10)               // read per-player slot-10 3-float vector
f12 = func_0x1406e6a40(v, 8);  f13 = func_0x1406e6a40(f12, 7)   // player-scale transforms
c = func_0x140a6e450(*(char*)(RBX+0xacb))
if (c == 0) {
    if (out._4_4_ < f13·0.5 + f12·0.5)   FUN_1407302b0();   // ball IN reach  -> KICK
    else                                 FUN_14072f670();   // ball far       -> run-up
} else {  FUN_140730000(); }
```
- **Reach gate** `out._4_4_ < 0.5·(f12 + f13)` — a player-size-relative distance threshold
  (`_DAT_142565600`=0.5 OBS; the two scale transforms are INF, nominally ≈1.8 m reach).
- **Call-graph (OBS, `calls.py`):** `FUN_14072f3c0` calls exactly `{FUN_1407302b0 (kick),
  FUN_14072f670 (run-up), FUN_140730000}`. It **never** calls `FUN_14073f13c`.
- `FUN_14073f13c` has **no direct call sites** (rel32 scan) — it is **table-dispatched** (the
  anim-row handler table `0x142edc2xx`), so it runs as an independent step/touch, not as part of
  the kick sequence.

### 1.4 The run-up — `FUN_14072f670` (ka4.c:123-437, OBS)
Sets `P+0x245c = 1` (MOVESTEP state), reads slot-10 (the same reach vector, `fVar26 = fStack_134`),
computes a target via `func_0x141eb50b0` + `FUN_1406e8b70` (ball-offset root target, same helper
as the touch), and drives the move solver `FUN_14072c840(param_1, P, param_3, ...)`. **No
`AdjustTransXZ` / `P+0x428`** in this function (OBS). It is the **offset move target**: a
locomotion run toward a point derived from the ball, not a root-pull correction.

---

## 2. The formula — what offset, from what, over which frames

**For the open-play kick itself: there is NO horizontal offset.** Alignment is
`approach run-up to (ball − R(yaw)·reachOffset)`, gated by a player-scale reach; the kick writer
`FUN_1407302b0` then adds only `P+0x498` height + `P+0x4d0` yaw.

**The horizontal foot-alignment offset exists only in the touch writer `FUN_14073f13c`:**
```
target T  = ball − R(yaw)·(hitFrame rootOffset · playerScale)      // FUN_1406e85b0
offset Δ  = T − currentRoot(P+8)
applied   = Δ spread linearly over N = (hitFrame + 1) ticks         // FUN_141eb39a0
(+ a direction spread FUN_1406e8480 rotated by body yaw, same N)
```
So: offset = **(ball − yaw-rotated hit-frame foot-offset) − current root**, applied
**linearly over `hitFrame+1` ticks**.

## 3. MOVESTEP-dwell comparison → (a) vs (b)

- **MOVESTEP dwell** (AS §5.2, OBS): 0.1–0.3 s = ~5.4–16.2 ticks.
- **Hit-tick window** for the touch correction: `N = hitFrame+1`; hitFrame from §1.1 is
  typically ~4–8, so `N ≈ 5–9` ticks — the **same order as the dwell**, and the correction is
  registered at the start of the step and spreads over it. For a *touch* the root IS pulled by an
  earlier-registered correction over the step (hypothesis (a) applies to touches).
- **For the kick** there is no such correction to compare: the dispatch (1.3) never reaches the
  XZ writer, and the run-up (1.4) carries the player to a ball-offset **move target** over the
  whole approach, not a correction over a dwell. **Hence (b): offset move target.**

## 4. GF counterpart (`src/onthepitch/player/humanoid/humanoid.cpp`)

GF has no two-phase "run to offset target, then kick." Its kick approach,
`GetBestCheatableAnimID`, computes the horizontal alignment as an **explicit root smuggle** and
always applies it — the direct analog of PES's `AdjustTransXZ`:
- `actionSmuggleVec2D = (ballPos − animBallPos).Get2D()` (3172) — offset to put the clip's
  foot-anchored ball position on the ball (= PES `T − cur`, from §2).
- Gate: `ballDistanceZ < 0.22f` (3202) **and** `bodyBallDistanceAdvantage ≥ 1.0f` (3320) — the
  analog of PES's player-scale reach gate.
- `fullActionSmuggle_ret = bestActionSmuggleVec2D.GetRotated2D(angle)` (3341), with cheat-discard
  scaling (3347-3428).
- **Applied over frames** (1332-1334):
  `actionSmuggleMovement = (actionSmuggle / (touchFrame + 1)) · value · 100;
   actionSmuggleOffset += actionSmuggleMovement / 100;` with `value = value·0.1 + 0.9` — a
  **cosine-eased** ramp over `touchFrame+1` frames (PES §2 spreads the same window *linearly*).

**What GF code each finding replaces:**
- **PES open-play kick = offset move target + reach gate, no XZ** → in GF this corresponds to the
  kick approach in `GetBestCheatableAnimID` (reach gate 3320, `actionSmuggle` 3172/3327/3341,
  spread 1332-1334). To reproduce PES's *kick* behavior, GF would **disable** the `actionSmuggle`
  root-pull for open-play kicks (GF currently smuggles for every touch incl. kicks).
- **PES dribble touch = `AdjustTransXZ` root-pull over `hitFrame+1`** → GF's `actionSmuggle`
  (same lines). This is the one that **already matches** PES's touch mechanism (modulo cosine vs
  linear easing and the cheat-discard terms).

## 5. Open items

1. **`FUN_14072f3c0` reach-gate scale** — the two `func_0x1406e6a40(v,8)/(·,7)` transforms are not
   pinned; the ≈1.8 m reach is INF from `_DAT_142565600`=0.5 + player-scale reasoning. Next step:
   decompile `FUN_1406e6a40` and the slot-10 vector loader `FUN_141eb51a0` to name the exact
   threshold. (Doesn't change the conclusion — the gate's *existence* and *call graph* are OBS.)
2. **`FUN_14072f670` "run-up" label** — OBS that it sets MOVESTEP=1, uses `FUN_1406e8b70` for a
   ball-offset target, and drives `FUN_14072c840`; INF(high) that this is the kick approach vs a
   generic chase. Would be settled by a live trace of `P+0x245c`/move-target during a kick, which
   is out of scope here (static-only).
3. **`+0x90` vs `+0x20` contact table identity** — OBS the two tables differ; INF(med) `+0x90` is
   a ball-contact/kick-specific record. Next step: find the `+0x90` table loader (`FUN_1406b0ed0`
   neighborhood) and its record layout.

## 6. Ledger addenda (IDs continue the anim-select-2 ledger)

| ID | Claim | Subsystem | Evidence | Method | Confidence | Dependents |
|----|-------|-----------|----------|--------|------------|------------|
| KA-1 | `FUN_1406b10d0` = hit-frame→ticks: `round(2·(bits13..22 + 2)·engineRate/(rate·60))`, rec from motion `+0x90` table | kick/timing | ka1.c:1-34; consts 60.0/1.0/0.5 | static | OBS | — |
| KA-2 | The two `AdjustTransXZ` blocks are in `FUN_14073f13c`, not `FUN_14073f750` (AS doc misattribution); 750 has one XZ | kick/touch | ka2.c:129,223; ka1.c:179; VAs 0x14073f3eb/0x14073f666 ∈ 13c | static | OBS | item 5 |
| KA-3 | Touch XZ: `T = ball − R(yaw)·(hitFrame rootOffset·scale)`, spread linearly over `hitFrame+1` ticks | touch | ka2.c:103-130,197-224; ka3.c:5-39,113-162 | static | OBS | §2, GF 1332-1334 |
| KA-4 | Open-play kick dispatch calls {302b0,2f670,30000} only; never the XZ writer; 13c is table-dispatched (no call sites) | kick | ka4.c:1-120; calls.py | static | OBS | KA-5 |
| KA-5 | Open-play kick horizontal alignment = offset move target (run-up `FUN_14072f670`) + reach gate, **not** an earlier-registered correction | kick | KA-4 + ka4.c:123-437 (2f670 no XZ, uses 1406e8b70) | static | INF(high) | §0 |
| KA-6 | GF `actionSmuggle` (3172/3327/3341, spread 1332-1334, gate 3202/3320) is the XZ analog; replaces the PES kick approach (would disable it for kicks) | GF | humanoid.cpp | source | OBS | §4 |
