# TurfColour: white balance is applied as the stored block's columns (the fix was wrong and is reverted); the exposure gap is a direct-sun radiance scale, not the key, compensation, clamp, or metering type

> **Update (PitchShaderRE, `pes-render-PitchMaterial.md`):** the pitch shader and its install constants match GF term for term; the 2× red is GF's white-balance in post (PES's frame is un-white-balanced), and the luma splits into ~1.7× global (metering content, INF) and ~1.2× material residual (OPEN). One small shader divergence (the T1 frame, specular only).

Branch `TurfColour` on `origin/main` `8fc3292`. Commits: `4397248` (cherry-pick of ExposureMatch `db1f2a6`, `match.gd --camera-pose`, kept for reproducing the pose), `99b9ae9` (the rows fix, now refuted), `17aef74` (revert of the WB orientation; `git ls-remote origin TurfColour` = `17aef745a3271708dbe6938a9c4e2a577b9bacfb`). No other code is kept. The branch holds no experiment edits.

## 0. Verdicts

| # | Claim | Verdict | Evidence |
|---|---|---|---|
| 1 | White balance: the stored rows are applied as rows | **REFUTED. Reverted.** PES applies the stored block's columns (= the transpose), as the original GF code did. | §1 |
| 2 | Turf albedo path (WithersParameter5 floor) | Not the cause for the WB; the path is faithful to asm 97–112 (ported earlier) | ExposureMatch §4; §4 here |
| 3 | Exposure key 0.18 and compensation −0.33 differ | **Ruled out (live).** Live key = 0.18, compensation −0.33 in the game state | §2.1 |
| 4 | Clamp [−14, −5] differs | **Ruled out (live).** Live min −14 in both states; max −5 (game state) | §2.1 |
| 5 | `32·a²` term in Tonemap_Lum | **Present in asm; inert on lit pixels and wrong in direction. Not the cause** | §2.3 |
| 6 | Log-mean metering | **Ruled out.** Reduction shaders contain no `log`/`exp` | §2.4 |
| 7 | GF's EV is ~3.7 EV (≈13×) too high versus PES's live EV | **CONFIRMED (live GF trace + live PES EV).** The cause is GF's direct-sun radiance scale, not the metering. The factor is not PES-derived yet | §2.5 |
| 8 | Residual 2× on grass and lines after the EV matches | **OPEN**: per-material (albedo/lighting). Not explained | §3 |

Nothing was ported. Main's condition (keep the fix only if PES's own shader confirms it) is not met, so the fix is reverted. The sun-scale fix is not ported because no PES constant has been decoded for it: a `1/(4π)` test matches the EV, but the constant is a fit, not a decode.

## 1. White balance orientation (Part 1): reversed

**Shader, CONFIRMED (asm).** `GrSystemShaders_dx11.fsop` from the install VFS (`build/apps/pesfmt_dump cat shaders/dx11/GrSystemShaders_dx11.fsop`; 1,000,431 B, matching the RE doc). Record `Tonemap` (PS DXBC 4,328 B). Disassembly: `vkd3d-compiler -x dxbc-tpf -b d3d-asm` (vkd3d 1.17, same pipeline as the B4 pitch dumps). RDEF: `cPSObject` = cb5 (bind slot 5), 288 B = `g_psObject[18]`; `inImage` t0 (point, s8), `inBloom` t7 (linear, s11).
- `Tonemap.ps.asm` line 27: `add r0.xyz, r1.xyz, r2.xyz` (c = bloom + scene).
- Lines 29–31: `dp3 r1.x, cb5[14].xyz, r0.xyz`; `dp3 r1.y, cb5[15].xyz, r0.xyz`; `dp3 r1.z, cb5[16].xyz, r0.xyz`. So out_k = dot(cb5[14+k].xyz, c): the dot-product form makes cb5[14+k].xyz **row k** of the applied matrix A (A[k][j] = cb5[14+k][j]).
- cb5 `.w` lanes: `cb5[14].w` = ToeScale (lines 59, 64), `cb5[15].w` = ShoulderScale (lines 42, 47), so each slot is (row-part, scale).

**CPU upload, CONFIRMED (decompile, `FUN_141804200`, Ghidra bridge).**
- Lines 177–182 (film path): `FUN_14175f590(param_1, &local_f8)`; `cb5[14] = (local_f8, local_e8, local_d8, …)` via slot id `0xe5`. The id mapping is `id = 16·k + 5` (`0xc5` = cb5[12] film, `0xd5` = cb5[13], `0xe5` = cb5[14], `0xf5` = cb5[15]); the asm uses cb5[12..15] consistently with this.
- Stack layout (16-byte rows, from the local addresses `−0xf8`, `−0xe8`, `−0xd8`): `out[4r+c]` = row r, column c. So `cb5[14] = (out[0], out[4], out[8])` = **column 0 of the block**, `cb5[15]` = column 1, `cb5[16]` = column 2.
- `FUN_14175f590` lines 141–149: copies its state block (`param_1 + 0x10 … 0x3f`, 16-byte rows) **verbatim** to its output. So `out` = the state block, which is the object block `TonemapFilterDx11+0x10` (VA `0xb345480` = `0xb345470 + 0x10`, the RE's address).

**Stored block, live (read-only pread, `probe_wb_block.py`).** `0xb345480`: row0 = (1.23445, −0.0223, 0.01165, 0), row1 = (0.43975, 0.82709, 0.06639, 0), row2 = (−0.00783, −0.01944, 1.67887, 0). The w-lanes are 0, so the scales are not stored in the block. Call the block S.

**Consequence.** cb5[14+c].xyz = column c of S, so the applied matrix is A = Sᵀ: `out_k = Σ_j S[j][k]·c_j`. The stored **columns** are applied. This matches B3Post's original "columns" reading and the original GF code (`WhiteBalanceRgb` = Sᵀ). The `99b9ae9` fix (apply S) is wrong. Reverted in `17aef74`, with the decode cited in the oracle comment.

**Why the earlier frames seemed to favour rows.** At matched exposure (−2 stops, X2 vs X3) the rows orientation gave grass R/G 0.51 against PES 0.59, and the transpose gave 1.25. That was real but confounded: the chroma error sits in the albedo/lighting chain (§3), and the WB orientation was compensating for it. The WB change moves luminance by only about +7%.

**Caveat (INF, high): which composition path matches use.** The WB rows are written only on the film path (lines 177–193). When a `ColorGradingParameter` is found (lines 213–229), the composition binds the grading texture to t6, switches to technique slot `+0x74` (`ColorCorrection_CubeLut`), and overwrites `cb5[13]` and `cb5[14]` with constants. The pack's CubeLut body reads neither cb5[14..16] nor any WB, and it is a 27-line stub (SHEX 656 B, STAT claims 70 instructions, against 2,996 B for `Tonemap` and 3,476 B for `ColorCorrection`). The RE says matches bind the day grade (3 live `ColorGradingParameter` objects). If so, **the WB matrix is not applied in live matches at all**, and GF's film+WB chain differs from the live path. Not verified: the live technique slot is not read.

## 2. Exposure (Part 2)

### 2.1 Live law constants (OBS, pid 2510857, read-only pread through `live.py`)
- Viewport `vp = 0x2195f10`; PluginTonemap `plugin = 0x1ff9c10`.
- Key: `sel = vp+0x6ac = −99` (< −90), so key = `vp+0x6b0 = 0.18`; `vp+0x6b8 = 0` (unused). `plugin+0xc8 = 0.18`. **Key = 0.18 = GF.**
- Game state (probe 1): EV −10.8182, min −14, max −5, comp −0.33, Lmeas(+0x410) 0.139571, copy(+0x420) 0.137906, prev EV −10.8155. **Matches GF's 15:00 span group (comp −0.33, clamp −14/−5).**
- Later state (dump): EV −11.1195, min −14, max −7, comp 0, prev EV −11.1359. The exposure group changed between captures, so the mode is not fixed.
- Film at `vp+0x51c` and `plugin+0x64`: (0.88, 0.45, 0.55, 0, 0.02, 6500, 1) = the global set. `plugin+0x94` holds the span set (0.88, 0.55, 0.26, 0, 0.04): stored, not active.
- Gain 10 (`plugin+0x10c`), g table = 0 (`vp+0x6c4`).
- Frame at 13:29, live play, 73:06, BNT 63–99 W, broadcast camera (`pes_now.png`).

### 2.2 Adaptation law (CONFIRMED as in the RE doc)
EV is pinned in the frame by key·2^comp = Lmeas at equilibrium. GF uses the same law and the same constants, so the law does not explain the gap.

### 2.3 The `32·a²` term
- `Tonemap_Lum.ps.asm` lines 5–12: `lum = max(32·a², max(Y, 0.001))` on a point-sampled sub-rect (`cb5[12]`). The term is **present**.
- `SSLighting_Sun.ps.asm` line 270 (`NoShadow` line 137, `PrecomputeShadow` line 315): `mov o0.w, 1.0`. If the lit HDR path carried alpha 1, `32·a² = 32` on lit pixels and the metered Lmeas would be ≈32, not the live 0.14. So the metered buffer's alpha on lit pixels is not 1 (INF).
- Direction: `max(…, 32a²)` can only raise PES's metered mean, which would lower PES's EV. PES's EV is **higher** than GF's, so the term cannot explain GF's excess. **Not the cause.**

### 2.4 Metering type
`Tonemap_Lum.ps.asm`: no `log`/`exp` (grep = 0). `blur_box2D.ps.asm` and `blur_kawase.ps.asm`: no `log`/`exp`. The reduction is arithmetic. **Log-mean ruled out.**

### 2.5 GF is pinned at the EV floor (live GF trace), and the sun radiance scale explains it
GF runs (matched pose, 150 frames, debug prints, reverted):
- E7 (global groups, floor −14): EV −10.00 (f0), −11.98 (f10), −13.17 (f20), −13.68 (f30), −13.81 (f40), −13.96 (f140). Still falling at the floor.
- E8 (floor −20): −14.45 (f50), −14.67 (f80), −14.76 (f110), **−14.79 (f130)**. Settles ≈ −14.8.
- E9 (E8 + `light_intensity_lux × 1/(4π)` on the sun, `pes_lighting.cpp` line 145): **−11.23 (f140)**. PES live: −11.1 (game state, later) and −10.8 (game state, earlier).
- Consequence: GF's metered scene is ~3.6 EV (≈13×) brighter than PES's at the same key. The `1/(4π)` factor is a test, not a decode. The EV matches to about 0.3 EV, so the factor lies between about 0.06 and 0.10.
- E6 and X1 frames are identical (max 1/255, 5.4% px) because the floor binds in both. Compensation and max-clamp changes do nothing while pinned.
- The earlier ExposureMatch "2 stops" (EV −11.69) is stale: GF's settled EV is now at the floor. The "−2 stops" that matched the grass was a proxy for this EV gap, not a PES offset.

**Port: not done.** The needed PES constant is the sun-radiance normalization that connects `sunLux` and the `.atsh` colour to the diffuse radiance. GF normalizes the sun colour by its maximum component (`pes_lighting.cpp` lines 131–135) and sets `light_intensity_lux = sunLux·fade` (line 145). The PES side (`cb2[23].x = 2^EV`, INF; the sun colour in cb2) is not decoded. `SSLighting_Sun*.ps.asm` contains no literal 1/π or 1/(4π), so the factor is not a shader literal.

## 3. Residual after the EV matches (OPEN)
With the EV at PES's value (E9), GF/PES p50 (linear):

| box | PES p50 | X0 (base, EV floor) | E8 (floor −20) | E9 (floor −20, sun/4π) |
|---|---|---|---|---|
| grass_a (470–540, 400–440) | 0.1322 | 0.4852 (3.67) | 0.2981 (2.26) | 0.2899 (2.19) |
| grass_b (340–420, 470–520) | 0.1062 | 0.3918 (3.69) | 0.2306 (2.17) | 0.2241 (2.11) |
| grass_c (700–740, 560–640) | 0.1036 | 0.4035 (3.90) | 0.2381 (2.30) | 0.2320 (2.24) |
| grass_d (860–960, 520–560) | 0.1051 | 0.3995 (3.80) | 0.2343 (2.23) | 0.2276 (2.17) |
| grass_top_right (900–1000, 190–230) | 0.1742 | 0.5042 (2.89) | 0.3128 (1.80) | 0.3042 (1.75) |
| giraffe_body (272–318, 452–474) | 0.3834 | 0.5350 (1.40) | 0.3557 (0.93) | 0.3736 (0.97) |
| circle_arc_right (570–600, 440–500) | 0.1311 | 0.5190 (3.96) | 0.3237 (2.47) | 0.3166 (2.41) |
| far_touchline (800–1000, 124–138) | 0.0801 | 0.2209 (2.76) | 0.1289 (1.61) | 0.1212 (1.51) |
| halfway_line (255–275, 400–440) | 0.1253 | 0.4908 (3.92) | 0.2957 (2.36) | 0.2903 (2.32) |
| frame mean (rows 15–92%) | 0.1507 | 0.4224 (2.80) | 0.2610 (1.73) | 0.2555 (1.70) |

E8 and E9 were built on the `99b9ae9` rows fix (now reverted). That fix moves grass_b by about +7% (X0 3.69 → X1 3.97), so E8 and E9 are comparable to X0 within about 7%. Box values are in the same 1152×648 coordinates as ExposureMatch.

Chroma at matched exposure, correct orientation (X3, −2 stops): grass_b R/G 1.247 (PES 0.585), B/G 0.135 (PES 0.052). **The grass chroma is about 2× too red at PES's exposure**, with the WB orientation now correct. The giraffe is matched in luminance (0.97) but not in chroma: R/G 0.83 vs 0.85, B/G 0.23 vs 0.37 (E9 with the wrong WB; re-measure with the revert).

Open per-material candidates (not tested): the Withers offsets (`WithersParameter3..5`), the `repaired`/`gW` terms, the wither/AO chain driven by pitch R (asm 86–96), the decal lines (`pes_ggx_decal`), and the sun-vs-sky mix. The pitch blue 0.007 (PES) sits below the 0.039 floor (asm 102).

## 4. Dead ends and experiments (all reverted, none committed)
- X2 (−2 stops): matches the frame mean but breaks the giraffe. A proxy for the EV gap, not a PES offset.
- X4 (pitch `SPECULAR = 0`): identical to X1 to 4 d.p. Sky reflection is negligible.
- E6 (global film and exposure groups): identical to X1 because the floor binds.
- Reduction `dispatch` check: `pes_post_effect.gd` `partial_count` and `pixel_count` are consistent with the kernels. No partial-sum bug.

## 5. Open items (for Main)
1. **Live composition path.** Whether matches bind the grading LUT (`+0x74`, `ColorCorrection_CubeLut`). If yes, the WB and film chain GF implements is not PES's live path. Needs the technique slot read live (`plugin`/`vp` chain) or the decode of the CubeLut body from the live D3D shader. Also: the pack CubeLut record is a stub (STAT 70 vs SHEX 656 B).
2. **Sun radiance scale** (the EV gap, ~13×). Decode the PES radiance normalization for the direct sun (`sunLux`, `.atsh` colour, `cb2` sun colour and `cb2[23].x`) and the Godot lux convention in GF. Then port the derived factor with a test. The `1/(4π)` test is evidence of the magnitude only.
3. **Per-material residual** (grass 2.1–2.2×, lines 2.3×, logo matched). Candidates in §3.
4. **Scene alpha on lit pixels** (the `32·a²` term): find the alpha writer in the lit path (not `SSLighting` `o0.w = 1`) if the term is ever needed.
5. **The 15:00 span assumption**: PES's live group flips between game and another state during a match; GF always uses the 15:00 span. The span choice (`SpanForDayNight`) is open.

## 6. Reproduce
- Pack: `build/apps/pesfmt_dump cat shaders/dx11/GrSystemShaders_dx11.fsop --pes21-root <PES21>`; `fsop_extract.py` (`gf-B4Grass-scratch`) with `Tonemap`; `vkd3d-compiler -x dxbc-tpf -b d3d-asm`.
- Decompile: Ghidra bridge `decompile_function?address=0x141804200`, `0x14175f590`.
- Live (read-only): `sudo -n python3 probe_vp.py`, `dump_vp.py`, `probe_wb_block.py` (pread on `/proc/<pid>/mem` through `live.py`).
- Frame: `tools/capture_viewer.sh --scene res://match/match.tscn --frames 150 -- --stadium 21 --camera-pose 7.94,19.135,37.07,0.022,-0.376,-0.926,23.28` (plus team args).
- Measure: `measure.py` (same boxes as ExposureMatch).

Tests run: `pes_tonemap_test` 9/9 and `pes_lighting_test` 9/9 (install-gated, `PES21_ROOT` set) on the revert. The mutation M1 was run on the refuted fix (oracle failed on the dropped transpose, then restored). Gate not run (Main gates).
