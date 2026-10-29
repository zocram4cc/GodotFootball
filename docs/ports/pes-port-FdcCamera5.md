# PES cutscene cut offset decoded and ported, entrance camera frame pair (FdcCamera5, 29-10-26): cut offset is zero at every entrance activation; rotY runs through the translated sine; two camera-matched PES/Godot pairs

Branch `FdcCamera` (worktree `<workdrive>/gf-FdcCamera`), head `b0e4ce38`. Labels: CONFIRMED = capture read, decompile/asm or test, with VA; INF = inference, reason given; OPEN = blocker named. Scratch (PES-derived, never committed): `<workdrive>/gf-FdcCamera-scratch/co/` (`rd2.py`, `rd3.py` capture readers, `pairs/` the frame pairs, `mutations5.log`).

## 0. Outcome

| ticket item | result |
|---|---|
| 1. merge `origin/main`, push | DONE (`f17a5638`, merge committed despite the submodule-symlink warning; pushed). |
| 2. read offset + played key from captures 008/010/011, decode `0x141F39CC0` | DONE, CONFIRMED (§1). The cut offset is **zero** (pos, rotY, flip) at all six activations of the entrance run (captures 007, 008, 010-013); the played keys are **view key 0 (`cam0000`) at 0 ticks and order key 11 (`cam0001`) at 180**; `cam0021/cam0022` (540) **never activated**. FdcCamera4's "shot C = the 540 record" was wrong: shot C is the `ent_007_passage01` cut. |
| 3. port offset + rotY via the translated sine; delete libm and zero-offset stand-ins; tests, mutations | DONE (§2, §3): `RotYMatrix` (translated `0x140A92C80`), libm `AxisYMatrix` deleted, `PesCutsceneCamera::set_cut_offset`; bit-equal against the translated builder `0x140A91E60`; two live-capture pose oracles; six mutations run. |
| 4. frames | DONE (§4): two camera-matched pairs (shot A corner flag, shot B behind-goal wide). Pitch landmarks agree to 1-2 % of frame height (A) and 0.3-0.6 % (B); scenery is not like-for-like. |

## 1. Decode of `0x141F39CC0(player, node)` (CONFIRMED: asm of `0x141F39CC0..0x141F39F46` + captures)

`rcx` = FixDemo player, `rdx` = map node of the record (record = node+8). Order of what it does (asm VAs):

1. `0x141F39CFA movss xmm0,[rdx+0x94]` = record `+0x8C` animeStartTime -> `rsp+0x48`.
2. Reads the player's cut offset into locals: `0x141F39D08 movsd xmm0,[rcx+0xA08]` and `0x141F39D16 mov eax,[rcx+0xA10]` (position x,y,z), `0x141F39D20 movss xmm0,[rcx+0xA14]` (**rotY, degrees**), `0x141F39D2E movzx eax,byte [rcx+0xA18]` (**flip**).
3. `0x141F39D69 call 0x141F31F00(player, record+0xA8 u16[4], 4, &animeStart, &pos, &rotY, &flip)`: the function-number terms. For a term of kind 1 it **overwrites** pos/rotY/flip from a map keyed by the number (player `+0xAE8`; the stage default from player `+0xAC0..+0xAD0` when the entry's flag `+0x49` is 0, else the entry's `+0x4C..+0x5C`; rotY wrapped into [0,360) with 360.0 at `0x14259858C`); kind 2 adds to animeStart, kind 3 scales by a player value. All six captured records have function numbers (0,0,0,0), so nothing is overwritten (script read of record `+0xA8..+0xAF`).
4. `0x141F39D79 cam+0x130 = record`; the anime path (record `+0xC`) picks the resource (`node+0x128 -> +0x88` or load via `0x1414A8B70`) and `SetCanm` (slot `+0x40`, `0x141F39DEB`).
5. `0x141F39DEE..0x141F39E45`: near (record `+0x90` -> cam `+0x200`), far (`+0x94` -> `+0x204`), loop (`+0x9D` -> `+0x208`), animeStart -> `+0x21C`, `+0x220 = 1`.
6. **`0x141F39E56 cmp byte [rbx+0xAE],sil`** (record `+0xA6`, the use-cut-offset byte): 0 -> pos `(0,0,0)`, rotY 0, flip 0 (`0x141F39E5F..E73`); else the locals of step 2/3 (`0x141F39E75..E81`). Then **`0x141F39E89 call [rax+0x60]`** = `FixDemoCamera::SetCutOffset` `0x141F7B700`: copies pos to cam `+0x154/+0x158/+0x15C`, rotY to cam `+0x160` **wrapped once into [0,360)** (`0x141F7B72F addss xmm2,[0x14259858C]` = 360.0 for rotY < 0; `0x141F7B747 movss xmm0,[0x14259858C]`, `subss` for rotY >= 360), flip to cam `+0x164`.
7. `0x141F39E8C..0x141F39F46`: the height correction: record `+0xA0` rate (`[rbx+0xA8]`) and target byte record `+0xB1` (`[rbx+0xB9]`); `0x141F39F10 subss [0x142598580]` = 180.0, `0x141F39F18 mulss [0x14256AE18]` = 0.01, times the rate, stored to cam `+0x20C` (as in the pose doc §2.1).

Writers of `player+0xA08..+0xA18` (decompiles of the fragments around `0x141F2E11C`, `0x141F327D9`, `0x141F3CD2B`, `0x141F3DB0C`): `0x141F2E11C..` (cut start) sets pos from its caller argument, rotY from player `+0xACC`, flip from `+0xAD0`, and overrides all three from a map node (`+0x4C..+0x54` pos, `+0x58` rotY wrapped by 360.0, `+0x5C` flip) when that node's `+0x49` flag is set; `0x141F327D9` zeroes them (`+0xA08`, `+0xA10` as qwords, `+0xA18` byte); `0x141F3CD2B` and `0x141F3DB0C` copy them into a human object (`+0x17C/+0x180`, `+0x1C8/+0x1CC`). The node's origin (which stage/map entry) is **OPEN**; it needs a cut that sets a non-zero offset to observe (INF: the Cut record's `offset/offsetPos/offsetRotY/offsetFlipX` fields at record `+0x19/+0x1C/+0x28/+0x2C` are a different structure from the node's `+0x49..+0x5C`; `ent_001_view_st000_cam2.fdc`'s Cut record has offset 0, pos (0,0,0), rotY 0, flip 0).

### Captures (pid 1694737, hook `cut_offset_1F39CC0`, `hooks.cfg` line `FIRST 1 EVERY 1`: calls 1..6 are consecutive, so the list is complete; read by `rd2.py` through `SnapMem`)

| seq (call) | record path (record+0xC) | startCut | +0xA6 | player | clock (+0x80) | offset pos / rotY / flip |
|---|---|---|---|---|---|---|
| 007 (1) | `ent/canm/ent_001_view_st000_cam0000.canm` | 0 | 1 | `0x1FFD6460` | 0.0 | (0,0,0) / 0 / 0 |
| 008 (2) | `ent/canm/ent_001_order_st000_cam0001.canm` | 180 | 1 | `0x1FFD6460` | 180.00034 | (0,0,0) / 0 / 0 |
| 010 (3) | `ent/canm/ent_007_passage01_cmn_cam0300.canm` | 0 | 1 | `0x363FCAA0` | 0.0 | (0,0,0) / 0 / 0 |
| 011 (4) | `ent/canm/ent_007_passage01_cmn_cam0310.canm` | 150 | 1 | `0x363FCAA0` | 150.00024 | (0,0,0) / 0 / 0 |
| 012 (5) | `ent/canm/ent_007_passage01_cmn_cam0100.canm` | 300 | 1 | `0x363FCAA0` | 300.0007 | (0,0,0) / 0 / 0 |
| 013 (6) | `ent/canm/ent_009_st020_cmn_cam0004.canm` | 0 (animeStart 120) | 1 | `0x3F881FB0` | 0.0 | (0,0,0) / 0 / 0 |

(011's camera object also holds `cam+0x154..0x164` = 0 and `cam+0x20C` = 0.0375 = (target − 180)·0.01·0.75, the height branch live.) Record `+0xA0` height rate: 0 for the two ent_001 records, 0.75 for the passage cam0300/cam0310 (target byte 0 and 11).

**Which key plays (answers OPEN 1 of the pose doc for this run):** `ent_001_view_st000_cam1.fdc` keys 0..4 all start at 0 and keys 10..13 at 180 (`fdc_parse.py` of the extracted file); the live records are **key 0 (record index 0)** and **key 11 (record index 6, `order_st000_cam0001`)**. `ent_001_view_st000_cam2.fdc`'s records at 540 (keys 20/21, `cam0021/cam0022`) did **not** activate: calls 1-6 are all the activations of the run and none is a 540 record. Capture 009 (hook `0x141F41CE0`) shows why the entrance moved on: the ent_001 player's clock read **534.0** with `player+0x6C = 0` and its camera pointer (`+0xC68`) 0, i.e. the cut ended 6 ticks before the 540 records (the end rule is **OPEN**, caller `0x141F41CE0`). So the tunnel frames FdcCamera4 matched to the 540 record are the `ent_007_passage01` cut (frames 741+ below), a different player and file.

Also read: sim step `player+0x6C` = 1.1111145 ticks per frame (`+0x68` = 0.0185186 s, `+0xAB0` = 54.0): the cut runs at 54 Hz with 60 ticks/s (CONFIRMED live read, capture 008/011).

## 2. What changed (all on `FdcCamera`)

- `src/pes/fdc.hpp/.cpp`: `PoseConstants` gained `radToDeg` (`0x140A91E7A mulss [0x14259BE40]` = 57.2957763671875), `quarterTurnDeg` (`0x140A91E99 addss [0x14259BE5C]` = 90.0) and the 513-float quarter-wave `sinTable` (`0x142655950`, checked through `0x140A92D04 lea rdx,[rip+disp]` = `48 8D 15`, operand VA verified before reading). `RotYMatrix(k, rotYDeg)`: `deg = (rotY·degToRad)·radToDeg`, `cos = SinTableDeg(deg + 90)`, `sin = SinTableDeg(deg)` through the **translated `pes_fn_140A92C80`** (the builder `0x140A91E60` calls it for the sine at xmm0 = deg+90 and xmm0 = deg, amp 1.0), element order of the builder's stores kept. The libm `AxisYMatrix` and its `ponytail:` are deleted.
- The SetCutOffset wrap of rotY into [0,360) is **not** ported: `0x140A92C80` wraps its own input once (`<0 -> +360`, `>=360 -> -360`), so the matrix cannot tell wrapped from unwrapped (mutation D below survived, so the dead code and its constant `fullTurnDeg` were deleted).
- `CMakeLists.txt`: `pesfmt` links `pestranslated` PUBLIC (fdc.cpp now needs `pes_fn_140A92C80`).
- `PesCutsceneCamera::set_cut_offset(pos, rot_y_deg, flip)` (bound to GDScript); `update` passes it to `EvaluateCameraPose` instead of a hard-coded `CutOffset{}`. **`ponytail:` left in place**: nothing in GF writes a non-zero offset (no FixDemo player); the PES site that removes it is `0x141F2E11C`. The default (zero) is the captured value for every entrance activation, so the entrance plays exactly as PES does. The height correction target (`0x141F7E3C0`) is still absent (rate 0 in the entrance's two ent_001 records; the passage records have 0.75).

## 3. Tests

Binaries run: `pes_fdc_test` **41/41** (`PES21_ROOT` set); `godot_cutscene_camera_test` (new `godot/tests/test_cutscene_camera.gd`, run directly: rc 0; with the pinned eye deliberately broken it fails with rc 1 and `FAIL: eye is the live camera's`). Full `cmake --build build -j3` (230 steps, every test binary and `godotfootball`, `gfsim_run`) finished rc 0 with `pesfmt` now linking `pestranslated`: no link error in any other target. The other test binaries were built, not run. Gate not run (Main gates).

Red: commit `5a57eb41` (tests only, `RotYMatrix`/`sinTable` absent so `pes_fdc_test` does not compile). Green: `2db0b4cc`.

New tests (`tests/pes_fdc_test.cpp`):
- `PesFdcCutOffset.RotYMatrixIsTheTranslatedBuildersBitForBit`: ten angles (-200.5..359.9, off-table values) through the **translated `pes_fn_140A91E60`** run on the install's table (args: rcx out, xmm1..3 axis, angle at `[entry rsp + 0x28]`, rows 0x10 apart) vs `RotYMatrix`, `EXPECT_EQ` on all nine elements.
- `PesFdcCutOffset.RotYTurnsTheEyeByTheAngleAboutWorldY` (relation vs `std::cos/sin`, 2e-5) and `...RotYOutsideZeroToThreeSixtyTurnsTheShotLikeItsWrappedAngle` (-90 = 270, 450 = 90, 360 = 0).
- `PesFdcLiveEntrance.ViewCam0PoseAtTheCaptureClockHasTheLiveCamerasPositionAndFov`: `cam0000` of `ent_001_view_st000_cam1.fdc` at the capture clock `0x43340016`: **eye and fov bit-equal** to the live camera's pose block (`cam+0x168`).
- `PesFdcLiveEntrance.ViewCam0RenderParametersAtThePreviousClockMatchTheLiveBlock`: at `c - dt` (`0x4332E3A4`) vs the render block `0x1437F4A40`: **eye, fov, near bit-equal**; far: block far = record far (400) and `RenderCameraParams` floors it at 4000 (`0x141EAF91C`); up within 2e-5 (**measured worst 1.5e-8**), `eye + dist·forward` vs live target within 2e-4 (**measured worst 3.8e-6**).
- `PesFdcLiveEntrance.OrderCam1AtTheCutsLastClockMatchesTheLiveBlock`: `order_st000_cam0001` (record index 6) at clock 534 (frame 177 of 180) vs capture 009's block: eye and fov bit-equal, up/target as above.

The expected values are float32 bit patterns read by script from `/proc` snapshots of the live game (capture-derived pins, as in the worked-example test; the tests skip without the install). The up/target tolerance is the INF Euler round trip (`0x14088D2A0`), not emulated.

Mutations (each applied, rebuilt, run, reverted; `mutations5.log`):

| # | mutation | result |
|---|---|---|
| A | `RotYMatrix` sine/cosine from libm | `RotYMatrixIsTheTranslatedBuildersBitForBit` fails |
| B | no deg->rad->deg round trip (`deg = rotYDeg`) | `RotYMatrixIsTheTranslatedBuildersBitForBit` fails |
| C | cosine = `SinTableDeg(deg - 90)` | `...BitForBit` and `RotYTurnsTheEyeByTheAngleAboutWorldY` fail |
| D | drop the negative-angle wrap | **survived (40/40)**: the wrap is unobservable; wrap and `fullTurnDeg` deleted |
| E | position lerp `a + (b-a)·f` | `PesFdcSample.PositionIsFTimesSecondPlusOneMinusFTimesFirst` fails (the live oracles pass: f = 0 at those keys; they are not the guard for the lerp) |
| F | sample at `floor(frame)` | `EntranceCutPoseMatchesTheWorkedExampleBitForBit` and the `ViewCam0RenderParameters...` live test fail |

Not covered by a mutation: the `lea`/operand check of the sine table load (needs a synthetic image), `set_cut_offset`'s flip/position in Godot beyond the one `.gd` run.

## 4. Frame pair

### 4.1 Aligning the PES burst to the cut clock
Burst `entrance_pid1694737/`: 897 frames, PNG mtimes are the capture times (median step 0.100 s). Unix time of a monotonic `t` in `pauses.jsonl` is `t + 1793048540.159` (constant: `taken_unix - t_release_file`, all 13 captures). Window frames are 1284x724: 2 px frame on each side, content 1280x720 (crop 2,2,1282,722; X captures need no sRGB decode).

Pauses are the anchor, not a clock estimate: while the hook parks the game thread and the watcher reads, the screen **freezes**, so the frozen frames carry the pose of the capture instant. Frame hash runs (md5): **f564-f593 (30 identical frames)** = pause 8 (`cut_offset` call 2, clock 180) and **f668-f740 (73 identical)** = pauses 9 + 10 back to back. Pauses in the burst (`pauses.jsonl` seq 7-11: 2.88, 3.03, 3.21, 2.54, 5.13 s; capture 006 `cut_create` 2.79 s, 005 2.69 s earlier) shift every later frame by their length; no wall-clock mapping across a pause is used.
Timeline from the frames (looked at every 5th frame, `sheets/s0..s3.jpg`): black until f533 (fade); **shot A** (corner flag, `view_st000_cam0000`) f534 on, live to f563, frozen f564-f593, flag moving again from f594 to f603; **shot B** (behind-goal wide, `order_st000_cam0001`; first the stadium-name/team panel) f604-f661, frozen f668-f740; crossfade into the **tunnel (passage01)** at f741, close-ups of faces from f746, second passage shot from f838.
A->B cut at clock 180 (f604): the live-then-frozen A span is 3.0 s of ticks at 54 Hz but 7.0 s of wall; consistent with the call times (capture 007 to 008: 6.82 s of wall, parked 3.43 s, 3.39 s running for 3.0 s of ticks: the run is ~13 % slower than 54 Hz; PES under capture load). The screen lags the hook by ~0.7-1.1 s at a cut change (the freeze starts at f564 while the call is at about f556; the new shot appears about 1 s after the release): INF, not decoded.

### 4.2 Pair A: the frozen frame of shot A against the same pose in Godot
- PES: `f_0580.png` (inside the frozen run). Its pose is the render block of capture 008 = `cam0000` at **178.88922 ticks** (CONFIRMED by the bit-equal live test above): eye (53.3259, 0.3697, 35.6354), vertical fov 53.13° (0.92731 rad), near 0.5.
- Godot: `capture_viewer.sh --scene res://match/match.tscn --frames 60 -- --home-team 1 --away-team 2 --fdc-file common/demo/fixdemo/ent/cut_data/ent_001_view_st000_cam1.fdc --fdc-camera 0 --fdc-t0 1.98148`, image 29 (rendered at t0 + 30/30 = 2.98148 s = 178.8889 ticks), cut offset zero (the captured value). GPU check passed (`Using Device #0: AMD Radeon RX 6900 XT (RADV)`). Image is 1152x648; PES content resized 1280x720 -> 1152x648 (both 16:9, both `KEEP_HEIGHT`/vertical fov).
- Files: `pairs/A_pes_f0580.png`, `pairs/A_godot_g1_n29.png`, `pairs/A_blend.png`, `pairs/A_side.png`. I looked at all of them.
- What matches (blend): the corner flag pole, the corner arc, both touchlines leaving the frame, and the far goal's post at the right edge are in the same places; pole axis x 0.625 of the width in both, corner arc and the right touchline end (x ≈ 1105, y ≈ 500).
- Numbers (landmark picks on 1152x648; the corner vertex and slopes are hand-intersected line fits, noisy): left touchline y at x = 350/450/550: PES 549/565/563, Godot 556/574/571 (Δ 7-9 px); right line y at x = 850/900/950/1000/1090: PES 574/561/545/531/504, Godot 581/567/550/534/505 (Δ 7 → 1 px); corner vertex PES (728, 610), Godot (719, 622); pole base y PES ≈ 617, Godot ≈ 623. **Residual 1-2 % of the frame height, largest at the corner**, with no systematic horizontal shift. The horizon is not comparable (backdrops differ: PES hills/trees and a far goal on the left; GF's scene is open with a stand wall).
- Not attributable: the residual could be the camera, or GF's pitch lines/flag placement (line width, corner arc radius, flag position) not being PES's st000. At a camera 0.37 m above the grass, 0.1 m of line offset is several pixels. The camera values themselves are bit-verified against the live block (§3), so I attribute it to the scene, but that is INF.

### 4.3 Pair B: the last frame of shot B
- PES: `f_0700.png` (inside the frozen run f668-f740). Its pose: capture 009's block 0x1437F4A40 = `order_st000_cam0001` at clock 534 (frame 177 of 180): eye (-72, 12, 0), fov 57.2° (0.99871 rad), near 0.5 (CONFIRMED by the `OrderCam1AtTheCutsLastClockMatchesTheLiveBlock` test).
- Godot: `--fdc-camera 6 --fdc-t0 7.9`, image 29 (t = 8.9 s = 534 ticks), zero offset. Files `pairs/B_pes_f0700.png`, `pairs/B_godot_g2_n29.png`, `pairs/B_blend.png`, `pairs/B_side.png`; all looked at.
- The pitch markings (goal frame, six-yard box, penalty area, centre circle, halfway line receding, both penalty-area sides) coincide in the blend. Numbers (1152x648, brightness peaks, `PES / Godot`): goal line y 586-589 / 585; penalty-area front line y 437 / 435; six-yard front line y 520 / 516; centre circle top y 420 / 418; penalty-area side lines at y = 560: x 335 / 337, y = 470 right side x 956 / 954; goal posts x 467 and 684.5 / 470 and 681 (goal width 217 vs 211 px). **Agreement 2-4 px (0.3-0.6 % of the height).** One picked left-side value (y = 470, x 193 / 169) is an outlier from other bright objects and is not used.
- Not like-for-like: the stadium (GF draws a different stand model and a floating white panel from its own scene, the PES frame shows hills and trees), goal nets (PES has them, GF none), players, and the team/stadium panel overlay on PES's earlier B frames (the chosen f0700 has none).

### 4.4 Verdict
Camera framing, angle and fov are like-for-like for the pitch markings in both pairs (same landmarks at the same normalised positions, 1-2 % in A, 0.3-0.6 % in B), because the poses come from the same record, the same clock and the same (zero) offset, and the pose maths is bit-equal to the live camera. The **scenery is not like-for-like** (PES stadium, goal nets, flag cloth, players vs GF's match scene), so nothing beyond camera geometry is claimed. Not visually verified: the passage/tunnel shots (frames 741+): GF has no tunnel to compare, and their records have height-rate 0.75 whose target (`0x141F7E3C0`) is not ported; the rotY ≠ 0 / flip paths (no capture has a non-zero offset): verified only against the translated builder, not against a live frame.

## 5. Corrections to earlier docs
- `pes-port-FdcCamera4.md` §5: shot C at +25 s is **not** `ent_001_view_st000_cam2.fdc`'s 540 record (never activated); it is `ent_007_passage01_cmn_cam0300/cam0310/cam0100` (calls 3-5). Its "OPEN 1/OPEN 2" are answered above for the entrance (offset zero, keys 0 and 11).
- `pes-re-camera-pose.md` §2.1: slot `+0x60` (`0x141F7B700`) wraps rotY into [0,360) with 360.0 `[0x14259858C]` (two sites listed above).

## 6. Left (numbered)
1. **A non-zero cut offset has not been observed.** The rotY/flip/pos port is bit-equal to the translated builder but has no live-frame oracle. Needs a cut whose function numbers or map node set an offset (the hook is installed; a goal celebration/PK cut would be the candidates, INF). Hook ask for Main: none needed.
2. Writers `0x141F2E11C` (map node `+0x49..+0x5C`) and `0x141F31F00` (function numbers) are decoded in structure only; the node's source is OPEN.
3. Why the ent_001 cut ended at 534 ticks, 6 before the 540 records (caller `0x141F41CE0`, capture 009). OPEN.
4. Passage records (captures 010-012): the height branch (target `0x141F7E3C0`, rate 0.75) and the external CANMs; the live blocks of capture 011/012 are ready oracles for it.
5. Euler round trip `0x14088D2A0` not emulated: up differs from live by 1.5e-8, target by 3.8e-6.
6. `src/sim/onthepitch/peskickerror.cpp` has its own `SinTableDeg` (KickQ's file, left alone): fold it into one shared helper after `KickSoftMin` lands; `RotYMatrix` is now a second user of `pes_fn_140A92C80`.
7. Screen lag after a cut change (~0.7-1.1 s) is undecoded; the frozen frames avoid it.
8. (closed) Full build of all targets after the `pesfmt` -> `pestranslated` link: rc 0. Other test binaries were not run.


## Main's visual check (29-10-26)

Main looked at `co/pairs/A_side.png` and `B_side.png`. Pair B: penalty box, goal and touchlines at the same places in both frames. Pair A: the corner flag at the same horizontal position (x about 489 in each panel) with the same corner-arc and touchline angles. Scenery differs (PES's modded stadium vs GF's). The camera match is confirmed by eye for these two poses; non-zero cut offsets and the passage shots remain unchecked.
