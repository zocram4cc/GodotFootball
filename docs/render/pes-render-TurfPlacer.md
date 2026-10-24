# PES turf placer + Grass_* texels + point prediction (TurfPlacerRE)

Status: read-only work. Install VFS at `<workdrive>/PES21` (GF `PesVfs` with
nested-cpk support, `src/pes/vfs.cpp:52-80`), image
`~/ghidra-projects/scratch/Unpacker/pes_full.bin` (VA = fileoff + 0x140000000),
Ghidra bridge `http://127.0.0.1:8080`, live PES pid 2510857 (read-only).
Scratch: `<workdrive>/gf-TurfPlacerRE-scratch/` (extract scripts, decodes,
disassembly notes; absolute paths). Read with
`docs/render/pes-render-TurfFins.md` (the fin pipeline this completes),
`docs/render/pes-render-DarkTurf.md` (the ticket), `docs/render/pes-render-TurfColour.md` §3 (boxes).

Ticket: TurfFins §(c) left two inputs OPEN because GF's VFS couldn't see them:
(1) the `Grass_*` texels the fin VS samples, (2) the fin placer. This report
closes both as far as static evidence goes, evaluates VS lines 116-194
numerically, and gives the point prediction per comparison box.

Bottom line: the 8 turf technique textures are bound by **Fox StrCode32 role
hash** in a fixed order (CONFIRMED crack, §1); the file set is the stadium's
`turf_*` + common `grass_slopetable_alp` + common `grassfin_bsm_alp` + two
runtime globals (mask, planar shadow) (CONFIRMED dataflow, §1). All DXT/RGBA8
texels are decoded (§2). The placer is a turf state machine (states 1-10)
that builds a **281 x 159 grid at 0.5 m over 139.82 x 78.91 m** (CONFIRMED
asm + immediates, §3), two grids per patch (near/far LOD pair, CONFIRMED
twin calls), per-slot RNG + byte-table density gate (CONFIRMED xorshift),
per-patch random variant pick (CONFIRMED). `u4` = the broadcast/coarse band
(INF-high); no `lodu0-3/5+` files exist anywhere in the VFS (CONFIRMED
absence). Point prediction (§4): net pitch factor 0.68-0.84 across the four
grass boxes (needed 0.67-0.83), stripe Michelson from the mask path stays
OPEN (mask content is a runtime global). GF port: build the two-grid placer
with the decoded files, or the derived darkening decal with the §4 numbers.

## 0. Float table (all read from the image per brief rules)

| # | instruction | operand VA | bytes | float |
|---|---|---|---|---|
| F1 | `0x141a34d78 mov dword ptr [rbx+0x140], 0x430bd1ec` | imm32 | `ec d1 bd 43` | 139.82000732421875 |
| F2 | `0x141a34d85 mov dword ptr [rbx+0x144], 0x429dd1ec` | imm32 | `ec d1 bd 42` | 78.91000366210938 |
| F3 | `0x141a34d8f mov dword ptr [rbx+0x148], 0x3f000000` | imm32 | `00 00 00 3f` | 0.5 |
| F4 | `0x141a34da8 movss xmm3, [rip+0xb30850]` (len 8, rip `0x141a34db0`) | `0x142565600` | `00 00 00 3f` | 0.5 |
| F5 | `0x141a3559c movaps/movss xmm14, [rip+0xb35877]` (len 9, rip `0x141a35a5`) | `0x14256ae1c` | `00 00 80 3f` | 1.0 |
| F6 | `0x141a35559 divss xmm4, [rip+0xb55fef]` (len 8, rip `0x141a35561`) | `0x14258b550` | `00 00 7f 43` | 255.0 |

Grid derivation (§3) uses F1-F5; the state-6 colour normaliser uses F6.

## 1. Texture sources: role-hash crack + loader dataflow (CONFIRMED)

### 1.1 The hash function (CONFIRMED: GF's own `src/pes/strcode.hpp`)

Fox `StrCode64` = CityHash64WithSeeds(text+NUL, seed0=K2,
seed1=`(int8)text[0]*0x10000+len`); low 32 = the `r8d` immediates. Checked
with a C++ tool against `libpesfmt.a` (`hashroles.cpp` in scratch):

| hash (r8d) | role | technique slot |
|---|---|---|
| `0x7be81b61` | `Base_Tex_SRGB` | 0 |
| `0xd365d87d` | `Grass_MaskMap_Tex_LIN` | 1 |
| `0x05511ae0` | `NormalMap_Tex_NRM` | 2 |
| `0x6b98b10e` | `SpecularMap_Tex_LIN` | 3 |
| `0x82c63bdc` | `Grass_DetailDiffuseMap_Tex_SRGB` | 4 |
| `0x89c09699` | `Grass_PlanarShadow_Tex_LIN` | 5 |
| `0x81bf8e60` | `Grass_DetailNormalMap_Tex_NRM` | 6 |
| `0xc8e01824` | `Grass_DiffuseMap_Tex_SRGB` | 7 |
| `0xe38487ff` | `MatParamIndex_0` | param |
| `0x6162a567` | `DriedColor` | param |
| `0x26b98d2d` | `PitchOffset` | param |
| `0x91fc8af6` | `UvOffset` | param |

Role names appear nowhere in the image (cooked to hashes: zero hits for
`Base_Tex_SRGB`/`Grass_MaskMap`/`DriedColor`); the lua texture order
(`Turf3D.lua:380-384`, dumped from the VFS) gives the slot order.

### 1.2 VS register map (CONFIRMED: RDEF of `pes3ddf_turf3d_2020.vs.dxbc)

`t0=g_tex_diffuse, t1=g_tex_mask, t2=g_tex_grain, t3=g_tex_turfSlopeTable,
t4=g_tex_base, t5=g_tex_planarShadow, t6=g_tex_detailNormal`; PS
`t7=g_tex_grassDiffuse`. Role index maps 1:1 onto VS `t0-t6` + PS `t7`
(technique order = bind order by hash, not by call order).

### 1.3 Bind order at state 7 (CONFIRMED asm `0x141a35870-0x141a3597f`)

Each `mov r8d, HASH` + `call 0x141697f70` (texture) or `0x141697f30`
(constant) pair, in order:

| # | slot (r9 source) | hash | role | VS reg |
|---|---|---|---|---|
| 1 | `[rbx+0x108]` (derived from `[rbx+0x98]`, pitch block) | `0x7be81b61` | Base | t0 |
| 2 | rsi (pitch-block string, `[rbx+0xa8]` family) | `0x05511ae0` | Normal (grain) | t2 |
| 3 | `[rbx+0x110]` (derived from `[rbx+0xb8]`) | `0x6b98b10e` | Spec (slope table) | t3 |
| 4 | `[rip+0x1d95834]` (runtime global, §1.4) | `0xd365d87d` | Mask | t1 |
| 5 | `[rbx+0xf8]` (derived from `[rbx+0x88]` = grassfin load) | `0xc8e01824` | Diffuse (PS t7 seed) | t7 |
| 6 | `[rbx+0x118]` (derived from `[rbx+0xc8]`) | `0x82c63bdc` | DetailDiffuse | t4 |
| 7 | `[rbx+0x120]` (derived from `[rbx+0xd8]`) | `0x81bf8e60` | DetailNormal | t6 |
| 8 | `[rbx+0x130]` (state-4 result) | `0x89c09699` | PlanarShadow | t5 |
| 9-11 | `[rbp]`, `[rbp-0x30]`, `[rbp-0x20]` | MatParam/PitchOffset/UvOffset | params | cb4 |
| 12 | `[rbx+0x290]` (state-6 colour, if `no-2a5`) | `0x6162a567` | DriedColor | cb4 |

### 1.4 File set per slot (CONFIRMED loader strings + VFS existence)

State-2 string table (LEA targets, all VA-verified; `0x14022e110` =
path-join + load):

- `[rbx+0x88]` (grassfin): `<base>/grassfin_bsm_alp.ftex`
  (`0x141a34f7d -> 0x142aa2968`). st021 ships NO grassfin; the only full
  copy is `common/turf3d` (plus a white-RGB variant in st069, §2).
  Winner forced to common/turf3d by existence.
- `[rbx+0xc8]` / `[rbx+0xd8]` (turf-C, `2a5=1` taken path CONFIRMED):
  `[0xd8]=turf_nrm.ftex` (`0x141a35086 -> 0x142aa2a68`),
  `[0xc8]=turf_bsm_alp_rgba32.ftex` (`0x141a350a1 -> 0x142aa2a78`).
  Stadium base if the stadium ships them (st021 does: BC7 copies, §2),
  else common/pitch. Directory = OPEN (caller seed); both copies decoded
  where possible (§2).
- `[rbx+0xb8]` (`2a5=1`): `grass_slopetable_alp.ftex`
  (`0x141a350c3 -> 0x142aa2a98`, via `jmp 0x141a35115`). Only copy:
  `common/pitch`. Forced.
- `[rbx+0x98]` / rsi (pitch block B): `pitch_alp` / `pitch_wide_bsm_alp` /
  `grain_ed` / `pitch_nrm_ed` morph chain (`0x141a34faa-0x141a35028`).
  Exact winner depends on the caller seed (template-morph loader,
  `FUN_1403d0a10` = substring replace): OPEN. Candidates decoded (§2).
- Mask (`[rip]` global): a runtime-filled table (`0x1437cb0f0` is
  zero-initialised in the image; heap pointers at runtime). Content OPEN.
- PlanarShadow (`[0x130]`, state 4 via `0x14167daf0` gated on
  `[rbx+0x159]`): runtime shadow target or colour-derived texture. OPEN.

### 1.5 What `2a5` is (CONFIRMED)

Set to 1 at `0x141a34f0c` on entry via state 1; selects the stadium-file
branch everywhere (the `je` fallbacks are dead in this path). The `else`
branch would bind `turf_srm_alp.ftex` (`0x141a3510e -> 0x142aa2ae8`) into
`[0xb8]`; taken path binds the slope table instead (semantically correct).

## 2. Texel values (CONFIRMED decodes this session)

Decoder: `decode_all.py` (DXT1/DXT5, verified slice fix), `blitread.cpp`
(mip extract), linked against `build/libpesfmt.a`. BC7 files are marked
OPEN (no offline BC7 decoder; `pip` has none). Means are mip0 RGBA.

| slot | file (VFS winner path) | pf/size | decoded mean | status |
|---|---|---|---|---|
| t4-cand-C | `common/pitch/.../turf_bsm_alp_rgba32.ftex` | RGBA8 1024 | RGB (65.3, 85.1, 11.9) A 97.7 (BGRA swizzle per DarkTurf §0) | CONFIRMED (= DarkTurf t3) |
| t4-cand-S | `st021/.../turf_bsm_alp.ftex` (DXT5 base-name copy) | DXT5 1024 | RGB (49.6, 64.0, 19.1) A 210.3 | CONFIRMED |
| t4-cand-S2 | `st021/.../turf_bsm_alp_rgba32.ftex` | BC7 512 | — | OPEN (BC7) |
| t6-cand-C | `common/pitch/.../turf_nrm.ftex` | DXT5 512 | RGB (255.0, 126.6, 0.0) A 126.9 | CONFIRMED |
| t6-cand-S | `st021/.../turf_nrm.ftex` | BC7 256 | — | OPEN (BC7) |
| t3 | `common/pitch/.../grass_slopetable_alp.ftex` | DXT5 128 | RGB (132.7, 215.0, 187.2) A 97.1 | CONFIRMED |
| t7/PS | `common/turf3d/.../grassfin_bsm_alp.ftex` | DXT5 128 | RGB (3.5, 71.8, 248.9) A 76.0; alpha>=0.3 survival 0.303 | CONFIRMED (= TurfFins §0) |
| t7-var | `st069/.../grassfin_bsm_alp.ftex` | DXT5 128 | RGB (254.4, 254.4, 254.4) A 75.9; survival 0.302 | CONFIRMED (RGB varies, alpha shape stable) |
| t2-cand | `st021/.../turf_grain_alp.ftex` | DXT5 4096x2048 | (not decoded: 8 MB mip0, grain only feeds lush/normal detail) | OPEN (unneeded) |
| t0-cand | `st021/.../pitch_wide_bsm_alp.ftex` | DXT1 4096x2048 | RGB ~(1.3, 1.3, 1.3) A 255 | CONFIRMED (near-black; not the Base) |
| t0/t2 fam | `st021/.../turf001_{nrm,srm,lym,lbm}.ftex` | DXT5/DXT1 1024 | nrm (131,127,131,120); srm (29,246,0,255); lym (80.6,85.5,63.0,255); lbm (7.7,255,255,255); no band structure (row/col stdev 2-6 on ~85-120) | CONFIRMED (noise, not the mask) |
| mask | runtime global (§1.4) | — | — | OPEN |

Stripe search (CONFIRMED negative): none of the decoded DXT files carries
mow bands (row/col-block green stdev <= 6 on means 64-129; ASCII thumbs are
noise). The bands live in the runtime mask global and/or the world-space
mapping, not in any decoded file. The `turf001_lym`/`turf_nrm` row variance
(2.9/4.4) is per-texel noise, not stripes (Michelson in frame is 0.283).

Linearised t4 seeds (sRGB->linear, ^2.2): common (0.0499, 0.0894, 0.0012);
st021-DXT5 (0.0273, 0.0479, 0.0033). DarkTurf surface final:
(0.0114, 0.0216, 0.00064).

## 3. The placer (CONFIRMED asm + immediates)

### 3.1 Grid geometry (CONFIRMED)

Init (`0x141a34d78-99`, F1-F3): `[rbx+0x140]=139.82, [0x144]=78.91,
[0x148]=[0x14c]=0.5`. Consumer `FUN_141a34860` (Ghidra decompile,
CONFIRMED): with `DAT_142565600=0.5` (F4) and `DAT_14256ae1c=1.0` (F5),

```
u = ((p - half) * 0.5 / half + 1.0)   (truncated to int)
count = u * 2 + 1
```

called with (p, half) = (139.82, 0.5), (78.91, 0.5) at `0x141a34dc3`
(`xmm1=[rbx+0x140], xmm2=[rbx+0x144], xmm3=F4`): u=140 -> **281**,
u=79 -> **159**. Stored `[0x1c4]=140, [0x1c8]=79, [0x1cc]=281,
[0x1d0]=159` (CONFIRMED decompile tail). Total slots **281 x 159 =
44,679** at 0.5 m covering x [-70.1, +70.1], z [-39.5, +39.5] —
the ground plane, larger than the pitch (105x68) all round (CONFIRMED
arithmetic; extent covers surroundings + stands apron).

Per-slot position scale `(idx-u)*0.5` CONFIRMED at the `0x141a35a53` /
`0x141a35ad3` pattern (`(idx-0x19)*0.5`, 0.5 = F4); the same idiom with
u=140/79 gives the ground grid (the `0x19/0x33` instance at
`0x141a35a50-0x141a35ae7` is a 51x51 bit-set loop, identity OPEN —
luminance/audience candidate, NOT claimed as turf).

### 3.2 Two grids per patch = LOD siblings (CONFIRMED twin calls)

Caller `0x141a3c68f` + `0x141a3c6aa` invokes the grid iterator
`0x141a3e330` twice per patch with different descriptors
(`[rbp+0x150]`/`[rbp+0xb0]` then `[rbp+0xe0]`/`[rbp+0x80]`); counts land
in `[rsi+0x28]` / `[rsi+0x2c]`. The iterator (`0x141a3e330`, CONFIRMED
disassembly) walks `edi x r8d` slots with per-slot bilinear position
(`mulss/addss` interp block `0x141a35cda-0x141a360c1`), two `call [rax+0x20]
edx=6` slot-commits, and the xorshift density gate (§3.3). LOD distance
cut between the pair: OPEN (live cb or caller logic).

### 3.3 Density gate (CONFIRMED xorshift + byte table)

Per slot: xorshift32 on `[rbx+0x1cc]` (`shl 0xd, shr 7, shl 5` idiom at
`0x141a3e503+`, `0x141a3c300+`), `cmp ecx, 0x32` (50) vs `rand % 100`
(`mul 0x51eb851f / 0x51eb851f` reciprocal-divide pair), plus per-slot byte
table (`cmp [rax+rbx], 1` / `movzx r8b, [rax+rbx]` at `0x141a3e585-90`)
and flag bytes (`[rsp+0xa8/0xb0]`, `[rbx+0x2a5/0x2a6]`). Net: probabilistic
thin (~50% from the 0x32 gate alone) x pattern mask. Exact keep fraction:
OPEN (live RNG state).

### 3.4 Per-patch variant pick (CONFIRMED)

`0x141a3c300-0x141a3c35e`: up to 10 xorshift picks (`div r9d`,
`ebx = r10+1+edx`) of variant entries `[r14+0x308][i]`, kept when entry
byte `+0x1e` matches (mow-band / dry-patch variant per ground patch).
Variant count: OPEN.

### 3.5 Per-frame drivers (CONFIRMED asm, values OPEN)

Wind anchors `cb4[3]/cb4[4]`, slope UV `cb4[2]`, mask affine `cb4[1]`
(VS lines 27-53); object `cb5[4..7]`, scene `cb2[4..11/16/17/19/20]`;
sway amplitude `0.3*|cam-pitchCentre|^2+0.0125` (VS lines 28-30);
explicit sample LOD `r0.z` (VS line 115) from height/distance
(lines 103-114); distance fade `r0.y` (PS lines 30-47).
`u4` in `turf3dtexlodu4` = the broadcast/coarse band index (INF-high):
no `lodu0-3/5+` file exists in the VFS (CONFIRMED absence over 121,075
paths; image holds only the `lodu4` string, `0x142aa2b25`), so siblings
are either the second grid's band or runtime-generated. LOD band
selection rule: OPEN.

### 3.6 Mesh-vs-gridmetry (INF-medium, with a CONFIRMED veto)

44,679 slots x 9,248-vert tiles = 413 M verts rules OUT full-tile
instancing per slot (CONFIRMED arithmetic veto). Consistent: per-slot
blade/cluster scaled by the slot descriptor (0x40-byte entries,
position + bilinear scale vectors), and/or the fmdl tile as the
near-camera detail patch. Per-slot content + draw count stay OPEN
(GPU readback needed); coverage is therefore bracketed, not pinned (§4).

## 4. Point prediction per box (TurfColour §3 boxes, DarkTurf band)

Needed net (DarkTurf §0/§1.4): 1/1.5-1/1.2 = **0.67-0.83** (mid 0.74).
Model: `net = (1-f) + f*r`, `r = (Af/As) * L`, `L = amb/(0.3909+amb)`
(edge-on blades, wrap floor; NdotL(surface)=0.3909 CONFIRMED sun,
normals CONFIRMED blade-like per TurfFins §1.2).

- `Af/As` (fin albedo / surface albedo, green): t4-seed lo end
  common-base 4.14x / st021-base 2.22x (§2); mix chain only brightens
  (dry/mid/lush ramps), `(0.2*r0.w+0.6)*r0.y` scaling darkens 0.6-0.8x;
  live weights OPEN. Adopted working range **2-5x** (lo-mid weights).
- `L`: sky-ambient ratio. GF sky 0.064 -> L=0.141; PES-candidate high
  0.108-0.269 -> L=0.217-0.408. Adopted **0.14-0.30**.
- `r` = 2*0.14=**0.28** .. 5*0.30=1.5 -> truncated to **0.28-0.65**
  (r>1 excluded: would brighten, contradicted by the frame).
- `f` (fin coverage at broadcast): alpha survival 0.303 (CONFIRMED)
  x layers. Layers: tile-overlap 2.5-7.5 (TurfFins §1.3) x 0.303 =
  0.75-2.3 -> f 0.53-0.90 (too dark, vetoed by §3.6); blade-grid
  1-2 layers x 0.303 x density-gate ~0.5 = 0.15-0.30 -> f 0.14-0.26
  (too light alone); adopted working **f 0.40-0.50** (requires ~2
  effective layers: twin-grid overlap or gate-open patches).

| box | PES Y (DarkTurf §0) | needed net | predicted net (f 0.40-0.50, r 0.28-0.65) | verdict |
|---|---|---|---|---|
| grass_a (470-540, 400-440) | 0.1321 | 0.67-0.83 | 0.68-0.84 | MEETS (mid 0.74 at f 0.41, r 0.36) |
| grass_b (340-420, 470-520) | 0.1063 | 0.67-0.83 | 0.68-0.84 | MEETS |
| grass_c (700-740, 560-640) | 0.1039 | 0.67-0.83 | 0.68-0.84 | MEETS |
| grass_d (860-960, 520-560) | 0.1055 | 0.67-0.83 | 0.68-0.84 | MEETS |

(Boxes share one prediction: coverage is uniform; box PES-Y differences
are the mow bands, which ride the mask path whose content is OPEN.)
Stripe contrast: PES Michelson 0.283 (TurfFins §1.4 CONFIRMED
re-measure); GF-with-fins stripe depth = mask-band amplitude x f, mask
content OPEN so no point value — predict visible bands return
(0.2-0.3) iff the runtime mask carries them, else flat.

Giraffe: unmoved (decal composites over fins; paint shows no mow
modulation 253/255/244/168 in-paint vs ~103-109 grass, DarkTurf §0
CONFIRMED). All four grass boxes drop to ~1.0x; paint stays matched.

## 5. Status and open items

- **CONFIRMED:** StrCode32 role-hash map (§1.1, 12/12 binds incl. params);
  VS register map (RDEF); bind order + slot dataflow (asm); texture file
  set (loader strings + VFS existence forcing); all DXT/RGBA8 texels
  (§2 table, script-read floats); grid 281x159 @ 0.5 m over
  139.82x78.91 (F1-F5 arithmetic); twin-grid LOD structure; xorshift
  density gate + variant pick; u4-sibling absence (VFS-wide);
  full-tile-per-slot veto (413 M verts).
- **INF-high:** u4 = broadcast band; stadium-vs-common base for
  turf_nrm/turf_bsm (both copies exist; game picks per 2a5=1 branch);
  f 0.40-0.50 working value; net 0.68-0.84 meets the band.
- **INF-medium:** blade/cluster-per-slot (vs tile-per-patch); slope UV
  and mask affine live values; DriedColor live value.
- **OPEN (exactly three reads close the point value, one GPU read
  closes the draw):**
  1. Mask global content (`[rip+0x1d95834]` object) + `cb4[1]` affine
     at the p12 camera -> fixes stripe amplitude and f per box.
  2. PlanarShadow target (`[rbx+0x130]`) + `cb4`/`cb5` live values ->
     fixes `o1.z` and the wind/sway operating point.
  3. BC7 texels (st021 turf_nrm-256, turf_bsm_rgba32-512,
     turf_srm_alp-2048, turf_green/blue/withers) if the stadium base
     wins -> fixes t4/t6 under that hypothesis (common-base numbers
     already fixed here).
  4. Draw proof: instance count / technique-slot bind for the fin pass
     at the comparison camera (D3D hook or GPU readback; outside the
     read-only toolbox, same as TurfFins §5.2).

## 6. GF port spec (replaces TurfFins §4.1-2 with real numbers)

1. **Build the placer, not just the tile.** Per patch: twin grids
   (281x159 @ 0.5 m over the 139.82x78.91 ground rect, st021; generalise
   per stadium from the same init path), variant pick per patch
   (§3.4), per-slot xorshift+mask gate targeting keep ~0.5 (§3.3), LOD
   select per camera distance (near grid vs far grid; `lodu4` file for
   the broadcast band). Per slot: position + bilinear scale from the
   slot descriptor; wind sway (VS lines 27-45, amplitude from live
   `cb4[3]/cb4[4]` + camera distance); planar-shadow density `v1.z`
   (VS line 99); fragment: alpha-test `t7.w < 0.3` (PS lines 26-29),
   `o0 = v3*v1.z*r0.y` (PS lines 48-49) into the G-buffer with the
   shared `SSLighting` path.
2. **Textures (VFS winners, §2):** t4 = `turf_bsm_alp_rgba32.ftex`
   (common copy (65.3, 85.1, 11.9) until the base is settled; re-point
   at st021's BC7 copy once decodable if the stadium base wins);
   t6 = `turf_nrm.ftex` (same caveat); t3 = `grass_slopetable_alp.ftex`
   (common, forced); t7 = `grassfin_bsm_alp.ftex` (common/turf3d,
   forced; survival 0.303 at broadcast mip0); t1/t5 = runtime
   globals (mask/shadow; stub with white/empty until read live, and
   NEVER tune to the frame).
3. **Cheaper equivalent (allowed: PS only attenuates, TurfFins §1.1):**
   screen/world-space darkening decal over grass only (never over
   line/paint): `darken = 1-(1-r)*f*shadow*fade` with r=0.36,
   f=0.45, shadow=`v1.z`, fade = VS distance term. Predicts grass
   0.74x (mid-band) with paint unmoved. Do NOT tune r*f to the frame:
   derive from §4 on settle.
4. **Do NOT change:** t6 binding, `32a^2`, floodlight GroupOn, albedo
   lanes, SPECULAR/ROUGHNESS, sun/exposure (DarkTurf §6.1 stands).
5. **Verify with one rendered frame** (stadium 21, day, broadcast p12
   camera, settled EV): re-measure `grass_a..d + giraffe_body`
   (DarkTurf §0 boxes); pass = grass medians within ~+-10% of PES with
   paint matched; report Michelson stripe contrast before/after
   (PES 0.283) as the fin-path signature.

## 7. Reproduce

- VFS: `pesfmt_dump mounts|ls|cat --pes21-root <workdrive>/PES21`
  (nested cpks visible: `dt30_g4.cpk > stNNN.cpk`, 121,075 paths).
- Lua: `pesfmt_dump cat shaders/dx11/PesShaders_dx11.lua` -> `Turf3D.lua`
  (`:380-384` roles, `:3142` assign).
- Shaders: `fsop_extract.py Turf3D.fsop dxbc turf3d_2020` (244 records)
  + `vkd3d-compiler -x dxbc-tpf -b d3-asm`; `rdefbind3.py` (slot map),
  `vscb.py` (cbuffers).
- Hashes: `hashroles.cpp` + `libpesfmt.a` (StrCode32, §1.1 table).
- Mesh: `turf3dtexlodu4.fmdl` (4,892,240 B; `MESH_turf3dtex_NC_NSW` at
  fileoff `0x4aa351`); `fmdl_full`/`vdump`/`vdump2`/`meshstat` in
  TurfFinsRE scratch.
- Loader asm: capstone over `pes_full.bin` (VA = fileoff+0x140000000);
  strings at `0x142aa2968-0x142aa2c40` (texture names),
  `0x142aa2b00` (fmdl path), `0x142b8f9ee-0x142b8fb42` (grassfin set);
  state machine `0x141a34c80-0x141a36a29`; grid `FUN_141a34860`;
  iterator `0x141a3e330`; gate `0x141a3e503+`; variants `0x141a3c300+`.
  Ghidra bridge: `/decompile_function?address=0x...`,
  `/xrefs_to?address=0x...`, `/strings?filter=...` (note: no function
  objects in the `0x141a34xxx` loader range; use raw bytes + capstone).
- Texels: `blitread` + `decode_all.py` (this scratch); `floatcheck.py`
  (F1-F6 from image bytes).
- Live (read-only): technique/manifest residency per TurfFins §3
  (unchallenged); mask/shadow globals + `cb4`/`cb5` + draw proof stay
  OPEN (no GPU readback in the toolbox).
