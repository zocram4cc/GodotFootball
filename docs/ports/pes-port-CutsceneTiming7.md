# CutsceneTiming7: why the goal cutscene shows the broadcast shot (procedural cameras), the first live PES goal of the same scene as GF's, the scorer ramp compared, and the score-branch test (31-10-26)

Branch `CutsceneTiming` (worktree `gf-CutsceneTiming`). Heads: `d8e442c6` (merge of `origin/main`), `bc905440` (test); the report commit follows (`git ls-remote origin CutsceneTiming`). Labels: CONFIRMED = decompile, capture/poll read, test, run or frame seen this session; INF = inference; OPEN = blocker named. PES-derived material (polls, frames, decompiles, GF movie) stays in `gf-CutsceneTiming-scratch/ct7/`, never committed.

## 0. Status against the ticket

| item | result |
|---|---|
| 1. merge `origin/main`, push | DONE (`d8e442c6`). |
| 2. camera | **Root cause found (CONFIRMED), not fixed.** Every camera record in force in the three scenes' layers is *procedural* for most of the timeline; the procedural camera engine (`0x141F7BFE0`, 255-function closure, 100 already in the layer, 155 not) is not ported, so the broadcast camera stays. Only one stretch plays a clip camera, and its frames are in section 1. |
| 3. scorer momentum / gate by PES's arm | **Premise corrected by a live PES goal (section 3):** PES's scorer is also near rest at the goal frame and ramps from 1.3 to 7 m/s over 3 s; PES's gate for this goal opened by the **timer** (215 frames), as GF's does. GF's ramp is slower (full speed at 3.7 s, two stops). The shot-clip stop in GF is measured (section 3.3) but the PES driver is not decoded. |
| 4. score-branch test | DONE (section 4): test added, mutation killed. It exposed a real gap: a goal that leaves the scorer behind gets pick 0 and GF plays **no scene**. |
| 5. PES burst vs GF movie | Looked at both (sections 1, 3.2): **not like-for-like** in time and framing; stated plainly there. |
| 6. fast tier | interim line at the end. |

## 1. Camera: which camera is in force when (CONFIRMED)

Scene `0x300bd` (the scene both GF and the live PES played). Layers per step, with the Camera records read by `fdc_parse.py` (key, `startCutTime` in ticks at 60/s, animePath):

| step (cut) | layer | camera records |
|---|---|---|
| 0 (`0x300bd`) | `goal_cmnCam_outM00` | key 0, start 0, **procedural** (empty anime path, targetKind 4) |
| | `goal_cmnCam_noSwitch20` | key 20, start 9999 (never), procedural |
| | `goal_celebrate_0254_outH10` | key 10, start 180, **procedural** |
| | `goal_celebrate_0254_mayaL1x` | key 11 `camSL` start 180, key 12 `camSR` start 180, key 21 `camSKY` start 360: **clip cameras** |
| 1 (`0x30132`) | `goal_2019_ins_lostGK_00` | keys 0 and 100, start 0, both procedural (targetKind 4, target 21) |
| 2 (`0x3011c`) | `goal_2021_F_turn_Jog03` | keys 0, 10 (start 180), 100, 110 (start 180): all procedural (targetKind 6 target 3 / 4 target 0) |

So the layers resolve (`.fdc` found, parsed, GF logs them) and the timeline hands each step to `PesCutsceneCamera::update`, which returns false for a procedural record in force (the `ponytail:` in `pes_cutscene_camera.cpp`), and `match.gd` falls back to the broadcast camera. Step 1 and step 2 are procedural for their whole length; step 0 is procedural for ticks 0-180, then the same-start group {`outH10` procedural, `SL`, `SR`} (GF takes the first in layer order, a procedural one: INF, PES picks by `selectByRate`, not decoded), then `camSKY` from tick 360.

GF movie (seed 7, teams 1 v 2, `--pes-loop`, 1152x648, RADV gamescope, 30 fps; `ct7/g1/sheet.png`, nine frames looked at: 2730, 2800, 2840, 2900, 2940, 2990, 3050, 3100, 3200; scene start 2592, steps start 2716 / 2959 / 3082, end 3267): the broadcast shot in frames 2730, 2800, 2840 (step 0 before and just after tick 180), 2990, 3050 (step 1), 3100, 3200 (step 2). **Frames 2900 and 2940 (step 0 after tick 360, `camSKY`) show a clip camera**: a high close view of grass and a pitch line, with only a foot at the top left of 2940. The scorer is not in shot.

PES frames of the same scene, step 0 about 6.6 s after the DemoInfo start (`ct7/pes/psheet.png`, six frames, PES frames 11707-11715, `import -window` real frames, no decode needed): a high close camera on the scorer **kneeling on the grass with arms out at the centre of the shot** (a team-mate's legs at the top). Same kind of camera (high, steep, close) as GF's frame 2900, so the clip camera plays and is placed over the same kind of spot, **but in GF the scorer is not there** (he is still jogging at about 2 m/s; section 3). Not like-for-like otherwise: different goal, stadium, kits, time of day, and PES frames 11707+ are a time stretch of the scene I did not align to GF's tick to the frame.

What fixes the camera: the procedural camera. `0x141F7BFE0` is the in-play camera engine driven by the record (mode from targetKind/targetNo bytes `+0xB0..+0xB3`, a 16-parameter style block `+0xD8..+0xEE`, zoom stages `+0xB8..+0xD4`, preset functions at `0x140892100/660/420`, `0x1408921A0` and four `thunk_EXT_*` into `.impdata`, the rig applied to the camera object at `this+0x38` through `0x14089D690`, whose update is a virtual call). `closure.py --dry-run 0x141F7BFE0`: **255 members** (150 `camera`, 58 `cutscene`), **100 of them already translated** in `src/pes/translated`, 155 not; no refusal this time (the `.trace` blocker of `pes-port-ImageGap.md` is gone). What is missing is not the formula but the camera object graph it writes into (the in-play camera instance, its modes, and the stage/human lookups for targetKind 4/6), and a capture oracle for it (no hook on `0x141F7BFE0`). A stand-in framing was not written: it would be an invented camera.

## 2. Same-start camera choice (OPEN)

`selectByRate` bytes of the group at tick 180 (script-read): `outH10` and `SL`/`SR` carry the same first bytes `[100, 70, 10, 0, 0, 100, 0, 100, ...]`; the 23 bytes are SelectByRate, target front/back, the camera classes and 16 camera-position flags (`docs/re/pes-re-cutscenes.md` section 3 row `+0xD8`). The consumer was not found: `0x141F3F5E0` (the candidate of `pes-re-camera-pose.md`) is the record-change comparison, not the choice; `0x141F42110`/`0x141F41F50` walk the cut list and the human records.

## 3. The scorer (live PES goal, CONFIRMED by poll)

### 3.1 The oracle
`live_dpi_poll.py` (read-only, `/proc/<pid>/mem` through `live_srv.py`) on pid 3061186 logged **a goal of scene `0x300bd`**, the scene GF played: DemoInfo from PES frame 11351 (scorer = man 1; DemoPlayerInfo `+0x18` = (49.56, 0, 32.06), `+0x6C` = 1), `ready` (the gate) at frame **11566 = 215 frames later** (`0x14047A710`'s timer, fps x 4, minus the creation frame, as CutsceneTiming3 section 2 logged for g4), state 4 then 5 at 11568, state 10 from 11572, still state 10 at 11727. Poll rows: `ct6/dpi_a.jsonl` (changed bytes of 22 men, scratch).
**Gate:** PES opened this goal's gate on the timer, not on distance/facing. GF's gate also opens on the timer (124 render frames at 30 fps = 223 ticks, `StartTimeline` stand-in lag included), so for this goal **the GF gate matches PES**; the ticket's "gate opens by PES's distance/facing arm after a PES-like run" is not what PES did here. Whether the distance/facing arm opens on a goal where the scorer reaches the spot earlier is OPEN (one goal).

### 3.2 The scorer's ramp, PES vs GF
PES man 1, position INF from man `+0x5B4` (x) and `+0x5BC` (z) (they change smoothly from (37.1, 2.8) to (45.0, 19.5), then freeze at the end of his run; the other candidate words `+0x5A0..0x5AC` were not used), speed over 0.5 s windows from the DemoInfo start: **1.26, 1.28, 2.83, 3.87, 5.22, 6.72, 7.01, 7.71 m/s at 0, 0.5, ... 3.5 s**; at 4 s he reaches (45.0, 19.5) and stops. So PES's scorer is also near rest at the goal frame (about 0.5 m/s at frame 11350) and **reaches 7 m/s at about 3 s**, no stops on the way.
GF (CutsceneTiming6 section 1, headless trace): `rate` 0.014 rising 0.08 per 111 ms, full speed class at 3.5-3.7 s, two stops (0.4 s and 0.9-1.4 s), 2.7 m/s at 4 s. GF is slower by about 0.5-1 s and has stops PES's ramp does not show.
The earlier "PES's scorer runs at 23-27 km/h at the goal frame" (`live_f.jsonl`) was another goal; this one starts from rest. One goal each: not a distribution.

### 3.3 Where GF's shooter stops (temporary trace, removed)
Headless `gfsim_run --pes-loop --seed 7`, the first goal (tick 812): the scorer ran at 2.8 to 4.0 m/s, then `0.0` about 117 ticks (2.2 s) before the goal registered, i.e. at the kick, then 0.6 rising to 2.1 m/s, falling to 0.1 at the goal. Later goals of the same run: 0.1-0.3 m/s before the shot (rebounds) and two with a dip to 0.0 before a slow re-start. So GF's shooter **stops at the kick** and creeps up after it; PES's kick action's end and the movement it hands back to MOVESTEP is the KickMotion/Trap lane's decode (`pes-port-KickMotion.md`: the kick motion pick is ported, its post-kick velocity is not). Not changed here.

## 4. The score branch (CONFIRMED, test, mutation)

`TEST(GoalScenes, ATrailingScorersGoalDoesNotPickTheScorersCelebration)` (`tests/pes_goal_anchor_test.cpp`, goal A's inputs, builder mode): counters {1,1} (the captured case) pick `0x30034`; {1,2} (the scorer's side one behind after its goal) pick something else. A grid (period 1/2/4, clocks, scores 1-0, 1-1, 2-1, 3-0 versus 0-1, 0-3, 1-2) showed the score changes the pick only through the *scorer-trails* condition: the pick is **0** and the builder's list is `{0, 0, closing, lead}` (`0x30136`/`0x30132` + `0x3011C`); level and leading scores give the same list. The decider functions that read the difference: `0x1404770A0`, `0x140471590`, `0x140479210` (all call `0x140A80EF0` -> `0x140A80D10`), `0x1404790B0` (not translated).
Mutation run, rebuilt, reverted: team counters written as 0 (`0u * b.score[team]`) -> the new test FAILS (it was the survivor of CutsceneTiming6). `pes_goal_anchor_test` 20/20 after.
**Gap this exposed (OPEN):** `FixDemoSelector::PlaceGoal` returns `nullopt` for a pick outside the goal table, so **a goal that leaves the scorer behind plays no scene in GF**, while PES's builder produces a list for it. What PES's anchor and radius read for scene 0 (and whether A0+0x54 per-side state, set to 8 by `0x140478090` and written by a caller not found, is what GF leaves wrong) is not decoded. The same decider `0x1404790B0` also sets A0+0x130, which GF leaves 0 (documented `ponytail:` in `fixdemo_select.cpp`).

## 5. Left (numbered)

1. Port the procedural camera: translate the missing 155 members of the `0x141F7BFE0` closure (`closure.py --tag ... --spec-set core`, then `gen_translated.py`), hook it for a capture oracle (Main: a hook on `0x141F7BFE0` and the rig apply `0x14089D690`), model the camera object it writes into, then GF frames per step.
2. `selectByRate` consumer (which of `SL`/`SR`/`outH10` plays at a shared start tick).
3. Scorer's run: PES's ramp is faster and has no stops; the MOVESTEP entry after the kick (KickMotion/Trap lanes). The live poll covers one goal; more goals will arrive in `ct6/dpi_a.jsonl` (the two pollers, pids 3127659 and 3147704, are still running until their own 7200/14400 s limits).
4. A trailing scorer's goal plays nothing in GF (section 4): decode what PES does with pick 0.
5. A GF-vs-PES burst aligned to the frame (same step, same clock) once the camera is ported.
6. Scratch `gf-CutsceneTiming-scratch/ct7/` (movie deleted; frames and polls kept small), `ct6/`, `ct5/`.

**`GATE fast (interim, tools/dev/gate.sh not run; machine load 30-60): pes_goal_anchor_test 20/20 (PES21_ROOT + PES_EXE set), gen_translated.py --check not run (no layer file touched), no Godot or gfsim code changed (the temporary trace in match.cpp was reverted, git diff clean).`**
