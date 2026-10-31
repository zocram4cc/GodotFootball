# ProcCamera: the FixDemo procedural camera engine 0x141F7BFE0 translated and proven bit-exact on three live captures; its output decoded (31-10-26)

Branch `CutsceneTiming` (worktree `gf-CutsceneTiming`). Labels: CONFIRMED = decompile, capture read, replay, test or run this session; INF = inference; OPEN = blocker named. PES-derived material (captures, traces, frames, decompiles) stays in `gf-CutsceneTiming-scratch/pc/`, never committed.

## 0. Status against the ticket

| item | result |
|---|---|
| 1. merge `origin/main`, push | DONE (`e38fe749`); also merged `origin/MergeBatchAR` (for `snapdiff --bounded`, `--members-file`; clean merge, `1f66963a`). |
| 2. translate the closure, prove | **DONE for the captured calls.** `snapdiff.py --bounded --only 0x141F7BFE0` over `live2/pid3234533/seq6220, seq6224, seq6228` (the goal scene's camera: the init call, state 1, and two per-frame updates, state 2): **`calls=3 replayed=3 mismatches=0 own_block=0.3673 (83/226) proven=True`** (CONFIRMED). Bounded closure **235 functions** (`closure_proccam.json`). The layer regeneration is in section 4. |
| 3. decode the camera object, hand it to GF | **Decoded (section 2); NOT handed to GF.** GF has no DmCameraScene object graph for the engine to write into (section 3). The step-0 tie (which camera plays) is not decoded. |
| 4. frames | **Not done** (nothing in GF changed; no frame claim is made). |
| 5. fast tier | interim line at the end. |

## 1. What 0x141F7BFE0 is (CONFIRMED)

- `FixDemoCamera` (RTTI `.?AVFixDemoCamera@@`, vtable `0x142BB0D90`) vslot 11 (`+0x58`) = `0x141F7BFE0` (vtable slot read from the image: `0x142BB0D90 + 11*8 = 0x142BB0DE8`). The decompile's `param_1` is the FixDemoCamera, `param_2` = `0x1437F4A40` (the camera params global `FixDemoCamera::Update` fills, `docs/re/pes-re-camera-pose.md` 1.4); it is called from `FixDemoCamera::Update` `0x141F7B770` when `this+0x128 >= 1` (procedural mode, set by SetProcedural `0x141F7B630`).
- It does not pose a free camera. It configures the **`match::camera::demo::DmCameraScene`** object at `this+0x38` (RTTI of the object in `live2/pid2122415/seq91158`: vtable `0x14260B330` -> `.?AVDmCameraScene@demo@camera@match@@`): `this+0x128 == 1` (first call) builds a rig from the record (`FUN_14087D830`, `FUN_140891A70`, preset by the first zero zoom stage in record `+0xB8..+0xD0`, ~26 style bytes `+0xD8..+0xEE`, zoom-stage time map through a `std::map` of stage -> weight) and applies it with `FUN_14089D690`; later calls (`+0x128 >= 2`) update the rig clock (`+0x210/+0x214/+0x218`) and call the object's update (vslot `+0x10`), which writes the pose into the params.
- Captured call state: `seq6220` has `+0x128 = 1`, `seq6224`/`seq6228` have `+0x128 = 2`; record `+0xB0..+0xB3` = `04 00 ff ff` (mode 4, targets none), `rcx` = `0x29D850F0`, `rdx` = `0x1437F4A40`, xmm1 = the player time (10.0, 2.5317, 2.5170).

## 2. The camera params it writes (CONFIRMED by the bit-exact replays; the field meaning is INF from `FixDemoCamera::Update`'s own layout)

Final bytes of the call at `0x1437F4A40` (harness `corpus.jsonl`, dwords as float; the translated side matches them byte for byte):

| offset | seq6220 (init) | seq6224 | meaning |
|---|---|---|---|
| `+0x00..+0x08` | (37.633, 1.3289, -11.479) | (37.677, 1.3306, -11.565) | target (x, y, z), metres, Fox space |
| `+0x0C..+0x14` | (55.5, 0.6, -13.0) | (55.5, 0.6, -13.0) | eye: 3 m behind the goal line (x = 55.5), 0.6 m high |
| `+0x18..+0x20` | (0.0405, 0.99917, -0.0034) | (0.0407, 0.99917, -0.0033) | up |
| `+0x24..+0x2C` | = target | = target | second copy of the target |
| `+0x30` | 0.073177 | 0.073386 | fov, radians (4.19 deg: a long lens, the eye is 21 m from the target) |
| `+0x34`, `+0x38` | 5.9111, 500.0 | 5.8596, 500.0 | near, far |
| `+0x3C`, `+0x4C`, `+0x50`, `+0x58`, `+0x5C` | 1.317, 17.946, 1.4, 55, 1.5 | 1.317, 17.895, 1.4, 55, 1.5 | other rig outputs (focus distance 17.9 and DOF-style values; INF) |

So the goal scene's step-0 procedural camera is a **telephoto behind-the-goal camera that tracks the scorer** (target = the man the record names, a few cm per frame), not the broadcast shot. The eye (55.5, 0.6, -13) is a rig constant for this pitch side; the target moves with the man (INF: `FUN_14089AE20`/the rig read the man's position).
These are the numbers GF's cutscene camera needs for a record with no anime path (position = eye, look-at = target, fov, near, far, up). The scene's other procedural records use other rig presets and are decoded the same way once more captures land.

## 3. Why it is not in GF yet (OPEN)

The engine writes into a constructed DmCameraScene (size > `0x1BB4`: 28 sub-camera slots at `+0x38`, a rig, vtables whose slots the translated code reaches through the dispatch table) and reads the FixDemoCamera object (`+0x38` object, `+0x130` the `.fdc` camera record in PES's layout, rig clock fields). GF holds neither: its `PesCutsceneCamera` keeps `pes::fdc::CameraRecord` as parsed fields, and no GF code constructs a DmCameraScene (its constructor and the FixDemoCamera constructor `0x141F7AE60` are not in the proven closure: the captures start after construction). Handing the pose over needs: (a) the constructors translated (hook them, or capture a scene start: `fixdemo_scene_start`/`cut_offset` captures exist in `pid3234533` at `seq6218`, `seq6219`), (b) a PES-layout camera record in GF's window (`+0xB0..+0xEE` bytes from the `.fdc` record, the same bytes `fdc_parse.py` reads), (c) GF's man positions in the window the rig reads, then `match.gd` reads `0x1437F4A40`'s pose instead of falling back to the broadcast shot (`PesCutsceneCamera::update` returns false for a procedural record today, the `ponytail:` in `pes_cutscene_camera.cpp`).

## 4. What changed (tools, all regenerable; PES-derived constants are baked from the image as before)

- `tools/dev/translate`: import/libm models the engine's closure needs, each with its evidence in `stubs.json` / `import_stubs.json`: ucrtbase `_isnan`, `_finite` (exact: the DLL's own bytes, mirrored register for register in `xlate.import_model_lines`), `floorf` (`roundss`, exact), and thunk models `sinf cosf tanf asinf atanf acosf atan2f` (host libm; `ponytail:` one-ulp difference from ucrtbase possible, same caveat as `logf`/`powf`; the proof compares the harness and the translation under the same model). `import_slots.py` names slots through `SnapMem.export_of` when a DLL maps unnamed (187 slots were `unmapped` in this snapshot; ReadFile now reads as `sider.dll`, no model, no behaviour change).
- **Two harness/translator bugs found by the replays** (CONFIRMED):
  1. `snapcase._on_read` kept a stack slot in `_call_slots` after real data was written over it (a callee's `mov [rsp+0x10], rbx` into a slot an earlier CALL had pushed to) and returned 0 for the restore, so `rbx` came back 0 and the replay died at `0x1408930D7` (found by a PC/memory trace of the replay, scratch `cmp.py`). A non-push write now clears the slot.
  2. xlate emitted `if (cond) __builtin_trap();` for a conditional tail call to a far function (`0x141EAE0F0`: `jnz 0x1423D43D0`; `pdata.extent` absorbs such a target only within `MAX_RANGE`), with no marker, so the bounded closure never pulled the target in and the translated binary died with SIGILL where PES jumped. Now it is the Jcc form of the unconditional tail jump: a sibling call + return, a stub's model, or a loud abort with an `OUT-OF-RANGE JCC` marker that `snapdiff` reads. `closure.py` follows a Jcc to a function start as a tail edge. New `tools/dev/translate/test_cond_tail_call.py` (3 tests, pass). **Mutation checks not run** (budget).
- `closure.py --no-specs`: rows + set file only, for a capture-bounded closure (the 235-member specgen does not finish; only the root spec `specs/proccam/141F7BFE0.json` is committed, as for the kick chooser).
- `results_proccam.json` (`proof: snapdiff`, `closure_file: closure_proccam.json`), `gen_translated.SETS` entry `("results_proccam.json", "proccam")`, `universe.json` (+8 rows), `specgen/sets/closure-proccam.txt` (235).
- The harness error text for a snapshot unmapped access now names the last RIP.

## 5. Captures and the watcher (for Main)

- `<workdrive>/gf-Rendezvous-scratch/live2/pid3234533/` held the three `proc_camera_141F7BFE0` captures (calls 1, 2, 3 of the 10:04 goal); the watcher then released every later call unsnapshotted ("disk under 400 GiB"; I messaged Main, who freed space at ~10:20). **Keep `seq6220, seq6224, seq6228` FULL** (they are the proof's input; a slim copy is read-set only and verified for these exact calls, but a re-translation after a closure change reads the same set, so slimming is safe once the layer is committed, not before).
- The cap is 12 per launch and the hook fires only after a goal's anchor calc: more captures (other steps' records, other presets, `+0xB0` modes 5 and 6 with a man target) arrive at the next goals, and widen the closure (`snapdiff --bounded` regenerates `closure_proccam.json`).

## 6. Left (numbered)

1. **GF handoff** (ticket item 3): the construction of the DmCameraScene/FixDemoCamera objects in GF's window (section 3), then `match.gd` reads the pose; delete the broadcast fallback for procedural records in the same change.
2. Step-0 tie of `0x300bd` (which of `outH10` / `SL` / `SR` plays at tick 180): `selectByRate` consumer still not found.
3. Frames: a GF goal movie vs a PES burst with the telephoto behind-the-goal camera (section 2) once item 1 lands; no frame claim is made here.
4. More captures for the other records (`+0xB0` mode 5/6, man targets), `snapdiff --bounded` over them.
5. Compile the layer in the GF build (`cmake --build build`): not run. `pes_stubs.hpp` changed (new models), so every translated TU rebuilds (4,757 functions); the 231 camera members compiled and linked under `g++ -O1` in the `snapdiff` builds (same generator and headers).
6. Prior proofs on the same harness: `snapcase._on_write` now clears a return-address slot on a real data write. A previously proven record whose replay read such a reused slot would have been proven under the old dead-slot-0 reading; re-run `snapdiff --bounded` on the other records (`pes_*_recorded` tests and `diff_gen_*` are Main's gate) to see none changes.

## Layer and gate (CONFIRMED)

- Layer regenerated: **4,757 functions** (3,707 recorded, 1,050 closure), `gen_translated.py --check` **rc 0, byte-identical** after the commit. The camera closure holds **231** functions; the four PES allocator entries that stand as native services (`0x1403FBC10`, `0x1403FBCB0`, `0x1403FBD00`, `0x141B34C30`) are left out (`gen_dispatch` refuses a function that is both translated and a native service); `snapdiff` had them in its non-native build.
- The tanf thunk `0x1415A17D4` was a translated closure member (`specs/core/1415A17D4.json`, from the cutanchor/gate closures); it is a model now (clean cutover: spec and `pes_fn_1415A17D4.cpp` removed; its callers `0x1404BAE20`, `0x1403FD830` regenerated).
- After the layer commit the proof was re-run: `calls=3 replayed=3 mismatches=0 proven=True`.
- SHAs on `origin/CutsceneTiming`: `c7f7b2be` (layer + proof), `648de03d` (conditional tail calls, snapcase fix, test), `b90c3324` (import/libm models), `673deb0b` (root spec); merges `1f66963a` (MergeBatchAR), `e38fe749` (main).

## Tests run

`test_cond_tail_call.py` 3/3, `test_closure_walk.py` 22/22, `test_tail_jump_loud.py` 9/9 (harness Python). `snapdiff --bounded` 3/3 bit-exact (twice: before and after the layer commit). `gen_translated.py --check` rc 0. No ctest binary, Godot or gfsim run; no mutation check run for the Jcc change.

**`GATE fast (interim, tools/dev/gate.sh not run; machine load; every translated TU rebuilds): gen_translated.py --check GREEN (4,757 functions byte-identical); test_cond_tail_call 3/3, test_closure_walk 22/22, test_tail_jump_loud 9/9; snapdiff --bounded 3/3 bit-exact. The GF build of the regenerated layer and the pes_* binaries linking it were NOT run.`**
