# PES FixDemo `.fdc` reader and first Godot cutscene camera playback (FdcCamera, 28-10-26)

Branch `FdcCamera`, pushed `88bbb155` (`git ls-remote origin FdcCamera` = `88bbb155`), on `origin/main` `f90abf65`. Worktree `<workdrive>/gf-FdcCamera`. Gate not run (Main gates). Labels: CONFIRMED = tool output, decode or test result with VA; INF = inference, reason given; OPEN = blocker named.

## 0. Status against the ticket

| item | status |
|---|---|
| 1. worktree, godot-cpp symlink, first gdext build | DONE. Worktree on `FdcCamera`. Build of `godotfootball` (rc 0) took about 41 min: it compiles the whole translated layer (5,242 steps, `pestranslated`), not only godot-cpp. It writes `gen/` and `bin/` into the symlinked godot-cpp, i.e. into the main checkout (brief-prescribed layout; noted for Main). |
| 2. C++ `.fdc` reader, consumes every byte | DONE. `src/pes/fdc.{hpp,cpp}`. Install: **4,925 listed, 4,925 parsed, 0 errors; 4,910 consume every byte; 15 fail with type-99 gaps (the OPEN class of `docs/re/pes-re-fdc-format.md` §5.1).** Embedded CANM: **5,330 streams, 0 bad**. |
| 3. camera semantics with VAs | DONE with one OPEN item (§2). |
| 4. Godot playback of one camera track, debug entry | DONE. `PesCutsceneCamera::load_scene` (gdext), `match.gd --fdc-scene 0xSCENE [--fdc-camera N]`: scene id resolved through the FixDemo table (`pes::FixDemoSceneFdcPaths`, §4). |
| 5. Godot frame next to a PES frame of the same moment | Godot frame DONE. **PES frame of the same moment NOT obtained: not like-for-like, see §5.** |
| 6. reader tests red-then-green, one mutation; frame pair | Tests DONE, 9/9 passing; one mutation run and reverted (§3). Frame pair: §5. |

## 1. Reader

- `src/pes/fdc.hpp`, `src/pes/fdc.cpp` (in `pesfmt`); `tests/pes_fdc_test.cpp` (target `pes_fdc_test`).
- Mirrors `tools/dev/cutscene/fdc_parse.py` (CutsceneSelect `96834d0a`): same claim, padding (zero, unreferenced, in the pool tail) and overlap rules; decodes Camera (type 6, `+0x00`…`+0xbc`) and Timing (type 2) records; every record's bytes are claimed by its size, unknown type bytes fail the parse.
- Reads through `PesVfs` (runtime VFS, mod winners honoured); `ListCutFiles` lists `common/demo/fixdemo/*/cut_data/*.fdc`.
- Counts are printed by the install test, not pinned. The install test asserts relations: no read or parse errors, and every non-consuming file carries a type-99 record.

## 2. Camera semantics (VAs; dec.sh decodes in `dec_camera.txt`, `dec_loader.txt`)

Scratch: `<workdrive>/gf-FdcCamera-scratch/dec_camera.txt`, `dec_loader.txt` (not committed; PES-derived).

| claim | VA / evidence | label |
|---|---|---|
| The loader dispatches type byte 6 to the camera adder `FUN_141f3dd00(param_1,rec,0,0,0)`; type 2 to `FUN_141f3f330` | `FUN_141f36200` switch: `case 6` and `case 2` (`dec_loader.txt` lines 26–39) | CONFIRMED |
| The adder copies the record into a map node; its `+0x00` u32 is the map key compared in `FUN_141f369f0` (`*(int*)(node+0x30)` vs the record's first u32) | `FUN_141f369f0` compare at `dec_loader.txt` ~l.95–96 | CONFIRMED (key), so `+0x00` is a key, **not** a start frame |
| `animePath` is `char[128]` at record `+0x0c`: the adder writes it to node `+0x14` with a 0x80-byte copy | `FUN_141f3dd00` tail: `(*_DAT_142530660)(plVar7+0x14, 0x80, …)`; node data starts at `+8`, so record `+0x0c` | CONFIRMED |
| The adder copies record bytes `0x00..0x9c` (loop of 16 qwords ×2 + tail) | `FUN_141f3dd00` body | CONFIRMED (extent), fields past `0x9c` are copied by `FUN_141f369f0` |
| `startCutTime` at `+0x04` is the start; units 1/60 s ticks, clip 30 fps (2 ticks per frame) | Doc (`pes-re-cutscenes.md` §3.2, OBS corpus ratio `maxTime/frameCount = 2.0` in 113 files) | INF (medium). **OPEN**: the consumer that reads `+0x04` was not located (no xref tool; the Ghidra bridge is down). `camera_cut.py` reads `+0x00` as a 30 fps `startFrame`; the decode above contradicts that. |
| CANM v1 layout: 6 channels 0 quat xyzw, 1 vec4 position (Fox metres), 2 vertical fov (deg), 3 near, 4 far, 5 aspect; keys = frameCount+1 | `camera_cut.py` `parse_canm`, `docs/PES21_CAMERA_TRACE.md` §4 | CONFIRMED by the reader on all 5,330 install streams (unit quats within 1e-3, keys = frameCount+1; test `EveryEmbeddedCameraStreamIsAWellFormedUnitRotationStream`) |
| Near/far from the record override the clip's channels 3/4 | `PES21_CAMERA_TRACE.md` §6.4; `pes-re-cutscenes.md` §3.2 `+0x94` note | INF (doc; not re-decoded) |
| Coordinates: Fox space (Y up, metres, pitch-centre origin, camera looks down local −Z), which is Godot's space in GF's match | GF `PesBroadcastCamera::update` writes Fox-space poses straight to `Camera3D` (`src/gdext/pes_broadcast_camera.cpp` l.96–108) | CONFIRMED (GF's own convention); PES's interpretation of the Fox frame is `camera_cut.py`'s |
| Interpolation: position and fov linear, rotation slerp (sign-flipped), held outside the key range | `SampleCanm` (`fdc.cpp`), tested in `SamplesLinearPositionAndSlerpsRotationBetweenKeys` | CONFIRMED against the reader's own semantics; the PES engine's interpolation is **OPEN** (sampler not decoded) |

## 3. Tests and mutation

- Red: the tests were written before the implementation. On `origin/main` the target does not compile (no `pes/fdc.hpp`), which is the red state. The first run was not red for the reader: it failed on one fixture bug (external stub built with `external=false`); the fixture was fixed and the reader was unchanged.
- Green: `PES21_ROOT=<workdrive>/PES21 ./build-fdc/tests/pes_fdc_test` → **9/9 PASSED** (`fdc_test_green.log`), with the install counts above.
- Mutation (run, rebuilt, reverted): drop the record type-byte claim (`src/pes/fdc.cpp`, `claim(base + to, base + to + 1)`). Result: **4 failing** (`DecodesCameraAndTimingRecordsAndConsumesEveryByte`, `StrayBytesAfterTheFileAreReportedAsGaps`, `ZeroBytesAfterTheTypePoolInTheBlockAreCountedAsPadding`, `EveryCutFileParsesAndConsumesItsBytesOrHasTheKnownDisabledRecord` with 0 files consuming every byte). Reverted; 9/9 again.
- `pes_fdc_test` is the only test binary run. Gate not run.

## 4. Godot playback and scene ids

- `src/gdext/pes_cutscene_camera.{hpp,cpp}`: `PesCutsceneCamera::load_scene(library, scene_id, camera_index)` decodes the scene's table (`table_<cat>.bin`, `pes::DecodeFixDemoTable`), walks the scene's steps to their first variant's `.fdc` layers (`pes::FixDemoSceneFdcPaths`, `src/pes/fixdemo_table.cpp`) and plays camera N of the first layer that has one. `update(camera, t_s)` poses a `Camera3D`: `t` in seconds, record start in 1/60 s ticks (reader: `f32` at record `+0x04`, `src/pes/fdc.cpp` l.41), 2 ticks per frame at 30 fps (INF, §2). Clip planes from the record override the clip's; near is clamped at 0.1 m (PES21_CAMERA_TRACE §6.4). KEEP_HEIGHT aspect (fov is vertical).
- `godot/match/match.gd`: `--fdc-scene 0x<id>` debug entry (hex, e.g. `0x10001` ENT_LOADING); when given, the scene's camera replaces the broadcast camera for the run (no delete; the broadcast path stays).
- Scene ids (offline, `tools/dev/cutscene/scene_map.py --json`, install): `0x20009` FOUL_PINCH_CARD_Y has no CANM camera (§5 note). `0x10001` ENT_LOADING (and anthem scenes `0x10002`+) list `ent_001_view_st000_cam2(_1).fdc` as a step layer (static map, not a live play record); that file's camera record 4 is `ent/canm/ent_001_view_st000_cam0021.canm` at `startCutTime` 540 ticks (install, decoded by `fdc_parse.py`). That clip is in the live `cut_create` capture (pid407792 `seq006`).

## 5. Frames

- Godot frame: `<workdrive>/gf-FdcCamera-scratch/frames_pk01b/frame00000060.png` (2 s into the viewer run, `capture_viewer.sh --scene res://match/match.tscn`, GPU check passed: AMD RX 6900 XT, RADV). Shows a goal-line camera behind a goal, a teal-kit figure on the pitch, and the stadium. Consistent with the doc's description of the pk shot (camera on the goal line at 1 m height); not verified against PES.
- Earlier runs in `frames_pk01/` and `frames_base/` were white: the first `godot` failed to load the extension (`undefined symbol PesCutsceneCamera::_bind_methods`: the source was not in the build yet, since the build started before the CMake edit). Fixed by regenerating and rebuilding (`build-godotfootball2.log`). Frames are kept as evidence of that failure only.
- PES frame: `<workdrive>/gf-FdcCamera-scratch/pes_now.png` (live PES, pid 463316, 260:39, 5–5): ordinary play with the broadcast camera. It does not show a cutscene, and the pk shootout intro is never played by the testbot match.
- **Not like-for-like:** no PES frame of the pk shot exists. The live match cannot be made to play that shot, and no capture hook covers it. Visual claims about the match against PES are therefore NOT verified. Needed to close: a PES capture of `pk_01_intro_setBall00_cam01` (a shootout intro) on a live run, plus the start offset of that cut in the track, and a same-frame Godot grab.

## 6. Open items (numbered, with blocker)

1. `startCutTime` (`+0x04`) consumer not located: needs the xref to the CANM-clip player (the Ghidra bridge is down; use `dec.sh` on candidates or the headless xref script). Until then the 1/60-tick start is INF (medium).
2. Interpolation and near/far precedence are the reader's and GF's choices, not PES's (sampler not decoded). Labelled as such in code.
3. Scene id → `.fdc` needs the table decoder on the main line (CutsceneSelect `fixdemo_table`).
4. No PES frame of the same moment (§5): needs a live shootout intro or goal camera, captured with its start offset.
5. The 15 type-99 files do not consume every byte (fdc_parse.py OPEN §5.1, unchanged).
6. The gdext build writes `gen/` and `bin/` into the main checkout's `extern/godot-cpp` through the brief-prescribed symlink (Main to know).
7. `pes_fdc_test` runs 9/9; gate and gfsim not run (Main gates).


## Main's note (28-10-26)

Main looked at `frames_pk01b/frame00000060.png`: a behind-the-goal shot with posts, pitch and stands placed coherently, players in T-pose (no cutscene animation yet). The camera track drives Godot's camera; nothing is verified against PES yet.
