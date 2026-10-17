# Hygiene: delete six dead PlayingStyles knobs; fix `godot.sh --import` abort

Branch `Hygiene` (from origin/main `c43fb22`), head `0f69bc4`, confirmed with `git ls-remote origin Hygiene`. Not merged.

| SHA | Change |
|---|---|
| `8c66be3` | Delete `GetShotAppetite`, `GetShootingRangeBonus`, `GetFinisherDemand`, `GetCreatorVision`, `GetPassTypeBias`, `GetSpaceRatingWeight` and the `k*` multipliers only they used (`playingstyles.{hpp,cpp}`, −146 lines). |
| `1e85cfc` | CMake: `godotfootball` links `pesfmt pesmovestepcore` (cycle order). |
| `efae1d9` | CMake: `sim_match_test` links `pesfmt pesmovestepcore` (cycle order). |
| `0f69bc4` | `tools/godot.sh`: no hidden import failure; import runs with `--quit-after 300`; caller's `--path` kept. |

## 1. Dead knobs (CONFIRMED)

- Grep over `src`, `tests`, `godot`, `apps`, `tools` finds no caller of the six `PlayingStyles::` functions. The `PlayerSkills::` namesakes in `playerskills.{hpp,cpp}` and `gameplaytuning.hpp`'s `GetShotAppetite(config)` are different functions and are unchanged.
- No test covered them, so none was deleted.
- The `k*` constants (`kPoacherDemand`, `kFoxDemand`, `kFinisherDemandCap`, `kNo10Vision`, `kPlaymakerVision`, `kCreatorVisionCap`, `kFinisherCardRunnerBonus`, `kCreatorCardVisionBonus`) had no other user; removed with them.
- `ninja -t deps`: 0 objects with `#deps 0`.
- Stale claims in docs: `docs/re/pes-port-StylePort.md` line 56 says the six have live callers. Its own correction at line 68 is right. `docs/re/pes-re-playing-styles.md` §8 (lines 223-229) still lists them as knobs to delete. Left for archival.

## 2. `godot.sh --import` abort (root-caused, two bugs)

**Bug A (hidden by `|| true`): the GDExtension did not load.** `libgodotfootball.so` was linked with `pes::movestep::LocomotionStyle` undefined. `pesfmt`'s `movestep_run.o` needs it from `pesmovestepcore`, which is a static cycle. A shared library links anyway, so the failure only appears at dlopen. The hidden import printed `Can't open dynamic library ... undefined symbol: _ZN3pes8movestep15LocomotionStyle...` and continued. CONFIRMED with `nm -DC` (U before, T after) and the import log.

**Bug B (exposed once A is fixed): shutdown abort in the editor.** With the extension loaded, a cold `--import` completes, prints `loading_editor_layout DONE`, then exits 134 with a core dump. Cause: Godot 4.7.2 `editor/doc/editor_help.cpp` queues `_gen_extensions_docs` (lines 3038-3044) and `cleanup_doc()` frees `doc` (line 3365). If the queued call is flushed after cleanup, `doc->generate` dereferences null. This matches upstream godotengine/godot#111645 (Main::cleanup → CallQueue::flush → _gen_extensions_docs). INF for our binary: it is stripped, so the frames are unsymbolized; gdb shows SIGSEGV at the same offsets as the systemd core.

Experiments:
- Registering zero classes: exit 0. Registering only `GFInfo`: abort. So any registered extension class triggers it.
- Compiling doc XML for all ten classes into the library (godot-cpp `target_doc_sources`): still aborts. Reverted; `doc_classes/` is not in the branch.
- `--frame-delay 200`: still aborts. `--quit-after 300`: exits 0.

**Fix:** `godot.sh` runs the import with `--quit-after 300` (ponytail comment; drop when upstream is fixed). A failed import now stops the wrapper; `|| true` is gone.

Other wrapper changes: the project's `--path` is added only when the caller gives none. A caller's second, relative `--path` broke (`Invalid project path specified: "godot"`); the cause is INF (Godot appears to resolve it after moving into the first one). Verified fixed.

## Verification (run without `gf-sim.lock`, per contract)

- Full build `cmake --build build -j3 -- -k 0`: exit 0, 0 FAILED. Three warnings exist in files this branch does not touch (`animcollection.cpp:772`, `humanoid.cpp:2868`, `tests/pes_editsave_test.cpp:69`); pre-existing.
- `godot.sh --import` on a fresh `.godot`:
  - from repo root: 5/5 exit 0, with `extension_list.cfg` written each time;
  - from `godot/`: exit 0;
  - explicit `--headless --path godot --import`: exit 0;
  - no new core dumps (`coredumpctl list`).
- `godot.sh --headless -s res://smoke/smoke.gd`: prints `0.1.0`, exit 0.
- `PES21_ROOT=<workdrive>/PES21 ctest -R "godot_match|godot_motion"`: 3/3 pass (`godot_match_test`, `godot_match_scene_test`, `godot_motion_test`).
- `sim_match_test` (its link line changed): 10/10 pass, exit 0 (397 s).
- Off-path 2-min smoke (`gfsim_run --db-sql assets/fixtures/teams.sql --home 1 --away 2 --seed 7 --minutes 2`, no `--pes-loop`): `[determinism] a3d5e43a3b7b03af`, exit 0. Unchanged from the brief's off-path hash. Run before and after the final edits.

## Mutations (each run, rebuilt, reverted)

- M1: gdext link without `pesmovestepcore` (keeps `pesfmt`). Links and `godot_motion_test` passes. Equivalent, not a bug; not caught.
- M2: gdext link with neither (original line). `godot_motion_test` FAILS: `Can't open dynamic library ... undefined symbol: ...LocomotionStyle`, `Parse error`. Caught. Restored, 3/3 pass.
- Part 1 is a pure deletion; no behavioural test applies.

## Open items

1. Upstream: godotengine/godot#111645 (null `doc` in `_gen_extensions_docs` after `cleanup_doc`). `--quit-after 300` is a frame-count mitigation, not a guarantee; a slow doc job could still race. Remove it when Godot is fixed.
2. Static cycle `pesfmt ↔ pesmovestepcore` is still worked around with explicit repeats (`gfsim_run`, `sim_match_test`, `godotfootball`). Root fix: break the cycle, e.g. move `LocomotionStyle`'s dependency. Not done.
3. Dangling comments: `pescarrier.cpp:1134`, `pescarrier.hpp:749-750`, `elizacontroller.cpp:2402-2403` cite "playingstyles.cpp" for the through ball. Fixed on A4Cont (`0f14239`), not on this branch. Merge A4Cont or they dangle.
4. The four dribble knobs and `GetDepthOffset`/`GetWidthOffset`/`GetPoacherTargetX` are live and untouched.
5. Gate not run (Main gates). Full ctest not run.
