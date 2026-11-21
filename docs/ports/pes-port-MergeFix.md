# pes-port-MergeFix: MergeLane2 links; gen --check clean from a clean export

Branch `MergeLane2`, pushed `e507d1271e` (fast-forward from `db9bfef3`). One commit, seven generated files, no source or test change.

## 1. The seven missing `pes_fn_*` (CONFIRMED)
- **The generator did not drop them, and no closure JSON lost them.** `closure_trapfinish_f560.json` (closure of record 0x14079F560) has 86 members at `44688747`, `4425ef0f` and `db9bfef3`. The two `0x14079F560` records in `results_trapaction.json` point at it.
- `gen_translated.build()` run in memory on `db9bfef3` emits all seven (`sub = closure`); the closure directory count is 6,758, the same as the committed layer.
- **The loss was in git.** `4425ef0f` (MergeBKBJ merge) deletes the seven from the index (`git diff --diff-filter=D 44688747 4425ef0f`). The regen run during that merge wrote them back into the worktree (mtime 08:23) as untracked files, and the merge commit never picked them up. `gen --check` compares against the disk, so it passed in that worktree. A checkout of the commit has no copies and fails to link.
- Verified: on `db9bfef3`, disk-only files under `src/pes/translated` are exactly the seven; no other untracked or tracked-only file.
- **Fix:** committed the seven exact generator outputs (`e507d1271e`, paths `src/pes/translated/closure/pes_fn_{1406EA1A0,1406EA290,140799DB0,14079D940,14079ED20,140846B90,14084D7F0}.cpp`). Nothing hand-copied.

## 2. Compile error in `44688747` (CONFIRMED, no change needed at the tip)
- `make_unique<PesHeap>()` into `unique_ptr<PesHeap, PesHeapDeleter>` (pesworldrecord.cpp:1162, :1175) exists only in `44688747`, an intermediate commit. The MergeBKBJ merge (`4425ef0f`) already ported those sites to HEAD's `heap_.reset(new PesHeap())`; `db9bfef3` and `e507d127` have no `make_unique` in that file, and they compile.
- History was not rewritten. The intermediate commit still does not compile; the tip does.

## 3. Claim in `db9bfef3`'s message (CORRECTED: not supported)
- The message says the 0x14073DD50 aborts (and 0x140791DD0) "are not this path". The gdb backtraces of all five aborting binaries show the same frame, `pes_fn_1406F6CE0`, `src/pes/translated/kickjoints/pes_fn_1406F6CE0.cpp:1078`, the same dispatch line that aborted with 0x140789D00 at `4425ef0f`.
- What the red fix measurably did: `pes_press_kickoff_test` went from abort to 6/6. The dispatch key that selects 0x14073DD50 is not decoded (see open items).

## 4. Build and link
- `build/` configured with the contract env (Ninja). The nine binaries built and linked (rc 0) on `db9bfef3`'s content; `nm` shows the seven symbols defined in `libpestranslated.a`. Building was done again on the committed tree: identical content, so no rebuild was needed.

## 5. Test results
Run under gdb batch through `tools/dev/simslot.sh`, `env -u GF_DEBUG_RECEIVE -u GF_DEBUG_ANKLE -u GF_PES_SURVEY`, `PES_EXE=pes_full.bin`, `PES21_ROOT=<workdrive>/PES21`. The gdb exit code is always 0, so the abort is read from the log line `Program received signal SIGABRT` and the `PES_DISPATCH_UNTRANSLATED` line. Logs: `<workdrive>/gf-MergeFix-scratch/<binary>.log`.

| binary | result | where it stops | abort |
|---|---|---|---|
| pes_press_world_test | PASS 29/29 | | |
| pes_press_kickoff_test | PASS 6/6 | | |
| pes_press_test | PASS 5/5 | | |
| pes_playerdb_wiring_test | PASS 14/14 | | |
| pes_press_approach_test | ABORT | 1st test, `ALoosePresserCommandNamesTheActionPressTarget`, 0 OK | 0x14073DD50 at pes_fn_1406F6CE0:1078 |
| pes_pass_target_test | ABORT | `SetUpTestSuite` (pes_pass_target_test.cpp:109) | 0x14073DD50 at pes_fn_1406F6CE0:1078 |
| pes_throwin_release_test | ABORT | 1st test, `ThePlayStateIsSetPlayReadyUntilTheReleaseTouch`, 0 OK | 0x14073DD50 at pes_fn_1406F6CE0:1078 |
| pes_movestep_wire_test | ABORT | 3rd test, `PesLoopWritesPlaybackOnlyWhenItIssues`, 2 OK | 0x140791DD0 at pes_fn_1406F6CE0:1078 |
| pes_restart_delay_test | ABORT | 2nd test, `...WithinTheInstallBoundWithPesLoop`, 1 OK | 0x14073DD50 at pes_fn_1406F6CE0:1078 |

Backtrace, identical in all five: `pes_dispatch_missing` (pes_dispatch_gen.cpp:21312) <- `pes_fn_1406F6CE0` (kickjoints:1078) <- `PesWorld::RunOnP` (pesworldrecord.cpp:628) <- `RunPlayerUpdate` (:507) <- `WritePesTrapActors` (teamAIcontroller.cpp:3458) <- `Match::Process` (match.cpp:896).

This matches the expected state: the per-action handlers 0x14073DD50 / 0x140789D00 reached from `pes_fn_1406F6CE0`, left as StallBreaker's work. The 0x140791DD0 abort (movestep_wire) goes through the same frame.

## 6. Gates
- `gen_translated.py --check` on a clean export of `e507d1271e` (`git archive`, no untracked files): rc 0, 10,637 functions (3,879 recorded bit-exact, 6,758 closure), byte-identical.
- `ledger.py --check` in the worktree: rc 0; 28,773 rows compared, 163 scope moves recorded; LEDGER current (47,642 rows).
- `tools/check_pii.sh`: rc 0.

## 7. Open items
1. The dispatch key that selects 0x14073DD50 from `pes_fn_1406F6CE0` (table at P+0xC40 via `pes_fn_1406D9010`) is not decoded.
2. `closure/pes_fn_14079ED20.cpp` holds `OUT-OF-SET CALL 0x14079F760` (a loud `pes_dispatch_missing`). 0x14079F760 is not in the layer; not reached in these nine runs.
3. `gen_translated` prints "held out" for three recorded roots: 0x1403BB670 (via 0x14037CB50), 0x1405549C0 (via 0x140555110), 0x14202BA30 (via 0x14202C250). Pre-existing; not changed here.
4. Process gap: `gen --check` reads the disk, not the index, so untracked regenerated files pass it. A pre-push guard would catch this: `git ls-files --others --exclude-standard src/pes/translated` must be empty, or run the check on `git archive <tip>`. Not implemented (outside this assignment).

## Killed theories
- "gen_translated dropped the seven": no; it emits them (in-memory build).
- "the closure JSON union lost members": no; 86 members at all three tips.
- "the compile error is still on the tip": no; only in `44688747`.
- "0x140791DD0 is a separate untranslated path": no; same caller frame as the 0x14073DD50 aborts.

## Cleanup
`build/` deleted; `/tmp/mf-clean` (clean export) deleted; `extern/godot-cpp` symlink restored after the git operations.
