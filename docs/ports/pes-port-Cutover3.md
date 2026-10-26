# PES port Cutover3: PlayerFacts reads position, velocity and facing from the PES man record (step 3 not done)

Branch `Cutover` (origin). Commits: `e577f68a` merge of origin/main (docs only); `1c0f1c02` red test; `99f1e0bc` green view and single writer (pushed, `git ls-remote origin Cutover` = `99f1e0bc`); a comment-only commit for the ponytail note (below). Worktree `<workdrive>/gf-Cutover`.

Labels: CONFIRMED = read from code, a test run, or a repro this pass; INF = inference; OPEN = blocker named.

## 1. Result

- Steps 1 (view), 2 (one writer, copies deleted, tests migrated) and 4 (proofs for those) are done. Step 3 (switch one translated function) is NOT done: section 5 gives the evidence and the blocker.
- Acceptance: positions, velocity and facing of `PlayerFacts` live in the man record and are read from it; the `PlayerFacts` copies are deleted; GF mode unchanged (repro hash equal to main); pushed.

## 2. Fields moved (man record, offsets from the record start; block = PES slot's 0x3F40 block, man at +0x32C)

| PES field | GF source (writer: `BuildRunWorld`, teamAIcontroller.cpp:3445-3461) | Unit in the record | Label |
|---|---|---|---|
| man +0x55C x | `Flat(GetPosition()).x` = coords[0] | m | offset CONFIRMED (docs/re/pes-re-player-record.md 2.2); unit INF (range fits metres) |
| man +0x564 z | `Flat(GetPosition()).z` = coords[1] | m | CONFIRMED (header comment "PES z (GF y)") |
| man +0x560 y | coords[2] of GetPosition() (height) | m | INF; no PES-path reader yet |
| man +0x574 vel x | `GetMovement().x / kPesFps` | m per frame | unit from the RE doc and the header; GetMovement in m/s CONFIRMED (`Predict`, teamAIcontroller.cpp:3080) |
| man +0x57C vel z | `GetMovement().z / kPesFps` (coords[1]) | m per frame | as +0x574 |
| man +0x578 vel y | `GetMovement()` coords[2] / kPesFps | m per frame | INF |
| man +0x554 facing | `pesDeg(GetDirectionVec())` (degrees, [0,360)) | degrees | INF: unit is GF's bearing; PES writer not decoded |

Not moved: `aim` and `slotPoint` (not block fields in this slice).

## 3. Copies deleted

- `PlayerFacts::pos`, `vel`, `facing` (pesoffball.hpp): replaced by `man` (record pointer) and accessors `pos()`, `vel()` (m/s, = record × kPesFps), `facing()`, and setters `SetPos`, `SetVel`, `SetFacing` (test and writer use).
- BuildRunWorld's three copy writes; the PES-layout writes replace them.
- Consumers converted (mechanical, via `xd://ast_edit` plus fixes): pesoffball.cpp (165 reads), teamAIcontroller.cpp (6 mark-table reads), pes_offball_test.cpp (196 writes, 17 reads, plus 4 binding sites). Two BallEvent `pos` fields were wrongly matched by the codemod and restored by hand.
- Helpers (pesworldrecord.hpp): `kManFacingOffset` 0x554, `kManPosYOffset` 0x560, `kManRecordBytes` (0xDC4), `LoadF32`, `StoreF32`.
- `kNoManRecord` (pesoffball.hpp): one shared zero record for a player with no record.

## 4. Proofs

- **Red, observed (1c0f1c02):** `pes_offball_test` does not build: `'struct pes::offball::PlayerFacts' has no member named 'man'`, `'StoreF32' is not a member of 'pes'`, `'kManFacingOffset' is not a member of 'pes'`.
- **Green:** `pes_offball_test` 76/76 (73 existing + 3 new `OffBallRecord`: facts read their record; no record reads zeros; setters write PES units). The first green run had 4 failures (NearestList x3, OffBallRecord.WithoutARecord): unbound players wrote into the shared zero record. Fixed by binding (`Bind()`, `Records()`) in `Spread()`, `TrapScene()`, `Running()`, `Place()`.
- **Touched suites** (simslot, PES_EXE, PES21_ROOT, GF_DIFF_PYTHON set): pes_world_test 6/6, pes_event_wiring_test 4/4, pes_contact_test 37/37, pes_team_sheet_test 5/5, pes_mark_table_test 19/19.
- **Mutation M1** (`vel()` without `* kPesFps`): RED: `OffBallRecord.FactsReadTheirManRecord` (0.25 vs 13.5), `OffBallRecord.SettersWriteTheManRecordInPesUnits` (lines 1669-1670), and the existing `OffBallReach.FramesToReachGrowsWithDistanceAndFallsWithSpeed` (122 vs 118). Reverted; `git diff` empty; 76/76 after the revert.
- **Repro** (seed 7, 5 min, `build/apps/gfsim_run --db-sql assets/fixtures/teams.sql --home 1 --away 2 --seed 7 --minutes 5 --pes21-root <workdrive>/PES21`, `--pes-loop` added for the second): switch-off `[determinism] 7ca3d08f68567016`, 31521 ticks = main; `--pes-loop` `[determinism] 67d115b984bab251`, 25398 ticks = main. Logs: `gf-Cutover-scratch/repro_off_view.log`, `repro_pl_view.log`.
- **Not run:** the gate (Main gates); `driver.py --check`; a per-call agreement run (no switch was made).
- **PII:** grep of the five changed files for machine paths and machine paths: clean.

## 5. Step 3 (switch one translated function): not done, and why

1. **Both-mode bearings are libm.** GF's bearings are `std::atan2`: `PBrg` (pesoffball.cpp:482), `Bearing` (:107), `PVec` (:486). The translated bearing `pes_fn_140439E50` (src/pes/translated/ai/) folds through the PES polynomial `pes_fn_140A92D40`, so it is not bit-identical. Any switch on a both-mode path (for example `FramesToReach`, called from `PassToMeStop`, pesoffball.cpp:1471) changes the GF-mode hash, which the brief forbids.
2. **PES-path-only bearing hand ports exist, but read snapshot copies and run at the wrong time.**
   - S1 pair table: `BearingDeg` at pesscorerows.cpp:124, inside `if (pesPath)` (:115), with a `std::atan2` stand-in (:26). Its inputs are `RowPlayers` x/z copies. It runs in `TickScoreRows` (match.cpp:881), before any team's `BuildRunWorld`, so the block there is one build behind. Switching it needs the writer moved earlier, which changes the snapshot time of every GF copy. A decision for Main.
   - Mark table: `PairBearing` (pesmarktable.cpp:60-62), `ponytail: std::atan2 stands in for PES's atan2 polynomial 0x140A92D40`. This is the natural S3 candidate, PES-path only if `TickMarkTable` is gated (to verify). It needs record VAs in `Candidate`/`Facts`, which are copies (posX/posZ, holderX/Z, gateMates). The mark-table tests build them from positions only, so they need records too.
3. Proposed S3, after Main decides item 2: carry man VAs in `Candidate`/`Facts`; switch `PairBearing` to `pes_fn_140439E50` under `pesPath` only; a per-call agreement counter (temporary) to show the ULP-level difference from the libm stand-in, with each difference explained; delete the PES-path `PairBearing` use.

## 6. Semantic changes Main must read

- **Live view over a snapshot.** Before: each `RunWorld` froze positions at its own build. Now a `RunWorld` reads the block, which every `BuildRunWorld` (either team, on its run frame, teamAIcontroller.cpp:3526) overwrites. A team's `lastRunWorld`, read between its builds, now sees the latest build by either team. The repro agrees with main in both modes for seed 7 and 5 min, so no decision changed there. Not proven for other seeds or lengths.
- **Velocity round trip.** GF m/s → m/frame (÷54) → m/s (×54): ulp-level changes. The repro hashes are equal.
- **Shared zero record hazard.** `kNoManRecord` is one object. A write into an unbound `PlayerFacts` changes every unbound player. Writers bind first (BuildRunWorld; tests via `Bind()`). Follow-up: make unbound writes impossible.
- **Writer stand-in.** BuildRunWorld is GF's stand-in for the PES per-tick movement writer (not decoded). A `// ponytail:` note names it (teamAIcontroller.cpp:3445).

## 7. What remains (S2/S3), numbered

1. Step 3 (section 5): the switch, its agreement run and the hand-port deletion.
2. Snapshots that still copy positions: mark table (`Facts`, `Candidate`: posX/posZ, holderX/Z, gateMates); carrier (pescarrier.hpp:1011-1015: pos, velPerFrame, facing_deg, action); press (pespress.hpp:215, :312: posX/posZ); dodge gate (pesdodge.hpp:270-289); reaction block `PlayerFacts` (pesreactionblock.hpp:104, a `FUN_140a48a10` copy); mover (pesmover.hpp:13, :26); S1 `RowPlayer` x/z (pesscorerows).
3. Block fields not moved: man +0x5D0/+0x5D4/+0x6B8 (action, prev, repeat: writer `0x140A6CA10` translated, source of the action word OPEN); +0x5B0 (movement state, writer OPEN); +0x5EC/+0x5F4 (the `aim` point, INF); slot +0x1414, +0x1430, +0x1438, +0x1A/+0x1C/+0x22 (writer `0x14092ED30` untranslated).
4. Units INF: height (+0x560, +0x578) and facing (+0x554) units; confirm from the PES writer or a live read before a PES reader uses them.
5. Remove the shared-zero-record hazard (section 6).
6. From Cutover2 (unchanged): side tables G+0x1928/0x1978 (S5/S7); MI struct for S3 phase words; block allocator (OPEN-1); anime class writer (lane-units2 S12).

## 8. Checks for Main

- `tools/dev/gate.sh` with `GF_DIFF_PYTHON`, `PES_EXE` and `PES21_ROOT` set, from `<workdrive>/GodotFootball` (the gate holds both sim slots).
- `tools/check_pii.sh` from `<workdrive>/GodotFootball`.
- A 2-minute smoke on the merged tree (tripwire only).
