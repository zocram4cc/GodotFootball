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
