# B2Lighting — match light rig (report §0-8)
Branch `B2Lighting`, commits `9532037` (parser) → `b56fd83` (Godot side) → `0fd7380` (sky fixes, rig_view).

## What landed
- `src/pes/lighting.{hpp,cpp}`: parses `light_stNNN_a{f,r}` — spans + merged settings (§2.4), slot mapper (§2.3), proxy enable-lists (§4.2), 147 Spot/PointLights w/ parent-chain world transforms, 9 probes (box = own scale, capture via locator transform), TppSky/TppAtmosphere/GrShadowSettings/GlobalLightArray; `SunToDirection` NOAA fit (matches §3.3 model+observed ≤0.005/component), `SunHorizonFade`, `.atsh` parse + `LoadAtsh` (params @0x70, 16-B stride, 48-bit hashes; non-IEEE fp16).
- `fox2.cpp`: EntityLink (type 22) target decode.
- `tests/pes_lighting_test.cpp`: 7/7 pass (`PES21_ROOT=<workdrive>/PES21`).
- `src/gdext/pes_lighting.*` (`PesLighting.setup`): sun (fit dir, atsh colour, PSSM + blur, 2° disk), moon, floodlights (lumens/K/range/cone/falloff, span-group visibility), ONCE box-projected probes, sky uniforms, fog, LINEAR + fixed rig exposure (B3 owns adaptation). `use_physical_light_units=true` in project.godot.
- `godot/shaders/pes_sky.gdshader` + atmosphere helpers in `pes_common.gdshaderinc`: Bruneton analytic single-scatter (ray-marched optical depth), 1° disk ×10·I·T, flat below-horizon colour, airglow, AT_CUBEMAP_PASS → atsh SH9.
- `match.tscn` owns WorldEnvironment/Camera3D/PesLighting; `match.gd` binds them (2-line hook after `_build_stadium`, `_build_environment` + `SUN_ROTATION_DEG` removed). B1Camera confirmed no collision (their hunk stays in `_place()`).
- `godot/match/rig_view.{tscn,gd}`: stadium + rig inspection scene (`--stadium/--weather/--time-zone/--season/--hour/--minute/--yaw/--pitch/--dist/--target`).

## Frames (all in `<workdrive>/gf-B2Lighting-scratch/frames/`)
- `rigday/frame00000004.png`: Godot st002, 15:00 span, yaw115/pitch-4/dist55 — sky gradient renders (zenith ~(131,166,219)); the stadium renders black: camera at y≈4.8 sits inside/behind stadium geometry from this angle (still diagnosing framing, not lighting).
- `probe.png`: OBS `PES-vk` source is live but shows the owner's foreground app (anime stream), not PES — no PES frame obtainable right now.

## NOT visually verified (stated, not guessed)
No Godot-vs-PES same-shot comparison yet: the Godot frame needs a working camera angle into the bowl, and the PES side needs the owner running a st002 day match (stadium, time, weather, broadcast camera TBD by owner). Consequently the two calibration knobs the report leaves to frames are at decoded defaults, uncalibrated: sky `sun_intensity` (= lux·π·skyColorSunScale) vs Godot's lux→radiance scale, and camera `exposure_multiplier` (= 2^(initEV+comp) = 2^-10.34 for the 15:00 span). Sky horizon band currently clips white — under calibration, not committed as final.
