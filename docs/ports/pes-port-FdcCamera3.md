> **Note (Main, 29-10-26):** the refutation below concerned `FixDemoHuman`, not the camera. The PES camera's rotation IS a shortest-arc slerp (`docs/re/pes-re-camera-pose.md`), so keeping GF's slerp was right; see that report for the remaining differences.

# PES cutscene camera sampler: decode check, reverted port, build, no frame pair (FdcCamera3, 28-10-26)

Branch `FdcCamera` at `bb355b4a` (`git ls-remote origin FdcCamera` = `bb355b4a`). Worktree `<workdrive>/gf-FdcCamera`. Labels: CONFIRMED = decompile, image read or test output with VA; INF = inference, reason given; OPEN = blocker named.

## 0. Outcome against the ticket

| item | status |
|---|---|
| 1. merge origin/main, push | DONE. Merge `e2d87a33` pushed. Later reverts sit on top (below). |
| 2. port PES sampler semantics (lerp, per-component quaternion lerp, 30 fps, start offset, near/far clamps), red/green/mutation tests | NOT SHIPPED. The decode the ticket cites does not say what the doc claims. I ported the lerp, tested it red/green/mutated, then reverted it (`6b9a6ae4`, `bb355b4a`). The tree equals `e2d87a33` in `src/` and `tests/`. Details §2. |
| 3. build godotfootball; render entrance camera at 0.5 s, 1.5 s, 3 s | BUILD DONE: rc=0, 1883 s wall (incremental after the merge, load ~25 on 16 cores). RENDER NOT DONE: the camera pose path is undecoded, so a render would not be a PES-derived camera. |
| 4. READY FOR ENTRANCE / PES frame burst / paired frames | SKIPPED per Main's handover (no entrance restart this session). No `READY` file written. No frames taken. |

Nothing is verified against PES. No camera comparison (framing, angle, fov) exists.

## 1. Decode check of the cited sampler (CONFIRMED, this pass)

Source of the decompiles: `<workdrive>/gf-OpenDecodes-scratch/dec_chan.txt`, `dec_animsample.txt`, `dec_camsample.txt` (Ghidra headless `dec.sh`, image `pes_full.bin`), plus `dec_acc.txt` (this pass, 12 accessors).

**1.1 `0x141eba8c0` is NOT a quaternion lerp.** The doc `pes-re-open-decodes.md` §1.2/§1.3 says it is the rotation channel, 4-component, per-component lerp. The body (`dec_chan.txt:185-321`) does this:
- For each key index `i` of a track (count `iVar5`), reads through accessors: start `t0 = u16(key+0x2a)/30` (`0x141ed62c0`), end `t1 = u16(key+0x2c)/30` (`0x141ed6170`), id `u16(key+0x28)` (`0x141ed6200`), value0 `u16(key+0x2e)/30` (`0x141ed6290`), value1 `u16(key+0x30)/30` (`0x141ed6260`), fade-in `u16(key+0x3c)/30` (`0x141ed61a0`), fade-out `u16(key+0x3e)/30` (`0x141ed61d0`), weight scale `float(key+0x38)` (`0x141ed6230`). Key record stride `0x70`, base `+0x18` of the track record. CONFIRMED (`dec_acc.txt`).
- Key active when `t0 <= s <= t1` (last key: `s >= t0`) (`dec_chan.txt:231-246`).
- Value: `out = value0 + ((s - t0) / (t1 - t0)) * (value1 - value0)` (`dec_chan.txt:270-271`). Output is ONE float per active key, plus the id byte and a weight (fade-in/out ramps × weight scale, `:272-307`). Up to 4 entries (`:310`).
- The output is a blended local time for a sub-track, not four quaternion components.
- Guard: if `value1 - value0 <= 0` the delta becomes `1e-4` (`_DAT_1425e49f0`, `:257-258`). A decreasing segment therefore does not reach `value1`. Whether this touches camera tracks is OPEN.

**1.2 Time unit: keys and sample time are seconds, keys are 30 fps frames.** `key/30` appears on every key read, and the sampler compares them with `s` directly. So `s` is seconds and `key` is a 30 fps frame index. CONFIRMED for the anime track. The GF reader's `frame = seconds * 30` is consistent with this (`pes_cutscene_camera.cpp:99-102`). The `+0x04` record tick (1/60 s) is INF: no decode isolates the conversion.

**1.3 The 0.001 and 0.05 constants are not near/far clamps.** The doc §1.4 says they clamp the far plane against the near plane. In the vfunc `0x141f7e510` (`dec_camsample.txt:210-292`):
- `0x1425a9668` (= 0.001) enters `fVar4 = (clip_end - 0.001) - t` (`:236`), a clip-end time bound.
- `0x1425a8a28` (= 0.05) is a threshold on the remaining time (`:244`), used to decide when the clip has ended.
- Neither touches near or far. The doc's near/far claim is REFUTED at these VAs.

**1.4 Near/far path: OPEN.** `0x141f7ecc0` (`dec_camsample.txt:49-177`) is clip setup (it resets the anime object and sets `node+0x40`, `+0x70`, `+0x74`). It does not push near or far. The sink of `node+0x5b`/`+0x5c` is not decoded this pass.

**1.5 Sampler dispatch.** `0x141eb9640` (`dec_animsample.txt:1-192`): `param_2 < 0x122d` and a valid track (`0x141ed66e0`) → four sub-samplers (`0x141ebac80`, `0x141eba8c0`, `0x141ebb160`/`0x141ebb640`) into node `+0x50`, `+0x80`, `+0xb0`, `+0xe0`. The caller `0x141f7e510` passes `node+0x6c` as the track id and `s = fVar5` (`:267`). Which of the four slots is the camera pose is OPEN (the consumer is not decoded).

## 2. The sampler port, and why it was reverted

- **Red:** I flipped `PesFdcCanm.SamplesLinearPositionAndSlerpsRotationBetweenKeys` to assert per-component lerp at mid (`rotation[1] = 0.5·sin45°`, `rotation[3] = 0.5·(1+sin45°)`, `|q_yz|² < 0.9`). Commit `48f4875a`. Run against the slerp code: FAILED at `tests/pes_fdc_test.cpp:297-299` (got `0.38268`, `0.92388`, norm 1.0).
- **Green:** `src/pes/fdc.cpp` `SampleCanm` rotation → `Channel(c, 0, frame)` (per-component lerp, no renormalise); deleted `Slerp` and `Normalize`; `src/pes/fdc.hpp` comment updated. Commit `b3a483ff`. `pes_fdc_test`: 9/9 PASSED (with `PES21_ROOT=<workdrive>/PES21`).
- **Mutation N (renormalise the rotation):** `pes_fdc_test --gtest_filter=PesFdcCanm*` FAILED (the lerp test). Reverted.
- **Mutation S (restore slerp via a `SlerpMutant` helper):** FAILED at `:297`, `:298`, `:299`. Reverted.
- **Why reverted:** §1.1 shows the cited VA is not a quaternion lerp. Shipping a lerp "matches the decode" would be false. `6b9a6ae4` and `bb355b4a` revert both commits (no force-push). The tree equals `e2d87a33` in `src/` and `tests/` (`git diff e2d87a33 -- src tests` empty). 9/9 PASSED at `bb355b4a`.
- **Result:** the GF rotation is still slerp (`src/pes/fdc.cpp:121`, GF's own choice, no PES citation). The PES rotation rule is OPEN.

**camera_cut.py:** the ticket names `tools/dev/cutscene/camera_cut.py`. It does not exist on `origin/main` or in any `gf-*` worktree (`tools/dev/cutscene/` holds `fdc_parse.py`, `replay_verdicts.py`, `scene_map.py`). The only slerp is the C++ one above. Nothing to fix there.

**Clamp:** `PesCutsceneCamera::update` clamps near to `kMinNearM = 0.1` (`src/gdext/pes_cutscene_camera.cpp:19`), which cites `PES21_CAMERA_TRACE.md` §6.4 as GF's stand-in. Not PES's near rule. Unchanged.

## 3. Build

- `cmake --build build -j3 --target godotfootball` (nice 19, ionice 3), on the merged tree `e2d87a33`: rc=0, wall 1883 s (31 min). Output `godot/bin/libgodotfootball.so`. Machine load ~25 on 16 cores. Build log `<workdrive>/gf-FdcCamera-scratch/build-merged.log`.
- The binary matches `bb355b4a` in `src/`, since the reverts restore the same sources.
- No render was run. The entrance cut needs a Godot run with a display and the entrance scene; that was not attempted once the sampler was found unsupported.

## 4. Frame pair

Not attempted. Main skipped step 4 (no entrance restart this session). No `READY` file. No PES frame of the entrance cut. The live screen was not checked this pass.

## 5. Open items (numbered, with blocker)

1. **Camera pose composition.** Decode the consumers of the four sub-track slots at node `+0x50`/`+0x80`/`+0xb0`/`+0xe0` (callers of `0x141eb9640`, and `0x141eba5b0`/`0x141eb95e0` after each call). Blocker: not decoded. This decides which slot is position, rotation, fov.
2. **CANM loader.** Decode `0x141f7de00` (`.canm` load): it turns the file's type 1 (vec4), type 4 (vec4) and type 8 (scalar) channels into the in-memory 0x70-stride key records. This settles the rotation representation, which the file alone does not. Blocker: loader not decoded.
3. **Near/far.** Decode the sink of node `+0x5b`/`+0x5c` (`0x141ebf690`/`0x141ebfab0`, per the older doc; not re-read here). Blocker: not decoded.
4. **Start offset.** Find the caller of `0x141f7ecc0` that supplies `param_5` (node `+0x40`) and whether record `+0x04` (1/60 s) is converted to seconds there. Blocker: caller not decoded; the 1/60 s unit is INF.
5. **Decrease guard.** Check whether the `value1 <= value0 → 1e-4` guard (`dec_chan.txt:257-258`) applies to camera tracks. Blocker: depends on 1 and 2.
6. **Render and pair (ticket 3 and 4).** After 1-5: relink `godotfootball` (~30 min under this load), render `--fdc-scene 0x10001 --fdc-camera 0` at 0.5 s, 1.5 s, 3 s. Then the PES side needs an entrance restart (Main). Blocker: 1-5, and Main's restart.
7. **Doc correction.** `docs/re/pes-re-open-decodes.md` §1.2/§1.3/§1.4 must be corrected: `0x141eba8c0` is not a quaternion lerp; the 0.001/0.05 are not near/far clamps. Not edited here (report only).

## 6. Scratch

- `<workdrive>/gf-FdcCamera-scratch/dec_acc.txt`: decompiles of the 12 accessors (`0x141ed6640`-`0x141ed6290`).
- `<workdrive>/gf-FdcCamera-scratch/build-merged.log`, `fdc-*-build.log`: build logs.
