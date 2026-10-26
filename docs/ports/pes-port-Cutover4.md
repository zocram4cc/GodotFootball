# PES port Cutover4: the block writer at the tick's start; the S1 pair table and the mark table run PES's bearing on the records

Branch `Cutover` (origin). Commits: `26fe4ead` merge of origin/main (docs); `112377c1` red: the records hold the tick's start positions; `196e3add` green: the writer runs first on the PES path; `b77941c8` red: bearing API tests; `4ff00922` green: the bearing switch, stand-ins deleted. Labels: CONFIRMED = read from code, a test run or a repro this pass; INF = inference; OPEN = blocker named.

## 1. Result

- Writer at the tick's start (PES path): done. `WritePesMovementRecords` is the first statement of `Match::Process` when `pes_loop` is on. Every reader in the tick (S1 rows, mark table, run world) sees the start positions.
- Slots bound at match start: already so (`match.cpp:196-197` binds slots 0..21). The shared zero record `kNoManRecord` is deleted. An unbound `PlayerFacts` holds no record; its reads assert in debug.
- The S1 pair table and the mark-table `PairBearing` run PES's bearing `0x140439EF0` in place on the man records. Stand-ins deleted: `BearingDeg` (pesscorerows), `PairBearing` (pesmarktable), `ScoreRowConstants::radToDeg/twoPi`, `CarrierConstants::s1RadToDeg/s1TwoPi`.
- GF switch-off repro: `7ca3d08f68567016`, 31521 ticks = main. GF mode unchanged.
- pes-loop 5 min, seed 7: `9df315a3c6d7d58b`, 29719 ticks. Main was `67d115b984bab251`, 25398 ticks (Cutover3). A rerun gave the same hash.
- 2 min smoke, pes-loop: completes, `1b5b777d77932430`, 39261 ticks. Not a crash and not a tick-cap hang, but a stall pattern (section 6).

## 2. What moved

- Writer: `TeamAIController` free functions `WriteManMovement` and `WritePesMovementRecords` (teamAIcontroller.cpp, before `BuildRunWorld`). PES mode writes the movement fields (+0x55C..+0x57C, +0x554) once per tick at the start. GF mode keeps its per-build write in `BuildRunWorld`, its stand-in for PES's movement writer (not decoded; `docs/re/pes-re-player-record.md` 2.2). The per-build write cannot be deleted until GF mode retires.
- Translated bearing: `PesWorld::Bearing(from, to)` and `PesWorld::BearingOfPoints` (pesworldrecord.cpp). The translated code runs in place on the window. Its stack and two points live in a 256-byte scratch at the image's tail (`kScratchVa`). The atan2 polynomial spills to `rsp+0x20`, so a real stack is required. No allocation per call.
- S1 writer (pesscorerows): `RowPlayer` carries the man record. The PES pair table is `Bearing(man[b], man[a])`, the goal angles are `BearingOfPoints(centre, 0, p.x, p.z)` (both modes; no production reader). `TickScoreRows` fills PES-path rows from the records and GF-path rows from live positions.
- Mark table (pesmarktable): `Candidate.man`, `Facts.holderMan`, `Facts.gateMan[11]` (null = no record, skipped), `Facts.world`. `ConePasses` and `CountGate` read positions from records and call `world.Bearing`. `TickMarkTable` passes records.
- `PlayerFacts`: `man` defaults to nullptr. `Rec()` asserts it is bound.

## 3. Which PES bearing, and why

- The cone's live bearing is `FUN_140439EF0` (`docs/re/pes-re-mark-cone.md` section 2, CONFIRMED): coincident is 0; else `dx = RDX.x - RCX.x`, `dz = RDX.z - RCX.z`, `atan2` through `0x140A92D40`, `+2pi` if negative, `*180/pi`. Rcx = from, rdx = to, result in xmm0. Harness record: `translated`, `diffed`, 76 cases (`tools/dev/translate/results.json`).
- `FUN_140439E50` (the name in the brief) computes the same fold with an out-pointer. Harness record: `diffed`, 84 cases.
- The S1 writer's own call site is OPEN (decode `0x14045B420`'s owner loop). The pair table uses `0x140439EF0`.
- OPEN, found by the probe: the two translated entry points differ at the bit level in 302 of 27,460,348 pair-table calls (0 of 79,561 mark-table calls). Not decoded; INF: a wrap at 0 or 360 degrees. The cone uses `0x140439EF0`.

## 4. Agreement and difference (5 min, seed 7, pes-loop; temporary probe, removed)

Each call computed the translated bearing (`0x140439EF0`), the same pair through `0x140439E50`, and the old `std::atan2` stand-in (float, PES's 2pi and 180/pi constants). The difference is the translated value minus the stand-in, wrapped to +-180.

| Reader | Calls | Max abs diff (deg) | Mean abs diff (deg) |
|---|---|---|---|
| S1 pair table | 27,460,348 | 0.161819 | 0.012727 |
| Mark table (cone live bearing and count gate) | 79,561 | 0.161758 | 0.014425 |

The stand-in differs from PES's polynomial by up to about 0.16 degrees. A hand-worked value in `pes_scorerows_test` (atan2(52.5, 10)) moves from 79.215702 to PES's 79.218506; the test now pins PES's value and says so.

## 5. Proof

Red (observed):
- `PesTickOrder.MovementRecordsHoldTheTicksStartPositions` FAILED on main: records one move behind (slot 0 x -48.455 against the start -48.489).
- `pes_scorerows`, `pes_mark_table`, `diff_cone_test`: do not build on main (no `Bearing`, `BearingOfPoints`, `man`, `world`, `gateMan`).
- `OffBallRecord.UnboundFactsHoldNoRecord` FAILED on main: `man` was the shared record's address.

Green:
- `pes_tick_order_test` 3/3; `pes_scorerows_test` 12/12; `pes_mark_table_test` 19/19; `pes_offball_test` 76/76.
- `diff_cone_test` 1/1 with `diff_gen_cone` (ctest, Passed; corpus generated): exact equality on every corpus case, with no atan2 margin.
- Not run: `diff_gen_band`, `diff_gen_cond_arrow`, `diff_gen_strategy` (not touched). Gate: not run (Main gates).

Mutations (each applied, built, run, reverted; green after revert):
- M1 `pesscorerows.cpp:118`, pair-table operands swapped: 3 red (`PairBearingsAreOwnerMinusOtherFoldedToZeroTo360`, `PairTableIsPESsBearingOfTheRecords`, `PairTableReadsTheManRecordNotTheCopy`).
- M2 `pesmarktable.cpp:68`, cone live bearing reversed: red (`MarkTable.ConeAllowsOneMateInside`, `DiffCone.ConeTestMatchesPES`).

Repros (Release, seed 7):
- Switch-off 5 min: `[determinism] 7ca3d08f68567016`, 31521 ticks (= main).
- pes-loop 5 min: `[determinism] 9df315a3c6d7d58b`, 29719 ticks; probe run and final run identical.
- pes-loop 2 min smoke: `[determinism] 1b5b777d77932430`, 39261 ticks; no crash; the rerun was not done.

## 6. Findings (not fixed)

1. The 2-min smoke runs 39261 ticks. In-play is 29078 ticks, against a 4-minute match. Kick-off: the set piece is started, taker H2 does not kick for 5 s or more, and the stalemate breaker forces play (47 `[stalemate]` lines). The GF switch-off 5-min run also logs 60 stalemate lines, so the mechanism is pre-existing. A main 2-min baseline was not run, so whether the tick count is new is OPEN.
2. The pes-loop 5-min tick count moved from main's 25398 to 29719 (+17%). INF: the cause is the writer timing (readers no longer see a stale-by-one-build position) or the PES polynomial. Not isolated.
3. The unbound-read assert is debug-only (Release is `NDEBUG`). The death test is `#ifndef NDEBUG`. The release-visible check is `UnboundFactsHoldNoRecord`.
4. `CountGate` now skips a holder-side slot with no record. Before, a zero position counted when within 12 m. Only a team with fewer than 11 active players changes.
5. Referee and restart teleports applied after the tick-start writer reach the records one tick late (INF; not checked against PES).

## 7. What S3 needs

- Decide the S1 writer's bearing call site (`0x14045B420`): `0x140439EF0` or `0x140439E50`. Decode it; then close the 302 bit disagreements.
- Remove the remaining copies on the PES path: `RowPlayers` x/z (now block-fed but still copies), `Candidate.pairToTarget`/`pairToHolder` (S1 pair table, block-fed), `Candidate.head` (facing from the record).
- Investigate the 2-min kick-off stall and the +17% pes-loop tick count (finding 1 and 2). Run main's 2-min pes-loop as the baseline first.
- The GF per-build movement write stays until GF mode retires.
- OPEN: where PES's referee and restart writers place players relative to the tick-start writer (finding 5).
