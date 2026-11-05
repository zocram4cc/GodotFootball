# Tool report: branch tier (GATE_TIER=branch) and sim-slot priority queue (GateQuick, revision 2)

Branch `GateQuick`, tip `5b137b34` (origin, confirmed by `git ls-remote`). Base: origin/main `784ffd5d`.

Revision 1 reported `GATE branch: GREEN` on a tool-only sample. That result is superseded: its run executed 3 of the 8 tests it
selected (section 1, item 3). The queue (`simslot.sh`) is unchanged since revision 1.

## 1. What was wrong, and the fixes

1. **The selection overflowed ctest's regex engine.** The branch tier passed the selection as a name regex (`ctest -R`). Over
   985 of this build's 2,337 names it matched nothing (the full list gives `Total Tests: 0`, exit 0). RestingBall4's sim branch
   failed with "Expression too big", and gate.sh then exited silently at its summary grep under `set -e`. Fixed: the selection
   reaches ctest as a list of test numbers (`ctest -I 0,0,0,<n>,...`), and the summary and verdict no longer abort silently.
2. **The repository root expanded to the whole tree.** `gfsim_run_test` and `branch_select_test` take the repository root as an
   argument. `os.path.relpath(root, repo)` is `"."`, not `""`, so the guard missed it and every change selected both tests. That
   inflated RestingBall4's sim-source count to 2,311. Fixed in `8037da6a`; the docs test now asserts the exact always-on set (red on
   the old code, which also selected `gfsim_run_test` and `branch_select_test`).
3. **`-E` beside `-I` ran the wrong tests (the false GREEN).** The gate passed its `-E` (`SimMatch.*`, `gfsim_run_test`). ctest
   numbers the `-I` list after `-E`, so my unfiltered numbers pointed past the end of the list. The run executed 3 of 8 tests
   (`repo_pii_scan`, `simslot_queue_test`, `branch_select_test`) and reported `100% tests passed`; the five recorded re-checks
   never ran. Fixed in `5b137b34`: the selector applies the exclusion (`branch_select.py --exclude`); the branch run takes no `-E`;
   the verdict compares the count from `ctest -N -I` with the count the run reports, and any difference is RED. The `-R` form
   (revision 1) was not affected, because names select directly.
4. I edited `gate.sh` while the branch run was executing it (bash reads a script by byte offset, so the run could not be trusted).
   I cancelled that run (bg_647; no processes of mine were left) and restarted on the committed file.

Commits, in order: `833841a6`, `99e9598a`, `b958f17c`, `9a51b28a`, `d797c1dd`, `ec72d49a` (revision 1), `360c496c` (-I),
`8037da6a` (root guard), `b7725ae0` (figures), `5b137b34` (exclusion and count guard).

## 2. Sizing (measured)

- A sim source (`src/sim/simcontext.cpp`) selects 1,568 of the 2,337 build tests; 1,554 remain after the gate's exclusion.
- In the 05-11-26 merge-lane fast log (`<workdrive>/gf-MergeLane/build/gate.ctest.log`), those 1,549 logged tests take 13,873 s of
  the log's 14,639 s of per-test time (95%). A sim-moving branch's branch tier therefore runs about as long as fast.
- The tier saves time only for branches outside the sim's link closure (for example `tools/`, `tests/`, docs).
- Object-level selection (which binaries pull the changed objects, from a link map) would narrow the set. It is not built, and
  its gain for core sim changes is unmeasured.

## 3. Verification (run here)

- `branch_select_test`: 9/9 (run directly and through ctest against the 2,337-test build; about 22 s).
- `simslot_queue_test`: 5/5 (unchanged since revision 1).
- Mutants of the new index and exclusion code. Each was a scratch copy edited with the edit tool, run against the real build,
  then removed (the worktree was not touched):
  - M5, index numbers off by one: 2 FAIL (both index tests).
  - M6, names instead of numbers (the failure mode of the old `-I`-with-names form): 2 FAIL.
  - M7, first selected test dropped from the index: 2 FAIL.
  - M8, `drop_excluded` as a no-op: 1 FAIL (`test_the_gates_exclusion_is_dropped_before_the_index`).
  - Revision 1's queue and selector mutants (M1 to M4) were applied with a one-line python replace to scratch copies, not with the
    edit tool. They touched no repository file.
- Guard logic: the guard's `sed` on the real partial-run log (`build/gate.ctest.log` of `b7725ae0`) yields `3`; against
  `expected=8` the verdict is RED. This checks the extraction, not a full gate.
- Branch gate on `5b137b34`: `GATE branch: GREEN` (8 of 8 selected ran, on 5b137b34)
- Fast gate on `5b137b34` (owner rule, before reporting): not run on 5b137b34 (stopped by Main at ctest 1225/2323); fast GREEN 2323/2323 on d797c1dd

## 4. Decisions for Main

1. **Sim-moving branches get no speed-up from the branch tier** (section 2). Options: keep the conservative rule (what this
   revision does), or build object-level selection from link maps. I recommend measuring link-map precision first.
2. The branch tier's `-I` list is computed after the build, because `ctest -N` numbers only built tests. The gate builds first, so
   this holds in `gate.sh`.
3. A CMake change naming a test selects that test; a CMake change naming none selects every test (conservative).
4. Queue (revision 1, unchanged): `simslot.sh` priority FIFO (`GF_SIM_PRIORITY`, default 5; gates 0), `GF_SIM_HOLD`, `GF_SIM_PIN`;
   the head re-checks every 0.5 s; old scripts that flock the slots directly bypass the queue. Merge-lane sims must set
   `GF_SIM_PRIORITY=0` explicitly.
5. Godot tests leave untracked `godot/**/*.uid` files in a worktree. The selector counts tracked changes only.

## 5. Open items

1. Sim-moving branches cost as much as fast (section 2).
2. The count guard compares totals. It does not check which tests ran; a `-I` list with a wrong number that still counts right
   would pass. The unit tests cover the mapping; the gate does not re-derive names.
3. `ctest -N` numbering is per build directory. A build-tree change between selection and run would shift it; the gate selects
   after its build, so this does not occur in the gate.
