# PES port Cutover7: the branch builds clean, every pes_* suite is green, the forfeit segfault is fixed; the loose-ball stall, C4, stale flags and the keeper branch stay OPEN

Branch `Cutover` (origin), on `6fc4381f` (main). Pushed, `git ls-remote origin Cutover` = `1472df11`:
- `a44196c5` merge of origin/main into Cutover (docs only).
- `34904960` restore `bsBoxSkipCount = 2.0f` and `bsAngleClass = 4.0f` in the carrier loader (zones test green).
- `b71c0a66` the two throw-in carry tests run on seed 3's seamless throw-in; the loose-ball test is re-disabled with its measured red.
- `dc448247` pes_team_tick_test reads the mark table's positions from the man records (the branch did not compile without it).
- `1472df11` OppNear reads only active opponents (the `pes_forfeit_test` segfault).

Labels: CONFIRMED = read from code, a log, a gdb backtrace or a test run; INF = inference; OPEN = blocker named.

## 1. Result

- **Build:** `cmake --build build -j2` (all targets) rc 0 at `1472df11`. CONFIRMED.
- **Every `pes_*` suite: green.** 96 binaries (`build/tests/pes_*_test`, `build/tests/sim/pes_*_test`, `build/tests/pilot/pes_*_test`), each through `tools/dev/simslot.sh timeout 3600`: rc 0 for all 96. CONFIRMED (sweep3/summary.txt).
- **Red 1, `PesCarrierInstall.TheZonesAndLaddersAreTheGamesOwn`: fixed** (`34904960`). pes_carrier_test 187/187.
- **Red 2 and 3, the throw-in carry tests: fixed in the test** (`b71c0a66`). Both pass on seed 3's seamless throw-in (2/2).
- **Forfeit segfault, `PesForfeitMatch.ASideAtTheMinimumKeepsPlaying`: fixed** (`1472df11`). Root cause, red, mutation and restore are in section 3.4. pes_forfeit_test 9/9.
- **C3 loose ball: OPEN.** No change landed. Section 4.
- **C4 goal-kick spot, stale `prepared`/`started`, outfield carrier on the keeper branch: OPEN.** Section 4.
- **GF mode unchanged:** switch-off 2-min and 5-min seed 7 hashes are the Cutover6 values (section 2).

## 2. Repros (final HEAD `1472df11`, sweep3)

| run | result | hash | ticks | notes |
|---|---|---|---|---|
| PES 2 min, seed 7 (smoke) | HOM 4-0 | `8f62fef96d033a84` | 27352 | idle 0 episodes; equals Cutover6 |
| GF 2 min, seed 7 (smoke) | HOM 1-2 | `f5fff64a9fd74b8c` | 24274 | equals Cutover6 |

Measured at `34904960` (the code of the 5-min runs; the later commits change no PES decision on full sides, see section 3.4):

| run | result | hash | ticks | breaker prints (`[stalemate]`) | idle episodes |
|---|---|---|---|---|---|
| PES 5 min, seed 7 | HOM 1-0 | `45e830a988ff01cf` | 28663 | 2 | 2 |
| PES 5 min, seed 3 | HOM 2-1 | `2c0749ccc8ca6735` | 27548 | 2 (one episode) | 1 |
| GF 5 min, seed 7 (switch-off) | - | `7ca3d08f68567016` | 31521 | (GF) | 28 episodes, 14 stalls |

Both 5-min PES hashes equal Cutover6's "after `8154ca73`" column. CONFIRMED.

## 3. Fixes and the red tests

**3.1 `TheZonesAndLaddersAreTheGamesOwn` (pes_carrier_test).**
- Red: `bsBoxSkipCount` and `bsAngleClass` read 0 (`pes_carrier_test.cpp:1032,1041`, "0 vs 0"). CONFIRMED (log).
- Made red by `4ff00922` (Cutover4), which deleted the two loader lines in the same hunk as the S1 rows (`git diff origin/main HEAD -- pescarrier.cpp`). CONFIRMED. With both at 0, the box-skip and facing gates are open for every man.
- Main keeps both lines (`pescarrier.cpp:378-379`); green on main by inspection (the test asserts only `> 0`). INF, not re-run on a main build.
- Fix: restored with their provenance comments (`cmp [lVar15+0xb48],2`, `cmp [recB],4`; docs/re/pes-re-carrier-helpers.md:79). They are compare immediates, not floats, so no install read exists yet (open item 6).

**3.2 The two throw-in carry tests (pes_restart_delay_test).**
- Red on HEAD and on main: both fail with "no throw-in carrier picked the ball up". CONFIRMED.
- Introduced by `66d5d18e` (`ACarriedThrowInReachesItsSpot`) and `8e8a9254` (WIP, `AThrowInCarrierStandsAtThePickupClipsContactOffset`). Both ran seed 7 for five minutes, and `LONG_MINUTES` says seed 7 "has no seamless throw-in within it" (pes-port-ThrowInTaker.md). The walker pickup works when a walker exists: in one run the taker picked up at tick 21222 and carried to his spot (`trace_carry.log`). CONFIRMED.
- Which commit made them red: NOT BISECTED (budget). INF that the seed-7 trajectory stopped producing a walker somewhere between `66d5d18e` and HEAD.
- Fix (the test was wrong): both run on `SEAMLESS_SEED` (3) with the twenty-minute search the path-A tests use. `[  PASSED  ] 2 tests` (carry_seed3.log). CONFIRMED.
- `pes_restart_delay_test` as a whole binary: rc 0 in sweep3 (3600 s cap; the earlier rc 124 was the 1800 s cap). CONFIRMED.

**3.3 Build break (blocked the branch).** `pes_team_tick_test.cpp` used `Facts::holderX`, `Facts::holderZ` and `Candidate::posX`, removed by `4ff00922` (positions come from the man records). Ported as `pes_mark_table_test` does (`ManAt`, a `PesWorld`), same numbers and expectation. `dc448247`. CONFIRMED (build rc 0; test rc 0). Main's FoulOffside2 port is the same change on its branch.

**3.4 `pes_forfeit_test` segfault (Main's report; fixed).**
- Red: `PesForfeitMatch.ASideAtTheMinimumKeepsPlaying` segfaults (rc 139) on the branch; passes on main (Main's gates). CONFIRMED.
- gdb backtrace (`forfeit_gdb.log`, Release, no line info): `SIGSEGV in pes::offball::(anonymous)::OppNear(RunWorld const&, int, float)` <- `TeamAIController::ProcessRunManager(Player*)` <- `TeamAIController::Process` <- `Team::Process` <- `Match::Process` <- the test. CONFIRMED.
- Mechanism (CONFIRMED, code): `PlayerFacts::pos()` reads the man record (`Rec()`, pesoffball.hpp:196). `BuildRunWorld` binds a record only for active players (teamAIcontroller.cpp:3475-3485), so the empty slots of a side short of eleven (four sent off here) hold `man == nullptr`. `Rec()`'s assert is compiled out in the Release build, so `OppNear` loops over all 11 opposing slots and reads a null record. Before Cutover3 (`99f1e0bc`, "PlayerFacts reads its position ... from the PES man record") `PlayerFacts` held its own position fields, which were zero for empty slots, so the read was harmless.
- Fix (`1472df11`): `OppNear` skips inactive slots, as every other slot loop in pesoffball.cpp does (lines 531, 544, 631, 746, 805, 1039, 1071, 1104, 1208, 1441, 1520, 1654, 1685, 1826). All five callers (lines 675, 960, 1464, 1698, 1874) route through `OppNear`, so this is the one shared place.
- Red and mutation: the existing test is the red. Mutant (guard removed, rebuilt): rc 139 again (`mut_guard_off.log`). Restored: `pes_forfeit_test` 9/9 green (`forfeit_fix_full.log`, 574 s), and the sweep rebuilt the binary and passed it again (rc 0).
- Cutover-caused? The regression is the record-backed read Cutover3 introduced. Branch-only commits do not touch `OppNear` or `ProcessRunManager` by symbol (`git log origin/main..HEAD -S`: no hits). CONFIRMED.
- Effect on normal matches: the guard changes nothing when a side has eleven active players, because no empty slot is read. The 2-minute PES and GF smoke hashes are unchanged (section 2). INF that the 5-minute hashes are unchanged too; not re-run after this commit.

## 4. C3 loose ball, C4, stale flags, keeper branch (OPEN)

**Translated check.** `FUN_140962440` (PES ActionPress, pes-re-press-life §8) is NOT in `src/pes/translated/` nor in `tools/dev/translate/results*.json` (grep). The hand port in `pespress.cpp` is what the code uses. CONFIRMED.

**Loose-ball test.** `ALooseDeadBallInPlayIsReachedAndPlayed` (bound 270 ticks = the 5 s stalemate clock): RED on HEAD, seed 7 longest 369 ticks, seed 3 640. It is DISABLED with its reason (`b71c0a66`).

**Mechanism at HEAD (CONFIRMED, code).** In `elizacontroller.cpp`'s dribble block, a designated man within 1 s of a ball at rest, with nobody holding it, gets no movement command on the PES path: the GF chase step is off there (line 629) and nothing replaces it. The far branch has PES's ActionPress.

**Attempt (not landed, reverted).** The far-branch ActionPress moved into a lambda and called from the near branch on the PES path.
- Seed 3 loose test passes; seed 7 fails: a new episode of about 30 s at (-37.3,-2.9), the forced man A4 at 1.1-2.3 m, a breaker print every 3 s (`green_loose_s7.log`). Worse than HEAD.
- Press trace (`trace_press_s7.log`): the command goes out every tick at 3.75 m/s (the class-1 cap), but the man's speed stays at 1-3 km/h and his clip frame restarts near 0 (`idle/000_accel` 0/27, 1/27). The command is not realised as motion. CONFIRMED (trace). The cause is OPEN (clip selection, not read).
- 5-min seed 3 with the attempt: `[idle] 3 episodes` (HEAD: 1), hash `6f9dace4a0f5644c`, 33958 ticks (`c3_pes_s3.log`).

Breaker firings and idle episodes, before and after: BEFORE (HEAD) seed 7: 2 prints, 2 episodes; seed 3: 2 prints, 1 episode. AFTER: unchanged (no C3 change landed).

- **C4 goal-kick spot:** not traced. The spot is `GoalKickSpot` (pessequencer.cpp:45, goal-area line plus push-out). The midfield holds (Cutover6: seed 3 ticks 1385, 21959; seed 7 ticks 25656, 27181) are not explained. OPEN.
- **Stale `prepared`/`started` after a taken restart:** the taken exit (referee.cpp:716-726) clears `active` only. Every arming site resets both (referee.cpp:137-141, 290-291, 394-396, 442-444, 519-521, 840-842, 875-877, 1000-1001, 1188-1190), so they do not block the next restart (CONFIRMED). They do mislead the `[stalemate]` print ("active 0 prepared 1 started 1"). A PES-only clear is needed because GF reads these flags. OPEN.
- **Outfield carrier on the keeper branch** (`elizacontroller.cpp:412`): not changed. OPEN.

## 5. Mutations (all run, rebuilt, reverted)

- Forfeit guard: removed, rc 139; restored, 9/9 green. Listed in 3.4.
- C3 approach: the attempted change turned the seed-7 loose test red (369 -> a 30-s episode); reverted. Listed in section 4.
- Zone fix, carry tests, team-tick port: red -> green was run; no separate mutation run. Not reported as mutation checks.

## 6. Proof

- `cmake --build build -j2` (all targets): rc 0 at `1472df11`. CONFIRMED.
- Sweep (sweep3): 96 `pes_*` binaries, every one rc 0; `build-all` rc 0; smoke PES and GF rc 0. CONFIRMED. (The earlier count of 86 in the first draft of this report was a miscount.)
- Carrier suite 187/187; carry tests 2/2 (seed 3); forfeit 9/9; team-tick rc 0; restart-delay rc 0.
- Gate: not run here (Main gates).

## 7. Open items (numbered)

1. C3 loose ball: the press command is issued but not realised as motion (1-3 km/h, clip frames restart). Needs the clip-selection cause decoded, then the PES first touch (`pes-re-first-touch`, FirstTouchPort). The loose-ball test stays disabled until then.
2. Which commit moved seed 7's throw-in trajectory (not bisected).
3. C4 goal-kick spot at midfield (trace the hold placement).
4. Stale `prepared`/`started` flags after a taken restart (PES-only clear; GF reads them).
5. Outfield carrier on the keeper-distribution branch (`elizacontroller.cpp:412`).
6. The two carrier compare immediates (`bsBoxSkipCount`, `bsAngleClass`) are still literals; read them with `ReadImm` once their VAs are decoded.
7. Coverage: the 5-minute PES hashes were not re-run after `1472df11` (INF unchanged; full sides only).
8. Other `PlayerFacts` readers were not audited beyond `OppNear`: `Rec()`'s assert is compiled out in Release, so any future unguarded read of an empty slot will crash the same way.
