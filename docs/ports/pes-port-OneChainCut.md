# OneChainCut: AnimeLayer7 one-chain cut, status (17-11-26)

Branch OneWorld-series-wip, pushed. `git ls-remote`: 4d6eff7d53892ba977b583d96b9614b5ea28405d (head). Commits: ee5d956e (regenerated translated layer), 4d6eff7d (the cut and its tests). Worktree <workdrive>/gf-AnimeLayer, clean after the push.

## Not finished: the cut cannot run the pes-loop path yet
- pes-loop smoke aborts on the first trap tick: `PES_DISPATCH_UNTRANSLATED: 0x1415a1720`, rc 134.
- Cause (CONFIRMED by gdb backtrace and the translated body): per tick, the driver FUN_141EB5E70 calls the chain runner FUN_141AFC4A0 (chain vtable +0x38). The runner's translated body has an out-of-set call `/*OUT-OF-SET CALL 0x1415A1720*/` (a `jmp [0x14252FD98]` thunk: VCRUNTIME140!memset, zeroing a 0x8010-byte stack buffer), then a second out-of-set call `0x1403EFDF0` (ledger: "undecoded", no decompile).
- The branch is taken in REAL captured ticks (CONFIRMED, pid3778393, 19 anime_tick captures, read with capread.py): A = [P+0xA08] = 0x2CA7CB90; A+0x5C = 0; A+0x50 = 0x1 or 0x101; L1 = [A+0x88]; L1+0x40 = 0. Those are the guard values for the memset path. The proof harness did not show this (INF: the proof replayed the closure with the abort unreached in its corpus).
- The pes-loop smoke therefore cannot run until: (a) the memset thunk is modelled (import_stubs.json already has the memset model; the translator emits OUT-OF-SET for a direct call to a `jmp [slot]` thunk that is not in the translated set); and (b) 0x1403EFDF0 is decoded and translated (or its callee shown to be unreached on the path). Changing the translator and regenerating the layer runs the diff harness per function; not done in this session.
- Hook for the runner entry was refused by hooks_for_image (stack-probe call at 0x141AFC4A7 inside the window). Main was asked; the existing captures were used instead.

## Step 1 (done): engine-chain calls run over the PES memory
- PesWorld::BindMemory keeps `memory_` (PesMemory*) and writes G's global (kWorldGlobalVa = kWorldWindowVa) into the image. The write moved from Match::BindPesTrapWorld (matchevents.cpp) into BindMemory.
- New PesWorld::MemoryCall(): memory_->NewCall() plus the window as a service (PesMem::Window{kWorldWindowVa, kWorldBytes, nullptr, Bytes()}). Used by BindAnimeChain, WriteAnimeG, WriteAnimeRootPoint, RunAnimeJoints, JointPosition, RunOnP, RunRootMotion, ReadAdjusterFrame, RunAdjusters. Throws without BindMemory.
- Other NewCall(Bytes()) trap paths are unchanged (outside the handover list).

## Test fixes found while driving the adjuster test (each with a reason)
1. ConstructActorP (pestrapactor.cpp) now stores the three constructor stores FUN_1406E03D0 makes first: P's own vtable at P+0 (0x1425E34D8, lea at 0x1406E0408, new Site kOwnVtableSite, PesActorConstants::ownVtable) and the interface vtables at P+0xAB8 / P+0xAC0. Reason: the driver reads [P] and calls its +0x18 (CONFIRMED: translated pes_fn_141EB5E70 line 41-44); without it the dispatch gets 0.
2. ArmYaw passes rdx = P (CONFIRMED: FUN_141EB35A0 reads [rdx+0xAB8]). Without it the callee read address 0.
3. The test binds the install's image tables (LoadPesImageTables -> BindImageTables) and the member vtable slots (AddImageSegment), as Match does (match.cpp 341-344). Reason: ArmYaw's state is NewCall(Bytes()) with no flat image; the vtable slots come from the tables only.
4. PeImage::Open + LoadActorConstants + ConstructActorP on kSlot in the test, as Match does (match.cpp 299-301).

## Tests (this session, the test binaries the cut touches)
- pes_adjuster_tick_test: 3 of 4 pass (WindowChainIsTheEnginePlayersBodyAnimControl, GWritesLandInTheEnginesB, TheManPositionIsTheEnginesRootPoint). The 4th (OneDriverCallWritesTheYawThatTheAdjusterHolds) aborts in the runner (see above): the whole binary exits 134, so no summary. Not weakened.
- pes_anime_engine_test: 14 passed, 1 skipped (render plots, unrelated).
- pes_fox_template_test: 3 of 3 passed.
- Not ported (the cut breaks their compile; not built or run): tests/pes_joint_block_test.cpp, tests/pes_kick_oracle_common.hpp (affects pes_ball_list_test, pes_kick_chooser_oracle_test, pes_kick_header_oracle_test, pes_kick_root_test), tests/pes_trap_oracle_test.cpp. Ported state not done. Step 2's port is open.

## Smokes (seed 7, 2 min, PES_EXE = pes_full.bin, via tools/dev/simslot.sh)
- switch-off: `[determinism] c2016bf08fd0bc98` = baseline c2016bf08fd0bc98. Unchanged, as expected (the trap path is pes-loop only). rc 0, 29807 sim ticks to full time.
- pes-loop: aborts at the first trap tick (PES_DISPATCH_UNTRANSLATED 0x1415a1720), rc 134. No hash. Baseline da3b1eff7174bc8b not reproduced; the move is the abort, not a number.

## Step 5 (not done): stand-in chain data
- BuildAnimeChain, kAnimeChainVa segments, kAnimeLayer/KeyTable/Quat VAs still in pestrapdata.cpp (BuildTrapLoaderData) and the loader. Not deleted: the trap path no longer reads them per the handover, but the tests still reference them, and the cut does not compile those tests. Left for the next session.

## Open items (numbered)
1. Model the memset thunk 0x1415A1720 in the translator (direct call to a `jmp [0x14252FD98]` import thunk runs the memset model), regenerate, and show the runner's branch on the real states (captures above) with the model.
2. Decode and translate 0x1403EFDF0 (undecoded), or prove the path it sits on is unreached on the captured states (none of the 19 captures had the path proven unreached: all 19 satisfy the guard).
3. Port pes_joint_block_test, pes_kick_oracle_common.hpp (and its 4 kick tests), pes_trap_oracle_test.
4. Delete the stand-in chain loader data (step 5) once the tests above are ported.
5. Re-run the pes-loop smoke and record its hash against da3b1eff7174bc8b.
6. Named gap kept: height adjuster P+0x498 (never running in 19 captures); chain X gate S+0x14 = 256 in all captures (open).
7. Worktree note: `git status` and the commit tool refuse the godot-cpp symlink ("expected submodule path not to be a symbolic link"). Path-limited `git add -- <paths>` and `git commit` work. Commits were made that way.

## Evidence
- Logs: <workdrive>/gf-AnimeLayer-scratch/adj_step1f.log, adj_dbg2.log (guard values), smoke_pesloop_s1.log, smoke_off_s1.log, final_*.log, build_*.log.
- Capture reads: <workdrive>/gf-Rendezvous-scratch/live2/pid3778393/seq061..seq1191 (anime_tick), reader <workdrive>/gf-ChainX-scratch/capread.py.

## Round 2 (17-11, Main's three items): result

Branch OneWorld-series-wip, pushed. `git ls-remote`: 3066f71c587d1f2b76cf3174bc82209eb86ef50d (head). Commits this round: edf2d4f0 (translated layer regenerated, closure_adjtick.json), 3066f71c (TEB TLS, memory stack for trap calls, half-angle yaw).

### 1. Thunk 0x1415A1720 -> memset (CONFIRMED)
- The thunk is `jmp [0x14252FD98]`. Live slot value in capture pid3778393/seq061: 0x6FE9E29719A0. The VCRUNTIME140 module header is at 0x6FE9E2970000, so the slot is base + 0x19A0 (memset's RVA in import_stubs.json).
- Routing: the closure mechanism that already carries 0x1415A1720 in closure_animelayer.json. The thunk is now a member of closure_adjtick.json, so the regenerated body is the import_stubs memset model (a byte loop returning the destination). The classification is the import_stubs.json memset entry (runtime-library code, the .impdata ruling); no new convention. The runner's direct CALL links it as pes_fn_1415A1720.

### 2. 0x1403EFDF0 and the guarded branch (CONFIRMED, with a corrected guard)
- 0x1403EFDF0: scope constructor (vtable 0x14258BCA8). Links the object into the TLS block's +0x38 list; cursor bump-allocator over (rdx, rdx+r8). Pop: 0x1403EFEB0 (JMP target of 0x1403EFF50), which calls 0x1403F0030 (walks the overflow list) and frees overflow with 0x1403CF770 (not reached: the list is empty).
- Guard, corrected: the earlier check read [A+0x88]+0x40. The code reads [[A+0x88]+0x10]+0x40 = 0x903000 in seq061, seq1128, seq1168, seq1191: bit 10 clear, so the memset path runs on every captured tick. Note the replay trace (proof_adjB.log) lacks 0x1403EFDF0 entries; that is a trace-coverage gap, not a skipped path.
- TLS: gs:[0x58] is the thread's TLS array; the exe's index is DAT_1437067A0 (0 in the image and in seq061); the block's +0x38 is 0 between ticks (CONFIRMED seq061). PesMemory now writes that array into the TEB window.
- Chain +0x10 step: vtable 0x142ABA1A8 slot +0x10 = 0x141AF4480 (the memset path dispatches to it). Its +0x30 target = 0x141AEEE50. Both added to closure_adjtick.json; generated with no out-of-set calls.
- Named gap: 0x141AFC4A0 still calls 0x141AEF890 (undecoded, 2478 bytes, cutscene) when [result of 0x141AE8950] + 0x350 is nonzero. Not read on the captured path: the replay (proof_adjB) shows no entry. The value is not checked against the 19 captures yet. The 6 height captures in pid848145 (Main) are not run.
- Proof on captures: NOT run. The adjuster test runs the real driver on a real match-built P/A (fresh boot, not the capture's memory). snapdiff bounded replay on the captures was not done this round.

### 3. Regeneration, tests, smokes
- Regenerated with gen_translated (12 min per run; 10056 functions: 3817 recorded bit-exact, 6239 closure). Two runs: one for the memset/scope members, one for 0x141AF4480 / 0x141AEEE50.
- pes_adjuster_tick_test: 4/4 PASS (yaw case included). Found on the way: (a) the yaw read was half the angle: B+0xA4/B+0xAC are sin/cos of half the yaw (handover values: P+0x4F0 = 3.5822, sin(1.7911) = 0.97583); fixed to 2*atan2; (b) the trap calls ran on the window's 8 KB stack, which the runner's 0x8010-byte scratch frame overruns into the trap images (slot 21's P+0 was zeroed in the match). Trap calls over the memory now take their frame from the memory's stack (MemoryFrame).
- pes_anime_engine_test: 14 PASS, 1 skipped (render plots). pes_fox_template_test: 3/3 PASS.
- Switch-off smoke (seed 7, 2 min): [determinism] c2016bf08fd0bc98 = baseline; 29807 ticks to full time, rc 0. UNCHANGED (the trap path is pes-loop only).
- Pes-loop smoke (seed 7, 2 min): NO HASH. Round 2 timed out at the 900 s slot limit (rc 124) after "Done creating match" and one stalemate line. A 6-second (0.1 min) run also exceeded 240 s. Not a crash: the earlier abort (PES_DISPATCH_UNTRANSLATED 0x1415A1720) is gone. Diagnosis (INF, from timing and the write log): per guarded call the memset model writes 0x8010 bytes one guest byte at a time through PesMem::wfind (about 70k logged writes per driver call, 22 slots per tick). Speed, not a hang, is the blocker. gdb could not sample the stall (Yama ptrace_scope blocks attach; a SIGINT under batch gdb kills gdb). Fix to do next: a bulk memset in the translator's memset import model (one service/window lookup per contiguous span, byte logging kept for the harness), then regenerate.

### 4. Handover steps status
- Step 2 (ports: pes_joint_block_test, pes_kick_oracle_common.hpp, pes_trap_oracle_test): NOT done.
- Step 4 (ctest over the touched binaries + smokes): partially done (above).
- Step 5 (delete stand-in chain data: BuildAnimeChain, kAnimeChainVa segments, kAnimeLayer/KeyTable/Quat VAs): NOT done.

### 5. Open items (numbered)
1. Bulk memset in the translator's memset model (performance), regenerate, then rerun the pes-loop smoke seed 7 to a hash vs da3b1eff7174bc8b.
2. Settle 0x141AEF890's guard: read [0x141AE8950's rax] + 0x350 on the 19 + 6 captures (or add the callee to the proof when a capture shows it nonzero).
3. snapdiff bounded replay of 0x141AFC4A0 on the anime_tick captures (pid3778393 19; pid848145 6 height) to make the branch bit-exact proof, not just a run-to-completion.
4. Port the three test files and delete the stand-in chain data (handover steps 2 and 5).
5. Chain X: in play, key 302 -> index 0x40 (Main); named gap of the cut, no stand-in.
6. Worktree: `git status`/`git add` need `-c status.submodule=none` (godot-cpp symlink).

## Round 3 (17-11, Main's items): result

Branch OneWorld-series-wip, pushed. `git ls-remote`: 4ac3c3893f0701eb54c1d184202414d95adc87dd (head). Round 3 commit: 4ac3c389 (bulk memset).

### 1. Bulk memset (DONE, output proven unchanged)
- `PesMem::WriteSpan` + `PesMem::wfill` (src/pes/translated/pes_state.hpp): one region lookup per contiguous span. The span is the region `wfind(addr,1)` picks, cut at the next region start above addr (every region start is a boundary), so no byte in a span would resolve to a different region. The byte log (`wlog`) is written per byte exactly as `wr(addr+i,c,1)` writes it; a byte no region takes is dropped as before. Snapshot mode keeps the per-byte path.
- `PesState::fill` wraps it. `xlate.py`: both memset models (the CALL/JMP import model at ~645 and the CALL/JMP `memset` model at ~515) now emit `s.fill`. The memcpy/memmove loops (lines 501-513) are NOT changed (outside this item; they still loop per byte).
- Regeneration (`gen_translated`, 10056 functions): the layer diff is exactly one body, `pes_fn_1415A1720` (the memset thunk), plus pes_state.hpp. Checked with `git diff` on the layer: 60 changed lines, all in those two files (the other memset sites are reached only through this thunk). Not run as `--check` (it is the regeneration itself).
- Byte-identical proof: a temporary dump of `ext_.writeLog` for the three driver calls of `OneDriverCallWritesTheYawThatTheAdjusterHolds` (slot 3): 189,967 writes before (binary from commit 3066f71c layer) and after (4ac3c389 layer): `cmp` identical. The temporary dump was removed before commit.
- `test_memset_model.py`: 6/6 OK (it tests the unicorn model from import_stubs.json, which is unchanged).
- Tests after the change: pes_adjuster_tick_test 4/4, pes_anime_engine_test 14 passed (1 skipped, render plots), pes_fox_template_test 3/3.
- Build note: pes_state.hpp is included by every translated TU, so the change rebuilt the whole layer (about 20 min under the lock).

### 2. Smokes (seed 7, 2 min, PES_EXE = pes_full.bin)
- Pes-loop: `[determinism] e46129415a2bdc1b`, 49001 ticks to full time, rc 0. Baseline `da3b1eff7174bc8b`: MOVED. Run took 15 min.
  - Why it moved (CONFIRMED by the lineage, INF on the exact causes): the baseline predates the one-chain cut. The trap path now runs PES's own driver FUN_141EB5E70 on the engine chain (round 1), reads the yaw half-angle correctly (round 2), uses the memory stack for trap calls (round 2: the window's 8 KB stack overran into the trap images and zeroed slot 21's P), and runs the runner's guarded memset branch (round 2). The bulk fill (this round) does NOT move the output: its write record is byte-identical, so it cannot be the cause.
  - Caveat: no rerun for the same-hash determinism check (one more 15 min run). The determinism line is the smoke's own hash.
  - Not done: the earlier round-2 pes-loop timeouts are gone (the run completes in 15 min); the bulk fill did not make the pes-loop faster by itself (not measured separately).
- Switch-off: `[determinism] c2016bf08fd0bc98`, 29807 ticks, rc 0. UNCHANGED (baseline c2016bf08fd0bc98).

### 3. The 0x141AE8950 guard (DONE, on the 25 captures)
- 0x141AE8950 is `return *(u64*)(&DAT_1437cea30 + (param_1 & 0xffffffff) * 8)` (Ghidra decompile, CONFIRMED). Its argument is the dword at [A+0x60] (rcx = A on the runner's entry; the runner's `s.gpr[3] = s.gpr[1]` after the 0x8070 store). The guard tests the qword at [ret+0x350]; nonzero calls 0x141AEF890.
- Read (script <workdrive>/gf-AnimeLayer-scratch/guard_read.py, capread.py on the packed captures):
  - pid3778393: 19 anime_tick captures (seq061..075, seq1128..1191): P = 0x3af71500, A = 0x2ca7cb90, idx = 0, entry = 0x1fe0e90, guard qword [entry+0x350] = 0x0.
  - pid848145 seq8934..8939 (6 height-adjuster captures): P = 0x37c03990, A = 0x227ec810, idx = 0, entry = 0x1fe0e90, guard = 0x0.
  - All 25: guard zero, so 0x141AEF890 is not called on any captured tick. CONFIRMED on these 25 (read from the capture at tick entry; the guard's inputs are not written by the memset/scope path before the call: they touch the stack and the TLS block).
- Named gap: other states (idx != 0 or a nonzero [entry+0x350]) are not observed. The layer keeps its loud abort for 0x141AEF890 on those.

### 4. Not done (budget)
- Snapshot-replay proof of the runner on the captures: not done.
- Test ports (pes_joint_block_test, pes_kick_oracle_common.hpp + four kick tests, pes_trap_oracle_test): not done.
- Step 5 (delete stand-in chain loader data): not done.

### Open items (numbered)
1. Snapshot-replay (snapdiff --bounded) of 0x141AFC4A0 on the 25 captures, to make the runner bit-exact on real calls.
2. Test ports (handover step 2) and stand-in deletion (step 5).
3. Same-hash determinism rerun of the pes-loop smoke (15 min).
4. The memcpy/memmove per-byte loops in the translator (lines 501-513) could take the same span fill; not changed here.
5. Chain X (key 302 -> index 0x40 in play, Main): named gap of the cut.

## Round 4 (17-11): result

Branch OneWorld-series-wip, pushed. `git ls-remote`: 7a88d07e6a3c460b80788b5991a07ac8bc22078f (head). Commits: 31f5c77f (engine fixture, joint and adjuster ports), 7a88d07e (step 5 + kick ports).

### 1. Determinism (DONE)
- Pes-loop rerun on 4ac3c389, seed 7, 2 min: `[determinism] e46129415a2bdc1b`, 49001 ticks, rc 0. Repeats round 3 exactly.
- Move from da3b1eff7174bc8b (INF, not measured): the first divergence is not located. I did not record which tick or man first differs from the baseline, so the paragraph below is code-level reasoning, not a traced divergence. The trap path changed in order: PES's driver now runs on the engine chain (round 1), the yaw is read as the half angle (round 2), trap calls use the memory stack (round 2), and the runner's guarded branch runs (round 2). The bulk fill (round 3) is output-identical by the write-record proof. A trace of the first differing (tick, slot) against the baseline binary is still open.

### 2. Snapshot-replay proof of the runner (NOT DONE)
- Budget ran out before the snapdiff run. The guard inputs on all 25 captures are recorded (round 3). The height branch (pid848145 seq8934-8939) is in the same 25.
- Open: snapdiff --bounded on 0x141AFC4A0 and the FUN_141EB5E70 proof row (height branch added).

### 3. Test ports
- DONE: pes_joint_block_test (2/2 PASS) and pes_adjuster_tick_test (4/4 PASS) run on the engine chain via tests/pes_engine_trap.hpp (shared fixture). Joint pose is written into the engine chain's own arrays (L1+0x50 positions, A+0x20 rotations).
- DONE (compile + skip-clean, NOT verified against PES): pes_kick_oracle_common.hpp (Build takes the engine: BindWorld, kicker chain = engine player 0; SharedEngine), pes_kick_chooser_oracle_test, pes_kick_header_oracle_test, pes_kick_root_test (renamed TheEngineChainGivesPesRootMotion...). pes_ball_list_test builds and passes 2/2 (it does not use Build).
  - Blocker found: every PES oracle set on disk (TrapOpen/oracle, KickKind/oracle2, TrapS5, StoppageFlow) has mem.bin symlinked into deleted capture folders (pid615629, pid1467004 seq019). Kick tests skip without GF_PES_KICK_ORACLE. Main is capturing kick_chooser_8364D0, kick_header_833CD0 and kick_root_141AFDF10 (in play).
- NOT DONE: pes_trap_oracle_test. It compares writes against the window's PhysiqueA/B, O1 and O2 (CapturedObjects/WindowObjects, Classify, ReferenceWrites), and those objects now live in the engine's heap. Porting it needs the object spans redesigned (A, B and O2 read through the engine chain; O1 has no engine counterpart in the cut). Main reports the trap oracle inputs exist (trapopen_14079A470 x72, traptick_140797CD0 x14). The test's env (GF_PES_TRAP_ORACLE) expects per-capture oracle directories made by tools/dev/translate/trap_oracle.py; not generated this round.

### 4. Step 5 (DONE)
- Deleted src/sim/onthepitch/pesanimechain.cpp (BuildAnimeChain, InitAnimeJointPose, the stand-in chain builder). Removed PesAnimeChain and its AddTo, the chain objects (kAnimeChainVa, kAnimeLayerVa, kAnimeKeyTableVa, kAnimePosArrayVa/QuatArrayVa and offsets, kAnimeKeyTableOffset, kPhysiqueAnimeOffset), the loader's animeChain (pestrapdata.hpp/cpp), and GfAnimeChain. Kept: the joint layout, PesJointPose, the root-motion frame, kClipDatabaseGlobalVa, kAnimeLayerOffset (used by the anime engine test).
- Verified: gfsim_run, pes_joint_block_test, pes_adjuster_tick_test, pes_anime_engine_test (14 passed, 1 skipped), pes_fox_template_test (3/3), pes_ball_list_test (2/2) all build and pass; kick tests build and skip.

### Open items
1. Snapdiff replay of the runner on the 25 captures (round-4 item 2), plus the height branch in the FUN_141EB5E70 proof row.
2. pes_trap_oracle_test port: redesign the object spans around the engine chain; generate oracle dirs from the Main trap captures (trap_oracle.py); record the set in the test doc.
3. Verify the kick tests on Main's new captures (kick_chooser_8364D0, kick_header_833CD0, kick_root_141AFDF10).
4. First-divergence trace of pes-loop against da3b1eff7174bc8b (which tick and man first differ, and why).

## Round 5 (17-11): result

Branch OneWorld-series-wip, pushed. `git ls-remote`: e29bcd15fe7e2f96d03c09972a7322a3f8df435f (head). Commit this round: e29bcd15 (set notes in the chooser and header tests; no behaviour change).

### 3. Snapshot-replay proof of the runner and the adjuster (DONE, bit-exact)
- Six real anime_tick captures (copies of pid3778393 seq061, seq062, seq1128, seq1171 and pid848145 seq8934, seq8935; unpacked copies in scratch, now packed again). Trace: frame_trace.py recorded with hooks PES21/rendezvous/hooks.cfg and spec dir diffharness/specs/kick. Replay: snapdiff.py --bounded.
- FUN_141AFC4A0 (the runner, spec kick/141AFC4A0.json): calls 6, replayed 6, mismatches 0, own-block 0.5897 (recorded, not gated). Result file: scratch pb_runner2/res.json.
- FUN_141EB5E70 (the driver, spec kick/141EB5E70.json): calls 6, replayed 6, mismatches 0, own-block 7/7 (1.0). Result: pb_runner2/res_adj.json.
- Height branch (the row Main asked for): read from the unpacked capture memory (P+0x498): height state 2 running in pid848145 seq8934 (count 2, delay 15, ticks 30) and seq8935 (count 4). The other four captures have height state 0. So the height branch is covered by two of the six replayed calls, bit-exact.
- Guard: 0x141AE8950 returns *(u64*)(&DAT_1437cea30 + idx*8), idx = dword [A+0x60] = 0 in all 25 captures; [ret+0x350] = 0 in all 25, so 0x141AEF890 is not called. The replay does not enter it (members list has no 0x141AEF890). Named gap: other states.
- prove_bounded.py was NOT used: its SPEC_DIR is trapaction, and these roots' specs live in kick/ (no trapaction spec for them). The equivalent chain (frame_trace + snapdiff --spec-dir kick) was run directly. No closure file was merged.

### 1. Kick oracles and tests (PARTIAL: chooser and header bit-exact; root not runnable)
- Chain built per capture (scratch kick_r5/chooser_chain.sh, gen_chain.sh): copy of the packed capture, unpack, frame_trace.py recorded (hooks PES21/rendezvous/hooks.cfg, spec kickkind for 1408364D0, kick for 140833CD0, kickroot for 141AFDF10), snapdiff.py --bounded for the root, trap_oracle.py for manifest.tsv, then assembled as the oracle dir (corpus.jsonl, manifest.tsv, maps.txt, mem.bin, ok_calls.bin, ok_delta.bin, pages.bin, teb.txt).
- Chooser (1408364D0), set pid1152106 seq299, seq421, seq152: pes_kick_chooser_oracle_test 1/1 PASS on the engine chain. Descriptor bit-exact on every reference byte. No differing case.
- Header (140833CD0), set pid1152106 seq011, seq013, seq027: pes_kick_header_oracle_test 2/2 PASS (request bytes and chooser answer). No differing case.
- Root (141AFDF10), set pid1152106 seq003, seq004, seq005: snapdiff proves the reader bit-exact (1 call each), but trap_oracle.py fails on all three: "P+0x3B18 is not a whole-cm height / 180". That check is the kind-0 trap layout; the reader's call has rcx = B and rdx = 0, so it is not a P-based oracle. Also pes_kick_root_test needs rootmotion.tsv, from rootrd.py, which is not in the repo (it lived in the deleted gf-KickMotion-scratch). So the root test is not runnable. To finish: redesign the root oracle around the reader's own arguments (the reference output is in the snapdiff corpus rows), or regenerate it from the chooser runs that enter the reader.
- The other nine kept captures of each set are not run.

### 2. pes_trap_oracle_test on engine memory (NOT DONE)
- Not started. Its object model (PhysiqueA/B, O1, O2 window objects; Classify, ReferenceWrites, CapturedObjects) needs redesign onto the engine's A/B/O2 (B = A+0xF0, O2 = A+0x188; O1 has no engine counterpart in the cut). The trapopen_14079A470 and traptick_140797CD0 sets are not generated yet. trap_oracle.py was built for this kind-0 trap, so the generator path is known; the test redesign is the cost.

### 4. First diverging tick/man vs da3b1eff7174bc8b (NOT DONE; the baseline is not on this line)
- da3b1eff7174bc8b was recorded at commit 4791bbbe ("OneWorld cut 4 (2)"), which lives on the OneWorld-cut4 branch, not on this branch. Merge-base with HEAD: efa4410e. The baseline has 5 commits this branch lacks, including WritePesTrapActors calling PesWorld::BuildDemoInfoRef (a second trap-path change). This branch has 27 commits after efa4410e (the cut, the memset, the fixtures, the TLS and memory-stack fixes).
- So the 49001 vs baseline difference is not attributable to the one-chain cut alone. A first-divergence trace would need: a worktree at 4791bbbe, a debug-only per-tick dump of the 22 player positions in Match's tick (identical text in both trees, not committed), two traced 2-min smokes, and a diff. Estimated: about one hour of builds plus two 15-minute smokes.

### Open items
1. Root oracle: redesign around the reader's arguments, or regenerate from chooser runs that enter the reader; rootmotion.tsv from snapdiff corpus rows.
2. Trap oracle: object-model port to engine A/B/O2, generate the trapopen/traptick oracle dirs, then run.
3. First-divergence trace (needs the cut4 baseline; see above).
4. Run the other nine kept captures for chooser and header.

## Round 6 (17-11): result

Branch OneWorld-series-wip, pushed. `git ls-remote`: 88ea37bd0e37921ff9743ab4c9e4bc6cba75fc27 (head). Commit this round: 88ea37bd (rootrd.py and the root test). The first divergence work used a debug-only dump that was removed from the repo (see 1).

### 1. First divergence, 4791bbbe vs 16234a42 (DONE)
- Method: a debug-only dump after match.Process() in apps/gfsim_run.cpp, env GF_TICK_DUMP=path, one line per man per tick: tick, team, player id, then GetGeomPosition() x, y, z as hex floats. Built in both trees (baseline worktree <workdrive>/gf-AnimeLayer-base at 4791bbbe, kept for reproduction; current at 16234a42). Each run: pes-loop, seed 7, 2 min, PES_EXE = pes_full.bin.
- Both hashes reproduce with the dump on: current e46129415a2bdc1b (49001 ticks), baseline da3b1eff7174bc8b (29193 ticks). So the dump has no effect on the sim.
- Result (CONFIRMED on the dumps): tick 0 identical for all 32 entries. First difference at tick 1: 16 men differ on x and 20 on y, max 3.81e-06, mean 1.9e-06. Growth: tick 2 max 4.45e-05; tick 5 2.96e-03; tick 10 1.55e-02; tick 20 0.217 (all 22 on-pitch men differ on x and y, one on z); tick 60 0.323.
- The first difference is one float ulp: the float spacing at x = 38 m is 2^-18 = 3.81e-06, equal to the max difference at tick 1. So the first divergence is single-ulp rounding of a root displacement computed from a 38 m absolute coordinate, not a behavioural change.
- Cause (INF, not yet traced in code): the displacement source changed in round 1. The baseline's StepAdjusters computed the strike displacement from the adjuster accumulator (A+0x14 minus the root point O2+0xD0); the cut's ReadAdjusterFrame takes it from PES's own step B+0x90/0x98 minus O2+0xD0. The two absolute bases round differently at 38 m. Growth after tick 2 is the usual amplification through the men's own dynamics (positions feed the next tick's decisions).
- PES-faithful? INF. The cut reads PES's step values (its own float arithmetic), so it is the closer reproduction of PES's displacement. Not verified against a live PES position for the same tick: that needs a capture of the step at tick 1 and the man's position, which the rig did not hold this round.
- Named cause to check next: the yaw half-angle (round 2) changes heading from tick 1 when the yaw adjuster is running; the 0.2 m spread by tick 20 may include that. Not separated.
- Dump removed from the repo (kept only as this report and the scratch files tick_cur.txt, tick_base.txt).

### 2. Kick root oracle (DONE, bit-exact)
- tools/dev/translate/rootrd.py (new): rootmotion.tsv rows from a capture call (motion at [rsp+0x28], time bits from xmm3) and the snapdiff case's PES bytes at the output pointer.
- Finding (CONFIRMED): the real calls (rdx set, r8 = 0) write their four-float vector at rdx; GF's RunRootMotion reads the vector at r8 with rdx = 0. These are two different output pointers of FUN_1423A3FD0, and the real call writes a different vector at rdx (e.g. (-0.000521, 0, -0.0105, 0) for motion 3621), not the identity. So the real-call rows cannot be compared with GF's r8 vector directly. The oracle rows therefore use the r8 convention: the real capture's state with rdx = 0 and r8 = rsp+0x100 (mapped), replayed by PES's own bytes (snapdiff, kickroot spec).
- Set pid1152106 seq003, seq004, seq005 (kick_root_141AFDF10), real states (motions 3621, 3621, 4194 at 1/54 s) and, for seq003, six synthetic states (motion 100 at 0.25, 0.5, 1.0 s; 3983 at 1/3 s; 2447 and 3621 at 0.5 s). Synthetic states by synth.py (static spec, scratch; the spec is not committed because its output pointer is per capture).
- Replay: snapdiff bounded, 9 of 9 calls bit-exact (own-block 13/13 = 1.0). A first attempt put two states with the same name in one snapdiff run and reported one mismatch; that was a label collision (both captures' states named real_m3621), not a translation error. Renamed; 0 mismatches.
- pes_kick_root_test: rewritten on the engine chain (the reader's inputs are the chain B and the call's own motion and time; no manifest, no Build). 1/1 PASS on the 9 rows (3 real identity rows, 6 synthetic including 3 turning rows).
- Not compared: the real calls' rdx-convention output (the vector at rdx); GF's RunRootMotion returns only the r8 vector.

### 3. pes_trap_oracle_test on engine memory (NOT DONE)
- Trap sets found: trapopen_14079A470 in pid2286718 (24 full), pid2523621 (8 full), pid2571173 (24 packed), pid3727195 (16 packed); traptick_140797CD0 in pid2523621 (14 packed).
- Not started: needs (a) a snapdiff case of 14079A470 over the trapopen captures (frame_trace recorded with the trapaction spec for 14079A470, bounded replay), (b) trap_oracle.py per case for manifest.tsv, (c) the object model of pes_trap_oracle_test moved from the window's PhysiqueA/B and O1/O2 to the engine's A (P+0xA08), B (A+0xF0) and O2 (A+0x188), with O1 handled as the cut leaves it. Budget ran out before (a)-(c).

### Open items
1. Trace the first ulp step at tick 1 in code (which of the cut's root-step vs the baseline accumulator gives the PES value): needs a capture of the step and the man's position at tick 1 (PES-side), or the baseline's StepAdjusters value for the same tick.
2. Separate the yaw half-angle effect from the displacement effect: rerun the current tree with the yaw read as before (1 line) and compare the dump; one more 15-minute smoke pair.
3. Trap oracle (item 3 above, steps a–c).
4. The real calls' rdx-convention output: decide whether GF's kick path needs that vector (it is not compared now).

## Round 7 (17-11): result

Branch OneWorld-series-wip, pushed. Head: dd04dcbd (trap oracle on engine memory). Earlier this round: none (the tick-1 analysis is static, no commit).

### 1. Trap oracle on the engine chain (DONE for the kind-0 suite; trap-tick section a named gap)
- Snapdiff case for 0x14079A470 over all 72 trapopen captures (pid2286718 x24, pid2523621 x8, pid2571173 x24, pid3727195 x16), in nine batches of eight (scratch trap_r7/batch.sh, chain.sh). Hooks: PES21/rendezvous/hooks.cfg.97hooks (the trap_kind0_79A470 displaced bytes). Packed captures were copied, unpacked, and packed again after each batch.
- Replay (snapdiff --bounded, all 72): 72 of 72 calls bit-exact (return value and every written byte), 0 mismatches, own-block per batch 0.13 to 0.58 (recorded, not gated).
- Manifests: trap_oracle.py, 72 of 72 written (trap_r7/oracle_bNN/<pid>_<seq>).
- pes_trap_oracle_test: the object model moved onto the engine. Build binds the window into the engine's memory (BindWorld), gives each capture a fresh engine player's chain for its slot (BindAnimeChain before WriteActorP; ConstructAnimPlugin gone with the cut), and lends the engine's shared regions. The physique assertions read the engine chain (A = P+0xA08, B = A+0xF0, G = B+0xC0). The motion-table, action-table and WriteActorP tests bind the same way.
- Per batch (run in turn, captures unpacked for the run, then packed):
  - Batches b01 to b07: 9 of 11 pass, and the two that fail are set-coverage checks only.
  - Batches b08 and b09: 8 of 11 pass; the EveryCaptured failure is a real mismatch (below).
- Set level (union over the nine batches): EveryCapturedCallOnGfsWindowGivesPesOutput 61 of 72 captures bit-exact; 11 mismatch. The coverage checks pass at set level: every binding of EachWindowBindingChangesTheCall is read by at least one capture, and TheMatchsOwnMemoryCommitsPesClipOnTheIssuedCaptures has issuing captures.
- The 11 mismatches, all from pid3727195 seq027 to seq037 (11 of that launch's 16 captures): GF's call returns out word 1 (a refusal) where PES's reference returns 4 (seq027, 028), 10 (seq029 to 032), 6 (seq035, 036) or 0 (seq037); on seq033 and 034 the issued flag and action bytes also differ. Named cause: OPEN. Tested and rejected: loader image from the engine's booted memory instead of the flat file (same refusal on seq027, 029, 031). Remaining candidates: the fresh per-capture chain's state (clip/motion table reads in the chain) or a state of this launch (the 16-capture pid3727195 set is a different launch from the other three), not separated.
- Gap (compiled out, #if 0): TrapTickOracle, 4 tests (EveryCapturedTickOnGfsWindowEqualsPes, TheCapturesCoverTheStartAndBothMirrors, ListWhatTheOverlaySupplies, LoaderDataTheTickReadsEqualsTheCapture). Its window objects are still the window's physique A, O1 and O2 (WindowObjects, WindowBytes, BuildTick); O1 has no engine counterpart in the cut.
- traptick_140797CD0 captures (14, pid2523621): not run (they are the tick oracle's inputs, which is the gap above).

### 2. The tick-1 ulp (STATIC analysis; no live position)
- The difference is one float ulp at x = 38 m (2^-18 = 3.81e-06), on the strike displacement x and z of the root point.
- New path (cut): frame.x = B+0x90.x - O2+0xD0.x, with B+0x90 written by PES's own translated FUN_1406EEDF0 (rootmotion/pes_fn_1406EEDF0.cpp). That writer does no float arithmetic on the strike branch: its SIMD lines are unpcklps interleaves (xmm3 = [d0,s0,d1,s1], then xmm2, then xmm3 again), so B+0x90 holds the bits the caller stored at [rsp+0x30] and [rsp+0x38]. The subtraction O2+0xD0 is the only float operation GF makes on it.
- Old path (baseline 4791bbbe): frame.x = A+0x14.x - O2+0xD0.x, where A+0x14 is written by the adjuster step (FUN_141EB3AF0 / FUN_141AF3CA0 fold, the layer's root point plus the step). Same subtraction.
- So the two sides read different stages of the same value: the baseline read the adjuster's own accumulator (A+0x14), while the cut reads the value PES's step writer copied into B. Their bits can differ by one ulp at 38 m because the value is rounded at different stages.
- Which matches PES: the cut. PES's own consumer of the displacement reads B+0x90 (the writer's output), so the cut reproduces what PES's downstream reads. The baseline reproduced an earlier stage, which PES does not consume directly.
- Not verified: a live PES value at tick 1 (not captured); the caller of FUN_1406EEDF0 (reached through the dispatch table, not a direct call, so not identified statically).
- Yaw half-angle separation: NOT run (it needs a code change and a smoke pair; not cheap within this round).

### 3. Merge-lane list (this line, OneWorld-series-wip at dd04dcbd)
| Test | Pass / total | Notes |
|---|---|---|
| pes_adjuster_tick_test | 4 / 4 | engine chain; PES_EXE, PES21_ROOT |
| pes_joint_block_test | 2 / 2 | engine chain |
| pes_anime_engine_test | 14 / 15 | 1 skipped (render plots, unrelated) |
| pes_fox_template_test | 3 / 3 (my last run) | Main's post-merge run reports 6 / 6 |
| pes_ball_list_test | 2 / 2 | |
| pes_kick_chooser_oracle_test | 1 / 1 | pid1152106 seq299, 421, 152 (GF_PES_KICK_ORACLE) |
| pes_kick_header_oracle_test | 2 / 2 | pid1152106 seq011, 013, 027 |
| pes_kick_root_test | 1 / 1 | pid1152106 seq003-005: 3 real rows, 6 synthetic (9 rows) |
| pes_trap_oracle_test (kind-0 suite) | 10 / 11 at set level | EveryCapturedCall 61 of 72 captures; the 11 mismatches are named above |
| pes_trap_oracle_test (trap-tick, 4 tests) | compiled out | named gap |

Open gaps (for the merge lane):
1. pid3727195 seq027-037 out-word mismatch (11 captures): cause open (see above).
2. Trap-tick oracle (4 tests) and traptick_140797CD0 captures: not ported to engine memory.
3. The tick-1 ulp: static analysis only; no live PES position; yaw half-angle separation not run.
4. Kick chooser and header: 6 of 24 captures run (the rest of pid1152106's kept sets).
5. Kick root: real rows use the r8 convention; the rdx-convention output of the real calls is not compared.
6. Smoke: pes-loop e46129415a2bdc1b (the line's own work; baseline da3b1eff7174bc8b is on OneWorld-cut4, already merged in 16234a42).

Scratch: trap_r7 holds the 72 manifests and the replay dirs; the mem links resolve only while a batch is unpacked (trap_r7 batch copies are packed). The baseline worktree <workdrive>/gf-AnimeLayer-base is removed.
