# PES re-port audit 4: SunPort, GradePort, PostPort, TurfFinPort (+ exposure, b2 lighting)

Read-only audit. GF code from `origin/main` (`eae5607`), PostPort `b3223ac`, TurfFinPort `dd70ad3`
(`git show <ref>:<path>` into `<workdrive>/gf-PortAudit4RE-scratch/`). Image
`~/ghidra-projects/scratch/Unpacker/pes_full.bin` (offset = VA − 0x140000000); Ghidra bridge
(one request at a time); constants below read from the image by script unless marked asm-literal.
No repo edits. Ticket order: SunPort (§1), GradePort (§2), PostPort (§3), TurfFinPort (§4),
exposure + b2 (§5). Reference docs: `docs/render/pes-render-SunPort.md`,
`pes-render-sun-radiance.md`, `pes-render-LumaResidual.md`, `pes-render-GradePort.md`,
`pes-render-grading-lut.md`, `pes-exposure-ExposureMatch.md`, `pes-render-b2lighting.md`,
`pes-render-b3post.md`, `pes-render-TurfFins.md`, `pes-render-TurfMask.md`,
`pes-render-TurfColour.md`, `pes-render-PitchMaterial.md`; branch reports
`pes-port-PostPort.md`, `pes-port-TurfFinPort.md`, `pes-port-SunLuma.md`.

Headline: **no sun scale/unit error explains the split** (SunPort exonerated, §1 — message sent to
SunLuma). GradePort and PostPort **PASS** (one advisory each). TurfFinPort **CONDITIONAL PASS**:
arithmetic and paint-immunity check out, but the adopted coverage sits below its own derivation
range and the (r, f) pair was picked to meet a stale band (F1–F3 below). Exposure law and b2
**PASS** with two carried advisories. No code fix is proposed for any port: the remaining grass
excess needs the live mask/fin read or an owner re-derivation decision, not a silent retune.

Box baseline used (PostPort `b3223ac` frames, p12 pose, settled): grass 1.63–1.70×, paint 0.75×
(contaminated per SunLuma — clean-paint value is SunPort F3's 0.62×), frame mean 1.31×.
TurfFinPort `dd70ad3`: grass 1.40–1.47×, paint 0.74× (same contamination caveat), frame 1.16×.

## 1. SunPort (on main) — EXONERATED as the split cause; PASS with 2 advisories

Landed code: `src/gdext/pes_lighting.cpp:109-131` (sun block), `:146-148` (moon),
`:226-230` (sky scales); `src/pes/lighting.cpp:548-556` (`SunHorizonFade`), `:693-705`
(`SunColour`, `SkyRadianceScales`).

- **F0 (the ticket's question — ANSWERED, negative): no scale or unit error in the sun raises lit
  grass while lowering paint.** GF product per channel: `150000 × π·0.2/√3 = 54414` (white).
  PES E-A row (`0x13321440+k·0x140`, sun-radiance §1.6): `(66526, 66526, ~15, 0)`.
  R,G: GF/PES = **0.818×** — GF is darker (wrong direction for a 1.63× grass excess), and in any
  case global, hence absorbed by the EV at settle (SunPort §2 E8: same frame within 1%).
  B: GF/PES ≈ 3600× relatively, but luminance-negligible: 0.0721 weight × grass-B albedo
  0.0018 (PitchMaterial §1 nominal `(0.0264, 0.0455, 0.0018)`) — grass-Y effect ≈ +0.3%.
  Paint-Y likewise ≈ 0.82× global. Magnitude `|row| = I·π·S·0.998` CONFIRMED (script-read π
  `3.14159274` at `0x142578480`, MULSS `0x14195526b/28a` re-read this pass; fade knee
  `−0.13744450`/`0.00518455` re-read). A global sun scale **cannot** split grass-up/paint-down at
  matched EV — the split must be per-material (mask-B/fins/albedo, §4). No sun fix proposed.
- Coverage that checks out: `lux = sunLux·fade` (no π — custom `light()` has no 1/π, SunPort §3
  probes); colour `π·S·chroma` sRGB-encoded for Godot (`SunColour`, L2-white 0.3628 at 15:00);
  `SkyRadianceScales` sun/star = lux·π·S (live moon slot 0.6283); `.atsh` saturated white at 15:00
  (131008×3); `toSun` NdotL 0.3909; fade knees/widths match image immediates; `SpanForDayNight`
  → `TimeSpan1500_day` both sides (LumaResidual §1 CLOSED).
- **A1 (advisory): chroma distribution is white vs PES's R==G,B~0 row — real, ~achromatic in Y,
  EV-absorbed.** GF distributes `π·S` as white; PES's row carries it as R==G,B≈15 (Sky_SunColor
  seeding column + B-killer, still OPEN per SunLuma §2). Y-effect ≈ 0.82× global (computed §above
  with the nominal texel). Porting the live R==G shape would darken GF grass ~18% in R,G but is
  live-derived, not a file constant — correctly not ported. No action.
- **A2 (advisory): moon misses π·S and the sRGB encode** (`pes_lighting.cpp:146-148`: lux raw,
  `set_color` raw `moonColor`; cf. sun `linear_to_srgb` at `:131`). Day-hidden
  (`set_visible(!sunOn)`); at night floodlights (8M lm) dominate the moon (lux 1) by ~7 orders —
  frame effect negligible. Note only; revisit only if a moonlit-night frame is ever compared.

## 2. GradePort (on main, `0c2a8bb`) — PASS, no findings

`godot/post/pes_post.glsl` COMPOSE (`cube_lut_stub` + `sdr_display_mapping`), params 6 floats.

- All 14 literal pairs check out against the pack asm (`ColorCorrection_CubeLut.ps` L13–26,
  `SdrDisplayMapping.ps` L9–23): CUBE_LUT_GAIN 0.25, DISPLAY_DECODE_GAIN 4, INV_SCALE 0.947867334,
  BREAK 0.0031308, slopes 12.92/1.055/2.4/1e-5, DARK_BREAK 0.00982, DARK_SLOPE as `4.0/12.92`
  computed in-shader (bit-exact; the doc-digit transposition is corrected in the code form and the
  grading-lut doc carries the correction note). Net `clamp(2^EV·scene,0,1)` identity in-gamut —
  the stub ×0.25 and display ×4 cancel exactly (grading-lut §3 CONFIRMED live: film path binds 0×).
  GPU test red→green with 8 mutations run+reverted; `pes_lighting_test` 13/13.
- Deviations carried honestly as `ponytail`: `vp+0x650` ×0.25 bypass unmodelled (live reads 0,
  gain applies); no LUT sampling (PES stub never samples t6 — nothing to port); bloom passes
  removed (no consumer: comp never samples t7). Frames: grass hue fixed (R/G 0.59 vs 0.585),
  blue deficit remains (B/G 0.007–0.01 vs 0.05–0.08 — upstream sky ambient, not grading).

## 3. PostPort (`b3223ac`, not in main) — PASS with 1 carried advisory

`godot/post/pes_post.glsl` REDUCE gather + 10-float params; `pes_post_effect.gd` PARAM 10/40;
`src/gdext/pes_lighting.cpp:80-88` full-frame static rect, `ponytail`-marked.

- **Gather math CORRECT vs `FUN_141805840`:** `uv = (ndc·(0.5,−0.5)+0.5)·zw + xy`, point load,
  clamped to target — matches Tonemap_Lum's s8 point-sample + clamp (ComposePath §4). Full-frame
  rect (zw=1, xy=0) reduces to the identity pixel-for-pixel (verified endpoints px 0→0,
  size−1→size−1), so `before==after` frames (max abs diff 0) are EXPECTED, not a no-op bug; LAW
  unchanged in shape; the en-route `totalHits`/`hitAcc` 16× bug was caught by the new GPU test
  and fixed in the shipped commit. Params `[6..9] = (0,0,1,1)` are PES's decoded static inputs
  with the untraced comp-block rect at zero and live `bss 0x143514170 = 1.0` — a full-frame rect by
  construction, honestly marked. Test: full-rect law target + left-half split-frame (meters bright
  only) + degenerate corner clamp; M2 mask-semantics correctly fails; chroma unchanged (R/G 0.581
  vs 0.585 — no WB in path, as claimed).
- **A3 (advisory, carried from the branch report): M1 (rect scale ×0.5) SURVIVED** — uniform-field
  full case is scale-blind by construction and the split-frame bright region covers the halved
  rect. Add a non-uniform rect test (quarter-rect on a gradient field) when the live `param_3+0x8c`
  rect lands. Not blocking: the shipped full-frame path is exactly covered.
- Strike the T1 item from the ticket: `ng.y·ng − UP = (nx·ny, ny²−1, ny·nz)` IS the negated
  Gram–Schmidt lane-for-lane for unit ng (`−(nx²+nz²) = ny²−1`; numeric diff 3.1e-15/1000 normals).
  PitchMaterial §1's "DIVERGENCE (CONFIRMED)" with slope-v 0.540-vs-0.389 is refuted by this
  algebra — all three spellings coincide; specular-only in any case. No change (PostPort already
  ships none).

## 4. TurfFinPort (`dd70ad3`, on PostPort) — CONDITIONAL PASS (3 findings, no silent fix)

`godot/shaders/pes_pitch.gdshader`: `const float FIN_NET = 0.775; ALBEDO *= FIN_NET;`
(post-`clamp(ao·uOut+tOut)`, asm-112 site); `test_model.gd` grass-has/decal-has-not assertions.

- **F1 — the adopted coverage sits BELOW its own derivation range.** TurfMask §4 derives fin
  coverage f = 0.40–0.50 from geometric layers 2.5–7.5× × alpha survival 0.303, but TurfFins §1.3
  derives per-pixel fin-fragment probability f ≈ 0.53–0.90 (Poisson cover, effective layers
  0.75–2.3). The adopted f = 0.45 is below the §1.3 range; the single-layer (no-stacking) value is
  f = 0.303 → net 0.85, not 0.775. Either derivation leg is honestly documented, but they are
  mutually inconsistent, and the shipped (r, f) = (0.50, 0.45) was picked to meet the
  stale 0.67–0.83 band (DarkTurf-era 1.2–1.5× baseline), not derived box-by-box. On the current
  1.63–1.70× baseline 0.775 predicts ~1.26× yet measures 1.40–1.47× (post-chain nonlinearity) —
  and the gate (±10%) is still not met. Fix (owner decision, NOT a silent retune): full fin
  geometry pass (TurfFins §4.1) with real per-box f from the mesh + live mask, or a re-derived
  (r, f) against the current baseline with the stacking assumption settled. `// ponytail:` marker
  present and correct — keep until the mesh ships. Paint-immunity CONFIRMED by construction
  (decal mesh untouched) + `test_model` assertion + giraffe 0.75→0.74× (render noise).
- **F2 — ALBEDO-scale vs G-buffer mix: diffuse-equivalent, specular unscaled (minor, bounded).**
  PES mixes shaded fin fragments over shaded surface (`o0 = v3·v1.z·r0.y`); GF scales ALBEDO
  pre-light. For diffuse-dominated grass these coincide (0.775× both); GF's `SPECULAR_LIGHT`
  uses full-sun `LIGHT_COLOR`, unscaled. Grass spec is ~1e-3 of frame (X4: SPECULAR=0 identical
  to 4 d.p. — ExposureMatch/TurfColour). Effect < 0.5% on the boxes. Note only; the full fin pass
  (F1) subsumes it.
- **F3 — shadow·fade ship as 1 (honest stand-in, OPEN drivers).** The formula's `v1.z`
  (planar-shadow density) and VS distance fade need the runtime mask target + wind anchors
  (`cb4[3]/cb4[4]`), GPU-side per TurfMask §5 items 1–2. The `ponytail` names the fin pass that
  removes it. No action until the D3D readback exists. Stripes consequently absent
  (Michelson 0.203 vs PES 0.283, as predicted).
- Ruled out as split causes (for the record): mask-R/gW lift would BRIGHTEN PES grass (wrong
  direction for a GF excess — E3: mask 0.5 brightens GF 1.9–2.5×); missing-fin darkening is the
  right sign but capped at the F1 numbers above; sun chroma (§1) is achromatic in Y.

## 5. Exposure + b2 lighting (budget allowed) — PASS with 2 carried advisories

- **Exposure law PASS (no change).** Main's LAW is `FUN_1416c7930` line-for-line (clamp EV0, Lc =
  Lmeas·2^−comp, target with ½ log2(key/Lc), rate step with min(|Δ|,0.5) gate); params key 0.18 /
  comp −0.33 / min −14 / max −5 / speed 10 match the live 15:00 span reads; floor −14 is PES's own
  value (it binds only because the scene is too bright — a symptom, not a law error). The ~3.2 EV
  gap is settled downstream of the law (scene excess, §1+§4), not in it.
- **b2 light rig PASS with advisories:** 147 Spot/PointLights with parent-chain transforms, 9
  probes, NOAA-fit sun dir, `.atsh`/SH9/span parse all hold. **A4:** `GroupOn` returns true for
  ungrouped lights (`chain.empty()`) — the 3 `SL_Room_*_DemoAdd` spots stay ON in GF; PES's
  ungrouped-light enable rule is unverified (SunPort §8 item 5). Frame effect at the p12 pose is
  bounded small (H4 runs identical per SunPort §6), but the rule stays OPEN — no change without a
  live 15:00 enable read. **A5:** `pes_sky.gdshader` default `sun_intensity = 471239`
  (= 150000·π·1.0, S=1.0) is stale for st021 (S=0.2 → 94248) — always overwritten by
  `set_shader_parameter` in `setup` (rig_view included), so harmless; refresh the default to the
  st021 value or mark it stale so a no-setup path can't silently render S=1.0.

## Coverage list (what checks out — no action)

Sun: lux·fade, π·S colour + sRGB encode, fade knees, toSun/NdotL, span/hour freeze, moon hidden
by day, sky lux·π·S scales, SH9 rows, fog/tonemap-linear/probe wiring. Post: all 14 grading
literals, stub+display identity, REDUCE-gather/clamp, LAW, EV slots/ext wiring, alpha passthrough,
[0,1] clip. Pitch: every PS lane 33–186 vs `pes_pitch.gdshader` (Withers/Srm/Repair/AO/Emissive/
U·V/slope-chain/normal/GB2/decal — PitchMaterial §1), T1 identical (§3), mask-unbound ≡ 0 both
sides. Turf: grassfin survival 0.303 re-verified, FIN_NET arithmetic 1−(1−0.5)·0.45 = 0.775,
paint-immunity. Docs: grading-lut dark-slope correction already applied in-code.

## Verdicts per port

| Port | Verdict |
|---|---|
| SunPort (main) | **PASS** — no sun scale/unit error; split is per-material. A1 chroma shape + A2 moon advisory. |
| GradePort (main) | **PASS, no findings.** |
| PostPort (`b3223ac`) | **PASS** — gather + LAW + rect wiring correct; A3 test blind spot carried. |
| TurfFinPort (`dd70ad3`) | **CONDITIONAL PASS** — F1 coverage below derivation range / tuned pair (owner call), F2/F3 bounded+marked. |
| Exposure law | **PASS** — line-for-line, live params, floor is PES's own. |
| b2 lighting | **PASS** — A4 ungrouped-light rule + A5 stale sky default advisories. |

## Recommended next (highest value first)

1. Owner call on F1: full fin geometry pass vs re-derived (r, f) against the 1.63–1.70× baseline
   (SunLuma's capture + clean giraffe boxes feed this; E9-anchored 2.15× needs re-anchoring on the
   current CubeLut-stub chain per SunLuma §3/§6).
2. Trace the metering `param_3+0x8c` writer (caller of `FUN_141805840`) + `32·a²` alpha writer —
   second-order at matched EV, but closes LumaResidual §7 items 1–3.
3. Decode the Sky_SunColor seeding column + B-killer (A1) and the live mask-B content (F3 driver) —
   both need D3D/GPU readback or a frame-capture agent; outside the read-only toolbox.
4. A5 one-liner (stale sky default) + A3 quarter-rect gradient test when the live rect lands.
