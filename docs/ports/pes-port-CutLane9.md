# CutLane9: M7 re-proof of the gfx fixed-block model (result: NOT MET, nothing landed)

Branch CutLane, tip 22b6acc58 (unchanged, nothing committed or pushed). Worktree restored to HEAD; godot-cpp symlink back.
Scenario claimed: M7 (model/service ruling). Verdict: the model cannot land. Answer-0 stays.

## What ran

Proof driver: scratch `m7/prove.sh` (mirrors prove_bounded.py: frame_trace recorded + snapdiff --bounded --native, no merge).
Captures: hard links into live2 for relaunch 4 (`pid3148289`, RV_HOOKS = `gf-CutLane-scratch/hooks_pid3148289.cfg`, a copy of
PES21/rendezvous/hooks.cfg taken 02:30); unpacked copies for `pid2024691/seq203` (hooks `cl5/hooks203.cfg`) and
`pid2831823/seq001` (hooks `hooks_pid2831823.cfg`). Originals in live2 never written.
Model for "fb": the six model files checked out from stash@{0} for the run only (stubs.json, stubs.py, xlate.py, run.py,
pes_stubs.hpp, pes_state.hpp), then restored to HEAD. Model files were confirmed present (6 files changed, then reverted).

## Results (tool output, scratch m7/a0.out, m7/fb.out)

| Root | Captures | answer-0 (HEAD) | fixed-block (M7) | Verdict |
|---|---|---|---|---|
| 0x1423D4A80 walker | pid2831823 seq001 (real, 1 call) | excluded: GF service stub_141957C50 reached | **proven 1/1, 0 mismatches, own_block 10/18** | bit-exact under fb |
| 0x140471A80 recompute | pid2024691 seq203 (real, 1 call) | excluded: stub reached (frame_trace: no return within 50M instructions) | excluded: `fetch inside sider.dll at 0x6f6c90d2abec, not an export entry` | not proven |
| 0x14047B460 tick | seq203 synthetic (tick_calls.py) | excluded: stub reached | excluded: same sider.dll fetch | not proven |
| 0x141F7AE60 proc_ctor | pid3148289 seq001-006 (real, 6 calls) | 6/6 mismatch: native abort `PES_DISPATCH_UNTRANSLATED: 0x14159f8b0` | 6/6 mismatch, same abort | blocked by closure gap, model-independent |
| 0x141AF4650 animelayer root | pid3148289 anime_bones seq757-772 (6 real) | 0 root calls | 0 root calls | cannot be re-proven from these captures |
| 0x141F7BFE0 proc camera | none | not captured | not captured | pending (no goal in pid3148289 yet) |

Old records not reproduced: proc_ctor (5/5 synthetic in results_proccam.json), animelayer (49 synthetic calls, states gone).

## Findings (CONFIRMED = tool output above; INF = inference)

1. **Walker under fixed-block is bit-exact on its real call** (CONFIRMED, 1/1). This is the one re-proof that passes.
2. **Recompute and tick under fixed-block are blocked by a rendezvous artefact, not a result**: the replay fetches into
   sider.dll at 0x6f6c90d2abec (the rig's DLL, not an export). CutLane5's round-2 pass used the 0x17B0000 trampolines of an
   earlier launch; the same class of fault again, from this snapshot's hook state (INF: hooks203.cfg does not restore a
   sider trampoline reached by the replay). Not a counterexample to the model. Needs a hook restore for pid2024691 or a
   fresh recompute capture (the goal-armed hooks on pid3148289 are the place).
3. **proc_ctor is not affected by the model** (its closure, closure_proccam_ctor.json, does not contain any function whose
   emitted code changes: the 12 closure files of the stash regen are 14195B820/BAD0/BBE0/BCB0/D660, 1419F6FF0/F7570,
   141A46890, 1423D3AA0/D43D0/D4A80/D4D10). Its real-call failure is a closure gap: 0x14159F8B0 is an indirect callee
   the bounded closure never translated. Fix: add 0x14159F8B0 (and its callees) to the closure via closure.py
   --members-file, regen, re-run both models.
4. **animelayer root 0x141AF4650 is not called on the anime_bones route** (CONFIRMED: traces show 0x141EB49B0 calling
   0x141AF3AF0 and 0x141AF3CA0, never 0x141AF4650). The root's real call sites are unknown (0x141AF4650 is unhookable).
   closure_animelayer.json (4,664 members) contains the walker 0x1423D4A80 and 0x1423D4D10, so all 22 records in
   results_animelayer.json reach the model; none can be re-run.
5. **Proc camera 0x141F7BFE0** (results_proccam.json, closure_proccam.json, 268 members) contains 10 affected functions
   (14195B820 ... 1423D43D0). No capture exists yet.
6. Affected set (CONFIRMED from the stash regen diff): 12 closure files (listed in 3). Direct caller list in stubs.json
   (1423D4A80 only) is incomplete; the thunk is reached through the 0x14195Bxxx family.

## Why nothing landed

M7 requires every recorded proof reaching the thunk to re-run bit-exact under the model. Not met:
- recompute, tick: not proven under fb (rendezvous fetch);
- proc camera: no capture;
- animelayer family (22 records): cannot be re-run;
- proc_ctor: real calls not provable on HEAD either (closure gap).
So per the M7 rule the model is rejected for now; answer-0 stands. The +0x1C key and the M4 test input (stash@{0}) depend on the
model and were not committed. No gate, goal_anchor, fixdemo, fox_world or PesStallQuick run (nothing to measure); ALiveBall not
requested.

## Open items (numbered)

1. Recompute/tick under fb: find the sider.dll fetch site (0x6f6c90d2abec) and restore the live hook for pid2024691, or take
   fresh recompute captures on pid3148289 (goal-armed).
2. proc camera 0x141F7BFE0: when pid3148289 produces a goal capture (cut_anchor_calc arms it), hardlink it and run
   `m7/prove.sh a0|fb 0x141F7BFE0 specs/proccam hooks_pid3148289.cfg ...`.
3. proc_ctor: close the closure gap at 0x14159F8B0 (closure.py --members-file, translate, record), then re-run both models.
4. animelayer root: ask Main for a hook on the real caller of 0x141AF4650 (the route is 0x141EB49B0 -> 0x141AF3AF0/0x141AF3CA0;
   the caller of 0x141AF4650 is not in these captures). Without it the 22 records stay unprovable under either model.
5. Synthetic states of the animelayer proof are gone (no scratch dir has them); those 49 calls cannot be re-proven.

## Scratch and state

- Scratch: <workdrive>/gf-CutLane-scratch/m7 (run scripts, res.json and logs per root; memory copies deleted; 6.7 MB).
- Worktree: <workdrive>/gf-CutLane at 22b6acc58, clean (no stash applied, no commit, no push). Walker spec and tick spec
  generated/restored for this run were removed.
- Stashes untouched: stash@{0} CutLane8 WIP, stash@{1} CutLane6 r3.
- Rig: not touched beyond reading watch.log and the capture folders.
