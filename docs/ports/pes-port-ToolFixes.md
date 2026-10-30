# pes-port-ToolFixes: finish_one skips undecodable coverage entries; stub-table note names the stem

Branch `ToolFixes`, from origin/main `682b52b7`. Pushed commits: `aacf055e` (driver fix, red test, ctest registration) and `12b601b5` (stubs.json text). `git ls-remote origin refs/heads/ToolFixes` = `12b601b557b9d9fad74c28cd3d8ba89c2dc7e440`.

## 1. Crash in the impure check (tools/dev/translate/driver.py, finish_one)

- Cause (CONFIRMED by traceback): the record comprehension `for v, (r, t) in cov.items() if isinstance(cov[v], tuple)` unpacks `(r, t)` before the filter runs. `block_coverage` maps an undecodable function to `None`, so the unpack raises `TypeError: cannot unpack non-iterable NoneType object`. On `682b52b7` it is at driver.py:880. (The brief's "~833" is a different base.)
- Fix: skip non-tuple entries first, then unpack. Recorded fields are unchanged: `reached`, `total`, and `fraction = round(r / t, 4)` (0.0 when `total` is 0).

## 2. Red-first test: `translate_finish_coverage`

- `tools/dev/translate/test_finish_coverage.py` (plain python3, unittest), registered in `tests/pilot/CMakeLists.txt`.
- It drives the real `finish_one` with `block_coverage` returning `{TOP: (3, 4), CALLEE: None}`. `run_one`, `load_spec`, `runnable_extra` and `dead_blocks` are stubbed. It asserts `coverage == {TOP: {reached 3, total 4, fraction 0.75}}`. Synthetic VAs only; no PES bytes.
- Red on the unfixed driver: `ERROR ... driver.py line 880 in finish_one: TypeError: cannot unpack non-iterable NoneType object`.
- Green after the fix: `OK`. ctest `translate_finish_coverage` 1/1 Passed (0.16 s).
- Mutants, each run then reverted (`git diff HEAD -- driver.py` empty afterwards):
  - M1, guard dropped (`if False:`): `TypeError: cannot unpack non-iterable NoneType object`, FAILED (errors=1).
  - M2, `None` recorded as `(0, 1)` instead of skipped: `AssertionError`, extra key `0x140002000 {'reached': 0, 'total': 1, 'fraction': 0.0}`, FAILED (failures=1).

## 3. Stale dispatch-table name (tools/dev/translate/stubs.json:12)

- `native_services_contract` named `pes_dispatch_gen.hpp`. It now reads `pes_dispatch_gen` (stem only). Text only; no table change.
- Deviation from the brief: not `pes_dispatch_gen.cpp`. The `.cpp` table is the LayerHeaders change, and it is not on origin/main: `2cc86e0d` is not an ancestor of `682b52b7`, and `gen_dispatch.py` on main still writes `pes_dispatch_gen.hpp`. The stem is true before and after that merge. LayerHeaders does not touch stubs.json, so without this edit the stale name survives its merge. If `.cpp` is wanted, it is one token once LayerHeaders is on main.

## 4. Impure check: `diff_impure_recorded`

- Passed, rc 0, 853.71 s. Env: `PES_EXE` = `pes_full.bin`, `GF_DIFF_PYTHON` = the harness venv, `GF_IMPURE_BANKS` = `<workdrive>/gf-Captures-scratch/banks_union9l`, `PES21_ROOT` = `<workdrive>/PES21`. Run in a sim slot under `ulimit -v 2000000`.
- Output: `driver: attempted 34 translated 34 diffed 34`; `driver --check: all 34 recorded-diffed functions still diff bit-exact`.
- Pre-fix crash: the red test reproduces the same TypeError at the same line. The impure check was not re-run on the unfixed tree (about 15 min). LayerHeaders recorded the traceback there (pes-port-LayerHeaders.md, gate section).

## 5. Other ctest runs (build dir configured with the gate env; nothing rebuilt, no C++ changed)

- `translate_finish_coverage` 1/1 Passed.
- `translate_select_recorded_test` and `translate_native_services` Passed (3/3 with the test above).
- `diff_translated_drift` Passed (269.84 s): the committed layer regenerates byte-identical with the driver and stubs.json edits.

## 6. GATE fast: interim, not run

`tools/dev/gate.sh` starts with `cmake --build build -j3`. On a fresh worktree that is godot-cpp plus the whole translated layer (about 4,000 TUs; pes-port-LayerHeaders counts 4,076 compiles). Any stubs.json change also makes `select_recorded.py` select every recorded function (pes-port-GateTier.md). On a fresh tree at load ~30 that is hours, so the targeted checks above were run instead.

`GATE fast (interim): NOT RUN, fresh build dir. Targeted: translate_finish_coverage, translate_select_recorded_test, translate_native_services, diff_translated_drift, diff_impure_recorded all Passed.`

## 7. Open items

1. The fast gate line is not produced (section 6). Main gates the batch; expect the stubs.json edit to select the full recorded set.
2. Sibling caller with the same hazard, not changed (outside the brief): `tools/dev/translate/traced_cov.py:51-52`. `pc = block_coverage(...)[va]` is `None` for an undecodable VA that has recorded hits, and `d["reached"], d["total"] = pc` then raises. A one-line skip-None guard fixes it. `snapdiff.py:332` already handles `None`.
3. stubs.json names the stem. Add `.cpp` when LayerHeaders lands, if wanted.

PII: 0 added lines carry machine paths (workdrive and home paths). Diff against `682b52b7`: 4 files, 55 insertions, 5 deletions.
