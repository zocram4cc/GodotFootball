# IdleEpisodes: classification of the 54 `[idle]` episodes (main a47f5f2, seed 7, 20 min)

Result: **Partially done.** Classification done; the largest stall group's root is outside the restart code (humanoid touch selection, owned by TouchReach). No code change committed. No red test written, because no fix was made.

## Baseline (unmodified `a47f5f2`, worktree `<workdrive>/gf-IdleEpisodes`)
- `[determinism] 6d0e1e05fa65998d`, `[restarts] 53 stoppages, longest 23.2 s (1251 ticks), bound 32.5 s (1757 ticks)`, `[ticks] 98381`, `[idle] 54 episodes`. Same numbers with temporary logging (logging does not change sim state).
- Method: temporary `DBG-TEMP` logging at the stalemate force site (`match.cpp`, reverted; not committed). One line per episode start (`dbg-start`, with restart state, ball, distance to restart spot, nearest player, designated player) and per episode end (`dbg-end`, `dbg-end-who`). Logs: `<workdrive>/gf-IdleEpisodes-scratch/smoke-base-dbg{,2,3,4}.log`.

An idle episode = a maximal run of ticks in which `stalemateCount` rises (`match.cpp`, the forced-ball branch): the ball has moved less than 2 cm for 5 s (`kStalemateForceAction_s`), it is slower than 0.5 m/s, and nobody has the ball. The clock restarts when the ball moves, including the referee's `ResetSituation` teleport on prep.

## Classification (54 episodes)

| Group | Count | Restart type (`desiredSetPiece`) | Stall cause (state at start, end) | Stall? |
|---|---|---|---|---|
| **A. In play, ball at rest after a restart** (`inPlay=1 inSet=0`) | **20** | free kick 8 (7,10,13,22,27,28,31,54); throw-in 6 (11,19,35,36,39,51); kick-off 3 (30,41,47); goal kick 3 (42,43,44) | Ball lies 0.4–1.0 m from a player with `HasPossession()=1` and `ballRetainer=null`, in Trap (fn 3) or BallControl (fn 2). The ball moves only when he finally touches it, 0.4–7 s later. | **Yes** |
| B1. Armed, not prepared, ball in the goal or at the centre spot (kick-off hold) | 9 | kick-off | Hold after a goal (ball at x≈±55.1 in the net: 14,33,34,40,45,46,49,50) and the 2nd-half kick-off (29). The idle clock counts the celebration/bracket hold. | No: normal hold |
| B2. Armed, not prepared, at the restart spot (dead-ball hold) | 9 | free kick (pc3): 1,3,15,17,20,23,25,37,52 | PES whistle bracket (`pesstoppage`) before prep. Idle counts the hold. | No: normal hold |
| C. Armed, prepared, not started (taker gate after prep) | 16 | free kick (pc3) | Ball on the spot (`dRest=0`); 1.0–1.4 s until `MayTakeRestart` releases. Idle clock resets at prep teleport. | No: normal gate |

Totals: 20 + 9 + 9 + 16 = 54. Restart stalls: none. `RestartWatch` longest stoppage 23.2 s is inside the 32.5 s bound.

B1 and B2 are both in the `!inPlay && armed && !prepared` state and are split only by restart type. Together they are 18.

## Largest stall group: A (20 episodes), root location

- Designated man at episode end: `HasPossession()=1`, `ballRetainer=null` (`dbg-end-who`) in 15 of the 16 A episodes that end with play still live. The other 4 end with play stopped. He is in Trap or BallControl and never touches the still ball.
- Likely mechanism: touch-frame selection in `src/sim/onthepitch/player/humanoid/humanoid.cpp` (the `touchNum` loop near lines 3140–3170) does not accept a still ball 0.4–1.0 m away. The out-of-bounds guard (`+0.11`) is in the same loop. Unverified; TouchReach's reach investigation covers this (see Main's FYI). **PES VA: OPEN.** No PES touch-reach refusal has been identified yet.
- The "step onto a still ball" branch in `elizacontroller.cpp` (~lines 441–470) queues nothing once `toBall ≤ 0.1 m` and `!hasIt`. [INFERENCE] It is not the cause here: at episode end `HasPossession()=1`, so `GetOnTheBallCommands` runs. `hasIt` at episode start was not logged.
- Not a restart-path root. Changing `referee.cpp` or `match.cpp` would only move the counter (B and C are the restart-side idle counts).

## Not done, and why
- Fixing group A needs a change in humanoid touch selection (TouchReach's area), plus a PES VA for the touch rule. I did not touch `humanoid.cpp`.
- No restart-path change reduces idle without tuning the metric. The idle clock correctly counts holds that exceed 5 s; the stall is in A.
- The `[idle]` count is not a stall count (per the table: 34 of 54 are holds). Consider counting A only, or the RestartWatch bound, as the smoke's health metric.

## Smoke runs in this worktree
- Baseline (no change): 1 run; instrumented runs 2–4 reproduce `6d0e1e05fa65998d` and 54 episodes. No gf-sim.lock taken (per contract).
- Final tree: `src/sim/onthepitch/match.cpp` reverted to `origin/main` (`git checkout --`). No commits, no branch pushed (nothing to land).

## Open
1. PES VA for the touch-selection rule when the ball is 0.4–1.0 m away (TouchReach).
2. Whether the referee's `ResetSituation` on prep (`referee.cpp:655`) should count as ball movement for the stalemate clock. Not changed; it is what makes B and C short episodes.
