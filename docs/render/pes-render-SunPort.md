# SunPort: the sun radiance in PES's convention; H2/H4/H5 measured; the remaining gap is exposure, not the sun

> **Main note 22-10-26:** branch SunPort a8128f5, NOT merged: the sun radiance is PES-faithful (π·S, the Godot sRGB/lux mapping), but the turf frame worsened (3.5× → 4.8× PES) because GF's exposure pins at its −14 EV floor. Next: the exposure-floor measurement. Corrects pes-render-sun-radiance.md: "π·S removes 2.26×" is wrong (origin/main's sun was 0.72× PES radiance).

Branch `SunPort` on origin, head `a8128f5` (merge of origin/main `cc20625`; the three SunPort commits `8f00991` WIP, `32b45e3` the port, `971f6b4` the magnitude test). Gate not run (Main gates). Scratch frames and probe output are in the SunPort scratch dir (cap_*/frame00000149.png, h2/*/frame00000005.png).

## 0. Summary
1. The sun's radiance is now PES's: colour = .atsh chroma at unit length × π·S, S = TppAtmosphere.skyColorSunScale (0.2, parsed); Godot gets the colour sRGB-encoded and lux × π, because Godot's directional light is `decode_srgb(colour)·decode_srgb(albedo)·lux/π·NdotL` (H2, probe).
2. The task's premise is wrong: the origin/main sun was NOT 2.26× too bright. Its green radiance was 0.72× PES's (Godot's decode and /π). The "removes 2.26×" figure (sun-radiance §1.6, §3) assumed a linear Godot colour and an L∞ peak; the probe shows neither. Measured: the max-normalised port (peak π·S, Godot decodes it) darkened the turf to 1.21×, not to the 2.26× step the doc predicted.
3. H4 (ungrouped floodlights) is killed: the three ungrouped lights have no pixel effect. H5 (per-material) is killed as the sole cause: specular ≈ 9% and wrap ≈ 19% of the turf.
4. The frame excess is therefore not the sun's radiance. In every GF frame the auto-exposure sits at the rig floor (EV −13.96 against floor −14) while PES's live EV is −10.8…−11.1. Frame ratios here are exposure-confounded; the next measurement is open item 1 in §7.

## 1. The port (PES side, CONFIRMED)

- PES sun row: |cb3[1]| = I·π·S·0.998 (live, sun-radiance §1.6). The row's magnitude is the L2 norm of the colour, so the chroma has unit length. The shader has no 1/π (§1.4), and π is the MULSS at VA 0x140195526b / 0x14019552 8a (the float 3.14159274, equal to π in float).
- S: `TppAtmosphere.skyColorSunScale` = 0.2 in the stock st021 rig (VFS winner, sun-radiance §1.1). Parsed into `pes::Atmosphere::skyColorSunScale`; test `SkyColorSunScaleIsTheTppAtmosphereProperty` reads the fox2 property directly.
- .atsh colour at 15:00 is (1, 1, 0.354) after the max normalisation (origin/main). Not white, as sun-radiance §2 said.

## 2. What the code does now

- `pes::SunColour(atmo, atsh*, cloudy, h, m)` (src/pes/lighting.cpp): the .atsh colour divided by its L2 length, × π·skyColorSunScale. A null atsh is white at the same magnitude.
- gdext `PesLighting::setup`: `light_intensity_lux = sunLux·fade·π` (Godot's /π returns PES's I·π·S); `light_color = SunColour.linear_to_srgb()` (Godot decodes it back).
- Moon, floodlights, sky (`sun_intensity`, `star_intensity` use skyLightSunScale) unchanged.

## 3. H2: Godot directional light (probe, CONFIRMED)

Scratch probe (godot/scratch_probe_sunport/probe.gd, not committed): one white Lambert plane, straight-down DirectionalLight3D, no ambient/sky/fog/post, linear tonemap, centre pixel of frame 5.

| case | lux | albedo | colour | raw | linear |
|---|---|---|---|---|---|
| C | 1 | 1.0 | 1.0 | 0.6000 | 0.3186 |
| A | 4 | 1.0 | 1.0 | 1.0000 (clipped) | >1 |
| B | 4 | 1.0 | 0.5 | 0.5569 | 0.2705 |
| D | 4 | 0.5 | 1.0 | 0.5569 | 0.2705 |

- Linear in lux (C = A/4 before clipping). 1 lux gives 0.3186 (R), 0.3005 (G), 0.3140 (B) against 1/π = 0.3183: the mean is within 2%; the 6% G-to-R channel spread is not resolved (it is not part of the decode claim).
- Colour 0.5 and albedo 0.5 both give 0.214 = sRGB decode(0.5). B and D match each other, so both are decoded.
- Model: out = decode(albedo)·decode(colour)·lux/π·NdotL. Predicted B = 4·0.214/π = 0.2725 (measured 0.2705, within 8-bit error).

## 4. Frames (stadium 21, day, camera 7.94,19.135,37.07 → 0.022,-0.376,-0.926 fov 23.28, home 708 / away 760, span 15:00, frame 149; ref p12_dec.png; GF/PES linear p50 from measure.py)

| frame | sun | EV (frame 149) | frame-mean ratio | grass_a | grass_b | halfway R/G, B/G (PES 0.777, 0.369) |
|---|---|---|---|---|---|---|
| base (origin/main) cap_base_nopis | origin/main colour, lux 150000 | −13.96 | 2.71 | 3.54 | 3.51 | 0.743, 0.096 |
| max-normalised π·S, no encode (killed) cap_after_sunpi_ev | peak π·S, Godot decodes | −13.66 | 1.21 | 1.55 | 1.54 | 0.801, 0.084 |
| + ungrouped off (H4) cap_h4_skip | as above | −13.66 | 1.21 | 1.55 | 1.54 | identical |
| **final: L2 π·S, sRGB encode, lux·π** cap_after2_h2 | PES-faithful | −13.96 | 3.66 | 4.80 | 4.76 | 0.723, 0.263 |
| final, sun off (lux 0) cap_sunoff | off | −7.21 | 0.90 | 1.06 | 0.99 | 0.756, 0.650 |
| final + spec off (H5a) cap_h5a_nospec | SPECULAR=0 | not logged | 3.42 | 4.37 | 4.59 | 0.715, 0.261 |
| final + wrap off (H5b) cap_h5b_nowrap | NdotL only | not logged | 3.01 | 3.90 | 3.83 | 0.740, 0.289 |

PES reference (docs/render/pes-render-GradePort.md §4): grass_a 0.1322, grass_b 0.1062, halfway R/G 0.777, B/G 0.369, grass_a B/G 0.082. Frame mean PES 0.1507.

Turf B/G (grass_a): PES 0.082; base 0.011; final 0.036. Halfway B/G: PES 0.369; base 0.096; final 0.263. Both lines and turf move toward PES's blue, because the decoded origin/main sun was too dark and too yellow in blue (its B channel decoded to 0.105).

Note: the sun-off frame shows the sun is essentially all of GF's metered light (EV −13.96 with the sun, −7.21 without, a 2^6.75 change).

## 5. Hypotheses

- H2 Godot directional gain: CONFIRMED (§3). It is a 1/π Lambert and an sRGB decode of light colour and albedo, not a gain error. Ported (sun only).
- H4 floodlight GroupOn enabling ungrouped lights: KILLED. Three ungrouped lights are on (SL_Room_0000/0001/0002_DemoAdd, 20 000 lm each). Hiding them changes nothing: the H4 frame equals the after-port frame to 3 decimals (EV difference 2e-6). They are DemoAdd lights and do not reach the pitch at this pose.
  Also found: in the st021 file the 15:00 span enables only ReflectionProbe, so GF has all 144 grouped floodlights off at 15:00. This disagrees with pes-re-lighting §4.2 (13–16 h: + Light_Pitch_F), which may be st002's table. Not changed (open item 3).
- H5 per-material scale: KILLED as the sole cause. Specular is 9–10% of the turf (grass_a 4.80 → 4.37 spec off). Wrap/translucency is 19% (4.80 → 3.90 wrap off). Removing both leaves the diffuse albedo·light chain at ~3.9×.
- Killed earlier claims: (a) "π·S removes 2.26× of the sun" (sun-radiance §1.6, §3): the origin/main sun was 0.72× PES's radiance, not 2.26× too bright (see §0). (b) "The atsh colour at 15:00 is white": it is (1, 1, 0.354). (c) "Gain 10 at plugin+0x10c" (TurfColour §2.1): plugin+0x10c is the adaptation speed (pes-re-lighting §adaptation), not a gain.

## 6. Tests

- `build/tests/pes_lighting_test` (PES21_ROOT set): 11/11 on the merged tree `a8128f5` (after the rebuild of godotfootball and pes_lighting_test; `ninja -t deps` shows no `#deps 0` objects).
- New: `LightingSun.SunColourHasMagnitudePiTimesSkyColorSunScale` (synthetic .atsh chroma (2,2,1): length 3, peak 2), `LightingSun.SunColourWithoutAtshIsWhiteWithTheSameMagnitude`, `LightingRig.SkyColorSunScaleIsTheTppAtmosphereProperty` (install, relation to the fox2 property).
- Mutations (each applied, built, run, reverted by `git checkout`; clean rebuild 11/11 afterwards):
  - M1: parse line removed → `SkyColorSunScaleIsTheTppAtmosphereProperty` FAILS (1 vs 0.2).
  - M2: S dropped (scale = π) → `SunColourHasMagnitude…` and `SunColourWithoutAtsh…` FAIL.
  - M3: peak normalisation instead of L2 → both magnitude tests FAIL. (The first version of the magnitude test used a chroma that peak and L2 agree on, and passed under M3; it was strengthened to (2,2,1) before the rerun.)
- gdext: the Godot mapping (lux·π, sRGB encode) has no C++ unit test. Guarded by the probe (§3) and by the frames (§4).

## 7. Open items (numbered)

1. Exposure floor (the decisive one). GF's EV is −13.96 at the −14 floor in every frame; PES's live EV is −10.8…−11.1. The reference frame p12's EV is not recorded. Next: an E8-style capture with the final sun and floor −20 (GF's equilibrium EV), plus PES's EV at the reference frame (read-only, live). Until then the frame ratios above are exposure-confounded.
2. Sky S. GF's sky `sun_intensity` and `star_intensity` use `skyLightSunScale` (span values: 0.40 at 12:00, 0.30 at 13:00, 0.10 at 15:00, 0.02 at 19:00). The live PES moon slot cb4[3].w = 0.6283 = moonLux·π·0.2 (13:29 day frame) matches skyColorSunScale, not the 13:00 span's 0.30. Not changed. Needs the live sun slot of cb4 (Sky_Draw) to decide.
3. Floodlights at 15:00. The file's 15:00 span enables only ReflectionProbe, so GF's 144 grouped floodlights are off; pes-re-lighting §4.2 says Light_Pitch_F is on at 13–16 h. Read PES's live Light_Pitch_F state at 15:00 before any change.
4. Sun chroma. GF's sun is (1, 1, 0.354)·(unit) with B/G 0.354; the PES row's B/G is ≈0 (R==G). The row's direction comes from Sky_SunColor (not ported).
5. Other Godot lights. The moon and floodlight colours pass through the same decode (not changed; no PES colour-space evidence for them). Floodlight lumen → Godot conversion not probed.
6. Doc corrections for archiving: sun-radiance §1.6/§2/§3 ("removes 2.26×", "white atsh", "π·S on the colour"); TurfColour §2.1 ("Gain 10 at plugin+0x10c" is the adaptation speed).
