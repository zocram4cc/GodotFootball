# pes-fix-NullHazards

Result: Done. Pushed `d124f62` to `refs/heads/NullHazards` (`git ls-remote origin NullHazards` → `d124f627de98e93ac9231f4e311dc2613fa45226`). Base: `946b760`.

Changes:
- `src/sim/onthepitch/AIsupport/mentalimage.cpp`: `GetPlayerImage` throws `std::runtime_error` for an ID not in the snapshot (was `players.at(0)`: another player's image, or out_of_range from an empty snapshot).
- `src/sim/onthepitch/player/player.cpp`: `GetClosestOpponentDistance` returns `+inf` when `AI_GetClosestPlayer` finds no active opponent (was a null deref).
- `tests/sim/sim_missing_player_test.cpp` (new, 4 tests), `tests/sim/CMakeLists.txt` (registration).

## Decisions per site

1. **GetPlayerImage: invariant, loud failure.** No PES guard decoded: the PES mental-image build is OPEN (`docs/re/pes-re-tick-order.md` §4). Callers always ask for a player they just read from an active list, and the snapshot holds every active player (`Team::InitPlayers` activates the XI; the only other activation is `Team::Substitute`, gated by `substitutions_enabled`, default false). So the throw is unreachable in the default config.
2. **GetClosestOpponentDistance: no-opponent is a designed state, +inf.** Four of five `AI_GetClosestPlayer` callers already handle null (`humanoid.cpp:90` pressure 1.0, `playercontroller.cpp:335`, `elizacontroller.cpp:331/335`, `teamAIcontroller.cpp:1558`). `Player::SendOff` forfeits at ≤6 active, so a side can run empty. +inf is the empty minimum; each consumer saturates at its far end: `humanoid.cpp:3469` (`NormalizedClamp` → 1), `humanoid_utils.cpp:203` (`Clamp01((1.6-inf)/1.2)` → 0), `humanoid_utils.cpp:328` (`1-NormalizedClamp` → 0). PES's value for an empty side is not located: named `ponytail:` in the code.

## Red → green

Red on `946b760` (baseline sources, new test file):
- `MentalImageLookupWithNoPlayersThrows`: FAILED, throws `std::out_of_range`. Not a segfault: `.at()` is bounds-checked, so this is an exception, not a crash.
- `MentalImageLookupOfAnAbsentPlayerThrows`: FAILED, returns the home player's image for an away ID, no throw (silent wrong data).
- `ClosestOpponentDistanceWithNoActiveOpponentIsInfinite`: SIGSEGV (rc 139).
- `ClosestOpponentDistanceMeasuresTheOnlyActiveOpponent`: PASSED on baseline (control, by design).

Green on fix: all 4 pass (`build/tests/sim/sim_missing_player_test`, PES21_ROOT set).

Note on the test first run: my first red attempt segfaulted in the setup (`Deactivate()` on a bench player with no humanoid). Fixed in the test (`IsActive()` guard, `FirstActive`) before the recorded red run.

## Mutations (each rebuilt and run)
- failsafe `return players.at(0)` restored: same as baseline → 2 red.
- `return PlayerImage()` instead of throw → `NoPlayers` and `AbsentPlayer` FAIL.
- `return 0.0f` instead of `+inf` → `NoActiveOpponent` FAILS.
- query own side `GetTeam(team->GetID())` → `NoActiveOpponent` and the control FAIL.

## Other callers of the same query checked
`AI_GetClosestPlayer`: teamAIcontroller.cpp:1558 (`taker &&`), humanoid.cpp:90 (`opp ?`), playercontroller.cpp:335 (`if (opp)`), elizacontroller.cpp:331/335 (`if`), :365 (stored as `forcedTargetPlayer`; `AI_GetPass` null-checks it, AIfunctions.cpp:1268). All safe. Only `Player::GetClosestOpponentDistance` (player.cpp:429) derefs unguarded.
`GetPlayerImage` callers (AIfunctions 268/287/329/498/505, elizacontroller 1587/1824/1844, playercontroller 98/604, teamAIcontroller 988): IDs come from active lists or the snapshot. The one with a possibly-null image, `teamAIcontroller.cpp:988` via `GetMentalImage(0)`, runs inside `Team::Process` after the tick's snapshot, so not reached before the first snapshot.

## Gate
- Run 1 (`ctest -j3`, PES21_ROOT + PES21_EDIT_SAVE): 889/890. The one failure: `SimMatch.AIManagerSwitchGovernsTactics` SEGFAULT at 25 s under -j3.
- That test then passed: alone under ctest (49 s), under gdb (45 s), and 6/6 back-to-back with ASLR on, unloaded.
- Run 2 (same command): **890/890 passed** (1976 s).
- Not established: whether the run-1 segfault exists on `946b760`. I did not run the baseline gate. Nothing in the diff adds a SIGSEGV path (my throw is a `runtime_error`, which would abort, not segfault).

## Smoke vs main
`gfsim_run --seed 7 --minutes 20 --pes21-root …` on the fixed build: `[balance-passing] passes 45-41 | accuracy 28%-9% | clean 24%-9% | clearances 2-3`, `[determinism] 63286486ec6c84de`. Identical to the main line in the brief. I did not rebuild a baseline `gfsim_run`: the main checkout has moved to `36108d0` since my base, so I compared to the recorded line.

## Verification
- Build: `cmake --build build -j3` pass, 0 warnings in the rebuilt objects.
- PII: `grep for machine paths on the 4 changed files: no hits.
- No struct-layout change, so no `#deps 0` check needed.

## Notes
- Run 1's segfault is unexplained (flaky under -j3; passes solo and 6/6 otherwise). Needs a baseline gate run to attribute.
- `AIfunctions.cpp:1189` (`AI_GetClosestPlayers`): `Player* closestPlayer = players.at(0).get();` is dead and throws only for a zero-squad team, which `Team`'s ctor asserts against. Not touched; one line for the owner to delete.
- Substitutions: if `substitutions_enabled` is turned on (its path segfaults today, team.cpp:189), a player who comes on is absent from earlier snapshots. Lookups of him via a delayed image now throw instead of returning another player's image. Before enabling subs, the sub path must refresh the snapshot or the readers must handle absence.
- Test-run lock use: the red/mutation runs and the 6× ASLR loop ran without `flock` (short gtest binaries, not ctest/gfsim_run). The first red attempt waited ~50 min on the lock before I switched to direct runs. Both gates and the smoke ran under `flock`.
