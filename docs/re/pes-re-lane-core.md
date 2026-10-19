# PES lane core 0x14063CD90, wrapper, five helpers, one-touch 0x142128C10, OneTwo receiver predicate 0x142124950: port spec

Scope: decode-first steps 1–3 of `docs/re/pes-carrier-A4Cont.md` §4. Read-only: Ghidra bridge 8080, PES2021.exe on disk (PE section table), CarrierAIRE decompiles. No live reads (no PES process was running). No engine edits. Scratch: `<workdrive>/gf-LaneCore-scratch/` (`dec_*.c`, `asm_*.txt`, `sintable.txt`, `readva.py`).

Verdict: the lane core is fully decoded and specified below. Its five helpers are decoded; one has a live-unconfirmed meaning (keeper category). The one-touch predicate is decoded control flow with all gates asm-checked; several helper internals are open (§10). The receiver predicate is `0x142124950`, not `0x14063C9F0`. `0x14063C9F0` is a snapshot filler that the predicate calls only for one field (§7).

Legend: **C-asm** = raw disassembly checked. **C-dec** = decompile, flow consistent with asm. **INF** = inference. **OPEN** = not decoded; see §10.

---

## 0. Corrections to the brief and A4Cont

1. `0x14063DE20` is a thin argument packer. It builds a 7-float lane struct and calls `0x14063DC10`, which picks the opponent side, calls the core, and returns `count == 0` (C-asm). The brief's "wrapper" is that chain.
2. `0x14063C9F0` is not a predicate. It fills a 0x5C-byte snapshot of one player (C-asm for stores). Its caller `0x14063C560` zeroes fields and calls it. The OneTwo receiver predicate is `0x142124950`, which reads only snapshot `[0xD]` from it.
3. `0x142124950` reads the **b6c record A** (`base + 0x68 + 0xF8·k`, §9). The snapshot is a separate local object.
4. Ghidra's `(uint)` casts on float stores in `0x14063C9F0` are display artifacts. The asm uses `MOVSS` (bit copy) for `[4]` and `CVTDQ2PS` for `[5]` (C-asm). Those fields are floats.
5. Ghidra drops arguments in several calls (e.g. `FUN_140a48a30()`, `FUN_140439ef0(P)`). Arguments below come from asm.
6. **Angles are degrees.** The install constants are 180, 360, 90, 57.2958 (§2). Radians appear only inside `atan2` (`0x140A92D40`).

---

## 1. Conventions (C-asm + C-dec)

- Ground plane is (x, z). Height is y. Heading/bearing θ in degrees, `bearing(A→B) = atan2(B.x−A.x, B.z−A.z)` mapped to [0,360) (`0x140439EF0`, `0x14046F260`). θ = 0 is +z; increasing θ turns toward +x.
- Offset along heading: `dir(θ) = (sin θ, cos θ)` in (x, z), via `0x140A92C80`. Sine is a 512-step table of `sin(90°·k/512)`, k=0..512, at VA `0x142655950` (513 f32, read from install). Index = trunc(|θ mod 90|/90·512) with linear interpolation and quadrant folding. Max error vs exact sine: 5.2e-7 (checked).
- `wrap180(d)`: result in [−180,180] (`0x14043A0A0`, `0x140481B00`).
- `wrap360(a)`: result in [0,360) (`0x140470D60`, `0x14046F260`).
- Player record `P` (`0x140A48A10(G, slot)`, `G = [0x1436F3DC8]`): position `+0x55C/+0x560/+0x564` (x,y,z); velocity `+0x574/+0x578/+0x57C` (x,y,z), **per PES frame** (C-dec: speed = |v|·fps·3.6 km/h, `0x140477130`, so v is per frame); facing `+0x554` (deg); movement state `+0x5B0`; AI action `+0x5D0` (0x25 = one-two run).
- Frame rate `fps` = `DAT_14351AE54` = 54.0f (`0x141E532D0`; .data slot; disk value 54.0; docs record 54.0 mid-match). Speed is km/h.
- Match active flag: `MI+0x1748[slot]` (byte), `MI = *(G+0x270)` (C-dec, docs cross-check).
- Side object `T(side) = 0x140A48A30(G, side) = *(G + (side+0xA1)·0x28)`. Its fields used: `+0x250` attack angle (deg, float), `+0x254` attack sign (signed byte; becomes float ±1/0 in snapshots), `+0x4BD4[11]` int, `+0x4C00[11]` position category (0 = keeper per docs; see O1), `+0xA294` team level×100, `+0xA29C` flag (zeroes level), `+0xA2A0[11]` int action table, `+0xB8A4` attack level byte-int, `+0x8E08` table (`0x140A62180`).
- Roster object `R(side) = 0x140A489D0(G, side) = *(G+(side+0xA3)·0x28)`: `+4+4k` = roster number of the k-th on-pitch man (k<40).
- Team mode: `0x141052E30(M) = *(M+8)`, `M = param_1[6]` of the caller chain.
- Slot → side: side(slot) = isAway(slot) = `11 ≤ slot ≤ 21` (`0x14043C410`, C-asm).

---

## 2. Install constants (read from `PES2021.exe` via PE section table; all in `.rdata` unless noted)

| VA | value | use |
|---|---|---|
| 0x142598580 | 180.0 | half-turn (deg) |
| 0x14259858C | 360.0 | full turn (deg) |
| 0x1425DDFFC | −180.0 | fold constant in sine |
| 0x142598598 | −360.0 | fold constant in sine |
| 0x1425795E4 | 6.2831855 (2π rad) | `atan2` negative wrap |
| 0x14259BE40 | 57.29578 (180/π) | rad → deg |
| 0x142565600 | 0.5 | cone scale |
| 0x142588210 | 10.0 | cone offset |
| 0x14259BE5C | 90.0 | cone floor; sine quadrant |
| 0x14259BDC4 | 1.1920929e-7 | eps on cross-product denominator |
| 0x142588218 | −1.0 | sign |
| 0x14256AE1C | 1.0 | sign |
| 0x142565D30 | fabs mask | |
| 0x142578480 | +π (rad) | atan2 |
| 0x142609F98 | −π (rad) | atan2 |
| 0x1425A35A0 | +π/2 (rad) | atan2 |
| 0x14260B394 | −π/2 (rad) | atan2 |
| 0x142655950 | sin table, 513 f32 | sine (§1) |
| 0x1426550E8 | 512.0 | sine table scale |
| 0x14351AE54 | 54.0 (.data) | fps (live 54.0 per docs) |
| 0x14259A764 | 60.0 | 60·60/1000 factor base; one-touch lane cone; turn ramp start |
| 0x14256AE34 | 1000.0 | km/h factor |
| 0x14259BDD0 | 0.6944444 | standing speed floor (÷fps → per-frame length) |
| 0x14259BE0C | 15.0 | km/h gate |
| 0x142565604 | 2.0 | one-touch band; b6c writer factor |
| 0x14259CE40 | 1600.0 | one-touch score floor (d²) |
| 0x14259A760 | 35.0 | curve index offset |
| 0x14259BE4C | 70.0 | curve index max |
| 0x14259BE84 | −10.0 | one-touch band floor |
| 0x14253EE30 | 100.0 | speed-angle gate |
| 0x14259D628 | 110.0 | receiver facing threshold |
| 0x142596E6C | 80.0 | receiver angle threshold when carrier fast |
| 0x1425A9464 | 36.0 | NearestList d² gate |
| 0x1425A9BF0 | 130.0 | turn ramp end |
| 0x1425C4900 | −70.0 | turn ramp divisor |
| 0x14258C264 | 3.0 | window floor |
| 0x14259BE1C | 20.0 | pass-angle gate |
| 0x14256AE30 | 120.0 | receiver angle threshold |
| 0x14259222C | 5.0 | window base |
| 0x14259BE2C | 40.0 | openness class threshold (b6c writer) |

---

## 3. Lane core `0x14063CD90`

### 3.1 Signature (C-asm, args from Win64 registers and stack)

```
core(out: uint32[2]+entries*, _unused, c: int, s: int, L: float[7]*, keep: byte, h: uint, _unused2)
```

| Arg | Meaning | GF mapping (to implement) |
|---|---|---|
| `out` | `out[0]` = best slot (0xFF = none); `out[1]` = count (≤11); entries `{slot:int, d2:float}` at `out+8·i` | GF lane-result struct |
| `_unused` (RDX) | carried to `0x140A62690` and `0x140A53DF0`, never read | dead |
| `c` | reference slot, excluded from the scan | GF carrier/receiver slot |
| `s` | side to scan (0 or 1). Opposite of the reference in every observed caller | GF team index |
| `L[0..2]` | origin O = (x, y, z) | GF position of the origin (PES frame) |
| `L[3]` | θ, lane heading, deg | GF heading converted to PES bearing |
| `L[4]` | len, m | lane length |
| `L[5]` | cone parameter c, deg | half-cone = 0.5·c |
| `L[6]` | rad, m | radius offset of the cone apex behind O |
| `keep` | 0 → keeper excluded; ≠0 → keeper included | GF "exclude keeper" |
| `h` | horizon, frames | prediction: D = P + h·V |
| `_unused2` | dead (`0x140A53DF0` p1 chain; the p8 "shift origin" is a dead store, §3.5) | dead |

### 3.2 Output (C-asm)

- `out[0]` = slot with minimum d² among recorded mates (strict `<`, first wins), else 0xFF.
- `out[1]` = number recorded (never exceeds 11; the `<11` guard is unreachable).
- `d2` in each entry = |D − O|² in (x,z), where D is the **predicted** position.
- Wrapper result: `lane free ⇔ out[1] == 0` (`0x14063DC10`).

### 3.3 Algorithm (exact; float32)

```
out[0] = 0xFF; out[1] = 0
K = keeper(s)                       # §5.1, 0xFF if none
opp = (s != isAway(c))               # opponent path iff scanned side is not the reference side
for j in 0..10:
    q = 11*s + j
    if q >= 22: continue             # guard (only reachable for s >= 2)
    if q == c: continue
    if not active(q): continue       # MI+0x1748[q] == 0
    if not keep and q == K: continue
    P = pos(q); V = vel(q)           # player record
    D = P + h*V                      # xz and y componentwise
    if opp:                          # T1: cone on bearing from O to D
        b = wrap180(bearing(O, D) - θ)
        if |b| > max(0.5*c + 10, 90): continue        # strict >: pass when |b| <= thr
    # AT: annulus + angle from P2 = O - rad*dir(θ)
    P2 = O + rad * dir(wrap360(θ + 180))              # xz; y = O.y
    d2a = |D - P2|^2 (xz)
    B   = wrap180(bearing(P2, D) - θ)
    AT  = (rad^2 < d2a) and (d2a < (rad+len)^2) and (|B| < 0.5*c)   # all strict
    CT  = False
    if not AT:
        E  = O + len * dir(θ)                         # xz; y = O.y
        CT = properly_cross(O, E, P, D) and |cross2(E-O, D-P)| >= 1.1920929e-7
    if AT or CT:
        d2o = |D - O|^2 (xz)
        if out[1] < 11: append (q, d2o); out[1] += 1
        if out[0] == 0xFF or d2o < best_d2: out[0] = q; best_d2 = d2o
```

Definitions:
- `cross2(u,v) = u.x*v.z − u.z*v.x`.
- `properly_cross(A,B,C,D)` = `(cross2(C−A,B−A)·cross2(D−A,B−A) < 0) and (cross2(A−C,D−C)·cross2(B−C,D−C) < 0)`. This is what `0x140A900E0` tests (C-dec, product forms match its code).
- `bearing(A,B) = 0x140439EF0`: 0 if A.xz == B.xz, else atan2(dx,dz) in rad → +2π if negative → ×57.29578. Result in [0,360].

Check order matches asm (C-asm): T1 (opponent only) → AT → CT → record. The teammate branch skips T1 only.

### 3.4 Helper contracts used by the core

| Helper | Contract | Status |
|---|---|---|
| `0x140A62690(_, side)` | keeper slot for side: `side·11 + j`, else 0xFF | C-asm (args + return); see §5.1 |
| `0x140A62610(side, MI, T(side))` | first j in 0..10 with `MI+0x1748[11·side+j] != 0` and `T(side)+0x4C00[j] == 0`; else 0xFF | C-dec |
| `0x140A48A10(G, q)` | player record pointer for slot q | C-dec |
| `0x14043C410(x)` | isAway: `11 ≤ x ≤ 21` | C-asm |
| `0x140470D60(θp, out, add)` | out = wrap360(wrap360(θp) + add) | C-dec + constants |
| `0x140A8F7C0(vec, θp, r)` | vec.x += r·sin(θp); vec.z += r·cos(θp); vec.y unchanged | C-dec (+ C-asm of sine) |
| `0x140A92C80(θ, r)` | r·sinDeg(θ) via §1 table | C-asm |
| `0x140474C40(O, out, D)` | out = bearing(O, D) | C-dec |
| `0x140A53DF0(_, pred, P, θ, c, rOut, rIn)` | AT test (§3.3) | C-asm |
| `0x140A90D80(out, A, E, Q, D)` | returns `properly_cross(A,E,Q,D) and |den| ≥ eps`; out = intersection (unused by core) | C-asm |
| `0x140A900E0(A,E,Q,D)` | `properly_cross` | C-dec |
| `0x14043A0A0(a,b)` | wrap180(b − a) | C-dec + constants |

### 3.5 Dead inputs and dead stores (C-asm)

- `RDX` (`_unused`): read only as an argument to helpers that ignore it. Dead.
- `p8` ("shift origin back by rad along θ+180" block, asm 14063CE4D–CE87): writes `[RSP+0x40..0x48]`. Every later read of that slot follows a per-mate overwrite. No output depends on p8. Dead.
- `p8` load at entry is the only use of the flag.
- The pass-through arg after `h` (`_unused2`): not read.
- The count cap `<11` is unreachable (≤11 mates per side).

### 3.6 Verification

- Asm flow and every compare sense checked against the decompile (JBE/JA/JNC senses; `COMISS` operand order).
- Constants read from the install (§2).
- The teammate branch is reachable only if a caller passes `s == isAway(c)`. Both observed callers (`0x14063DC10` and `0x142128C10`) pass the opposite side. Port it for fidelity; no caller exercises it today (INF until caller sweep, O14).

---

## 4. Wrapper chain `0x14063DE20` → `0x14063DC10`

`0x14063DE20` (C-asm, 0x68 frame):
- Builds the struct `[x, y, z, θ, len, cone, rad]` at `RSP+0x40..0x58`:
  - `[0..2]` = `*p3` (vec3 origin)
  - `[3]` = `*p4` (θ, deg)
  - `[4] = p5` (len), `[5] = p6` (cone), `[6] = p7` (rad)
- Calls `0x14063DC10(p1, p2, &struct, p8, p9, p10, p11)`.

`0x14063DC10` (C-asm):
- `c = p2`. `isAway(c)` → `R10`. `s_scan = (isAway(c) ^ 1)` (opponent side). Only when `R10 < 2`, which is always.
- Calls `0x14063CD90(out, p1, c, s_scan, &struct, p8, p9, p10)`.
- Returns `out[1] == 0`.

| Caller arg | Feeds | Use |
|---|---|---|
| p1 | core `_unused` | dead |
| p2 | core `c` | reference slot |
| p3 | struct origin + θ + len/cone/rad | lane geometry |
| p8 (byte) | core `keep` | keeper inclusion |
| p9 (uint) | core `h` | horizon frames |
| p10, p11 | core dead param, dead `local` | dead |

---

## 5. The five undecoded helpers (brief item 2)

### 5.1 `0x140A62690` — side keeper slot
- Body (C-asm): `0x140A48A30(G, side)` → side object; `0x140A62610(side, MI, obj)` → j; returns `side·11 + j` if j ≠ 0xFF, else 0xFF.
- `0x140A62610` returns the first active slot j whose `obj+0x4C00[j] == 0`. The slot is the side's keeper iff `position category 0 = keeper`. Docs (`pes-re-defensive-shape.md` §474) mark this INF(med). **Confirmed as a selector; the category→keeper mapping is OPEN (O1).** _Note 19-10-26 (pes-re-community-qa.md §6 C8): auditor and class map agree that array-B 0 = GK family (dec FUN_140A62690/FUN_140A62610 + FUN_140A59F90 switch); keep OPEN until the live writer is checked — role-writer §6 closes O1 on static instead, see its note below._
- Verdict: **the brief's guess is right in shape**: it is the side's keeper index under the category-0 rule. _Corrected 19-10-26 from pes-re-role-writer.md §6: CLOSE O1 — class 0 = GK by the dumped map (role-writer §5); selector returns first active B==0; post-swap caveat (B==0 sits at the swapped-in slot)._

### 5.2 `0x140A53DF0` — annulus-and-angle test (AT)
- C-asm (full, 0x140A53DF0). Args: `(_, pred, P, θ, c, rOut, rIn)`.
- Pass iff `rIn² < d2 < rOut²` and `|wrap180(bearing(P→pred) − θ)| < 0.5·c`, all strict.
- `d2 = |pred − P|²` (xz). `bearing` via `0x140439EF0`, `wrap180` via `0x14043A0A0`.
- Verdict: **decoded, C-asm.**

### 5.3 `0x140A90D80` — segment crossing (CT)
- C-asm. Args `(out, A, E, Q, D)`. Returns 1 iff `properly_cross(A,E,Q,D)` (via `0x140A900E0`) **and** `|cross2(E−A, D−Q)| ≥ 1.1920929e-7`.
- Sets `out = A + s·(E−A)`, with `s = cross2(Q−A, D−Q) / cross2(E−A, D−Q)`. The core discards `out`.
- Verdict: **decoded, C-asm.**

### 5.4 `0x140470D60` — heading add with wrap
- `out = wrap360(wrap360(*θp) + add)`.
- Core uses `add = 180` (reverse heading) and `add = 0` (none).
- Verdict: **decoded, C-dec (constants read).**

### 5.5 `0x14043C410` — isAway
- Returns `11 ≤ x ≤ 21`. Decompile reads `10 < x && x < 22`.
- Verdict: **decoded, C-asm.**

---

## 6. One-touch predicate `0x142128C10` (brief item 3, first half)

Called from `0x14212AEB0` (OneTwo) and the DirectHighBall gate (docs). Returns bool.

### 6.1 Inputs (C-asm)

| Name | Source | Fields used |
|---|---|---|
| `R` | `*(arg0)` | `R[0]` slot s, `R[2]` side (= isAway(s)), `R[5]` attack sign k (float ±1/0) |
| `X1` | `arg[1]` | `T18 = X1+0x18` (table), `E = X1+0xE0` (event table pointer) |
| `X4` | `arg[4]` | `byte +0x6C` flag |
| `X5` | `arg[5]` | `+0x9CC` IsClose struct; `+0x2B0` curve table |
| `M` | `arg[6]` | `*(M+8)` team mode |
| `X7` | `arg[7]` | `int +0xC4` kind; `float +0xC0` pass direction (deg) |
| `F` | `*(G+0x2AA8)` | `float +0x18` bound |

Player of s: `P_s = player(s)`. Team of side: `T(R[2])`.

### 6.2 Gates (in order; any failure → false). All compares as in asm.

| # | Gate | asm-verified form |
|---|---|---|
| G1 | `k·x ≥ k·T18[(sd+3)·0x14]` (float at entry) | `COMISS` line→x; pass iff `k·line ≤ k·x` |
| G2 | `k·x ≤ F[+0x18] + 2.0` | pass iff `k·x ≤ bound` |
| G3 | `level(T(sd)) ≥ 2`, `level = (+0xA29C ? 0 : +0xA294)`; pass iff `level/100 ≥ 2` | `0x140A40BD0`, `0x140A60360` |
| G4 | `mode ∈ {3,4}` **or** `T(sd)+0x4C00[s%11] ≥ 4` (for s < 22) | `0x141052E30` |
| G5 | `score = 0x140587E70(T18, k, s) ≥ 1600` | see §10 O5 |
| G6 | `¬IsClose(X5+0x9CC)` | `0x140699820` |
| G7 | not `(X4.+0x6C == 0 ∧ E.type == 1 ∧ E.side == sd ∧ slot(E.id) ≤ 21 ∧ action(slot(E.id)) == 0x25)` | `0x140A3E190(E,0)` = `E+0x180698`: `int type @0`, `side @4`, `roster id @8` (C-asm read offsets); `0x140A62EA0` |
| G8 | `runCount < cap` | see below |
| G9 | `kind ∈ {1,2,3,5,6}` | asm: `≠4`, `>0`, `<7` |
| G10 | `¬(speed(P_s) ≥ 15 ∧ markCount(s) > 3 ∧ |wrap180(heading(P_s) − X7.+0xC0)| > 100)` | `0x140477130`, `0x140A58320`, `0x140458740`, `0x140481B00` |
| G11 | `lane(s) count ≠ 0`, lane = core(origin=P_s current, θ=X7.+0xC0, len=15, cone=60, rad=2, keep=0, h=0, s_scan = (sd==0)) | `0x14063CD90` with the struct built at `RSP+0x58` |
| G12 | `−10 ≤ k·(x − curve(z)) < 0`, `curve = 0x142139650(X5+0x2B0, z)`, x,z = P_s | asm: `SETBE` after `JC`; strict `< 0` |

**G8 detail (C-asm):** `runCount = #{j∈0..10 : 11·sd+j ≠ s ∧ T(sd)+0xA2A0[j] == 0x25}`.
`base = 2 if level/100 ∈ {5,6}; 1 if level/100 == 4; else 0`. `inc = 1 if (T(sd)+0xB8A4 ≥ 3) or mode ∈ {2,3}`. `cap = min(2, base + inc)`. Pass iff `runCount < cap`.

**G10 detail:** speed and heading both use `vel_eff(P, r)` with `r = 0.6944/fps` (see 6.3).

**G11 lane struct (C-asm, `RSP+0x58..0x70`):** `[0..2] = P_s (x,y,z)`, `[3] = X7.+0xC0`, `[4] = 15.0`, `[5] = 60.0`, `[6] = 2.0`. Out at `RSP+0x90` (best = 0xFF, count = 0).

### 6.3 Helper contracts for §6

- `vel_eff(P, r)` (`0x140A6C6D0`): returns `velocity (x,y,z)` = `(+0x574, +0x578, +0x57C)` unless `state == 1` **or** (`state == 5` and `(vx²+vz²) < r²`), in which case returns `(r·sinθ, 0, r·cosθ)` with `θ = facing +0x554`. C-dec.
- `speed(P)` (`0x140477130`) = `fps·3.6·|vel_eff(P, 0.6944/fps)|` in km/h. C-dec.
- `heading(P)` (`0x140458740`) = bearing of `vel_eff(P, r)` in deg; 0 when the vector is zero. C-dec (the atan2 call args are dropped; INF that it is atan2(x,z) as in `0x140439EF0`).
- `markCount(live)` (`0x140A58320`) = `+0x12A4 < 5 ? +0x12A4 : 0`. C-dec + docs.
- `wrapDiff(p, θ) = |wrap180(θ − *p)|` (`0x140481B00`). C-dec.
- `IsClose(q)` (`0x140699820`): 1 iff `q+0x170 ≥ 0` (float) ∧ (`q+0x16C & 1` ∨ (`q+0x17C < 3` ∧ `q+0x178 < 3`)) ∧ `q+0x17C < 1`. C-dec. Name from docs.
- `curve(C, z)` (`0x142139650`): `u = clamp(z·C[+0x5E4] + 35, 0, 70)`; `i = floor(u)`; if `i == 70` → `C[+0x34C]`; else linear interpolation of `C[+4+12i]`, `C[+4+12(i+1)]` by `u−i`. C-dec. Table contents: runtime (OPEN O6).
- `score(T18, k, s)` (`0x140587E70`): `base = 0x140A714C0(T18, (char)k)` = `T18+0x228EC` if `k==1` else `T18+0x22AD4`; entry index = `rank[s] + 11·(s≥11)` where `rank = base[0xB2+s]`; returns `float` at `base+4+8·idx`; 0 if s > 21. C-asm (base selection) + C-dec. Layout = NearestEntry (docs `pes-port-NearestList.md`): `{int id; float d2}` × 11 per side, rank byte at `+0xB2`.
- `E.side` and `E.id` at `E+0x180698+4` and `+8`. Slot of event player: `0x140A62EA0(X1, {side, id})` = `side·11 + k` for roster number `id` in `R(side)`, else 0xFF (`0x140A3B320`, C-dec).

### 6.4 Output

Bool. True iff all of G1–G12 pass.

Meaning (INF from the gates): the receiver is in the attacking band, is not being "played" (one-two run), the forward lane is occupied by at least one opponent, and his lateral position relative to the attack curve is 0–10 m behind it. In prose: the receiver can one-touch because the next lane is blocked.

---

## 7. Snapshot `0x14063C9F0` (filled via `0x14063C560`)

`0x14063C560` zeroes the record header, calls `0x14063A370` and `0x140DD39D0` (init, not decoded), then `0x14063C9F0`. Fill C-asm (store offsets), C-dec (logic):

| Dword | Field | Source |
|---|---|---|
| [0] | slot x | arg |
| [1] | x % 11 (any x) | arithmetic |
| [2] | side = isAway(x) | `0x14043C410`-equivalent |
| [3] | roster number `R(side)[4+4·k]` | `0x140A3AE30` |
| [4] | `wrap360(T(side)+0x250)` float bits | asm (`MOVSS`) |
| [5] | `float(int8(T(side)+0x254))` bits | asm (`CVTDQ2PS`) |
| [6..8] | P.x, P.y, P.z | player `+0x55C..+0x564` |
| [9..11] | (k·P.x, 0, k·P.z) | decompile |
| [0xC] | facing `+0x554` (may be adjusted) | decompile; adjustment branch not decoded (INF) |
| [0xD] | `T(side)+0x4BD4[k]` (k < 11, else [0]) | decompile; **read by 0x142124950** |
| [0xE..0x10] | `0x140A62180(X1, side, j)` at j = 0, 1, 2 | decompile; **[0xF] read by 0x142124950** (carrier only) |
| [0x11] | `*0x140A40BD0(T(side))` | decompile |
| [0x12] | byte `live+0x34` | decompile |
| [0x13..0x15] | three values from `0x1408C75C0` | decompile, unused by predicates |
| [0x16] | bool from event table | decompile, unused by predicates |

Snapshot fields read by the predicates:
- One-touch: `[0]`, `[2]`, `[5]`. Position is taken from the player record directly, not `[6..8]`.
- OneTwo receiver predicate: receiver `[0xD]`; carrier `[0]`, `[4]`, `[0xF]`.

Port note: the predicate can be specified against these named fields and does not need the rest of the writer.

---

## 8. OneTwo receiver predicate `0x142124950` (brief item 3, second half)

Returns bool. Inputs: `P[0]` carrier snapshot `C`, `P[1]` = `X1`, `P[4]` = `X4`, `P[5]` = `X5`, `P[6]` = `M`. Receiver slot r. Asm-verified control flow; decompile agrees.

Define `c = C[0]`, `k = r % 11` (r<22).

Pre-gates:
- P0. `r ≤ 21` and `r ≠ c`.
- P1. `status(X5+0xB6C, k) == 11` (valid). `status = int at +0x3C + 4k` (`0x142118D20`; k ≥ 11 → 1).
- P2. Record `A = X5+0xB6C+0x68+0xF8·k` (`0x142118C90`). Let `ψ = snapshot(r)[0xD] ≠ 0`.
- P3. `player(r)+0x5D0 ≠ 0x25`.

Band 1 (difficulty by position):
- If `dist ≥ 15` or `θfac ≥ 110` (with `dist = A+0xEC`, `θfac = A+0xE8`): require `A+0xF0 ≤ 1.0`.
- Else: require `A+0xF0 ≤ 2.0`.

Angle block (runs iff `X4+0x6C ≠ 0` **or** `mode ∉ {3,4}` **or** `C[0xF] ≠ 1` **or** `NL(c,r) ≥ 36`):
- `NL(c,r) = 0x1404C8EA0(T18, c, r)` = d² in carrier c's NearestEntry (base `T18+0x22E4C+200c`, rank byte `+0xB2+r`, entry idx `rank + 11·(r≥11)`).
- `thr = (speed(player(c)) > 15) ? 80 : 120`.
- Require `wrapDiff(X4+0x38, A_tab[c][r]) ≤ thr`, where `A_tab[c][r] = int at T18+0x22F7C + 0x58·c + 4r` (c<22; else row base) (`0x140439FE0`-style indexing; name INF).

Window:
- `base = (A+0xE4 < 120) ? 10 : 5`.
- `ramp = 1` if `θfac ≤ 60`; `(130 − θfac)/70` if `60 < θfac < 130`; `0` if `θfac ≥ 130`.
- `u = base + 10·ramp`. Require `3 ≤ dist ≤ max(u, 3)`.

Lane-angle tests:
- `m1 = M[r][c]` where `M[a][b] = float at T18+0x23F7C + 0x58·a + 4b`. Require `wrapDiff(player(r)+0x554, m1) < 120` (strict).
- `m2 = M[c][r]`. Require `wrapDiff(C[4], m2) > 20` (strict) **and** `A+0x00 > 2`.

Output: 1 iff all pass.

Verification: asm (`0x142124950`, 191 lines) checked for every compare sense and both block branches. The `ψ` test uses the snapshot `[0xD]` at asm `RSP+0x54`, i.e. `snapshot+0x34`.

Notes:
- `dist` is `A+0xEC`, ball→target 2-D distance (writer uses `0x140A8F860`; INF on the helper's exact metric).
- `θfac` is `A+0xE8`, angle from target-pass to `X4+0x38`.
- `C[0xF]` is written from `0x140A62180` (returns a byte in AL) and stored as a dword; only the value 1 is tested. Upper-byte state is unverified (O11).

---

## 9. b6c record A (`X5+0xB6C+0x68+0xF8·k`), field meanings

Writer: `0x1421174F0` (`~/ghidra-projects/scratch/CarrierAIRE/d_1421174f0.c`, read this session, C-dec with the asm-verified store types above). Dword indices:

| Dword | Byte | Field | Derivation (writer) | Read by |
|---|---|---|---|---|
| [0] | +0x00 | openness class 0..5 | from `[0x3D]`: `<10`→0; `<40`→1; `<60`→2; `<100`→3; `≥100` → 4, or 5 if `≥130`; forced 5 if the flag `local_128` survives the opponent scan | predicate (`>2`) |
| [1] | +0x04 | direction class | `|wrap180(pass − attack)| ≤ 60` → 0; `≤110` → 2; else 1 | — |
| [2] | +0x08 | turn class | `|wrap180(X4+0x38 − pass)| ≤ 60` → 3; `≤110` → 4; else 5 | — |
| [3..5] | +0x0C | target (x,y,z) | copied from mate's predicted position | — |
| [6..] | +0x18 | opponents by side: `(slot, d)` pairs, counts at +0xC8/+0xC9; sorted by `0x140A79640` | `0x142118700` | — |
| [0x38] | +0xE0 | pass angle, deg | `bearing(ball → target)` (`0x140474C40`) | — |
| [0x39] | +0xE4 | angle to attack, deg | `|wrap180(pass − T(carrier side)+0x250)|` (writer's `param_3[2]` side) | predicate (base 10/5 choice) |
| [0x3A] | +0xE8 | angle to carrier facing, deg | `|wrap180(X4+0x38 − pass)|` | predicate (ramp, band 1) |
| [0x3B] | +0xEC | ball→target distance, m | `0x140A8F860(ball, target)` (metric INF) | predicate (window) |
| [0x3C] | +0xF0 | difficulty (float bits) | ramp terms on +0xE8 and +0xE4, carrier/speed terms, then ×(fVar19·DAT_142590D98 + DAT_14257A194) | predicate (band 1) |
| [0x3D] | +0xF4 | openness score (float bits) | init 150.0; reduced by the opponent scan `0x142118700` (not decoded) | writer only (→[0]) |

Status table (not part of the record): `X5+0xB6C+0x3C+4k`. Value 11 = valid. Other values documented in `pes-re-carrier-ai.md` §4.3 (not re-verified).

Writer constants read this session: 60, 110, 130, 40, 100, 2, 10, 80, 120, 1, 90, 15, 20, 36, 3, 0x14259BE68/0x1425C48FC/0x14259BE34/0x142591140/0x14259CE38/0x14259BDDC/0x142598958/0x142590D98/0x14257A194 **not read** (writer port only).

---

## 10. Open items

- **O1.** Keeper meaning: CLOSED — `T(side)+0x4C00[j] == 0` is the keeper (class 0 = GK by the dumped writer map; live `T1+0x4C00[0]=0` matches). Post-swap caveat: B==0 sits at the swapped-in slot. _Corrected 19-10-26 from pes-re-role-writer.md §6: old "mapping INF(med), live check needed"._
- **O2.** Velocity units: C-dec says per-frame (speed uses fps·3.6). Live sample not taken; docs still list it open.
- **O3.** `atan2` core `0x140A92DC0` and sqrt import `0x1415A1780` (jump via `_DAT_1425301F0`) not decoded. A bit-exact port needs the PES atan polynomial. `std::atan2` is an approximation (must be labeled as such).
- **O4.** `0x140458740` atan2 argument mapping (dropped by Ghidra): INF.
- **O5.** `0x140587E70` score: NearestEntry d² list selected by sign `k` (`T18+0x228EC` vs `+0x22AD4`); owner of the list and the 1600 threshold semantics OPEN. Records by layout (docs `pes-port-NearestList.md`).
- **O6.** Curve table `X5+0x2B0` contents and builder (runtime) OPEN; scale `+0x5E4`, end `+0x34C`.
- **O7.** Builder of the carrier snapshot `C` passed as `P[0]` to `0x142124950`: caller not decoded. Only `C[0]`, `C[4]`, `C[0xF]` matter.
- **O8.** Event record `E`: `type @0`, `side @4`, `id @8` from asm offsets. Name and writer INF.
- **O9.** `T18+0x22F7C` (`A_tab`) and `T18+0x23F7C` (`M`) tables: layout confirmed (22×0x58 rows); writer and semantics OPEN (docs: "analysis angle table").
- **O10.** `0x140A6C6D0` and `0x140458740` are C-dec only. Confirm by asm before porting speed/heading.
- **O11.** `C[0xF]` / `0x140A62180`: byte-return helper; upper-byte state of the stored dword unverified. Table `0x8E08` via `0x1408C5030` not decoded.
- **O12.** `0x140A40BD0` level writers (`+0xA294`, `+0xA29C`) and `T(side)+0x4BD4` semantics not decoded (docs have level = `/100`).
- **O13.** `0x140699820` (IsClose): decompile contract only; name from docs; caller argument traced through asm (`X5+0x9CC`).
- **O14.** Caller sweep (BackSpace `0x142129110`, GoalFront `0x14212ABA0`, Safety `0x14212B520`, OneTwo `0x14212AEB0`, DHB `0x142129BE0`): their lane-struct arguments (origin, θ, len, cone, rad, keep, horizon) are not yet mapped. The Ghidra `get_xrefs_to` bridge route returned 404 from curl; use the mounted MCP tool. The teammate branch is unexercised until then.
- **O15.** Receiver predicate `A_tab` gate (`|wrapDiff(X4+0x38, A_tab[c][r])| ≤ thr`) and the `M` tests use names from docs only. Semantics of `A_tab` OPEN.
- **O16.** The pass-into-lane band in the one-touch (`−10 ≤ k(x−curve) < 0`) depends on O6.
- **O17.** The receiver predicate's band 1 and window constants are asm-verified; the difficulty writer (`0x1421174F0`) is only C-dec for the openness path and is not needed for the predicate.

---

## 11. Port order recommendation (for the next dispatch)

1. Lane core `0x14063CD90` as a pure function on (positions, velocities, active flags, keeper slot), with the install constants (§2) and the sine table read from the install. Tests: synthetic opponents on the cone boundary (`|b| = thr`), annulus radii, parallel segments (`|den| < eps`), and the teammate branch.
2. Wrapper `0x14063DC10` = `count == 0`. Test with explicit opponent-side flip.
3. One-touch `0x142128C10` after O5/O6/O10 are closed. Until then, mark the score and curve inputs "stand-in" with `// ponytail:` naming O5/O6.
4. OneTwo receiver predicate `0x142124950` after O9 and the snapshot fields §7 are provided by the carrier builder (O7).
5. b6c writer `0x1421174F0` is not part of this port; the predicate reads its stored fields.

No code was written in this session. No test was run. Nothing was committed.
