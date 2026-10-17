# MotionRecords: one motion-record read for MOVESTEP groups A, B, D (review items 2 and 3)

Branch `MotionRecords` (base `02ceef2`). Pushed, confirmed by `git ls-remote`:
- `b88f7a7` the port, the single read and the group changes (WIP: its tests were not yet green).
- `3f7554a` smoke-app loader call, test fixtures on the record, the loader-decoded window test. Head.

## Result

Partially done. The port, the single read and the deletions are done and the touched binaries are green.
Not done here: the smoke run (Main's rule change moved the gate and smoke to Main, so I did not run
them), and the DECELERATE argument re-check (see Notes).

## Changes

- `src/pes/mbinfo.{hpp,cpp}`: the motion table. Per motion, the lists PES's builders fill
  (FUN_1406aef60 and FUN_1406ac4d0..FUN_1406ae670), count-capped in file order, with the caps read from
  the exe at DAT_1425dc370 (`LoadMotionCaps`). Kinds: Animation 0, SubHitData 2, CancelData 5, RotData 6,
  FootType 9, PivothitInfo 12. Motion masks per builder: Animation/CancelData/RotData `w0&0x1fff`,
  SubHitData `(w0&0x7ff)+0x61`, FootType `(w0&0x1fff)+0x10`, PivothitInfo `(w0&0xfff)+0x20b`.
  Readers answer with PES's zero-word fallback (`RotRecord{}` = offset -878, start -517, end 1).
  `ParseMbinfo(bins, caps)`, `LoadMbinfo(vfs, exe)`.
- `src/pes/movestep.hpp`: `MoveHost::Motion(motion)` is the single record read. The five group A readers
  (`MotionFixedFrames`, `MotionStartFrames`, `MotionTotalFrames`, `MotionSubFrames`, `MotionSubCount`) are
  gone. New `MoveHost::ClipFlag()` (P+0x2472, the mirror argument of FUN_1406b1c70). Shared
  `movestep::RecordTicks` (int(2v/rate * ticks/60 + 0.5)).
- `src/pes/movestep_turn.cpp` (group D): `RecordWord` and its float-pattern decoders (`RecordSubFrames`,
  `RecordByteAt`, `RecordFootPair`) are deleted. The helpers now read the record. Fixed on the way:
  `RecordEndFrames` read bits 16..25; PES's FUN_1406b1d00 reads bits 0..9. Two `(void)` casts that belonged to
  deleted decoders are gone. The count of `(void)` in the file is 35 (was 37). The rest are on values that were
  never decoders (e.g. `echoRow`, `win`, `mirrorSlot`), left as they are.
- `src/pes/movestep_run.cpp` (group B): `HitRecordCount` and `HitFirstTicks`/`HitTotalTicks`/`HitEndTicks`/
  `HitSignedTicks` are gone. `ReadClipHits` reads the live motion's record: RotData entry 0 (total, end,
  signed offset), the CancelData entries 0..2 with the Animation blend as the fallback. GOTO_RUN's `clipLen`
  was a `req` stand-in and its `clipTotal` was `hit0` (wrong variable); both now come from the record.
- `src/pes/movestep_group_a.cpp` (group A): six readers go through `MotionTicks` on `Motion()`.
- `src/sim/onthepitch/pesmovestep.cpp`: `Motion()` answers from `match_.GetPesMbinfo()`. `ClipFlag()` is a
  ponytail returning false.
- Callers of `LoadMbinfo` updated: `src/gdext/pes_library.cpp`, `apps/gfsim_run.cpp`, `tests/pes_dodge_test.cpp`,
  `tests/sim/pes_match_constants.hpp`.
- Tests: `tests/pes_mbinfo_test.cpp` (rewritten: builder semantics on synthetic bins, two install tests);
  `tests/pes_movestep_{test,run_test,turn_test,step_test}.cpp` (fakes answer `Motion()`; fixtures re-based).

## Tests

- Install, `pes_mbinfo_test` `PesMbinfoInstall.TheMotionTableHoldsTheFirstRecordsOfEachMotionAsTheBinsSay`:
  the first records of 64 motions from `RotData.bin` read back equal to an independent decode of the raw
  words. Ran against the install, 296 ms (not skipped). `EveryKeptListIsNoLongerThanItsCap`: 216 ms.
- Synthetic: `RotDataReadsItsThreeFieldsAndKeepsTheCap`, `FootTypeAndPivotUseTheirOwnMotionOffsets`,
  `SubHitDataUsesItsOwnMotionOffset...`, `CancelDataFramesAndCountFeedTheReaders`.
- Branch: `pes_movestep_turn_test` `RunTurnRun.TheReorientWindowIsTheDecodedRecordsSpan`: the
  loader decodes a RotData record (end 2 frames vs 18 frames); the same request reorients only under the
  longer one.
- Fixture change (behaviour, not just plumbing): the turn host's default record is now the one its tests
  were written against (start -930 frames, end 1615, window (-930, 342]). The old float-pattern record was
  garbage, and its end was about 1,617 frames. Three zero-word tests keep the zero record explicitly.
  Group A `IdleTurn`: the record gives a total of 4 frames (the old fixture's 3 cannot be reached
  through the integer frame conversion), so the lock writes 3 ticks where it wrote 2.
- Mutation checks (applied, built, run, reverted; `git diff` clean afterwards):
  - RotData end read from bits 16..25: `RotDataReadsItsThreeFieldsAndKeepsTheCap`, the install table test and
    `RunTurnRun.TheReorientWindowIsTheDecodedRecordsSpan` fail.
  - RotData cap +1: `RotDataReadsItsThreeFieldsAndKeepsTheCap` fails.
  - FootType motion offset 0: `FootTypeAndPivotUseTheirOwnMotionOffsets` fails.
  - PivothitInfo flag bit 28 (isolated): the pivot assertion fails.
  - `RecordTicks` without the 60 Hz divisor: 2 group A (`IdleTurn`, `IdlePkGk`) and 2 group D tests fail.
  - Gap: no group B test changes under the tick conversion (`pes_movestep_run_test` fixture is zero), so the
    group B reads have no test.
- TDD order, disclosed: the loader was written before its tests (I did not see its red first). Tests and
  mutations were run afterwards. Group D/A tests were red during migration (the fixtures and expectations
  were re-derived; see above).

## Verification

- Full build (`cmake --build build -j3`), after the smoke-app fix: exit 0, including the Godot glue.
- Touched test binaries (direct, not via ctest): `pes_mbinfo_test` 8, `pes_movestep_test` 41, `pes_movestep_run_test` 35,
  `pes_movestep_turn_test` 41, `pes_movestep_step_test` 13, `pes_dodge_test` 45, all PASSED. With `PES21_ROOT`.
- Gate (ctest, under `gf-sim.lock`, before Main's rule change): 99% passed, 1 failed out of 931. The failure:
  `godot_match_scene_test` SIGSEGV (signal 11) inside the GDExtension while creating a `PesLibrary`-taking node.
  A direct solo run of the same command (`tools/godot.sh --headless --path godot res://tests/test_match_scene.tscn`,
  with PES21_ROOT) exits 0 with no signal 11 (17 s). Matches the SceneCrash note (parallel `-j3` gate flake).
  Not checked on the origin/main baseline. The solo ctest re-run was cancelled under Main's rule.
- Smoke: not run by me (Main runs the gate and smoke per batch). Main's line is `[determinism] 63286486ec6c84de`
  per the brief; the handover records a later value `2f6242c5556975df`. The smoke will move. I have no
  before/after number.

## Notes and open items

- PES no-record value is not 0. PES evaluates the same formula over a zero word, so a missing RotData record
  reads start -517 (= -930 frames), end 1 (= 2 frames), offset -878. Group B's `0` stand-in was wrong for these.
  Assumption: PES's fallback object DAT_1436f23e8 is zero-filled (taken from the group B port's note; not
  read from the exe).
- HitData (kind 1) in PES is indexed `(w0&0x1fff)+0x11` (FUN_1406ad300). GF's `Mbinfo` (and `pesdodge`'s KindRow)
  uses `w0&0x1fff`. That looks like a 0x11 mis-index in existing GF code. Not changed here (out of scope).
- Legacy `Mbinfo::Cancel(m)` keeps last-record-wins; the motion table keeps PES's cap order. Two parses of
  Animation and CancelData remain. Fold them into one when the legacy readers move.
- DECELERATE (FUN_1406bcd20): its record arguments were not re-decompiled. I took them from the port's
  ACCELERATE mapping, which the port uses for it. GOTO_RUN and ACCELERATE were decompiled and checked.
- GOTO_RUN's `local_104 = max(blend, last cancel)` (its `iVar13`) is not in the port and is not added.
- Group C reads no motion record (its only record read is P+0x2468, `RecordF2468`). So "B, C and D" is B and D
  plus A, which I also moved.
- `MotionStartTime` (FUN_1406b97d0's switch) is a stand-in returning 0 and is still used by group D's footprint
  start. It is not a record read; not touched.
- ponytails added: `MoveHost::ClipFlag()` (P+0x2472 writer not decoded); `SubRecord` words not typed (the L4
  solver FUN_1406aff90 / FUN_1406b05e0 reads them).
- Not verified: how many PivothitInfo records map past the 0x1125-entry table (dropped by the loader).
- Group B port deviations noticed and left: GOTO_RUN's rolled value uses `PickN * kmh8 / ticks`, PES uses
  `runtime * k / 60`. Not in the scope of this change.
