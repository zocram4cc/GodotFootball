# TRAP reach: picker window test for action 7, and the live trap watch (S1Watch, round 15)

> **Main audit 19-10-26:** `0x142648668` = [0.15, 0.335, 0.56, 0.785] (T1 = 0.335, T2 = 0.56): match. The window test is CONFIRMED; the live trap is not measured (no action-7 pick in 3 min of play). PARKED until a loose-ball window.

Tags: **CONFIRMED** = decompile, image read, or live read/watch with VA. **INF** = inference. **OPEN** = not resolved.

**Summary.** (1) The action-7 window test is decoded (CONFIRMED). The windows are in clip units, scaled per player by `anime+0x3B18` (≈1.0 ± 0.05). (2) The live trap was NOT measured: in 5 min of play the picker was called ~2,900 times in 3 min and **never with action 7**, and the 5-minute `SetMotion` watch saw no trap clip id. A trap issue was not observed, so the distance-at-issue is still unknown.

## 1. Action-7 rows (CONFIRMED, image read, row stride 0x1C)

| clip | row | prio (+5) | keys (+6,+7) | xb (+8) | zb (+0xC) | xe (+0x10) | ze (+0x14) |
|---|---|---|---|---|---|---|---|
| 0x547 | 34 | 0 | 0, 0 | −0.250 | −0.557 | 0.250 | 0.218 |
| 0x548 | 35 | 0 | 8, 0 | −0.180 | −0.767 | 0.270 | −0.353 |
| 0x549 | 36 | 0 | 10, 0 | −0.300 | −1.013 | 0.210 | −0.143 |
| 0x57D | 84 | 1 | 8, 1 | 0.256 | −0.729 | 0.620 | −0.323 |
| 0x57F | 86 | 1 | 10, 1 | 0.138 | −0.913 | 0.860 | −0.153 |

Only these five rows have action byte (+4) = 7 in the first 400 rows (the picker keeps rows whose +4 equals the requested action).

## 2. The window test for action 7 (CONFIRMED from `FUN_14081FF70`)

Let `s = anime+0x3B18` (per-player scale). The picker's ball vector `p = ball / s`, with `p.x = p[0]`, `p.y = p[1]` (height), `p.z = p[2]`.

Action 7 takes the default branch of the picker's switch, so the extension terms are zero: `fVar26 = 0` and `local_138 = 0` (both set before the switch; action 7 is not a listed case).

- **x window:** `xb ≤ p.x`, `p.x ≠ xb`, `p.x < xe + 0`. So `p.x ∈ [xb, xe)` (decompile: `(xb <= fVar1 && fVar1 != xb) && (fVar1 < fVar26 + xe)`, with `fVar1 = *param_2`).
- **z window:** `zb − 0 < p.z` and `p.z < 0 + ze`. So `p.z ∈ (zb, ze)` (decompile: `(zb < param_2[2]) && (param_2[2] < local_138 + ze)`). **The ze rule: absolute** (an upper bound, not an offset), because `local_138 = 0` for action 7.
- **Height:** `h_hit − p.y ≤ T`, where `h_hit = 0.01 · clip_hit.y` (clip hit record, `FUN_1406AFF90`, track 0; `0.01 = 0x14256AE18`). `T` = `fVar27`:
  - `fVar27 = local_12c` when `(key1 < 10 or prio < 10)` and the candidate flag `bVar6` is false; otherwise `local_120`.
  - `local_12c = 1.15 · (T2 − T1) · v` and `local_120 = 2 · (T2 − T1) · v`, with `v = 1.0` (`DAT_14256AE1C`). `T1 = 0.335` and `T2 = 0.56` (`DAT_142648668 + 4·1` and `+ 4·2`, image read). So **`local_12c = 0.25875`** and `local_120 = 0.45`. For action-7 rows (prio ≤ 1 < 10) the normal value is **0.25875**.
- **Candidate gate** (must hold for action 7, because `bVar7 = false`): `bVar6` is true, i.e. `|key0_request − prio| < 2` and `|key1_request − row.+6| < 4` (with a special case for request key1 = 0xB). `key0/key1` are the caller's hit record (`param_4`). **OPEN:** the caller's values for a loose-ball TRAP, which is what picks the candidates in practice.
- **Selection:** among admitted rows, the lowest priority byte wins (`bVar3 ≤ local_144`), then the smallest hit-relative distance `|p − 0.01·hit|` (`fVar23`).

So the **trap reach in the picker, per row, is an axis-aligned box in the man's clip frame**: `x ∈ [xb, xe)`, `z ∈ (zb, ze)`, plus a height bound, scaled by `s`. Clip-to-metre conversion: multiply by `anime+0x3B18` (read live: 1.0 for 8 of 24 anime objects, 1.0278, 0.9722 and 1.05 for the rest).

Metre examples (`s = 1`): clip 0x547 covers `x ∈ [−0.25, 0.25)`, `z ∈ (−0.557, 0.218)`. Clip 0x57F covers `x ∈ [0.138, 0.86)`, `z ∈ (−0.913, −0.153)`.

## 3. The live trap watch (NOT measured: no trap in the window)

Method and what ran:
- Per-frame recorder (`trap2/rec_trap.py`, `time.monotonic`, ball 0x3562E904 and 22 player sim/anim positions). Frames recorded: 2,744 (about 1 in 7 frames during the watch, because of perf overhead; not enough for contact timing).
- `SetMotion` entry watch (`mem:0x1406F0F70:x`, `-I`, `-k mono`), main thread and 4 JobGraph workers, ~5 min of play: **109,930 hits, 105,007 with motion 0x899**. Trap-set ids `0x547, 0x548, 0x549, 0x57D, 0x57F`: **0 hits**. Other ids in 0x520–0x580: 0x528 and 0x52E once each. No `SetMotion` call site lies within ±0x800 of the action-7 chooser `0x1407999F0`, so the trap clip is not set through this function with its picker id (INF: the trap goes through another path, OPEN).
- Row-picker entry watch (`mem:0x14081FF70:x`, workers, 180 s): **2,947 calls; action (R8) values 0xD, 0xF, 0x19, 0x18, 0x8, 0x12, 0x27, 0x9, 0x13, … and never 7.** So no action-7 (TRAP) selection occurred in that window.

Conclusion: the TRAP selection is rare in ordinary play (a loose ball within reach of a defender, which the 10 m gate and the reaction gates must allow). The measurement needs a longer or loose-ball-focused watch, which I did not get inside this round.

## 4. Other findings this round

- `FUN_1406F0F70` (SetMotion) arguments (CONFIRMED signature): `(anime, motion id, rate, mirror, …)`. Its first argument is the anime object, not the match player. RCX = anime, RDX = motion id.
- `perf -k mono` timestamps are on the same clock as `time.monotonic()`, so per-frame records can be joined to trap hits.
- Process-hygiene note: while stopping my own watchers I sent SIGINT to five perf processes that matched my breakpoint pattern but were not mine (output `smp_*.dat`, no `sudo` prefix). They exited. The recorder PIDs I stopped were my own.

## 5. Open

1. The live trap distance, clip and contact frame (no trap observed; needs a loose-ball window).
2. The caller's `key0/key1` values for a loose-ball TRAP (`bVar6`).
3. The path that issues the trap clip (not `SetMotion` at the chooser's neighbourhood).
4. Contact frame 18 (not checked live).
