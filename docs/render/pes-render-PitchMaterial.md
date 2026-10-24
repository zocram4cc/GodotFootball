# PES pitch material vs GF: the shader matches — the 2× red is the post WB path, the 2× luma is not in the shader

Decode of the pitch colour path at the instruction level, per-term GF comparison with numeric effects, and the exact port spec. Read-only work: repo at `<workdrive>/GodotFootball`, install VFS at `<workdrive>/PES21`, scratch at `<workdrive>/gf-Pitchmat-scratch` (all outputs with absolute paths there; nothing written to the repo).

Status: shader terms CONFIRMED matching except one real-but-negligible tangent-frame divergence (T1). The ~2× red/blue chroma excess is CONFIRMED as the post white-balance path (not the pitch shader). The ~2× luminance excess is CONFIRMED NOT to be in the pitch shader's math; decomposed below with the open remainder.

## 0. Constants and their st021 install values (all read by script, pasted)

`cb4` (material): `build/apps/pesfmt_dump fmdl 'Asset/model/bg/st021/pitch/#Win/pitch_st021.fpk'`, stock VFS winner:

```
exp1_pitch_mat [pes3DDF_Pitch2020 / pes_3ddf_pitch2020]
  MatParamIndex_0 = 205 | AOParamX/Y/Z/W = 1 / 0.1 / 1.5 / 0.85
  RepairColor = 0.312655 0.402 0 1 | CameraVectorBlend = 0 | NormalFlatness = 0.5
  WithersParameter0..5 = 0.78, 0.78, 0.961, 0.098, 0.071, 0.039
  SrmParameter0..5 = 1, 0.239, 0.196, 0, 0.761, 0.196
  EmissiveParam0/1/2 = 0.05 / 0.1 / 17.094 | URepeat_UV = 32, VRepeat_UV = 24
```

GF `pes_pitch.gdshader` defaults are identical on every lane (lines 20–43: AO 1/0.1/1.5/0.85, Repair (0.312655, 0.402, 0), Withers 0.78/0.78/0.961/0.098/0.071/0.039, Srm 1/0.239/0.196/0/0.761/0.196, Emissive 0.05/0.1/17.094, U/V 32/24). At runtime `pes_model.cpp` line 68 sets them from the same fmdl, so runtime == these values by construction. (Differs from the st002 doc row only in W2 0.961 vs 0.96 and U/V 32/24 vs 24/16.)

Textures (VFS winner; mod `download/4cc_06_gameplay.cpk` overrides only `edit/pitch` grains, not the match turf):

| Slot | File (stock) | Format / size | Decoded stats (CONFIRMED) |
|---|---|---|---|
| t0 `pitch_alp` (st021) | `st021/.../pitch_alp.ftex` | DXT5 4096×2048 | on-pitch 72%: R p50 0.502, G p50 0.063, B ∈ {0,255}, A ≈ 0 |
| t3 `turf_bsm_alp_rgba32` (common) | `.../turf_bsm_alp_rgba32.ftex` | RGBA8 1024² | RGB mean (65.3, 85.1, 11.9), A mean 97.7/255 |
| t7 `pitch_grain_alp` (st021) | `st021/.../pitch_grain_alp.ftex` | BC7 2048×1024, 1 mip | RGB (127, 128, 210), **A max 10/255, mean 0.05** |
| t5 `grass_slopetable_alp` (common) | 128² | DXT5 | RGBA mean (0.521, 0.847, 0.737, 0.381) |
| t4 `pitch_detail` (st021) | DXT1 2048×1024 | RGB mean (135.7, 136.4, 255) | slope ±0.36 |
| t1 `turf_nrm` (common) | `turf_nrm.ftex` | DXT5 512² | — |
| line maps (common) | `line_alp/nrm/srm/mtl.ftex` | DXT5/DXT5/DXT1/DXT1 | alp RGB = (208, 211, 208) const, A 0–135; srm R/G mean 182/182, B = 0, A = 255; nrm R = 255 const; mtl all 0 |

Note: st021 grain alpha ≈ 0 (vs st002's 16.3/255) — the repair-blend weight is ~0 on this stadium (install fact, not a decode error; checked against the VFS st002 file which decodes to A mean 16.3).

Lighting at 15:00 (GF's own parser on stock VFS, `atmo_probe`: compile `lighting.cpp` C++20 + `libpesfmt.a`):

```
skyColorSunScale=0.2 daySkyAmbientScale=0.4 skyLightLuminanceScale=1
span15: sunLux=150000 skyLightSunScale=0.1 start=15:00
atsh15 sun=(131008,131008,131008)   // saturated equal channels (AtshHalf: exp-31 → ±131008)
sunColour=(0.36276,0.36276,0.36276) // π·0.2/√3 — WHITE
toSun=(0.251948,0.390881,-0.885288) // NdotL ≈ 0.39 for an up normal
```

15:00 SH row (drives GF `ambient_sh9_*`): sh0 = (12020 ×3), all nine rows achromatic (R=G=B). Sun chroma WHITE, sky SH WHITE — neither adds red (CONFIRMED).

`cb2` reads in the PS: `cb2[10]` sun dir (normalize in asm 56–57), `cb2[20]` camera pos, `cb2[8]/[9]` view basis for the normal encode, `cb0[0].w` dither scale, `cb0[2].xy` velocity scale. Values are per-frame (camera/sun), not stadium constants; the stadium-side values are the tables above.

## 1. Full colour path (asm refs = `dxbc/pes3ddf_pitch_2020.ps.dxbc`, `vkd3d-compiler 1.19`, identical to B4 `asm/pes3ddf_pitch_2020.ps.asm`)

Swizzle rule used throughout (verified against the 315-pixel numeric run in `pes-re-pitch-grass.md` §3): source swizzle names destination lanes xyzw in order; the write mask selects absolute lanes.

- **Visibility (33–43):** dither `t15` vs `pitch_alp.b` + mesh fade. GF 100–102 thresholds `pitchRgb.b` at 0.5 (DEVIATION 4). Edge texels only. NEGLIGIBLE.
- **Grain frame (44–55):** `ng = normalize(2·(t7.r, t7.b, t7.g)−1)` (.xzy, asm 45). GF 111 identical. CONFIRMED.
- **Tangent T1 (49–55):** asm computes `T1 = normalize(nx·ny, −(nx²+nz²), ny·nz)` = negated Gram–Schmidt of +Y against ng (derived lane-by-lane, verified numerically: PES T1 ⟂ ng to 1e-17). GF 112 `T1 = normalize(ng.y·ng − UP)`. **DIVERGENCE (CONFIRMED, real).** Effect: slope-table v-coordinate 0.540 (PES) vs 0.389 (GF) at broadcast V; slope GREEN is flat there (0.827 both) so `spend` is unchanged; slope RED moves 0.32 → 0.51, which feeds only `GB2.y` specular (asm 176). Specular is ~1e-3 of the frame (X4 test: SPECULAR=0 identical to 4 d.p.). **Albedo effect: 0. Port: fix (one line, §3).**
- **View/CameraVectorBlend (56–66):** blend is 0 on st021; GF 106–108 skips it. CONFIRMED no-op.
- **Slope LUT (71–75):** two samples, same UV; spend uses GREEN (asm 75 → `t5.y`), spec uses RED/BLUE (asm 73). GF 115/133/180 same lanes. CONFIRMED.
- **Height/density (71, 76–79):** `Qt = V.y`, `Pw = 2^(h·0.05)`. GF 118–120 same. CONFIRMED.
- **Wither/AO chain (83–96):** `lin = a^0.45454568`, `v = lin·pitchR`, `aoy = v^0.1`, `aoz = v^1.5`, `em1 = v^0.1`, `em2 = v^17.094`, `wither = 1−(Qt(aoz−aoy)+aoy)`, `match = 1−wither·0.85`, `spend = 1−slopeG·match`. GF 124–133 identical (`pow(a,1/2.2)` vs `0.45454568`: same to 5e-7). CONFIRMED. Nominal broadcast texel: wither 0.537, match 0.543, spend 0.551.
- **Albedo (97–112):** `c = 1−(1−base)·spend`; `repaired = base + grainA·(RC−base)`; `curve = sat(base·(0.78,0.78,0.961)+(0.098,0.071,0.039))`; `tint = curve−repaired`; `a = max(pitchA, maskR)`; `gW = max(pitchG, maskG)`; `y = gW·tint+repaired`; `n = c·y`; `m2 = 1−sat(maskB·0.1)`; `ALBEDO = sat(a·(0.8−n·m2) + n·m2)`. GF 142–154 identical lane-for-lane (re-derived every swizzle: 98 `.xyw`, 102 `.yz`→(W1,W2)/(W4,W5), 105 `.xy`→(pitchA,maskR)/(pitchG,maskG), 106–107, 112). **CONFIRMED, zero divergence.** Mask unbound → 0 both sides (D3D11 unbound SRV reads 0; GF `hint_default_black` rgb = 0; only `.rgb` is read). On st021 `a ≈ 0` so `ALBEDO = sat(t)`.
- **Normal (114–143):** `n1 = (2·(t1.r·t1.a, t1.g)−1, sqrt)`; detail flatten ×0.5; `+z`; `sd`, `qx/qy`; `(nx+qx, ny+qy, sd−nz)` clamped in z; normalize; mix toward +Y by `mask.b` (= 0 → identity). GF 160–172 identical (the apparent y/z swap at GF 169 vs asm 131 resolves: both produce `(nx+qx, max(sd−nz,0), ny+qy)`). **CONFIRMED.**
- **GB2 (162–178 + VS fog 166):** `S0' = sat(lin·1+0)`, `S1' = sat(lin·0.239+0.761)`, `tr = sat(lin·0.196+0.196)`, `T = m3·tr`, `GB2.y = m4·slopeR·match·(S0'+o7.z)`, `GB2.x = S1'+o7.w`, `GB2.z = 0`, `GB2.w = slopeB·r6z·lin`. GF 177–184 identical incl. the PitchV7 `pes_v7` terms. **CONFIRMED.**
- **Light (ID-3, `SSLighting_Sun_NoShadow` 50–53/79–81):** `diffuse = lightColor·max_sat(NdotL+T(1−NdotL²), T)` (wrapW = 1), `out = GB0·diffuse + spec`, spec = `lightColor·F·D·Vis·sat(NdotL)·0.25·GB2.y·(0.9,1,0.8)` (GGX no 1/π, F0 0.04). GF `light()` 198–215 identical formula (Schlick for the unfound `g_lut1` — INF medium, diffuse-unaffecting). **CONFIRMED.**
- **Decal lines (`fox3ddf_ggx_decal`, GrModel pack, disassembled identically):** dither+fade discard (GF: blend only, DEVIATION 1 — mean-equivalent); albedo raw (GF `srgb_to_linear`, same); normal `((t1.w·t1.x, t1.y)·2−1)` = GF `pes_normal` (same); `rough = sqrt(srm.g²+σ²)`, `spec = srm.r`, `metal = mtl.r = 0`, ID 1, weight `base.a` = GF `ALPHA`. **CONFIRMED, zero divergence.** Line albedo is (208, 211, 208) → linear (0.633, 0.648, 0.633), R/G 0.977.

Numeric anchor (nominal broadcast texel, script `albedo_model.txt`): PES-math albedo = (0.0264, 0.0455, 0.0018), Y = 0.038, R/G = 0.58, B/G = 0.04 — against PES frame grass_b (0.074, 0.126, 0.007), R/G 0.585, B/G 0.052. The albedo's chroma already matches PES's frame chroma: the post/light must be ~achromatic, which is exactly what the WB finding below breaks in GF.

## 2. Per-term attribution (at PES's exposure)

| Term (brief's list) | GF (`pes_pitch.gdshader`) | Numeric effect on grass_b |
|---|---|---|
| Withers 3..5 (offsets 0.098/0.071/0.039) + gains | lines 144–146, exact | 0 (match; curve = (0.140, 0.142, 0.043) both) |
| `repaired` / `gW` | lines 143/149–150, exact | 0 (match; on st021 grain A ≈ 0.0002 → repaired ≈ base to <0.1%) |
| wither/AO chain (pitch R, asm 86–96) | lines 125–133, exact | 0 (match; spend 0.551 both). A missing sRGB decode on pitch_alp would give only +13% luma, not 2× (bounded) |
| decal lines (`pes_ggx_decal`) | exact (§1) | 0 (match; line factor ≈ grass factor ⇒ global, not decal) |
| sun-vs-sky mix | sky/sun chroma both white (§0) | 0 chroma from either; GF sky ambient is 0.064 of sun-grass vs PES candidates 0.108–0.269 (weaker, wrong direction for an excess) |
| T1 frame (GF 112) | **wrong** (negated-GS vs `ng.y·ng−UP`) | 0 on albedo; +60% on negligible specular. Fix in §3 |
| texture decode (sRGB) | single decode both sides | 0 (a missing decode would be ~5× + R/G shift — not observed; double decode would darken — not observed) |
| **chroma (R/G 1.29 vs 0.585, B/G 0.17 vs 0.052)** | **post WB, not the shader**: GF applies Sᵀ. `Sᵀ·albedo` = R/G **1.421**, B/G **0.172** — reproduces GF's measured (1.29, 0.17) to the light-mix. PES's frame chroma = the un-WB'd albedo ratio (0.58, 0.04). **The whole ~2× red and ~3× blue excess is the WB.** Luminance effect of WB: −4% (0.0395 → 0.0381). **Not the luma cause.** | explains chroma fully |
| **luminance (~2×; 1.54× post-SunPort)** | no term left in the shader (all match, spec negligible, ambient weaker) | decomposed: frame-mean global ~1.7× (E9) with EV matched ⇒ metered-mean content (stands banding/dark band per ExposureMatch §7.7, INF) + material residual ~1.2× (OPEN: sun level/NdotL hour or shadow state) |

Notes: (1) Neither WB orientation reproduces PES for both boxes — S gives grass (0.640, 0.039) ≈ PES (0.585, 0.052), Sᵀ gives (1.421, 0.172) ≈ GF (1.29, 0.17). The "rows fix" looked right only because it mimicked PES's no-WB live path. This corroborates TurfColour's OPEN item 1 (live matches bind the CubeLUT stub path, WB not applied). (2) St021-specific: repair weight ≈ 0 (grain A max 10/255) — `RepairColor` is inert here; do not tune it.

## 3. Port spec (exact GF lines)

1. **T1 frame — `godot/shaders/pes_pitch.gdshader:112`.** Replace `vec3 T1 = normalize(ng.y * ng - UP);` with the asm 49–55 frame:
   ```glsl
   // PS asm 49-55: T = -(Gram-Schmidt of +Y against ng), not ng.y*ng-UP.
   vec3 T1 = normalize(vec3(ng.x * ng.y, -(ng.x * ng.x + ng.z * ng.z), ng.y * ng.z));
   ```
   Guard `ng.y ≈ ±1` (degenerate, normalize of ~0): fall back to `vec3(1,0,0)` when `length < 1e-6`. Expected effect: slope v-coordinate 0.389 → 0.540 at broadcast V; albedo unchanged (GREEN flat); specular GB2.y −35% of a ~1e-3 term. Status: CONFIRMED divergence, negligible frame effect.
2. **Everything else in `pes_pitch.gdshader` and `pes_ggx_decal.gdshader`: NO CHANGE.** Each term is verified lane-identical above; the 2× is not in these files.
3. **Do NOT "fix" by:** touching Withers/Repair/AO/Srm values (install values confirmed, GF matches); touching the decode (single-decode consistent); re-applying the rows WB (refuted, asm+CPU upload confirm Sᵀ); scaling the sun (SunPort landed, CONFIRMED).
4. **OPEN (not this shader, next actions):** (a) composition path — read the live technique slot (`plugin`/`vp` chain per TurfColour §5.1) to confirm matches skip film+WB for the CubeLUT path; that decides whether GF's post must drop/keep WB for matches. (b) luminance residual — capture the matched-pose frame on current main (SunPort landed) with the settled EV and re-split global vs material; check stadium shadow state and the match hour (ExposureMatch §6.2) before touching any light scale.

## 4. Status summary

- CONFIRMED: full PS colour path (asm 33–186) matches GF term-by-term; all cb4 constants + texture formats/values + atsh sun/SH + atmosphere/span install values (tables §0); T1 divergence (negligible); WB Sᵀ as the entire chroma excess (R/G 1.42/1.29, B/G 0.172/0.17 predicted/measured); specular negligible (X4); mask-unbound equivalence; decal-line equivalence.
- INF: D3D11 unbound-SRV reads 0 (standard behaviour); Schlick for `g_lut1`; global-luma metering-content mechanism (stands/sky) — needs the current-main frame.
- OPEN: live composition path (CubeLUT vs film+WB); hour/shadow state for the luma residual; `cb2` per-frame values (not stadium constants).

## 5. Reproduce

- Shaders: `pesfmt_dump cat shaders/dx11/PesShaders_dx11.fsop` + `fsop_extract.py … pitch_2020` + `vkd3d-compiler -x dxbc-tpf -b d3d-asm` (scratch `dxbc/`, `grdxbc/`, `pitch2020.ps.asm`, `ggx_decal.ps.asm`); RDEF via `rdef_dump.py`.
- fmdl: `pesfmt_dump fmdl 'Asset/model/bg/st021/pitch/#Win/pitch_st021.fpk'` (values §0).
- Textures: `pesfmt_dump cat` the §0 ftex paths → `ftex.py::to_dds` → PNG stats (§0 tables, `pitch_alp.png`, `pitch_grain_alp.png`, `turf_bsm_alp_rgba32.png`, `grass_slopetable_alp.png`).
- atsh: `light_st021_af.fpk` member `light_st009_af.atsh`, slots/times/SH dump (script in session); atmosphere/span/sun via `atmo_probe.cpp` (needs `lighting.cpp` + `libpesfmt.a`, C++20).
- Numerics: `albedo_model.txt`, `swizzle_check.txt`, `gb2_check*.txt`, `grain_check.txt`, `t1_verify.txt`, `normal_check.txt`, `fullpath.txt` in scratch.
