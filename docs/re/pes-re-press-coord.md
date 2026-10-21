# Press coordinator RE: FUN_140634470 (BallPlayer_PressingCoordinator), its gate, holder, class and maintain (PressCoordRE)

> **Main note 22-10-26:** PressCoordRE, checker clean (30 constants, 42 asm rows, 6/6 mutations); not yet Main-audited. Key finding: the press holder MA+0x2601C is the LAST TOUCHER (FUN_140457070 re-derives it from the newest K=8 touch key; nothing clears it on loss), which conflicts with pes-port-FirstTouch.md's "PES loose = no carrier": a live read settles it.

Scope: read-only static decode of the coordinator and every helper it needs from the press-coordinator layer. Image `pes_full.bin`, flat (file offset = VA − 0x140000000; the section table is not used). Tags: CONFIRMED = decompile re-read this pass and, where the branch or argument matters, checked against the asm (checker, §13). INF = inference. OPEN = not resolved (§12). The target eval FUN_140633940 and its helpers FUN_140633350, FUN_140A51D50 and FUN_14043B8B0 belong to PressEvalRE (`pes-re-press-eval.md`); this report uses only their call and return contract (§10).

## 0. Constants (script output: each VA read from the image; checker re-reads them)

| VA | u32 | f32 | used in |
|---|---|---|---|
| 0x14253EE30 | 0x42C80000 | 100.0 | class 4 cone (50°) |
| 0x142565600 | 0x3F000000 | 0.5 | arrival-race tolerance factor |
| 0x142565D30 | mask | abs | double abs mask (low dword 0xFFFFFFFF, high 0x7FFFFFFF) |
| 0x14256AE1C | 0x3F800000 | 1.0 | angle fold sign |
| 0x142588210 | 0x41200000 | 10.0 | class 4 outer radius; coordinator clause |
| 0x142588218 | 0xBF800000 | -1.0 | angle fold sign |
| 0x14258C264 | 0x40400000 | 3.0 | class 5 distance; score test in FUN_140633710 |
| 0x14259222C | 0x40A00000 | 5.0 | class 5 kind-10 distance; class 4 width band |
| 0x142598580 | 0x43340000 | 180.0 | angle fold |
| 0x14259858C | 0x43B40000 | 360.0 | angle wrap |
| 0x142598598 | 0xC3B40000 | -360.0 | angle wrap |
| 0x14259A764 | 0x42700000 | 60.0 | gate facing limit |
| 0x14259BDC4 | 0x34000000 | 1.1920928955078125e-07 | FLT_EPS in FUN_140A604D0 |
| 0x14259BDDC | 0x3FC00000 | 1.5 | ball height and lift threshold |
| 0x14259BDE4 | 0x40200000 | 2.5 | FUN_140632C70 near band |
| 0x14259BDFC | 0x41300000 | 11.0 | FUN_140632C70 outer radius |
| 0x14259BE0C | 0x41700000 | 15.0 | class 4 second radius |
| 0x14259BE20 | 0x41C80000 | 25.0 | maintain: carrier ramp end |
| 0x14259BE30 | 0x42340000 | 45.0 | maintain: bearing band (reason 4/7) |
| 0x14259BE34 | 0x42480000 | 50.0 | FUN_140633710 table gate |
| 0x14259BE38 | 0x425C0000 | 55.0 | FUN_140633710 lane gate |
| 0x14259BE40 | 0x42652EE0 | 57.2957763671875 | rad to deg (FUN_140439EF0, FUN_140439E50) |
| 0x14259BE4C | 0x428C0000 | 70.0 | class 1 bearing band (FUN_140632320) |
| 0x14259BE5C | 0x42B40000 | 90.0 | class 4 second cone (45°) |
| 0x14259BE64 | 0x430C0000 | 140.0 | FUN_140632C70 cone (70°) |
| 0x14259D60C | 0x41100000 | 9.0 | maintain: reason 5 distance² (3 m) |
| 0x1425795E4 | 0x40C90FDB | 6.2831854820251465 | 2π (FUN_140439EF0) |
| 0x1425A2670 | 0x43C80000 | 400.0 | FUN_140632C70: 20 m² test |
| 0x1425C48B8 | 0x41600000 | 14.0 | FUN_140632C70 far band |
| 0x14351AE50 | int | 54 | fps = FUN_141E532E0() (the docs' gs) |

Not image constants: G = `*(0x1436F3DC8)` (runtime pointer; the on-disk value is not used). Immediates in FUN_140A4DB20 (0x43870000 = 270.0, 0x42B40000 = 90.0) and FUN_140A4F210 (same two, swapped): checker tests the bytes in their bodies.

## 1. Fields used

- PP = press state `uint[3]` (param_1): [0] holder H (0..21, 0xFF none); [1] X = T+0x7C, copied only when H < 22; [2] second presser man (0..21, 0xFF none).
- SS = side struct `longlong[3]` (param_2): [0] ctx; [2] side (0 or 1).
- T = team record = `*(G+(0xA1+side)*0x28)`. The slot `G+(0xA1+s)*0x28` holds a pointer (FUN_140A48A30). Live notes that read `T_s+0x…` at the slot address are wrong (§11.2).
- MI = `*(G+0x270)` (asm, §11.1). B = `*(G+0x40)` (ball). F = `*(G+0x2AA8)` (F+0x18 = penalty-area line X, D11; F+0x14, F+0x1C runtime).
- ctx+0x18 = MA. MA+0x2601C = carrier (last-touch man, §4). MA+0x2482C/+0x24834 = team point TP (x, z). NearestList row of carrier c = MA+0x22E4C+c·200: two sides × 11 entries of {u32 id, f32 d²} (side 1 starts 88 bytes in); rank byte at +0xB2+man.
- T fields: +0x78 last-kick id (live: writer exists, VA open; `pes-re-foundation-verify.md` V1); +0x7C = X (MATCH_UP writer sets it to the man idx, `pes-re-defensive-shape.md`; INF here); +0x250 attack bearing (f32); +0x254 attack sign s (int8); +0x4B10 goal-kick man; +0xA53C press-timer byte; +0xA294 level (+0xA29C flag); +0xA298 phase word (3 = aggressive); +0xA2A0[k] assignment word (0x34 PRESS, 0x35 SAND, 0x36 MARK, 0x3A MATCH_UP); +0xA2CC+k·0x34 state ring; +0xB774[k] SAND request word; +0xB88C[k] u16 reason (2, 3, 4, 5, 6, 7); +0xB8A4 attack level; +0xB8C4[k] byte; +0xC1C[k] man-target word (u32; 0xFF none; written only by FUN_140A42B90; read as `tgt` by FUN_140633710); +0x87D0+8k u32 slot flag; +0x8E08 table (FUN_140A62180); +0x4BD4[k] role flag (0 = keeper); +0x6EC heading mode (INF); +0x21C frame stamp (INF).
- MI fields: +0x116C state (1 = in play); +0x1198 frame; +0x14C8[side] possession-change flag (INF); +0x1748[man] on-pitch byte (INF).

## 2. Coordinator FUN_140634470 (CONFIRMED structure; branch asm checked)

Signature `FUN_140634470(PP, SS, T)`. Caller: press pass (docs: per team tick); not re-traced here. One side per call.

1. Level flag: `FUN_140A604D0(FUN_140A40BD0(T), 5)` must hold (asm 0x14063449A–0x1406344AE). FUN_140A40BD0 = T+0xA294 unless T+0xA29C ≠ 0; FUN_140A604D0 = FUN_140A603C0(level, 5) > FLT_EPS. Then `FUN_140CA1A50(0xF)`, which is `return 1` (asm 0x1406344B4–0x1406344C0). Else exit.
2. Reason clear: T.B88C[k] = 0 for every k except the slot of PP[2] (when PP[2] < 22).
3. Existing second presser (PP[2] < 22), k = PP[2] % 11. Call `FUN_140632E70(PP, SS, T, k)` (maintain, §7; asm 0x140634553):
   - KEEP (0): `FUN_140A42940(T, k, 0x34)` (assignment = PRESS; pushes the state ring). Then if `FUN_140633710(PP, SS, T, k)` (asm 0x1406345BE): `FUN_140A42B90(T, k, 0xFF)` (target = none).
   - RELEASE (1): if k ≥ 11 or T.A2A0[k] ≠ 0x3A (asm 0x140634573): `FUN_140A40820(T, k)` (slot reset). `FUN_140A42B90(T, k, 0xFF)` (C1C[k] = none). If k < 11: direct store B88C[k] = SI (asm 0x14063458B–0x140634590; SI is 0 per decompile). PP[2] = 0xFF.
4. Holder: `PP[0] = FUN_140632BC0(side, ctx)` (§4; asm 0x1406345DE). PP[0] > 21 → exit.
5. PP[0] < 22: PP[1] = T+0x7C is written first, then MI.state == 1 is required (asm 0x1406345F6–0x14063460B). Else exit.
6. `FUN_140A53F90(ctx, &0xFF) == 0` required (keeper hold, §8.7; asm 0x140634627). Else exit.
7. class = `FUN_140632AE0(SS, T, PP[0])` (§6; asm 0x140634642). Require (class == 5 or PP[1] < 22) and `FUN_140632780(SS, T, PP[1], PP[0], class) != 0` (gate, §3; asm 0x14063466C). Else exit.
8. Ball: high = (B+4 > 1.5), with B+4 replaced by `FUN_140A57800(B)+4` when B+4 ≤ 0 (asm 0x1406346AC). The designation block runs only when not high and B+0x18 ≤ 1.5 (grounded). Else exit.
9. Aggressive flag A (used by class 7 and by the dispatch):
   - A = (FUN_140A62180(ctx, side, 4) == 0 and s·V.x > 0), V = `FUN_14063FDC0(ctx)` (first float; INF).
   - A |= (s·TP.x < 10.0 − F+0x18 and |TP.z| < F+0x14).
   - A |= (T.A53C ≠ 0) or (FUN_140A40BC0(T) == 3).
   - A2 = (FUN_140A63950(ctx, side, 0xD, 0xFF) ? 1 : A); A2 |= FUN_140A63950(ctx, side, 0xE, PP[0]); A = (T.B8A4 < 3) ? A2 : 1.
   - A |= (FUN_140A60340(FUN_140A40BD0(T)) == 6).
   - A |= (FUN_140A74EC0(*(ctx+0xE0), MI) < 4·fps and MI.14C8[opp] == 1).
10. Dispatch:
   - class 5: c = MA+0x2601C; if c > 21: c = T_opp.+0x78. Row = NearestList row of c (row 0 when c > 21). cand = side-row entry 0 id; if cand == MU (FUN_140A62960(ctx, side)), cand = entry 1 id. If cand < 22: k = cand % 11; `FUN_140A42940(T, k, 0x34)`; B88C[k] = 5; PP[2] = cand. No distance or kind test here (asm 0x140634911–0x140634A54; §11.4).
   - class 6: for each k with T.B774[k] == 0x35: `FUN_140A42940(T, k, 0x35)`; B88C[k] = 6.
   - class 3: unreachable (§6; §11.3).
   - otherwise (class 4 and 1; class 7 only when A): `PP[2] = FUN_140633940(T, SS, PP[1], PP[0], PP[2], class, A)` (asm 0x140634A84; PressEvalRE §10).

## 3. Gate FUN_140632780(SS, T, X, H, class) → bool (CONFIRMED; asm 0x140632780–0x14063284E)

- FAIL if B+4 > 1.5 (raw B+4, no predicted value), or T+0x78 < 22 (asm 0x1406327BE–0x1406327C2).
- PASS if T+0xA53C ≠ 0 (0x1406327C4).
- PASS if `FUN_140A63950(ctx, side, 0xD, 0xFF)` (0x1406327DF).
- PASS if T+0xB8A4 ≥ 3 (0x1406327E8).
- PASS if class == 5 (0x1406327F1).
- r = `FUN_140632C70(SS, T, X, H)` (0x140632804). r ≠ 0 → PASS.
- ang = `FUN_140481B00(&H+0x554, T+0x250)` (0x14063282A). ang > 60.0 → FAIL. Else PASS.
- Constants: 1.5 (0x14259BDDC), 60.0 (0x14259A764).

## 4. Holder FUN_140632BC0(side, ctx) → man or 0xFF (CONFIRMED)

1. c = MA+0x2601C (`FUN_140A627E0(ctx, 0)`). c < 22 → return c.
2. o = T_opp+0x78 (`FUN_140A48A30(G, side == 0)`). o < 22 and kind(o) ∉ {0x11, 0x14, 0x50, 0x52, 0x57} → return o. kind(o) = byte +1 of `FUN_140A48910(G, o)` (base G+(o+0xA5)·0x28; INF which object, §11.8). The mask 0x8000000000000009 >> (kind−0x11) has bits 0, 3 and 63, i.e. kinds 0x11, 0x14, 0x50.
3. Return 0xFF.

### 4a. Writers of MA+0x2601C (CONFIRMED: flat scan for disp32 0x2601C, capstone decode; checker §13)

- 21 operands in total: 19 reads, 2 writes.
- W1 = 0x14045725A `MOV dword ptr [RBP+0x2601C],EDI` in FUN_140457070 (decompile line 129). Value = the actor of the newest ring-1 record (`FUN_140A3E7D0(E, ·, 0)`, E = `*(ctx+0xE0)`) when `FUN_140A60530` holds (team < 2, idx < 0x28) and maps to a slot (`FUN_140A62E40`); else 0xFF. Ring 1 is at E+0x1908 (800-byte records, head E+0x320C, count E+0x3208). The write runs only when TA+0x11 is set (line 46), and any call with param_2 ≠ 0 sets it (line 41-42). Caller chain from the docs (E8 scan): FUN_140458940 ← FUN_140458880 ← FUN_1404582C0 (its caller OPEN).
- W2 = 0x140A71DF9 `MOV dword ptr [RBX+0x2601C],0xFF` in FUN_140A71C30: an object reset (memset 0xEAE20 bytes at the MA base, then sub-object resets; decompile lines 13-78). Not a per-tick clear. Caller OPEN.
- Reads include the carrier accessor FUN_140A627E0 (0x140A627EC) and validity tests (e.g. 0x140701980, 0x140930F21).
- Writes through lea, memcpy or memset that cover +0x2601C are not searched (OPEN).

## 5. Answer: which writer keeps MA+0x2601C valid on a loose ball

- W1 (FUN_140457070, store 0x14045725A) is the only writer that sets a valid id. It re-derives the value from the newest K=8 touch key on each call. It never reads B+0x4C (loose flag), and nothing clears the value when the ball is lost. So MA+0x2601C keeps the last toucher's slot on a loose ball until (a) a newer touch key is pushed (FUN_140A0DFD0, docs), (b) the newest key maps to no on-pitch slot (→ 0xFF), or (c) W1 is not called (TA+0x11 clear).
- Consequence: on a loose ball the coordinator's holder H is the last toucher, read from MA+0x2601C. The fallback T_opp+0x78 applies only when MA+0x2601C is 0xFF (no touch yet, or an off-pitch last toucher). This explains the live coordinator designations on far loose frames (108 of 109, `pes-re-live-farchase.md`): they need H < 22.
- The side gate fits this: the gate (§3) requires T_S+0x78 invalid, so side S designates only when S did not make the last kick (INF, using the live meaning of T+0x78).
- FarChasePress's inference is right in substance: GF must keep the last-touch actor on loose balls. The first-touch note `pes-port-FirstTouch.md` says "PES's loose = no carrier"; W1 contradicts that unless the newest K=8 key turns invalid on loss. OPEN: a live read of MA+0x2601C and T_opp+0x78 on a far loose frame settles it. Not done: no match was loaded in this pass (recorder: `gf-FarChaseLive-scratch/fcl_rec.py`).

## 6. Class FUN_140632AE0(SS, T, H) → {5, 7, 4, 6, 1} (CONFIRMED; asm 0x140632AF8–0x140632BBA)

Order: FUN_1406324A0(SS, T) < 22 → 5. FUN_140A63950(ctx, side, 0xD, 0xFF) → 7. FUN_140632850(SS, T, H) → 4. T+0xB716 byte ≠ 0 → 6. FUN_140632320(SS, T, H) → 1. Else 7.
Class 3 is unreachable: no return path gives 3. The coordinator's `iVar2 == 3` branch (goal-kick PRESS on T+0x4B10) is dead code.

## 7. Maintain FUN_140632E70(PP, SS, T, k) → 1 = RELEASE, 0 = KEEP (CONFIRMED structure; call site asm 0x140634553)

Checks in order (decompile line numbers):
1. MI.state ≠ 1 → RELEASE (l.33).
2. k > 10, or T.A2A0[k] ≠ 0x34 → RELEASE (l.36).
3. `FUN_140A53F90(ctx, &0xFF)` ≠ 0 → RELEASE (l.40).
4. `FUN_140632BC0(side, ctx)` ≠ PP[0] → RELEASE (l.44).
5. B88C[k] == 5 and `FUN_1406324A0(SS, T)` ≠ m, with m = side·11 + k → RELEASE (l.48).
6. lifted = (B+0x18 > 1.5) or ballHigh (as §2.8).
   - lifted and T+0x78 valid → RELEASE (l.65–67).
   - grounded and T+0x78 valid: MU = `FUN_140A62960(SS)` must be valid and equal T+0x78, else RELEASE (l.70–78).
7. B88C[k] == 3 and PP[1] ≠ T+0x4B10 → KEEP (l.91). Reason 3 is written only by the dead class-3 branch (§6) and possibly by the eval (§10).
8. `FUN_140633350(SS, T, m, B88C[k]) ≠ 0` → RELEASE (l.94; the refusal is PressEvalRE's).
9. Otherwise:
   a. Reason 5: c = MA+0x2601C, else T_opp+0x78. c invalid → RELEASE. dist²(M, c) ≥ 9.0 (0x14259D60C) → RELEASE (l.96–111).
   b. PP[0] and PP[1] valid, and MA+0x2601C invalid: dist(X, TP) < dist(H, TP) → RELEASE (l.114–133).
   c. Reason 4 or 7: if flag 0xD is off and |bearing(H→M) − bearing(H→X)| (folded) < 45.0 (0x14259BE30) → RELEASE. Also T.87D0[8k] == 0 → RELEASE (l.137–160).
   d. Window: (rec+8) − (rec+4) ≤ 2·fps → KEEP. rec = `FUN_140A401C0(T+0xA2CC+k·0x34, 0)` (l.164–167).
   e. PP[1] invalid → KEEP (l.168).
   f. `FUN_1404C8EA0(MA, X, m)` ≤ 25.0 (0x14259BE20) → KEEP (l.171–173).
10. Otherwise RELEASE.

Caveat for 9.d: FUN_140A40200 writes +4 and +8 with the same frame when it creates an entry, and no other writer of +8 was found in this pass. So the window is 0 and the test keeps (INF; §12).

## 8. Helpers in scope

### 8.1 FUN_140632C70(SS, T, X, H) → bool (CONFIRMED structure; args from gate asm 0x1406327F8–0x140632804)

- d = sqrt(d²) with d² = NearestList entry of X in H's row (row = MA+0x22E4C+H·200 for H < 22, else row 0). Entry index = rank(X) + 11·[X ≥ 11]. X ≥ 22 → d = 0.
- d < 2.5 (0x14259BDE4) → TRUE. d > 14.0 (0x1425C48B8) → FALSE.
- Heading h: T+0x6EC == 0 → h = 180 if s > 0, else 0. T+0x6EC == 1 → h = 0 if s ≥ 1, else 180. Other values: u = d² of H in `FUN_140A714C0(MA, s)` (0x228EC if s == 1, else 0x22AD4). u ≥ 400.0 (0x1425A2670) → h = `FUN_140A4DB20(s)` (270 if s = +1, 90 if s = −1). Else h = `FUN_140587E20(MA, side, H)` = wrap360(FUN_140A71440(MA, side)[H] + 180).
- Return `FUN_140A53DA0(ctx, X, H.pos, h, 140.0, 11.0, 0)`: X lies in the annulus (0, 11] m around H and |wrap180(bearing(H→X) − h)| < 70°.
- Tags: CONFIRMED structure. INF on the list meaning of `FUN_140A714C0` and on the angle table `FUN_140A71440`.

### 8.2 FUN_140481B00(ref*, θ) → |wrap180(θ − *ref)| in [0, 180] (CONFIRMED)

- Constants: 360 (0x14259858C), 180 (0x142598580), ±1 (0x14256AE1C, 0x142588218), abs mask. Gate use: ref = H+0x554 (facing), θ = T+0x250.

### 8.3 FUN_140632850(SS, T, H) → bool, class 4 (CONFIRMED structure)

- mu = `FUN_140A62180(ctx, side, 5)`. If mu == 1: fail if |H.z| < F+0x1C. Else fail if F+0x1C − 5.0 (0x14259222C) < |H.z|. H.z = H+0x564.
- MU = `FUN_140A62960(ctx, side)` must be valid with T.87D0[8·(MU % 11)] ≠ 2.
- For each own man m = side·11 + i (i < 11) with MI+0x1748[m] ≠ 0:
  (a) `FUN_140A53DA0(ctx, m, H.pos, hA, 100.0, 10.0, 0)` with hA = 180 if H.z > 0, else 0 (bA).
  (b) `FUN_140A53DA0(ctx, m, H.pos, T+0x250, 90.0, 15.0, 0)` (bB).
- TRUE iff any bA or bB.
- Tags: CONFIRMED structure. FUN_140A62180 OPEN (§9). MI+0x1748 INF (on-pitch).

### 8.4 FUN_140632320(SS, T, H) → bool, class 1 (CONFIRMED structure; asm 0x140632352–0x140632359)

- A = newest K=8 key (`FUN_140A3E7D0(E, ·, 0)`). Require `FUN_140A63830(ctx, A) == 0`, i.e. A is not an on-pitch man (asm 0x140632359: JZ taken when the actor is off-pitch).
- ev = `FUN_140A3E190(E, 0)`: ev.kind == 1 (pass).
- MI.frame − ev.time ≤ 2·fps (108 frames at fps 54).
- R = `FUN_140A62EA0(ctx, ev.(team, idx))`; R < 22.
- AB = `FUN_140A4F210(s_opp)`: 90.0 unless s_opp == −1 (then 270.0). s_opp = T_opp+0x254, T_opp = `FUN_140A48A30(G, side == 0)`.
- b = bearing(R → H) (`FUN_140439E50`). TRUE iff |wrap180(b − AB)| ≤ 70.0 (0x14259BE4C).
- Tags: CONFIRMED structure. INF: the first condition is equivalent to "no on-pitch carrier", so class 1 is the no-carrier case.

### 8.5 FUN_140633710(PP, SS, T, k) → bool "clear the target" (CONFIRMED structure)

- tgt = `FUN_140A41160(T, k)` = T.C1C[k]; m = side·11 + k. FALSE unless tgt < 22 and PP[0] < 22.
- score = `FUN_14043B8B0(MA, m, ·)` (PressEvalRE §5.1). score < 3.0 (0x14258C264) → TRUE.
- back = `FUN_140A4DB20(s)` (270 for s = +1, 90 for s = −1).
- a = fold(norm360(TBL_A[H][tgt]) − back), TBL_A[H][tgt] = float at MA+0x22F7C + H·0x58 + tgt·4 (= MA+0x228D4 + H·0x58 + 0x16A8 + tgt·4). |a| < 50.0 (0x14259BE34) → TRUE.
- Else b = fold(norm360(TBL_B[H][m]) − norm360(TBL_B[H][tgt])), TBL_B = `FUN_140439FE0` = float at MA+0x23F7C + H·0x58 + col·4. TRUE iff |b| > 55.0 (0x14259BE38).
- Tags: CONFIRMED structure. Table contents OPEN (§11.7).

### 8.6 FUN_140A62960(ctx, side) → side·11 + k, or 0xFF (CONFIRMED asm 0x140A62966–0x140A629B1)

- T = `FUN_140A48A30(G, side)`. First k in 0..10 with T.A2A0[k] == 0x3A → side·11 + k. Else 0xFF. ctx is unused.

### 8.7 FUN_140A53F90(ctx, out*) → byte (CONFIRMED structure)

- E = ctx+0xE0; A = newest K=8 key. `FUN_140A60530(A)` (team < 2, idx < 0x28) → man = `FUN_140A3B320(FUN_140A489D0(G, team), idx)`.
- If man < 22 and T(man > 10).4BD4[man % 11] == 0 (keeper) and (*out > 21 or *out == man) → return B+0x2917 (byte). Else 0.
- The coordinator passes *out = 0xFF, so the result is B+0x2917 when the newest touch actor is a keeper. B+0x2917 meaning OPEN.

### 8.8 FUN_1406324A0(SS, T) → man or 0xFF, the class 5 predicate (CONFIRMED structure)

- c = MA+0x2601C; if c > 21: c = T_opp+0x78. c must be < 22.
- Only if (B+0x18 ≤ 1.5 and not high) or T+0x78 > 21: MU = `FUN_140A62960(ctx, side)`; cand = side-row entry 0 of NearestList[c] (entry 1 if cand == MU).
- Requires: cand < 22, cand ≠ MU, `FUN_140A55D40(G+0x180, side, 2) == 0` (ball-state test; doc-cited, §9), slot k = cand % 11 with T.B8C4[k] == 0 and (T.87D0[8k] ≠ 0 or kind == 10).
- d = `FUN_14044E0D0(MA, cand)` (distance from the ball list). If c > 21, d = sqrt((cand.x − MA.x)² + (cand.z − MA.z)²) against TP.
- thr = 5.0 (0x14259222C) when kind == 10, else 3.0 (0x14258C264). Kind excluded: {0x11, 0x14, 0x50, 0x52, 0x57}. d < thr → return cand. Else 0xFF.
- Tags: CONFIRMED structure. T+0xB8C4 meaning INF.

### 8.9 Arrival-race core FUN_140A53DA0 → FUN_140A53DF0 (CONFIRMED)

- FUN_140A53DA0(ctx, man, Q, h, p5, p6, p7) is a wrapper: it loads M(man).pos and tail-jumps (asm 0x140A53DE3 `JMP 0x140A53DF0`), so the caller's stack args reach the core unchanged.
- Core: d² = |P − Q|² in xz, with P = M.pos. TRUE iff p7² < d² < p6² and |wrap180(bearing(Q→P) − h)| < 0.5·p5 (0x142565600). Asm 0x140A53E46–0x140A53E57 passes Q in RCX and keeps P in RDX (INF on the full path).

## 9. Leaves used (CONFIRMED decompile unless marked)

- `FUN_140A48A30(G, s)` = `*(G+(s+0xA1)·0x28)` (team; s ≥ 2 → `*(G+0x1928)`). `FUN_140A48A10(G, i)` = `*(G+(i+0xBF)·0x28)` (man; i ≥ 26 → `*(G+0x1DD8)`). `FUN_140A48910(G, i)` = `*(G+(i+0xA5)·0x28)` (INF base, §11.8).
- `FUN_140A627E0(ctx, 0)` = MA+0x2601C (asm 0x140A627E8, read at 0x140A627EC).
- `FUN_140A3E7D0(E, out, n)`: n-th most recent record of the 8-deep ring at E+0x1908; type 0x37 → `FUN_140A60500()`.
- `FUN_140A3E190(E, i)`: event (kind +0, (team, idx) +4, time +0x10) (docs).
- `FUN_140A60530(p)` = p[0] < 2 and p[1] < 0x28. `FUN_140A3B320(rec, idx)`: slot 0..21 or 0xFF (docs). `FUN_140A62E40`: docs (`pes-re-first-touch.md` §5.2).
- `FUN_140A62EA0(ctx, (team, idx))` = `FUN_140A3B320(FUN_140A489D0(G, team), idx)` or 0xFF. `FUN_140A63830(ctx, key)` = team < 2 and slot < 22 (asm 0x140A6386B).
- `FUN_140A40BD0(T)` = T+0xA294 (or `FUN_140A602B0` when T+0xA29C ≠ 0). `FUN_140A604D0(level, col)` = `FUN_140A603C0(level, col) > 1.19e-7`. `FUN_140A603C0`: table at 0x142645A40 (32-byte rows by col), interpolated over level/100 clamped to 0..6 (docs; values not read).
- `FUN_140A40BC0(T)` = T+0xA298.
- `FUN_140A42940(T, k, v)`: T.A2A0[k] = v; `FUN_140A40200(T+0xA2CC+k·0x34, v, T.+0x21C)`; `FUN_140A85AA0(T+0x28C+k·0x40, v)`.
- `FUN_140A42B90(T, k, v)`: T.C1C[k] = v.
- `FUN_140A40820(T, k)`: T.A2A0[k] = 0; ring push 0; `FUN_140A85AA0(…, 0)`; zero T+0x26C+0x40k, T+0x274, T+0x27C, T+0x284, T+0x288 (press geometry).
- `FUN_140A40200(rec, v, frame)`: if the head value ≠ v: advance the head when head.+4 < frame; write {v, frame, frame} as the new head.
- `FUN_140A401C0(rec, n)`: head − n entry (12-byte entries; head index at rec+0x30).
- `FUN_140A41160(T, k)` = T.C1C[k].
- `FUN_140A4DB20(out, s)`: 270.0 (imm 0x43870000), or 90.0 (imm 0x42B40000) when s == −1. `FUN_140A4F210(out, s)`: 90.0 unless s == −1 (270.0).
- `FUN_140A714C0(MA, s)` = MA+0x228EC if s == 1, else MA+0x22AD4. `FUN_140A71440(MA, side)` = MA+0x22A7C if side == 1, else MA+0x22C64.
- `FUN_140587E20(MA, side, H)` = wrap360(FUN_140A71440(MA, side)[H] + 180).
- `FUN_1404C8EA0(MA, row, m)`: NearestList read, sqrt of d² (docs, `pes-re-carrier-ai.md` §21BD).
- `FUN_14044E0D0(MA, m)` = sqrt(MA+0x24710 + (rank(m) + 11·[m ≥ 11])·8) (ball-list d²).
- `FUN_140439E50(out, A, B)` = bearing A→B in degrees, [0, 360). `FUN_140439EF0(A, B)` = the same, returned. `FUN_140439FE0(MA, a, b)` = float at MA+0x23F7C + a·0x58 + b·4 (writer OPEN). `FUN_14043A0A0(a, b)` = wrap180(b − a).
- `FUN_1415A1780` = sqrt (docs, `pes-re-carrier-helpers.md` §table).
- `FUN_141E532E0()` = 54 (value CONFIRMED).
- `FUN_140CA1A50(·)` = 1 (CONFIRMED).
- `FUN_140A63950(ctx, side, id, val)`: team instruction test (docs `pes-re-OffBallSelectors.md` §5B12): scan G+0x25CF8 (ids) and G+0x25D38 (values) at side·4+i; val 0xFF = any.
- `FUN_140A62180(ctx, side, k)` = k < 7 and side < 2 ? `FUN_1408C5030(T+0x8E08, k, 3)` : 0 (leaf OPEN).
- `FUN_140A57800(B)`: predicted ball record, +4 = height (internals OPEN). `FUN_140A74EC0(E, MI)`: OPEN (docs call it the kick-off timer). `FUN_14063FDC0(out, ctx)`: OPEN. `FUN_140A55D40(G+0x180, side, k)`: ball-state test (docs `pes-re-constant-team.md`), OPEN for me.

## 10. Boundary with PressEvalRE

- FUN_140633940 call (CONFIRMED asm 0x140634A80–0x140634A89): RCX = T, RDX = SS, R8 = X = PP[1] (excluded), R9 = H = PP[0], [RSP+0x20] = PP[2] (incumbent), [RSP+0x28] = class, [RSP+0x30] = A. EAX → PP[2]: man 0..21, or 0xFF for none. PressEvalRE confirmed this map and the return in their reply. On accept their eval writes PRESS and B88C[slot] = mode; mode 1 also releases.
- FUN_140633350 (refusal) is called by maintain at §7 step 8: PressEvalRE's.
- FUN_140A51D50: PressEvalRE's.
- "keep-max" (docs/re/pes-re-far-chase.md §6): not in the coordinator; the eval's first-passing rule is PressEvalRE's (already stated in FarChasePress §3).

## 11. Findings against existing docs and live notes

1. MI chain. Asm: `MOV RAX,[0x1436F3DC8]; MOV RCX,[RAX+0x270]; CMP [RCX+0x116C],1` (0x1406345F6–0x140634604). So MI = `*(G+0x270)`. `pes-re-live-farchase.md` (Method) writes MI = G+0x270 with state at MI+0x116C. Both agree only if `*(G+0x270)` = G+0x270. One live read settles it before any live number that depends on MI is reused.
2. Team point. The live doc reads `T_s+0x2482C` / `+0x24834` with `T_s = G+(0xA1+s)·0x28`, which is the slot, not the team object. The gate's point is MA+0x2482C with MA = `*(ctx+0x18)`. The live (0, 0) and (90, 0) values are not that point, so the 13 m split in that doc is not evidence.
3. Class 3 is dead. `pes-re-defence-leftovers.md` §14.4 (3 = goal-kick slot T+0x4B10 → PRESS) is not reachable: FUN_140632AE0 returns only {1, 4, 5, 6, 7}. INF: the eval may write reason 3; PressEvalRE to confirm.
4. Class-5 re-pick. The coordinator's class-5 branch takes the row's entry 0 (or 1 when that is MU), with no 3 m, 5 m or kind test. FUN_1406324A0's filters only decide the class. So the designated man can differ from the one that qualified the class (asm 0x140634911–0x140634A54).
5. Dead clause. In the designation block, `T+0x78 > 21 || …` is always true once the gate passed, because the gate fails when T+0x78 is valid. The MU == T+0x78 clause is therefore dead.
6. Writers. `pes-re-OffBallSelectors-inventory.md` and `pes-re-foundation-matchball.md` E5b are right about W1 = 0x14045725A (in FUN_140457070). W2 = 0x140A71DF9 is an object reset (memset), not the per-tick clear the inventory's wording suggests.
7. A_tab. FUN_140633710 reads MA+0x22F7C + H·0x58 + tgt·4 as a bearing (decompile l.37). `pes-re-s1-writer.md` says that range holds NearestList data (live: no 22×0x58 bearing table found) and calls A_tab dead. If S1Watch's bytes are right, the lane test reads NL bit patterns as angles. A port must reproduce the layout, not a table. INF; live check OPEN.
8. `pes-port-FirstTouch.md`: "PES's loose = no carrier" conflicts with W1 (§5).
9. Doc vs code on the release path. `pes-re-far-chase.md` §6 says FUN_140A42B90 clears the B88C reason word. FUN_140A42B90 writes u32 T+0xC1C[k] only; the coordinator's release zeroes B88C[k] with its own store (asm 0x140634590, `MOV word ptr [RBP+RAX*2+0xB88C],SI`). PressEvalRE agrees on FUN_140A42B90.

## 12. OPEN

1. TA+0x11 flag and FUN_140457070's call rate (caller FUN_1404582C0 not traced). W1 writes only when the flag is set.
2. T+0x78 writer (live V1: writer exists, VA open).
3. Live check of MA+0x2601C and T_opp+0x78 on far loose frames (blocked: no match loaded in this pass).
4. MI pointer chain (§11.1): one live read.
5. FUN_140A74EC0 meaning; MI+0x14C8[side]; B+0x2917 meaning; FUN_14063FDC0 vector.
6. FUN_140A62180 (T+0x8E08 via FUN_1408C5030); T+0x6EC meaning; T+0x7C = X meaning (docs say the MATCH_UP idx).
7. FUN_140439FE0 table (MA+0x23F7C) and A_tab (MA+0x22F7C) contents (§11.7).
8. Caller of FUN_140A71C30 (object reset: per life or per period?). Kind-byte base: FUN_140A48910 (0xA5) vs man base FUN_140A48A10 (0xBF) (§4).
9. memset/memcpy covering MA+0x2601C (not searched).
10. Coordinator-to-eval contract: the eval writes PRESS and B88C itself (PressEvalRE report).

## 13. Checker

`<workdrive>/gf-PressCoord-scratch/check_press_coord.py` (run under `ulimit -v 2000000; nice -n 19 ionice -c3 python3 check_press_coord.py`). It parses §0 and §14 of this report, re-reads each constant from the image, decodes each asm row with capstone, re-runs the holder scan, checks the immediates in FUN_140A4DB20 and FUN_140A4F210, and runs six mutations that must each fail.

Last run (this report, this session): `checker: constants 30/30, asm rows 42/42, holder scan 21 operands (writes 0x14045725A, 0x140A71DF9) OK, immediates OK; mutations fired 6/6; CLEAN`

## 14. Asm claims (checker input; Ghidra text, decoded from the image)

| VA | expected asm | role |
|---|---|---|
| 0x1406327BE | `CMP dword ptr [RDX + 0x78],0x15` | gate: T+0x78 valid → fail |
| 0x1406327C2 | `JBE 0x1406327BA` | gate fail branch |
| 0x1406327C4 | `CMP byte ptr [RDX + 0xa53c],0x0` | gate: press-timer pass |
| 0x1406327DF | `CALL 0x140A63950` | gate: flag 0xD |
| 0x1406327F1 | `CMP dword ptr [RSP + 0x50],0x5` | gate: class 5 pass |
| 0x140632804 | `CALL 0x140632C70` | gate: X-cover helper |
| 0x14063282A | `CALL 0x140481B00` | gate: angle test |
| 0x14063282F | `COMISS XMM0,dword ptr [0x14259A764]` | gate: 60.0 compare |
| 0x140632AF8 | `CALL 0x1406324A0` | class 5 predicate |
| 0x140632B02 | `MOV EAX,0x5` | class 5 result |
| 0x140632B32 | `MOV EAX,0x7` | class 7 result |
| 0x140632B50 | `CALL 0x140632850` | class 4 predicate |
| 0x140632B59 | `MOV EAX,0x4` | class 4 result |
| 0x140632B6E | `CMP byte ptr [RBX + 0xb716],0x0` | class 6 test |
| 0x140632B77 | `MOV EAX,0x6` | class 6 result |
| 0x140632B95 | `CALL 0x140632320` | class 1 predicate |
| 0x1406345DE | `CALL 0x140632BC0` | coordinator: holder |
| 0x1406345F6 | `MOV RAX,qword ptr [0x1436F3DC8]` | G |
| 0x1406345FD | `MOV RCX,qword ptr [RAX + 0x270]` | MI = *(G+0x270) |
| 0x140634604 | `CMP dword ptr [RCX + 0x116C],0x1` | in-play test |
| 0x140634553 | `CALL 0x140632E70` | coordinator: maintain |
| 0x14063466C | `CALL 0x140632780` | coordinator: gate |
| 0x140634627 | `CALL 0x140A53F90` | coordinator: keeper hold |
| 0x140634642 | `CALL 0x140632AE0` | coordinator: class |
| 0x1406346AC | `CALL 0x140A57800` | coordinator: predicted ball |
| 0x1406346E9 | `CALL 0x140A62960` | coordinator: MATCH_UP (side in EDX) |
| 0x140634719 | `CALL 0x140A62180` | coordinator: table test (k = 4) |
| 0x140634911 | `MOV RDX,qword ptr [R8 + 0x18]` | MA in class-5 block |
| 0x140634935 | `MOVSXD RDX,dword ptr [R14 + 0x10]` | side for the NearestList row |
| 0x14063494F | `CALL 0x140A62960` | class 5: MATCH_UP |
| 0x140634A80 | `MOV dword ptr [RSP + 0x20],EAX` | 5th arg = incumbent |
| 0x140634A84 | `CALL 0x140633940` | coordinator: target eval |
| 0x140632352 | `CALL 0x140A63830` | class 1: actor test |
| 0x140632359 | `JZ 0x14063236A` | class 1 proceeds if actor off-pitch |
| 0x140A6386B | `CMP EAX,0x15` | FUN_140A63830 on-pitch test |
| 0x140A629A7 | `IMUL ECX,EBX,0xB` | MATCH_UP scan: side·11 |
| 0x140A53DE3 | `JMP 0x140A53DF0` | arrival-race wrapper tail-jump |
| 0x140A627E0 | `SUB RSP,0x28` | flat mapping sanity (FUN_140A627E0 start) |
| 0x140A627EC | `MOV EAX,dword ptr [RAX + 0x2601C]` | carrier read |
| 0x14045725A | `MOV dword ptr [RBP + 0x2601C],EDI` | writer W1 (FUN_140457070) |
| 0x140A71DF9 | `MOV dword ptr [RBX + 0x2601C],0xFF` | writer W2 (FUN_140A71C30) |
| 0x140634590 | `MOV word ptr [RBP + RAX*0x2 + 0xb88c],SI` | release: B88C[k] = SI (0 per decompile) |
