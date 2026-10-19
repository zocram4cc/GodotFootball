# PES lane units 2: OneTwo `0x14212AEB0` and DirectHighBall `0x142129BE0` (decode + port specs)

Scope: decode-only. No engine edits, no live PES, no commits. Image `pes_full.bin` (base `0x140000000`).
Scratch: `<workdrive>/gf-LaneUnits2-scratch/` (`dec_0x14212AEB0.c`, `dec_0x142128C10.c`, `dec_0x142124950.c`,
`dec_0x140456ea0.c`, `dhb_0x142129BE0.txt` (fresh headless decompile, `dec.sh`, read-only), `asm_dhb_full.txt`,
`asm_14212AEB0.txt`, `asm_pred_head.txt`, `asm_pred_lane.txt`, `asm_target.txt`). Helper decompiles were read from
the saved sets `gf-A4Cont-scratch/`, `gf-LaneCore-scratch/`, `gf-CarrierHelpers-scratch/`, `gf-A4Lanes-scratch/`
and re-checked where marked.

Legend: **CONFIRMED** = decompile and asm agree (asm VA cited). **C-dec** = decompile only. **INF** = inference, named.
**OPEN** = not decoded; the blocker is named. All VAs are image VAs. Angles are degrees. `d2` = squared xz distance.
Velocities are per PES frame. `fps` = 54.

Bridge state this session: `decompile_function` resolves `0x14212AEB0`, `0x142128C10`, `0x142124950`, `0x142126A80`,
`0x142129590`. It returns "No function found" at `0x142129BE0` (DHB is an interior entry). DHB was decompiled with
`dec.sh` (PESLive, read-only) and checked against raw asm (`objdump`) block by block.

---

## 0. Float table (mechanical conversion, `struct.unpack('<f')` on the image bytes at VA-0x140000000)

Every float used by either unit or by their one-touch / receiver / closing callees. Immediates are listed as well.

| VA (or immediate) | bits | value | used by |
|---|---|---|---|
| `0x142590D98` | `0x3E99999A` | 0.30000001192092896 | OneTwo: frames = int(fps·0.3+0.5) = 16 (asm `0x14212AECA`) |
| `0x142565600` | `0x3F000000` | 0.5 | frames `+0.5` (OneTwo, DHB 27-frame pred); core cone scale |
| `0x142598580` | `0x43340000` | 180.0 | OneTwo fold, closing sweep `param_4` |
| `0x14259BDEC` | `0x40C00000` | 6.0 | OneTwo closing outer radius (asm `0x14212AEEE`) |
| `0x142588218` | `0xBF800000` | -1.0 | OneTwo fold sign; DHB initial best-d² sentinel (`0x142129DC9`) |
| `0x14256AE1C` | `0x3F800000` | 1.0 | OneTwo fold sign (+); receiver predicate |
| `0x14259858C` | `0x43B40000` | 360.0 | wrap360 folds (DHB θ, 4C4E0) |
| `0x14259BE5C` | `0x42B40000` | 90.0 | OneTwo `|d| <= 90` gate; closing threshold (`wrap360(180)/2`) |
| `0x142598598` | `0xC3B40000` | -360.0 | DHB θ fold |
| `0x14259A760` | `0x420C0000` | 35.0 | curve index offset (one-touch G12) |
| `0x14259BE4C` | `0x428C0000` | 70.0 | curve index max (one-touch G12); mate-speed gate |
| `0x14259BE84` | `0xC1200000` | -10.0 | one-touch G12 band floor |
| `0x14259CE40` | `0x44C80000` | 1600.0 | one-touch G5 score floor (d², 40 m) |
| `0x14259BDD0` | `0x3F31C71C` | 0.6944444179534912 | speed floor (`0.6944/fps` = per-frame length) |
| `0x14259BE0C` | `0x41700000` | 15.0 | speed gate (G10, DHB); DHB lane len |
| `0x142565604` | `0x40000000` | 2.0 | closing divisor (`180/2`); one-touch band |
| `0x14259A764` | `0x42700000` | 60.0 | DHB lane cone; one-touch G11 cone (`DAT_14259a764`) |
| `0x14259222C` | `0x40A00000` | 5.0 | DHB lane radius |
| `0x14259D60C` | `0x41100000` | 9.0 | DHB: mate 9-frame pred must be `>= 9` (d²) |
| `0x14259BE70` | `0x43610000` | 225.0 | DHB score floor (d², 15 m) |
| `0x1425C65F4` | `0x44610000` | 900.0 | DHB score ceiling for branch A (d², 30 m) |
| `0x14253EE30` | `0x42C80000` | 100.0 | DHB branch-A skip (d² < 100); byte/100 divisor |
| `0x1425656C0` | `0x40800000` | 4.0 | DHB origin advance (m along θ) |
| `0x14259BE20` | `0x41C80000` | 25.0 | DHB nearest-opponent gate (d², **5 m**) |
| `0x142588200` | `0x3DCCCCCD` | 0.10000000149011612 | DHB height threshold `+0.1` |
| `0x142596E6C` | `0x42A00000` | 80.0 | DHB `|X| <= 80` gate |
| `0x14259BE1C` | `0x41A00000` | 20.0 | receiver predicate `wrapDiff > 20`; helper |
| `0x14258C264` | `0x40400000` | 3.0 | receiver predicate window floor |
| `0x14256AE30` | `0x42F00000` | 120.0 | receiver predicate angle thr (no-speed) |
| `0x14259D628` | `0x42DC0000` | 110.0 | receiver predicate facing threshold |
| `0x1425A9BF0` | `0x43020000` | 130.0 | receiver predicate ramp end |
| `0x1425C4900` | `0xC28C0000` | -70.0 | receiver predicate ramp divisor |
| `0x142588210` | `0x41200000` | 10.0 | receiver predicate base; core cone offset |
| `0x1425A9464` | `0x42100000` | 36.0 | receiver predicate NearestList gate (d², 6 m) |
| `0x142565D30` | qword `0x7FFFFFFFFFFFFFFF` | (abs mask) | `|x|` via ANDPS/ANDPD (OneTwo `0x14212AF1D`) |
| `0x14351AE50` | int `0x36` | 54 | fps int (via `0x141E532E0`, asm `mov eax,[0x14351AE50]`) |
| `0x14351AE54` | `0x42580000` | 54.0 | fps float (via `0x141E532D0`) |
| imm `0x41700000` | | 15.0 | one-touch G11 lane `len` (asm `0x142129025`) |
| imm `0x40000000` | | 2.0 | one-touch G11 lane `rad` (asm `0x14212902D`) |
| imm `0x000000FF` (`3.57331e-43` in dec) | | 0xFF (int sentinel, not a float) | DHB `best = 0xFF` (asm `0x142129DC9`, `MOV R12D,0xFF`) |

Table constants (not floats): `0x1426484F0` = u32[94] bit table for `0x140A6E5E0` (bit 5 = first-time-while-receiving); read from the install.

---

## 1. Verdict

1. **OneTwo** is fully decoded. Its gate is: closing sweep count (`0x140A4E720` → `0x140A4E3F0`) OR `|wrap180(ball_angle − C[4])| <= 90`; then the one-touch predicate `0x142128C10`; then the target picker `0x142126A80`. Answer: `(type 2, target, b=0, f10=1)`. The one-touch's lane is the only ScanLane call inside OneTwo, and it sits inside the predicate.
2. **DirectHighBall** is fully decoded. Its gate is a low-ball contest test, then a teammate scan over the carrier's own NearestList row with score, branch-A and 9/27-frame tests, then an answer that picks `b=3` (free teammate lane) or `b=0`, with f10 when `|X| <= 80`.
3. **Both units depend on three PES team tables** whose writers are not in this image: the score row, the target-list row and the per-slot angle table (the one-touch G12 curve table is a fourth). The layout of all four is now mapped (§2.5). Stand-ins are required; they are listed in §7 with the PES site that will remove each one.
4. The teammate branch of the lane core is **live**: DHB calls it (§4, asm `0x142129C4A`–`0x142129C58` and `0x14212A21F`). The lane-core doc claim that every caller uses the opposite side is wrong (§8).
5. The "receiver" in the lane-structs doc for OneTwo's `R` is the **carrier snapshot C** (`ctx[0]`). Its `s` and `c` are both the carrier's (§8, C1–C3).

---

## 2. Shared callees (decoded here)

### 2.1 Closing query `0x140A4E720` → sweep `0x140A4E3F0` (CONFIRMED, asm `0x140A4E720`–`0x140A4E7AE`)

- `0x140A4E720(T, slot, A_raw float, 180.0, 6.0, frames byte, flag byte, inner float)`. Args: `RCX=T` (ctx[1]), `EDX=slot` (C[0]), `XMM2=C[4]`, `XMM3=180.0`, `[RSP+0x20]=6.0`, `[RSP+0x28]=frames`, `[RSP+0x30]=1`, `[RSP+0x38]=0.0`.
- Normalises `A_raw` and `180.0` into `[0,360)` (asm `0x140A4E724`–`0x140A4E769`, two fold blocks, the same wrap as `0x140A4C4E0`).
- Stores the stack args, then `CALL 0x140A4E3F0` at `0x140A4E7A9`, then `ADD RSP; RET`. **AL passes through unchanged**, so the caller's `cVar1` is the sweep's count (CONFIRMED; Ghidra showed `void` for the wrapper).
- `0x140A4E3F0(T, ref, &A, &half, outer, frames, flag, inner)`. CONFIRMED arg order from asm (`0x140A4E3FE` `MOV R13,R9`, `0x140A4E441` `CALL 0x140A48A10` with EDX still = ref).
  - `ref > 21` → return 0 (`0x140A4E401`).
  - `side(ref) = (ref >= 11)`; the scanned side is `opp = (side(ref) == 0)` via `0x140456EA0` = `*p == 0` (C-dec and asm `setge`). So **it counts opponents of the reference**.
  - Reference predicted position: `pr = pos(ref) + frames·vel(ref)` (xz).
  - For each opponent `q` in `opp`'s block (j = 0..10, `q = 11·opp + j`, skip `q` ≥ 22):
    - `pq = pos(q) + frames·vel(q)`; `d2 = d2(pr, pq)`.
    - Keep iff `inner² < d2 < outer²` (strict, C-dec, `0x140A4E3F0` float compares).
    - `br = bearing(pr → pq)` in deg `[0,360]` (`atan2(dx, dz)` via `0x140A92D40`, ×180/π; O3 applies: `std::atan2` approximation).
    - Count iff `|wrap180(br − A)| <= 90` (`0x140A4C4E0` on `half=180`, divisor `2.0` → 90; `0x140481B00` = `|wrap180(θ − *p)|`; the `<=` is `fVar13 < thr || fVar13 == thr`).
  - Returns `count` (byte).
- For OneTwo: `T`=ctx[1], `ref`=C[0], `A=C[4]`, `frames=(int)(fps·0.3+0.5)=16`, `outer=6.0`, `inner=0.0`, `flag=1` (flag 0 would enter an extra per-side active check that OneTwo does not hit).

### 2.2 One-touch predicate `0x142128C10` (C-asm for the gates I re-read; C-dec for the rest)

Inputs: `C` = carrier snapshot = ctx[0] (C[0] slot, C[2] side, C[4] attack angle, C[5] attack sign float, C[0xF] flag); `ctx[1]`=TC (team ctx, `T18 = TC+0x18`), `ctx[4]`=carrier kinematics K (`+0x34` angle, `+0x40/+0x44/+0x48` ball point, `+0x6C`, `+0x6E`), `ctx[5]`=IA (`+0x9CC` zone, `+0x2B0` curve table, `+0x5E4` scale, `+0x34C` end), `ctx[6]`=M (`+8` mode), `ctx[7]`=plan (`+0xC0` heading, `+0xC4` mode).
`s = C[0]`, `sd = C[2]`, `k = C[5]` (int). `P = player(s)` (`0x140A48A10`).

Gates in order (C-asm verified for G1–G4 and G11/G12 senses; G5–G10 from the saved decompile, asm-checked in the lane-core session):

| # | Test | Callee | Verdict |
|---|---|---|---|
| G1 | `k·T18_line(sd) <= k·P.x`, `T18_line(sd) = float at T18+(sd+3)·0x14` | asm `0x142128CB8`–`0x142128CC3`: `LEA RCX,[R15+3]; LEA RCX,[RCX+RCX*4]; MULSS XMM0,[R13+RCX*4]` = byte offset `(sd+3)·0x14` | CONFIRMED (asm `0x142128CB8`–`0x142128CD3`); **no +0x40** |
| G2 | `k·P.x <= F+0x18 + 2.0` | `F = *(G+0x2AA8)` | CONFIRMED (asm `0x142128CDC`–`0x142128CED`) |
| G3 | `level(T(sd)) >= 2`, `level = (T.+0xA29C ? 0 : T.+0xA294)` | `0x140A40BD0`, `0x140A60360` (`level/100 < 2` reject) | CONFIRMED (asm `0x142128CF7`–`0x142128D0F`) |
| G4 | `mode ∈ {3,4}` or `T(sd).+0x4C00[s%11] >= 4` (s<22) | `0x141052E30` | CONFIRMED (asm `0x142128D11`–`0x142128D2B`) |
| G5 | `score(T18, k, s) >= 1600` | `0x140587E70` (§2.4) | C-dec; threshold `0x14259CE40` = 1600 CONFIRMED const |
| G6 | `!IsClose(IA+0x9CC)` | `0x140699820` = GF `IsClose(ShootZone)` | C-dec; GF HAS |
| G7 | `!(K.+0x6C == 0 && E.type == 1 && E.side == sd && slot(E.id) <= 21 && action(slot(E.id)) == 0x25)` | `0x140A3E190` (`E+0x180698`), `0x140A62EA0` | C-dec (lane-core §6.2) |
| G8 | `runCount < cap` (§2.4a) | `0x140A602E0`, `0x141052E30`, `0x140A40BD0` | C-dec |
| G9 | `plan.mode ∈ {1,2,3,5,6}` | `ctx7.+0xC4` | C-dec |
| G10 | `!(speed(P) >= 15 && markCount(P) > 3 && |wrap180(heading(P) − ctx7.+0xC0)| > 100)` | `0x140477130`, `0x140A58320`, `0x140458740`, `0x140481B00` | CONFIRMED structure (decompile lines 137–147) |
| G11 | `count != 0` of `ScanLane(opp=!sd, ref=s, origin=P.pos, θ=ctx7.+0xC0, len=15 (imm 0x41700000), cone=60 (0x14259A764), rad=2.0 (imm 0x40000000), keep=0, h=0)` | `0x14063CD90` (`0x142129076`, args CONFIRMED asm `0x142129025`–`0x142129076`) | CONFIRMED (asm) |
| G12 | `-10 <= k·(P.x − curve(P.z)) < 0`, `curve = 0x142139650(IA+0x2B0, P.z)` | §2.4b | CONFIRMED sign/flow (`0x142129099`–`0x1421290C5`); curve table OPEN |

Result = G1∧…∧G12. A `count == 0` after G11 returns false (asm: `JZ` to the false exit).

### 2.3 OneTwo target picker `0x142126A80` (CONFIRMED, asm `asm_target.txt`, C-dec)

```
OneTwoTarget(ctx):                       # ctx[0] = C
  c = C[0]; side = (11 <= c <= 21)       # uVar10 = IsAway(c)
  L = 0x140A714E0(T18, k)                # T18+0x22B9C (k==1) else T18+0x229B4   (row, see §2.5)
  n = byte(L + 0xB0 + side)              # 0x142124F90
  for j in 0..n-1:
      r = L.team[side][j].id             # entry at L + (side*11 + j)*8
      if Receiver(ctx, r): return r      # 0x142124950
  return 0xFF
```

### 2.4 Score `0x140587E70`, curve `0x142139650` (CONFIRMED structure, writers OPEN)

**2.4(a) Score.** `0x140587E70(T18, k, s)`: `base = 0x140A714C0(T18, k)` = `T18+0x228EC` (k==1) else `T18+0x22AD4`; `if s > 21 return 0`; `idx = rank[s] + 11·(s>=11)`, `rank[s] = byte at base+0xB2+s`; return float at `base + 4 + 8·idx` (C-dec, `dec_140587E70.c`; `0x140A714C0` CONFIRMED asm-equivalent in lane-core §6.3). So `score(k, s)` is the **d² stored in a 200-byte NearestEntry row** for slot `s`, read at its rank. Owner of the row: OPEN (§2.5).

**2.4(b) Curve.** `0x142139650(C, z)`: `u = clamp(z·C[+0x5E4] + 35, 0, 70)`; `i = trunc(u)`; if `i == 70` return `C[+0x34C]`, else linear interpolation of `C[+4+12i]`, `C[+4+12(i+1)]` by `u−i` (C-dec, `dec_0x142139650.c`). So the curve table is 71 entries at stride 12 starting at `+4` (entry 70 lies at `+0x34C`). Writer OPEN (O6).

**2.4(c) One-touch G8 cap.** `runCount = #{j ∈ 0..10 : 11·sd+j ≠ s, T(sd).+0xA2A0[j] == 0x25}`; `base = 2` if `level/100 ∈ {5,6}`, `1` if `== 4`, else `0`; `inc = 1` if `T(sd).+0xB8A4 >= 3` or `mode ∈ {2,3}`; `cap = min(2, base+inc)`; pass iff `runCount < cap` (C-dec; asm not re-read).

### 2.5 The four team rows (structure CONFIRMED by VA arithmetic; owners and writers OPEN)

Team ctx `T18 = TC+0x18`. Two sign-dependent blocks (k==1 vs k≠1), each made of a 200-byte NearestEntry-shaped row (team[2][11] `{int id; float d2}` + count[2] at `+0xB0` + rank[22] at `+0xB2`) and an 88-byte float[22] angle array (`0x140A71260`, C-dec in lane-core §5 and carrier-helpers §3):

| sign | block | VA | size | reader |
|---|---|---|---|---|
| k==1 | score row | `T18+0x228EC` | 200 | `0x140A714C0`, `0x140587E70` |
| k≠1 | list row | `T18+0x229B4` | 200 | `0x140A714E0` (OneTwo picker) |
| k≠1 | angle table | `T18+0x22A7C` | 88 | `0x140A71260` (k≠1), `0x140439F90` |
| k≠1 | score row | `T18+0x22AD4` | 200 | `0x140A714C0` (k≠1), `0x140587E70` |
| k==1 | list row | `T18+0x22B9C` | 200 | `0x140A714E0` (k==1) |
| k==1 | angle table | `T18+0x22C64` | 88 | `0x140A71260` (k==1) |
| all | per-slot NearestList | `T18+0x22E4C + 200·slot` | 22×200 | `0x140507740` (GF `NearestList`, `BuildNearest`) |

The sizes tile exactly: `0x229B4+0xC8 = 0x22A7C`, `0x22A7C+0x58 = 0x22AD4`, `0x22AD4+0xC8 = 0x22B9C`, `0x22B9C+0xC8 = 0x22C64`, `0x22C64+0x58 = 0x22CBC`, and `0x228EC+0xC8 = 0x229B4`. INF: the k==1 and k≠1 blocks are the two halves of one per-attack-direction layout. Owner of each row (a fixed centre point per k, most likely the attacked goal) and the sort key of the list row are **OPEN**. The writer is not located; the helpers doc §5 notes that the writers use computed addresses. Reading the rows is enough for both units.

### 2.6 Receiver predicate `0x142124950` (lane-core §8, spot-checked this session)

Spot checks against `dec_0x142124950.c` (this session): pre-gate `r ≤ 21, r ≠ c` (decompile head); `A_tab = T18+0x228D4+0x58·c+0x16A8+4r = T18+0x22F7C+0x58·c+4r` (decompile lines 40–43, matches §8); snapshot `local_e8 = 0x14063C560(receiver r)`, `ψ = local_b4 = snapshot(r)[0xD]` (declaration and the `local_b4 != 0` test); `player(r).+0x5D0 != 0x25`; angle block iff `K.+0x6C ≠ 0 ∨ mode ∉{3,4} ∨ C[0xF] ≠ 1 ∨ NL(c,r) >= 36`; `thr = speed(c) > 15 ? 80 : 120`; `wrapDiff(K.+0x38, A_tab[c][r]) <= thr`; window `3 <= dist <= max(u,3)`, `u = base + 10·ramp`, `base = (A+0xE4 < 120) ? 10 : 5`; M-tests `wrapDiff(player(r).+0x554, M[r][c]) < 120` and `wrapDiff(C[4], M[c][r]) > 20` with `A.class > 2`. The head and table offsets agree with lane-core §8. Full gate list: lane-core §8 (asm-verified there; not re-verified in full here).

Note: the predicate's `*param_1` is the **carrier snapshot C** (`C[4]` = `+0x10`, `C[0xF]` = `+0x3C`), not a player record. See C1.

---

## 3. OneTwo `0x14212AEB0` (CONFIRMED, asm `asm_14212AEB0.txt`, 64 lines)

**Signature.** `void OneTwo(undefined8 dead_rcx, answer* out (RDX=RBX), ctx** (R8=RDI))`. Answer record: `out[0]` type (`+0x0`), `out[2]` target (`+0x8`), `out[3]` b (`+0xC`), `out[4]` f10 byte (`+0x10`).

**Inputs.** `C = ctx[0]` (carrier snapshot: C[0] slot, C[4] attack angle, C[5] attack sign, C[0xF] flag); `K = ctx[4]` (+0x34 angle); `ctx[1]`, `ctx[5]`, `ctx[6]`, `ctx[7]` as §2.2.

**Constants.** `0.3` (`0x142590D98`), `0.5` (`0x142565600`), `180` (`0x142598580`), `6.0` (`0x14259BDEC`), `1.0`/`-1.0` (fold), `360` (`0x14259858C`), `90` (`0x14259BE5C`), `2.0` (`0x142565604`, closing divisor, inside 4C4E0).

**Pseudocode, checked against asm.**

```
OneTwo(ctx):                                                   # 0x14212AEB0
  fps   = 0x141E532D0()                                        # CALL 0x141e532d0
  frames = int(fps*0.3 + 0.5)                                  # 0x14212AECA..AEFA = 16
  cl = Closing(ctx.T, C[0], C[4], 180, 6.0, frames, 1, 0.0)    # 0x140A4E720 (AL = count)
  d  = K.+0x34 - C[4]                                          # MOVSS XMM1,[K+0x34]; SUBSS XMM1,[C+0x10]
  if |d| > 180: d -= sign(d)*360                               # 0x14212AF40..AF60 (sign: d>=0 -> +1)
  if (|d| <= 90) or (cl != 0):                                 # 0x14212AF76 JBE; 0x14212AF7F TEST EDX
      if OneTouch(ctx):                                        # 0x142128C10 -> AL     (0x14212AF86)
          t = OneTwoTarget(ctx)                                # 0x142126A80          (0x14212AF92)
          if t != 0xFF:                                        # CMP EAX,0xFF; JZ      (0x14212AF9C)
              out.type = 2; out.target = t; out.b = 0; out.f10 = 1   # 0x14212AF9E..AFAE
  return
```

**Branches.** Only one answer write exists (type 2, b 0, f10 1). Every other path writes nothing. The `|d| <= 90` test's `JBE` skips the closing check (0x14212AF76 → 0x14212AF83, which is the one-touch call).

**Callees, disposition.**
- `0x141E532D0` fps float: GF HAS (`kPesTickHz`, 54).
- `0x140A4E720`: decoded §2.1. Sweep `0x140A4E3F0`: decoded §2.1.
- `0x142128C10` one-touch: decoded §2.2. Sub-callee writers: score (§2.4a, OPEN writer), curve (§2.4b, OPEN writer), level/mode/markCount/event/action: GF-absent (§7).
- `0x142126A80` target picker: decoded §2.3. Writer of the list row: OPEN (§2.5).
- `0x142124950` receiver predicate: lane-core §8 (§2.6).

**Verdict.** The OneTwo unit is CONFIRMED end to end, with one OPEN dependency per branch (the one-touch's score and curve, the picker's list row, the receiver's b6c records and snapshot). The one-touch's lane is `ScanLane(opp, ref=C[0], …)` (G11), not a free-standing unit lane.

---

## 4. DirectHighBall `0x142129BE0` (CONFIRMED, asm `asm_dhb_full.txt`, decompile `dhb_0x142129BE0.txt`)

**Entry.** Interior of `FUN_142129590` (Cross). The bridge has no function here; `dec.sh` decompiled the body from `0x142129BE0`, and the asm is complete from `0x142129BE0` to `0x14212A2C4` (`RET`).

**Signature.** `void DHB(undefined8 dead_rcx, answer* out (RDX), ctx** (R8))`. Same answer layout as OneTwo.

**Inputs.** `C = ctx[0]` (C[0] = c, C[5] = k); `ctx[1]` = TC (T18 = TC+0x18); `K = ctx[4]` (`+0x40/+0x44/+0x48` ball point, `+0x6E` flag); `IA = ctx[5]` (`+0x9CC` zone, `+0xB6C` b6c table); `F` not used; `ctx[6]`, `ctx[7]` not used.

**Constants.** `25.0` (`0x14259BE20`, d² → 5 m), `0.1` (`0x142588200`), `100.0` (`0x14253EE30`), `9.0` (`0x14259D60C`), `225.0` (`0x14259BE70`), `900.0` (`0x1425C65F4`), `0.5` (`0x142565600`), `4.0` (`0x1425656C0`), `5.0` (`0x14259222C`), `60.0` (`0x14259A764`), `15.0` (`0x14259BE0C`), `80.0` (`0x142596E6C`), `360` / `-360` (`0x14259858C` / `0x142598598`), `0.6944` (`0x14259BDD0`), `-1.0` (`0x142588218`, initial best-d²).

**Pseudocode, checked against asm.**

```
DHB(ctx):                                                       # 0x142129BE0
  c    = C[0]                                                    # MOV R13D,[RAX] (0x142129C42; lane-structs §3.5)
  side = (11 <= c <= 21)                                         # r14 = IsAway(c)   (0x142129C4A..C58)
  rec  = AIrec_1(c)                                              # 0x140A48910: G+(c+0xA5)*0x28   (0x142129C98)
  if FirstTimeWhileReceiving(rec.state):                          # 0x140A6E5E0(movsx [rec+1]); JNE exit
      return NONE
  if K.+0x6E == 0: return NONE                                   # CMP BYTE [K+0x6E],0; JE exit (after CALL 0x140A6E5E0 @0x142129CB7, before CALL 0x140A489F0 @0x142129CDD)
  rec2 = AIrec_2(c)                                              # 0x140A489F0: G+(c+0xF3)*0x28
  thr  = float(byte(rec2+0x70+0x31))/100.0 + 0.1                 # 0x14124DAD0; CVTDQ2PS; DIVSS 100; ADDSS 0.1
  if K.+0x44 <= thr:                                             # COMISS; JA skip to 0x142129DC9 (block 0x142129CDD..0x142129DC9)
      opp  = NearestOpp(Tc, 1-side, K.ball, rank 0)              # 0x140A4EE40(TC, opp, &ball, 0)
      if opp > 21: return NONE                                   # CMP EAX,0x15; JA exit
      P    = player(opp)
      dh   = d2(hist(P), K.ball)     # 0x140A76240 then 0x140A8F890   (INF: hist accessor)
      dn   = d2(P.pos, K.ball)       # 0x140A8F890 on +0x55C
      if 25.0 < dh or 25.0 < dn: return NONE                     # COMISS xmm1(25.0); JB exit (0x142129D9B..0x142129DA4)
      if IsMid(IA.+0x9CC): return NONE                           # 0x1406997A0; JNE exit (0x142129DB8)

  best = 0xFF; bestD2 = -1.0                                    # 0x142129DC9..0x142129DE0
  row = NearestRow(c)                                            # 0x140507740(T18, c) = T18+0x22E4C+200c
  for j in 0..10:                                                # block side(c): row + (side*11 + j)*8
      m = row.team[side][j].id
      if m > 21 or m == c: continue                              # CMP EBX,0x15; JA; CMP R13D,EBX; JE
      cat = T(side).+0x4C00[m % 11]                              # magic-div mod 11; CMP EDX,0; JE skip
      if cat == 0: continue                                      # keeper
      if b6cStatus(IA.+0xB6C, m%11) != 0xB: continue             # 0x142118D20; CMP EAX,0xB; JNE
      if recA(IA.+0xB6C, m%11).class < 1: continue               # 0x142118C90; CMP [RAX],1; JL
      sc = score(T18, int(k), m)                                 # 0x140587E70(T18, int(C[5]), m)
      if sc < 225.0: continue                                    # COMISS xmm9(225); JA skip
      if sc < 900.0:                                             # COMISS xmm8(900); JBE branch-B (0x142129EDE)
          n = row_of(m).team[1 - side][0]                        # 0x14212xxxx: block = !side, entry 0
          if n.id <= 21 and n.d2 < 100.0: continue               # CMP [RCX],0x15; JA; COMISS xmm7(100); JA skip
      p9  = pos(m) + 9*vel(m)                                    # 9 = int(fps/6) (0x141E532E0 * 0xAAAAAAAB >> 34)
      if d2(K.ball, p9) < 9.0: continue                          # COMISS xmm11(9.0); JA skip (0x142129F9F)
      p27 = pos(m) + 27*vel(m)                                   # 27 = int(fps*0.5+0.5)  (0x141E532D0; XMM10=0.5)
      d   = d2(K.ball, p27)
      if best == 0xFF or d < bestD2: best = m; bestD2 = d        # COMISS xmm6,xmm0; JBE keep (0x14212A031–0x14212A052)
  if best > 21: return NONE                                       # CMP R12D,0x15; JA exit (0x14212A05F)

  out.type = 2; out.target = best                                # MOV [R15],2; MOV [R15+8],R12D (0x14212A091)
  X = wrap180(heading(player(best)) − T(side).+0x250)            # 0x140458740 then 0x14043A0A0 (XMM0=T+0x250, XMM1=heading)
  if |X| > 80.0:                                                 # COMISS xmm0(80); JB 0x14212A278
      out.b = 0; return                                          # f10 NOT written (see note)
  mc    = markCount(AIrec_2(best))                               # 0x140A58320 (+0x12A4 if <5 else 0)
  spd   = speed(player(best))                                    # 0x140477130
  free  = (mc == 4) or (mc not in {0,2} and spd > 15.0)          # TEST EDI,~2 / CMP EDI,4 / COMISS 15 (0x14212A106..A11A)
  if not free:                                                   # lane (0x14212A11F)
      origin = pos(best) + 4.0 * dir(θ)                          # 0x140A8F7C0, r = 4.0
      θ      = wrap360(angleTab(k)[best] + 180.0)                # 0x140439F90 (T18, int(k), best)  (CALL 0x14212A144; fold 0x14212A14F–0x14212A178)
      out_l  = ScanLane(teammate side, ref=c, origin, θ, len 15, cone 60, rad 5, keep 0, h = fps>>2 = 13)   # 0x14063D2F0
      free   = (out_l.count == 0)                                # 0x1421B7D40; CMOVE EBX,1 (0x14212A232–0x14212A247)
  if K.one_touch():  # 0x142128C10(ctx) AND 0x142124950(ctx, best)    (0x14212A24D–0x14212A265)
      out.f10 = 1                                                # MOV BYTE [R15+0x10],1
  out.b = 3 if free else 0                                       # TEST BL; MOV [R15+0xC],3 / 0 (0x14212A26A..A278)
```

**Branch and sense notes (asm-checked).**
- Low-ball block (`0x142129CDD`–`0x142129DC9`): `JA 0x142129DC9` skips the block when `height > thr`. Inside, every failure is `goto 0x14212a280` (RET, no answer). The `IsMid` test is `jne exit` (true → no answer).
- Branch A (`0x142129EDE`–`0x142129F28`): `COMISS XMM8(900),XMM0; JBE 0x142129F28` leaves branch A when score ≥ 900. `CMP [RCX],0x15; JA 0x142129F28` (rank-0 id invalid) also leaves branch A without skipping the mate. `COMISS XMM7(100),[RCX+4]; JA 0x14212A048` skips the mate iff `d2 < 100`; `d2 == 100` is kept.
- Best update (`0x14212A031`–`0x14212A052`): the first candidate is always taken (`CMP R12D,0x15; JBE`). Later candidates replace on a strictly smaller d² (`COMISS XMM6,XMM0; JBE keep`), so the first in row order wins ties.
- `|X|` gate: `COMISS XMM0(80.0), XMM2; JB 0x14212A278` (the `b=0` exit that skips f10).
- Lane choice: `TEST EDI,0xFFFFFFFD; JE LANE` (mc ∈ {0,2}); `CMP EDI,4; JE FREE` (mc == 4); `COMISS XMM0(spd),XMM6(15); JBE LANE` (speed ≤ 15 → lane); else FREE.

**Note on f10 in the `|X| > 80` path.** The asm jumps straight to `MOV [R15+0xC],0` (`0x14212A278`), so f10 is not written. The record's pre-value is whatever the caller left (INF: the caller clears the record; not traced).

**Callees, disposition.**
- `0x140A6E5E0(state)`: `(u32[0x1426484F0 + 4·state] >> 5) & 1` for `state ≤ 0x5D`, else 0 (C-dec, `dec_0x140a6e5e0.txt`). Bit table is an install constant; reading it is a PeImage read.
- `0x140A48910`, `0x140A489F0`, `0x140A48A10`, `0x140A48A30`: AI records and team records (C-dec). GF-absent AI record bytes (§7 S12, S13). `0x140A48A30` = `T(side)` (GF HAS team tables through `pesoffball`/`pescarrier`).
- `0x14124DAD0`: `p+0x70` (trivial; C-dec).
- `0x140A4EE40` → `0x140A4EE60` → `0x140A794C0` (sorted point list; tie rule OPEN): **rank-0 nearest opponent of a point**. GF has the id-based `NearestOpponents`; a point query needs a stand-in (§7 S16).
- `0x140A76240(P)`: `if P[4] != 0 return P + (P[4]+1)·12 else return P + 8` (C-dec). Semantics INF (ring accessor). Stand-in (§7 S17).
- `0x140A8F890` d2 (xz): GF HAS.
- `0x1406997A0` IsMid: GF HAS (`IsMid`, `pescarrier.hpp:275`).
- `0x140507740` NearestList row: GF HAS (`NearestList`, `BuildNearest`, `pesoffball.hpp:210,241`).
- `0x142118D20`, `0x142118C90`: b6c status and record A (GF-absent; writers `0x1421174F0`, enum `0x142117C30`, §7 S5).
- `0x140587E70` score: §2.4a (OPEN writer).
- `0x141E532D0` / `0x141E532E0`: fps float / int = 54 (`0x14351AE54`, `0x14351AE50`). **CONFIRMED** (asm: `mov eax,[0x14351AE50]`).
- `0x140458740` heading of `vel_eff`: C-dec (lane-core O10 says confirm by asm before porting; not done this session). GF HAS heading.
- `0x140477130` speed: C-dec, GF HAS.
- `0x140A58320` markCount: C-dec, `+0x12A4 < 5 ? +0x12A4 : 0` (CONFIRMED against `dec_0x140a58320.txt`). GF-absent field (§7 S11).
- `0x14043A0A0` wrap180(b−a): GF HAS.
- `0x140439F90` angle table + 180 with wrap360: C-dec (asm fold `0x14212A14F`–`0x14212A178`). Table `0x140A71260` writer OPEN.
- `0x140A8F7C0` advance: GF HAS (`lane-core` §1).
- `0x14063D2F0` packer B → core: GF `pes::lane::ScanLane` (`peslane.hpp`, main).
- `0x1421B7D40`: count read; trivial.
- `0x142128C10`, `0x142124950`: §2.2, §2.6.

---

## 5. Port spec: OneTwo (format of carrier-helpers §4)

```
OneTwo(C, K, ctx) -> (type 2, target, b = 0, f10 = 1) | no-answer
  # inputs (named)
  #   C.slot, C.attackAngle_deg (C[4]), C.attackSign (C[5]), C.flag15 (C[0xF])
  #   K.angle34 (ball/carrier angle, +0x34, deg), K.ball (+0x40/+0x44/+0x48)
  #   players[22] {pos, vel, active, positionCategory}, team tables (rows, angle, score, list)
  frames = int(fps*0.3 + 0.5)                       # 16
  count  = ClosingCount(players, C.slot, A = wrap360(C.attackAngle_deg),
                        half = 90, outer = 6.0, frames, inner = 0.0)
  d = K.angle34 - C.attackAngle_deg; fold once if |d| > 180
  if !(|d| <= 90 or count != 0): return NONE
  if !OneTouch(C, K, ctx): return NONE
  t = OneTwoTarget(C, ctx); if t == 0xFF: return NONE
  return (2, t, 0, true)
```

**Port order.** (1) `ClosingCount` (pure function on positions, velocities and angles; GF has the arithmetic). (2) `OneTouch` (§2.2 G1–G12). (3) `OneTwoTarget` over the list row. (4) `Receiver` (lane-core §8). (5) the unit.

**GF has.** Velocity, position, speed, heading, wrap180/360, d2, bearing (`std::atan2` = labelled approximation of `0x140A92D40`, O3), keeper slot (`pes::lane::KeeperSlot`), `ScanLane` (`pes::lane::ScanLane`, `peslane.hpp` on main), `IsAwaySlot`, `IsClose`/`IsMid`/`IsCentral` (`pescarrier.hpp:274–276`), `NearestList`/`NearestOpponents` (`pesoffball.hpp:210–243`), `kOneTwoPassRun = 0x25` (`pesoffball.hpp:55`), `PassAnswer` (`pescarrier.hpp:384`), plan `kPlanPassThrough = 4` (`pescarrier.hpp:402`). **Fps** `kPesTickHz`.

**Stand-ins (`// ponytail:`), each naming its PES site.**
- S1 score row `T18+0x228EC/0x22AD4` (`0x140587E70`) — writer OPEN; one-touch G5 uses 1600 against it.
- S2 curve table `IA+0x2B0`, `+0x5E4`, `+0x34C` (`0x142139650`) — writer OPEN (O6); one-touch G12.
- S3 list row `T18+0x22B9C/0x229B4` (`0x140A714E0`) — writer OPEN; OneTwo picker order.
- S4 team mode `M+8` (`0x141052E30`), team level `T+0xA294/A29C` (`0x140A40BD0`), `T+0xB8A4`, `T+0xA2A0` — writers OPEN (O12, mode UNKNOWN).
- S5 b6c records `IA+0xB6C` status, recA, recB (`0x142117C30`, `0x1421174F0`) — stand-in pattern of GoalFront.
- S6 snapshot `0x14063C560`/`C` builder (O7); the snapshot's `[0xD]` = `T(side)+0x4BD4[k]` (writer INF).
- S7 event record `E` (`0x140A3E190`) and per-player action `+0x5D0` — GF-absent; GF's `kOneTwoPassRun` enum exists.
- S8 pitch bound `F+0x18` (G2) — name INF; stand-in: GF pitch half length (`ComputeShootZone` halfLen).
- S9 `K.angle34` (`+0x34`) semantics INF; reads as a ball/carrier angle in degrees.

---

## 6. Port spec: DirectHighBall (format of carrier-helpers §4)

```
DHB(C, K, ctx) -> (type 2, target, b, f10) | no-answer
  # inputs (named)
  #   C.slot (c), C.attackSign (k), K.flag6E, K.height (+0x44), K.ball (+0x40/+0x48)
  #   AI.state(c) (rec+1), AI.byte70_31(c) (rec2+0x70+0x31)
  #   IA.zone (IsMid), IA.b6c, team T(side) (+0x250, +0x4C00), NearestList row(c)
  if K.flag6E == 0: NONE
  if FirstTimeWhileReceiving(AI.state(c)): NONE           # bit 5 of install table
  thr = AI.byte70_31(c)/100 + 0.1
  if K.height <= thr:
      opp = NearestOpponentToPoint(K.ball, side = !side(c), rank 0)
      if opp > 21: NONE
      if max(d2(hist(opp), K.ball), d2(pos(opp), K.ball)) > 25.0: NONE   # 5 m
      if IsMid(IA.zone): NONE
  best = NONE; bestD2 = -1
  for m in row(c).team[side(c)] (nearest-first):
      skip m>21, m==c, keeper(m) (T(side).+0x4C00[m%11] == 0)
      skip unless b6c.status(m) == 0xB and recA(m).class >= 1
      sc = score(k, m)
      skip sc < 225
      if sc < 900 and row_of(m).team[!side][0] valid and d2 < 100: skip
      skip if d2(K.ball, pos(m)+9·vel(m)) < 9
      d = d2(K.ball, pos(m)+27·vel(m))
      if best == NONE or d < bestD2: best = m, bestD2 = d
  if best == NONE: NONE
  out = (2, best, b=?, f10=0)
  X = wrap180(heading(best) - T(side).+0x250)
  if |X| > 80: return (2, best, 0, f10 = unset)          # INF: caller clears record
  mc = markCount(AI.rec2(best)); spd = speed(best)
  free = (mc == 4) or (mc not in {0,2} and spd > 15)
  if !free:
      origin = pos(best) + 4.0 * dir(wrap360(angleTab(k)[best] + 180))
      θ      = wrap360(angleTab(k)[best] + 180)
      free   = (ScanLane(side(c), ref=c, origin, θ, 15, 60, 5, keep=false, h=13).count == 0)
  f10 = OneTouch(C, K, ctx) and Receiver(C, K, ctx, best)
  b   = free ? 3 : 0
  return (2, best, b, f10)
```

**Port order.** (1) the NearestList reads (GF HAS), (2) `score` and `angleTab` stand-ins, (3) the ScanLane teammate call (`pes::lane::ScanLane(k, players, ref=c, side=IsAwaySlot(c), {origin, θ, 15, 60, 5}, false, 13)`), (4) the one-touch and receiver shared with OneTwo.

**GF has.** `pes::lane::ScanLane` with the teammate side (`side = IsAwaySlot(c)`), keeper exclusion (`keepKeeper = false`), horizon 13 (`fps>>2`), `NearestList`, `IsMid`, speed/heading, d2, wrap.

**Stand-ins.**
- S10 `angleTab(k)` `T18+0x22C64/0x22A7C` (`0x140A71260`, writer OPEN). GoalFront's facing-vector stand-in pattern (`A4Lanes` d5e8417, `elizacontroller.cpp`).
- S1 `score` (see §5). Owner INF.
- S11 `markCount` `AI rec2 +0x12A4` (`0x140A58320`) — GF-absent; stand-in 0.
- S12 `AI rec+1` state byte (`0x140A48910`) — writer OPEN; the bit test is exact once the byte is known.
- S13 `AI rec2+0x70+0x31` byte (`0x14124DAD0`) — writer and semantics OPEN. The DHB threshold reads it as `/100 + 0.1` (asm: `CVTDQ2PS`, `DIVSS 100.0`, `ADDSS 0.1` between `CALL 0x140A489F0` at `0x142129CDD` and `JA 0x142129DC9`).
- S14 b6c status `0x142118D20` and record A `0x142118C90` (as S5).
- S15 team line/`T(side)` +0x250 (side attack angle; GF has the attacking direction per side).
- S16 point nearest-of-side (`0x140A4EE40`) — GF stand-in: linear sort by d² to the point with ties by slot order; PES tie rule OPEN (0x140A794C0).
- S17 history position `0x140A76240` — stand-in: current position. Under the stand-in the two tests collapse into one (`d2(pos, ball) <= 25`); this is named as a stand-in, not a claim.
- S18 `K.flag6E` and `K.flag6C` (carrier flags; writers in carrier-ai §Unit context, not re-traced).

---

## 7. GF-absent inputs and writers (summary)

| ID | Input | VA | Writer | Status | Used by |
|---|---|---|---|---|---|
| S1 | score row `T18+0x228EC/0x22AD4` | `0x140A714C0`, `0x140587E70` | OPEN (computed address) | stand-in | one-touch G5, DHB |
| S2 | curve table `IA+0x2B0/+0x5E4/+0x34C` | `0x142139650` | OPEN (O6) | stand-in | one-touch G12 |
| S3 | list row `T18+0x22B9C/0x229B4` | `0x140A714E0` | OPEN | stand-in | OneTwo picker |
| S4 | team mode `M+8`, level `T+0xA294/A29C`, `T+0xB8A4`, `T+0xA2A0` | `0x141052E30`, `0x140A40BD0` | UNKNOWN / OPEN (O12) | stand-in | one-touch G3/G4/G8, receiver |
| S5 | b6c `IA+0xB6C` status/recA/recB | `0x142118D20/C90` | `0x1421174F0`, enum `0x142117C30` | stand-in | all three |
| S6 | carrier snapshot C builder | — | OPEN (O7) | stand-in | OneTwo, DHB, one-touch |
| S7 | event record E | `0x140A3E190` | INF | stand-in | one-touch G7 |
| S8 | pitch bound F+0x18 | `F = G+0x2AA8` | OPEN | stand-in (GF halfLen) | one-touch G2 |
| S9 | `K.+0x34` angle | ctx4 | INF | named | OneTwo gate |
| S10 | angle table `T18+0x22C64/0x22A7C` | `0x140A71260`, `0x140439F90` | OPEN | stand-in (GoalFront pattern) | DHB θ |
| S11 | markCount AI rec2+0x12A4 | `0x140A58320` | OPEN | stand-in 0 | DHB, one-touch G10 |
| S12 | AI rec+1 state byte | `0x140A48910`, `0x140A6E5E0` | OPEN | stand-in | DHB gate |
| S13 | AI rec2+0x70+0x31 byte | `0x14124DAD0` | OPEN | named | DHB threshold |
| S14 | team line `T18+(sd+3)·0x14` | `0x142128CB8` | OPEN | named | one-touch G1 |
| S15 | `T(side)+0x250` attack angle | `0x140A48A30` | (GF has direction) | named | DHB `|X|`, OneTwo C[4] |
| S16 | point nearest-of-side | `0x140A4EE40/60`, `0x140A794C0` | OPEN (tie rule) | stand-in | DHB low-ball gate |
| S17 | history position | `0x140A76240` | INF (ring accessor) | stand-in = current pos | DHB low-ball gate |
| S18 | `K.+0x6C`, `K.+0x6E`, `K.+0x6F` flags | ctx4 | not re-traced | named | OneTwo, DHB, one-touch |

Every stand-in gets a `// ponytail:` comment naming its PES site (the table's VA).

---

## 8. Doc corrections (doc:line, old, new, evidence)

C1. `docs/re/pes-re-lane-structs.md:123` — old: "`c: r8d = [rcx] = R[0]` where `R` = the **receiver** record". New: "`R` = `ctx[0]` = the **carrier snapshot C** (C[0] slot, C[4] attack angle, C[5] attack sign, C[0xF] flag). The unit's own carrier, not a receiver." Evidence: OneTwo asm `0x14212AEB0`–`0x14212AEFA` (`MOV RAX,[RDI]` → `MOV EDX,[RAX]`, `MOVSS XMM2,[RAX+0x10]`); one-touch `0x142128C41` (`MOV RAX,[RCX]`); predicate `*param_1+0x3C` (C[0xF]); lane-core §7 snapshot layout.
C2. `docs/re/pes-re-lane-structs.md:124` — old: "`s = !IsAwaySlot(receiver)` = the receiver's **opponent** side". New: "`s = !IsAwaySlot(C[0])` = the carrier's opponent side (the lane scans the opposite side from the carrier)". Evidence: one-touch `0x142129035`–`0x14212903F` (`R[8]==0`, the side of C).
C3. `docs/re/pes-re-lane-structs.md:253` (§6) — old: "OneTwo needs `ScanLane` with `side = !IsAwaySlot(receiver)`". New: "…`side = !IsAwaySlot(carrier)`, `ref = carrier`, lane = G11 (15/60/2 imm/.rdata), h = 0, keep = 0, count ≠ 0 is part of G11 and the lane-core G12 follows it." Evidence: as C1–C2 and `0x142129076`.
C4. `docs/re/pes-re-lane-structs.md:118–131` (§3.4) — old: OneTwo's lane call is the only call. New: OneTwo's gate is closing sweep `0x140A4E720` (asm `0x14212AEC5`–`0x14212AF15`), `|d| <= 90`, one-touch `0x142128C10` (`0x14212AF86`), target picker `0x142126A80` (`0x14212AF92`), answer `(2, t, 0, 1)` (`0x14212AF9E`). Evidence: asm `0x14212AEB0`–`0x14212AFC1`.
C5. `docs/re/pes-re-lane-structs.md:142–144` (§3.5 origin) — old: "INF on which ball/anchor position feeds it". New: origin = **target mate's own position** (`+0x55C`, `+0x564`) advanced `4.0` m along θ by `0x140A8F7C0`. Evidence: asm `0x14212A11F`–`0x14212A189` (`MOVSD XMM0,[RBX+0x55C]`, `MOV EAX,[RBX+0x564]`, `CALL 0x140A8F7C0`, `XMM2 = 0x1425656C0 = 4.0`).
C6. `docs/re/pes-re-lane-structs.md:264` (§7 item 2) and `:150` — old: "`0x141E532E0` OPEN, INF a distance/step helper". New: CONFIRMED. `0x141E532E0` = `mov eax,[0x14351AE50]` = int fps 54; DHB `h = fps>>2 = 13`. Evidence: objdump `0x141E532E0`; DHB `0x14212A1CC`–`0x14212A1D1` (`CALL 0x141E532E0; SHR EAX,2`).
C7. `docs/re/pes-re-lane-core.md:183` (§3.6) — old: "Both observed callers pass the opposite side. … teammate branch … no caller exercises it today." New: DHB is a teammate caller (`s = IsAway(c)`, `r9d = r14d`, `0x14212A21F`, call `0x14212A22D` into packer B `0x14063D2F0`). The teammate branch is live. Evidence: asm `0x142129C4A`–`0x142129C58`, `0x14212A21F`–`0x14212A22D`.
C8. `docs/re/pes-re-lane-core.md:299` (§6.4) — old: "the receiver is in the attacking band … the receiver can one-touch because the next lane is blocked." New: the subject is the **carrier** C (the one-touch is the carrier's own first-touch decision; G1–G2 test the carrier's position against the line). INF on the wording; the slot in every gate is C[0]. Evidence: one-touch `0x142128C41`–`0x142128CFC` (`[RAX]` = C[0] for `P_s`).
C9. `docs/re/pes-re-carrier-helpers.md:59`, `:67`, `:353` — old: "team-table lines `T18+0x40+side*0x14`" / "`line = T18+0x40+oppSide*0x14`" / "`lineB = T18+0x40+(side+3)*0x14`". New: the one-touch G1 team line is `T18+(sd+3)*0x14` with **no +0x40** (asm `0x142128CB8`–`0x142128CC3`). The helpers doc's `+0x40` is unverified for the validator and must be rechecked against its asm before a port. Evidence: as stated.
C10. `docs/re/pes-re-carrier-ai.md:63` (Unit context) — old: "`+0x44` pressure 0..1 (§4.3)". New: `+0x40/+0x44/+0x48` is the ball point (x, y, z). The xz d² calls take `+0x40` as the point and read `+0x48` as z, so `+0x44` is the **height** (INF on the name; DHB compares it to `byte/100+0.1`). Evidence: DHB asm `MOVSS XMM0,[RAX+0x44]` then `COMISS XMM0,XMM1; JA 0x142129DC9` (block `0x142129CDD`–`0x142129DC9`); DHB's d2 calls (`0x142129D35`–`0x142129DA4`) take `K+0x40` as the point.
C11. `docs/re/pes-re-carrier-ai.md:60` (Unit context) — old: "`ctx[0]` = &BallPlayer+0x1C (u32 player index 0..21 …)". New: `ctx[0]` is the **carrier snapshot C** with the lane-core §7 layout (C[0] slot, C[2] side, C[4] = wrap360(T(side)+0x250), C[5] attack sign float, C[0xF] flag). Reconcile the two; the unit code reads the snapshot layout. Evidence: OneTwo `0x14212AEB0` (`MOVSS XMM2,[RAX+0x10]` = C[4]), predicate `*param_1+0x3C` (= C[0xF]).
C12. `docs/re/pes-carrier-A4Cont.md:27` (DHB row) — old: "keeper > 25 m via `0x140a4ee40`, `0x140a76240`, `0x140a8f890`". New: the test is on the **nearest opponent of the ball** (rank 0 of side `!side(c)`, `0x140A4EE40`), applied only when `K.height <= byte/100+0.1`. No answer unless `d2(hist, ball) <= 25` **and** `d2(pos, ball) <= 25` (25 = 5 m, `0x14259BE20`), and `IsMid(zone)` is false. The keeper is not tested. Evidence: DHB asm `CALL 0x140A4EE40` (`0x142129D35`), `CALL 0x140A76240` (`0x142129D6B`), `COMISS XMM1(25.0)` (`0x142129D9B`, `0x142129DA4`), `CALL 0x1406997A0` (`0x142129DB8`).
C13. `docs/re/pes-re-A4Carrier.md:118` (row 11, DHB) — old: "receiver records; keeper > 25 m; team pass-request table". New: the mate loop is the carrier's own NearestList row, block `side(c)` (`0x140507740(T18,c)`, `CALL` at `0x142129DE0`), with b6c status and record A class, score ≥ 225 and branch A (< 900, and the mate's rank-0 opponent < 100 skips), and 9/27-frame predictions. There is **no** pass-request table in this unit. Answer: `(2, best, b = 3 if free else 0, f10 if |X| <= 80 and one-touch and receiver)`. Evidence: DHB asm `0x142129DE0`–`0x14212A052` (loop), `0x14212A05F`–`0x14212A278` (answer).
C14. `docs/re/pes-re-A4Carrier.md:117` (row 10, OneTwo) — old: "opponent within 6 m in 16 frames (`0x140A4E720`) or facing within 90° of attack". New: the closing count (opponents within 6 m of the carrier's 16-frame point, bearing within 90° of the carrier's `C[4]` attack angle), **or** `|wrap180(K.+0x34 − C[4])| <= 90`. "facing" → "ball/carrier angle `K.+0x34` vs team attack angle `C[4]`". Evidence: OneTwo `0x14212AF40`–`0x14212AF7D`.
C15. `docs/re/pes-carrier-A4Cont.md:24` (`0x140a4e720` row) and `:39` (open item 4) — old: "**OPEN:** Ghidra drops the carrier index … (`0x140a48a10(G)` is called with one argument)". New: RESOLVED. The carrier slot is `EDX` (OneTwo asm `MOV EDX,[RAX]`), passed unchanged to `0x140A4E3F0` and to `CALL 0x140A48A10(G, EDX)` at `0x140A4E441`. Evidence: asm `0x140A4E3F0`–`0x140A4E441`.
C16. `docs/ports/pes-port-LanePort.md:115` (item 5, O14) — old: "The teammate branch has no caller yet (INF until O14)". New: DHB (`0x14212A22D`) is the teammate caller; the port's `ScanLane` teammate branch is needed by DHB. Evidence: §C7.
C17. `docs/re/pes-re-lane-core.md:413` (O14) — old: "The teammate branch is unexercised until then." New: exercised by DHB (C7).
C18. `docs/re/pes-re-lane-core.md:404` (O5) — old: "owner of the NearestEntry list behind `score` and the 1600 threshold's semantics OPEN". New: the structure is decoded (§2.4a, §2.5): 200-byte NearestEntry row at `T18+0x228EC` (k==1) / `+0x22AD4`. The 1600 (G5) and the DHB pair 225/900 are read on the same row. Owner still OPEN; the writer is not located. Evidence: `0x140587E70`, `0x140A714C0`, the VA arithmetic in §2.5.
C19. `docs/re/pes-re-A4Carrier.md:113`/`pes-re-carrier-helpers.md` — no change.

---

## 9. Open items (each with the next step)

1. **Score row owner and writer** (`T18+0x228EC/0x22AD4`, §2.5). OPEN. Next: decode the team-table builder (candidate: the image builder `0x142119390`, which writes the pressure table, §5 of carrier-helpers) and search its stores for `T18+0x228EC/0x22AD4/0x22B9C/0x229B4/0x22C64/0x22A7C`. The VA arithmetic says the four rows share a builder with the per-slot NearestList rows.
2. **Curve table** `IA+0x2B0` (+0x5E4 scale, +0x34C end) — OPEN (O6). Next: the same builder search; the scale is a single float.
3. **Angle table** writer (`0x140A71260`, k==1 `+0x22C64`, k≠1 `+0x22A7C`) — OPEN, shared with GoalFront and Safety (carrier-helpers §5).
4. **AI rec2+0x70+0x31 byte** (DHB threshold) and **AI rec+1 state** (first-time bit) — writers and semantics OPEN. The bit table is read; the state byte is GF-absent.
5. **History position** `0x140A76240` (P+4 count, P+12·(n+1) ring) — semantics INF. Needs a caller trace in the DHB unit list or an ring writer.
6. **Point nearest-of-side** `0x140A794C0` tie rule — OPEN (affects DHB's `opp` only when two opponents are at the same d²).
7. **`0x140458740` heading** — C-dec only (O10). Confirm by asm before porting speed/heading in the predicates.
8. **`0x140A92D40` atan2** — O3. The port uses `std::atan2` labelled as an approximation.
9. **Carrier snapshot C caller** (O7) and the naming conflict in C11.
10. **Team mode `M+8`, team level `+0xA294/A29C`, `T+0xB8A4`, `T+0xA2A0`** — writers OPEN (O12).
11. **Event record E** (`0x140A3E190`) — writer INF.
12. **Pitch bound `F+0x18`** (one-touch G2) — name INF.
13. **f10 when `|X| > 80`** (DHB) — the caller's pre-value; trace the unit-list caller that clears the answer record.
14. **Receiver predicate full re-verification** (lane-core §8 was asm-verified in its own session; this session spot-checked the head and table offsets only).

---

## 10. Method and verification

- Decompiles: `dec_0x14212AEB0.c` (bridge), `dec_0x142128C10.c` (bridge, lines 39–182 read), `dec_0x142124950.c` (bridge, lines 20–140 read), `dhb_0x142129BE0.txt` (`dec.sh`, read-only PESLive). Helpers: saved decompiles in `gf-A4Cont-scratch/`, `gf-LaneCore-scratch/`, `gf-CarrierHelpers-scratch/`, `gf-A4Lanes-scratch/`, plus new `dec_0x140456ea0.c` (bridge).
- Asm: `objdump -D -b binary -m i386:x86-64 -M intel --adjust-vma=0x140000000` on `pes_full.bin` for DHB (`0x142129BE0`–`0x14212A2C4`), OneTwo (`0x14212AEB0`–`0x14212AFC1`), the one-touch head (`0x142128C10`–`0x142128E20`) and lane tail (`0x142128E20`–`0x142129100`), the closing wrapper and sweep (`0x140A4E720`–`0x140A4E7B0`, `0x140A4E3F0`–`0x140A4E460`), the target picker (`0x142126A80`–`0x142126B70`).
- Constants: `struct.unpack` of image bytes at VA-0x140000000 (§0).
- **Not run:** no tests, no build, no live PES, no commit. The unit answers are specs; nothing in GF was changed.
