# Tracked parts (ap+0x508+12k) and the feet test (S1Watch, round 21)

> **Main audit 19-10-26:** `0x1425EFBE0` = (0, 3, 6, 10) and `0x1425AB4A8` = 0.23: match. Readers CONFIRMED; the writer of P+0x508 is open (live check queued).

Status: **static decode of the readers CONFIRMED; the writer of the tracked block is NOT identified** (no clean mirror copy found statically). The live watch is queued with the other checks. Tags: CONFIRMED = decompile or image read this round; INF = inference; OPEN = not resolved.

## 1. The part table (CONFIRMED, image read)

`DAT_1425EFBE0` dwords 0..3 = **0, 3, 6, 10**, matching Main's note. Part k uses bone `DAT_1425EFBE0[k]` in `FUN_141EB51A0(P, out, bone)`. Bone-valid flags `DAT_14351E540[bone]` for the four parts (bones 0, 3, 6, 10) are all **1** in the install.

## 2. Readers (CONFIRMED, decompile)

**`FUN_140842BE0(P, rec, k)`** (41 lines): the per-part trip distance.
- Early exit: if `rec+0x51 == 0`, reads `frame = *(MI+0x1198)` and `cap = FUN_141E532E0() >> 1`; returns 0 when `cap + rec+0x48 < frame` (the part is out of the time window).
- Tracked point `T_k = (P+0x508+12k) [x, y, z]`: `x` from `+0x508+12k`, `z` from `+0x510+12k`. The y is `+0x510`, not z: the code reads `(u64 @+0x508+12k)` (x,y) and `f32 @+0x510+12k` (z). So the layout is **x at +0x508, y at +0x50C, z at +0x510** (per 12 bytes, k = 0..3).
- Current bone point `Q_k = FUN_141EB51A0(P, out, DAT_1425EFBE0[k])` = `P+0x538+12·bone` (the current bone block, not the tracked block).
- Distance:
  - **k = 0 (HIP):** `d = |T_0 − (Q_0 − (rec+0x3C, rec+0x40, rec+0x44))|` in 3D, i.e. the point offset by the record's reference `(rec+0x3C, +0x40, +0x44)`, compared with `Q_0`'s `(x, y, z)` (`local_18/14/10`).
  - **k ≥ 1:** horizontal `d² = (z_k − z_Q)² + (x_k − x_Q)²` only (no y term), where `x_k`, `z_k` are the tracked point and `(x_Q, z_Q)` the current bone.
- Returns `sqrt(d²)` through `FUN_1415A1780` (sqrt).

**`FUN_140843C50(P, id)`** (96 lines): the feet/part reach test (the "trip" model's per-part maximum).
- Reads `iVar14`, `iVar12` from the anime-object's touch record counts `rec+0x624`, `rec+0x65C` (4 = near, 7 = far bands: `<4 → 1`, `<7 → 0`, else 2).
- If the owner's stance (`FUN_140A6E4B0` class) and `rec+0x8` bit 15 hold, and the band does not match, it returns `DAT_142598578` (0.2) or the bone distance.
- Otherwise it loops parts `uVar10 = 0 .. uVar13−1` (`uVar13 = 4`, or `1` when band 2) and returns **`max over k of FUN_140842BE0(P, rec+0x610, k)`**.
- Where the early return skips the loop, it returns `rec+0x63C − DAT_1425AB4AC`.

**`FUN_140843E30(rec, t)`** (11 lines; the feet test, CONFIRMED):
```
if (rec[0] < 0x16 && (DAT_1425AB4A8 − t) + (DAT_1425AB4A8 − t) < rec[0xB] as float) return 1
else return 0
```
- `DAT_1425AB4A8 = 0.23` (image read, f32 `0x3E6B851F` area; confirmed `0.23000000417`).
- So the feet test is `2·(0.23 − t) < rec+0x2C (f32)` with `rec[0] < 0x16` (a valid player). `t` is the caller's tracked-foot height; `rec+0x2C` a height threshold.

## 3. The writer (NOT FOUND statically)

- The tracked block `+0x500..+0x53F` (4 × 12 B) has many stores (`+0x508` alone: 3,801 raw-decoded hits; a broad copy scan gave 9,560 candidate sites), most unrelated displacements.
- The writer region is the planner in `0x140458xxx` (decompile `FUN_140458b20`, `FUN_140457f90`): it stores the tracked points inside the path-planning loop with computed indices (`mov [rsi+0x508], eax` at `0x140458D7C`, `mov [rbp+0x500], eax` at `0x1401CC506`). These are not a block mirror copy, so the "per-frame copy like B/N/+0x661" pattern does **not** appear here.
- **Source of the tracked points: OPEN.** Likely candidates from the names: the lagged bone positions of the AnimePlayer bones, written at the trip-clip frame. Not confirmed.
- So no statement about rate or lag is given: it would be an INF.

## 4. Queued live check (not done: no match loaded)

- Watch the tracked block `P+0x508` (`P` = the player object, `p = G+(0xBF+i)·0x28`) with `perf -I -e mem:<aligned>:w` on the JobGraph workers, on a player running a trip clip. Record the writer IP and the source registers at the trap; the source gives the lag.
- Compare the tracked point with the bone block `P+0x538` per frame to get the lag in frames.
- Read `FUN_140843E30`'s inputs live: `rec[0]` (the player id, valid when < 0x16) and `rec+0x2C`.

## 5. Summary for FirstTouchPort2

- Readers CONFIRMED: `FUN_140842BE0` (per-part distance, x/y/z at `+0x508/+0x50C/+0x510`), `FUN_140843C50` (max over four parts within the band), `FUN_140843E30` (feet test `2·(0.23 − t) < rec+0x2C` with `rec[0] < 0x16`).
- Part bones 0, 3, 6, 10; valid flags 1.
- Writer: not found statically; no per-frame block copy identified in this region. The lag and rate are OPEN until the queued live watch runs.
