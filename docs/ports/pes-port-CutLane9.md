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

## Round 2 (Main's IRC: three blockers)

### 0. Hook request for the animelayer root (for Main; INF until a capture confirms it)
- Root 0x141AF4650 = vtable 0x142ABA1A8 slot 4 (+0x20). The class's ctors write the vtable at 0x141AED403 and 0x141AEDBC7 (owners 0x141AED3E0, 0x141AEDBB0). No E8 or E9 calls reach the root; one qword reference (0x142ABA1C8) is the table slot.
- Factory 0x141AEF380 (creates the layer) is called from 0x141EB5250, 0x141F7F002 (owner 0x141F7EFD0), 0x141F29341, 0x1423E9249 and 0x1423E9711.
- Virtual caller found (CONFIRMED by disassembly, the call target being the root is INF): pdata function 0x141F7F500. It reads kind edx (3, 6, 0xE, 0x12), walks `rbx=[rcx+0x50]; rbx=[rbx+0x118]; rbx=[rbx+idx*8+0xB0]` with idx 8 / 7 / 6 / 5 for kind 3 / 6 / 0xE / 0x12, and calls `[rax+0x20]` with rcx=rbx at 0x141F7F579 (a second site, 0x141F7F5F0, is in fn 0x141F7F5C7).
- **Requested hook:** `fn_141F7F500`, VA 0x141F7F500, entry, FIRST 1 EVERY 20, cap 12, `--when '(rdx&0xffffffff) in (3,6,14,18) and q(q(rcx+0x50)+0x118)!=0'`, FULL. Offline filter: keep the captures whose element `q(q(q(rcx+0x50)+0x118)+idx*8+0xB0)` has vtable qword 0x142ABA1A8. That check is ours, not the rig's.
- 0x141AF4650 itself stays unhookable (prologue). The proc camera capture (0x141F7BFE0) is still needed for the proc-camera record.

### 1. The sider.dll fetch was my harness bug (CONFIRMED)
- sider.dll patched 28 code sites in rwxp pages of the live PES image, e.g. 0x1414C0913 `push rax; movabs rax,0x6F6C90D2ABEC; call rax; pop rax` (also 0x140C53DEB, 0x141ED9CB0, 0x141F51855 and others). The image bytes at those VAs are the original code.
- `full_snapshot.SnapMem.read` already serves exec, non-writable image sections from the image (is_code). SnapMem takes the image from the env var PES_EXE (full_snapshot.py:292), not from an argument. My prove.sh passed `--image` to snapdiff only, and snapdiff's SnapMem (snapdiff.py:337) and frame_trace's (frame_trace.py:689) never got PES_EXE. So code fetched from the capture, and the sider stub was reached.
- Proof: the same replay with PES_EXE exported runs to completion (no sider refusal). Probe output: m7/dbg/snapfb2.log.
- M6 fix (not landed): snapdiff must pass args.image into fs.SnapMem (snapdiff.py:337) and frame_trace must take the image from its args, not the env. Until then every harness run must export PES_EXE.

### 2. proc_ctor 0x141F7AE60: no closure gap (CONFIRMED)
- The `PES_DISPATCH_UNTRANSLATED: 0x14159f8b0` abort came from `--native` (my prove.sh hard-coded it on snapdiff). Without --native, proc_ctor's six real calls are bit-exact on answer-0 and on fixed-block: 6/6, 0 mismatches, own-block 7/8.
- 0x14159F8B0 = a spin-lock wrapper (import call at +0x10, Sleep loop via 0x14159F974). The native-dispatch gap for this indirect target is still open (INF: the native dispatch table lacks it); it does not affect the plain proof.
- proc_ctor's closure does not reach the thunk, so it is an M5 record either way. Merge needs prove_bounded --merge into results_proccam; that step and the regen are not done (next step).

### 3. Re-proof table (CONFIRMED; m7/run_all2.out, both models, PES_EXE set)
| root | captures | answer-0 (HEAD) | fixed-block (stash model) |
|---|---|---|---|
| recompute 0x140471A80 | pid2024691 seq203 (real, 1 call) | excluded: gfx thunk reached (HEAD refuses) | **proven 1/1, 0 mismatches, own 54/138** |
| tick 0x14047B460 | seq203 synthetic (tick_calls.py) | excluded: gfx thunk reached | **proven 1/1, 0 mismatches, own 34/82** |
| walker 0x1423D4A80 | pid2831823 seq001 (real; --native) | excluded: gfx thunk reached | **proven 1/1, 0 mismatches, own 10/18** |
| proc_ctor 0x141F7AE60 | pid3148289 seq001-006 (real, 6 calls) | **proven 6/6, 0 mismatches, own 7/8** | **proven 6/6, 0 mismatches, own 7/8** |
| animelayer root 0x141AF4650 | pid3148289 anime_bones seq757-772 (real) | 0 root calls | 0 root calls |
| proc camera 0x141F7BFE0 | none (no goal capture yet) | not run | not run |
- The answer-0 "excluded" rows are HEAD's gf-service refusal of the thunk: the HEAD records of recompute and tick (CutLane5) are stale and do not reproduce at HEAD. They cannot be used as the answer-0 baseline here.
- No counterexample found: every fixed-block proof that ran is bit-exact.

### 4. M7 decision: NOT MET, model NOT committed, answer-0 stays
- M7 requires every recorded proof reaching the thunk 0x141957C50 to be re-run under the model and stay bit-exact. Status:
  - recompute 0x140471A80: fixed-block 1/1 ✓
  - tick 0x14047B460 (stash record): fixed-block 1/1 ✓
  - walker 0x1423D4A80 (in the animelayer closure): fixed-block 1/1 ✓
  - proc camera 0x141F7BFE0 (results_proccam; its closure reaches 14 affected functions): ✗ no captures; re-run when the goal captures land.
  - animelayer family (21 records in results_animelayer.json, all with closure_animelayer.json, which contains 0x1423D4A80): ✗ the root 0x141AF4650 has no real call in the anime_bones captures; the synthetic states are gone. Re-run when the 0x141F7F500 hook captures land.
- Per the M7 rule ("else rejected") the model is rejected for now. Nothing is committed: no model, no +0x1C key, no M4 test input (they depend on the model). The brief's "commit the model" condition is not met.
- Pending: (a) Main's proc camera goal captures → rerun 0x141F7BFE0 under both models; (b) hook 0x141F7F500 captures → test the 21 animelayer records' root via the filter in section 0 (note: the records need a root-call proof, not the walker-only one); (c) M5 merge of proc_ctor (real calls) once the merge path is run.

### 5. State
- Worktree <workdrive>/gf-CutLane: HEAD 22b6acc58, clean (checked with the symlink swapped), stashes untouched, godot-cpp symlink restored.
- Scratch: <workdrive>/gf-CutLane-scratch/m7 (run_all2.sh, prove2.sh, res.json per root under a0/ and fb/). Full-memory copies deleted after the report (~53 GB).
- Live captures: read-only hardlinks/copies of relaunch 4 (pid3148289) and pid2024691/seq203, pid2831823/seq001; originals in live2 untouched.
