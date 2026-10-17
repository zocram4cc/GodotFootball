# Exposure, matched camera (Track B), round 2: the pitch-origin gap is resolved; the GF grass is about 3.7× PES's in linear light with a per-channel shift; no single cause found, no fix

Branch `ExposureMatch`, worktree `<workdrive>/gf-ExposureMatch`, base `origin/main` 9892f50. No code change kept: all shader and post edits were temporary experiments and are restored (verified `git show HEAD:<file> | cmp`). `--camera-pose` is already on main (`db1f2a6`). Nothing committed or pushed.

Supersedes round 1 (`pes-exposure-ExposureMatch.md`, earlier version). Round 1's "grass 0.76–0.94×, lines 1.33×" came from a misaligned pair (wrong pitch origin, stadium 29) and does not stand.

## Result
1. **Live match state (read-only, match running):** stadium **21**, weather **fine**, time **day**, teams **708 BNT (home) vs 760 W**. Kits not read from memory (open; see §1).
2. **Pitch-origin mapping:** the sampled PES camera sees the pitch centre at (5.57, 0, −0.16) in its own frame. GF at camera pose (7.94, 19.135, 37.07) reproduces the PES frame's pitch layout (centre circle, giraffe logo, far-touchline boxes, halfway line) to about 10 px.
3. **Aligned like-for-like pair:** GF stadium 21 vs PES frame p12, same boxes. GF grass is **3.7×** PES in linear luminance and its channel ratios are off: grass R/G 1.29 vs 0.59, B/G 0.17 vs 0.05. The giraffe paint is only 1.4× (R/G 1.40 vs 0.85).
4. **Hue and brightness causes tested, none is the gap:** turf speckle off (≤5%), line-decal specular off (0%), bloom off (≤7%), unbound mask default (cannot darken; see §3). EV freeze shows GF's exposure converged by f100; frozen EV clips, so it is not a usable test.
5. **Verdict:** not a single-cause fix. The remaining candidates are per-material turf albedo in the pitch shader and a post-stage per-channel difference (the 33³ grading LUT is documented as not applied). No fix made; no VA-cited single cause established.

## 1. Live PES state (read-only, pid 2510857; `G = *(0x1436F3DC8)`, `MI = *(G+0x270)`)
- **Stadium 21, CONFIRMED by four independent strings in live memory:** `pitch_st021` (heap, 0x181b470); `/Assets/pes16/model/bg/st021/scenes/{left1,back1,right1}.fmdl` (63 hits, scene region 0x13a90984); `light_st021_af.fpk` (VFS, 0x1b02dc69, and a 0x1437d6943 hit); `st021_df.fpk` (heap 0x19bfa237). No `pitch_st029` hit in live memory. Stadium-29 strings are only in the VFS listing.
- **Home stadium is not 21:** `Team.bin` record 7 (team 708, `/bant/`) and record 59 (team 760, `/w/`) both have home stadium 29 at +0x3C. The match stadium is therefore overridden (competition or random stadium; INF).
- **Weather fine:** light rig `light_st021_af` (rain selects `_ar`, per `pes-re-lighting.md` §2.2; the `af` string is loaded). CONFIRMED string, INF mapping.
- **Time day:** overlay `st021_df` (`d` = day, `f` = fine; `pes-re-effects-particles.md` §stadium overlays). INF (high). Hour not read.
- **Teams:** 708 BNT home, 760 W away (scoreboard names; Team.bin names). CONFIRMED.
- **Kits: NOT determined.** All kit configs (`708_DEF_{1st,2nd,3rd,…}_realUni.bin`, etc.) are loaded in the catalog, so the strings do not identify the selection. Visual only (INF): BNT player in light-blue shirt and blue shorts; W player in navy shirt and white shorts. GF was rendered with home 708 kit 1 and away 760 kit 0, chosen by eye. Live kit index still open (match setup struct layout not decoded).
- Hour not read; the GF lighting used defaults for st021.

## 2. Pitch-origin mapping (resolved)
- Method: the GF projection model (vertical FOV 23.28°, Godot-style basis, origin at the pitch centre) was validated on GF: the predicted centre circle (radius 9.15 m, centre (0,0)) lies on GF's white circle line.
- Applied to PES p12 (1280×720): the centre circle top (330, 378), right extreme (645, 520) and the halfway-line/far-touchline intersection (415, 145) are fitted by least squares to a centre offset of **(5.57, 0, −0.16)** in the sampled camera frame. Residuals 1–9 px; with FOV free the fit gives 23.8°, consistent with 23.28°. Eyeballed points, so ±0.3 m.
- GF camera = PES camera − offset = **(7.94, 19.135, 37.07)**, same forward (0.022, −0.376, −0.926), FOV 23.28. Validated: the stadium-21 render matches PES's giraffe, far-touchline rectangles and halfway line (see `gf_st21_aligned_circle.png`, `boxes_aligned_pair.png`).
- The PES camera sample is `cam_F.jsonl`, object `0x1c14a10`, at t=…961.495; PES frame at …961.362 (dt +0.13 s).
- Not explained: why PES's sampled camera position sits 5.6 m off the pitch centre in its own world. The sim and camera conventions are not verified.

## 3. Aligned luminance (linear light, Rec.709 Y; identical boxes in both frames; 1152×648 coordinates; GF f149 settled)
| box | PES p50 | GF p50 | GF/PES | PES R/G | GF R/G | PES B/G | GF B/G |
|---|---|---|---|---|---|---|---|
| grass_a (470–540, 400–440) | 0.1322 | 0.4852 | 3.67 | 0.625 | 1.285 | 0.082 | 0.202 |
| grass_b (340–420, 470–520) | 0.1062 | 0.3918 | 3.69 | 0.585 | 1.298 | 0.052 | 0.171 |
| grass_c (700–740, 560–640) | 0.1036 | 0.4035 | 3.90 | 0.600 | 1.297 | 0.064 | 0.177 |
| grass_d (860–960, 520–560) | 0.1051 | 0.3995 | 3.80 | 0.590 | 1.293 | 0.057 | 0.173 |
| grass_top_right (900–1000, 190–230) | 0.1742 | 0.5042 | 2.89 | 0.615 | 1.277 | 0.085 | 0.211 |
| giraffe_body (272–318, 452–474) | 0.3834 | 0.5350 | 1.40 | 0.85 | 1.40 | 0.37 | 0.31 |
| circle_arc_right (570–600, 440–500) | 0.1311 | 0.5190 | 3.96 | — | — | — | — |
| far_touchline (800–1000, 124–138) | 0.0801 | 0.2209 | 2.76 | 0.491 | 1.415 | 0.148 | 0.247 |
| halfway_line p99 (255–275, 400–440) | 0.9889 | 0.9614 | 0.97 | | | | |

- Whole frame (rows 15–92%): mean PES 0.151 vs GF 0.422 (2.80×); median 3.47×; geometric mean 2.43×.
- Grass RGB (linear, grass_b): PES (0.074, 0.126, 0.007); GF (0.475, 0.366, 0.063). Per channel R ×6.4, G ×2.9, B ×9.
- Giraffe RGB: PES (0.354, 0.415, 0.152); GF (0.727, 0.519, 0.160). R ×2.1, G ×1.25, B ×1.05.
- Lines clip near 0.96–0.99 in both frames; their peak colour is warm in both (light colour is shared).

Tests on the aligned pose (150 frames each; ratio = experiment / base GF p50):

| test | change | grass_b R/G, B/G | verdict |
|---|---|---|---|
| E1 turf speckle off (`textureLod(Base_Tex_2_SRGB, tiled, 12)`, mip average) | ×1.01–1.05 | 1.30, 0.17 (unchanged) | **rejected** |
| E5 bloom off (bloom weight 0, `pes_post_effect.gd` `_run`) | ×0.93–0.95 | 1.35, 0.17 | **rejected** |
| E3 unbound `Mask_Tex_SRGB` = 0.5 (GF default 0) | ×1.9–2.5 (clips at 0.96) | 1.06, 0.82 | **not a darkening cause** (see below) |
| E4 line decal `SPECULAR_LIGHT = 0` (`pes_ggx_decal.gdshader`) | halfway p50/p99 unchanged; arc p50 −0.3%, p99 unchanged; touchline p99 −0.05% | — | **rejected**: line decal specular contributes ~nothing here |
| E2 EV frozen at initial (law pass skipped) | grass clips at 0.99 (2–2.5× base) | — | initial EV is far above the settled value; not a usable test |

- **Mask (E3):** `ao = max(raw0.a, mask.r)` (asm 105) floors at `raw0.a`, and `m2 = 1 − 0.1·mask.b` (asm 108–109) is at most 10%. GF's default 0 is already the floor, so the unbound value cannot darken the grass by 3.7×. Its sensitivity is large in the brightening direction, so PES's real unbound value still matters; it is undecoded (B4 §3).
- **Adaptation:** GF's grass settles by f100 (0.4019) and f149 (0.3918), so f149 is the settled state. The 3.7× is in the converged exposure, not a convergence artefact.
- **Stadium-21 params match GF's defaults:** the pitch fmdl (`Asset/model/bg/st021/pitch/#Win/pitch_st021.fpk`, material `exp1_pitch_mat`) has `WithersParameter0..5 = 0.78, 0.78, 0.961, 0.098, 0.071, 0.039`, `RepairColor (0.3127, 0.402, 0)`, `AOParam* 1/0.1/1.5/0.85`, `SrmParameter*`, identical to the shader defaults. The values are not the cause.

## 4. Interpretation
- **Global component (INF, medium):** the whole frame is 2.8× brighter in GF. But the giraffe paint (same pitch mesh, same light; INF) is only 1.40×. So the light is not 3.7× stronger; the gain is concentrated in the dark turf.
- **Per-material turf component (CONFIRMED in the means; cause open):** GF's dark turf carries a large R and B excess relative to G (R ×6.4, B ×9 vs G ×2.9). PES's dark turf blue is 0.007 (linear), below the blue offset floor `WithersParameter5 = 0.039` (asm 101–102 curve: `base.b·0.961 + 0.039`, `mad_sat r8.yz … cb4[5].z, cb4[6].y`). Either PES's blue is floored differently, or the path after the curve (tint/`gW`/`ao`, asm 104–112) or the post carries a per-channel darkening GF lacks.
- **Post (INF):** the 33³ grading LUT is documented as "not applied, body is a stub" (`pes-render-b3post.md` Open). A per-channel grade would produce this kind of R/B shift on dark colours, and it is the leading candidate for the post stage. Not testable without the LUT.
- **Lines:** no line-specular issue (E4). Peak colour is warm in both frames.

## 5. Fix status
No fix. None of the single causes tested accounts for the gap, and no VA-cited single cause is established for the turf-channel excess or the grading LUT. Nothing committed.

## 6. Open items (for Main)
1. **Kits.** Live kit index for 708 and 760 not read. Needs the match-setup team block (`G+0x2ad0+0xc400+s·0x2804`, offsets not yet decoded for kit) or a kit-selection write trace. Shirt comparisons are invalid until then.
2. **Hour.** Not read; the GF stadium-21 lighting used defaults. The sun angle would change grass shading and needs checking.
3. **Turf blue floor.** Why PES's dark-turf blue (0.007) is below the `WithersParameter5` floor (0.039) that GF applies identically (asm 101–102, 104–112). Decode whether PES applies the floor to blue, or whether `ao`/`gW`/`m2` change the blue path.
4. **Grading LUT.** The 33³ grade (`ColorCorrection_CubeLut` stub). Decode the LUT input domain (`pes-render-b3post.md` Open) before any post fix.
5. **Unbound `Mask_Tex_SRGB`.** Still undecoded (B4 §3); sensitivity in §3.
6. **PES camera offset.** The PES sampled camera sits 5.6 m from the pitch centre in its own frame. Find the convention (a fixed camera offset, or a stored target) so the GF pose is derived, not fitted.
7. **GF stadium-21 stands.** The top of the frame shows horizontal banding and a dark band where PES shows a crowd (`gf_st21_aligned/frame00000149.png`). A separate defect, and it sits in the adaptation metric.

## 7. Files (`<workdrive>/gf-ExposureMatch-scratch/`)
- Aligned GF (stadium 21, settled): `gf_st21_aligned/frame00000149.png`; circle check: `gf_st21_aligned_circle.png`; boxes: `boxes_aligned_pair.png`.
- Experiments: `gf_E1_speckleoff/`, `gf_E2_evfrozen/`, `gf_E3_mask05/`, `gf_E4_linespecoff/`, `gf_E5_bloomoff/` (frame00000149.png each); measurements `aligned_luma*.json`.
- Projection and fit scripts: `proj.py`, `circfit.py`, `hough.py` (the circle-fit scores were not reliable; the final offset is from the 3-point least squares). Live scans: `LiveSample/stadium_scan.py` (read-only), outputs `stadium_scan.txt`, `st_targeted.txt`, `live_suffix_scan.txt`.
- PES frame: `~/ghidra-projects/scratch/obs/p12_dec.png` (decoded). Live frame now: `~/ghidra-projects/scratch/obs/now_dec.png`.
