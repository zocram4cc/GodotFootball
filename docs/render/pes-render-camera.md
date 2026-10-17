# PES broadcast camera (B1, Dynamic Wide): target and framing port

Surfaces: `src/gdext/pes_broadcast_camera.{hpp,cpp}`, `src/pes/camera.{hpp,cpp}`,
`tests/pes_camera_test.cpp`. **No edit to `godot/match/match.gd`** — the existing
hook `_place()` → `_broadcast.update(_camera, sim.ball_position(), delta)` is all the
decoded target path needs. Branch **B1Camera**, commit `25ce119`, pushed.

## What the port does (every number from the install)

Dynamic Wide = mode 5 from `constant::CameraInplay` (`common/match/constant/constant_match.bin`,
object `cameraInplay.o`) + `PES2021.exe` `.rdata`/`.data`, per `docs/re/pes-re-camera-target.md`,
`-camera-settings.md`, `-camera-live.md`, `-stadium-camera.md` §4.3-4.6.

1. **Shot.** `u = 1−zoom`, `h = height` (live sliders 0.80/0.20),
   `t = clamp((tz + (W − marginUpper)) / ((W − marginUnder) + (W − marginUpper)), 0, 1)`,
   `W = FieldInfo.halfWidth = 34.06`, margins 5/10 (re-verified byte-exact against the
   install's own `cameraInplay.o` this session). Far/near values and the `t` blend,
   `panEff = 1 + (Pan − 1)·0.2` with `Pan` the bilinear of records #0…#3; both
   `paramMax` and `paramMin` carry `isCustomize = 1` (re-verified), so the far/near
   branch is the active one, not the `.rdata` preset bilinear.
2. **k spring.** `v' = clamp((goal − k)·0.02 − v, ±0.005·k); v += v'; k += v`, goal 1,
   reset `k = goal, v = 0`.
3. **Target.** `tgt = (prev + w·raw)/(1+w)` per axis, weight 1.0 → ×1.1/÷1.1, cap 10000,
   floor 1; `target.x = clamp(focus.x, ±xHi)`, `target.z = clamp(focus.z, zLo, zHi)` with
   `xHi = (L+1) − ((L+1)−X_limit)·k`, `zHi/zLo = ±(W+3) ∓ (W+3) ∓ Zmax/Zmin by k`;
   `Z_min/Z_max = −8 / 21.921` always in mode 5.
4. **Framing.** `target.y = LookY + clamp((y + 4)·(60/fpsRef), 0, 5)` — **this session's
   correction:** the lift input is the **y of the smoothed focus triple** (asm
   `0x1408a3d4f`/`0x1408a3d5a` read `[rbp+0x68]/[rbp+0x70]` after the smoothing writes back;
   `0x1408a3e73`–`0x1408a3e79` is the match-state-1 "input := 0" branch, a separate word);
   `cam = (tgt.x·(1 + k(1/panEff − 1)), PosZ·PosY·k, tgt.z + PosZ·k)`; vFOV vertical
   (`KEEP_HEIGHT`), near 0.9, far 4000; always on the +Z touchline looking −Z.

## Live evidence (2026-10-16, owner's match running)

Third independent broadcast sample read live (mode 5, sliders 0.80/0.20):
cam `(−6.55, 19.70, 41.15)`, target `(−7.00, 0.90, −5.34)`, vFOV `23.63°`, near `0.9`.
The model reproduces `cam.y/(cam.z−tgt.z)` as the blended `PosY` (0.4238 vs 0.4238),
`cam.z−tgt.z = PosZ·k` (46.49 m, k = 1.051) and the pan ratio to 2e-3 — all k-free.
Pinned as `DynamicWideInstall.ReproducesTheSecondLiveBroadcastSample`.

**Second live datum: `target.y = 0.90` constant in open play** (every readable instant,
three separate captures) → the live lift is **0.40**, not 0 (which the report's own
cross-check note claimed, and which contradicts the observation), and not the `0.5`
override (that gate sets **k**'s goal, not `target.y` — re-read from raw asm). The decode
gives 0.40 only for a smoothed focus y of **−3.64 m**, so the input is not the ball's
height. Pinned as `DynamicWideInstall.LiveGroundBallLiftIsFortyCentimetres`.

## Acceptance frames (PES OBS vs Godot match scene, same broadcast shot)

`…/gf-B1Camera-scratch/accept_final.png` (also `cmp_accept.png`) — PES (OBS `PES-vk`,
1280×720, left) against the Godot match scene driven by the ported camera (right).
PES frame: `final/pes_1792134900_3.png`, **live play, 59:50, score 85–99**; one crowd, a
green mown pitch, ad-board ring, goalmouth at the left edge, keeper in **yellow**,
free-kick wall in **white**; **daylight, clear weather** (hard sun shadows, dry pitch,
no rain); the ad boards show `O NO BETTING` / `IVIBET` style rings. Godot frame:
`mf_day/frame00000075.png` (`tools/capture_viewer.sh --scene res://match/match.tscn
-- --stadium 2 --hour 14 …`).

**What is verified and what is not — stated plainly:**
- The camera *law* is verified numerically against **three** independent live samples
  (`DynamicWideInstall.{ReproducesTheLiveCamera, MatchesTheLiveBroadcastSample,
  ReproducesTheSecondLiveBroadcastSample}`) at centimetre / 0.05° level: height ≈ 19 m,
  distance ≈ 46 m, vFOV ≈ 23.3–23.6°, near 0.9, +Z side looking −Z, `target.y = 0.90`,
  the z clamp at −8.
- The **frame is NOT visually verified** in the Godot render: the pitch surface renders
  **pure black** in the current match scene (`mf_day/frame00000075.png`,
  `mf_main/frame00000030.png`), from the broadcast camera *and* from the `--follow`
  inspection camera, so the ground is black from every angle. This reproduces on
  `origin/main` (`a3804aa`) **without my commit** — it is a regression from the B2
  lighting merge, not a camera defect: the pitch's surface, its albedo or its lighting is
  lost, while the stands, goal nets and sky still render. B1 cannot accept the visual
  half of the framing comparison until the pitch renders green again.

## Gate

`flock <workdrive>/gf-sim.lock … ctest --test-dir build -j3 --timeout 2400`:
**887/888 passed** in 972 s. The only failure is `SimMatch.AIManagerSwitchGovernsTactics`
(SEGFAULT) — `src/sim/**`, a Track A surface, unrelated to the camera.
`godot_camera_test`, `godot_match_test` and `godot_match_scene_test` all **Passed**
(2.31 s / 8.28 s / 25.46 s), so B2's `_broadcast = PesBroadcastCamera.new()` and the
`PesLighting` node on main are intact with my commit.

## Open (as the ticket asks me to list)

1. **The ball-height lift's numeric input, with the ball in the air.** Formula and every
   constant OBS; the live ground-ball pose shows lift = 0.40, which the decode pins to a
   smoothed focus y of −3.64 m (i.e. *not* the ball's height), and match-state 1 forces the
   input to 0. No in-air sample was obtainable: the ball never went aloft in any window
   where the plugin object was in the heap (the plugin at `0x142609C28` (mode 5) is only
   intermittently resident; the `fox::gr::Camera` pose — which does not expose the lift —
   was readable throughout). Needs one live read of `InplayCamera+0x38/0x44` with the
   ball above ~3 m.
2. **The user's sliders' persisted location** — `CAMERASETTINGS` `+0x04` (zoom/height
   `[2×f32]`) and `+0x0C` (mode) are behind the mutation-obfuscated serializer; the live
   plugin carries 0.80/0.20, which the port hardcodes as `LIVE_ZOOM`/`LIVE_HEIGHT` (the
   user's values, not PES defaults — the ctor initialises both to 1.0).
3. **The mode writer** `[InplayCamera+8]` — still one hop past `0x14087a930`.
4. **k's goal** when the match-state word is 5 (`1.2 − 0.2·clamp(dist/FieldInfo[6], 1)`,
   live-confirmed active: k seen up to 1.062) — the `dist` operands are unidentified, so
   the port keeps goal 1 rather than inventing a distance.
5. **The FOV blend damping** (23.89° predicted vs 23.63° live on identical inputs, while
   `PosY`/`PosZ` are exact), and the mode-5 state-3 `±8.268` stored-target x bias and the
   `cameraMoveAreaSize` dead zone (both need match-state flags GF does not have).

## Note for other tracks

The black pitch in the match scene reproduces on `origin/main` `a3804aa` with
`tools/capture_viewer.sh` (no `--hour` needed). Evidence: `gf-B1Camera-scratch/mf_main/
frame00000030.png` (main checkout, my commit excluded) is black in the same way as
`mf_day/`. Affected tracks: B4 (grass shader / pitch albedo) and B2 (lighting), since the
stands and sky still light.
