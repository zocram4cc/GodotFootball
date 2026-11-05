# Tool report: two-stage gate (GATE_TIER=branch) and the sim-slot priority queue (GateQuick)

Branch `GateQuick`, origin tip `ec72d49a` (confirmed by `git ls-remote origin GateQuick`), based on origin/main `784ffd5d`.
Commits: `833841a6` (red tests), `99e9598a` (queue, gate queues its pair), `b958f17c` (branch selector), `9a51b28a`
(submodule-free diff), `d797c1dd` (repository-root argument adds no inputs), `ec72d49a` (tracked changes only).
The two gate runs below were on `d797c1dd`; `ec72d49a` differs only in `git_diff` (tracked files only) and one README sentence.
`branch_select_test` and `simslot_queue_test` were rerun on `ec72d49a`: both green.

## What changed

1. **`tools/dev/simslot.sh`: priority FIFO queue.** Same slot files (`$lock` = slot 0, `$lock.N`), so jobs still on the old
   script and plain `flock gf-sim.lock` holders still count. Waiters queue in `$lock.queue` as `<priority>-<arrival ns>-<pid>`.
   Only the lowest live ticket probes the slots. A waiter that is not the head holds nothing, so a head that holds part of its
   set cannot deadlock a gate. `GF_SIM_PRIORITY` (default 5; gates 0), `GF_SIM_HOLD` (default 1), `GF_SIM_PIN` (candidate
   slots), `GF_SIM_HELD` exported to the command. Kept: 6-slot default and the `MemAvailable` check (`GF_SIM_MIN_AVAILABLE_MIB`,
   5800), now applied to each slot admission. The poll is 0.5 s and runs only in the head's probe (was 1 s in every waiter).
2. **`tools/dev/gate.sh`.** Pair logic removed (pair polling, 600-s fallback, 12,000 MiB third-pair rule). The gate holds two
   slots from the queue at priority 0 (`GATE_SLOTS` now pins candidates). New `GATE_TIER=branch`: build, both 2-minute smokes,
   the recorded re-checks (same `select_recorded.py`), the ledger, and ctest with `-R` on the branch selection, minus
   `SimMatch.*` and `gfsim_run_test` as in fast. Verdict `GATE branch:`; only fast lands.
3. **`tools/dev/branch_select.py`: the selection.** A test runs when one of its inputs changed since the merge base:
   - its binary's build closure (`ninja -t inputs`: sources, headers, every static archive object it links). A sim source
     therefore selects every gfsim binary, including the seeded families (their matches run the same sim). A pesfmt-only binary
     is not selected by a sim source;
   - build outputs its command names (`gfsim_run`), repository files it names, and for a script or data file its directory
     recursively (roots `tools/ tests/ src/ apps/ godot/ assets/ extern/` contribute only the file); `res://` names `godot/`,
     `tools/godot.sh` adds the extension's closure;
   - EXTRA inputs the command does not name: `src/pes/translated` for `diff_translated_drift`, `tools/dev/parity.py` for
     `parity_metrics_test`;
   - always on: `repo_pii_scan` and the five `diff_*_recorded` tests; fixture setup tests run with their dependants.
   - Conservative fallbacks: a change under `assets/` selects every test; a changed CMake line naming a test selects it; a
     changed CMake line naming none (flag, include, subdirectory, variable), or a changed CMake file without lines, selects every test.
   - Ceiling: closures are static-library granular.
4. **Tests** (registered at the end of `tests/CMakeLists.txt`):
   - `simslot_queue_test` (`tools/dev/test_simslot.py`, scratch lock in the build dir): (a) three tickets, A(5), B(0), C(5) in
     arrival order run B, A, C; (b) six commands on two slots, never more than two at once and two are used; (c) a head that
     loses its place gives back its partial hold (a priority-0 gate overtakes it and finishes); (d) memory below the threshold
     blocks admission, and SIGTERM leaves no ticket; (e) a plain `flock` holder of slot 0 is respected.
   - `branch_select_test` (`tools/dev/test_branch_select.py`, against this build's ninja graph): a sim source selects
     `pes_judge_test` and the seeded `pes_restart_delay_test`, not `pes_fdc_test`; a test source selects its own binary only;
     a docs change selects only the always-on tests; an assets change selects every test; a CMake line naming a test selects it;
     a CMake line naming none selects every test.
5. **`tools/dev/README.md`**: tier table (branch column), the branch-tier rules, the queue, and the changed knobs.

## Verification (run here)

- Build `cmake --build` rc 0 (1,369 s, ccache-warm). `simslot_queue_test` 5/5 green (16.8 s); `branch_select_test` 6/6 green (19–31 s).
- Red check: the queue tests on the pre-change `simslot.sh` (784ffd5d): 3 of 5 fail (priority order, partial-hold deadlock,
  memory wait); the two others pass (slot count, old-flock interop), as they should.
- Mutations, each on a scratch copy (the worktree was not touched; C++ not rebuilt, the mutants are shell and Python):
  - M1 priority ignored: `test_priority_then_arrival` FAIL, `test_a_head_that_loses_its_place...` FAIL.
  - M2 no partial-hold release: `test_a_head_that_loses_its_place...` ERROR (deadlock, timeout). This mutant left two orphaned
    waiters holding scratch locks; I killed their two PIDs (180727, 180752).
  - M3 binary closure dropped: `test_sim_source_selects...` FAIL, `test_test_source_selects...` FAIL.
  - M4 CMake-line naming ignored: `test_cmake_line_naming_a_test...` FAIL.
- Gates, both on `d797c1dd`, both with the gate env (`PES_EXE`, `GF_DIFF_PYTHON`, `PES21_ROOT`, `GF_IMPURE_BANKS`), queue at priority 0:
  - `GATE branch: GREEN`: selection 9 tests, 8 ran (`gfsim_run_test` is in the selection and excluded by the same `-E` as fast);
    ctest 100% of 8 in 24 s; recorded re-check 0 selected in all five sets. The same sample on an earlier tip took 18 min 39 s
    of wall time (`bg_491`, same selection size).
  - `GATE fast: GREEN`: ctest 100% of 2,323 (8,942 s); recorded 0 selected in all five sets; ledger current (38,116 rows);
    gate held slots 3 and 5 through the queue.
  - Smokes (seed 7, 2 min), both gates and both equal to main's baselines: switch-off `399e180f39459c2b` (27 stoppages, longest
    23.2 s), `--pes-loop` `85c7d7c515cf92ae` (17 stoppages, longest 23.7 s).

## Open items and limits

1. **A sim-moving branch does not get the 30-minute tier.** A sim source selects every gfsim binary, which includes the seeded
   families. `pes_restart_delay_test` alone took 3,829 s in the last full fast log. Such branches pay about the fast tier's
   ctest minus `SimMatch.*`. Narrowing needs object-level (link-map) selection, which is not built.
2. The head polls every 0.5 s; there is no kernel wake-up. Old scripts bypass the queue and can win a free slot.
3. Merge-lane sims must set `GF_SIM_PRIORITY=0` explicitly (default 5). Pinned `GATE_SLOTS` is now a candidate list, not an ordered pair.
4. Godot tests leave untracked `godot/**/*.uid` files in worktrees. The selector counts tracked changes only. In worktrees,
   `git status` fails on the godot-cpp symlink (the `status.submodule=none` workaround applies).
5. The memory rule is per slot (5,800 MiB at each admission). The old third pair needed 12,000 MiB for the whole pair.
