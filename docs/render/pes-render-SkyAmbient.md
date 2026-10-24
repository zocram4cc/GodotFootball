# PES sky ambient vs GF: the cb7 writer, live values, and why the 1.87x split is not sky (SkyAmbientRE, 2026-10-24)

Ticket: GF grass ~1.31x PES, paint ~0.70x PES at p12 (split 1.87x, stable f200-f399); the split moves with the sun:sky balance; sun math/constants, post path, hour, shadow, metering, LACC and SunPort's scale are eliminated. Open leg: the sky ambient energy (PES `cb7` writer undecoded; GF `daySkyAmbientScale` ponytail) and the live sun row's R=G,B~0 shape vs GF white.

Read-only on the repo and install. Image `~/ghidra-projects/scratch/Unpacker/pes_full.bin` (VA = offset + 0x140000000); Ghidra bridge one-at-a-time; live PES pid 2510857 `pread` only. Scratch: `<workdrive>/gf-SkyAmbientRE-scratch/` (absolute paths; probes, asm, scripts).

## 0. Script-read constant table (floats read from the image, offset = VA - 0x140000000; operand VA paired)

| VA | value | used at |
|---|---|---|
| 0x142578480 | 3.14159274 | `0x140195526b` / `0x140195528a` MULSS (sun/moon xπ; re-read this pass, matches sun-radiance §0) |
| 0x142a26568..0x142a26580 | 0.282094806, 0.315391570, 0.488602519, 0.546274245, 1.09254849, -0.488602519, -1.09254849, 0.0 | SH9 basis constants (pes_sky.gdshader:39-46 cites them; dump confirmed this pass) |
| 0x142a79180 | "daySkyAmbientScale" string | property registration LEA `0x14193fa6e` (code xref scan this pass) |
| 0x142a792c8 | "skyLightLuminanceScale" string | property registration LEA `0x14193fecc` (code xref scan this pass) |
| 0x142a218a7 | "BuildSphericalHarmonicsSphereMaps" / "SelectionSHLight" | bake job + SH-source selector names (string context dump this pass) |

No new immediates are ported (the spec below ports file values through GF's existing parser, per the dt18 rule).

## 1. The cb7 writer: what it is, its inputs, status INF(high)

- **The writer is the `BuildSphericalHarmonicsSphereMaps` Fox DataJob** (job name at `0x142a218a7`; `DataJobPacket@UBuildSphericalHarmonicsSphereMapsParameters` RTTI at `0x143679d95`; job class at `0x14367a7c8`). It is an async GPU bake, not one CPU store — that is why no single "cb7 store" function was found in 20+ bridge requests. Status INF(high): the job name, its params type, and the consumer shader below agree; the job's dispatch callgraph is OPEN.
- **Consumer (CONFIRMED, disassembled this pass from GrModelShaders_dx11.fsop):** `SH_SphereMap.ps` (60 lines) evaluates `E = n4'M_R,G,B n4 * cb2[23].z` (+ `w`-gated second set `cb7[16..27]`), i.e. cb7 holds Ramamoorthi-Hanrahan 4x4 irradiance matrices = 3-band 9-coeff SH, matching docs/re/pes-re-lighting.md §5. `SSLighting_SH_SkyLight.ps` (48 lines) samples the baked atlas (`max(tex,0)`); `SSLighting_SH_SkyLight_Encode.ps` (73 lines) adds the Gaussian pitch-centre boost (`cb5[13..15]`).
- **Inputs (file-grounded):** the SH source selector string **`SelectionSHLight`** sits adjacent to the job name (`0x142a218b8`); the span's `.atsh` SH9 row at the active time/weather is the documented source (lighting doc §5, and `COLOR = sh9(atsh[t]) * skyLightLuminanceScale * exposure` for the cubemap pass). The `.atsh` layout (40 times x 2 weathers x 30 fp16) is CONFIRMED by GF's parser against stock (this pass: st021 aliases st009's file, times step 2160 s, §8).
- **What remains OPEN:** the job's exact input scale (Slum inside the bake vs as cubemap gain — GF does the latter, see §4) and the dispatch hour rule. **No GF code change is proposed for the writer**: GF's `AMBIENT_SOURCE_SKY` + cubemap path is the same idea (bake once per span, convolve at use), and §4 shows the energy already matches PES's documented mapping.

## 2. Live values (read-only, st018 dusk match — NOT the p12 scene; §5 maps to st021 15:00)

| Item | live value (2026-10-24) | method |
|---|---|---|
| EV / min / max / comp | -11.306 / -13 / -9 / +0.33 | `pread` VP `0x2195F10+0x4C4/4C8/4CC/4D0` |
| Lmeas | 0.22465 | `VP+0x410` |
| LACC byte | 0x00 | `pread 0x14373e951` (lit-frame meter, matches Lacc doc) |
| technique `TS+0x74` | 0xc40002 (CubeLut grading branch) | ComposePath L2 pattern |
| sun row `0x13321440+0x00` | (170942.8, 170942.8, 14.56, 0) | `fvec`, R==G to 6 digits, B independent |
| row magnitude | 241749.6 = I·π·S·0.513 (S=1.0 st018) | I=150000 file |
| sun dir `+0x50` | (0.0130,-0.3592,-0.9332), frozen | to-sun ≈ 21° elevation evening sun |
| stadium | st018 (dusk) | heap `light_st018_af*` strings (scan, 83 s); rig file values §5 |

- The R==G,B~0 shape is STABLE across sessions (E-A Oct-19: 66526,66526,~15; SunLuma f-series; today 170943,170943,14.6) and its magnitude tracks the sun (Tn 0.998 at noon → 0.513 at 21° elevation). B is INDEPENDENT of R (0-24 while R drifts ×2.6), so B is a separate small term, not a colour channel.
- **Why R==G (INF, medium):** the `Sky_SunColor` LUT's columns 2-3 are `S/luma` chromaticities (disassembly re-read this pass: `div r7/r4, luma` then `ftoi` column select at line 545). A yellow (R=G,B=0) seed normalises to (1.078,1.078,0) = R==G,B=0. The noon file light colour at 15:00 is saturated white (131008×3, exponent-31 fp16), whose normalised chroma is grey — so the live row is NOT the file light colour normalised; it is the LUT-path chromaticity seed (dusk-yellow) with B killed upstream. The seeding column + B-killer stay OPEN (needs the D3D readback, same as PortAudit4 A1).

## 3. GF's sun colour vs PES, and the predicted effect (numbers, not adjectives)

- GF `SunColour` (lighting.cpp:693): file chroma normalised × π·S. At st021 15:00 the file chroma is saturated white → GF (0.36276,×3), |.| = π·0.2 = 0.6283. PES E-A row (66526,66526,~15): GF/PES = **0.818× in R,G** (GF darker — wrong direction for a grass excess), B ~3600× (weight 0.0721 × grass-B albedo 0.0018 → +0.3% on grass-Y; negligible).
- Q3 sweep re-analysis: the opposite motion (grass ×0.74, paint ×1.35) is CONFIRMED dirty — the runs never reached matched EV (f300→f399 still 0.988×/100 frames; predicted matched-EV light-ratio is 0.775×, measured 0.55×). Its DIRECTION (paint up, grass down when the sun is cut) is just the blue ambient taking over: ambient-only grass/paint = 0.0577 vs sun-only 0.0595. No new physics; the sweep needs no follow-up beyond what SunLuma §7.8 already says.
- **Prediction if GF ported the R==G shape at the same magnitude:** split 1.010× vs PES (moves AWAY from PES by 1%). **Do NOT port the live shape** (live-derived, not a file constant; PortAudit4 A1 concurs). At st021 15:00 the file ground truth is white anyway (§5: both sides white, no divergence).

## 4. GF's ambient vs PES Sloan matrices at st021 15:00 (the 1.87× verdict)

Sloan R-H matrices built from the stock file SH row (GF parser output, S=0.2/dayAmb=0.4/Slum=1.0):

- File SH15 G-channel → E(up) G = 12145.7 (R 9945.8, B 17417.0; B/G 1.43, R/G 0.82 — mildly blue/red-poor).
- GF cubemap zenith × Slum(1.0): (3284.5, 4665.8, 7991.8); GF exposes it with `ambient_scale = Slum` (1.0) and `ambient_light_energy = dayAmb` (0.4) — exactly the doc §5 mapping (`sh9 × Slum`, energy ↔ dayAmb). GF ambient G = 5862 = **0.48× PES Sloan**.
- Direction agrees with LumaResidual's post-chain bounds (GF 0.064 of sun-grass vs PES 0.108–0.269 → GF/PES 0.24–0.59; pre-exposure Sloan gives 0.48 — same direction, same size).
- **Light-level split test** (grass (0.0264,0.0455,0.0018) vs paint (0.633,0.648,0.633), NdotL 0.3909, EV cancels):
  split_PES = 0.06200, split_GF(light) = 0.05908 → **GF/PES = 0.953**. Porting PES-level ambient moves the split to 0.953 (no closing); porting the sun shape moves it to 1.010 (wrong way); both = 1.0 by construction.
- **Verdict: the sky ambient is ELIMINATED as the 1.87× cause (CONFIRMED direction, INF(high) magnitude).** GF's ambient is if anything WEAKER (0.48×); strengthening it to PES's level would need +3–4 dB more blue on the paint and moves the light-level split the wrong way. The residual grass excess is per-material (fin/mask/albedo chain), not illumination. The `daySkyAmbientScale` ponytail (pes_lighting.cpp:257-262) STAYS — it names exactly this write-up as the decode, and the energy it carries matches PES's documented mapping.

## 5. st021 15:00 mapping (file values both sides use at p12)

- Rig: `light_st021_af` (VFS winner = stock dt30; st021 fpkd reuses st009's fox2/atsh); S = 0.2, Slum = 1.0, dayAmb = 0.4, cloud = 0; span `1500_day`, sunLux 150000 all day spans, fade 1.0; toSun NdotL 0.3909; atsh slot 25 (54000 s) exact hit — GF nearest-slot and PES floor-slot AGREE here (they differ only in the second half of each 36-min interval; the bake job additionally lerps between entries per `FUN_1419321f0` — secondary, flagged §7.3).
- atsh15: sun (131008,131008,131008) saturated white; SH row0 (12024,14360,19984), B/R 1.66; rows 1–8 ±(200–6600) (GF `xsh` probe output, stock VFS).
- Live scene note: today's match is st018 dusk (one 12:00 span, S=1.0, Slum=0.4, dayAmb=1.0; dir frozen; Tn 0.513). Scene-dependent values: row magnitude (Tn), B term, EV/comp/min/max, technique slot. Scene-INDEPENDENT: R==G shape, LACC=0, the file rig/SH numbers above, the Sloan matrices, the 0.48×/0.953× verdicts.

## 6. Port spec

1. **No ambient port.** Keep `ambient_scale = skyLightLuminanceScale`, `ambient_light_energy = daySkyAmbientScale`, `AtshSkySh` rows (pes_lighting.cpp:233-241). Keep the ponytail comment; it now cites this report. The cb7-equivalent (Sloan matrices of SH×Slum, baked to the cubemap) is already what GF's sky-ambient chain computes.
2. **No sun-colour port.** Keep `SunColour` (chroma × π·S). Do not port the live R==G shape (live-derived; at noon the file truth is white on both sides).
3. Secondary (not this ticket, no change): GF `AtshSlot` picks nearest, PES floor + bake-lerp — one-slot divergence in :18–:36 of each 36-min step. Relevant to dusk/dawn scenes only.

## 7. Status and next

- CONFIRMED: cb7 consumer shader + matrix form (asm this pass); DataJob writer identity (job strings); live R==G,B~0 + Tn 0.513 + frozen dusk dir (pread); LACC=0; st018 scene; all st021 15:00 file numbers (GF parser, stock VFS); Sloan E(up) (12024→12145.7 G etc.); GF/PES ambient 0.48×; light-level split 0.953×; sweep dirtiness; slot-agreement at 15:00.
- INF(high): writer inputs = atsh SH9 × Slum (doc line + selector string, no dispatch trace); ambient verdict magnitude (Sloan-vs-Godot-convolution normalisation gap named in the ponytail).
- INF(med): R==G = LUT S/luma yellow seed with B killed upstream.
- OPEN: (1) job dispatch + bake-lerp trace; (2) seeding column + B-killer (D3D readback); (3) the actual 1.87× cause — per-material, outside this ticket (fin/mask/albedo; TurfFinPort F1 stays with the owner).
- Recommended next: the mesh-grounded fin pass (PortAudit4 §4 F1) with per-box f from the real mesh + live mask — the only remaining grass-only leg with the right sign and size.
