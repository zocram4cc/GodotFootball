# Carrier and dribble fields: writers, readers, GF status (decode only, no code change)

Basis: live Ghidra bridge decompiles. The scratch decompile directories the reports cite are gone from disk; only the four agents' own scratch dirs remain. OBS = read in a decompile at the stated VA. INF = inferred, naming not confirmed.

Notation: `G` = the game manager (`*(DAT_1436F3DC8)`); `MI` = `*(G+0x270)` (the match instance); `bp` = the BallPlayer object; `fps` = 54.

## 1. Corrections to earlier reports

- `FUN_141e532e0` (`0x141e532e0`) returns the game speed, the int `DAT_14351ae50` = 54 (OBS). It is not a random draw. The carrier-units report (sections 1 and 3.3) and the carrier-AI report's "RNG draws via 0x141e532e0()/6" are wrong. Its float twin `0x141e532d0` = 54.0.
- The opponent-list builder cited as `FUN_14212e110` is not an entry in the live project. The function containing it is `FUN_14212dc80` (entry 0x14212dc80, 0x30-byte records; section 4).
- `ctx4` (`bp+0x78`, the `ctx[4]` snapshot) is the **ball** kinematics. The shoot zone uses `ctx4+0x40` as the ball position, and the image-state writer uses `ctx4+0x40` as the ball x. The object-map text that calls it carrier kinematics is wrong. `ctx4+0x6C` is the ball-at-feet byte (OBS, read in the dribble unit).

## 2. Image state `bp+0x32B0+8` (writer `FUN_1421197B0`)

- Writer call (OBS, MainEval `FUN_142110D30` @142110E78). It runs only inside the `bp+0x10` byte != 0 block; otherwise the value stays as it was.
  Args: (bp+0x32B0, `*(MainEval.p2+0x38)`, bp+0x1C, bp+0x78, bp+0xEC = image A, team data `FUN_140A48A30(team)`).
- Output: the int at `bp+0x32B0+8` (read by `FUN_141052E30`, which returns it). Values written: 4, 3, 2, 1. The writer never writes 0; the initial value is not pinned (INF: 0 = never computed).
- Decision (OBS, in order):
  1. prev == 4 → 4 (sticky).
  2. `FUN_140699820(imageA+0x9CC)` (IsClose of the shoot zone) and `|wrap180(bp+0x2C − bp+0x4C)| < 100.0` → 4.
  3. prev == 3, or `attackSign × ball.x > F[6]` (strict; F[6] = penalty-area line, from the pitch-marks table at `DAT_1436F3DC8+0x2AA8`), or `imageA+0xB3C < imageA+0xB58 + 10.0` (strict, from the equality test) → 3.
  4. else if `team+4 < 2` and prev != 2 → corridor branch:
     - origin = the carrier's player object position (`FUN_140A48A10(G, bp+0x1C index)`) moved 30 m along the team angle `team+0x250` (`FUN_140A8F7C0`, 30.0 @0x142591140).
     - corridor 1 = `FUN_14063D2F0` with side flag = (team == 0). If its count is 0, → 2.
     - goal and clock test: `goals(other) ≤ goals(own)` or `(period ≠ 3 or minute < 40)` and `(period ≠ 7 or minute < 10)`. `minute` = `FUN_140A6BD60` of the period clock (minutes; INF from the thresholds).
     - corridor 2 = `FUN_14063CD90` with side flag = (corridor-1 count == 0). If its count is ≤ 1 → 2.
     - else → 1 when `team+0x134 > 2` or the team-sheet slider 0 != 1 or `bp+0x88 ≥ 2·fps` (108 at fps 54); otherwise → 2.
  5. else → 2.
- Open inputs: `bp+0x2C`, `bp+0x4C` (floats; angles in degrees; writers not pinned), `imageA+0xB3C`, `imageA+0xB58` (writers in the image-A updater `FUN_142119390`, not decoded), `bp+0x88` (u32), `bp+0x10` (byte gate), and the corridor helpers `FUN_140A53DF0` and `FUN_140A90D80` (undecoded; the carrier-port report also lists them as open).
- Known meanings in the `bp+0x1C` block: `+0x1C` player index (u32, `*param_3`); `+0x24` team index (u32); `+0x30` attack sign (float ±1).
- GF status: absent. The elizacontroller says "the image state has no GF counterpart", and the state is hard-coded to 0.

## 3. Team and match fields the writer reads

- `team+4` (int; `< 2` gate). The team data object is `FUN_140A48A30(team)` (OBS).
- `team+0x134` (int) (OBS; compared with 2).
- `team+0x250` (float) — the team's attack angle in degrees, folded ±180 (OBS). GF status: absent (GF has only the attack sign).
- Team-sheet slider byte: `FUN_140A62180(team, field)` = `*(u8*)(team+0x8E08 + 0x13 + field + 7·kind)`, with kind = `*(team+0x8E08)` when the table kind is 3 (`FUN_1408C5030`). The writer reads field 0 and compares it with 1. GF status: absent (GF tactics are not this byte table).
- Goals: `FUN_1408C9DF0(MI+0x750D0 + side·0x59650, 5)` = goal sum (constant-team report). The writer compares the other side's goals with its own. GF status: present (match score), not compared the same way.
- Period `MI+0x1168` (3 = second half, 7 = extra-time second half; OBS in the clock reports). Clock `FUN_140A3C050(MI+0x1168)`. In-play `MI+0x116C` (clock report). Frame counter `MI+0x1198` (PES frames; MainEval cadence). GF: the think cadence uses a possession clock, not `MI+0x1198 − bp+0x4224`; this is a known difference.
- fps: 54 (`0x141E532E0` int, `0x141E532D0` float). GF status: `pes::kGfTicksPerPesTick` and the tick constants cover the conversion.

## 4. Opponent list and records (builder `FUN_14212DC80`)

- Caller (OBS, dribble unit `FUN_14212FE70` @142130100): `FUN_14212DC80(unit+0x40, ctx, &origin, frames)`.
  - origin: if `ctx4+0x6C == 0` (ball not at feet) → the carrier player object's position (zero velocity); else the ball prediction `FUN_140A6CF70(·,0)`.
  - frames = `int(0.3·fps + 0.5)` = 16 at fps 54 (0.3 @0x142590D98, 0.5 @0x142565600), unless `u16 @lVar12+0x1A` ≠ 0 and `u16 @+0x1A ≤ u16 @+0x1C`; then frames = `u16 @+0x1C − u16 @+0x1A`. `lVar12` is not pinned.
- Sorted source: `ctx[5]` = image A. Count = `u32 @imageA+0`; element k is 24 bytes at `imageA+4+24k`, with `[0]` = slot and `[1]` = distance (float). The element getter is `FUN_14063C930`. The sorter and its reference point are **not pinned** (open).
- Per element, in order, with `dist = element[1]`:
  - `R_o` = 10/f (default); busy → 1.0; keeper → 5.0 (the keeper test runs after busy and overrides it).
  - STOP: if `R_o + 10.0 < dist`, the builder returns, so the list is the prefix that passes.
  - f from the image state (ctx[6] = the image): state 0 → 1.0; states 1–2 → `3 − 2t`, with `t = clamp01((hi − x)/(hi − lo))`, lo/hi = centre ∓ width/2 of the zone band at `imageA+0x1EC` (flag int; flag 0 → band at `imageA+0x1F0` with +4 centre, +8 width; flag != 0 → static default `DAT_1425CF058`, values not pinned); state 3 → 2.5 (0x14259BDE4); state ≥ 4 → 3.0 (0x14258C264).
- Record at `unit+0x54 + 48k` (12 words; the count is `u32 @unit+0x50`):
  - w0 slot; w1 `|pred − pos|`; w2 carrier→opponent distance d; w3 bearing (degrees); w4 lead = `(int)(frames · clamp01(d/10))`; w5..7 position; w8..10 predicted position = `pos + vel · lead`; w11 `R_o`.
  - Position `+0x55C/+0x560/+0x564` and velocity `+0x574/+0x578/+0x57C` of the opponent's player object (OBS). **Velocity units are not confirmed** (per second or per PES frame). The builder multiplies velocity by a frame count, so a per-second velocity needs ÷54.
- Threat reader `FUN_14212E490` (carrier-units §1.1): w1 = slot (< 0x16), w2 = path test (≤ 1.19e-7 → static), w5..7 = position, w8..10 = predicted position, w11 = R_o.
- GF status: `pes::DribbleOpponent` exists. The list is GF's 5 nearest (`AI_GetClosestPlayers`), R_o is 10 for everyone, and pred = pos. Absent: the distance-sorted prefix, the stop rule, busy, the keeper R_o from the builder, prediction.

## 5. Corridor family (the writer's state 1/2 branch)

- `FUN_14063CD90` is a body-block evaluator. Output: out[0] = best slot (0xFF none), out[1] = count (≤ 11), records of (d², slot). It walks the 11 players of one side, skipping the carrier and entries whose alive flag is 0 (`MI+0x1748 + slot`). Each candidate goes through `FUN_140A53DF0` (body block, undecoded), `FUN_140A90D80` (undecoded), `FUN_14043C410` and `FUN_140470D60`.
- `FUN_14063D2F0` is a wrapper: it copies two vectors and calls `FUN_14063CD90`.
- `FUN_1421B7D40` returns out[1] (the count); `FUN_141587AA0` zeroes it; `FUN_14063CD80` writes 0xFF.
- GF status: absent (no corridor evaluator). The carrier-port report lists the body-block helpers as undecoded (section 9.1).

## 6. Anime property table (busy predicate `FUN_140A6E3C0`)

- Table `0x1426484F0`: one u32 per anime id, valid for ids ≤ 0x5D (94 entries). Shared with PoseVeto's predicates; PoseVeto confirmed the bits below from their own decompiles.
- Bits (OBS): 4 (`0x140A6E1C0`), 5 (`0x140A6E5E0`, trap), 13 (`0x140A6E3C0`, busy), 15 (`0x140A6E300`, jostle), 16 (`0x140A6E340`, kick; byte `0x1426484F2` bit 0), and the 0x30000 pair (`0x140A6E320`, committed; not decoded by me).
- Busy for a slot (OBS): `r = FUN_140A48910(G, slot)`. That function returns the 8-byte pointer stored at `G + (slot+0xA5)·0x28` (`G+0x19C8` for slot ≥ 0x1A), so `r` is a record pointer, not the table address. Busy when `r+8` has bit 13, or when the anime id `r+1` ≤ 0x5D and the table entry has bit 13. The list builder also treats `r+8` bit 2 as busy.
- Writers of `r+1` and `r+8` are not pinned (open).
- GF status: absent (GF has no per-player anime id or property table; clips are imported in pesclips).

## 7. Player kinematics and alive flags

- The player object `FUN_140A48A10(G, slot)` (OBS). Position `+0x55C/+0x560/+0x564`, velocity `+0x574/+0x578/+0x57C`, facing `+0x554` (carrier-units §1.1; OBS in the builder).
- Alive flag `MI+0x1748 + slot` (1 = alive; OBS in the corridor evaluator). GF status: active list (`team->GetActivePlayers`), not the same byte.
- Keeper test `FUN_140A638C0(team, slot)`: true for the team's keeper (OBS in the builder). GF status: `GetGoalie()`.

## 8. Open for the inventory

1. Writers of `bp+0x2C`, `bp+0x4C`, `bp+0x88`, `bp+0x10`, and `bp+0x18` (passed to the image-A updater).
2. Image-A updater `FUN_142119390`: the sorted list (its sorter and key origin), `imageA+0xB3C`, `imageA+0xB58`, and the zone band `imageA+0x1EC`.
3. Corridor body-block helpers `FUN_140A53DF0` and `FUN_140A90D80`.
4. Velocity units at `+0x574` (per second or per PES frame).
5. `FUN_140A6CF70` (the ball prediction used as the origin when the ball is at feet).
6. `lVar12` (the u16 pair that can replace frames = 16).
