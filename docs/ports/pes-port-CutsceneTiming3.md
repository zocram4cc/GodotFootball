# CutsceneTiming3: a goal cutscene's start gate is PES's goal-run-object timer (4 s of frames, `0x14047A710`) or an early opening (scorer within radius and facing the spot, `0x140472640`), NOT the kind-7 `0x140447290`/`0x14044E460` wait GF copied (108 frames); GF now uses the timer; early opening and GF frames not done (30-10-26)

Branch `CutsceneTiming` (worktree `gf-CutsceneTiming`). Pushed heads (`git ls-remote origin CutsceneTiming`): `a2f48946` (merge of `origin/main`), `cf1db4ec` (poller), `980ff429` (`goal_gate_end.py`), `3bd9268f` (the gate change + test). Labels: CONFIRMED = decompile (bridge), image read, live read or capture read this session; INF = inference; OPEN = blocker named. Captures, live logs and frames are PES-derived: scratch only (`gf-CutsceneTiming-scratch/`), never committed. Machine load 35-40; PES ran at 1-4 frames per second all session.

## 0. Status against the ticket

| item | result |
|---|---|
| 1. merge `origin/main`, push | DONE (`a2f48946`). |
| 2. decode `0x14044E460` / `+0x868`; translate if on the path; delete the 108 stand-in; gates reproduce | **`0x14044E460` decoded and is NOT on a goal's path** (section 1): the 108-frame stand-in mirrored the wrong step handler (kind 7). **108 deleted for goals**: GF's goal gate is now PES's own run-object timer (section 2), `GoalRunTimerTicks`, red test + 4/4 mutations; the oracle reproduces the logged g4 with that timer (216 -> gate 215). **NOT done:** the early opening (scorer within radius and facing the spot), which PES used on 10 of 13 captured goals and on the live goal below; GF therefore opens *later* than PES whenever PES opened early (g2 213 vs GF 215, g3 147 vs 215, live goal 174 vs 215) and exactly with PES when the timer decided (g4 215). g1 (232) is a scene rebuilt mid-DemoInfo (two scenes), also OPEN. |
| 3. frames | **PES burst taken and examined** (section 5). **GF frame NOT taken**: `godotfootball` was at 1140/3391 steps (load 38) when I stopped. So the start delay is not compared like-for-like; the PES side is measured, the GF side is the model's number only. |
| 4. scene list from `0x140477BC0` | NOT started (budget). Blocker as CutAnchor6: 27 functions of closure untranslated. |
| 5. fast tier | `GATE fast: not run (interim: pes_cutscene_timeline_test 9 tests, 8 pass + the oracle skips without GF_CUT_EPISODES; with episodes from live_a/live_b the oracle reproduces all four goals 448/448, 468/468, 451/451, 470/470 rows (gates 232, 213, 147, 215); gf_match.cpp syntax-checked (-fsyntax-only rc 0); the full build, tools/dev/gate.sh and the pes_* binaries linking fdc.cpp not run: machine load 38)`. |

## 1. `0x14044E460` and `+0x868` (CONFIRMED)

- Returns a **float in XMM0** (`0x14044E56B MOVSS XMM0,[RSP+RBP*4+0x20]`, `0x14044E58B..0x14044E592 MOVD/CVTDQ2PS`, `0x14044E594 MOVSS XMM0,[0x14259222C]`); the bridge calls it `void`, callers store `eax`, which is why the predecessor could not read it. `float f(DemoInfo*, man*, int idx /*R8D*/)`.
- Body: stack table, `[0..3]` from `0x14259CE60` = {8.0, 5.0, 3.0, 0.0}, `[4..7]` from `0x14259CE50` = {7.0, 2.5, 0.0, 0.0}, `[8]` = 0.0 (script-read from the image). `idx == 9` -> 0.0; clock `*[G+0x1A8] < 0x16` with `[FUN_140A489F0(G)+0x1400] != 0` -> 0.0; else the first man with `FUN_140A73E40(.., 1) == 0x14` and `[FUN_140A73E20(..)+8] < 3` returns `(float)[FUN_140A48910(G,slot)+0x24]` (u16) if record byte `+1 == 0x4F`, else `[0x14259222C]` = 5.0; no hit -> `table[idx]`.
- **Live:** DemoInfo `+0x868` = 8.0 on the goal scene `0x30034` (idx 0, caller `0x140446B20`) and 5.0 on a foul scene (idx 1, caller `0x140446960`), then 0 next frame.
- Callers by step kind (dispatcher `0x14044C880` over the list `DemoInfo+0x878[+0x8F0]`, kind handlers `0x140446390`): 3 `0x140447230`, 4 `0x140446B20`, 5 `0x140446960`, 6 `0x140446520`, 7 `0x140447290`, 8 `0x140446F80`, 9 `0x1404466F0`, 10 `0x140446DD0`, 0xB `0x140447690`. Goals run kind 4 (list `{3,4,69,29}` on `0x30034`, `sel` 1); a foul `0x30019` ran `{3,5,69,17}`. `+0x868` gates only kind 7 (`0x140447290`: sub-state 1 waits fps x `[0x142565604]` = 2.0 = 108 frames, sub-state 2 fps x (idx 4 -> 7.0) x `[0x142565600]` = 0.5 = 189: 297 frames in all) and `0x14047C2D0` when the manager is absent. The live DemoInfo's `+0x900` counter stayed 0 through a goal and a foul scene. **So the previous rule (and GF's 108) described a handler goals do not run.**

## 2. The goal gate (CONFIRMED)

DemoInfo state 2 waits for `+0x1D4`. Kind 4 sub-state 2 runs `0x140472640(A0, ctx, DemoInfo)` then `0x14047B460` every frame; `0x14047B460` stores `A0+0xEC` into `DemoInfo+0x1D4` (`0x14047B50D movzx eax,[r15+0xEC]`, `0x14047B515 mov [rcx+0x1D4],al`, rcx = the DemoInfo stack argument). A0 = the goal run object at `DemoInfo+0x1E8`.

- **Timer** `A0+0x14` is set at creation by `0x14047A710` (called from the goal builder `0x140478190` at `0x14047881F`): `(int)((float)(fps*k) / [[G+0x2C0]+0x5C])`, k = 10 for scenes `0x3013D..0x3013F` (`0x14047A720 SUB EAX,0x3013D / CMP EAX,2 / JA`), k = 4 otherwise (`0x14047A7F7 SHL EAX,2`, taken when `A0+0x130 == 0`; it is 0 on all 13 captured goals), and `A0+0x18` = fps x 30 / scale (`0x14047A822 IMUL EAX,EAX,0x1E`). Live (goal `0x30140`): `A0+0x14` = **216** and `A0+0x18` = **1620** at the creation frame, scale 1.0, fps 54 (live reads). It decrements once per frame (`0x140472670`).
- **Ready (`A0+0xEC`)** in `0x140472640` for an ordinary goal (BC 8, `+0xC8` 9, `+0xED` 0, `+0x131` 0): `A0+0x14 == 0 && +0x131 == 0` (`0x140472989`) **or** `dist(scorer, T=A0+0x30) < A0+0xC - A0+0x10` and then a tail (`0x1404729xx`): `|wrap180(scorer facing +0x554 - bearing(scorer -> T))| > 30.0` (`[0x142591140]`; wrap with 180.0 `[0x142598580]`, 360.0 `[0x14259858C]`) clears it again, with further DB-dependent clears (`0x140A7C800` kinds 9/10, `[man+0x54] <= [0x1425A1FFC]` = 13.5). The special scenes `0x3013D..F` use threshold `[0x14259BDE8]` = 3.5 m and skip the angle clear. Latched once set; `MI+0x135B` sets it at once.
- **Checked at the frame the gate opened** (full snapshots `cut_anchor_write_47D280`, DemoInfo state 5, 13 goals over 4 launches, `tools/dev/cutscene/goal_gate_end.py`): every one is explained by "distance < radius" (10) or "timer 0" (3: `2621613/seq1531` `0x30034`, `91171/seq1114`, `91171/seq2938`). The distance rows sit just under the radius (21.32/21.733, 22.99/23.289, 22.89/23.213, 18.53/18.854): the first frame inside.
- **The distance alone is not sufficient.** Live goal `0x30140` (PES pid 1572516, per-frame A0 log `live_e.jsonl`): the scorer (slot 7) was **8.44 m from T at creation and 4.4-8.4 m throughout, radius 30.2, yet `+0xEC` rose only at frame +174** (timer then 42 = 216 - 174; DemoInfo state 5 at +174, `b75` = 1): the facing/animation clears kept it shut. This is why GF cannot take the distance rule alone (it would open on tick 1) and why the facing tail is the remaining work. `+0x131` was 0 and `A0+0xED` 0 all along.
- g4 (`0x30034`, radius 13.546): `A0+0x14` = 0 at the gate on the capture of the same scene, and the log's gate is 215 = timer 216 - 1 (the poll's first row is one frame after the DemoInfo exists): the timer decided it, so GF = PES there.

## 3. GF's change (`3bd9268f`)

- `pes::GoalRunTimerTicks(scene, fps, scale)` (`src/pes/cutscene_timeline.*`), `gf_match.cpp` `StartTimeline`: `gateTicks = GoalRunTimerTicks(scene, fps, 1.0) - 1` for goals (category 3); a foul keeps `fps x 2.0` with a rewritten `ponytail:` (its gate `0x14047EB00`: countdown fps*3 may open only once down to fps, i.e. earliest 108 of 162 frames; foul port OPEN).
- Test `GoalRunTimer.FourSecondsOfFramesDividedByTheTimeScaleTenForTheSpecialScenes` (red on `origin/main`: `GoalRunTimerTicks` did not exist; relations: 4 x fps at scale 1, 288 at scale 0.75, 240 at fps 60, 10 x fps for `0x3013D/E/F`, 4 x fps for `0x3013C`/`0x30140`). Mutations run, rebuilt, reverted (scratch copy): k 4 -> 3, special 10 -> 4, scale division dropped, special range `< 3` -> `< 2`: **4/4 killed**. Not testable here: the `- 1` (a GF-side tick convention fixed by the oracle's g4) and the foul branch.
- `ponytail:` stand-ins now in `gf_match.cpp`: early opening not ported (site `0x140472640`/`0x140472989` and the tail), scale `[[G+0x2C0]+0x5C]` fixed at 1.0 (read live), foul gate (`0x14047EB00`).

## 4. Start delay in numbers (what is measured)

| goal | PES gate (frames from the DemoInfo) | GF before (108) | GF now |
|---|---|---|---|
| g1 `0x300A9` -> `0x3011F` (rebuilt) | 232 | 108 | 215 |
| g2 `0x300C0` | 213 | 108 | 215 |
| g3 `0x3013D` (special: 10 s timer, 3.5 m arrival) | 147 | 108 | 108 (special scenes keep the old 2.0 s stand-in: their own timer is 540 frames, never PES's gate here; arrival test not ported) |
| g4 `0x30034` (timer) | 215 | 108 | 215 |
| live `0x30140` | 174 | not modelled | 215 |

(GF's gate additionally runs after the same 2-frame pick-to-DemoInfo lag as before.) Not like-for-like against GF frames: no GF frame exists.

## 5. PES frame burst of the live goal `0x30140` (CONFIRMED by looking; sheet `pes_burst1_sheet.png` in scratch; `burst.py` grabs the PES window and names each frame by the game frame counter)

- f37701-37703: the ball is in the net (frame #0, #10: live broadcast camera, 2-2 on the HUD, players still moving; scene id appears at 37703). The camera stays the live one through f37864 (#35, scorer running to the corner, small) and f37880 (#42, DemoInfo state 8).
- **First cutscene shot (scorer close-up, `st` 10) at f37896: 195 frames (3.6 s) after the goal**; HUD now 3-2 at f37977-38185 (#50, #60: close-ups of the scorer and a fan view). At f38588 and f38790 (#75, #84) a hand-drawn cartoon layer is composited over the live-action cut (the multi-layer scene); the last grabbed frame of the scene is f38790 (state 0): **about 1089 frames (20 s) from the goal to the end**.
- Calibration: the frames are `import -window` grabs of the PES window (1284x724, HUD clock at the same place in all), no sRGB decode needed per the brief. Seen, not measured: the interval between grabs is wall time (PES ran at ~1-2 fps), the game frame is read from the poller row nearest each grab (+/- a few frames).
- GF: **no frame** (build unfinished). Whether GF's cut selection, layers and end match is NOT visually verified.

## 6. Left (numbered)

1. Early opening: port the tail of `0x140472640` (angle clear with `0x140439E50`, `0x1404583B0`, DB rows `0x140A7CC00/0x140A7C800`, side-1 slot map `0x140A3B320`) and feed it from GF's own scorer each tick (pos x/z, facing), `Tick(bool gateReady)` replacing `gateTicks`. The per-frame A0 log exists for one goal (`live_e.jsonl`); the poller does not log the scorer's facing (`+0x554`): add it for the next goal.
2. GF frame + like-for-like comparison with the PES burst (finish `godotfootball`: `b4.pid`; then `--cutscene` logging on a goal).
3. g1's rebuilt-scene gate (232) and the scene list from `0x140477BC0`.
4. Foul gate `0x14047EB00` in GF.
5. Fast tier on the finished change.
6. My poller (PID 1898509) is stopped and the burst watcher finished; scratch (449 MB: `live_e.jsonl` with the per-frame A0 of goal `0x30140`, `pes_burst1/` 141 frames, `pes_burst1_sheet.png`, the decompiles) stays until this report is archived; Main may delete `gf-CutsceneTiming-scratch/` afterwards.
