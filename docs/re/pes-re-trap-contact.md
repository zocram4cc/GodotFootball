# Trap contact: solver, row table, class-7 admission (S1Watch, round 14)

> **Main audit 19-10-26:** vtable `0x1425E1ED0`+0x10 = `0x14079BBE0` (class-7 admission) matches. PARTIAL: the live trap check is not done. FUN_1406B5C20 is a man-vs-man contact solver, not the trap's ball solver.

Status: **static decode CONFIRMED for the three items; the live trap measurement (distance at issue, clip, contact frame) is NOT done** (blocker in §5). Tags: **CONFIRMED** = decompile or image/live read with VA; **INF** = inference; **OPEN** = not resolved.

Main's premise needs one correction up front. **`FUN_1406B5C20` is not a ball-path solver.** It takes no ball input. It is a two-man contact solver, called only for a man-vs-man pair (§1). The loose-ball trap goes through the row picker and the admission chain (§2, §3), and the 10 m gate is in the reaction (§4).

## 1. The contact solver `FUN_1406B5C20` (CONFIRMED structure)

**Signature.** `FUN_1406B5C20(ctx, clip, pA, pB, outA*, outB*, flag*)` (decompile lines 1–8). Parameters used: `clip` (`param_2`), two man objects (`lVar6 = FUN_140A48A10(G,pA)`, `lVar7 = FUN_140A48A10(G,pB)`), two output vec3 (`param_5`, `param_6`), and a bool (`param_7`). **No ball parameter, and the body never reads the ball.**

**Caller chain (CONFIRMED):**
- `FUN_1405043F0(param_1, param_2)` (per-pair resolver; not in the docs; it sits in the match tick) calls `FUN_1406B6820(ctx, idA, idB, animeA, animeB)` at `0x14050476B`, under the branch `FUN_140A73E40(animeA) == 6` and `FUN_140A73E40(animeB, 1) == 7`. So the solver runs for a pair where **A is in class 6 and B is in class 7** (the class word read from `x+0x24`, or `x+0x28` when the flag is set and non-zero).
- `FUN_1406B6820` (28 lines) calls `FUN_1406B53C0(ctx, A, B)` for the clip id, returns without solving if the id is `0xBE5` (none), otherwise calls `FUN_1406B5C20(ctx, clip, A, B, outA, outB, flag)`.

**Clip id and mode (CONFIRMED).** `FUN_1406B53C0` (112 lines) returns one of `0x42..0x4E` (or `0x47`, `0x4A`, `0x4B`, `0x4D`, `0x4E`; or `0xBE5`). `FUN_1406B56D0(clip)` maps it to a solver mode: `0x44,0x45,0x46,0x47,0x48,0x49,0x4C,0x4E → 1`; `0x4A,0x4B,0x4D → 2`; `0x42,0x43 → 3`; else `0` (the solver then returns 0). Mode 1 uses the 0.5 fraction; modes 2 and 3 use the other branch.

**What it computes (CONFIRMED from the decompile; the labels are INF):**
1. Speed of each man: `FUN_140477130(man)` = `|v|·54·3600/1000` (**km/h**, from the sampled root velocity `FUN_140A6C6D0`, floored at `0.6944 = 2.5 km/h`, `0x14259BDD0`).
2. Lead time: `f_raw = (sA + sB)/15` with `sA, sB` in m/s (`1/3.6 = 0x1425C48A8`; `15 = 0x14259BE0C`). `f = clamp((f_raw − 0.1)/0.9, 0, 1)` (`0.1 = 0x142588200`, `0.9 = 0x14258C260`). **`t = trunc(54·f)` ticks, 0..54** (`54 = 0x14351AE54`). So the lead is 0 at combined speed ≤ 1.5 m/s and reaches a full second at combined speed ≥ 15 m/s.
3. Predicted root of each man after `t` ticks: `P(t) = P + t·v` (velocity at `+0x574/+0x578/+0x57C`, position at `+0x55C/+0x560/+0x564`).
4. Clip samples: `FUN_1406AFF90(motion, clip, track 0, 0)` gives each man's clip root offset; the solver scales it by `0.01 = 0x14256AE18` (cm→m). Clip keys `FUN_1406ABB30(ctx, clip, key 9 / key 0)` pick the sub-clip ids `sVar2`/`sVar3` (key bytes `0x9`, `0x0` from `1.26117e-44` = bits 9, and `0.0`). The meaning of those keys is OPEN.
5. Relative heading: `FUN_140439E50` gives the bearing from A to B; `fVar10 = bearing − A.+0x554` (wrap ±360, `0x14259858C`, `0x142598598`). `param_7 = (fVar10 < 0)` (which side the other man is on); if so the x-components are negated.
6. Mode 1 (`0.5 = 0x142565600`): the two predicted hit points are interpolated to their midpoint (`local_100`), each man is rotated by ±90° (`0x14259BE5C = 90`, `FUN_140A92C80`) and the outputs are the offset from each man's root to that point.
7. Outputs: `param_5` (man A) and `param_6` (man B) are vec3 **root corrections** (xz from the solved hit points, y = a constant `0.1`, `0x142588200`... see the z/y writes at the end of the function), and the return is `1` on success, `0` if the mode is 0.

**Verdict on the premise.** The solver places both men at the contact point for a man-vs-man contact at clip id `0x42..0x4E`. It does not compare the clip root travel with the ball path, and it does not use frame 18 (frame 18 may be a clip-internal key; not read here, OPEN).

## 2. The row table `DAT_14348DAC0` (CONFIRMED layout, read from the image)

Stride `0x1C`. Picker `FUN_14081FF70` (421 lines) reads it. Row fields (confirmed by the picker's indexing):

| offset | type | meaning (CONFIRMED by use) |
|---|---|---|
| `+0x00` | u32 | clip id (`0x520..0x564` in the first 60 rows; row 16 is all zero) |
| `+0x04` | u8 | **action class**: the picker keeps rows whose `+4` equals its `param_3` (`(&DAT_14348dac4)[row] == param_3`) |
| `+0x05` | u8 | priority (compared with `param_4[..]` and the keys) |
| `+0x06`, `+0x07` | u8 | hit keys (compared with `param_4[1]`, `param_4[2]`) |
| `+0x08` | f32 | x-window base `xb` (m) |
| `+0x0C` | f32 | z-window base `zb` (m) |
| `+0x10` | f32 | x-window extension `xe` (m) |
| `+0x14` | f32 | z-window extension `ze` (m) |

**Scoring (CONFIRMED structure).** The picker first divides the ball by the context scale `*(ctx+0x3B18)` (`param_2 / scale`), then subtracts the clip's hit offset `FUN_1406AFF90(motion, clip, 0, 0)·0.01`, giving the ball relative to the man in clip space. A row is admitted when that point lies inside its x and z windows (decompile lines 141–262; the window arithmetic is in the picker's later branches, partly OPEN).

**Action 7 (TRAP) rows (CONFIRMED, read numerically):**

| row | clip | prio | keys | xb | zb | xe | ze |
|---|---|---|---|---|---|---|---|
| 34 | 0x547 | 0 | 0,0 | −0.250 | −0.557 | 0.250 | 0.218 |
| 35 | 0x548 | 0 | 8,0 | −0.180 | −0.767 | 0.270 | −0.353 |
| 36 | 0x549 | 0 | 10,0 | −0.300 | −1.013 | 0.210 | −0.143 |
| 84 | 0x57D | 1 | 8,1 | 0.256 | −0.729 | 0.620 | −0.323 |
| 86 | 0x57F | 1 | 10,1 | 0.138 | −0.913 | 0.860 | −0.153 |

Read: the TRAP clips accept a ball that is roughly 0.5–1 m in front of the man (z from about −1.0 to −0.14 in these rows, with the `ze` values as stored). Those are the clip-space windows, not the 10 m gate (§4). The window values are the raw image floats; the picker's use of `ze` (absolute vs offset) is the OPEN part (its z test is `zb − m < z < local_138 + ze`, with `local_138 = 0` for action 7).

## 3. The class-7 admission (CONFIRMED vtable chain, live vptr)

**Path.** `FUN_1409FA520(R)` (reaction, Entry 3 gate 4a) calls `FUN_1406B4C10(anime = R+0x40, class 7)`, which calls `FUN_1406D9010(anime+0xC40, 7)`: returns the action object at `*(anime+0xC40+0x1528+8·7)`, or the default at `+0x1530` if that slot is null (CONFIRMED, `0x1406D9010`: `cmp edx,0x5e`; `*(this+0x1528+8·id)`; fallback `*(this+0x1530)`). It then calls the action's **vtable `+0x10`** with `(action, anime)`.

**Live read (CONFIRMED, not a static guess).** On all 24 live AnimePlayer candidates (`anime_hits.json`, primary vtable `0x1425E34D8` with secondary `0x1425E3508` at `+0xAB8`):
- action-7 object vptr = **`0x1425E1ED0`** (24 of 24);
- its **slot `+0x10` = `0x14079BBE0`** (`FUN_14079BBE0`, 45 lines; 24 of 24);
- its **slot `+0x68` = `0x1407999F0`** (24 of 24; not decoded here, the bridge has no function entry at that address);
- default action vptr `0x1425E1790` (24 of 24).

**`FUN_14079BBE0(action, anime)` (CONFIRMED decompile):** reads the current action id `c = anime_sec->vt[+0x28]()` (the secondary vtable's slot `+0x28`). Then:
- `c ∈ {2,3,4,5,6,8,9,10,0xB,0x10,0x11,0x15,0x44,0x48,0x50,0x51,0x53}` → `FUN_1406D9010(anime+0xC40, c)->vt[+0x70](current, anime, 7)`: the **current** action decides whether it can transition to 7;
- `c == 0x17` → **admit** (returns 1);
- `c ∈ {0x4A, 0x4F}` and `anime+0x3AD8 != 0` and `*(anime+0x3AD8+0x62) == 0` → admit;
- otherwise refuse (0).

So the class-7 admission is "the current action allows a transition to TRAP", with two direct admits (0x17, and 0x4A/0x4F with a sub-object flag). The reaction gate 4a in the docs is this call.

## 4. The 10 m gate and trigger (from the docs; not re-decoded here)

`FUN_1409FA400`: the man's C1 (squared distance to the ball, m²) must be ≤ 100 (`0x14253EE30`, 100.0), i.e. 10 m. This is the only distance in the TRAP trigger chain (`pes-re-first-touch.md` §6, reaction Entry 3). The clip choice then uses the clip-space windows of §2, not the distance.

## 5. Live trap measurement: NOT DONE (blocker)

Wanted: distance at trap issue, the clip issued, the contact frame. Blocker: the trap issue is the reaction's `+0x878 = 7`, on a per-man reaction object (`R`) that I did not locate; and the AnimePlayer → match::Player mapping is not established (position-match and pointer-match scans of the 24 candidates were inconclusive or empty; the player objects contain no direct pointer to them in their first 8 KB for players 0–4).

What the live data did establish: the 24 AnimePlayers are live (full action tables), and the class-7 vptr is stable across them. The trap clip ids and windows are in §2 and can be checked against a trap once the reaction object is found.

## 6. Rules for FirstTouchPort2 (sent)

1. The TRAP clip is chosen from action-7 rows only: clips 0x547, 0x548, 0x549, 0x57D, 0x57F (§2), by the ball's clip-space position, after the row's priority and key tests (partly OPEN).
2. TRAP admission = the class-7 action's `+0x10` (`FUN_14079BBE0`): the current action must allow a transition to 7 (or be 0x17, or 0x4A/0x4F with the sub-flag). Refuse otherwise (§3).
3. The 10 m trigger is C1 ≤ 100 m² (§4). It does not select the clip.
4. `FUN_1406B5C20` is man-vs-man, not a ball-path solver. It runs for a pair (A class 6, B class 7) with clip ids `0x42..0x4E`, and places both men at the contact point; it has no ball input (§1).

## 7. Open

- The mapping AnimePlayer → match::Player, and the reaction object (for the live trap, §5).
- Meaning of keys 9 and 0 in `FUN_1406ABB30` (§1.4), and the sub-clip ids.
- The picker's z-window extension rule (`ze` absolute or offset) and the priority/key tests (§2).
- `FUN_1407999F0` (slot `+0x68`, the clip chooser for TRAP; not decoded).
- Whether frame 18 is the TRAP contact key (no live trap measured).
