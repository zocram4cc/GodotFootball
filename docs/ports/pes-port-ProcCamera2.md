# ProcCamera2: the FixDemo camera object graph decoded, the constructor chain run in the harness, three import models and one latent memcmp bug fixed (31-10-26)

Branch `CutsceneTiming` (worktree `gf-CutsceneTiming`). CONFIRMED = decompile, capture read, replay, test or run this session; INF = inference; OPEN = blocker named. PES-derived material stays in `gf-CutsceneTiming-scratch/`, never committed.

## 0. Status against the ticket

| item | result |
|---|---|
| 1. merge `origin/main`, push, build the layer into `godotfootball` and the `pes_*` tests | merge pushed (`9f2a9fca`). The build of the regenerated layer is running (`nice -n 19 cmake --build build -j4 --target godotfootball`, 4,892 steps); result in section 6. The `pes_*` tests were NOT built (see 6). |
| 2. translate the DmCameraScene / FixDemoCamera constructors, place the object in GF's window, run the engine per frame | **NOT done.** The constructor chain (80 static members, 76 in the capture-bounded closure) runs to completion in the harness on PES's bytes (5/5 synthetic calls, `err: null`, rax = the block). The translated side does not yet match (section 3). No GF object, no per-frame run. |
| 3. hand the pose to `match.gd` | **NOT done** (needs 2). The broadcast fallback for procedural records is still in. |
| 4. step-0 tie of `0x300bd` | **NOT looked at** (budget). |
| 5. frames | **None.** No visual claim is made. |
| 6. fast tier | interim line at the end. |

What landed (pushed, `9e349131`): three import models, a model bug fix, the constructor call-state tool, tests, the root spec, rip-relative slot naming. Section 4.

## 1. The camera object graph (CONFIRMED by the decompiles and a snapshot walk of `live2/pid3234533/seq6224`)

`FixDemoCamera` (`0x228` bytes, vtable `0x142BB0D90`; ctor `0x141F7AE60`, called from the layer runner `0x141F2F6B0` state 3, which allocates it through `0x1403FB410(pool 0x19, 0x228)` and stores it at runner+0xC68):

| offset | what | evidence |
|---|---|---|
| +0x08 | a `0xB8`-byte anime-interface object (`0x1408903F0` base ctor, vtable `0x142BB0D80`) | ctor decompile |
| +0x10 | `DmCamEnv` (vtable `0x14260B250`), built by `0x14088FFB0`: it allocates the `DmCameraScene` (`0x1D88` bytes, pool 0x18, ctor `0x1408905F0`, vtable `0x14260B330`) and stores it at env+0x28 = **FixDemoCamera+0x38**, then `0x14089AEE0` fills the env | decompiles; RTTI walk of seq6224 |
| env+0x38.. | 28 `CameraTargetInfoRef` (vtable `0x142608738`, stride 8 pointers; each `0x40` bytes: +8 resolved flag, +0x10 handle H, +0x28 the name `CAMERA_TARGET_%02d%02d` = format `0x142608750`, group 02, slot 00..27) | snapshot walk (names printed) |
| +0x128 | rig state: 0 clip, 1 first procedural call, >= 2 update | `0x141F7B630` (SetProcedural) stores 1 at +0x128, record+0xB1 at +0x224, zeroes +0x210/+0x214/+0x21C, sets +0x220 = +0x222 = 1 |
| +0x130 | the camera record in disk layout (bytes +0xB0..+0xB3 = mode, targets; +0xB8..+0xD4 zoom stages; +0xD8..+0xEE style bytes; +0xFC/+0x104/+0x10C floats) | engine decompile |
| +0x214 / +0x218 | player time / delta, set by vslot 9 `0x141F7B4A0` (state > 0: +0x218 = new - old) | decompile |

Per frame PES calls `FixDemoCamera::Update` (vslot 4, `0x141F7B770`): if `+0x222 || +0x218 != 0` and `+0x128 >= 1` it calls the engine (vslot 11 `0x141F7BFE0`), then `0x141F7B680` (camera-manager cut flags) and `0x141F7B250` (the widescreen fov, `PesCutsceneCamera::set_widescreen_fix` already holds that option).

### 1.1 Where the target comes from: a registry, not the world (CONFIRMED)

The engine reads **no match-world global** (the static references of the 231 closure functions are listed in 1.2). The camera's target is a `CameraTargetInfo` record found through the Fox name registry:
- `CameraTargetInfoRef` resolves lazily through `0x140871FF0` -> `0x141B55730` (a find-or-create on a registry object reached by `0x141B55F40` -> vslot +0x38; lookup vslot +0x70, insert +0x68; not-found callback `0x140421F40`, creator `0x14086C830`). `DmCamEnv::Init` `0x14089AEE0` resolves all 28 eagerly, inside the constructor.
- Handle layout read from seq6224: `ref+0x10 = H`, `*(H+0x10) = R`, `info = R+4`. In the captured goal only slot 1 (`CAMERA_TARGET_0201`) holds a valid record. `info+0x00` byte valid; `+0x04..+0x0C` ground position (37.65, 0.00, -11.43); `+0x10..+0x18` (37.71, 1.59, -11.52) (INF: head); `+0x1C..+0x24` (37.64, 0.94, -11.40) (INF: chest/hip); `+0x28` float, facing degrees (149.98). Readers: `0x14089AD80` (+4), `0x14089AC40` (+0x1C, +0x10), `0x14089AB80` (+0x28, wrapped by 360), `0x14089C2C0` (+0).
- The pose target (37.677, 1.3306, -11.565) of the proof's call 2 is a blend of those vectors (INF; the engine's DmCamTarget `0x52819318` read 37.669 from slot 1).
- INF: so the "scorer-tracking telephoto" is a registry slot written by the match's human objects (writers not found: the registry users of the name format are `0x141F36450`, `0x14217D340`, `0x14217C4B0`, `0x1409227D0` (PLAYER_%s_%02d), `0x140870020`, `0x140A4A300`, `0x1420C0D70`). GF needs those 28 records filled per frame from its players (ground position, two body points, facing) and a registry whose find returns them.

### 1.2 Statics the engine reads (CONFIRMED by a read-before-write trace, scripts in the scratch dir)

Calls 6220 (init) and 6224 (update): the camera preset table `0x1434A0290..0x1434A0E88` (28 presets of 0x30 bytes: constants in `.data`, read from the install); the stack cookie `0x14350B340`; `0x143516830` and `0x1437C9238` (pointers, `0x141957C50/C60`); fps `0x14351AE54`; `0x1436EDE58` (= 10,000,000, the QPC frequency); the allocator globals `0x1436EF2B0/2C8` and pool table; `0x1436F36B8`, `0x1437067A0`; the camera manager singleton `[0x1437F4820]` (read by `0x141E9BA30`) and `[0x1437F4810]` (`0x141E92C20`, an `EffectList`); `0x1437F0110/18`, `0x1437F1000/1048`. Statics inside the closure's own code: the noise/shake state `0x1436F33D8..0x1436F340E` (`0x14089C270`), the RNG `0x1436F36A8` (`0x140899030`). None is a world object.

## 2. Constructor chain, harness side (CONFIRMED)

`synth_ctor.py` builds the call state of `0x141F7AE60` from a full capture: its registers, rcx at a block (the unused stack below rsp, or a live object), `mem.bin` a symlink. Five states (seq6218, 6219 = the scene-start captures, block in free stack; 6220, 6224, 6228 = the proc_camera captures): `frame_trace.py recorded` runs PES's bytes to the constructor's return in all five (rax = rcx, `err: null`). The chain calls the allocator `0x1403FB410` (pools 0x18/0x19), `0x1403CF8B0`/`0x1403815F0` (snprintf: needed the new vsprintf and memchr models), the registry find (needed the memcmp fix), and the SRW shared lock (named slot `0x14252F488`).

## 3. Constructor chain, translated side (OPEN: the blocker)

`snapdiff.py --bounded --only 0x141F7AE60` over the five states: **5 replayed, 5 mismatches, proven=False.** All five end the same way: the translated run aborts with `PES_DISPATCH_UNTRANSLATED: 0x140421F40`, the registry find's not-found callback. So the translated lookup misses where PES's finds. Before that, two real faults were fixed (section 4): the unmodelled SRW slot (`0x6FFFFFBBD300` = the image's dump-time value; the snapshot holds `...D380`) and the missing models. Not yet decided between: (a) another slot or `.data` value the translated run reads from the image's dump-time bytes where the harness reads the snapshot's (the first failure was exactly that: `pes_full.bin` holds `0x6FFFFFBBD300` at `0x14252F488`, the snapshot `0x6FFFFFBBD380`); (b) a model mirror that differs from its `code_hex` (vsprintf: `pes_model_vsprintf_s` vs `vsprintf_model.s`; memcmp). Next step: run `snapdiff_141F7AE60` under gdb past the SRW call and dump the name string and the registry bucket compared (`gdb -batch -ex run -ex bt --args ./snapdiff_141F7AE60 ...` works; the command line is in section 5), or write the C++ and harness values of `[buf]` after the vsprintf call into one table.

Even once the constructor proves, GF has no registry: the plan is the heap's pattern: leave `0x141B55730`'s closure out of the committed layer, and make the registry object GF's own (singleton `0x141B55F40` result, vslots +0x38 / +0x70 / +0x68 as native services over a table of the 28 records), the way `PesHeap` stands for the allocator; the engine and constructors then run unchanged on it. The per-frame driver is then `SetProcedural` (7 stores) + the time setter + `pes_fn_141F7BFE0` with rcx = the camera, rdx = `0x1437F4A40` and the pose read from the params block (section 2 of `pes-port-ProcCamera.md`).

## 4. What changed (pushed: `bf1fa537`, `9e349131`)

- **`memcmp` model bug (CONFIRMED, old).** `import_stubs.json` `VCRUNTIME140.dll!memcmp` began one byte late (RVA `0x11F1`): it dropped the REX.W prefix of `48 2B D1` (`sub rdx, rcx`) and ran `sub edx, ecx`. The DLL's bytes were read from `live2/pid3234533/seq6228` at `0x6FFFFB8011F0`. Right only when both pointers share their upper dword; a heap pointer against an image string read `0x2033F4FC0`. Earlier proofs compared heap against heap, so none changes. Fixed (`482bd1...`), `do: memcmp` mirror added (`xlate.import_model_lines`).
- **`ucrtbase.dll!__stdio_common_vsprintf_s` model** (`import_stubs.json`, source `tools/dev/translate/vsprintf_model.s`, mirror `pes_model_vsprintf_s` in `pes_stubs.hpp`): literal text, `%%`, `%s`, `%[0][NN]{d,i,u,x}`; anything else, a NULL `%s`, a zero count or an overflow is `int3`. `test_vsprintf_model.py` 6/6 (formats against Python's, callee-saved registers and rsp restored, the loud stops).
- **`VCRUNTIME140.dll!memchr` model** + mirror; `test_memchr_model.py` 5/5.
- `synth_ctor.py` (constructor call states from a full capture); root spec `specs/proccam/141F7AE60.json`.
- `closure.py` now writes rip-relative indirect calls as the bridge's absolute `CALL qword ptr [0x...]` (`import_slots.py` could not read the others, so rows decoded from the image never had their slots named); `import_slots.json` +2 named slots.
- Layer regenerated for the models: 3 closure functions changed (`0x1403CF8B0`, `0x1415A1708`, `0x1415A1720`); `gen_translated.py --check` was 3 drifts before, 0 after (regeneration rc 0, 4,757 functions).
- No mutation checks were run on the new tests.

## 5. Reproduce

Synthetic captures and traces: `synth_ctor.py <live2 capture> <out> --entry 0x141F7AE60 --hook-name fixdemo_camera_ctor_141F7AE60 --rcx stack`; `frame_trace.py recorded <out> --hooks hooks.cfg --spec-dir tools/dev/diffharness/specs/proccam --out <tr>`; `snapdiff.py --bounded --image $PES_EXE --out-dir <sd> --spec-dir tools/dev/diffharness/specs/proccam --hooks hooks.cfg --snapshot <out>=<tr> ... --only 0x141F7AE60 --results <json>`. The failing binary: `<sd>/gen/141F7AE60/snapdiff_141F7AE60 <sd>/gen/141F7AE60/snap_seqN/ok_calls.bin <capture>/mem.bin .../maps.txt out.jsonl .../ok_delta.bin <tr>/pool/pages.bin <teb>`.

## 6. Build and gate

Build (CONFIRMED): `cmake --build build -j4 --target godotfootball` over the regenerated layer, 4,892 steps, **0 FAILED**; `godot/bin/libgodotfootball.so` linked 12:14 (the 231-function camera engine closure and the three regenerated functions compiled; `pes_stubs.hpp` now includes `pes_state.hpp`, no breakage). Nothing needed fixing in the C++ build. Targets built and run: `pes_native_heap_test` 9/9 (`build/tests/pilot/`), `pes_camera_test` 17/17, `pes_cutscene_timeline_test` 7 passed + 1 skipped (`CapturedGoalsReplay`, needs captures), `pes_fixdemo_select_test` 6/6. Python: `test_vsprintf_model` 6/6, `test_memchr_model` 5/5, `test_cond_tail_call` 3/3. The other ~170 `pes_*` targets were not built (budget); Godot and `gfsim_run` not run.

**`GATE fast (interim; tools/dev/gate.sh not run, machine load): gen_translated.py --check was RED (3 drifts) after the model changes and was fixed by regenerating (rc 0, 4,757 functions; not re-run after the commit); libgodotfootball.so builds clean; pes_native_heap_test 9/9, pes_camera_test 17/17, pes_cutscene_timeline_test 7/7 (+1 skip), pes_fixdemo_select_test 6/6; harness Python model tests 14/14. The ctor proof is unproven (5/5 mismatches, section 3) and not in the layer.`**

## 7. Left (numbered)

1. Find why the translated registry lookup misses (section 3), prove the constructor (5 states), add it to `results_proccam.json` + the layer.
2. GF-native registry (heap pattern) holding the 28 `CameraTargetInfo` records (layout 1.1), filled per frame from GF's players; find their PES writer to port the two body points and the facing.
3. GF rig class `pes::FixDemoCameraRig` (PesState window, statics of 1.2 from the install, camera manager stand-ins `[0x1437F4820]`/`[0x1437F4810]`, the params block `0x1437F4A40`), `CameraRecord` keeping the raw record bytes, `PesCutsceneCamera::update` taking the pose for empty-anime records; delete the broadcast fallback in `match.gd` for them in the same change.
4. Step-0 tie of `0x300bd` (`selectByRate` consumer).
5. Frames: a GF goal movie vs a PES burst (the telephoto eye (55.5, 0.6, -13), fov 4.19 deg, `pes-port-ProcCamera.md` section 2).
6. The engine's proof covers 83 of 226 own blocks (modes 4 only): other records' presets/targets abort loudly in GF until more captures widen the closure.
