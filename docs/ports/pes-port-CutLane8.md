# CutLane8: 0x1419CB300 proven bit-exact on its real writer calls (M5, pushed 22b6acc58); the fixed-block model (M7) and the +0x1C key with the M4 test are NOT committed; M7 cannot pass as written (two recorded roots' captures are gone)

Branch CutLane, worktree <workdrive>/gf-CutLane, base tip 5b608ffb4. Pushed: `22b6acc58` (`git ls-remote origin CutLane` = 22b6acc58, fast-forward from 5b608ffb4, no force). Labels: CONFIRMED = tool output or read this round; INF = inference; OPEN = blocker.

## 0. Rig requests and ratchet asks (top)

1. **Rig (Main's hooks), needed before M7 can pass.** Two recorded roots that reach the graphics thunk 0x141957C50 have no captures left (CONFIRMED: neither `live2/pid3741448`, `live2/pid3234533`, `live2/pid952648` nor the Dati III archive holds them):
   - 0x141F7BFE0 proc camera (`results_proccam.json`, 18 real calls; `closure_proccam.json`, 13 of its members are direct thunk callers) and its ctor 0x141F7AE60 (5 synthetic states from `pid3234533`).
   - 0x141AF4650 BodyAnimControl::Exec (`closure_animelayer.json`, 4,664 members, the closure includes 0x141957C50 and 0x1423D4A80; 49 synthetic calls over `pid952648` seq3154/3160/3164/3170).
   - Ask: proc_camera_141F7BFE0 `--when 'd(rcx+0x128)==1 and d(q(rcx+0x130)+0xb8)!=0'` (INIT calls, cap 6, armed by cut_anchor_calc_474F00); anime_bones_141EB49B0 for the animelayer root. Hook lines come from `hooks_for_image`.
2. **Ratchet (ALiveBall):** not requested. Nothing that changes GF behaviour landed: the M5 commit is translated-layer output only (4 new functions, reached only from the writer, which GF's path does not call at HEAD because the walker refuses under answer-0; INF). Main may run the 20-min ALiveBall on 22b6acc58 to confirm.

## 1. Scenario and verdict

| item | verdict |
|---|---|
| M5 record: 0x1419CB300 on its real calls | **LANDED on CutLane (22b6acc58).** `calls=2 replayed=2 mismatches=0 own_block 3/3 (1.0)` (CONFIRMED). |
| M7 model (fixed-block 0x141957C50, stash `stash@{0}`) | **NOT COMMITTED.** The rule "every recorded proof reaching the thunk re-run and still bit-exact; else rejected" cannot be met: proccam and animelayer are unverifiable (section 0). Re-run of recompute, tick and the walker: NOT DONE (budget; section 4). |
| M1/M4: the +0x1C key and the LoadSeq203PickState test input | **NOT COMMITTED.** The wide test is green only on top of the model (stash), so it waits for M7. |
| Measurements (goal_anchor, fixdemo_select/table, fox_world, PesStallQuick) | **NOT RUN** (no build or sim this round; the layer changed, so a rebuild of every translated TU is needed). |
| Production follow-ups (ctx facts zeroed after ArrangeGoal; RNG seed from 0x141B34AD0) | **NOT DONE** (no budget left; CutLane7 §8.2 holds the facts). |

## 2. Step 1 (done): 0x1419CB300 on its real calls (CONFIRMED)

- **Root spec**: `closure.py --spec-set trapaction --members-file [0x1419CB300]` wrote `tools/dev/diffharness/specs/trapaction/1419CB300.json`. The spec generator needs no hand completion for the real-call replay (CutLane7's hand-completed cutdb spec was for synthetic states, which are not used here).
- **Captures** (launch `live2/pid2831823`, 24-11 relaunch 2, PES pid 2831823; hooks cdbwalk_1423D4A80 and cdbw_1419CB300, the launch's 4-line `hooks.cfg`, copied to `gf-CutLane-scratch/hooks_pid2831823.cfg`): seq002 and seq003 (the writer's calls, `calls=2` in the watch log), seq001 (the walker's call).
- **Copy rule, learned here**: `cp --sparse=always` turned zero pages into holes, and `SnapMem` refused the case as "page not captured at 0x7ff5fff27000" (CONFIRMED; the replay returned cleanly after `cp --sparse=never`). Copies are deleted; the originals stay in live2.
- **Command**: `PES_EXE=pes_full.bin RV_HOOKS=hooks_pid2831823.cfg prove_bounded.py 0x1419CB300 <work> --tag cdbw --real seq002 --real seq003 --native --merge`.
- **Result (CONFIRMED)**: `0x1419CB300 calls=2 replayed=2 excluded=0 mismatches=0 own_block={reached 3, total 3} proven=True`; `native_compared 2`.
- **Closure** (`closure_cdbw.json`, the functions the replays entered): 0x1415A1708, 0x1419B7930, 0x1419B7940, 0x1419B7CD0, 0x1419B7CE0, 0x1419CB340 (+ the root). **0x1419C9C60 is not entered** by the writer; it is reached from 0x1423D4D10 (the walker's parent path). So the bounded closure does not need it.
- **Walker (seq001)**: its parent 0x1423D4A80 calls the graphics thunk, and under answer-0 (HEAD) the replay refuses the gf-service stub (CONFIRMED: `prove_bounded` exit 1 at the walker's `frame_trace`). With the fixed-block model its trace does call 0x1419CB300 once (CONFIRMED: `calls {1415A1708: 35, 1419CB300: 1}`, rc 0). So the walker's real call is proof evidence only under M7.
- **Universe lesson (CONFIRMED)**: the first layer regen with only the root's universe rows dropped the root (not emitted; the regen wrote nothing for it). Adding the closure's universe rows (`closure.py --no-specs --members-file <prove members.json>`: "2 universe rows added, 5 already in the universe") made the root emitted.

## 3. Commits and pushes

- `22b6acc58` M5 record (pushed, `ls-remote` confirmed): `results_trapaction.json` (+37), `closure_cdbw.json`, `universe.json` (+92), `specs/trapaction/1419CB300.json`, `specgen/sets/closure-cdbw.txt`, the layer: `src/pes/translated/cdbw/pes_fn_1419CB300.cpp` (new), `closure/pes_fn_1419B7930.cpp`, `closure/pes_fn_1419B7940.cpp`, `closure/pes_fn_1419CB340.cpp` (new), `pes_dispatch_gen.cpp`, `translated.hpp`.
- Layer count: **11,003 functions (4,161 recorded bit-exact, 6,842 closure)**, HEAD had 10,999 (4,160, 6,839). The regen held out 0x140928FC0 and printed its reason (a read-only image table read live; `ro_tables.json`), as before.
- **gen --check**: run after the regen, before staging: `4 drift(s)`, and those four are exactly the four new untracked files (CONFIRMED in `gen_record_check.log`); every other layer file was byte-identical. The check was NOT re-run after staging (a second regen+check is ~40 min; the budget rule stopped it). The four files are the regen's own deterministic output, so the check should pass on 22b6acc58; re-run `gen_translated.py --check` before any merge (OPEN, cheap for Main).
- PII: grep of the staged `+` lines for `<workdrive>` and `<home>` = 0 (CONFIRMED); `check_pii.sh` not run (worktree symlink; Main runs it).
- **Stash (not pushed)**: `stash@{0}` "CutLane8 WIP: CutLane6 fixed-block+key stash + M4 test input" = stash@{1} (CutLane6's, untouched) plus the M4 test change. Patches in `gf-CutLane-scratch/`: `wt-tracked-5b608ffb4.patch` (the whole WIP vs 5b608ffb4), `wt-untracked.tar` (its new files), `stash0-CutLane6.patch` (CutLane6's stash alone).

## 4. M7 re-run list (what was and was not done)

| recorded proof reaching the thunk | status |
|---|---|
| 0x140471A80 recompute (`results_core`, `results_snapdiff_recompute`, seq203 of pid2024691) | **NOT RE-RUN** this round. CutLane5 R2.2 (fixed-block) and R2.3 (answer-0) passed 1/1 with `cl5/hooks203.cfg`; the script `gf-CutLane-scratch/cl5/run_fb.sh` re-runs it (its inputs are in `gf-CutsceneLine-scratch`, present). |
| 0x14047B460 tick (stash `results_snapdiff_tick.json`, 1/1 in the stash) | **NOT RE-RUN.** Its closure exists only in the stash (`closure-tick.txt`, 375 members). |
| 0x1423D4A80 walker (seq001 real call) | **NOT RUN.** Only the root 0x1419CB300's walk was shown (above). A `prove_bounded --root 0x1423D4A80 --real seq001` under the model is the run. |
| 0x141F7BFE0 proccam (18 real calls) | **UNVERIFIABLE** (captures gone; section 0). |
| 0x141F7AE60 proccam ctor (5 synthetic) | **UNVERIFIABLE** (its source captures are gone). |
| 0x141AF4650 animelayer (49 synthetic, closure contains the thunk and 0x1423D4A80) | **UNVERIFIABLE** (its source captures are gone; the synthetic states are not in any scratch dir I found). Its record is already stale under HEAD (the thunk is refused there), so the model does not make it worse, but M7 is not met. |
| 0x14091E9F0 (pressx, 6 real calls) | closure has no direct thunk caller (scan of `thunk_closure_image.json`, 1,646 functions, CONFIRMED); indirect reach OPEN. |

## 5. Discrepancies and what the M7 model would change (INF, not measured)

- The model (`fixed_block_gfx`: answer = GF's zeroed `kGfxScratchVa` block) is what makes the walker's real call reach 0x1419CB300. Under answer-0 the walker never calls the writer (its non-null arm is skipped), so the writer's record is independent of the model, but the walker's is not (CONFIRMED by the refusal and the trace).
- The wide test's green state (M4 + the +0x1C key) needs the model's tick path (`ArrangeGoal` runs 0x14047B460 with D and arg6). Nothing of that is in HEAD.

## 6. Open items (numbered)

1. **M7 blocked on captures** (section 0, rig request 1): proccam 0x141F7BFE0 / 0x141F7AE60 and animelayer 0x141AF4650 cannot be re-run. Until then the fixed-block model cannot satisfy M7; the model stays in the stash.
2. Re-run recompute (0x140471A80), tick (0x14047B460) and the walker (0x1423D4A80, seq001) under the model: `gf-CutLane-scratch/cl5/run_fb.sh` for the first two; `prove_bounded` with `--real seq001` for the walker (after `closure.py --spec-set trapaction` for 0x1423D4A80).
3. Stash pop onto 22b6acc58 (conflicts expected only in `universe.json` and generated layer files: resolve with `json_union_merge.py BASE OURS THEIRS`, regenerate). Then: model commit (M7); key + M4 test commit (M1/M4), where the key hunks are `kTimeScaleStadiumKey = 0x1C` in `fixdemo_select.cpp` and `SetStadiumCutKey` in `tests/pes_goal_anchor_test.cpp`.
4. Measurements after the model commit: `pes_goal_anchor_test`, `pes_fixdemo_select_test`, `pes_fixdemo_table_test`, `pes_fox_world_test` (build these targets only, under `flock /tmp/gf-build.lock nice -n 19 cmake --build build --target <t> -j8`; the layer change rebuilds every translated TU, so expect a long first build), PesStallQuick and ALiveBall (Main's 20-min run; `pes_restart_delay_test --gtest_filter='PesStall.ALiveBall*'`).
5. `gen_translated.py --check` on 22b6acc58 (the four new files are untracked at check time; re-run after they are committed, OPEN).
6. Production follow-ups (not started): the ctx facts (stadium 0x52, subject side 1, event position, goal difference -1, event player 1/8) are zeroed after `ArrangeGoal` (CutLane7 §8.2: the event object the recompute reads); the RNG seed from PES's clock 0x141B34AD0 (not decoded).
7. Worktree state: `extern/godot-cpp` is the symlink again (git status errors on it by design); `build/` is the lane's old Ninja tree, untouched this round.
