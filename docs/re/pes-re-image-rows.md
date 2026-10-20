# PES image-A rows and team-AI rows: writers of the carrier "image" tables (decode, 19-10-26)

Scope: the writers of the rows the A4 carrier units read but GF stands in for: the score row, the target-list row, the per-slot angle table (`0x140A71260`), and the curve table `X5+0x2B0` (`0x142139650`). Start point: the image builder `0x142119390`. Read-only: no GF edits, no live PES reads or writes, no builds or tests.

Evidence tags: **CONFIRMED** = asm read (`objdump -D` of `pes_full.bin`, or bridge disassembly) plus decompile. **C-dec** = bridge decompile only, asm not re-read. **INF** = inference, with the reason. **OPEN** = blocked, with the next step.

Scratch: `<workdrive>/gf-ImageRows-scratch/` (`dec_*.c`, `asm_*.txt`, `decB_*.c`, `decA_*.c`, `store_hits.txt`, `refs_band.txt`, `consts.py`, `e8scan.py`).

---

## 0. Summary

1. **The image builder `0x142119390` is not the writer of the score, list or angle rows.** It writes only image A (`BP+0xEC`; rows in §2). Its only access to the team-AI object is a pass-through to callees. The team rows live in the team-AI object `MA = *(team-AI+0x508)` (§3). The link `T18 = MA` is INF (same offsets, §3.0).
2. **The curve table `IA+0x2B0` writer is `0x142138220`**, called from the builder at `0x142119468` with `this = IA+0x2B0`. Structure CONFIRMED (call site, C-dec body). Per-sample formula C-dec. The segment-list construction is OPEN (§2.3).
3. **`IA+0x898` (per-teammate receiver score) is decoded**: writer `0x142137F90`, value `(0.2·v_a + v_b + v_c)/2.2`. `v_a` and `v_b` are C-dec exact (`v_b` given the curve); `v_c` OPEN (§2.4).
4. **Four `T18` rows are decoded**: goal geometry `MA+0x228D4..0x228E8` (writer `0x14045DF70`, and `0x14045B090` for the ball-side pair), ball-centred NearestEntry `MA+0x2470C` and ball-bearing angle table `MA+0x247D4` (writer `0x14045B090`). The ball angle table is **not** the table `0x140A71260` reads (§3.2). The sign-selected score/list/angle blocks `MA+0x228EC..0x22CBC` and the pair `0x22CBC/0x22D84` have **no writer found** after an immediate scan and a store scan (§3.3). The NearestList `MA+0x22E4C` writer stays OPEN.
5. **Cadence**: the builder runs on every MainEval call for each BallPlayer whose `BP+0x10` is set (CONFIRMED gate `0x142110DE2 JZ 0x142111183`; no rethink gate before the call at `0x142110E4B`). That is per logic tick. The team-AI ball list is time-sliced (§3.2).
6. **GF stand-ins**: none of the asked-for rows is replaced exactly by this decode. The GoalFront facing stand-in must stay: the ball-bearing table `MA+0x247D4` is a different table. Curve and receiver-score stand-ins can be partly replaced (§5).

---

## 0a. §0 Float table

Mechanical read: `struct.unpack('<f')` on `pes_full.bin` at `VA-0x140000000` (`consts.py`). Constants listed are every `DAT_`/`_DAT_` used by the decompiles of the builder and its image-A callees. `0x1436F3DC8` (G pointer) and `0x14350B340` (stack cookie) are data, not constants. The `0x142565D30` value is the low dword of an 8-byte abs mask (not re-read as 8 bytes).

| VA | bits | value | used by (C-dec unless noted) |
|---|---|---|---|
| 0x14253EE30 | 42c80000 | 100.0 | 0x142138220 (`fVar20`, lerp divisor); 0x140699960; 0x142139650 |
| 0x142565600 | 3f000000 | 0.5 | many: AT half-cone (0x140A53DF0 `param_5·0.5`), midpoints |
| 0x142565604 | 40000000 | 2.0 | 0x140699960, 0x14069A500 |
| 0x1425656D0 | 80000000 | -0.0 | sign mask: `x ^ mask` = `-x` in 0x142119390, 0x142137F90, 0x142138220, 0x142138FF0 |
| 0x142565D30 | ffffffff (low dword) | nan (low dword) | fabs mask (AND) in 0x140A53DF0, 0x140A90C20 |
| 0x14256AE1C | 3f800000 | 1.0 | default fallback (0x142138FF0, 0x142139410, 0x142139080) |
| 0x14256AE34 | 447a0000 | 1000.0 | 0x142138220 (`uVar4`, segment parameter) |
| 0x1425795E4 | 40c90fdb | 6.2831855 (2π) | angle wrap in 0x14045B090, 0x14045DF70 |
| 0x142588200 | 3dcccccd | 0.1 | 0x140699960 |
| 0x142588210 | 41200000 | 10.0 | 0x142139080 (`fVar5`, 10 m); 0x140617CA0 penalty |
| 0x142588218 | bf800000 | -1.0 | sign literal (0x140698340, 0x140699960, 0x142117C30, 0x142139080) |
| 0x14258C260 | 3f666666 | 0.9 | 0x140699960 |
| 0x14258C264 | 40400000 | 3.0 | divisor `F.x/3` and `-2F.x/3` (0x142119390, 0x142139410, 0x142139080) |
| 0x142590D98 | 3e99999a | 0.3 | 0x142117C30 |
| 0x142591140 | 41f00000 | 30.0 | 0x142117C30 |
| 0x14259222C | 40a00000 | 5.0 | 0x142119390 (goal-mouth band), `F+0x18 − 5.0` (0x142139410); 0x140699960; 0x14069A500 |
| 0x142598578 | 3e4ccccd | 0.2 | 0x142138FF0 (round trip `v·0.2/0.2`, treat as `v`); 0x142139080. The 0.2 weight in 0x142137F90 is a code literal |
| 0x142598580 | 43340000 | 180.0 | wrap `x − 180` (0x142138220 `fVar22`) |
| 0x14259858C | 43b40000 | 360.0 | wrap (0x140470D00, 0x140470D60, 0x142117C30) |
| 0x142598598 | c3b40000 | -360.0 | wrap (0x142138220) |
| 0x14259A760 | 420c0000 | 35.0 | curve centre `u = z·scale + 35` (0x142139650); sample base (0x142138220) |
| 0x14259A764 | 42700000 | 60.0 | goal-mouth facing threshold (0x142119390, 0x142117C30) |
| 0x14259A768 | c0000000 | -2.0 | `F.x·(-2)/3`: own-third line (0x142119390) |
| 0x14259B89C | 3ecccccd | 0.4 | 0x142139080 |
| 0x14259BDC4 | 34000000 | 1.19e-7 | parallel-test epsilon (0x140A90C20: `|den| < ε` → no hit) |
| 0x14259BDD0 | 3f31c71c | 0.6944 | `r = 0.6944/fps` (lane-core §6.3 `vel_eff`) |
| 0x14259BDD8 | 3fa66666 | 1.3 | 0x14069A500 |
| 0x14259BDDC | 3fc00000 | 1.5 | 0x140699960 |
| 0x14259BDE4 | 40200000 | 2.5 | 0x142139080 |
| 0x14259BDF4 | 41000000 | 8.0 | 0x142117C30 |
| 0x14259BE0C | 41700000 | 15.0 | 0x142139080 |
| 0x14259BE1C | 41a00000 | 20.0 | 0x142117C30 |
| 0x14259BE20 | 41c80000 | 25.0 | 0x140699960 |
| 0x14259BE30 | 42340000 | 45.0 | 0x142117C30 |
| 0x14259BE34 | 42480000 | 50.0 | 0x142139080 |
| 0x14259BE40 | 42652ee0 | 57.2957764 (180/π) | rad→deg |
| 0x14259BE4C | 428c0000 | 70.0 | curve clamp `u ≤ 70` (0x142138220, 0x142139650, 0x1421396E0) |
| 0x14259BE5C | 42b40000 | 90.0 | goal-mouth facing (`K+0x6C` set) (0x142119390, 0x140699960, 0x142117C30, 0x142139080) |
| 0x14259BE60 | 43070000 | 135.0 | 0x140699960 |
| 0x14259BE6C | 43200000 | 160.0 | 0x140699960 |
| 0x14259BE70 | 43610000 | 225.0 | 0x140699960 (15 m, squared) |
| 0x14259D5D4 | 3f99999a | 1.2 | 0x140698340 |
| 0x14259D654 | c0400000 | -3.0 | 0x14069A500 |
| 0x1425A2670 | 43c80000 | 400.0 | 0x140699960 (20 m, squared) |
| 0x1425AB4C4 | 441c4000 | 625.0 | 0x142139080 (25 m, squared) |
| 0x1425C65F4 | 44610000 | 900.0 | 0x142119390 goal-mouth reach (30 m, squared); 0x140699960 |
| 0x1425C65F8 | 451c4000 | 2500.0 | 0x142139080, 0x142138CF0 (50 m, squared) |
| 0x142BF10C0 | 44ea6000 | 1875.0 | 0x142139080 (divisor, `(d²−625)/1875`) |

---

## 1. Image builder `0x142119390` (CONFIRMED asm, `asm_142119390.txt`)

### 1.1 Signature, caller, arguments

- Entry `0x142119390`. Single E8 caller: MainEval `0x142110D30`, call at `0x142110E4B` (`e8scan.py`).
- Arguments at the call (`0x142110E21`–`0x142110E4B`, CONFIRMED asm):

| builder arg | value at call | role |
|---|---|---|
| `param_1` (RCX=RSI) | `BP+0xEC` | image A (IA) |
| `param_2` (RDX) | `[MainEval.p2 + 0x38]` (`ctx[1]`) | team-AI object X1; `T18 = *(X1+0x18)` (lane-core) |
| `param_3` (R8) | `BP+0x1C` | own identity block `C`: `C[0]` u32 slot, `C[2]` u32 side, `C[5]` float attack sign at `+0x14` |
| `param_4` (R9) | `BP+0x78` | own kinematics `K`: `K+0x40/44/48` ball point (x, height, z); `K+0x6C/0x6E` flags |
| `param_5` (stack `+0x20`) | MainEval `RDI` | not used by the builder body |
| `param_6` (stack `+0x28`) | `[BP+0x18]` | flags word: bit 0 pressure; bits 1–2 (`&6`) b6c; bit 3 (`&8`) goal-mouth |

- **Gate**: `0x142110DE2 CMP byte [BP+0x10],0; JZ 0x142111183`. Between that and the call the only branch (`0x142110E05`) selects the Intention argument `BL`; both paths reach the call. So the builder runs on every MainEval for an active BP, before the rethink gate (CONFIRMED). Cadence: once per logic tick per active BP (C-now `pes-re-A4Carrier.md` §2; the 54 Hz rate is INF from `fps = 54`).
- `C` is the BP's own identity. The phrase "ctx[0] = &BallPlayer+0x1C" in `pes-re-carrier-ai.md:60` is this block (offsets CONFIRMED by `LEA R8,[RBP+0x1C]` at `0x142110E3F`).

### 1.2 Call graph in order (CONFIRMED)

| step | CALL VA | callee | args (asm) | writes | section |
|---|---|---|---|---|---|
| a | 0x1421193DD | `0x140A48A30(G, C[2])` | team(side) | none (result dropped) | — |
| b | 0x1421193EC | `0x140A489F0(G, C[0])` | AI record | none | — |
| c | 0x1421193FB | `0x140A48A10(G, C[0])` | BP player record (`+0x55C` pos, `+0x574` vel) | none; kept in RDI | — |
| d | 0x142119411 | `0x140A48910(G, C[0])` | anim state | none | — |
| e | 0x14211941B | `0x140A3E190(E, 0)` | event record, `E = [X1+0xE0]` | none | — |
| f | 0x14211943D | `0x14063C670(IA, flag, &BP.pos, ctx)` | flag = `(C[2]==0)` | **IA+0x000** | R0 |
| g | 0x142119455 | `0x140698340(IA+0x10C, ctx, C, 0)` | zone | **IA+0x10C..** | R1 |
| h | 0x142119468 | `0x142138220(IA+0x2B0, ctx, C[2])` | curve | **IA+0x2B0..** | R2 |
| i | 0x142119482 | `0x142137F90(IA+0x898, ctx, C[2], IA+0x2B0)` | receiver table | **IA+0x898..** | R3 |
| j | 0x1421194A2 | `0x140698D10(IA+0x9CC, ctx, C, K+0x40, IA+0x10C, 0)` | shoot zone / IsClose | **IA+0x9CC..** | R4 |
| k | 0x1421194AD | `JZ 0x1421196A7` unless `flags & 8` | goal-mouth block | **IA+0x31C0** | R7 |
| l | 0x142119629 | `0x14063DD20(ctx, C[0], …)` | lane clear test (args C-dec) | bool into IA+0x31C0 | R7 |
| m | 0x1421196D0 | `0x142117C30(IA+0xB6C, ctx, C, K, flags, 0xFF)` if `flags & 6` | b6c | **IA+0xB6C..** | R5 |
| n | 0x1421196FA | `0x14069A500(IA+0x2124, ctx, C, K)` if `flags & 1` or `[IA+0x2124]==0xFF` | pressure | **IA+0x2124..** | R6 |
| o | 0x14211970F | `0x140699960(IA+0x2124, ctx, C, K)` | danger bool | **IA+0x31C1** | R8 |
| p | 0x142119746 | `0x14069A3E0(IA+0x2124, 0xFF)`, `0x142118CB0(IA+0xB6C, 3, 1)` | pressure record test | **IA+0x31C1 = 1** (forced) | R8 |

Steps a–e and the callees in f–p were checked for stores into the `T18`/`MA` block: none (§3.3, closure reasoning).

### 1.3 Decode notes

- Goal-mouth reach (`0x142119562`–`0x142119584`, CONFIRMED): the test point is the **BP's own position predicted `fps/3 = 18` frames ahead** (`pred = pos + vel·18`, velocity `+0x574/+0x57C` of `lVar11 = player(C[0])`). `d² = 0x140A8F890(pred, (sign·F.x, 0))`; gate `d² < 900` (`0x1425C65F4`) and `IsClose(IA+0x9CC)` via `0x140699710` (`0x142119571`). The ball is not used here (corrects carrier-ai §4.2; §6).
- Own-third test (`0x142119720`–`0x14211973F`, CONFIRMED): `F.x·(−2)/3` (`0x14259A768`, `0x14258C264`) against `sign(C[5]) · K.x` (`K+0x40`, the ball x). If the ball is beyond the own third, the danger path runs.
- `IA+0x31C0` writes: `0x1421194B3` clear (when `flags & 8`); `0x142119632` set (lane test `0x14063DD20` true); `0x142119690` set (facing test, §2.8).
- No builder body store reaches `IA+0x000..0x31C1` directly. Every row is written by a callee (§1.2).

---

## 2. Image-A rows

Notation: `IA` = `BP+0xEC`. `s = sign(T(side)+0x254)` (char, ±1; read in 0x142138220, 0x142137F90, 0x142138FF0). `F` = `G+0x2AA8` (pitch record), `F.x = F[0]`, `F+0x18` = pitch bound. `T(side) = 0x140A48A30(G, side)`.

### 2.1 R0: opponent list `IA+0x000` (`0x14063C670`; CONFIRMED asm, C-dec body)

- Layout: `uint count` at `IA+0`; `{ int slot; float dist; float bearing; float x; float y; float z }` × 11 from `IA+4` (24 B; asm stores at `param_1[6k+1..6k+6]`).
- Population: block `flag·11` with `flag = (C[2]==0)`: the **opposite side** of BP (slots 11..21 for side 0, 0..10 for side 1). Only slots `< 22` with `G+0x270+0x1748+slot != 0` (in play) and `G+0x270+0x175E+slot == 0` (not sent off).
  - `dist = sqrt(dx² + dz²)`, `(dx,dz) = opp − carrier` (`0x140A8F860` = `sqrt(0x140A8F890)`; C-dec).
  - `bearing = 0x140474C40(carrier, opp)` (bearing carrier→opp, degrees; name from carrier-helpers §3; asm not re-read).
- Sort: `0x14063C250` (merge sort over 24-B elements, only when `count > 1`). Key: comparator at `LAB_14063CD60`, not decoded (INF: `dist`). Tie rule OPEN.
- Rewritten every call (`*param_1 = 0`, CONFIRMED).

### 2.2 R1: zone `IA+0x10C` (`0x140698340`; writer CONFIRMED, body C-dec, not decoded)

- Writes `IA+0x10C + 4k` floats up to `IA+0x214` (asm/decompile `param_1[3..0x43]`). Inputs: `C`, and the team lines `*(ctx+0x18)+0x40+side·0x14` (CONFIRMED read). Constant `0x14259D5D4` (1.2, C-dec). Field semantics: OPEN. It is the 5th argument of `0x140698D10` (§2.5).
- Size check (CONFIRMED arithmetic): R0 occupies `4 + 11·24 = 0x10C` bytes, so R1 starts exactly after it.

### 2.3 R2: curve table `IA+0x2B0` (`0x142138220`; call CONFIRMED, body C-dec)

Layout (offsets from `IA+0x2B0`; C-dec uint index in brackets):

| offset | element | meaning |
|---|---|---|
| `+0x000` | uint `count` `[0]` | entries (71 when built) |
| `+0x004 + 12·i`, `i = 0..70` | float `x_i`, `y_i`, `z_i` | sample `i`; `curve()` reads `x_i` |
| `+0x34C` | = `x_70` `[0xD3]` | end value (`curve` at `i == 70`) |
| `+0x358` | uint segment count `[0xD6]` | segment list length |
| `+0x35C + 20·k` | `{float slot; x,y,z; float w}` `[0xD7+5k]` | candidate segment list |
| `+0x5C8`, `+0x5CC`, `+0x5D0` | `(x, y, z)` deepest point `[0x172..0x174]` | front-most candidate point |
| `+0x5D4`, `+0x5D8`, `+0x5DC` | `(x, y, z)` best-of-side point `[0x175..0x177]` | |
| `+0x5E0` | max signed value `[0x178]` | |
| `+0x5E4` | **scale = s** `[0x179]` | `curve()` scale: `u = z·s + 35` |
| `+0x6E8` | count `[0x10E]` | point list (`0x114` base, 9-uint entries) |

Procedure (C-dec, `dec_0x142138220.c` lines 61–350):
1. `s = sign(T(side)+0x254)`; `psi = T(side)+0x250`; `a = wrap(psi − 180)` (`0x142598580`, `0x14259858C`).
2. Candidates: each opponent slot `flag·11 + j`, in play, not the keeper (`0x140A638C0(ctx, slot)` false). Lane list of `(slot, s·x)`, max 11 (`0x14063CD70`, `0x14063E9D0`). **If the list is empty the table is not rebuilt** (`if (iVar7 != 0)`, line 94): rows persist (CONFIRMED guard).
3. Sort the list by depth (`0x14063EA30` → `0x141E53540`, 8-byte entries).
4. For each candidate: advance the opponent point 1.0 m along psi. `0x140A8F7C0(out, &a, d)` adds `d·sin` to x and `d·cos` to z; the angle is `wrap(wrap(a) − 180)` = psi (`0x140470D00`); `d` is `xmm14` = `[0x14256AE1C]` = 1.0 at the call site `0x1421384CE` (decoded at e950efa). Dedupe with `0x140A53DF0`: `r_lo` = `xmm10` = 0, `r_hi` = `[0x14256AE34]` = 1000, and the cone is `0.5 · [0x14253EE30]` = 50 about the entry's stored angle, which is `a` (`xmm6` at the append). Append `{slot, point}` to the segment list.
5. Initial deepest point per sample: `x = −s·F.x` (line 187), `z_i = (i−35)·s` (line 186).
6. **Per sample `i = 0..70`**: intersect each segment with the sample line (`0x140A90C20`, C-dec: 2-D segment intersection, `0` when `|den| < 1.19e-7`, else `(t,u)`); update `x_i = max_s(x_hit)` (strict `<` on `s·x`, starting from `−s·F.x`); store `(x_i, y_i, z_i)` at `IA+0x2B4 + 12i`.
7. The deepest-point block (`+0x5C8..`) is `max_s(x)` over the segment points (lines 160–164, 235–243).

Curve evaluation `0x142139650` (C-dec, same as lane-core §6.3): `u = clamp(z·scale + 35, 0, 70)`, `i = floor(u)`, `i == 70 → x_70`, else `lerp(x_i, x_{i+1}, u−i)`. With `scale = s`, `u = i` at `z = (i−35)·s`, so samples are exact.

Verdict: writer CONFIRMED (`0x142138220`, this = `IA+0x2B0`). Per-sample rule decoded at e950efa: the arm crossing is `0x140A90C20`, with the sample-line parameter `t` (its first output) giving x and the arm parameter `u` (its second output) filtered by `u ≥ 0`. `x_i` starts at `−s·F.x` and takes the strict maximum. The arms are at `a ± 50` with length 1000 (`xmm12`). OPEN: (b) whether `x_i` is world x or rotated lane x (INF: world; the lateral sign cannot change the curve, because the arm pair and the dedupe cone are symmetric about the attack axis); (c) the sort tie rule (equal `s·x` keys only).

### 2.4 R3: receiver score `IA+0x898` (`0x142137F90`; structure CONFIRMED, body C-dec)

- Layout: 28 B per teammate `j = 0..10`, base `IA+0x898 + 28j`, slot `C[2]·11 + j` (own side: uVar3 = `param_3·11 + j`, CONFIRMED). Fields `[0]` result, `[1]` v_a, `[2]` 0.2, `[3]` v_b, `[4]` 1.0, `[5]` v_c, `[6]` 1.0.
- Write rule: only slots `< 22` in play. Other rows are **not** zeroed (stale values persist; CONFIRMED guard).
- `result = (0.2·v_a + 1.0·v_b + 1.0·v_c)/(0.2+1+1)`; denominator 2.2 (CONFIRMED arithmetic order `f[-3]·f[-2]`, `f[-1]·f[0]`, `f[2]·f[1]`).
- Sub-terms (C-dec):
  - **v_a** = `0x142138FF0(ctx, side, pos)`: `v_a = 0` if `s·x ≤ −F.x`; `1` if `s·x ≥ F.x`; else `(s·x + F.x)/(2F.x)`. Teammate depth normalised 0 (own line) → 1 (opponent line). The `·0.2/0.2` round trip is a float no-op. C-dec exact.
  - **v_b** = `0x142139410(ctx, side, pos, C=IA+0x2B0)` = `t² · g_a · g_b`:
    - `lo = min(s·C[0x5D4], F.x/3)`; `hi = min(s·C[0x5C8], F+0x18 − 5)`;
    - `t = 0` if `s·x ≤ lo`; `1` if `s·x ≥ hi`; else `(s·x − lo)/(hi − lo)`;
    - `m = max(0, scale·(C[0x5C8] − curve(z)))`; `g_a = s·(x − curve(z))/m` if `m ≠ 0` else `1`;
    - `g_b = scale·(C[0x5C8] − curve(z))/C[0x5E0]` if `C[0x5E0] ≠ 0` else `1`.
    - C-dec exact given the curve table.
  - **v_c** = `0x142139080(ctx, side, pos, &lst)`: sums over a per-slot defender list `lst` (built by `0x142138CF0`; count at `+0x2C`, 32-B entries from `+0x30`), with distance bands `0x1425AB4C4` (625), `0x142BF10C0` (1875), `0x1425C65F8` (2500), `0x14259BE34` (50), `0x14259BE0C` (15), `0x14259BDE4` (2.5) and helpers `0x140474C40`, `0x14043A0A0`, `0x1415A1780`. **Body partly read (C-dec); `0x142138CF0` not decoded: OPEN.**
- Consumers: carrier-helpers §5 "`imageA+0x898` per-slot floats UNKNOWN" now has its writer. Whether the OneTwo/DHB receiver predicates (lane-core §8) read this table is INF; they read a different table (`T18+0x228D4` block, §3.1).

### 2.5 R4: shoot zone / IsClose `IA+0x9CC` (`0x140698D10`; writer CONFIRMED, body not re-decoded)

- Called at `0x1421194A2` with `(IA+0x9CC, ctx, C, K+0x40, IA+0x10C, 0)`. Consumers: `IsClose` `0x140699710`/`0x140699820`, `0x14063E010`. Carrier-ai §4.1 and carrier-helpers §4.1 describe the shoot zone (stat `0x1A`, near range `18 + 3·clamp01((x−70)/29)`). Not re-decoded here.

### 2.6 R5: b6c `IA+0xB6C` (`0x142117C30`; writer CONFIRMED, body in lane-core §9 / carrier-helpers §5)

- Called at `0x1421196D0` with `(IA+0xB6C, ctx, C, K, flags, 0xFF)` when `flags & 6`. Not re-decoded here.

### 2.7 R6: pressure `IA+0x2124` (`0x14069A500`; writer CONFIRMED, body in carrier-ai §4.2)

- Called at `0x1421196FA` with `(IA+0x2124, ctx, C, K)` when `flags & 1` or `[IA+0x2124] == 0xFF`. Entry `0x14069A3E0(IA+0x2124, slot)`, stride `0x5C`, base `+0x33`; counts `+0x30/+0x34/+0x38`; per-opponent distances `+0x4C + k·0x1C`, angles `+0x50 + k·0x1C` (carrier-ai §4.2). Constants `0x14259222C`, `0x142598578`, `0x14259D654`, `0x14259BDD8`, `0x142565604`.

### 2.8 R7: goal-mouth `IA+0x31C0` (inline in the builder; CONFIRMED structure)

Set to 1 when, in order:
1. `flags & 8` (`0x1421194AD`).
2. `d² = |pred − (sign·F.x, 0)|² < 900` and `IsClose(IA+0x9CC)` (`0x142119562`–`0x142119584`), where `pred` = BP position `+18` frames (§1.3).
3. Either the lane clear test `0x14063DD20` is true (`0x14211962E` → set), **or** `d² < IA+0xB54²` and then (`0x142119639`–`0x14211968E`, CONFIRMED):
   - `heading = wrapDiff(BP+0x554, bearing(BP.pos → goal))` via `0x140474C40` and `0x1404583B0`;
   - if `K+0x6E != 0` → set (`0x142119672`–`0x142119690`);
   - else threshold `thr = 60` if `K+0x6C == 0`, `90` if `K+0x6C != 0` (`0x14259A764`, `0x14259BE5C`); set iff `heading < thr`.

### 2.9 R8: danger `IA+0x31C1` (`0x140699960`; writer CONFIRMED, body in carrier-ai §4.2)

- `0x14211970F`: `IA+0x31C1 = 0x140699960(...)`. Then the builder's forced set (`0x142119714`–`0x142119770`, CONFIRMED): if `sign(C[5])·K.x < F.x·(−2)/3` (`K+0x40`, the **ball** x), and the pressure record `[0xFF].+0x30 > 0`, and `0x142118CB0(IA+0xB6C, 3, 1) == 0`, set `IA+0x31C1 = 1`. This is the own-third danger rule (carrier-ai §4.2 shape).

---

## 3. Team-AI rows `T18 = MA`

### 3.0 Identity of `T18` and `MA` (INF on the link; offsets CONFIRMED)

- Team-AI tick `0x140457070` (C-dec): `MA = *(this+0x508)`. The ball-list writer and the goal-geometry writer index `MA+0x228D4..0x22E4C` on that object.
- lane-core `T18 = *(X1+0x18)` and teamctx `MA = X+0x18` use the same offsets (`0x228EC`, `0x22E4C`). Link `this+0x508 ↔ X1+0x18` is INF: `X1` and `this` would be the same team object, while teamctx says `X` is per player. **OPEN**: a write watch on `this+0x508` at match setup settles it.
- Caller of the tick: not traced (INF: per frame; `0x140457070` is called from the MA dispatcher, teamctx §2.4).

### 3.1 `MA+0x228D4..0x228E8` goal geometry (`0x14045DF70`, `0x14045B090`; stores CONFIRMED, formulas C-dec)

- `0x14045DF70` is called at `0x140457070` line 43, when `param_2 != 0`. `0x14045B090` is called at line 236 when the time-slice below fires.

| offset | value | formula (C-dec) | store VA |
|---|---|---|---|
| `+0x228D4` | heading, deg | `wrap360(atan2(−x0, 0))·57.2958`; `0` if `x0 = 0`; `x0 = *(*(this+8)+0x10)` (INF: `x0 = F.x`) | `0x14045DFC5` |
| `+0x228D8` | heading, deg | `wrap360(atan2(x0, 0))·57.2958`; `0` if `x0 = 0` | `0x14045DFD8`, `0x14045E012` |
| `+0x228DC` | d² | `ball.z² + (ball.x + x0)²` (ball = `0x140A57800`) | `0x14045B2E7` |
| `+0x228E0` | heading, deg | `wrap360(atan2((−x0) − ball.x, −ball.z))·57.2958`; `0` when both zero | `0x14045B35A` |
| `+0x228E4` | d² | `ball.z² + (ball.x − x0)²` | `0x14045B388` |
| `+0x228E8` | heading, deg | `wrap360(atan2(x0 − ball.x, −ball.z))·57.2958`; `0` when both zero | `0x14045B3E0` |

- `atan2` = `0x140A92D40(dx, dz)` (C-dec; argument order INF, O3 in lane-core). `wrap360`: add `2π` when negative.
- Meaning (INF): the headings and squared distances from the ball to the two goal centres `(±x0, 0)`.

### 3.2 `MA+0x2470C` ball NearestEntry (200 B) and `MA+0x247D4` ball-bearing table (88 B) (`0x14045B090`; layout CONFIRMED, formulas C-dec)

- **Time slice** (`0x140457070` lines 215–237, C-dec): `this+0x10` counts ticks. `0x14045B090` runs when `param_2 != 0` or when `this+0x10` exceeds `iVar23 = 4 − step(|ball velocity|)` (`0x140A57720`, `0x1415A1780`; step formula C-dec). It then resets `this+0x10 = 0`. So the ball list is refreshed every 1–5 ticks, not every tick (INF on the step table).
- **NearestEntry at `MA+0x2470C`** (layout = teamctx §2.1; CONFIRMED by init stores): `{u32 id, f32 d²}` × 11 per team at `+0x00 + 8k` (team 0) and `+0x58 + 8k` (team 1); counts `u8` at `MA+0x247BC + side`; rank `u8[22]` at `MA+0x247BE` (`+0xB2`), init `0x0A` (the absent rank, `kNearRankAbsent`). Ids init `0xFF`.
  - Entries: for each slot `< 22` in play (`*(*(this+8)) + 0x1748 + slot`, the same flag as R0) and not sent off: `d² = (ball.x − p.x)² + (ball.z − p.z)²`; id = slot; appended to its team block; count++.
  - Then `0x140A79640(MA+0x2470C, 0)` ranks it (sort by `d²`; tie rule OPEN).
- **Ball-bearing table at `MA+0x247D4`** (`float[22]`): `angle[slot] = wrap360(atan2(p.x − ball.x, p.z − ball.z))·57.2958` for each active slot; `0.0` for the rest (init loop of 22 stores, CONFIRMED).
- **This is not the table read by `0x140A71260`.** That function reads `T18+0x22A7C` (k≠1) and `T18+0x22C64` (k==1) (CONFIRMED `lea` at `0x140A7126C`, `0x140A71440`, `0x140A71260`). `MA+0x247D4` is ball-relative and has no k. GoalFront's facing stand-in therefore stays.

### 3.3 Score, list and angle blocks `MA+0x228EC..0x22CBC` and pair `0x22CBC/0x22D84` (writer **OPEN**)

Method (CONFIRMED scan of the text `0x140001000–0x14252F000`):

1. **Direct stores** (`mov/movss/movq/add [reg+disp]`, disp `0x22xxx`/`0x23xxx`): 21 sites (`store_hits.txt`). The `0x228D4..0x228E8` stores are the goal-geometry writers of §3.1. The `0x22144/0x2214C/0x22154` stores (in `0x140B9B4EF`, `0x1412F6723`, `0x141327DDD`, `0x14133B6FD`) are another object below the NearestList; not analysed.
2. **Any reference** to immediates `0x228E..0x22EE` (`refs_band.txt`, 772 lines). For the list, angle and score bases (`0x229B4`, `0x22B9C`, `0x22A7C`, `0x22C64`, `0x228EC`, `0x22AD4`), and the pair (`0x22CBC`, `0x22D84`), the sites are `mov reg,imm` or `add/lea reg,[…+imm]` used as **block-selection bases**. Checked (decompile, `decB_*.c`): `0x140512CD0`, `0x14057A580`, `0x1405C0630`, `0x1405DF430`, `0x1405E6E80`, `0x1406216A0`, `0x1406295A0`, `0x14062D350`, `0x140958D00`, `0x14096F029`, `0x14097CFA0`, `0x140548F20`, `0x140618382` (`0x140617CA0`), `0x14066E900`, `0x1405D05A0`. **All read** `*puVar` (ids, `d²`) through the selected base (for example `0x1405DF430`: `puVar9 = lVar3 + 0x22b9c / 0x229b4`, then `*puVar4` read and counted). None stores into a block.
   - **Selection conflict** (OPEN): `0x140512CD0` selects `0x22B9C` when team `+0x254 < 0`, while `0x1405DF430` selects `0x22B9C` when `+0x254 > 0`. Either the two functions read different team objects, or one decompile has the sign inverted. Not resolved.
   - The pair base: `0x140548F20` and `0x14066E900` select `0x22CBC` when `MA+0x24834 > 0`, else `0x22D84`.
3. **Stores through a computed base**: not found by this method. Rank builder `0x140A79640` has 73 callers; the one checked (`0x140617CA0`, `decB_140618382.c`) builds a **stack** list `local_1b8` of `(id, score)`, ranks it, and returns a kind. Its score is distance from the carrier plus penalties, with the point `(sign(MA+0x2482C)·F.x, sign(MA+0x24834)·F.z8)`. It is a per-carrier receiver ranking, not the team-row writer. The other 72 callers were not checked.

Conclusion: the score (`0x140587E70` reads `base+4+8·rank`), list (`0x140A714E0`) and angle (`0x140A71260`) blocks have **no writer found** by immediates or by the store scan. INF: they are written through a pointer the immediate scan does not see (a copy from a stack list, or a loop-indexed base). **OPEN** next steps: (a) decompile the 72 unchecked `0x140A79640` callers and look for `*(float*)(p+4)` stores; (b) a read-only write watch on `MA+0x228EC` during one kick, if a PES match is running.

### 3.4 NearestList `MA+0x22E4C + 200·i` (writer OPEN, teamctx §2.4)

- Layout CONFIRMED (teamctx §2.1). No writer by immediate scan (the `0x22E4C` immediate appears only in reads). The pair `0x22CBC/0x22D84` ends at `0x22E4C` (`0x22D84 + 0xC8 = 0x22E4C`, CONFIRMED arithmetic), so the pair is the last 200-B row before the NearestList.

---

## 4. Port spec (pure functions over GF state; signatures)

Types (GF side): `Vec3 {float x, y, z}` (world; xz for distances); `PlayerView` (slot, side, active, sentOff, pos, vel, facing); `Pitch {float halfX; float bound; float z8}`; `TeamView {float attackDeg; int sign}`.

```cpp
// R0. Opponents of the carrier, sorted by xz distance (ties: OPEN, use stable slot order).
struct OppEntry { int slot; float dist; float bearingDeg; Vec3 pos; };  // 24 B in PES
int BuildOpponentList(const std::array<PlayerView,22>& p, int carrierSlot, int carrierSide,
                      Vec3 carrierPos, OppEntry out[11]);

// R2. Curve table (71 samples). PARTIAL: segment construction OPEN (§2.3 step 4).
struct Curve { float x[71]; float xEnd; float scale; /* = team sign */ };
float CurveEval(const Curve& c, float z);   // EXACT (C-dec):
                                            //   u = clamp(z*scale+35, 0, 70); i = floor(u);
                                            //   i == 70 ? xEnd : lerp(x[i], x[i+1], u-i)
// BuildCurve(...) : OPEN in its segment step.

// R3. Receiver score for teammate j (0..10).
float FrontFraction(float teamSign, float halfX, Vec3 pos);            // v_a, C-dec exact
float CurveLineTerm(const Curve& c, float teamSign, const Pitch& F,
                    Vec3 pos, float deepestX, float bestX);            // v_b, C-dec exact given Curve
float DefenderTerm(/* v_c: needs 0x142138CF0 list, OPEN */);
float ReceiverScore(float v_a, float v_b, float v_c);                  // (0.2*v_a + v_b + v_c)/2.2
// Write rule: only for active slots; other rows keep their previous value.

// T18: goal geometry (MA+0x228D4..0x228E8). C-dec; x0 = F.x is INF.
struct GoalGeom { float hdgNegX0, hdgZero, d2NegX0, hdgToNegX0, d2PosX0, hdgToPosX0; };
GoalGeom BuildGoalGeometry(Vec3 ball, float x0);

// T18: ball NearestEntry (MA+0x2470C) and ball-bearing table (MA+0x247D4). C-dec.
struct NearEntry { uint32_t id; float d2; };
struct BallNearest { NearEntry team[2][11]; uint8_t count[2]; uint8_t rank[22]; };  // rank absent = 0x0A
void BuildBallNearest(const std::array<PlayerView,22>& p, Vec3 ball, BallNearest& out);
void BallBearings(const std::array<PlayerView,22>& p, Vec3 ball, float out[22]);   // deg in [0,360)
// Time slice (every 1..5 ticks, §3.2) is the caller's job.

// Goal-mouth reach (IA+0x31C0). Structure CONFIRMED (§2.8). Uses the BP's own predicted position.
bool GoalMouthReach(Vec3 bpPredicted18, float halfX, float sign, bool isClose, bool clearLane,
                    float bnd54, float headingDiffDeg, bool kFlagE /*K+0x6E*/, bool kFlagC /*K+0x6C*/);
```

The `T18` score, list and angle blocks (`0x228EC..0x22CBC`) and the NearestList (`0x22E4C`) have **no spec**: the writer is not identified (§3.3, §3.4).

---

## 5. Stand-in replacement map

| GF stand-in (where) | PES row | Replaced exactly by this decode? | Action |
|---|---|---|---|
| GoalFront facing vector as per-slot angle table `0x140A71260` (`elizacontroller.cpp`; `pescarrier.cpp` GoalFront) | `T18+0x22A7C/0x22C64` (k-blocks) | **No.** Writer OPEN. `MA+0x247D4` looks similar but is ball-relative and k-free | keep; `// ponytail:` cites the OPEN writer |
| Carrier NearestList / `NearestOpponents` (GF `BuildNearest`, `pesoffball.cpp`) | `MA+0x22E4C` | **No.** Layout known, writer OPEN | keep |
| Ball NearestEntry (GF `BuildNearest` owner-list) | `MA+0x2470C` | **Partly.** Layout and rule exact (C-dec); time slice and tie rule to add | do not treat as the NearestList; it is ball-centred |
| Curve table `IA+0x2B0` (one-touch G12 stand-in; BackSpace validator `0x1421225E0` stand-in) | `0x142138220` | **Partly.** Writer and sample rule known (§2.3); segment construction OPEN | keep; port `CurveEval` once `BuildCurve` is decoded |
| `imageA+0x898` per-slot receiver value (carrier-helpers §5 "UNKNOWN") | `0x142137F90` | **Partly.** `v_a`, `v_b` C-dec exact; `v_c` OPEN (`0x142138CF0`) | keep absent until `v_c` is decoded |
| Opponent-of-carrier distance lookup (GF) | `IA+0x000` opponent list | **Yes for the distance rule** (xz `sqrt`, carrier-relative, opposite side, in play, not sent off). Tie rule OPEN | may replace GF lookups that mean "opponents of the carrier"; ball-relative lookups are a different origin |
| Danger `IA+0x31C1` (GF stand-in, carrier-ai §4.2) | `0x140699960` + builder forced set | **Partly.** Forced-set condition exact (ball `K.x`); `0x140699960` body not re-read | keep stand-in |
| Goal-mouth `IA+0x31C0` (GF stand-in) | builder inline | **Yes for structure** (§2.8). The predicted point is the **BP's own** predicted position, not the ball | fix the stand-in input if it uses the ball |
| Pressure `IA+0x2124` / b6c `IA+0xB6C` (GF stand-ins) | `0x14069A500` / `0x142117C30` | **No.** Writers known; bodies not decoded here | keep |
| Shoot zone `IA+0x9CC` (`0x140698D10`) | R4 | **No** (not re-decoded this session) | keep |

---

## 6. Doc corrections (doc:line, old, new, evidence)

1. `docs/re/pes-re-lane-units2.md:456` (§9 item 1): old "decode the team-table builder (candidate: the image builder `0x142119390`) and search its stores for `T18+0x228EC/…`". New: the image builder writes **no** `T18` row; its callees write image A only. The `T18` writers found: goal geometry `0x14045DF70` and ball NearestEntry/angles `0x14045B090`, both from the team-AI tick `0x140457070`. The sign-selected rows have no writer found (§3.3). Evidence: `asm_142119390.txt` (no `T18` store), `decA_14045b2e7.c`, `decA_14045dfc5.c`, `store_hits.txt`, `refs_band.txt`.
2. `docs/re/pes-re-lane-units2.md:457` and `:408` (S2, O6): old "writer OPEN (O6)". New: writer **`0x142138220`**, called at `0x142119468` with `this = IA+0x2B0`. Evidence: builder asm `0x142119465`–`0x142119468`, `dec_0x142138220.c`.
3. `docs/re/pes-re-carrier-helpers.md:582` (angle table): old "only readers found; the filler uses a computed address". New: CONFIRMED that the immediates `0x22A7C` and `0x22C64` occur only in the readers `0x140A71260`/`0x140A71440` (`refs_band.txt`). Writer still OPEN; "computed address" is now a checked fact.
4. `docs/re/pes-re-carrier-helpers.md:585` and `docs/re/pes-re-carrier-ai.md:278–283` (danger `+0x31C1`, goal-mouth `+0x31C0`): old "`imageA+0x31C0` = goal-mouth reachable: **ball** predicted 18 frames ahead within 30 m". New: the predicted point is the **BP's own** position (`lVar11 = player(C[0])`, velocity `+0x574/+0x57C`), 18 frames; the ball enters only the own-third test (`K+0x40`, `F.x·(−2)/3`). Evidence: `0x142119518`–`0x142119558`, `0x142119720`–`0x14211973F`.
5. `docs/re/pes-re-carrier-ai.md:60–61`: old "`ctx[0]` = `&BallPlayer+0x1C` (u32 player index …)". New: confirmed as the BP's identity block (`C[0]` slot, `C[2]` side, `C[5]` sign at `+0x14`), distinct from the lane-core snapshot (lane-units2 C11). Evidence: MainEval `0x142110E3F` (`LEA R8,[RBP+0x1C]`).
6. `docs/re/pes-re-carrier-helpers.md:588–589` (`imageA+0x898` UNKNOWN): new writer `0x142137F90`, formula `(0.2·v_a + v_b + v_c)/2.2`, own-side teammates (§2.4).
7. `docs/re/pes-re-A4Carrier.md:40` (row d, image units): old "image A updated by `0x142119390`, holds the receiver tables". New: image A holds the opponent list (R0), zone (R1), curve (R2), receiver score (R3), shoot zone (R4), b6c (R5), pressure (R6), goal-mouth and danger (R7–R8). The `T18` team rows are not in image A.
8. `docs/re/pes-re-lane-core.md:291` and `:405` (O6): old "Table contents: runtime (OPEN O6)". New: writer `0x142138220` (§2.3); contents are determined by the opponent segments (segment step OPEN).
9. `docs/re/pes-re-teamctx.md:71–79` (§2.4 MA writer OPEN): partial. The ball list `MA+0x2470C` and the ball angles `MA+0x247D4` are written by `0x14045B090`, called from `0x140457070` (time-sliced). `0x14045B090` is not in the excluded list of §2.4. The `MA+0x22E4C` writer is still OPEN.
10. `docs/re/pes-re-lane-units2.md:153`, `:156`: unchanged. The `0x140A71260` reader is confirmed (`lea` at `0x140A7126C`, `0x140A71440`).

---

## 7. Open items (each with the next step)

1. **Writer of `T18+0x228EC..0x22CBC` (score, list, angle) and the pair `0x22CBC/0x22D84`.** Immediate and store scans both failed. Next: decompile the 72 unchecked `0x140A79640` callers (look for `*(float*)(p+4)` stores), then a read-only write watch on `MA+0x228EC` during one kick if a PES match is running.
2. **`MA+0x22E4C` NearestList writer** (as 1).
3. **Sign selection conflict** (§3.3): `0x140512CD0` (`+0x254 < 0 → 0x22B9C`) vs `0x1405DF430` (`+0x254 > 0 → 0x22B9C`). Next: re-read the team pointer each function takes.
4. **`this+0x508` vs `X1+0x18` identity** (§3.0). Next: write watch on `this+0x508` at match setup.
5. **Curve segment step** (§2.3 step 4): `0x140470D00` and `0x140A8F7C0` lose an argument in the bridge decompile. Next: asm of both and of the call-site loads at `0x142138220`, the `FUN_140a8f7c0(&fStack_1b4, uVar9)` call.
6. **`0x142138CF0`** (defender list used by `v_c`): decode.
7. **Sort tie rule** (`0x14063C250`, key `LAB_14063CD60`): decode the key.
8. **`0x140A92D40` argument order** (atan2 `(dx,dz)` vs `(dz,dx)`): one asm read settles it.
9. **`0x140698340` (R1 zone) and `0x140698D10` (R4 shoot zone)** bodies.
10. **Intention flags** `[BP+0x18]` (builder param 6): writer `0x142117370`. Not traced.
11. **Time-slice step** of the team-AI ball list (`iVar23`, §3.2): C-dec only.

---

## 8. Method and verification

- Bridge: `decompile_function`, `disassemble_function`, `decompile_function_by_address` on `http://127.0.0.1:8080`. Image `pes_full.bin`, base `0x140000000`.
- Scans: E8 caller scan (`e8scan.py`). Text disassembly: `objdump -D -b binary -m i386:x86-64 -M intel --adjust-vma=0x140000000` over `0x140001000–0x14252F000`, filtered into `refs_band.txt` (immediates `0x228E..0x22EE`) and `store_hits.txt` (store form, disp `0x22xxx`/`0x23xxx`). The first `objdump -d` run gave no output (raw binary needs `-D`).
- Constants: `consts.py` (§0).
- Not run: no build, no tests, no live PES read or write, no GF edit, no commit. This is a spec and a doc-correction list.
