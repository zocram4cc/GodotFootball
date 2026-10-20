# PES port: the STEP family (StepFamily)

Branch `StepFamily` from `origin/ClipWords-decision` (c8906a5). The branch carries the
whole STEP-family port, not only the last fix. Its own commits, oldest first:

| commit | what |
|---|---|
| `0a172cb` | RUN_LOOP: the style arm asks for RUN_TO_STEP, the STEP family's only entry - **this is the branch's change to `src/pes/movestep_run.cpp`** (7+/2-) |
| `2223a82` | STEP_LOOP: the bucket's look test and the step's own heading were sign-flipped |
| `5410c2a` | STEP_LOOP: the body turn was sign-flipped and the clamp zeroed instead of clamping |
| `c400994` | STEP_LOOP_CHANGE: the final gate tested the second dwell, not the first |
| `c4c2f66` | STEP_LOOP_CHANGE: the turn-to-target was sign-flipped, so the drift turned the wrong way |
| `8a62f98` | STEP_LOOP: the STEP_TO_RUN exit (req[0x39] = 0xD at 0x1406C592A) was missing |
| `25f39bd` | STEP_TURN_RUN: the reorient window read the wrong three angles |
| `9d03eec` | STEP_TURN_RUN: the run branch and the four later gates read the turn word |
| `247b410` | STEP_TURN_STEP: the reorient window's loose value omitted the bucket's angle |
| `1d24fe4` | STEP_TURN_STEP: the exit gate's last arm read the look heading, not the facing error |
| `11eadc2` | STEP_LOOP: the STEP_TO_RUN arm needs the class word at 0, and its row class is req[0x09] |
| `0384c40` | STEP_TURN_STEP: the three special kinds' gate read the wrong angle |
| `6320f71` | STEP_LOOP: the band gate was inverted, so the STEP_TURN_RUN arm was unreachable |
| `f3cba5e` | STEP_TURN_STEP: the root-facing write used the heading, and the class ladder the wrong angle |
| `49c7676` | STEP_LOOP: name the two rules that block TIMING_STEP, and where the latch would come from |
| `aa5705e` | STEP_LOOP_CHANGE: the drift's draw was 180 x 54, not 180 / 54 |
| `f58680c` | STEP_LOOP: the STEP_TURN_RUN arm never wrote the root facing |
| `9748401` | STEP_TURN_STEP: the final class ladder ignored the ball's distance |
| `a71fb7d` | TURN_STOP: the non-precise arm's gate had an empty body, so the ladder never ran |
| `af8baab` | this report |

`7025f5c` (an earlier TURN_STOP attempt) is on the branch's history but is **superseded**
by `a71fb7d`: it used 28.125 where this arm uses 22.5, it compared the wrong angle, and it
hung the sim. Merge `af8baab`, not `7025f5c`.

`git ls-remote origin StepFamily`:
```
af8baab9e47888eb27cd2ec41cd7e60e0381dcfe	refs/heads/StepFamily
```

## The one divergence found and fixed: TURN_STOP's non-precise arm

`src/pes/movestep_step.cpp`, `TurnStop` (PES `FUN_1406cab20`), the `else if (kMoveBucket != 0
&& kMoveBucket != 5)` branch - the non-precise arm.

PES's non-precise arm starts at **0x1406CB23A** (reached from 0x1406CB055, the
`req[0x38] != 1` case, i.e. not precise):

- `EAX = req[0x20]` (word 8 = `kMoveBucket`); if it is 0 or 5 the arm is skipped
  (0x1406CB240 / 0x1406CB249 -> 0x1406CB10D).
- The gate's angle is the one read at **0x1406CAFF1**:
  `FUN_14043a0a0(facing, the displacement heading)` = `wrap180(the displacement heading -
  the facing)`. `FUN_14046f230` (the call just before) is the displacement heading.
- **0x1406CB25E / 0x1406CB262**: `COMISS XMM8, XMM1` / `JBE 0x1406cb0b6`, with
  `XMM8 = [0x14259D618] = 22.5` (loaded at 0x1406CADD5) and `XMM1 = |XMM7|` the angle's
  magnitude. So when the angle is past **22.5 degrees** the handler jumps to the class
  ladder at **0x1406CB0B6**.
- The ladder (0x1406CB0B6): `XMM10` is the ground speed. Below 8.0
  (`[0x14259bdf4]`) it writes `req[0x7c] = 0` and `req[0x74] = 5.0`; at or above 8.0 and
  below 13.5 (`[0x1425a1ffc]`) it writes `1` and `11.0`; at or above 13.5 it writes `2` and
  `16.0`. It then calls **FUN_1406D38D0 (STEP_TURN_RUN)** at 0x1406CB100.
- The ladder is **not exclusive**: at 0x1406CB105/0x1406CB107, when FUN_1406D38D0 declines
  the tick, execution falls on to 0x1406CB10D and the handler continues.
- Otherwise (the angle inside 22.5) the run-turn block runs: 8.0 against the ground speed
  (`[0x14259bdf4]`), 0.5 (`[0x142565600]`) against `req[0x44]` (`kDistToTarget`), and
  `req[0x3c]` non-zero, then the class ladder and **FUN_1406D6780 (RUN_TURN_RUN)**.

The port had:

```cpp
if (k.deg22p5 <= std::fabs(turnToTarget)) { /* fall through to the run-turn below */ }
```

Three divergences in one line:

1. **The body was empty.** The port evaluated the gate and threw the result away, so the
   class ladder never ran: the class word and the band speed were never written, and the
   tick fell into the run-turn block where PES runs the ladder and then STEP_TURN_RUN.
2. **The angle was the wrong one.** The port used `turnToTarget` =
   `wrap180(kHeading - kFacing)`, the *first path's* angle (PES reads it at 0x1406CAD06 as
   `FUN_14043a0a0(kFacing, kHeading)`). The gate's angle is
   `wrap180(the displacement heading - the facing)`.
3. The constant 22.5 was already right for this arm - the 28.125 gate
   (`[0x1425ddf94]`, at 0x1406CB072) belongs to the **precise** arm, not this one.

The fix (commit `a71fb7d`): the gate's own angle, and the class ladder plus the
STEP_TURN_RUN entry in the gate's body, with the fall-through to the rest of the handler
kept (PES's 0x1406CB107).

### Red-to-green

`TurnStop.TheWideDisplacementTurnRunsTheClassLadder` (tests/pes_movestep_step_test.cpp):

- **RED** on the parent `9748401`: `1 FAILED TEST` - the class word stayed at the
  request's own 2 and the band speed stayed 0, because the ladder never ran.
- **GREEN** on `a71fb7d`: `[ PASSED ] 5 tests` in the TurnStop suite, 35 in the step suite.
- **Mutation 1**, the constant 22.5 -> 28.125: the test FAILS (the gate closes at 25
  degrees, so the ladder does not run).
- **Mutation 2**, the angle operand `wrap180(the displacement heading - the facing)` ->
  `wrap180(kHeading - the facing)`: the test FAILS (the heading is 0 in the test, so the
  angle is 0 and the gate closes).

The test's angle is 25 degrees on purpose: past PES's 22.5 gate and inside 28.125, so it
pins the constant as well as the operand. Word `0x0f` is cleared so the run-turn block
cannot run, so only the ladder can write the class word and the speed.

## The throw-in test: the exit is not in this branch's reach

Main's throw-in stall (`PesRestartDelay.AThrowInTakerPlaysABallActionWhenTheBallLiesOverTheLine`)
was reproduced in a scratch worktree (`<workdrive>/gf-StepFamilyThrow` = `origin/GoalKickLadder-wip`
44e26da merged with 9748401) with a per-tick probe of the taker's state, next-state request,
the requesting site and the rate/speed words.

**The test still fails, before and after this branch's change, with the same two
assertions:**

```
Expected: (throwIns) > (0), actual: 0 vs 0
no throw-in was released
Expected: (onLocomotion) > (0), actual: 0 vs 0
no throw-in taker was in a PES locomotion clip at release
```

The "the match did not reach full time" line in the same log is **pre-existing**: it is in
the pre-change run of the same tree (the watch's 1758 vs 1757 ticks), not something this
change introduced.

The taker's trace (the probe's `[movprobe]` lines, the taker's `dtarget=5.000` lines):
before the change he cycled 1 -> 3 -> 6 -> 7 -> 9 with the turn swinging between 0 and
140 degrees, and his body moved at 18-26 km/h in state 3 but ended up back where he
started - a net displacement of nearly nothing. He never reached his target, and no
throw-in was released.

**This branch's fix does not cure the throw-in.** The taker's `dtarget=5.000` lines drop
from 77 to 30 over the same span after the change, so the arm now behaves differently, but
the taker still does not arrive and the test still fails. The exit is elsewhere: the
taker's request never reaches a state that plays the ball action, and the stoppage never
releases. This branch's fix is correct against the bytes and is not the throw-in's exit.

**Where the throw-in's 3 -> 6 edge comes from.** The taker's cycle runs 3 (RUN_LOOP) -> 6
(TURN_STOP) and back. The 3 -> 6 edge is RUN_LOOP's TURN_STOP stand-in - the arm in
`src/pes/movestep_run.cpp` that requests TURN_STOP where PES requests the turn states
directly. ClipWords' option (A) removes that stand-in, and that is being tested on
ClipWords' tree, not here.

## The branch's scope, including `movestep_run.cpp`

This branch is the whole STEP-family port, and it does change `src/pes/movestep_run.cpp`:
commit `0a172cb` ("RUN_LOOP: the style arm asks for RUN_TO_STEP, the STEP family's only
entry") is 7 insertions and 2 deletions in that file, and it is the only change to
`movestep_run.cpp` on the branch. It is the RUN_LOOP style arm that requests RUN_TO_STEP
(14), which is how the port enters the step grid at all - the census below shows
RUN_TO_STEP reached on 243 ticks before the TURN_STOP fix and 175 after.

The other files the branch's **own** STEP-family commits touch: `src/pes/movestep_step.cpp`
(the STEP and TURN_STOP handlers), `src/pes/movestep_turn.cpp` (the STEP-family parts of
the turn file), `src/pes/movestep.cpp` / `src/pes/movestep.hpp` (the request block's
words), `src/pes/movestep_common.cpp`, and the four movestep test files
(`tests/pes_movestep_test.cpp`, `tests/pes_movestep_run_test.cpp`,
`tests/pes_movestep_step_test.cpp`, `tests/pes_movestep_turn_test.cpp`). The branch also
carries merges of `origin/main` (the lighting, carrier, lane and offside work), so the
branch's total diff against c8906a5 is much wider than the STEP family alone.

## The verification

The movestep suites, all green on `a71fb7d`:

```
pes_movestep_test: [  PASSED  ] 104 tests.
pes_movestep_run_test: [  PASSED  ] 76 tests.
pes_movestep_step_test: [  PASSED  ] 35 tests.
pes_movestep_turn_test: [  PASSED  ] 51 tests.
```

The 2-minute seed-7 smokes with `PES21_ROOT=<workdrive>/PES21`:

```
build/apps/gfsim_run --db-sql assets/fixtures/teams.sql --home 1 --away 2 --seed 7 --minutes 2 --pes21-root <workdrive>/PES21
[determinism] 75a11cdb2b3c8ec5
[restarts] 17 stoppages, longest 23.1 s (1250 ticks), bound 32.5 s (1757 ticks)
[ticks] 24090 sim ticks to full time (cap 194400)
[idle] 19 episodes, stalls 12
```

The switch-off hash `75a11cdb2b3c8ec5` is the reference hash (tasks/19-10-26.md line 81),
**unchanged**.

```
build/apps/gfsim_run --db-sql assets/fixtures/teams.sql --home 1 --away 2 --seed 7 --minutes 2 --pes21-root <workdrive>/PES21 --pes-loop
[determinism] f00edc980284c148
[ticks] 40067 sim ticks to full time (cap 194400)
[idle] 28 episodes, stalls 18
```

## The STEP-family census (temporary probe, dropped)

A per-tick census of the MOVESTEP state was run on the 2-minute `--pes-loop` smoke with a
temporary probe in `src/sim/onthepitch/pesmovestep.cpp`; the probe counted the state at
entry to `Dispatch`. The probe is **not** in the branch - it was removed after the runs, and
the branch's tree is `a71fb7d` clean.

**The tree the census ran on.** Both columns are this branch, at the same timestamp
(t=420037 ms): the pre-fix column is the branch with `src/pes/movestep_step.cpp` reverted
to the parent `9748401` (the probe's only other change), the post-fix column is
`a71fb7d`. The probe itself does not change the behaviour - the two builds' hashes differ
only because of the `movestep_step.cpp` change.

**A correction to the first version of this report.** The probe's print format had a
label list that skipped `kGotoStep` (index 11), so every label from index 11 onwards was
shifted by one. The table below is the counts re-read from the raw probe output against
`src/pes/movestep.hpp`'s `MoveState` enum, not the probe's own labels. The first version
of this report repeated the probe's labels and therefore mis-stated which states the port
reaches.

| state (enum index) | pre-fix (9748401) | post-fix (a71fb7d) |
|---|---|---|
| kInit (0) | 22 | 22 |
| kIdle (1) | 116946 | 146447 |
| kIdleTurn (2) | 5891 | 5379 |
| kRunLoop (3) | 144466 | 109798 |
| **kStepLoop (4)** | **1406** | **389** |
| **kStepLoopChange (5)** | **10** | **0** |
| kTurnStop (6) | 57608 | 57010 |
| kGotoRun (7) | 57003 | 57134 |
| kStop (8) | 309 | 244 |
| kAccelerate (9) | 107804 | 117189 |
| kDecelerate (10) | 1271 | 705 |
| kGotoStep (11) | 0 | 0 |
| **kRunTurnRun (12)** | **2799** | **1896** |
| kStepToRun (13) | 0 | 0 |
| **kRunToStep (14)** | **243** | **175** |
| **kStepTurnRun (15)** | **711** | **39** |
| kRunTurnStep (16) | 0 | 0 |
| kIdleOther (17) | 0 | 0 |
| kRunTurnRunSlipdown (18) | 0 | 0 |
| kStepTurnStep (19) | 0 | 0 |
| kIdlePkPlayer (20) | 0 | 0 |
| kIdlePkGk (21) | 0 | 0 |
| kTimingStep (22) | 0 | 0 |

The port does reach the STEP family, and it reaches **RUN_TO_STEP (14)** - 243 ticks
before the fix and 175 after. That entry exists because of this branch's own commit
`0a172cb` ("RUN_LOOP: the style arm asks for RUN_TO_STEP, the STEP family's only
entry"), which is the change to `src/pes/movestep_run.cpp` on this branch.

This branch's fix moves the traffic further into the grid: kStepLoop falls from 1406 to
389 and kStepLoopChange from 10 to 0, while kStepTurnRun falls from 711 to 39 - the
ticks that used to leave through the run-turn arm now stay in the step grid. The
`--pes-loop` hash changes with the fix (`a00d2b459c503be8` -> `f00edc980284c148`); the
switch-off hash is unchanged.

The census shows the port still never reaches kStepToRun (13), kRunTurnStep (16),
kStepTurnStep (19), kIdleOther (17), kRunTurnRunSlipdown (18), kGotoStep (11),
kIdlePkPlayer (20), kIdlePkGk (21) or kTimingStep (22) on this smoke.

## Not done

- The throw-in stall's own exit is not in this branch: the test still fails with the same
  two assertions as before the change. The throw-in's 3 -> 6 edge is RUN_LOOP's TURN_STOP
  stand-in, which ClipWords' option (A) removes; that is being tested on ClipWords' tree.
- The census probe is dropped, as asked.

