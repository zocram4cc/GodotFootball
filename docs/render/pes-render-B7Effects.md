# B7 effects (B7Effects): report

Branch `refs/heads/B7Effects`, head `793adda`. Worktree `<workdrive>/gf-B7Effects`. Commits on top of origin/main: `6b15e2f` parser, `51d94b8` JSON + `pesfmt_dump effects`, `f0aa591` scene, `2e0ad50` merge of origin/main (69ddc77), `793adda` scene uses the PesLighting rig.

## Verdict

The parser, the JSON export and the standalone scene are built and tested. The acceptance (a live PES frame and a Godot frame of the same effect, compared) is **not met**: no live PES frame of any placed effect was captured. Several entry kinds are **not placed** (section 3).

## 1. What landed

- `src/pes/effects.{hpp,cpp}`: parser for effect_config.xml v2 (settings, typed params, creates with extra attributes, comments skipped whole). Numeric types converted at parse time; rgba `0xRRGGBBAA` -> r,g,b,a in [0,1]; `EffectConfigToJson` for the scene.
- `tests/pes_effects_test.cpp` (+ CMake target `pes_effects_test`): 7 tests. Red first (link failures on the missing symbols, then `EffectConfigToJson`). Green: 7/7 with `PES21_ROOT`. Install test covers every canonical `#Win/effect_*.fpk` pack.
- `apps/pesfmt_dump.cpp`: new `effects <fpk>` subcommand (JSON on stdout).
- `godot/effects/`: `effects_demo.tscn/.gd` (scene), `effect_entries.gd` (placement), `screen_rain.gdshader`. `godot/tests/test_effects.gd` + CTest `godot_effects_test` (exit 0 with `PES21_ROOT`).
- Not wired into `godot/match/**`.

## 2. Verification run

- `pes_effects_test` on the merged tree: 7/7 pass.
- `godot_effects_test` with `PES21_ROOT`: rc 0 (placement logic, VFS texture mapping, five models build with meshes, four textures load). Red first: the preload of `effect_entries.gd` failed.
- Scene headless with st002 day-rain config: `effects: 3 placed; unplaced LightBillboard, LightBillboard, LensFlare, LensFlare; 0 orphan creates`. Warnings only for the model techniques in section 4.
- GPU frames (gamescope, RX 6900 XT): overview with all entries `scratch/wt-all/frame00000029.png` (rain streaks, vignette darkening, net_splash block); focus `scratch/wt-leaf/` (pitch_leaf alone, tiny and dark).

## 3. Scope and gaps (stated, not silent)

- "effect_config.xml entries" read as the entries the config instantiates (`<create>`). Placed: `Vignetting` (screen overlay, alpha 0x80), `MultiUvScrollScreen` (two scrolling screen quads, UV speed/wrap/tint from params), `RainDemo` (two GPU particle zones; the emitter box height is not in the config, 10 m assumed; the num_unit split is half/half).
- **Not placed**, reported at run time:
  - `LightBillboard`: st002's `locstar_*.model` are 32 zero bytes (no positions). The populated locators (st069, 2.7 to 4 KB) use an undecoded format.
  - `LensFlare`: layer positions/scales and the `scale` units (20 "m" at 50 m range) are not decoded.
  - `Halo` (st010, 59 of them): `scale` units not decoded (118 "m" tall).
  - Emitter templates in `effect_demo.fpk` (SmokeBomb, Firework, Confetti, …; 15 settings, no creates): cut-triggered by `.fdc` type-3 records, so out of scope for static placement.
- These are the honest blockers: the RE needed is in the lighting and effects docs, not in code I can write from what's decoded.

## 4. Known fidelity gaps (existing gdext mapping, not changed)

- `3d_rain` (fox3DFW_ParallaxReflection), `effect_rainModel` (fox3DFW_ConstantSRGB) and `net_splash` (DBG_BaseTexView) have no shader in `ShaderFor`: drawn as Blin.
- `pitch_ground`/`pitch_leaf` (fox3DDF_Blin_Translucent) draw through Blin, which ignores alpha: translucent quads show opaque.
- Each of these needs a shader decision in `src/gdext/pes_model.cpp` (owner of B5/B4).

## 5. PES frames (live, read-only)

- `scratch/pes/probe0.png` (1280, match clock 185:18) and `scratch/pes/live1-1920.png` (1920, 237:01): day, fine weather, live play. No rain, no vignette darkening, no halo/flare, no net splash.
- The teal leaf rings under two standing players are about 1.2 to 2 m across. `pitch_leaf` is 9 x 20 cm and `pitch_ground` 12 cm, so the ring is **not** matched to either model. Unidentified; not claimed.
- Result: no PES reference for any placed effect. The Godot frames are not compared against PES.

## 6. Install findings (for owners)

- `effect_st069_nf.fpk` is rejected by `FpkArchive::Parse`: its `effect_config.xml` entry overruns EOF by 13 bytes (the XML is complete, followed by NULs). The FPK reader's bounds check is the cause (src/pes/fpk.cpp, not B7's file).
- `#Win/tmp/` copies of stadium packs carry creates without their settings; the test reads only canonical `#Win/effect_*.fpk` (INF: staging leftovers).
- `nf` packs carry rain creates (MultiUvScrollScreen, RainDemo) with no settings; `st046 nf` has a LightBillboard create for index 3 with no setting. Consumers skip them (`placement()` reports orphans).

## 7. Frame environment note

- Early GPU frames were pure white. Cause: this worktree's base (e6487f8, then origin/main 69ddc77) renders a hand-built sky/sun/tonemap as white; the viewer on the owner's main (90007df) configures the same environment through `PesLighting`. The scene now uses the rig (`793adda`); frames show the expected dark ground and grey sky.
- gamescope logs "Failed to bind X0" (the owner's Xwayland `:0`); it falls back to `:1` and captures fine. My shell exports `DISPLAY=:0`; no effect on the result.
- I briefly edited `apps/pesfmt_dump.cpp` by mistake (wrong path) and reverted it with exact line edits; `git diff` on it is empty. No git checkout used.

## 8. Match-scene hook (proposal for Main, not wired)

- Needs a read of a stadium's `effect_config.xml` in GDScript. Today only `pesfmt_dump effects` reaches it, so the scene uses a JSON file. Proposed: a `PesEffects` GDExtension method returning the Dictionary for a pack (wraps `ParseEffectConfig` + the same shape as `EffectConfigToJson`). That lives in `src/gdext/**`, outside B7's files.
- Hook in `godot/match/match.gd` (after the PesLighting rig exists): `EffectEntries.build_entries(camera, screen_canvas_layer, library, config)`, where `camera` is the broadcast camera (rain and the screen layers are parented to it; they read `camera.fov`) and `screen_canvas_layer` is a CanvasLayer. The entries script is `godot/effects/effect_entries.gd`.

## 9. Checks for Main to run

1. `cmake --build build --target pes_effects_test pesfmt_dump godotfootball -j3`, then `PES21_ROOT=... ctest -R 'PesEffects|godot_effects_test'`.
2. `bash tools/check_pii.sh` in the main checkout after the merge.
3. Frame: `PES21_ROOT=... tools/capture_viewer.sh --scene res://effects/effects_demo.tscn --out DIR -- --effects JSON --focus pitch_leaf`, with `JSON` from `build/apps/pesfmt_dump effects "Asset/model/bg/st002/effect/#Win/effect_st002_dr.fpk" --pes21-root ...`.
4. Look at the frame before any visual claim (the placed rain and vignette have no PES counterpart yet).

## 10. Not verified

- Every visual claim about placed entries (rain zone geometry, screen-rain UV scroll, vignette strength). I saw the GPU frames; I did not compare them with PES.
- Each placed entry's semantic against PES (no PES frame exists in the current live state).
- ctest full gate (not run; only the targeted tests above).
