# GF goal and foul cutscene run in Godot, with frames: goal scenes play their clip camera (but at a zero cut offset, so the framing is not PES's); the foul scene has no clip camera at all (procedural cameras, not ported) (CutsceneFrames, 29-10-26)

Branch `CutsceneSelect`, worktree `<workdrive>/gf-CutsceneSelect`, head `030dc702` (`git ls-remote origin CutsceneSelect` = `030dc7027c9810a6c9cb7f924807253a1df436d8`). Labels: CONFIRMED = run/frame/test this session; INF = inference; OPEN = blocker named. Frames are screenshots of the owner's PES install and live game: scratch only, never committed (`<workdrive>/gf-CutsceneFrames-scratch/`: `pairs/`, `gdmovie/`, `burst2/`, `burst3/`).

## 0. Status against the ticket

| item | result |
|---|---|
| 1. merge `origin/main` (symlink trap), push, build `godotfootball`, build time | DONE. Merge `a561f771` (docs only: CutsceneHybrid4's report), pushed. Build: the first `cmake --build build -j3` (all targets, first godot-cpp build in this worktree) took **12 m 11 s** and then **failed to link `libgodot-cpp.a`: `ar: unable to copy file '../../bin/...'`** because the Makefile generator does not create `build/bin`; `mkdir -p build/bin` + `--target godotfootball` finished in **1 m 59 s** (total about 14 min, machine load 35). Incremental gdext rebuild afterwards ~2 min. |
| 2. run the PES-loop match, take the request, camera plays, scene ends with its clip | Goal: DONE (CONFIRMED, 4 goals in the 20,000-frame movie). Foul: request taken, **no camera plays** (§2). |
| 3. frames | Godot goal frames and foul frames (GPU, RADV); PES goal and foul frames from a live burst. **Not like-for-like for either** (§3, §4). |
| 4. tests | 97 of 98 `pes_*` binaries green; `pes_restart_delay_test` exceeded a 20-minute cap with 5 cases green (slow seed-searching sim cases, not a hang; §5); `gen_translated.py --check` clean. |

Acceptance "a GF goal and a GF foul each play PES's chosen cutscene camera (frames)": **NOT MET.** The goal's clip camera plays and ends with its clip, but its framing is wrong (zero cut offset, §3). The foul plays no cutscene camera (§2).

## 1. Godot run (CONFIRMED)

Command (the only run that rendered): `gamescope --backend headless -W 1280 -H 720 -- <godot 4.7.2> --path godot --write-movie m.avi --fixed-fps 30 --quit-after 20000 res://match/match.tscn -- --pes21-root <install> --home-team 1 --away-team 2 --pes-loop`; log has `Vulkan 1.4.354 ... Using Device #0: AMD Radeon RX 6900 XT (RADV NAVI21)`. Frames cut from the AVI with ffmpeg `select=eq(n,...)`. A headless `--fixed-fps 270` run (no rendering, 8 min for 12 simulated minutes) found the event frames first; the 30 fps run reproduces them (same tick sequence: first goal at process frame 37809/270 = 140.03 s = 4201/30 + 0.03).

`match.gd` now logs the frame a scene is taken and when it ends. From the movie log:

| movie frame | event | picked | camera | ended |
|---|---|---|---|---|
| 4201 | goal 1 | scene `0x300a4`, cut `0x300a4` row 0, layers `goal_celebrate_0302_base`, `..._mayaL0x`, `goal_Effect_h` | `goal_celebrate_0302_mayaL0x.fdc` camera 0 | 4341 (4.700 s) |
| 11517 | goal 2 | `0x300bd`, row 1, 6 layers (`base`, `Effect_h`, `goal_cmnCam_outH00`, `..._noSwitch10`, `..._noSwitch20`, `0254_mayaL1x`) | `goal_celebrate_0254_mayaL1x.fdc` camera 0 | 11876 (12.000 s) |
| 15698 | goal 3 | `0x300a4` again | mayaL0x | 15838 |
| 17693 | goal 4 | `0x300a5`, row 0 | `goal_celebrate_0303_mayaL0x.fdc` | 17833 |
| ~19570 | foul stop (`setpiece=3` FreeKick, stop time 652111 ms) | request OK (`has_value`, 1 step): cut `0x20007` row 0, layers `["foul_warn_card_n05.fdc"]` | **none** | never (no scene end) |

- `take_cutscene()` is consumed once per request; `load_layers` takes the first layer that has a clip camera; the scene ends when `_cut_t >= duration_s()` and `cutscene_finished(scene)` is called (all four goals, CONFIRMED by the log lines). Nothing syntax-checked-only was found broken in the goal path.
- Layers without a camera record (`*_base.fdc`, `goal_Effect_h`) return `ERR_INVALID_PARAMETER` quietly. Layers whose camera record has an **empty anime path** (the three `goal_cmnCam_*` layers, and both camera records of `foul_warn_card_n05.fdc`) used to push `PesCutsceneCamera: no resource for` (noise on every goal and foul). **Changed:** `load_file` returns `ERR_DOES_NOT_EXIST` for an empty anime path with a `ponytail:` comment naming PES's procedural slot (`FixDemoCamera` slot `+0x50` SetProcedural `0x141F7B630`, update `0x141F7BFE0`, which frames the record's `targetKind`/`targetNo` humans with its zoom stages; `pes-re-camera-pose.md` §2.1). `match.gd` prints `cutscene scene 0x.. : no clip camera in its layers, broadcast camera kept` when every layer is procedural.

## 2. Why the foul has no camera (CONFIRMED by a probe script over the VFS, INF for PES's side)

`foul_warn_card_n05.fdc` camera 0 and 1: both `no resource` (empty anime path); camera 2 does not exist. The foul scene's cameras are the card-scene `DmCamera` records with `targetKind 6`, `targetNo [22,1]` (referee, carded player; `pes-re-cutscene-roles.md` §6, `pes-re-foul-choreography.md`), i.e. a **target-following procedural camera** (`0x141F7BFE0`, decompile fetched this session: 454 lines, calls the target camera object at `this+0x38`, zoom stage chain on record `+0xB8..+0xD0`, reads the FixDemo humans). It needs the scene's humans (referee, carded player) at their cut positions; GF has no FixDemo humans. So the foul camera is not a missing clip but a missing subsystem (procedural camera + scene actors). `pes-port-CutsceneHybrid4.md` §6 item 1 ("whether the first layer ... has a camera record at index 0") is answered: no, for the foul it never does.

Same for goals in PES: PES plays all layers by key time (mayaL clip + the cmnCam procedural cameras + replay). GF plays one clip camera only and a request with several steps plays the first step only (`_poll_cutscene` returns at the first success). PES's captured goal events have 2-3 steps (`0x30005`+`0x30103`, `0x3014B/C/D`), GF's own `PickGoalScene` picked single-step scenes in all four goals here.

## 3. Goal frames (`pairs/goal_pes_over_godot.jpg`; top row PES `burst2/f_00064,00080,00104,00124`, bottom row Godot movie frames 4203, 4240, 4280, 4320; I looked at both rows)

- PES (live, burst at 8 fps, window capture 1284x724, no sRGB decode needed): the goal scene is a sequence of shots: scorer at the net, sky, celebration close-ups, run-away tracking shot, then the replay wipe. Scene length about 10 s of the burst.
- GF: shot of a green player from behind facing the stand (4203), then empty stand and roof for the rest of the clip (4240-4320). The camera plays the picked clip (`0302_mayaL0x`) at the right clock (4.7 s) but at **cut offset zero**: GF has no FixDemo player to write the cut offset (`ponytail:` already in `PesCutsceneCamera::set_cut_offset`'s caller; PES writer `0x141F2E11C` copies the stage default player `+0xAC0..+0xAD0` into `player+0xA08..+0xA18`, confirmed again in the bridge decompile of `FUN_141f2dd80`; where `+0xAC0` comes from for a goal is OPEN). INF (not tested: no non-zero offset was applied): the clip is authored around the scorer's celebration spot, so with the origin at the pitch origin it looks at the stand.
- **Verdict: NOT like-for-like and not a camera match.** Different picked scene (PES's pick for this goal is unknown: the watcher's per-hook cap of 6 was reached, and the pick cache at ctx `+0x30` was empty when read live: `livemap.py`, size 0), no scene humans in GF, zero offset in GF, PES is shot-sequenced and GF plays one shot. The only like-for-like items this session establishes: the clip is the right clip for the scene id GF picked, and runs its own length.
- PES goal cut frames are a live comparison source for the later offset decode: `burst2/f_00056..f_00140` (scene, then replay wipe at f_00144) and `burst3/f_06040..f_06200` (second goal: celebration at 6040, the replay with the goal-net camera at f_06220-06230, wipe at f_06240).

## 4. Foul frames (`pairs/foul_pes_over_godot.jpg`; top row PES `burst3/f_01100,01125,01150,01172`, bottom row Godot 19575, 19600, 19700)

- PES: the card scene is a close-up pair: the referee (laptop-head model of this modded roster) and the carded player, camera 2-3 m from them, a "Skip" prompt, then the replay wipe at the end (`f_01172`). A second card scene in the same burst: `burst3/f_0461x` (referee with a browser-icon head, then the wipe).
- GF: the in-play broadcast camera, unchanged, for the whole stop (as §1 says); no card scene.
- **Verdict: not like-for-like, and not a camera match: GF plays no foul cutscene camera.** PES's frame is the target the procedural camera port must reproduce.
- Note: PES fouls without a card show no scene in the burst; GF requests the card scene `0x20008` for every FreeKick stop (the existing `ponytail:` on the sanction tree `0x14047F170`), so the GF foul log line appears more often than PES's scene does.

## 5. Tests (no gate, no sim slot; Main gates)

- All 98 `pes_*` binaries (`build/tests` and `build/tests/sim`, every one links `pesfmt` or the sim library that holds the changed files), each `PES21_ROOT` set, `timeout 600`, two at a time: **96 rc=0** (2004 gtest cases passed in total), including `pes_fixdemo_select_test` 6/6, `pes_fixdemo_table_test` 6/6, `pes_fdc_test` 41/41.
- **2 hit the 600 s cap (rc 124): `pes_restart_delay_test`, `pes_throwin_release_test`.** Rerun at load ~20 with `timeout 1200`: `pes_throwin_release_test` **rc 0 in 13 m 52 s**; `pes_restart_delay_test` again **rc 124 at 20 m**, with five cases `OK` (72, 85, 87, 159 s ...) and the sixth running (`AThrowInTakerWalksToHisSpotAndIsNotPlacedOnIt`; its log shows `[stalemate] 5.0s idle ... forcing A4` recovery lines, i.e. the sim's own watchdog fired and play continued). These cases search seeds for their throw-ins (commit `14be50b0`), so under shared load they run for minutes each. I read this as slow, not hung, and unrelated to the cutscene files (the test references nothing of goal/celebration); not proven on `origin/main`: **OPEN, Main's gate decides**.
- `gen_translated.py --image pes_full.bin --check`: `4588 functions (3683 recorded bit-exact, 905 closure) regenerate byte-identical`, one held-out function reported (`0x14202BA30`, mistranslated callee `0x14202C250`, pre-existing). 10 min 33 s.
- `godot_*` tests were not run. No new unit test: the changes are a log line and a quiet return; the oracle is the frames.

## 6. Open items (numbered)

1. **Goal cut offset.** Nothing writes a non-zero offset, so every goal camera sits at the origin. Needs: who writes `player+0xAC0..+0xAD0` (stage default) for a goal scene, and the offset's relation to the scorer/goal; read the FixDemo player live during a goal cut (`burst2/f_00056` onwards is the visual target) or hook it.
2. **Procedural camera** (`0x141F7B630` SetProcedural, `0x141F7BFE0` update, target object at `this+0x38`, zoom chain `0x14087d830`.. `0x140892660`): not decoded or ported. Blocks the foul scene's camera and the three `goal_cmnCam_*` layers.
3. **Scene actors** (FixDemo humans: referee slot 22, carded player slot 1, nearest mates/opponents; `0x141F5F330`): GF has none, so even a ported camera frames empty grass until they exist.
4. **Multi-layer, multi-step playback:** GF plays the first clip camera of the first step; PES plays every layer's cameras by key time and every cut of the scene.
5. **Scene end for a scene with no clip camera:** `cutscene_finished` is never called for the foul (no duration source; the cut's end time is not parsed), so PES's category rotation never advances for category 2 in GF.
6. PES's own pick for a live goal/foul is unreadable after the fact (hook cap 6 per launch; the cache at ctx `+0x30` is cleared at the next event). To pair a PES frame with a GF frame of the **same** cut, Main must raise the `fixdemo_ctx_init_141F514D0` / `fixdemo_wrapper_141F4FDA0` hook caps or restart PES.
7. Build hygiene: `cmake --build build` in a fresh Makefile worktree fails at the godot-cpp link unless `build/bin` exists (`mkdir -p build/bin`); not fixed in CMake here.

## 7. Files

Changed this session (commit `030dc702`): `godot/match/match.gd` (frame-stamped scene log lines, procedural-only log), `src/gdext/pes_cutscene_camera.cpp` (quiet skip of procedural records, ponytail comment). Scratch: `gd/layers.gd`, `gd/foul.gd` (probes), `livemap.py` (live pick cache reader, read-only `/proc` pread), `pairs/`, `gdmovie/` (3 GB AVI), `burst2/`, `burst3/`, `t/` (test logs).

GATE fast (interim): not run; `pes_*` binaries 97/98 GREEN (+ `pes_restart_delay_test` slow, see §5) and `gen_translated.py --check` GREEN.
