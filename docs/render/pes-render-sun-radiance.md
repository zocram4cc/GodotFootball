# Sun radiance normalization (Track B): PES pitch sun = I·π·S live (row (66500, 66500, ~15, 0) = I·π·S·0.998); E9's 1/(4π) = 2.26× colour × ~5.6× elsewhere; grading/display eliminated

Scope: the GF↔PES EV gap (GF ~3.6 EV/≈13× too bright at a matched pose; GF EV pinned at the −14 floor, PES live EV −10.8…−11.1), the sun term end to end, and TurfColour blocker 2 (grading-LUT path). Read with `pes-render-TurfColour.md` (the EV gap, the E9 1/(4π) test) and `docs/re/pes-re-lighting.md` Part B §10 (sky π·S convention).

Budget note: report-first; the decisive live-cb3 read is listed as experiment E-A.

## 0. Script-read constant table (all floats read from `pes_full.bin`, offset = VA − 0x140000000; operand VA paired per instruction)

| VA | value | used at |
|---|---|---|
| 0x142578480 | 3.14159274 | `0x140195526b` / `0x140195528a` MULSS (sun/moon ×π) |
| 0x1429dc110 | (0, −1, 0, 0) | `0x140195505c` MULPS (fade dot, up = −Y) |
| 0x14256ae1c | 1.0 | `0x1401955115` MOVSS (fade base) |
| 0x142a801d0 | −0.13744450 | `0x14019550bf` COMISS (fadeSun knee) |
| 0x142a801c8 | 0.00518455 | `0x1401955129` DIVSS (fadeSun width ~0.3°) |
| 0x142a801d4 | −0.14262904 | `0x1401955154` MOVSS (fadeMoon knee) |
| 0x142a801cc | 0.01035687 | `0x1401955165` DIVSS (fadeMoon width) |
| 0x142565d20 | (3, 3, 3, 3) | normalize shape (Newton–Raphson `3−k·r²`) |
| 0x1425658b0 | (0.5, 0.5, 0.5, 0.5) | normalize shape (`0.5·r`) |
| 0x1425a0c50 | (1, 1, 1, 0) | `0x1401699817` MULPS (cb3 colour scale: identity) |
| 0x142588210 | 10.0 | `0x140169983e` MULSS (dirty-flag accumulator only, not the colour) |
| 0x14259bdfc | 11.0 | SAP sentinel `+0x1c0` |
| 0x142a8032c | −999.0 | light-mode default threshold |
| 0x142a80328 | 1e−9 | degenerate-vector guard |
| 0x14259be40 | 57.29578 | rad→deg (angular-size test) |
| 0x142577f6c | 1e−6 | zero-length guard |
| 0x14253ee60 | (0, 1, 0, 0) | fallback up in dir normalize |
| 0x1425a0c48 | 0.01745329 | deg→rad (radius upload) |
| 0x1425a9668 | 0.001 | change epsilon |
| 0x1425568d0 | 1e−5 | change epsilon |

## 1. The chain (PES side)

### 1.1 `sunLux` source: stock rig, st021 aliases st009 (CONFIRMED, VFS winner = stock)

- `light_st021_af.fpkd` (VFS winner, 459024 B) contains **st009's** files (`light_st009_af.fox2`, `.atsh`, `.pcsp`): Konami-authored alias, verified by member listing. The mount report shows `4cc_30_stadiums.cpk`/`4cc_32_stadiums2.cpk` carry **no** `light_st021_af` path, so the winner is **stock** `Data/dt30_g4.cpk` (per the dt18 rule: stock values quoted below).
- Stock values (GF parser against the winner): 12:00–16:00 spans `sunLux` 150000; 15:00 `skyLightSunScale` 0.1; `TppAtmosphere.skyColorSunScale` **0.2** (no span override exists for it); `moonLux` 1, `starLight` 1; `sunColorExtinctionOfClearSky` (1,1,1,1), `sunColorOfCloudySky` (0.5,0.5,0.5,1). st029 (for reference only): day `sunLux` 100000, same key 0.18.
- GF impact: `src/pes/lighting.{hpp,cpp}` parses `skyLightSunScale` but **never reads `skyColorSunScale`** — the sun-gain field is absent from GF's structs.

### 1.2 CPU: `FUN_141955000` — intensity × π (CONFIRMED, decompile + disassembly + script-read constants)

- Fade: `e = dot(sunDir, (0,−1,0))` (`0x140195505c` MULPS XMM2,[0x1429dc110]; up confirmed `(0,−1,0,0)` script-read). `fadeSun = e<−0.13744450 ? sat(1−(−0.13744450−e)/0.00518455) : 1` (≈0.3° band); moon mirrored at `−0.14262904/0.01035687`.
- `I_sun = sunLux·fadeSun → mgr+0x1c0`; `I_moon = moonLux·(1−fadeMoon) → mgr+0x1c4`.
- **×π**: `*(scene+0x1cc) = SAP+0x1c8·[0x142578480]·fVar11` (`0x140195526b` MULSS, `[0x142578480]` = 3.14159274 script-read); `*(scene+0x1c4) = I·[0x142578480]·fVar11` (`0x140195528a`). Moon path identical. So the sky/atmosphere light intensity is `I·π·skyColorSunScale` (fVar11 plays the S role; INF high — the 0.62832 = 1·π·0.2 live equality in Part B §10 is the same product).
- Caller: single `E8` x-ref `0x141954d70` (image scan); dirty-flag helper `FUN_1416b49f0` is a bit-set stub.

### 1.3 CPU: `FUN_141955d60` + `FUN_141699810` — direction normalize, **scale 1.0** (CONFIRMED)

- Mode select (`iVar3 = light+0xd0`): colour block `light+0x20` (mode 1) or `+0x40` (mode 2) else zero; threshold test vs angular size (`57.29578` at `0x14259be40`, guard `1e−9`).
- Normalize is the `(3−k·r²)·0.5·r` Newton–Raphson direction normalize (`[0x142565d20]` = 3, `[0x1425658b0]` = 0.5, script-read) — direction-only, no scale.
- Upload `FUN_141699810`: `MULPS colour,[0x1425a0c50]` = `(1,1,1,0)` (script-read; disasm `0x1401699817`) — **identity, CONFIRMED no-op**; the `×10.0` (`[0x142588210]`, disasm `0x140169983e`) feeds only a `CVTTSS2SI` dirty-flag accumulate. No 1/π, no 1/4π, no /4 anywhere on the CPU path.

### 1.4 GPU: `lightColor = cb3[1].xyz·cb2[23].z`, no π literal (CONFIRMED, asm)

- `SSLighting_Sun_NoShadow.ps.asm` line 71: `mul r1.yzw, cb2[23].z, cb3[1].xxyz`; diffuse `mul o0.xyz, r1.yzwy, r2.w` (NdotL-term). Shadowed `SSLighting_Sun.ps.asm` line 76: same product. PrecomputeShadow line ~270: `mul r7.xyz, cb2[23].z, cb3[1].xyzx`.
- Literal scan of **all 12** `SSLighting_Sun*.ps.asm` variants: only `0.96/0.04` (spec), `0.25` (Smith), `0.5`, `1.59e−2` (1/63), `0.02`, `1/1023`, `1023`, `1e4` (clamp), `1e−6` — **no 1/π, 1/4π, 0.318, 0.0796, or /4**.
- `DeferredRendering.ps.asm` (32 lines): `o = lerp(albedo·diffAcc + specAcc, fog, w)` — plain, no π.
- RDEF: `cPSLight` (slot 3, 176 B, `g_psLight`) + `cPSScene` (slot 2) confirm the `cb3`/`cb2` slots; field names are engine-side (all-`float4` arrays).

### 1.5 `cb3[1]` seed shape (static): Sky_SunColor columns and phase constants
- `Sky_SunColor.ps.asm` (559 lines): a 7-pixel-wide sun-colour LUT (`ftoi r0.x` on position selects columns: col 0 = transmittance T, col 1 = white, cols 2–3 = luma-normalized chromaticities `r5 = S/luma`, `r6`, col 4 = `r3 = T·fade` [line 181 `mul r3.xyz, r0.y(fade), r3.xyzx`], cols 5–6 = inscatter S or white). Uses Cornette–Shanks/phase constants `0.0596831031 = 3/(16π)`, `0.119366206 = 3/(8π)` — the **only** π in any sun-adjacent shader, inside phase functions, not a gain.
### 1.6 E-A result (2026-10-19, live BNT vs W match, read-only): the pitch sun row is (66500, 66500, ~15, 0) — the CPU colour is only 2.26×, not 12.6×
- Live sun-colour row, found by magnitude scan over from-light-dir triples (`(-0.5644, −0.7055, 0.4286)`, tolerance 3e−4): a 0x140-stride struct array at `0x13321440 + k·0x140` (slots 0/16/32 live) whose `+0x00` row reads `(66526.2, 66526.3, 8.09, 0)`, `(66532.3, 66532.4, 17.80, 0)`, `(66538.6, 66538.7, 19.42, 0)` (drifting up as the match sun moves; R==G to 5 digits, B independent 0–21). Magnitude `66526·√2 = 94081` vs `150000·π·0.2 = 94247.78`: ratio **0.99823** (0.2% low). So `|cb3[1]| = I·π·S·Tn` with `Tn ≈ 1` (chromaticity-like seed, R==G yellow): **the CPU sun colour exceeds the `I·π·S` scale by nothing — the colour ratio GF/PES (green) is `150000/66500 = 2.2558×`.**
- Consequence: the 12.6× E9 fit = **2.26× (sun colour, this section) × ~5.6× (elsewhere)**. Killed for the remainder: grading/display (GradingLut report `docs/render/pes-render-grading-lut.md` §3: stub x0.25 + SdrDisplayMapping x4 cancel exactly, net identity in-gamut, achromatic — Main), `cb3` magnitude (this row), `cb2[23].z` (= 2^EV, verified against live EV at every read), CPU /4 and GPU /π (literal scans, §1.3–1.4). The stale manager copy (`0x25691820`: `I_sun = 0`, `I_moon = 1`, light mode 0, upload struct gone) is a dead light — the active pitch sun is this row's owner, not the manager mirror.
- Live sky cross-check (same session): the `Sky_Draw` cb4 block at `0x1c15b70` reads `cb4[3] = (to-sun, 0.6283)` = `1.0·π·0.2` (moon-slot intensity in a day frame — the atmosphere manager is night-parked while the pitch sun runs the day value; two pipelines), `cb4[5] = (0.18, 0.18, 0.18, 0)` (CONFIRMS the doc's open item: below-horizon is live **0.18**, not `groundColor` 0.5), `cb4[2] = (0.9584, 0.9, 0, 0)` (Schlick k + `1−abs`), `cb4[6] = ((0,1,0), 0.27)`.
## 2. GF path and the missing factor

- GF (`src/gdext/pes_lighting.cpp:142`): `light_intensity_lux = sunLux·fade` (no π, no S); colour = `atsh/max(atsh)` (≈white at 15:00 saturation) (§131–135). Sky, by contrast, **does** carry PES's convention (`sun_intensity = lux·π·skyLightSunScale`, line 239) — GF is internally inconsistent: π on the sky, missing on the sun.
- GF's Lutsk: `DIFFUSE_LIGHT = NdotL·LIGHT_COLOR·ATTENUATION` (plain NdotL multiply, same shape as PES) — no BRDF-level π on either side.
- **Missing factor (CONFIRMED part): `π·skyColorSunScale = π·0.2 ≈ 0.628` on the sun colour.** GF renders the sun ≈1.59× too bright from this alone. The live row (§1.6) pins the colour ratio at **2.2558× green** (`150000` vs `66500`; the row's R==G shape vs GF's white atsh-max colour adds a small chroma term on top).
- **The remaining ≈5.6× (12.6/2.26, sun-fraction-corrected ≈5.3×) is OPEN — it is not in PES's colour, grading, display, or exposure.** Killed: CPU colour (live row §1.6), grading/display (GradingLut `docs/render/pes-render-grading-lut.md` §3: net identity in-gamut, achromatic — Main), `cb2[23].z` (= 2^EV at every live read), CPU /4 (upload ×1.0), GPU /π (literal scans of all 12 sun variants + compose), ambient folding (E9 sun-sweep slope dEV/dln = 1.406 ≈ log2(e): the sun drives ≈97.5% of GF's Lmeas), sky excess (GF sky is 0.5× PES's), stadium/sunLux mismatch (live = st021, sunLux 150000 both).
- Ranked hypotheses for the ≈5.6× remainder: **H2** Godot's directional-lux effective gain vs Fox's cb3 convention (lux-sweep in GF + renderer-source check); **H4** a GF-side second sun/ambient path at the metered pose (floodlight proxy groups: GF `GroupOn` enables ungrouped lights by default — count enabled lights, zero floodlight energies and re-measure; moon is off); **H5** per-light/per-material shader scale in the pitch/character lighting (specScale/GB2.y, wrapW, Fresnel LUT — spectrally constrained: the grass residual is diffuse-dominated, so a specular-only error cannot carry it). H1 (seed column, E-A) is CLOSED by §1.6 (`Tn ≈ 1`); H3 (span fold-in) is demoted (span-independent: row magnitude matches `I·π·S` at both live EVs).

## 3. Port statement (where in GF, which constant from which site)

1. **Now (CONFIRMED):** in `PesLighting::setup` (`src/gdext/pes_lighting.cpp`, Sun block ~lines 123–143): multiply the sun colour by `π·skyColorSunScale` — π from `pes_full.bin [0x142578480]` (MULSS `0x140195526b/28a`), S = 0.2 from the stock rig's `TppAtmosphere.skyColorSunScale` (no span override; GF's parser must read the field — add `skyColorSunScale` to the Atmosphere/LightSettings structs, do **not** internalize 0.2). Keep `light_intensity_lux = sunLux·fade` unchanged. Live proof the scale is right: the row's `|·| = I·π·S·0.998` (§1.6). This removes the 2.26× CONFIRMED part of the excess (1.59× achromatic π·S plus the white-vs-R==G chroma term).
2. **Next (OPEN ≈5.6×):** not a PES colour constant — E-A closed the colour (`Tn ≈ 1`). The remainder gets its port only from E-B (Godot lux-sweep + renderer source) or E-D (GF light audit: count enabled DirectionalLights/SpotLights at the metered pose, zero floodlight energies and re-measure Lmeas/EV). **Do not port `1/(4π)`**: it is a fit; §1.6 shows its colour half is 2.26× (not π) and the rest has no PES site.

## 4. TurfColour blocker 2 — SETTLED: matches bind the grading path

- The pack's `ColorCorrection_CubeLut.ps.asm` **declares `t6` (texture3d) but never samples it** (only `t0`); `ColorCorrection.ps.asm` samples `t6` twice (lines 96–97). The pack CubeLut body is a t6-ignoring stub (SHEX 656 B vs STAT 70).
- Live (read-only, current st021 match): the grading registry keyed by hash `0xcb716e233439` (the `FUN_1416a44b0` constant) resolves at 3 holders to the **same** `ColorGradingParameter` object `0x2524ae40`, whose string table names `lut_s_day_game` (+ demo/hdr day LUTs); `lut_s_day_game`/`evening`/`night` strings are live on the heap. So `FUN_1416a3b30` returns non-null → `FUN_141804200` takes the `local_138[0] != 0` branch: binds the grading texture to t6 and switches technique slot `+0x74` to `ColorCorrection_CubeLut`, overwriting `cb5[13]`/`cb5[14]`.
- Consequence: **the film+WB chain GF implements is not PES's live path** — the WB matrix GF uploads is bypassed on the grading branch. GF needs the LUT path (domain/encoding still open) or a proof the live D3D CubeLut differs from the pack stub. The stored WB block (`0xb345480`) is still written (film values live), but not consumed on this path.

## 5. Relation to the E9/giraffe evidence (why the sun port is still right despite the OPEN 4×)

- At E9 (sun/4π) the giraffe luminance matches PES (0.97×) while grass stays 2.1–2.2× and lines 2.3× with a **spectral** signature (R ×6.4, B ×9 vs G ×2.9, ExposureMatch §3): a pure gain error scales channels equally, so the turf residual is per-material (albedo/lighting chain, TurfColour §3/§5.3), not sun gain. The EV match absorbs the sun term; the giraffe confirms E9's sun level for direct-sun paint.
- At base, giraffe 1.4× vs grass 3.7× (ExposureMatch §3): same conclusion — the gain is concentrated in dark turf, i.e. mostly not the sun.

## 6. Open items (numbered)

1. **E-A (CLOSED 2026-10-19):** live pitch sun row `0x13321440+k·0x140 +0x00` = `(66526, 66526, 8–21, 0)`, `|·| = I·π·S·0.998` (`Tn ≈ 1`). Method: magnitude scan over from-light-dir triples + 0x140-stride struct array; R/G drift with the match sun, B independent. The seeding-column question is answered (chromaticity-like R==G seed, not transmittance-magnitude).
2. **E-B (decisive for H2):** Godot directional-lux absolute gain: lux-sweep in GF (lux/2, /4) slope + an analytic predict from the renderer source.
3. **E-C (demoted):** live span/hour is span-independent for the colour (row matches `I·π·S` at EV −10.67…−11.35); keep only for the integer-hour sun-motion check (175 steps/day).
4. **E-D (decisive for H4, GF-side, Main's side):** light audit at the metered pose — count enabled Directional/Spot/Omni lights, zero floodlight energies, re-measure Lmeas/EV. `GroupOn` enables ungrouped rig lights by default (suspect for the ≈5.6×).
6. `SAP+0x1c0 = 11`, `+0x1c8 = 0.27` writers; `cb3[3] = 1.4974e6`; fog `cb4[2].z = 1.25` (untouched).

## 7. Reproduce

- Pack: `python3 <workdrive>/gf-B4Grass-scratch/fsop_extract.py GrSystemShaders_dx11.fsop <out> Sky_SunColor DeferredRendering` (+ `GrModelShaders_dx11.fsop` for `DeferredRendering`/`SSLighting_Sun*` — TurfColour scratch `light/` already holds all 12 sun variants); `vkd3d-compiler -x dxbc-tpf -b d3d-asm`.
- Image: `pes_full.bin` (offset = VA − 0x140000000); bridge `decompile_function?address=0x141955000|0x141955d60|0x141699810|0x141941c40|0x141954d70|0x1416821f0|0x1416a44b0|0x1416a3b30`, `disassemble_function?address=0x141955000|0x141699810`.
- Stock rig: GF `LoadLightRig(vfs,21,false)` (winner = stock dt30; st021 fpkd reuses st009 fox2); `AtshSunColor` 15:00 = saturated `(131008,131008,131008)`.
- Live (read-only, sudo): pitch sun row `0x13321440+k·0x140 +0x00` (slots 0/16/32; no heap back-refs — pool without links); manager copy `0x25691820` (STALE: `I_sun = 0`, dead light); inline seed `M+0x1e0+0x10` = `(106890, 78212, 38756, 0)` (stale, low-sun-like 2.76:2.02:1); sky cb4 `0x1c15b70` (`cb4[3] = (to-sun, 0.6283)`, `cb4[5] = (0.18,0.18,0.18,0)`); SAP scene `0x37e33a10`; technique table `*(0x14373ea98)` (55 entries, §8); grading registry `0xcb716e233439` → `0x2524ae40`; exposure `0x2195f10` (EV −10.67…−11.35 drifting, comp −0.33/min −14/max −5 at 15:1x — the 15:00 span, matching GF's E-run span); scripts `ea_bound.py`/`ea_fast.py` in scratch.

## 8. Live technique table (for the record; read 2026-10-19, current match)

`TONEMAP` = hash `463adc7c8406` @ `0x1ff9c10`; `ANTI_ALIASING` = `5247d8da9d5b` @ `0x1c2dd90` (the `FUN_1416821f0` lookup); `COLOR_CORRECTION` = `7005c88f0d3c`; `SUNLIGHT` = `b5d8ec6e9154`; `SUN_SHADOW` = `5ab550c3bdb1`; full 55-entry name+hash list in scratch (`techniques.txt` companion — see §7 method).
