# StallCap: restart-delay cap, watch and test

Branch `StallCap` at `7ad7650` (pushed; `ls-remote origin StallCap` confirms). Based on `origin/main` `6691a8a`. Not merged.

## Result: Partially done

The acceptance criterion "the test fails on `A2Merge-held` `7e6cbac` within seconds" is NOT met. The test passes on both trees.

## Changes
- `src/sim/onthepitch/restartwatch.hpp/.cpp` (new): `RestartWatch` observes a `Match` after each `Process()`. It measures whistle, goal and period-break stoppages (the penalty shootout is excluded), tracks the longest delay (including an open one), counts idle episodes from the stalemate counter, and sets `Stalled()` past the limit. `RestartLimitTicks` derives the bound from the install: the foul bracket with a player down (`FoulSequence::RestartTakeAt_ms`), plus `SetPieceLaws::kMaxWait_ms` (GF), plus the longest taker gate (exe), with goal (`GoalSequence::RestartKickOffAt_ms` with `kLongestCelebration_ms`) and half-time (`SetPieceLaws::HalfTimeTakeAt_ms`) terms; the largest is taken, plus 2 ticks of slack.
- `apps/gfsim_run.cpp`: `kMaxSimTicks = 30 min × 54 Hz`; exits 1 on a stall past the bound and on the cap. Prints `[restarts] N stoppages, longest X s (ticks), bound Y s` and `[idle] N episodes`.
- `tests/sim/pes_restart_delay_test.cpp` (new) and `tests/sim/CMakeLists.txt`: asserts no measured stoppage exceeds the bound on a 2-minute seed-7 match, and that the match reaches full time.

## Tests and evidence
- Main (`7ad7650`): `pes_restart_delay_test` PASS, 36 s. Longest measured stoppage 762 ticks without goals; with goals and period breaks the bound is 1757 ticks.
- `7e6cbac` (scratch worktree `<workdrive>/gf-StallCap-scratch/held`): PASS, 38.6 s. 2-min longest 1251 ticks (goal). 20-min seed 7 hit the 97200-tick cap with no violation: longest 1251, 51 stoppages, 43 idle episodes, match not over.
- Mutation (main): `MayTakeRestart` return `false` for gated set pieces. Test FAILS in 8 s: "a stoppage ran past 1757 ticks without a restart (longest 1758)". Reverted from a backup; restored file has no diff; test green again.
- Build: `gfsim_run` and `pes_restart_delay_test` build with no warnings. Gate not run (Main gates). `gfsim_run` not run (agent rule).

## Diagnosis of the held stall (not a single stuck restart)
- Seed 7, 20 min, transitions traced on main and on held (`<workdrive>/gf-StallCap-scratch/trace*.log`).
- Goal stoppages are identical on both: about 23.2 s each.
- Out-of-play holds on held are +2.5 s: throw-in (piece5) 8.0 s vs 5.5 s on main; corner (piece2) 8.5 vs 5.9. That is `downedPlayerExtra_s`. Main's 5.5 s equals `RestartTakeAt_ms(0, k, downed=false, holdKind 5)`. So held applies the downed hold when no player is down.
- Suspect: `AnyPlayerDown` (`referee.cpp:92`, `GetInjuryLevel() >= kPesDownedInjuryLevel`) reads a stale injury level on held. Not verified. Sent to A2Stall.
- Held stopped time: 29293 ticks vs 18883 on main; in-play time is similar (65.9k vs 68.1k). It slows the match, it does not freeze it.
- A per-episode bound from the PES brackets cannot see +2.5 s: held's worst stoppage (23 s) is within the goal bound. A tight per-kind bound for out-of-play stops would need the true downed state.

## Open items
1. Red on held is not reproduced. Needs either a tight out-of-play bound with the downed truth exposed (`kPesDownedInjuryLevel`), or A2Stall's root cause.
2. Smoke (`gfsim_run --minutes 2 --seed 7`) not run, per the agent rule. Main should confirm the `[restarts]` and `[idle]` lines.
3. `PrintMatchReport` still prints `[stalemate] forced N` (tick count, `matchreport.cpp:122`). Left in place because it is shared; drop it if the report should not show ticks.
4. Half-time and goal stoppages are in the bound by GF constants (`kLongestCelebration_ms`, `HalfTime*`), not PES exe values.
