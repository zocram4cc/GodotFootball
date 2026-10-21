# Press target eval FUN_140633940 (BallPlayer_PressingTargetEval): full decode

> **Main note 22-10-26:** PressEvalRE, checker 151/151, 4/4 mutations; not yet Main-audited. Corrects pes-re-far-chase.md §6: the target eval returns the FIRST passing man in the nearest list (no keep-max), writes only 0x34 (PRESS) and u16[T+0xB88C+2·slot] = mode; FUN_140A42B90 writes T+0xC1C[slot], not B88C.

Status: decoded from bytes (`pes_full.bin`, file offset = VA − 0x140000000) and checked against the Ghidra bridge decompile. Every instruction VA and constant in §10 is re-verified by `<workdrive>/gf-PressEval-scratch/check_eval.py`. Tags: CONFIRMED = asm bytes at the cited VA (or decompile with the same asm shape); INF = inference; OPEN = not resolved (§8).

## 0. Rule in short

Given holder `ref`, presser side `s`, excluded man `excl`, incumbent `inc_m` and mode `m`, the eval walks side `s`'s 11 men in `ref`'s nearest list (ascending d², table order) and returns the first man that passes, in order:
1. active, not `excl`, kind not in {0x11,0x14,0x50,0x52,0x57}, slot word `T+0x4C00` non-zero, no flag-15, `T+0xB748` ≠ 0x24 (and, when the side flag W is set, not a G180 entry with flag 0);
2. `S = |ref − cand|` ≤ θ (θ = 22 / 15 / 13 / 10, +3 for the incumbent, plus the Block-A +10 and the W/X overrides), with a second threshold θ2 and the G_sub test;
3. the refusal test FUN_140633350 is 0;
4. mode-specific test: m=1 MARK/PASS_COURSE takeover (releases a partner, replacement from the ball event), m=4 lateral line test, m=7 bearing test against `excl`, other modes none.

On accept it writes `setter(T, slot, 0x34)`, `T.u16[0xB88C+2·slot] = m`, and returns the pid. Otherwise it returns 0xFF and writes nothing. There is no keep-max: the first passing man returns.

## 1. Contract (CONFIRMED)

- Win64 signature `uint FUN_140633940(T, P, excl, ref, inc_m, mode, aggr)`: RCX=`T` (team record), RDX=`P` (`[P]` = ctx, MA = `ctx+0x18`, `dword [P+0x10]` = side 0/1), R8=`excl` (candidates equal to it are skipped), R9=`ref` (pivot; `FUN_140A48A10(G,ref)`), `[rsp+0x28]`=`inc_m`, `[rsp+0x30]`=`mode` (written to the target word), `[rsp+0x38]`=`aggr` (byte). Callee-side offsets: `[rsp+0x1a0]`=excl, `[rsp+0x1a8]`=ref, `[rsp+0x1b0]`=inc_m, `[rsp+0x1b8]`=mode, `[rsp+0x1c0]`=aggr.
- Caller FUN_140634470 at 0x140634A84 (verified in bytes): R8=`[r15+4]`, R9=`[r15]`, 5th=`[r15+8]` (stored to `[rsp+0x20]`), 6th=`r13d` (class, stored to `[rsp+0x28]`), 7th=`dil` (aggr, stored to `[rsp+0x30]`), RDX=`r14`. This matches PressCoordRE's contract.
- Sign: `s = i8[T+0x254]` (0x140633A0C), always ±1.
- Return: accepted pid (0..21) or 0xFF. Exits: early exit 0x140633B39 (`ja 0x1406343F1`, with `ebx` = 0xFF set at 0x140633A3E); loop end 0x1406343F5 (`mov eax, 0xFF`); accept 0x1406343F1 (`mov eax, ebx`, `ebx` = cand from 0x1406343EF).
- Writes only on accept (§5). The accepted pid's slot is `pid % 11` ∈ 0..10 for every pid ≤ 21, so the slot ≥ 11 arms are dead.
- The call FUN_140439EF0 at 0x140634030 sits in a reject arm, and its float result is discarded: no effect (CONFIRMED).

## 2. Gate sequence (CONFIRMED by asm; VAs are the test or call that implements each step)

Per call (prologue):
- `W := F13 = FUN_140A63950(ctx, s, 13, 0xFF)` (0x140633A53; `test bpl,bpl` at 0x140633AF8, `je` at 0x140633AFB).
- `Z = FUN_140A63950(ctx, s, 14, ref)` (0x140633A2F). Then `Z := 1` if `i32[T+0xB8A4] ≥ 3` and `FUN_140A55D40(G+0x180, s, 2) == 0` (0x140633A66–0x140633A9C).
- `K = FUN_141E532E0()` (int fps), `fps = FUN_141E532D0()` (float). Both: §6 H13.
- EARLY EXIT (0x140633AFB–0x140633B39): if `F13` and `s·ref.x > s·L`, return 0xFF. Here `L = f32[MA + 0x40 + 0x14·(s==0)]` (side 0 reads +0x54, side 1 reads +0x40; 0x140633B13–0x140633B30). Compare at 0x140633B36 with xmm1 = −s·L, xmm0 = −s·ref.x.

Loop: k = 0..10 (table order, side block of `ref`'s list). `cand = u32[MA + 0x22E4C + 200·ref + 8·(s·11 + k)]` (ref < 22; for ref ≥ 22 the base is `MA+0x22E4C`). The first accept returns.

Per candidate, in order:
- G1 `cand < 22` (0x140633BFF–0x140633C02).
- G2 active: `u8[MI+0x1748+cand] ≠ 0` (0x140633C1E–0x140633C2F).
- G3 `cand ≠ excl` (0x140633C35–0x140633C38).
- G4 only if W: find the first i in 0..23 with `i32[G180 + 0x74i + 0x20] == cand`. Reject if found and `u8[G180 + 0x74i + 0xD] == 0` (0x140633C3E–0x140633C87).
- G5 kind: `k8 = i8[FUN_140A48910(G,cand) + 1]`. Reject if k8 ∈ {0x11,0x14,0x50} (bit test with 0x8000000000000009, 0x140633CA7) or k8 ∈ {0x52,0x57} (0x140633CB3–0x140633CBF).
- G6: `i32[T+0x4C00+4·slot] ≠ 0` (0x140633CC5–0x140633CDC) and `FUN_140A63950(ctx, s, 15, cand) == 0` (0x140633CEE–0x140633CFB).
- G7: `i32[T+0xB748+4·slot] ≠ 0x24` (0x140633D04–0x140633D0D).
- `inc = 5.0` if cand == inc_m else 0 (0x140633D17–0x140633D20).
- `B29 = (FUN_140A40BC0(T) == 3)` (0x140633D29–0x140633D3A).
- `D_ev = FUN_140A74EC0(E, MI)`, `E = u64[ctx+0xE0]` (0x140633D56). If `D_ev < 4K` (0x140633D65–0x140633D67) and `u32[MI+0x14C8 + 4·(s==0)] == 1` (0x140633D88), set `W := 1` (store 0x140633D9D). The loop back-edge reloads W from `[rsp+0x38]` (0x1406341E1), so **W is sticky across candidates** (CONFIRMED behaviour; intent OPEN, §8 O9).
- `CLS = FUN_140A60340(FUN_140A40BD0(T))` (0x140633DA1–0x140633DB1) = `i32[T+0xA294] / 100` when `u8[T+0xA29C] == 0`, else 0.
- `REC2 = FUN_140A489F0(G,cand)`; `bit23 = (u32[REC2+0xB0] >> 23) & 1` (0x140633DC2–0x140633DCA; FUN_14124DAD0 = +0x70, field +0x40).
- Mode-4 pre-gate (only mode == 4; 0x140633DCF–0x140633E18): `RI = i32[T+0x87D0+8·slot]`. If `CLS==6 ∨ W ∨ B29 ∨ RI≠2`: reject iff `RI == 0`. Otherwise (RI == 2): reject iff `bit23 == 0` and `i32[T+0xB8A4] ≤ 2`.
- `S = FUN_14043B8B0(MA, ref, cand)` = `|ref − cand|` in metres (0x140633E29; §6 H36).
- `X = u8[T+0xA53C]`, `Y = u32[T+0xA544]` (0x140633E2E, 0x140633E39).
- `θ = 22` if `X≠0 ∨ W`; else 15 if Z; else 13 if B29; else 10 (0x140633E40–0x140633E7A). Then `θ += 3` if cand == inc_m (0x140633E82–0x140633E8B).
- Block A (0x140633E93–0x140633F41): if `P10(ctx, 1−s) == 0` (0x140633EC5–0x140633ECC), `FUN_140477130(ref) > 5.0` (0x140633ED3–0x140633EDC), `CLS == 6` (0x140633EDE–0x140633EE2), and `FUN_14043A020(FUN_140458740(ref, 0.6944/fps), heading(ref→cand)) < 60.0` (0x140633EE4–0x140633F3B): set `dil := 1` (0x140633F41) and `θ += 10.0` if `θ < 10` (0x140633F3D–0x140633F46). `dil` is the bool bVar27.
- Accept-1: `S ≤ θ`, else reject (0x140633F4B–0x140633F4F).
- G_sub (0x140633F55–0x140633F9E): pass iff `W ∨ (X≠0 ∧ trunc(5·fps + 0.5) < Y) ∨ FUN_140A4C6E0(ref.pos, cand.pos, s)`, where the last term is `s·cand.x > s·ref.x`. Otherwise reject.
- θ2 (0x140633FA8–0x140633FF1): if `i32[T+0xB8A4] ≥ 3`, θ2 = max(12, θ) (0x140633FB9–0x140633FC5). Else if mode == 4, θ2 = θ. Else θ2 = 12 if `CLS==6 ∨ B29`, else 6.
- Accept-2 (0x140633FF1–0x14063401F): pass iff `S ≤ θ2 ∨ dil ∨ W ∨ (X≠0 ∧ trunc(5·fps+0.5) < Y)`. Otherwise reject.
- Refusal (0x14063404D–0x140634078): `FUN_140633350(P, T, cand, mode) ≠ 0` → reject (§6 H42).
- Mode arms (0x140634094 `cmp r15d,7`; 0x1406341EA `cmp r15d,1`; 0x1406341F4 `cmp r15d,4`; other modes jump to the accept write at 0x1406343D1):
  - Mode 7 (0x14063409E–0x14063419F). Require `RI_c == LR ∨ W`, `RI_c ≠ 0`, `T.u32[0xA2A0+4·slot] ≠ 0x3C`, where `RI_c = i32[T+0x87D0+8·slot]` and `LR` is 1 if `RI(excl)==0`; 2 if `RI(excl)==1`; 2 if `RI(excl)==2 ∧ aggr`; else 3 (prologue 0x1406339B3–0x1406339ED). Then `d = wrap(heading(ref→cand) − heading(ref→excl))` (FUN_140439E50 ×2, wrap at 0x140634153–0x14063416C) and accept iff `|d| ≥ (40 if cand == inc_m else 50)` (0x14063417B, 0x140634194, `jbe` at 0x14063419F).
  - Mode 4 (0x1406341FE–0x14063430B). Unless cand == inc_m: require `|d| ≥ 100` with `d = wrap(heading(ref→cand) − heading(ref→excl))` (0x1406342C2–0x1406342CD). Then accept iff `s·cand.x − 10 ≤ s·f32[T+0x4B4C+12·slot] + inc` (0x140634308 → 0x14063419F).
  - Mode 1 (0x140634310–0x1406343C9): §2a.
  - Other modes: accept.
- Accept write (0x1406343D1–0x1406343E6): `FUN_140A42940(T, slot, 0x34)`; `u16[T+0xB88C+2·slot] := mode`; return cand (0x1406343EF `mov ebx,esi`; 0x1406343F1).

2a. Mode 1 (CONFIRMED, 0x140634310–0x1406343C9):
- `r = 0x558 word` if `a2a0[slot] == 0x36` (MARK) (`r = u32[T+0x558+4·slot]`); else `r = u32[T+0x620+4·slot]` if `a2a0[slot] == 0x3D`; else `r = 0xFF`. (`a2a0[slot] = u32[T+0xA2A0+4·slot]`, 0x14063407A–0x140634087.)
- If `r ≤ 21`: `FUN_140A42B90(T, slot, r)` (0x140634331–0x1406343C4).
- Else: `ev = FUN_140A3E190(E, 0)` (0x140634343–0x14063434F). `repl = FUN_140A62EA0(ctx, u64[ev+4])`. `r := 0xFF`. If `repl ≤ 21` and `side(repl) ≠ s` (`repl ≥ 11` is side 1) and `(type−1 ≤ 2 ∨ type == 16)` and `FUN_1404C8EA0(MA, cand, repl) < 30.25`, then `r := repl` (0x1406343AA `cmova`). If `r ≤ 21`, `FUN_140A42B90(T, slot, r)`.
- Then the accept write.

## 3. Return rule (CONFIRMED unless marked)

- Return the first accepted candidate in table order (k = 0..10, side block of `ref`'s list). Otherwise 0xFF.
- "keep-max" (pes-port-FarChasePress §3, pes-re-far-chase §6) is not in the code. The accept path returns at 0x1406343F1 with no running max.
- The score is in metres: `S = sqrt(d²)`, and the gate is `S ≤ θ` (smaller = closer). 30.25 = 5.5² is the mode-1 replacement d² test, not the score threshold.
- Table order is ascending d² (INF). The list writer is OPEN (O7). P10's loop (§6 H43) breaks on the first d² beyond its radius, so it assumes ascending order. Under that assumption, "first passing in table order" is "nearest passing man to the holder".

## 4. Ball-event record (CONFIRMED structure; enum INF)

- `E = u64[ctx+0xE0]`. `ev = FUN_140A3E190(E, 0) = E + 0x180698` (no type check; H15).
- `type = i32[ev+0]`; the qword at `ev+4` is `(side lo32, idx hi32)`, consumed by FUN_140A62EA0 (H17).
- The type field selects only the mode-1 replacement, through the set {1, 2, 3, 16}: `lea eax,[rdx-1]; cmp eax,2; jbe` (0x140634381–0x140634389) and `cmp edx,0x10` (0x14063438B). It does not filter the 11 candidates.
- FUN_14063FDC0 (press point) reads the same type set with a 6-frame window (H23).
- Enum names for 1, 2, 3, 16 are INF/OPEN (§8 O6). 0x37 marks an empty ring slot (H15, H16).

## 5. Writes (CONFIRMED)

- Setter FUN_140A42940(T, slot, v) (VA 0x140A42940; decompile): `i32[T+0xA2A0+4s] = v`; `FUN_140A40200(T+0xA2CC+0x34s, v, u32[T+0x21C])` (4-entry history ring); `FUN_140A85AA0(T+0x28C+0x40s, v)` (zero a 0x20-byte block, plus per-state fields). The eval calls it once, at 0x1406343DC, with v = 0x34 (`mov r8d,0x34` at 0x1406343D1).
- Target word: `u16[T+0xB88C+2·slot] = mode` (0x1406343E6, `mov word ptr [rdi+rbp*2+0xb88c], r15w`), only when slot < 11 (always).
- Release FUN_140A42B90(T, slot, v) (VA 0x140A42B90): `u32[T+0xC1C+4·slot] = v`. It does NOT write the B88C word (§7).
- The eval writes only on accept. The 0xFF paths write nothing.

## 6. Helpers (VA; inputs → return; rule; constants with VA; tag)

H1 FUN_140A48A10 (0x140A48A10): (G,pid) → `u64[G+(pid+0xBF)·0x28]` for pid < 26, else `u64[G+0x1DD8]`. The value is the man pointer stored in a 0x28-stride slot (a load, not an address). CONFIRMED.
H2 FUN_140A48910: (G,pid) → `u64[G+(pid+0xA5)·0x28]` (pid < 26) else `u64[G+0x19C8]`. Kind byte at `rec+1`. CONFIRMED.
H3 FUN_140A48A30: (G,side) → team pointer `u64[G+(side+0xA1)·0x28]` (side < 2) else `u64[G+0x1928]`. CONFIRMED.
H4 FUN_140A489F0: (G,pid) → `u64[G+(pid+0xF3)·0x28]` (pid < 22) else `u64[G+0x25F8]`. CONFIRMED.
H5 FUN_140A489D0: (G,side) → `u64[G+(side+0xA3)·0x28]` (side < 2) else `u64[G+0x1978]`. CONFIRMED.
H6 FUN_14124DAD0: (rec) → rec+0x70. CONFIRMED.
H7 FUN_140A63950 (flag query): (ctx, side, flag, pid) → 1 iff ∃ j<4 with `i32[MA+0x25CF8+4(4·side+j)] == flag` and `i32[MA+0x25D38+4(4·side+j)] == pid`; 0 if flag ≥ 0x11 or side ≥ 2. Used with pid 0xFF as "any unassigned". CONFIRMED (decompile).
H8 FUN_140A55D40 (G180, side, thr) → 1 iff ∃ i < 24 with `u8[e+6]==0`, `u32[e+0x20] < 22`, `u8[e+0xD]==0`, side(pid)==side (side = 1 iff 10 < pid < 22), and `cls_i ≤ thr`, where `e = G180+0x74i` and `cls_i = i32[G180+0x10E8+4i]`, or `u8[e+0x68]` when that word == 4. Eval uses thr = 2. CONFIRMED (decompile).
H9 FUN_140A40BC0: (T) → `u32[T+0xA298]`. CONFIRMED.
H10 FUN_140A40BD0: (T,out) → `out = 0` if `u8[T+0xA29C] ≠ 0`, else `out = i32[T+0xA294]`. CONFIRMED.
H11 FUN_140A60340: (p) → `i32[p] / 100` (C truncation). CONFIRMED. CLS = H10 then H11.
H12 FUN_140A602B0: (out, v) → `out = clamp(100·v, 0, 700)`. CONFIRMED.
H13 FUN_141E532E0 / FUN_141E532D0: () → int `DAT_14351AE50` (on disk 54) / float `DAT_14351AE54` (on disk 54.0). The on-disk fps; the live value is OPEN (O5). CONFIRMED read.
H14 (K in the eval) = H13 int. `4K`, `5·fps`, `fps/10` windows use it.
H15 FUN_140A3E190 (E,k): k = 0 → `E+0x180698` (no check). k ≥ 1 → if k < `u32[E+0x1900]`, the ring-0 record k back (800-byte slots, index `(u32[E+0x1904]−k) mod 8`) + 0x14; if its type is 0x37 → `&DAT_1436F3D60`. Else `&DAT_1436F3A40`. CONFIRMED (decompile).
H16 FUN_140A3E150 (E,k,m): k < 10: ring k (stride 0x1908), m-th most recent 800-byte record if `m < u32[+0x1900]`, else `&DAT_1436F3A40`. CONFIRMED (decompile). FUN_140A74EC0 uses k=5, m=0 (asm 0x140A74EDB–0x140A74EE2).
H17 FUN_140A62EA0 (ctx, q): if `lo32(q) < 2` → `FUN_140A3B320(FUN_140A489D0(G, lo32 q), hi32 q)`, else 0xFF. CONFIRMED.
H18 FUN_140A3B320 (obj, id): scan `u32 obj[1..40]` for id. At index j: j ≤ 10 → `u32[obj]·11 + j`, else 0xFF; not found → 0xFF. CONFIRMED.
H19 FUN_140A627E0 (ctx, 0): → `u32[MA+0x2601C]` (the stored holder/possession man). The param ≠ 0 branch (FUN_140A3E7D0) is not used here. CONFIRMED.
H20 FUN_140A62960 (ctx, side): → `side·11 + j` for the first j ≤ 10 with `u32[T_side+0xA2A0+4j] == 0x3A` (MATCH_UP); else 0xFF. CONFIRMED.
H21 FUN_140A62690 (ctx, side): → `side·11 + k` with `k = FUN_140A62610(side, MI, T_side)`, where k is the first j ≤ 10 with `u8[MI+0x1748+side·11+j] ≠ 0` and `u32[T_side+0x4C00+4j] == 0`; else 0xFF. CONFIRMED (decompile).
H22 FUN_140A74EC0 (E, MI): → `u32[MI+0x119C]` iff `u32[MI+0x116C] == 1`, `FUN_140A3E150(E,5,0)` has type ≠ 0x37, and `u32[+0x18] == 1`; else 0xFFFFFFFF. Frame units INF. CONFIRMED (asm 0x140A74EC0–0x140A74EF2).
H23 FUN_14063FDC0 (press point) (out, ctx) → point. (a) `c = FUN_140A627E0(ctx,0)`; if c ≥ 22, c = `u32[MA+0x2483C]`. If c < 22, `FUN_140A6E340(kind(c))` and `fps/10 < u16[rec(c)+0x1A]`: point = FUN_140A57800(ball) + FUN_140A8F7C0(rec(c)+0x11C, kind==9 ? 30 : 10) (lead). (b) Else if `type ∈ {1,2,3,16}` and `u32[MI+0x1198] − u32[ev+0x10] < 6`: point = `(ball obj +0x27A0, +0x27A8)`. (c) Else the team point `MA+0x2482C / MA+0x24834`. CONFIRMED structure. OPEN: the ball point (O2), the lead, and the meaning of `ball+0x27A0`.
H24 FUN_140A6E340 (kind): → `kind ≤ 0x5D and (u8[0x1426484F2 + 4·kind] & 1)`. Bit set for kinds 8, 9, 10 among 0..16. CONFIRMED (table bytes; meaning OPEN).
H25 FUN_140A4DB20 (out, sgn): → `out = 270.0f` (imm 0x43870000), or `90.0f` (imm 0x42B40000) if `sgn == −1`. CONFIRMED.
H26 FUN_140A4C720 (a, b, s, k) → `s·b > s·a + k` (a, b f32; s i8; k f32). CONFIRMED (decompile; the 2-arg and 4-arg call sites are in §10 N line).
H27 FUN_140A4C6E0 (a, b, s) → FUN_140A4C720(a, b, s, 0). Eval: a = ref.x, b = cand.x. CONFIRMED.
H28 FUN_140439E50 (out, a, b) → heading of (b − a) in degrees ∈ [0,360), zero vector → 0. Arithmetic: `atan2(dx,dz)` in rad, +2π if negative (`DAT_1425795E4`), ×57.2958 (`DAT_14259BE40`), wrap at 360 (`DAT_14259858C`). CONFIRMED arithmetic; the atan2 argument order INF.
H29 FUN_140439EF0 (a, b) → heading (b − a) unwrapped, returned, unused by the eval. CONFIRMED.
H30 FUN_14043A020 (f, vec) → `|wrap180(vec − wrap360(f))|`. Wrap: ±360 when |d| > 180 (`DAT_142598580`, `DAT_142588218`, `DAT_14256AE1C`). CONFIRMED.
H31 FUN_14043A0A0 (a, b) → signed `wrap180(b − a)`. CONFIRMED.
H32 FUN_140A92D40 (y, x) → atan2 in radians. Quadrant constants: ±π (`0x142609F98`, `0x142578480`), ±π/2 (`0x14260B394`, `0x1425A35A0`). Calls FUN_140A92DC0 for atan(y/x). CONFIRMED structure.
H33 FUN_140A92DC0 (x) → atan(x) approximation. Constants 0.785398 (π/4), 0.98279, 1/3, 1.5. INF on the exact polynomial.
H34 FUN_140477130 (rec) → speed = `(fps·60·60/1000)·|v|`, with `v = FUN_140A6C6D0(rec)` (x, y, z at rec+0x574/0x578/0x57C), `|v|` via sqrt (H37). Constants 60 (`0x14259A764`), 1000 (`0x14256AE34`). Units (km/h if v is m/frame) INF.
H35 FUN_140458740 (rec, t) → heading of `FUN_140A6C6D0(rec,t)` (x,z); 0 if both zero. CONFIRMED (structure); arg order INF as H28.
H36 FUN_14043B8B0 (MA, a, b) → `FUN_1415A1780(d²(a,b))` (tail call), i.e. S = sqrt(d²); 0 if b > 21. CONFIRMED structure (asm 0x14043B8D1–0x14043B8DA).
H37 FUN_1415A1780 → `jmp [0x1425301F0]`. The pointer is runtime-bound (on-disk word 0xFEC86620). Identified as sqrt by use (S and speed) — INF (O4).
H38 FUN_1404C8EA0 (MA, a, b) → f32 d²(a,b): base = `MA+0x22E4C+200·a` (a < 22, else `MA+0x22E4C`); if b > 21 → 0; else `f32[base + 4 + 8·(u8[base+0xB2+b] + 11·side(b))]`, `side(b) = (b ≥ 11)`. CONFIRMED (asm 0x1404C8EA0–0x1404C8EEE, `movss xmm0, [r9+rcx*8+4]`).
H39 FUN_140A6C6D0 (rec, out, t): normal branch (not `i32[rec+0x5B0]==1`, and not (`==5` ∧ `x²+z² < t²`)): `out = Vec3 at rec+0x574` (x @0x574, y @0x578, z @0x57C). Else `out = FUN_140A8F7C0(0, rec+0x554, t)`. Normal branch CONFIRMED; facing branch OPEN (O3).
H40 FUN_140A8F7C0 (out, θ, t): `out.x += S(θ,t)`, `out.z += S(θ+90, t)`, `S = FUN_140A92C80`. OPEN (O3).
H41 FUN_140A92C80: sin-like table (512 steps via DAT_1426550E8; wraps DAT_142598598 = −360, DAT_1425DDFFC = −180, 90 = DAT_14259BE5C). Decompile lost its return. OPEN (O3).
H42 FUN_140633350 (refusal) (P, T, cand, mode) → 1 = refuse, 0 = not refused. Asm 0x140633350–0x14063370A (0x14063370B is padding; 0x140633710 is FUN_140633710, not this function). CONFIRMED.
  - `slot_c = cand % 11` (cand < 22; else 0); `pm = u32[T+0x558+4·slot_c]`; `pm > 21` → 0.
  - `F13 = FUN_140A63950(ctx, s, 13, 0xFF)` ≠ 0 → 0.
  - mode 1, 3, 4 (0x1406336DF–0x1406336FE: flag-14 call with 0xE at 0x1406336EE, `cmp byte ptr [rsi+rbp+0xb8c4],al` at 0x1406336F7, `je` at 0x1406336FE): refuse unless `¬flag14(s,pm) ∧ u8[T+0xB8C4+slot_c] == 0`.
  - mode 5, 6 → 0. Mode 0, 2, 7+: if `flag14(s,pm)` → 1. Else:
    - `P0 = FUN_14063FDC0(ctx)`; if `|cand − P0|² < 16` (`DAT_14259BE10` = 16, 0x140633445–0x140633450) → 0.
    - `u8[T+0xB8C4+slot_c] ≠ 0` → 1.
    - `partner = FUN_140A48A10(G,pm)`; `sg = i8[T+0x254]`; `L = f32[MA+0x40+0x14·s]` (0x1406334B5, `rdx = 5·s`).
    - `¬FUN_140A4C720(L, partner.x, sg, 6.0)` → 1 (0x140633497 `DAT_14259BDEC`, 0x1406334BE).
    - If `sg·partner.x ≤ sg·L + 20.0` (0x140633509 `DAT_14259BE1C`, 0x140633514 `ja`) and `¬FUN_140A4C720(P0.x, partner.x, sg, 10.0)` (0x140633516 `DAT_142588210`, 0x14063352B): `dir = FUN_140A4DB20(sg)`; `iMU = FUN_140A62960(ctx,s)`; `iFree = FUN_140A62690(ctx,s)`; `R = |L − partner.x| + 5.0`. For k = 0..10 (`j = s·11+k`): reject-candidate-set: if `j < 22 ∧ active(j) ∧ j ≠ cand ∧ j ≠ iMU ∧ j ≠ iFree ∧ T.u32[0xA2A0+4k] == 0 ∧ kind(j) ∉ {0x11,0x14,0x50,0x52,0x57} ∧ FUN_140A53DA0(ctx, j, partner.pos, dir, 50.0, R, 0)` → return 0. No such j → 1.
    - Otherwise → 0.
  - Constants: 16.0 (`0x14259BE10`), 20.0 (`0x14259BE1C`), 6.0 (`0x14259BDEC`), 10.0 (`0x142588210`), 5.0 (`0x14259222C`), 50.0 cone (`0x14259BE34`) (cone/2 used inside H44).
  - Tag: CONFIRMED for the arms and constants; the sub-helper meanings are in H23, H21, H20, H25, H44.
H43 FUN_140A51D50 = P10 (ctx, o), o = 1 − s. Partly decoded; tail OPEN (O1). CONFIRMED steps:
  - `h = u32[MA+0x2601C]` (H19). `c = h` if h < 22, else `u32[T_o+0x78]` (T_o = FUN_140A48A30(G,o)). `c ≥ 22` → 0.
  - `N = trunc(0.1·fps + 0.5)` (`DAT_142588200`, `DAT_142565600`), predicted carrier point `p_c = pos(c) + N·v(c)` (rec+0x55C, +0x574).
  - If h ≥ 22: `p_b` = FUN_140A6CF70(ball, N) (OPEN); `|p_c − p_b|² > 100` (`DAT_14253EE30`) → 0.
  - If `i32[T_self+0xB8A4] > 2` and `D_ev < 5·fps` → 0.
  - `δ = wrap180(norm360(f32[T_o+0x250]) − f32[c+0x554])`. If `|δ| > 85` (`DAT_14259BE58`) and not (h ≥ 22 ∧ `T_self.b8A4 < 3`) → 0.
  - Scan the side-`s` block of `c`'s list (k = 0..10), radius `R = 6 + 4·ramp` (`DAT_14259BDEC`, `DAT_1425656C0`, `DAT_142565604`, `DAT_142565600`), break on d² > R² (sorted ascending). A man with `T_self.a2A0 ∈ {5, 0x35, 0x3A}` (mask `0x420000000000020`) whose N-frame point is within 60° of the opponent direction from `p_c` → 0. Tail: FUN_140456EA0 and a d² compare (OPEN). Else 1.
  - Tag: CONFIRMED to the tail. OPEN: FUN_140456EA0, FUN_140A6CF70.
H44 FUN_140A53DA0 (ctx, j, pos, dir, cone, R, Rmin) → FUN_140A53DF0(ctx, j.pos, pos, dir, cone, R, Rmin) (thunk; the decompile drops the stack args). INF on the forwarding.
  FUN_140A53DF0 (0x140A53DF0): true iff `Rmin² < |A−B|² < R²` (A = j.pos, B = pos) and `|wrap180(heading − dir)| < cone·0.5` (`DAT_142565600`). Asm head 0x140A53DF0–0x140A53E3C checked. The heading's direction (B−A or A−B) is INF.
H45 FUN_140A42940 / FUN_140A40200 / FUN_140A85AA0 / FUN_140A42B90: see §5. CONFIRMED (decompile; press-life §1 agrees).
H46 FUN_140A627E0 / FUN_140A62960 / FUN_140A62690: §6 H19–H21.
H47 FUN_140A57800 → FUN_140A6CF70 (ball predicted point, sample k). OPEN (O2). Doc: "predicted point sample 0" in pes-re-live-farchase.md §method.

## 7. Corrections to the docs (for Main)

1. "keep-max" (pes-port-FarChasePress §3; pes-re-far-chase §6) is wrong. The eval returns the first passing man in table order (§3).
2. "scored by sub_1404C8EA0 against dword_1425C73A4" (pes-port §4 / §5.1): the score is S = sqrt(FUN_1404C8EA0) via FUN_14043B8B0 against θ (§2). DAT_1425C73A4 = 30.25 is only the mode-1 replacement test.
3. "releases go through sub_140A42B90 ... clearing the target word T+0xB88C + 2·slot" (pes-re-far-chase §6): FUN_140A42B90 writes `u32[T+0xC1C+4·slot]`. Only the eval writes the B88C word, and it writes the mode there (§5).
4. press-life §… lists "FUN_140634470/FUN_140633940 | 0x34 PRESS / 0x35 SAND overwrites": FUN_140633940 writes only 0x34 (0x1406343D1). No 0x35 store in its listing (grep of the whole function).
5. run-targets-b2: `lineOpp = MA+0x44+0x14·opp`. Decode: eval reads f32 at `MA+0x40+0x14·(s==0)` (0x140633B30) and refusal at `MA+0x40+0x14·s` (0x1406334B5). The 4-byte difference is unresolved (INF which is right).
6. pes-re-live-farchase.md (method): `pl(i) = G+(0xBF+i)·0x28` with pos at `pl+0x8`. FUN_140A48A10 returns the qword stored at that slot, so the man object is one dereference away (H1). INF on what the live capture actually read; Main should check.
7. Same concern for `T_s = G+(0xA1+s)·0x28` (live doc): FUN_140A48A30 returns the qword stored there (H3).

## 8. OPEN

- O1 P10 tail (FUN_140456EA0 and the final d² compare); exact use of DAT_1425656C0 in the ramp.
- O2 Ball predicted point FUN_140A57800 / FUN_140A6CF70 (used by P10 and H23).
- O3 FUN_140A92C80 (sin-like table) and therefore the facing branch of FUN_140A6C6D0 / FUN_140A8F7C0 (affects spd and heading of refs with `rec+0x5B0 == 1`, or `== 5` and slow).
- O4 FUN_1415A1780 identity (runtime-bound jmp). sqrt is INF from use.
- O5 Runtime fps: DAT_14351AE50/54 are 54 on disk. The GF skill notes say some .data sim-step slots are runtime-written. Live value unverified.
- O6 Field meanings: flags 13/14/15 (MA+0x25CF8/0x25D38 table); MI+0x14C8/+0x14CC (live doc: kickoff type, constant); T+0xA53C (live doc: press timer); T+0xA544; T+0xB8C4[slot]; T+0x87D0 (role); T+0x4C00; T+0xB748 (0x24); T+0x558 / +0x620 / +0xC1C; T+0x4B4C; G+0x180 (24 × 0x74 list); MI+0x1748; ball event type enum {1,2,3,16} and 0x37 (empty); `LR` role mapping.
- O7 Nearest list (MA+0x22E4C, 200 B per holder, 2 × 11 entries of {id, d²} + rank bytes at +0xB2): writer OPEN; ascending order INF (P10 break supports it).
- O8 Class arms: 5 and 6 get no extra test (refusal = 0). The coordinator's class meanings are its own.
- O9 Sticky W (§2): intent unknown. First candidate that reaches the D_ev test changes θ, G_sub and accept-2 for later candidates.
- O10 FUN_140A6E340 kind table meaning (kinds 8, 9, 10 set).
- O11 FUN_140A53DF0 heading direction (B−A vs A−B) and the 25° half-cone test's meaning.
- O12 Kickoff-type reading of MI+0x14C8 (INF).

## 9. Notes for PressCoordRE / Main

- Return contract matches the coordinator's use: pid 0..21 or 0xFF (§1). The eval performs writes on accept, so the coordinator should not repeat the setter.
- FUN_140633350 (refusal) is in this report (H42). FUN_140A51D50 (P10) is in H43 with OPEN tail.

## 10. Machine-checked claims

Every line below is re-verified against the bytes by `check_eval.py` (C = constant value at VA; I = instruction text at VA; R = instruction at VA references the operand VA; N = count of instructions in [lo,hi) containing the needle). Constants are read from `pes_full.bin`, so they are script output, not typed.

```claims
C 0x14259222c f32 5.0
C 0x142588218 f32 -1.0
C 0x142588210 f32 10.0
C 0x14256ae1c f32 1.0
C 0x14259858c f32 360.0
C 0x142598580 f32 180.0
C 0x142565600 f32 0.5
C 0x14253ee30 f32 100.0
C 0x14259be0c f32 15.0
C 0x1425c48c0 f32 22.0
C 0x1425c5948 f32 13.0
C 0x14258c264 f32 3.0
C 0x14259a764 f32 60.0
C 0x14259be04 f32 12.0
C 0x14259bdec f32 6.0
C 0x14259be34 f32 50.0
C 0x14259be2c f32 40.0
C 0x14259bdd0 f32 0.6944444179534912
C 0x1425c73a4 f32 30.25
C 0x142565d30 u64 0x7fffffffffffffff
C 0x14351ae50 u32 0x36
C 0x14351ae54 f32 54.0
C 0x14259be10 f32 16.0
C 0x14259be1c f32 20.0
C 0x14259be5c f32 90.0
C 0x14259be58 f32 85.0
C 0x142565604 f32 2.0
C 0x1425656c0 f32 4.0
C 0x142588200 f32 0.10000000149011612
C 0x14256ae34 f32 1000.0
C 0x1425795e4 f32 6.2831854820251465
C 0x14259be40 f32 57.2957763671875
C 0x142598598 f32 -360.0
C 0x142591140 f32 30.0
C 0x142648512 u8 0x01
C 0x142648516 u8 0x01
C 0x14264851a u8 0x01
I 0x140633940 mov dword ptr [rsp + 0x20], r9d
I 0x140633a53 call 0x140a63950
I 0x140633b30 mulss xmm1, dword ptr [rcx + rdx*4 + 0x40]
I 0x140633b36 comiss xmm1, xmm0
I 0x140633b39 ja 0x1406343f1
I 0x140633bff cmp esi, 0x15
I 0x140633c02 ja 0x1406341cf
I 0x140633c1e cmp byte ptr [rsi + rdx + 0x1748], 0
I 0x140633c35 cmp esi, r15d
I 0x140633c38 je 0x1406341cf
I 0x140633c5d cmp dword ptr [rcx + r8 + 0x20], esi
I 0x140633c7c cmp byte ptr [rcx + r8 + 0xd], 0
I 0x140633c9b movsx ecx, byte ptr [rax + 1]
I 0x140633cc5 cmp ebx, 0xb
I 0x140633cdc test ecx, ecx
I 0x140633cf4 call 0x140a63950
I 0x140633d04 cmp dword ptr [rdi + r13*4 + 0xb748], 0x24
I 0x140633d0d je 0x1406341ad
I 0x140633d17 cmp esi, dword ptr [rsp + 0x1b0]
I 0x140633d29 call 0x140a40bc0
I 0x140633d56 call 0x140a74ec0
I 0x140633d5d call 0x141e532e0
I 0x140633d65 cmp ebx, eax
I 0x140633d67 jae 0x140633da1
I 0x140633d88 cmp dword ptr [r8 + rcx*4 + 0x14c8], 1
I 0x140633d9d mov dword ptr [rsp + 0x38], ebp
I 0x1406341e1 mov ebp, dword ptr [rsp + 0x38]
I 0x140633dcf cmp dword ptr [rsp + 0x1b8], 4
I 0x140633dd7 jne 0x140633e1a
I 0x140633e18 je 0x140633e09
I 0x140633e29 call 0x14043b8b0
I 0x140633e2e movzx ebp, byte ptr [rdi + 0xa53c]
I 0x140633e82 cmp esi, dword ptr [rsp + 0x1b0]
I 0x140633ec5 call 0x140a51d50
I 0x140633ecc jne 0x140633f4b
I 0x140633ed3 call 0x140477130
I 0x140633ee4 call 0x141e532d0
I 0x140633f3d comiss xmm12, xmm7
I 0x140633f41 mov dil, 1
I 0x140633f4f ja 0x140634035
I 0x140633f55 mov ecx, dword ptr [rsp + 0x38]
I 0x140633f5b jne 0x140633fa8
I 0x140633f7c jb 0x140633fa4
I 0x140633f97 call 0x140a4c6e0
I 0x140633fb0 cmp dword ptr [rax + 0xb8a4], 3
I 0x140633ff1 comiss xmm8, xmm7
I 0x140633ff5 jbe 0x14063404d
I 0x140634030 call 0x140439ef0
I 0x140634071 call 0x140633350
I 0x140634078 jne 0x140634045
I 0x140634094 cmp r15d, 7
I 0x1406340a6 cmp ecx, dword ptr [rsp + 0x3c]
I 0x1406340bf cmp eax, 0x3c
I 0x1406341a5 mov r15d, dword ptr [rsp + 0x1a0]
I 0x1406341ea cmp r15d, 1
I 0x1406341f4 cmp r15d, 4
I 0x140634331 cmp r15d, 0x15
I 0x140634335 jbe 0x1406343bc
I 0x14063439a call 0x1404c8ea0
I 0x1406343aa cmova r15d, ebx
I 0x1406343c4 call 0x140a42b90
I 0x1406343d1 mov r8d, 0x34
I 0x1406343dc call 0x140a42940
I 0x1406343e6 mov word ptr [rdi + rbp*2 + 0xb88c], r15w
I 0x1406343f1 mov eax, ebx
I 0x1406343f5 mov eax, 0xff
R 0x140633b3f 0x1436f3dc8
R 0x140633b7f 0x142565d30
R 0x140633b8b 0x14259222c
R 0x140633b96 0x142588210
R 0x140633ba9 0x142588218
R 0x140633bb6 0x14256ae1c
R 0x140633bbf 0x14259858c
R 0x140633e49 0x1425c48c0
R 0x140633e5a 0x1425c48c0
R 0x140633e7a 0x1425c5948
R 0x140633e6b 0x14259be0c
R 0x140633e8b 0x14258c264
R 0x140633ee9 0x14259bdd0
R 0x140633f28 0x14259a764
R 0x140633f6c 0x142565600
R 0x140633fb9 0x14259be04
R 0x140633fdf 0x14259bdec
R 0x140633fe9 0x14259be04
R 0x140634157 0x142598580
R 0x14063417b 0x14259be34
R 0x140634194 0x14259be2c
R 0x14063439f 0x1425c73a4
R 0x1406342c2 0x14253ee30
N 0x140633940 0x140634466 call 0x140a42940 | 1
N 0x140633940 0x140634466 call 0x140a42b90 | 1
N 0x140633940 0x140634466 call 0x140633350 | 1
N 0x140633940 0x140634466 call 0x140a51d50 | 1
N 0x140633940 0x140634466 mov eax, 0xff | 1
N 0x140633940 0x140634466 mov r8d, 0x34 | 1
N 0x140633940 0x140634466 call 0x140a74ec0 | 1
I 0x140633350 push rbx
I 0x140633445 movss xmm0, dword ptr [rip + 0x1f689c3]
I 0x140633450 ja 0x1406333a6
I 0x140633456 cmp byte ptr [rsi + rbp + 0xb8c4], 0
I 0x140633497 movss xmm3, dword ptr [rip + 0x1f6894d]
I 0x1406334b5 movss xmm6, dword ptr [rcx + rdx*4 + 0x40]
I 0x1406334be call 0x140a4c720
I 0x1406334eb movss xmm2, dword ptr [r13 + 0x55c]
I 0x140633509 addss xmm0, dword ptr [rip + 0x1f6890b]
I 0x140633514 ja 0x140633534
I 0x140633516 movss xmm3, dword ptr [rip + 0x1f54cf2]
I 0x14063352b call 0x140a4c720
I 0x140633687 call 0x140a53da0
R 0x140633445 0x14259be10
R 0x140633497 0x14259bdec
R 0x140633509 0x14259be1c
R 0x140633516 0x142588210
N 0x140633350 0x14063370b call 0x140a4c720 | 2
```

Checker: `<workdrive>/gf-PressEval-scratch/check_eval.py` (self-test with four in-memory mutations: a constant, an instruction text, a rip target, a count). Summary line is printed by the run.
