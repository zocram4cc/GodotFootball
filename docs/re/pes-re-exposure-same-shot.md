# Exposure, same shot (Track B): st29 broadcast, baseline `6691a8a`, no fix

## Pair
- PES: `pes_play2_dec.png` (OBS `PES-vk` 1280, live play, scoreboard 198:08, decoded `magick IN -colorspace RGB OUT`; decode verified identical re-run, B5 procedure).
- GF: `gf_base/frame00000149.png` (`tools/capture_viewer.sh --scene res://match/match.tscn -- --pes21-root <workdrive>/PES21 --home-team 708 --away-team 760 --stadium 29`, 150 frames, settled f149; Godot movie frames need no decode, B5).
- Side-by-side: `pair_exposure.png` (PES left, GF right, both 1280x720).
- All in `<workdrive>/gf-ExposureShot-scratch/`.

## Not like-for-like (stated plainly)
- Camera angle differs: PES is a low broadcast wide on the far touchline (crowd + adboards + near-goal box); GF's broadcast camera sits higher/behind (empty red seats, centre circle). Same stadium (29), same broadcast mode, same weather (day/fine), different viewpoint.
- Kits differ: PES white-vs-dark duel; GF white-vs-blue. No white-shirt pair exists.
- PES crowd is packed people; GF stands are empty red seats — no stands comparison possible.

## Luminance table (linearised sRGB, Rec.709 weights; grass = green-dominant pixels in the same relative box)

| surface (same rel. box) | PES disp | PES lin med | GF disp | GF lin med | GF/PES |
|---|---|---|---|---|---|
| pitch L (0.28-0.45, 0.55-0.70) | (0.342,0.434,0.109) | 0.1272 | (0.429,0.530,0.203) | 0.2329 | 1.83 |
| pitch C (0.45-0.62, 0.55-0.70) | (0.362,0.449,0.134) | 0.1404 | (0.458,0.547,0.209) | 0.2655 | 1.89 |
| pitch R (0.62-0.79, 0.55-0.70) | (0.344,0.436,0.113) | 0.1254 | (0.398,0.497,0.174) | 0.1643 | 1.31 |
| pitch far (0.35-0.65, 0.35-0.50) | (0.377,0.474,0.134) | 0.1562 | (0.539,0.626,0.233) | 0.3074 | 1.97 |
| white line cores (control) | (0.889,0.934,0.851) | 0.8466 (n=4732) | (0.801,0.791,0.754) | 0.5822 (n=2953) | 0.69 |

## Reading
- GF grass is 1.3-2.0x PES in matched boxes; GF white lines are 0.69x PES. Opposite directions: **not a global exposure gain**, so no exposure scalar was touched.
- The stale B4 claim (GF pitch 4-5x darker, ratio 0.03 vs 0.25-0.49) is reversed on this baseline: B4Grass fixed the grey pitch (rain/snow layers) and PitchV7 changed the VS; those numbers predate both.
- SceneExposure (`3aab078`, bg_intensity 1.0) is on this baseline; B5's shirt finding (GF 1.0 vs PES 0.468, unlit viewer) does not transfer to the lit match scene — GF whites here render 0.47-0.58 lin, not clipped.
- GF adaptation converges (f0 med 0.97 -> f30 0.35 -> f120+ 0.14-0.15); f149 is settled.
- Single clear VA-cited cause: none. The pitch-side candidate is the unbound `Mask_Tex_SRGB` (t6) zero default (`ao = max(raw0.a ~ 0.003, 0)`), open in B4 §3 with VA `asm/pes3ddf_pitch_2020.ps.asm` lines 104-109 but PES's unbound-slot value still undecoded (null-path `0xb8a0bf169f98` content unrecorded). Fixing the default without that decode would be tuning. Not done.

## No commit
Worktree is clean at `6691a8a` (only the godot-cpp symlink diff). Nothing to push.
