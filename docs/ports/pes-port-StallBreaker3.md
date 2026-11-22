# pes-port-StallBreaker3: merge of origin/StallLive into StallBreaker; the tree builds; the merged sim aborts at runtime; walker fix NOT landed

Status: NOT MET. Branch `StallBreaker`, pushed **09a5f7e2** (`git ls-remote origin StallBreaker` confirmed `09a5f7e2be89`). Commits: `55ff8f64` (merge of origin/StallLive 1b56b726 into a9cda866) and `09a5f7e2` (compile fixes after the merge). The merged tree BUILDS (pes_restart_delay_test, pes_hang_test, pes_throwin_release_test, pes_pass_target_test, pes_press_approach_test, gfsim_run) but ABORTS at runtime in the seed-7 match. No PesStall, PesHang, throw-in, pass-target or smoke measurement has passed on this tree, and the walker red test has not run to a verdict.

## 1. Merge (done, pushed)

- Merge base 89d0b5f0. StallBreaker had 106 commits StallLive lacks (TrapContact, MergeBKBJ, RootMotion, dribble and frame-builder work); StallLive had 465 StallBreaker lacks. 22 conflicted paths, all resolved; `git diff --name-only --diff-filter=U` empty; no conflict markers left.
- Source resolutions: additive hunks kept on both sides. Trap-chain physique: StallLive's engine chain (`BindAnimeChain`, `WriteAnimeG`) replaces the trap world's own chain writes and the layer-rate stand-in (the engine's SetMotion owns the rate). Trap-tick issue: StallLive's `MotionRequest` form inside `RunPesTrapTicks`, keeping `NotePesTrapTickIssue` and the dribble switch. `_PressApproach` inserted before HEAD's PES `_PassGetApproach` dispatcher; `_PassGetApproachGf` (pes_loop off) kept.
- Compile fixes (09a5f7e2): `PesHeap::Host` on the arena pointer (bounds from `arenaBytes_`); the two braces the merge dropped (`PesOwnsDribble`, `OnPesDribble`); the duplicate `p` and the trap-world physique chain block in `ConstructActorP` (superseded by the engine chain); `heap_.reset(new PesHeap())` so the owned deleter type holds; the census class brace and the holder-probe seam in `pes_stall_test.cpp`.

## 2. Generated, data and ledger (done, green)

- Proof sets: `results_trapaction.json` and `ro_tables.json` are unions by VA (76 and 75 entries). Shared VAs 0x14079F560 and 0x14079E0B0 keep HEAD's real-capture proofs (6 and 3 REAL calls) over StallLive's synthetic ones (2 each).
- Layer (`translated/`, closure, `translated.hpp`, `pes_dispatch_gen.cpp`) regenerated with `gen_translated.py --image pes_full.bin`. **`gen_translated.py --check`: GREEN, 10,471 functions byte-identical (3,865 recorded bit-exact, 6,606 closure).**
- `universe.json`: StallLive's (superset of HEAD's keys). The universe is a BFS over the Ghidra bridge (port 8080), which is DOWN, so no re-crawl was possible.
- `joins.json`, `LEDGER.md` regenerated. **`ledger.py --check`: GREEN** (47,642 rows; no unrecorded regression; 95 off-path ports, all allow-listed).
  - `scope_moves.json` (3 recorded): 0x140845BD0 `ported` -> `leaf accessor` (HEAD's row cited `pesreceive.cpp`, but no source on HEAD, main or StallLive names the VA; main and StallLive both say leaf accessor); 0x140A4C4A0 and 0x140A93030 `leaf accessor` -> `undecoded` (StallLive's universe reaches them from doc roots).
  - `offpath_allow.json` (37 recorded, cite "universe re-crawl pending"): HEAD-lineage and StallLive boot/kick ports that the merged universe does not reach. Need a re-crawl with the bridge and a real classification; NOT verified on path.

## 3. The runtime blocker (MEASURED)

- `PES_DISPATCH_UNTRANSLATED: 0x140789d00` at the first tick of the census match (`PesStall.TheBreakerNeverForcesABallUnderAnArmedRestart`, seed 7, full match) and in `PesHang.SeedElevenTwoMinutePesLoopMatchReachesFullTime`. The process aborts (SIGABRT, 134) in about 40 s. Logs: `gf-StallBreaker-scratch/run_census1.log`, `run_hang1.log`.
- 0x140789D00 is in no layer on any side (grep over `src/pes/translated` on the merge, HEAD~1 and StallLive: 0 hits). `match.cpp:2011` and `pescontact.hpp:127` name it as the slide phase (FUN_140789d00 sets bit 15 in slide phase 4). So the merged sim reaches a slide phase that neither side translated. Which caller reaches it (a vtable or pointer table in translated code) is NOT yet traced (OPEN).
- INF: the merged trap-tick and engine-chain ordering changes which man enters a slide early in the match; neither side's own measured tree showed this (StallLive16 reports a clean seed-7 smoke of 50,435 ticks on its tree).

## 4. Walker (NOT landed)

- On the merged tree the round-2 premise must be re-checked, and it changed:
  - `_ReceiveCandidate` (playercontroller.cpp:382-418) already runs PES's gates on the PES loop: C1 table, designated-owner refusal, carrier clause, RunTrapAdmit (gate 4a). The GF designation gate at 416-418 is `!match->PesLoop() && ...`, so it does not refuse on the PES loop.
  - `hasIt` (elizacontroller.cpp:660-661) is `GetPesHolder() == player` on the PES loop. The `AI_HasPossession` 1.0 m band is not on the PES-loop path.
  - The remaining GF refusal in the walker class is the walk-away conjunct of the body-contact trigger (humanoid.cpp ~694-697, `ballDistanceFuture > ballDistanceNow`), which never fires for an arriving man.
  - StallLive16 (on its own tree) measured 19,369 receive admissions accepted, refusals only by the carrier clause, and 196 trap chain issues against ~16,200 refused ticks (translated `RunTrapKind0`). So on that tree the wall was the translated trap chain's inputs, not a GF band.
- Red test (committed in the merge, on purpose): `PesStall.ANonOwnerWalkerBesideARestingBallStartsATrapWithinASecond`, counted in the shared census probe (`BreakerCensusProbe`): a man in MOVE, not the designated owner, within 0.5 m of a ball at rest (`GameplayTuning::kStalemateBallSpeed_ms`) with no PES holder, whose walk ends with no trap start within `pes::kPesTickHz` ticks. Expected 0. NOT RUN (the census aborts first). Note: the test counts TRAP starts, which depend on the trap chain, not on the body-contact band; so a humanoid band change would not turn it green. Whether the band matters must be decided by the census, not by the decode.

## 5. Not done (numbered)
1. Trace the caller of 0x140789D00 in the merged sim (the slide phase; find the vtable or pointer table that reaches it from translated code, then decide: translate the slide path on the merged tree, or find which merge change admits the slide). This blocks everything below.
2. Then run: the walker red test and PesStall.* (ALiveBall vs the StallLive baseline 2,401), PesHang seed 11, throw-in 9/9, PesPassTarget, PesPressApproach, the seed-7 2-minute smoke (determinism on rerun).
3. Walker fix only after the red test is measured: the candidate is removing the walk-away conjunct for the PES loop (humanoid.cpp ~694-697), but it must be judged by the census, not assumed.
4. `Dump(Match&)` (StallLive's census helper, `pes_stall_test.cpp`) is kept and UNUSED; wire it into the census (replacing the inline STALLDUMP block) or delete it.
5. Re-crawl `universe.json` with the bridge; reclassify the 37 allow-listed ports.
6. `rm -rf build` in the worktree once no build is running (not done: the build dir is still needed for step 1).

## 6. Verification run this session
| Check | Result |
|---|---|
| unmerged paths / conflict markers | 0 / 0 |
| `gen_translated.py --check` | GREEN, 10,471 functions byte-identical |
| `ledger.py --check` | GREEN, 47,642 rows |
| `cmake --build` of six targets | GREEN (BUILD_RC 0) after the compile fixes |
| `pes_hang_test` (seed 11) | ABORT 134, `PES_DISPATCH_UNTRANSLATED: 0x140789d00` |
| `PesStall.TheBreakerNeverForcesABallUnderAnArmedRestart` (census) | ABORT 134, same |
| walker red test, ALiveBall, throw-in, pass-target, smoke | NOT RUN (blocked by the abort) |
| mutations | none |
