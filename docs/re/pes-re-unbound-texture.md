# pes-re-unboundtex: what PES binds to `Mask_Tex_SRGB` (t6) on the pitch

Scope: asm 104–109 of `pes3ddf_pitch_2020` PS, the engine's unbound-slot fill for `Mask_Tex_SRGB`, and the GB2.y/GB2.w question from B4.
Image: `the session dir` (base 0x140000000), Ghidra bridge on 127.0.0.1:8080.
Scratch: `the scratch dir/` (`sc`/`sc2` hash tools, `scan.py`, `dec_*.txt` decompiles).

## 0. Answer

- **OBS:** when the pitch material does not bind `Mask_Tex_SRGB`, the engine's mask binding (FUN_1419b13a0) assigns the global default `DAT_1437cb0e0`, unless a registry texture `K` is resident. The default is a **4×4 RGBA8, every texel `00 00 00 ff`, i.e. opaque black**. So `mask.r = 0`, `mask.g = 0`, `mask.b = 0`, `mask.a = 1`.
- **Consequence:** `ao = max(t0.a, 0) = t0.a ≈ 0.003` (pitch_alp alpha). Our port's zero default for `t6` is the same value PES gets on the default path. The "PES needs ao ≈ 0.3–0.5" inference is not supported by this (see §4).
- **OPEN:** whether a registry texture `K` is resident in a match. If it is, PES uses that texture's R, G, B instead. `K` is a runtime key and was not resolvable statically. Live memory read is blocked (§5).

## 1. The binding function (CONFIRMED, static)

| Item | VA | Evidence |
|---|---|---|
| Mask hash `Mask_Tex_SRGB` = `StrCode32` `0x8d57aa1e` | — | `pes::StrCode32` from `src/pes/strcode.cpp`. Control: `Base_Tex_SRGB` gives `0x7be81b61`, matching the documented value. |
| Only hit for the hash in the image | `0x1419b1475` | `mov edx, 0x8d57aa1e` at `0x1419b1474`, inside `FUN_1419b13a0`. The `Base_Tex_SRGB` hits at `0x1419b11cc`, `0x1419b120f`, `0x1419b1244` are in the same function's neighbourhood. |
| `FUN_1419b13a0(param_1)` | `0x1419b13a0` | Runs only if `param_1+0x40 == 3` and `param_1+0x250 == 0`. |
| Caller | `0x1419b1d1e` in `FUN_1419b1bc0` | State machine case 2 (`FUN_1419b1bc0`, `FUN_1419b13a0(param_1)` between `FUN_1419b14c0` and `FUN_1419b1730`). |

Decompiled logic of `FUN_1419b13a0` (Ghidra, `dec_1419b1474.txt`):

1. `param_1+0x250 ← FUN_141995a80()`: a 0xd0-byte "pitch effects" object (constructor `FUN_141995700`, below).
2. `uVar2 = FUN_1419962e0(...)` returns the **registry key `K`**: `FUN_1403d27d0(&DAT_1437cb640, out)` does `out = *(*DAT_1437cb640 + 0x10)`.
3. `local_res8 = FUN_14167dcd0(K)` builds a `fox::gr::Texture` reference. It finds the table entry for `K` in `DAT_14373e938` through `FUN_14167e4c0`. That function returns −1 for the null path `0xb8a0bf169f98`. The reference gets a handle only if the entry state `+0x48 == 1`.
4. **Fallback:** `if (local_res8 == 0 || local_res8->+0x20 == 0) local_res8 = DAT_1437cb0e0`.
5. For every part `p` in `param_1+0xa0` (count `+0x98`): `FUN_1419b0820(p, 0x8d57aa1e, local_res8)`. That writes the mask slot on every sub-object through `FUN_14172f700` (key → texture handle).

So for a `+0x40 == 3` instance the slot is **overwritten** with `K` or the default, whether or not the fmdl bound a mask. Pitch fmdls bind no mask, so nothing is overwritten.

## 2. The default texture (CONFIRMED, static)

- Constructor: `FUN_141976ed0` (init). It creates four 4×4 default textures (`local_30 = 0x84`, `uStack_2a = 4`, `local_28 = 4`, `local_14 = 4`) with the null path `0xb8a0bf169f98`. They are stored at:

| Global | Pixel buffer | Bytes (64 B = 4×4×4) | Decoded (RGBA8, byte order 0 = R) |
|---|---|---|---|
| `DAT_1437cb0e8` | `0x142a87510` | `ff ff ff ff …` | white, opaque |
| **`DAT_1437cb0e0`** (Mask fallback) | `0x142a87550` | `00 00 00 ff …` | **black, opaque** |
| `DAT_1437cb0f8` | `0x142a87590` | `00 00 ff ff …` | a single hue (red or blue, depending on byte order), opaque |
| `DAT_1437cb0f0` | `0x142a875d0` | `00 … 00` | transparent black |

- Assumption: format `0x84` is a 4-byte RGBA/BGRA layout with alpha at byte 3. The conclusion (R = G = B = 0) holds in either order. In an ARGB order the alpha would be 0, which I consider unlikely.
- Other users of `DAT_1437cb0e0`: `FUN_1419ed660` (texture slot resolver, modes 0 and 1 bind it when `DAT_1437cd1f8 == 0`), `FUN_14197f1f0` (fills four slots). So the black default is the engine-wide default, not a pitch-specific one.
- **Per slot vs per technique:** the Mask path picks `DAT_1437cb0e0` directly. It does not depend on the technique. The per-slot defaults differ only by which global the caller picks.
- Teardown: `FUN_14252a740` releases `DAT_1437cb0e0`.

## 3. The override `K` and its object (PARTIAL)

- `FUN_141995700` (constructor for the 0xd0 object at `+0x250`) loads `/Assets/pes16/model/bg/common/pitch/sourceimages/pitch_effects_rgba32.ftex` into `+0x20` (VA string at `0x142a8be40`). It also creates a texture `K` at `+0x28` with `FUN_141995b50`, which keys the registry object `DAT_1437cb640` and makes a 4096×2048 format `0x84` texture (`FUN_1423c9090`). The `+0x28` texture is therefore not the mask. The mask is a separate lookup of `K` in `DAT_14373e938`.
- Registry object `DAT_1437cb640`: writers are `FUN_1419962e0` (reader), `FUN_141995bc6` (reader), `FUN_14252a900` (destructor) and a `[DATA]` xref at `0x14020e7bb` outside any function. The constructor was not located, and the key value is not in the static image.
- I tested whether `K` is the path code of `pitch_effects_rgba32.ftex` (`StrCode64(path w/o ext) & 0xFFFF_FFFF_FFFF`). Candidates `0x8d2dd807f1a9` (with leading `/`) and `0xc7642e63801c` (without) are **not** present in the image, so this is not a static constant. Status: **not established**.

## 4. Consequence for the port (INF, high)

- With `K` absent or unloaded (`+0x20 == 0`), `mask = (0,0,0,1)`, so `ao = max(t0.a, 0) = t0.a` (≈0.003 in st029). This matches our port's zero default. Nothing in the port needs to change for `t6`.
- The B4 premise, "PES's albedo implies `ao ≈ 0.3–0.5`", rested on undecoded PES frames. Main's correction says those frames are double-encoded (+1.0 to +2.2 stops). Using only the decoded frame (below), the gap is ≈2.8×, not 4–5×, and it is not explained by `ao`.
- Lighting cross-check is not done: whether the pitch instance really has `+0x40 == 3` is INF (medium). I did not find the code that sets `+0x40`.

Decoded brightness, same convention for both (Main's `magick -colorspace RGB`):

| Frame | Box | Median R, G, B (0–1) |
|---|---|---|
| PES live_1, decoded | (460,420)–(540,500) | 0.314, 0.408, 0.082 |
| PES live_1, decoded | (880,430)–(990,500) | 0.314, 0.408, 0.082 |
| PES live_2, decoded | (460,420)–(540,500) | 0.329, 0.424, 0.086 |
| Ours, fine st029 frame (display values, no decode) | (820,470)–(1100,620) | 0.118, 0.145, 0.039 |

Ratio PES/ours (green) ≈ 2.8, red ≈ 2.7, blue ≈ 2.1. The boxes differ and the frames are not pixel-matched, so treat this as indicative. The lit-scene exposure gap (B5/B2/B3) is a separate open item.

Main's suggested discriminator: st069 (`alpha = 1` everywhere) would give `ao = 1`, so albedo = 0.8 in our port. That is an expected brightness, not a PES measurement. I did not run the port.

## 5. Live read (BLOCKED)

- `/proc/2510857/mem` (PES2021.exe, running, stadium 29) returns Permission denied. `/proc/sys/kernel/yama/ptrace_scope = 1`, and the process is not a descendant. I did not attach with ptrace or change privileges, because that would pause the game.
- The globals hold heap pointers (for example `DAT_1437cb0e0 → 0x163ed8a0`), which are outside the unpacked image, so the chain cannot be followed statically.
- Next step for a live read: run a read-only path under permission to read the game's memory, or ask for ptrace access. Read `*(DAT_1437cb640)+0x10` (K), then `DAT_14373e938` entry for K (`+0x48`, `+0x20`→`+0x18`), then the pitch instance `+0x40` and `+0x250`.

## 6. GB2.y and GB2.w (B4's open item 4) (CONFIRMED, asm)

Exact sequence, from `asm/pes3ddf_pitch_2020.ps.asm` (1-based lines; scratch copy `the scratch dir/asm/`):

- 38: `r2 ← t0` (`r2.w = pitch_alp.a`, `r2.y = pitch_alp.g`).
- 104: `r8.xyz ← t6` (`r8.x = mask.r`, `r8.y = mask.g`, `r8.z = mask.b`).
- 105: `r2.xy = max(r2.w, r8.x), max(r2.y, r8.y)` → `ao = r2.x`, `gW = r2.y`.
- 106: `r2.yzw = gW·r3.xyz + r7.xyz`. **This overwrites `r2.w`** (the Withers term, not `ao`).
- 108: `r2.yzw = sat(r8.z·(0, 0.1, 0.5, 1))` → `r2.y = sat(mask.b·0.1)`, `r2.z = sat(mask.b·0.5)`, `r2.w = sat(mask.b)`.
- 109: `r2.yzw = (0,1,1,1) − r2.yzw` → `r2.y = 1 − sat(mask.b·0.1)` (= `m2`), `r2.z = 1 − sat(mask.b·0.5)`, **`r2.w = 1 − sat(mask.b)`**.
- 141–142: `r2.x`, `r2.y` are reassigned (`dp3`). `r2.w` is untouched.
- 164: `r1.x = S0`, `r1.y = S1`, `r1.w = S2` (`S_i = sat(lin·Srm_i + Srm_{i+3})`).
- 166: `r1.xy += v7.zw` (VS `o7.z`, `o7.w`; see §7).
- 174: `r0.x = slope.R·match`, `r0.y = slope.B·r6z` (`r6z = Pw·(em2−em1)+em1`).
- 175: **`o2.w = r0.y · r1.z`** → **GB2.w = slope.B·r6z·lin** (`r1.z = lin` from asm 83–85).
- 176: `r0.x = slope.R·match·r1.x` (`r1.x = S0 + v7.z`).
- 177: `o2.x = r1.y` → **GB2.x = S1 + v7.w** (roughness).
- 178: **`o2.y = r2.w · r0.x` → GB2.y = (1 − sat(mask.b))·slope.R·match·(S0 + v7.z)** (specular).

Answers:

- **GB2.y** is the scalar specular intensity. It is multiplied with the ID-3 F0 and tint in lighting. Its `r2.w` is `1 − sat(mask.b)`, **not** the Withers `gW` value. The port's `m4 = 1 − clamp(mask.b)` (`pes_pitch.gdshader`, GB2 section) matches this factor, and the factor is zero for a black mask. It omits `(S0 + v7.z)`, so it is missing the VS term.
- **GB2.w** is `slope.B·r6z·lin`. Pitch emits shading-model ID 3, and lighting reads GB2.w only for IDs other than 1/3, so **GB2.w is dead for the pitch**. The port's GB2.w is not used.
- The B4 port's "`m4·slope.R·match·S0`" is correct for `m4 = 1 − sat(mask.b)` and wrong only in dropping `v7.z`.

## 7. VS fog terms feed roughness and specular (CONFIRMED, asm; magnitudes INF)

- VS `o7.z` (asm 125) and `o7.w` (asm 129) are `clamp(poly(r5.y)+0.5) − 0.5`, where `r5.y = max(…, 0)` (VS asm 107–118). `o7.z` uses `(−0.0693, 0.0488, −0.00453, 0.000109)` and `o7.w` uses `(0.3929, 0.0167, 0.00118, −0.000208)`.
- At `r5.y = 0` (near fog zero): `o7.z ≈ −0.069`, `o7.w ≈ +0.393`. They grow slowly with `r5.y`. Exact `r5.y` range was not evaluated (needs the VS `r3.w`/camera constants).
- PS asm 166 adds them: `GB2.x = S1 + v7.w` (roughness, **no clamp** before the output, ≈1.15 at near range), and `GB2.y` uses `S0 + v7.z` (≈ −0.07).
- Effect: roughness is about +0.39 above the port's `S1` at near range; specular is about −0.07 on `S0`. This does not change diffuse albedo, so it does not explain the pitch brightness gap. It does change the pitch specular and roughness, and the port currently drops it ("report 7.6 skip"). That skip is wrong for the PS as shipped.

## 8. Unresolved and blockers

1. `K` (the registry texture key) and whether it is resident in a match: open (§3, §5).
2. Whether the stadium pitch instance has `+0x40 == 3`: open (INF).
3. Live `t6` SRV in the pitch draw: blocked by Yama `ptrace_scope=1` (§5).
4. Format `0x84` byte order: assumed RGBA/BGRA with alpha at byte 3 (§2).

## 9. Recommended next step

- If K is never resident for st029 in a match, the port's zero mask is the engine's value and the `ao` premise should be dropped. The remaining gap must be looked for in exposure or lighting (B2/B3 rig, per Main's correction).
- Add the VS `v7.zw` terms (`S0 + v7.z`, `S1 + v7.w`) to the port's GB2 path. Clamp only where Godot requires it, and mark as `RE-NEW` with this ledger reference.
- Gate the mask on `+0x40 == 3` and `+0x250`, or record that the port treats every pitch as kind 3.
