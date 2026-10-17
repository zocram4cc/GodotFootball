# NearestList: PES nearest lists replace OppByDist

Result: Done for the port. The PES writer of `MA+0x22e4c` stays OPEN (ponytail in `BuildNearest`).

Branch `NearestList` (base `0a1784b`), pushed; `git ls-remote` confirms `bae7ed4`.
- `f9e643d` WIP red tests (stub `BuildNearest`)
- `2a3050f` green: NearestEntry, BuildNearest, NearestOpponents, OppByDist deleted, 7 callers moved
- `bae7ed4` ponytail note cites the reader VAs

## Changes
- `src/sim/onthepitch/pesoffball.hpp`: `NearestEntry` (200 bytes, `static_assert`; team[2][11] of `{int id; float d2}` at +0x00/+0x58, count[2] at +0xb0, rank[22] at +0xb2), `kNearRankAbsent = 0x0a`, `RunWorld::nearest[22]`, declarations of `BuildNearest` and `NearestOpponents`.
- `src/sim/onthepitch/pesoffball.cpp`: `BuildNearest` (stable sort by d², ties keep slot order, active only, ranks by global id), `NearestOpponents` (opponent block, count-bounded span). `OppByDist` deleted; its 7 callers (`L68a8c0`, `L68efc0`, `L68e900`, `L691ef0`, `L692fe0`, `L690cf0`, `L68d0b0`) read `NearestOpponents`.
- `src/sim/onthepitch/teamAIcontroller.cpp`: `BuildRunWorld` calls `ob::BuildNearest(w)`.
- `tests/sim/pes_offball_test.cpp`: `Running()` and each of the 23 `RunStop` call sites rebuild the list (the list is a snapshot; tests move players after building it). 5 new tests: `NearestList.OpponentsAreSortedNearestFirst`, `EqualDistancesKeepSlotOrder`, `CountsHoldOnlyTheActivePlayersOfEachTeam`, `RanksGiveTheSlotWithinItsTeamsListAndAbsentForInactive`, `ATeamOneOwnerListsTeamZeroAsOpponents`.

## Tests, red to green
- Red: stub `BuildNearest` / `NearestOpponents`; `./build/tests/sim/pes_offball_test --gtest_filter='NearestList.*'` → 5 FAILED, all on assertions.
- Green: `pes_offball_test` 52/52 (47 existing + 5 new).

## Mutations (each alone; rebuilt, run, reverted; `git diff` empty after each)
| # | Mutation | Failing tests |
|---|---|---|
| M1 | comparator `a.d2 > b.d2` | 4: OpponentsAreSorted, EqualDistances, RanksGive, ATeamOne |
| M2 | fill order reversed (ties) | 1: EqualDistances |
| M3 | `count = kSlots` | 2: CountsHold, RanksGive |
| M4 | rank = slot index | 1: RanksGive |
| M5 | absent rank 0 instead of 0x0a | 1: RanksGive |
| M6 | accessor returns own team | 4: OpponentsAreSorted, EqualDistances, CountsHold, ATeamOne |
| M7 | inactive players kept | 2: CountsHold, RanksGive |

## Verification
- `cmake --build build --target pes_offball_test pes_challenge_counters_test gfsim_run`: builds. The only warning is pre-existing (`humanoid.cpp:2864`, `-Wdeprecated-enum-float-conversion`).
- `pes_offball_test` (PES21_ROOT set): 52/52.
- `pes_challenge_counters_test` (PES21_ROOT set): 3/3.
- `ninja -t deps`: 0 objects with `#deps 0`.
- Path scan of changed src/tests for machine paths: no matches. `tools/check_pii.sh` not run here (fails in a worktree); Main runs it.
- Gate not run (Main gates).

## Smoke (2-minute tripwire, `gfsim_run --seed 7 --minutes 2`, two runs)
- Both rc=0. Determinism `49476a9e1e0b39d9` on both runs.
- Full match complete. Passes 14-13, shots 2-1, goals 0-1. Ticks 36125 (cap 194400). Restarts longest 23.1 s (bound 32.5 s).
- Not run on main: no main-tree binary for `0a1784b` to compare against. Not verified: bit-identity with main. The sort is stable, and the distances are the same floats OppByDist computed, so the expected result is identical.
- Run without `gf-sim.lock`, following my contract (the shared-machine brief asks for flock).

## Open items
1. PES writer of `MA+0x22e4c` is undecoded (pes-re-teamctx §2.4). Ponytail in `BuildNearest`.
2. Tie rule is undecoded. GF keeps slot order.
3. Absent rank `0x0a` and the unread fill `-1` follow pes-re-teamctx §2.1. `0x0a` also equals rank 10, so the doc's sentinel is ambiguous. No GF reader uses the rank bytes yet.
4. Owner's own team block includes the owner at distance 0. Inferred from the 11-slot layout (INF); no reader checks it.
5. `BuildRunWorld` runs once per team controller per tick (two builds per tick). Making it one per tick needs the caller restructured; not changed here.
6. Snapshot semantics: a hand-built `RunWorld` must call `BuildNearest` after setting positions.
7. `origin/main` moved to `c22787a`. My four files are unchanged between `0a1784b` and `c22787a`, so the merge should be clean.
8. AccelRamp has the accessor (`NearestOpponents(const RunWorld&, int)`, message sent). Their adapter is theirs to wire.
