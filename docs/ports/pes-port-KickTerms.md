# PES port: KickTerms (branch `KickSoftMin`; continues `docs/ports/pes-port-KickSoftMin.md`)

## Verdict
- Ported and bit-exact: the PES sine table `FUN_140a92c80` (`SinTableDeg`). `Normal` now calls it, so every kick's yaw/pitch/speed draw uses PES's sine instead of `std::sin` (the old code said "<0.2% off"). This is not one of the seven terms; it is the prerequisite for term (g) and it sits in `Normal`, which every scatter draw runs through. Included because it is the only term whose inputs are all readable today.
- Not ported: terms (a) Q, (b) run term R, (c) F1/F2, (d) xmm8 angRamp, (e) post-scales `FUN_1408414A0`, (f) yaw redraw loop, (g) shot pitch term. Each stays a named stand-in in code with its PES site. Reasons are in the per-term table. I stopped new term work at about 70% of the request budget.
- Pushed: `8fa8c6d4` (merge of origin/main, which carried the `pes-port-KickSoftMin.md` doc), `efb7ed55` (red test), `89851d14` (port), `8000065b` (knot test), `ebb956d3` (stand-in comment). `git ls-remote origin KickSoftMin` = `ebb956d3`.

## What was decoded (CONFIRMED, raw disassembly of the unpacked image)
`FUN_140a92c80(xmm0 = deg, xmm1 = amp) -> xmm0`:
- Wrap: `deg <= 0` (`0x140a92c8c`) adds 360 (`0x14259858c`) once. Else if `deg >= 360` (`0x140a92c98..c9f`) it adds −360 (`0x142598598`) once.
- Sign: `x >= 180` (`0x140a92cb1`, constant `0x142598580`) sets sign = −1 (`0x142588218`) and adds −180 (`0x1425ddffc`). sign starts at +1 (`0x14256ae1c`).
- Mirror: `x >= 90` (`0x140a92cd9`, `0x14259be5c`) sets `x = 180 − x`.
- Index: `t = x / 90 * 512` (`0x140a92ce5`, `0x1426550e8`), `cvttss2si` truncation. Indices 0..511 interpolate; anything else returns `sign*amp` (`0x140a92d36`).
- Interpolation: `v = (1 − f)·T[i] + f·T[i+1]`, returned `v·(sign·amp)` (`0x140a92cf5..d2e`). Table `T` = 513 floats at `0x142655950` (`0x140a92d04`).
- Caller `Normal` (`0x140a92590`): call at `0x140a92721` with `xmm0 = angle (+90 on attempt 0)`, `xmm1 = xmm6 = 1.0` (`0x14256ae1c`, set at `0x140a925ae`). So `Normal`'s sine is `SinTableDeg(angle, 1.0)`.

Script-read constants (`pes_full.bin`, offset = VA − 0x140000000):

| VA | value |
|---|---|
| 0x142598580 | 180.0 |
| 0x14259858c | 360.0 |
| 0x142598598 | −360.0 (port subtracts; bit-identical) |
| 0x1425ddffc | −180.0 |
| 0x14259be5c | 90.0 |
| 0x14256ae1c | 1.0 |
| 0x142588218 | −1.0 |
| 0x1426550e8 | 512.0 |
| 0x142655950 | table, 513 floats: T[0] = 0.0, T[1] = 0.0030680000782, T[256] = 0.7071070075, T[511] = 0.9999949932, T[512] = 1.0 |

## Port and tests (TDD, `tests/sim/pes_kick_error_test.cpp`, `peskickerror.cpp/.hpp`)
- Red (`efb7ed55`): `SinTableIsDegreesScaledByAmpAndWrapped` used the not-yet-existing `SinTableDeg`, `halfTurn_deg`, `sinTable`. Build failed with "has no member named" errors. Feature missing, not a typo.
- Green (`89851d14`): `pes_kick_error_test` 13/13 with `PES21_ROOT` set (install test included).
- Knot test (`8000065b`): `SinTableIsExactAtItsKnots`. Checks that the lookup at angle `i·90/512` equals the table entry, exactly.
- Final (`ebb956d3`): 14/14 pass.
- Oracle (unicorn, scratch copy of the diff harness, not committed): spec `0x140A92C80`, inputs `deg` in [−720, 1080] and `amp` in [−2, 5], with edges (0, ±180, ±360, 90, 1.0, ±1ulp, 1080 region, …). Corpus 20,008 valid cases (0 emulation failures, icount max 40). Checker: the port with constants read from PES2021.exe, compared bit-for-bit (return value). **20,008 cases, 0 mismatches.**
- Mutation M1 (interpolation weights swapped: `frac*T[i] + (1-frac)*T[i+1]`): before the knot test, gtest still 13/13 PASS (tolerance 1e-3 misses it). Oracle: **12,234 mismatches**. With the knot test: fails at knots 0, 37, 74, 111, … (caught). Reverted; 14/14 and oracle 0 again.
- Not mutated: the wrap/sign branches (covered by the oracle's edge cases, not separately mutated).

## Per-term status
| term | status | PES site | stand-in / blocker |
|---|---|---|---|
| (a) Q situation product | NOT PORTED | Q block 0x14083e57c..e607; contact class FUN_1407205a0 (0.9 / 0.88 / 0.86, constants 0x14258c260, 0x1425ddf70, 0x1425eb850); subclass FUN_14071e520 (0.95 / 0.8 / 0.85 / 0.9 / 0.7); first-time 0.925 | `kSituationNotPorted` (1.0), existing. Blocker: GF has no PES kick-kind byte (`ap+0x2752`), so the class/subclass/first-time gates have no input. Mapping GF `e_FunctionType` to PES kind ids is a design decision (needs the kick-kind table 0x14344e380). Body-turn needs `ap+0xbe4` (facing) vs `ap+0x247c` (heading); not traced to a GF field. |
| (b) run term R | NOT PORTED | FUN_140723320(ap) · 1.5, squared, capped at 1, ×0.3 for kind 0x6e (INF high, from the prior decode) | `ponytail:` in `PressureFactor` (run = 0). Blocker: FUN_140723320 (nearby-threat sum over weights at 0x14344ef78) not decoded this session. GF has `AI_GetClosestPlayer`, so wiring is possible once decoded. |
| (c) F1 / F2 | NOT PORTED | F1, F2 at 0x14083e749 (= 1.0) unless `0x1406b0250(...) == 1` (0x14083e76f) | `kSituationNotPorted` (1.0). Blocker: `0x1406b0250` reads a per-motion descriptor byte (`tbl[m·0x390 + 1 + (flag != 0)]`); GF has no PES motion table. |
| (d) xmm8 angRamp in D | NOT PORTED | D = `(dBase − 0.4·edge)·distRamp·xmm8` (0x14083edef) | Not in `PressureFactor`. Blocker: xmm8 is set before 0x14083ed60, not traced. |
| (e) post-scales FUN_1408414A0 | NOT PORTED | 0x14083f038 onward; A (action ≥ 0x5e): S.scale×0.1, S.pSign = 0.5; B1: Y.scale×0.9, P.scale×0.1, S.scale×0.1; B2 pSign rewrites | Blocker: branches gate on kick kind (`ap+0x2752`) and motion id (vtbl+0x18); GF has neither. arg6 (`[rsp+0x78]`) meaning open. |
| (f) yaw redraw loop | NOT PORTED | 0x14083a550, ≤6 draws (`cmp ebx,5`), `Y.scale·h` without the w term | `ponytail:` in `DrawKickError` (loop skipped). Blocker: the sign test on `[r14]·[r14+8]` is not traced. |
| (g) shot pitch term | NOT PORTED (formula decoded) | `I = xmm12 − 0.25·xmm6 − 0.02·g` for action byte 10 (0x14083dd0f..dd27); non-10 branch `I = … − 0.35·xmm6 − 0.04·xmm7` (0x14083dcec..dd04) | `ponytail:` in `ShotLaunchPenalty`, now citing the sites. Constants script-read: 0.02 at 0x1425c4894, 0.04 at 0x1425a9334, 0.35 at 0x14259d5c8, 0.25 at 0x142592228. g = `SinTableDeg(90·x, xmm12)` (the sine is now ported) with `x = 0.25·(xmm14 − xmm1)` if `xmm14 < xmm7` (0x14083dbfa), else `x = xmm12`. Blocker: xmm14, xmm7, xmm1 are set before 0x14083da40 and not traced. Moves I by at most 0.02. |

## Other findings
- `Normal`'s Box–Muller and angle-pair logic are unchanged and are NOT oracle-tested. Only the sine they call is now bit-exact.
- `ApplyKickError` rebuilds the velocity with libm `sin`/`cos`/`atan2`. That is not PES-bit-exact. Out of this ticket's scope; open.
- Behaviour change: every kick draw now uses the exact sine. The change is small (under 0.2% per draw). Not smoked here (Main's gate and tripwire smoke).
- Scratch harness note: `run_softmin.py` computes `TRANSLATE_DIR` from its own location, which is wrong for the scratch copy. Run with `PYTHONPATH` pointing at a translate directory that has `stubs.py`.

## Verification run
- `pes_kick_error_test`: 14 passed (PES21_ROOT set), 0 failed.
- Oracle `sin_oracle` (SinTableDeg): 20,008 cases, 0 mismatches.
- `ninja -t deps`: `#deps 0` count 0.
- Gate not run (Main gates). `gfsim_run` smoke not run. The test build compiled `gfsim` (including `peskickerror.cpp.o`), so the library the sim links compiles; humanoid and match call sites are unchanged.

## Open items (numbered)
1. (a) Q situation product: map GF `e_FunctionType` (and header/cross) to PES kick-kind ids (kick-kind table 0x14344e380), then port contact class, subclass, first-time, body-turn, final factor. Owner-level decision needed for the mapping.
2. (b) run term: decode FUN_140723320 and its weight table (0x14344ef78); wire to GF nearby opponents.
3. (c) F1/F2: decode the motion descriptor byte of `0x1406b0250` / `0x1406b0ed0` and map GF motions.
4. (d) xmm8 angRamp: trace upward from 0x14083ed60.
5. (e) post-scales: needs kind + motion id (item 1, item 3).
6. (f) yaw redraw: trace the sign test on `[r14]·[r14+8]` at 0x14083a550.
7. (g) shot pitch: trace xmm14, xmm7, xmm1 from before 0x14083da40; then a block oracle over 0x14083dbfa..dd27 (needs memory for `0x2bd0(%r13)` and the vtable calls).
8. `ApplyKickError` libm trig is not bit-exact.
9. Real-call oracle cases for `SinTableDeg` do not exist (the function is pure; generated cases are the check).
