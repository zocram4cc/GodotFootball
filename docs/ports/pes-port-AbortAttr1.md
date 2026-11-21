# pes-port-AbortAttr1: where PES_DISPATCH_UNTRANSLATED 0x14073DD50 first aborts on the MergeLane2 chain

Worktree <workdrive>/gf-AbortAttr (branch AbortAttr, base 43e96723). Attribution only: no fix, no commit, no push. The worktree is reset to 43e96723 and clean, and `build/` is deleted. Logs are in <workdrive>/gf-AbortAttr-scratch/ (gdb_*, build_*, db_*, tw_*, g44_*, g4425_*).

## Result
- **Tip: db9bfef3** ("MergeBKBJ red fix: kick-family body action ids map to MOVE"), the MergeLane2 tip. This is the first tip on the chain where 0x14073DD50 aborts. Measured on a shimmed tree (see Link).
  - `pes_press_approach_test`: ALoosePresserCommandNamesTheActionPressTarget and ADesignatedManWithNoHolderIsCommandedToTheRestingBall both abort `PES_DISPATCH_UNTRANSLATED: 0x14073dd50` (t = 5376302416), SIGABRT.
  - `pes_pass_target_test`: aborts in `PesPassTarget::SetUpTestSuite` (its match) with the same message.
- **Caller: `pes_fn_1406F6CE0`**, `src/pes/translated/kickjoints/pes_fn_1406F6CE0.cpp:1078` (PES VA 0x1406F6CE0). Line 1078 is the indirect call `pes_dispatch_lookup(rd(rd(gpr[0]) + 0x90))` (db9bfef3 file, lines 1068-1080). Backtrace, identical in all three binaries:
  `pes_dispatch_missing` (pes_dispatch_gen.cpp:21312) <- `pes_fn_1406F6CE0` (kickjoints:1078) <- `PesWorld::RunOnP` (pesworldrecord.cpp:628) <- `PesWorld::RunPlayerUpdate` (pesworldrecord.cpp:507) <- `WritePesTrapActors` (teamAIcontroller.cpp:3458) <- `Match::Process` (match.cpp:896) <- the test's `RunPesLoopMatch`.
- **Earlier abort on the same caller:** 4425ef0f (the MergeBKBJ merge, parent 44688747) aborts `PES_DISPATCH_UNTRANSLATED: 0x140789d00` (t = 5376613632) from the same line 1078 in all three binaries. 43e96723 and 44688747 do not abort.
- **Through which change:** the caller's file is identical across 44688747, 4425ef0f and db9bfef3 (0 changed lines in each step). The change is in the action the caller dispatches on, not in the caller.
  - CLAIMED by db9bfef3's commit message, not re-decoded: MergeBKBJ's `AnimeActionOfClip` (pestrapactor.cpp, auto-merged) writes the kick-family ids (8, 9, 0xA, 0xB, 0xD, 0x38, 0x3A) into P+0xAC8.
  - CONFIRMED (line 1078 of db9bfef3): the target is `[obj + 0x90]`, where obj is produced by the call to `pes_fn_1406D9010` (table at P+0xC40, id byte read through gpr[14]). INF that this selects the entry by the action id; not decoded. The kick-family ids give 0x140789D00 at 4425ef0f.
  - CONFIRMED (db9bfef3 diff, pestrapactor.cpp only): the red fix returns kAnimeActionMove for those cases. The same line-1078 dispatch then yields 0x14073DD50 (measured).
  - INF: the object the MOVE selection reaches has +0x90 = 0x14073DD50. Not decoded.
- **Caveat on the red fix's claim:** db9bfef3's commit message says the 0x14073DD50 aborts (and 0x140791DD0) "are not this path". The backtrace shows 0x14073DD50 from the same caller and line 1078 in all three binaries, so that claim does not hold for these runs. Flagged for the owner.
- **Not established:** that the MergeLane2 tips as committed run at all. See Link.

## Measurements
gdb under `tools/dev/simslot.sh`; `PES21_ROOT=<workdrive>/PES21`, `PES_EXE=<home>`; `GF_DEBUG_RECEIVE`, `GF_DEBUG_ANKLE`, `GF_PES_SURVEY` unset. Each test takes about 6.5 to 7 min under current load, so tests ran one process each, in parallel.

| tip | build | press tests | pass tests | abort |
|---|---|---|---|---|
| 43e96723 (StallLive) | as committed | ALoosePresser OK; ADesignatedMan FAILED :116 (4 of 259 designated-man movement ticks named no target); ANoMan OK | IsSetInFlight SKIPPED (named gap); EveryPass... OK | none |
| 44688747 (PressCut merge) | **fails**: pesworldrecord.cpp:1162,1175 (see Link); shim `heap_.reset(new PesHeap())` | ALoosePresser FAILED :90 (was OK at 43e96723); ADesignatedMan FAILED :117; no ANoMan test at this tip | as 43e96723 | none |
| 4425ef0f (MergeBKBJ merge) | **fails to link as committed**; shim (5 restored + 2 tripwires) | ALoosePresser ABORT 0x140789D00; ADesignatedMan ABORT 0x140789D00 | ABORT 0x140789D00 in SetUpTestSuite | 0x140789D00 at pes_fn_1406F6CE0:1078 |
| db9bfef3 (MergeLane2 tip, red fix) | **fails to link as committed**; same shim | ALoosePresser ABORT 0x14073DD50; ADesignatedMan ABORT 0x14073DD50 | ABORT 0x14073DD50 in SetUpTestSuite | **0x14073DD50 at pes_fn_1406F6CE0:1078** |

Not run: c25b4938 (PassArrival merge, between 4425ef0f and db9bfef3). INF: it aborts 0x140789D00, since the red fix is the only change to the kick-family mapping and it lands only in db9bfef3.

## Link: the MergeLane2 tips do not link as committed
- **44688747 does not compile.** pesworldrecord.cpp:1162 and :1175 assign `std::make_unique<PesHeap>()` to `std::unique_ptr<PesHeap, pes::PesHeapDeleter>`, which has no operator=. Static grep: PressCut's own tip 2b9e4786 has the same assignments. The measurement used `heap_.reset(new PesHeap())`, the form 4425ef0f:pesworldrecord.cpp:126 uses. Reverted after measuring.
- **4425ef0f, c25b4938 and db9bfef3 do not link.** Seven symbols are undefined: `pes_fn_1406EA290`, `pes_fn_1406EA1A0`, `pes_fn_140799DB0`, `pes_fn_14079ED20`, `pes_fn_14084D7F0`, `pes_fn_140846B90`, `pes_fn_14079D940` (18 references from pes_dispatch_gen.cpp and closure/pes_fn_14079A310.cpp). None is defined at db9bfef3; the translated tree is identical across 4425ef0f, c25b4938 and db9bfef3.
  - The MergeBKBJ merge deletes five closure files: 1406EA290, 140799DB0, 14079D940, 14079ED20, 14084D7F0 (`git diff --diff-filter=D 44688747 4425ef0f`).
  - PressCut's commit 2b9e4786 drops "the two stale closure copies": 1406EA1A0 and 140846B90 (both tracked at 43e96723).
- **Shim used for the 4425ef0f and db9bfef3 runs (scratch only, not committed, removed):** the five deleted files restored byte-identical from 44688747; and two tripwire files for 1406EA1A0 and 140846B90 that abort with `SCRATCH_TRIPWIRE ... reached`. No tripwire fired in any log. The abort happens before either would run; the backtraces contain only committed frames.
- INF: the owner's four binaries were linked against files not in these commits. The gen_translated `--check` rule (21-11-26, item 5) therefore fails on these tips. I did not run gen_translated.

## Open items
1. Which object the line-1078 dispatch lands on after the red fix, and why its +0x90 is 0x14073DD50 (INF; `pes_fn_1406D9010` not decoded).
2. c25b4938 (PassArrival merge) not built (see above).
3. Whether the owner's MergeLane2 tree has the seven functions. As committed it does not link, so the owner's four abort binaries came from a tree that differs from these commits.
4. The red fix's "not this path" claim for 0x14073DD50 contradicts the backtrace here (same caller, same line). Needs the owner's review.
