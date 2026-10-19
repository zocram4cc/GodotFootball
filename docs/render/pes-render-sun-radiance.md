# Sun radiance normalization (Track B): PES's direct-sun factor is lux·π·S (π CONFIRMED, S=0.2 missing in GF); the 1/(4π) fit = π × ~4, and the ~4 is OPEN (not a PES constant); live matches bind the grading LUT path

Scope: the GF↔PES EV gap (GF ~3.6 EV/≈13× too bright at a matched pose; GF EV pinned at the −14 floor, PES live EV −10.8…−11.1), the sun term end to end, and TurfColour blocker 2 (grading-LUT path). Read with `pes-render-TurfColour.md` (the EV gap, the E9 1/(4π) test) and `docs/re/pes-re-lighting.md` Part B §10 (sky π·S convention).

Budget note: report-first; the decisive live-cb3 read is listed as experiment E-A.

## 0. Script-read constant table (all floats read from `pes_full.bin`, offset = VA − 0x140000000; operand VA paired per instruction)

| VA | value | used at |
|---|---|---|
| 0x142578480 | 3.14159274 | `0x14195526b` / `0x140195528a` MULSS (sun/moon ×π) |
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
- **×π**: `*(scene+0x1cc) = SAP+0x1c8·[0x142578480]·fVar11` (`0x14195526b` MULSS, `[0x142578480]` = 3.14159274 script-read); `*(scene+0x1c4) = I·[0x142578480]·fVar11` (`0x140195528a`). Moon path identical. So the sky/atmosphere light intensity is `I·π·skyColorSunScale` (fVar11 plays the S role; INF high — the 0.62832 = 1·π·0.2 live equality in Part B §10 is the same product).
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

### 1.5 What feeds `cb3[1].rgb`: Sky_SunColor column, magnitude ~0.13 (INF, the OPEN point)

- `Sky_SunColor.ps.asm` (559 lines): a 7-pixel-wide sun-colour LUT (`ftoi r0.x` on position selects columns: col 0 = transmittance T, col 1 = white, cols 2–3 = luma-normalized chromaticities `r5 = S/luma`, `r6`, col 4 = `r3 = T·fade` [line 181 `mul r3.xyz, r0.y(fade), r3.xyzx`], cols 5–6 = inscatter S or white). Uses Cornette–Shanks/phase constants `0.0596831031 = 3/(16π)`, `0.119366206 = 3/(8π)` — the **only** π in any sun-adjacent shader, inside phase functions, not a gain.
- The light-object colour init (fox2 load → `light+0x20/+0x40`) was **not** traced to its source column, and live manager reads were stale (menu-scene values: `mgr+0x1c0 = 0` with the sun up). So which column seeds `cb3[1]` — and its exact magnitude Tn — is **OPEN**. Arithmetic constraint from E9 (§3): with `S = 0.2`, `Tn ≈ 0.127` if renderer gains are equal.

## 2. GF path and the missing factor

- GF (`src/gdext/pes_lighting.cpp:142`): `light_intensity_lux = sunLux·fade` (no π, no S); colour = `atsh/max(atsh)` (≈white at 15:00 saturation) (§131–135). Sky, by contrast, **does** carry PES's convention (`sun_intensity = lux·π·skyLightSunScale`, line 239) — GF is internally inconsistent: π on the sky, missing on the sun.
- GF's Lutsk: `DIFFUSE_LIGHT = NdotL·LIGHT_COLOR·ATTENUATION` (plain NdotL multiply, same shape as PES) — no BRDF-level π on either side.
- **Missing factor (CONFIRMED part): `π·skyColorSunScale = π·0.2 ≈ 0.628` on the sun colour.** GF renders the sun ≈1.59× too bright from this alone.
- **The remaining ≈7.9× (to reach the 12.6× E9 fit) is OPEN — it is not a PES constant.** Killed: CPU /4 (upload ×1.0), GPU /4 or /π (literal scans), compose /π, ambient folding (E9 sun-sweep slope dEV/dln = 1.406 ≈ log2(e): the sun drives ≈97.5% of Lmeas), sky excess (GF sky is 0.5× PES's: `150000·π·0.1` vs `150000·π·0.2`), floodlights (unchanged in E9), stadium/sunLux mismatch (live = st021, `pitch_st021`/`light_st021` strings live; sunLux 150000 both), time-of-day-in-T (shadow ≈27° elevation rules out T ≈ 0.13 from transmittance alone).
- Ranked hypotheses for the remainder: **H1** the seeding Sky_SunColor column has |·| ≈ 0.13 at the live sun position (needs experiment E-A); **H2** Godot's directional-lux effective gain vs Fox's cb3 convention (needs a Godot-source lux-sweep or analytic check); **H3** a live-span/elevation fold-in (needs the live tod/span read; live exposure group flips mid-match per TurfColour §2.1/§5.5: comp 0/max −7 matches no single stock span).

## 3. Port statement (where in GF, which constant from which site)

1. **Now (CONFIRMED):** in `PesLighting::setup` (`src/gdext/pes_lighting.cpp`, Sun block ~lines 123–143): multiply the sun colour by `π·skyColorSunScale` — π from `pes_full.bin [0x142578480]` (MULSS `0x14195526b/28a`), S = 0.2 from the stock rig's `TppAtmosphere.skyColorSunScale` (no span override; GF's parser must read the field — add `skyColorSunScale` to the Atmosphere/LightSettings structs, do **not** internalize 0.2). Keep `light_intensity_lux = sunLux·fade` unchanged. This removes the 1.59× CONFIRMED part of the excess.
2. **After E-A:** the residual scale (currently ≈1/7.9 folded into the 1/(4π) fit) gets its own PES site (Sky_SunColor column or Godot-gain correction), with a test. **Do not port `1/(4π)`**: it is a fit, and §2 shows its π half is real while its ~4× half has no PES site.

## 4. TurfColour blocker 2 — SETTLED: matches bind the grading path

- The pack's `ColorCorrection_CubeLut.ps.asm` **declares `t6` (texture3d) but never samples it** (only `t0`); `ColorCorrection.ps.asm` samples `t6` twice (lines 96–97). The pack CubeLut body is a t6-ignoring stub (SHEX 656 B vs STAT 70).
- Live (read-only, current st021 match): the grading registry keyed by hash `0xcb716e233439` (the `FUN_1416a44b0` constant) resolves at 3 holders to the **same** `ColorGradingParameter` object `0x2524ae40`, whose string table names `lut_s_day_game` (+ demo/hdr day LUTs); `lut_s_day_game`/`evening`/`night` strings are live on the heap. So `FUN_1416a3b30` returns non-null → `FUN_141804200` takes the `local_138[0] != 0` branch: binds the grading texture to t6 and switches technique slot `+0x74` to `ColorCorrection_CubeLut`, overwriting `cb5[13]`/`cb5[14]`.
- Consequence: **the film+WB chain GF implements is not PES's live path** — the WB matrix GF uploads is bypassed on the grading branch. GF needs the LUT path (domain/encoding still open) or a proof the live D3D CubeLut differs from the pack stub. The stored WB block (`0xb345480`) is still written (film values live), but not consumed on this path.

## 5. Relation to the E9/giraffe evidence (why the sun port is still right despite the OPEN 4×)

- At E9 (sun/4π) the giraffe luminance matches PES (0.97×) while grass stays 2.1–2.2× and lines 2.3× with a **spectral** signature (R ×6.4, B ×9 vs G ×2.9, ExposureMatch §3): a pure gain error scales channels equally, so the turf residual is per-material (albedo/lighting chain, TurfColour §3/§5.3), not sun gain. The EV match absorbs the sun term; the giraffe confirms E9's sun level for direct-sun paint.
- At base, giraffe 1.4× vs grass 3.7× (ExposureMatch §3): same conclusion — the gain is concentrated in dark turf, i.e. mostly not the sun.

## 6. Open items (numbered)

1. **E-A (decisive for H1):** read the live `cPSLight` shadow (`cb3[1].rgb`, slot 3, 176 B) or trace the light-object colour init (fox2 load → `light+0x20/+0x40`) to name the seeding Sky_SunColor column and its magnitude Tn. (Live manager reads attempted: only stale menu-scene copies found; the current-manager scan needs the sun-vector anchor `0x25691960`-family re-vetted in a live match.)
2. **E-B (decisive for H2):** Godot directional-lux absolute gain: lux-sweep in GF (lux/2, /4) slope + an analytic predict from the renderer source.
3. **E-C (decisive for H3):** live span/hour + sun elevation (tod blocks, SAP dir) at the compared frame.
4. Grading LUT input domain/encoding (Carried from ExposureMatch open 4; pack stub gives nothing).
5. `SAP+0x1c0 = 11`, `+0x1c8 = 0.27` writers; `cb3[3] = 1.4974e6`; fog `cb4[2].z = 1.25` (untouched).

## 7. Reproduce

- Pack: `python3 <workdrive>/gf-B4Grass-scratch/fsop_extract.py GrSystemShaders_dx11.fsop <out> Sky_SunColor DeferredRendering` (+ `GrModelShaders_dx11.fsop` for `DeferredRendering`/`SSLighting_Sun*` — TurfColour scratch `light/` already holds all 12 sun variants); `vkd3d-compiler -x dxbc-tpf -b d3d-asm`.
- Image: `pes_full.bin` (offset = VA − 0x140000000); bridge `decompile_function?address=0x141955000|0x141955d60|0x141699810|0x141941c40|0x141954d70|0x1416821f0|0x1416a44b0|0x1416a3b30`, `disassemble_function?address=0x141955000|0x141699810`.
- Stock rig: GF `LoadLightRig(vfs,21,false)` (winner = stock dt30; st021 fpkd reuses st009 fox2); `AtshSunColor` 15:00 = saturated `(131008,131008,131008)`.
- Live (read-only, sudo): sunLux `*(mgr+0x140)` = 150000.0 at `0x25691960` (stale copy; real manager re-scan per E-A); SAP scene `0x37e33a10` (`+0x110` = −sunDir, `+0x1c0` = `(11, 0.62832, 0.27, 0.62832)`); technique table `*(0x14373ea98)` (55 entries, names+hashes §8); grading registry hash `0xcb716e233439` → `0x2524ae40`; exposure `0x2195f10` (EV −10.83, comp 0, min −14, max −7 at 14:2x).
- Frame: `obs_frame.ts OUT.png PES-vk 1280` + `magick -colorspace RGB` decode; scoreboard shows a new match (MORNING LEADER vs MINIMALIST), long shadows.

## 8. Live technique table (for the record; read 2026-10-19, current match)

`TONEMAP` = hash `463adc7c8406` @ `0x1ff9c10`; `ANTI_ALIASING` = `5247d8da9d5b` @ `0x1c2dd90` (the `FUN_1416821f0` lookup); `COLOR_CORRECTION` = `7005c88f0d3c`; `SUNLIGHT` = `b5d8ec6e9154`; `SUN_SHADOW` = `5ab550c3bdb1`; full 55-entry name+hash list in scratch (`techniques.txt` companion — see §7 method).
