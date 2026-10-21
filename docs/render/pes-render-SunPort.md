# SunPort: sun radiance on PES's shader convention; sky scale; the 3.2-EV reference gap is NOT explained

> **Main note 22-10-26 (final):** branch SunPort 6923da2, to land with batch B. Frames vs PES p12: full-frame 2.34× → 1.04×, grass 3.54× → 1.54×, halfway B/G 0.096 → 0.245 (PES 0.369). The exposure law is PES's; the sun was 2.26× too strong (Godot's 1/π applies only to StandardMaterial3D, not to the PES materials' custom light() path). Open: a 3.2 EV gap to PES's documented EV (possibly a ~0.1 factor in the pitch shader), the sky/ambient blue fill.

Status (21-10-26). Branch `SunPort` on origin, head `6923da2` (`ls-remote` confirmed); it contains the earlier WIP `8f00991`, the port `32b45e3` (now superseded: its lux·π is wrong, see §3), the test `971f6b4`, the merge of origin/main `cc20625` as `a8128f5`, and the fix `6923da2`. Gate not run (Main gates). Frames are in the SunPort scratch dir (names below).

## 0. Verdicts

| # | Claim | Verdict | Evidence |
|---|---|---|---|
| 1 | Exposure law and range are PES's | CONFIRMED | §2 |
| 2 | The floor (−14) is the cause of the 3.5× turf excess | CONFIRMED (proximate); the floor is PES's own value | §2, frames F0–F2 |
| 3 | Godot's light is colour·lux/π (my H2 claim, `32b45e3`) | REFUTED for the custom light() path (true only for StandardMaterial3D) | §3 probes |
| 4 | GF's origin/main sun is 2.26× PES's (doc §1.6) | CONFIRMED on the custom path (I had said 0.72×: wrong) | §3 |
| 5 | The sun fix (lux·fade, π·S in colour) makes the sun radiance PES's | CONFIRMED by probes + sun-pass asm; committed `6923da2` | §3, F0→F1 |
| 6 | Sky sun/star scales are lux·π·skyColorSunScale (not skyLightSunScale) | CONFIRMED by live moon slot (0.6283 = π·0.2) and tests; committed | §4 |
| 7 | PES's EV at the reference frame | NOT recorded for p12; documented −10.82 / −11.12 used, as Main asked | §5 |
| 8 | GF equilibrium EV vs PES's: the 3.2-EV scene gap | NOT explained; candidates and exclusions in §5 | §5 |
| 9 | RGBM alpha (max(16a,1)) explains the gap | REFUTED in its simple form (sun pass stores 2^EV·cb3·NdotL, no /16) | §5 |
| 10 | Metering sub-rect explains it | NOT established (rect is caller-supplied; frames are uniform) | §5 |
| 11 | Items 2–4 | 2 done; 3 file-consistent, live unverified; 4 open | §6 |

## 1. What is on the branch

- `pes::SkyRadianceScales` (src/pes/lighting.cpp): sun = sunLux·sunFade·π·S, star = moonLux·π·S, with S = `TppAtmosphere.skyColorSunScale`. gdext uses it for `sun_intensity` / `star_intensity` (was skyLightSunScale).
- Sun block (src/gdext/pes_lighting.cpp): `light_intensity_lux = sunLux·fade` (was ×π); colour = unit .atsh chroma × π·S, sRGB-encoded for Godot (`pes::SunColour`).
- `pes::Atmosphere::skyColorSunScale` is parsed from TppAtmosphere (install value 0.2 on st021).
- Tests (tests/pes_lighting_test.cpp, 13/13 with PES21_ROOT): `LightingSky.SkyScalesAreLuxPiSkyColorScale`, `LightingRig.St021SkyScalesUseTheTppAtmosphereSkyColorScale`, `LightingRig.SkyColorSunScaleIsTheTppAtmosphereProperty`, `LightingSun.SunColourHasMagnitudePiTimesSkyColorSunScale`, `LightingSun.SunColourWithoutAtshIsWhiteWithTheSameMagnitude`.
- Mutations (applied, built, run, reverted): skyLightSunScale in SkyRadianceScales → both sky tests FAIL; parse line removed → install relation test FAILS (1 vs 0.2); S dropped from SunColour → magnitude tests FAIL; peak normalisation instead of L2 → magnitude tests FAIL. The sun lux convention (no π) has NO committed test: it is guarded by the probes (§3, not committed) and the frames (F0→F1).

## 2. Exposure (Main's question)

Law (CONFIRMED): PES's FUN_1416c7930 (pes-re-lighting Part C §1, OBS): EV0 = clamp(EV, min, max); Lc = Lmeas·2^−comp; tgt = clamp(EV0 + ½(log2(key+0.001) − log2(Lc+0.001)), min, max); step = (1−0.98^(60dt))·speed·min(|Δ|,0.5)·Δ; EV = (|step|<|Δ|) ? EV0+step : tgt. GF's `LAW` pass (godot/post/pes_post.glsl, lines 116–128) is the same, line for line; Lmeas is the arithmetic mean of max(Y·2^EV, 0.001) over the full frame (PES: the same over a sub-rect, see §5).

Parameters (CONFIRMED against live PES, docs): key 0.18, comp −0.33, max −5, speed 10 on the st021 15:00 span (GF's POSTPARAMS print matches these); min −14 (PES live min −14 in the reference match states; GF −14).

So no port of the law or range is needed. The floor is PES's own value; it bound in origin/main because GF's sun was 2.26× too strong on the custom path (plus the π error I had introduced, now reverted). Equilibrium check (floor −20 scratch runs, the same frame and pose):
- origin/main sun: pinned at −13.96 (F0).
- sun fix: EV trajectory −13.88 (f100), −13.95 (f150), −13.98 (f199); extrapolated asymptote −14.02 (F2). The floor −14 binds by ~0.02 EV (F1 vs F2 differ by 1–2%).
- Cross-check: the π error (lux·π) shifted the equilibrium by +1.59 EV in the measured run (−15.61 at f200 vs −14.02), against log2 π = 1.65 predicted.

## 3. The sun convention (CONFIRMED on the custom path; my H2 was wrong)

Probes (scratch, not committed): a white Lambert plane, straight-down DirectionalLight3D, linear tonemap, centre pixel, sRGB-decoded.
- StandardMaterial3D: lux 1 → 0.3186 (1/π); colour 0.5 → 0.214 of full; albedo 0.5 → 0.214.
- Custom spatial shader with light() of the same shape as pes_pitch (`DIFFUSE_LIGHT = NdotL·LIGHT_COLOR·ATTENUATION`): lux 0.25, albedo 1, colour 1 → 0.2502; colour 0.5 → 0.0529 (≈ 0.0535, decoded); lux 0.5 × albedo 0.5 → 0.2502 (product rule); lux 1 → clipped at 1.0 (no 1/π).

So the custom path gives albedo·decode(colour)·lux·NdotL. PES's shaders give albedo·cb3[1]·NdotL with cb3[1] = I·π·S·Tn·chroma (live row, sun-radiance §1.6), and the SSLighting sun pass has no constant in the diffuse term (SSLighting_Sun_NoShadow.ps.asm lines 106–111: r2.w = max_sat(mad(r2.w, r5.y, NdotL)...), o0 = cb2[23].z·cb3[1]·r2.w; no literal). So lux = sunLux·fade and colour = π·S·chroma is the faithful port; the earlier ×π on lux was wrong by π (1.65 EV), and the origin/main 2.26× was right.

Frames (same stadium 21, pose, 15:00 span):
- F0 origin/main (`cap_base_nopis/frame00000149.png`): full-frame mean 2.34× PES, rows 15–92% 2.71×, grass_a 3.54×, giraffe 1.38×, halfway 3.38×; halfway B/G 0.096 (PES 0.369); grass B/G 0.011 (PES 0.082).
- F1 sun fix (`cap_fix_lux/frame00000199.png`): 1.03× / 1.19×, grass_a 1.54×, giraffe 0.61×, halfway 1.46×; halfway B/G 0.241; grass B/G 0.037.
- F2 sun fix, floor −20 (`cap_fix_sky_floor20/frame00000199.png`): 1.02× / 1.18×, grass_a 1.52×, giraffe 0.60×, halfway 1.44×.
- F3 sun + sky, committed build, floor −14 (`cap_sun_sky/frame00000199.png`): 1.04× / 1.20×, grass_a 1.54×, giraffe 0.62×, halfway 1.47×, top band 0.0152 (F1 0.0139): the sky port changes only the top band (1.2% of pixels changed).
- E8 (the π-wrong port, floor −20, not committed; `cap_e8_floor20`): equilibrium frame 1.02× / 1.19× — the same as F2 within 1%, as expected: at equilibrium the output is key-normalised, so a global scale error is invisible in the frame and visible only in the EV.

## 4. Sky (item 2, done)

The sky shader's sun term is `L = inscatter·sun_intensity` and the airglow term `star_intensity·…` (godot/shaders/pes_sky.gdshader). PES's moon slot in a day frame reads cb4[3].w = 0.6283 = moonLux·π·0.2 (sun-radiance §1.6: live), i.e., S = skyColorSunScale; the span's skyLightSunScale (0.1 at 15:00, 0.3 at 13:00) is not in it. GF's sun_intensity doubles (47 124 → 94 248 at 15:00) and star_intensity follows. Frame effect: F1 → F3 (above). The ambient (AMBIENT_SOURCE_SKY) also follows the sky; its share of the metered scene is small (sun-off run: EV −7.2 vs −14.0).

## 5. The 3.2-EV gap: what is established, what is excluded, what is open

Target (Main): GF equilibrium EV −14.02 against PES's documented −10.82 (probe-1) / −11.12 (later): gap 2.9–3.2 EV (7.5–9×).

Reference values used as asked. PES's EV for the p12 frame itself was never recorded. The live match resumed by the owner is NOT the p12 match: it is /biz/ vs /xs/ (4090, 45+123', snowy stadium, no giraffe; not stadium 21). Its viewport reads EV −11.77, min −13, max −9, comp +0.33, speed 30, and the bypass word vp+0x650 has low nibble 2 (documented match states read 0; with 2 the ×0.25 of the stub is skipped), so its EV is not comparable to the reference without that correction. The menu read earlier (EV −11.52) has the same bypass.

Live sun row (struct array 0x13321440 + k·0x140, slots 0/16/32; fresh mapping validated by R = G to 6e-7): R = G = 30 987, B 3–24, |row| 43 823, against 66 526 at the earlier E-A read (0.66×). The sun magnitude therefore moves about 0.6 EV with the moment; it does not explain 3 EV.

Excluded (with evidence):
- Floor binding: now ≈0.02 EV (F1 vs F2).
- Sun term: fixed; the sun pass diffuse has no hidden constant (asm, §3).
- Composition alpha: DeferredRendering.ps.asm samples only .xyz of t8/t9/t10 and writes o0.w = 1; the sun pass writes o0.w = 1.0 (SSLighting_Sun_NoShadow.ps.asm line 137; SSLighting_Sun.ps.asm line 270) and stores 2^EV·cb3·NdotL with no /16. So the RGBM encoding (max(16a,1), debug view) does not divide the stored sun light; the "16× = 4 EV" model is refuted in its simple form. (Open detail: Tonemap_Lum's 32a² term would be 32 for a=1 pixels, so the metered alpha is not the sun-pass alpha; not resolved.)
- Sunlit fraction: player shadows are visible on the PES pitch, so the sun reaches it; the column-median profile across the pitch is flat in both frames (p95/p5 1.28 PES, 1.27 GF).
- Black top band: GF's top 15% is black (0.014 mean; PES 0.154); that makes GF's meter lower, i.e., it pushes GF's EV up, the wrong sign.
- Sun elevation: a 2× NdotL change is 1 EV at most for plausible elevations; not established either way.
- Metering sub-rect: FUN_141805840 (Tonemap_Lum's cb5[12] upload, Ghidra bridge, one request) computes the rect as (0.5/W + p[0x8c], 0.5/H + p[0x90], p[0x94], p[0x98]), clamped to the unit square, where p = param_3 is supplied by the caller. The live object at 0xb345470 reads zeros at those offsets, so the caller's rect is unresolved. Frames are uniform (§5 profile), which bounds any sub-rect effect to a few tenths of an EV.

Open candidates (INF):
- (a) A PES-side absolute factor of about 0.1 in the pitch albedo·diffuse chain (1/π² = 0.101 and 1/9 = 0.111 both fit the 3.2 EV; 1/(4π) = 0.080 overshoots by ~0.5 EV). The per-material pattern (grass 14× and giraffe 5.6× in absolute radiance, from the EV) says it is not a uniform sun factor. Decision needed: decode the pitch PS's ALBEDO and diffuse chain for an absolute constant (asm: docs/render/pes-render-*, B4Grass pes3ddf_pitch_2020.ps).
- (b) The reference moment: the documented EVs come from states whose sun magnitudes were not recorded with them; the owner resuming BNT vs W at the probe-1 moment would settle it (read cb3 and EV in the same moment).

## 6. Items 3 and 4

- Item 3 (floodlights at 15:00): the st021 file's 15:00 span enables ReflectionProbe only, so GF's grouped pitch floodlights are off (consistent with the file). Live verification needs a 15:00 match; the live match is in another span. The three ungrouped `SL_Room_*_DemoAdd` lights are on in GF (20 000 lm each); no pitch effect (H4 frames identical). PES's rule for ungrouped lights is unverified.
- Item 4 (sun chroma): PES's direct-sun row is R = G with B/R ≈ 0.0006 (live now and E-A); GF's colour is the .atsh colour (B/R 0.35 at 15:00). Porting the chroma alone would lower GF's blue further, and GF's blue is already low on lit surfaces (grass B/G 0.037 vs PES 0.082; halfway 0.24 vs 0.37; giraffe 0.18 vs 0.37). So the blue deficit is in the sky/ambient fill, not the direct sun. Not ported; open.

## 7. Verification run

- `pes_lighting_test` 13/13 on `6923da2` (PES21_ROOT set; no `#deps 0`).
- Frames F0–F3, E8, probes (§3), live reads (viewport, sun row, live frame), one Ghidra bridge request (FUN_141805840), and the sun-pass and composition asm reads listed above.
- Not run: the full gate (Main gates); no smoke.
- Probe scripts are scratch (not committed); the convention has no committed test (§1).

## 8. Open items (numbered)

1. The 3.2-EV gap (§5): decode the pitch PS ALBEDO/diffuse chain for an absolute ~0.1 factor (a), or record the EV with the sun row at the reference moment (b).
2. Tonemap_Lum's rect: identify the caller of FUN_141805840 and its param_3 values (one or two bridge requests).
3. The metered alpha: which buffer Tonemap_Lum reads and its alpha (the 32a² term).
4. Blue deficit: sky/ambient fill (item 4).
5. PES's ungrouped-light enable rule (DemoAdd lights; item 3).
6. Floodlight and other standard-light paths: the custom path has no 1/π; Godot's spot/omni lumen conversion may differ from PES by π or 4π. Not checked (matters for night spans).
7. Live match: the current viewport reads vp+0x650 nibble 2 (×0.25 skipped) with the /biz/ match; confirm the bypass state before using live EVs.
8. The top black band (GF stands/sky) is a separate defect; it biases GF's meter.
