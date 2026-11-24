# PES port: WalkLane6 (branch WalkLane, tip 843a2e305, NOTHING LANDED)

## 0. Verdict
- Item 1 (strike root lock on Trap men): REJECTED. Measured PesStallQuick 697 (bound 53). ATrapsRoot improves 0.156 m -> 0.068 m but stays red (tolerance 1 mm). Reverted. Scenario: none (no M1/M2 claim).
- Item 2 (dribble series wired through the body queue): implemented, measured PesStallQuick 253 (bound 53). REJECTED and reverted. Patch kept in scratch. Dribbler test result: see section 4.
- Item 3 (re-land PES action ids 2f8fcca3 + walker anchor together): NOT RUN. Blocked on the ALiveBall rise (section 5).
- Tip unchanged: `843a2e305` (no push; nothing new to land). The branch holds the ratchet (PesStallQuick 53, ALiveBall PASS per WalkLane5).

## 1. The +0x81 writer (CONFIRMED, decompile of `FUN_1406EEDF0`, Ghidra headless, `gf-WalkLane-scratch/dec_strike.txt`)
- `B+0x81` (strike valid) = `FUN_141EB3320(A) && !FUN_141EB3350(A)`, with `A = P+0x428` (the strike adjuster).
- `FUN_141EB3320(A)` = `A+8 == 2` (translated: `src/pes/translated/restart/pes_fn_141EB3320.cpp`). `FUN_141EB3350(A)` = `A+0x6D == 3` (translated: `src/pes/translated/closure/pes_fn_141EB3350.cpp`).
- So the flag is "adjuster state 2 and mode != 3". The docs said "not finished" (`pes-port-PassArrival.md:119`, `LEDGER` row 0x1406EEDF0); that wording is wrong: state 2 is the done state per `pes-re-gaps-anime.md:213`.
- GF already reads this byte per tick: `PesWorld::ReadAdjusterFrame` -> `frame.strikeMoved` (`pesworldrecord.cpp:1227`). `B` is refreshed for every active man (`teamAIcontroller.cpp:3508`, `RunAdjusters`), so the flag is PES's value per man. The lock therefore only differs from PES in which men reach state 2 (INF below).

## 2. Item 1 measurements (tree = 843a2e30 + lock on `frame.strikeMoved && ac8 == kAnimeActionTrap`)
- Build: pes_pass_target_test, pes_restart_delay_test (ccache), BUILD_RC=0.
- ATrapsRootMovesByExactlyTheStrikeAdjusterStep: worst gap 0.068035 m vs 0.001 (was 0.156 on the tip). Red. (`s1_atraps.log`)
- PesStallQuick: `forced_live=697` vs 53. Red. Census (GF_STALL_DUMP, 1,365 firings, all owned=0): ac8=7 (Trap) 806, ac8=2 483, ac8=13 54, ac8=21 37. (`s1_quick.log`)
- Earlier on the WIP tree: lock on all strikers 1,602; lock off 319 (WalkLane5). So the Trap-only lock is the big term too, not just the non-Trap men.
- INF: the lock keeps a trapping body still on every tick the flag is set (the body moves only by WarpRoot), and the flag stays 1 after state 2, so the body holds a finished adjuster's point for the rest of the Trap action. PES's trap-end/reset (FUN_1406EB440 on action change, which resets A) is the likely missing piece; not verified.
- Rejected per LAND-OR-REVERT. Note: `git diff` fails in this worktree (submodule symlink) and a `2>/dev/null` diff wrote an empty patch, so the lock diff was lost; its content is recorded in `gf-WalkLane-scratch/trap-lock-only.md`.

## 3. Item 2 (dribble series) measurements
- Change: `SeriesMembersOf(slot)` reads all series entries (0x38 bytes each: motion +8, startTime +0xC, mirror +0x10); `HumanoidBase::PlayDribbleSeries` plays member 0 and queues the rest; `Humanoid::Process` starts the next queued member when the playing one ends during PES's Dribble action. The single issue goes through the same call. `SeriesFirstOf` removed. The blend FUN_141AF52B0 is NOT ported (ponytail).
- Build: BUILD_RC=0 (pes_restart_delay_test).
- PesStallQuick: `forced_live=253` vs 53. Red. Census: ac8=7 236 (Trap clip `trap_0_0_000_y0_sole_mirror`, owned=0), ac8=2 187, ac8=21 68, ac8=13 6. The rise is on Trap men and locomotion/trip clips, none owned by PES's dribble. INF: the seeded match diverges from the first changed dribble issue, so the census moves chaotically (WalkLane4 saw the same). Not attributed to a named stand-in, so M2 does not apply.
- Patch: `gf-WalkLane-scratch/dribble-series.patch` (187 lines). Reverted from the tree.
- Dribbler test (`PesStall.APesDribblerNeverHoldsHisClipsLastFrame`): see section 4.

## 4. Dribbler test
- MEASURED on the item 2 tree (run 1,749 s, `s2_dribbler.log`): `worstLastFrameHold` 264 vs 1. Red, but below the tip's 343 (WalkLane5). That change is reverted with item 2, so the tip's dribbler stays at 343.

## 5. Item 3 (action ids + walker anchor): not run
- The action ids (`2f8fcca3`, `src/sim/onthepitch/pestrapactor.cpp` AnimeActionOfClip) and the walker clip-start anchor (`walker-clipstart.patch`) were measured earlier: ids alone ALiveBall 4,965 (Integrate bea22064, Main's A/B), walker alone 2,375 (WalkLane1), both over the 2,229 bound. The walker-only and ids-only ALiveBall rises have not been split; the combined change has not been measured on a tree that holds item 2 or 1.
- Not re-run this round: the series change (item 2) already fails the bound on its own, and item 3 needs the ALiveBall rise attributed to named stand-ins first (M2).

## 6. Open items (numbered)
1. Strike lock: find the reset PES applies to A on action change (`FUN_1406EB440`, called by the action setter `FUN_1406EEBB0`, per `pes-re-gaps-anime.md:263/387`) and check whether GF runs it; the lock is only sound once A is reset at trap end. Correct the "not finished" wording in `pes-port-PassArrival.md:119` and the LEDGER row.
2. Dribble series: the blend `FUN_141AF52B0` between members (position from `FUN_1406F14E0`), so each member is not played whole. Attribute the 253 census rise (Trap clips) before retrying.
3. Item 3: attribute the walker/ids ALiveBall rise to named stand-ins (M2) or re-check the walker anchor on the trap-reset tree.
4. Worktree: `git diff` / `git status` fail (`extern/godot-cpp` symlink); use `git show HEAD:path` and `diff -u` (as in `dribble-series.patch`).

## 7. Commands and environment
- Configure: `cmake -S . -B build` with `GF_DIFF_PYTHON`, `PES_EXE=<home>/ghidra-projects/scratch/Unpacker/pes_full.bin`, `PES21_ROOT=<workdrive>/PES21`. Build: `flock /tmp/gf-build.lock nice -n 19 cmake --build build --target pes_restart_delay_test -j8`.
- Sims: `tools/dev/simslot.sh timeout <n> nice -n 19 ./build/tests/sim/pes_restart_delay_test --gtest_filter=...`.
- Build dir `gf-WalkLane/build` is kept for the next round; delete when the lane ends.
