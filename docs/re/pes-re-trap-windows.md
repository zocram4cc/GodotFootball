# TRAP reach: picker window test for action 7, and the live trap watch (S1Watch, round 15)

> **Main audit 19-10-26:** `0x142648668` = [0.15, 0.335, 0.56, 0.785] (T1 = 0.335, T2 = 0.56): match. The window test is CONFIRMED; the live trap is not measured (no action-7 pick in 3 min of play). PARKED until a loose-ball window.
> **Main audit 19-10-26 (§6):** default +0x70 `0x141863350` = `xor al,al; ret`, and the KickoffReceiver/DemoMove/OutofPlay vtables (`0x1425E1220/6428/2810`) +0x70 all point to it: match. A moving man (Move/StepMove) is admitted to TRAP by default.

Tags: **CONFIRMED** = decompile, image read, or live read/watch with VA. **INF** = inference. **OPEN** = not resolved.

**Summary.** (1) The action-7 window test is decoded (CONFIRMED). The windows are in clip units, scaled per player by `anime+0x3B18` (≈1.0 ± 0.05). (2) The live trap was NOT measured: in 5 min of play the picker was called ~2,900 times in 3 min and **never with action 7**, and the 5-minute `SetMotion` watch saw no trap clip id. A trap issue was not observed, so the distance-at-issue is still unknown. (3) Admission (§6, round 16): the trap is refused for a moving man only in narrow clip-id cases with a frame-count test; see §6.

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
- Process-hygiene note: while stopping my own watchers I sent SIGINT to five perf processes that matched my breakpoint pattern but were not mine (output `smp_*.dat`, no `sudo` prefix). They exited. The recorder PIDs I stopped were my own. From now on I kill only PIDs I recorded at launch.

## 5. Open

1. The live trap distance, clip and contact frame (no trap observed; needs a loose-ball window).
2. The caller's `key0/key1` values for a loose-ball TRAP (`bVar6`).
3. The path that issues the trap clip (not `SetMotion` at the chooser's neighbourhood).
4. Contact frame 18 (not checked live).
5. Clip names: none of the clip ids in §6 are named in the docs; which of them is "accelerate" is OPEN.

## 6. TRAP admission: action names and the `+0x70` answer (round 16, S1Watch)

**Admission function (CONFIRMED, `FUN_14079BBE0`, 45 lines; slot `+0x10` of the action-7 vtable `0x1425E1ED0`).** Current action id `c` = the signed byte at `anime+0x11` (CONFIRMED: `FUN_1406E66B0` = `movsx eax, byte [rcx+0x11]`, the secondary slot `+0x28`). Then:
- `c` ∈ {2,3,4,5,6,8,9,10,0xB,0x10,0x11,0x15,0x44,0x48,0x50,0x51,0x53}: admit iff `action[c]->vt[+0x70](action[c], anime, 7)`.
- `c == 0x17` (KickoffReceiver): **admit** (`return 1`).
- `c ∈ {0x4A, 0x4F}`: **admit** iff `anime+0x3AD8 != 0` and `*(u8*)(anime+0x3AD8+0x62) == 0`; otherwise refuse (falls to `return 0`).
- else: refuse.

**Action names (CONFIRMED, RTTI from the image, `vt-8 → COL → TypeDescriptor`).** Action objects sit at `anime+0xC40+0x1528+8·id`:

| id | class | vtable | `+0x70` target |
|---|---|---|---|
| 0x17 (23) | `action::KickoffReceiver` | 0x1425E1220 | 0x141863350 (default) |
| 0x4A (74) | `action::DemoMove` | 0x1425E6428 | 0x141863350 (default) |
| 0x4F (79) | `action::OutofPlay` | 0x1425E2810 | 0x141863350 (default) |
| 1 | `ResetIdle` | 0x1425E1790 | 0x141863350 |
| 2 | `Move` | 0x1425E13F0 | 0x140782570 |
| 3 | `StepMove` | 0x1425E1960 | 0x140789E60 |
| 4 | `Dribble` | 0x1425E2440 | 0x1407B9280 |
| 5 | `SlideDribble` | 0x1425E2540 | 0x1407C6F40 |
| 6 | `DribbleStepMove` | 0x1425E2640 | 0x140789E60 (shared with StepMove) |
| 7 | `Trap` | 0x1425E1ED0 | 0x1407995D0 |
| 8–10 | `Kick` | 0x1425E2358 | 0x140724820 |

Full table names ids 1–93 (sampled from one live object; 24 objects had the same vtables for ids 0x17, 0x4A, 0x4F).

**Default `+0x70` (CONFIRMED):** `0x141863350` = `xor al,al; ret`, i.e. **always false**. So KickoffReceiver, DemoMove and OutofPlay never answer yes through `+0x70`; they reach 7 only through the direct `0x17` admit or the `0x4A/0x4F` sub-flag rule above.

**Helpers used by the locomotion `+0x70`s (CONFIRMED):**
- `anime+0x28` (u16) = current clip id (secondary slot `+0x18` = `FUN_1406E6BD0`, returns `anime+0x28`).
- `sec+0x38` = `FUN_1406E6960` → `FUN_1406E68C0(anime-0x157-base, clip)`: searches the anime's current motion table (count at `+0x420`, entries of 0x14 bytes: clip id at `+0x354+k·0x14`, duration in seconds at `+0x35C+k·0x14`) and returns `trunc(54·dur + 0.5)` frames, or **0 if the clip is not in the table**.
- `FUN_1406AFBC0(db, clip, 5)` (key-5 existence) and `FUN_1406AF8E0(db, clip, 1.0, 0)` (frames at speed 1) read the clip database; if key 5 is missing, `iVar4 = 0`.
- Frame test used by both locomotion slots: **allow iff `iVar4 ≤ sec+0x38`**.

**Clip-id sets (CONFIRMED from the predicates):**
- `FUN_140A6E290` (set A): for id < 0xBE8: {0xBE7, 0x8C7, 0x8CB, 0xB4F, 0xBE5}; for id ≥ 0xBE8: {0xBE9, 0xC08, 0xC0F}.
- `FUN_140A6E2E0` (set B): {0xC09, 0xC0D, 0xC0E, 0xC11, 0xC12}.
- `FUN_140A6E4D0` (set C): {0xBF0, 0xC0A, 0xC13}.

**`Move.+0x70` (CONFIRMED, `FUN_140782570`, 37 lines):** if the current clip is in none of A, B, C → **return true (allow)**. Otherwise → allow iff the frame test holds.

**`StepMove.+0x70` (CONFIRMED, `FUN_140789E60`, 30 lines), also `DribbleStepMove`:** if the current clip is not in {0xC0A, 0xC0D, 0xC11, 0xC13} → **return true**. Otherwise → allow iff the frame test holds.

`Dribble` (`FUN_1407B9280`, 149 lines) and `SlideDribble` (`FUN_1407C6F40`, 147 lines): not decoded in this round (OPEN).

**Answer to the question.** Does PES refuse the trap while the man is moving/accelerating? **Not in general.** For `Move` (2), the default answer is allow, and the refusal needs both (a) the current clip in sets A, B or C, and (b) its database frame count (at speed 1, with a key 5) to exceed its duration in the anime's current motion table (or the clip missing from that table, in which case `sec+0x38 = 0` and any positive frame count refuses). `StepMove` and `DribbleStepMove` refuse only for clips {0xC0A, 0xC0D, 0xC11, 0xC13} under the same frame test. The "accelerate" clip in particular is not named here (clip names OPEN), and the pressing man's current action id is not established live: the `anime+0x11` values I sampled do not map to players (INF: a locomotion man is in `Move`, id 2, most of the time).

**Not measured:** the live answer for a real pressing man (needs the current clip id and table of the man's anime, which I could not map to a roster slot this round).
