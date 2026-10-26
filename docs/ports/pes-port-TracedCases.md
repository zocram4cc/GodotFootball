# TracedCases: own-block coverage and bit-exact from traced real calls (26-10-26)

Branch `TracedCases` (from `origin/FullSnap` fccd8471), pushed as **e7b7428**; `git ls-remote origin TracedCases` confirms `refs/heads/TracedCases` at e7b7428. Scratch (snapshots, banks, runs) is PES-derived and not committed: `<workdrive>/gf-TracedCases-scratch/`.

## 0. Caveat from Main (EmuWatch), applied to every number below

The emulated H tick does not advance the match. Over 50 frames, MI+0x1198 never moves and MA's nearest-list is never written; world state is written outside H, by the pump dispatch `0x141B3EA30`. So **50 frames from one snapshot is one world state repeated** (team-internal state can evolve, positions do not). Therefore: coverage is reported per snapshot and over the union of the three snapshots; extra frames are not counted as extra situations; more snapshots add more than more frames.

## 1. Result

- **Coverage from traced real calls (the number that matters):** 21 of 50 impure functions reached over the three snapshots (snap1 20, snap2 19, snap3 19). Own-block coverage over all traced calls and all three snapshots: **13 of the 21 at or above 0.50**. Median over the 21 reached: 0.5455. Median over all 50 specs: 0.00 (29 unreached). Per spec: `traced_cov_all.json` (snap1-only: `traced_cov_snap1.json`).
- **Bit-exact on traced cases (driver, traced banks only, count 128, perturb):** 5 functions bit-exact and at floor (1404F8C70, 1404FB2A0, 140529490, 14052A820, 14057BA50). Another 11 are bit-exact below floor, and 3 have an empty corpus. 2 are unmodellable (1404F6C30, 1404FEDC0).
- **Full run, union (canonical banks_A + traced snap1-3 banks), count 128: `results_impure.json` committed (in e7b7428):** 46 of 50 translated, **15 bit-exact and at floor**, median own-block over the 46 with coverage **0.304**. Failure classes: harness cannot run 30, unmodellable callee 4, translator bug 1 (14064DE60, `ret_rax` mismatch on case 49, present in the canonical run too).
- **Canonical only, same count 128 (run for comparison):** 16 bit-exact and at floor, median 0.2987.
- **Committed baseline before my run** (the FullSnap-branch `results_impure.json`, read as-is): 16 diffed, 16 at floor, median 0.2987 over 46. This does not match the "15/50" in the brief; I report the file as measured.

## 2. Reach per snapshot (50 frames each, cap 32, no stop)

| Snapshot | Reached | Ticks | Wall | Stop |
|---|---|---|---|---|
| snap1-142012 | 20 | 100 | 369 s | none |
| snap2-142339 | 19 | 100 | 334 s | none |
| snap3-142654 | 19 | 100 | 335 s | none |
| **union** | **21** | | | |

Per-spec traced calls are in `traced_cov_all.json` (e.g. 14057A3E0: 3,350 calls per snapshot; 10,350 over three).

## 3. Own-block coverage from traced calls (union over snap1-3; leader hooks on the emulated tick)

Coverage = the driver's `block_coverage` on the function's own live blocks, from the block leaders executed in the emulated tick. Reached/total are own blocks.

| Function | Calls (3 snaps) | Own cov | Blocks |
|---|---|---|---|
| 1404386D0 | 150 | 0.038 | 9/238 |
| 1404405D0 | 150 | 0.288 | 32/111 |
| 1404F6C30 | 300 | 0.855 | 153/179 |
| 1404F8C70 | 300 | 0.750 | 213/284 |
| 1404FA720 | 300 | 0.091 | 2/22 |
| 1404FB2A0 | 300 | 0.438 | 7/16 |
| 1404FE9C0 | 300 | 0.532 | 117/220 |
| 1404FEDC0 | 300 | 0.674 | 91/135 |
| 140509A40 | 300 | 0.030 | 2/67 |
| 14051CF70 | 300 | 0.900 | 18/20 |
| 140529490 | 2650 | 0.848 | 39/46 |
| 14052A190 | 2000 | 0.222 | 8/36 |
| 14052A820 | 1400 | 0.603 | 219/363 |
| 14053B110 | 300 | 0.259 | 7/27 |
| 14057A3E0 | 10350 | 0.286 | 4/14 |
| 14057BA50 | 300 | 0.545 | 6/11 |
| 1405CE4F0 | 150 | 0.727 | 64/88 |
| 1405CE930 | 300 | 0.869 | 113/130 |
| 1405DE950 | 100 | 0.571 | 32/56 |
| 14062E240 | 100 | 0.527 | 78/148 |
| 140687310 | 1400 | 0.857 | 66/77 |

Floor (≥ 0.50), 13 (the JSON `floor_union` field agrees): 1404F6C30, 1404F8C70, 1404FE9C0, 1404FEDC0, 14051CF70, 140529490, 14052A820, 14057BA50, 1405CE4F0, 1405CE930, 1405DE950, 14062E240, 140687310. Below floor among reached: 1404FB2A0 (0.44), 14052A190 (0.22), 14053B110, 14057A3E0, 1404405D0, 1404386D0, 1404FA720, 140509A40.

## 4. Bit-exact, traced cases, and comparison

| Run | Specs | Bit-exact | Floor (bit-exact & own ≥ 0.50) | Median own (46 or 50) |
|---|---|---|---|---|
| Traced banks only (21 reached, count 128) | 21 | 5 | 5 | 0.286 (19 with coverage) |
| Canonical banks_A only (count 128) | 50 | 16 | 16 | 0.2987 |
| **Union canonical + traced (count 128), committed** | 50 | **15** | **15** | **0.304** |

Union floor list (15): 1404F8C70, 1404FB2A0, 140503430, 1405055E0, 140510D30, 140529490, 14052A190, 14052A820, 14057BA50, 1405AD720, 140639100, 140654E60, 14065DAB0, 140A539F0, 140A57650. Of these, 6 are traced-reached functions (1404F8C70, 1404FB2A0, 140529490, 14052A190, 14052A820, 14057BA50); the other 9 are canonical-only.

Differences: union lost **14053B110** (canonical 0.63 → union 0.30 own; still bit-exact on 175 cases, so it falls below the floor, not a mismatch). Cause ([INFERENCE] from the code path, not measured per case): `--perturb` takes `bank[(k+1) % N]` as the neighbour instance, so the union's larger N changes which inputs each case gets, and coverage moves with them. The floor count is therefore sensitive to the bank mix, not only to the traced cases. For 14052A190 the traced-only own coverage is 0.39 and the union's is 0.72; the union adds the canonical instances, so this is the same mix effect, not a traced-case effect ([INFERENCE]).

No traced case mismatched the translated side. The only mismatch in any run is 14064DE60 case 49 (`ret_rax`), identical in the canonical run (pre-existing).

**Verification of the recorded set:** `driver.py --check --record tools/dev/translate/results_impure.json --seed-bank banks_U --perturb --count 128`: all 15 recorded-diffed functions still diff bit-exact.

## 5. Changes (e7b7428)

- `tools/dev/diffharness/bankstore.py` (new): disk JSONL banks with a line-offset index; `Bank[k]` reads one instance; `merge` unions banks over source dirs (roots from source headers first). `--selftest` passes.
- `tools/dev/translate/frame_trace.py`: reservoir slots on disk (one JSON per slot, streamed into `banks/<VA>.bank.jsonl`); per-impure-function own-block leader hooks over the function's own decode span (`executed/<VA>.json`); `record.json` gets `executed_leaders`.
- `tools/dev/translate/driver.py`: `block_bounds()` factored out of `block_coverage` (same definition); banks read through `bankstore`; the diff main gets its instances from a binary file (argv[3]) instead of compiled-in C arrays (a bank of 32 instances is 100 to 250 MB of hex; source embedding cannot compile).
- `tools/dev/translate/diffmain.py`: `write_instances()` writes the instance file (u64 n, n offsets, per-region {off, len, bytes}); `gen()` emits a runtime reader `inst_apply()`; the old embedded seed arrays are removed.
- `tools/dev/diffharness/run.py`: `--seed-bank` reads `<tag>.bank.jsonl` through `bankstore.Bank` (streamed).
- `tools/dev/translate/traced_cov.py` (new): own-block coverage over the union of traced runs (per-run and union), summary with floor count and medians.
- `tools/dev/translate/results_impure.json`: the union full run (count 128, perturb).

Disk: banks_U 3.1 GB, banks_T 3.0 GB, one snapshot run 0.95 GB. Memory: frame_trace and the driver ran under `ulimit -v 2000000`; no run exceeded it.

## 6. Checks run

- `py_compile` on all changed files; `bankstore.py --selftest` (ok).
- Single-spec driver runs on the traced bank: 1404FA720 157 cases bit-exact (own 0.09, below floor). 1404F6C30 unmodellable (import slot 14252FDE0 has no name), same as the committed run.
- Full driver run (union) and canonical-only (count 128); `--check` 15/15.
- PII: no machine paths in changed files or in `results_impure.json`.
- Gate not run (Main gates). No mutations (harness tooling, no engine code).
- Worktree note: `git status` reports the `extern/godot-cpp` symlink as a submodule error; I staged explicit paths only.

## 7. Open items (numbered)

1. **Reach beyond the tick.** 29 specs are unreached. Main's analysis (ledger call graph): 15 are under the team tick but situational (stoppages, set pieces, events), and 15 are reached from H slot 2 `0x14041ECF0` or the per-player chain (`0x140925750` → `0x140925470` for 22 players; action-switch `0x1409F8030`). Next step: widen the emulated entry to H slot 2 and add the per-player driver. EmuWatch notes the per-player driver may fault without the pump's context; if it does, record the fault and move on. Not done here (scope).
2. Frames do not evolve the world (see §0). More situations need more snapshots (stoppage, set pieces) or the pump's dispatch.
3. 1404F6C30 (100 calls per snapshot; 0.855 traced coverage) is unmodellable: import slot 14252FDE0 has no name in the image. This blocks its bit-exact check.
4. 1404FEDC0 unmodellable: global store in callee 14159FD0F.
5. Floor count depends on the bank mix via perturbation (§4, 14053B110). Report floor counts with the bank list, not as a function property.
6. 14064DE60 `ret_rax` mismatch (case 49) is pre-existing; not investigated here.
7. Three specs have an empty corpus (14051CF70, 1405DE950, 140687310 in the traced-only run): the translated function never reaches its end under the harness.
8. Spec 1404386D0 (own 0.04 traced, 238 blocks) and 140509A40 (0.03, 67 blocks) have low traced coverage: their reached blocks are a small slice.


## Main's reading

Traced real calls cover 13 of the 21 reached functions at 50% or more (median 0.55), but the bit-exact floor did not
rise (15 union, 5 from traced cases alone), because the diff does not run the calls that produced that coverage: it
samples 128 instances from the trace bank and `--perturb` mixes them across cases, so the diffed cases cover less than
the trace did; and two of the best-covered functions (the team tick at 0.85, `0x1404FEDC0`) are refused as
unmodellable on an abort-trap import and a global store. These are harness gaps, not translation results.
