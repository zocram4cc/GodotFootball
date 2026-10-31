# CutsceneTiming5: GF's scorer now runs to PES's celebration spot T, but the gate still opens on the timer (GF's run is 2.6 m/s, PES's 5+); the step list (0x140477BC0) is decoded and its arena blockers named, NOT fed (31-10-26)

Branch `CutsceneTiming` (worktree `gf-CutsceneTiming`). Pushed heads (`git ls-remote origin CutsceneTiming`): `f997b663` (merge of `origin/main`; `origin/CutsceneSelect` was already an ancestor), `62030952` (final head). Labels: CONFIRMED = bridge decompile, capture read, test or frame this session; INF = inference; OPEN = blocker named. PES-derived material (captures, polls, frames) is in `gf-CutsceneTiming-scratch/` (`ct5/`, `keep5/` 14 MB), never committed.

## 0. Status against the ticket

| item | result |
|---|---|
| 1. merge `origin/main`, `origin/CutsceneSelect`, push | DONE (`f997b663`). |
| 2. scorer runs to PES's spot; gate opens by distance/facing | **PARTLY.** GF's scorer now heads for PES's T (section 1). **The gate still opened on the timer** (call 216, step 0 at frame 2716, same as before): GF's scorer covers only 7 m in 3.9 s (section 2). The PES driver of the scorer's run is NOT decoded (section 3); the sprint to T is a named `ponytail:`. No PES-vs-GF gate-frame equality was reached. |
| 3. feed the builder `0x140477BC0` instead of `{sceneId}` | **NOT DONE.** Decoded what the list is and what the arena lacks (section 4). `f.cuts = {sceneId}` and its `ponytail:` stay. |
| 4. frames | GF goal movie looked at (section 5): one step, same start/end as before. PES burst not retaken. Not like-for-like. |
| 5. fast tier | interim line at the end. |

## 1. What changed (CONFIRMED by build + 18/18 test run)

- `Match::SetGoalCelebrationSpot / GetGoalCelebrationSpot` (`match.hpp`); cleared with the goal state (`match.cpp`, the `goalScoredTimer = 0` branch).
- `GfMatch::PollCutscenes` hands the anchor's `target` (T = `A0+0x30`, translated `0x140476C50`; PES x, up, z map to GF x, -y, up) to the match when the goal anchor is built.
- `ElizaController::_AddCelebration`: the scorer (`GetLastGoalScorer()`) takes T as his celebration position instead of the 16 m-capped `GoalCelebration::RunTarget`. The capped target stays for the headless sim (no PES goal scene). `ponytail:` in the code names the missing PES driver.
- `goalcelebration.hpp`: `kApproachCap_ms` 2800 to 6000, `ponytail:` (PES's gate times out at `fps x 4 / scale`, `0x14047A710`, 4 s; a 2.8 s cap made the scorer stop and perform before the gate's distance test could pass).
- `tools/dev/cutscene/live_dpi_poll.py`: read-only poller that logs, while a goal DemoInfo is up, the changed DemoPlayerInfo (`G+(i+0xD9)*0x28`), man (`+0x5A0..`) and slot-record (`+0x1400..`) bytes of all 22 men per frame. It ran 08:26 to 08:45 on PES pid 3061186 and **caught no goal** (empty log): the next goal gives the scorer's role/target words.
- Tests: `pes_goal_anchor_test` 18/18 (GoalAnchor 7, GoalGate 7, FoulGate 4), `pes_cutscene_timeline_test` and `pes_fixdemo_select_test` no failures (timeline oracle skips without `GF_CUT_EPISODES`). **No new test and no mutation**: the new behaviour lives in `ElizaController` and `GfMatch` (no test harness reaches them without a Godot match); it is covered only by the movie run below. A relation test needs a `Match` fixture; left (item 1 below).

## 2. Why the gate still opens on the timer (CONFIRMED: temporary `GATEDIAG` print each 6 ticks, removed)

Same match (seed, teams 1 v 2, `--pes-loop`), goal at movie frame 2592 (scene `0x300a4`), step 0 at 2716 (124 frames = 4.13 s), scene ended 2801: identical to CutsceneTiming4. The scorer after the change:

- start (46.3, 5.0), T = (49.6, -31.5): 36.5 m. Class 10 (SHOOT) for 18 ticks, then class 2 (accepted).
- Facing goes 130 to 171 to 175 deg, the bearing to T (174.8): the heading is right.
- **Ground speed ramps to ~2.6-2.75 m/s (anime +0x54 feed 9.9 = 3.6 x 2.75) and stays there**: z 5.0 to -2.3 over the 216 gate calls (7 m). The request is `sprintVelocity` (8 m/s, `velocitystate.hpp`). PES's scorer runs 5.2 m/s (`live_f.jsonl` goal: 31 m in about 5.7 s, +0x54 20-28).
- So the radius test (7.16 m here) can never pass inside 4 s; the timer decides. Where GF's run speed is capped between the command (8 m/s) and the motion (2.7 m/s) is OPEN: it was already slow before this change (CutsceneTiming4 saw the scorer near (46.3, 5..12)). INF: the pes-loop movement port or the animation choice limits the speed, not `_AddCelebration`. Next step: log `desiredVelocityFloat` against the man's resulting speed on the same goal.

## 3. What drives PES's scorer to T (CONFIRMED decodes, driver OPEN)

- The "run object" at `G+0x21E8` (accessor `0x140A489A0`, index `(i+0xD9)*0x28`) is the **DemoPlayerInfo** table (`DemoPlayerInfoRef` 0xD8 in `pes-re-defensive-shape.md`; man table 0xBE = PlayerMoveRef, anime 0xA4). Fields read by the goal cluster: `+0x18` target position (`0x1419E0900`), `+0x24`/`+0x28` role word (`0x140A73E40(obj, 1)`: 9 and 0xB are cast roles in `0x140473990`), `+0x34` byte (`0x140A73F10`, tested by `0x1404441D0`), `+0x6C` byte (`0x140A73E30`: "has an ordered target").
- `0x140476190` (T for kind 1) uses the DemoPlayerInfo only as an **override**: when `+0x6C` is set and `A0+0x48/+0x50` are nonzero it copies `A0+0x48..0x50`; if `+0x6C` is set it returns `obj+0x18`; otherwise it **computes** the corner spot from the scorer's position and velocity (`0x14046F260`, field info, the generator draw `0x140A92530`). It never writes the object. So `T == DemoPlayerInfo+0x18` in the captures is a copy elsewhere, not the writer. The existing `ponytail:` in `fixdemo_select.cpp` ("writer of +0x18 not decoded") stands.
- Writers of the DemoPlayerInfo roles in a stoppage are `0x140504D20` (loops 22 men, `0x14102CAD0`, `0x140A73F60(obj,0,3)`, `0x140A73FE0(obj, &pos)` when `+0x6C == 0`, then by out-of-play kind: 0 `0x1405B1740` (generic: roles 8..0xE for set pieces), 1 `0x1405AD120` (foul: 0xF..0x13), 2 `0x1405AD720` (offside)). **The goal role writer is not among these**; `0x140473990` is the scene's cast mapper (cut role words `0x1300..0x131A` to men, per list entry), not the pre-gate run. The caller list of `0x140A489A0` (66 sites: `0x1404441D0`, `0x140452AF0`, `0x14044E460`, `0x14044E720`, `0x14044FB10`, `0x1404500C0`, `0x1405055E0`, `0x1405B5230`, `0x1405AEF70`, `0x1405AF630`, `0x1405AF880`, `0x1406B50A0..0x1406B6707`, 30 handlers in `0x1407E1C60..0x1407F4AF3`, `0x14087DF60`, `0x1409243F0..0x14092A340`) is the shortlist for the next decode; the 30 `0x1407E*` are anime-action handlers reading `+0xAB8` (they classify, not drive).
- Still open: who orders the scorer's run in kind-4 state 2 (before the gate), and at what speed. Needs the poller's goal rows (DemoPlayerInfo `+0x18/+0x24/+0x6C` on the scorer) to pick the writer from.

## 4. The scene list (CONFIRMED on captures; feed OPEN)

- **The DemoInfo list is a list of scene ids, one FixDemoMatch step each.** The 07:00 goal's DemoInfo (`pid2122415/seq70116`, state 10): scene `0x3006C`, count 3, entries at `D+0x10 + 0x1C*i` = `0x3006C, 0x30136, 0x3011C`; the FixDemoMatch holds 4 steps (`0x3006C, 0x30136, 0x3011C`, then the terminator `0x3011C` repeated at `+0x60`). `scene_map.py` shows each of those three scenes is **one step** (cut = the scene's own id: `GOAL_CELEBRATE_0279`, `GOAL_I_SET06`, the `0x3011C` lead). So GF's `Request(sceneId)` returns the first step only and `f.cuts = {sceneId}` (also the input of the radius `0x140474CF0`) is the whole gap: PES's list is the builder's `{picked, closing (0x30130 + A0+0x20 - 1), 0x3011C, 0x3012F}` per its conditions.
- **The builder also makes the pick** (`0x140477BC0` first calls `0x140475A90`, which sets `A0+8`; GF's `PickGoalScene` is a port of part of it). A cut-over replaces `PickGoalScene` + `f.cuts` + `kGoalRule` in one change.
- **A0 across five captured goals (script-read from full snapshots, `pid2122415` seq12946/31076/40581/53099/53100)**: `A0+0` goal level = **2 on all five** (GF's `kGoalRule` level is 0: a stand-in the builder's pick pool filter reads; writer `0x140477570`, not translated, reads the setup struct `G+0x2AD0` records and the event ring); `A0+0x13C` (previous pick) carries over from one goal to the next (`0x30126 -> 0x30052 -> 0x30151 -> 0x300D4 -> 0x3013D`: GF zeroes A0 each goal and loses it); `A0+0x20` (closing selector) 5, 7, 7, 0, 0; `A0+0x2D/0x2E/0xED` 0/1/1 by case; `A0+0xA8` and `+0x54` words 8 (default kinds).
- **The arena the builder needs beyond GF's (blockers, each with its PES site):**
  1. The event ring behind `ctx+0xE0` (`0x140A3E6D0(ctx+0xE0, 0)`: count `+0x74FC0`, index `+0x74FC4`, entries at `+0x27400 + k*0x640`): the newest record read at `+0x14C` (event side, 0), `+0x150` (place, 6), `+0x154` (2), `+0x158` (255), `+0x168` (0), `+0x16C` (1). Values are seq53100's; the meanings of `+0x154/+0x158/+0x168/+0x16C` are INF.
  2. The setup struct `G+0x2AD0` (byte 0 read by `0x140471210`; the level writer reads records at `+0xC400`).
  3. The slot table `G+0x180` (24 x 0x74, `+0x20` = 0xFF, `+0xD` = 0 on all: "no designation"; `0x140473990` reads it).
  4. MI: `+0x1168` (period), `+0x116C` (play state 2), `+0x1174` (clock float), `+0x1178..+0x1188` (16 bytes read by `0x140A3C070`), `+0x1574` (3), `+0x15C0`, `+0x162C/+0x1630` (2, 255), presence bytes `+0x1748+slot` (1) and `+0x175E+slot` (0).
  5. The DB goal lists `0x140A7CCE0` / `0x140A7CE90` (DB+0x428/0x430) that `0x140474B20` draws from (the lead-in pick).
  6. A persistent A0 (previous pick) and the `0x14047DF00` clock seed of the generator (`A0+0x108`; GF lends the goal pick's state).
  Seen only on goal A (CutAnchor6): `0x1408C9190` name-hash lookup (owner OPEN).
- Verification plan once fed: replay `0x140477BC0` over the 07:00 goal's anchor snapshot (`seq53100` is another goal: scene `0x3013D`; the 07:00 goal's calc capture is not in the `pid2122415` list I read: demoinfo_update `seq70116/90314..` carry DemoInfo and A0 but not the builder's call) with A0's generator rewound two draws, as CutAnchor6 did for goal A, and compare with the DemoInfo list above.

## 5. Frames (looked at; NOT like-for-like)

GF: `godotfootball` rebuilt with the change; GPU capture (`gamescope --backend headless`, RADV `AMD Radeon RX 6900 XT`), `res://match/match.tscn -- --pes21-root ... --home-team 1 --away-team 2 --pes-loop`, 1152x648 @ 30. Sheet `keep5/sheet5.png`, process frames 2560, 2640, 2716, 2730, 2760, 2800 (all viewed).
- Seen: 2560 and 2640 the live broadcast camera at the goal end (white kit attacking, green defending, men converging on the box); from 2716 the cutscene camera: a low shot of a green-kit man in front of the red bench with the stands behind (2716, 2730), then stands and roof only (2760, 2800). The scorer is not in shot. Same framing as CutsceneTiming4's frames.
- Log: request at 2592, **step 0 starts 2716 (4.13 s), scene ended 2801 (2.8 s later)**: unchanged by this branch. PES: opens at 3.6 s on the logged goal, plays 2-3 steps for ~20 s. Not seen in a frame: the scorer running to the spot, the multi-step cuts (not built).

## 6. Left (numbered)

1. Find why GF's celebration run is 2.6 m/s against a requested 8 m/s (log the requested vs achieved speed in `_AddCelebration` on pes-loop); until then the gate opens on the timer. Then a relation test on `Match` (a scorer given a far spot arrives and the gate opens before the timer).
2. Decode the PES driver of the scorer's run: next goal in `live_dpi_poll.py` (restart it at PES's next goal; command in the file header) gives `DemoPlayerInfo+0x18/+0x24/+0x6C`, man `+0x5B0` and the slot-record target (`+0x1414`) per frame; then the writer among the section 3 callers.
3. Feed `0x140477BC0`: arena items 1-6 of section 4, `kGoalRule` level 2, persistent A0 previous pick, then `Request` per list entry (concatenated steps, each entry one step) and the radius over the list; delete `PickGoalScene`'s use, `f.cuts = {sceneId}`, `kGoalRule`.
4. Frames after 1 and 3: a PES burst of one goal (stored at 10 fps) against the GF movie of the same shots.
5. `gen_translated.py --check` and the full `pes_*` set: Main's batch (no layer file was touched here).
6. Scratch `gf-CutsceneTiming-scratch/`: `ct5/` (448 KB: decompiles, snapshot readers `a0.py`, `ev.py`, `dinfo.py`), `keep5/` (14 MB frames), the old 529 MB CutsceneTiming4 files; Main may delete after archiving.

**`GATE fast (interim, tools/dev/gate.sh not run; machine load 22-27): pes_goal_anchor_test 18/18, pes_cutscene_timeline_test and pes_fixdemo_select_test no failures, godotfootball and the gfsim lib build; gen_translated.py --check not run (no layer change); other pes_* not run.`**
