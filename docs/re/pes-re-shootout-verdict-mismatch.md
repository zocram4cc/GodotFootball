# Shootout verdict: the recorded outcome is what the ball did

Result: Done. The shootout records what physically happened to the ball. The test that pinned the verdict to the pre-rolled plan (`SimMatch.DISABLED_ShootoutVerdictIsThePlannedOutcome`) is removed, and `SimMatch.ShootoutVerdictIsWhatTheBallDid` replaces it.

## Premise

A shootout kick is aimed from a pre-rolled outcome (`PenaltyShootoutController::SetUpKick`, the plan shapes the taker's aim). The plan is not a guarantee: PES does not promise that a kick aimed for a save is saved, or that a planned goal goes in. The verdict is therefore defined by the physical outcome:

- **Goal**: the whole ball crosses the goal line inside the frame. The engine's plane is the line plus its half-width plus the ball radius (`Match::CheckForGoal`, `match.cpp`).
- **Save**: the defending keeper touches the ball and it does not go in.
- **Miss**: anything else.

Goals take priority over a keeper touch, since a rebound off the keeper can still go in.

## Keeper touch: why it did not register as a save

It did register. Traced on seed 7 (`ObserveBall` per tick), kick 1: the keeper's touch was recorded by the engine at 0.85 m from the ball, and `keeperTouchedBall` latched. The verdict then went wrong for a different reason:

1. `ObserveKick` resolved a keeper touch as a save only once the ball had stopped or left the field.
2. The deflected ball kept rolling at 1.5 to 2.8 m/s for well over 5 s, so neither condition held.
3. The kick then ended on the 5 s timeout, which returned Miss without consulting the touch.

Fix (at the cause): the timeout now settles the kick like a stop or an exit does, and a keeper touch on a settled kick is a save (`penaltyshootout.cpp`, `ObserveKick`). The touch itself is read from the engine's last-touch record (`match->GetLastTouchPlayer() == keeper`) during the kick, not from the 1.2 m proximity latch. A deflection is made at arm's length, and proximity both misses real contacts and credits near passes that were never touched.

An earlier note blamed the 1.2 m radius for the miss, on the ground that the deflect point sits 2 m out. That was wrong: the contact at 0.85 m was inside the radius. The trace above is the evidence.

## The defending keeper's dive

The keeper's penalty target is ported from PES (`docs/re/pes-re-penalty-keeper.md`): the first predicted ball position within 3.5 m of him, when his bearing lies within 10° of the goal axis; otherwise he stays. The keeper's controller branch now also runs during a live kick, since a kick is a set piece and the branch was gated off during set pieces. PES's timing gate in front of the dive is not ported (a `ponytail:` comment names it).

## Test

`SimMatch.ShootoutVerdictIsWhatTheBallDid` (`tests/sim/sim_match_test.cpp`) runs a shootout-only match on seed 7. Each tick of a kick it reads the ball and the engine's touch record and works out the physical outcome: a goal-plane crossing inside the frame (`|y| <= goalHalfWidth`, `0 <= z <= goalHeight`, the whole ball past the line by `lineHalfW + ballRadius`, the radius read from the install) is a goal; otherwise a defending-keeper touch is a save; otherwise a miss. It compares that with the recorded `lastOutcome` at the end of each kick.

Red and green evidence:
- Before the fix (the classifier as it was on the branch base), kick 1 recorded Miss where the ball had been touched by the keeper (the 5 s timeout case above).
- Mutation A (the old save clause that needed the ball to stop or leave): kick 1 recorded Miss (3) against a physical Save (2). Caught.
- Mutation B (keeper touch never registered): kick 1 recorded Miss (3) against a physical Save (2). Caught.
- Fixed code: green on the committed source, and 20 of 20 runs of the single test pass with identical verdict sequences. The full `sim_match_test` binary passes 9 of 9 (one run; the earlier segfault in `AIManagerSwitchGovernsTactics` did not recur).

## Open

- PES's timing gate for the dive (`FUN_1409e5600`, state 0): the shot record `FUN_140a3e940(state,0x11)` and the taker's animation frame fields are not decoded. See `pes-re-penalty-keeper.md` §7.
- The `[MISMATCH]` log tag still compares the recorded verdict with the plan. It is diagnostic only and no longer part of any test.
