# CameraKeeper: the goal camera frames the conceding keeper at step 1, the kick-off waits for the goal scene (the step-2 scorer jump is gone), the scorer / keeper / referee slots are fed (01-11-26 handover, run 03-11-26)

Branch `CutsceneTiming` (worktree `gf-CutsceneTiming`), pushed heads `2c364a4a` (WIP) and `ff362f40` (cleanup; `git ls-remote origin CutsceneTiming` = `ff362f40`), on `d808d8cb` (origin/main, merged). Labels: CONFIRMED = run or read this session; INF = inference; OPEN = blocker named. Scratch (movies, frames, dumps; PES-derived) is `gf-CameraKeeper-scratch/`; never committed.

## 0. Status against the ticket

| item | result |
|---|---|
| 1. merge `origin/main`, push | DONE (`d808d8cb` is now an ancestor; pushed). |
| 2. slot 21 (conceding keeper), 22/23, slot -> actor map vs captures | 21 and 22 DONE; 23 not decodable (section 2). Map checked against the captures (section 2). |
| 3. step-2 jump confirmed by a movie; restart hold; red-first test | DONE (sections 3, 4). |
| 4. seed-7 goal movie, frames per step | DONE (section 5, every frame examined and measured). A live PES goal burst was taken (section 6) but is a different scene variant: not like-for-like. |
| 5. fast gate | section 7. |

## 1. What changed

- `Match::HoldRestartForScene(bool)` / `RestartHeldForScene()` (`match.hpp`); `Referee::Process` (`referee.cpp`, before the prepare gate): while held and not yet prepared, `prepareTime` is pushed one tick past now and `startTime` moves with it by the same amount (the schedule's prepare-to-start spacing is kept). Not through `AlterSetPiecePrepareTime`: its switch-off path clamps a deferral at 30 s from the stop and a scene can run past that.
- `GfMatch::step` raises the hold before `Process()` while a goal timeline exists (`timeline_ && gateIsGoal_`); a foul scene does not hold (the stoppage programme times its own; StoppageFlow owns that).
- `GfMatch::cutscene_actor()` now returns an `Array` of `{slot, index, ground, facing_deg}`: slot 0 the scorer, slot 21 the conceding keeper (the other side's roster slot 0 = the first man of that side in `teamOf_`), slot 22 the chief referee (`index` -1: GF's referee has no Godot node). Names in `src/pes/goal_cast.hpp`.
- `match.gd::_advance_cut_camera` calls `set_target(slot + 1, ...)` for every entry (the registry slot is the fdc slot + 1: ProcCamera8 section 1). A man with no node gets his two body points as his ground raised by the scorer's measured joint-10 and joint-0 heights (ponytail, goes when officials are drawn). The old `CAMERA_TARGET_SCORER` constant is deleted; the debug `--fdc-target` path builds the same dictionary.
- Tools: `fixdemo_player_dump.py` gains `cast_lists()` (the `--cast` print now uses it); new `test_goal_cast.py`.

## 2. The slot -> actor map against the captures (CONFIRMED)

`tools/dev/cutscene/test_goal_cast.py` (env `GF_CAST_SEQS`, `PES_EXE`, `GF_HOOKS_CFG`; it unpacks each capture with `capz.py`, reads the FixDemo player's human map and the manager's cast list, repacks). Run on six captures (`pid3741448/seq71718, seq142542, seq134037, seq11590`, `pid2738437/seq81215, seq82080`): OK (213 s). Rules it asserts: the cast entry `0x1315` (fdc slot 21) is `(side = scorer side ^ 1, roster slot 0)` (scorer side = entry `0x1300`'s); the human map's actor of slot 0 is `side * 11 + roster` of entry `0x1300`; every present `0x13xx` entry's actor key is `side * 11 + roster`, member = roster; slot 22's actor is key 22, member 0. **The decisive captures are `pid2738437/seq81215` and `seq82080`: the human map holds fdc slot 21 -> actor key 11, roster member 0 (a side-1 keeper, scorer side 0)**, the first captures with the keeper in a human map (ProcCamera8 had only the cast entry). All 29 proc_camera captures of `pid3741448`, `pid1043691` and `pid2738437` were dumped (`dump.txt`): none holds fdc slot 23, so the assistant is not decodable (INF: official roster 1; which of GF's two linesmen is not known) and is not fed. Slots 1..6, 9..18 (team-mates and opponents) stay unset: the draw behind their permutation is ProcCamera8 OPEN 1.
Mutation (run, reverted): keeper entry compared with the scorer's own side -> the test fails on seq71718 (`(1, 0)` vs `(0, 0)`).

## 3. The jump: the kick-off placement fires under the scene (CONFIRMED by a movie)

Seed 7, teams 1 v 2, `--pes-loop`, 1152x648, 30 fps, RADV, through `simslot.sh`; temporary env toggle `GF_NO_SCENE_HOLD` and a `[scene-hold]` log every 27 ticks (both removed in `ff362f40`). Goal scene `0x300bf`, steps start at movie frames 2718, 3082, 3206, ends 3511 (same frames as ProcCamera7).
- Hold OFF: tick 5751 (step 1) scorer at (36.85, 13.69), `prepared false`; tick 5778 (step 2, 27 ticks later) scorer at (-0.66, -9.38), `prepared true`: a 44 m jump, the referee's prepare (the teleport to the kick-off positions) fired inside step 2; `started true` at tick 5913, still inside the scene (ends tick ~6318).
- Hold ON: `prepared false started false` on every logged tick of the scene (4671..6318); the scorer is at (36.85, 13.69) at the start of step 2 and moves continuously; after `ended` (frame 3511) the kick-off is placed and taken (frame 3520 shows the centre circle with both sides in place).

## 4. Tests (the hold)

`tests/sim/pes_goal_scene_hold_test.cpp` (`pes_goal_scene_hold_test`, 3 tests): seed 7, teams 1 v 2, first goal of the match, drive `HoldRestartForScene(true)` every tick for 15 s past the scheduled take: never prepared, never started, goal state kept; release: prepared within 3 ticks, the prepare-to-start spacing equals the armed schedule's, the match restarts. Both loops (`PesLoop` 228 s, `SwitchOff` 31 s); a control without a scene (SwitchOff only; the PES loop's first goal is 4 minutes of wall time) prepares at its scheduled time.
- RED first: with `HoldRestartForScene` present but not read by the referee, both hold tests failed (prepared under the scene); without the method the test does not compile on origin/main.
- GREEN: 4/4 on the final code (before the control was cut to SwitchOff only; the PES-loop hold test took 228 s); after the cut, `*SwitchOff*:GoalSceneNoHold*` 2/2 (the baseline of each mutation below).
- Mutations (each rebuilt, run, reverted): M1 drop `startTime += ...` -> the hold test fails (kick-off taken under the scene); M2 ignore `RestartHeldForScene()` (hold always) -> both tests fail ("no goal in the match": the first restart never comes; the pre-goal loop is bounded so it fails in 80 s instead of hanging). Not mutated: the `!buffer.prepared` guard (a hold raised after the teleport would stretch `startTime` every tick; GfMatch never raises it that late).

## 5. Frames of each step (CONFIRMED: movie `hold/m.avi`, 1152x648; every frame below was looked at; aim angles are the camera forward vs the actor's ground +1.2 m, logged from `match.gd` by a temporary print, removed)

- **Step 0 (2718-3082), frames 2850, 2900, 2950: telephoto on the scorer.** One green-kit man, head and torso filling the frame, tracked (2950 an opponent's head at the left edge). Angle to the scorer 0.14-0.49 deg over frames 2850-2950 at a 45.6 m lens; the keeper is 9-11 deg off. Frame 2760 (before the first pose) is the high pitch overview; **frames 3000 and 3070 are grass close-ups**: at tick 480 the in-force record is a CANM clip (frames 2960-3080: angle to the scorer 27-36 deg, eye 6.8 m up): the layer pick is still ProcCamera8 OPEN 2, not changed here.
- **Step 1 (3082-3206), frames 3086, 3110, 3140, 3170, 3200: telephoto on the conceding keeper's side of the pitch, no 40 m jump.** Camera eye (-0.5, 1.2, 15.2) looking along +x at the keeper (slot 21, ground (46.5, -1.5), 6 m off his goal line, standing). Angle to the keeper **0.40-0.50 deg at 49.8-51.8 m for every frame logged 3090-3200**; the scorer is 9-17 deg off. The pictures show two white-kit (conceding side) men bent forward in front of the stand wall, the nearer one filling the frame's centre-right, with a goal post at the right edge; a green man's arm/shoulder enters at the left. Which of the two is the keeper is read from the numbers (the aim), not the kit: GF draws every man of a side in the same kit. Before: ProcCamera7 frames 3086-3200 showed a corner flag / stand door (empty slot 22, aim at the pitch centre).
- **Step 2 (3206-3511), frames 3215, 3250, 3300, 3350: the scorer, continuous.** Rig 0 eye (56.5, 1.1, -0.2) aims at the scorer 0.48-0.78 deg; the scorer's ground is (36.85, 13.69) at 3210 and 3300 (no jump), and the frame shows him in front of a red barrier, then (3350, after the camera cut at 3310 to eye (-10, 7.7, 16)) kicking, close. Frames 3400 and 3450 are the high broadcast shot (record mode 6, no engine), 3520 the kick-off set-up after the scene. For contrast, hold OFF frames 3200-3350 show the scorer among the kick-off jostle at the centre circle (white and green men in pairs on the halfway line).
Not visually verified: any like-for-like PES frame (section 6); whether PES frames the keeper and the scorer from these eyes. Which of the two white men in the step-1 frames is the keeper is read from the aim numbers, not the picture.

## 6. PES burst

At 09:01:31 CET `watch.log` logged `cut_anchor_calc_474F00` (a PES goal pick; its anchor capture was over the cap, so the log line was the trigger) and the detached watcher (`goalburst.sh`; OBS `PES-vk`, decoded with `magick -colorspace RGB`) took 101 frames over 60 s (`pesburst/`, sheets `sheet_all.png`, `sheet_101.png`; every frame looked at). What PES showed (a different variant from GF's `0x300bf`; the roster is a modded one): frame 0 the high pitch broadcast shot; frames 1-16 (about 10 s) a telephoto from BEHIND the scorer, arms out, running away from the camera; frames 17-36 the scorer from the front and below with the stand behind him; frames 37-60 a low side camera on the scorer and two team-mates hugging; frames 61-79 a mod overlay; frames 80-100 the crowd. **No shot of the conceding keeper appears in this PES goal**, so GF's step-1 keeper shot cannot be compared with it, and the scorer shots differ in camera side (behind vs GF's front/side). OPEN: a PES burst of the SAME scene id (`0x300bf`-class, layer `goal_2019_ins_lostGK`) needs a goal whose scene logs that id (`fixdemo_scene_start` is not in the watcher's allowlist now).

## 7. Gate

`GATE fast: GREEN` at `ff362f40` (`GATE_TIER=fast GATE_SLOTS="3 2" tools/dev/gate.sh <abs build dir>`, full gate env incl. `GF_IMPURE_BANKS`): ctest 100% passed out of 2283 (3970 s; the three new hold tests Passed: PesLoop 205.8 s, SwitchOff 31.8 s, control 29.9 s); `select_recorded`: 0 recorded functions selected in all five sets (no translated-layer file changed); translated-layer drift check and ledger check (`LEDGER.md current`, 38076 rows) green; smokes (seed 7, 2 min) `[determinism] 3b73c968b4a377e0` (switch-off) and `0e480b158cf80bd4` (pes-loop) = main's baselines (gfsim_run never raises the hold: the hold is GfMatch's, so the smoke is not exercised by it).

## 8. Open / next

1. Slot 23 (assistant) and the team-mate / opponent slots: ProcCamera8 OPEN 1 (the permutation's draw), a capture holding fdc 23.
2. The layer pick when records start together (step 0 at tick 480; step 1's second record): `0x141EB0570`, ProcCamera8 OPEN 2.
3. The set word `player+0x78` and the scene RNG seed (ProcCamera8 OPEN 3); the four engine gaps of ProcCamera7.
4. A like-for-like PES goal burst to compare steps 0-2 (section 6).
5. The hold is time-agnostic: after the scene the restart's prepare comes within 3 ticks and the take 2 s (the goal schedule's spacing) later; whether PES's wipe/fade programme runs after the scene (as it does for fouls) is not decoded for goals.
6. The goal's kick-off is held only for goals; foul scenes still run under the GF stoppage schedule (the same fault may exist there; StoppageFlow owns it).
