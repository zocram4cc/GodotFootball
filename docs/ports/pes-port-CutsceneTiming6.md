# CutsceneTiming6: GF's goal now plays PES's scene list (the builder 0x140477BC0 runs on GF's arena, bit-exact on goal A); the scorer's slow run is a standing start, not a cap; the PES driver of his run is still not decoded (31-10-26)

Branch `CutsceneTiming` (worktree `gf-CutsceneTiming`). Pushed heads (`git ls-remote origin CutsceneTiming`): `46662841` (merge of `origin/main`), `22dfd09f` (builder feed + red test), `a9d1d18b` (GF cut-over). Labels: CONFIRMED = decompile, capture read, test, run or frame this session; INF = inference; OPEN = blocker named. PES-derived material (captures, polls, frames) stays in `gf-CutsceneTiming-scratch/ct6/`, never committed.

## 0. Status against the ticket

| item | result |
|---|---|
| 1. merge `origin/main`, push | DONE (`46662841`). |
| 2. scorer speed: where GF caps it, PES's rule, gate by distance/facing | **NOT DONE; cause found.** There is no cap (section 1): GF's scorer is *standing* when the goal registers and walks the MOVESTEP ramp from rest, where PES's scorer is already running at 24-27 km/h and keeps it. The PES driver of the run is not decoded: `live_dpi_poll.py` ran on pid 3061186 from 08:46 and caught **no goal** (log empty). The gate in GF still opens on the timer (124 frames after the request). |
| 3. feed the builder, play its list, red test | **DONE for goal A** (section 2), cut over in GF (section 3). The 07:00 goal's list could not be the oracle: its builder call is not captured (section 2). |
| 4. frames | GF looked at (section 4). The PES burst was not taken: no goal happened on the live PES while I was on it. NOT like-for-like. |
| 5. fast tier | interim line at the end. |

## 1. Scorer speed (CONFIRMED by a temporary per-6-tick print, removed; headless `gfsim_run --pes-loop`, seed 7, the first goal)

- `_AddCelebration` asks for `sprintVelocity` (8 m/s) the whole way; nothing between the command and the man caps it.
- The man is in the PES MOVESTEP FSM (pes-loop): at the goal he plays `idle_0_3_run_000_v3_mirror`, the *accelerate-from-rest* clip (state ACCELERATE 9, speed class 2), then RUN_LOOP 3. The locomotion `rate` (`MoveStepRecord::rate`, req[0x4a]) is reset to 0.014 on entering the loop and rises about 0.08 per 111 ms, to the ceiling 0.788 at 3.7 s: speed class 4 (dash) from 3.5 s. Two stops (`have 0.00`, STOP 8) at 0.4 s and 0.9-1.4 s on the way.
- So the run reaches full speed after about 3.7 s, the gate's timer (`fps * 4`, `0x14047A710`) ends at 4 s, and in the Godot run of CutsceneTiming5 the man was at 2.7 m/s when it did: the ramp, not a limit.
- PES's scorer at the same moment (earlier poll, `live_f.jsonl`, goal 1 rows): already running at the goal frame, anime speed 23-27 km/h (6.5-7.5 m/s), the same motion until the gate; he never starts from rest. The difference is the state the man arrives in: GF's shooter stops in his shot clip (scorer 1.8 m/s at the goal frame, `have 3.5` quantised), PES's shooter keeps his momentum. Whether PES's own accelerate ramp is as slow from rest is not measured.
- OPEN (needs a PES goal): which PES word orders the scorer's run in DemoInfo state 2 and at what speed (`DemoPlayerInfo +0x18/+0x24/+0x6C`, man `+0x5B0`): the poller is armed (`tools/dev/cutscene/live_dpi_poll.py 3061186 OUT.jsonl`, two instances running, pids 3127659 and 3147704, killed by their own 7200 s / 14400 s limits).
- Not changed in code: the cause is the shooter's momentum through the shot clip (the kick clip's root motion and the MOVESTEP entry after it), which is the shot/animation port, not the celebration. `_AddCelebration` is as CutsceneTiming5 left it.

## 2. The scene-list builder on GF's arena (CONFIRMED, test)

`FixDemoSelector::PlaceGoal` has a builder mode: `GoalAnchorFacts::build` set runs the translated `0x140477BC0(A0, &list, event)` (the pick `0x140475A90` and the list around it, 4461-function layer, already proven by snapdiff) on the arena, then the radius `0x140474CF0` over *its* list, the spot `0x140476C50`, the anchor and the flip as before. `GoalAnchor` now returns `sceneId` (A0+8) and `scenes` (the list). With `build` unset the given `sceneId` and `cuts` are used (the tests' kind-3 and slim captures, whose list was recorded).

What the builder read over goal A (1139 reads, `rw_trace.py` over `pid2621613/seq1532`, classified by structure) and where GF now gets each cell:

| PES cell | what | GF source |
|---|---|---|
| A0+0 | goal level | `kGoalLevel = 2` (`ponytail:` 0x140477570, all five captured goals held 2) |
| A0+0x13C | previous pick | `GfMatch::previousGoalPick_` (persists goal to goal) |
| A0+0x108/+0x114 | generator | `GoalAnchorFacts::rngState` (GF draws it from the goal generator each goal; `ponytail:` 0x14047DF00's clock seed) |
| G+0x180 slot table, G+0x2AD0 setup | designation 0xFF, byte 0 | zeroed stand-ins (`ponytail:`) |
| event+0xE0 history (0x140A3E6D0) | newest record: side (+0x14C, +0x168), squad key (+0x150); counters | the goal: side, place; team counters = GF's score |
| team counters, clock | **protected** u32s (nonce + FNV-1a hash of the decoded bytes, checked by `0x1408C9190` / `0x140A3C070`, which abort on a mismatch) | `WriteProtected` writes them (nonce 0) |
| MI+0x1168/0x116C/0x1574/0x15C0/0x162C, presence bytes | period, stopped, goal event, side, assist pair, 22 present | match phase, constants, `assistSlot` (none = the invalid pair), `true` |
| men +0x55C/+0x564 | the keepers' and the assist's positions (the closing-scene keeper test `0x14047A850`) | `build.men`, GF's 22 men |
| [0x1434A2D40] stage interface, byte [0x143446058] | `0x140477400` calls vtable +0x88 (translated, answers false) and reads the byte (0 in the file, 1 live) | vtable read from the install; the byte is 1 (`ponytail:`, writer not decoded) |
| DB+0x428/+0x430 | the goal table's **edit-scene id list** (u32 array, count) | `goalEditScenes` from the decoded table |
| scorer's player record (squad +0xE0 + key*0x128, u64 at +0x10, bytes 2-3) | his two edit-mode celebration slots | `build.celebration`, **{0, 0} in GF** (`ponytail:`: not in GF's player data) |

Findings worth keeping:
- **Goal A's pick 0x30034 is the scorer's own registered celebration** (slots 53 and 98 index the edit-scene list: slot 53 = 0x30034, slot 98 = 0x30061; `0x140471280` draws between them and the generic pool). A GF scorer has none, so GF takes PES's generic branch.
- The list is `{pick, closing 0x30130 + A0+0x20 - 1, 0x3011C}`; A0+0x20 = 4/6/8/10 minus the keeper-off-the-line bit, by the minute and the score (`0x14047A850`).
- The builder's draws: entry state is the snapshot's A0+0x114 rewound **three** draws (two in the builder, one in the spot), not two: CutAnchor6's K=2 replayed the right list by luck. GF's goal A entry state is 179127131; the oracle's `rngPre` 459971373 is the builder's exit state.
- The clock: MI+0x1178 protected float 60590.0 plus MI+0x1174 = 616.0 on goal A (0x140A6BD80 splits the sum into minutes by 60); GF puts its clock in seconds in the protected one.

Test `GoalScenes.BuilderListsGoalAsScenesOnTheCapturedInputs` (`tests/pes_goal_anchor_test.cpp`): goal A's inputs pinned from the snapshot by script, builder mode: scene `0x30034`, list `{0x30034, 0x30134, 0x3011C}`, radius and spot bit-equal to PES's. Red on `origin/main` (no builder mode); it also failed through each missing cell on the way (abort in `0x1408C9190`, `0x140A3C070`, `0x140477400`'s dispatch, then a wrong pick, a wrong closing scene, a wrong spot z).
Mutations (each run, rebuilt, reverted): `kGoalLevel` 2 to 0 KILLED; scorer's celebration slot 1 zeroed KILLED. **Survived: team counters written as 0** (goal A's list does not depend on the score: the score-sensitive branch of `0x140A80D10` (`iVar8 == 0` with the setup's tie-break flag) is not reached; no test covers it).
The 07:00 goal's list (`0x3006C, 0x30136, 0x3011C`) could not be the oracle: `pid2122415` holds its DemoInfo (`demoinfo_update`) but not the builder's own call or its entry generator state; goal A is the only goal with the builder's call captured full. `0x30136` is a closing scene with A0+0x20 = 7.

## 3. GF plays the list (CONFIRMED by a build and a Godot run)

- `GfMatch::PollCutscenes`: `BeginEvent`, then `PlaceGoalScene(ball)` (builder mode, the facts above), then `Request` of **each** scene of `anchor.scenes` concatenated into one request (each scene is one FixDemoMatch step in the captures). Deleted in the same change: `PickGoalScene` and `pesgoalscene.{cpp,hpp}`, its test and CMake target, `kGoalRule`, `goalTable_`, `f.cuts = {sceneId}`.
- Godot run (seed 7, teams 1 v 2, `--pes-loop`, 1152x648, RADV): the goal at frame 2592 requests `0x300bd`, `0x30132`, `0x3011c` (the pick, the lost-keeper scene, the lead); steps start at 2716, 2959, 3082 and the list ends at 3267: **551 frames (18.4 s)**, against one step ending 85 frames after it started (2.8 s) before.
- The gate still opens 124 frames after the request: the timer (`A0+0x14`, fps x 4), because of section 1. So **"GF's gate opens by PES's distance/facing rule" is not met**.

## 4. Frames (looked at)

Looked at: GF movie of the Godot run above (`ct6/g1/sheet6.png`, 1152x648, RADV; process frames 2600 before the gate, 2720 and 2800 in step 0, 2965 in step 1, 3090 in step 2, 3200 in step 2's tail; all six viewed). **What they show: the same wide aerial broadcast shot of the goal end in all six** (a clump of white and green men in the box at the right-hand corner, the red stand behind, no close-up, no cut between steps). So the 3-step list and the 18.4 s are in the log, but **no frame shows a changed camera or the scorer in shot**: the cutscene camera is not visibly applied in this run, in step 0 or after (CutsceneTiming5 section 5's frames showed a low shot of a man in front of the bench, so the camera behaviour differs between the two runs: OPEN, not investigated, no budget left). I make no visual claim that the multi-scene cut plays correctly. Not like-for-like with PES: no PES burst of a goal was taken (no goal on the live PES in the session).

## 5. Left (numbered)

1. A PES goal on the live pid 3061186 with `live_dpi_poll.py` running (both instances armed) to read the scorer's DemoPlayerInfo words and speed; then the writer among the CutsceneTiming5 section 3 callers; then the scorer's run on PES's rule, so the gate's radius/facing test passes before the timer.
2. The shooter's momentum through the shot clip (why GF's scorer is at 1.8 m/s when the goal registers).
3. A test for the score branch of `0x140A80D10` (the mutation that survived) and for the period and assist inputs; the other captured goals' builder calls need full `cut_anchor_calc` captures (CutAnchor6 item 3).
4. The scorer's celebration slots from GF's player data; the assist; `0x140477570`'s level; the generator's clock seed.
5. A PES burst of one goal against the GF movie of the same shots.
6. Scratch `gf-CutsceneTiming-scratch/ct6/` (movie 570 MB: delete after viewing).

**`GATE fast (interim, tools/dev/gate.sh not run; machine load high): pes_goal_anchor_test 19/19, pes_fixdemo_select_test 6/6, pes_cutscene_timeline_test 7 passed + 1 skipped (no GF_CUT_EPISODES), godotfootball and gfsim_run build, one Godot goal run plays the 3-step list; gen_translated.py --check not run (no layer file touched).`**
