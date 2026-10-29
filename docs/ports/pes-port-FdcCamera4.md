# PES cutscene camera pose in GF (FdcCamera4, 29-10-26): FixDemoCamera ported, worked example bit-equal; frame pair not like-for-like

Branch `FdcCamera`, worktree `<workdrive>/gf-FdcCamera`. Labels: CONFIRMED = test, decompile or image read with VA; INF = inference, reason given; OPEN = blocker named.

## 0. Outcome

| ticket item | result |
|---|---|
| 1. merge `origin/main`, push | DONE. Merge `7f3a763d` (the symlink trap: `rm extern/godot-cpp`, `git checkout -- extern/godot-cpp`, merge, re-link). Pushed. |
| 2. port the 7 differences | DONE in `src/pes/fdc.{hpp,cpp}` and `PesCutsceneCamera` (§1). Constants: 17 floats read from the exe, each through the instruction that loads it, opcode and RIP-relative operand VA checked (`LoadPoseConstants`). |
| 3. pose test | DONE: `PesFdcPoseInstall.EntranceCutPoseMatchesTheWorkedExampleBitForBit`: eye, forward, up, fov (radians) and clip frame at 30 / 91 / 180 ticks equal the worked example's float32 values (`EXPECT_EQ`, bit-equal); near/far also checked against the record. |
| 4. frames | §5 |

## 1. The seven differences, as ported

(each cites the VA in `docs/re/pes-re-camera-pose.md`; the VA↔operand pairs are the table in `kConstSites`, `src/pes/fdc.cpp`)

1. **Rotation**: `SampleCanm` snaps to the first key for `f < 0.001` and to the second for `f > 0.999` (`0x14213A184/1AC`); slerp (`0x140A93210`) with the linear weights `(1-f, ±f)` when `1-d <= 1e-5` (`0x140A932AA`), no renormalise (the matrix's `2/|q|²` does it, `0x140A91D24`). The old `d > 0.9995` nlerp, its normalise and `Normalize` are deleted.
2. **Lerps**: position `f*v1 + (1-f)*v0` (`0x14213A110`), scalar `(1-f)*v0 + f*v1` (`0x14213A0E0`). `Lerp4`/`Locate` deleted; the new `Locate` follows `0x14213A210` + the equal-time walk of `0x142139EB0` and does **not** clamp `f` (before the first key position extrapolates and rotation is the first key).
3. **Key index / clamp**: `frame` clamped to `[0, s16(+4)]` (non-looping, `0x142139B3B`), `iframe = floorf(frame)` (`0x142139B57`); channels without a times table are one key per frame; channel type → interpolator kind (1/3 rotation, 4/5/6/10 position, 8 scalar).
4. **Time**: `ClipFrame`: `t = clock` (record `+0x9C == 0`) or `max(clock - startCutTime, 0)`; `t += animeStartTime`; `frame = t * (fps / 60)` (`0x141F7B4FC`, `0x141F7B51A`, `0x141F7B526`; fps from the stream only when header byte `+0xC` bit 0 is set, else 30.0 from `0x142139E30`); loop wrap when record `+0x9D`. The `ParseCanm` check `u32(+12) == 1` is replaced by the bit test; length is `(float)s16(+4)`.
5. **near/far**: the CANM's own near/far/aspect channels are no longer read (`CameraSample` lost them). `RenderCameraParams`: near = record `+0x90` (`0x141EAF903`), far = `max(4000, record +0x94)` (`0x141EAF914/91C`). `kMinNearM = 0.1` and its PES21_CAMERA_TRACE.md §6.4 citation are deleted (no PES counterpart in this path).
6. **fov**: clamp to `[0, 2.4434609]` (`0x141EAF5D0`, constant `0x142B672C8`); widescreen widening `2·atan(tan(fov/2)·(16/9)/aspect)` for aspect < 16:9 (`0x141F7B250`).
7. **Cut offset, flip, rotY, height**: after sampling, `EvaluateCameraPose` applies `eye.y += height`, X-mirror (m01, m02, m10, m20, eye.x × −1), `Ry·M` and `Ry·eye` (world rotation: `FUN_140A91810` is `a·b`, `FUN_140A91A30` is `a·p`, both decoded this session), then `eye += offset` (`0x141F7B866`, `0x141F7B87D`, `0x141F7B8FC`, `0x141F7B9BF`). Columns of the matrix are (−right, up, forward); `PesCutsceneCamera::update` builds Godot's basis (right, up, −forward) from them.

New decodes this session (CONFIRMED, `dec.sh` output + asm): matrix multiply `0x140A91810` = row-major `out[r][c] = Σ a[r][k]·b[k][c]`; point transform `0x140A91A30` = `a·p` (column-vector); axis-angle builder `0x140A91E60` (Ry = [[c,0,s],[0,1,0],[−s,0,c]], `x*x*omc + c` element order kept); the block matrix has its translation at `m[3], m[7], m[11]` (`0x140A923A0`, `0x14088D2A0`), so it is a column-vector 4×4. Height offset order `((h − 180) · 0.01) · rate` (asm `0x141F39F10..F20`).

## 2. Stand-ins (named `ponytail:` in code)

- **Cut offset (`player+0xA08..+0xA18`) is zero and the height correction target is absent** in `PesCutsceneCamera::update`: no FixDemo player. Site that removes it: the player's offset writer (OPEN 2 of the pose doc) and `0x141F7E3C0` (target height). The entrance record has height rate 0.
- **sin/cos of rotY**: libm; PES takes them from a 512-entry degree table via `0x140A92C80` (not decoded). Only reached when the cut offset's rotY ≠ 0.
- **acosf/sinf**: the double function rounded to float (UCRT's float functions are INF to behave so; glibc `sinf` could differ in the last bit). The worked example is bit-equal with this.
- **Widescreen option byte** `G(0x141F500D0)+0x522E`: a setter `set_widescreen_fix`, default off. The display-side scale in `0x141EAF5D0` (`display+0x7C != 0`) is not ported (INF: 0 for the desktop display).
- **Euler round trip** (`0x14088D2A0` → `0x14088AD50`): not emulated; the pose is the matrix before decomposition (the doc's worked-example convention; rebuilt orientation equals it to rounding, INF).

## 3. Tests (all in `tests/pes_fdc_test.cpp`; `PES21_ROOT` for the install-reading ones, which skip without it)

Binary run: `pes_fdc_test` 35/35 passed (with the install). No other binary run; Gate not run (Main gates).

Red: on `origin/main` the new API (`LoadPoseConstants`, `SampleCanm(k, …)`, `EvaluateCameraPose`, `RenderCameraParams`) does not exist, so the target does not compile; commit `cf482832` is that red state (tests + header, no implementation). Green: `ab7ce3a7`, 34/34, then extended per mutation below.

Mutations (each applied, rebuilt, run, reverted; list in `gf-FdcCamera-scratch/mutations.log`):

| # | difference | mutation | failing test(s) |
|---|---|---|---|
| M1 | 1 | no snap branches | `RotationBelowTheLowSnap…`, `BeforeTheFirstKeyTime…` |
| M2 | 1 | linear threshold ×50 | `KeysJustPastTheLinearThreshold…` |
| M3 | 1 | normalise the slerp output | `KeysWithOneMinusDot…`, `SlerpDoesNotRenormalise…` |
| M4 | 2 | `a + (b−a)·f` | `PositionIsFTimesSecond…` |
| M5 | 3 | no frame clamp | `FramesPastTheLength…` |
| M6 | 3 | no equal-time walk | `EqualKeyTimes…` |
| M7 | 3 | clamp `f` ≥ 0 | `BeforeTheFirstKeyTime…`, `EqualKeyTimes…` |
| M8 | 4 | drop animeStartTime | `ClipFrameIsClock…` |
| M9 | 4 | `t·fps/60` instead of `t·(fps/60)` | `ClipFrameIsClock…` (24 fps, t = 9: the bits differ; at 30 fps they cannot) |
| M10 | 4 | ignore `timeMode` | `ClipFrameIsClock…` |
| M22 | 4 | ignore the fps flag | `ClipFrameIsClock…` |
| M11 | 5 | far without the 4000 floor | `NearIsTheRecordsAndFarIsAtLeastTheFloor`, install pose test |
| M12 | 5 | near clamped to 0.1 | `NearIsTheRecords…` |
| M13 | 6 | no fov clamp | `FovIsClampedToZeroAndTheMaximum` |
| M14 | 6 | widescreen without the aspect gate | `WidescreenFix…` |
| M15 | 7 | flip misses m20 | `FlipMirrors…` (after the test was widened to the whole matrix; the first version did not catch it) |
| M16 | 7 | offset before rotY | `RotYTurnsTheWholeShot…` |
| M17 | 7 | `M·Ry` (camera-local) | `RotYIsAWorldRotation…` (the first test, identity rotation, could not tell: Ry commutes with diag(−1,1,−1)) |
| M18 | 7 | no column negation | `IdentityRotationLooksDown…`, `RotYTurnsTheWholeShot…`, install pose test |
| M19 | 1 | `s = 1/n` | `TheMatrixNormalises…` (after pinning the absolute scale; relative-only could not catch it), install pose test |
| M20 | 7 | height dropped | `HeightOffsetRaisesTheEye` |
| M21 | 7 | cut offset ignores `useCutOffset` | `RotYTurnsTheWholeShot…` |

Not covered by a mutation: the opcode/operand check of `LoadPoseConstants` (no synthetic PE image in the test set; the install test only proves it passes on the real exe), the `timesOff == 0` parse branch beyond the one sampling test, and `loop` wrap beyond one case.

The pinned worked-example rows come from `tools/dev/cutscene/camera_pose_example.py` (committed; PES-derived values from the install's entrance cut, so the test skips when the install's record differs: start 540, relative time, animeStart 0, length 330, 30 fps). That is a test pin from the install, not from a dt18 file.

## 4. Build

`cmake --build build --target godotfootball` on the merged tree took 160 s (incremental, `libpestranslated` already built). `PesLibrary::load` now requires the pose constants (like the keeper/stamina constants): an exe that fails the opcode/operand check refuses to load with `PES21 cutscene camera constants unavailable`.

## 5. Frames

Branch head `ebef91f9` (`git ls-remote origin FdcCamera` = `ebef91f9`).

**Godot side (DONE).** `PES21_ROOT=… tools/capture_viewer.sh --scene res://match/match.tscn --frames 100 -- --home-team 1 --away-team 2 --fdc-file common/demo/fixdemo/ent/cut_data/ent_001_view_st000_cam2.fdc --fdc-camera 0 --fdc-t0 9.0` (new `--fdc-file` entry, `load_file` now bound; `--fdc-t0` starts the player clock at the record's 540 ticks = 9 s). GPU check passed (RX 6900 XT, RADV). Frames at 0.5 / 1.5 / 3.0 s after the record start (image indices 14 / 44 / 89; image n is rendered at t0 + (n+1)/30 s): scratch `sel/godot_n{14,44,89}.png`. Each shows a low camera behind the goal line looking up the pitch (goal post and net left, horizon at the lower third); the pan between frames matches the table's yaw 30.6° → 40.3°. The scenery is GF's match scene, not PES's.

**PES side (DONE, two attempts).** First restart: my fixed-region `x11grab` came out black because the PES window had moved (0:51 of play when I looked): entrance missed, Main restarted again. Second restart (MATCH_STARTED 08:06:05.67): `ffmpeg x11grab -window_id` at 10 fps with per-frame wall times (`pes2/s2/log`, absolute epoch seconds), 140 s kept. Real frames (no decode needed for X captures): scratch `sel/pes_off{0.5,1.5,3.0}_t*.jpg`; pair sheet `sel/pair.jpg`; all 55 contact-sheet frames were looked at.

PES entrance timeline from the burst (seconds after MATCH_STARTED, ±0.25 s, INF for the zero): black until +15.6; **clock zero ≈ +16.0** (fade-in); shot A (low camera at a corner flag, records at 0 ticks of `cam1.fdc`) until ≈ +18.7; shot B (wide pitch view with the stadium-name panel, the `order_st000` records at 180 ticks) until ≈ +24.8; **shot C from ≈ +25.0: the player tunnel with the players lined up**, until ≈ +36.5; then play. Transitions at 180 and 540 ticks fall 6.1 ± 0.35 s apart (360 ticks = 6.0 s at 60 ticks/s); C lasts ≈ 11.5 s (cam0021 is 330 frames at 30 fps = 11 s, 660 ticks to the 1200-tick records). So the record start 540 ticks = zero + 9 s ≈ +25.0 matches (CONFIRMED to the burst's resolution) and the tick clock of §1 is right.

**Verdict: NOT like-for-like. No camera comparison (framing, angle, fov) was made, and none is claimed.** Reasons:
1. PES places the cut in the world with the player's cut offset (`player+0xA08..+0xA18`: position, rotY, flip), OPEN 2 of the pose doc; the PES frames at +25.5 / +26.5 / +28.0 show the **tunnel with players**, the Godot frames show the zero-offset pose over the match pitch. Same clip, different world placement: eye height and tilt are plausibly alike (low camera, looking up a little) but that is an impression, not a measurement.
2. GF has no tunnel, walkout players or FixDemo scene around the camera, so there are no common landmarks to measure angle or fov against.
3. Which of cam0021 (key 20) and cam0022 (key 21), both starting at 540, plays is OPEN 1: the PES frame at +28.0 is a close-up of faces, a different framing from +25.5/+26.5, so a second layer or a cut within the shot may be involved; the burst does not tell.
To close: read `player+0xA08..+0xA18` and the played key during the entrance (a hook on `0x141F39CC0` or on `0x141F7B700` would give both), apply them in `PesCutsceneCamera::update` (the `ponytail:` stand-in), and render the tunnel scene or measure with landmarks.

## 6. Left (numbered)
1. Cut offset (OPEN 2) and the played key (OPEN 1): one hook on `0x141F39CC0` (record + offset) answers both; then the frame comparison.
2. rotY sin/cos table `0x140A92C80` (stand-in: libm).
3. Euler round trip `0x14088D2A0`/`0x14088AD50` not emulated (pose is the matrix before decomposition).
4. Widescreen option byte `G+0x522E` and the display scale in `0x141EAF5D0` (`display+0x7C`).
5. A synthetic-image test of the opcode/operand check in `LoadPoseConstants`.
6. Doc corrections already listed in the pose doc (§5 item 6): `pes-re-open-decodes.md` §1 and `pes-port-FdcCamera3.md` still describe FixDemoHuman.

Gate not run (Main gates). Test binaries run: `pes_fdc_test` 35/35 with `PES21_ROOT`. Not run: the other test binaries (the gdext header change touches only `PesLibrary`; `godotfootball` built, rc 0).


## Main's note (29-10-26)

The sin/cos table `0x140A92C80` is already decoded and translated (`pes::SinTableDeg`, `pes_fn_140A92C80`; docs/ports/pes-port-KickTerms.md, -KickTerms2.md): use it instead of libm for rotY.
