# ProcCamera7: the camera engine now links every callee recorded bit-exact in a proven results set; the seed-7 goal's three steps all run PES's procedural engine with no stop; step 0 shows PES's telephoto framing the scorer (frames 2845-2955); steps 1 and 2 run the engine but their target slots other than the scorer are not fed (01-11-26)

Branch `CutsceneTiming` (worktree `gf-CutsceneTiming`), commits `7c9b3bb4` (merge of origin/main) and `4645193f` (the change). Labels: CONFIRMED = run this session; INF = inference; OPEN = blocker named. Scratch (movie, frames, traces) is in `gf-ProcCamera7-scratch/` and is PES-derived; never committed.

## 0. Status against the ticket

| ticket item | result |
|---|---|
| 1. merge `origin/main`, push | DONE (`7c9b3bb4`, conflict-free). |
| 2. root fix of the out-of-set guard, allow-list from the results files, tests, mutations | DONE (section 1, 2). |
| 3. seed-7 movie: per step ran/failed, look at frames | DONE: all three steps ran, no failure (section 3). Frames looked at: sections 3 and 4. |
| 4. interim gate line | section 6. |

## 1. The ruling at its root (CONFIRMED)

The engine `0x141F7BFE0` was proven bounded (`snapdiff --bounded`): a callee no camera replay entered is an `OUT-OF-SET CALL` site, emitted by the translator as `pes_dispatch_missing(VA)`. `FUN_140891FE0` (the zoom-ramp hold) and `0x140A91A30` (a vec3 x 3x3 matrix leaf called from `0x140889760` / `0x140897040`) are recorded bit-exact standalone (`results_cutscene.json`) but were gaps there.

Fix (`tools/dev/translate/gen_translated.py`): `recorded_vas()` = every VA of a record with `diffed` true in any of the nine `SETS` results files (read at generation time, no hand list); `link_universe(universe, members)` = `universe | members | recorded_vas()`, used by `build` where the per-record universe was built. The translator links a callee in the universe (`self.pilot`) as a direct `pes_fn_` call, so every layer function now links every recorded callee; a callee no results file records (or a record with `diffed` false, e.g. `0x1403FB710` "struct map gap") stays an `OUT-OF-SET CALL`. Closure-only members of another record's closure are not in the universe, so they stay blocked in a caller that did not prove them (the ruling's second half).

Effect on the layer: `gen_translated.py` rewrote **11 files** (no new members, 4,837 functions as before): `closure/pes_fn_1403F7DC0, 140889760, 14088D850, 140890DD0, 140893A10, 140893AF0, 140896150, 140897040, 14089BE70, 14089F490` and `cutscene/pes_fn_141F7BFE0`. For the engine the three sites `0x140891FE0`, `0x14089D4B0`, `0x1408921A0` are now linked. **Still gaps in the engine (never recorded, so still fail the frame):** `0x140892240`, `0x1408922E0`, `0x140892560` (the stage 3/6/else preset thunks into `.impdata`) and `0x141F7AD90` (-> `0x15B59C060`). The proof of the engine is unchanged (`results_proccam.json`, 18/18): a newly linked site was never reached in a replay, which is the ruling's premise; each linked callee carries its own standalone proof.

The rig's recovery (`pes_dispatch_missing_handler`, `FixDemoCameraRig::Step`) is unchanged: it still turns a remaining gap into `Step -> nullopt, *missingVa`. Its ponytail comment now names the generator rule and the remaining gaps (`fixdemo_camera_rig.hpp`).

## 2. Tests and mutations (CONFIRMED)

- `pes_proc_camera_test` (now 6; the stale `AnUncapturedRecordFailsTheFrameNotTheProcess` is replaced: its stage-1 preset `0x140892100` had become proven by ProcCamera6, so that test could no longer fail):
  - `ARecordedLeafNoReplayEnteredRunsInTheEngine` (new): the captured record with a ramp key (stage 4 = 120) and a player clock past it (1000 ticks) must pose with no missing call. **RED on the old layer** (`stopped at 0x140891fe0`, 4 ms), green after.
  - `AnUnrecordedPresetFailsTheFrameNotTheProcess` (replaces the stale one): stage 3 selects the unrecorded thunk `0x140892560`; the frame must fail naming it, and a camera of the captured record on the same rig still ties PES's first frame.
- `tools/dev/translate/test_recorded_callee_link.py` (4 tests, new): a callee recorded in another set (`0x140891FE0`, `0x140A91A30`) is in `link_universe`; one no results file lists (`0x140892560`) is not; a listed-but-not-bit-exact record (`0x1403FB710`) is not; the set's own universe and closure are kept. **RED on the old generator** (4 errors: no `link_universe`).
- Mutations, each run, rebuilt, reverted: (a) the generator without `recorded_vas()` (the pre-change tree): C++ leaf test red + Python test 4 errors; (b) `OnMissingCall` storing `gMissingVa = 0`: `AnUnrecordedPresetFailsTheFrameNotTheProcess` fails (5 passed, 1 failed), reverted, green again. Not done: a mutation that makes the unrecorded test fail by linking `0x140892560` (no function exists to link; the Python test pins that rule).

## 3. Seed-7 goal movie, per step (CONFIRMED by the run log)

Movie: seed 7, teams 1 v 2, `--pes-loop`, 1152x648, 30 fps, RADV (`s7/`; the same events at the same frames in three reruns, so the match is deterministic). New log line `PesCutsceneCamera: procedural camera N engine ran: first pose at clock T ticks` (`pes_cutscene_camera.cpp`, once per rig). The merged tree's seed-7 match has a foul scene (`0x20008`) and then the goal:

| step (scene `0x300bf` goal_cmnCam_inM00p) | starts at frame | engine | tracks (record start tick, kind) |
|---|---|---|---|
| 0 | 2718 | **ran**, first pose at tick 250 (rig 0), no failure | 0 mode 6 (no rig); **250 mode 4 key 10 (rig 0)**, 250 two CANM clips (SL, SR); 480 CANM SKY x2 and mode 4 key 20 (rig 1) |
| 1 | 3082 | **ran** (rig 0 from tick 0), no failure | 0 mode 4 key 0 (rig 0), 0 mode 4 key 100 (rig 1): two procedural records start together, the first in layer order is in force |
| 2 | 3206 | **ran** (rig 0 from tick 0, rig 1 from tick 205), no failure | two layers: 0 mode 4 key 0 (rig 0), 205 mode 4 key 10 (rig 1), 350 mode 6 (no rig); 0 key 100 (rig 2), 205 key 110 (rig 3), 350 key 120 (mode 6) |
| end | 3511 | | |

(The track table is from a temporary debug print, reverted: the tracks of each step in load order, rig ids are per step. Step 2's log shows `engine ran` for rig 0 and rig 1, never for rigs 2 and 3, because the first record in layer order wins among records that start together.) No `stopped ... untranslated` line appears: before this change step 0 stopped at `0x140891fe0` (frame 576 of ProcCamera6's run) and step 1 at `0x140a91a30`. A second goal of the same movie (scene `0x300be`, outH00/noSwitch10) also ran its steps 0 and 2 with no stop (step 1 logged no engine line: no procedural record in force there).

## 4. What the frames show (looked at; not compared with a PES frame of the same step)

Frames from `gf-ProcCamera7-scratch/f/` (`sheet0..3.png`), movie frame numbers:

- **Step 0, frames 2843-2957 (ticks 250-476): PES's telephoto frames the scorer.** Pose by the engine: eye (69.3..71.2, 13.9, -14.3..-13.3), target (29..33.3, 1.1, 2.7..8.2) = the scorer's torso height following him, vertical fov 0.0192-0.0196 rad (1.1 deg), a 44 m long lens: frames 2845, 2870, 2900, 2930, 2955 show one green-kit player, head and torso filling the frame, tracking him as he moves; 2955 shows an opponent's head entering at the left edge. The high broadcast shot before (2640, 2740, 2800) is the pre-cut play. This is the PES shot the captured goal showed (record mode 4, no CANM clip), now in GF.
- **Step 0, frames 2957-3081 (ticks 480-): not the procedural camera.** At tick 480 three records start together (CANM SKY x2, mode 4 key 20); `InForce` takes the first in layer order (a CANM clip camSKY), the documented stand-in for PES's `selectByRate` pick (`pes_cutscene_camera.cpp`, INF). Frames 3000 and 3070 show grass close-ups from that clip's pose, not a framed player. Whether PES shows that clip, or rig 1, at tick 480 is OPEN (the pick `0x141F42110`).
- **Step 1, frames 3086-3200: the engine runs but frames no actor.** The record (mode 4, zoom `[-1,0,90,-1,300]`, **target byte 21**) points at CAMERA_TARGET slot 22; GF writes only the scorer's slot, so the engine reads an empty record: target = (0.03, 1.5, 0.06) (the pitch centre at 1.5 m), eye (-42.5, 1.58, 26.2), fov 0.0183 -> 0.034 rad, near plane 38 m. The picture is a tight telephoto across the pitch (a corner flag, a stand door, a green player at the far corner, widening as the zoom ramps). It is PES's engine on a wrongly-empty target, not PES's shot.
- **Step 2, frames 3212-3350: the engine runs, target discontinuity.** Rig 0 (target byte 0 = the scorer): eye (56.5, 1.1, -0.2) then (48.8, 1.07, -1.4), the target jumps from (37, 1.15, 13.7) to (-0.6, 0.96, -9.0) within 20 ticks (the scorer's ground position as GF's sim reports it jumps), fov 0.031-0.038 rad; frames 3250 and 3350 show one to three players beside the stand wall. From tick 205 rig 1 (record mode 4 with the stage-2 key 300): eye (-46, 10.5, -3.5..-4.1), target near the same player; frame 3350 two players. Frames 3450 and 3520 are the broadcast shot again (after the step). Whether these match PES is not verified.

## 5. The next blocker (OPEN, exact)

1. **Target slots other than the scorer's are not fed (Godot pose hand-off).** `match.gd::_advance_cut_camera` writes only `CAMERA_TARGET_SCORER` (slot 1). Records carry up to three target bytes (`+0xB1..+0xB3`); step 1's first record uses byte 21 (slot 22). PES's actor update writes CAMERA_TARGET_02nn for the scene's other actors (the conceding team's GK and players: the layers are `ins_lostGK`, `lostFP`, `lostPlayers`). What each slot holds, and which GF player fills it, is not decoded; until then those cameras aim at the origin. Owner of the needed decode: the FixDemo actor/registry writer (vtable `0x142BB1048`, ponytail in `match.gd`).
2. **Which record is in force when several start together** (`selectByRate`, `0x141F42110`): decides whether step 0 continues with rig 1 or a CANM clip at tick 480.
3. **Scorer ground position jump at step 2 start**: the sim's `cutscene_actor` ground changes by ~40 m in 20 ticks; the cause is in the sim's placement, not the camera.
4. The four remaining gaps (`0x140892240`, `0x1408922E0`, `0x140892560`, `0x141F7AD90`) are not recorded: records whose first zero stage is 3, 6 or 7+ (mode 4 stages 0/2/3 and the stage 3 and 5 variants of the census) still fail the frame and keep the broadcast camera.
5. The movie was run outside a sim slot (all four were held by two gates for over 40 minutes; one Godot process, `nice 19`).

## 6. Tests and gate (CONFIRMED)

`GATE fast (interim; tools/dev/gate.sh not run, Main gates): gen_translated.py --check GREEN (4,837 byte-identical, run on the committed layer); pes_proc_camera_test 6/6; pes_goal_anchor_test 20/20; test_recorded_callee_link 4 OK; test_closure_walk 23 OK (1 skipped, no PES_EXE), under the DiffHarnessRE venv.`
