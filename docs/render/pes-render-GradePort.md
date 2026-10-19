# Match grading = PES's ColorCorrection_CubeLut stub + SdrDisplayMapping (GradePort)

> **Main 19-10-26:** frames viewed (before/after frame 149). Grass hue now matches PES; luminance still ~3.5x PES (upstream: sun remainder + turf). Lines read yellow after the port (halfway_line B/G 0.096 vs PES 0.369): a material defect, queued with the turf residual. Branch not merged yet (lands with a gate).

Branch `GradePort` on origin: **`0c2a8bb`** (`ls-remote` confirmed). Two commits on base `origin/main` `626f6f0`:
`cddc58b` (`--camera-pose` hook, cherry-pick of `e1bef27`) and `0c2a8bb` (the port). Decode source: `docs/render/pes-render-grading-lut.md` (read whole; §1.3 stub, §3 composition, §3.7 display).

Corrections to the brief: the `--camera-pose` hook is not in `b62e87c` (that commit is the WB revert). The hook is `e1bef27` on TurfColour (same message as `db1f2a6`), cherry-picked as `cddc58b`. `origin/main` is `626f6f0`, not `c8c0aca`.

## 1. What the port computes (CONFIRMED from the pack asm; constants checked)

Per pixel, `godot/post/pes_post.glsl` COMPOSE:

1. `scene = colour · 2^EV` (GF's exposure, unchanged).
2. `ColorCorrection_CubeLut` stub, only its encode and gain run (the 33³ LUT is bound, never sampled): `v = 0.25 · enc(scene)`, with `enc(s) = s ≥ 0.0031308 ? 1.055·max(s,1e-5)^(1/2.4) − 0.055 : 12.92·s`.
3. `SdrDisplayMapping`: `o = v ≤ 0.00982 ? 0.309597522·v : ((4v + 0.055)·0.947867334)^2.4`.
4. `o` clamped to [0,1]; alpha passed through; Godot's final pass sRGB-encodes.

Net: `o = clamp(2^EV · scene, 0, 1)`. Identity on [0,1], hard clip above 1. Highlights are the exposure plus the clip.

### Asm sites (vkd3d 1.17 `d3d-asm` of the pack records; line = `cat -n` of the dump)
- `ColorCorrection_CubeLut.ps` (GrSystemShaders_dx11.fsop, 2,028 B): L13-14 UV and the only sample (t0, s8); L15 `max 9.99999975e-06`; L17 `div 2.40000010`; L19 `mul 1.05499995`; L20 `add -5.49999997e-02`; L21 `mul 1.29200001e+01`; L22 `ge 3.13080009e-03`; L23 `movc`; L24 `mul 2.50000000e-01`; L25 `eq cb5[10].x, 0`; L26 `movc o0`.
- `SdrDisplayMapping.ps` (1,512 B): L9-10 UV and `sample t0`; L11 `mad 4.0, 5.49999997e-02`; L12 `mul 9.47867334e-01`; L13 `max 9.99999975e-06`; L15 `mul 2.40000010`; L17 `ge 9.82000027e-03, v`; L19-22 mask and alpha; L23 `mad 3.09597522e-01`.

### Constants (`<workdrive>/gf-GradePort-scratch/check_consts.py`, float32 bits vs the printed immediates)

```
SRGB_BREAK               asm L22  printed 0.00313080009  glsl 0.0031308  f32 bits 3b4d2e1c vs 3b4d2e1c  OK
SRGB_LINEAR_SLOPE        asm L21  printed 12.9200001  glsl 12.92  f32 bits 414eb852 vs 414eb852  OK
SRGB_SCALE               asm L19  printed 1.05499995  glsl 1.055  f32 bits 3f870a3d vs 3f870a3d  OK
SRGB_OFFSET (stub)       asm L20  printed -0.0549999997  glsl -0.055  f32 bits bd6147ae vs bd6147ae  OK
SRGB_OFFSET (disp)       asm L11  printed 0.0549999997  glsl 0.055  f32 bits 3d6147ae vs 3d6147ae  OK
SRGB_POW_EXPONENT        asm L17  printed 2.4000001  glsl 2.4  f32 bits 4019999a vs 4019999a  OK
SRGB_POW_EXPONENT(d)     asm L15  printed 2.4000001  glsl 2.4  f32 bits 4019999a vs 4019999a  OK
SRGB_POW_FLOOR (stub)    asm L15  printed 9.99999975e-06  glsl 1e-05  f32 bits 3727c5ac vs 3727c5ac  OK
SRGB_POW_FLOOR (disp)    asm L13  printed 9.99999975e-06  glsl 1e-05  f32 bits 3727c5ac vs 3727c5ac  OK
CUBE_LUT_GAIN            asm L24  printed 0.25  glsl 0.25  f32 bits 3e800000 vs 3e800000  OK
DISPLAY_DECODE_GAIN      asm L11  printed 4  glsl 4  f32 bits 40800000 vs 40800000  OK
DISPLAY_INV_SCALE        asm L12  printed 0.947867334  glsl 0.947867334  f32 bits 3f72a76f vs 3f72a76f  OK
DISPLAY_DARK_BREAK       asm L17  printed 0.00982000027  glsl 0.00982  f32 bits 3c20e411 vs 3c20e411  OK
DISPLAY_DARK_SLOPE       asm L23  printed 0.309597522  glsl 0.3095975232  f32 bits 3e9e8391 vs 3e9e8391  OK
doc claim 0.30997522 == 4/12.92 ? False  (asm prints 0.309597522 )
MISMATCHES: 0
```

**Doc error for the archive:** `pes-render-grading-lut.md` §3.7 and §5 give the dark slope as `0.30997522 (= 4/12.92)`. The pack immediate is `3.09597522e-01`, which is 4/12.92. The doc's digits are transposed, and the wrong value is 0.12 % off the identity.

## 2. What was cut (clean cutover, same commit)

- `pes_post.glsl`: film curve (FilmToneMap), white balance, AP1 matrices, RRT/post desaturation, 1/2.2 display gamma, bloom add. Passes BRIGHT and KAWASE removed. The bloom has no consumer: PES's composition never samples its bloom texture (`t7`, grading-lut §3.2).
- `pes_post_effect.gd`: passes `reduce, law, compose`; params 32 → 6 floats (`[0..3]` exposure, `[4]` tonemapSpeed, `[5]` stepGain); push block `vec2 slots, vec2 ext`; bloom targets and sampler removed.
- `pes_lighting.cpp` `PostParams`: exposure and tonemapSpeed only. `pes/tonemap.hpp` include removed.
- `src/pes/tonemap.{cpp,hpp}` and `tests/pes_tonemap_test.cpp` deleted (film curve, Judd/Krystek/Bradford WB, bloom kernel helpers); dropped from the `pesfmt` sources and the tests CMake.
- `src/pes/lighting.{hpp,cpp}`: the tonemap group (`overrideTonemap`, `acesFilm*`, `whiteTemperature/Tint`) and the bloom fields of `PluginSettings` are not parsed or merged. `tonemapSpeed` stays.
- `tests/pes_lighting_test.cpp`: `St002TonemapPluginAndWhiteBalance` → `St002PluginSettings` (tonemapSpeed only); `LightingMerge.SpanOverrideTakesTonemapGroup` deleted.
- Not touched: exposure values and the EV law, sun, turf, `match.tscn`/`rig_view.tscn` (`tonemap_mode = 0`, Godot's linear tonemapper is the identity), `set_glow_enabled(false)`, the match camera.

## 3. Checks

**GPU shader test** `godot/tests/test_post_compose.gd` (new; CTest `godot_post_compose_test`). It runs the real `pes_post.glsl` COMPOSE pass on a 256-pixel ramp (per channel, −0.25 to 1.75, alpha 0.75) at EV 0 and EV −1, through a local RenderingDevice under gamescope. Expected: `clamp(2^EV·in, 0, 1)` per channel, alpha kept. Tolerance 5e-4 on the half-precision input and output.

- **Red on origin/main**: the main COMPOSE (film + WB), run through the same harness with a throwaway variant that binds main's layout, fails:
  ```
  ev=0.0: worst colour error 0.258299, bad values 607
  ev=-1.0: worst colour error 0.166016, bad values 575
  post compose: 2 failure(s)
  ```
- **Green on the port**:
  ```
  ev=0.0: worst colour error 0.000488, bad values 0
  ev=-1.0: worst colour error 0.000488, bad values 0
  post compose: 0 failure(s)
  ```
  0.000488 is one half-float ulp below 1.
- **Mutations** (each applied to the shader, run, reverted; the shader is green again after the last revert, and `grep MUTANT` is empty):

| id | mutation | result |
|---|---|---|
| M1 | CUBE_LUT_GAIN 0.25 → 0.5 | FAIL, 2 failures (worst 0.7849) |
| M2 | no [0,1] clip | FAIL, 2 (worst 0.7490 at EV 0; 298 bad) |
| M3 | exposure ignored in COMPOSE | FAIL at EV −1 only, 1 (worst 0.4990) |
| M4 | alpha forced to 1.0 | FAIL, 2 (256 bad per case) |
| M5 | display dark/bright select inverted | FAIL, 2 (worst 0.9225) |
| M6 | encode linear/power select inverted | FAIL, 2 (worst 0.9206) |
| M7 | DISPLAY_DECODE_GAIN 4 → 2 | FAIL, 2 (worst 0.7854) |
| M8 | SRGB_BREAK 0.0031308 → 0.04045 | FAIL, 2 (20 and 39 bad values) |

- **CTest**: `1/1 Test #47: godot_post_compose_test ..........   Passed    1.25 sec`. Verdict is the script's `post compose: 0 failure` line through PASS/FAIL regexes. gamescope exits 0 whatever the child does, so `$?` is useless there. Without gamescope the entry fails (it does not skip).
- **C++ unit test** `build/tests/pes_lighting_test` with `PES21_ROOT=<workdrive>/PES21`: `[  PASSED  ] 8 tests.` (none skipped).
- **Build**: first gdext build in the fresh worktree compiled godot-cpp. Targeted `godotfootball pes_lighting_test` (1248 steps, about 17 min under fleet load), exit 0. Later swaps rebuilt only the changed objects. Full gate and smoke not run (Main gates).

## 4. Visual

Setup, same for both frames: `tools/capture_viewer.sh --scene res://match/match.tscn --frames 150 -- --stadium 21 --camera-pose 7.94,19.135,37.07,0.022,-0.376,-0.926,23.28 --home-team 708 --away-team 760`, frame 149, RX 6900 XT under gamescope. The `before` tree is `cddc58b` (hook only); the `after` tree is `0c2a8bb`. PES reference: `~/ghidra-projects/scratch/obs/p12_dec.png` (the aligned frame ExposureMatch matched to this pose: stadium 21, day, BNT 95-99 W HUD, wide pitch and giraffe). Boxes and ratios: `measure.py` (TurfColour scratch), GF/PES of Rec.709 linear p50.

**Frame-mean (rows 15-92 %) luminance**: PES 0.1507; GF before 0.4229 (2.81×); GF after 0.4086 (2.71×).

| region | PES p50 | before GF/PES | after GF/PES | PES R/G, B/G | before R/G, B/G | after R/G, B/G |
|---|---|---|---|---|---|---|
| grass_a | 0.1322 | 3.67 | 3.54 | 0.625, 0.082 | 1.285, 0.202 | 0.622, 0.011 |
| grass_b | 0.1062 | 3.69 | 3.51 | 0.585, 0.052 | 1.296, 0.170 | 0.591, 0.007 |
| grass_c | 0.1036 | 3.90 | 3.69 | 0.600, 0.064 | 1.297, 0.177 | 0.602, 0.009 |
| grass_d | 0.1051 | 3.80 | 3.60 | 0.590, 0.057 | 1.293, 0.173 | 0.593, 0.007 |
| grass_top_right | 0.1742 | 2.89 | 2.83 | 0.615, 0.085 | 1.277, 0.211 | 0.626, 0.010 |
| giraffe_body | 0.3834 | 1.40 | 1.38 | 0.853, 0.366 | 1.401, 0.308 | 0.788, 0.055 |
| circle_arc_right | 0.1311 | 3.96 | 3.62 | 0.788, 0.314 | 1.242, 0.341 | 0.734, 0.068 |
| far_touchline | 0.0801 | 2.76 | 2.71 | 0.491, 0.148 | 1.415, 0.247 | 0.719, 0.037 |
| halfway_line | 0.1253 | 3.92 | 3.38 | 0.777, 0.369 | 1.195, 0.381 | 0.743, 0.096 |

Per-channel gains on grass_a: before R 6.34, G 3.09, B 7.58; after R 3.50, G 3.52, B 0.47.

**Reading the frames** (both opened and looked at): before, the pitch has a yellow-olive cast and the white lines are warm. After, the grass is lime and the lines are neutral, with no film shoulder. The grass hue now matches PES (R/G 0.62 vs 0.59–0.63). The halfway line's R/G went 1.20 → 0.74 (PES 0.78), and the giraffe's went 1.40 → 0.79 (PES 0.85). The stands band across the top shows horizontal striping in both frames; it is not in the grading path and I did not investigate it.

**Not like-for-like**: three live PES captures this session (`obs_frame.ts`, PES-vk) show a title card, a cut-away close-up and a cut-scene, not the wide shot. The numbers use the earlier p12 capture instead. So there is no live same-shot comparison.

## 5. Open items

1. **Luminance ~3.5× PES on turf, unchanged by this port** (frame ratio 2.71). This is upstream (sun, turf albedo, exposure), which the brief put out of scope.
2. **Blue is now low**: grass B/G is 0.01 against PES 0.05–0.08, and the giraffe's is 0.06 against 0.37. The WB had been boosting blue (after its removal the blue gain is 0.47 against R and G at 3.5). The residual is in the upstream lighting or albedo, not the grading.
3. **Constants are literals** with asm sites, the same form as `tonemap.cpp` on main. AGENTS.md says PES-derived values are read from the install at runtime. I did not build a runtime read of the fsop record. Say if you want one.
4. **`vp+0x650` bypass** of x0.25 (`cb5[10].x`, upload site `0x1418044bf-0x141804515`) is not modelled, marked `ponytail`. The live match reads 0, so the gain always applies.
5. **Comp RT format** (grading-lut open item 3) does not change the displayed pixel: values above 1 clip at [0,1] either way.
6. **TurfColour's WB work** (`b62e87c`, transpose revert) is moot: the port has no WB.
7. **No live like-for-like frame** of the same shot (see §4). The reference is p12.
8. **Doc digits**: `pes-render-grading-lut.md` §3.7 and §5 should read `3.09597522e-01` (4/12.92). Main to fix when archiving.
9. Untracked `godot/**/*.uid` files from Godot's import runs are in the worktree and deliberately not committed.
