# PES render static-RE: five cheap items (survey §5 S4/S1/S2/S3/S5)

Status: read-only RE. Repo `<workdrive>/GodotFootball` (main `9f1f2d9`), install VFS `<workdrive>/PES21`,
image `~/ghidra-projects/scratch/Unpacker/pes_full.bin` (VA = fileoff + 0x140000000), Ghidra bridge
`http://127.0.0.1:8080` (`decompile_function`/`disassemble_function`/`xrefs_to`/`function_xrefs`/`searchFunctions`/`strings`
only — `get_xrefs_to`, `get_function_xrefs`, `list_*` are 404 on this bridge). Scratch:
`<workdrive>/gf-RenderStaticRE-scratch/` (absolute output paths; nothing written to the repo).
Budget: ~60 requests used (bridge ~25, VFS/probe/reads ~15, doc reads ~20).

Prior work reused, not re-derived: `docs/re/pes-re-camera-settings.md` (CameraSettingsRE: copy blob, ctor stores,
mode chain to `[matchstruct+0x44]`), `docs/re/pes-re-whole-body-uniform.md` (uniform assembly + marking pass +
§7 corrections), `docs/re/pes-kit-assembly.md` §8.7 (Marking_Tex_LIN placement), grading-lut/compose-path docs.

## S4. st021 `groundColor` — CONFIRMED (byte-verified, 1 request + probe)

- VFS: `Asset/model/bg/st021/light/#Win/light_st021_af.fpkd` (459,024 B), fox2 member
  `Assets/pes16/model/bg/st009/light/light_st009_af.fox2` (458,864 B).
- `TppAtmosphere` (entity addr `0x300`): `groundColor` present, type 19 (`FOX2_COLOR`), value
  **`(0.5, 0.5, 0.5, 1.0)`** — read through GF's own `LoadLightRig(vfs, 21, false)` + a fox2
  property walk (probe sources in scratch: `gc_probe.cpp`, `gc_probe2.cpp`; the `_exit(0)` in
  probe 2 is a static-teardown workaround, not a finding — the prebuilt `libpesfmt.a` + a
  locally linked probe double-free at exit; the test binary `build/tests/pes_lighting_test`
  links the same lib without it).
- So GF's default `Vec3 groundColor = {0.5,0.5,0.5}` (`src/pes/lighting.hpp:64`) is not a
  stand-in: the install carries 0.5 for st021/day. The read path (`src/pes/lighting.cpp:311`
  → `src/gdext/pes_lighting.cpp:242` → `pes_sky.gdshader:24,78` `mix(L, ground_color, below)`)
  already delivers it; with the st021 file the frame already matches on this input.
- Sky_Draw asm corroborates the slot: below-horizon path selects `cb4[5]` (`movc`, sky_out
  line 240, per survey §2.4); live `cb4[5]` = 0.18 is the GPU constant, not this file field —
  the two inputs differ and must not be conflated (0.5 file vs 0.18 live cb).
- Verdict: **CONFIRMED, no port change.** Survey §2.4 promoted to byte-verified.
  GF counterpart: already ports (file-driven). Port spec: none.

## S1. Camera writers (mode + sliders) — chain CONFIRMED to the holder byte; final writer OPEN (live)

Mode chain (all bridge-verified this session):
- Ctor store: `0x14087e965` `MOV [RDI+0x8],EAX` (disasm line 287), defaults `[+0xC]=1.0f`
  (`0x14087e968`), `[+0x10/+0x14]={1.0f,0.0f}` (`0x14087e96f`), `k=[+0x68]=1.0f`
  (`0x14087e97b`) — matches CameraSettingsRE §4.
- Switcher `FUN_14087ab70`: rebuilds the plugin iff `[r9+0x44] != edx` (decompile,
  `movzx ebx,[r9+0x44]`; early-out `param_2 == 0x10` with `[param_3+0x116c] != 2` gate).
  Sole caller `FUN_14087a6c0` (`function_xrefs`, from `0x14087ab52`).
- Dispatch `FUN_14087a6c0` (this session's full decompile, `fun_14087a6c0_full.txt`):
  demux `1…6 → InplayCamera` (`0x2e8`), `7…9 → VerticalCamera` (`0x598`), mode 0 = neither;
  ends in the `iVar13==5/6` early-outs or the `FUN_14087ab70` tail.
  (Note: the survey's "one hop past `0x14087a930`" is the same function — `0x14087a930` lies
  inside `FUN_14087a6c0`; there is no separate function at that VA.)
- Manager `FUN_1408799d0` (full decompile `fun_1408799d0.txt`): owns the switcher/demux
  selection — `0x14087a6c0` called only on the `([+0x116c]==2…)/([+0x1554]==1 && [+0x1574]==6)`
  fall-through; everything else selects fixed plugins (`0x140878aa0/0x140878470/0x140885840/0x14087d940`)
  or `FUN_14087a170`. Caller `FUN_14087afd0` ← `FUN_140427740` (frame update; `xrefs_to` chain).
- The mode byte at holder `+0x44` is read at every level (`0x140879cae [r15+0x44]`,
  demux, switcher) but the writer of `[matchstruct+0x44]` has no E8-visible caller
  (vtable-driven manager update) — CameraSettingsRE §4's OPEN hop stands; closing it needs
  a live break on the vtable slot, not more static.
- Serializer: `CAMERASETTINGS` string VA `0x14259cb98` → getter `0x14044e060` (one-line body)
  → computed-call users `FUN_140877810` (×2), `FUN_1404d5110`, `FUN_140441490`
  (`function_xrefs`). `FUN_140877810` is a vtable-construction function (11 KB decompile:
  `param_1[N] = &PTR_…` chains), not the copy path. Copy blob byte-verified from the image
  this session at file offsets `0x450ff0/0x450ffa/0x451000/0x451008`:
  `movsd [rcx+4],[rcx+0x13]` / `mov [rcx+0xc],[rcx+0x1b]` / `movzx/mov [rcx+0x10]` /
  `mov [rcx+0x12],[rcx+0x21]; ret` — i.e. data `+0x04` (8 B) = zoom/height pair INF-high,
  `+0x0C` (u32) = mode int INF-high, `+0x10/+0x12` still OPEN.
- Verdict: mode writer **CONFIRMED to `[matchstruct+0x44]`; the writer of that byte stays OPEN
  (owner/live)**. Slider persistence **INF-high** (copy blob + consumer f32 reads at
  `0x1408a1e77/0x1408a1e5b`); `+0x10/+0x12` meanings and menu names for 0/1/7–9/`0xC` OPEN.
- GF counterpart: `src/gdext/pes_broadcast_camera.cpp:17` hardcodes `LIVE_ZOOM/HEIGHT =
  0.80/0.20` with a `set_sliders` hook; mode fixed at 5 (Dynamic Wide). Port spec if it
  differs: persist sliders per CameraSettingsRE §3 layout (data `+0x04` pair, `+0x0C` mode)
  and select the plugin per the demux map (1…6 Inplay / 7…9 Vertical / 0 none) — needs the
  live `+0x44` writer + a match where the mode actually changes; until then GF's hardcoded
  mode-5 + live sliders stand, no change.

## S2a. Whole-body markings — CLOSED, no port change (PES binds markings to `uni_*` only)

- Marking pass `FUN_141a8b7b0` re-decompiled this session (`fun_141a8b7b0.txt`, 13,641 B):
  iterates `param_1+0x38..0x40` (UniformUnit parts) only; shirt binder `FUN_141a86310`
  where category `< 2`/`== 0` (lines 192–212, 263–309), pants `FUN_141a85fe0` where `== 3`
  (line 227–234). No BootsUnit touch — confirms whole-body-uniform §2.
- Assembly `FUN_141a89120` reads kit+appearance only (no boots consult, §1) — so for W the
  shirt part is built when the look selects a real shirt; but every team-760 save look is
  long-sleeved tail-in → `shirt_in` is a 3-vertex `basic_fx` stub → no `uni_shirts` → the
  marking pass has nothing to bind to (§7). Where PES's per-player W numbers come from is
  still OPEN, but it is **not** a UV-placement gap: placement is closed (kit-assembly §8.7:
  one `Marking_Tex_LIN` on `uni_shirts`/`uni_collar`, 4 affine samples of UV0; leg number
  via `Number_Tex_LIN` on `uni_pants`).
- GF counterpart: `MakeUniformMaterial` binds `Marking_Tex_LIN`/`Number_Tex_LIN` + scales for
  every `uni_*` part (`src/gdext/pes_character.cpp:359–376`); non-`uni_*` (Suit/Skin/Patches)
  correctly get none — this matches PES. The Suit rebind to the kit atlas
  (`pes_character.cpp:214–218`, `dummy_kit` → team atlas) reproduces the PES navy torso
  (whole-body-uniform §4/§6 INF-high, visually verified).
- Verdict: **CLOSED as a no-op — GF already implements the PES rule.** No new static RE can
  place markings on the body Suit because PES never binds them there. Remaining OPEN is a
  live question (where W's per-player numbers come from when `shirt_in` is a stub), not a
  static one. Port spec: none.

## S2b. Whole-body monitor picture — CLOSED static (binding identified; GF binds the same file)

- Face 76020 (`Asset/model/character/face/real/76020/#Win/face.fpk`, via `pesfmt_dump fmdl`):
  `hair_high.fmdl` material `Head [fox3DFW_Constant_NDR]`, `Base_Tex_LIN =
  /Assets/pes16/model/character/face/real/76020/sourceimages/head.dds`. No second picture
  candidate: the pack's `sourceimages/#windx11/` holds only `head.ftex` (2048² DXT5,
  `pf 4`, 1 mip) + `face_{nrm,srm,trm}.ftex` + stubs. GF resolves the same reference to
  `head.ftex` through its fmdl path resolver (`MakeTechniqueMaterial`).
- `head.ftex` decoded (chunked-zlib DXT5 assemble + block map, `head76020_full.png` in
  scratch): a single group image — five figures (mecha + characters) under a cloudy sky,
  i.e. exactly the "group image" the kit-compare doc reports GF rendering vs PES's "one
  anime character". So the file both sides bind is the same group artwork; the difference
  PES-shows-one-character vs file-shows-group is a UV/region selection (which sub-rect of
  `head.dds` the monitor face samples), not a texture-binding gap.
- Verdict: **binding CLOSED (same file both sides); the monitor's picture REGION stays OPEN**
  (needs the `Head` material's UV transform / which rect of `head.dds` PES samples — a few
  bridge requests on the Constant_NDR VS or a live UV read, no owner needed).
  GF counterpart: binds `head.ftex` as `Base_Tex_LIN` (correct file). Port spec if it
  differs: the UV rect/transform for the monitor face once decoded; no texture change.

## S3. Registry-creation path — shape CLOSED (type registers at boot; viewport entries are per-frame)

- Lookup `FUN_1416a3b30` re-decompiled (viewport `+0x430` root / `+0x450` lock, red-black
  lower-bound walk over key `+0x20`, value `+0x28`) — matches compose-path §1.3. Its twin
  `FUN_1416a3a50` is byte-identical logic (second lookup site, used by `FUN_1416a4530`).
- Code callers of the lookup (`function_xrefs`): `FUN_141804200` (`0x14180493a`, the comp
  grading branch) and `FUN_1416a4530` (`0x1416a4586`, the vtable[14] bind-and-return path
  that binds slot 6 when found, else the `local_60` fallback chain).
- Creation side: `ColorGrading` type string `0x142a7f3a8` → `FUN_141950600` (type registration:
  builds the `ColorGrading` object, binds `sdrGameGradingLut`/`sdrDemoGradingLut`/
  `hdrGameGradingLut`/`hdrDemoGradingLut` path slots at `+0xc` offsets `0x130–0x160` and the
  `Light` link) → called once from the boot registrar `FUN_1418fe8f0` (`0x1418fead4`,
  a ~100-call init chain). `FUN_1416a44b0` (hash `0xcb716e233439`, one-store body) has a
  third caller `FUN_141915340`/`FUN_141917990` besides the comp pass.
- So: the LUT *paths* register once at boot (type creation found); the *viewport registry
  entry* (`+0x430` tree) is populated per-frame upstream of the comp pass — the insert call
  itself was not isolated (no insert twin of the lookup in budget; the tree insert likely
  shares the caller's update path). For the port this distinction does not matter: GF never
  needs the registry — the live behaviour is "grading branch always hit in matches, stub
  program bound, t6 never sampled".
- Verdict: **shape CLOSED (INF-high); the per-frame insert call stays OPEN but is not
  port-blocking.** GF counterpart: `godot/post/pes_post_effect.gd` + `pes_post.glsl`
  (reduce/law/compose; grading-lut doc is its spec) already ports the taken branch
  (stub + sRGB + ×0.25). Port spec: none — no registry to port; the `+0x74 = 0xc40002`
  technique id and slot-6 bind are engine internals with no GF equivalent.

## S5. PitchV7 hardening — DONE on main already (no PES read needed)

- `godot/shaders/pes_pitch.gdshader` on main (`9f1f2d9`) already carries both guards the
  survey listed as OPEN:
  - log2 guard: line 85 `max(depth, V7_DEPTH_FLOOR)` with `V7_DEPTH_FLOOR = 6.25e-02`
    (= 1/16, VS asm 113) — camera-below-surface yields `h = max(…, 0)` via line 87.
    (Strictly: a negative `V7_LOG_SCALE * -dy * P[1][1]` argument with dy > 0 still gives
    log2(negative) = undef; in practice `-dy` is positive exactly when the surface is
    below the camera, the only rendered case. Noted, no change.)
  - ROUGHNESS: line 181 `ROUGHNESS = s1 + pes_v7.y` unclamped (as PES asm 177), but the
    only consumer that can blow up is line 207 `r = max(ROUGHNESS, 0.02)` before the
    GGX chain — effectively clamped at use. Godot also clamps ROUGHNESS at the material
    level. No change.
- The survey's other V7 assumptions are now documented in-shader (asm-VA comments
  lines 69–91, 175–182); polynomial literals are asm immediates (hard-coded, per PS style).
- Verdict: **CLOSED, no change.** GF counterpart: itself (PitchV7 `6b3753f` in main).
  Port spec: none.

## Updated Track B port-ready list

| # | Item | State after this session |
|---|---|---|
| P1 | Turf3d fin-cover `FIN_NET 0.775` (`dd70ad3`) | unchanged (unlanded; needs G1 frame) |
| P2 | Sub-rect meter (`b3223ac`) | unchanged (unlanded; needs owner ask 1) |
| P3 | Whole-body markings | **dropped as a port item** — PES binds markings to `uni_*` only (S2a); GF matches. Residual live question (W numbers under tail-in stub) is not a port diff. |
| P4 | Whole-body monitor binding | **file binding confirmed same both sides** (S2b); remaining work is the monitor UV rect (static, small), not a texture bind. |
| P5 (new) | Monitor-UV rect (`head.dds` sub-rect for the `Head` face) | OPEN, static, ~5 req (Constant_NDR VS / `hair_high` UVs). Only new port work this ticket produces. |
| P6 (new) | Camera sliders persist + mode demux per CameraSettingsRE §3–4 | OPEN, needs live `+0x44` writer; GF keeps hardcoded mode-5 + live sliders until then. Not started. |

## Reproduce

- S4: `build/apps/pesfmt_dump cat "Asset/model/bg/st021/light/#Win/light_st021_af.fpkd"` →
  probe `gc_probe.cpp`/`gc_probe2.cpp` (link `build/libpesfmt.a -lz`, same `-O3 -DNDEBUG`
  flags as the lib; static `PesVfs` teardown double-frees — use heap + `_exit(0)`).
- S1: bridge `disassemble_function?address=0x14087e480` (lines 287–296),
  `decompile_function?address=0x14087ab70 / 0x1408799d0 / 0x14087afd0 / 0x140427740`,
  `function_xrefs?name=FUN_14087ab70`, `xrefs_to?address=0x14087a6c0 / 0x1408799d0 / 0x14087afd0`,
  `strings?filter=CAMERA`, `xrefs_to?address=0x14259cb98`, `function_xrefs?name=FUN_14044e060`;
  image bytes at file `0x450ff0–0x45100f` (`fun_*.txt` in scratch).
- S2a: bridge `decompile_function?address=0x141a8b7b0` (`fun_141a8b7b0.txt`).
- S2b: `build/apps/pesfmt_dump fmdl "Asset/model/character/face/real/76020/#Win/face.fpk"` +
  `ls "Asset/model/character/face/real/76020/sourceimages"`; `head76020_full.png` via
  chunked-zlib DXT5 assemble (script pattern in `~/ghidra-projects/scratch/LegNumberRE/ftex_png.py`
  for the container, DXT5 decode inline).
- S3: bridge `function_xrefs?name=FUN_1416a3b30 / FUN_1416a44b0 / FUN_1416a3a50`,
  `decompile_function?address=0x1416a4530 / 0x1416a3b30 / 0x1416a3a50 / 0x141950600 / 0x1418fe8f0`,
  `strings?filter=ColorGrading`, `xrefs_to?address=0x142a7f3a8`.
- S5: `godot/shaders/pes_pitch.gdshader` lines 69–91, 175–182, 207 (main `9f1f2d9`).

## Open questions / blockers

1. `[matchstruct+0x44]` writer (S1 tail) — needs live vtable-slot break; static exhausted.
2. Monitor-UV rect (S2b residue) — static, ~5 req, recommended next static item.
3. Viewport registry insert call (S3 residue) — not port-blocking; skip unless the comp
   branch behaviour ever contradicts the stub spec.
4. Survey corrections to file when archiving: grading-lut digit fix (filed), T1-divergence
   refutation (filed) — untouched by this ticket.
