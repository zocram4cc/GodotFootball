# A3Salvage salvage table (main 10edd6f)

Snapshots (uncommitted, untracked, full diffs) are in this directory: `<Name>-uncommitted.patch`,
`<Name>-untracked.tar` / `.txt`, and `MovestepRun-stash0.patch` (the stash is repo-global; `stash@{0}` is still in `gf-MovestepRun`'s repo, left for Main).
Commit backups pushed: `origin/backup/MovestepRun-7ad9620`, `origin/backup/MovestepTurn-c172e02`, `origin/backup/Wave1MBF-diag-5714da4`. `origin/backup/MovestepStep-260a58f` already existed.

Method: `git range-diff` for commits; for files, the worktree blob against `origin/main` (`diff`), counting lines only in the worktree (`>`) and checking each worktree-only line for presence in main's file.

## Commits

| Source | Commit | Disposition | Why |
|---|---|---|---|
| MovestepRun | e7931ba (group B v0.10, index fix) | Dropped: on main as d72c49d | `git range-diff`: identical |
| MovestepRun | 360f49d (group B readers + tests) | Dropped: on main as 35ff4ff | `git range-diff`: identical |
| MovestepRun | 7ad9620 (GOTO_RUN re-plan, hit-record finding) | Dropped: on main as 7e1e9c2 | `git range-diff`: identical |
| MovestepStep | 52bfec9 (group C states) | Dropped: on main as ec39324 | `git range-diff`: only the CMake source list differs, and main's list is the newer one |
| MovestepStep | 260a58f (group C tests, two corrections) | Dropped: on main as ad1806f | `git range-diff`: identical |
| MovestepTurn | c172e02 (contract v0.8) | Dropped: already an ancestor of main | `merge-base --is-ancestor` |
| Wave1MBF-diag | 5714da4 and its ancestors (MB-F) | Not MOVESTEP: left to the MB-F owner | Commits are MB-F ball prediction, not anime |

## Uncommitted

| Source | Item | Disposition | Why |
|---|---|---|---|
| MovestepRun | 26 files byte-identical to main: docs/re/pes-re-clock-live.md, docs/re/pes-re-foundation.md, src/sim/gametypes.hpp, src/sim/onthepitch/{pesclips,pesdefshape,pesjudge,peskeeper,pesoffball,pesstrategy}.{hpp,cpp}, setpiecelaws.hpp, goalie_default.cpp, tests/sim/{pes_ball_collision,pes_clips,pes_defensive_shape,pes_judge,pes_keeper,pes_offball,pes_fieldinfo}_test.cpp, tools/dev/pes_endless_match.py | Dropped | Already on main |
| MovestepRun | src/pes/movestep.cpp, movestep.hpp (25 and 154 main-only lines, 0 and 2 worktree-only; the 2 are comments) | Dropped | Main is a superset: loaders, RecordIssue, MoveHost accessors, yawLock, kStyle and the Motion() read all exist only on main |
| MovestepRun | src/pes/movestep_run.{cpp,hpp} deleted, tests/pes_movestep_run_test.cpp deleted | Dropped | Main has both files, newer |
| MovestepRun | stash@{0} (on 3555adc): `dwell03`, `dwell05`, `dwell14` fields replacing the STOP literals, and the stop band | Dropped; patch kept | The stash's new `RunConstants` fields have no loader, so they would read as zero. Main already has the stop band fix (`kStopBand`, movestep_run.cpp:587) and the three dwells as named constants with their VAs (movestep_run.cpp:142-144). |
| MovestepRun | tests/CMakeLists.txt `godot_locomotion_test`, godot/tests/test_locomotion.gd (81 lines) | Dropped | Main has the same test as `godot_motion_test` / test_motion.gd |
| MovestepRun | tests/pes_movestep_test.cpp (482 main-only lines, 0 worktree-only), tests/sim/pes_camera_test.cpp, tests/sim/CMakeLists.txt, tests/sim/pes_receive_test.cpp, tests/sim/pes_strategy_test.cpp | Dropped | Worktree versions are subsets of main |
| MovestepRun | src/sim/onthepitch/{ball.cpp (66 worktree-only lines), match.cpp (18), match.hpp (26), pescarrier.hpp (21), referee.cpp (2), player/controller/elizacontroller.cpp (73), player/humanoid/humanoid.cpp (18), teamAIcontroller.cpp (40)}, src/gdext/gf_match.cpp (1), tests/sim/{pes_carrier (24), pes_setpiece_team (55), pes_team_concepts (8), sim_match (2)}_test.cpp, tasks/14-10-26.md (4) | Dropped | Not MOVESTEP. Worktree-only lines are stale pre-main versions of files other owners changed since: BallTick (ball, match, referee), FieldInfo, events, possession latch, MB-F. Applying them would revert main. Other owners' files were not touched. |
| MovestepTurn | CMakeLists.txt, tests/CMakeLists.txt (5 lines) | Dropped | Main lists the group targets, pesmovestepcore and the test set |
| MovestepTurn | untracked src/pes/movestep_turn.{cpp,hpp}, tests/pes_movestep_turn_test.cpp (WIP group D) | Dropped | Main's d6002a4 (MovestepTurn2) supersedes them: every function name in the WIP file exists on main. The WIP test file's 3 test names not on main: `LoadsTheExeConstantsAndTablesWithTheirRelations` (main has `MoveTurnConstantsInstall.LoadsTheExeConstantsAndTablesWithTheirRelations`), `RequestsAStepRowWhenTheClipHasRunOut` (main covers RunTurnRunSlipdown), `KeepsTheSlipFlagClearWhenTheClipStillHasFrames` (weak: its comment says the synthetic host reports zero, so it asserts little) |
| MovestepTurn | scratch_regions.txt ("ok") | Dropped | Scratch |
| MovestepStep | untracked: build-rebased.log, 23 godot/**/*.uid | Dropped | Build log and generated Godot UID files |
| Wave1MBF-diag | src/sim/onthepitch/{AIsupport/AIfunctions.cpp, AIsupport/mentalimage.{hpp,cpp}, ball.{hpp,cpp}, matchreport.cpp}, tests/sim/CMakeLists.txt, untracked diagprobe.hpp, diag_grid_test.cpp | Not MOVESTEP (0 movestep/anime/MoveHost mentions in its patch) | Left for the MB-F owner; snapshot kept |

## Brought onto A3Salvage
Nothing. Every MOVESTEP commit and file is either on main already or superseded by main's newer version. The only cherry-picks are none.
