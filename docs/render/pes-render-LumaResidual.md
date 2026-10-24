# PES→GF pitch-luminance residual 1.2×: sun, shadow, metering inputs and the GF port spec

Status: read-only work; repo at `<workdrive>/GodotFootball`, install VFS at
`<workdrive>/PES21`, scratch at `<workdrive>/gf-LumaResidualRE-scratch/`
(all outputs with absolute paths there; nothing written to the repo).
Read with `docs/render/pes-render-PitchMaterial.md`,
`docs/render/pes-render-ComposePath.md`,
`docs/render/pes-render-TurfColour.md`,
`docs/render/pes-render-SunPort.md`,
`docs/render/pes-render-sun-radiance.md`.

Ticket claim: after the composition match, GF's pitch still carries a ~1.2×
material residual. The shader math and its constants match (PitchMaterial
§1), so the residual is in the inputs. This report resolves the four named
inputs. Bottom line: **the match hour is CLOSED (both sides 15:00, same sun);
the comparison boxes are sunlit on the PES side (shadow term 1.0, INF by
frame evidence); the live metering rect stays OPEN (writer untraced) but is
bounded to second order; none of the three explains a 1.2× material excess —
the hour and shadow legs are now closed as causes, and the open remainder is
the per-material dark-turf chain (PitchMaterial §3 item 4), not these inputs.**

> **Caveat (/');
> The three render docs were written against stadium 21: ExposureMatch §1
> identifies the p12 comparison match as stadium 21 (day, fine, BNT vs W)
> by live strings, and the GF comparison frames are stadium 21
> (`--stadium 21`, SunPort §3). The live match running today (pid 2510857)
> is a DIFFERENT match: stadium 18 (`light_st018_af`, `st018_df`,
> `pitch_st018` heap strings; scoreboard BNT–W). Live values read today
> (exposure, sun row, SAP, grading) therefore calibrate the MECHANISM, not
> the p12 moment. Every live value below is labelled with which match it
> comes from. The p12 moment's own sun/exposure were never recorded live
> (SunPort §5 item 7); the file-derived p12 inputs (§1) do not need a live
> read because st021 day matches are frozen (see §1).

## 0. Script-read constant table (floats read from `pes_full.bin`, offset = VA − 0x140000000, operand VA paired per instruction)

| VA | value | used at |
|---|---|---|
| 0x142578480 | 3.14159274 | `0x140195526b` / `0x140195528a` MULSS (sun/moon ×π, disasm `dis_141955000.txt`) |
| 0x1429dc110 | (0, −1, 0, 0) (only .x read: 0.0) | `0x140195505c` MULPS (fade dot) |
| 0x14256ae1c | 1.0 | `0x1401955115` MOVSS (fade base) |
| 0x142a801d0 | −0.13744450 | `0x14019550b4` COMISS (fadeSun knee) |
| 0x142a801c8 | 0.00518455 | `0x1401955129` DIVSS (fadeSun width) |
| 0x142a801d4 | −0.14262904 | `0x1401955154` MOVSS (fadeMoon knee) |
| 0x142a801cc | 0.01035687 | `0x1401955165` DIVSS (fadeMoon width) |
| 0x142565d20 | (3, 3, 3, 3) | direction-normalize shape (Newton–Raphson) |
| 0x1425658b0 | (0.5, 0.5, 0.5, 0.5) | direction-normalize shape |
| 0x1425a0c50 | (1, 1, 1, 0) | `FUN_141699810` MULPS (cb3 colour scale: identity, CONFIRMED no-op) |
| 0x142588210 | 10.0 | dirty-flag accumulate only (not the colour) |
| 0x142565600 | 0.5 | bloom-rect half (`FUN_141804200`/`FUN_141805840`) |
| 0x14256ae1c re-used | 1.0 | `FUN_141804200` `cb5[10].y` numerator; `DAT_14256ae1c / DAT_143514170` |
| 0x142a801d0 re-used | −0.13744450 | `SunHorizonFade` knee (same constant in GF `pes_lighting.cpp`) |

Live `bss 0x143514170` = 1.0 (CONFIRMED live today, f32 read; prior sessions: INF ≈ 1.0).

## 1. The match hour and the PES sun (CLOSED: both sides 15:00)

### 1.1 The p12 comparison needs no live hour: st021 day matches are frozen at 15:00

- File values (clean rebuild of current `src/pes/*` in scratch `cleanbuild/`,
  stock VFS winner; probes `final`, `sky`, `tsec9` — full output in scratch):
  `light_st021_af.fpkd` aliases st009's fox2; 9 spans
  `TimeSpan1200…TimeSpan2000` with **correct start hours 12–20**
  (the earlier scratch reads showing `startHour=0` used the `.d` union field
  on int-typed properties; the loader's `Num()` reads `.u` — clean-probe
  output: `TimeSpan1500_day 15:00`).
- Day-match slot selection (`TimeOfDaySlotMapper`, lighting doc §2.3): kind 0
  (day) + season → `daySummer`/`dayWinter` slots. st021 has exactly ONE `_day`
  span (`TimeSpan1500_day`), so **every st021 day match — including the p12
  match — uses the 15:00 span regardless of clock**: `sunLux` 150000,
  `skyLightSunScale` 0.1, `sunLightMode` 0 (ALL = dynamic sun + cascades),
  merged global exposure (all span override flags 0: comp −0.33, min −14,
  max −5, key 0.18 — the live Oct reads match this row).
- The sun position input is the span hour (GF `pes_lighting.cpp:106`:
  `useHour = span.startHour` since `overrideDateTime` is 0): **15:00**.
  `timeAdvanceMode` = 0 (file), and no sun movement was seen during 4 min of
  play (lighting doc §3.1: the 175-step/day move is not observed live) — the
  sun is frozen for the whole match. There is no per-minute hour to read;
  the "match hour" of the p12 frame is the span hour, 15:00, by construction.
- GF does exactly the same (`match.gd` default `hour=-1` →
  `SpanForDayNight(day, season)` → `TimeSpan1500_day` → `useHour=15`;
  CONFIRMED in code, `src/gdext/pes_lighting.cpp:103-107`).
- **Status: CLOSED.** The hour cannot carry the residual: both sides render
  the 15:00 span and the 15:00 sun.

### 1.2 The 15:00 sun constants actually uploaded (file-derived, clean probe)

`final`/`sky` output (stock VFS, current source — this supersedes the stale
`atmo_probe` numbers quoted in PitchMaterial §0 where they differ; the
`toSun` value itself is identical):

- Atmosphere: lat 45, lon 90, gmt **1** (int-typed file value; the loader
  carries it — earlier `.d`-field reads showed 0), north 90,
  date 2019-03-06, `skyColorSunScale` **0.2**.
- `toSun(15:00)` = **(0.2519, 0.3909, −0.8853)** (GF `SunToDirection`, clean
  probe) — **NdotL = 0.3909** for an up normal. Identical to PitchMaterial
  §0's value (the archived probe was correct after all; the interim
  contradiction in scratch notes came from hand-set gmt=0/date=0 in scratch
  harnesses, not from the loader).
- `AtshSunColor(15:00)` = **(131008, 131008, 131008)** — saturated equal
  channels (AtshHalf exponent-31). Sun chroma WHITE (achromatic).
- `SunColour(15:00)` (GF) = **(0.3628, 0.3628, 0.3628)** = π·S·chroma with
  S = 0.2 (|.| = π·0.2 = 0.6283). Achromatic.
- `AtshSkySh(15:00)` row0 = **(12024, 14360, 19984)** — mildly blue
  (B/R 1.66), rows 1–8 carry ±(200–6600) (clean `sky` output). PitchMaterial
  §0's "all nine rows achromatic" is refuted by the clean probe (it was
  based on the capped `atmo_probe` print, which only showed row0 as
  `(12020,12020,12020)` — re-read: the archived doc row sh0 = 12020×3 is
  wrong; the file value is (12024,14360,19984)).
  Effect on the boxes: the sky ambient is a small blue-ish fill. GF feeds
  these rows to `pes_sky.gdshader` (`AtshSkySh` at `pes_lighting.cpp:235`).
  PitchMaterial §2 already bounds the ambient share: GF sky ambient is 0.064
  of sun-grass vs PES candidates 0.108–0.269 — **weaker in GF, wrong
  direction for a GF excess**. Not the residual cause (CONFIRMED direction).
- Radiance scales: `SkyRadianceScales` sun = 150000·π·0.2 = **94247.8**,
  star = π·0.2 = **0.628319** (== the live moon-slot value, sun-radiance
  §1.6). `fade15` = 1.0 (sun high above the knee).
- Uploaded live sun row (today's st018 match, 0x13321440 family,
  R==G≈157746→157905 drifting +0.03%/min): magnitude ≈ 223087 ≈
  150000·π·1.0·0.473. The 0.473 is the transmittance-like factor for the
  live (evening?) st018 sun, NOT a constant — do not port it. For the p12
  (st021, S=0.2, 15:00) the E-A result stands: row |.| = I·π·S·0.998
  (sun-radiance §1.6, Oct-19 BNT-vs-W match).

### 1.3 Predicted effect of the hour on the box luminance

None (both sides 15:00 by construction). For the record, neighbouring-hour
sensitivity with the file atmosphere (clean probe, up normal):
NdotL(13h)=0.035, 14h=0.218, **15h=0.391**, 16h=0.542 — a one-hour error
would be ±40–90%, i.e. far larger than 1.2×, and it is excluded by the
single-`_day`-span file structure.

## 2. Shadow state at the comparison boxes (INF sunlit; term 1.0 on the PES side)

- Frame evidence (`~/ghidra-projects/scratch/obs/p12_dec.png`, decoded):
  the far-pitch strip (rows 130–160, the `grass_top_right`/`far_touchline`
  neighbourhood) is flat (≈0.075, no dark band = **no stand shadow on the
  pitch**); every comparison box is unimodal at the mow-stripe level
  (p95/p5 1.38–1.61; `far_touchline` 2.51 includes the line paint;
  `halfway_line` 9.91 is the line itself) — no bimodal shadow distribution.
  Player shadows are contact-only at this resolution (no cast blobs beyond
  the body at 2 px). The boxes (grass_a…d, top_right, touchline, halfway)
  are all mid-pitch open turf.
- Shader side: the pitch sun pass is one of the `SSLighting_Sun*` family.
  The shadowed variant multiplies the diffuse by a tap-averaged term that
  returns exactly 1.0 outside shadow (`movc` on the cascade test fail,
  then `1 − avg·0.125` shaped to 1 when taps read lit; full 8-tap code in
  `SSLighting_Sun.ps.asm` L150–200, scratch copy from the TurfColour
  session). For sunlit pixels the shadow term is **1.0 under either variant**,
  so the variant question does not move the box values.
- Span state: `sunLightMode` = 0 (ALL) on all st021 day spans — dynamic sun
  with cascaded shadows where casters exist; nothing is cast over the open
  boxes (frame evidence above). `GrShadowSettings.method` = 1 (precomputed
  VSM for the static stadium) + `shadowRange` 120 / `Extra` 80 /
  `hiRes` 35 / `Projection` 30 / `selfShadowBias` −15 (file).
- GF counterpart (`src/gdext/pes_lighting.cpp:133-139`): `sun->set_shadow(true)`,
  `SHADOW_PARALLEL_4_SPLITS`, max distance = `shadowRange`,
  blur/opacity from the file; `godot/shaders/pes_pitch.gdshader:205,214`
  multiplies by `LIGHT_COLOR·ATTENUATION` (Godot applies its PSSM before
  `light()`). Whether GF's stands cast onto GF's pitch boxes at 15:00 is
  **not assessed** (no GF framebuffer read in this session); any such
  shadowing would darken GF (wrong direction for the excess), and so is
  bounded as ≤ a few % brightening-equivalent at most — it cannot produce a
  1.2× GF excess either way.
- **Status: INF (high).** PES box shadow term = 1.0 by frame + shader
  structure; GF side unassessed but wrong-signed for the residual.
  Predicted effect on the boxes: **0% from the PES side** (both variants give
  1.0 on sunlit turf). Port spec: no shadow change; optionally verify once
  with a GF capture that the stands do not shadow the pitch boxes at 15:00.

## 3. Metering rectangle `cb5[12]` (live inputs captured; GPU values OPEN)

- Static (CONFIRMED, decompiles `fun_141804200.c` + `fun_141805840.c` in
  scratch, Ghidra bridge 2026-10-24): `FUN_141805840` uploads `cb5[12]`
  three times — (a) `0xb5` full-rect from target dims, (b) `0xb5` meter-rect
  from `vp+0x584/0x588 × light-scale × vp+0x4a8`, (c) `0xc5` clamped rect
  from the **comp parameter block** `param_3+0x8c/0x90/0x94/0x98` scaled by
  `bss 0x143514170` (= 1.0 live). The Tonemap_Lum UV is
  `uv = (ndc·(0.5,−0.5)+0.5)·cb5[12].zw + cb5[12].xy` (sub-rect of the
  source). Reduction weights are arithmetic (no log/exp anywhere in
  `Tonemap_Lum`/`blur_box2D`/`blur_kawase` — ComposePath §4).
- Live inputs captured today (st018 match, read-only pread):
  `vp+0x584/0x588` = 1280/720, `vp+0x4a8` = (1644.08, −1, 1, 0),
  `vp+0x4f0..` = (0, 1, 1, 0), Lmeas `+0x410` = 0.22714 (RGBA, W = 1),
  copy `+0x420` = 0.22714, luminance targets `+0x5e8` =
  (0x1c29280, 0x1c29400), index `+0x660` = 1,
  exposure EV −11.309/min −13/max −9/comp **+0.33**/key 0.18
  (`+0x4c4/+0x4c8/+0x4cc/+0x4d0`), plugin film = the span set
  (0.88, 0.55, 0.26, 0, 0.04, 6500, **tint 0.05**), speed **30**,
  bypass `vp+0x650` low nibble **0** (×0.25 ACTIVE).
- The `0xc5` writer (the comp block's `+0x8c` rect) is **not traced**:
  `vp+0x8c` reads (0.386, 1.455, −0.634, 0.141) — view-matrix rows, not a
  rect — so the metering `param_3` is the comp block, a different object
  whose writer stays OPEN (ComposePath §6 carries it). GPU `cb5[12]` values
  are unreadable with the read-only toolbox (no GPU readback).
- GF counterpart (`godot/post/pes_post.glsl` REDUCE + LAW,
  `godot/post/pes_post_effect.gd`): arithmetic mean over the **full frame**,
  `max(dot·2^EV, 0.001)` floor, same law line-for-line (ComposePath §5);
  deviations carried as `ponytail`: full-frame vs sub-rect, `32·a²` term
  omitted, LACC branch unmodelled, `×0.25` bypass unmodelled (live 0 = gain
  always applies).
- Predicted effect: the rect only reweights which pixels set the EV; at
  equilibrium the frame output is key-normalised, so a global scale error is
  invisible in the frame and visible only in the EV (SunPort §2 E8 result:
  same frame within 1%). Uniform-frame bound is tenths of an EV (SunPort §5).
  The residual under test is measured **at matched EV**, so metering cannot
  carry a 1.2× material excess. **Status: OPEN live values, CLOSED as a
  residual cause.**
- Port spec: no change. If the `param_3+0x8c` writer is ever traced, model
  the sub-rect in REDUCE (one uniform + UV scale/bias); until then the
  full-frame mean stands (wrong-signed or neutral at matched EV).

## 4. What GF uses instead (port spec — mostly "already matches")

| Input | PES (this report) | GF (current `origin/main`) | Delta / action |
|---|---|---|---|
| Match hour / span | `TimeSpan1500_day`, frozen (only `_day` span) | `SpanForDayNight(day)` → same span; `useHour=15` | **None.** Both 15:00. |
| Sun dir / NdotL | (0.2519,0.3909,−0.8853), NdotL 0.3909 | `SunToDirection` identical (§1.2) | **None.** |
| Sun radiance | lux = `sunLux·fade` (150000·1); colour π·S·chroma, white | `pes_lighting.cpp:128` lux; `SunColour` (0.3628³) sRGB-encoded; SunPort `6923da2` | **None** (SunPort landed). |
| Sky/SH ambient | SH15 row0 (12024,14360,19984), mildly blue; small share | `AtshSkySh` → `pes_sky.gdshader`; `AMBIENT_SOURCE_SKY` | **None for luma** (weaker than PES, wrong direction). Blue deficit is a separate (sky-fill) item, SunPort §8 item 4. |
| Shadow at boxes | term 1.0 (sunlit, §2) | Godot PSSM applied in `light()` | **None**, pending one GF capture proving the stands don't shadow the boxes. |
| Metering | arithmetic mean over `cb5[12]` sub-rect (values OPEN) | arithmetic mean, full frame | **None** (neutral at matched EV). |
| Composition | CubeLut stub (t0 only, ×0.25, no WB/LUT) + `SdrDisplayMapping` | `pes_post.glsl` COMPOSE pair, params (key, comp, min, max, speed, stepGain) | **None** (GradePort `0c2d8bb` on origin). |

## 5. Attribution of the 1.2× (status per leg)

- Hour/sun inputs: **CLOSED, 0%**. Both sides 15:00, same file, same
  direction (NdotL 0.3909), same white chroma, same π·S scale.
- Shadow at the boxes: **INF, 0% from PES** (term 1.0); GF side wrong-signed.
- Metering rect: **OPEN values, CLOSED as cause** (neutral at matched EV).
- Remaining open remainder (NOT these inputs): the per-material dark-turf
  chain — GF grass stays ≈1.2–1.5× while the giraffe paint matches
  (≈0.6–1.0×), i.e. the gain is concentrated in dark turf, which a sun- or
  exposure-scale error cannot produce (both scale all materials equally).
  Next RE per PitchMaterial §3.4b and sun-radiance §2 H4/H5: the
  albedo/diffuse chain absolute level for dark turf (NOT the shader math,
  which matches — an input scale: mask-unbound semantics, `32·a²` alpha
  path, or a GF-side second light at the metered pose), verified by a
  matched-pose frame on current main with the settled EV.

## 6. Reproduce

- File values: `final` / `sky` / `tsec9` sources in
  `<workdrive>/gf-LumaResidualRE-scratch/` + `cleanbuild/libpesfmt_clean.a`
  recipe (§notes above); stock VFS winner; no repo writes.
  `PES21_ROOT=<workdrive>/PES21 ./final` prints spans, day span, sun dirs,
  atsh/SH (needs the `cleanbuild` lib — the checked-in `build/` tree is
  stale: `build/libpesfmt.a` predates `SunColour`, and its test binaries
  list only 9 of the file's 13+ tests).
- Live reads: `sudo -n python3` + `LiveSample/live.py` (`os.pread` on
  `O_RDONLY /proc/<pid>/mem`, pid re-resolved per call; one small read per
  VA; short region scans only for string/registry work). Nothing pinned,
  no writes, no attach. Techniques/registry: §2 table + grading walk
  (`root *(vp+0x430)` → node `0x286bdcf0`, exact key `0xcb716e233439`,
  value `0x1a318b90` → wrapper `+0x28 = 0x28b07420`, pathcode
  `0x34093d3d64665f0`, LUT names `lut_s_day_game`/`lut_s_day_demo`/
  `lut_h_day_*` — grading path CONFIRMED live today, WB bypassed).
- Decompiles: Ghidra bridge `decompile_function?address=0x141955000`
  (`fun_141955000.c`), `0x141804200` (`fun_141804200.c`), `0x141805840`
  (`fun_141805840.c`), `0x141699810` (`fun_141699810.c`, 33 lines:
  colour ×(1,1,1,0), dirty-flag ×10 only); disassembly
  `dis_141955000.txt` (π at the two MULSS, fade knees/widths, 3/0.5
  normalize shapes — all paired with operand VAs in §0).
- Frame: `~/ghidra-projects/scratch/obs/p12_dec.png` (OBS double-sRGB
  decoded; 1280×720). Box statistics script in scratch notes.

## 7. Open items (numbered; owners in brackets)

1. Live `cb5[12]` GPU values + the comp-block `+0x8c` writer (tracer: xref the
   caller of `FUN_141805840` for its `param_3`). Bounded second-order;
   not the residual.
2. GF capture proving the stands don't shadow the pitch boxes at 15:00
   (one frame on current main; wrong-signed in any case).
3. The per-material dark-turf remainder (albedo-chain absolute level for
   dark turf; §5). The actual next RE step — not sun/shadow/metering.
4. Housekeeping found en route (not this ticket, no code touched):
   `build/` is stale (predates `SunColour`; test binaries list 9 of 13+
   tests); `LoadLightRig(21)`-style fresh-harness links against it produce
   ABI-soup crashes — rebuild `build/` before trusting any fresh test
   binary. `bss 0x143514170` confirmed 1.0 live today.
