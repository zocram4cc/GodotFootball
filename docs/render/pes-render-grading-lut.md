# Grading LUT: the running shader is the pack's CubeLut stub (t6 bound but never sampled); display ×4 cancels its ×0.25

> **Main 19-10-26: UNCONFIRMED, do not port yet.** That the pack stub is the shader the GPU runs in a match is inferred: the draw-time capture was not made (the live PES was in menus), and it conflicts with live match frames showing tonemapped highlights (pes-render-TurfColour.md). The asset, the bind chain and the x0.25/x4 pair are confirmed; the composition order needs a draw-time capture in a live match first.

Live PES binds the colour-grading path (registry key `0xcb716e233439` → `ColorGradingParameter`, `lut_s_day_game` live), but the pack's `ColorCorrection_CubeLut` record is a 27-line stub that never samples t6. This decode traces what actually runs so GF can replace its CubeLut stub (`docs/render/pes-render-b3post.md` open item).

## 0. Script-read constant table (floats read from `PES2021.exe` via section map, never typed)

| VA | value | used at |
|---|---|---|
| 0x14256ae1c | 1.0 | `0x1418044d7` MOVSS (cb5[10].y numerator) |
| 0x142565600 | 0.5 | `0x141804539` MOVSS (bloom-rect half; consistent with sun doc §0 row `(0.5,×4)`) |
| 0x142a38240 | (string `Tone Mapping and Bloom Composition`) | `0x14180426c` LEA (pass name for `FUN_141804200`) |
| 0x143514170 | (bss, live 0.99999900) | `0x1418044df` MOVSS (cb5[10].y denominator) |

Shader-immediate literals below are quoted from `vkd3d-compiler -x dxbc-tpf -b d3d-asm` output (vkd3d 1.17), not typed.

## 1. The shader the GPU runs for the grading pass (HIGH, not CONFIRMED at draw time)

### 1.1 CPU side (CONFIRMED, decompile + disassembly + live reads)

- Pass setup `FUN_141804200` (`curl .../decompile_function?address=0x141804200`, 253 lines; disasm `disassemble_function?address=0x141804200`). Grading branch lines 213–230: `FUN_1416a44b0` returns hash `0xcb716e233439` (CONFIRMED: the function body is one store, `fun_1416a44b0.txt`); `FUN_1416a3b30` registry lookup on the viewport (`+0x430` root, `+0x450` lock); if `local_138[0] != 0`: bind that object's texture (`+0x28 → +0x20` handle) to **slot 6** (`0x200` call, disasm `0x1418049fe`), overwrite `cb5[13]`/`cb5[14]` (`0xd5`/`0xe5` calls), set technique `*(u32*)(DAT_1437712e0 + 0x74)` (disasm `0x141804a04`), bind via renderer vtable `+0x128` (disasm `0x141804a5b`).
- Live (read-only pread, menu scene 2026-10-19): viewport `0x2195f10` registry root `0xcff0400` contains key `0xcb716e233439` → node `0x2469e920` → `ColorGradingParameter` `0x24682f10` (vtable `0x142a01c70`); its `+0x28` = texture wrapper `0x2524ae40` (pathcode `+0x10` = `0x34093d3d64665f0`, Fox file code = loaded `.ftex`, not a render target). Technique struct `DAT_1437712e0` = `0x1fed1c0`; u32 map `+0x30…+0x78` = `0xb80002…0xc50002` (family `0002`); **`+0x74` = `0xc40002` live**.
- Binder `FUN_141796a90` (vtable `0x142a161c0+0x128`, decompiled): index low `0x3f`-mask `== 2` → table `DAT_14373c900` (`0x21a0200`, base `0xa7e0000`), entry `(idx>>0x10)·0x30 + base`: entry `0xc4` = `0xa7e24c0`, `+0x24` = `0x000f000c` (redirect, nonzero) → family `000c` table `DAT_143743678` (`0x2191200`, base `0x23d0000`), entry `0x0f` = `0x23d05a0` → shader pair `(0x181cd20, …)` recorded as pending key via `FUN_141796b90` (sets context `+0x318/+0x320`, decompiled).
- The `0x181cd20` object is the shared shader registry (names `ColorCorrection_DrawLut_Gen/Tex`, `ColorCorrection_Hdr`, `CopyRenderBuffer*`, `DbgView_*` at pool `0x1b78f20+`; `ColorCorrection_CubeLut` IS a pool entry at `0x1b78f20` but was NOT found in the scanned `0x0–0x2000` span of that object). The final draw-time `*puVar1/puVar1[1]` resolution was not captured (needs frame-synced introspection; open item 1).

### 1.2 Why the running pixel shader is the pack stub (HIGH)

- The pack record `ColorCorrection_CubeLut` (`GrSystemShaders_dx11.fsop`, PS DXBC 2,028 B): chunks `RDEF` 1008 / `ISGN` 80 / `OSGN` 44 / **`SHEX` 656** / `STAT` 148 with `STAT_nins` = **70**, but the SHEX disassembles to **15 body lines** (script-counted; intact siblings match: `ColorCorrection` 93/93, `ColorCorrection_Hdr` 102/102). RDEF binds `g_CubeLut` = `texture3d` t6 + `inImage` t0 + `inBloom` t7, `cPSObject` cb5 / `cPSSystem` cb0.
- Full body (`cc/ColorCorrection_CubeLut.ps.dxbc` via vkd3d, line numbers from `cat -n`):
  - L13–14: `mul r0.xy, v1.xyxx, cb0[2].zwzz` + `sample_indexable(texture2d) r2.xyzw, r0.xyxx, t0.xyzw, s8` — the ONLY sample: point-sampled scene. **t6 and t7 are declared and never sampled.**
  - L15–23: piecewise sRGB encode: `max(v,1e-5)`; `log/2.4/exp`; `·1.055−0.055`; linear `·12.92`; select at `0.0031308` (`ge`+`movc`).
  - L24–26: `mul r4, r2, (0.25,0.25,0.25,1)`; `eq r3.w, cb5[10].x, 0`; `movc o0, r3.w, r4, r2` — **×0.25 iff `cb5[10].x == 0`**.
- No second CubeLut exists: full record-name scans of `GrSystemShaders_dx11.fsop` (205 records), `PesShaders_dx11.fsop` (244), `GrModelShaders_dx11.fsop` (only `fox3dfw_glass_color*` match `color`); exe contains zero `CubeLut`/`g_CubeLut`/`inColorLUT` bytes (shaders come from packs only). A `texture3d` volume bound at t6 is harmless to a shader that never samples t6, so binding the 33³ LUT with the stub changes nothing observable.
- Blow to the alternative (live D3D shader differs from the pack): there is no other bytecode source in the install. Mark: pack stub runs = HIGH (all static + bind-type evidence agrees; draw-time capture would promote to CONFIRMED).

### 1.3 `cb5[10].x` switch (CONFIRMED statically, menu value read live)

- Disasm `0x1418044bf–0x141804515`: `cb5[10].x = (float)(((vp+0x650) & 0xf) == 2)`; `cb5[10].y = [0x14256ae1c] / [0x143514170]` (= 1.0 / ~1.0); `.z/.w` from `param_4`. Slot id `0xa5`.
- Live menu: `vp+0x650` low nibble = 0 → `cb5[10].x = 0` → **×0.25 active**. (Match value of `vp+0x650` still open; if matches also read 0, the ×0.25 applies there too.)

## 2. The LUT asset (CONFIRMED, VFS + parse + live bind)

- Path: `Asset/model/bg/common/lut/#windx11/lut_s_day_game.ftex`, winner = stock `Data/dt30_g4.cpk` → nested `Asset/model/bg/common.cpk` (2,117 entries; VFS `mounts` + `ls` show the 32-file `lut_*` set; no `download/*.cpk` mount overrides it — 37 download mounts checked by listing; `pesfmt_dump cat` serves the stock bytes).
- Referenced by the rig fox2 (`light_stNNN_af.fpkd` → `sdrGameGradingLut …/Assets/pes16/model/bg/common/lut/lut_s_day_game.ftex`, Fox2Parser OBS) with siblings per stadium/time: `lut_{s,h}_{day,evening,night,cloudy}_{game,demo}.ftex` (+ `lut_sepia_*`, 32 files).
- Format: FTEX `pf 12` (RDEF-consistent RGBA16F), `33×33×33`, 1 mip, chunked zlib (5 chunks; GF `src/pes/ftex.cpp` parses it: pf@8, w/h@10/12, mips@16, chunk table at mip offset). Decompressed 287,496 B = 33³·4ch·2B. Layout **`L[b][g][r]`** (CONFIRMED: `L[0,0,1]` moves R only `0.0208→0.0294`, `L[0,1,0]` moves G, `L[1,0,0]` moves B). Alpha ≡ 1.
- Values (`lut_s_day_game.npy` in scratch, float64 from f16): grey `k=0 → 0.0208`, `k=8 → 0.277`, `k=16 → 0.616`, `k=24 → 0.666`, `k=32 → 0.689`; max|grey − t| = 0.31 (not identity). `lut_h_day_game` byte-identical (md5 `a0df37095419b6c1dc0035140f8652ca` both); `lut_s_day_demo` same md5; evening/night differ (night grey32 = 1.0).
- Sampling: **the running stub never samples it** (no coordinates, no scale/offset, no filtering причаст). For the record, RDEF assigns t6 the linear-clamp sampler s11 (`g_samplerLinear_Clamp`), and the fox2 `ColorCorrectionData` (`colorScale`, `startSlope/endSlope` → `cb4`, per RE doc) only feeds the DrawLut/BlndLut strip builders, which the grading branch bypasses (`ColorGradingParameter` vtable[14] `FUN_1416a4530` binds slot 6 and returns when found — disasm `0x1416a4595–0x1416a45ae` CONFIRMED).
- Before/after tone curve or WB: neither — the LUT output never enters the frame on this path.

## 3. Composition order for a match frame on the grading path (CONFIRMED static; menu-state live)

Per-pixel, in order (all citations above):

1. **Exposure**: pre-applied in lighting (`cb2[23]`, RE doc; no exposure scalar in any comp shader). NOT in the comp pass.
2. **Scene resolve**: point sample `t0` (stub L14, sampler s8). **Bloom `t7` is NOT sampled** (no t7 line in the stub) — the film path's scene+bloom add (`ColorCorrection` L25 `add r0, r1, r2`) does not run.
3. **Tone curve**: NONE of UE4 FilmToneMap / film constants (`cb5[12..16]` film slots are overwritten with `(vp+0x500…)` gamma uniforms by the branch, then ignored by the stub). What runs instead is the **piecewise sRGB encode** (stub L15–23, linear 12.92 below 0.0031308).
4. **White balance**: NOT applied (stub never reads `cb5[14..16]`; the stored WB block is bypassed on this branch — agrees with TurfColour §1 caveat and sun-radiance §4).
5. **LUT**: bound (33³ volume at t6) but **never sampled** — no-op.
6. **`×0.25`** iff `cb5[10].x == 0` (stub L24–26; live menu 0 → applied).
7. **Display** `GammaControl` (pack record, PS DXBC 1,588 B, full asm in scratch): `sample t0` → `·(4,4,4,1)` → `log` → `·cb5[11]` → `exp` → `cb5[10]·x + cb5[9]`, i.e. **`o = cb5[10]·|4x|^cb5[11] + cb5[9]`**. With the stub's ×0.25 input, `4x` restores unity before the options gamma/gain/offset (`brightnessSetting`/`SetGamma`, RE doc §6).

Net grading-path transfer (menu state, `cb5[10].x=0`): `out = disp( sRGB(scene)·0.25 )` with `disp` undoing the 0.25 via its ×4. Film+WB+LUT contribute nothing.

Contrast — film path (`ColorCorrection`, non-grading): scene+bloom → WB dp3 (`cb5[14..16]`) → lin→always-on FilmToneMap chain → gamma-1/2.2 (`pow 0.4545…`, L~84) → 16³ 256×16 strip sample at t6-as-2D (two taps + `·16` lerp, L~90–104) → ×0.25 switch → out. (Asm `cc/ColorCorrection.ps.dxbc`, 93 body lines.)

## 4. The remaining ~×4 in the sun chain (pes-render-sun-radiance.md open item)

**Not explained by this trace; still OPEN, with two suspects killed:**

- The ×0.25 (stub) + ×4 (`GammaControl`) pair **cancels exactly** (`4·0.25 = 1`) and is achromatic and sun-independent (applies to every pixel equally). It cannot produce a sun-only ≈4× (or ≈7.9×) gain error, and it leaves no residual when display gain = 1. **Killed: "a hidden ×4 lives in the grading/display path".**
- **Killed: "the live CubeLut samples the LUT with a ×4-ish domain gain".** The only CubeLut bytecode in the install never samples t6; no runtime-compiled variant exists (no shader strings in the exe at all).
- Standing: H1 (Sky_SunColor seeding column |·|≈0.13, needs E-A live `cb3[1]` read), H2 (Godot directional-lux gain, needs lux-sweep), H3 (span/elevation fold-in). E-A is now harder to run opportunistically: the live machine currently sits in menus (Game Plan screen, frames `live1/2.png`), so E-A needs a live match.

## 5. Port statement for GF's post pass

1. **Replace GF's film+WB post chain on the grading path with the stub+display chain** (`godot/post/pes_post.glsl` COMPOSE kernel + `src/gdext/pes_lighting.cpp` `PostParams`): when a `ColorGradingParameter` is present (always, in matches): skip bloom add, skip WB matrix, skip UE4 film curve; apply piecewise sRGB encode (threshold `0.0031308`, `12.92` / `1.055·x^(1/2.4)−0.055`, floor `1e-5` — stub L15–23 literals); multiply `0.25` when `cb5[10].x == 0` (wire `vp+0x650 & 0xf == 2`, default 0); then the display map `o = gain·(4x)^gamma + offset` with gain/gamma/offset from options (defaults gain 1, gamma 1, offset 0). Do NOT feed the 33³ volume to Godot's `adjustment_color_correction`: PES binds it but never samples it — sampling it in GF would *add* grading PES does not apply. Delete the GF CubeLut-stub path in the same change (clean cutover).
2. **Do not port any ×4**: there is no ×4 constant — the display ×4 literally cancels the comp ×0.25. A test asserting `disp(stub(x)) == sRGB(x)` at gain 1/gamma 1/offset 0 (relation, not a pinned pack value) guards the pair.
3. After open item 1 (draw-time capture): if the live PS differs from the pack stub, replace this statement with the captured sampling lines.

## 6. Open items

1. **Draw-time PS capture.** The bind chain is traced to the pending-key record (`FUN_141796b90` → ctx `+0x318/+0x320`); the last hop to the D3D PS object needs frame-synced introspection (RenderDoc attach or D3D debug layer) **in a live match** (machine is in menus now). Settles HIGH → CONFIRMED for §1.2.
2. **`vp+0x650` in matches.** Menu reads 0 (`×0.25` on). Read it in a live match; decides whether matches render the ×0.25/display-×4 pair or the unity bypass.
3. **Display-stage cb5 values** (`cb5[9..11]` for `GammaControl`: gain/gamma/offset from `brightnessSetting`/`SetGamma`). Unread live.
4. **LUT domain question is moot for rendering** (bound, unsampled) but the asset doc stays for the strip path (menus/demo without grading param) and archive completeness.
5. `SAP+0x1c0 = 11`, `cb3[3]`, fog (untouched, carried from sun-radiance §6.5).

## 7. Reproduce (scratch `<workdrive>/gf-GradingLut-scratch/`)

- Pack: `pesfmt_dump cat shaders/dx11/GrSystemShaders_dx11.fsop --pes21-root <workdrive>/PES21 > sys.fsop`; `fsop_extract.py sys.fsop cc ColorCorrection_CubeLut ColorCorrection ColorCorrection_Hdr GammaControl`; `vkd3d-compiler -x dxbc-tpf -b d3d-asm cc/<N>.ps.dxbc`. Chunk check: `python3` DXBC walk (`SHEX` 656 vs `STAT_nins` 70 for CubeLut; 3476/93 and 3648/102 for intact siblings).
- Image: `PES2021.exe` (base `0x140000000`, section-mapped reads); bridge `decompile_function?address=0x141804200|0x1416a44b0|0x1416a3b30|0x1416a4530|0x141796a90|0x141796b90|0x1416821f0|0x1416827a0|0x1416d00c0|0x1417996f0`, `disassemble_function?address=0x141804200|0x1416a4530`.
- VFS/LUT: `pesfmt_dump ls "Asset/model/bg/common/lut/#windx11"` (32 files); `cat` → ftex parse (pf@8 = 12, 33³, zlib chunks) → `L[b][g][r]` layout test; md5 `a0df37095419b6c1dc0035140f8652ca` (s_day_game = h_day_game = s_day_demo).
- Live (read-only pread via `LiveSample/live.py`, sudo): technique table `*(0x14373ea98)` (55 entries); grading node walk from `vp+0x430` (`vp = 0x2195f10`); `+0x74` of `*(0x1437712e0)` = `0xc40002`; `vp+0x650 & 0xf` = 0 (menu).
- Files kept: `sys.fsop`, `cc/*.dxbc`, `*_asm` notes, `fun_*.txt`/`dis_*` decompiles, `w_lut_*.ftex` + `lut_s_day_game.npy`, `live1/2.png` (menu Game Plan), this report.
