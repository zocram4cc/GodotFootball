# Goal cut anchor decoded (writer, function, live values), Cut-record start offset ported with tests; procedural cameras and frames NOT done (CutsceneCam, 29-10-26)

Branch `CutsceneSelect`, worktree `<workdrive>/gf-CutsceneSelect`. Labels: CONFIRMED = live read, capture, decompile or test this session; INF = inference; OPEN = blocker named. Live evidence is PES-derived scratch (`<workdrive>/gf-CutsceneSelect-scratch/live/scene2.jsonl`, `scene3.jsonl`, `poll_scene.py`, `srv.py`: read-only `/proc/<pid>/mem` through a persistent `sudo -n python3` reader, no pauses, no writes), never committed.

## 0. Status against the ticket

| item | result |
|---|---|
| 1. merge `origin/main`, push | DONE (`8282322d`; the symlink warning is harmless, the merge committed). |
| 2. goal anchor: what writes the cut offset | **Decoded, not yet translated (§1, §2).** The writer is `0x14047D280`; the value is a function of the scorer (`0x140474F00`). Live values read (§3). No translated function, no oracle yet: the existing hook captures are all entrance (§4). |
| 2b. what GF can do now | Ported and tested: the Cut-record part of the offset (§5). GF's stage default stays zero (`ponytail:` names `0x14047D280`/`0x140474F00`), so goal framing in Godot is unchanged. |
| 3. procedural cameras `0x141F7B630`, `0x141F7BFE0` | **NOT done.** Update body re-read (§6): it is the whole camera-work engine, not a framing formula. |
| 4. frames | **NOT taken.** Nothing rendered this session; no claim about framing. |
| 5. fast tier | see end. |

Acceptance (goal and foul cutscene cameras frame the same subjects as PES, frame pairs) is **NOT MET**.

## 1. Where a goal scene's offset comes from (CONFIRMED by decompile + live reads)

Three layers, in order:

1. **Scene anchor** (`FixDemoMatch`/`FixDemoList` object, vtable `0x142BA6BC0` or `0x142C701F8`/`0x142C70458` for other lists): `+0x110..+0x118` position, `+0x11C` rotY (degrees, wrapped once into [0,360) with `[0x14259858C]`), `+0x120` flip byte.
   **Writer: `FUN_14047D280(DemoInfo)`** (called from `FUN_14047C830` at `0x14047CC3F`; the only direct stores to `[scene+0x110..0x11C]` in `0x140470000..0x1404C0000`, found by an operand scan of the image). For DemoInfo kind 1 (`param_1[2] == 1`, goals): `FUN_140473990(...)` first, then **`FUN_140474F00(goal, matchInfo, &pos, &rotY)`** gives pos (3 floats) and rotY; they are stored to scene `+0x110/+0x114/+0x118/+0x11C` (and mirrored into DemoInfo `+0x2940..`); the flip byte `scene+0x120` is set to 1 when `FUN_140478FF0(goal, matchInfo, &pos)` returns non-zero. Kind 2 uses `FUN_14047ECD0` instead; kind 5 `FUN_1404864B0`.
2. **Stage default** `player+0xAC0` pos, `+0xACC` rotY, `+0xAD0` flip: written at scene start by **`FUN_141F5CD00(scene)`** (hooked, `fixdemo_scene_start`): it zeroes them, then a switch on the step's placement mode `FUN_141F613A0(scene, step)` = step `+0x10`: **1** pos `scene+0x110..0x118`, rotY `scene+0x11C`, flip `scene+0x120`; **2** the step node's own `+0x4C..+0x5C` when any is non-zero, else (first time, player flag `+0x1D1` clear) the scene anchor, else a value from the scene's map (`scene+0x60`, keyed by the step's first id; pos from the map value, rotY via `FUN_141EA2F60`); **3** flip = (`scene+0x110` x < 0); **4** flip = (x > 0); **5** the node's `+0x4C..+0x5C`; 0/other: zero.
3. **Cut start** (`0x141F2DD80`, `0x141F2E115..0x141F2E1B0`): `player+0xA08..+0xA18` = stage default, then replaced whole by the **first Cut record** (smallest id >= 0 of the player's cut map `+0xAE8`) when that Cut's offset flag (record `+0x19`, node `+0x49`) is set: pos `+0x1C`, rotY `+0x28`, flip `+0x2C`. The camera record then applies `+0xA08` when its use-cut-offset byte `+0xA6` is 1 (`0x141F39CC0`, FdcCamera5), and the function-number terms (`0x141F31F00`, record `+0xA8` u16[4]; kind 1 = another Cut's offset by Cut id) may override it.

Cut-offset data in the install (script over the parsed `.fdc` set): of the goal Cut records most have offset flag 1 with explicit `offsetPos`/`offsetRotY` (the `*_offSet_BL/BR/ML/..` variants), but `goal_celebrate_0302_base`, `0254_base` and others have flag 0, pos 0: for those the stage default (layer 2) **is** the placement. The camera layers (`*_mayaL0x`, `goal_cmnCam_*`) carry no Cut record; the base layer's does.

## 2. The anchor function (`0x140474F00`) and the event position (`0x1404B77D0`)

- `FUN_140474F00` (400 lines, decompiled to `scratch/dec_140474f00.txt`): early-out zero for scenes `0x3013D..0x3013F`, `0x3013B` and bits of `0x300D7+` (`0x2400001`); otherwise pos starts at the **scorer's position** (`FUN_140A48A10(DB, idx)+0x55C/0x564`), rotY = `FUN_140458740(scorer, DAT_14259BDD0 / fps)` (fps `FUN_141E532D0`), then per-axis modes read from the scene table row (`FUN_140A7CBA0(DB, sceneId)` bytes `+2` x-mode, `+3` z-mode: 0 = `FUN_1404771F0` clamp, 1/2 = pull in from the pitch edge by the pitch half-dimensions `[DB+0x2AA8]`, 3 = `FUN_140A7C9B0/9F0`, else clamp to half-dimension minus a constant), i.e. the scorer's spot pulled inside the pitch bounds. **Not translated**: the callees `FUN_1404771F0`, `FUN_140A7C9B0`, `FUN_140A7C9F0`, `FUN_140458740` and the mode bytes need the match DB layout and a case-by-case oracle.
- The ctx event position `ctx+0x5208..0x5210` is a **different** value: `FUN_140478190` calls `FUN_140476C50` (anchor by the first step's kind; kind 3 = `FUN_1404B77D0`; its non-final branch is `x = (pitch[0] + [0x14259CE3C]) * sign(scorer.x)`, `z = (pitch[1] - c) * sign(scorer.z)`, y from the scorer) and passes it to the DemoInfo builder `FUN_14047C500`, which stores it in the ctx for the evaluator conditions. It never reaches the scene anchor. Live: ctx event position read `(-49.56, 0, -31.52)` after a goal whose scene anchor was `(-39.48, 0, -14.07)` rotY 210.02 (so the FixDemo evaluator's "event position" is the corner-side spot, the placement anchor is the scorer's spot).

## 3. Live values (pid 2580963 and 1981120; poller 0.15-1.3 s period; CONFIRMED reads)

Goal events, scene object `scene+0x110..0x120`, player `+0xAC0` (stage default) and `+0xA08` (cut offset):

| event | scene vtable, steps (+0x10 modes) | scene anchor pos / rotY / flip | player stage default per step | player cut offset per step |
|---|---|---|---|---|
| goal A (pid 2580963) | `0x142BA6BC0`, 5 steps | (-39.48, 0, -14.07) / 210.02 / 0 | step 1 (-39.48,0,-14.07,210.02); step 2 (-46.69,0,-26.89,210.02); step 3 (-49.06,0,-30.97,210.02) | = default for steps 1-2; step 3: **(0,0,0) rotY 180** (a Cut record with the offset flag overrides the default) |
| goal B | `0x142BA6BC0` modes [2,5,0], then `0x142C70458` modes [1,0] | (-33.4, 0, 5.51) / 10.75 / 0 (same anchor for all three scene objects) | (-33.4,0,5.51,10.75) | = default |

So the default of a later step is **not** the anchor (goal A: -39.48 -> -46.69 -> -49.06 along the way to the corner flag); it comes from the mode-2 map branch (layer 2). Entrance scenes: placement mode 0, anchor and offsets zero in every capture (also consistent with FdcCamera5). `0x141F5CD00` capture of the entrance scene: 13 steps, step kinds `[1,4,4,...,4,3]`, modes all 0.

## 4. Captures: what exists (all entrance)

`pid2580963` seq020, 042, 051 (`fixdemo_scene_start`, calls 1, 9, 12 per `pauses.jsonl`) and seq021, 040, 041, 043, 048, 049 (`cut_offset`, calls 1, 12, 13, 14, 17, 18): the 13-step entrance list and entrance camera records, zero offsets. `FIRST` did not skip as planned (call 1 was captured), so both hooks' caps of 6 were spent before kick-off; the first goal came at about 15:44 and was released without snapshot. Needed from Main at the next restart (only if a hook oracle is wanted rather than my poller): `cut_offset` FIRST >= 25 and `fixdemo_scene_start` FIRST >= 16 (the entrance made at least 18 and 15 calls), and a hook on `0x14047D280` (rcx = DemoInfo; the output is at `[[rcx+0x1E0]+0x110..0x120]` after return, or hook its two callees' results) and on `0x140474F00` (args: rdx = match info; r8 = &pos, r9 = &rotY; read after return) so the translation has real-call cases.

## 5. Ported and tested

- `src/pes/fdc.{hpp,cpp}`: `CutRecord` (type 0: id, `+0x19`, `+0x1C`, `+0x28`, `+0x2C`), `File::cuts`, `CutStartOffset(file, stageDefault)` = layer 3 above.
- `PesCutsceneCamera`: the player's cut offset is now `CutStartOffset(base layer's cuts, stageDefault)` (the first layer of the variant that has a Cut record; `ponytail:` PES merges the layers' maps in its load order). `set_cut_offset` is renamed `set_stage_default` (clean cutover; `godot/tests/test_cutscene_camera.gd` updated). The stage default stays zero in GF (`ponytail:` naming `0x141F5CD00`/`0x14047D280`/`0x140474F00`), so nothing visible changes for goals yet: the base cut of `0302_base` has flag 0.
- Tests (`tests/pes_fdc_test.cpp`, `PesFdcCutStart.*`): a flagged Cut replaces the default with its fields; an unflagged Cut leaves the default whole even with fields set; the smallest id starts, not the first in the file; no Cut leaves the default. Mutations run, rebuilt, reverted: (a) ignore the flag -> `ACutWithoutTheOffsetFlag...` fails; (b) first-in-file instead of smallest id -> `TheCutOfTheSmallestId...` fails. The wrap of rotY into [0,360) in the writer is not repeated: `RotYMatrix` wraps its input the same way (FdcCamera5 mutation D).

## 6. Procedural cameras (`0x141F7B630` SetProcedural, `0x141F7BFE0` update): decode status

`FUN_141F7BFE0` (decompile in `gf-CutsceneFrames-scratch/dec_141F7BFE0.txt`, re-read this session) is not a framing formula: with `cam+0x128 == 1` it initialises a rig struct (`FUN_14087D830`, `FUN_140891A70`), picks a preset by the first zoom stage (record `+0xB8..+0xD0`) whose value is 0 (`0x140892100/0x140892660/0x140892420/0x1408921A0` plus four `thunk_EXT_*` into `0x1440xxxxx`, outside the Ghidra bridge's program but inside `pes_full.bin` (`.impdata`; see `pes-port-ImageGap.md`)), sets about 26 per-record bytes (`+0xD8..+0xEE`) through `FUN_140891B20/CD0/D80`, builds the zoom-stage time map (`FUN_141F7CDD0`, `FUN_14089AE20`), and hands the rig to the in-play camera object at `this+0x38` (`FUN_14089D690`, `FUN_14089DB50`, update vslot `+0x10`, fields `+0x2F8`, `+0x340`, `+0x18EC`, `+0x1B10..`). It is the broadcast-camera engine's "camera work" mode; the target actors come from that engine's player table, not from FixDemo humans. Porting it needs that engine (its `this+0x38` class) and the `0x144...` externals, which are not in the image. OPEN; not approximated (a stand-in would not be PES's framing and no frame pair exists to tune it).
Also OPEN: scene actors (GF has no referee or scene humans), and scene end for a scene with no clip camera (`cutscene_finished` is still never called for the foul).

## 7. Left (numbered)

1. Translate `FUN_140474F00` (+ `FUN_140473990`, `FUN_140478FF0`) and feed `PesCutsceneCamera::set_stage_default` from GF's goal (scorer position and the PES rotY), with hook captures or live-poller values as oracle (§3 gives two goal events; goal A needs the scorer position at the goal, not recorded).
2. Port `FUN_141F5CD00`'s mode switch (layer 2) and the scene map (`scene+0x60`) so later steps get -46.69/-49.06 style defaults.
3. Function-number terms (`0x141F31F00`) on the camera record (Function records, type 7, still undecoded).
4. Procedural camera engine (§6), scene actors, scene end rule.
5. Frames (goal and foul pairs), after 1-2.

## Tests run

`pes_fdc_test` 45/45, `pes_fixdemo_select_test` 6/6 (`PES21_ROOT` set), `godot_cutscene_camera_test` (`test_cutscene_camera.gd`) rc 0. `godotfootball` builds. `gen_translated.py --check` not run (no translated-layer file changed). GATE fast: not run (`GATE_TIER` not tried); interim line: `pes_fdc_test` + `pes_fixdemo_select_test` + godot camera test GREEN.
