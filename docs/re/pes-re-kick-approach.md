# Open-play kick approach: horizontal alignment (S1Watch, round 7 + update)

> **Main audit 19-10-26:** `FUN_1406B10D0` operands re-read: `movss xmm6,[0x14256AE1C]`=1.0 at `0x1406B1133`, `divss xmm1,[0x14259A764]`=60.0 at `0x1406B1148`, `addss xmm0,[0x142565600]`=0.5 at `0x1406B116B`: match. PARTIAL: the approach planner is not found; live sample n=6 (INF). The move target is `cmd+0x20` (ClipWords correction), not `cmd+0x14`. The per-frame ball address `0x3562E904` is session-specific.
> **Main audit 19-10-26 (round 2):** planner constants re-read: `0x1425656C0`=4.0, `0x14259BDF4`=8.0, `0x14259BE10`=16.0, `0x14259BDCC`=0.125, `0x14259BE5C`=90.0: match. Still PARTIAL: the XZ approach mover, t0, and the side-mode condition are open. Parked by Main.

Status: **planner identified (CONFIRMED structure, INF on the lead constant); live distributions measured on 45 kicks; facing test negative; aim test weak.** Tags: **CONFIRMED** = decompile or live read with VA; **INF** = inference; **OPEN** = not resolved.

## 1. Who sets the kicker's target (static)

**Dispatch chain (CONFIRMED):**
- `FUN_14072C5C0(player, cmd, ...)` — kick-action gate (ball distance, can-kick checks, `MI+0x116C != 2`). No E8 callers; reached through a function table. Its kick branch calls `FUN_14072F3B0` at **`0x14072C807`** (`call 0x14072f3b0`), and another branch calls `FUN_14072EA00` at `0x14072C7F7`.
- `FUN_14072F3B0(p, player, cmd)` (111 lines): three branches, all keyed on `FUN_140A6E450(player+0xACB)`:
  - `cVar4 == 0` and `local_44 < 0.5·(f(P,7)+f(P,8))` → `FUN_1407302B0` (kick writer, call site `0x14072F458`);
  - `cVar4 == 0` otherwise → `FUN_14072F670` (call `0x14072F44A`);
  - `cVar4 != 0` → `FUN_140730000` (call `0x14072F420`).
- **Approach planner: `FUN_14072C840(p, player, cmd, int* st, float flag)`** — returns a desired **yaw in degrees** for the player. Callers: `0x14072FA7A`, `0x14072FBDB`, `0x14073010B`, `0x140730C4C` (the three kick branches).

**What FUN_14072C840 computes (CONFIRMED from decompile; names mapped to VAs by Ghidra):**
1. `cmd+0x20` = receiver/target player index T (`cmd[8]`, valid if `< 0x16`). The receiver object is `FUN_140A48A10(G, T)`; its position is at `+0x55C` (x), `+0x560` (y), `+0x564` (z). The kicker's position is `player+0x8` (x), `+0xC` (y), `+0x10` (z); the kicker's current yaw is `player+0x20` (degrees).
2. **Lead prediction (INF on the constant):** `fVar12 = int( (fps·4.0)/60.0 + t0 )`, with `fps = 54` (`0x14351AE54`), `4.0` at `0x1425656C0`, `60.0` at `0x14259A764`. The predicted target point is `T.pos + fVar12·v_T`, with `v_T` from `FUN_140A6C8F0` (receiver velocity, scaled by `0.6944444/fps`, `0x14259BDD0`). `t0` is the value from the previous branch (not decoded). So the lead is about **3.6 ticks + t0**, truncated to an integer.
3. **Heading to the point:** `FUN_140A92D40(dx, dz)` = atan2 (radians), then `·57.2957764` (`0x14259BE40`) to degrees; `< 0` → `+2π` (`0x1425795E4` = 6.2831855). The planner works in degrees with wrap `±360` (`0x14259858C`, `0x142598598`) and `±180` (`0x142598580`, `0x1425DDFFC`).
4. **Blend with distance:** `w = 0` for `d ≤ 8` (`0x14259BDF4`), `w = (d−8)·0.125` for `8 < d < 16` (`0x14259BE10`, `0x14259BDCC`), `w = 1` beyond.
5. **Side mode (lateral 90°):** when the current heading is far from the target direction, the planner adds `±90°` (`0x14259BE5C` = 90.0) through `FUN_140470D60(p+0x20, …, sign·90)`. The sign comes from the sign of the signed difference (`fVar11 < 0` → `fVar24` = −1 at `0x142588218`, else `fVar4` = +1 at `0x14256AE1C`). This is the **lateral-approach mechanism** (INF on the exact branch condition: `|fVar11| < 90`, `0x140872…` comparisons).
6. The output yaw is then clamped against the current yaw `p+0x20` with the `±360` wrap.

**The kick writer `FUN_1407302B0`** (757 lines) does not call `AdjustTransXZ`. Its target is ball-relative only for the local velocity frame: `local_1c8/1c0` = kicker velocity from history (`FUN_141EB50B0`, `(p+8−p+0x14, p+0xC−p+0x18, p+0x10−p+0x1C)`), rotated by `FUN_140A92C80(wrap(−p+0x20)+90)` (degrees, `0x14259BE5C`). It writes `P+0x498` (height) and `P+0x4D0` (yaw, `FUN_141EB35A0`).

**AdjustTransXZ (`FUN_141EB39A0`) callers (CONFIRMED E8 scan, 58 sites):** `0x1406DF53B 0x1406EF2F9 0x14071AD00 0x14071AF2D 0x140726268 0x140727904 0x140728293 0x14072ED67 0x14073417F 0x14073D15C 0x14073D729 0x14073D9E7 0x14073DC13 0x14073EC0F 0x14073F3EB 0x14073F666 0x140740009 0x140742923 0x1407443FB 0x1407465F7 0x14074719F 0x1407482DF …` (58 total). None is in `FUN_14072F3B0`, `FUN_1407302B0`, `FUN_14072C5C0` or `FUN_14072C840`. The XZ approach is therefore issued by a caller outside the kick chain, **OPEN** which one feeds the kick's approach point.

**Constants used (values read from `pes_full.bin`, VA = Ghidra label):**

| VA | value | role |
|---|---|---|
| `0x14259BE5C` | 90.0 | lateral side offset (deg) |
| `0x142598580` | 180.0 | wrap half |
| `0x14259858C` | 360.0 | wrap full |
| `0x142598598` | −360.0 | wrap |
| `0x1425DDFFC` | −180.0 | wrap |
| `0x1425795E4` | 6.2831855 | 2π (radians→wrap) |
| `0x14259BE40` | 57.2957764 | 180/π |
| `0x1425656C0` | 4.0 | lead scale |
| `0x14351AE54` | 54 | fps |
| `0x14259A764` | 60.0 | ticks/s |
| `0x14259BDD0` | 0.6944444 (25/36) | receiver velocity floor (×1/fps) |
| `0x14259BDF4` | 8.0 | blend start (m) |
| `0x14259BE10` | 16.0 | blend end (m) |
| `0x14259BDCC` | 0.125 | 1/8 blend slope |
| `0x142590D98` | 0.3 | threshold (planner branch) |
| `0x14256B140` | 0.45 | threshold (planner branch) |
| `0x142588210` | 10.0 | threshold (kick writer) |
| `0x14256AE34` | 1000.0 | kick writer (speed scale) |
| `0x142588218` | −1.0 | sign |
| `0x14256AE1C` | +1.0 | sign |
| `0x142565600` | 0.5 | rounding |

## 2. Clip tick count (CONFIRMED, from round 7 part 1)

`FUN_1406B10D0`: `trunc(0.5 + (2n/rate)·(fps/60))`, `n = byte((w>>13)+2)` from the clip header, `rate ≤ 0 → 1.0`. The kick writer calls it with track 0, rate 1.0. This is the whole clip length, not the hit frame. The hit frame is **OPEN**.

## 3. Live measurement (45 kicks, CONFIRMED method)

**Ball:** `0x3562E904`, the per-frame ball found by the full signature scan and per-frame change test (see `pes-re-out-side.md` §1 for the BallInfo object `*(G+0x40)`, which is stale in position). The ball moves every frame; flight after contact decays smoothly.

**Players:** `G+0x1DD8 + 0x28·i → +0x8` (sim position), `+0x20` (yaw field), `+0x55C` (anim position). Recording `rec5.json`: 18,551 rows, 420 s, in play (`MI+0x116C == 1`). About 15% of rows lose a single frame in the player table (transient read failures); gaps of up to 8 frames per window are held.

**Kick detection:** ball step ≥ 0.2 m/frame, jump ≥ 0.10, previous step < 0.15, kicker = nearest player ≤ 1.5 m at the frame before contact. **70 detected**, 51 analysed, **45 after the filter** (approach speed ≤ 12 m/s, kick speed 8–35 m/s; the excluded ones were track swaps at 47 m/s).

**Distributions (45 kicks):**

| quantity | mean | median | sd | p10 | p90 |
|---|---|---|---|---|---|
| contact offset along approach dir (m, ball ahead of root) | 0.48 | 0.55 | 0.31 | 0.16 | 0.82 |
| contact offset lateral, signed (m) | 0.02 | 0.05 | 0.49 | −0.67 | 0.63 |
| \|lateral\| (m) | 0.43 | 0.41 | 0.25 | 0.05 | 0.75 |
| run-up angle, approach vs kick dir (deg) | 53.8 | 48.5 | 41.1 | 4.5 | 102.6 |
| approach speed, 12 frames before contact (m/s) | 3.28 | 3.32 | 1.46 | 1.17 | 5.12 |
| kick ball speed (m/s) | 19.4 | 19.8 | 4.3 | 14.0 | 26.2 |
| frames, speed peak → contact | 16.4 | 17 | 5.0 | 9 | 22 |
| frames, plant (speed < 0.3·peak) → contact (n=40) | 14.6 | 15 | 5.0 | 9 | 21 |

The root is the `+0x8` sim position, not the foot; the foot offset is not measured.

**Test of the planner (`FUN_14072C840`):**
- **Facing test: negative.** `player+0x20` is not the velocity heading. Over 27,199 in-play frames, the offset between `atan2(dx,dz)` of the frame velocity and `+0x20` is uniform (histogram flat, mean resultant R = 0.03). So `+0x20` is not the facing that moves the player; the planner's use of it as "current heading" cannot be checked with this field. **OPEN**: find the facing field the animation uses.
- **Aim test: weak.** The minimum angle between the kick direction and any other player has median 4.1° (n = 51). A uniform baseline for 21 other players is about 5.6°, so the kicks aim at players but the test does not separate planner from chance. The receiver test (player nearest the ball 30 frames after contact) has only 7 cases, median 25°.
- **Lateral 90° mode:** the run-up distribution (median 48.5°, p90 103°) has no peak at 90°, so the side mode in §1.5 is not visible in this sample. The planner's side branch is not confirmed by the data.

## 4. What is not established

1. The lead constant `t0` in §1.2 (set in the previous branch of `FUN_14072C840`).
2. The facing field used for the run-up (`+0x20` is not facing; §3).
3. The branch condition of the 90° side mode (§1.5).
4. Which caller of `AdjustTransXZ` moves the kicker's XZ to the approach point (58 sites, none in the kick chain).
5. The hit frame of the kick clip; so the plant-to-contact timing cannot yet be compared with the clip.

## 5. Next steps

1. Decode `FUN_14072C840`'s previous-branch value `t0` and the `FUN_140470D60` side-rotation branch condition (`0x14072CDxx` block) to pin the lead and the 90° rule.
2. Find the facing field the animation uses (search `FUN_140A6C6D0` consumers for yaw) and rerun §3 against it.
3. Decode the kick clip hit frame (track of clip id from `FUN_1407302B0`) and compare with the 14.6-frame plant timing.
