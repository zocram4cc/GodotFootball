# CutsceneLine round 2: the cut-over is on main; goal pid2024691 is kind 2 (not D/F); the recompute replay stops at an encoded import slot; D/F stay blocked

Branch `CutsceneLine` (worktree `<workdrive>/gf-CutsceneLine`). Pushed head `12c3c64a` (`git ls-remote origin CutsceneLine` = `12c3c64a363c0fc8ef08f1ca3597408574279d53`). Commits this round: `293d0e70` (merge of origin/main, which carries CutsceneTiming6's cut-over), `12c3c64a` (full_snapshot: code pages from the image). Earlier: `361baea1`, `6b154632`. Labels: CONFIRMED = decompile, capture read, test or run this session; INF = inference; OPEN = blocker named.

## 0. Answers to Main's questions

1. **Is goal pid2024691 kind 3 (D/F)?** NO. At recompute_471A80 entry (seq203) A0 = rcx = 0x31C53DB8, **A0+0xBC = 2** (CONFIRMED, index.json + snapshot read). D and F hold 0 and 1 (CutAnchor4 §3). BC=2 takes the else arm of `0x140471A80` (BC not in {6, 8}). Scene 0x300D9, side 1, scorer slot 19. At the calc (seq204) radius = 0.0, PES anchor pos (-41.55, 0, 34.0), rotY 0.0. So it is a new kind (call it kind 2), not D/F.
2. **Do these captures unblock D/F (7/7)?** NO. The D/F inputs (`pid132854` seq3783, seq4721) are gone from disk and from every deletion list. Kind 2 gives no D/F case.
3. **Cut-over of the cut list?** ALREADY ON MAIN. CutsceneTiming6 (`a9d1d18b`, an ancestor of origin/main's base) makes GF play the builder's list (`0x140477BC0` on GF's arena) and deletes `PickGoalScene`, `kGoalRule` and `f.cuts = {sceneId}`. My earlier "NOT done" came from the stale CutAnchor6 doc and is WRONG. Verified: `git grep -n 'PickGoalScene\|f\.cuts *=' origin/main -- src godot` finds nothing in src/godot. Remaining `f.cuts` uses are in `tests/pes_goal_anchor_test.cpp` only.
4. **Arm for D/F:** sent to Main: `--when 'recompute_471A80=d(rcx+0xbc)<=1'` (rcx = A0 at entry), cap 12, FULL, armed by cut_anchor_calc_474F00.

## 1. Finding: sider patches in captured code (CONFIRMED)

Main's ruling: `pes_full.bin` code is the image; a capture's code pages can hold other patches. Measured on seq203: **28 of 9524 captured code pages differ from the image.** Three are rendezvous hook pages (recompute 0x140471000, anchor 0x140474000, bcwriter 0x1404B8000; the hook config restores them). The rest are the sider-hosted patches (e.g. 0x1414C0000, 0x141440000, 0x141ea5000).

Fix (commit `12c3c64a`, `tools/dev/translate/full_snapshot.py`): `PeImage.is_code()` and `SnapMem.read()` serve every executable, non-writable image page from the image file, never from the capture. Hook restores still apply on top. Data pages still come from the capture. Tests: `test_image_identity.py` OK, `test_image_reads.py` 9 OK, `test_libm_models.py` OK (1 skip), `test_snapmem_holes.py` 5 OK (1 skip), `test_goal_cast.py` rc 0.

Before the fix the recorded recompute call died in the sider-patched code (`fetch inside sider.dll`). After the fix the recorded call returns cleanly (rax 1, `err null`).

## 2. Recompute else arm (kind 2): replay reaches an encoded import slot (CONFIRMED; not proven)

- Trace recorded (`frame_trace.py recorded seq203 --hooks hooks.cfg.97hooks --out ... --spec-dir scratch/impure-spec`, one call of `0x140471A80`). snapdiff on the core spec set: translated body reached `0x1404B8290` (the switch). Its `mov ebx,7; cmp r8d,ebx; ja` bound is a register, which the translator's jump-table prepass did not resolve (only `cmp reg, imm`). **Fix written, not committed**: patch `<workdrive>/gf-CutsceneLine-scratch/xlate-bound-fix.patch` (26 lines, `tools/dev/translate/xlate.py`); after it the switch resolves. Reverted in the worktree (not on the branch) because `gen_translated --check` drift has not been measured.
- Then the translated body aborts with `PES_DISPATCH_UNTRANSLATED: 0x6ffffec77e00`. Path: `0x140471A80` → `0x14047C500` → `0x141F514D0` (fixdemo_ctx_init, hooked) → `0x141F51630` → `0x141F50B90` → `0x141F50D80` → `0x1408C75E0`, whose `call qword ptr [rip+0x1c68fe2]` reads slot **VA 0x142530664**.
- The slot is **not in `tools/dev/translate/import_slots.json`** (its neighbours 0x1425304B0..0x142530538 are ucrtbase exports). Its value is **0x94A6851000006F6C in the capture and 0xFEC7851000006FFF in the image file**: neither is a module pointer, so the value is encoded at rest. The call would need the protector's decode to reach a target, which the 17-11-26 rule forbids. **OPEN, decision needed from Main:** classify slot 0x142530664 (an import whose name we can recover, or protector-encoded). If it is protector-encoded, the else arm stops here and D/F's recompute cannot be proven in this design.

Consequence for the cut-over and the goal-anchor test: GF runs only the builder (BC=8 default arm). For kind-2 goals PES's T and radius come from the else arm, so GF's answer differs. No red test is added for kind 2 yet: the expected values need the else arm, which is blocked (item 2).

## 3. Tests run this round (head `293d0e70`; the full_snapshot commit touches tools only)

| binary | result |
|---|---|
| pes_camera_list_test | 5/5 PASS |
| pes_goal_anchor_test | 20/20 PASS (D and F not asserted) |
| pes_fixdemo_select_test | 6/6 PASS |
| pes_camera_test | 17/17 PASS |
| pes_cutscene_timeline_test | 7 PASS, 1 SKIP (`CapturedGoalsReplay`, capture gone) |
| pes_goal_scene_hold_test | 3/3 PASS |
| godotfootball (gdext) | builds (rc 0) at `293d0e70` |
| gfsim_run | builds (rc 0) at `293d0e70` |

Mutations: none this round (no GF logic changed; the only GF change is the earlier rename, compile-only).

## 4. Smokes

Last run: at `361baea1` (before the merge of origin/main): switch-off `a0fbe2d5cd445a52` and pes-loop `17941efc16b4a6a5`, both identical to origin/main `1235c3d4`. **Not re-run on `293d0e70`/`12c3c64a`**: the merge brought CutsceneTiming6 (a gf_match change) and the budget ran out. Main gates: please smoke the merged head. The full_snapshot change does not affect gfsim.

## 5. Open items (numbered)

1. **Slot 0x142530664 classification (decision for Main).** See §2. Blocks the kind-2 else arm and so D/F.
2. **Translator bound fix** (`xlate-bound-fix.patch`): commit only after `gen_translated.py --check` shows no unexpected drift, and after the recompute proof.
3. **D/F (kinds 0/1):** needs a goal captured with recompute_471A80 armed on `A0+0xBC <= 1` (sent). Without it D/F stay not asserted.
4. **Goal-anchor test kind-2 case:** pins PES's T and radius from seq204 (calc entry state, radius 0.0). Add when item 1 is decided.
5. **Captures in `live2/pid2024691`:** auto_slim repacked seq203 twice while I read it. Scratch copies live in `gf-CutsceneLine-scratch/recompute/seq203` (unpacked, ~4 GB). Delete them when the proof is recorded.
6. Smokes on the merged head: not run (§4).
