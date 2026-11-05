# Batch BE: gate speed (GateCache + SeedShare), merged to main

**Contents:** `GateCache` (`feebd120`, plus the test-only `4a980b42`) and `SeedShare` (`3bfaf25a`), merged in `gf-MergeLane` as `MergeBatchBE`. Session reports: `pes-gate-GateCache.md`, `pes-gate-SeedShare.md`.

- **GateCache:** the shared diff cache's corpus stored one record per function, so up to six worktrees gating different trees kept evicting each other, and every gate re-ran every proof (BC: corpus miss 2253/2253). Records are now kept per harness state under `records/<key>/<digest>`, with a global LRU cap `CORPUS_CAP_BYTES` = 10 GiB; the key (`corpus_key`, v:2) is unchanged.
  - Soundness: a mutated translated function's object misses and its proof fails, while an unrelated function hits and passes.
  - `diff_bank_miss_recorded` pins that a bank-corpus miss re-records from `GF_IMPURE_BANKS`.
- **SeedShare:** tests that play the same match share one run (`tests/sim/pes_shared_match.hpp`), and each probe sees exactly the ticks it saw alone. `PesRestartDelay` + `PesStall` run as one ctest process, and the three carried throw-in tests share their runs. Assertions and seeds are unchanged; 24 old-form proof lines match the shared runs on frame and trajectory hash.

## Conflict

`tests/sim/pes_throwin_release_test.cpp`: SeedShare's `kSeed = 3` (the pre-BD carried seed) against main's `HOME_TEAM`/`AWAY_TEAM`. Neither is used in the merged file, because the shared probes take BD's re-staged `kThrowInSeeds`, so both were dropped. The binaries built.

## Gate

`GATE fast: GREEN` on `d0f98581` (main `5f7b12d7` + GateCache + SeedShare), slots 1 0, load ~26:
- 2,314 of 2,314 passed.
- Smokes switch-off `399e180f39459c2b`, pes-loop `85c7d7c515cf92ae`, both equal to main.

| | BC gate (`0dd1c2b9`) | BE gate |
|---|---|---|
| ctest wall | 15,858 s | 8,798 s |
| `diff_core_recorded` | 6,091 s (corpus miss 2253) | 757 s (corpus hit 2093, miss 160) |
| longest seeded test | PesRestartDelay, 1,071 s | PesForfeitMatch, 506 s (its six matches diverge, so not shared) |

After the gate, `4a980b42` (test-only) and main (`58e0acf6`: batch BF FlatKeys, the slot change, docs) were merged. Main's BF is a product change this gate did not cover. The merged tree was built, and both smokes were re-run on it and both smokes re-run on it: switch-off `399e180f39459c2b`, pes-loop `85c7d7c515cf92ae`, unmoved; `ledger.py --check` current.

## Left

- From GateCache, not fixed: recorded-file digests are taken after the run, not when each file is opened, so a file edited mid-run could give a stale hit. Fix: digest at open in `record_run`. Also: `stubs.json` is a whole-file dependency; the object cache has no size cap; the old `corpus/` directory can be deleted once no lane runs old code.
- From SeedShare: six tests lack an old-form comparison (their verdicts pass); cross-seed and perturbing tests still run their own matches (~1,900 s of baseline); throw-in duplicates a seed-3 run the restart family also plays (~500 s); families ≥120 s other than these four are unexamined.
