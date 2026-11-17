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
