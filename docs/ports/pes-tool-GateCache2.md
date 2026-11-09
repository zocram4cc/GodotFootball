# GateCache2: GateCache follow-ups 1-4 (branch GateCache2, tip 94680db6)

**Status:** done on the branch. `GATE branch: GREEN` on `94680db6` (gate-branch.out; ctest 27/27 passed). `origin/GateCache2` = `94680db6` (ls-remote). Base: `origin/main` 2d0759c4 (the gate's base). `origin/main` is now `879f3e33`, 10 commits past the base; none of them touch the four files this branch changes (diff of those paths empty; CONFIRMED). Not merged: no merge was announced to this agent, and merging would need a new gate.

## Commits

- `8477b508` red tests for items 1 and 3 (file rewritten during its run; object cache cap).
- `bc2125de` red test for item 2 (edit to an unused stub entry keeps records warm); record fixtures use the path-to-digest shape.
- `94680db6` implementation: `tools/dev/translate/buildcache.py`, `driver.py`, `tools/dev/README.md`.

## What changed

1. **Digests at open (stale-hit risk).** `record_run` takes each file's digest when `run.py` opens it, and every module and mapped library before `run.py` starts. `corpus_store` keeps a run only while each recorded file still holds its open-time digest, so a file rewritten while `run.py` ran leaves no record. Before: `entry_doc` digested after the run, so a mid-run rewrite was stored under the new bytes and a later hit returned an output computed from the old bytes (CONFIRMED by the red test before the change).
2. **`stubs.json` is no longer a whole-file dependency.** `run.py` reads `stubs.json` only through `stubs.load`, `stubs.models` and `check_spec_refs` (CONFIRMED by reading `run.py` `resolve_stubs` and `stubs.py`). It installs every model (VA, `do`) in every run, and a legacy `stub_refs` must name a model. So a record depends on `stub_models_digest()` (the sorted (VA, `do`) pairs of the models). An edit to a why-text, a run-real or an unmodellable entry keeps every record. Adding, removing or re-kinding a model invalidates all records, which is correct because every run installs every model. The key is unchanged. A reach-based key was not built: it would need instrumenting every table address, and the used set is the same for every record anyway.
3. **Object cache cap.** `object_hit` touches an object on a hit; `object_trim` evicts least recently used `*.o` down to `OBJECT_CAP_BYTES = 4 GiB`, once per driver run, next to the record trim. Provenance is beside the constant: 37,835 objects, 628 MB apparent on 2026-11-09 (CONFIRMED by `du`/`ls`); the BE gate's core re-check alone hit 12,879 objects (from `pes-merge-BE.md`). In-flight `.tmp` compiles are neither counted nor evicted.
4. **Old `corpus/` directory.** Nothing reads it: `CORPUS_DIR` is `records`, and the other `"corpus"` strings are the results-JSON field and `test_bank_miss.py`. The shared on-disk `~/.cache/gf-diff-cache/corpus` (449 MB, 7,590 entries) had no entry modified since 2026-11-05 00:00 (CONFIRMED by `find -newermt`). It is deleted on disk; that is not in git.

Docs: `tools/dev/README.md` "Diff caches" now describes the record format, the object cap and the stub dependency.

## Tests

- `translate_buildcache` = `tools/dev/translate/test_buildcache.py`, run directly: 35 tests.
- Red on origin/main code, before any tool change (run and observed): `ObjectCacheCap` (4 errors: `object_trim`/`object_hit` missing); `RecordedFileChangedDuringTheRun.test_a_file_run_py_rewrites_after_reading_it_is_not_kept` (stale hit returned); `StubsJsonEntriesThatRecordsUse.test_an_edit_to_an_entry_no_record_uses_keeps_every_record_warm` (miss on a why-text edit).
- Controls green on origin/main code: `test_a_file_run_py_only_reads_is_kept`, `test_a_model_that_goes_away_invalidates_the_records`, and the existing 27.
- Green on the implementation: 35/35 OK.
- Mutations (each run alone, then reverted; suite re-run to 35/35 after each revert; the final tree was then built by the gate):
  - M1 digest taken at store time (the old behaviour): fails `test_a_file_run_py_rewrites_after_reading_it_is_not_kept`.
  - M2 whole-file stub digest: fails `test_an_edit_to_an_entry_no_record_uses_keeps_every_record_warm`.
  - M3 no stub-model check on lookup: fails `test_a_model_that_goes_away_invalidates_the_records`.
  - M4 trim never evicts: fails `test_the_least_recently_used_objects_go_first`, `test_a_hit_makes_an_object_recently_used`, `test_a_compile_in_flight_is_neither_counted_nor_evicted`; the under-cap control stays green.
  - M5 a hit does not touch the object: fails `test_a_hit_makes_an_object_recently_used`.
  - M6 `.tmp` compiles counted: fails `test_a_compile_in_flight_is_neither_counted_nor_evicted`.

## Gate: `GATE_TIER=branch` on `94680db6` (timeout 0, `ulimit -S -v unlimited`)

- Verdict: `GATE branch: GREEN` (CONFIRMED, last line of gate-branch.out). Diff jobs 2, ctest -j 2, load ~30.
- ctest: `100% tests passed out of 27`, total 8,379.72 s. Branch selection: 27 tests (`branch select`).
- Recorded checks ran and PASSED (not skipped), from `build/gate.ctest.log`:
  - `diff_core_recorded` 8,379.06 s (critical path; cold re-record, see residuals)
  - `diff_cutscene_recorded` 2,679.86 s; `diff_presentation_recorded` 1,734.71 s; `diff_pilot_recorded` 1,040.34 s
  - `diff_impure_recorded` 161.15 s (`GF_IMPURE_BANKS` set); `diff_bank_miss_recorded` 56.72 s
  - `diff_translated_drift` 615.17 s; `translate_buildcache` 2.00 s; `repo_pii_scan` 9.60 s
- `select_recorded`: all five sets fully re-checked (results.json 245, cutscene 685, presentation 546, core 2,253, impure 34), because `driver.py` and `buildcache.py` changed.
- Smokes (seed 7, 2 min): switch-off determinism `a0fbe2d5cd445a52`, pes-loop `17941efc16b4a6a5`. Both equal the main baselines in `tasks/06-11-26.md`, so unmoved.
- Ledger: no unrecorded status regression (18,857 rows compared, 160 scope moves recorded); off-path ports 120, all allow-listed; `LEDGER.md` current (38,338 rows).
- Seed drift: none reported. The branch tier does not census seeds, and nothing was re-staged.
- Test binaries run by this agent: `test_buildcache.py` directly (35 OK); the gate ran 27 ctest tests.

## Sample run: verdicts identical to origin/main

VAs `0x14239D310`, `0x1400020B0`, `0x141B109C0` (`specs/core`, `driver.py --only`, `--jobs 2 --cc-jobs 4`), origin/main in a pristine detached worktree vs GateCache2, each cold then warm, on fresh caches (CONFIRMED from the driver output):

| VA | verdict (both trees) |
|---|---|
| `0x1400020B0` | translated, diffed, 64 cases bit-exact |
| `0x14239D310` | translated, diffed, 42 cases bit-exact |
| `0x141B109C0` | translated, not diffed (`harness cannot run`, 1 valid of 64; unchanged) |

- `results.json` `results` arrays equal: base cold = new cold; base warm = new warm; cold = warm within each tree.
- Tallies equal: cold `corpus miss=3 object compiled=6` in both trees; warm `corpus hit=3 object hit=6` in both trees.

## Costs and residuals

- **One-time re-record (INF).** `buildcache.py` is the recorder, so every record is invalid once. The first gate after landing should re-record all five sets cold. This is inferred from the record-format change and `diff_core_recorded`'s 8,379 s wall; ctest prints no cache tallies, so the miss count is not logged. BE measured 5,193 s cold under another load; the difference is not isolated.
- **Image digest per record miss (measured hash, INF for the aggregate).** `pes_full.bin` is 463 MB and is in every record's `deps.files` with the digest `d3eb5ea3…` that `sha256sum` gives. `sha256sum` took 1.85 s wall (0.44 s user) on this loaded machine. Each record subprocess now digests the image at its open, where before it was digested once per driver process at store. The cold core re-check has about 2,253 misses, so this is roughly 2,253 extra image digests. The aggregate cost is INF, not measured. Ceiling: a per-run digest handoff from the driver. Not built.
- **Gap.** A module or shared library loaded without an open of its own (a `.so` loaded by the loader, or a `.py` without its own open) is digested at the end of the run, so a rewrite of it mid-run would not be caught. None observed.
- **Residual.** A file rewritten during the run and then rewritten back to its open-time bytes is not detected: there is no content check between open and close.
- **Object trim race (ponytail, named in the constant).** A trim may evict an object another driver process has just hit. That build's link then fails loudly; it never links a wrong object.

## Open items

1. `origin/main` is `879f3e33`, 10 commits past this branch's base. None of them touch the four changed files. Main's merge lane should merge and run the fast gate; the branch was gated on `2d0759c4`.
2. The first gate after landing re-records every set cold (about 2.3 h of gate wall here).
3. The per-record image digest (above) is a cost Main may want to measure on the first cold gate; the handoff that would remove it is not built.
4. Worktree `<workdrive>/gf-GateCache2` (with `build/`) and scratch `<workdrive>/gf-GateCache2-scratch` (cache-base, cache-base2, cache-new, out dirs) are left for Main to remove once the report is archived. The temporary pristine worktree `gf-GateCache2-base` used for the baseline is removed.
