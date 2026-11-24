# CutLane6: 0x1419CB300 is the slot-2 writer of a PES record class, reached from pes_fn_1423D4A80 at init; wide test still red (scene 0x300DE vs PES 0x300D9)

Branch CutLane, worktree <workdrive>/gf-CutLane. Tip **5b608ffb4** (`git ls-remote origin CutLane` = 5b608ffb4, CONFIRMED; fast-forward from c5937155c, no force). Labels: CONFIRMED = read or run this round; INF = inference; OPEN = blocker.

## 0. Rig requests and ratchet ask (top)

- **Main, ALiveBall on the tip 5b608ffb4 (20 min, `pes_restart_delay_test --gtest_filter='PesStall.ALiveBall*'`).** Not run by me. PesStallQuick is measured (section 5): 53, at the bound.
- **Rig (Main's hooks; hook lines from hooks_for_image, not written here):** hook **0x1419CB300** (slot 2 writer of vtable 0x142A92F88; FIRST 4 EVERY 1, cap 12, FULL; it fires at init) and **0x1423D4A80** (its parent; FIRST 2 EVERY 1, cap 6, FULL). Capture must show: the object at rcx with vptr 0x142A92F88, the output buffer at rdx, the child count at obj+0x18/0x20, and the record the parent walks.

## 1. Scenario and verdict

- Scenario: **none landable this round.** The fixed-block model (M7 candidate) aborts PesStallQuick and ALiveBall at 0x1419CB300 (CutLane5 R2.5; re-confirmed by the survey, section 2). It is stashed, not pushed.
- Pushed tip 5b608ffb4 = Integrate c7fa9d01d merged into CutLane, answer-0 model. Pushed for the integration lane; no behaviour claim beyond Integrate's own numbers (section 5).
- Step 1 (who dispatches 0x1419CB300, which object): **answered, CONFIRMED** (section 2). The layer is unchanged: 0x1419CB300 is not translated.
- Step 2 (re-apply 297071b0 + the +0x1C key, find the scene input): **not landed.** The fixed-block delta is ready (stash, section 6); it needs the 0x1419CB300 subtree in the layer first. The remaining pick input is OPEN (section 4).
- Wide test: **red**, numbers unchanged from CutLane5 R2.4: scene 196830 (0x300DE) vs PES 196825 (0x300D9); T (-38.6197, 0, 30.3613) vs PES (-41.5502, 0, 34); rotY 329.98 vs 0 (line 439).

## 2. Step 1: the dispatcher of 0x1419CB300 (CONFIRMED)

Method: `GF_PES_SURVEY=<file>` on the fixed-block merged tree, `pes_restart_delay_test --gtest_filter=PesStallQuick.*` under simslot (file `gf-CutLane-scratch/survey_r3.txt`, 261 targets, exit 79 expected for a survey). The survey logs each untranslated target with its caller's return offset; `addr2line -f -C` on the test binary names the caller.

- Survey line: `0x1419cb300 1 0 0x497c46e` → caller **pes_fn_1423D4A80** (PES 0x1423D4A80; closure member, translated). Its source (`src/pes/translated/closure/pes_fn_1423D4A80.cpp`) dispatches `rd(gpr0+0x10)` through `pes_dispatch_lookup` at lines 60, 126, 152: it walks child records and calls their slot 2.
- Object: vtable **0x142A92F88** in the flat image's `.rdata`. Slot 2 = 0x142A92F98 = 0x1419CB300. Found by an 8-byte image scan (only qword reference to 0x1419CB300 is 0x142A92F98) and by 15 rip-relative loads of the vtable's slots from code at 0x1419CA71C ... 0x1419CC626 (constructor stores and slot reads; the ctor is not decoded yet).
- Vtable (image bytes; decompiles in `gf-CutLane-scratch/dec_vt_r3.txt`):
  - slot 0 0x1419CAF20: deleting destructor (`_guard_check_icall`, frees when `param_2 & 1`).
  - slot 1 0x1419CB810: `FUN_1419cb840(obj+0x10) + size`; already translated (`closure/pes_fn_1419CB810.cpp`).
  - slot 2 **0x1419CB300**: the writer. Real code (prologue `48 89 5C 24 08 57 48 83 EC 20`), calls 0x1419B7930 and 0x1419B7CD0, writes 0x90 and the child count (obj+0x18/0x20) into the output. Ghidra's body has "bad instruction" halts, so the decompile is partial: the body must be read from disassembly (OPEN).
  - slot 3 0x1419CC260: loader from a blob (offsets relative to the blob).
  - slot 4 0x1419CBBD0: record parser (fields to obj+0xD8; keyed by 4-byte tags at 0x142A9304C / 0x142A93058 / 0x142A9306C).
- Next untranslated target in the survey: **0x1419C9C60**, caller 0x497F9EE = pes_fn_1423D4D10. So the subtree is larger than one slot.
- The other survey targets (e.g. 0x1401B8740 ...) are at frame 0 from a GF-side init site (caller 0x143D18). Not examined.

Verdict on step 1: caller and object CONFIRMED; the 0x1419CB300 body is not yet translated or proven. Whether the layer should take it: yes, once the closure is translated and proven (section 3).

## 3. What "in the layer" needs (not done this round)

1. Disassemble 0x1419CB300 fully (capstone, `.pdata`-anchored) and translate it with its closure (0x1419B7930, 0x1419B7CD0, 0x1419CB840 and the child calls).
2. Prove each by synthetic state (synth.py, at most 4 live) or by the rig capture in section 0. Then the driver record and gen --check.
3. Then 0x1419C9C60 (the next survey target, from 1423D4D10).

## 4. Step 2: the scene-pick input (OPEN; partly decoded)

- Pick 0x1404B6400 (translated: `cutscene/pes_fn_1404B6400.cpp`). Returns the 32-bit field at record+0x28 of a variant record: `psVar8 = FUN_141F4FC30(FixDemoEnv, variant)`; `variant` from `A0+0xBC`: 0 -> 0x30003, 1 -> 0x30002, 2 -> 0x30005, 3 -> 0x30001, 4 -> 0x30004, 5 -> 0x30006, else 0x30009 (decompile `dec_b6400.txt`, lines 60-125 and the switch). BC = 2 on the wide scorer (CutLane5 R2.3), so variant **0x30005**.
- The 0x46xx..0x4Axx tag scan only sets `*param_6`; it does not change the returned scene. So the 0xDE/0xD9 difference lies in the record of variant 0x30005 (category 3, group 4).
- 0x141F4FC30 (`closure/pes_fn_141F4FC30.cpp`) finds the group through 0x140A7CB40 (rows) and the cache map (0x141F4FDA0, ctx+0x30). GF's `Pick()` (`fixdemo_select.cpp:878`) walks the same path through PES's wrapper, so the table is PES's once the cut DB it reads (`db`) matches.
- Also GF builds the tick list at D+0x17860 as `{event, event}` with an empty walk (`fixdemo_select.cpp:498`; "the captured list at seq203 ends before its begin"). PES's pair is {owner+0x228, owner+0x18}, published at Flow+0x17860 by four publishers (0x140421DC4, 0x140422047, 0x14042334B, 0x140423B04; `docs/re/pes-re-sanction-caller.md`). The pick does not read it directly (INF), so it is not the 0xDE/0xD9 input by itself.
- OPEN: which DB field or group index differs from PES. Experiment (read-only, no new capture): dump GF's `db` group rows for category 3, group 4, and compare to PES's rows in the seq203 snapshot (`gf-CutsceneLine-scratch/recompute/seq203`, unpacked by the harness).

## 5. Tests and ratchets (tip 5b608ffb4, answer-0; CONFIRMED runs this round)

- gen --check on 5b608ffb4: **rc 0, 10,999 functions byte-identical** (4,160 recorded bit-exact, 6,839 closure). Log `gf-CutLane-scratch/gen_check_merge.log`. Also on the fixed-block merge tree: 11,007 byte-identical (`gen_check_r3.log`), not pushed.
- pes_goal_anchor_test: 25 pass, 1 fail (WideChannelScorerTakesTheElseArmAndEqualsPes; same as base).
- pes_fixdemo_select_test 8/8; pes_fixdemo_table_test 7/7; pes_fox_world_test 15/15.
- **PesStallQuick: forced_live = 53, PASS** (bound 53; 12,960 of 12,960 ticks; `a0_quick.log`, run in a sim slot under timeout 900).
- PesStall.ALiveBall on the tip: **not run** (section 0, Main's run requested).
- PII: grep of the merge's added lines for `<workdrive>` and `<home>` = 0 (`tools/check_pii.sh` is run from main's checkout by the integration owner; it fails in worktrees on the symlink).
- Gate: not run (agents do not run the fast gate; Main gates).

## 6. Branch state and stash

- Pushed: 5b608ffb4 (merge of origin/Integrate c7fa9d01d into CutLane c5937155c; clean git merge; translate inputs not hand-edited).
- Stash (not pushed): `stash@{0}` "CutLane6 r3 fixed-block+key WIP" = 297071b0 content plus the key at +0x1C, 46 files. Patch copies: `gf-CutLane-scratch/fixedblock_key.diff` (45 files, c5937155c..87f4f928) and `fixedblock_merged_wip.diff` (merged tree). `git apply -R` on the merged results_core.json failed (Integrate changed the same rows), so the stash is the reversal, not a hand-edit.
- Worktree: `extern/godot-cpp` symlink was replaced by an empty dir during the stash and was restored (`ln -s`). Build dir `build/` is Ninja (the Makefile generator fails on the tracked `src/pes/translated/out:_career/` directory; it is tracked on Integrate too).

## 7. Open items (numbered)

1. Hook 0x1419CB300 and 0x1419CB300's parent 0x1423D4A80 (section 0), then translate 0x1419CB300 + closure, prove it, then re-apply the stash and the +0x1C key.
2. The next survey target 0x1419C9C60 (from pes_fn_1423D4D10).
3. Scene input for variant 0x30005 (section 4): compare GF's cut DB group rows with PES's on seq203.
4. ALiveBall on 5b608ffb4 (Main).
5. The build dir and scratch (about one full build of Ninja objects, ccache-backed) stay until the lane's next step; delete on finish per the rules.

## 8. Next exact step

Main: send the two hooks in section 0 at the next rig relaunch. Lane: translate 0x1419CB300's body from disassembly and prove its closure (no rig needed for the synthetic states; the capture is for the record).
