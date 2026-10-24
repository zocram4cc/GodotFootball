# PES dark-turf residual: the four named candidates, each to a verdict

Status: read-only work; repo at `<workdrive>/GodotFootball`, install VFS at
`<workdrive>/PES21`, scratch at `<workdrive>/gf-DarkTurfRE-scratch/` (all
outputs with absolute paths there; nothing written to the repo).
Read with `docs/render/pes-render-LumaResidual.md` §5,
`docs/render/pes-render-PitchMaterial.md` §3.4b,
`docs/render/pes-render-sun-radiance.md` §2 H4/H5,
`docs/render/pes-render-TurfColour.md` §3,
`docs/render/pes-render-ComposePath.md`.

Ticket: GF's grass stays ~1.2-1.5x brighter than PES while the giraffe paint
matches (0.6-1.0x), with exposure, hour, sun, shadow and metering eliminated
(LumaResidual §5). The shader math matches, so the cause is an input scale
that hits dark turf, not every material.

Bottom line: candidates 1-3 are REJECTED (two CONFIRMED-no-effect, one bounded
insufficient / out-of-range). Candidate 4 splits: the surface albedo chain is
EXONERATED lane-by-lane (every texel input identical, math identical), and the
last standing mechanism is the turf3d grass-fin geometry PES draws over the
surface and GF never builds (INF, giraffe-immune by construction, magnitude
plausible). Port spec (§6) names the single GF change that can close it and
the two live reads that would confirm it.

## 0. Reference numbers (all re-derived this session, stock VFS winner)

Boxes are the ExposureMatch 1152x648 set, measured on `p12_dec.png`
(OBS double-sRGB decoded, 1280x720) this session:

| box | PES Y p50 (linear) | PES R/G, B/G |
|---|---|---|
| grass_a (470-540, 400-440) | 0.1321 | 0.638, 0.085 |
| grass_b (340-420, 470-520) | 0.1063 | 0.595, 0.052 |
| grass_c (700-740, 560-640) | 0.1039 | 0.579, 0.060 |
| grass_d (860-960, 520-560) | 0.1055 | 0.579, 0.056 |
| grass_top_right (900-1000, 190-230) | 0.1743 | 0.608, 0.086 |
| giraffe_body (272-318, 452-474) | 0.3834 | 0.853, 0.366 |
| far_touchline (800-1000, 124-138) | 0.0801 | 0.463, 0.100 |

(These reproduce TurfColour §3 to <1%: grass_b 0.1063 vs 0.1062.)

Mow-band contrast in PES (this session, `p12_dec.png` green channel): a
smoothed horizontal profile across grass_b/c modulates 97-171 (Michelson
0.275) — the stripes ARE in PES's frame. A vertical profile through the
giraffe paint (x=295, y=440-484) reads 253/255/244/168 inside the paint vs
~103-109 in adjacent grass: the paint has NO mow modulation riding on it at
this resolution (paint opaque, stripes absent over paint).

PES install texels decoded this session (stock VFS, `pesfmt_dump cat` +
independent decode; R/B order cross-checked against
`gf-Pitchmat-scratch/turf_bsm_alp_rgba32.png`: ftex pf 0 is D3DFMT_A8R8G8B8,
file bytes BGRA, GF `pes_texture.cpp:132-133` swizzles to RGBA — correct):

| slot | file (stock) | format | decoded (CONFIRMED this session) |
|---|---|---|---|
| t0 pitch_alp (st021) | `Asset/model/bg/st021/sourceimages/tga/#windx11/pitch_alp.ftex` | DXT5 4096x2048 | play area: R p50 125, G p50 16, B=255 (mask), A p50 0 (max 100/255) |
| t3 turf_bsm (common) | `Asset/model/bg/common/pitch/sourceimages/#windx11/turf_bsm_alp_rgba32.ftex` | RGBA8 1024 | RGB mean (65.3, 85.1, 11.9), A mean 97.7/255 |
| t7 grain (st021) | `.../pitch_grain_alp.ftex` | BC7 2048x1024 | A max 10/255, mean 0.05 (repair weight ~0) |
| t5 slope (common) | `.../grass_slopetable_alp.ftex` | DXT5 128 | RGBA mean (0.521, 0.843, 0.734, 0.381) |
| t4 detail (st021) | `.../pitch_detail.ftex` | DXT1 2048x1024 | (per PitchMaterial §0) |
| t1 turf_nrm (common) | `.../turf_nrm.ftex` | DXT5 512 | — |
| t6 mask | UNBOUND (no fmdl role maps it) | — | effective 0 (see §1) |
| line_alp (common) | `.../line_alp.ftex` | DXT5 512 | RGB const (207, 210, 207), A 0-135 |
| line_srm | `.../line_srm.ftex` | DXT1 512 | (per PitchMaterial §0: R/G mean 182/182) |
| grassfin (common turf3d) | `Asset/model/bg/common/turf3d/sourceimages/#windx11/grassfin_bsm_alp.ftex` | DXT5 128 | R p50 0, G p50 80, B p50 255, A p50 0/max 255 |

fmdl roles (this session, `pesfmt_dump fmdl pitch_st021.fpk`): exp1_pitch_mat
binds t0/t1/SpecularMap(dummy)/t3/t4/t7/t5 — NO mask role. Placed stadium
members (this session, `LoadStadiumScene(vfs,21,0)`, 18 members): pitch,
stands, goals, nets, corner flags — NO turf/grass member. GF `ShaderFor`
(`pes_model.cpp:40-48`) has no turf3d technique; `turf3dtexlodu4.fmdl`
(4.9 MB, common/turf3d) is never referenced by GF code.

## 1. Candidate 1: mask-unbound semantics — REJECTED (no effect, CONFIRMED)

PES: `pitch2020.ps.asm` declares `t6` (line 20) and samples it once (line 104:
`sample r8.xyz, v1, t6, s6`). Downstream only `r8.x` (maskR), `r8.y` (maskG),
`r8.z` (maskB) are read (`max r2.xy, r2.wyww, r8.xyxx`; `mul_sat r2.yzw, r8.z,
(0, 0.1, 0.5, 1)`); `r8.w` (mask alpha) is NEVER read. The st021 pitch fmdl
lists no texture role for slot 6 (§0), so no texture is assigned to t6 for
this draw. D3D11 unbound-SRV reads return 0 (PitchMaterial §1, INF-standard).
Effective PES mask RGB = (0,0,0).

GF: `pes_pitch.gdshader:53` declares
`Mask_Tex_SRGB : hint_default_black`; `Material()` (`pes_model.cpp:69-76`)
sets only roles present in the fmdl — no mask role exists, so the uniform
keeps its default (black, rgb 0; only `.rgb` is sampled at lines 136-139,
`srgb_to_linear(0) = 0`). Effective GF mask RGB = (0,0,0). The shader alpha
path is inert on both sides (PES never reads `r8.w`; GF reads `.rgb` only).

Frame proof the effective PES mask is 0: any stale-nonzero maskR/G would lift
dark-turf albedo toward 0.8 (`ALBEDO = a*(0.8-n)+n*m2`: a=0.1 on n=0.03 gives
0.105, a 3.5x lift). PES dark turf is DARK (Y 0.106 with R/G 0.58 = the
a~0 albedo chroma, §4). A stale maskB>0 can only darken via
`m2 = 1-sat(maskB*0.1)` to a floor of 0.9, i.e. at most 1.11x — insufficient
for 1.2-1.5x even in the most contrived state, and it requires the previous
draw to leave a white texture in slot 6. Bounded and rejected.

Predicted effect on dark-turf boxes: 0. On the giraffe: 0 (the line-decal
material never reads t6). Leaves the giraffe matched. VERDICT: REJECTED.
Status: CONFIRMED (asm use + fmdl roles + frame darkness bound).
No port change.

## 2. Candidate 2: the `32*a^2` alpha path — REJECTED (global-only, CONFIRMED present)

`Tonemap_Lum.ps.asm` lines 12-16 (TurfColour scratch `post/`):
`lum = max(dot709(rgb), 0.001, 32*a^2)` where `a` is the sampled scene-buffer
alpha (line 11 samples `t0.xyzw`, line 13-14 square and x32 with literal
`3.20000000e+01`). Present, CONFIRMED.

The pitch's own alpha on dark turf: `pitch2020.ps.asm` line 113
`mov o0.w, 1.0` — CONSTANT 1 on all pitch pixels, dark or bright, CONFIRMED.
The light accumulation also writes 1:
`SSLighting_Sun_NoShadow.ps.asm` lines 137-139
(`mov o0.w/o1.w, 1.0`; `mov o2.xyzw, 1,1,1,1`), shadowed variant line 270
`mov o0.w, 1.0`. So a=1 everywhere lit.

`max()` can only RAISE PES's metered mean (lowering PES's EV). GF omits the
term (`ponytail`, ComposePath §4). Two consequences, both fatal to this
candidate: (i) it scales ALL lit pixels equally — grass, lines AND giraffe —
so it cannot produce a dark-turf-only 1.2-1.5x with the giraffe matched; the
giraffe (paint alpha from `line_alp` A 0-135, scene alpha 1 after lighting
like everything else) would move with the grass. (ii) The comparison is made
at MATCHED EV (E9 at PES's EV), where any global meter-scale error is absorbed
by construction (LumaResidual §3 CLOSED metering as cause; live Lmeas 0.226
vs the 32 the term would give if the metered rect were all alpha-1 also shows
the live rect/alpha state is not the naive all-1 — OPEN values, but CLOSED as
a residual cause).

Predicted effect on dark-turf boxes: global gain, absorbed at matched EV (0
in the comparison). On the giraffe: identical global gain (moves together).
VERDICT: REJECTED as the residual cause. Status: CONFIRMED present, CONFIRMED
global-only. No port change (keep the `ponytail`; modelling needs the scene-
alpha writer per TurfColour item 4, still OPEN but not this ticket).

## 3. Candidate 3: a GF-side second light — REJECTED (out of range, CONFIRMED)

Audit this session (`light_audit` probe on stock rig, 147 enabled lights):
the 15:00 day span (`TimeSpan1500_day`) enables group `{ReflectionProbe}`
only. All 10 pitch floodlight chains (`Light_Pich`, 8.0M lm) are OFF in GF at
day; all DemoAdd omnis grouped OFF. `GroupOn` (`pes_lighting.cpp:45-50`)
returns true for EMPTY chains (ungrouped lights stay as authored): exactly 3
lights are ungrouped and therefore ON in GF at the day pose —
`SL_Room_0000/0001/0002_DemoAdd`, 20000 lm spots, range 15 m, at
(72.7,4.5,52.3), (-70.3,3.5,49.6), (-0.4,3.2,59.0).

Range check vs the comparison boxes (mid-pitch/centre-circle/far-touchline,
all tens of metres from these room lights at the stands ends): nearest box
distance ~90 m >> 15 m range — Godot hard-cuts at `spot_range`, contribution
exactly 0 on every box. Moon is off at day (`visible = !sunOn`, sun fade 1.0,
mode 0). Sky ambient in GF is 0.064 of sun-grass vs PES candidates 0.108-0.269
(weaker, wrong direction — PitchMaterial §0/LumaResidual §1). Reflection
probes are file-driven in both (no decode divergence; specular negligible per
X4). So there is NO second light at the metered pose that touches the boxes.

(Pedantic metering note: the 3 room spots can only lift GF's frame-mean via
the stands, which would LOWER GF's EV — wrong-signed for a GF excess, and
absorbed at matched EV anyway.)

Predicted effect on dark-turf boxes: 0 (out of range). On the giraffe: 0.
VERDICT: REJECTED. Status: CONFIRMED (rig audit + range check).
Port spec: none — except optionally gating ungrouped DemoAdd lights by group
membership (a `ponytail` cleanup, NOT a fidelity fix; zero box effect).

## 4. Candidate 4: albedo/diffuse absolute level for dark turf — SPLIT

### 4a. The surface albedo chain: EXONERATED (every lane 1.0, CONFIRMED)

Recomputed this session with correctly-ordered texels (base (65.3, 85.1, 11.9)
sRGB; earlier scratch note had R/B swapped in one script — the independent
`turf_bsm_alp_rgba32.png` and GF's BGRA swizzle at `pes_texture.cpp:132-133`
confirm (65.3, 85.1, 11.9)): nominal dark-turf albedo = (0.0114, 0.0216,
0.00064), R/G 0.525, B/G 0.029 — the PES frame chroma (0.585, 0.052) at
achromatic light, consistent.

Per-lane PES-vs-GF (values §0; math PitchMaterial §1 lane-identical):

| input | PES | GF | ratio |
|---|---|---|---|
| base t3 rgb | (65.3, 85.1, 11.9) sRGB, HW-decoded | same bytes (BGRA swizzle correct), `srgb_to_linear` | 1.0 |
| pitchR t0 r | 125 sRGB, HW-decoded | same texel, `srgb_to_linear` | 1.0 |
| lin t3 a | pow(97.7 raw, 1/2.2), alpha raw | `pow(turf.a, 1/2.2)`, Godot never sRGB-decodes alpha | 1.0 |
| pitchG t0 g | 16 sRGB, HW-decoded | same, `srgb_to_linear` | 1.0 |
| grainA t7 a | ~0 (max 10/255) | same | 1.0 (repaired ~= base both) |
| slopeG t5 g | 0.843 mean; GREEN flat at broadcast V (T1 frame divergence moves only RED/specular, X4-negligible) | same LUT | 1.0 (spend equal) |
| mask t6 | 0 (§1) | 0 (§1) | equal |
| cb4 Withers/Repair/Srm/U/V | fmdl dump = GF defaults lane-for-lane (§0) | runtime set from same fmdl (`pes_model.cpp:68`) | 1.0 |
| filters/mips | DX11 auto LOD | Godot auto LOD; nearest-vs-linear only re-noises (median-unbiased) | 1.0 median |

Renderer-side per-material knobs also close the wrong way or negligibly:
`SPECULAR` (Godot ambient reflection) is 0.17 on dark turf (specScale) vs 0.71
on the decal (`srm.r` = 182/255) — a Godot-ambient carrier would brighten the
GIRAFFE more, but the giraffe matches: REJECTED direction. `ROUGHNESS` ~0.9
(turf) vs 0.71 (decal): both rough, no sharp differential. T-wrap, shadow
(sunlit INF), visibility-discard (binary B, edge texels) all match/negligible.

VERDICT 4a: the surface material has NO input scale left. REJECTED as the
carrier. Status: CONFIRMED (decodes + frame chroma + wrong-way renderer
knobs).

### 4b. The missing turf3d fin geometry: last standing mechanism (INF)

What remains must darken PES grass but not PES paint, at ~26% (1-1/1.35 mid):
- PES draws a grass-fin mesh over the surface (technique `pes3DDF_Turf3D2020`,
  `turf3dtexlodu4.fmdl` 4.9 MB in common/turf3d; PS `pes3ddf_turf3d_2020.ps.asm`
  84 lines: fin alpha test at `t7.a < 0.3`, distance fade, `o0 = finColor`; VS
  237 lines: wind + world-space stripe-mask lookup). GF NEVER BUILDS IT: no
  turf3d technique in `ShaderFor`, no turf member in `LoadStadiumScene` (18
  placed members, §0), no reference in GF code. CONFIRMED missing.
- The fins sit just above the surface (pitch fmdl AABB y 0..0.008) and at the
  broadcast grazing angle (V.y ~0.45) dense fin coverage occludes/shadows the
  surface: net darkening of the grass boxes by tens of % is plausible.
- Giraffe-immune by construction: the paint shows NO mow modulation (profile
  §0: 253/255/244 inside paint vs ~103-109 grass) — the decal draws over (or
  the fin mask kills fins on) the paint, so missing fins lift grass only.
  This is EXACTLY the observed signature (grass 1.2-1.5x, paint 0.6-1.0x).
- The mow stripes themselves live on the fins' world-space mask
  (pitch-grass §5.2), consistent with stripes visible in PES's frame and
  absent from GF's surface-only render.

What blocks CONFIRMED: (i) the fin PS darkening factor is not yet reduced to
a number (fin colour `v3`/luminance scale `v1.z` come from the 237-line VS,
OPEN per pitch-grass §8); (ii) no live read proves the fins draw in this
match/camera (technique-slot or vertex-buffer read). Magnitude needed (~26%
mean occlusion) is plausible but not yet derived. Status: INF (high).

## 5. Attribution

| leg | status | share of the 1.2-1.5x |
|---|---|---|
| hour/sun/exposure/shadow/metering | CLOSED/INF-0% (LumaResidual §5) | 0 |
| mask-unbound (§1) | CONFIRMED no-effect | 0 |
| `32a^2` (§2) | CONFIRMED global-only, absorbed at matched EV | 0 in-comparison |
| second light (§3) | CONFIRMED out-of-range | 0 |
| surface albedo inputs (§4a) | CONFIRMED 1.0 every lane | 0 |
| missing turf3d fins (§4b) | INF, last standing | ~full residual (~26% PES-side darkening) |

## 6. Port spec (GF changes; no mask/alpha/light/albedo change)

1. NO CHANGE: t6 binding (leave `hint_default_black`), `Tonemap_Lum`
   (keep the `32a^2` `ponytail`), floodlight `GroupOn`, every albedo lane,
   `SPECULAR`/`ROUGHNESS` handling.
2. The fix that can close the residual: PORT THE TURF3D FINS (or their net
   darkening): build `Asset/model/bg/common/turf3d/#Win/turf3dtexlodu4.fmdl`
   into the stadium (placement source: the pitch-area locator set — OPEN,
   the fox2 placer that references it is not yet identified; it is NOT in
   `LoadStadiumScene`'s 18 members), with technique `pes3DDF_Turf3D2020`
   (PS done: 84-line alpha-test fin shader above; VS wind/mask dry-lush
   ramps still OPEN), OR — if the full fin port is out of budget — a
   measured fin-darkening factor on the surface (needs the VS `v1.z`/`v3`
   reduction first; do NOT tune to the frame: derive from the VS).
3. Confirm reads (either closes §4b to CONFIRMED or reopens the hunt):
   (a) live technique-slot read proving the turf3d technique binds in the
   p12 match at this camera; (b) the VS `v1.z`/`v3` scale reduction giving
   ~0.65-0.85 net fin transmission on dark turf; (c) one GF frame with fins
   re-measuring grass_a..d + giraffe (predict: grass drops to ~1.0x, paint
   unmoved).
4. Incidental (not the residual, do not bundle): gate the 3 ungrouped
   `SL_Room_*_DemoAdd` spots by group membership (`ponytail` hygiene).

## 7. Open items (numbered)

1. Turf3d VS `v1.z`/`v3` reduction (fin luminance scale + dry/lush ramps) —
   decides the fin port's absolute level. (Blocks §6.2.)
2. Turf3d placement source (which fox2 references `turf3dtexlodu4` for st021).
3. Live proof the fins draw in-match at this camera (technique slot).
4. `param_4` writer / live `cb5[12]` (carried, second-order, not the residual).
5. `build/` staleness (carried from LumaResidual §7.4).

## 8. Reproduce

- Fmdl/roles: `PES21_ROOT=<workdrive>/PES21 pesfmt_dump fmdl
  'Asset/model/bg/st021/pitch/#Win/pitch_st021.fpk'`.
- VFS texels: `pesfmt_dump cat <path> --pes21-root <workdrive>/PES21` for the
  §0 table; DXT5/RGBA8 decode as in `gf-DarkTurfRE-scratch/stats*.py`
  (NOTE: ftex pf 0 bytes are BGRA — swizzle R<->B, cf. `pes_texture.cpp:133`;
  the naive parse swaps turf R/B and refutes itself against the frame
  chroma; the PNG cross-check `turf_bsm_alp_rgba32.png` R 65.3 settles it).
- Rig audit: `gf-DarkTurfRE-scratch/light_audit(.cpp)` + cleanbuild
  `libpesfmt_clean.a` (stale `build/` links crash — use the cleanbuild lib).
- Shaders: `pitch2020.ps.asm` (mask sample L104, `o0.w` L113),
  `SSLighting_Sun_NoShadow.ps.asm` (w L137-139),
  `Tonemap_Lum.ps.asm` (32-term L12-16), `pes3ddf_turf3d_2020.ps.asm` (84
  lines, alpha test), `.vs.asm` (237 lines).
- Frame: `~/ghidra-projects/scratch/obs/p12_dec.png` + box script (§0 table);
  zooms `gf-DarkTurfRE-scratch/zoom_grass.png`, `zoom_giraffe.png`.
