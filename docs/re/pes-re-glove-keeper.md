# Keeper gloves and the whole-body glove (GloveRule, round 2)

Result: **Partially done.** Dark block root-caused and fixed. Keeper glove
connection NOT met: it needs a geometry decision (below). W whole-body glove
framed and tested.

Branch `GloveRule` = `origin/GloveRule` = `9c33567` (new this round). Earlier:
`64dd3eb` red, `61e56f9` green (hand/glove rule).

## 1. Were keeper 75201's gloves broken before the change? (answer)

No glove was drawn before. Before frame built from the old production code
(`64dd3eb` src = `origin/main` code path). Log: no `glove_l`/`glove_r` lines.
The glove pack was gated off by `!wholeBody_` in `pes_character.cpp` (the
keeper's boots pack `k1377` is a whole body: `boots.fmdl` material `medic`).
The keeper showed bare `hand_l`/`hand_r` (log: `keep .../hand_l.fmdl`, 1781 v).

So the fragmented glove is new output of this branch, not a pre-existing break.
For merge: this branch replaces a bare keeper hand with a fragmented 4cc glove,
so the keeper acceptance decides whether it can merge.

## 2. Root causes (VA = `fmdl`/exe data, CONFIRMED unless marked)

### 2a. Dark block: the texture resolver borrowed foreign maps (FIXED)

- Glove `g1398` `basic_L_fx` names `/Assets/pes16/model/character/glove/g266/glove_nrm.dds`
  and `glove_srm.dds`. `g266` is absent from the install (`pesfmt_dump ls` empty).
- `TextureResolver::Resolve` (`src/pes/pack_rules.cpp`) fell back to a
  same-stem scan of all of `Asset/model/character/`, returning the first match:
  normal map from `glove/g0555`, specular from `glove/g0427`.
- The same scan bound other kits into the whole-body boots pack `k1377`:
  `medic_nrm` and `medic_srm` from `boots/k0130`, `medic_detail_*` from
  `boots/k0130` and `k0684`. Scan run over all glove and boots packs; face and
  common packs need no scan (every reference resolves in its own directory).
- Isolation (viewer, glove only, same camera):
  - base texture only: dark block gone;
  - `NormalMap` skipped: dark block gone;
  - `SpecularMap` skipped: dark block stays.
  So the normal map was the cause.
- Fix: scan deleted (`pack_rules.cpp`, `characterFiles_` and `CHARACTER_ROOT`
  removed, `EndsWith` now unused and removed); header comment updated.
  A missing directory now leaves the texture unbound, as PES binds only the
  directory an fmdl names.
- Test: `TextureResolverInstall.MissingDirectoryDoesNotBorrowAnotherPacksTexture`
  (`tests/pes_formats_test.cpp`). Red on the scan tree: resolves to
  `Asset/model/character/boots/k0986/#windx11/face_nrm.ftex`. Green: `""`.

### 2b. Fragmentation: the 4cc glove geometry is disconnected (NOT FIXED)

Checked and ruled out (all OBS):
- Binding is correct. Per rig joint, the centroid of single-influence vertices
  sits 1–5 cm from that joint's render position, the same as the bare hand
  (`jcentroid`). Names resolve exactly (`hand_l.fmdl` bind = `hand_l.skl`).
- Finger rigidity: for every finger joint, residual
  `(anim_f - anim_wrist) - W_wrist (render_f - render_wrist)` = 0.000 (`resid`).
  All finger joints share the wrist `align` quaternion. The rig is consistent.
- Index buffer complete: 0 unreferenced vertices in every glove mesh (`refd`).
- Winding consistent with normals across the whole glove (`wind`).
- Control: vanilla `g0001` glove (555 v) is one connected piece (rest space)
  and renders as solid connected hands through the same path (frame
  `ctrl_gloveonly_70301`).

What the 4cc glove is (OBS, `skindiag` rest space):
- `g1398`: 1515 verts, 226 index-connected components, 27 after welding
  coincident positions. Islands are finger segments with no shared vertices
  across the seams.
- Skinned, the islands join into one component only at 10 mm
  (`weldtol`: 2 mm -> 19 components, 4 mm -> 10, 6 mm -> 7, 8 mm -> 3,
  10 mm -> 1).
- `g1400` (outfield 75215) is also split (19 pieces).

Decision needed (not implemented): connecting the keeper glove requires a
10 mm weld of cross-island vertices. That is a geometry edit with no PES
evidence that PES does it. Options:
- (A) Weld at load, 10 mm, marked `// ponytail:` as a stand-in for a PES site
  (none identified yet). Makes the glove connected; alters asset geometry.
- (B) Keep PES-faithful geometry and merge with the 4cc keeper seams visible
  at close range. Acceptance "connected" not met.
- My recommendation: (B) for merge, plus a follow-up to find the PES site
  that closes 4cc glove seams (if any). I have no frame of PES showing this
  glove up close; the live PES frame (`pes_now.png`) is too far to show it.

## 3. Current state of the keeper (after fix, `fix1_*`)

- Dark block: gone (both gloves white, `fix1_gloves.png`).
- Fragmentation: still visible. White strokes with holes at the seams.
- Small dark slivers remain at the seams (`zoom_fix_wrists.png`), interiors
  of islands. Not the normal map (that is gone). Not analysed further.
- Acceptance "connected gloves, no dark blocks": **connected NOT met; dark
  blocks met (large ones).**

## 4. W whole-body player 76020 (item 2)

Look: team 760 kit 0, face 76020, glove 1578, boots 1595 (body `k1595`, 42 bones).
Frames: `frames/W_before_76020` (old code), `frames/W_after_76020` (head).
- Before: bare `hand_l`/`hand_r` drawn (1781 v each, `arm_mat`), no glove
  (gate). Hands read as blunt stubs.
- After: `glove_l`/`glove_r` drawn (`Hand`/`skin`, 1848 v each), bare hands
  absent. Fingered hands, connected at the wrist, no seam visible at 1280 px.
  The glove 1578 pack is a skin-coloured hand, not the 4cc cloth glove.
- Test: `test_player.gd` (EDIT-save block, face 76020) fails on the old code
  (`hand_l`, `hand_r` present, no `glove_l`/`glove_r`) and passes on head.

## 5. Tests and mutations

- `pes_formats_test`: red on the scan tree (see 2a); green: 25 passed, 1
  skipped (`VfsInstall.NestedOriginalsSurviveWithoutDownloads`, install-gated).
- Mutation M3 (step 1 disabled, `if (false && vfs_.Exists(p))`): caught by
  `TextureResolverInstall.PlainDirectoryResolvesThroughThePlatformSuffix`
  (FAILED). Reverted; 3/3 pass.
- Mutations from round 1 (M1 `if (true)`, M2 `if (false)`) still stand.
- `test_player.gd` on head: exit 0 for faces 75201 (keeper), 76020 (W), 70301.
  On old code: 75201 and 76020 FAIL the gloved-look check.
- All `pes_*` test binaries linking `pesfmt`/`pesrig` (26 targets) run directly
  with `PES21_ROOT`: all pass (`tests_run.txt`). Skips: `StadiumInstall.EveryTeamResolvesItsStoredStadium`
  and `VfsInstall.NestedOriginalsSurviveWithoutDownloads`, both install-gated.
- GDScript: `test_textures`, `test_model`, `test_character`, `test_player`: exit 0.
- Gate not run (Main gates).

## 6. Frames (viewer, EDIT save, Vulkan RADV NAVI21, log clean)

Keeper 75201 (team 752 kit 0; `frames/`):
- `before_75201` (old code, full), `gloveonly_75201` (gloves only, pre-fix),
  `nogloves_75201`, `fix1_gloveonly_75201` (post-fix, gloves only),
  `fix1_full_75201` (post-fix, full), `exp_*` / `skip_*` (experiments).
Outfield control 70301 (glove 1 = vanilla g0001): `ctrl_gloveonly_70301` (clean).
W 76020: `W_before_76020`, `W_after_76020`.

Not like-for-like with a PES reference (none available of this glove up close).

## 7. Open

1. Keeper glove connection: decision (A) vs (B) above. Blocks "connected".
2. Dark slivers at keeper seams: unexplained, small.
3. The boots body `k1377` (keeper) now has unbound `medic` normal/spec/detail
   maps (were borrowed). Not compared to PES; visually the torso looks unchanged.
4. Docs stale, not edited (`docs/re/` is archive-only):
   `pes-re-whole-body-uniform.md` §7 gloves bullet (l.182–184) and
   `pes-kit-assembly.md` §8.4 (l.175, hands "not traced").
5. Worktree: `git status` shows `extern/godot-cpp` gitlink change from the
   symlink the brief sets up. Not code. Do not stage it.
