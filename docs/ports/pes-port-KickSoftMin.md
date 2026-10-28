# PES port: KickSoftMin, the soft-min fold of FUN_1408413c0 (branch `KickSoftMin`, commit `1eb9d08d`, pushed, `ls-remote` confirmed)

## Verdict
- Ported: `pes::SoftMin4` (`src/sim/onthepitch/peskickerror.cpp`) now matches PES's fold bit for bit. Sort descending, fold from 1.0, four folds, PES's operation order. Old GF fold: ascending sort, fold from the smallest factor, three folds.
- Bit-exact over 54,000 generated cases (PES's own machine code emulated over the unpacked image). Old code: 45,217 mismatches on the same cases.
- No real-game calls were captured for this function (no rendezvous hook exists for `0x1408413C0`; Main owns `hooks.cfg`). The oracle cases are generated, not real-call.
- Not closed: the situation product Q, the F1/F2 factors, the run term, the `xmm8` factor in D, and the post-scale branch `FUN_1408414A0` are still GF stand-ins. Fidelity for pressured and first-time kicks stays open until they are ported.

## Premise check (handover item 1, `docs/re/pes-re-end-line.md` §2.2 (ii))
- The claim "`KickQuality` is a pure stat ramp, PES soft-mins stat × situation × launch × run" is stale for the scatter. `KickQuality(k, stat)` is PES's `a` = q(S), the kick-type quality (FUN_14071d770 tail). PES also uses `a` in I (launch window) and A = 0.75a + 0.25. It is not the per-draw qi.
- The per-draw qi already flowed through a soft-min in `BuildScatter` (`soft(x) = SoftMin4(launchPenalty, x, F, pressure)`). The defect was the fold's order and start, which only show with three or more non-unit factors. The amp soft-min has three (I, A, R). The scale soft-min has two (I, R) when Q = F = 1, and is order-insensitive there.
- `KickQuality` is unchanged. Its name is misleading, and I did not rename it in this change.

## Decode of FUN_1408413c0 (CONFIRMED, raw asm)
Source: linear objdump of `pes_full.bin` (file offset = VA − 0x140000000). The Ghidra bridge on 127.0.0.1:8080 was down (HTTP 000), so no Ghidra decompile was used.

| VA (instruction) | operation | constant (operand VA → value, script-read) |
|---|---|---|
| `0x1408413C0` | `sub rsp,0x48`; args `xmm0..3` stored at `[rsp+0x20..0x2C]` | Win64: args 1..4 are f32 `a,b,c,d` |
| `0x1408413FF` | `call 0x141E53540` (MSVC qsort: `rcx=&arr, edx=4, r8d=4, r9=cmp`) | `cmp` = `lea 0x14083CC10` at `0x1408413E7` |
| `0x14083CC10` (comparator) | returns `1` if `a<b`, `-1` if `a>b`, `0` if equal | ⇒ qsort order is **descending** |
| `0x140841404` `movss` | `x = 1.0` | `0x14256AE1C` = 1.0 (bits `0x3F800000`) |
| `0x14084140C` `movss` | `k = 0.5` | `0x142565600` = 0.5 (bits `0x3F000000`) |
| `0x140841414..0x14084142B` (×4 folds) | `lo=min(x,v), hi=max(x,v), x=(hi*lo + lo)*k` | 4 folds, in sorted order |
| `0x14084148B` `call 0x14159F3E0` | stack-cookie check (`0x14350B340` xor rsp) | not part of the value |

Fold (per factor v, in descending order): `x ← (max(x,v)·min(x,v) + min(x,v))·0.5`, starting at `x = 1.0`.

Worked check, `{1, 0.9, 0.5, 0.2}`: 1 → 0.9 → 0.475 → 0.1475. GF's old code gave 0.1425.

## Terms of the per-draw qi (decode from the existing reports, R path re-read raw)
Raw window `0x14083ED60..0x14083EE70` (inside FUN_14083CC30) confirms the R and D structure. The operands it cites are re-read by script:

| constant | VA | value (script) | used at |
|---|---|---|---|
| dBase | `0x142572348` | 0.8 | `0x14083EDE3` (`d = (dBase − 0.4·edge)·distRamp·xmm8`) |
| rWeight | `0x14259B89C` | 0.4 | `0x14083EDD4`, `0x14083EE3E` (`R = 1 − 0.4·X`) |
| 1.3 (run weight) | `0x14259BDD8` | 1.2999999524 | `0x14083EE0F` |
| dWeight | `0x14259BDDC` | 1.5 | `X = (1−0.5a)·1.3·run·(1−D) + 1.5·D` (`xmm9`, loaded before the window; doc value, not re-read) |
| 0.5 (a·0.5, Q·0.5) | `0x142565600` | 0.5 | `0x14083EE03` |
| distance near/far | `0x14256AE1C` / `0x142565604` | 1.0 / 2.0 | `0x14083EDB5..EDD3` (`distRamp = 1` inside 1 m, `2 − d` to 2 m) |
| contact span | `0x14259BE1C` / `0x14259BE2C` | 20.0 / 40.0 | `0x14083ED6C..ED86` (`edge = clamp((P − O + 20)/40)`) |
| sign of distance slope | `0x142588218` | −1.0 | `0x14083EDCC` |

- `R = 1 − 0.4·X`, `X = (1−0.5a)·1.3·run·(1−D) + 1.5·D`: CONFIRMED from raw (`0x14083EE00..EE3E`). The GF port takes `run = 0` (ponytail, FUN_140723320 not ported), so `X = 1.5·D`.
- `D`: CONFIRMED form `(dBase − 0.4·edge)·distRamp·xmm8`. `xmm8` is set before the window (not traced). GF omits it, which is **INF(med) = angRamp**, not ported.
- Launch penalty I, A = 0.75a + 0.25 (`0x14083E610..E633`), the situation product Q, F1/F2, the Qk factor and the post-scales of `FUN_1408414A0`: see `docs/re/pes-re-kick-scatter.md` §1–7. Not re-decoded here.

## Translated layer and recorded sets
- `1408413C0` and `1408398D5` are not in `src/pes/translated/` and not in any `tools/dev/translate/results*.json` (case-insensitive grep). They appear only in `tools/dev/ledger/joins.json` (cites) and `offpath_allow.json` (reason: unreachable by the walk). There is no proven translation to call, so the port is hand-written.

## Port and tests (TDD, `tests/sim/pes_kick_error_test.cpp`)
- Red, on the ascending fold (origin/main behaviour): `SoftMinFoldsTheSortedFactorsFromOne` (expected 0.1475, got 0.1425) and `AmpSoftMinFoldsThreeNonUnitFactorsInPesOrder` (yaw amp expected 1.9214060, got 1.9749978). 2 failing, 10 passing.
- Scenario note: the brief's example ("pressured first-time shot, 99-stat kicker") was not made red. Its pressured-vs-planted relation already holds on origin/main, and at a = 1 the amp fold has only two non-unit factors, so it cannot see the order. The first-time factor is part of Q, which is still a stand-in. The red test therefore pins the fold value from the decode.
- Green: `pes_kick_error_test` 12/12 with `PES21_ROOT` set (install test included).
- Build: `cmake --build build --target pes_kick_error_test` (fresh worktree, 4143 steps, about 60 min under load; godot-cpp symlinked per brief).

## Oracle (diff harness, scratch only)
- Spec: `softmin4.json` (scratch, not committed): `FUN_1408413C0`, args `xmm0..3` as f32, output `xmm0`, inputs uniform in [−0.25, 1.25] plus edges {1, 0, 0.5, −1, 1+ulp, 1−ulp}.
- Emulator: a scratch copy of `tools/dev/diffharness/run.py` (not the committed tool; `diffharness/` is RefReplay's). It adds one stub kind, `sort4_desc`, modelling the qsort call. Reason: the emulated CRT qsort reads an unmapped global at `0x12D0380` (CRT path `0x1403FB410`). The stub sorts descending, as the decoded comparator does. So the oracle partly trusts the comparator decode for the order, but checks the fold, start, half, and operation order against PES's code.
- Cases: 4,000 (seed 1) + 50,000 (seed 7) = **54,000 valid** (0 failed; `icount_max` 52). Checker: `softmin_oracle.cpp` links the test's own libraries.
- Result: port `1eb9d08d`: **0 mismatches / 54,000** bit-exact. Old fold: 45,217 / 54,000 mismatches.
- Real-call cases: none exist for this function. Adding them needs a rendezvous hook at `0x1408413C0` (Main's `hooks.cfg`).

## Mutations (one per term; each run, rebuilt, reverted; final state = `1eb9d08d`, diff vs HEAD 0 lines)
| # | term | mutation | test binary result | oracle |
|---|---|---|---|---|
| M1 | sort order | `std::sort` ascending | 2 fail | 45,898 mismatches |
| M2 | fold start | 1.0 → 0.5 | 5 fail | 43,842 |
| M3 | fold half | 0.5 → 0.25 | 5 fail | 48,194 |
| M4 | fold form | drop `+min` term | 5 fail | 48,194 (same count as M3; see note) |
| M5 | I (launch penalty) | `SoftMin4(1.0, …)` in BuildScatter | 2 fail (`AmpSoftMin…`, `PerfectKick…`) | n/a (wiring) |
| M6 | A (stat amp) | drop `+0.25` | 2 fail (same two) | n/a |
| M7 | R (pressure) | drop `rWeight = 0.4` | 1 fail (`PressureOnlyFromAnOpponentWithinReach`) | n/a |

- M4 note: M3 and M4 give the same mismatch count because both are wrong on every non-degenerate case. The degenerate cases (a zero factor) agree with PES. The two mutants are different formulas (checked by hand on `{1, 0.9, 0.5, 0.2}`: 0.0267 vs 0.0056).
- Not mutated: F (`kSituationNotPorted` on F1/F2) and Q (on `q`). Both are neutral stand-ins with no GF value to vary. Not run for budget.

## Not done / open (numbered)
1. **Q (situation product)** is 1.0. Missing: contact class (0.9/0.88/0.86), subclass, first-time/volley, body-turn, AI bits. GF has no PES kick kinds. Stand-in `kSituationNotPorted`, `ponytail:`. Needed for pressured first-time kicks.
2. **F1/F2** neutral (`0x1406B0250` flag, geometry of the target entity). Stand-in.
3. **run term** in R is 0 (`FUN_140723320` nearby-threat sum not ported; `ponytail:` in the code).
4. **angRamp** (`xmm8` at `0x14083EDEF`) omitted from D. INF(med); needs a trace of the code before `0x14083ED60`.
5. **Post-scales `FUN_1408414A0`** (S.scale ×0.1, Y.scale ×0.9, P.pSign rewrites) not ported. Needs kick kinds and motion ids.
6. **Yaw redraw loop** (≤6, `0x14083A550`) skipped. `ponytail:`.
7. **Shot pitch term** (`0.02·g`) omitted. `ponytail:`.
8. **Signed zero**: the oracle has +0 edges but no −0 edge; the fold's min/max ties on −0/+0 are not separately exercised.
9. **Other test binaries** linking `gfsim` were not run (budget). Only `pes_kick_error_test` was run. The gate was not run (Main gates).
10. `ulimit -v` was set once in the persistent shell by mistake (no Godot running) and reset to `unlimited`.

## Files
- Commit `1eb9d08d` on `KickSoftMin` (`src/sim/onthepitch/peskickerror.cpp`, `peskickerror.hpp`, `tests/sim/pes_kick_error_test.cpp`).
- Scratch only (`<workdrive>/gf-KickSoftMin-scratch`): `softmin4.json`, `run_softmin.py` (with the `sort4_desc` stub), `softmin4_corpus.jsonl`, `softmin4_big.jsonl`, `softmin_cases.txt`, `softmin_oracle.cpp`, `relink_oracle.py`, `run_mut.sh`, `sm_disasm.txt`, `cmp_disasm.txt`, `pressure_disasm.txt`.
