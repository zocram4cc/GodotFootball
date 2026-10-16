# pes-render-B4Grass: track B4 (pitch grass, lines, mowing)

Status: the stadium-29 grey pitch is fixed and pushed. The pitch colour is still about 4–5× darker than live PES, so the acceptance is not met.

## 1. Commits and tests

- `origin/B4Grass` = `6bd1dc8`, on `origin/main` 36108d0 (force-pushed over `ba272a4`; verified with `git ls-remote`).
  - `7a7da9f` pes_marking: keep print-name codes raw on a `_name_ex` page
  - `2239a0d` pitch: turf from the install's asm; lines as the `fox3DDC_GGX` decal, scratch as the `fox3DDC_Blin` decal; routing in `pes_model.cpp`
  - `6bd1dc8` stadium: place rain and snow layers only in their weather
- Tests on `6bd1dc8`: build exit 0; `pes_marking_test` 15/15; `pes_stadium_test` 6/6 (includes `WeatherOnlyLayersAreKeptOnlyInTheirWeather`, red first with 4 failures); `test_model.gd` exit 0, 0 FAIL, no SHADER ERROR; `test_stadium.tscn` exit 0.
- Coordination: B6Stadium has the new `LoadStadiumScene(vfs, id, weather, err)` signature and SHA `6bd1dc8`. B5Materials was told about the `pes_model.cpp` change. SceneCrash confirmed `match.gd` was free.

## 2. Stadium 29 grey pitch (fixed)

- Cause: stadium 29's base scene pack places two weather-only meshes in every match.
  - `st029_center1_rain_detail.fmdl`: one `rain` mesh, technique `pes3DDC_Puddle_GGX`, which has no shader mapping, so it falls back to opaque Blin. It sits at y = +0.031 over the pitch and covers 124×87 m.
  - `center1_snow.fmdl`: one snow mesh, technique `fox3DDC_GGX_AlphaMask`.
- Evidence (observed): skipping only the `_rain`/`_snow` members turned the pitch from dark grey with sheen to green with bands, speckle and the centre circle (`diag_skip29`). The fine-weather frame is `fix_st29_fine/frame00000149.png`, captured before the rebase onto 36108d0, not re-captured after.
- Ruled out on stadium 29 (observed): a flat turf normal (no visible change); 14:00 instead of the default time (no change); sun shadows off (no change). The sun is on at 15:00 (horizon fade 1.0, 100 klx).
- Fix: `LoadStadiumScene` takes the weather. Members whose file name has `_rain` or `_snow` are kept only in that weather. Rain and snow sharing one weather flag is [INFERENCE] from the names and RE §5.7; no live rain frame was checked.

## 3. Pitch colour (open; not accepted)

Linear pitch / linear white line, same frame:

| Source | Pitch, sRGB median | Ratio |
|---|---|---|
| Ours, fixed st029 fine frame (box 820–1100×470–620) | (30, 37, 10) | ≈ 0.03 |
| PES live OBS, `live_obs/live_1.png` (7:22) and `live_2.png` (7:56) | (119–158, 143–174, 64–95) | 0.25–0.49 |
| PES broadcast wide, B3Post `ref/pes_1.png` (58:06) | (151, 170, 83) | 0.285–0.304 |
| PES refs `pes_ref_a.png`, `pes_ref_b.png` | — | 0.27–0.31 |

Ours is about 4–5× darker in sRGB and about 10× lower relative to the lines.

### Verified in the pitch PS (`asm/pes3ddf_pitch_2020.ps.asm`, file lines)

- `t6` (`Mask_Tex_SRGB`) is sampled once, at line 104. Line 105, `max r2.xy, r2.wyww, r8.xyxx`, gives `ao = max(t0.a, mask.r)` and `gW = max(t0.g, mask.g)`. Lines 108–109 derive m2 from `mask.b`.
- `MatParamIndex` (`cb4[0].x`) is never read. The PS reads only `cb4[0].y` (line 77) and `cb4[0].zw` (line 89). The preset values (200/205, `material_params.fmtt`, RE §5.1) therefore cannot reach `ao`.
- Slope table: both samples (lines 73 and 75) use the same coordinates, `(0.5+0.5·dVN, 0.5+0.5·dVT)`, whenever `CameraVectorBlend` (`cb4[4].w`) is 0. Lines 67–70 give `r5 = (dVN, dVT)`. The R and B lanes come from line 73, the G lane from line 75, and `spend = 1 − G·match` is at line 96. Our port reads the same lanes, so RE open item 3 (R or A) is resolved and the slope table is not the cause.

### Bindings and params

- Identical in st002 and st029: `Base_Tex_SRGB` = `pitch_alp` (t0); `NormalMap_Tex_NRM` = `turf_nrm` (t1, tiled); `SpecularMap_Tex_LIN` = `turf_srm_alp` (st029) or `turf_srm_dummy` (st002), unsampled by the PS; `Base_Tex_2_SRGB` = common `turf_bsm_alp_rgba32` (t3); `GrainNormal_Tex_LIN` = `pitch_detail` (t4); `SlopeTable_Tex_LIN` = common `grass_slopetable_alp` (t5); `Grain_Tex_LIN` = `pitch_grain_alp` (t7). `Mask_Tex_SRGB` is not bound.
- Stadium-wide scan (32 pitch packs):
  - 29 packs bind no mask, including st002 and st029.
  - st011, st031 and st081 bind a 64×64 texture that is constant black (R=G=B=0, A=1) in every sampled texel.
- Two parameter sets across the 32 stadia:
  - 28 stadia: `MatParamIndex` 205, `URepeat` 32, `VRepeat` 24.
  - st029, st052, st064, st084: `MatParamIndex` 200, `URepeat` 24, `VRepeat` 16.
  - The other pitch parameters are identical in both sets.
- `pitch_alp` alpha: mean 0.003 (0.8% of texels above 0.05) in 29 stadia, including st002, st029, st011, st031 and st081. st034 is 0.009, st045 is 0.000, and st069 is A=1.0 everywhere (2048×1024).

### Open

- What PES supplies for `t6` when the pitch pack does not bind it (29 of 32 stadia, including the stadium in the match). PES's ALBEDO implies `ao ≈ 0.3–0.5` [INFERENCE from the ratio, with the other terms as ported]. With `t6` sampled as zero, `ao = alpha ≈ 0`, which is the dark result we get. So PES's unbound `t6` is probably non-zero, or PES's `ao` comes from another source [INFERENCE].
- The engine's null-path texture `0xb8a0bf169f98` (pes-re-shaders §5, pes-kit-assembly §8.7–8.8) has no VFS file, and its content is not recorded in the RE.
- Not done: decompile the function that fills a material's unbound texture slots, or find the creation code of the null-path default, then read its value.
- Not verified: the GB2 output lines (175, 178) write `r2.w` and `r0.y`/`r0.x` to GB2.y and GB2.w. `r2.w` is reassigned at lines 106–109, so the RE's reading of GB2.y (`m4·slope.R·match·S0`) needs a check against those lines. I did not change the port for it.
- The zero default for `Mask_Tex_SRGB` in our port matches the three packs that bind black. No value was tuned.

## 4. Same-shot status

- Broadcast reference: B3Post `ref/pes_1.png` (58:06) is a broadcast wide shot. Our fix frame is an elevated wide view, but the camera positions differ, so the frames are not pixel-matched.
- Live OBS, stadium 29, 0–0, daytime: `live_1.png` and `live_2.png` are live wide shots. `live_3.png` is a live close-up behind the net. `live_6.png` is a replay, kept for reference only. The celebrations and replays are in `live_obs/discard/`.

## 5. Corrections for the archive

1. The line decal on st002 is `fox3DDC_GGX` (lua `fox3ddf_ggx_decal`, `MetalnessMap` `line_mtl`). RE §7.5 says Blin. st029 uses `fox3DDC_Blin` for the line, and the scratch decal is `fox3DDC_Blin` in both stadia.
2. `URepeat`/`VRepeat` on st002 is 32/24. The 24/16 values in the RE are st029's, and also those of st052, st064 and st084.
3. `MatParamIndex` is 205 in 28 stadia and 200 in st029, st052, st064 and st084.
4. Translucency `T` is non-zero in the asm (line 165). The RE says 0.
5. The pitch PS samples `t1` (`turf_nrm`, tiled) at line 82. RE §5.4 and §7.6 say `t1` is unbound.
6. RE §8 item 1 (`t6`): partly resolved. 3 of 32 pitch packs bind a constant black mask, and 29 bind none.
7. RE §8 item 3 (slope channel): resolved. The lanes match the port (R and B from one sample, G from the other, same coordinates when `CameraVectorBlend` is 0).
8. The rain member is `st029_center1_rain_detail.fmdl` (RE: `center1_rain.fmdl`). The snow member `center1_snow.fmdl` is a mesh (RE: snow textures only).
9. `MatParamIndex` (cb4[0].x) is read by no pitch instruction, which confirms RE §3.8 and §5.1.
10. The turf3d mesh is not built in the match scene, so the RE's mow claims are not reproduced. Mow bands are not in `pitch_alp` R.
