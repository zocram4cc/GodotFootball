# CutsceneTiming: PES's FixDemo scene timeline decoded (step machine, step length and clock, end rule) with one captured goal's frame stamps; GF port, oracle test, translation and frame bursts NOT done (30-10-26)

Branch `CutsceneTiming` (from `origin/CutsceneSelect` `6220190c`, worktree `gf-CutsceneTiming`). Labels: CONFIRMED = decompile, capture read or tool output this session; INF = inference; OPEN = blocker named. PES-derived captures and dumps stay in scratch (`gf-CutsceneTiming-scratch/`), never committed. Request budget reached its 70% mark during decode: **the ticket's items 4 (GF scene timeline), 5 (oracle, frame bursts) and 6 (fast tier) are not delivered.** What exists: the decode below, three small tools, and a `CutRecord` extension (compiles).

## 0. Status against the ticket

| item | result |
|---|---|
| 1. worktree, push early | DONE. Pushed `CutsceneTiming` first thing; merged `origin/CutsceneSelect` (fast-forward to `6220190c`, brings LayerHeaders). |
| 2a. delay event -> scene start | **One goal measured** (§4): goal pick frame 15502, anchor write 15671, first manager update about 15678-15679 (INF, from the manager's own time accumulators), i.e. **about 176 frames (3.26 s at 54 fps) from the pick**. Mechanism decoded (§2, DemoInfo states 2-4 wait on async load cues); whether the delay is constant is OPEN (one goal only; poller armed, §6). |
| 2b. step durations and transitions | **Decoded** (§3): length = first Cut record (min/max, ticks), clock `t += (60/54)*speed` per update, step ends at `t >= length`, next step prefetched when the remaining time is under the step's fade frames, starts when ready. Measured on the captured goal: 654, 221, 442 frames for lengths 720, 240, 480 ticks (648, 216, 432 updates): **residual +6, +5, +10 frames OPEN** (§4). |
| 2c. layer sequencing | **Partly** (§5): a step's cameras are time-sequenced by `startCutTime` on the step clock; selection among same-time cameras by `selectByRate` is OPEN. |
| 2d. scene end | **Decoded** (§3): the step vector's terminator step -> step phase 7 -> manager `+0x1D4 = 4` -> DemoInfo state 0xB. Not "first layer ends". |
| 3. bounded closures, snapdiff | NOT DONE (see §6 item 3). `hook_plan.py` verdict for four hooks sent to Main (§6). |
| 4. GF scene timeline | NOT DONE. |
| 5. oracle, frame bursts | NOT DONE. No frame taken; no claim about pixels. |
| 6. fast tier | not run. Interim line: `GATE fast: not run (only a CutRecord field extension; fdc.cpp.o compiles; no test binary built: pestranslated is a 4,480-object build, ~3 h at load 35)`. |

## 1. Objects (CONFIRMED, snapshot reads of `pid483669/seq12256,12260,12268`, bridge decompiles)

- **DemoInfo** (the match-side event object): root `[0x1436F3F88]` (found by pointer scan of seq12251; also stored at the DemoInfo-holder). `+0x00` first scene id, `+0x08` kind (1 = goal), `+0x1D8` state (u32), `+0x1E0` the FixDemoMatch it drives, `+0x1D4` (byte) flag read by state 2.
- **FixDemoMatch** (vtable `0x142BA6BC0`): `+0x08..+0x18` request list (first u32 = scene id), `+0x20/+0x28` step vector (0x1E0-byte steps), `+0x38` side, `+0x54` go flag, `+0x55` end flag, `+0xE0` current FixDemoPlayer, `+0xE8` current step byte, `+0xF0` fps = **54.0** (read), `+0xF4` 57.0 (goal scenes), `+0xFC` seconds of the current step's play time, `+0x100` total seconds of all updates, `+0x128` the scene's Random1000, `+0x1D4` manager phase (0 idle, 1-2 loading, 3 started, 4 ended), `+0x1D8` step phase (1..7), `+0x21C` load flags (`& 0x1F == 0x1F` loaded), `+0x254` next-step wait counter.
- **Step node** (0x1E0 bytes): `+0` kind u32 (3/4 = play, 5 or 0 = terminator kinds seen), `+4` fade f32 (seconds), `+8` cutRef, `+0x10` placement mode, `+0x3C` slot mask, `+0x44` the variant's `f`, `+0x1D0` layer list head and `+0x1D8` its count (10 slots; 6 used in the captured step).
- **FixDemoPlayer** (vtable `0x142BA51B8`): `+0x58` state (0..6), `+0x60` clock (ticks), `+0x64` length (ticks; -1 until the cut starts), `+0x68` seconds per update, `+0x70` start time, `+0xAA8` speed, `+0xAB0` fps (54.0 read), `+0xADA`/`+0xAD8` flags.

## 2. Control flow (CONFIRMED by decompile, VAs)

- **DemoInfo state machine `0x14047C830`** (callers `0x140446390`, `0x14044A180`): state 1 starts the load cue `FUN_140a67960(cue[DemoInfo+0x1D0])`, state 2 runs the manager's update each frame (`vslot +0x28 = 0x141F59050`) and waits for `+0x75`; states 3-4 wait on the cue objects (table `0x1436F4ED0 + k*0x48`, status `0x140A67AA0`: 0 = ready, 1 = finished, 2 = pending; set by a loader thread, so the wait is **wall-time dependent**: OPEN whether PES adds a fixed minimum); **state 5 = scene anchor write `0x14047D280`** (`cut_anchor_write` hook); 6, 7 (one frame each); **state 8 sets the manager's go flag `+0x54 = 1`, calls its update once and starts the scene**; state 10 runs the manager each frame until `manager+0x1D4 == 4`; state 0xB ends (`0x14047C6A0`), 0xD/0xE are the skip paths.
- **Manager update `0x141F59050`** (vslot 5): phase 0 -> 1 (`0x141F5A920`) -> 2 (loading, `0x141F5B9E0`, waits `+0x21C` flags) -> 3 -> the step machine; then `+0xF8 = player+0x68`, `+0xFC += +0xF8` (only while the player's clock is non-zero), `+0x100 += +0xF8`.
- **Step machine `0x141F59380`**: 1/2/3 prepare players (`0x141F57690`, `0x141F5A760`, `0x141F5B810`), **4 waits for the go flag `+0x54` and starts step 0 (`0x141F5D420`, `0x141F5D2E0`, `0x141F5CD00`; phase 5)**, 5 queues the next step's player, **6 plays**: when the step is over (`0x141F572F0`) and the next is ready (`0x141F5B6E0`) it starts it (`0x141F5D2E0` + **`0x141F5CD00`** = the hooked scene start, phase 5 again; `+0x254 = 0`); when the next is not ready it adds 1.0 per frame (`[0x14256AE1C]`) up to 3600 (`[0x142598590]`) then ends the scene (`+0x55 = 1`); 7 tears down and returns 1 -> manager phase 4.
- **Step over `0x141F572F0`**: finished = `player+0x58 == 6`. Prefetch of the next step: when the player has a length (`+0x64 > 0`), speed `> 0` and nothing queued, `transition = vslot +0x10 (0x141F61290)(step)` frames, and `length - transition <= clock` calls `vslot +0x98 (0x141F5A0B0)(next kind, transition / speed)`.
- **Transition frames `0x141F61290`**: `fade * 60.0` (`[0x14259A764]`) for step kinds other than 3, 4, 5; **0 for kinds 3, 4, 5** (every captured goal step is kind 4); out-of-range step: `[0x142591140]` = 30.0.
- **Player update `0x141F2F6B0`**: state 5: finished (`state = 6`) when `speed > 0 && length > 0 && length <= clock && flag +0xADA == 0`; clock advance per update = `(60.0 / +0xAB0) * +0xAA8` ticks (`0x141F2F93B MOVSS XMM4,[0x14259A764]` = 60.0), clamped to the length; `+0x68` = advance / 60 seconds. With fps 54 and speed 1 one update = 1.1111 ticks = 1/54 s.
- **Step length, cut start `0x141F2DD80`** (decompile lines 195-199): from the player's smallest-id Cut record (record = map node - 0x30): `minTime` (+0x08), `maxTime` (+0x0C), `playSpeed` (+0x10 -> `+0xAA8`, rescales when it differs from `+0xAAC`). `length = maxTime` when `min <= 0` or `max <= 0` or the env flag `+0x522C` is set or there is no scene; otherwise `min + rand99 * (max - min) / 99.0` (`[0x1425C48CC]` = 99.0) with the raw draw `0x141F7A150(scene+0x128, 100)`. The Cut's function-number term (`0x141F31D80`, record `+0x38`) may override min/max (type-7 Function records, undecoded; all Cut records read here have functionNo 0).

## 3. Rules a GF timeline needs (derived from §2, CONFIRMED where stated)

1. Scene list: one FixDemoMatch holds all steps of the DemoInfo's scene list (captured goal: steps `0x30101` run, `0x300BF` celebration, `0x30136` set, `0x3011C` turn, then a terminator; picks read from the ctx map: rows 8, 2, 532, 7).
2. Step i plays `ceil(length / ((60/54) * speed))` updates (CONFIRMED against `+0xFC` = 3.26667 s = 196 ticks and 12.00005 s = 720 ticks).
3. The next step is prefetched `fade*60` ticks before the end (kinds 0/1/2 only) and starts as soon as the current finished and loaded.
4. The scene ends when the step vector is exhausted; then `EndScene` (category rotation) and the DemoInfo advances.
5. Layers of a step are all played at once: each camera record starts at its `startCutTime` on the step clock (§5).

## 4. The captured goal (`pid483669`, launch with the rvwatch22/23 hooks; frames = `MI+0x1198`, read by `tools/dev/cutscene/capture_frames.py`)

| capture | hook | frame | note |
|---|---|---|---|
| seq12229 | judge_whistle_50E770 call 4 | 15499 | play 1 (the whistle call before the stop) |
| seq12231 | goal_pick_140477400 | 15502 | play 2, restart 1, reason 3 (goal) |
| seq12251/12252 | cut_anchor_write / calc | 15671 | DemoInfo state 5 (read: state 5, manager `+0x54 = 0`...) |
| seq12256 | scene_start call 5 | 15856 | scene step 1 (cut `0x300BF`), manager `+0x100` = 3.2852 s, `+0xFC` = 3.26667 s |
| seq12260 | scene_start call 6 | 16510 | step 2 (`0x30136`), `+0x100` = 15.3037, `+0xFC` = 12.00005 |
| seq12263 | scene_start call 7 | 16731 | step 3 (`0x3011C`) |
| seq12268 | scene_start call 8 | 17173 | a second manager (vtable `0x142BA6BC0`, 2 steps, kinds 0, mode 1): the next scene |

Install lengths (Cut min = max, `fdc_parse.py` on the picked variants): `goal_celebrate_0218_base` 720 ticks, `goal_2019_ins_lostPlayers_240f_L` 240, `goal_2020_F_turn_headHug` 480, `goal_2018_run_30_banzai` 196. Install values, not pinned anywhere (they are the install's: a mod may differ).

- Step start to start: **654, 221, 442 frames** against 648, 216, 432 updates (= 720, 240, 480 ticks at 1.1111 ticks/update). **Residual +6, +5, +10 frames: OPEN.** The pair 240 -> 221 and 480 -> 442 is proportional (x1.0231), the 720 step is 9 frames short of that ratio. Not explained by the decode: candidates are loads between steps (`0x141F5B6E0` returns not-ready, `+0x254`), the player's state 4 -> 5 load flag (`+0xAD8`), and MI frames that are not 1:1 with manager updates. Per-frame polling of one goal and one foul (poller below) settles it.
- First step: the previous steps' `+0x100` = 3.2852 s = 177.4 updates (196 ticks = 176.4 updates + 1), so the first update was about frame 15856 - 177.4 = 15678.6: **7 frames after the anchor write, 176 frames after the pick** (INF, from the accumulator; the poller measures it directly).
- Each step adds one update to `+0x100` beyond its `+0xFC` (0.0185 s = 1/54 s: the update in which the clock reaches the length).
- `scene+0xF4` 57.0 and `+0xF0` 54.0 for goal scenes; 54.0/54.0 for the second manager. What `+0xF4` scales is OPEN (no effect on the clock in the data: 648 updates for 720 ticks).

## 5. Layers and cameras (CONFIRMED by `fdc_parse.py` on the picked variant, INF for the playback rule)

Variant for `0x300BF` row 2: `goal_celebrate_0218_base` (the Cut record, no camera), `goal_Effect_h` (effect only), `goal_celebrate_0218_mayaL0x` (cameras key 0 `camSL` start 0, key 1 `camSR` start 0, keys 21 and 22 `camSKY` start 480), `goal_cmnCam_noSwitch10` (key 10, start 9999 = never, no clip: procedural), `..._inH11` (key 11, start 250, procedural), `..._inM20` (key 20, start 480, procedural). Cameras carry `selectByRate` percentages (23 entries) and `targetKind/No`. INF: the camera in force is the last-started one on the step clock (0 -> 250 -> 480 ticks here), with same-time alternatives chosen by their rates; the clip cameras of GF's current `load_layers` (first layer's camera index 0) show only the first 480 ticks of a 720-tick step. OPEN: the active-camera rule (`0x141F41CE0` per-camera start, `0x141F42110` update) and the rate selection.

## 6. What is unfinished (numbered)

1. **GF scene timeline** (plan, nothing written): `pes::CutsceneTimeline` in `src/pes/` (pure, steps of `{cut id, length ticks, speed, fade frames}`; `Advance()` per sim tick; started, step index, step start frames, done), fed by `GfMatch::step` instead of `match.gd`'s wall-clock `_cut_t += delta`, one request per scene with every step and layer; delete `_poll_cutscene`'s "first playable layer, return" and the `_cut_t >= duration_s()` end. Constants from the install: 60.0 at `0x14259A764` (opcode-checked site `0x141F2F93B`), fps from `kPesFps` (54, the value `+0xF0`/`+0xAB0` hold), Cut min/max/speed from the new `CutRecord` fields. The start delay and the +5..10 residual need the live measurement and must be `ponytail:` stand-ins naming `0x14047C830` states 2-4 / `0x141F5B6E0` until decoded.
2. **Oracle (red first)**: for the captured goal: lengths 720/240/480 ticks give 648/216/432 updates; PES intervals 654/221/442; start 176 frames. Not written: cannot be green until item 4 (residual) is explained; the part that is decided (updates from lengths) can be pinned as a relation.
3. **Translation and proof**: nothing translated. Candidates on the path with a recorded entry: `0x141F5CD00` (scene start, hooked; its stage-default part already proven by `scene_start_oracle.py`), none of the clock/step functions has a hook. **Hooks requested from Main** (hook_plan verdict PASS, `real_branches_into_window = []`, windows 15/15/18 bytes; bytes in scratch `hp/pes_hooks.cfg`): `fixdemo_step_end_141F572F0` (`--when 'd(q(rcx+0xe0)+0x58)==6'`), `fixdemo_step_machine_141F59380` (`--when 'd(rcx+0x1d8)==5'`), `demoinfo_update_47C830` (`--when 'd(rcx+0x1d8) in (8,10,11)'`), cap 12, armed by goal_pick. Then `closure.py --members-file` + `snapdiff --bounded` on `0x141F572F0`, `0x141F61290`, `0x141F59380`.
4. **Residual +6/+5/+10 frames and the start delay**: poller `tools/dev/cutscene/live_timeline_poll.py PID OUT.jsonl` (read-only sudo pread of `/proc/<pid>/mem` through `live_srv.py`; root `[0x1436F3F88]` -> DemoInfo -> manager -> player; logs frame, play, restart kind/reason, DemoInfo state, manager phases, flags, step, player state/clock/length on every change and every frame while a manager exists). It was started on PES pid 1572516 (menu at the time: no match, no goal) writing to scratch `live_a.jsonl`; PES was between launches during most of the session, so **no second goal or any foul was measured**.
5. **Foul**: no scene-start capture of a foul exists (only `foul_sanction` and `judge_whistle`); the foul scene's timeline is unmeasured.
6. **Cameras**: the active-camera rule and `selectByRate` (§5); the foul camera and `goal_cmnCam_*` are procedural (CutsceneCam's open item).
7. **Frame bursts**: none (PES goal burst vs a Godot movie of a goal): not taken; the build cost (§0 item 6) made a Godot movie of this branch unavailable. Not like-for-like claims: none made.

## 7. Files

Committed: `src/pes/fdc.{hpp,cpp}` (`CutRecord` gains `minTime`, `maxTime`, `playSpeed`, `functionNo`; `fdc.cpp.o` compiles), `tools/dev/cutscene/live_timeline_poll.py`, `live_srv.py`, `capture_frames.py`, `capture_sm.py` (all read-only on PES; `capture_*` need `PES_EXE`). Scratch (not committed): `gf-CutsceneTiming-scratch/` (bridge decompiles `d_*.txt`, `scene_map.json`, `hp/`, `live_a.jsonl`).

Tests run: none (nothing buildable in budget). `gen_translated.py --check`: not run (no layer file changed). GATE fast: not run, see §0.
