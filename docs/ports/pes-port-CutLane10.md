# CutLane10: M5 (proc_ctor native seam) and M7 (gfx fixed-block) re-proof: NOT LANDED (in progress at report time)

Branch CutLane, worktree <workdrive>/gf-CutLane, HEAD e2ff7f8c8 (M6). Nothing committed or pushed by this lane.
Uncommitted WIP in the worktree (tests build and pass; the translated layer is NOT regenerated):
- tools/dev/translate/stubs.json: native_services entry 0x14159F8B0 op spin_lock, with the decode.
- tools/dev/translate/stubs.py: spin_lock added to KNOWN_NATIVE_OPS.
- src/pes/translated/pes_native.hpp, pes_native.cpp: pes_native_spin_lock (free word 0 -> -1; held word aborts).
- tools/dev/translate/xlate.py: a DIRECT call to a native service VA is emitted as a dispatch-table call (as a computed call already is), not as pes_dispatch_missing. NATIVE_TARGETS from stubs.natives(). Only affects calls to native VAs.
- tests/pilot/pes_native_heap_test.cpp: NativeSpinLock.TakesAFreeWordAndWritesNothingBeside, NativeSpinLockDeathTest.AHeldWordAbortsInASingleThread.
Test run: build/tests/pilot/pes_native_heap_test, 12 tests, 12 passed (the two new ones included). Mutation check not yet run (planned: write 0 instead of -1 in the native; expect TakesAFreeWord to fail).
Not run: gen_translated.py regen, gen --check, closure, goal_anchor, fixdemo, fox_world, PesStallQuick, ALiveBall. Gate not run (Main gates).

## M5 (proc_ctor 0x141F7AE60): blocked, not proven under --native

Ruling (Main): add 0x14159F8B0 to the native dispatch, re-prove under --native on pid3148289 seq001-006, merge, regen, gen --check.

Done:
- Decode of 0x14159F8B0 (Ghidra FUN_14159f8b0 via dec.sh; bytes via capstone): enter the global CS 0x143706758 (call [0x14252F340]); if [lock]==0 -> [lock] = -1 and leave the CS by tail jump (lea rcx,0x143706758; jmp [0x14252F6E7]); if [lock]==-1 -> FUN_14159f974(100) (Sleep-style, never ends with one thread); other value -> owner id at TLS+0x84, halt. Imports are packer-encoded (no names).
- Native added (above). Captures: scratch cap/proc_ctor_141F7AE60/seq001-006 hardlinked to live2/pid3148289 and unpacked (mem.bin copies deleted after use; disk back to 322G free).
- First --native proof: 6/6 "PES_DISPATCH_UNTRANSLATED: 0x14159f8b0". Cause (CONFIRMED): pes_fn_1403FB240 calls 0x14159F8B0 DIRECTLY, and xlate emitted pes_dispatch_missing for direct out-of-set calls. Fixed in xlate.py (above).
- Second --native proof: 6/6 "PES_DISPATCH_UNTRANSLATED: 0x1403fb8a0" (CONFIRMED, res.json in scratch m7/m5_ctor).
  Cause (INF, from the translated 1403FB240): the lock's free path leaves [0x1436EF2B0] == -1; 1403FB240 then takes its `!nz` block, which calls 0x1403FB8A0 (guarded-static/pool init), then 0x14159F6C0 and 0x14159F850. None of the three is in closure_proccam_ctor.json (80 members). The plain (non-native) proof was 6/6 bit-exact, so the plain harness never reached this block; I did not establish why (INF: the plain harness's stub for the packer's LeaveCS leaves the word unchanged or differs in rax; not checked).
- Next step (not done): decide whether the lock native must also set rax / reproduce the packer's LeaveCS result, or whether 1403FB8A0 and its callees join the closure (closure.py --members-file, then regen). Either way the proc_ctor record is NOT merged and results_proccam.json is unchanged.

## M7 (fixed-block model): NOT MET at report time; animelayer re-proof waiting on captures

Model state: stash@{0} (CutLane8 WIP) still holds the fixed-block model, the +0x1C key and the M4 test input. Not applied, not committed.

Captures (Main's ruling, relaunch 6):
- lyrupd_141F3BDC0 in pid134759 (relaunch 6, PES started 04:44). First 12 captures (seq001-012, 04:52) were PRE-MATCH (in-play byte 0). They used the whole global cap. Requested Main to add in-play and a class predicate; Main re-armed the watcher at 04:58 with in-play (`b(q(q(0x1436F3DC8)+0x270)+0x116C)==1`), cap 40, EVERY 20. Main declined the vtable clause (it tests the wrong object, see below). Captures after 04:58 had not arrived by 04:55 (watcher released them: disk under 300 GiB, now 322 GiB free).
- Offline filter (scratch m7/lyr_filter.py). Corrected chain (Main, CONFIRMED against the 12 pre-match captures): r14 = q(q(rcx+0xc70)+(q(rdx+8)&0xffff)*8) (the record object; vtable 0x142BB1048, the ScriptBit-class layer, in all 12), L = q(q(q(r14+0x50)+0x118)+idx*8+0xB0) with idx from the dispatcher's edx (the idx-to-kind map of 0x141F7F500 is 3->8, 6->7, 0xE->6, 0x12->5; the lyrupd edx is not established).
- Observed L vtables, pre-match (12 captures): vt(idx5..8) = 0x142ABF5E0, 0x142ABF690, 0x142ABF798, 0x142ABF798. None is 0x142ABA1A8 (the animelayer class: slot 4 = root 0x141AF4650). Pre-match, so no animelayer L is expected yet.
- Requested: no animelayer-class L yet. If the in-play captures show none in 12, the report must give the observed L vtables at the top (as Main ruled). Not yet available.

Re-proof of the 21 animelayer records under both models: NOT RUN (no in-play capture). The earlier table (CutLane9, scratch m7/run_all2.out) stands: walker 1/1, recompute 1/1, tick 1/1 bit-exact under fixed-block; proc_ctor was blocked (above); animelayer root 0x141AF4650 had 0 root calls on the anime_bones captures.

## Open items (numbered)
1. M5: resolve the 0x1403FB8A0 gap (closure or lock-native rax/LeaveCS semantics), then --native proof, --merge, regen, gen --check, one commit (M5+M6).
2. M7: in-play lyrupd captures (watcher re-armed 04:58, cap 40): run lyr_filter.py on new dirs; if L vtable 0x142ABA1A8 appears, re-prove 0x141AF4650's real root calls under both models (prove.sh a0|fb with hooks_pid134759.cfg = copy of current hooks.cfg, scratch hooks_pid134759.cfg).
3. Mutation check for spin_lock (native writes 0 instead of -1) not yet run.
4. Stash@{0} model: apply only after 1 and 2 pass; then +0x1C key and LoadSeq203PickState, goal_anchor (26/26 expected), fixdemo_select/table, fox_world, PesStallQuick, ALiveBall (simslot).

## Scratch and state
- Scratch: <workdrive>/gf-CutLane-scratch/m7 (43 GB incl. proof builds; lyr/ = hardlinks into live2/pid134759, no extra space; cap/proc_ctor_141F7AE60 zst hardlinks only).
- Stashes untouched: stash@{0} CutLane8 WIP, stash@{1} CutLane6 r3.
- Live captures not modified (hardlinks and copies only). Hooks unchanged; Main owns the watcher.
- Final tip SHA: e2ff7f8c8 (unchanged). No push.

## Status at 05:06 (last update)
- PES relaunched 05:01:38 as pid 254401 (watcher restarted; new launch folder live2/pid254401). The lyrupd hook there is `EVERY 500` (hooks.cfg, copy in scratch hooks_pid254401.cfg), so the in-play captures come slowly. No lyrupd capture had landed in pid254401 by 05:06.
- Watcher cap for lyrupd is global (12 spent in pid134759, pre-match); Main's new predicate is in play, cap 40.
- Not done: M5 proof (blocked as above), M7 animelayer re-proof (no in-play capture yet), no commit, no push.
- Next: run <workdrive>/gf-CutLane-scratch/m7/lyr_filter.py on pid254401 seqNNN dirs with lyrupd (hardlink call.bin, index.json, mem.bin into scratch first); if an L with vtable 0x142ABA1A8 appears, re-prove 0x141AF4650 under both models with hooks_pid254401.cfg.

## Update 05:30 (final for this run)
- Lock word at proc_ctor entry (CONFIRMED, read from pid3148289 seq001-006 mem.bin, 0x1436EF2B0): 0x80000002 in all six. Not 0 and not -1.
- Decode of 0x14159F8B0 for that value (CONFIRMED by the asm at 14159F8DC-14159F904): word != 0 and != -1 takes the "other" path: writes the u32 at [0x14350B358] into TLS slot +0x84 (thread block gs:[0x58] + __tls_index*8, index at 0x1437067A0) and tail-jumps to the packer primitive via [0x14252F6E7]. The word is NOT modified. It does not abort and does not spin.
- So pes_native_spin_lock as written is WRONG: it dies on any non-zero word (the "held" abort). The real wrapper only spins on -1. After the real call the word stays 0x80000002, so 0x1403FB240's `cmp [0x1436EF2B0], -1` test is false and the `!nz` block (0x1403FB8A0, 0x14159F6C0, 0x14159F850) is NOT taken on PES. That block is the translated-run's own out-of-set path; it is reached in the native run only because the replayed word reads 0 (my Die did not fire), not because PES takes it. Why the replay sees 0 at that address: NOT established (INF: the replay's writable-page source for 0x1436EF2B0 differs from the capture's mapping; not checked).
- Decision point (not acted on): fix the native to the decode (0 -> -1; -1 -> abort; other -> owner write at TLS+0x84, word unchanged), then establish the replay's word source before re-running --native. Until then the M5 proof is NOT valid.
- Mutation / test status: pes_native_heap_test 12/12 with the current (wrong for non-zero, non-(-1)) native. The "held word aborts" test pins the wrong behaviour and must change with the fix.
- lyrupd: no in-play capture yet. The watcher released all captures from 04:53 to 05:25 (disk under 300 GiB). pid254401 (relaunch 7, EVERY 500) has none. Scratch cleaned: my hardlinks removed; m7 is 28 MB. The 43 GB of pre-match lyrupd captures sit in live2/pid134759 (Main to decide).
- Worktree: HEAD e2ff7f8c8, uncommitted WIP as listed above, nothing committed or pushed. stash@{0} and stash@{1} untouched.
- Open items: (1) decide the spin-lock semantics and fix the native and its test; (2) establish why the replay reads 0 at 0x1436EF2B0; (3) M5 proof, merge, regen, gen --check; (4) M7: in-play lyrupd captures (disk permitting), then the 21 animelayer re-proofs under both models; (5) the stash@{0} model, +0x1C key, M4 input, and the sim checks, only after 3 and 4.

## Final status (07:50, budget used)

### M5 (proc_ctor 0x141F7AE60): PROVEN under --native, merged, regen pending
- `PesHeap::Attach` shadowed PES's whole page 0x1436EF000 with GF's zeroed page, so PES's packer spin word 0x1436EF2B0 (0x80000002 at every proc_ctor entry, CONFIRMED from pid3148289 mem.bin) read 0 in the native replay. That is why the native run took the `!nz` block and hit 0x1403FB8A0. Fix (pes_native.cpp): Attach copies PES's bytes into every page word GF does not model (`CopyPesGlobals`).
- `pes_native_spin_lock` now follows the decode: word 0 -> -1; word -1 -> abort (a wait the single thread cannot end); any other word -> owner id (u32 at [0x14350B358]) written to the thread block +0x84, word unchanged.
- xlate.py: a direct call to a native VA goes to the dispatch table, and this check now comes before the pilot check (a native must win over a pilot member).
- gen_translated.py: native VAs are excluded from the closure walk (`NATIVE_VAS`), so a native is never translated as a closure member.
- Result (CONFIRMED): `prove_bounded 0x141F7AE60 --native --merge` on pid3148289 seq001-006 gives calls=6, replayed=6, mismatches=0, proven=True, own_block 7/8. Merged into results_proccam.json (record 0x141F7AE60 now 6 real calls; closure_proccam_ctor.json).
- 0x1403FB8A0 / 0x14159F6C0 / 0x14159F850: NOT in the proven path (the block is not taken on PES's real entry state). They are not translated; pes_fn_1403FB240 keeps a loud `OUT-OF-SET CALL` abort for them. The closure gap for those three is OPEN (no real call reaches them; closing it needs a capture that reaches the block, or a closure walk through them). Main's rule (translate closure members) is not met for these three; flagged.
- Mutations (run, rebuilt, reverted): (a) remove CopyPesGlobals -> NativeHeapPage.AttachCopiesPesWordsAndKeepsTheModelledSlots FAILS; (b) lock takes word as 0x7FFFFFFF -> NativeSpinLock.TakesAFreeWordAndWritesNothingBeside FAILS. Reverted: pes_native_heap_test 14/14 PASS.
- Regen: NOT YET VERIFIED. Two regen attempts failed rc=1 before the native-ordering fix (gen_dispatch clash for 0x1403FBC10 / 0x14159F8B0 / 0x141B34CB0; then "0x1403FB240 calls 14159F8B0, not committed"). Fixed both (NATIVE_VAS filter; native-before-pilot in xlate). Regen + `--check` chain running in scratch m7/regen4.log and m7/gencheck4.log at report time; result unknown. Gate and goal_anchor etc. not run.

### M7 (fixed-block model): NOT MET. Animelayer re-proof NOT RUN.
- No in-play lyrupd capture in any relaunch: relaunch 6 (pid134759) 12 pre-match captures (L vtable 0x142BB1048 record object; idx5..8 L vtables 0x142ABF5E0/0x142ABF690/0x142ABF798/0x142ABF798; none 0x142ABA1A8); relaunch 7 (pid254401) and relaunch 8 (pid861979, 06:45, in play only, EVERY 500, cap 40): zero lyrupd captures. Watcher released every capture from 04:53 onward with "disk under 300 GiB" (disk fell back below the floor), and the relaunch 8 folder has no captures at all.
- So the 21 animelayer re-proofs under both models are not run. Stash@{0} model not applied, +0x1C key and LoadSeq203PickState not committed.
- Per Main's ruling, if no animelayer L appears in 12 in-play captures, report the observed L vtables at top: NOT YET AVAILABLE (zero in-play captures).

### State
- Worktree HEAD e2ff7f8c8. Uncommitted: stubs.json (spin_lock native), stubs.py (op), pes_native.hpp/.cpp (spin_lock, CopyPesGlobals), pes_native_heap_test.cpp (4 tests), xlate.py (native direct call; native before pilot), gen_translated.py (NATIVE_VAS), results_proccam.json and closure_proccam_ctor.json (M5 merge). Nothing committed or pushed.
- Next: (1) read regen4/gencheck4 logs; if GREEN, commit M5+M6 (one commit; gate fast by Main); (2) M7: needs disk above 300 GiB and in-play lyrupd captures (Main), then filter with m7/lyr_filter.py and re-prove 0x141AF4650 under both models; (3) close the 1403FB8A0/14159F6C0/14159F850 closure gap with a capture reaching that block or a Main decision.

## Commit and push (08:55)
- Pushed CutLane: bf1dbd397 (M5+M6 as one commit; `git ls-remote origin CutLane` confirms bf1dbd397). Previous tip e2ff7f8c8.
- Regen (gen_translated.py): REGEN_RC=0, 11002 functions (4161 recorded bit-exact, 6841 closure). gen --check: CHECK_RC=0, byte-identical.
- pes_native_heap_test: 14/14 PASS after regen. Mutations (run, rebuilt, reverted): drop CopyPesGlobals -> FAIL; lock takes 0x7FFFFFFF -> FAIL.
- tools/check_pii.sh (run from <workdrive>/GodotFootball): exit 0. No gate or smoke run (Main gates).
- Closure coverage (Main ruling 1): 0x1403FB8A0, 0x14159F6C0, 0x14159F850 are on no real PES path in the 6 proc_ctor calls; they remain loud OUT-OF-SET aborts in pes_fn_1403FB240. No synthetic states.
- Deleted from the translated layer: pes_fn_14159F8B0.cpp (native now; it was a closure member).

## M7 status at 08:55 (top-line for Main)
- No animelayer L found: ZERO in-play lyrupd_141F3BDC0 captures in any launch. Relaunch 6 (pid134759): 12 pre-match (L vtables 0x142ABF5E0/0x142ABF690/0x142ABF798/0x142ABF798; record object 0x142BB1048; none 0x142ABA1A8). Relaunch 7 (pid254401) and relaunch 8 (pid861979): zero lyrupd captures. Relaunch 9 (pid1257475): 16 seq dirs, zero lyrupd.
- Cause observed: the watcher releases every capture with "disk under 300 GiB" (log 04:53 onward, and again 08:50, 297 GiB free). So the in-play lyrupd hook has never snapshotted since the predicate change.
- Not done: the 21 animelayer re-proofs under both models; the scan of a capture's heap for 0x142ABA1A8 owners (requested by Main only if 12+ in-play captures show no animelayer L, which has not been reached). stash@{0} model, +0x1C key, M4 input not applied or committed.
- Next (needs disk > 300 GiB on the watcher): run `m7/lyr_filter.py lyr/seqNNN` on the first in-play lyrupd captures (hardlink call.bin, index.json, mem.bin.zst from live2/pid1257475, then capz unpack); if an L with 0x142ABA1A8 appears, re-prove 0x141AF4650 under both models.
