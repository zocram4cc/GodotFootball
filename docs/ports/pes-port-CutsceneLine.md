# CutsceneLine: CutsceneSelect, CutsceneTiming and CameraSelect on one branch; CutAnchor6 not closed (its captures are gone)

Branch `CutsceneLine` (worktree `<workdrive>/gf-CutsceneLine`). Pushed head `6b154632` (`git ls-remote origin CutsceneLine` = `6b1546321f8d681373b8bba647ee5d13c075141b`). Base `origin/main` `1235c3d4`. Commits: `361baea1` (merge of origin/CameraSelect, no rebase), `6b154632` (rename fixing the extension build). Labels: CONFIRMED = decompile, git output, test or smoke run this session; INF = inference; OPEN = blocker named.

## 0. Does FixDemoHuman's anime blend (0x141EB9640) read the player anime chain? NO (CONFIRMED for direct reads; INF for leaf callees)

- `FUN_141EB9640` (VA `0x141EB9640`, 6236 bytes, Ghidra decompile): inputs are an output struct `param_1` (writes `+0x50..+0x128`), a motion id `param_2` (bounded `< 0x122D`), a float time `param_3` (`0.0 <= t`), and two flags. No player, chain or `P+0xA08` pointer is an input.
- Globals read by the blend: `FUN_141ED6640()` returns `DAT_1437F4FF0` (the motion DB); `FUN_141ED66E0` reads `DAT_1437F53A8` (+0x6B + id*0x70) and the bound table `DAT_142B906F0`. Nothing else in `0x141EB9640`.
- The four sub-samplers `0x141EBA8C0`, `0x141EBAC80`, `0x141EBB160`, `0x141EBB640` take `(output*, float, int, int, byte*)` and read only the tables `DAT_14256AE1C`, `DAT_142591140`, `DAT_1425E49F0`. None reads the anime singleton `0x1437CEA40` or `P+0xA08`.
- INF: the leaf accessors `FUN_141ED6xxx` (DB reads) were not all decompiled.
- Consequence: nothing in this round waits on the one-chain cut (OneWorld-series-wip). The whole round is the non-anime scope.

## 1. Inventory (CONFIRMED by `git merge-base --is-ancestor` and `rev-list --count`)

| branch | head | on main? | open |
|---|---|---|---|
| CutsceneSelect | `3bb1e6bd` | YES (ancestor of origin/main); doc `docs/ports/pes-port-CutAnchor6.md` on main | D/F 7/7 (not asserted: `tests/pes_goal_anchor_test.cpp` pins A, B, C, E, G); the GF cut-over of the list (`src/pes/fixdemo_select.cpp:449 std::vector<uint32_t> scenes = f.cuts;` stays) |
| CutsceneTiming | `ff362f40` | YES (ancestor of origin/main; ProcCamera1-8 reports and the CameraKeeper head are on main) | none new in this round |
| CameraSelect | `a1860bbd` | NO before this round: 4 commits (`cc86cc22`, `ab22076f`, `a1860bbd`, merge `32cc9f18`); report `docs/ports/pes-port-CameraSelect.md` on main | translated builder `0x141F2DD80` not proven (its §2); the cross-layer choice `0x141EB0570` and which layer shows: OPEN |
| OneWorld-series-wip | `2be76714` | NO: 291 commits not on main | out of scope for this round (the one-chain cut) |

## 2. What this round did

- Created `CutsceneLine` from `origin/main`. Merged `origin/CameraSelect` (`--no-ff`). Conflict: `tests/CMakeLists.txt`, both sides additive (main's `simslot_queue_test`, `branch_select_test`, `translate_*`; CameraSelect's `pes_camera_list_test`). Kept both.
- SnapMem patch: CameraSelect's `tools/dev/translate/full_snapshot.py` change (full-capture holes read as zero pages) is reverted. The file equals `origin/main` (`git diff origin/main -- tools/dev/translate/full_snapshot.py` is empty). Main's `capz unpack` already restores sparse holes (brief 03-11-26).
- Build break found and fixed (CONFIRMED by the build log, `gdext-build.log`): the merged `godotfootball` target failed with 3 errors. CameraSelect's scene-generator class `pes::FixDemoScene` (`src/pes/fixdemo_camera_list.hpp:58`) clashed with main's table struct `pes::FixDemoScene` (`src/pes/fixdemo_table.hpp:46`), and the `SceneState` constructor (`src/gdext/pes_cutscene_camera.hpp:89`) then resolved to the struct. Fix (commit `6b154632`): renamed the camera class to `pes::FixDemoCameraScene` in the five camera files (`fixdemo_camera_list.hpp/.cpp`, `pes_cutscene_camera.hpp/.cpp` comment and member, `tests/pes_camera_list_test.cpp`). The table struct and its users are untouched. Rebuilt: `godotfootball` linked (`gdext-build2.log`, rc 0).
- `tools/dev/ledger.py` run (rc 0): no tracked file changed.
- Layer not regenerated: the merge changes no translator input (CameraSelect's report: "no layer change"). `gen_translated.py --check` not run.
- `extern/godot-cpp` symlink in the worktree restored after the merge (git rejects a symlinked submodule path during merge; swapped for an empty dir around the commit).

## 3. Tests (`flock /tmp/gf-build.lock nice -n 19 cmake --build build ... -j8`)

| binary | result (head `6b154632` unless noted) |
|---|---|
| godotfootball (gdext extension) | BUILT (rc 0) after the rename; it did NOT build at `361baea1` (section 2) |
| pes_camera_list_test | 5/5 PASS (rebuilt after the rename) |
| pes_goal_anchor_test | 20/20 PASS (at `361baea1`; 7 scene cases, D and F not asserted) |
| pes_fixdemo_select_test | 6/6 PASS (at `361baea1`) |
| pes_goal_scene_hold_test | 3/3 PASS (at `361baea1`) |
| pes_camera_test | 17/17 PASS (at `361baea1`) |
| pes_cutscene_timeline_test | 7 PASS, 1 SKIP: `CutsceneTimeline.CapturedGoalsReplay` (its goal capture is gone, section 5) |
| gfsim_run | built (at `361baea1`) |

Not run: the GDScript / godot-side tests (`tools/godot.sh ... -s res://tests/...`); none were identified for the camera wiring in this round. The full gate was not run (Main gates).

Mutations: none. This round wrote no new GF logic or test. The rename is compile-only.

## 4. Smokes (2 min, seed 7, `gfsim_run --db-sql assets/fixtures/teams.sql --home 1 --away 2 --seed 7 --minutes 2`, in sim slots via `tools/dev/simslot.sh`)

| run | CutsceneLine `361baea1` | origin/main `1235c3d4` | ticks |
|---|---|---|---|
| switch-off | `[determinism] a0fbe2d5cd445a52` | `a0fbe2d5cd445a52` | 24539 both |
| pes-loop (`--pes-loop`) | `[determinism] 17941efc16b4a6a5` | `17941efc16b4a6a5` | 29532 both |

Identical on both. The same stalemate and restart lines appear in both (no new stall). The rename commit `6b154632` is not re-smoked: `src/pes/fixdemo_camera_list.cpp` is not in gfsim's source list (`CMakeLists.txt:36`); it is in the extension list (`CMakeLists.txt:68`). INF: the smokes are headless sim only. CameraSelect's extension wiring (`PesCutsceneCamera`, `match.gd`) is not exercised by them, and no frames were taken.

## 5. Open items (numbered, each with its evidence)

1. **CutAnchor6 D/F 7/7: blocked by missing captures.** The D and F goal cases (`pid132854` seq3783 for D, seq4721 for F) are not asserted, and the recompute chain `0x140471A80` / `0x1404B88B0` else arm has no full kind-3 replay. Needed: a goal capture with `recompute_471A80` and `bcwriter_4B88B0` FULL (plus `cut_anchor_calc_474F00` as the arm). Those hooks were dropped from the watcher allowlist on 04-11-26 (rvwatch56, brief log). Main must arm them for the next goal.
2. **Capture evidence gone (CONFIRMED):** `<workdrive>/gf-Rendezvous-scratch/live2/` has no `pid132854`, `pid2621613`, `pid2122415`, `pid3061186`, `pid3234533`, `pid3439759`, `pid3741448`, `pid1043691`, `pid2738437`, `pid2580963`. The cited seqs (`pid132854/seq3783`, `seq4721`; `pid2621613/seq1531`, `seq1532`; `pid3741448/seq70461`, `seq71718`; `pid1043691/seq16574`) are in no `deleted_*.txt`. The 09-11-26 handover says "live2 was emptied". The StoppageFlow scratch copy `<workdrive>/gf-StoppageFlow-scratch/full12/roots/pid3741448/` holds dangling symlinks into `live2/pid3741448`. Any replay of these inputs cannot run from this disk.
3. **GF cut-over of the list: NOT done.** `f.cuts = {sceneId}` and the goal-scene picker remain (`src/pes/fixdemo_select.cpp:449`; `tests/pes_goal_anchor_test.cpp:144/265/270/554` set `f.cuts`). Blocker: the builder's arena (CutAnchor6 §3 read set) is not written, and goal A's full capture (item 2) is gone, so a red test from PES facts cannot be proven. No stand-in added.
4. **CameraSelect translated builder `0x141F2DD80`: not proven** (its report §2: harness import model for `VirtualAlloc`, the `0x25a3900` call, the stub `0x6ffffa6619a0`; or a FULL entry hook). The camera-list GF port is wired into `PesCutsceneCamera` and `match.gd` on this branch. The cross-layer choice `0x141EB0570` and which layer shows: OPEN (`src/gdext/pes_cutscene_camera.cpp:149` says "ponytail: ... this stands in").
5. **CutsceneTiming `CapturedGoalsReplay`: SKIP** (item 2).
6. **OneWorld-series-wip (291 commits, one-chain cut):** not merged. Not needed: item 0 says the anime blend does not read the chain.
7. Frames: none taken (no visual claim).

## 6. Pushed

`git push origin HEAD:refs/heads/CutsceneLine` → `6b154632`, confirmed with `git ls-remote origin CutsceneLine`.
