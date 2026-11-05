# tools/dev

Developer scripts. The landing gate is `gate.sh`; sim runs hold a slot via `simslot.sh`.

## Gate tiers

```
PES21_ROOT=<install> GF_DIFF_PYTHON=<harness python> PES_EXE=<unpacked image> GF_IMPURE_BANKS=<bank dir> \
  [GATE_TIER=fast|branch|full] [GATE_BASE=<rev>] [GATE_JOBS=<n>] [GATE_SLOTS="3 2"] tools/dev/gate.sh [build-dir]
```

The last line is `GATE <tier>: GREEN` or `GATE <tier>: RED <failing tests>`. The summary also prints the tier and, for fast,
how many recorded functions were selected per set.

| | **fast** (default) | **branch** | **full** |
|---|---|---|---|
| build | yes | yes | yes |
| ctest | everything except `SimMatch.*` and `gfsim_run_test` (whole-match tests; the smokes cover a match) | only the tests the branch reaches (see "The branch tier"); same two exclusions | everything |
| `diff_*_recorded` (pilot, cutscene, presentation, core, impure) | **incremental**: only the recorded functions the change can reach | the same incremental selection | every recorded function |
| translated-layer drift (`gen_translated.py --check`, ctest `diff_translated_drift`) | yes | when the translated layer, the translator or the specs changed | yes |
| ledger check | yes | yes | yes |
| smokes, seed 7 | one 2-minute run per path (switch-off, `--pes-loop`) with their `[determinism]` lines | the same | 5-minute prefilter, then 20-minute runs |
| verdict | `GATE fast:` lands | `GATE branch:`: a branch may claim it; it never lands | `GATE full:` |
| target wall time | ctest 8,798 s at 2,314 tests (merge lane, 05-11-26) | ~30 min for a branch outside the sim's link closure. A sim-moving branch selects 1,554 of 2,337 tests on 05-11-26 (95% of fast's seconds) and runs about as long as fast (see "The branch tier") | hours: the core re-check (2,253 functions, `--jobs 2`) takes 2.0 h cold and 8 min warm (02-11-26; uncached baseline 2.97 h) |

### When each runs

- **fast lands batches.** Main runs it on the merged tree before every landing.
- **branch** runs on each branch before it is claimed. Its verdict is `GATE branch:`; only fast lands.
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
`gen_dispatch.py` edits, `translated.hpp` / `pes_dispatch_gen.cpp` (derived lists).
The selector's oracle is `ctest -R translate_select_recorded_test`: a changed callee two calls down selects its recorded caller,
an unrelated change selects nothing.

### Knobs

- `GATE_JOBS`: ctest `-j`. Default: free cores (`nproc` minus the 1-minute load average, at least 2).
- `GF_DIFF_JOBS`: the driver's `--jobs` in each `diff_*_recorded` check (VAs checked at once). Default: sized from free memory
  and cores, 2 to 6 (`GATE_RIG_MEM_GB`, default 10, and `GATE_RIG_CORES`, default 4, stay free for the live rig; each job is
  budgeted `GATE_DIFF_JOB_MEM_GB`, default 2). `diff_core_recorded` has ctest COST 1000, so it starts first.
- `GATE_SLOTS`: the candidate sim slots for the gate's two smokes (default: any two free ones). `GATE_SLOTS="3 2"` pins them. Every tier holds two at priority 0 (see "Sim slot queue"); the old pair polling is gone.
- `GATE_BASE`: fast and branch, what "changed" is measured against.
- `GATE_SKIP_PREFILTER=1`, `GATE_PRE_MINUTES`: the full tier's 5-minute prefilter.
- The PES diff suites run only with `GF_DIFF_PYTHON` and `PES_EXE` set (export both before `cmake -S . -B build` as well); a
  skipped diff test is then a red gate.

### Diff caches

Each `driver.py --check` keeps two caches under its out dir (`<build>/<set>-check/cache/`, never in `gen/`, which every run
regenerates). A hit gives exactly what the uncached step gives; whatever cannot be shown unchanged runs as before
(`translate/buildcache.py` has the keys and the validity rules).

- **Objects** (`cache/obj/<sha256>.o`): one object per translation unit, keyed by the compiler, the flags, the TU's bytes and
  the bytes of every build-tree header it includes (transitively). A build compiles its TUs in parallel (`--cc-jobs`, default 4,
  across the whole run) and links them. A TU is shared across VAs with identical content.
- **Corpus** (`cache/corpus/<sha256>/`): `run.py`'s outputs for one VA (corpus, trace, addresses, aborts), keyed by the image,
  the spec, this VA's seed and bank files, `run.py`, the harness interpreter and the flags. A hit also needs every file the run
  read (its data files, the modules it loaded, the libraries it mapped) to hash as recorded, and every decode it took from
  `xlate` (`capstone_decode`, `CCMAP`) to recompute as recorded. So a translator-only change (`xlate.py`, `flow.py`, `pdata.py`,
  `gen_translated.py`, the layer) hits; a change to anything `run.py` reads misses. Failed runs are never kept.
- A layer header edit misses the objects of the TUs that include it and hits the corpus. To reset, delete
  `<build>/<set>-check/cache`. The driver ends with `driver: cache corpus hit=N miss=M object compiled=N hit=N`.

### The branch tier

`GATE_TIER=branch` runs the build, both smokes, the recorded re-checks and the ledger, and ctest only for the tests the branch's
diff can reach (`tools/dev/branch_select.py`; the reasons go to `<build>/gate.branch.log`). A test runs when one of its inputs
changed since the merge base:

- its binary's build closure (`ninja -t inputs`): sources, headers and every static library object it links. A change to an
  object of a linked archive selects every binary linking it. So any sim source selects every gfsim binary, the seeded match
  families among them (their matches run the same sim code); a pesfmt-only binary is not selected by a sim source;
- the build outputs its command names (`gfsim_run` for `gfsim_run_test`) and the repository files it names. For a script or
  data file, every file under its directory, recursively, except the roots `tools/`, `tests/`, `src/`, `apps/`, `godot/`,
  `assets/`, `extern/`, which contribute only the file;
- `res://` paths name `godot/`, and a godot test (`tools/godot.sh`) adds the extension's closure;
- EXTRA inputs the command does not name: the translated layer for `diff_translated_drift`, `tools/dev/parity.py` for `parity_metrics_test`.

Always on: `repo_pii_scan` and the five `diff_*_recorded` tests (their `GF_RECORDED_SELECT` decides what they re-check). A
fixture's setup test runs with the test that needs it.

Fallbacks, conservative: a change under `assets/` runs every test (tests read sim data through `GF_SOURCE_DIR`). A changed CMake
line that names a test selects that test (its ctest name, binary or script); a changed CMake line that names none (a flag, an
include, a subdirectory, a variable) runs every test.

Ceiling: closures are static-library granular, not object granular, and a file a test reads through a path its command does not
name is covered only by the fallbacks.

The selection reaches ctest as a list of test numbers (`ctest -I`), not a name regex: a `-R` over the names matched nothing once
it passed 985 of this build's 2,337 (05-11-26; the list overflowed ctest's regex engine, "Expression too big"). `branch_select.py
--index` prints the argument; `branch_select_test` checks that ctest lists back exactly the selected names at full size.

**Size.** A sim source (anything in `src/sim`, or in a library it links) selects every gfsim-linked binary: 1,554 of the 2,337
tests (not counting the two excluded families), 95% of fast's per-test seconds in the 05-11-26 merge-lane log. Such a branch runs
about as long as fast. The tier saves time for branches outside the sim's link closure. Object-level selection (which binaries
pull the changed objects, from a link map) would narrow it; it is not built.

### Sim slot queue

`tools/dev/simslot.sh <command>` holds sim slots: at most `GF_SIM_SLOTS` (default 6) sims run machine-wide, and a slot is taken
only while `MemAvailable` is at least `GF_SIM_MIN_AVAILABLE_MIB` (default 5800). Waiters queue in `<lock>.queue`, one ticket each,
named by priority and arrival. Only the lowest ticket probes the slots, so a later ticket never overtakes an earlier one of the
same priority, and a gate (`GF_SIM_PRIORITY=0`) runs before queued agents (default 5). `GF_SIM_HOLD` takes several slots for one
command (gate.sh holds two); `GF_SIM_PIN` names the candidates. Slots are the same flock files as before (slot 0 is the lock file
itself), so a script that flocks them directly still counts. It bypasses the queue, so a queued head can wait behind it for a slot.
