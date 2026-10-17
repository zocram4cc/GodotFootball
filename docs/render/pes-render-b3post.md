# B3Post: tonemap and post chain (Track B item B3)
Branch `B3Post` (worktree `<workdrive>/gf-B3Post`, base `946b760`). Scratch and frames: `<workdrive>/gf-B3Post-scratch/`.

## Built
- `src/pes/tonemap.{hpp,cpp}`: `MakeFilmCurve` (FUN_14175f430), `WhiteBalanceRgb` (FUN_14175f590 with the Judd, Krystek, tint and Bradford helpers), `BloomKernelCount` and `BloomCopyBranch` (FUN_141804af0).
- `src/pes/lighting.{hpp,cpp}`: the rig's tonemap group (`overrideTonemap`, `acesFilm*`, `whiteTemperature`, `whiteTint`, merged by span override) and the per-rig `PesPluginSettings` (`bloomSize`, `bloomBrightnessExtraction`, `bloomWeight`, `tonemapSpeed`).
- `src/gdext/pes_lighting.cpp`: `PostParams` (32 floats) and `configure()` on the camera's `PesPostEffect`. The `CameraAttributesPractical` exposure multiplier is removed, so exposure is not applied twice.
- `godot/post/pes_post_effect.gd` (CompositorEffect, POST_TRANSPARENT) and `godot/post/pes_post.glsl` (kernels REDUCE, LAW, BRIGHT, KAWASE, COMPOSE). The chain: EV adaptation (OBS law), quarter-res bright pass, Kawase bloom with the R/B fringe, WB, UE4 FilmToneMap with the toe/shoulder hard switch, pow(1/2.2). The output is `srgb_to_linear(display)`, so Godot's sRGB encode returns the PES display value.
- `godot/match/match.tscn`: Compositor on Camera3D (last step, after the SceneCrash ack). The Environment block is unchanged: LINEAR tonemap, glow off.
- Tests: `tests/pes_tonemap_test.cpp` (9 tests, no PES data needed), plus install-gated `LightingRig.St002TonemapPluginAndWhiteBalance` and `LightingMerge.SpanOverrideTakesTonemapGroup` in `pes_lighting_test.cpp`.

## Decoded (Ghidra, plus constants read from PES2021.exe .rdata)
- WB builder: the rows stored at +0x10.. are the columns of the column-convention matrix. The shader's `mat3(rows)·hdr` is therefore correct. The transcription reproduces the report's live matrix at 6500 K / tint 1 to about 5e-4. The rig's tint of 1 in memory came from st009, not st002 (0.075).
- Film: `ToeMatch = log10(0.18) − ½·(ToeScale/slope)·ln((x+1)/(1−x))`, `x = (black+0.18)/ToeScale − 1`, for toe ≤ 0.8 (else `log10(0.18) + ((1−toe)−0.18)/slope`). This reproduces the report's global and span sets (−0.23491, −0.39028).
- Bloom: `N = max(1, floorf(10·size))`. The bloom target is quarter-res (`>> 2`). Per pass, `cb12 = (tx, ty, z, w/N)` with `z = w` on pass 0 (as decoded; applied literally).
- Import names: `log10f` (IAT 0x1425301b0), `logf`, `floorf`.

## Verified
- `pes_tonemap_test` 9/9; `pes_lighting_test` 9/9 (install-gated tests run against `<workdrive>/PES21`).
- GPU chain vs a numpy reference of the same formulas (synthetic ramp, identity WB, bloom off): max abs error 0.0026 (one 8-bit step is 0.0039).
- Same with the rig's WB (6500 K / 0.075): max error 0.0026. Ignoring the WB gives 0.11, so the WB path is exercised and oriented correctly.
- Output encode: a written linear 0.5 is captured as 0.733 = sRGB(0.5).
- EV adaptation, `cap2` (30 fps, 150 frames, match scene): f10 −9.49, f20 −10.15, f40 −11.06, f80 −11.49, f150 −11.69. Still drifting, inside [−13, −9], so the clamp does not bind.
- Wired scene `final1` (150 frames, `res://match/match.tscn`): no script or shader errors; same numbers as the unwired run (deterministic).
- Corrections to the report: the install's st002 `af` rig has one `PesPluginSettings`, reading (bloomSize 0.3, threshold 0.3, weight 0.3, tonemapSpeed 10). The report's survey row (0.8, 0.4, 0.3, 30) does not hold for this file; the live st009 values match this file.

## OBS reference frames (captured, read, not used for acceptance)
- `ref/pes_1.png` (58:06), `ref/pes_2.png` (58:53), `ref/pes_4.png` (60:26): broadcast camera, live play, daytime, fine weather, sunlit pitch, red seats. Stadium: 29 (Konami Stadium), resolved (see next bullet). Scoreboard BNT 85–99 W.
- PES stats (linear luma, pitch band): median 0.39, 99th percentile ~0.83–0.89, clipped 0.1–0.2%. Sunlit grass display ~(0.63, 0.69, 0.38).
- Stadium resolved from the install: the effective team table (`pesfmt_dump cat common/etc/pesdb/Team.bin`, 210 records, the 4cc override) gives BNT = team 708 (+0x3C stadium 29) and W = team 760 (stadium 29). The Godot frame on st029 shows the same red seats with white aisles (`st29/frame00000149.png`, `--home-team 708 --away-team 760 --stadium 29`).

## Not visually verified (plainly)
- The Godot frame does not match the OBS shot, so there is no acceptance yet. Three causes, two outside B3:
  1. Observed on st002 (the default stadium, not the running one): the top third of the frames is black (display ~0.005). It is not the sky: the rig view's sky renders bright (clipped white) at the same exposure. On st029 the stands render lit (red seats), so this does not apply to the running match.
  2. Camera framing is B1's and differs from the OBS shot.
  3. Stadium is resolved (29). The pitch is NOT grass in the Godot st029 frame: it reads dark grey with a sheen (B4Grass's turf shader is not on this branch), so the pitch's albedo drives the adaptation average, and the frame cannot be matched on the pitch yet.
- Bloom: the haze around players is the 0.3 threshold catching the pitch, which sits above the adapted average in this pitch-only frame. PES shows no halo on its white kit (`crop_compare2.png`). Not judged until the frame carries the stands and sky.

## Open (named in code as `ponytail:`)
- 33³ grading LUT (ColorGrading): not applied. Input domain and encoding are unresolved. The shipped `ColorCorrection_CubeLut` body is a stub. Upgrade: dump the live D3D shader.
- AA (FXAA/TAA), sharpen and grain: not applied (Part C §5).
- Reduction: arithmetic mean over the full frame (type open). The 32·a² alpha term is omitted (Godot's alpha is not PES's bright-pixel code).
- Bloom: bright pass samples bilinearly at quarter-res texel centres (PES downsample filter not decoded). The 0.1–0.2 copy branch is one zero-offset Kawase pass (decoded in shape only). The EV→offset table g() is zero, as live.
- Adaptation runs per rendered frame with dt = 1/60 (PES's per-frame behaviour), not wall time.

## Needs the owner or Main
- Nothing from the owner: the stadium is 29 (team table, see above). The stadium-29 pitch needs B4Grass's turf shader before the frame can be compared.
- The st002 exterior reading black is B2/B6's lighting and materials; st029 is unaffected by it.
