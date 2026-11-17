# AnimeLayer7 handover: FUN_141EB5E70 proof and the one-chain cut

## Pushed (OneWorld-series-wip)
- dee9d88c: proof row for 0x141EB5E70 in tools/dev/translate/results_animelayer.json, plus closure_adjtick.json (the 10 closure members). Snapdiff --bounded on real calls: walk-out seq061/seq062 (res_adjtick.json) and in-play seq1128/seq1171 (res_adjB.json): 4 calls, 0 mismatches, own-block 7/7. Strike and yaw proven. Named gaps: height adjuster P+0x498 running (not observed in 19 captured calls); chain X (FUN_141EBC820 -> FUN_141B00910), gate S+0x14 <= 255 closed in 19/19 captures (S+0x14 = 256, DAT_1437F4CB0 = 1).
- ff86210d: closure specs and universe rows.
- Deleted after recording: caps/pid3778393_{061,062,1128,1171}. Kept: caps/pid3199291_001 (earlier decision).

## Uncommitted in the worktree (the cut, not pushed)
- Patch: <workdrive>/gf-AnimeLayer-scratch/pes-cut-AnimeLayer7.patch (status list beside it). Do not commit until the tests below are green.
- Layer regenerated: src/pes/translated (19 files, pes_fn_141EB5E70 in gameplay/). Two held-out functions reported by the generator (0x1405549C0, 0x14202BA30) are unrelated to this slice.
- Core: pestrapworld.hpp (physique and O1/O2 buffers removed; B offsets kB*), pesworldrecord.hpp/.cpp (BindAnimeChain(slot, engineP), WriteAnimeG, WriteAnimeRootPoint, RunAnimeJoints(slot, ext) without the pose copy, RunAdjusters -> pes_fn_141EB5E70 with xmm1 = 1/54, ReadAdjusterFrame from B: strike B+0x81, step B+0x90/98, yaw rate B+0xB8/BC or absolute atan2(B+0xA4, B+0xAC); RunRootMotion reads the engine's B), pestrapactor.cpp (WriteActorP writes G and the root point through the engine), teamAIcontroller.cpp (BindAnimeChain before WriteActorP; RunAdjusters where RunFrame was), pesmovestep.cpp (ReadAdjusterFrame; degToRad removed), pesfoxworld.cpp/.hpp (RunFrame and kFramePhases removed; ReadPose kept), match.cpp + pestrapactor.* (ConstructAnimPlugin removed: the window's O1 was the stand-in).
- Tests: tests/pes_engine_harness.hpp (RunEngineFrame: the measured fixed phase list, test-only), pes_anime_engine_test.cpp and pes_fox_template_test.cpp (routed through it), pes_adjuster_tick_test.cpp (new, 4 tests), tests/CMakeLists.txt (target).

## Status
- Builds (niced): pes_adjuster_tick_test, pes_anime_engine_test, pes_fox_template_test, gfsim_run, pestranslated: all compile and link.
- pes_anime_engine_test: 14 OK, 1 skipped (render plots, unrelated); the harness keeps the engine's behavior.
- pes_adjuster_tick_test: 0 of 4 pass. Cause (confirmed): PesWorld::Bytes() is the window's host buffer, and NewCall(uint8_t*) (pesworldrecord.cpp:30) builds the translated state over the window only, so guest reads of the engine heap return 0. BindAnimeChain reads engineP+0xA08 and gets 0; the same blindness would hit RunAdjusters, RunAnimeJoints and RunOnP on the engine chain (the cut is not correct yet, though it compiles).
- Not run: pes-loop and switch-off smokes (the cut is not runnable correctly yet).
- Not ported (the cut breaks their compile): tests/pes_joint_block_test.cpp (stand-in chain, PhysiqueA, O2, RunAnimeJoints pose); tests/pes_kick_oracle_common.hpp (Build calls BindAnimeChain(kSlot) with the install's stand-in chain; affects pes_ball_list_test, pes_kick_chooser_oracle_test, pes_kick_header_oracle_test, pes_kick_root_test); tests/pes_trap_oracle_test.cpp (PhysiqueA/B/AVa/BVa, O2, ConstructAnimPlugin).

## Expected moves (when runnable)
- pes-loop smoke (baseline da3b1eff7174bc8b) and switch-off smoke (baseline c2016bf08fd0bc98): expected to move. The adjusters now run PES's own FUN_141EB5E70 on the engine chain (its phases and joint writer), the root step reads B, and the hand degToRad conversion is gone. Report each move with that reason.
- Anime engine tests: unchanged (14 OK).

## Next steps, in order
1. Build the trap world's translated states over the full PES memory: keep a PesMemory* in PesWorld::BindMemory and use
   memory_->NewCall() (pesmemory.hpp:151) instead of NewCall(Bytes()) in every trap-world path that touches the engine
   chain: BindAnimeChain, WriteAnimeG, WriteAnimeRootPoint, RunAnimeJoints, ReadAdjusterFrame, RunAdjusters, RunOnP and
   RunRootMotion. Check LendExternals/ReturnSegments with the full state.
   Mechanics (17-11, static): `PesWorld` holds no reference to the memory today — `BindMemory`
   (`pesworldrecord.cpp:80-92`) copies the window into `memory.AddWindow`, points `heap_` at `memory.Heap()` and keeps
   nothing else, so a member is needed (`PesMemory* memory_ = nullptr;` beside `heap_`, `pesworldrecord.hpp:1333`).
   `PesMemory::NewCall()` maps the image, the heap, the stack/TIB and the OS arena, while the world's `Bind`
   (`pesworldrecord.cpp:21-28`) maps only the window plus one global seg and puts `rsp` at `kStackVa + kStackBytes`:
   check that `rsp` difference first, because a translated body that indexes its frame by the world's `kStackVa` breaks
   once the state comes from the memory.
2. Run pes_adjuster_tick_test. The four tests must pass. If the yaw test fails on the sign or axis, swap B+0xA4 and B+0xAC in ReadAdjusterFrame (the test compares the output to P+0x4F0 within 1e-5 rad).
3. Port pes_joint_block_test (write the test's pose into the engine chain's arrays: positions at [L1+0x50], rotations at [chain+0x20], 16-byte entries; RunAnimeJoints(slot, ext)); pes_kick_oracle_common.hpp (Build takes an engine player; BindMemory before writes; no stand-in segments); pes_trap_oracle_test.cpp (drop the PhysiqueA/B and ConstructAnimPlugin assertions; reach O2 through the chain).
4. Build the full test set and run ctest; run the pes-loop and switch-off smokes through tools/dev/simslot.sh (seed 7, 2 min); report the expected moves with reasons; commit; push; ls-remote.
5. Then delete the stand-in chain loader data (BuildAnimeChain, kAnimeChainVa segments, the kAnimeLayer/KeyTable/Quat VAs) that the trap path no longer reads.

## Named gaps kept
- Height adjuster branch (`P+0x498`): state, counters and accumulator are all zero in `seq1128`, so it is **never
  armed**, not armed late; who stores state 2 into the three adjusters is open. See the adjtick note.
- Chain X: **open, not closed.** The gate `S+0x14` is 256 in 19/19 captures, but that means "no X chain selected":
  `FUN_141ebced0`, reached through a second handle at `P+0x3AE8`, stores an index below 256 from the parametric-request
  path. No stand-in. To settle it when the rig is back: hook `FUN_141ebced0`. See the adjtick note.
- TrapAdmit2 item 1 (rewire onto the real control) and item 3 (planner) wait on this cut.

## Evidence files
- local/pes-adjtick-AnimeLayer7.md, local/pes-report-adjtick-AnimeLayer7.md (earlier), proof logs in scratch: proof_adjB.log, snapdiff_adjB.log, res_adjB.json, res_adjtick.json, build_cut.log, test_adjtick2.log, test_engine_full.log.
