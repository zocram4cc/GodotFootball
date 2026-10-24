# PES turf mask content, cb4[1], st021 BC7 texels + refined point prediction (TurfMaskRE)

Status: read-only work. Install VFS at `<workdrive>/PES21` (GF `PesVfs`),
image `~/ghidra-projects/scratch/Unpacker/pes_full.bin` (VA = fileoff + 0x140000000),
live PES pid 2510857 (read-only `pread` only). Scratch:
`<workdrive>/gf-TurfMaskRE-scratch/` (absolute paths; extraction scripts,
decodes, disassembly notes). Read with `docs/render/pes-render-TurfFins.md`
(the fin pipeline), `docs/render/pes-render-TurfPlacer.md` (placer + DXT texels),
`docs/render/pes-render-TurfColour.md` §3 (boxes), `docs/render/pes-render-DarkTurf.md`
(the ticket).

Ticket: three named stand-ins — (1) the mask content (runtime global at t1/t5),
(2) `cb4[1]`, (3) the BC7 texels of st021's `turf_nrm`/`turf_bsm`/`turf_srm` —
then the refined point prediction per box (`pes-render-TurfColour.md` §3) and
the stripe contrast.

Bottom line: `cb4[1]` is **CONFIRMED** as the PitchOffset affine
`(1/139.82, 1/78.91, 0.5, 0.5) = (0.00715205, 0.01267266, 0.5, 0.5)` (static
codecs + image immediates, §2). All six st021 BC7 turf files are **CONFIRMED**
decoded with an off-the-shelf BC7 decoder (cross-validated, §3); none carries
mow bands (extends the Placer negative to BC7 + the pitch family). The mask
builder path is **CONFIRMED** (Fox singleton texture family + all creation/
reader sites, §1); its texel content stays **OPEN** (GPU-side D3D11 resource —
CPU heap holds descriptors only; exact object evidence + why the read fails in
§1.4). Refined prediction (§4): net pitch factor **0.69–0.86, mid 0.78**
(needed 0.67–0.83, mid 0.74) — MEETS with overlap; per-box values share one
range (coverage is uniform; box differences ride the still-OPEN mask).
Stripe Michelson stays a range, not a point: visible bands return (0.2–0.3)
iff the runtime mask carries them, else flat.

## 0. Float/byte table (all read from the image per brief rules)

| # | instruction | operand | bytes | value |
|---|---|---|---|---|
| M1 | `0x141a357cd movaps xmm0,[rip+0x106d8cc]` (len 7, rip `0x141a357d4`) | `0x142aa30a0` | `00 00 00 00 00 00 00 00 00 00 00 3f 00 00 00 3f` | UvOffset = (0, 0, 0.5, 0.5) |
| M2 | `0x141a35810 movaps xmm2,[rip+0x106d809]` (len 8, rip `0x141a35817`) | `0x142aa3020` | `66 06 49 3f 00 00 00 00 00 00 00 00 00 00 00 00` | MatParam = (0.78431373, 0, 0, 0) |
| M3 | `0x141a34d78 mov [rbx+0x140],0x430bd1ec` | imm32 | `ec d1 0b 43` | 139.82000732421875 |
| M4 | `0x141a34d85 mov [rbx+0x144],0x429bd1ec` | imm32 | `ec d1 19 42` | 78.91000366210938 |
| M5 | `0x141a34d8f mov [rbx+0x148],0x3f000000` | imm32 | `00 00 00 3f` | 0.5 |
| M6 | `0x141a34da8 movss xmm3,[rip+0xb30850]` (len 8, rip `0x141a34db0`) | `0x142565600` | `00 00 00 3f` | 0.5 |
| M7 | `0x141a3559c movaps xmm14,[rip+0xb35877]` (len 9, rip `0x141a355a5`) | `0x14256ae1c` | `00 00 80 3f` | 1.0 |
| M8 | `0x141a357dc divss xmm2,[rbx+0x144]` | struct+0x144 | — | xmm2 = 1.0/78.91 |
| M9 | `0x141a357e8 divss xmm3,[rbx+0x140]` | struct+0x140 | — | xmm3 = 1.0/139.82 |
| M10 | `turf3d.ps.asm:27 add r0.w,-3.00000012e-01` | literal | — | alpha-test 0.3 (re-confirms TurfFins §1.1) |

PitchOffset derivation (§2) uses M3–M9; `cb4[1]` value = M3/M4 reciprocals.

Decoder validation (this session): `grassfin_bsm_alp` DXT5 re-decode gives
mean RGBA (3.5, 71.8, 248.9, 76.0), alpha≥0.3 survival 0.303 — identical to
TurfFins §0; `common turf_nrm` DXT5 gives (255.0, 126.6, 0.0, 126.9) —
identical to TurfPlacerRE §2; `common turf_bsm_alp_rgba32` RGBA8 raw gives
(11.9, 85.1, 65.3, 97.7) = TurfPlacerRE's (65.3, 85.1, 11.9) BGRA-swizzled
(DarkTurf §0 swizzle). BC7 smoke: zero-block → (0,0,0,0), one-block →
(255,255,255,255).

## 1. The mask: builder (CONFIRMED) + content (OPEN, with exact evidence)

### 1.1 Which global (CONFIRMED: cell, object, live pointers)

- The state-7 turf bind reads the mask from `[rip+0x1d95834]` at
  `0x141a358b5` (`4c 8b 0d 34 58 d9 01`, i.e. `mov r9,[rip+disp]`) with
  `r8d=0xd365d87d` (`Grass_MaskMap_Tex_LIN`; TurfPlacerRE §1.3 #4).
  Target: `0x141a358bc+0x1d95834 = 0x1437cb0f0` (CONFIRMED arithmetic).
- Live (read-only `pread` pid 2510857, this session): cell holds
  `0x173ed900`, next slot `0x1437cb0f8` holds `0x173ed8d0`; neighbours
  `0x1437cb0e0=0x173ed8a0`, `0x1437cb0e8=0x173ed870` (four consecutive
  texture nodes; §1.3).
- First qword of each node = vtable `0x1429fad90` (live read; slot0 fn =
  `0x14167d900`, a Fox refcounted-texture dtor/init, §1.2). Image bytes at
  the cell are a stale heap pointer from the dumped process (`0x163ed900`),
  i.e. the cell is written at/after load — **contradicts TurfPlacerRE §1.4's
  "zero-initialised in the image"** (that claim is REFUTED for this cell;
  the dump predates zeroing or never zeroes it).

### 1.2 Builder family (CONFIRMED: 11 constructors + lazy-singleton thunks)

- The vtable `0x1429fad90` is LEA'd by 11 constructor variants in
  `0x14167d600–0x14167e4c0` (`0x14167d67f/6ee/75f/7cf/837/882/919`
  (turf bind's class) `/db4b/dc66/dd27/de09`; CONFIRMED byte-scan).
  Sibling calls: `0x1403cf7e0` (Fox GetObject/AddRef),
  `0x1403d29e0`+`0x14022d070` (create-pair), `0x1416ce1a0/0x1416d00c0/
  0x141702ef0` (per-variant resource alloc), `0x1415a02b0` (GPU upload).
- Lazy-singleton accessors: thunks `0x14020e2d6` (cell `0x1437cb0e8`) /
  `0x14020e306` (cell `0x1437cb0f0`) / `0x14252a7df`+`0x14252a800/810`
  (cell `0x1437cb0f0`), each `lea rcx,[cell]; call 0x1403cf7e0`
  (CONFIRMED disassembly). Sibling init thunks `0x141977130/190` create
  the neighbouring slots (`0x1437cb0d0/0xd8`) via allocator
  `0x1403fb410` + init `0x141976ed0`/`0x14167dba0` (CONFIRMED).
- Turf-side creation call chain for the *sibling* planar-shadow target
  (`[rbx+0x130]`, state 4 via `0x14167daf0` gated on `[rbx+0x159]`) is the
  template: mask creation follows the same Fox-texture path (INF-high).

### 1.3 Reader set (CONFIRMED: 126 code sites; turf is one of many)

- Byte-scan `0x141500000–0x141b00000` for RIP-relative refs to
  `[0x1437cb0c0, 0x1437cb120]`: **126 hits**. The mask cell `0x1437cb0f0`
  is read by ~70 sites spanning `0x14196dxxx` (shared-object wiring),
  `0x1419axxx–0x141aaxxx` (match systems), and the turf state machine
  itself: `0x141a358b5` (state 7 bind), `0x141a33bc1` (variant loader),
  `0x141a373ee/0x141a374ef/0x141a37522` (post-loop passes — same mask
  reused across LOD/patch passes), plus non-turf systems (`0x141a1b9be,
  0x141a2351a/e8, 0x141a2774f/86f/886f/0x141a27b73`, …).
- Consequence: the mask is a **world-space ground mask shared across
  systems** (turf, pitch, shadow), not a turf-private table. Its world
  mapping is the PitchOffset affine (§2): `maskUV = worldXZ·(1/139.82,
  1/78.91)+(0.5, 0.5)` (VS line 52), covering x ±69.91, z ±39.46.

### 1.4 Content: OPEN (GPU-side; exact negative evidence)

- Tried (all read-only, this session): the live node `0x173ed900` + its
  linked blocks (`0x1fee2f0/0x1ff24b0/0x1fee2a0/0x1ff2400/0x02173760/
  0x1ff24e8`) and siblings (`0x173ed8a0/870/8d0`, table `0x02173760`,
  draw objects `0x172467d0/780/7e8/0x1722e110`) were dumped as u16/u32/
  float quads and histograms. They parse as Fox descriptors/linked lists
  (vtable, refcounts, back-pointers, small-float structs), **not texel
  arrays**: no width×height-consistent buffer, no band structure, block
  entropy/distinct-counts of metadata (e.g. 256 B with 23–51 distinct
  bytes, zero-dominated). The D3D11 texture bytes are GPU-resident; no
  CPU-side copy is reachable through the read-only `pread` toolbox.
- The four nodes' `+0x08` fields (7 / 203 / 237 / 1) do not parse as
  texture dimensions under any consistent layout (width×height product
  matches no mip chain); recorded verbatim as an open layout question.
- Stripe-carrier search (this session, CONFIRMED negative): row/col-band
  analysis over **every** decoded file texture — all six st021 BC7 (§3),
  the DXT/RGBA8 set (TurfPlacerRE §2 re-confirmed), plus the pitch family
  (`pitch_alp` DXT5 4096×2048, `pitch_grain`/`pitch_wide` DXT1,
  `pitch_grain_alp`/`pitch_detail_alp` BC7, `pitch02_bsm_alp` DXT5):
  green row/col-band stdev ≤ 0.75 everywhere except pitch-paint maps
  (`pitch_alp`/`pitch_wide`: edges ~150 vs middle ~25 = painted lines/
  apron, no alternation; TurfPlacerRE's "noise" verdict stands). **No file
  texture carries mow bands** — the bands live in a runtime target (mask
  global and/or planar-shadow target `[rbx+0x130]`, likewise runtime).
- VS role of the mask (CONFIRMED asm, bounds what the content must do):
  `r4=t1@maskUV` (lod 0); `r4.x` (red) joins the lush gate
  `max(r4.x, 1.5·r3.w)` (line ~137: `max r0.y, r4.x, r0.y`) feeding
  `o2.w`; `r4.xy` drives the dry/lush ramps (lines 141–144:
  `r4.xy=(r2.w+(−0.04,−0.21))·(2.70,10)`); `r4.z` (blue) weights detail
  (`r5=r4.zzwz·(0.5,1,0.01,0.5)`); `r4.w` (alpha) gates the sway term
  (`r0.x=(0.99−r4.w)·0.06…`). The mask is a **multi-channel control map**,
  and its red channel is the only texel input to the lush gate — if the
  frame's stripes ride the fins, they enter through `r4.x` (or the
  shadow-density `v1.z` from t5).

## 2. `cb4[1]` (CONFIRMED static: value, derivation, slot proof)

- Value: **`cb4[1] = PitchOffset = (0.00715205, 0.01267266, 0.5, 0.5)`**,
  i.e. `(1/139.82, 1/78.91, 0.5, 0.5)` with M3 = 139.82000732421875,
  M4 = 78.91000366210938 (CONFIRMED M3–M9 table + arithmetic
  1/139.82 = 0.00715205, 1/78.91 = 0.01267266).
- Construction (CONFIRMED disassembly `0x141a357d4–0x141a3580c`): UvOffset
  `[rbp−0x20] = (0,0,0.5,0.5)` straight from image const M1; PitchOffset
  `[rbp−0x30]` built as `xmm3=1.0/[rbx+0x140]`, `xmm2=1.0/[rbx+0x144]`
  (M8/M9; xmm14=1.0 from M7), `xmm0=[rbx+0x148]=0.5`, `xmm1=[rbx+0x14c]
  =0.5`, then two `unpcklps` → `(1/139.82, 1/78.91, 0.5, 0.5)`.
- Slot proof (CONFIRMED): bind order at `0x141a3593c/35950` is
  `[rbp−0x30]`↔`0x26b98d2d`=PitchOffset, `[rbp−0x20]`↔`0x91fc8af6`=
  UvOffset, `[rbp]`↔`0xe38487ff`=MatParamIndex_0; technique packing for
  `pes3DDF_Turf3D2020` (`{0x0,0x4,0x5,0x6, 0x8,…}`) puts PitchOffset at
  words `0x8–0xb` = `cb4[1]` (words `0x0–0x6` = index + DriedColor RGB);
  VS uses `cb4[1..4]` only (`cb4[0]` unused — grep CONFIRMED), with
  line 52 `mad r2.xy, r1.xzxx, cb4[1].xyxx, cb4[1].zwzz` the mask affine.
- Neighbours (for the port): `cb4[2]` = UvOffset = `(0,0,0.5,0.5)` (M1;
  VS lines 46/93: slope-UV centre + 0.5 scales); `cb4[0]` =
  `(0.78431373, DriedColor.rgb?)` (M2; DriedColor bind at `0x141a3596d`
  is **skipped** when `2a5=1` — the taken match path — so live
  `cb4[0].yzw` keeps the material default, and the VS never reads it);
  `cb4[3]/cb4[4]` = Parameter0/1 (wind anchors, from Fox material objects
  via `0x1416982d0/0x14172ed90` — values OPEN, live-material only).

## 3. st021 BC7 texel values (CONFIRMED: texture2ddecoder 1.x, cross-validated §0)

Files `Asset/model/bg/st021/sourceimages/tga/#windx11/*.ftex`, mip0 via GF's
own `ParseFtex` (chunked+zlib; `blitbc7.cpp` in scratch, links
`libpesfmt.a`), decoded with the `texture2ddecoder` pip package
(`decode_bc7`; extreme-block smoke + §0 cross-validation). Means are raw
RGBA (file order); engine swizzle (BGRA per DarkTurf §0, re-confirmed on
the common copy) noted where it matters.

| slot | file (st021 VFS winner) | pf/size | decoded mip0 mean (raw RGBA) | min–max | status |
|---|---|---|---|---|---|
| t6 | `turf_nrm.ftex` | BC7 256², 9 mips | (132.0, 127.5, 132.0, 129.4) | RGB 117–133, A 123–136 | CONFIRMED |
| t4 | `turf_bsm_alp_rgba32.ftex` | BC7 512², 10 mips | (24.6, 69.1, 52.7, 248.2) | R 14–41, G 54–82, B 40–69, A 217–255 | CONFIRMED |
| t3-alt | `turf_srm_alp.ftex` | BC7 2048², 12 mips | (3.4, 214.6, 44.2, 169.7) | G 170–231, A 82–255 | CONFIRMED |
| — | `turf_green.ftex` | BC7 256², 9 mips | (51.8, 68.4, 57.3, 255.0) | R 8–125, G 17–140 | CONFIRMED |
| — | `turf_blue.ftex` | BC7 256², 9 mips | (83.0, 55.0, 27.4, 255.0) | R 40–97, G 24–75 | CONFIRMED |
| — | `turf_withers.ftex` | BC7 1024², 11 mips | (29.7, 74.7, 67.7, 255.0) | G 40–127 | CONFIRMED |
| t4 | `turf_bsm_alp.ftex` (DXT5 base-name copy) | DXT5 1024² | (— re-confirms TurfPlacerRE §2: RGB (49.6, 64.0, 19.1) A 210.3) | — | CONFIRMED (prior) |

Notes:
- t6 swizzle: VS samples `t6.xywz`; raw (132, 127.5, 132) ≈ flat
  mid-grey in all channels (band stdev ≤ 0.28) — consistent with a
  near-flat normal/roughness map under swizzle; no bands.
- t4 swizzle: raw green 69.1/255 = 0.271 sRGB (linear 0.0567);
  BGRA-swizzled (52.7, 69.1, 24.6) keeps green. Common-copy base green
  85.1/255 = 0.334 (linear 0.0894). Af/As (green) vs DarkTurf surface
  final 0.0216: st021 **2.63x**, common **4.14x** (latter = TurfPlacerRE
  §2 lo-end; both feed §4).
- `turf_srm_alp` (2048²) is the `2a5=1`-branch slope-table replacement
  path's stadium copy (TurfPlacerRE §1.5); green 214.6 dominant, alpha
  mid 169.7 — sampled as `t3.xwyz` (VS line 177) for the slope LUT.
- Comparison-box values: boxes are screen rects (TurfColour §3), not
  texture UVs — the report gives file means + ranges, not per-box texels
  (screen→world→maskUV needs the live camera; stated openly). All six
  maps are band-free (row/col-band green stdev: nrm 0.20/0.28, bsm
  0.15/0.06, srm 0.75/0.55, green 0.33/0.26, blue 0.32/0.30, withers
  0.33/0.38 — all « 0.283-stripe scale).

## 4. Refined point prediction per box (TurfColour §3 boxes, DarkTurf band)

Needed net (DarkTurf §0/§1.4): 1/1.5–1/1.2 = **0.67–0.83** (mid 0.74).
Model: `net = (1−f) + f·r`, `r = (Af/As)·L` (TurfPlacerRE §4, edge-on
blades on the wrap floor; NdotL(surface) = 0.3909 CONFIRMED sun, normals
CONFIRMED blade-like per TurfFins §1.2).

- `Af/As` (fin/detail albedo ÷ surface albedo, green): t4-seed green
  st021 2.63x / common 4.14x (§3); VS dry/mid/lush anchors (literals
  CONFIRMED this session: dried (0.1451, 0.3216, 0.0353),
  mid (0.2706, 0.3020, 0.2275), lush (0.3124, 0.4017, 0.0)) the mix can
  only sit inside ~0.27–0.40 sRGB (lin 0.057–0.13); adopted **2.6–6x**.
- `L` (sky-ambient ratio): GF sky 0.064 → 0.141; PES-candidate high
  0.108–0.269 → 0.217–0.408 (TurfPlacerRE §4, unchanged). Adopted **0.14–0.30**.
- `r` = 2.6·0.14 = **0.37** … 6·0.30 = 1.8 → truncated to **0.37–0.65**
  (r > 1 excluded: would brighten, contradicted by the frame).
- `f` (fin coverage at broadcast): alpha survival 0.303 (CONFIRMED,
  re-verified §0) × working layers; adopted **0.40–0.50** (TurfPlacerRE
  §4 working value, twin-grid overlap; the full-tile veto stands).
- `net` = 1−(1−r)·f: corners (r 0.37, f 0.50) → **0.685**; (r 0.65,
  f 0.40) → **0.86**; mid (r 0.50, f 0.45) → **0.775**.

| box | PES Y (DarkTurf §0) | needed net | predicted net (f 0.40–0.50, r 0.37–0.65) | verdict |
|---|---|---|---|---|
| grass_a (470-540, 400-440) | 0.1321 | 0.67–0.83 | 0.69–0.86 (mid 0.78) | MEETS (overlap; mid +5% of needed-mid, inside ±10% frame gate) |
| grass_b (340-420, 470-520) | 0.1063 | 0.67–0.83 | 0.69–0.86 (mid 0.78) | MEETS |
| grass_c (700-740, 560-640) | 0.1039 | 0.67–0.83 | 0.69–0.86 (mid 0.78) | MEETS |
| grass_d (860-960, 520-560) | 0.1051 | 0.67–0.83 | 0.69–0.86 (mid 0.78) | MEETS |

(Boxes share one prediction: coverage is uniform; box PES-Y differences
are the mow bands, which ride the runtime mask/shadow path whose content
is OPEN §1.4.) Stripe contrast: PES Michelson 0.283 (TurfFins §1.4
re-measure, unchallenged); GF-with-fins stripe depth = mask-band
amplitude × f — mask content OPEN so no point value: predict visible
bands return (0.2–0.3) iff the runtime mask carries them, else flat.
Giraffe: unmoved (decal composites over fins; paint shows no mow
modulation, TurfFins §1.4).

## 5. Status and open items

- **CONFIRMED:** `cb4[1]` = PitchOffset = (0.00715205, 0.01267266, 0.5,
  0.5) (image immediates + unpcklps construction + bind/packing slot
  proof); mask cell `0x1437cb0f0` + live object `0x173ed900` (+3 sibling
  nodes, 4-cell table); 11-ctor Fox texture family + lazy-singleton
  accessors; 126-reader shared-ground-mask dataflow (turf state 7 +
  sibling passes + non-turf systems); all six st021 BC7 texel means/
  ranges (§3 table, decoder cross-validated); no-bands negative extended
  to BC7 + pitch family; TurfPlacerRE "zero-initialised cell" REFUTED.
- **INF-high:** mask = Fox singleton texture family, created at/after
  load via the `0x14167d6xx` + `0x1403cf7e0/0x1403fb410` path (same
  template as the planar-shadow target); per-patch couverture via the
  twin-grid placer stands.
- **INF-medium:** `cb4[2]` = UvOffset (0,0,0.5,0.5); `cb4[0]` unread by
  the VS (DriedColor skip on the `2a5=1` match path); t4 base = stadium
  BC7 copy preferred when present (2.63x) else common (4.14x) — game
  picks per the `2a5=1` taken branch (TurfPlacerRE §1.5).
- **OPEN (exactly what closes it):**
  1. Mask texel content + `cb4[3]/cb4[4]` (wind anchors) live values:
     needs a D3D11/GPU readback or the material-uniform source — outside
     the read-only `pread` toolbox (D3D hook or frame-capture agent).
     Fixes stripe amplitude and f per box.
  2. PlanarShadow target (`[rbx+0x130]`) content: same tooling; fixes
     `v1.z` (shadow density) per box.
  3. Draw proof: instance count / technique-slot bind for the fin pass at
     the comparison camera (unchanged from TurfFins §5.2).

## 6. Port-spec updates (replaces TurfPlacer §6.1–2 inputs, TurfFins §4.1–2 constants)

1. **Mask affine (pin it):** `cb4[1] = (1/W, 1/H, 0.5, 0.5)` with
   W = 139.82, H = 78.91 (st021; generalise per stadium from the same
   init immediates `[rbx+0x140/0x144]`): `maskUV = worldXZ·cb4[1].xy +
   cb4[1].zw`. `// ponytail: W/H from the turf init path
   (0x141a34d78/85); removes the GF stand-in affine.`
2. **Slope/offset defaults (pin them):** `cb4[2] = (0, 0, 0.5, 0.5)`
   (image const `0x142aa30a0`); `cb4[0]` need not be ported (VS-unread).
   Wind anchors `cb4[3]/cb4[4]` stay stand-ins until read live (item 1).
3. **Textures (VFS winners, §3):** t4 = st021 `turf_bsm_alp_rgba32`
   (BC7 (24.6, 69.1, 52.7, 248.2) raw; apply the DarkTurf BGRA swizzle at
   bind); t6 = st021 `turf_nrm` (BC7 (132.0, 127.5, 132.0, 129.4));
   t3-slope = `grass_slopetable_alp` (common, forced) with stadium
   `turf_srm_alp` BC7 (3.4, 214.6, 44.2, 169.7) on the `2a5=1` path;
   t7 = `grassfin_bsm_alp` (common/turf3d, forced; survival 0.303);
   t1/t5 = runtime globals — stub with white/empty until read live, and
   NEVER tune to the frame.
4. **Cheaper equivalent (allowed: PS only attenuates, TurfFins §1.1):**
   screen/world-space darkening decal over grass only (never over
   line/paint): `darken = 1−(1−r)·f·shadow·fade` with r = 0.50, f = 0.45
   (predicts net 0.775, mid-band), `shadow = v1.z`, `fade` = VS distance
   term. Do NOT tune r·f to the frame: derive r from §3 on settle.
5. **Do NOT change:** t6 binding, `32a²`, floodlight GroupOn, albedo
   lanes, SPECULAR/ROUGHNESS, sun/exposure (DarkTurf §6.1 stands).
6. **Verify with one rendered frame** (stadium 21, day, broadcast p12
   camera, settled EV): re-measure `grass_a..d + giraffe_body`
   (DarkTurf §0 boxes). Pass = grass medians within ~±10% of PES with
   the paint still matched; report Michelson stripe contrast before/after
   (PES 0.283) as the fin-path signature.

## 7. Reproduce

- VFS/extract: `PES21_ROOT=<workdrive>/PES21 pesfmt_dump cat|ls|fmdl|skl`
  (see TurfPlacerRE §7); `blitbc7.cpp` (links `libpesfmt.a`, `-lz
  -lsqlite3`; handles chunked+zlib mips) + `texture2ddecoder.decode_bc7`
  (`pip install texture2ddecoder`) + `decode_all.py` (DXT1/5) for band
  checks (scripts in scratch).
- Image reads: M1–M9 table (§0) via `pes_full.bin` offsets
  (VA − 0x140000000); LEA scans + capstone over
  `0x141a34xxx/0x141977xxx/0x14167d6xx` (note: `python3` lacks capstone;
  use `/usr/lib/python3.10/site-packages` path or the Ghidra bridge —
  the `0x141a34xxx` turf range has no function objects, raw bytes only).
- Lua: `pesfmt_dump cat shaders/dx11/PesShaders_dx11.lua` → `Turf3D`
  technique params/packing (cb-slot proof, §2).
- Shaders: `fsop_extract.py` + `vkd3d-compiler -x dxbc-tpf -b d3d-asm`
  (TurfFinsRE scratch: `turf3d.{ps,vs}.asm`).
- Live (read-only): `sudo -n python3` + `os.pread` on
  `/proc/<pid>/mem` (pid re-resolved; cells `0x1437cb0e0/0xe8/0xf0/0xf8`,
  objects `0x173ed8a0/870/900/8d0` — heap addresses, re-resolve per run).
