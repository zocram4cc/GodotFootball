# PES cutscene camera port: scene-id entry, live cut identification, and why no like-for-like frame pair (FdcCamera2, 28-10-26)

Branch `FdcCamera`, pushed head `194a6b83` (`git ls-remote origin FdcCamera` = `194a6b83`). Chain: `88bbb155` → merge `f032e753` (origin/main `3e7cc67a`) → merge `92ac2790` (origin/CutsceneSelect `96834d0a`) → `1bb17cb0` (scene path resolver + install test) → `194a6b83` (gdext `load_scene`, `match.gd --fdc-scene`, port doc). Labels: CONFIRMED = tool output, decode or test with VA; INF = inference, reason given; OPEN = blocker named.

## 0. Outcome against the ticket

| item | status |
|---|---|
| 1. merge origin/main + CutsceneSelect; debug entry takes a scene id; push | DONE. Entry is `match.gd --fdc-scene 0x<id> [--fdc-camera N]`; `--fdc-cut` removed (clean cutover). Pushed `194a6b83`. |
| 2. find a CANM camera cut the live match plays often; name `.fdc` and scene id | DONE with caveat. Live cut = `common/demo/fixdemo/ent/cut_data/ent_001_view_st000_cam2(_1).fdc`, scene `0x10001` ENT_LOADING (and anthem `0x10002`+ list the same layer). "Often" = INF (see §2). |
| 3. decode the `+0x04` start-time consumer; tick unit; interpolation rule, with VAs | PARTIAL. Field read CONFIRMED (`f32` at record `+0x04`, `src/pes/fdc.cpp` l.41; value 540.0 in the live cut's camera record). Consumer VA: OPEN. Tick unit: INF (1/60 s). Interpolation (PES's sampler): OPEN. |
| 4. PES frame at a known offset into the cut + Godot frame at same offset; side-by-side | NOT DONE. Not like-for-like (no PES frame of the cut, no time counter, no linked Godot binary). §4. |
| 5. fix the camera port where the pair shows a mismatch | NOT DONE: no pair, so no mismatch is measured. No camera code changed for fidelity. |

## 1. What changed (tests and mutation)

- `src/pes/fixdemo_table.{hpp,cpp}`: new `pes::FixDemoSceneFdcPaths(table, sceneId, error)`: for each step up to its terminating `cut == 0`, the first variant (row 0, condition keys not evaluated) of the step's cut group, as `<table dir>/cut_data/<layer>`. Empty with `*error` if the scene is not in the table or the category is out of range.
- `tests/pes_fixdemo_table_test.cpp`: `SceneCutPathsAreUnderTheCategoryCutDataAndMountedInTheInstall`. Red first (the function did not exist: compile failure). Green: `PES21_ROOT=<workdrive>/PES21 ./build/tests/pes_fixdemo_table_test` → **6/6 PASSED**. Asserts relations only (prefix `common/demo/fixdemo/foul/cut_data/`, every path `Exists` in the VFS, unknown scene → empty), no pinned names.
- Mutation (run, rebuilt, reverted): `cutData` built without `cut_data/` (`src/pes/fixdemo_table.cpp` l.83). Result: `SceneCutPaths…` FAILED at `pes_fixdemo_table_test.cpp:161` and `:162` (prefix and existence). Reverted; `git diff HEAD -- src/pes/fixdemo_table.cpp` empty; 6/6 again.
- `src/gdext/pes_cutscene_camera.{hpp,cpp}`: `load` (path) becomes private `load_file`; public `load_scene(library, scene_id, camera_index)` reads the category table (`DecodeFixDemoTable`), takes the first existing layer of `FixDemoSceneFdcPaths` that has camera N, and prints the chosen path. Bound as `load_scene`. **Guarded by compile only**: `ninja -C build-fdc CMakeFiles/godotfootball.dir/src/gdext/pes_cutscene_camera.cpp.o` → rc 0. No linked `godotfootball` exists in the worktree (§4), so nothing ran.
- `godot/match/match.gd`: `--fdc-scene` (`String(...).hex_to_int()`), replaces `--fdc-cut`. Not run.
- `docs/ports/pes-port-FdcCamera.md`: §0 row 4 and §4 bullets updated to the scene-id entry.
- Gate not run (Main gates). `pes_fdc_test` not rerun (reader untouched).

Merge notes: the only conflict was `CMakeLists.txt` `pesfmt` source list; resolved to `formation.cpp fdc.cpp fixdemo_table.cpp`. `git status`/`git add` on the worktree print `error: expected submodule path 'extern/godot-cpp' not to be a symbolic link` (the gitlink is `507ed9d8` on every ref; the symlink is the brief's layout). Merges, commits and pushes by explicit path still work; status does not.

## 2. Live cut identification (VAs, captures)

Capture hooks (from `<workdrive>/PES21/rendezvous/hooks.cfg`): `cut_create_141F57840`, `cut_load_141F36200`, `cut_load_141F5F330`.

- `call.bin` layout (`tools/dev/rendezvous/rv.c` `entry_record`): 16 GPR at `+0x00` (rax, rcx, rdx, rbx, rsp, rbp, rsi, rdi, r8…r15), 16 XMM lo/hi at `+0x80`, return address at `+0x180`. CONFIRMED by the return address `0x141f5a8e6` at `+0x180` in the cut_load capture.
- `cut_create` captures (entry of `FUN_141F57840`, `calls` = 1): `live2/pid407792/seq006` (rcx `0x37d107a0`) and `live2/pid161465/seq006` (rcx `0x3ea53540`). The cut object at `rcx` holds, through its track vector (`obj+0x20`, 0x1e0-stride tracks, fdc name pointers at track `+0x68/+0x88/…`), these layers: `pid407792`: `ent_001_view_st000_cam2.fdc` ×2, `ent_001_view_st000_cam1.fdc`, `ent_001_view_st000_mob.fdc`, `ent_001_gk_d.fdc`, `ent_001_view_st000_cam0021.canm` ×2; `pid161465`: `ent_001_view_st000_cam2_1.fdc` ×3, `ent_001_view_st000_cam1_1.fdc`, `ent_001_view_st000_cam2_1…`, `ent_007_passage02_*`. Tool: `<workdrive>/gf-FdcCamera-scratch/cap/chase.py` (pointer chase, read-only).
- The install's own `ent_001_view_st000_cam2.fdc` (extracted with `pesfmt_dump cat`, parsed with `tools/dev/cutscene/fdc_parse.py`): records `Cut` (type 0, `playSpeed` 1.0, `layout` DECODED), `Env` ×2, `Effect` (OPEN), `Camera` ×4. **Camera record index 4: key 20, `startCutTime` 540.0, `animePath` `ent/canm/ent_001_view_st000_cam0021.canm`, `nearClip` 0.5, `farClip` 400.0**. `ent_001_view_st000_cam1.fdc` camera record 0: key 0, `startCutTime` 0.0, `animePath` `ent/canm/ent_001_view_st000_cam0000.canm`, near 0.5, far 400.0. CONFIRMED (parse output).
- `scene_map.py --json` (install, read-only; output `<workdrive>/gf-FdcCamera-scratch/scene_map_all.json`, 139 MB, PES-derived, not committed): the layer `ent_001_view_st000_cam2(_1).fdc` appears in ENT_LOADING `0x10001` and in `0x10002`+ (ENT_ANTH0_CMN, ENT_ANTH1_CMN_HOME/AWAY, ENT_ANTH2_CMN, ENT_CLUB_CMN, …): 54,495 hits. CONFIRMED (static map). That the entrance plays at every match start is INF (two `cut_create` samples only; `plays often` not measured).
- Scene id: `0x10001` = (cat 1 `ent` << 16) | 1. Its first variant's layers are what `FixDemoSceneFdcPaths` returns; the debug entry then takes the first layer that has camera N. `camera_index` counts `Camera` records within a file (`pes::fdc::File::cameras`), so `--fdc-camera 0` on `ent_001_view_st000_cam2.fdc` is its first Camera record (file record index 4: key 20, `startCutTime` 540, `cam0021`), the clip in the live capture. **Open:** whether live PES plays that record or a later one of the four in that file is not established.

Other cuts seen in the same captures (not chosen): `goal_celebrate_0032_camSL.canm` (goal, `pid407792` cut_create), `goal_celebrate_0013_camSR.canm`, `goal_move4_0014_cam_Z_fromR.canm`, `goal_celebrate_0307_camSL.canm` (goal cut_load captures). Goal cuts are the other candidates; not measured for frequency.

Pulled-out consumer of record fields: `startCutTime` is `f32` at `+0x04` (`src/pes/fdc.cpp` l.41; also `t.startTime = f32(0x04)` l.63 for Timing). `nearClip`/`farClip` are the record fields the GF reader already applies (overrides clip channels 3/4).

## 3. Start time and interpolation (VAs)

| claim | VA / evidence | label |
|---|---|---|
| Loader dispatch: per-frame `FUN_141F5F330` switches on the track record's type byte (`record+2`); `5 → FUN_141F5F480`, `2 → FUN_141F5FA90`, `3 → FUN_141F5FC00`, `4 → FUN_141F60140`, `6 → FUN_141F60590` | `dec_cut.txt` (dec.sh, `0x141F5F330`) | CONFIRMED (decompile) |
| `FUN_141F36200` loader dispatch: type 6 → `FUN_141F3DD00` (camera adder, copies the record's `0x00..0x9c` into a map node); type 2 → `FUN_141F3F330` | `dec_loader.txt` (existing) | CONFIRMED (decompile) |
| `FUN_141F57840` (`cut_create` hook): grows the cut-track vector at `param+0x78` by one slot per call, `param+0xdc` index | `dec_cut.txt` | CONFIRMED (decompile); what it creates per track: INF |
| Camera track setup `FUN_141F60590`: sets `param+0x288 = 1` on first call; pushes channel ids `0x16…0x1A` and 0..0x15 through `FUN_141F56600` and `thunk_FUN_15B59A660` (= `0x141F56800`) | `dec_cam_player.txt`, `dec_chan.txt` | CONFIRMED (decompile): both helpers push 8-byte (channel, value) entries into a vector at `param_1` (command emitters). **Not the CANM sampler.** |
| Start-time consumer (reads record `+0x04`, or node `+0x0c`, after the adder's copy) | not found; the adder (`FUN_141F3DD00`) copies `0x00..0x9c` and does not read `+0x04` itself (`dec_camera.txt` l.277 reads record `+0x58`) | **OPEN**: needs xrefs to the copied node (Ghidra bridge down; dec.sh decompiles only known addresses) |
| Tick unit: `startCutTime` in 1/60 s ticks, clip 30 fps (2 ticks per frame) | `pes-re-cutscenes.md` §3.2 corpus ratio; `pes-port-FdcCamera.md` §2 | INF (medium). The live cut's 540 ticks = 9.0 s at 60 ticks/s (INF). |
| Cut-object floats `+0xec` = 1.0, `+0xf0` = 54.0, `+0xf4` = 57.0, `+0x108/+0x10c` = 54.0 (cut object at `rcx`, `cut_create` capture `pid161465/seq006`) | `cap/dump.py` | candidates for a cut-time or length field: **unverified** (one snapshot; no change observed) |
| Interpolation between keys (PES's rule) | CANM sampler not located | **OPEN**. GF's linear position / slerp rotation / held ends are GF's choice (`src/pes/fdc.cpp` `SampleCanm`), not PES's. |

## 4. Frame pair: not obtained, and why

1. **No PES frame of the entrance cut.** A live frame taken now (`<workdrive>/gf-FdcCamera-scratch/pes_now2.png`, `DISPLAY=:0 import`, read-only) shows ordinary play on the broadcast camera, score 7–5, no cutscene.
2. **No cut-time counter.** The offset of a PES frame from the cut start cannot be read: the cut object's candidate fields (§3) are unverified, and the current launch (pid 463316) has no `cut_*` captures yet (`live2/pid463316/` holds only `carrier_clear`, `clear_angle`, `kick_scatter`, `press_cmd_writer`, `ref_involve`, `trap_*` hook captures). Two `cut_create` samples exist in older launches.
3. **No Godot frame.** The `godotfootball` target needs the whole translated layer (`pestranslated`, 5,245 steps). `build-fdc` was stopped at step ~70 and `build` at step ~100 (both were my own `cmake --build` jobs, PIDs 777345/777347 and 780292/780294, killed by PID), because the link is far beyond the budget under `nice -n 19 ionice -c3` on the shared machine. Only the gdext object compiled (`OBJ_RC=0`). So the `--fdc-scene` path has never been run.
4. **Verdict: not like-for-like.** No camera comparison (framing, angle, fov) is possible from the material above. Nothing is verified against PES; no camera fix (step 5) is made.

## 5. Open items (numbered, with blocker)

1. Link `godotfootball` on a quiet machine (the 5,245-step translated layer). Blocker: shared CPU; the `build-fdc` objects are all dirty after the merge.
2. Start-time consumer VA: find the readers of the copied camera node (xrefs to node `+0x0c`, or the callers of `FUN_141F3DD00`'s map). Blocker: Ghidra bridge down; `dec.sh` needs candidate addresses.
3. Interpolation rule: decode the CANM sampler (the reader of the clip's channels). Blocker: not located; `FUN_141F60590` is the setup, `FUN_141F56600` / `FUN_141F56800` are command pushes (§3).
4. Cut-time counter: find the cut object's elapsed-time field and confirm the `+0xf0`/`+0xf4` candidates over two snapshots of the same cut. Blocker: needs a `cut_load_141F5F330` capture during a live entrance (none in the current launch).
5. Frame pair: capture the entrance (`0x10001`, camera record with `cam0021.canm`) at a known counter value with a read-only snapshot or a timed `import`, then render `--fdc-scene 0x10001 --fdc-camera <N>` at the same counter. Blocker: items 1 and 4. Also confirm which of cam2's four camera records is live (§2).
6. Which goal cut is the "plays often" one: count `cut_create`/`cut_load` layers over a longer set of captures. Blocker: only two `cut_create` samples so far.
7. Worktree: `git status` fails on the `extern/godot-cpp` symlink (brief layout); Main to know. `build/` and `build-fdc/` are half-built and must not be used as "built" evidence.
8. Process note: I wrote a configure log to `a scratch file outside the work drive` by mistake and deleted it; nothing else went to `/tmp`. `pesfmt_dump` for the extraction came from `<workdrive>/gf-KickSoftMin/build/apps/pesfmt_dump` (read-only use) because the worktree has none.

## 6. Scratch (not committed, PES-derived)

- `<workdrive>/gf-FdcCamera-scratch/cap/`: `capmem.py` (GPR + C-string at GPRs), `chase.py` (object pointer chase for `.fdc` / `.canm` names), `dump.py` (dwords/floats), `ent_*.fdc` (extracted entrance layers), `ent_ls.json`.
- `<workdrive>/gf-FdcCamera-scratch/dec_cut.txt`, `dec_cam_player.txt`, `dec_chan.txt`: dec.sh output for `0x141F57840`, `0x141F5F330`, `0x141F3F330`, `0x141F60590`, `0x141F56600`, `0x141F56800`.
- `<workdrive>/gf-FdcCamera-scratch/scene_map_all.json`: full FixDemo scene map (install).
- `pes_now2.png`: live PES frame (broadcast camera, 7–5).
