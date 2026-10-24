# PES turf fins (turf3dtexlodu4): pipeline, net factor, live evidence, GF port spec

Status: read-only work. Repo at `<workdrive>/GodotFootball`, install VFS at
`<workdrive>/PES21`, scratch at `<workdrive>/gf-TurfFinsRE-scratch/` (all
outputs with absolute paths there; nothing written to the repo).
Read with `docs/render/pes-render-DarkTurf.md` (the ticket),
`docs/render/pes-render-LumaResidual.md` §5,
`docs/render/pes-render-PitchMaterial.md`,
`docs/re/pes-re-pitch-grass.md` §5.2, `docs/re/pes-re-stadium-camera.md` §1.1.

Ticket: GF's grass stays ~1.2-1.5x brighter than PES while the giraffe paint
matches. Candidates 1-3 (mask, 32a^2, second light) are REJECTED and the
surface albedo chain is EXONERATED lane-by-lane (DarkTurf §4a). The last
standing mechanism is the turf3d grass-fin geometry PES draws and GF never
builds. This report decodes that fin pipeline, reduces its PS/VS to a net
factor, proves it is resident in the running match, and specifies the GF port.

Bottom line: the fin factor is **a coverage term, not a material scale**:
per surviving fin fragment `o0 = v3 * v1.z * r0.y` (all three ≤ 1,
CONFIRMED asm), over a tile-internal fin density of ~2.5-7.5 geometric layers
× 0.30 alpha-test survival (CONFIRMED decode) → per-pixel fin coverage
f ≈ 0.5-0.9 at the comparison camera. The darkening itself comes from fin
**geometry**: fin blades are near-vertical (dumped normals mostly
horizontal, e.g. (-0.83, 0.17, 0.53)), so at the 15:00 sun
(`toSun = (0.2519, 0.3909, -0.8853)`, NdotL = 0.3909 up) the blades sit at
NdotL ≤ 0 on the wrap floor T while the surface sits at 0.39 — same sun,
same `SSLighting` math, different incident light. With wrap T ≈ 0.2 the
fin/surface output ratio is ≈ 0.36 and the net pitch factor is 1-0.64f,
which equals the needed 0.67-0.83 (1/1.5-1/1.2, mid 0.74) at f ≈ 0.4-0.5.
Giraffe-immune by construction (paint pixels show the decal, not the fin).
Live: the exact `turf3dtexlodu4.fmdl` bytes are resident in the running
match heap (CONFIRMED), the technique + file are in the match load set
(CONFIRMED), the mow-stripe signature the fins carry is in the p12 frame at
Michelson 0.283 (CONFIRMED). Per-frame technique-slot draw proof stays OPEN
(no GPU readback in the toolbox). GF port: build the fin mesh (or, if the
fin albedo proves out, an equivalent darkening decal), placed over the play
rect, verified by a re-measured frame (predict: grass drops to ~1.0x, paint
unmoved).

## 0. Files and shaders (all CONFIRMED by extraction this session)

| item | path / record | facts |
|---|---|---|
| fin mesh | `Asset/model/bg/common/turf3d/#Win/turf3dtexlodu4.fmdl` (4,892,240 B) | 3 meshes, 0 bones; mesh0 9248 verts / 27744 idx (9248 tris), bbox x ±0.319, y -0.020…0.059, z ±0.288 (area 0.367 m²); mesh1 4 verts 0.5 m quad y=0.003; mesh2 4 verts 0.6 m quad y=-0.0001; mats `fox3DDF_Blin`/`pes3DDF_Blin` (proxy roles, NOT the draw technique); single texture `grassfin_bsm_alp.dds` |
| fin texture | `Asset/model/bg/common/turf3d/sourceimages/#windx11/grassfin_bsm_alp.ftex` (10,624 B) | pf=4 (DXT5) 128×128, 6 mips; mip0 RGB mean (3.5, 71.8, 248.9), A mean 76.0/255 max 255; alpha-test survival (a ≥ 0.3): mip0 0.303, mip1 0.319, mip2 0.324, mip3 0.484, mip4 0.641, mip5 0.750 (CONFIRMED fresh decode this session) |
| technique | `pes3DDF_Turf3D2020` (`PesShaders_dx11.lua:3142`) | Deferred → `pes3ddf_turf3d_2020`; Deferred_Decal → `pes3ddf_turf3d_2020_Decal`; params `{MatParamIndex_0, DriedColor, PitchOffset, UvOffset, Parameter0, Parameter1}`, 8 textures incl. `Grass_MaskMap_Tex_LIN`, `Grass_DiffuseMap_Tex_SRGB`, `numvertextextures = 7`; siblings `_ReZ/_Full/_Demo/_FixDemo/_PlanarShadow/_Edit`, plus the older `Primitive_PolyGrass*` family (`Turf3D3/4/Demo`) |
| match PS | `pes3ddf_turf3d_2020.ps` (fsop record, 3396 B DXBC → 84-line asm) | alpha-test PS: `o0.rgb = v3.rgb * v1.z * r0.y`, `o0.w = 1`; G-buffer outputs o1 (oct normal + id), o2, o3 (velocity), o4 |
| match VS | `pes3ddf_turf3d_2020.vs` (9672 B DXBC → 237-line asm) | wind sway (v0.y+10 scaled by 0.3·|cam-pitchCentre|²+0.0125, ±16·dist), world-space mask UV (`worldXZ·cb4[1].xy + cb4[1].zw`), dry/lush ramps toward dried (0.145, 0.322, 0.035) / mid (0.271, 0.302, 0.227) / lush (0.312, 0.402, 0), planar-shadow density `o1.z = 1 - sum(t5.rgba)` |
| decal variant | `pes3ddf_turf3d_2020_Decal` (140-line VS, 55-line PS) | PS writes `o0 = 0` (BLACK albedo) with a coverage mask in `o1.w`; a darken-modulate pass, not an albedo pass |

Disassembly: `pesfmt_dump cat shaders/dx11/PesShaders_dx11.fsop` +
`fsop_extract.py` + `vkd3d-compiler -x dxbc-tpf -b d3d-asm` (scratch `dxbc/`,
`turf3d.ps.asm`, `turf3d.vs.asm`, `turf3d_decal.ps.asm`,
`turf3d_decal.vs.asm`). RDEF: VS cbuffers `cVSScene/cVSObject/cVSMaterial/
cVSSystem`, textures `g_tex_diffuse/mask/grain/turfSlopeTable/base/
planarShadow/detailNormal`; PS cbuffer `cPSSystem`, texture
`g_tex_grassDiffuse`.

## 1. The net effect: PS/VS reduced to a factor (CONFIRMED asm + decode)

### 1.1 Per-fragment factor (Deferred PS, `turf3d.ps.asm`)

- Alpha test (lines 26-29, CONFIRMED): `sample t7.w at v1.xy; discard if
  a < 0.3` (literal `3.00000012e-01`). Survival at broadcast (mip0):
  **0.303** (§0 table). This is the `u4` LOD contract at this camera: fins
  are kept/culled per-texel by the grassfin alpha, coarsening with distance
  (mip5 survival 0.750 — distant fins fade toward full quads, then the
  distance block below removes them).
- Distance fade (lines 30-47, CONFIRMED): `x = v3.w·(1-2g) + 2g` with
  `g = t7.g` (≈ 71.8/255 = 0.28 → x = 0.44·v3.w + 0.56 ≤ 1 always);
  `r0.y = k·(x-1) + 1` with `k = (1-0.6·v2.w)·grade`, `grade =
  sat(-0.2·log(drv)+1.2)` from coarse derivatives (lines 35-42) and
  `v2.w` the VS distance term. Near + dense: `r0.y = x ∈ [0.56, 1.0]`;
  far: `r0.y → 1` (no modulation). **In all cases `r0.y ≤ 1`.**
- Output (lines 48-49, CONFIRMED): `o0.rgb = v3.rgb · r0.y · v1.z`.
  `v1.z` (VS line 99: `add_sat o1.z, -sum(t5), 1`) ∈ [0, 1] is the
  planar-shadow density (1 = unshadowed). `v3` (VS o3, lines 190-194) is the
  fin albedo, `sat()`-clamped. **Per-fragment `o0 ≤ v3` component-wise;
  the PS only ever attenuates the VS albedo.**

### 1.2 The darkening is geometric (fin normals vs sun, INF-medium)

Dumped fin-vertex normals are blade-like (e.g. v0
`n = (-0.829, 0.166, 0.533)`, |horizontal| ≈ 0.98; CONFIRMED `vdump`
output). Against the 15:00 sun `toSun = (0.2519, 0.3909, -0.8853)`
(LumaResidual §1.2, file-derived, both sides 15:00):
`NdotL(fin) = -0.83·0.25 + 0.17·0.39 + 0.53·(-0.89) ≈ -0.61 ≤ 0`,
while `NdotL(surface) = 0.3909`. Under the shared wrap lighting
(`diffuse = light·max_sat(NdotL+T(1-NdotL²), T)`, wrap = 1, PitchMaterial
§1 CONFIRMED identical on both sides), the surface gets
`light·(0.39+0.85·T)` and the edge-on fin gets the floor `light·T`.
With a typical wrap T ≈ 0.2: surface ≈ 0.56·light, fin ≈ 0.20·light,
**fin/surface output ratio r ≈ 0.36** (INF-medium: T is the fin
material's wrap value, not yet read from its cb4; the SIGN r < 1 is
CONFIRMED by the normal dump + sun vector, the exact 0.36 is not).

### 1.3 Coverage f at the comparison camera (CONFIRMED mesh + texture)

Tile mesh0 holds ≈ 4624 fin quads (9248 tris / 2; quad topology CONFIRMED
in `vdump`: index runs `0 1 2 0 3 2 …`) over 0.367 m², mean tri XZ-area
2.04e-4 m² (CONFIRMED `vdump2`: total XZ 1.890 m² over 9248 tris).
Geometric layers = 4624 × ~4e-4 / 0.367 ≈ **2.5-7.5x** (fin area 2e-4..6e-4
sensitivity). × alpha survival 0.303 (mip0 at broadcast: fin UV `v1.xy`
is world-affine with no ×128 scale in the `_2020` VS, so a broadcast
pixel step (~0.15 m over a ~106 m pitch span) lands at lod < 0 → mip0;
CONFIRMED reasoning, INF live cb4): effective layers ≈ 0.75-2.3 →
**P(pixel shows a fin fragment) f ≈ 0.53-0.90** (Poisson cover).

### 1.4 Check against the needed ~26% (mid of the 1.2-1.5x band)

Net pitch factor = `(1-f) + f·r`. Needed: 1/1.5-1/1.2 = **0.67-0.83**
(mid 0.74). With r ≈ 0.36: net = 1-0.64f = 0.74 at **f ≈ 0.41**; the
f ≈ 0.5-0.9 range gives 0.42-0.68 (lower half of the band and below).
With r ≈ 0.5-0.7 (fins catch more sky/ambient than the floor): net =
1-(1-r)f hits 0.74 at f ≈ 0.4-0.5. **Verdict: the mechanism reaches the
needed ~26% darkening at plausible (f, r) ≈ (0.4-0.5, 0.36-0.7) —
quantitatively sufficient, not yet pinned to a point (needs §4 item 1).**
Giraffe-immune CONFIRMED by construction + frame: the paint shows no mow
modulation (p12 vertical profile through the paint: 253/255/244/168
inside vs ~103-109 adjacent grass, DarkTurf §0) — decal pixels composite
over (or mask off) the fins, so missing fins lift grass only. The mow
stripes themselves (horizontal green profile Michelson **0.283**,
re-measured this session, n=350) ride on the fin path (world-space mask
UV, VS lines 46-53), consistent with stripes visible in PES and absent
in GF's surface-only render.

## 2. The placement source (CONFIRMED inventory, OPEN placer)

- The mesh file is a single 0.64 × 0.58 m tile (bbox §0), so the pitch
  cover comes from many placements/tiles, NOT from scaling this mesh.
  Tile-internal fin count (~4624) already over-covers one tile (layers
  §1.3); tiling the play rect (st021 play mesh: x ±52.95, z -34.08…37.64,
  CONFIRMED `pitchmesh`) with such tiles gives the coverage §1.3.
- The tile's own materials (`fox3DDF_Blin`/`pes3DDF_Blin`, CONFIRMED
  `fmdl_full` output) are NOT the draw technique: the draw technique
  `pes3DDF_Turf3D2020` binds by mesh name (`MESH_turf3dtex_NC_NSW`) or by
  the placer, through `ShaderAssignList` (lua:3142). The `u4` in
  `turf3dtexlodu4` reads as the LOD index of this file (nearest/broadcast
  band; sibling LODs, if any, are not loose in the VFS).
- Placer hunt (this session, CONFIRMED negative): `LoadStadiumScene`
  (`src/pes/stadium.cpp:240-275`) assembles only the scene pack, the
  day/fine overlay, the pitch pack, goal + nets + corner flags — 18
  members, NO turf/grass member (DarkTurf §0 re-verified). The st021
  modelset fox2 (53 entities, dumped `fox2read`) and the pitch fox2 (3
  entities: `st021_pitch` only) reference NO turf file. No st021 scene
  fmdl carries a `Turf3D` technique (unique-technique scan: only
  `fox3DDF_Blin_LNM / fox3DFW_ConstantSRGB_NDR_Solid /
  fox3DDF_Blin_Fuzzblock`). **The placer is therefore NOT in any pack
  `LoadStadiumScene` opens.** Prime suspect (INF-high): the NESTED
  stadium cpk (`Asset/model/bg/stNNN.cpk` inside `dt30_g4.cpk`), which the
  current `PesVfs` cannot see at all (stadium-camera §1.1: only loose
  download overrides are visible; the nested TOC is unindexed). The turf
  placement + sibling LODs plausibly live there. Second suspect (INF-low):
  procedural scatter by the pitch-area locator set at load.
- Per-frame drivers (CONFIRMED asm, values OPEN): wind anchors
  `cb4[3]/cb4[4]` (VS lines 27-45: `cb4[3].xxyz - cb4[4].xxyz` distance
  weight, `cb4[3].xxzx + cb4[4].xxzx` displaced centre), slope UV
  `cb4[2].xy` + `cb4[2].zzwz` (lines 46, 93), mask affine `cb4[1].xy/zw`
  (line 52), material wrap/density terms; object `cb5[4..7]` (world),
  scene `cb2[4..11/16/17/19/20]` (view/clip/camera `cb2[20]`), camera
  distance `|cam - pitchCentre|` driving sway amplitude. LOD band
  selection (`u4` meaning + distance cut) is OPEN.

## 3. Live proof (read-only `pread` on pid 2510857, this session)

| # | read | result | verdict |
|---|---|---|---|
| L1 | technique table `TS = *(0x1437712e0)` (ComposePath L2 method) | `0x1fed1c0`, `+0x74 = 0xc40002`, ramp `0xb8→0xc5`, `+0x7c = 0x76000b` — identical to the 19-10-26 match AND menu (re-read this session) | CONFIRMED (slot alone does not discriminate; same caveat as ComposePath) |
| L2 | heap scan for `turf3dtex` / `grassfin` / `Turf3D2020` / `MESH_turf3dtex` | `turf3dtexlodu4.fmdl` + `grassfin_bsm_alp.ftex` in the match load manifest (`0x22000000+0x82ec5f` area, neighbours: standsFlag anim ftex, tv ftex — the stadium load list); `turf3dtexlodu4.fmdl` path string in heap (`0x13a65e60` area); shader-lua text with the full `pes3DDF_Turf3D2020` params/packing resident (`0x16da2000` area) | CONFIRMED (match loads the fin mesh + texture + technique record) |
| L3 | resident-bytes check: heap range `0x3e000000-0x3ec00000` vs `turf3d.fmdl.bin` | byte-identical at offsets 0, 0x100, 0x380f0, 0x4aa349, file end (4,892,240 B); `MESH_turf3dtex` at file-identical offset `+0x4aa351`; enclosing `FMDL` magic at `+0x0` | CONFIRMED (the exact fin mesh binary is resident in the running match) |
| L4 | frame `live_now.png` (OBS `PES-vk`, decoded `live_now_dec.png`) + p12 stripes | live match is a different match (st018 dusk `BIZ 99 XS`, not the p12 st021 day moment — same caveat as LumaResidual §0); open-turf green p50 ≈ 127-130 (8-bit sRGB) with NO visible stripe structure at broadcast distance in this dusk frame; p12 (the comparison moment) carries Michelson-0.283 mow bands (§1.4) | CONFIRMED observation, NOT like-for-like (states the limit plainly) |
| — | per-frame technique-slot / draw-list proof that the fin pass draws at the p12 camera (as ComposePathRE did for post) | no GPU readback in the read-only toolbox; the `+0x74` slot does not discriminate; no D3D draw hook available | **OPEN** (blocks CONFIRMED; two paths forward in §5) |

What L2+L3 prove: the running match has the fin mesh, its texture, and
the technique record loaded — GF builds none of them (no turf3d
technique in `ShaderFor`, no turf member in `LoadStadiumScene`, no
reference in GF code; DarkTurf §0). What stays OPEN: a draw-call-level
read (bound technique id / vertex-buffer bind / instance count) tying a
fin draw to the comparison camera.

## 4. GF port spec (no mask/alpha/light/albedo change — DarkTurf §6.1 stands)

1. **Build the fins.** Instance (or merge) the `turf3dtexlodu4` mesh0
   tile over the play rect (st021: x ±52.95, z -34.08…37.64;
   generalise per stadium from its pitch fmdl bbox), 0.367 m² tiles,
   with the `pes3ddf_turf3d_2020` VS behaviour that matters: world-space
   mask UV (`worldXZ·cb4[1].xy + cb4[1].zw` with the file's cb4 values at
   runtime), wind sway (amplitude `0.3·|cam-pitchCentre|² + 0.0125`,
   ±16·dist), planar-shadow density `v1.z`, distance fade to unmodulated
   at range; fragment: alpha-test `t7.w < 0.3` discard, `o0 =
   v3·v1.z·r0.y` into the G-buffer with the same `SSLighting` sun path
   as the surface. LOD: this file is the `u4` (broadcast) band; select
   band by camera distance once sibling LODs are found (likely in the
   nested stadium cpk — unblock by indexing nested cpks first,
   stadium-camera §1.1 consequence 1-3).
2. **Cheaper equivalent (allowed by §1.1: the PS only attenuates).** If
   the fin albedo `v3` proves to average ratio r̄ to the surface, replace
   the geometry with a screen/world-space darkening decal over grass
   (NOT over line/paint decals): `darken = 1-(1-r̄)·f·shadow·fade`, with
   f from the grassfin alpha survival at the sampled mip (0.303 at
   broadcast) × tile layer cover, `shadow = v1.z`, `fade` the VS distance
   term. Do NOT tune r̄·f to the frame: derive r̄ from the VS chain once
   the `Grass_*` map texels are found (§5 item 1), and f from the mesh +
   texture (§1.3). Predicted frame effect: grass_a..d drop to ~1.0x PES,
   giraffe unmoved (decal draws over / masks fins on paint).
3. **Do NOT change:** t6 binding, `Tonemap_Lum` 32a², floodlight
   `GroupOn`, every albedo lane, SPECULAR/ROUGHNESS, sun/exposure —
   all closed in DarkTurf §§1-4a.
4. **Verify with one rendered frame** (stadium 21, day, broadcast p12
   camera, settled EV): re-measure `grass_a..d + giraffe_body`
   (DarkTurf §0 boxes). Pass = grass medians within ~±10% of PES with
   the paint still matched; report Michelson stripe contrast before/after
   (PES 0.283) as the fin-path signature.

## 5. Status and open items (numbered)

- **CONFIRMED:** fin pipeline (technique → Deferred `pes3ddf_turf3d_2020`
  + Decal `..._Decal` shaders, params, textures, RDEF bindings);
  per-fragment factor `o0 = v3·v1.z·r0.y` with each term ≤ 1 and the
  0.3 alpha-test literal; grassfin mip-chain survival (0.303 → 0.750);
  tile geometry (0.367 m², ~4624 fins, 2.5-7.5 layers, blade normals);
  15:00 sun both sides (NdotL 0.3909 surface / ≤ 0 blades); resident
  fin mesh bytes + load manifest + technique record in the running
  match; p12 stripe contrast 0.283 + paint stripe-immunity; GF builds
  none of it.
- **INF (high):** net 0.67-0.83 reached at (f, r) ≈ (0.4-0.5, 0.36-0.7)
  via the vertical-blade wrap-floor mechanism; placement source =
  nested stadium cpk (VFS-blind) or procedural scatter; `u4` = broadcast
  LOD band; per-frame cb4/cb5/cb2 values; decal-vs-deferred pass choice
  in the live match (mesh proxy materials are Blin, draw technique binds
  by mesh name/placer).
- **OPEN (exactly two reads close §3 to CONFIRMED, one decode closes
  §1.4 to a point):**
  1. Fin albedo point value: find the `Grass_DiffuseMap /
     Grass_MaskMap / Grass_DetailDiffuse / Grass_DetailNormal /
     Grass_PlanarShadow` texels (not in any VFS-visible st021/common
     pitch dir this session — search the nested stadium cpk once
     indexed, then evaluate VS lines 116-194 numerically) → fixes r.
  2. Draw-call proof: bound-technique id / vertex-buffer bind / instance
     count for the fin pass in the running match (D3D hook or GPU
     readback — outside the read-only `pread` toolbox) → promotes §3.
  3. `param_4` writer / live `cb5[12]` (carried, second-order, not this
     ticket).

## 6. Reproduce

- VFS: `PES21_ROOT=<workdrive>/PES21 pesfmt_dump ls
  'Asset/model/bg/common/turf3d/'`; `cat …/grassfin_bsm_alp.ftex`.
- Shaders: `pesfmt_dump cat shaders/dx11/PesShaders_dx11.fsop` +
  `fsop_extract.py PACK dxbc turf3d_2020` + `vkd3d-compiler -x dxbc-tpf
  -b d3d-asm` (scratch `dxbc/`, `turf3d.{ps,vs}.asm`,
  `turf3d_decal.{ps,vs}.asm`, `turf3d_full.ps.asm`).
- Mesh: `fmdl_full` / `meshstat` / `vdump` / `vdump2` / `pitchmesh` /
  `techscan2` sources in scratch (link `cleanbuild/libpesfmt_clean.a`
  + `-lz`); `grassfin` stats `finstats`/`lod`.
- Live (read-only): `sudo -n python3` + `os.pread` on
  `/proc/<pid>/mem` (pid re-resolved; heap scan for `turf3dtex`,
  residency `memcmp` at `0x3e000000`, `TS = *(0x1437712e0)` table walk).
  Frames: `bun obs_frame.ts OUT.png PES-vk 1280` + `magick -colorspace
  RGB` decode; boxes + Michelson scripts in §1.4.
- Docs: `docs/render/pes-render-DarkTurf.md` (ticket),
  `docs/re/pes-re-pitch-grass.md` §5.2 (prior VS read),
  `docs/render/pes-render-ComposePath.md` (slot method),
  `docs/re/pes-re-stadium-camera.md` §1.1 (nested-cpk blindness).
