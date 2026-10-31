# PES port ToolFixes2: dispatch-table note names the .cpp; traced_cov skips undecodable functions

Branch `ToolFixes`, head `9afdf163f870c2df106e295b7b55d8e748cb6154`. `git ls-remote origin ToolFixes` confirms it. All pushes were fast-forward; nothing was forced.

## Commits (on top of origin/main e0fdf4e3, batch AQ)
- `f7ff534c` merge origin/main into ToolFixes. Clean merge. It brings the LayerHeaders rename `src/pes/translated/pes_dispatch_gen.hpp` -> `pes_dispatch_gen.cpp`.
- `b070837e` stubs.json: the dispatch-table note names `pes_dispatch_gen.cpp`.
- `9afdf163` traced_cov: undecodable functions are left out of the report, not unpacked.

## Verified
- CONFIRMED: `gen_dispatch.py` writes `<out>/pes_dispatch_gen.cpp` (`write()` line 70, print line 83). `gen_translated.py` writes the same file (docstring line 16, line 222).
- CONFIRMED: `native_services_contract` is documentation text. Nothing in tools/, tests/ or src/ reads it. The generator reads `native_services` (gen_dispatch.py header).
- CONFIRMED by reading: `block_coverage` maps an undecodable VA to `None` whatever the executed set is (driver.py:828-837, via `block_bounds` returning None).

## Change
- traced_cov.py:52. `if pc is None: pc = (0, 0)` runs before the per-run unpack. Before this, an undecodable VA with recorded hits raised here. `(0, 0)` matches the existing `if cov else 0` on the no-hits path.
- traced_cov.py:61. The own record reads `cov[0] if cov else 0` and `cov[1] if cov else 0`. The same `None` reached `cov[0]` right after the per-run loop. Its own fraction stays `None`.
- Decodable VAs: no output change, since neither guard fires for them.
- New `tools/dev/translate/test_traced_cov.py`, ctest `translate_traced_cov` (tests/pilot/CMakeLists.txt, after translate_finish_coverage; plain python3, TIMEOUT 120). It runs the real `traced_cov.main` over real spec and run directories. `block_coverage` is stubbed to its contract: `None` when undecodable, otherwise (reached, total) from the executed addresses. Assertions: decodable own 2/4 = 0.5; undecodable per-run reached 0; undecodable own fraction None; summary reached 2; floor_union 1.

## Red, green, mutations
- RED on the unfixed tree (origin/main traced_cov.py): `TypeError: cannot unpack non-iterable NoneType object` at traced_cov.py:52 in `main`. 1 error.
- GREEN: `translate_traced_cov` passed.
- M1, per-run branch: the None path set to `pc = (1, 1)`. FAIL `AssertionError: 1 != 0` at the per-run reached assertion. Reverted.
- M2, own-record branch: the guard removed (`cov[0]`). ERROR `TypeError: 'NoneType' object is not subscriptable` at traced_cov.py:61. Reverted.
- After the reverts, `git diff` of traced_cov.py against the commit is empty.

## Tests
Fresh build dir in the worktree (`cmake -S . -B build -G Ninja`, configure only, no target build). `ctest -R` over four tests:
- translate_finish_coverage: Passed
- translate_select_recorded_test: Passed
- translate_traced_cov (new): Passed
- translate_native_services (reads stubs.json): Passed
- Total: 4/4, 100%. The tests are plain python3 and each runs in under 1 s, so they ran outside a sim slot.

## Gate
GATE fast: NOT RUN. The fast tier builds everything, and this worktree has no build dir yet. The assignment says no full builds at load ~30. Main's batch gate covers it. Smoke: not run; no match behaviour changed.

## Open items
1. traced_cov.py:36, :45 and :74 use `open()` without closing. unittest prints ResourceWarning for them in the test output. This predates the change and is not part of the hazard, so it is unchanged. Fix: `with open(...)`.
2. `diff_translated_drift` not run. The stubs.json change is text only, and the generated layer does not carry it. Main's gate runs the drift check.
3. In this worktree `git status` fails with `error: expected submodule path 'extern/godot-cpp' not to be a symbolic link`. That comes from the godot-cpp symlink recipe against the gitlink. `add`, `commit -- <paths>`, `merge` and `push` by explicit path work.
4. The first push failed with a transient error from the gh credential helper. The retry pushed.
