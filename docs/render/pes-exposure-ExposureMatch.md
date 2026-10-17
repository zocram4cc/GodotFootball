# Exposure, matched camera (Track B): GF pitch matches PES in mean luminance; the gap is spatial lighting and grass colour, not global exposure

Branch `ExposureMatch`, worktree `<workdrive>/gf-ExposureMatch`, base `origin/main` 9892f50. No code change. Nothing committed or pushed (no tracked-file diff; `--camera-pose` already exists on main as `db1f2a6`, which this report uses).

## Result in one paragraph
At the closest camera pose I could transfer, GF's pitch has about the same mean linear luminance as PES's (0.76 to 0.94x across two empty grass boxes). The earlier 1.3 to 2.0x pitch gap (`pes-re-exposure-same-shot.md`) does not reproduce here, so it came with the camera angle, not with exposure. What remains is (a) a much wider spread of pitch luminance in GF (bright patches and dark bands that PES does not show at this framing), (b) GF grass hue that is more yellow and less saturated than PES, and (c) a white-line comparison I cannot separate from local ground lighting. The like-for-like frame pair is not achieved: the two frames have different pitch origins and different stadiums (see Pair). Verdict: **not a global exposure issue**. Per-material and lighting-distribution causes are open. No fix made (no single VA-cited cause established).

## Pair (not like-for-like in geometry; stated plainly)
- PES: live OBS frame `p12` (`bun ~/ghidra-projects/scratch/obs/obs_frame.ts`, PES-vk 1280, decoded `magick IN -colorspace RGB`). Scoreboard BNT 95 vs W 99, match clock 943:29 (read on screen). Stadium with a full crowd, adboards, far-end penalty boxes.
- GF: `gf_final/frame00000149.png` (`tools/capture_viewer.sh --scene res://match/match.tscn --frames 150`, settled f149; movie PNG, display-correct, 1152x648, no decode). Args: `--home-team 708 --away-team 760 --stadium 29 --home-kit 1 --away-kit 0 --camera-pose "13.512,19.135,36.909,0.022,-0.376,-0.926,23.28"`.
- Camera pose: from the read-only sampler `cam_F.jsonl` (object `0x1c14a10`, `live.fvec` at +0x60/+0x50, `f` at +0x100) at t=…961.495; the OBS frame is at …961.362 (dt +0.13 s; PES camera moves ~4 m/s, so ~0.5 m). `fov` 23.28 is the sampled value; whether it is vertical or horizontal in PES, and the metres unit, are INF.
- Differences that stop this being like-for-like:
  1. **Pitch origin differs.** PES's centre circle sits near x ≈ 300 of 1152 px; GF's sits at the left edge (x ≈ 0 to 100). Far-end penalty boxes are visible in PES, absent in GF. Same pose, different world-to-pitch mapping by several metres (not resolved).
  2. **Stadium not verified.** GF uses stadium 29 (carried over from the previous attempt, not read from PES). PES shows a crowd and adboards; GF shows empty red seats. No stands comparison possible.
  3. **Kits by eye, not by id.** PES: Aussie Girl light-blue shirt, blue shorts; Eva Thread navy shirt, white shorts. GF home 708 kit 1 and away 760 kit 0 were chosen as the closest visual match. Shirt comparison is not valid.
  4. **Weather and time not read from PES** (day/fine assumed in GF via defaults).
  5. **Sky not in either frame.** Not measured.

## Luminance table (linear light, sRGB to linear, Rec.709 Y, box in fractions of frame; n = pixels; per box, raw values)
| Box | Side | n | p10 | p50 | p90 | p99 | mean RGB |
|---|---|---|---|---|---|---|---|
| grass A (880-1000, 400-450 PES; 880-1000, 338-438 GF) | PES | 7504 | 0.107 | 0.1274 | 0.147 | 0.164 | 0.091 0.149 0.011 |
| grass A | GF | 12000 | 0.085 | 0.1195 | 0.169 | 0.305 | 0.100 0.146 0.016 |
| grass B (1000-1100, 500-540 PES; 700-800, 480-540 GF) | PES | 4995 | 0.107 | 0.1254 | 0.144 | 0.162 | - |
| grass B | GF | 6000 | 0.062 | 0.0958 | 0.151 | 0.284 | - |
| white line (688-706, 318-345 PES; 116-140, 300-330 GF) | PES | 600 | 0.125 | 0.2385 | 0.435 | 0.461 | 0.231 0.301 0.081 |
| white line | GF | 720 | 0.243 | 0.3174 | 0.713 | 0.764 | 0.353 0.426 0.166 |
| shirt, light blue (568-598, 292-338 PES; 30-52, 355-382 GF) | PES | 1683 | - | 0.160 | - | - | 0.193 0.201 0.169 |
| shirt | GF | 572 | - | 0.288 | - | - | 0.331 0.319 0.296 |
| sky | both | - | not visible | | | | |

Grass ratios at matched pose: GF/PES p50 0.94 (A) and 0.76 (B). Hue: R/G PES 0.61, GF 0.69; B/G PES 0.075, GF 0.107 (GF grass is more yellow and less saturated). Spread: PES p10 to p99 is 0.107 to 0.164; GF is 0.062 to 0.305.

Boxes were placed on clean grass (no players or markers) and checked on a composite (`pair_composite.png`, `boxes.png`). The GF line box initially missed the line; it was moved to x 116-140 and still sits on sunlit grass (see below).

## Interpretation
- **Global exposure: not the gap.** Mean pitch luminance matches to within 0.76 to 0.94x at this pose. A global EV or bloom-level change would move both grass boxes together; the medians do not move together in a way that suggests a single scale (GF box B is 0.76x, box A is 0.94x).
- **Spatial lighting distribution (INF, visible in frame, not fully measured):** GF's pitch has a bright sunlit band on the left (box at the line: local ground p10 = 0.243 vs PES 0.125) and a dark stand-shadow band across the top (p10 of GF grass 0.06 to 0.085 vs PES 0.107). PES does not show this distribution at this framing. The line brightness difference is largely this local ground difference: line-peak to local-ground ratio PES 0.461/0.125 = 3.7, GF 0.764/0.243 = 3.1. So the white-line gap is **not** established as per-material.
- **Grass hue (per-material, CONFIRMED in the means, cause INF):** GF grass is more yellow and less saturated than PES (B/G 0.107 vs 0.075; R/G 0.69 vs 0.61). The GF frame also has yellow speckles on grass that PES's frame does not show clearly; these fall in the GF p99 tail (0.28 to 0.31) and are a turf material or texture candidate (B4 / turf3d, the pitch shader's Base_Tex_2 albedo; also the open unbound-slot default for `Mask_Tex_SRGB` in `pes-re-exposure-same-shot.md` §3, not decoded).
- **Tonemapping: not separable here.** PES's pre-tonemap scene value is not observed, so the film curve (`PesPostEffect`, B3) cannot be checked against PES's own compression from these frames alone.

## What I did not do
- No fix. No single cause with a VA citation was established, and the brief forbids tuning a stand-in.
- No tests run: no code changed. (`pes_uniform_test` 36/36 passed earlier in this session, unrelated.)
- No commit, no push.

## Open items (for Main)
1. Read PES's live stadium id, weather and time (not done; `light_st029` heap scan was killed before output). Without it, the GF stadium 29 pairing is unverified.
2. Find the pitch-origin mapping between PES's and GF's world (the centre circle is offset by several metres at the same pose). Until it is fixed, no like-for-like pair is possible.
3. Separate the grass hue cause: turf speckle vs `Mask_Tex_SRGB` default vs B2 light colour. Proposed test: render GF with the speckle texture disabled and compare B/G on the same box.
4. Separate the line-brightness cause: a GF frame with the `fox3DDC_GGX` line decal's specular term off (`SPECULAR_LIGHT` 0 on lines only) would show whether the line peak is specular. Not run.
5. Kit ids for the live match (PES `Aussie Girl` and `Eva Thread` team/kit) are not read from memory; the shirt row is not a valid comparison until they are.

## Files
All in `<workdrive>/gf-ExposureMatch-scratch/`: `gf_final/frame00000149.png` (GF), `pair_composite.png`, `boxes.png`, `luma_table.json`, `cam_F.jsonl` (camera samples). PES frame: `~/ghidra-projects/scratch/obs/p12_dec.png`.
