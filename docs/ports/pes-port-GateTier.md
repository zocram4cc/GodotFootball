# GateTier: two-tier landing gate (fast / full) with an incremental recorded re-check

Branch `GateTier` (worktree `gf-GateTier`, off `f99e35ee`), pushed; last commit `672882cd`. No PES decode: tooling only, no VAs.

## What changed

| File | Change |
|---|---|
| `tools/dev/gate.sh` | `GATE_TIER=fast\|full` (default `fast`); `GATE_JOBS` default = free cores (`nproc` minus 1-min load, floor 2); `GATE_SLOTS` (default `"1 0"` = today's slots; two slots held, one per smoke); `GATE_BASE`. Summary prints the tier, and for fast the per-set selection counts. Last line `GATE <tier>: GREEN\|RED <failing tests>`. |
| `tools/dev/translate/select_recorded.py` (new) | The selector (below). CLI: `--base REV --out FILE`. |
| `tools/dev/translate/test_select_recorded.py` (new) | 9 tests; ctest `translate_select_recorded_test` (plain `python3`, never skips). |
| `tools/dev/translate/driver.py` | `--check --select FILE`: narrows the check to the selected recorded VAs of `--record`'s file; an empty selection passes ("nothing to re-check"). 12 lines. |
| `tests/pilot/CMakeLists.txt` | The five `diff_*_recorded` tests pass `--select $GF_RECORDED_SELECT` when it is set (unset = every recorded function, as before). |
| `tests/CMakeLists.txt` | Registers the selector test. |
| `tools/dev/README.md` (new) | The tiers, when each runs, the selector rules, the knobs. |

### Tiers
- **fast:** build; ctest minus `SimMatch.*` and `gfsim_run_test` (whole-match tests; the smokes cover a match). That keeps every `Pes*`/`pes_*` test, the python tooling tests (`translate_*`, `parity_metrics_test`, the new selector test), the godot tests, `diff_gen_*`, `diff_image_verify` and `diff_translated_drift` (= `gen_translated.py --check`); the five `diff_*_recorded` run incrementally; ledger check; one 2-minute seed-7 smoke per path with its `[determinism]` line. No 5-minute prefilter (the smoke is already 2 minutes).
- **full:** exactly today's gate (5-min prefilter, 20-min smokes, whole ctest, complete re-checks). `GF_RECORDED_SELECT` is exported empty.

### The selector (`select_recorded.py`)
Closure source: the committed layer. `gen_translated.py` commits, per recorded function, exactly the callees its diff build compiled (transitively), and each `pes_fn_<VA>.cpp` calls its callees as `pes_fn_<VA>(`; the selector reads that call graph. A recorded (diffed, not `proof: snapdiff`) VA is selected when:
1. its generated file, or the generated file **or spec** (`diffharness/specs/<set>/<VA>.json`) of any function in its closure, changed since the base;
2. its record in `results*.json` is new or changed since the base (read with `git show <base>:`);
3. the harness or runtime headers changed (GLOBAL: `stubs.py`, `stubs.json`, `diffmain.py`, `diffparse.hpp`, `diffharness/{run,bankstore,snapcase}.py`, `pes_state.hpp`, `pes_stubs.hpp`, `pes_cond.inl`, `pes_dispatch.hpp`): everything.

Design decisions (stated, not hidden):
- A translator edit (`xlate.py`, `flow.py`) is **not** GLOBAL: its effect shows as a diff of the generated files (rule 1), and `diff_translated_drift` in the same fast gate fails if the committed layer is not what the translator now produces. Making it GLOBAL would turn every translator commit into a multi-hour check.
- Not seen (full tier covers): calls through the indirect-dispatch table, `driver.py` and `gen_dispatch.py` edits, and the derived lists `translated.hpp` / `pes_dispatch_gen.hpp`.
- Selection size estimate on history (today's graph, each commit's own diff): `1c719193` (two functions recorded) selects 2 core; `fd42e177` (Batch AJ regeneration) 9/16/20/53/14 per set (pilot/cutscene/presentation/core/impure); a stubs.json or runtime-header change selects everything.

## Selector test (oracle): red, green, mutations
- **Red first:** the test file was run before `select_recorded.py` existed: `ModuleNotFoundError: No module named 'select_recorded'` (a red that cannot be confused with a pass).
- **Green:** 9 tests OK. Synthetic layer (R recorded → C closure → L leaf; U recorded, calls nothing; N not diffed, also calls C): a changed closure callee C selects R; a changed leaf two calls down selects R; U's file selects only U; unrelated paths (`src/gf/...`, docs, tests, `translated.hpp`) select nothing; a changed spec selects its function; stubs.json / `pes_state.hpp` / `run.py` select everything; a new/changed record is selected; snapdiff records never. Plus a **real-layer** test: for a real recorded function with a call chain ≥ 2 deep, its direct and its deep callee each select it.
- **Mutations (each run, then reverted; selector back to the committed hash):**
  1. `closure()` returns only the root → 3 tests fail (callee, leaf, real-layer).
  2. `closure()` one level only → 2 fail (two-level leaf, real-layer).
  3. `everything = True` (over-select) → 6 fail (incl. `unrelated_changes_select_nothing`).
  4. `everything = False` (globals ignored) → 1 fails (`harness_or_runtime_header_change_selects_everything`).
- Real mutated callee on the committed layer: appending a comment to `closure/pes_fn_1403FB410.cpp` made the CLI select 2 `results_impure.json` records (then reverted).

## Fast tier on the branch (`GATE_TIER=fast`, `GATE_SLOTS="3 2"`, `GF_SIM_SLOTS=4`, image + harness Python + bank set)
- **Result line: `GATE fast: GREEN`.** Wall time **96 min 52 s** (user 364 min). Of that, ctest 5,533 s with `-j 2`: the machine ran at load 28-40 on 16 cores, so the free-cores default floored to 2 jobs. The ~30 min target needs free cores: ctest's CPU sum (≈ 11,900 s per the AO gate log, without the `diff_*_recorded` tests) is divided by `GATE_JOBS`, and the longest tests (`PesRestartDelay.*`, `PesThrowInRelease.*`, 400-810 s under this load) are the floor. The branch's own change touched no generated file, so the selection was 0 of 3,763 recorded (pilot 0/245, cutscene 0/685, presentation 0/546, core 0/2,253, impure 0/34) and the five `diff_*_recorded` tests passed in 0.14-0.19 s each ("nothing to re-check").
- ctest: 100% passed, 2,044 tests (`diff_translated_drift` 393 s, `diff_image_verify` and the 4 `diff_gen_*` passed, `translate_select_recorded_test` passed). Ledger current (37,863 rows; 129 off-path ports, all allow-listed).
- Smokes, seed 7, 2 min, both reached full time: switch-off `[determinism] baaad005c17a2005` (33,755 ticks, 16 stoppages), `--pes-loop` `07dec280e80c0db7` (25,975 ticks, 9 stoppages). These are 2-minute hashes, not the 5/20-minute baselines in `tasks/28-10-26.md`; no comparison to those is claimed.
- The non-empty path, end to end through ctest: `GF_RECORDED_SELECT` with `results_core.json: [0x14063A370]` → `diff_core_recorded` ran `driver --check` on that one function: `64 cases bit-exact; own-block coverage 1.00`, passed in 3.4 s; `diff_cutscene_recorded` with an empty list: "nothing to re-check".
- The full tier was not run (as ordered). Its shell path was exercised only with stubbed `ctest`/`gfsim_run` (prefilter, no `-E`, empty `GF_RECORDED_SELECT`, RED line naming `diff_core_recorded`); a bad `GATE_TIER` exits 2.
- The first build of the fresh worktree compiled 5,310 steps (about 60 min at `-j3` then `-j6` under the same load); the gate's own build step was then a no-op.

## Open items
1. **Slots.** `GATE_SLOTS` default `"1 0"` keeps Main's lane on slots 0-1; on this run slots 2-3 were held by other jobs for part of the time (queueing is inside the 96 min, I did not separate it).
2. **Selector blind spots** (listed above): dispatch-table-only callees, `driver.py`/`gen_dispatch.py` edits. A change to either wants the full tier (README says so).
3. **Cadence is policy, not code:** "full nightly or every third batch, recorded in the batch notes; a red full blocks the next landing" is documented in `tools/dev/README.md`; nothing enforces it. Main's lane should record each full run (date, base, result line) in `tasks/DD-MM-YY.md`.
4. `tasks/28-10-26.md`'s "never edit gate.sh while a gate runs" still applies when this branch lands: land between gates.
5. No `GATE GREEN` string consumers found in the repo (grep over tools/tests/docs/plans); the last line is now `GATE <tier>: GREEN`.

## Checks run
`python3 tools/dev/translate/test_select_recorded.py` (9 OK, plus 4 mutations above); `ctest translate_select_recorded_test` passed inside the fast gate; `GATE_TIER=fast tools/dev/gate.sh` GREEN as above; `diff_core_recorded` with a one-VA selection passed.
