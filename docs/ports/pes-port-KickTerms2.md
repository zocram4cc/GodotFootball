# PES port: KickTerms2 (branch `KickSoftMin`; continues `docs/ports/pes-port-KickTerms.md`)

## Verdict
- **Wired and proven:** the PES sine `FUN_140a92c80` (`SinTableDeg`) now runs PES's own translated function (`ball/pes_fn_140A92C80.cpp`). The hand interpolation is deleted. Commit `036b2814`.
- **Settled from the image (CONFIRMED, operand-VA pairs below):** the two TENTATIVE affines. The R per-case affine and the xmm8 contact-byte affine are now decoded. The R final scale is 0.1, not "small TENTATIVE". Two decode facts were wrong and are corrected below.
- **Not ported:** Q, R, F1/F2, angRamp, post-scales `FUN_1408414A0`, yaw redraw `FUN_1408398D5`, shot pitch. Each needs PES record fields GF does not produce (listed as named stand-ins). I did not port them and did not run new oracles for them.
- **Soft-min `FUN_1408413C0`:** not translated. The CRT qsort (`0x141E53540`) is an unmodelled callee in the harness, so the translation cannot run. The hand port `SoftMin4` stays, with the KickSoftMin oracle result (54,000 cases, 0 mismatches; old fold 45,217). I did not re-run that oracle this session.
- Pushed: merge `ca648268` (origin/main), cutover `036b2814`, comments `f1c82642`. `git ls-remote origin KickSoftMin` = `f1c82642`.

## Corrections to `docs/re/pes-re-kick-scatter-inputs.md` (CONFIRMED by re-read)
1. **xmm8 global is `0x14351AE50`**, not `0x1416C7B6A` (the bytes at the decode's address are `64 24 20 0f ...`, not 54). The value 54 is set by `0x141E532F0` (`movss` float 54.0 from `0x14259D624`, then falls into `0x141E53300`, which stores int `0x36` to `0x14351AE50`). `0x141E532F0` has static-init callers (`0x1404481B3`, `0x141FD175A`, `0x141FD1C48`, `0x141FDD0A8`, `0x15B5BCC54`). The image dump holds 54 at `0x14351AE50` (script-read).
2. **R final scale k = 0.1** at `0x142588200` (f32 0.10000000149), operand `0x140723865 mulss xmm6,[rip]`. CONFIRMED.
3. **R's final sum has three more weights**: `0x14344EF98 = -0.0029828210826963186`, `0x14344EF9C = 2.7572925090789795`, `0x14344EFA0 = 8.718459129333496`. The loop (`0x140723848..0x140723863`, `rax = 0,4,8`) sums `[rsp+0x68]` (xmm8) × EF98, `[rsp+0x6C]` (the case affine result) × EF9C, and `[rsp+0x70]` (= 1.0, `0x140723767`) × EFA0. The decode listed only 8 weights.
4. **R runs three passes** (`0x140723881 cmp ebp,3`, `0x140723884 jl`): the running max `xmm15 = max(Σ·0.1, xmm15)` with `xmm15` initialised from `xmm7` at `0x140723359`. The decode did not show the outer loop.

## (b) R per-case affine: CONFIRMED (case mapping; xmm14 in R is OPEN)
Dispatch `0x1407236B6..0x1407236DA`: `case = movsx(byte[r15+1]) - 8`, `ja` on unsigned `> 13`. Table: 32-bit RVAs at `0x14072394C + 4*case` (image base `rbx = 0x140000000`, `0x1407236BB lea`). Script-read entries:

| byte[r15+1] | case | target | affine on xmm6 (operand VAs, f32 constants) |
|---|---|---|---|
| 8, 9, 10 | 0,1,2 | `0x14072370C` | `0.8 - 0.6*xmm14` (0.8 `0x142572348`, 0.6 `0x14257A190`); then if `byte[rsi+0xB04] <= 1` and `byte[r15+0x3C] <= 1`: `1.3 - 0.4*xmm14` (1.3 `0x14259BDD8`, 0.4 `0x14259B89C`) |
| 11 | 3 | `0x1407236F2` | `1.3 - 0.8*xmm14` |
| 12 | 4 | `0x140723756` | unchanged |
| 13, 14 | 5, 6 | `0x1407236DC` | `xmm11 - 0.7*xmm14` (0.7 `0x14257A194`) |
| 15..20 | 7..12 | `0x140723756` | unchanged |
| 21 | 13 | `0x14072374E` | `-1.0` (`0x142588218`) |

Default path (table value at `[[rsp+0x20]+idx*4+0x4C00]` = 0, `0x140723680`): `call [vtbl+0x20]`; if `byte[rax] != 0xB`: `2.0 - 1.5*xmm14` (`0x142565604`, `0x14259BDDC`); if `== 0xB`: `xmm11 - 0.7*xmm14` (`0x1407236E0`). **OPEN:** the R caller's `xmm14`, `xmm11`, `xmm12`, `xmm9`, `xmm8` at entry, and the vtbl object; they need the R caller trace.

## (d) xmm8 contact-byte affine: CONFIRMED
`0x14083ECCE cmp byte[obj+1],0x48`; `jne 0x14083ECDA`: fast path `xmm8 = 1.0` (`movaps xmm8,xmm12`, `xmm12 = 1.0`). Else `B = movzx byte[obj+0x2C]` (`0x14083ECDA`), `V1 = getter(0x141E532E0)`, `a = (float)V1` in xmm7, `V2 = getter()`, `hi = floor(V2*0xAAAAAAAB >> 34)` = `floor(V2/6)` (`0x14083ED01 mul ecx`, `0x14083ED07 shr edx,2`), `x = (float)B` (`cvtdq2ps`). Then:
- `hi >= x` (`0x14083ED11 comiss; jb` not taken): `xmm8 = 1.0`.
- else `x >= a` (`0x14083ED1D comiss; jb` not taken): `xmm8 = 0.0` (`xmm14`).
- else `xmm8 = (x - a) / (hi - a)` (`0x14083ED29..ED32`).

With V = 54: `hi = 9`, `a = 54`. So **t(B) = 1 for B ≤ 9, (54−B)/45 for 9 < B < 54, 0 for B ≥ 54**. `obj = 0x140A48910(FUN_1406EA340(kicker))` (anime accessor, translated in `pesworldrecord`).

angRamp is **xmm7**, not xmm8: `ramp = clamp((err+20)/40)·0.4`, `err = byte[obj2+0x28] − byte[r13+0x28]` (`0x14083ED47..ED58`, constants `0x1425A946C` −20, `0x14259BE1C` 20, `0x14259BE2C` 40, `0x14259B89C` 0.4). The KickSoftMin doc's "angRamp = xmm8" was wrong.

## (a) Q, (c) F1/F2, (e) post-scales, (f) yaw redraw, (g) shot pitch: NOT PORTED
Decode status (unchanged from KickDecode, with the notes above). Blockers:
- **Q**: needs PES kick kind `ap+0x2752` (151-row table `0x14344E380`, class byte 13, subclass byte 12). GF has no kick-kind bridge (decode Unresolved 1). Stays 1.0 (`kSituationNotPorted`), named.
- **F1/F2**: motion descriptor from the lazily built motion table (`0x1406B0ED0`, singleton `0x1436F23C0`, built at run time). Not in the image, so the rows cannot be read statically. Stays 1.0.
- **post-scales `FUN_1408414A0`**: needs kind, motion id and PES predicates. Not in code (absent, not a stand-in). Decode complete (tiers 0–3, flag tables `0x1408417F8` 0x6E bytes, `0x140841884` 0xD8 bytes).
- **yaw redraw `FUN_1408398D5`**: needs the `[rbp-0x78]` vector provenance (OPEN). Existing `ponytail:` in `DrawKickError` (loop skipped).
- **shot pitch g**: needs `xmm6`, `xmm12`, `xmm11` (F from `0x141E532D0`), decoded but the caller chain is not traced. Existing `ponytail:` in `ShotLaunchPenalty`.
- **Header predicate**: three disagree (`team.cpp:26 IsKickFunction` excludes Header; `gamedefines.hpp:72` and `match.cpp:1643` include it). PES maps Header to touch (7). **Not changed here**: `IsKickFunction` gates `inKickBeforeContact` (team.cpp:401), a possession change outside the scatter terms.

## Translated-layer finding (for Main)
`pes_fn_140A92C80` reads the table's upper neighbour `T[i+1]` through its **memory view** (`s.rdf`, line 85), but `T[i]` through the baked copy (`ro_rd`, line 82). The harness maps the image, so the 72-case pilot proof passed. A GF `PesState` with no table mapped gives `(1−frac)·T[i]` (30° → 0.166 instead of 0.5). The cutover maps `k.sinTable` as the read-only window. Suggest: the generator should read every constant through the baked copy, or the layer should assert its window is mapped.

## Changes (branch `KickSoftMin`)
- `036b2814` **cutover**: `SinTableDeg(k, deg, amp)` runs `::pes_fn_140A92C80` on `PesState`; `k.sinTable` (install read, `0x142655950`) is the window. Hand interpolation and `halfTurn_deg` deleted. `Normal` calls the new signature. Tests: `SinTableIsDegreesScaledByAmpAndWrapped` (wraps and sign against true sine), `SinTableIsTheSineAtItsKnots` (replaces the table-equality knot test, which the synthetic table made tautological), install test relation.
- `f1c82642` **comments only**: every stand-in now cites its PES field or site (Q, xmm8/angRamp, run term).

## Tests and mutations
- **Red**: (1) the new `SinTableDeg(deg, amp)` signature: compile error "invalid initialization ... from int" (feature missing). (2) First green attempt with the table unmapped: 5 failures at off-knot angles (30°, 210°, −150°, 390°, 30°×3). Fixed by mapping the install table.
- **Green**: `pes_kick_error_test` 14/14 with `PES21_ROOT` set (install test included). Run directly (not under `simslot`: the slot wrapper blocked for 300 s on a unit test).
- **Mutation M1** (`s.mem.window = {}`, table unmapped): rebuilt, run: `SinTableIsDegreesScaledByAmpAndWrapped` FAILED, 13 passed. The knot test passed (frac = 0 at knots, so it cannot see the `frac·T[i+1]` term). Reverted by the edit tool; `git diff HEAD` on the file is empty; rebuilt; 14/14.
- `ninja -C build -t deps`: 0 objects with `#deps 0` after the struct change (`halfTurn_deg` removed).
- Not run: other binaries linking `gfsim` (their tests), the gate (Main), any sim smoke. The `gfsim` lib was rebuilt (includes `peskickerror.cpp.o`).

## Oracle status
- Sine: translated record `results.json` `0x140A92C80`: diffed, 72 cases (pilot harness, not re-run here). Hand port previously 20,008 cases / 0 mismatches (KickSoftMin, now deleted).
- Soft-min: 54,000 cases, 0 mismatches (KickSoftMin, unchanged code; not re-run).
- No new unicorn oracle was run for R, xmm8, angRamp, Q, F1/F2, post-scales, yaw or shot pitch. The R and xmm8 affines are settled by disassembly and script-read constants only (no oracle yet).

## Stand-ins remaining (enumerated, each with its PES field)
1. Q: `kSituationNotPorted` = 1.0 (kind `ap+0x2752`, class/subclass table `0x14344E380`, `ap+0x2756/0x2764`, `ap+0xBE4 - ap+0x247C`, AI bit `ap+0x2460` bit 29).
2. F1/F2: 1.0 (motion table `0x1436F23C0`, runtime).
3. R run term: 0 (`FUN_140723320`: contact vector `ap+0x2BF4..`, kicker position `FUN_140A6CF70`, state id `vtbl+8`, per-opponent table `r13+rbx*4+0x16A8`).
4. xmm8 contact factor: 1.0 (`obj = 0x140A48910(FUN_1406EA340)`, `byte[obj+1]`, `byte[obj+0x2C]`).
5. angRamp xmm7: omitted (`byte[obj2+0x28]`, `byte[r13+0x28]`).
6. Post-scales `FUN_1408414A0`: absent (kind, motion id, predicates).
7. Yaw redraw loop: skipped (`[rbp-0x78]` provenance OPEN).
8. Shot pitch g: omitted (xmm6, xmm11 via `0x141E532D0`).
9. Header predicate disagreement: not changed (gates possession).
10. Soft-min translation: blocked by CRT qsort (unmodelled); hand port `SoftMin4` stays.

## Open items
- Settle R's caller registers (xmm14, xmm11, xmm12, xmm9, xmm8) and its vtbl object to prove R with an oracle.
- Q: decide the GF→PES kind bridge (decode Unresolved 1); until then Q stays 1.0.
- Trace the yaw reference vector `[rbp-0x78]` to its caller (`FUN_1408398D5`).
- Add a qsort stub model to the harness, or accept the hand port, to close soft-min translation.
- Translated-layer note above (table read through the memory view).
