# pes-port-LayerHeaders: adding a translated function no longer rebuilds the layer

Branch `LayerHeaders`: 474002af (change on c9842d30), merged with origin/main f445018e (batch AP: native services) as 2cc86e0d; pushed, `ls-remote` confirms 2cc86e0d. Machine load 31-44 while measuring and building.

## What was wrong (measured, not assumed)

- Every `pes_fn_<VA>.cpp` already forward-declares only its own callees (the translator emits them). `translated.hpp`
  (declarations of all 4,075 functions) is **not** included by any generated TU.
- The all-functions dependency was `pes_dispatch_gen.hpp`: `pes_dispatch.hpp` ended with `#include "pes_dispatch_gen.hpp"`,
  which holds 4,075 declarations plus an *inline* 4,075-case `switch` (8,167 lines). All 4,075 TUs include `pes_dispatch.hpp`
  (checked: 4075 of 4075 `pes_fn_*.cpp`), so any added function changed that header and rebuilt everything. Each TU also parsed
  8,167 lines it never used.
- `translated.hpp` is included by 8 GF objects only: `src/pes/fdc.cpp`, `src/sim/onthepitch/{peskickerror,pesoffball,pesteamconcepts,pesworldrecord}.cpp`,
  `tests/pes_fdc_test.cpp`, `tests/pilot/{pes_team_sheet_test,pes_world_test}.cpp`. Kept (GF-side callers need the declarations).

## Measurement method

A real full build of `pestranslated` at load 31-44 takes 28-50 minutes, and `ninja -n` is unusable here (the globbed-directory
re-check makes it stop at "Re-running CMake" without listing objects). Instead: a scratch build dir configured with
`CMAKE_CXX_COMPILER_LAUNCHER=fakecc.sh`, a launcher that runs the real compiler with `-fsyntax-only` (so the `-MD` depfiles ninja
records are the real include graph) and touches the object. That gives ninja the same dependency state as a full build in 14 min
(before) / 10 min (after) of parse time. Then one function is added the way the generator adds it (new `closure/pes_fn_14FFFFFF0.cpp`,
`translated.hpp` and the dispatch table rewritten by the old/new `gen_dispatch.text`), `cmake .`, and ninja's dirty set is read.

| | objects rebuilt after adding one function |
|---|---|
| before (origin/main generator) | **4,076** (ninja `[1/4078]`: 4,076 compiles + link; run stopped once the count was read) |
| after (this change) | **2**: the new TU and `pes_dispatch_gen.cpp` (ninja `[1/6]`..: 2 compiles, 1 link) |

Plus the 8 GF objects above whenever `translated.hpp` changes (not part of `pestranslated`; bounded by GF callers, not by layer size).
Scratch additions were reverted; the committed layer is untouched. Logs in `gf-LayerHeaders-scratch/{after_add,after_base}.log` (before count: ninja header line).

## Change

- `gen_dispatch.py`: the table is now `pes_dispatch_gen.cpp` (declarations, `pes_dispatch_lookup`, `pes_dispatch_missing` out of line);
  `write()` writes that file. `pes_dispatch.hpp` no longer includes the table (it already declared both functions).
- `gen_translated.py`: emits `pes_dispatch_gen.cpp` instead of `pes_dispatch_gen.hpp`; `translated.hpp` unchanged (GF-side only).
- `driver.py`, `snapdiff.py`: the diff builds compile the generated `pes_dispatch_gen.cpp` with their closure (they used to get the
  table through the header).
- `CMakeLists.txt`: `pestranslated` gains `src/pes/translated/pes_dispatch_gen.cpp` (the glob only picks `*/pes_fn_*.cpp`).
- Regenerated layer (`gen_translated.py --image pes_full.bin`, 68 s): **all 4,075 `pes_fn_*.cpp` and `translated.hpp` byte-identical**
  (git sees only `pes_dispatch_gen.hpp` -> `pes_dispatch_gen.cpp` and the `pes_dispatch.hpp` edit). `pes_dispatch.hpp` is in
  `select_recorded.py`'s runtime-header list, so the fast gate re-checks every recorded function once for this change (expected).

## Test: `translate_layer_headers_test` (plain python3, 2.6 s, no image, no compiler)

1. No `pes_fn_*.cpp` reaches `translated.hpp`, `pes_dispatch_gen.hpp` or `pes_dispatch_gen.cpp` through any `#include` chain in the layer.
2. Rebuilding the committed layer's members through `gen_translated.layer_files` reproduces the committed `translated.hpp` and
   dispatch TU; adding one member changes exactly {its TU, `translated.hpp`, `pes_dispatch_gen.cpp`} and removes nothing.

Red on origin/main's layout (test 1 fails with the first TUs listed; test 2 errors on the missing `pes_dispatch_gen.cpp`).
Mutations run on a scratch copy of the layer (worktree untouched), each fails then reverted:
A `pes_dispatch.hpp` includes `translated.hpp` -> test 1 fails; B one TU includes `pes_dispatch_gen.cpp` -> test 1 fails;
C `translated.hpp` loses a function -> test 2 fails; D generator output made to depend on the member count (every TU gets `// n=`) -> test 2 fails.

## Verification

Against the merged tree 2cc86e0d (4,346 functions: 3,694 recorded, 652 closure):
- `gen_translated.py --check --image pes_full.bin`: "4346 functions ... regenerate byte-identical" (rc 0). The merge's regeneration changed no `pes_fn_*.cpp` and not `translated.hpp`; only the table moved.
- `translate_layer_headers_test`, `translate_native_services` (3), `translate_select_recorded_test` (9): green.
- Full build of the whole project (`gate.sh` build step: `pestranslated`, gfsim, every test binary, godotfootball): succeeded, `-j3`, load ~34.
- `GATE_TIER=fast GATE_SLOTS="3 2" tools/dev/gate.sh` on 2cc86e0d: **INTERIM, not finished.** Build green; both 2-minute smokes ran to rc 0 (`smoke-off rc=0`, `smoke-pesloop rc=0`; their `[determinism]` hashes were not compared to Main's baselines e663d1e917d6c4f4 / 980c54e27f93ced2); ctest 2,125 Passed, 0 Failed/Timeout so far. `diff_pilot/cutscene/presentation/core/impure_recorded` and `diff_translated_drift` had not finished when I stopped the run: each re-check takes 20-40+ minutes at load ~34, and `pes_dispatch.hpp` is on the selector's runtime-header list, so the fast gate selects **every** recorded function (3,763 of 3,763). I killed the run to release sim slots 3 and 2 rather than hold them for hours. `gen_translated.py --check` was run directly (above).
- `pes_*` test binaries linking `pestranslated` that ran in that gate's ctest, all Passed: NativeHeap (9, `pes_native_heap_test`), PesFdc* (about 40, `pes_fdc_test`), PesTeamSheet (15, `pes_team_sheet_test`). `pes_world_test` I did not find by name in the log; not verified.
- An earlier gate on c9842d30 + this change (before the merge; stopped for the merge): `diff_pilot_recorded` Passed; `diff_translated_drift` Passed (51 s); `diff_cutscene_recorded` and `diff_presentation_recorded` **timed out at 2400 s** (load; origin/main has since raised presentation to 4800 s, aa050692); `diff_impure_recorded` **failed with a traceback that does not touch the dispatch path**: `driver.py finish_one` unpacks `for v, (r, t) in cov.items() if isinstance(cov[v], tuple)`, which crashes when `block_coverage` returns `None` for an undecodable function (its docstring says None is "never counted"; the `isinstance` filter runs after the unpack). Not fixed here (not my ticket); not re-run on the merged tree. Main should check whether it still fires at its gate.

## Open / not done

1. `translated.hpp` still changes with each function (8 GF objects). Per-function headers or GF callers declaring what they call would
   make that zero; not done (the ask was the layer; each GF caller of 4,075 would otherwise hand-maintain declarations).
2. The dispatch TU's 4,075-case switch is recompiled per addition (about one TU of ~8k lines; seconds).
