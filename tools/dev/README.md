# tools/dev

Developer scripts. The landing gate is `gate.sh`; sim runs hold a slot via `simslot.sh`.

## Gate tiers

```
PES21_ROOT=<install> GF_DIFF_PYTHON=<harness python> PES_EXE=<unpacked image> GF_IMPURE_BANKS=<bank dir> \
  [GATE_TIER=fast|full] [GATE_BASE=<rev>] [GATE_JOBS=<n>] [GATE_SLOTS="1 0"] tools/dev/gate.sh [build-dir]
```

The last line is `GATE <tier>: GREEN` or `GATE <tier>: RED <failing tests>`. The summary also prints the tier and, for fast,
how many recorded functions were selected per set.

| | **fast** (default) | **full** |
|---|---|---|
| build | yes | yes |
| ctest | everything except `SimMatch.*` and `gfsim_run_test` (whole-match tests; the smokes cover a match) | everything |
| `diff_*_recorded` (pilot, cutscene, presentation, core, impure) | **incremental**: only the recorded functions the change can reach | every recorded function |
| translated-layer drift (`gen_translated.py --check`, ctest `diff_translated_drift`) | yes | yes |
| ledger check | yes | yes |
| smokes, seed 7 | one 2-minute run per path (switch-off, `--pes-loop`) with their `[determinism]` lines | 5-minute prefilter, then 20-minute runs |
| target wall time | ~30 min | hours (the core re-check alone was ~2.8 h on 28-10-26) |

### When each runs

- **fast lands batches.** Main runs it on the merged tree before every landing.
- **full runs nightly, or every third batch**, whichever comes first. Record the full run (date, base, result line) in the
  batch notes (`tasks/DD-MM-YY.md`).
- **A red full blocks the next landing** until it is fixed or the failing function is shown to be an environmental flake
  (rerun alone, as for any gate failure).
- Run **full** (not fast) for a change to `driver.py`, the dispatch table (`gen_dispatch.py`) or anything the selector cannot see
  (below).

### The incremental recorded re-check

`translate/select_recorded.py` computes, per `results*.json`, the recorded VAs to re-check relative to `GATE_BASE` (default:
the merge base of HEAD and `origin/main`, so on a branch it is the branch's own change, on Main's merged tree it is the batch).
A recorded function is selected when

1. its generated file, or the generated file or spec of any function in its call closure, changed. The closure is read from the
   committed layer: every `pes_fn_<VA>.cpp` calls its callees as `pes_fn_<VA>(`, and `gen_translated.py` commits exactly the
   callees each record's diff build compiled; or
2. its record in `results*.json` is new or changed; or
3. the harness or the runtime headers changed (`stubs.py`, `stubs.json`, `diffmain.py`, `diffparse.hpp`,
   `diffharness/{run,bankstore,snapcase}.py`, `pes_state.hpp`, `pes_stubs.hpp`, `pes_cond.inl`, `pes_dispatch.hpp`): everything
   is selected. A translator edit (`xlate.py`, `flow.py`) is not in the list: what it changes shows as a diff of the generated
   files (item 1), and `diff_translated_drift`, in the same gate, fails if the committed layer is not what it now produces.

The gate writes the selection to `<build>/gate.select.json` and exports it as `GF_RECORDED_SELECT`; each `diff_*_recorded` test
then runs `driver.py --check --select`. With no selection for a set the test passes at once ("nothing to re-check"). Without
`GF_RECORDED_SELECT` (the full tier) they re-check every recorded function.

Not seen by the selector (full tier covers them): calls through the indirect-dispatch table, `driver.py` and
`gen_dispatch.py` edits, `translated.hpp` / `pes_dispatch_gen.hpp` (derived lists).
The selector's oracle is `ctest -R translate_select_recorded_test`: a changed callee two calls down selects its recorded caller,
an unrelated change selects nothing.

### Knobs

- `GATE_JOBS`: ctest `-j`. Default: free cores (`nproc` minus the 1-minute load average, at least 2).
- `GATE_SLOTS`: sim slots held, in the order acquired (default `"1 0"`, Main's gate). Both tiers hold two (one per smoke). A branch
  gate that must leave 0-1 to Main: `GATE_SLOTS="3 2"` (needs `GF_SIM_SLOTS=4`).
- `GATE_BASE`: fast only, what "changed" is measured against.
- `GATE_SKIP_PREFILTER=1`, `GATE_PRE_MINUTES`: the full tier's 5-minute prefilter.
- The PES diff suites run only with `GF_DIFF_PYTHON` and `PES_EXE` set (export both before `cmake -S . -B build` as well); a
  skipped diff test is then a red gate.
