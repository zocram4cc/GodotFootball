# Grading LUT: matches run the pack's CubeLut stub (CONFIRMED live); t6 LUT bound but never sampled; highlights compress via exposure + sRGB roundtrip clip

> **Correction (RenderSurveyRE, 23-10-26):** the dark-slope constant's digits are transposed below: it is `3.09597522e-01`, not `0.30997522`.

> **Main 19-10-26: CONFIRMED for match frames by the live capture** (bind histogram: only the CubeLut pair bound; live VS md5 `d30ae42a…` = pack `ColorCorrection_CubeLut.vs`, re-checked by Main). Caveat: the pixel shader was not dumped live; its identity with the pack stub rests on the bound pair.

Live PES binds the colour-grading path (registry key `0xcb716e233439` → `ColorGradingParameter`, `lut_s_day_game` live), but the bound `ColorCorrection_CubeLut` program is the pack's 27-line stub that never samples t6. Traced live in a match (BNT vs W, endless clock, frames `match2_dec.png` + `live/now_dec.png`) from the technique slot to the bound pair, its VS bytes, and an 8 s bind histogram, so GF can replace its CubeLut stub (`docs/render/pes-render-b3post.md` open item).

## 0. Script-read constant table (floats read from `PES2021.exe` via section map, never typed)

| VA | value | used at |
|---|---|---|
| 0x14256ae1c | 1.0 | `0x1418044d7` MOVSS (cb5[10].y numerator) |
| 0x142565600 | 0.5 | `0x141804539` MOVSS (bloom-rect half; consistent with sun doc §0 row `(0.5,×4)`) |
| 0x142a38240 | (string `Tone Mapping and Bloom Composition`) | `0x14180426c` LEA (pass name for `FUN_141804200`) |
| 0x143514170 | (bss, live 0.99999900) | `0x1418044df` MOVSS (cb5[10].y denominator) |

Shader-immediate literals below are quoted from `vkd3d-compiler -x dxbc-tpf -b d3d-asm` output (vkd3d 1.17), not typed.

## 1. The shader the GPU runs for the grading pass (CONFIRMED in a live match)

### 1.1 CPU side (CONFIRMED, decompile + disassembly + live match reads)

- Pass setup `FUN_141804200` (253-line decompile; disasm `dis_141804200.txt`). Grading branch lines 213-230: `FUN_1416a44b0` returns hash `0xcb716e233439` (one-store body); `FUN_1416a3b30` registry lookup on the viewport (`+0x430` root, `+0x450` lock); if `local_138[0] != 0`: bind that object's texture (`+0x28 -> +0x20` handle) to **slot 6** (`0x200` call, disasm `0x1418049fe`), overwrite `cb5[13]`/`cb5[14]` (`0xd5`/`0xe5`), set technique `*(u32*)(DAT_1437712e0 + 0x74)` (disasm `0x141804a04`), bind via renderer vtable `+0x128` (disasm `0x141804a5b`).
- Live match (read-only pread, BNT vs W, endless clock 288:21-329:42, 2026-10-19): viewport `0x2195f10` registry root `0xcff0400` holds key `0xcb716e233439` -> node `0x2469fb50` -> `ColorGradingParameter` `0x24680250`; `+0x28` = texture wrapper `0x2524ae40` (pathcode `+0x10` = `0x34093d3d64665f0` = loaded `.ftex`). Technique struct `DAT_1437712e0` = `0x1fed1c0`, **`+0x74` = `0xc40002`** (match; same as menu).
- Binder `FUN_141796a90` (vtable `0x142a161c0+0x128`; full disasm `dis_141796a90.txt`, correcting the first decompile's lost tail-call): index low-`0x3f` `== 2` -> table `DAT_14373c900` (`0x21a0200`, base `0xa7e0000`), entry `(idx>>0x10)*0x30 + base` (0x30 stride in disasm `0x141796b54-0x141796b5c`): entry `0xc4` = `0xa7e24c0`, `+0x24` = `0x000f000c` nonzero -> tail-call `(0x128)(renderer, 0x000f000c)` (disasm `0x141796b62-0x141796b74`). Low `0x0c` -> table `DAT_143743678` (`0x2191200`, base `0x23d0000`), entry `0x0f*0x60 + base` (0x60 stride in disasm `0x141796adb-0x141796adf`) = `0x23d05a0`: `RDX = q(entry+8)` = `0x181cd20`, `R8 = q(RDX+8)` = `0xcfba478`, `RDX = q(RDX)` = `0xcfbc568` -> pending key `(0xcfbc568, 0xcfba478)` via `FUN_141796b90` (ctx `+0x318/+0x320`; disasm `0x141796aed-0x141796b08`).
- Shader registry (`0x181cd20` region) alternates `(progA, progB)` / `(name, owner 0x1bf169f98)` 16-byte entries; short names point at pool `0x1b78f20+`, long names at heap Fox-strings (e.g. `0xcff05b0` = `ColorCorrection_Hdr_Chromatic_Aberration`, read live). Direction proven by the `SdrDisplayMapping` anchor (name `@0x181e0d0` -> bound pair `@0x181e0e0`, the 1.1M-hit top bind): **name precedes its pair**. Hence pair `(0xcfbc568, 0xcfba478)` after the `ColorCorrection_CubeLut` name entry = **the CubeLut program**. Neighbours: `(0xcfbc540,...)` = Chromatic_Aberration, `(0xcfbc590,...)` = DrawLut_Gen, `(0xcfbc5e0,...)` = Hdr, `(0xcfbc400,...)` = plain ColorCorrection (heap name `@0x181cbe0`).

### 1.2 Draw-time capture (CONFIRMED: three independent live signals)

- **Bind histogram** (read-only fast-poll of ctx `+0x318/+0x320` with a held `/proc/mem` fd: 741,414 samples / 5 s and 1,153,262 / 8 s, saved as `bind_hist.pkl`): the CubeLut pair binds **208x / 8 s (~26/s, once per frame)**. Plain `ColorCorrection` / `1DLUT` / `DrawLut_Gen` / `Hdr` pairs bind **0x**. The film path never binds in a live match. Cross-pairs sharing one CubeLut side (`0xcfbc568+0xcfb6778` 120x, `0xcfb90d0+0xcfba478` 101x) are neighbouring-pass rebinds, not film.
- **Live VS bytes == pack VS bytes**: the CubeLut program's VS blob at `0x1c59d00` (672 B, 3 hops from pair side A) dumped live -> `live_vs.dxbc`, **byte-identical** to `cc/ColorCorrection_CubeLut.vs.dxbc`. Fox instantiates VS+PS together from one fsop record at load; the record's PS is the stub in §1.3. (All ColorCorrection VS records share md5 `d30ae42a...` — one fullscreen VS — so this corroborates record-loading while the registry slot pins the technique.)
- **No other bytecode source**: full record scans of all three `*Shaders_dx11.fsop` packs (205/244 records) show no second CubeLut; the exe contains zero `CubeLut`/`g_CubeLut`/`inColorLUT` bytes. The PS bytecode itself is not CPU-readable (freed after D3D compile / driver-side; BFS from pair side B over 159 objects finds no DXBC, one candidate page EIOs), but there is nowhere else it could come from.
- Verdict upgrade: HIGH -> **CONFIRMED**. The match comp pass runs the pack stub.

### 1.3 The stub body (pack record, CONFIRMED static)

- `ColorCorrection_CubeLut` (`GrSystemShaders_dx11.fsop`, PS DXBC 2,028 B): chunks `RDEF` 1008 / `ISGN` 80 / `OSGN` 44 / **`SHEX` 656** / `STAT` 148 with `STAT_nins` = **70**, but SHEX = **15 body lines** (intact siblings match: `ColorCorrection` 93/93, `ColorCorrection_Hdr` 102/102). RDEF: `g_CubeLut` = `texture3d` t6 + `inImage` t0 + `inBloom` t7 (linear-clamp s11), `cPSObject` cb5 / `cPSSystem` cb0.
- Full body (vkd3d, `cat -n` numbers): L13-14 `mul r0.xy, v1.xyxx, cb0[2].zwzz` + `sample_indexable(texture2d) r2, t0, s8` — the ONLY sample: point-sampled scene; **t6/t7 declared, never sampled**. L15-23 piecewise sRGB encode (`max(v,1e-5)`; `log/2.4/exp`; `*1.055-0.055`; `*12.92`; select at `0.0031308`). L24-26 `mul r4, r2, (0.25,0.25,0.25,1)`; `eq r3.w, cb5[10].x, 0`; `movc o0, r3.w, r4, r2` — **x0.25 iff `cb5[10].x == 0`**.
- `cb5[10]` upload (disasm `0x1418044bf-0x141804515`): `.x = (float)(((vp+0x650) & 0xf) == 2)`; `.y = [0x14256ae1c] / [0x143514170]` (= 1.0 / ~1.0); `.z/.w` from `param_4`. Slot id `0xa5`. **Live match: `vp+0x650` low nibble = 0 -> `cb5[10].x = 0` -> x0.25 ACTIVE in matches.**

## 2. The LUT asset (CONFIRMED, VFS + parse + live bind)

- Path: `Asset/model/bg/common/lut/#windx11/lut_s_day_game.ftex`, winner = stock `Data/dt30_g4.cpk` → nested `Asset/model/bg/common.cpk` (2,117 entries; VFS `mounts` + `ls` show the 32-file `lut_*` set; no `download/*.cpk` mount overrides it — 37 download mounts checked by listing; `pesfmt_dump cat` serves the stock bytes).
- Referenced by the rig fox2 (`light_stNNN_af.fpkd` → `sdrGameGradingLut …/Assets/pes16/model/bg/common/lut/lut_s_day_game.ftex`, Fox2Parser OBS) with siblings per stadium/time: `lut_{s,h}_{day,evening,night,cloudy}_{game,demo}.ftex` (+ `lut_sepia_*`, 32 files).
- Format: FTEX `pf 12` (RDEF-consistent RGBA16F), `33×33×33`, 1 mip, chunked zlib (5 chunks; GF `src/pes/ftex.cpp` parses it: pf@8, w/h@10/12, mips@16, chunk table at mip offset). Decompressed 287,496 B = 33³·4ch·2B. Layout **`L[b][g][r]`** (CONFIRMED: `L[0,0,1]` moves R only `0.0208→0.0294`, `L[0,1,0]` moves G, `L[1,0,0]` moves B). Alpha ≡ 1.
- Values (`lut_s_day_game.npy` in scratch, float64 from f16): grey `k=0 → 0.0208`, `k=8 → 0.277`, `k=16 → 0.616`, `k=24 → 0.666`, `k=32 → 0.689`; max|grey − t| = 0.31 (not identity). `lut_h_day_game` byte-identical (md5 `a0df37095419b6c1dc0035140f8652ca` both); `lut_s_day_demo` same md5; evening/night differ (night grey32 = 1.0).
- Sampling: **the running stub never samples it** (no coordinates, no scale/offset, no filtering причаст). For the record, RDEF assigns t6 the linear-clamp sampler s11 (`g_samplerLinear_Clamp`), and the fox2 `ColorCorrectionData` (`colorScale`, `startSlope/endSlope` → `cb4`, per RE doc) only feeds the DrawLut/BlndLut strip builders, which the grading branch bypasses (`ColorGradingParameter` vtable[14] `FUN_1416a4530` binds slot 6 and returns when found — disasm `0x1416a4595–0x1416a45ae` CONFIRMED).
- Before/after tone curve or WB: neither — the LUT output never enters the frame on this path.

## 3. Match-frame composition order (CONFIRMED: bind histogram + pack asm + live frame)

Per-pixel, in order. The comp pass is the CubeLut stub (§1); the film path (WB + UE4 film + 16x16 strip) binds 0x in matches.

1. **Exposure**: pre-applied in lighting (`cb2[23] = 2^EV`, EV ≈ -11 live; no exposure scalar in any comp shader). NOT in the comp pass.
2. **Scene resolve**: stub point-samples `t0` (L14, s8). **Bloom `t7` NOT sampled** (no t7 line) — the film path's scene+bloom add (`ColorCorrection` L25) does not run.
3. **No tone curve in comp**: no UE4 FilmToneMap, no shoulder; film constants `cb5[12..16]` are overwritten with gamma uniforms then ignored. What runs is the **piecewise sRGB encode** (stub L15-23).
4. **No white balance** (stub never reads `cb5[14..16]`; stored WB block bypassed — agrees with TurfColour §1 caveat and sun-radiance §4).
5. **LUT bound but unsampled** (33³ volume at t6; no sample instruction) — no-op.
6. **x0.25** (`cb5[10].x == 0` live in matches).
7. **Display**: `SdrDisplayMapping` — the frame's dominant bind (1,113,522 / 1,153,262 samples ≈ 96%; it is the last bind of the frame, still bound during the present wait). Pack asm (`disp/SdrDisplayMapping.ps.dxbc`, 1512 B): `sample t0` -> `mad r1, r0, (4,4,4), (0.055,0.055,0.055)` -> `*0.94786734 (= 1/1.055)` -> `max(1e-5)` -> `log *2.4 exp` (sRGB-decode branch) vs `ge(0.00982, v)` select with `*0.309597522 (= 4/12.92)` dark branch -> `o.rgb = 0.30997*v + decode*(v>=0.00982)`, `o.a = v.a`. I.e. **`o = sRGB_decode(4x)` exactly** (junction continuous at v = 0.00982: both branches give 0.00303; checked numerically). The x4 is folded into both branches (dark slope 4/12.92, decode of 4v).
8. Net comp+display (in-gamut): `sRGB_decode(4 * 0.25 * sRGB(scene)) = scene` — the pair is an **sRGB roundtrip**, exact identity for scene in [0,1].

Where highlights compress (Main's conflict resolved): **nowhere in comp/display — in exposure plus the roundtrip's [0,1] clamp**. Exposure (≈ 2^-11) scales sunlit HDR into [0,1]; anything still > 1.0 clips at the comp RT (UNORM) or the sRGB encode's representable range — a hard clip, not a film shoulder. Live match frame `match2_dec.png` (OBS-decoded, broadcast view): 99.9th-pct display lum = 1.0, 0.55% of pixels ≥ 0.99 (pure 1,1,1: lines, white kits), i.e. highlights **clip** — consistent with no film curve. The `GammaControl` shader (`o = cb5[10]*|4x|^cb5[11] + cb5[9]`) is the *options/Brightness path*, not the match display path (its pair never appears hot; SdrDisplayMapping does).

Contrast — unused film path (`ColorCorrection`, 93 lines, 0 binds): scene+bloom -> WB dp3 (`cb5[14..16]`) -> UE4 film chain -> gamma-1/2.2 (`pow 0.4545`) -> 16³ 256x16 strip at t6-as-2D (two taps + `*16` lerp) -> x0.25 switch. Intact but dead in matches.

## 4. The remaining ~x4 in the sun chain (pes-render-sun-radiance.md open item)

**Still OPEN, with the grading path eliminated as a suspect and the display x4 explained:**

- The stub's x0.25 + `SdrDisplayMapping`'s x4 (folded into `4x` / `4/12.92`) **cancel exactly** for in-gamut pixels (net identity, §3.8) and are achromatic and sun-independent. **Killed: "a hidden x4 gain lives in the grading/display path".**
- **Killed: "the live CubeLut samples the LUT with a x4-ish domain gain".** The match-bound program is the pack stub, which never samples t6 (§1.2).
- New note: the display stage is `SdrDisplayMapping` (sRGB roundtrip), not `GammaControl`; GF ports must target the former. The old §3 claim "display = GammaControl x4" is superseded (GammaControl is the options path).
- Standing for the sun remainder: H1 (Sky_SunColor seeding column, E-A live `cb3[1]` read — needs a live match, available now), H2 (Godot directional-lux gain, lux-sweep), H3 (span/elevation fold-in).

## 5. Port statement for GF's post pass (supersedes the first-issue version)

1. **Replace GF's film+WB post chain on the grading path with stub + sRGB-roundtrip** (`godot/post/pes_post.glsl` COMPOSE kernel + `src/gdext/pes_lighting.cpp` `PostParams`): when a `ColorGradingParameter` is present (always in matches — registry hit live): skip bloom add, skip WB matrix, skip UE4 film curve; apply piecewise sRGB encode (threshold `0.0031308`, `12.92` / `1.055*x^(1/2.4)-0.055`, floor `1e-5` — stub L15-23 literals); multiply `0.25` (`vp+0x650 & 0xf == 2` bypasses; live match reads 0 so applied); then `SdrDisplayMapping`: `o = sRGB_decode(4x)` with the pack's exact branches (dark: `0.309597522*v`; bright: `((4v+0.055)/1.055)^2.4`, select `v >= 0.00982`; alpha passthrough). Net in-gamut identity — implement both stages literally (relation test `disp(stub(x)) == x` on a ramp, not pinned pack values). Do NOT feed the 33³ volume to Godot's `adjustment_color_correction`: PES binds it but never samples it — sampling it in GF would *add* grading PES does not apply. Delete the GF CubeLut-stub path in the same change (clean cutover).
2. **Do not port any x4 or film shoulder**: there is no x4 constant (the display x4 literally cancels the comp x0.25) and no tone curve in comp/display. Highlight compression = exposure scale + [0,1] clip (frame-measured: 0.55% pixels clip to pure white).
3. `GammaControl` (`o = gain*(4x)^gamma + offset`) is NOT the match display path — leave it for the options/Brightness path if GF ports that.

## 6. Open items

1. ~~Draw-time PS capture~~ CLOSED (§1.2): match-bound pair + VS-byte identity + exclusive bind. Residual: the PS bytecode itself is not CPU-readable (driver-side); a RenderDoc capture could still triple-confirm but is not needed for the port.
2. ~~`vp+0x650` in matches~~ CLOSED: 0 in a live match (x0.25 on).
3. `SdrDisplayMapping` cb inputs (any live variation in its constants; RDEF shows only cb0, so likely static) and the comp RT format (UNORM clamp vs float — decides hard-clip vs rollover above 1.0).
4. LUT domain moot for rendering (bound, unsampled); asset doc stays for archive completeness.
5. `SAP+0x1c0 = 11`, `cb3[3]`, fog (untouched, carried from sun-radiance §6.5).

## 7. Reproduce (scratch `<workdrive>/gf-GradingLut-scratch/`)

- Pack: `pesfmt_dump cat shaders/dx11/GrSystemShaders_dx11.fsop --pes21-root <workdrive>/PES21 > sys.fsop`; `fsop_extract.py sys.fsop cc ColorCorrection_CubeLut ColorCorrection ColorCorrection_Hdr SdrDisplayMapping HdrDisplayMapping GammaControl`; `vkd3d-compiler -x dxbc-tpf -b d3d-asm cc/<N>.ps.dxbc`. Chunk check: `python3` DXBC walk (`SHEX` 656 vs `STAT_nins` 70 for CubeLut; 3476/93 and 3648/102 for intact siblings).
- Image: `PES2021.exe` (base `0x140000000`, section-mapped reads); bridge `decompile_function?address=0x141804200|0x1416a44b0|0x1416a3b30|0x1416a4530|0x141796a90|0x141796b90|0x1416821f0|0x1416827a0|0x1416d00c0|0x1417996f0`, `disassemble_function?address=0x141804200|0x1416a4530`.
- VFS/LUT: `pesfmt_dump ls "Asset/model/bg/common/lut/#windx11"` (32 files); `cat` → ftex parse (pf@8 = 12, 33³, zlib chunks) → `L[b][g][r]` layout test; md5 `a0df37095419b6c1dc0035140f8652ca` (s_day_game = h_day_game = s_day_demo).
- Live (read-only pread via `LiveSample/live.py`, sudo): technique table `*(0x14373ea98)` (55 entries); grading node walk from `vp+0x430` (`vp = 0x2195f10`); `+0x74` of `*(0x1437712e0)` = `0xc40002`; `vp+0x650 & 0xf` = 0 (menu).
- Files kept: `sys.fsop` (+`disp/`, `pescc/`), `cc/*.dxbc`, `live_vs.dxbc`, `bind_hist.pkl` (8 s binds), `fun_*.txt`/`dis_*` decompiles, `w_lut_*.ftex` + `lut_s_day_game.npy`, `match2*.png` (live match 329:42) + `live/now_dec.png` (288:21), this report.
