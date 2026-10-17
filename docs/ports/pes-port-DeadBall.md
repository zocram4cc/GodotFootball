# DeadBall: keeper goal-kick stall (item 1) — resolved

Branch `DeadBall` (pushed, `origin/DeadBall` = `e906f25`). Base `32212a9` (requeue gate). Commits on top:
- `c24a2b3` test: `PesRestartDelay.EveryStoppageRestartsWithinTheInstallBoundAtTwentyMinutes` (seed 7, pes_loop, 20 min, 60-min sim cap). Red at `32212a9`.
- `e906f25` fix: `humanoid.cpp` SelectAnim, too-similar requeue gate skipped for PES locomotion clips.

## Acceptance

| Run | Result |
|---|---|
| 20-min `--pes-loop` seed 7 (gfsim_run) | full time, `[ticks] 114606`, `[determinism] 5142ab9e307b3bfb`, exit 0 (was: stall at 1758 ticks, limit 1757) |
| 20-min test (gtest, seed 7, pes_loop) | PASSED at `e906f25` (red at `32212a9`, see Tests) |
| 2-min `--pes-loop` seed 7 | full time, `44806` ticks, `[determinism] a817a13303958f90` (was `b462bee36a3ae01b`) |
| Off-path 2-min (no `--pes-loop`) | `[determinism] a3d5e43a3b7b03af` — unchanged |

Idle episodes (GF stalemate counter, not a stall): 2-min 46 episodes / 30 stalls (was 33 / 23); 20-min 67 / 46. Item 2 below.

## Mechanism (measured, 20-min run before the fix)

Keeper is the home goal-kick taker (log id H10). Trace data: requeue results per tick (temporary `GKREQ`/`SAW` prints in `SelectAnim`), MOVESTEP words per 9 ticks (temporary `GKMS`).

1. Release at ~61.6k ticks. Keeper at 2.1–2.3 m from a stationary ball at (47.76, 2.60). `poss=0`, designated possession man. Body faces the ball (`face` matches `cmdDir`), so the facing issue noted by A5Tail is not this stall.
2. HighPass (`cmd=6`) is refused on every requeue (`found=0`).
3. The walk (`cmd=1`, direction toward the ball, 5.88 m/s) is accepted once, at t=62262 (clip 1108).
4. At t=62264 MOVESTEP issues locomotion clip 3343 (Special, PES locomotion family; `PlayIssuedClip`, humanoidbase.cpp:461-517). That clip's `originatingCommand` is the accepted walk: `oc=(-5.87,-0.35)` equals `cmd`. `PlayIssuedClip` does not reset `originatingCommand`, so the clip inherits it.
5. From then on the too-similar gate (humanoid.cpp ~2015) refuses every walk: `gSim=1` on all 17 walks t=62264–62278. The gate's premise (the body is already doing this) is false: MOVESTEP holds with speed 0.
6. MOVESTEP holds at 2.3 m. Its command speed and look point come from `originatingCommand` (pesmovestep.cpp:45–49), so the carried command is load-bearing and cannot be cleared. The stop is PES's front-gate run-turn decision (`StepLetsRunTurn`, movestep_step.cpp:208–222) with the ball as look point. Adapter gap, not PES: `FillCommandWords` sets the move target one second along the command direction (pesmovestep.cpp:53–57) and leaves class and precise words at PES init (kind RUN, precise clear).
7. Restart limit (1757 ticks) runs out at t≈63400 with the keeper still at 2.3 m.

Fix: skip the too-similar gate when the current anim is a PES locomotion clip (`!OnPesLocomotion()`). The momentum check below it still weighs the clip's planned change, so a clip already doing the walk is still refused on momentum. Comment in humanoid.cpp names the stand-in (ponytail, FUN_1406eebb0, not re-verified).

## Tests

- Red (`c24a2b3` on `32212a9`, gate in place): `a stoppage ran past 1757 ticks without a restart (longest 1758)`; `the match did not reach full time`; `1758 vs 1757`. Log: `red_test20.log`.
- Green (`e906f25`): `[  PASSED  ] 1 test` (`green2_test20.log`). First green attempt failed only on the 30-minute cap (`IsGameOver` false at 97200 ticks; the match needs 114606). Fixed by a per-run cap parameter (`kTwentyMinuteMaxSimTicks`, 60 min), which is in `c24a2b3`.
- Mutation: gate restored (no skip) = red above. Other mutations not run (budget). The pes_restart_delay_test binary was not run in full (no full ctest); only the new filter and the existing short-match tests were in scope.

## Tried and rejected

- Clear `originatingCommand` in `PlayIssuedClip`: breaks MOVESTEP's command input (see 6).
- Taker look gate off (`kDistToLook := kDistToTarget` for the taker): stall persisted at 63405. Keeper ran to (46.2, −4.3), then came back at ~7 m/s. Changes the approach path; not a fix.
- Earlier LooseBallChase stand-in (commit `cf2915c`, removed): Main directed no chase stand-in.

## Open (not verified, not changed)

- INF: the exact check that refuses HighPass at 2.3 m (cheatable reach in `GetBestCheatableAnimID`, humanoid.cpp ~3096–3450, or the 2.0 m pre-check at ~1957) was not pinned. Only the refusal is measured.
- OPEN: whether PES's own approach stops at 2.3 m. That needs the FUN_1406b7560 fill for the speed-class and precise words (docs/re/pes-re-anim-select.md 1.1), which GF does not fill. The gap is the adapter's target model, not the FSM.
- Item 2 (loose ball, Main deferred): first-touch decode (`AI_HasPossession`, humanoid.cpp contact gate ~526–531). PES clips have `touchFrame -1`. Idle episodes unchanged in kind.
- Item 3: MOVESTEP holds at the look gate for sprint commands whose look point is the ball (A3States movement path). This fix avoids the stall but does not change the hold.
- Item 4: LooseBallChase dropped; Main to decide if needed.
- Item 5: FUN_1406eebb0 cited from pes-re-fox-registry.md, not re-verified.
- Item 6: `humanControlled` (pesoffball.hpp:180) never set.
- Facing (A5Tail): ResetPosition (humanoidbase.cpp ~521) and the round trip at ~1332 are 90° off. Not in this path; not changed.

## Logs (scratch: <workdrive>/gf-DeadBall-scratch)

`final_smoke20.log` (stall, pre-fix), `h1trace20.log`, `sa20.log`, `sa20b.log` (gate trace, pre-fix), `lookoff20.log` (diagnostic), `sim20.log` (20-min, fix, full time), `fix_smoke2.log`, `fix_off2.log`, `red_test20.log`, `green_test20.log` (cap fail), `green2_test20.log` (pass).
