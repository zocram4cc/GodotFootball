# ProcCamera6: the nine unexamined proc_camera captures are one real INIT call (state 1) plus eight updates of a mode 6 record; the engine is proven 15/15 and the closure now builds .impdata code; the init watcher predicate is armed, but no GOAL_CELEBRATE init has landed yet, so `0x140892100` is still out of set and no GF goal step shows PES's camera (01-11-26)

Branch `CutsceneTiming` (worktree `gf-CutsceneTiming`), proof commit `513499b0` (`git ls-remote origin CutsceneTiming` confirmed). Labels: CONFIRMED = run this session; INF = inference; OPEN = blocker named. PES-derived scratch (traces, synthetic states) stays in `gf-ProcCamera6-scratch/`, never committed.

## 0. Status against the ticket

| ticket item | result |
|---|---|
| 1. merge `origin/main`, push | DONE (`3cf5751a`, conflict-free). |
| 2. examine `seq71718..71750`, prove, widen, regenerate, `--check` | DONE. **`calls=15 replayed=15 mismatches=0 own_block=0.4867 (110/226) proven=True`** (the six earlier captures plus the nine). Closure 241 -> **270** (266 files + the 4 allocator natives). Layer **4,837** functions (was 4,815), `gen_translated.py --check` byte-identical, `ledger.py --check` current. |
| 2b. is any capture a state-1 init call, is `0x140892100` covered? | **One is: seq71718 is state 1.** Its record's first zero stage is stage **0**, which is the `.impdata` preset `0x1440558C0`, not `0x140892100`. So `0x140892100` is **NOT** covered (section 2). |
| 3. exact watcher predicate, message Main early | DONE and armed by Main (rvwatch38, 05:48): section 3. |
| 4. init captures land, prove `0x140892100`, rerun the GF goal movie | **NOT ACHIEVED: no state-1 capture with first zero stage != 0 has landed.** The watcher is armed but waits for the next goal's anchor calc (a goal is a rare event in the bot match, last at 02:48). No movie was run (it would hit `0x140892100` at step 0 and show the broadcast camera, as in ProcCamera5): section 5. |
| 5. interim gate line | section 6. |

## 1. What the nine captures are (CONFIRMED, memory of each capture read through `SnapMem`)

`live2/pid3741448/seq71718 .. seq71750`, hook `proc_camera_141F7BFE0`, one camera object (rcx `0x1b35d080`) and one record (`0x1b35cf50`) throughout:

| seq | state `[rcx+0x128]` | note |
|---|---|---|
| 71718 | **1 (init)** | the camera's first call |
| 71722 .. 71750 (8) | 2 (update) | per-frame calls |

Record (284 bytes, `.fdc` camera record): map key 0, **mode `+0xB0` = 6**, target bytes `+0xB1..+0xB3` = (1, 2, 255), rate 90, zoom stages `+0xB8..+0xD4` = `[0, -1, 60, -1, -1, -1, -1, -1]`, first zero stage = **`+0xB8`** (stage 0). Bytes 8..283 equal the records `goal_celebrate_0165.fdc` (map index 13), `goal_celebrate_0171.fdc`, `goal_option_0001.fdc` and `goal_A_flag_boxing00_hugA.fdc` (map index 11), found by comparing against every goal `.fdc` camera record of the install; scenes `GOAL_HUGA_LV3` and `GOAL_A_FLAG_BOXING00` (a hug celebration, **not** one of the 215 GOAL_CELEBRATE scenes of the census; no census record has this mode/zoom/rate). So the goal that produced them was a different celebration branch than the ones GF requests.

## 2. Proof, and the toolchain fix it needed (CONFIRMED)

First `snapdiff --bounded` over the 15 snapshots: 14 of 15 replays bit-exact, **seq71718 failed**: `diff main rc=-6: PES_DISPATCH_UNTRANSLATED: 0x1440558c0`. The init's stage-0 preset (`FUN_141F7BFE0`: `if (record+0xB8 == 0) thunk_EXT_FUN_1440558c0()`) lives in **`.impdata`** (VA `0x143D14000`, the game's relocated code, rwx in the process), which Ghidra shows as an external thunk. `.pdata` has entries for it (`pdata.is_boundary(0x1440558C0)` true, as for `0x144056E40, 0x1440516B0, 0x144052490`, and the layer already holds `.impdata` functions such as `0x14428B510`), but `snapdiff.build_closure` accepted a replay-entered out-of-set callee only inside `CODE_LO..CODE_HI` (`.trace`, `0x140001000..0x14252E800`), so it left the call as the loud abort.

Fix (`tools/dev/translate/snapdiff.py`): `is_code(va)` = the `.trace` range or `.impdata` (`0x143D14000..0x15B952000`, up to `.reloc`), used at the one out-of-set check. Red test first (`test_closure_walk.BuildRange`, fails on the old code with `AttributeError`), green after (`test_closure_walk` 23 tests OK, 1 skipped for no `PES_EXE`). Mutation, run and reverted: `is_code` without the `.impdata` clause fails `BuildRange` (1 failure). After the fix: `calls=15 replayed=15 mismatches=0 ... proven=True`, the new members include `0x1440558C0` (the stage-0 preset) and the mode 6 init's style leaves `0x140891E40, 0x140891E60, 0x140891F40` (`record+0xB1/B2/B3 >= 0` branches), `0x14089AE20`, `0x141F7A970/9C0`, `0x1408924C0`. Files: `closure_proccam.json` (270), `results_proccam.json` (15/15, coverage 110/226), `specgen/sets/closure-proccam.txt`, `universe.json` meta (270), layer regenerated.

**Still out of set in `pes_fn_141F7BFE0.cpp` (so still a loud abort, recovered by `FixDemoCameraRig::Step`'s handler):** the stage 1 / 4 / 5 presets `0x140892100`, `0x140892420`, `0x1408921A0` (plain leaves writing the rcx block from `.rdata` constants `0x1434A02F8..`, `0x1434A0370..`, `0x1434A0398..`; same shape as the proven stage-0 one), the stage 2/3/6/else thunks `0x140892240` (-> `0x1440516B0`), `0x1408922E0` (-> `0x144052490`), `0x140892560` (-> `0x144056E40`), the set-target leaf `0x140891FE0`, the mode 5 update `0x14089D4B0`, and `0x141F7AD90` (-> `0x15B59C060`). Stage 2 (`0x140892660`) is covered (the S_* records of ProcCamera1).

## 3. The watcher predicate (CONFIRMED by evaluation on live2 captures with rv_watch's own environment)

Decompile of `0x141F7BFE0` (bridge): `rcx` = the FixDemoCamera object; `if (*(int*)(rcx+0x128) != 1)` is the per-frame update path, `== 1` is the init (the path that picks the preset); `*(rcx+0x130)` = the `.fdc` camera record; the preset choice reads `record+0xB8, +0xBC, +0xC0, ...` in order (first zero stage wins; `0` -> `.impdata` stage-0 preset, `1` -> `0x140892100`, `2` -> `0x140892660`, `3` -> thunk `0x140892560`, `4` -> `0x140892420`, `5` -> `0x1408921A0`, `6` -> thunk `0x140892240`, else `0x1408922E0`). The scene id is NOT in the camera object (the record identifies the preset; not needed).

`--when 'proc_camera_141F7BFE0=d(rcx+0x128)==1 and d(q(rcx+0x130)+0xb8)!=0'` (init, first zero stage not 0, i.e. not the preset seq71718 already proved). Checked on captures: seq71718 `d(rcx+0x128)==1` True, whole predicate False (stage 0); seq6220 (pid3234533, init of the S_RB record) True/True; seq71722 and seq11590 and seq6224 (updates) False/False. Main armed it as rvwatch38 (05:48, cap 24, armed by `cut_anchor_calc_474F00`, FULL). Captured inits land in `live2/pid3741448/` with seq above 130000; none by 06:53 (the last anchor calc was 02:48:59; the arm resets at a watcher restart, so the hook is released until the next goal).

## 4. GOAL_CELEBRATE census (unchanged by this session)

124 of 125 byte-distinct GOAL_CELEBRATE procedural records still have no capture of any kind; none has an init capture. The 25 classes (mode, first zero stage, target bytes) by size (script output of the ProcCamera5 census `celeb_records.json`): mode 4 stage 1 (34 records, `0x140892100`: `goal_cmnCam_outM00` is of this kind and is what GF's seed-7 goal requests), mode 4 stage 4 (25), mode 6 stage 1 (15 records in 7 target variants), mode 6 stage 4 (10 in 4), mode 4 stages 0/2/3 (10/11/5), mode 5 stages 0/3 (5/2), mode 6 stages 0/2/3/5 (2/3/2/1).

## 5. What a GF goal step shows now (not visually verified; no new frame)

No Godot movie was run. By code (INF for the mode 6 records; CONFIRMED for `outM00`, ProcCamera5's decompile and its frames): GF's goal steps start on a mode 4 stage 1 record (`outM00`, `noSwitch`, `outH10`: `0x140892100` is out of set) or a mode 6 stage 1 record (`outH1x`, `lostGK`: also `0x140892100`), so the engine stops at the first init call and `PesCutsceneCamera` keeps the broadcast camera (the ProcCamera5 behaviour, frames of that run: all broadcast). The stage-0 preset now being in the layer changes nothing visible for those. **No frame shows PES's procedural camera in GF.**

What unblocks it, in order of cost:
1. **Real init captures (armed).** One goal gives 1-3 inits; to cover presets 1, 4, 5 and the `lostGK` mode 6 targets the next goals suffice (cap 24). When one lands: `frame_trace.py recorded` on it, `snapdiff --bounded` over the 15 + the new ones (script `gf-ProcCamera6-scratch/pc6.sh` is that command), copy `sd/gen/141F7BFE0/members.json` minus the 4 allocator natives into `closure_proccam.json`, `closure.py --spec-set proccam --tag proccam --no-specs --members-file ...`, `gen_translated.py`, `--check`, `ledger.py`; then the movie `gf-CutsceneTiming-scratch/ct8/run_gf.sh NAME 3300` (seed 7, teams 1 v 2, `--pes-loop`).
2. **Synthetic init states (not done; OPEN, an option if goals stay rare).** The leaves `0x140892100 / 420 / 1A0 / 240 / 2E0 / 560`, `0x140891FE0` are pure writers of the rcx block; `synth_ctor.py`-style states (rcx = a stack block, as for the constructor proof) would prove each bit-exact against PES's bytes without a goal. I tried the other synthetic route (seq71718's state with the camera's record replaced by an install record, one per class) and it is **blocked**: `slim.py` cannot slim a proc_camera capture (control: an unpatched copy of seq71718 also ends `kept full: ... output_equal=False`, the slim replay returns through an uncaptured page), so each state would cost a 4 GB `mem.bin` (25 classes = 100 GB against the 360 GiB floor). A call-time-delta overlay (add the patched record pages to `deltas.jsonl`/`pool/pages.bin`) would avoid the copies but needs `frame_trace` to read the patched bytes too; not built.

## 6. Tests and gate (CONFIRMED)

- `pes_proc_camera_test`: **5/5 passed** (3 ProcCamera + 2 ProcCameraGodot), rebuilt on the regenerated layer (4,848-step build, 34 min under load).
- `pes_goal_anchor_test`: **20/20 passed**.
- `gen_translated.py --check`: **4,837 functions (3,719 recorded bit-exact, 1,118 closure) regenerate byte-identical**; `ledger.py --check`: LEDGER.md current (38,076 rows).
- `test_closure_walk.py`: 23 OK (1 skipped, no `PES_EXE` for the real-image switch test); mutation as in section 2.
- `snapdiff.py --bounded --only 0x141F7BFE0`: 15/15 bit-exact.

**`GATE fast (interim; tools/dev/gate.sh not run, Main gates): gen_translated.py --check GREEN (4,837 byte-identical); pes_proc_camera_test 5/5; pes_goal_anchor_test 20/20; test_closure_walk 23 OK; snapdiff --bounded 0x141F7BFE0 15/15 bit-exact.`** The sim code was not touched (a closure and tool change only); no smoke run.

## 7. Left (numbered)

1. State-1 captures with first zero stage 1, 4, 5 (and mode 6 target variants): armed (rvwatch38); then the steps of section 5.1, then the GF goal movie and its frames.
2. Prove the remaining out-of-set callees listed in section 2, either from those captures or as synthetic leaf states (section 5.2).
3. The `.impdata` rule now reaches every `snapdiff` closure, bounded or not (`is_code`); no existing proof was re-run for it (the layer `--check` is byte-identical, so no existing function changed).
4. The mode 6 hug celebration (`GOAL_HUGA_LV3`) is not a scene GF requests; why GF never gets it is CutAnchor6's pick inputs.
5. Scratch: `gf-ProcCamera6-scratch/` holds only traces and scripts (about 70 MB; no mem.bin copies remain); delete after Main reads this.

## 8. Addendum (01-11-26, after Main's message: init captures landed)

Three more init captures (predicate `d(rcx+0x128)==1 and d(q(rcx+0x130)+0xb8)!=0`, rvwatch38; `live2/pid3741448/seq134037`, `seq137125`, `seq142542`), all state 1:

| seq | record | first zero stage | preset |
|---|---|---|---|
| 134037 | mode 4, key 0, target (0,255,255), rate 90, zoom `[-1,0,-1,-1,120,-1,-1,-1]` | `+0xBC` (stage 1) | **`0x140892100`** |
| 137125 | mode 6, key 100, target (9,10,255), rate 90, zoom `[-1,-1,-1,240,0,-1,-1,-1]` | `+0xC8` (stage 4) | **`0x140892420`** |
| 142542 | mode 6, key 1, target (1,22,255), rate 60, zoom `[-1,-1,-1,120,0,-1,-1,-1]` | `+0xC8` (stage 4) | `0x140892420` |

(CONFIRMED, read through `SnapMem`.) `frame_trace.py recorded` 3/3 rc 0, then `snapdiff --bounded` over 18 snapshots: **`calls=18 replayed=18 mismatches=0 own_block=0.5929 (134/226) proven=True`**. Closure 270 -> **272** (new members `0x140892100`, `0x140892420`). **The init preset `0x140892100` is proven**, and so is the stage 4 preset. Layer unchanged at 4,837 functions (both were already layer files, now engine members), `gen_translated.py --check` byte-identical, `ledger.py --check` current. Pushed `c4288998`. Still out of set: `0x1408921A0` (stage 5), thunks `0x140892240/2E0/560`, `0x140891FE0`, `0x14089D4B0`, `0x141F7AD90`.

**GF goal movie on the merged tree** (full rebuild, 5,077 steps): the default seed now has no goal in 3,300 frames (fouls only; the merge moved the match; that movie is `not a goal`, deleted). Headless scan of seeds 1-12: goals at seed 7 (frame 814 at 54 fps) and seed 12 (frame 3427), both scene `0x300bf` (`goal_cmnCam_inM00p`). Movie seed 7, teams 1 v 2, `--pes-loop`, `--seed 7`, 1152x648, 30 fps, RADV (`gf-ProcCamera6-scratch/s7`): scene `0x300bf` cut at frame 452, steps start 576 / 940 / 1063, end 1248. The log now shows the engine **getting past the preset** and stopping later:

- step 0 (576): `procedural camera 0 stopped: the engine 0x141F7BFE0 called untranslated 0x140891fe0; the broadcast camera stays` (the zoom-stage ramp's post-ramp hold branch, `FUN_140891fe0(local_1f8, iVar9, piVar15[8])`: reached only when the step's time passes the stage's end, never in a captured call);
- step 1 (940): stopped at `0x140a91a30` (a vec3 x 3x3 matrix leaf; some closure member calls it in a branch no capture ran);
- step 2 (1063): no error.

Frames looked at (`f/sheet.png`: 520, 620, 800, 1000 inside steps 0/1, 1100 and 1200 in step 2/after): **the same high broadcast shot of the penalty box in 520/620/800/1000 and the broadcast kick-off shot in 1100/1200. No frame shows PES's telephoto or the scorer framed.** Visually verified: no PES procedural camera in GF yet. (Step 2's frame is not a PES shot either; whether the engine ran silently there and returned the broadcast pose was not checked.)

Both blockers are leaves already proven standalone (`results_cutscene.json`: `0x140891FE0` and `0x140A91A30`, 64/64 cases bit-exact, coverage 1.00), absent only because no replay entered them. Two ways to close, for Main to pick:
1. **Captures of late updates** of mode 4 / mode 6 records (state 2 calls after the stage ramp end, and the frame that reaches `0x140A91A30`): needs a predicate on the camera's clock; the offset of the elapsed-time field in the FixDemoCamera object is not decoded (set by `0x141F7B4A0`), so I cannot give an exact one yet. Without it, a state-2 predicate `d(rcx+0x128)==2 and d(q(rcx+0x130)+0xb8)!=0` with a high cap catches random update frames (the 12 earlier ones were all early).
2. **Toolchain policy**: let `snapdiff --bounded` build a callee that is already proven bit-exact standalone even when no replay entered it (today it keeps the loud abort). Not done: it changes what "bounded" means, and the proof of the engine would then not cover those call sites.

Gate (interim, Main gates): `gen_translated.py --check` GREEN (4,837 byte-identical); `pes_proc_camera_test` 5/5 and `pes_goal_anchor_test` 20/20 were run on the pre-addendum layer (the closure additions change no layer file, so they were not re-run); `snapdiff --bounded 0x141F7BFE0` 18/18.
