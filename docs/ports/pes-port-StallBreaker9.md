# pes-port-StallBreaker9: the penalty shootout loop: root cause found, fix not landed

Status: **not landed.** Tree reverted to the pushed tip `64426c53`; nothing pushed this round. The work is saved in `<workdrive>/gf-StallBreaker-scratch/r9/shootout-referee-arm.patch` (275 lines: the referee arming, the shootout arrangement, the displacement-based strike, a red test, and temporary DIAG logs to remove). PesHang full time, the ALiveBall number, PesRestartDelay 2/3 and gen --check were **not** re-run this round; the round-8 numbers stand (PesHang: frame cap, no full time; ALiveBall: 2,510 forced live ticks in 87 episodes).

## 1. Census: how a shootout kick is set up and why it never starts (CONFIRMED)
- `Match::ProcessRefereeStage` runs `penaltyShootout->Process()` **instead of** `referee->Process()` during `e_MatchPhase_Penalties` (match.cpp:843). So the referee's restart machinery never runs for a shootout kick.
- The shootout drives the team controllers directly (`PrepareSetPiece(e_SetPiece_Penalty)` in `SetUpKick`) and calls `match->StartSetPiece()`/`StartPlay()` itself (`BeginKick`). The referee's penalty buffer is never filled (`desiredSetPiece`, `teamID`, `restartPos`, `prepareTime`, `startTime`, `prepared`, `started`), so:
  - `buffer.prepared` stays 0 and PES's restart programme never arms (`mi[kMiRestartArmedOffset] = buffer.prepared`, referee.cpp:828);
  - `buffer.started` stays 0, so the PES release never happens;
  - the referee's `buffer.taker` stays null: the stalemate lines print `buf active 0 prepared 0 started 0 piece 0 taker none` for every kick (the loop as logged in round 8).
- Verdict path (penaltyshootout.cpp `ObserveKick`): a Miss needs `settled` (ball stopped, left the field, or `kickTimeout_ms` = 5 s since the strike). Before any strike the watchdog (`runUpTimeout_ms` 12 s from the kick start) scores the MISS. That is the 12 s the round-8 log shows.
- PES reference in the repo: none for the shootout flow itself (`pes-re-penalty-keeper.md`, `pes-re-penalty-scatter.md`, `pes-re-shootout-verdict-mismatch.md` cover the keeper, the scatter and the verdict, not the restart); the restart machinery is `pes-re-setpiece-*` and `pessequencer.cpp`.

## 2. The fix I built and measured (RED then partly GREEN)
Changes (in the patch):
- `Referee::AwardShootoutKick(teamID, spot)`: the penalty award's buffer (foul penalty timings, `ScheduleStop(kPesHoldFoul)`), so the referee's own prepare/release apply.
- `Referee::StageShootoutKick()`: Process's two stages (prepare when due → `PrepareSetPiece`; release when `ReleasesRestart` → `StartPlay`/`StartSetPiece`), run by the shootout each tick (the referee's `Process` stays out of the shootout).
- `PenaltyShootoutController`: `SetUpKick` arms through the referee; `ArrangeKick` (once `prepared`) places the players and sets the taker (`SetPieceTaker` and `Buffer().taker`); the run-up clock starts at the **release** (`IsInPlay`), not at the set-up.
- Strike: `ballStruck` from displacement from the spot only. The old `GetMovement().GetLength() > 2` clause fired on a **stale 42 m/s** movement (the ball still on the spot: DIAGOBS `speed=42.7`, `x=41.56`), so every kick was settled as a MISS 5 s later, untouched.
- Red test `SimMatch.ShootoutKickIsTakenWithinTheRunUp` (pes_loop, `penalty_shootout_only`): each kick must have a shooting-side touch **and** the ball leaving where it lay at the release (0.5 m), within 12 s of the release.

Measured on the fixed tree (`SimMatch.ShootoutKickIsTakenWithinTheRunUp`, seed 7):
| tick | state |
|---|---|
| set-up | taker placed 1 m behind the spot |
| +~7 s | referee prepared (`prep=1`), then `ArrangeKick`: taker at 2.5 m behind |
| +~10 s | released: `started=1 inPlay=1 inSP=1 released=1` (the referee's release works) |
| +10–15 s | taker drifts 37.8–40.6 m, y −1..+0.1; `lastTouch=none`; ball 41.56, stays on the spot (struck=0 by displacement) |
| verdict | `MISSED`, `struck=1 sinceKick=5518` after the 42 m/s and 0.85 m nudge: settled by the 5 s timeout |

So: the referee arms, prepares and releases the kick (the bypass was the cause of "never prepared / never released"). **Not fixed:** the PES penalty **taker does not run up and strike** after release (no kick command, no touch). Run-up and strike are in the PES restart taker path (`TeamAIController` penalty branch, `pessequencer` penalty beats, `ReleaseFrames`/`PieceTakerArrived` for the penalty kind), which I did not trace. The red test fails on the fixed tree with `kick N was never taken by team T` (the test's last run).

## 3. Next step (needs Main's OK: this is a diagnosis-then-fix round)
1. Trace the PES penalty taker's kick command in `teamAIcontroller.cpp` (the penalty set-piece branch, `PieceTakerArrived`, the taker's `TouchAnim`/`TouchPending` path) and `pessequencer.cpp` (`BuildProgramme(Penalty)`, `RunProgramme`), with the same DIAG taker log.
2. Apply the patch's referee/controller changes together with that kick-command fix; remove the DIAG logs; then PesHang to full time, ALiveBall number, PesRestartDelay 2/3, gen --check.
3. Land the red test with its fix (the suite must not carry a failing test).

## 4. Other facts from this round
- `build/` was removed; the tree is clean at `64426c53`.
- A diagnostic `timeout`-wrapped run is a few minutes per kick; a full shootout to the cap is ~10 min per run.
