# GateSpeed: diff gate wall time (object cache, corpus cache, jobs knob)

Branch `GateSpeed` (origin `af783a4e`), from origin/main `1e2930db`. Commits: `55291d5b` (driver, buildcache, tests, ctest),
`6f0cd5a2` (gate.sh, README), `af783a4e` (README timings). Worktree `gf-GateSpeed`, scratch `gf-GateSpeed-scratch`.

## Result (CONFIRMED: measured)

Full recorded core re-check, `driver.py --check` on `results_core.json` (2,253 diffed records), `--jobs 2` in all three runs:

| run | cache state | wall | vs baseline |
|---|---|---|---|
| baseline: origin/main, uncached, one serial g++ per VA | none | 10,697 s (2 h 58 min) | 1.00 |
| cold: this branch | empty | 7,226 s (2 h 00 min) | 1.48x |
| warm: this branch, same out dir | populated | 474 s (7 min 54 s) | 22.6x |

Cold run: 2,253 run.py runs; 7,046 objects compiled, 5,833 object hits (TUs shared across VAs in the same run).
Warm run: 2,253 corpus hits; 12,879 object hits. Load average: baseline 10 to 13; cold 10.3 to 13.2; warm 13.2 to 15.5
(shared machine, other agents and the live rig running).

Verdicts: every per-VA record (translated, diffed, cases, failure_class, note, coverage, dead_blocks) of the cold and warm
results equals the baseline's: 0 of 2,253 differ. All 2,253 stay bit-exact in all three runs.

Cache size for one core run: objects 140 MB (7,042 files), corpus 190 MB (2,253 entries), under `<build>/core-check/cache/`.

## Design

- **Object cache** (`cache/obj/<sha256>.o`). Key (`buildcache.tu_object_key`): compiler identity (realpath and version line),
  flags `-std=c++20 -O1`, the TU's basename and bytes, and every build-tree header it includes, transitively, by content.
  The `-I` path is not in the key. A quoted include that resolves nowhere makes the TU uncached (compiled per VA). Each TU
  compiles to a temp file, renamed into place; the build links the objects. The compile error filter is unchanged.
  Sharing across VAs needs path-independent objects: no `__FILE__`, `assert` or `source_location` in the emitted code or the
  layer. Checked by inspection of the xlate emitter and `src/pes/translated`; not a test.
- **Corpus cache** (`cache/corpus/<sha256>/`). Key (`buildcache.corpus_key`): run.py's input flags, plus the digests of the
  image, the spec, this VA's seed and bank files (or their absence), run.py, and the harness interpreter. run.py runs under
  `buildcache.py record`: an audit hook records every file opened (failed opens included), every module in `sys.modules`
  with a file, the mapped libraries, and every `capstone_decode` call (image, VA, digest of the result). A hit requires the
  key to match, every recorded file to hash as recorded, every recorded decode to recompute, and the `CCMAP` digest to
  match. Only successes are kept; failures run again.
  - `xlate.py`, `flow.py` and `pdata.py` are not file dependencies. run.py reaches the translator only through
    `xlate.capstone_decode` and `xlate.CCMAP` (grep: the only `_x.` uses), and both are recorded by value and recomputed.
    So a translator-only change hits, and a change to a decode result or the table misses.
  - Fidelity: the wrapper prints `sys.exit(text)` and prints an uncaught traceback from run.py's frame only. Tests compare
    stdout, stderr and rc with a direct `python run.py` run, byte for byte.
- **Jobs.** The five `diff_*_recorded` tests take `--jobs \${GF_DIFF_JOBS:-2}`. `diff_core_recorded` has COST 1000, so it
  starts first. `gate.sh` sets `GF_DIFF_JOBS` from MemAvailable (minus `GATE_RIG_MEM_GB`=10, at `GATE_DIFF_JOB_MEM_GB`=2 per
  job) and from cores (minus load and `GATE_RIG_CORES`=4), clamped to 2 to 6. Compile concurrency is `--cc-jobs` (default 4),
  shared by the whole run. On this machine at load 11.6 the gate chose 2.

## Verification

- **Unit tests** `translate_buildcache` (`tools/dev/translate/test_buildcache.py`, 20 tests, registered in
  `tests/pilot/CMakeLists.txt`, needs `GF_DIFF_PYTHON`): pass.
- **Killing mutants**, applied together to `buildcache.py`, then restored (6 failures, each named):
  1. object key ignores headers: `ObjectKey.test_an_included_header_changes_the_key_through_its_own_include`.
  2. corpus validity skips the decode recompute: `test_a_decode_that_can_no_longer_be_recomputed_invalidates_the_entry`.
  3. corpus validity skips file digests: `test_a_data_file_run_py_read_invalidates_the_entry`,
     `test_an_imported_module_invalidates_the_entry`, `test_a_file_run_py_tried_and_found_absent_invalidates_the_entry_when_it_appears`.
  4. exit text dropped: `test_a_sys_exit_message_is_printed_and_exits_as_a_direct_run_does`.
  After restoring: 20 of 20 pass.
- **End-to-end mutations** on a 4-VA select (0x1400020B0, 0x1400100D0, 0x140018F70, 0x14007BCC0), warm cache, each reverted and
  checked with `cmp`:
  - E1, neutral comment in `xlate.py` (translator-only): objects 14 of 14 hit; corpus 3 hit, 1 miss (the E2 VA). Hits.
  - E2, neutral key in spec `1400020B0.json`: corpus miss 1 (that VA only), hit 3; check bit-exact.
  - E3, neutral comment in `tools/dev/translate/stubs.py` (a module run.py loads): corpus miss 4; check bit-exact.
  - E4, semantic, one translated TU: `s.gpr[0] += 1` before every `return;` of 0x14007BCC0. Objects: 1 compiled (that TU),
    13 hit. Check RED: `1 recorded-diffed functions no longer diff: 0x14007BCC0`, rc 1.
    The first E4 attempt (one `+=1` at the head of the function) was not observable: later code overwrites rax on every path,
    and the check correctly stayed bit-exact. The mutant was strengthened, not the check.
- **Fast gate**: `GATE_TIER=fast GATE_SLOTS="3 2" GF_IMPURE_BANKS=<workdrive>/gf-Captures-scratch/banks_union9l tools/dev/gate.sh`
  on `af783a4e`: `GATE fast: GREEN`. 2,278 of 2,278 tests pass (`translate_buildcache` included). Smoke determinism
  `3b73c968b4a377e0` (switch-off) and `0e480b158cf80bd4` (pes-loop) match main's recorded baselines. Wall 1,588 s (26.5 min).
  The first run (3,072 s) was RED on `diff_impure_recorded` alone, because `GF_IMPURE_BANKS` was unset: the driver refuses
  bank-backed records before it applies the selection (0.10 s failure). The rerun with the bank set is the verdict.

## Open items and caveats

1. The fast gate's recorded re-check selected 0 VAs for this branch: `select_recorded.py` does not select on `driver.py` edits
   (the full tier does). So the fast gate does not exercise the caches on real VAs; the full core runs above do. Its 26.5 min
   is not comparable with batch AW's ~3 h critical path, which was the full core re-check (AW: 10,558 s; here 7,226 s cold,
   474 s warm). The full tier was not run: the pilot, cutscene, presentation and impure re-checks with the new driver are
   unverified by me.
2. Corpus soundness depends on run.py's translator use staying `capstone_decode` and `CCMAP`. If run.py starts using another
   `xlate` name, `buildcache.record_run` must record it by value too. Main should check this on any run.py edit.
3. Object sharing assumes path-independent objects (see Design). Inspected, not tested.
4. A link failure's note names a cache object path instead of the TU. Compile-error notes keep their form. No failure
   occurred in these runs.
5. The baseline was the origin/main driver (one g++ per VA, no cache); the cold and warm runs are the new driver at
   `--jobs 2`, with `--cc-jobs` 4. Only verdicts were compared across job counts (both runs used 2).
6. Warm's 474 s is translation, one binary run per VA, and validation. run.py is not run on a warm hit.
7. Decision for Main: the corpus cache's translator exemption (item 2 of Design) is the one interpretation of "a change to
   anything run.py reads must miss" that makes "xlate.py must hit" possible. It is value-based, not file-based.
