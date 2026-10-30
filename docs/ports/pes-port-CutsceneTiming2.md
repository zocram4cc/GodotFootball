# CutsceneTiming2: GF's cutscene timeline (start, steps, cameras, end) from PES's FixDemo machine; four captured goals reproduced row for row by an oracle; the +6/+5/+10 residual explained as the transition object's refusals; frame bursts and the fast gate NOT delivered (30-10-26)

Branch `CutsceneTiming` (worktree `gf-CutsceneTiming`), merged `origin/CutsceneSelect` (3bb1e6bd, LayerHeaders) and `origin/main`; pushed heads confirmed by `git ls-remote origin CutsceneTiming`: `df35bc92` (merges), `d944713f` (poller), `e166afe1` (timeline + tests), `66ef3870` (GF playback). Labels: CONFIRMED = decompile, read, or test output this session; INF = inference; OPEN = blocker named. Captures and logs are PES-derived: scratch only (`gf-CutsceneTiming-scratch/`), never committed. Machine load 28-35 the whole session; the `godotfootball` build (5665 steps) was at 2101 when I stopped.

## 0. Status against the ticket

| item | result |
|---|---|
| 1. merge CutsceneSelect + main, push | DONE. |
| 2. GF scene timeline, delete "first layer, end with its clip" | **DONE** (§4): `pes::CutsceneTimeline` (`src/pes/cutscene_timeline.*`), fed one Tick per logic frame by `GfMatch::step`; `match.gd` plays every layer's cameras of the step in force on the step's clock; the scene ends when the step list runs out (`EndScene` from C++). `duration_s`, `load_layers`, `cutscene_finished` and the wall-clock `_cut_t` end are deleted. Start delay: decoded as far as game-tick driven (§2); the rest named `ponytail:` (§5). |
| 3. residual +6/+5/+10 | **EXPLAINED** (§3), modelled as inputs, not derived. |
| 4. oracle, red first | **DONE for the timeline** (§6): 4 captured goals reproduce every poll row. **Frame bursts: NOT taken** (no Godot build finished); no pixel claim made. |
| 5. fast tier | **NOT run**: `GATE fast: not run (interim: pes_cutscene_timeline_test 8/8 with the 4-goal oracle; gf_match.cpp, pes_cutscene_camera.cpp, fdc.cpp objects compile; pestranslated build 2101/5665 at load 30; tools/dev/gate.sh and the pes_* binaries that link fdc.cpp not run)`. |

## 1. Live data (CONFIRMED)

Poller `tools/dev/cutscene/live_timeline_poll.py` on PES pid 1572516 (No Mod), now also logging the frame counter re-read after the objects (`f1`), manager `+0x254`, the player's start/flags and DemoInfo `+0x8F2/+0x900/+0x868`. `live_a.jsonl` (the predecessor's poller, killed) held **three goals**; `live_b.jsonl` caught a **fourth with clean rows** (`f == f1`). Both pollers (1578341, 1847124, 1861723) are killed; `live_c` (the DemoInfo sub-state fields) caught no goal before I stopped.

| goal | scene | steps (length ticks, start) | DemoInfo created -> state 4 (gate) |
|---|---|---|---|
| g1 `live_a` | 0x300A9 then 0x3011F (rebuilt at frame 6705) | 330 | ~232 |
| g2 `live_a` | 0x300C0 | 359, 240 | 213 |
| g3 `live_a` | 0x3013D | 150, 178 (start 15), 480 (start 90) | 147 |
| g4 `live_b` | 0x30034 | 184, 180, 480 | 215 |

The staged hooks (`fixdemo_step_end_141F572F0` etc.) are not live (PES still the 16:17 launch, 53 hooks); the watcher also released captures "disk under 400 GiB". No capture used.

## 2. Start: what is game-tick driven (CONFIRMED by decompile unless marked)

- DemoInfo (the object at `[0x1436F3F88]`) exists **2 frames after the pick frame** on all four goals (first row with a scene id). INF (creator not decoded).
- State 2 waits for the byte `+0x1D4`. **`0x140447290` sets it** (`0x140447667` region): a sub-machine on `+0x8F2` counts frames in `+0x900` via `0x1404534F0(this, seconds)` = `counter++ ; fps * seconds < counter`. Sub-state 1 waits `fps * [0x142565604]` = **2.0 s = 108 frames**; sub-state 2 waits `fps * (+0x868 * 0.5)` where `+0x868 = FUN_14044E460(...)` (a goal analysis over the players, inputs not decoded; the decompile calls it `void`, the caller reads `eax`). Observed totals 147..232 frames: **game-tick driven (a frame counter), not an asset wait**, but its second term is OPEN. `live_c` logs `+0x8F2/+0x900/+0x868` for the next goal.
- States 4 -> 5 -> 6 -> 7 -> 8 -> 10 are one frame each (`0x14047C830` cases): **state 5 is the anchor write** `0x14047D280`; state 8 sets the manager's go flag `+0x54`.
- The cue (`FUN_140A67AA0` status, sound) gates states 3/4/0xB; it did not decide any logged wait (rows show state 4 immediately after `+0x1D4`).

## 3. The residual +6/+5/+10 frames (EXPLAINED, CONFIRMED on the clean goal)

The step clock is exact: a step of length L ticks and start s plays `1 + ceil((L - s) * 54 / 60)` updates and the player reads finished one update later; the manager's total seconds `+0x100` equals `(L + 60/54)/60` for s = 0 (g4 step 0: 3.0852 s) and `+0.25 s` for the start jump of 15 ticks (g3). **The extra frames are between steps**: after the player reads finished (`0x141F2F6B0` state 6), the manager (`0x141F59380` case 6) asks `0x141F5B6E0` whether the next step is ready; that asks `vslot 0x98 = 0x141F5A0B0`, which queues a screen-transition effect object (`FUN_141F15FC0`, slots at `[0x1437F5CA8]`, vtables `0x142B9E758/0x142B9EB50/0x142B9E938`, render passes) and returns `FUN_141F15A50` (finished?). Each refusal adds 1.0 to manager `+0x254` (`0x141F59380`: `+= [0x14256AE1C]` = 1.0). **Measured on g4: `+0x254 == 4.0` at both step switches**, switch frame = clock's last update + 6 (state 6 on the next frame, then 4 refusals). g3's switches fit 4 (and 5 within its poll slack), g2's fits **6** (a 0.7 s wall hitch froze the frame counter at the switch: render-dependent). The same machine gives 1 refusal before step 0 (go frame) and 3 before the end teardown. The +6/+5/+10 of the first goal are these waits plus the poll's phase; they are render-pass counts, not part of the step clock. The `+0x1F0` timer (`0.05 + 0.0333`, `0x1425A8A28`, `0x1429E81D8`) is read only on the env-flag path (`+0x522C != 0`), not on the normal one.
INF: `+0xADE` (player clock advances) is set after the first update (writer outside the decoded closure; the clock is 0 after update 1, then +1.1111/update).

## 4. GF's timeline (what was written)

- `src/pes/cutscene_timeline.{hpp,cpp}` (pure): DemoInfo states 2,4,5,6,7,8,10,0xB,0; manager phases 4,5,6,7; player clock exactly as `0x141F2F6B0` (`(60/fps)*speed`, start jump, clamp, finished test before the advance). Constants: 60.0 (`0x141F2F93B MOVSS XMM4,[0x14259A764]`), 99.0 (`0x141F2E0C4 DIVSS XMM0,[0x1425C48CC]`, new `PoseConstants::ninetyNine`, opcode-checked), fps `kPesFps` 54.
- Step length/start/speed from the first Cut record of the layers (`FirstCut`, `StepFromCut`; `CutRecord::startTime` = record +0x04 = node +0x34, `0x141F2DD80` lines 184-185).
- `GfMatch::cutscene_state()`: `{scene, step, step_started, clock_prev_s, clock_s, ended}`. `PesCutsceneCamera::load_step` takes **every camera record of every layer**; `update` plays the record in force = the latest `startCutTime <= clock` (INF), false for a procedural record in force (broadcast camera stays, `ponytail:` 0x141F7BFE0).

## 5. `ponytail:` stand-ins (each names the PES site that removes it)

1. Gate = `fps * 2.0` frames (the decoded part of `0x140447290`); PES waited 147..232. Removed by translating `0x140447290` + `0x14044E460`. **GF's cutscenes therefore still start about 1.5-2 s earlier than PES's on a real goal.**
2. Transition refusals 1 / 4 / 3 and end-cue wait 2 frames: the transition object (`0x141F15FC0`, vslot `0x28` at `0x141F17B70..`) and cue objects (`0x140A67AA0`). Measured, not derived; `TimelineStep::notReadyBefore` lets a replay override them.
3. DemoInfo creation 2 frames after the pick (creator `0x14047DF00` not decoded).
4. Length draw for a Cut with min < max (`0x141F7A150` on the scene's Random1000, not exposed by `FixDemoSelector`): minimum used; all captured Cuts have min == max.
5. The scene list: GF still requests `{sceneId}`; PES's manager played 2-3 steps on g2/g3 (the list builder `0x140477BC0`, recorded by CutAnchor6, is not fed). The timeline takes whatever steps the request has.
6. Step node `+0x1C8/+0x1CC` (start/length overrides, writer not decoded): not applied.

## 6. Oracle and tests (red first)

`tests/pes_cutscene_timeline_test.cpp` (its own light target, compiles `cutscene_timeline.cpp` directly): red on `origin/main` (no header). Relations (synthetic, install-free): first step starts 6 ticks after the gate, anchor write at gate+2; finished = `UpdatesForLength + 1` after the first update; start jump = start/60 s in the first update and shortens the step; next step 5 ticks after finished; end 8 ticks after the last finished; absorbing done state (found a real bug: `ended` stayed set after done); `CutLength`. **`CapturedGoalsReplay`** (`GF_CUT_EPISODES` from `tools/dev/cutscene/timeline_episodes.py`; skips when unset): for each logged goal, some gate (and refusals 4..8) reproduces **every row** (demo state, manager phase, step, player state, clock, total seconds) within the poll phase: g1 gate 232 (448/448 rows), g2 gate 213 refusals 6 (468/468), g3 gate 147 refusals 5,4 (451/451), g4 gate 215 refusals 4,4 (470/470). Rows are matched to the model state at their frame or the frames before it (clean rows: between the two counter readings; old rows: 4 frames of slack, the old poller could be starved between reads) or the mid-frame views of a step switch and of the total accumulation. The gate is a free input: **the oracle does not check the start delay** (§2, OPEN), it checks everything after DemoInfo state 4.
`pes_cutscene_timeline_test`: 8/8 with the episode file (7 + skip without).
**Mutations** (each on a scratch copy, built, run, discarded): transition refusals 4->3 (killed: next-step relation), first step 1->0 (killed: oracle + first-step relation), last step 3->4 (killed), end cue 2->3 (killed), finished test `<` for `<=` (killed, 6 tests), clock moves in the first update (killed), start jump ignored (killed), anchor write one frame late (killed), done-state flags not cleared (killed), seconds not accumulated (killed). 10/10.

## 7. Frames and gate: NOT done

No Godot frame of a GF goal and no PES burst were taken: the `godotfootball` target needs the whole translated layer (build 2101/5665 at load 30; the PES goal burst would also need a live goal). **Not visually verified**: camera selection among same-time cameras, procedural cameras, and anything about pixels. `pes_fdc_test`, `pes_fixdemo_select_test`, `pes_goal_anchor_test` (link `fdc.cpp`) and the gate were not run; `gen_translated.py --check` not run (no layer file changed).

## 8. Left (numbered)

1. Frame burst: finish the `godotfootball` build (running as `gf-CutsceneTiming-scratch/b3.pid`), run a GF match to a goal with `--cutscene` logging (`frame N cutscene scene ... step k starts`), take a PES burst of a live goal; compare like-for-like.
2. Gate: decode `0x14044E460` and `+0x868`; `live_c` (restart the poller with `live_timeline_poll.py PID OUT.jsonl`) logs `+0x8F2/+0x900/+0x868` on the next goal.
3. Feed the scene list (`0x140477BC0`) so GF's step list equals PES's (CutAnchor6 item 1).
4. Foul scenes: no timeline was measured (same machine, INF).
5. Translate the transition objects to remove the measured refusal counts; add the hooks (`fixdemo_step_end`, `fixdemo_step_machine`, `demoinfo_update_47C830`) once PES restarts and the disk floor allows captures.
6. Camera: same-time selection `selectByRate`, procedural cameras, `0x141F42110` update.
7. Gate and `pes_*` binaries on a finished build.
