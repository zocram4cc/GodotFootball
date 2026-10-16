# Shootout verdict vs plan: the keeper is never driven to the planned save

Result: Partially done. Cause located and reproduced deterministically; the fix is PES's keeper penalty logic, over budget. The reproducing test is committed `DISABLED_`.

## Reproduction

`SimMatch.DISABLED_ShootoutVerdictIsThePlannedOutcome` (`tests/sim/sim_match_test.cpp`) runs a shootout-only match on seed 7 and compares each kick's recorded verdict with the outcome planned before it. Run with `--gtest_also_run_disabled_tests`:

```
verdict GOAL! (aimed for SAVED!) ball at x=52.680000 y=0.397410 z=0.380067 goalline x=52.500000 [MISMATCH: recording what the ball did]
sim_match_test.cpp:505: Failure: Which is: 1 vs Which is: 2 (kick 2)
```

Kick index 2 (the third kick) is the first mismatch. Kicks 0 and 1 match. The captured log shows later kicks planned GOAL with a GOAL verdict; the rest of the run was not inspected.

## Cause

The plan is rolled in `PenaltyShootoutController::SetUpKick` (`penaltyshootoutcontroller.cpp:189-197`). It only shapes the taker's aim:

- `penaltyshootoutcontroller.cpp:199-216` maps a planned Save to `aimY = ±0.6`, `aimPower = 0.55`.
- The taker reads `GetAimY` / `GetAimPower` (`player/controller/elizacontroller.cpp:231-237`).

Nothing on the defending side reads the plan. `GetPenaltyShootout()` is used only by the taker's controller (`elizacontroller.cpp:231`) and by the shot's goal-line lookup (`humanoid.cpp:1005-1009`). The keeper runs the ordinary keeper AI, which has no input from `plannedOutcome` (`GetPlannedOutcome` is only consumed by the test). So a planned Save is recorded as Save only if the keeper's independent dive happens to stop the aimed shot. In the reproduction the ball crossed the line at y≈0.4 against a y=0.6 aim and the keeper did not stop it. The gap to the aim is attributed to kick error [INFERENCE, not measured here].

The verdict comparison and labels are correct: `OutcomeText` maps each enum correctly and `observed != plannedOutcome` compares like with like. The goal-line frame is correct: `ObserveBall` uses `ballIsInGoal`, set by `Match::CheckForGoal` (`match.cpp:879-884`) on the same plane the verdict is logged against (`x=52.5` goal line, crossing at `x=52.68`).

## Why it is not a one-line fix

Making a planned Save stop the ball would script the outcome, against the design comment at `penaltyshootoutcontroller.cpp:372-375` (the recorded verdict is the ball's). The honest fix is that the defending keeper is driven by PES's penalty logic (dive timing and direction from the kick, not from the plan). That needs PES's keeper penalty behaviour, which is out of this change's budget.

## Left

- Keeper penalty behaviour (PES source of the keeper's shootout dive) to remove the mismatch at its source.
- Enable `DISABLED_ShootoutVerdictIsThePlannedOutcome` when that lands.
