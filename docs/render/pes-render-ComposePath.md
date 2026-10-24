# PES composition path: which post path live matches take, and the GF port spec

Decode of the post technique slot selection, live reads of that slot plus the bound LUT identity and the WB parameters, and the metering region/weights behind the luma split. Read with `docs/render/pes-render-PitchMaterial.md`, `pes-render-grading-lut.md`, `pes-render-b3post.md`, `pes-render-GradePort.md`, `pes-render-TurfColour.md`, and `docs/re/pes-re-lighting.md` Part C.

Status: slot selection CONFIRMED static; grading-registry HIT CONFIRMED live (key `0xcb716e233439` resolves, technique `+0x74 = 0xc40002`); the bound t6 resource this session is NOT the file LUT (OPEN, see §3); WB bypass on the grading branch CONFIRMED static; metering weights CONFIRMED arithmetic/uniform, metering rect formula CONFIRMED static with live rect inputs captured but GPU `cb5[12]` values OPEN.

## 0. Script-read constant table (floats read from `pes_full.bin`, offset = VA − 0x140000000, never typed)

| VA | value | used at |
|---|---|---|
| 0x14256ae1c | 1.0 | `0x1418044d7` MOVSS (`cb5[10].y` numerator) |
| 0x142565600 | 0.5 | `0x141804539` MOVSS (bloom-rect half) |
| 0x142578480 | 3.14159274 | sun/moon ×π (sun-radiance §0) |
| 0x142565d20 | (3,3,3,3) | direction-normalize shape |
| 0x1425658b0 | (0.5,0.5,0.5,0.5) | direction-normalize shape |
| 0x142a38240 | `Tone Mapping and Bloom Composition` | `0x14180426c` LEA (pass name) |
| 0x143514170 | (bss, image-zero — not script-readable) | `0x1418044df` MOVSS (`cb5[10].y` denominator; live ≈ 1.0 per prior sessions, INF) |

## 1. Static: the post technique slot selection (CONFIRMED, decompile + disassembly)

Function `FUN_141804200` (pass setup, "Tone Mapping and Bloom Composition"). Decompile `fun_141804200.txt`, disassembly `dis_141804200.txt` (both in scratch).

### 1.1 Film-vs-plain select (feeds `cb5[12..16]`, not the technique)

- Guard (decompile): `if ((char)param_4[0x15] == 0 && *param_4 != 1)` → film branch: `FUN_14175f430` (film constants → slot `0xc5` = `cb5[12]`), `FUN_14175f590` (WB builder → slots `0xe5`/`0xf5` = `cb5[14]`/`cb5[15]`), final upload id `0x105`. Disasm `0x14180460b` (`CMP byte [R14+0x54]` = `param_4[0x15]`) / `0x141804616` (`CMP [R14],1`).
- Else plain branch: uploads `0xc5`/`0xd5`/`0xe5`/`0xf5` from `param_4` words directly (`(float)(*param_4 == 0)` etc.), disasm `0x141804810+`.
- `param_4` is the caller's comp parameter block; its writer is **OPEN** (caller of `FUN_141804200` not traced this session).

### 1.2 Pre-grading technique shortlist (slots `+0x50..+0x70` of `DAT_1437712e0`)

- Selector (disasm `0x14180428e–0x141804444`): `uVar6 = [0x143748538+8]`; bit 3 (`TEST R9B`) gates `FUN_1416827a0(param_3,3)`; bit 4 + `bVar3` refine. `bVar3` (disasm `0x1418042a3–0x1418042e1`) = (`*(param_2+0x98)+0x30/0x31` nonzero) AND (`*(param_3+0x190 chain) & mask == 0x2537d46548c3`).
- Result: `R15D = [R13+0x50/0x54/0x58/0x5c/0x60/0x64/0x68/0x6c/0x70]` selected by (`param_4[0x17]`, bit4, `bVar3`). This is the technique id used when the grading branch misses.

### 1.3 Grading branch — the CubeLut-vs-film decision (the ticket's slot selection)

Decompile lines 213–230, disasm `0x141804925–0x141804a04`:

1. `FUN_1416a44b0` writes hash **`0xcb716e233439`** (one-store body, CONFIRMED).
2. `FUN_1416a3b30(param_3, &local_138, hash)` — registry lookup on the viewport: `param_1+0x430` root, `+0x450` lock; binary-tree lower-bound walk over key `+0x20`, value `+0x28` (CONFIRMED, decompile `fun_0x1416a3b30.txt` + live walk §3).
3. `if (local_138[0] != 0)` (disasm `0x141804940 CMP [RSP+0x50],0 / JZ 0x141804a08`):
   - overwrite `cb5[13]` (slot `0xd5`, disasm `0x1418049bf`) with `(vp+0x500, vp+0x508, 0, 0)`-shaped data and `cb5[14]` (slot `0xe5`, disasm `0x1418049d8`) with `(300, …)`-shaped data — the film constants are **destroyed**;
   - bind `*(local_138[0]+0x28)+0x20` handle to **slot 6** via vtable `+0x200` (disasm `0x1418049e6–0x1418049fe`);
   - `R15D = [R13+0x74]` (disasm `0x141804a04`), i.e. technique-table slot **`+0x74`** replaces the §1.2 shortlist.
4. Bind: `FUN_141799bc0`, `FUN_1416821f0(param_3)` (hash `0x5247d8da9d5b` = next pass), `(0xd8)(uVar12)`, then slot-0 texture + `(0x128)(R15D)` (disasm `0x141804a0b–0x141804a5b`) — the renderer programs the selected technique id.

So the variable that picks **CubeLut vs film + WB** is `local_138[0] != 0`, i.e. **whether a `ColorGradingParameter` is registered on the viewport under `0xcb716e233439`**. It is set by `FUN_1416a3b30`, not by a flag. Where the registry entry itself is created is **OPEN** (creation path not traced).

Slot-id encoding (CONFIRMED, binder `FUN_141796a90`, disasm `dis_141796a90.txt`): low `0x3f == 2` → table `DAT_14373c900`, entry `(id>>0x10)*0x30 + base`, `+0x24` nonzero → tail-call `(0x128)(renderer, id)`; low `0x0c` → table `DAT_143743678`, stride `0x60`. Live slot ids `0xbb0002…0xc50002` are all low-2 (high 187–197); `TS+0x74 = 0xc40002` → high 196 → entry `0xa7e24c0`, `+0x24 = 0x000f000c` nonzero (CONFIRMED live) → renderer programs it.

Program identity (CONFIRMED static + live adjacency): pack `ColorCorrection_CubeLut.ps` declares `t6` (`texture3d`) and `t7` but **samples neither** — sole sample is `t0` (L14, point); body = piecewise sRGB encode (L15–23) + `×0.25 iff cb5[10].x == 0` (L24–26). `ColorCorrection.ps` (film path) by contrast does scene+bloom add (L28), WB `dp3` rows `cb5[14..16]` (L30–32), UE4 film chain, gamma `pow 0.4545` (L87), 16³ strip at t6-as-2D (L96–101), `×0.25` switch (L102–104). The stub never reads `cb5[14..16]` → **WB bypass on the grading branch is structural, not parametric**.

## 2. Live reads (read-only `pread` on `/proc/2510857/mem`, 2026-10-24; G = `*(0x1436F3DC8)`)

| # | VA / site | live value | how read | status |
|---|---|---|---|---|
| L1 | `G` | `0x1eb942c0` | `q(0x1436F3DC8)`; `MI = *(G+0x270) = 0x0` (between ticks) | CONFIRMED |
| L2 | `TS = *(0x1437712e0)` `+0x74` | `0x1fed1c0`, `+0x74 = 0xc40002` (full table `+0x30..+0x88` = `0x??0002` ramp `0xb8→0xc5`, `+0x7c = 0x76000b`) | sparse `pread`, one `u32` per offset | CONFIRMED (same as 19-10-26 match *and* menu — the slot alone does not discriminate; the registry hit does) |
| L3 | grading registry hit | `vp = 0x2195f10` (pattern `+0x4d8 = 0.2/0.1/0.4/0.4/0.3` + EV plausibility); root `*(vp+0x430) = 0xcff0400`; lower-bound walk for `0xcb716e233439` reaches node `0x286bdcf0` with **exact key**, value `0x1a318b90` | `pread` tree walk (`+0x00` left / `+0x10` right / `+0x19` colour / `+0x20` key / `+0x28` value), 2 steps | CONFIRMED hit |
| L4 | CubeLut program pair | name `ColorCorrection_CubeLut` at `0x1b78f20` (entry `0x181cd10`), pair **`(0xcfbc568, 0xcfba478)`** at `0x181cd20` (name-precedes-pair, same as the `ColorCorrection` anchor at `0x181cbf0→0x181cc00`) | registry walk from the known `ColorCorrection` anchor, 16-byte entries | CONFIRMED adjacency (= 19-10-26 pair) |
| L5 | `vp+0x650` (`cb5[10].x`) | `0x304f0`, low nibble **0** → `cb5[10].x = 0` → `×0.25` ACTIVE | `u32(vp+0x650)` | CONFIRMED |
| L6 | exposure | EV **−11.309**, min −13, max −9, comp **+0.33**; `Lmeas +0x410 = 0.22611`, copy `+0x420 = 0.22436`; key sel `+0x6ac = −99` → key `+0x6b0 = 0.18`; g-table `+0x6c4 = 0`; bloom `+0x4e0/4e4/4e8 = 0.4/0.4/0.3`; sub-rect guess `+0x4f0.. = (0,1,1,0)`; meter size `+0x584/0x588 = 1280/720` | sparse `pread` `u32`+`f32` per offset | CONFIRMED (note: comp is **+0.33** today vs −0.33 on 19-10-26; min/max −13/−9 vs −14/−5 — the exposure group flipped mid-session both days) |
| L7 | plugin `0x1ff9c10` | film `(0.88, 0.55, 0.26, 0, 0.04, 6500, 0.05)` = the **span** set (override active), tint **0.05**; `+0xc8` key 0.18; `+0x108 = 0.1789`; `+0x10c` speed **30** | `pread` block | CONFIRMED (19-10-26 read tint 1 / speed 10 — session-dependent) |
| L8 | stored WB block `0xb345480` | **near-identity**: `(1.0111, −0.00118, 0.00037, 0)`, `(0.0234, 0.9893, 0.00251, 0)`, `(0.00003, −0.00078, 1.0229, 0)` | `fvec(0xb345480, 16)` | CONFIRMED (differs from 19-10-26's strong tint `(1.234,−0.0223,…) (0.44,0.827,…) (…,1.679)` — consistent with tint 0.05 today vs 1 then; and **unconsumed** on this path in either case, §1) |

How each was read: `sudo -n python3` with `LiveSample/live.py` (`os.pread` on an `O_RDONLY` `/proc/<pid>/mem` fd, pid re-resolved per call). One small read per VA; short region scans only for the viewport pattern and the registry walk. No writes, no attach, nothing pinned.

## 3. Bound CubeLut resource identity (OPEN — this session does NOT show the file LUT)

- 19-10-26: registry value → `ColorGradingParameter 0x24680250`, `+0x28` → wrapper `0x2524ae40`, pathcode `+0x10 = 0x34093d3d64665f0` (loaded `.ftex`); VS bytes of the CubeLut program byte-identical to the pack stub.
- Today: value `0x1a318b90` (vtable `0x142a01c70`) is followed inline by `doh_steward00.fmdl` path strings, and the parameter object at `0x286bbda0` fans out to `CAP_RT*` render-target names (`CAP_RT23835/25139/27368/28850`) with wrapper/handle chains (`0x286bfc88 → 0x286bfc10`), **no `.ftex` pathcode at the old `+0x10` offset and no `lut_s_day_game` anchor reached** (heap string scan for `lut_s_day_game` timed out twice at 100 s; not retried — the match was live and scans are the most expensive reads here).
- What this blocks: whether the installed `lut_s_day_game` (stock `Data/dt30_g4.cpk`, 33³ RGBA16F, grey 0.0208→0.689, `L[b][g][r]`, md5 `a0df37095419b6c1df…`) is even the bound object *this* session. It does **not** reopen the shader question: the program pair `(0xcfbc568, 0xcfba478)` is the same adjacency as the byte-verified session, the pack record's PS never samples t6 whatever is bound, and `cb5[13]/[14]` are overwritten on this branch — so no WB and no LUT consumption run in either case.
- Next: one short targeted read of `*(val+0x28)`-style wrapper offsets for the CURRENT value node (not the old offsets), or a re-run of the string scan with a tighter heap range, to name the bound t6 object.

## 4. Luma split: metering region/weights (the ~1.7× global)

- Weights CONFIRMED (static, `Tonemap_Lum.ps.asm` L9–17): `lum = max(dot(rgb, (0.2125, 0.7154, 0.0721)), 0.001, 32·a²)` — Rec.709 luma, **arithmetic**, no `log`/`exp` in `Tonemap_Lum`, `blur_box2D`, `blur_kawase` (grep count 0). GF's REDUCE (`max(dot·2^EV, 0.001)`, arithmetic mean over the full frame) matches except the `32·a²` term (Godot alpha ≠ PES bright-pixel code — open `ponytail`, correctly unported).
- Region CONFIRMED static, OPEN live values: source UV = `(ndc·(0.5,−0.5)+0.5)·cb5[12].zw + cb5[12].xy` (sub-rect of the source). `FUN_141805840` uploads `cb5[12]` three times: full-rect (`0xb5`, from target dims), meter-rect (`0xb5`, from `vp+0x584/0x588` × `*(light+8)·(vp+0x4a8)`), clamped rect (`0xc5`, from `param_3+0x8c/0x90/0x94/0x98` scaled by bss `0x143514170`, clamped [0,1] in-shader). Live rect inputs captured: `vp+0x584/0x588 = 1280/720`, `vp+0x4f0.. = (0,1,1,0)`; double-buffered luminance targets `vp+0x5e8 = (0x1c29280, 0x1c29400)`, index `vp+0x660 = 3`. The actual `cb5[12]` constants live GPU-side and were **not** read (no GPU readback in the read-only toolbox).
- Consequence for the split: there are **no content weights** — no stands/sky weighting exists in the metering. The INF "metering content" mechanism is therefore only the trivial one: an arithmetic mean over a rect that *contains* stands/sky in the broadcast view. That needs no extra decode; what remains OPEN is the rect itself (full-frame vs sub-rect) and the `32·a²` alpha term's live magnitude — both second-order next to the sun-radiance remainder (sun-radiance §2: colour ratio 2.26× CONFIRMED, ≈5.6× elsewhere OPEN).

## 5. GF port spec (`godot/` read-only; GradePort `0c2a8bb` on origin)

`godot/post/pes_post.glsl` COMPOSE + `godot/post/pes_post_effect.gd` + `src/gdext/pes_lighting.cpp::PostParams`:

- **Path: the grading path, correctly.** `scene = colour·2^EV` → `cube_lut_stub` (`0.25 × piecewise-sRGB-encode`, stub L15–24 literals, f32 bit-checked against the pack asm) → `sdr_display_mapping` (`sRGB-decode(4x)`, display literals incl. `4/12.92` dark slope) → `clamp [0,1]`, alpha passthrough. Net in-gamut identity; highlights = exposure + clip. No bloom add, no WB matrix, no UE4 film, no LUT sampling — matching §§1–3. Params 6 floats: key, compensation, minEV, maxEV, tonemapSpeed, stepGain.
- **Parameters actually in use:** exposure group from the rig span (`gameKeyValue/ExposureCompensation/Min/MaxExposure`, tonemapSpeed); the grading pair takes no parameters. Nothing from the WB block, the film group, or the LUT asset is consumed — correct for the live path.
- **Known deviations (all marked or spec'd):**
  1. `vp+0x650` bypass (`cb5[10].x = 1` skips `×0.25`) is `ponytail`-marked unmodelled — live reads 0 in every session, so the gain always applies.
  2. Meter rect: GF meters the full frame; PES meters the `cb5[12]` sub-rect (§4). Unmodelled; live rect values still OPEN.
  3. `32·a²` term omitted (`ponytail`: Godot alpha is not the bright-pixel code). Needs the alpha writer (TurfColour item 4) before it can be modelled.
  4. LACC branch (`FUN_141684180` select in `FUN_141805840`) unmodelled; live branch untaken/unknown.
  5. Grading-lut doc digit erratum stands: dark slope is `3.09597522e-01` (= 4/12.92), not `0.30997522`.

## 6. Status ledger

- CONFIRMED: grading-branch slot logic (`local_138[0] != 0` → slot 6 + `+0x74`, film constants destroyed); binder encoding; stub body (t6/t7 declared, never sampled; WB rows never read); film-path body (WB `dp3 cb5[14..16]`, UE4 film, strip at t6-2D); metering weights/arithmetic/no-log; live `+0x74 = 0xc40002`, registry HIT, CubeLut pair adjacency, `vp+0x650 = 0`, live exposure/plugin/WB values above.
- INF: bss `0x143514170` ≈ 1.0 (prior live reads, not re-read); `cb2[23].z = 2^EV` (prior sessions); metering-content = arithmetic mean over a stands/sky-containing rect (no weights exist to decode).
- OPEN: registry-entry creation path; `param_4` writer (film-vs-plain inputs); bound t6 object identity this session; live `cb5[12]` rect values; LACC branch taken; `cb3[3]`/fog/`SAP+0x1c0` (carried).
