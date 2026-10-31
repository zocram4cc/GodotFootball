# ProcCamera4: GF's rig reproduces PES's procedural camera pose bit-for-bit on three captured goal frames; scorer handed to the mode-4 camera (31-10-26)

Branch `CutsceneTiming` (worktree `gf-CutsceneTiming`), pushed `81056099` (`git ls-remote` confirmed). CONFIRMED = run, capture read, replay, test or decompile this session; INF = inference; OPEN = blocker named. PES-derived material (captures, traces, dumps) stays in `gf-CutsceneTiming-scratch/`, never committed.

## 0. Status against the ticket

| item | result |
|---|---|
| 1. merge origin/main, push; build | DONE (`77fc2081`, conflict only in `docs/ports/README.md`). `godotfootball`, `pes_proc_camera_test` built once at `nice` (4,814 TUs, about 55 min under load 25). |
| 2. native registry (heap pattern) | DONE. `PesCameraRegistry` (pes_native.hpp/.cpp): GameImpl singleton `[0x1437F0AC8]`, RegistryImpl whose vtable slots `+0x70` / `+0x68` are the GF-private VAs `0x2500000070` / `0x2500000068`, routed by `stubs.json native_services` (`registry_find`, `registry_insert`) to `pes_native_registry_find` / `_insert`; 28 handles + records of group 02 in the arena. The translated find `0x141B55730` reaches it unchanged; a foreign name answers 0, the not-found callback runs, the creator reaches the aborting insert (loud). |
| 3. rig | DONE for mode 4. `pes::FixDemoCameraRig` (src/pes/fixdemo_camera_rig.*): FixDemoCamera on the arena via the proven ctor `0x141F7AE60`, `CameraRecord::raw` (284 bytes) copied behind `+0x130`, SetProcedural (`0x141F7B630`, 7 stores) and the time setter (`0x141F7B4A0`) written from the disassembly (`ponytail:` in the code: not hooked, not in the layer), engine `0x141F7BFE0` per frame. `PesCutsceneCamera`: a mode-4 record with an empty anime path gets a rig camera; `advance(t_s)` (once per logic frame) + `set_target(...)`; `update()` poses the Godot camera from the engine's eye/target/up/fov/near/far. Other procedural modes keep the broadcast fallback (`ponytail:` naming `0x141F7BFE0`: the translated engine aborts loudly on an uncaptured mode). |
| 4. oracle FIRST | DONE. `tests/pes_proc_camera_test.cpp`: seq6220 (init), seq6224, seq6228 of pid3234533, record + CAMERA_TARGET_0201 + time pinned from `tools/dev/cutscene/proc_camera_cases.py`; the GF rig's params block ties PES's **word for word (eye (55.5, 0.6, -13), fov 0.073177 / 0.073386 / 0.073386 rad, near 5.9111 / 5.8596 / 5.8260, up, target, far 500, focus 17.946 ...) on all 24 words except +0x54**, which the engine does not write (a write trace of PES's bytes over seq6220 stores 27 words of the block, none at +0x54: the capture's 0.002 is an earlier writer's value). `2/2` pass. |
| 5. hand the pose to `match.gd`; frames | Code DONE (below). **Frames NOT done**: no Godot goal movie was run, no pixel claim is made. |
| 6. fast tier | interim line at the end. |

## 1. What was wrong, and the stand-ins (section 3 is the list)

Two things stopped the first green: (a) `pes_full.bin` is a flat dump whose PE section raw pointers are the installed exe's, so `PeImage::Open(PES_EXE)` read wrong bytes (the test opens `PES21_ROOT/PES2021.exe` like every other PeImage test); (b) the install's `.data` holds zeros or pre-load values where PES's startup code writes runtime state.

## 2. How the rig reaches PES's bytes

- `.rdata` (read-only) and `.data` (writable) of the install are read through `PeImage::ReadVa` (a chunk the file does not map whole is read per 0x200 sector, the zero-filled tail stays zero). Vtables, constants, the preset table `0x1434A0290..` and fps `0x14351AE54` come from there.
- Allocator: `PesHeap` as the selector. Registry: section above. The record's target byte `+0xB1` = 0 and the scorer in slot **1** (CAMERA_TARGET_0201): slot 2 fails the oracle (mutation M1).

## 3. Runtime stand-ins (each in `fixdemo_camera_rig.cpp`, named, with the PES writer that removes it)

| VA | value | what | verdict |
|---|---|---|---|
| `0x14350B360` | 5 | CPU feature level (CRT cpu init) | **not needed by the pose**: mutation M2 (value 1) still ties; kept only because PES reads it in the registry hash GF no longer runs. INF; delete if nothing else reads it. |
| `0x1436EDE58` | 10,000,000 | QueryPerformanceFrequency | read in the capture; unproven by the oracle |
| `0x1437F0110/18`, `0x1437F1000/1048`, `0x143AC3DD0` | forwarder / heap static objects | static initialisers of the allocator objects | read by the closure's allocator calls; OPEN: not individually mutated |
| `0x1436F36A8` object (words at +8..+0x14 = 0x2F3D0..0x2F3D3) | the RNG's pre-first-draw state (capture seq6220) | camera RNG `0x140899030` | **needed**: with the words at +0x10 (wrong offset, my first attempt) the pose is wrong (M4 fails the oracle); the right offset ties. |
| `[0x1437F4820]` manager, `[0x1437F4810]` effect list | zeroed block, null | render-layer owners | `0x141E9D2F0` skips its whole body when `manager+0xA68 == 0`; the pose still ties, so those layer flags are not an input of the pose (CONFIRMED by the tie). |
| `camera+0x138` (layer mask object) | null | the engine ORs a 0x38-byte mask into the flags it passes the manager (`0x141F7B130`) | flags only; the tie holds |
| time | `Step(timeTicks)` | **the oracle does not pin it**: mutation M3 (never storing `+0x214`) still ties, because this record's zoom stages are all off. A record with time-dependent zoom stages is untested. OPEN. |

## 4. Scorer, body points, axes (match.gd / GfMatch)

- `GfMatch::cutscene_actor()` (new, bound): `{index, ground (Fox axes, y 0), facing_deg}` of the goal's scorer (`gateScorer_`) while a goal timeline runs.
- `match.gd::_advance_cut_camera`: each logic frame of the scene, `set_target(1, ground, sk_head, sk_root_hip, facing)` then `advance(clock_s)`. **The two body points are INF**: in the captured goal the registry record's points are the demo actor object's (`vtable 0x142BB1048`, ctor `0x141F7D010`) joint entries 10 and 0 (`actor+0xF4`, `actor+0x7C`, stride 12; 1.595 m and 0.941 m above the scorer, found by byte-matching the record in `seq6224`). Which GF bones stand for them (`sk_head`, `sk_root_hip`) is a height match, not a decode; the writer of CAMERA_TARGET_02nn is undecoded. The skeleton is the render frame's, up to a logic frame old.
- Group/slot semantics: only the scorer in slot 1 is shown by a capture; every other slot stays `valid 0`.

## 5. Verification run

- `pes_proc_camera_test` 2/2 (`PES21_ROOT=<workdrive>/PES21`).
- Mutations (each run, rebuilt, reverted by `git checkout`): M1 scorer slot 1 to 2: **fails** (both tests); M2 ISA level 5 to 1: survives (not needed); M3 time store removed: survives (time not pinned); M4 RNG words at +0x10: **fails**. Earlier, before the RNG fix, the wrong-statics runs failed on 13 words, which is how the RNG was found.
- `test_native_services.py` 3/3 (dispatch regenerated; **only `pes_dispatch_gen.cpp` was rewritten** so the 4,789 TUs did not rebuild; it changed by 4 lines).
- `godotfootball` builds (links `PesCutsceneCamera`, `GfMatch::cutscene_actor`). No Godot run, no `godot/tests/test_cutscene_camera.gd` run.

## 6. Left (numbered)

1. **Frames**: a GF goal movie with the scorer framed and a PES burst of the same shot (telephoto from behind the goal line, `0x300bd` step 0). Run `godotfootball` with a goal cutscene; check the Godot camera frames the scorer (the Basis built from eye/target/up, the vertical fov in `KEEP_HEIGHT`). Not done; no visual claim.
2. Identify the GF bones for actor joints 10 and 0 (decode the writer of `actor+0x7C` entries / CAMERA_TARGET_02nn: the FixDemo actor update near `0x141F7D010`).
3. A record with time-dependent zoom stages, and the other procedural modes (5, 6, others), need captures; the translated engine aborts on them today.
4. Drop `0x14350B360` if no other consumer needs it; mutate the allocator-static stand-ins.
5. The camera's `+0x144`-style fields PES sets after the ctor (garbage `0x7e...` words are heap fill, not inputs) are not set by GF; the tie shows the pose does not read them.

## Gate

**`GATE fast (interim; tools/dev/gate.sh not run: budget and load average 25): pes_proc_camera_test 2/2, test_native_services 3/3, godotfootball builds; gen_translated.py --check and pes_fixdemo_select_test result below; no other pes_* ctest binary, no Godot run, no gfsim_run.`**
