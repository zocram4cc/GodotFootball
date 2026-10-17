# pes-fix-ClipSwap: the FSM's issued clip is the clip the viewer plays

Branch `refs/heads/ClipSwap`, head `2b8d0f4` (confirmed by `git ls-remote`). Base `origin/main` `02ceef2`.
Commits: `0b00554` IssueClip playback and start; `c642ee0` WIP clip route (the sim records, the viewer plays); `2b8d0f4` INIT stay branch and comments.

## Result: partially done

Done and tested: the route from the FSM's Issue to the played pose, with mirror, start time, clock, playback and crossfade. The FSM now reaches the viewer (every player issues a clip in the scene test).
Not done: a visual acceptance frame. The captures in this checkout show no pitch and no player (see Visual). Gate and post-fix smoke were not run: Main's rule forbids them for this task.

## What the route is

1. `pesmovestep.cpp` adapter, `MoveHostAdapter::Issue`: `pes::RecordIssue(player.GetIssuedClip(), request, c_.parametric)`. The style index (req[0x37]) becomes its PES blend group through `DAT_14344e0e0` (`pes::LoadParametricGroups`, VA `0x14344e0e0`, 17 i32). Each issue bumps a serial. The per-tick playback is stored after `ApplyTail` (P+0x3758).
2. `GfMatch.player_clip(i)` (gdext) returns a Dictionary: serial, motion, group, mirror, start_time, start_rate, rate, duration, playback.
3. `PesMotionPlayer` (`src/gdext/pes_motion.{hpp,cpp}`) replaces `PesLocomotion` (deleted). A new serial starts the clip at its start time, with the outgoing clip crossfaded over the issued duration. The clock advances by `delta * playback` (single clip: seconds; parametric group: cycles over the bracketed cycle length). Mirror: `pesrig::MirrorPose` (limb swap by GF name, axial reflection `(x,-y,-z,w)`, root x negated). Parametric groups: `pes::Bracket` on the request's rate.
4. `match.gd` `_place`: `motion.update(i, skeleton, sim.player_clip(i), delta)`. The phase accumulator and the speed-based pick are deleted. `pes::RateForSpeed`, `pes::SpeedClassLimits`, `speedClasses_` and the stand clip are deleted with their tests.

Deleted GF selection: `PesLocomotion` (speed to group-80 bracket, phase rate, stand clip for still), `RateForSpeed`, `SpeedClassLimits`, `match.gd` `_phases`.

## Bugs found on the way (fixed, each with a red test)

- `IssueClip` (movestep_turn.cpp): playback was an `int` (0.95 became 0) and the start went into `startRate`, which SetMotion never reads. Red on main: `IssueClip.CarriesTheFractionalPlaybackAndTheStartTimeOfASingleClip` (playback 0 vs 0.95; startTime 0 vs 0.25; startRate 0.25 vs 0).
- INIT stay branch (movestep_group_a.cpp:152): `if (!StayDecision)` went idle. The report (pes-re-anim-select.md 2.3) says stay goes to IDLE. A standing player never left state 0, so nothing was issued in the live match. Red: `Init.AStandingPlayerWhoStaysGoesIdle` (NextState 7 vs 1). This changes sim behaviour (see Smoke).

## Tests

Red-first (on main, before each fix):
- `IssueClip.CarriesTheFractionalPlaybackAndTheStartTimeOfASingleClip` (turn test): fails, values as above.
- `Init.AStandingPlayerWhoStaysGoesIdle` (movestep test): fails, NextState 7 vs 1.
- `IssuedClip.*` (3) and `ParametricGroupsInstall.EveryStyleNamesABlendGroupWithMembers` (install): feature missing (compile).
- `MirrorPose.*` (2): feature missing (compile).
- `godot/tests/test_motion.gd` (new): feature missing until built.

Green (run directly, `PES21_ROOT=<workdrive>/PES21`, no lock):
- `pes_movestep_test` 46, `pes_movestep_turn_test` 41, `pes_movestep_run_test` 35, `pes_movestep_step_test` 13, `pes_clip_pose_test` 10, `pes_locomotion_test` 4, `pes_anim_test` 14, `pes_rig_test` 14.
- All sim binaries in `build/tests/sim` (38 suites): all PASSED, none FAILED.
- Godot, all exit 0: `test_motion.gd`, `test_animation.tscn`, `test_match.gd`, `test_character.gd`, `test_player.gd`, `test_textures.gd`, `test_camera.gd`, `test_model.gd`, `test_stadium.tscn`, `test_match_scene.tscn` (passes after the INIT fix; before it, 0 players had a clip).
- Not run: `ctest` gate (forbidden by the new rule), `godot_match_test` and the other ctest entries (the direct runs above cover them by name).

Mutation checks (each applied, run, restored; all caught by the named test):
- IssueClip start back to `startRate`: IssueClip test fails.
- IssueClip playback back to `int`: IssueClip test fails (0 vs 0.95).
- RecordIssue serial not bumped: `IssuedClip.AReIssueIsANewClipAndCountsAsARestart` fails.
- RecordIssue group not mapped: `IssuedClip.AParametricStyleIsRecordedAsItsBlendGroup` fails.
- MirrorPose root x not negated: `MirrorPose.SwapsEach...` fails.
- MirrorPose partner swap removed: `MirrorPose.SwapsEach...` fails.
- Style map VA +4: `ParametricGroupsInstall` fails (85 vs 80).
- INIT condition inverted again: the red test above.

## Smoke

- Run before the INIT fix, seed 7, 20 min: `[determinism] 63286486ec6c84de`, `[balance-passing] passes 45-41 | accuracy 28%-9% | clean 24%-9% | clearances 2-3`. Both equal the brief's main line.
- Caveat: that run did not export `PES21_ROOT` in its shell, so the adapter may have been a no-op. Shell calls here do not inherit exports. The identical line is therefore not evidence of zero delta. It shows only that the run was unchanged.
- After the INIT fix the sim changes (still players now idle and issue). Smoke was not run after it, per Main's rule. Main must run smoke with `PES21_ROOT` set in the environment of `gfsim_run`.

## Visual

Not verified. The viewer (`tools/capture_viewer.sh`, gamescope, RX 6900 XT) renders:
- Broadcast camera, daytime (`--hour 15`): frames 0 and 100 show a stadium exterior (stands, cars, a building) over a black lower half. No pitch, no player in frame. `<workdrive>/gf-ClipSwap-scratch/cap-broad/frame00000000.png`, `frame00000100.png`.
- `--follow 3`, daytime: black. `cap-follow/frame00000040.png`, `frame00000100.png`.
- No examined frame shows a player turning or stepping. The pose evidence is numeric: the scene test has every player's thigh off rest (`posed == 22`) and every player issued (`serial > 0`). Camera and pitch rendering are outside this change (B1 camera, B2 lighting, B4 turf). The owner of that path needs to make a frame of the pitch render before the acceptance frame can be taken.

## Ponytails and open items

- Command speed class: the adapter's `CommandSpeedClass` reads P+0x245a, which nothing writes. The A-AN6 writer (`pes::SpeedClassOf`) is not called from the adapter. Every command is class 0 until it is.
- `CommandSpeedKmh` is current ground speed; PES reads the command's desired speed (vt+0x20 index 0).
- Parametric blend group is matched by its field value (`pes::BlendGroup`). The docs (sim-motion-mapping §6.8) say the key is the sequence ordinal. They agree for group 80 (tested). Groups 81-93 are not checked against their member names.
- Mirrored clips: the body mirrors, the hands do not (the hand-table mirror rule is not decoded).
- Clip end: the sampler clamps (holds the last frame). PES's loop/end rule is not decoded.
- Crossfade keeps only the immediately outgoing clip. PES keeps up to N records until each ends (pes-re-anim-blend §2-4).
- Clips load on first use (a hitch on first play).
- Five arms in movestep_turn.cpp (lines 2227, 2297, 2501, 2564, 2637, marked `// OPEN`) write a slot or rate-ceiling integer into req[0x35] (the playback word, a float per OBS). Their playback is not a PES rate. I kept their numeric value and did not fix them: they need the PES decode.
- The FSM's own `CurrentMotion` / `CurrentClipFrames` still read GF's humanoid clip. The sim's root motion still comes from GF's SelectAnim (A3).
- Remaining sim disconnect: the viewer plays PES clips, the sim moves players with GF's humanoid.
- `A2Wiring` (peer) will add the event-ring loaders on top of this branch; I told them the SHA.
- Not added: docs/changelog entry for the route (AGENTS.md asks for one on permanent changes).
