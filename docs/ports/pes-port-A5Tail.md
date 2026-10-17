# pes-port-A5Tail: PES restart tail (throw-in spot, release on reach, kick-off receiver, dead ball)

Branch `A5Tail` at `afdfb9fadd56447536e5fa7162b5af7a72bbaa5e` (pushed, ls-remote
confirmed). Merged `origin/main` (`f99908a`: A6Breaks clock, LanePort) and
`origin/A3States` twice (playback req[0x35] `df8fac9`, bearing atan2 `5477448`).
Worktree `<workdrive>/gf-A5Tail`, clean. All TEMP trace prints and all three
bisect env switches (`GF_NO_BALL_KILL`, `GF_SPOT_ON_LINE`, `GF_NO_REACH`)
stripped; tree builds clean.

## What was built (all four assignment items, production code committed)

1. **Throw-in spot F1+0.25 by VA.** `DAT_142592228` (=0.25) and `DAT_14256ae1c`
   (=1.0) verified by direct read of the install; added as
   `throwInLineOutset_m`/`throwInEndInset_m` to `RestartSpotConstants`;
   `ThrowInSpot(x,z,k)`; `PrepareSetPiece` skips the foul-path F1 clamp for a
   PES-path throw-in (null-spot path has no F1 clamp — TouchReach §2); the
   taker's throw is no longer refused as out of bounds on the writer spot
   (PES-path gated in `GetBestCheatableAnimID`).
2. **Release on reach.** `MayRelease(kind, frames, takerReached, reach_m,
   takerBallDistance_m)`: reach releases immediately for every kind; the frame
   gates are PES's time-out. `takerReach_m` read from `0x14259d5d0` (0.65).
3. **Kick-off receiver test.** Gate restored to 0 (an earlier 972-frame forced
   bound regressed the kick-off: the taker walks away 0.66→3.5 m and the wait
   never ends). Release at arming IS the receiver test: the receiver owns the
   ball from the placement teleport (setpiece doc 12, live 28 exactly x3).
4. **Ball dead at the whistle.** `Ball::Stop()` (momentum + spin zeroed, track
   rebuilt) from `Match::StopPlay()` under `pesLoop`. The spin zero is required:
   without it the resting-ball gate never catches and the flight function
   re-accelerates the "dead" ball (measured 0→6 m/s with position pinned).

## Tests

- `pes_restart_sequencer_test` **13/13 green**; neighbours green
  (`pes_stoppage_timing_test` 10/10, `pes_tick_order_test` 2/2,
  `pes_setpiece_taker_test` 8/8, `pes_setpiece_team_test` 18/18,
  `pes_event_wiring_test` 4/4).
- `pes_restart_delay_test`: off-path 2-min **green**; item-4 whistle test
  **green**; the other 4 on-path tests stall (below).
- Off-path 2-min `gfsim_run` hash: **`a3d5e43a3b7b03af`** — the expected hash,
  off path provably unchanged.
- 5-min `--pes-loop` smoke: **stalls** (free kick, piece 3, taker H1, ball dead
  at (37.3,-0.5), chaser frozen 12.5 m out; `no restart within 1758 ticks`).

## Mutations (each run, rebuilt, reverted with the edit tool)

- M1 outset VA →`0x14259222c`: caught by `AThrowInSpotComesFromTheInstalledOffsets`.
- M2 reach check removed: caught by `ATakerAtTheBallReleasesEveryRestart`.
- M3 kick-off gate 972→0: caught (then design corrected to gate 0).
- M4 prepare skip removed: run fails. M5 `ball->Stop()` removed: run fails
  with `ball flying at 11.17 m/s during the stop`.

## Root cause with evidence: the taker never kicks (not my items)

Three valid bisects exonerate items 1, 2, 4 (spot-on-line still stalls;
kill-disabled still stalls; reach-disabled still stalls). Picker instrumentation
(added, measured, stripped) showed for the stalling throw-in: `oobSkips=0`,
`scoreFails=5`, `bestAdv=0.000` at every taker distance 0.26–9.99 m — a
categorical denial, plus post-release recession 1.8→3.5 m and `fn=14` garbage
(no anim ever assigned). DeadBall's trace adds the shared root: `SelectAnim`
ReQueue refuses a Movement command while the current anim is Special, leaving a
stale command. A3States owns the movement/animator files (`movestep*`,
`pesmovestep.cpp`); DeadBall owns the loose-ball chase (coordinated, no overlap:
I touch neither). My release/spot/ball work waits on that fix; with the picker
fixed, release-at-reach is strictly earlier with the taker already at the ball.

## Open items

1. Taker-never-kicks stall (A3States/DeadBall): picker denies every pass anim
   with advantage exactly 0.000; taker recedes post-release.
2. Post-throw short death (A3States measured 4.73 m/s dying in ~2.5 m).
3. 5-min on-path run + remaining on-path delay tests: blocked on (1).
4. Live-PES whistle confirmation for item 4 still partial (sampler aliasing).
