# pes-port-StallBreaker10: the penalty taker's kick after release: traced, not fixed

Status: **nothing landed.** Tree at `64426c53` plus the uncommitted referee-arm / displacement-strike patch (with temporary DIAG logs). No push. PesHang, ALiveBall, PesRestartDelay and gen --check not re-run (round-8 numbers stand).

## 1. The taker's command path (traced, CONFIRMED)
- `elizacontroller.cpp` set-piece block (`else if (IsInSetPiece && GetPieceTaker()==player || ballRetainer==player)`, ~line 296): for `e_SetPiece_Penalty` it queues an `e_FunctionType_Shot` command with the shootout's aim **every tick**, before the taker is anywhere near the ball.
- The PES-loop run-to-ball branch (~line 540, `restart == FreeKick || Corner || Penalty`) also queues a `Movement` run to the ball, after it. So for a penalty both commands are queued, the Shot first.
- `pieceHeld` is never set for a penalty (teamAIcontroller only holds GK goal kicks, throw-in and corner takers), so the taker is not held.
- Placement: `PrepareSetPiece` places the penalty taker 3 m from the ball (teamAIcontroller ~2223); the shootout then puts him 2.5 m behind the spot (`ArrangeKick`). The team's penalty placement loop (teamAIcontroller ~1963) places every player once, at preparation.

## 2. What I tried (MEASURED, shootout test `SimMatch.ShootoutKickIsTakenWithinTheRunUp`, pes_loop, seed 7)
- Fix A (referee arm + displacement strike; in the saved patch): the referee prepares (`prep=1`) and releases (`started=1 inPlay=1 inSP=1`) the penalty at about 10 s after set-up. Kick flow now starts.
- Fix B (strike reach, `r9/taker-strike-reach.patch`, reverted): the penalty Shot is queued only inside 1.0 m of the ball, in PES loop; the run branch runs for a penalty taker out of reach. Result: unchanged. Taker positions after release: (40.3,−1.0), (40.8,−1.8), (42.0,−1.4), (40.4,−1.0), `lastTouch=none`, ball never moved. The taker reaches the ball's area only by drifting, and does not strike.
- DIAGOBS finding (kept in the patch, to remove): after the spot reset, `Ball::GetMovement()` reads a stale 42 m/s on a stationary ball. The struck test is now displacement only (in Fix A).

## 3. Why the taker does not strike (OPEN)
The run branch is queued, yet the taker moves about 1 m, not toward the ball at 2 m/s. Candidate causes not yet checked: the run `desiredVelocityFloat` (2.0 x distance, `kPieceTakerApproachGain`) is too small to move him at the humanoid's walk; the humanoid's movement ignores a Movement command while the taker's set-piece placement (`place`, budget) is active; or the queue's first command (Shot, before the run) still wins on some ticks. Next: log the taker's `commandQueue` contents and the humanoid's applied function per tick.

## 4. Decision needed (Main)
- Landing the referee-arm + displacement commit now means shipping `SimMatch.ShootoutKickIsTakenWithinTheRunUp` red (the suite rule forbids it) or disabled with a reason (hides the open taker). I did not do either without your OK.
- Otherwise: one more round on the taker's per-tick command and movement (section 3), then land the whole fix with the test green.

Saved: `gf-StallBreaker-scratch/r9/shootout-referee-arm-with-diag.patch` (referee arm, arrangement, displacement strike, red test, DIAG), `r9/taker-strike-reach.patch` (Fix B, reverted).
