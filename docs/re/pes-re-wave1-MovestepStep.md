# Wave 1 A-MOV group C: the stepping states of the MOVESTEP machine

Branch `MovestepStep`, commits `575c42d` and `5f20a13`, based on `origin/main` `7543aef` plus contract
`origin/Wave1AMov` `e2d08c6` (pulled into the tree, not committed here). Files:
`src/pes/movestep_step.{hpp,cpp}`, 13-test `tests/pes_movestep_step_test.cpp`, and the CMake entries for both. The contract's own structs and loaders (`StepTables`, `StepScalars`, the angle buckets,
the playback constants, `MoveHost`) are Wave1AMov's and were pulled in.

## PES sites ported

| Handler | VA | Body |
|---|---|---|
| STEP_LOOP | `FUN_1406c4c10` | 0x1406c4c10..0x1406c64f7 (901 assembly lines) |
| STEP_LOOP_CHANGE | `FUN_1406c6480` | 0x1406c6480..0x1406c6ad0 (395) |
| TURN_STOP | `FUN_1406cab20` | 0x1406cab20..0x1406cba5d (469) |
| STEP_TO_RUN | `FUN_1406c6f80` | 0x1406c6f80..0x1406c79c0 (147) |
| RUN_TO_STEP | `FUN_1406c1070` | 0x1406c1070..0x1406c1265 (170) |
| TURN_STOP entry | `FUN_1406d2590` | 0x1406d2590..0x1406d2e64 (only group C calls it) |
| helper: the step loop may stop | `FUN_1406b6ee0` | 0x1406b6ee0..0x1406b6f47 |
| helper: the step loop may turn into a run turn | `FUN_1406b70d0` | 0x1406b70d0..0x1406b7406, with its four distance tables at 0x1425dde70 / dde80 / dde90 / dea0 (loaded by `LoadTurnDistances`) |

## State-transition table (each cell is the exit, and the site in the decompile that takes it)

STEP_LOOP (`FUN_1406c4c10`):
- the band walk restores the previous band and speed, no state change (lines 56-73);
- the slow-player re-plan → GOTO_RUN (`FUN_1406ce820`, 0x1406c6c calls);
- `FUN_1406d38d0` STEP_TURN_RUN takes the tick;
- the turn gates re-plan → GOTO_RUN;
- `FUN_1406b6ee0` true → STOP (`FUN_1406d5d60`);
- the wide turn lowers the band to 1 and halves the speed;
- the keep-tick tail (`LAB_1406c5eba`) records band/bucket and keeps the state;
- the band gate (`A = band<0 || (nextBand<3 && (nextBand<2 || kind 3 || kind 6) || !precise) || (bucket -3 or -8 outside) || (turnToLook <= drift || distanceFactor*22.5 + 45 <= |bodyToTarget|)`) decides: true → the dwell tail then STEP_LOOP_CHANGE (req[0x39]=5); false → the other tail (req[0x39]=0x15, STEP_TURN_RUN's neighbour);
- `FUN_1406b70d0` true → RUN_TURN_RUN (`FUN_1406d6780`).

STEP_LOOP_CHANGE (`FUN_1406c6480`):
- the precise branch's wide turn → STEP_TURN_RUN, or RUN_TURN_STEP at the ceiling term
  `(rateCeiling - 0.6) * clockStep`;
- the non-precise branch restores the band from the ground speed, then the same two entries;
- the arrival flag decides the change completes or re-plans through GOTO_RUN;
- the class-3 accelerate (`FUN_1406ccd00`), else the fallback stop;
- the stop fallback's gate `(kind != 4 || 0.15 <= req[0x2d])` → STOP;
- the blended root facing (req[0x3a]/0x3b]) with the clock's 180-degrees-per-tick dr+;
- dwell out → STEP_LOOP (req[0x39]=4).

TURN_STOP (`FUN_1406cab20`):
- the motion record's toe-off window writes req[0x3d]/0x3e]/0x3f];
- the first path: a small error with the stay decision and `FUN_1406b70d0` false turns the facing
  back with a step clip;
- the second path: the class-3 and bucket gates → STEP_TURN_RUN or RUN_TURN_STEP, or (non-precise)
  RUN_TURN_RUN;
- the third path: the request's own destination re-plans into the step grid → STEP_LOOP;
- the replay gate `(MI+0x116c == 2 && MI+0x1554 != 1 && MI+0x11d4 == 0) || anime vt+0x28 == 0x4a`
  picks the dwell;
- the stop's own row → IDLE (req[0x39]=1);
- otherwise the band -1 and GOTO_RUN.

STEP_TO_RUN (`FUN_1406c6f80`): both heading gates → STOP; the class 2+ non-precise band-below-1
branch → ACCELERATE (`FUN_1406ccd00`) or GOTO_RUN; the blend tail's dwell out → RUN_LOOP
(req[0x39]=3).

RUN_TO_STEP (`FUN_1406c1070`): the class -1, or either heading/body error past its gate → the
blend tail; its dwell out → STEP_LOOP (req[0x39]=4); otherwise the band restore under the dwell
window and STOP / TURN_STOP / RUN_TURN_RUN from the body turn.

## Stand-ins (`// ponytail:` in the source, each naming its removal site)

- the match's ball-position sample: `ctx+0x40` through `FUN_140a6cf70` (the port samples the
  player's own displacement);
- the roster slot: `FUN_140a627e0` (the port reads `FootBand`);
- the motion record's hit and cancel frames: `FUN_1406b1f00`, `FUN_1406b1d00`, `FUN_1406af620`,
  `FUN_1406af8e0`;
- the MI phase gates: `MI+0x116c`, `+0x1554`, `+0x11d4` (MB-E);
- the anime interface's own mirror and blend: vt+0x30, vt+0x50, vt+0xd0;
- the sidestep trap `FUN_1406d6290`'s team marking flag: `ctx T+0x255` (MB-C);
- the two feet: `FUN_141eb50b0` indices 3 and 6 (the port flips a coin through the player's own
  generator, as the orchestrator decided);
- the left/right foot positions and the look-ahead: `FUN_1404bcad0`'s rotation, not ported.

## Tests and mutation results

`tests/pes_movestep_step_test.cpp`, 13 tests, all passing (a `FakeHost` carries synthetic install
values; the request block's fields are set per branch). Mutations applied, run, and restored:

| Mutation | Caught | By |
|---|---|---|
| STEP_TO_RUN's blend-branch gate thresholds swapped (8100 <-> 2025) | yes | `StopsWhenBothHeadingGatesFire` |
| STEP_TO_RUN's dwell resets removed | yes, 4 failures | the RUN_LOOP tail test |
| RUN_TO_STEP's class -1 gate removed | yes | `AsksForStepLoopWhenTheClassIsMinusOne` |
| RUN_TO_STEP's rate-walk clamp removed | yes | `RaisesTheRateTowardTheTargetAndStopsAtIt` |
| RUN_TO_STEP's band-restore scale uses the wrong field | yes, 3 failures | the three restore tests |
| RUN_TO_STEP's band-restore scale constants swapped | yes, 5 failures | the three restore tests |
| RUN_TO_STEP's band-restore writes removed | yes, 4 failures | the three restore tests |
| STEP_LOOP's band-walk keeper threshold flattened to 0.6 | yes, 3 failures | `WalksTheBandDownAfterTheShorterKeeperThreshold` |
| STEP_LOOP's band-walk speed restore removed | yes, 5 failures | both walk tests |
| TURN_STOP's rate scale removed | yes, 3 failures | `ScalesTheRateByTheRecordConstant` |
| TURN_STOP's second-dwell gate widened | yes, 3 failures | `KeepsTheTickWhileTheSecondDwellRuns` |

Not caught, because no test reaches them yet: TURN_STOP's idle row and its re-plan, STEP_LOOP's
keep-tick and clip tails, STEP_LOOP_CHANGE's gates. Those handlers port and compile but have no
coverage beyond what is listed above.

## Two corrections the tests found (in the port, not the tests)

1. STEP_LOOP reads `param_2[0x1f]` at each use, so the band walk is visible to every later read;
   the port had captured the live band before the walk.
2. RUN_TO_STEP's band-restore scale compares the live band against the previous band
   (`req[0x1f] < req[0x24]`), and the restore writes `req[0x1f] = req[0x24]` with the *previous*
   band speed; the port had the wrong field in the comparison and the wrong word direction.

## Verification state

- `pesfmt` builds with 0 errors; `pes_movestep_step_test` 13/13 pass on its own.
- The fleet-locked gate, taken with `PES21_ROOT` and `PES21_EDIT_SAVE` under
  `flock <workdrive>/gf-sim.lock`, after rebasing onto the contract's `fa9d3ec`:
  **100% tests passed out of 743**, RC=0, 544 s of test time. All 13 of this group's tests pass
  inside it. The two standing skips are `VfsInstall.NestedOriginalsSurviveWithoutDownloads` and
  `EditSaveInstall.DecryptsAndMatchesTheRunningGame` (both install-dependent, both skipped in
  Wave1AMov's gate too).
- PII scan clean (two machine paths found and removed during the pass).
- Pushed as `origin/MovestepStep` at `5f20a13`; not merged to main.
- An earlier full-build attempt failed in godot-cpp's `ar` archive (`file truncated`); a rebuild
  of the `godot-cpp` target alone cleared it, so the gate above is against a clean build.
  Several other agents' ctest runs were contending for the sim lock at the same time, which is
  why the gate took a long wall-clock time to acquire.

## Next

1. Wave1AMov's `tests/pes_movestep_test.cpp` against the current `RunRateAllowed` signature,
   then the fleet-locked gate.
2. Tests for STEP_LOOP's keep-tick and clip tails, STEP_LOOP_CHANGE's gates, TURN_STOP's idle
   row and its re-plan, and the TURN_STOP entry, one per branch with a mutation per test.
3. The install relations test for group C's own tables (the bucket columns, the kind/band grid's
   64 entries, the bucket degrees, the four distance tables) — the shapes are loaded, the
   relations are not yet asserted.
4. Where the `ponytail:` sites list a GF input that does land (MB-E's phase, MB-C's team
   context, the motion record's frames), replace the stand-in.
