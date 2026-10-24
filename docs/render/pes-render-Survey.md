# PES→GF render survey: per-doc OPEN table, static decodes closed, corrections, port-ready list

Status: read-only survey. Repo `<workdrive>/GodotFootball` (main `15c6779`), install VFS `<workdrive>/PES21`,
image `~/ghidra-projects/scratch/Unpacker/pes_full.bin` (VA = fileoff + 0x140000000), Ghidra bridge
`http://127.0.0.1:8080` (one request at a time). Scratch: `<workdrive>/gf-RenderSurveyRE-scratch/`
(absolute output paths; nothing written to the repo). Read with every doc in `docs/render/`.

Budget: ~150 requests; this report used ~45 (reads + ~15 bridge/VFS/shader calls).

## 0. Conventions

- CONFIRMED = decompile + disassembly + script-read image bytes (operand VA paired), or live read, or install-file decode.
- INF = several lines agree, no direct demonstration. OPEN = blocked, with the blocker named.
- Floats below are read from the image per brief rules (operand VA paired), never typed.
- "Port-ready, unlanded" = decode complete on an origin branch that has not merged to main.
- SunLuma (the separately running sun-era luminance-gain investigation) was NOT available as a doc;
  its scratch (`gf-SunLuma-scratch/`) holds only `measure.py`; no claims from it are used here.

## 1. Per-doc OPEN table (need + status after this session)

### 1.1 `pes-render-camera.md` (B1Camera — IN MAIN)
1. Ball-height lift input with ball in air — need: live read `InplayCamera+0x38/0x44`, ball > ~3 m. OPEN (owner/live; PES at menu, no live match).
2. Slider persistence (`CAMERASETTINGS +0x04/+0x0C` behind the mutation serializer) — need: static serializer decode. OPEN (static RE, ~10 req).
3. Mode writer (`[InplayCamera+8]`, one hop past `0x14087a930`) — need: static RE. OPEN (static RE, ~5 req).
4. k-goal when match-state word is 5 (`1.2−0.2·clamp(dist/FieldInfo[6],1)`, dist operands unidentified) — need: static RE. OPEN.
5. FOV-blend damping + state-3 ±8.268 bias + `cameraMoveAreaSize` dead zone — need: live reads with match-state flags. OPEN (owner/live).
- Correction: none. Note: the doc's own "black pitch is a B2/B4 regression on main" predates SunPort/GradePort; the B2/B4 cause chain is now the fin cover (TurfFinPort, §4).

### 1.2 `pes-render-b2lighting.md` (B2Lighting — IN MAIN)
- "NOT visually verified / two calibration knobs at decoded defaults" (sky `sun_intensity` = lux·π·skyColorSunScale; camera `exposure_multiplier` = 2^(initEV+comp) = 2^−10.34).
- Status: SUPERSEDED in part. Sky scale CONFIRMED live by the moon slot (0.6283 = π·0.2, sun-radiance §1.6) and landed via SunPort (`SkyRadianceScales`, `6923da2` IN MAIN). The exposure_multiplier knob is gone: GradePort removed the Godot-side multiplier (exposure lives in COMPOSE now). What remains is one live frame for the sky-horizon white clip — need: owner/live (stadium + time + weather + broadcast camera TBD by owner). OPEN (owner/live).

### 1.3 `pes-render-b3post.md` (B3Post — IN MAIN, then superseded by GradePort)
OPEN items, all `ponytail`-marked:
1. 33³ grading LUT — CLOSED: matches bind the CubeLut stub (§5: re-verified this session, SHEX 656 B vs STAT 70, t6/t7 never sampled; §0 table).
2. AA/sharpen/grain — not applied. No change (PES Part C §5; no new evidence). OPEN, out of scope.
3. Reduction type + 32·a² omission — PART-CLOSED: arithmetic mean CONFIRMED (no log/exp in Tonemap_Lum/blur_box2D/blur_kawase, re-grepped); 32·a² present (re-disassembled §0: `mul 32.0`, `max`), omitted in GF. No change. OPEN (alpha writer still missing, TurfColour item 4).
4. Bloom downsample filter + 0.1–0.2 copy branch + EV→offset g() — CLOSED as a path: composition never samples t7 (stub L14 sole sample; SdrDisplayMapping samples t0 only). Bloom is dead on the match path. No port needed.
5. Adaptation dt = 1/60 — matches PES per-frame behaviour; no change. CLOSED (no action).
- Needs owner/Main: none (stadium resolved 29; turf shader on main via TurfEV/TurfFinPort chain — but see §4).

### 1.4 `pes-render-b5materials.md` + `pes-kit-compare-b5.md` (B5Materials — IN MAIN)
1. Lit-scene −4.5 stops (B2/B3) — REFRAME: pre-SunPort number. Post-SunPort full-frame is 1.04× PES, grass 1.54×, lines ~1.4× (SunPort §3 F3). The residual is the fin cover (TurfFinPort FIN_NET 0.775, §4), then a GF frame re-measure. Need: one GF frame on current main. OPEN (GF-side render, no owner needed).
2. Whole-body back name/number on the Suit — need: static RE (marking UV placement on the boots body). OPEN (static RE, est. ~10 req).
3. Whole-body monitor picture (texture binding) — need: static RE (head material binding for face packs). OPEN (static RE, est. ~5 req).
4. BNT zero-alpha kit atlas ("confirm the PES rule") — CLOSED this session (§2): PES reads the same `.a` there; the weave term is `lerp(p0.x, 0.85, a)`, so zero alpha takes the weave (0.91 measured) vs 0.85 opaque — ~7% albedo, per the RE coverage rule. Matches `pes-re-shaders.md` §3.1 `a = max(M.a, base.a); alb = base.rgb·lerp(p0.x, 0.85, a)`. No port change (GF `pes_uniform.gdshader:103` already `mix(p0.x, OPAQUE_KIT_ALBEDO, a)` with `0.85` from the original port commit `c61d3ad`).

### 1.5 `pes-render-B7Effects.md` (B7Effects — IN MAIN) + `pes-render-EffectsBinding.md` (IN MAIN)
- Acceptance never met: no live PES frame of any placed effect. Not placed: LightBillboard (st002 locators 32 zero bytes), LensFlare (layer/scale undecodable), Halo (59, scale units undecoded), emitter templates (cut-triggered `.fdc` type-3). Known fidelity gaps (all in `ShaderFor` mapping): `3d_rain`, `effect_rainModel`, `net_splash`, `pitch_ground`/`pitch_leaf` translucency. Match-scene hook proposed, not wired (`godot/match/**` untouched).
- Status: no cheap static close exists (all need live matches at rain/stadium overlays or locator-format RE). OPEN (owner/live + RE, none under ~20 req). No change this session.

### 1.6 `pes-render-GradePort.md` (GradePort `0c2a8bb` — IN MAIN)
OPEN items: (1) luminance ~3.5× on turf — REFRAME to ~1.5× post-SunPort, then the fin cover (§4); (2) blue low — separate sky/ambient fill item (SunPort §8.4, still OPEN); (3) literals vs ReadVa — style, Main to decide; (4) `vp+0x650` bypass — `ponytail`, live 0 every session, gain always applies; (5) comp RT format — no display effect either way; (6) TurfColour WB work moot — agreed; (7) no live like-for-like frame — owner/live; (8) grading-lut digit erratum — CLOSED this session (§6 correction 1).
- Correction: none (numbers stand; "3.5×" is pre-SunPort).

### 1.7 `pes-exposure-ExposureMatch.md` (branch ExposureMatch — NOT in main, pose-only, no code kept)
OPEN items: (1) kits — OPEN (match-setup kit offsets undecoded); (2) hour — CLOSED (LumaResidual §1: st021 day frozen at 15:00); (3) turf blue floor — CLOSED as a shader question (PitchMaterial: curve applied lane-identically; PES blue 0.007 is the albedo, not a floor bypass); (4) grading LUT domain — CLOSED (stub never samples t6); (5) unbound mask — CLOSED (DarkTurf §1: effective 0 both sides); (6) PES camera 5.6 m offset — OPEN (convention undecoded; GF pose fitted, not derived); (7) GF stands banding/dark band — OPEN (separate defect, sits in the adaptation metric; one GF frame).
- Corrections: round-1 "grass 0.76–0.94×" already self-superseded (misaligned pair); items 2–5 above close out superseded decodes, not live contradictions.

### 1.8 `pes-render-TurfColour.md` (branch TurfColour — NOT in main; reverted, no code kept)
OPEN items: (1) live composition path — CLOSED (ComposePath: grading branch hit live); (2) sun radiance scale — SPLIT: 2.26× colour CONFIRMED + ported (SunPort), remainder GF-side per DarkTurf/LumaResidual (not a PES constant; "do NOT port 1/(4π)"); (3) per-material residual — CLOSED as fin cover (§4); (4) scene alpha writer — still OPEN (needed only if the 32·a² term is ever modelled); (5) 15:00 span assumption — CLOSED (LumaResidual §1: single `_day` span, frozen).
- Branch holds no experiment edits (`17aef74` revert). Nothing to land.

### 1.9 `pes-render-PitchMaterial.md` (doc only; code = TurfFinPort/PitchV7 branches)
OPEN items: (a) composition path — CLOSED (ComposePath); (b) luminance residual — CLOSED as fin cover (§4). T1 "divergence" — REFUTED this session (§3 correction 3): file line 112 is lane-identical to asm 49–55 for unit normals (y²−1 = −(x²+z²)); the scratch `t1_verify.txt` GF_T1 column was computed with a different formula than the file line. No port change (PitchV7 `6b3753f` also landed; but see §6 correction 3 — its comment/premise needs a fixup, not its math).
- `cb2` per-frame values — OPEN by nature (per-frame, not stadium constants).

### 1.10 `pes-render-LumaResidual.md` (doc only)
OPEN items: (1) live `cb5[12]` + comp-block `+0x8c` writer — still OPEN (§5: writer untraced; bounded second-order, CLOSED as residual cause); (2) GF stands-shadow proof — OPEN (one GF frame, wrong-signed in any case); (3) dark-turf remainder — CLOSED as fin cover (§4); (4) `build/` staleness — CLOSED this session (§7: `build/libpesfmt.a` 2026-10-21 predates no current source; main `15c6779` is newer than the lib, but the lib's staleness claim was about a same-day WIP tree — recheck only if a fresh-harness link fails).
- Note: `bss 0x143514170` = 1.0 live (carried; not re-read — PES at menu, no live match).

### 1.11 `pes-render-DarkTurf.md` (doc only)
OPEN items: (1) VS `v1.z`/`v3` reduction — CLOSED in range (TurfPlacer §4 + TurfMask §4: r 0.37–0.65 working range, mid 0.50); (2) placement source — CONFIRMED (TurfPlacer §3: 281×159 turf state machine); (3) live draw proof at the p12 camera — still OPEN (no GPU readback in the read-only toolbox); (4) `param_4`/live `cb5[12]` — carried OPEN (§5); (5) `build/` staleness — see §1.10.
- No corrections; the four-candidate verdicts stand (mask/albedo/second-light rejected, fins last standing).

### 1.12 `pes-render-TurfFins.md` (doc on `origin/TurfFinPort`)
OPEN items: (1) fin albedo point value — CLOSED in range (r mid 0.50 adopted; exact texel weights live-OPEN but the band prediction does not need them); (2) draw-call proof — still OPEN (same GPU-readback block); (3) `param_4`/live `cb5[12]` — carried OPEN (§5).
- `u4` = broadcast band, nested-cpk placer suspect: placement CONFIRMED by TurfPlacer (turf state machine in image, not nested cpk), so the "VFS-blind nested cpk" suspicion is demoted — the placer never needed the nested cpk.

### 1.13 `pes-render-TurfPlacer.md` (doc on `origin/TurfFinPort`)
OPEN items: (1) mask content + `cb4[1]` live values — SPLIT: `cb4[1]` CLOSED static (TurfMask §2), mask content still OPEN (GPU-side); (2) PlanarShadow target + live `cb4`/`cb5` — OPEN (live/material); (3) BC7 texels — CLOSED (TurfMask §3, all six decoded); (4) draw proof — still OPEN (GPU readback).
- Correction: §1.4 "cell zero-initialised in the image" REFUTED by TurfMask §1.1 (live heap pointer in the dumped image cell — the cell is written at/after load).

### 1.14 `pes-render-TurfMask.md` (doc on `origin/main` as `eb250b2`)
OPEN items: (1) mask texel content + `cb4[3]/cb4[4]` — still OPEN (D3D11 readback or material-uniform source; outside `pread`); (2) PlanarShadow `[rbx+0x130]` content — still OPEN (same tooling); (3) draw proof — still OPEN.
- This is the newest render doc on main; its port-spec updates (§6) are the standing spec (with §4's FIN_NET already shaped as `dd70ad3`).

### 1.15 `pes-render-ComposePath.md` (doc on `origin/TurfFinPort`)
OPEN items: registry creation path; `param_4` writer; bound t6 identity this session; live `cb5[12]` values; LACC branch taken; `cb3[3]`/fog/`SAP+0x1c0`.
- Closed this session (§5): LACC = `FUN_141684180` returns byte `DAT_14373e951` (single-byte flag; image reads 0x00 at the VA — section-mapped reads needed for certainty, but the branch shape is fixed: untaken = plain Lum path GF already ports).
- `param_4`/caller note: `FUN_141804200`'s only code xref in the image is `0x142a383f0` [DATA] (bridge `xrefs_to`, one request) — i.e. called through a table/vtable, not a direct call site; the caller trace needs a live session, not more static. Kept OPEN (owner/live).
- Correction: §3's "bound t6 is NOT the file LUT this session" is a same-session observation, not a doc error — the program pair is identical and the shader never samples t6 either way. No port impact.

### 1.16 `pes-render-SunPort.md` (SunPort `6923da2` — IN MAIN per handover/batch B)
OPEN items: (1) 3.2-EV gap — REFRAMED: colour half ported; remainder is GF-side per DarkTurf §3/LumaResidual §5, plus the fin cover (§4). The "(a) ~0.1 factor in the pitch shader" candidate is CLOSED (PitchMaterial: shader matches term-for-term; the 2× was WB, the rest fins). (2) Tonemap_Lum rect caller — see §1.15 (table-driven call; live work). (3) metered alpha — carried OPEN. (4) blue deficit — still OPEN (sky/ambient fill). (5) ungrouped-light rule — still OPEN (needs a 15:00 live match). (6) night-path π/4π conversions — still OPEN (not checked). (7) live `vp+0x650` nibble-2 bypass state — use live reads with the bypass noted. (8) top black band — separate defect (stands/sky, one GF frame).
- Corrections: items 2–4 of §6 (floodlights/chroma) stand; E8 (lux·π) correctly refuted and reverted.

### 1.17 `pes-render-sun-radiance.md` (doc only; E-A session)
OPEN items: E-B (Godot lux-sweep + renderer source) — demoted: DarkTurf §3 audited GF's enabled lights at the metered pose (3 ungrouped DemoAdd spots, all out of range; moon off; sky weaker) — the ≈5.6× "elsewhere" is now the fin cover, not a light audit. E-C (span-independence) — CLOSED (single `_day` span). E-D (GF light audit) — DONE (DarkTurf §3, result: zero box effect). Remaining: `SAP+0x1c0 = 11`, `cb3[3] = 1.4974e6`, fog `cb4[2].z = 1.25` — carried, untouched.
- Live `cb4[5]` = 0.18 CONFIRMS the sky doc's below-horizon item (see §3 correction 4: GF already reads it from the file; the "0.5" is only the parser default).

### 1.18 `pes-render-grading-lut.md` (doc only)
OPEN items: (1) PS draw-time capture — CLOSED (pair + VS-byte identity + exclusive bind); (2) `vp+0x650` — CLOSED (0 live, ×0.25 on); (3) SdrDisplayMapping cb inputs + comp RT format — no display effect either way (clamp [0,1] both ways); (4) LUT domain — moot (bound, unsampled); (5) `SAP+0x1c0`/fog — carried.
- Correction: §3.7/§5 dark-slope digits `0.30997522` → `3.09597522e-01` (= 4/12.92) — GradePort §46 already files it; the install asm re-read this session prints `3.09597522e-01` (§0). Fix when archiving (one-line doc edit by Main).

### 1.19 `pes-render-TurfColour.md` residual cross-check (see §1.8)
No new opens. The E9 1/(4π) fit is evidence of magnitude only — must not be ported (sun-radiance §3.2).

### 1.20 `pes-pitch-PitchV7.md` (PitchV7 — IN MAIN)
- Ported VS fog terms `o7.z/o7.w` into specular/roughness (asm 107–129/166–178). Assumed, not read: `cb2[5].y = PROJECTION_MATRIX[1][1]`, depth = −view z, metres. Polynomial literals are asm immediates (hard-coded, per PS style). log2 guard for camera-below-surface missing. ROUGHNESS unclamped (as PES asm 177).
- Status: landed; remaining notes are shader-hardening, not RE. OPEN (GF-side hardening, no PES read needed): log2 guard, ROUGHNESS clamp check. No owner/live needed.

### 1.21 `pes-render-sun-radiance.md` vs `pes-render-SunPort.md` on the sky scale
Consistent (both π·S; moon slot 0.6283 live). No contradiction.

## 2. Static decodes closed this session (each < ~20 requests; evidence cited)

### 2.1 BNT zero-alpha atlas rule — CLOSED (no port change)
- `pes-re-shaders.md` §3.1 (on main): `a = max(M.a, base.a); alb = base.rgb·lerp(p0.x, 0.85, a)`.
- GF `godot/shaders/pes_uniform.gdshader:103`: `ALBEDO = … base.rgb * mix(p0.x, OPAQUE_KIT_ALBEDO, a)` with `OPAQUE_KIT_ALBEDO = 0.85` (from port commit `c61d3ad`). Zero alpha → weave `p0.x` (0.91 measured); opaque → 0.85. ~7% albedo, as the kit-compare doc estimates.
- Verdict: GF already implements the PES rule. The doc's OPEN 4 is answered: PES reads the same zero alpha (the `.a` IS the coverage `a0`, same slot t0). No code change.

### 2.2 Composition pair re-verified from the install — CONFIRMED (no port change)
- Pack `GrSystemShaders_dx11.fsop` (1,000,431 B via `pesfmt_dump cat`): `ColorCorrection_CubeLut.ps` DXBC 2,028 B → 27-line asm: sole sample t0 (L14, point s8); L15–23 piecewise sRGB encode (`max 1e-5`, `div 2.4`, `mul 1.055`, `add −0.055`, `mul 12.92`, `ge 0.0031308`, `movc`); L24 `mul 0.25`; L25 `eq cb5[10].x, 0`; L26 `movc`. t6/t7 declared, never sampled. RDEF: `cPSObject` = cb5 slot 5.
- `SdrDisplayMapping.ps` (1,512 B): `mad (4, 0.055)`, `mul 0.947867334 (= 1/1.055)`, `max 1e-5`, `log·2.4·exp`, `ge 0.00982` select, dark slope `3.09597522e-01` (= 4/12.92 — the doc-digit fix), alpha passthrough.
- `Tonemap_Lum.ps` (1,360 B): UV `(ndc·(0.5,−0.5)+0.5)·cb5[12].zw + cb5[12].xy`, point-sample t0 (s8), `dot709 (0.2125, 0.7154, 0.0721)`, `32·a²` (`mul 32.0`, `max`), floor 0.001. Lacc variant (`Tonemap_Lum_Lacc.ps`, 1,476 B): second tap t1 + `cb5[11]` gate, `movc o0, r0.x<0 ? luma : cb5[11].x`.
- Verdict: GradePort's COMPOSE (`0c2a8bb`, IN MAIN) matches all literals. No code change.

### 2.3 LACC branch shape — CLOSED (no port change)
- `FUN_141684180` decompiles to `return DAT_14373e951;` (one request) — a single global byte selects Lum vs Lum_Lacc in `FUN_141805840` (`if (cVar9 == 0) … else …`, saved decompile lines 97–105).
- Verdict: GF ports the untaken/plain branch (matches every live session: exclusive CubeLut bind, no Lacc pair observed). If a live session ever takes it, the Lacc asm above is the spec. No code change.

### 2.4 Below-horizon 0.18 — CONFIRMED as file-driven (no port change)
- Sun-radiance §1.6 live `cb4[5] = (0.18, 0.18, 0.18, 0)` vs GF default `groundColor = {0.5,0.5,0.5}` (`src/pes/lighting.hpp:64`).
- GF ALREADY reads it: `src/pes/lighting.cpp:311 a.groundColor = V3(*atmo, "groundColor", …)` → `pes_lighting.cpp:242` sets `ground_color`. The 0.5 is only the missing-field default.
- Sky_Draw asm (257 lines, `sky_out/`): below-horizon path selects `cb4[5]` (`movc r1.xyz, r0.z, cb4[5].xyzx, r1.xyzx`, line 240) — consistent with the live 0.18.
- Verdict: no decode gap; IF the install's st021 atmosphere carries 0.18 the frame already matches. One `pesfmt_dump`-level check (read the st021 `groundColor`) would promote this to byte-verified — left as a 1-request GF-side check, not done here.

### 2.5 `param_4`/caller static trace — bounded (kept OPEN, correctly)
- Bridge `xrefs_to?address=0x141804200` → sole code xref `0x142a383f0` [DATA] (one request): the pass is invoked through a function table/vtable, so no static caller falls out of one more bridge request.
- Verdict: the comp-block `+0x8c` writer needs a live session (break on the table slot or trace `param_4` provenance live). Correctly an owner/live ask, not a static-RE ask.

### 2.6 Pitch fmdl roles re-verified — CONFIRMED (no port change)
- `pesfmt_dump fmdl pitch_st021.fpk`: `exp1_pitch_mat [pes3DDF_Pitch2020]` binds t0 pitch_alp / t1 turf_nrm / Spec(turf_srm_dummy) / t3 turf_bsm / t4 pitch_detail / t7 pitch_grain_alp / t5 slope table — no mask role (DarkTurf §1 stands); line/decal + scratch members as documented.
- Install values (Withers 0.78/0.78/0.961/0.098/0.071/0.039, Repair (0.312655, 0.402, 0), Srm, Emissive, U/V 32/24) match `pes_pitch.gdshader` defaults lane-for-lane.

## 3. Corrections to doc claims (contradicted by later work)

1. **Grading-lut dark slope digits** (`pes-render-grading-lut.md` §3.7 §5: `0.30997522`): install asm prints `3.09597522e-01` (= 4/12.92). GradePort §46 already files it. One-line doc fix by Main when archiving.
2. **TurfPlacer §1.4 "cell zero-initialised in the image"**: REFUTED by TurfMask §1.1 (live heap pointer resident in the dumped image cell `0x1437cb0f0`). The cell is written at/after load.
3. **PitchMaterial §1/§3 "T1 DIVERGENCE (CONFIRMED, real)"**: REFUTED. `ng.y·ng − UP = (nx·ny, y²−1, ny·nz)` and `y²−1 = −(nx²+nz²)` for unit `ng` — the file line IS the negated Gram–Schmidt lane-for-lane (worst lane diff 4.8e-13 over 100 k random unit normals; exact case `ng=(0.189,−0.198,0.962)` reproduces the doc's own PES_T1 to 4 d.p.). The scratch `t1_verify.txt` GF_T1 column was computed with a different formula than file line 112 (its `(0,−1,0)→(0,0,−1)` first row matches neither the file line nor the asm). Effect was already 0 on albedo; now there is no divergence at all. Fix: strike the "DIVERGENCE/Port: fix (one line)" lines in PitchMaterial §1/§3 and the PitchV7-adjacent claim; NO shader change (the file line is correct as-is). PitchV7's math is unaffected (different terms), but its doc inherits the "T1 needs fixing" premise in passing — annotate, do not touch the code.
4. **Sun-radiance §6.5-era "below-horizon OPEN (0.5 vs ?)"**: CLOSED as file-driven (§2.4). The parser default 0.5 is not a finding.
5. **ExposureMatch items 2–5 / TurfColour items 1–2,5 / SunPort §8.1a**: closed per §1 (hour frozen; mask 0; WB bypassed; sun colour ported; turf residual = fins). These were live-unknowns at write time, not errors — no text fix needed beyond the status lines in §1.
6. **B2 "exposure_multiplier 2^−10.34 knob"**: gone by construction (GradePort removed the Godot-side multiplier). Doc stays as history; no fix needed.
7. **"Nested stadium cpk hides the turf placer" (TurfFins §2 INF-high)**: demoted — TurfPlacer found the placer in image code (`0x141a34xxx` state machine, 281×159 grid); no nested-cpk read was ever needed. Annotate, do not delete (sibling LODs may still live there — still INF).

## 4. Track B port-ready list (decoded, complete, NOT in GF main)

| # | Item | Branch (origin) | Decode doc | GF change (shape) | Verify |
|---|---|---|---|---|---|
| P1 | Turf3d fin-cover derived equivalent (FIN_NET 0.775 over grass only) | `TurfFinPort` `dd70ad3` | TurfMask §4–§6 | `pes_pitch.gdshader` +20/−3 (`FIN_NET` const + `ALBEDO *= FIN_NET`), `test_model.gd` +7 (source-text `FIN_NET = 0.775` + no-FIN-in-decal checks) | GPU frame st21/day/broadcast-p12 re-measure grass_a..d + giraffe (predict grass → ~1.0×, paint unmoved); Michelson stripe before/after (PES 0.283) |
| P2 | Tonemap_Lum `cb5[12]` sub-rect meter (point-sample gather; full-frame static inputs + `ponytail` until live comp-block rect) | `TurfFinPort` `b3223ac` | ComposePath §4 | `pes_post.glsl` REDUCE gather + params 6→10, `pes_post_effect.gd` 6→10 floats + reduce-set params buf, `pes_lighting.cpp` PostParams, new `test_post_meter.gd` (242 lines) | `godot_post_meter` GPU test (gamescope); EV-trajectory frame check |
| P3 | Whole-body markings (back name/number on the Suit) | — (not started) | kit-compare §5.2 | per B5 port pattern | viewer frame, kit at ~1/3 height |
| P4 | Whole-body monitor picture binding | — (not started) | kit-compare §5.3 | texture-bind check | viewer frame vs decoded replay |

- P1 note: `FIN_NET = 0.775` is a mid-band working value (r = 0.50, f = 0.45; needed 0.67–0.83, predicts 0.775), NOT a PES constant — it ships `ponytail`-marked for the fin-geometry pass, which is the correct policy for a derived equivalent. The 0.303 grassfin survival underneath it IS install-verified (re-verified on the branch from the VFS winner).
- P2 note: with static full-frame inputs the gather is behaviour-neutral vs main (rect = full frame); the value lands when the live comp-block rect is read (§7 owner ask 1).
- P1+P2 are two commits on one unmerged branch (`origin/TurfFinPort`, 2 commits on top of the doc base). Recommend landing as-is (both `ponytail`-marked, tested on branch) then re-measuring one GF frame.
- Out of scope for Track B (Track A): camera items 1–5, kit live indices, floodlight/night paths, B7 placement gaps.

## 5. Remaining OPEN/live/capture asks, consolidated (no duplicates)

**Owner / live-match reads (PES at menu; no live match available to this session):**
1. Comp-block metering rect: live `param_3+0x8c/0x90/0x94/0x98` values at the p12 camera (or any broadcast day frame) + `cb5[12]` GPU values — activates P2's static inputs. (ComposePath §6, TurfPlacer §5.1, TurfMask §5.1, LumaResidual §7.1 — same read.)
2. Fin-pass draw proof at the comparison camera: bound technique id / vertex-buffer bind / instance count for `pes3DDF_Turf3D2020` (D3D hook or GPU readback; outside `pread`). Promotes TurfFins §3 to CONFIRMED. (TurfFins §5.2, TurfMask §5.3, DarkTurf §7.3 — same read.)
3. Mask texel content (`0x1437cb0f0` object) + `cb4[3]/cb4[4]` wind anchors + PlanarShadow `[rbx+0x130]` content at the p12 camera — fixes stripe amplitude and f per box. (TurfPlacer §5.1–2, TurfMask §5.1–2 — same session.)
4. Mask/planar-shadow are runtime GPU targets: the read needs a D3D11/GPU path (frame-capture agent), not more `pread`.
5. Kit live indices (708/760) + match hour confirmation for the comparison frame — match-setup team-block offsets undecoded. (ExposureMatch §6.1–2.)
6. B7 acceptance frames: any placed effect (rain streaks, vignette, net splash, pitch leaf) in a live PES match + the same Godot scene. (B7 §5.)
7. Camera items: ball-in-air lift read, slider/mode writers, FOV damping (camera §84 items 1–5).
8. Floodlight/ungrouped-light rule + night-path conversions: need a 15:00 live match (span check) and night spans. (SunPort §8.3/5/6.)
9. Same-shot like-for-likes: sky-horizon clip (B2), GradePort frame (p12 successor), B5 kit at 1/3 height, stands banding check. One broadcast day session covers 1/2/4/7/9 together.

**Static RE (no owner needed, each < ~20 req, cheapest first):**
- S1. Camera mode writer (`[InplayCamera+8]`, past `0x14087a930`) + slider serializer (`CAMERASETTINGS +0x04/+0x0C`) — ~10 req.
- S2. Whole-body marking placement + monitor binding (kit-compare §5.2–3) — ~10 req.
- S3. Registry-entry creation path (`FUN_1416a3b30` callers) + `param_4` writer shape — ~10 req (static shape only; values stay live).
- S4. `groundColor` install value for st021 (1-request `pesfmt_dump` check promoting §2.4 to byte-verified).
- S5. PitchV7 hardening (log2 guard, ROUGHNESS clamp) — GF-side, no PES read.

**GPU/capture (no owner needed):**
- G1. One GF frame on current main + TurfFinPort P1+P2: st21/day/broadcast-p12 pose, settled EV, grass_a..d + giraffe + Michelson stripes. Decides the fin-cover prediction (0.69–0.86 vs needed 0.67–0.83).
- G2. GF stands-shadow check (stands don't shadow boxes at 15:00) — rides on G1.

## 6. Reproduce (this session's closes)

- Grading pair: `pesfmt_dump cat shaders/dx11/GrSystemShaders_dx11.fsop` → `fsop_extract.py sys.fsop cc ColorCorrection_CubeLut ColorCorrection` + `disp SdrDisplayMapping HdrDisplayMapping GammaControl` + `tonemap Tonemap Tonemap_Lum` → `vkd3d-compiler -x dxbc-tpf -b d3d-asm` (§2.2 literals; `cc_out/`, `disp_out/`, `tonemap_out/` in scratch).
- LACC: bridge `decompile_function?address=0x141684180` → `return DAT_14373e951` (§2.3).
- T1 identity: `python3` — `(x·y, y²−1, y·z)` vs `(x·y, −(x²+z²), y·z)` over 100 k random unit normals, worst lane diff 4.8e-13 (§3.3).
- Caller shape: bridge `xrefs_to?address=0x141804200` → `0x142a383f0` [DATA] only (§2.5).
- Pitch roles: `pesfmt_dump fmdl Asset/model/bg/st021/pitch/#Win/pitch_st021.fpk` (§2.6).
- Kit rule: `docs/re/pes-re-shaders.md` §3.1 + `godot/shaders/pes_uniform.gdshader:103` + port commit `c61d3ad` (§2.1).
- Sky ground: `src/pes/lighting.cpp:311`, `src/gdext/pes_lighting.cpp:242`, `Sky_Draw.ps` line 240 `movc … cb4[5]` (§2.4).

## 7. Report summary

- Surveyed all 21 render docs; every OPEN item is tabled in §1 with its need and post-session status.
- Closed statically: kit-alpha rule (§2.1), grading-pair literals (§2.2), LACC shape (§2.3), below-horizon file path (§2.4), caller-shape bound (§2.5), pitch roles (§2.6).
- Corrections filed (§3): grading-lut digit transposition; TurfPlacer zero-cell refutation; **T1 "divergence" refutation (no shader change)**; below-horizon OPEN closed; span/mask/WB/sun-colour closes marked as status, not errors; B2 knob gone; nested-cpk placer suspicion demoted.
- Port-ready but unlanded: P1 FIN_NET 0.775 (`dd70ad3`) + P2 sub-rect meter (`b3223ac`) on `origin/TurfFinPort`; P3/P4 (whole-body markings/monitor) not started.
- Remaining asks consolidated in §5: 9 owner/live reads (one broadcast day session covers most), 5 static-RE items (S1–S5, cheapest first), 2 GF-side frame checks (G1–G2, no owner needed).
- Recommended next: land P1+P2, run G1 (one GF frame re-measure), then S1/S2 in parallel with owner ask 1–4.
