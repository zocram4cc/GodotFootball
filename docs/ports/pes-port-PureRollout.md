# pes-port-PureRollout: six pure subsystems through the pipeline (final, 26-10-26)

Branch `PureRollout`, worktree `<workdrive>/gf-PureRollout`, scratch `<workdrive>/gf-PureRollout-scratch`. Head pushed: `5d6d00bb` (ls-remote confirmed). Image `pes_full.bin`; harness Python = DiffHarnessRE venv. Gate not run (Main gates).

Status: the core set ran end to end on the final harness and its record is committed (`1cbd8a28`). **The committed layer is NOT regenerated.** `gen_translated.py` refuses to build (see Open items 1 and 2), so the layer and the `SETS` core entry are pending.

## Commits (in order)

| SHA | What |
|---|---|
| `b8ec08f0` | WIP: core set (`specgen/sets/core.txt`, 2,856 rows), specs (`specs/core`, 2,856), driver `--exclude-recorded`, gate test `diff_core_recorded` |
| `1e3e664d` | Harness oracle fix: a case that does not return is dropped, not recorded (run.py) |
| `f051864b` | Harness parser fix: 64-bit values parsed with `strtoull` (diffparse.hpp) |
| `ec66e683` | Translator fix round: 8/16-bit shift counts masked to 5 bits; packed `CMPccPS` predicates (xlate.py) |
| `c60a2691`, `32b01c22` | Merges of origin/main (batch AF, then AG) |
| `d83f7f55` | Harness: every skipped case contributes nothing; per-function non-return cap; measured instruction budget (run.py, driver.py) |
| `1cbd8a28` | Core record `results_core.json`: 2,570 rows from the full run on the final harness |
| `5d6d00bb` | Driver note fix: an empty corpus reports 0 valid cases |

## Set and attempt rule

- `specgen select --subsystem ball referee restart animation pre-match gameplay`: 2,856 closure rows (2,539 seeds, 317 pure callees), each tagged with subsystem and role. A callee belongs to the first subsystem that reaches it.
- `driver --exclude-recorded`: rows that another committed `results*.json` already records are not re-attempted (286 rows). Their specs stay in `specs/core` as the callee universe; without them a seed's call to a recorded callee would become `rax = 0`.
- Attempted: 2,570 rows (2,489 new seeds, 81 new callees).

## Results: final record (final harness)

Seeds bit-exact at the 50% own-block floor, per subsystem. First pass = untouched translator and the first harness (before).

| subsystem | seeds | first pass | final | coverage median (p25/p75), translated seeds | final not-bit-exact seeds, by class |
|---|---|---|---|---|---|
| ball | 184 | 134 (72.8%) | 144 (78.3%) | 1.00 (0.57/1.00) | harness cannot run 38, translator bug 1, struct map gap 1 |
| referee | 54 | 36 (66.7%) | 39 (72.2%) | 1.00 (0.46/1.00) | harness cannot run 15 |
| restart | 302 | 240 (79.5%) | 252 (83.4%) | 1.00 (0.75/1.00) | harness cannot run 49, translator bug 1 |
| animation | 256 | 150 (58.6%) | 160 (62.5%) | 0.80 (0.25/1.00) | harness cannot run 95, translator bug 1 |
| pre-match | 361 | 249 (69.0%) | 270 (74.8%) | 1.00 (0.50/1.00) | harness cannot run 88, struct map gap 3 |
| gameplay | 1,332 | 742 (55.7%) | 895 (67.2%) | 0.86 (0.33/1.00) | harness cannot run 421, translator bug 7, struct map gap 9 |
| **all seeds** | **2,489** | **1,551 (62.3%)** | **1,760 (70.7%)** | | harness cannot run 706, translator bug 10, struct map gap 13 |

Callees: 68 of 81 bit-exact at floor. Rows bit-exact at floor in the record: 1,828 of 2,570 (all roles). Of the 706 seeds in "harness cannot run", the verdicts that are bit-exact but below the floor are ball 21, referee 9, restart 15, animation 21, pre-match 31 and gameplay 316. The floor rule refuses them; they are not translator failures. The rest have empty or half-valid corpora (see the valid-case rule below).

## Fix round: before and after

Translator fixes (general, in xlate.py; no spec or generated file edited):
1. Shift counts: `SHL/SHR/SAR` masked the count to the operand width. `0x140A7B410` (`shr ax, cl` with count 16 to 31; a 16-bit result of 0) mismatched on 21 of 64 cases. After: bit-exact on 64 of 64. The 32- and 64-bit emitted text is byte-identical.
2. Packed compares: `CMPEQPS`, `CMPLTPS`, `CMPLEPS`, `CMPUNORDPS`, `CMPNEQPS`, `CMPNLTPS`, `CMPNLEPS`, `CMPORDPS` were `UNHANDLED` no-ops. `0x1403ED250` (`cmpneqps` then `movd`) mismatched on all 70 cases. After: all 70 match, but own-block coverage is 0.40, below the floor, so the verdict is "harness cannot run".

Harness fixes (outside the xlate-only scope; each approved by Main as a separate commit):
- Oracle (`1e3e664d`): a PES run that does not return was recorded as an output (unicorn stops silently at the timeout). `0x140A54770`: 29 of 68 cases with `d = r9 - r8 <= 0` are non-returns; before, the translated loop hung and was classed "translator bug".
- Parser (`f051864b`): `strtoll` saturated 64-bit values >= 2^63 to `INT64_MAX`. Nine of the first-pass mismatches read "want 7FFFFFFFFFFFFFFF" against negative PES results. The four recorded sets are unchanged by this fix (183, 8, 549, 472 bit-exact).
- Skipped cases (`d83f7f55`, Main's rule): a case that does not return within `CASE_INSN_BUDGET`, or fails (unmapped access, emulation error), contributes nothing (coverage and call traces rolled back). Each function has a cap of 3 non-returning cases; after the third it generates no further cases, and the note says so. A verdict needs valid cases of at least half the requested count, and the floor is computed on valid cases only. Every note reports valid, skipped and capped counts.
- Budget: `CASE_INSN_BUDGET = 4 * 2,377,688 = 9,510,752`. Measured with `run.py`'s per-case `valid_icounts`: the largest valid case of pilot (252 specs), cutscene (833), presentation (639) and core (2,570) is 2,377,688 instructions at VA `0x1405628D0` (core). That is below 5M; the 4x budget is above 5M, and it is stated here, not forced to 5M. Pilot, cutscene and presentation ran every case. Core ran under the cap, so 9 capped core functions had later valid cases that were not measured. Valid-case counts: pilot 18,411, cutscene 47,318, presentation 37,235, core 170,962 (54 valid cases above 1M, none above 5M).

Before and after, the core totals (bit-exact rows, all roles):
- First pass, untouched translator and first harness: 1,611.
- + oracle and parser fixes (re-run of the 30 translator-bug rows): +15 (for example `0x140A54770`, nine negative-64-bit rows).
- + shift fix: +1 (`0x140A7B410`). Packed compare fix: 0 (below floor).
- = 1,627 (the pre-merge run).
- After merging AF and AG, with the old skip rules: 1,895 (inflated: 270 rows became bit-exact on 1 to 2 valid cases while their failed cases counted toward coverage).
- Final skip rules (valid-only coverage, half-valid rule, cap 3): **1,828**. Of the 270 inflated flips, 203 survive and 67 become "harness cannot run".

## Verification (exercised)

- `driver --check` on the final harness: pilot 183/183, cutscene 549/549, presentation 472/472, core 1,828/1,828 (`diff_core_recorded`, run by hand on the record). Impure against the AG record with banks `banks_union9` (newest 9-capture set; assumed canonical, see Open items): 14/15; 0x1404FB2A0 loses its verdict (below).
- Earlier checks under each commit: oracle (`1e3e664d`) and parser (`f051864b`) and translator (`ec66e683`) changes each kept pilot 183, cutscene 549, presentation 472 and impure 8 bit-exact (before the AG record).
- Merged AG, before this round's skip rules: pilot, cutscene and presentation bit-exact; impure 5 of 16 did not diff (0x1404F8C70, 0x140503430, 0x140529490, 0x14053B110, 0x1405AD720). A pre-fix snapshot reproduces the same five outcomes exactly, so they are not from the translator change. They come from the record's capture-union configuration, which the default check does not reproduce (the record was not checked with its banks).
- Measurement: every row of the four recorded sets was run through `run.py` alone for its valid-case instruction counts (1 pilot, 7 cutscene, 3 presentation and 14 core specs crash in `run.py` before any case: IndexError or `UC_ERR_WRITE_UNMAPPED`; they have no valid case).
- Machine-path scan of changed files (`run.py`, `driver.py`, `xlate.py`, `gen_translated.py`, `results_core.json`): no hits.
- Layer: `gen_translated.py --image` refused: "closure members with a recorded translator bug: 0x14037CB50 0x141F5B810". Nothing was written to `src/pes/translated`.

## Losses and corrections, by VA

Bit-exact in the pre-merge run, not bit-exact in the final record (5). In every case the new rule removed valid cases:
- `0x140388710` (gameplay): 0 valid, 3 non-returning, capped after 3 (71 cases not generated).
- `0x14049FAF0` (gameplay): 2 valid of 64, 1 non-returning, 76 failed.
- `0x140A54770` (ball, the oracle-fix showcase): 3 valid of 64, 3 non-returning, capped (66 not generated). Its PES loop does not return for 29 of its 68 inputs; the cap stops generation at the third.
- `0x140A94540` (animation): 14 valid of 64, 3 non-returning, capped (60 not generated).
- `0x141350F90` (gameplay): 1 valid of 64, 68 failed.

Impure record (Captures' AG record, banks `banks_union9`): `0x1404FB2A0` loses its verdict. Record: bit-exact, own-block coverage 0.75 (counted blocks of 40 failed cases). Now: 42 valid of 64 requested, 40 failed skipped, own-block coverage on valid cases 0.31 < 0.50.

Flips from the inflated merged run: 270 rows (struct map gap before, bit-exact on 1 to 2 valid cases in the inflated run). 203 survive the final rule; 67 are "harness cannot run".

## Translator defects left (VA-level evidence; not fixed)

1. `0x14037CB50` (gameplay seed): the translated code takes a tamper-abort branch (SIGILL, diff binary exit 132) on cases where PES does not. The flag divergence is upstream and not located. The tamper stub is in `stubs.json`; no rip-relative call to the abort slot appears in the first 0x900 bytes. This row blocks the layer: its caller `0x1403BB670` is recorded bit-exact.
2. `0x14114B850` (gameplay seed): the same trap as (1).
3. `0x140448310` (restart seed): `w_A1_0_d0` want 65535 got 4 on 66 of 66 cases. Not diagnosed.
4. `0x1404B1A60` (gameplay seed): `ret_eax` want 21800000 got 21580601 on 13 of 108 cases.
5. `0x1404BEFF0` (gameplay seed): `w_A0_0` want 541065216 got 92 on 96 of 96 cases.
6. `0x140555110` (gameplay seed): `ret_rax` want 22800088 got 133 on 62 of 64 cases.
7. `0x14058F430` (ball seed): `ret_eax` want 0xF500 got low bits 0xFF00 on 12 of 88 cases.
8. `0x14068FD90` (gameplay seed): `ret_eax` want 21400000 got 0 on 42 of 87 cases.
9. `0x1406B97D0` (animation seed): `ret_xmm0` want 3E99999A got 0 on 1 of 73 cases.
10. `0x1409F6960` (gameplay seed): `ret_eax` want 34B900 got 20D80600 on 12 of 119 cases.

## Open items (decisions for Main)

1. **Layer blocked (guard).** `0x14037CB50` (item 1 above) is a translator defect and a closure member of a recorded bit-exact caller. Fix it in a follow-up translator task, or decide how the guard treats a closure member whose caller is bit-exact. The `SETS` core entry is saved as a patch at `<workdrive>/gf-PureRollout-scratch/gen_translated-core-SETS.patch`; it stays out of the branch until the layer regenerates.
2. **Stale cutscene row `0x141F5B810`.** The committed cutscene record still says "translator bug, diff timeout after 600 s" (old harness). On the final harness it has 0 valid cases (3 non-returning, capped after 3), so it is "harness cannot run". The guard reads the committed row, so the row needs an approved update, or a full cutscene run on the final harness.
3. **Impure `0x1404FB2A0`.** It now fails the valid-only floor (0.31). Decide whether its 40 failed cases are a bank or instance-binding fault (fix the banks) or accept the loss.
4. **Cap.** The cap of 3 turns `0x140A54770`-type functions (43% of inputs never return) into harness-cannot-run. Decide whether the cap should be per function or a fraction of the requested count.
5. **Failed cases.** 25,517 failed cases (unmapped access or emulation error) in the core record; most are struct-map gaps in the specs. Generator work.
6. **Crashing specs.** 1 pilot, 7 cutscene, 3 presentation, 14 core specs crash in `run.py` before any case (IndexError, `UC_ERR_WRITE_UNMAPPED`). Generator work.
7. **Canonical banks.** `banks_union9` (`<workdrive>/gf-Captures-scratch`, newest of the 9-capture sets) is assumed canonical. With it the impure record reproduces 14 of 15 (the earlier 5 failures were without banks). Captures to confirm.
8. **Measurement limit.** Core's 9 capped functions were measured only up to the cap, so the budget is the maximum over cases generated, not over every possible case.
9. **Stale notes.** Empty-corpus rows in the committed core record still say "64 valid of 64 requested" (fixed in the driver in `5d6d00bb`; the next full run corrects the notes).
10. **Gate.** `tools/dev/gate.sh` not run (Main gates). `diff_core_recorded` added (`b8ec08f0`) and run by hand: 1,828 of 1,828.
