# A2Stall: seed-7 restart freeze

Result: Done (fix pushed; final smoke below).

Branch: `A2Stall` = `4f7358b`, confirmed by `git ls-remote origin A2Stall`. Base `7e6cbac` (A2Merge-held), plus `67cbd3d` (StallCap `7ad7650`, cherry-picked, not rebased: the StallCap branch is on `6114ef3`, so a rebase would drop A2Wiring), plus `4f7358b` (the fix).

## Root cause (CONFIRMED by trace and by the fix)

The freeze is a free kick, not piece 1. The state, from the uncapped head run:
`inPlay 1 setPiece 1 | ball 0.00 m/s at 5.3,34.2 | prepared 1 started 1 piece 3 taker H11`, idle for 16 000 s.

Chain:
1. `referee.cpp:911` sets `foul.foulPosition = tripee->GetPosition()`. The tripee stood at y 34.2, 0.2 m past the touchline (`pitchHalfH` = 34, `gametypes.hpp:68`).
2. `referee.cpp:1017` sets the free-kick spot to that position. `PrepareSetPiece` (`referee.cpp:654`) folds it with `OffsideRule::RestartPosition`.
3. `offsiderule.cpp:154-157` folded only the depth (x). The touchline overshoot was not folded, so the ball was placed at y 34.2.
4. `humanoid.cpp:3156-3157` rejects every touch frame of a loose ball past `pitchHalfH + lineHalfW + 0.11` = 34.17 m. Every touch frame was rejected, so the kick never played.
5. `referee.cpp:579` waits on `buffer.taker->TouchAnim()`, which never comes. The stalemate forcer (`match.cpp:793-827`) re-picks the same taker, so the restart holds for the rest of the match.

No PES VA is cited for the 34.17 m touch reach. It is a GF stand-in: `humanoid.cpp:3156` is the GF site. OPEN: PES's own touch-reach VA is not decoded.

Why A2 exposes it: [INFERENCE] the x-only fold has been there since main. A2's event-store latch changes move seed 7's trajectory, so the tripee's position at the foul lands past the touchline. I did not run main's seed-7 trajectory to confirm main never reaches it.

## Disproved and not bisected

- The latch (step 2 one-tick read, step 3a flag) is not the blocker. At the freeze the latch is forced by rule R to the restart team, and the taker is refused by the touch-reach geometry alone.
- I did not bisect commits. I reproduced the freeze directly on 7e6cbac and read the state.
- StallCap's `PesRestartDelay` test is green on 7e6cbac and does not reproduce this freeze. It passes on the fixed tree too (37.8 s, `PES21_ROOT` set).

## Piece-1 shape

Post-goal `piece 1` with the ball at x 55.1 is the goal countdown. It resolves. In the pre-fix trajectory the restart watch flagged one goal stoppage at 1758 ticks against a 1757-tick bound. It was not a freeze. The fixed run's longest stoppage is 1251 ticks (23.2 s). I did not root-cause that one-tick overrun.

## Fix

- `src/sim/onthepitch/offsiderule.cpp`: the same mirror fold across the touchline. A spot on the line (throw-in, corner) is untouched.
- `tests/sim/pes_offside_test.cpp`: `PesOffsideRestart.ATouchlineSpotPastTheLineStaysInsideThePitch`.
- `apps/gfsim_run.cpp`: sim-tick cap raised from 30 to 60 minutes. Seed 7 reaches full time at 128912 ticks (about 40 minutes), since the clock stops in every stoppage, so the 30-minute cap cut the run at 97200 ticks before full time. The run now prints `[ticks]`.

## Tests

- Red: `PesOffsideRestart.ATouchlineSpotPastTheLineStaysInsideThePitch` failed before the fix: `34.2 vs 34`.
- Green: `pes_offside_test` 21/21 with `PES21_ROOT=<workdrive>/PES21`.
- Green: `pes_restart_delay_test` (StallCap) 1/1 with `PES21_ROOT`, 37.8 s.
- Mutations (each rebuilt, run, reverted):
  1. positive touchline fold disabled: caught by the new test (fails at 34.2 vs 34).
  2. negative touchline fold disabled: caught by the new test (fails at the `-y` case).
- Gate not run (Main gates).

## Smoke, seed 7, 20 min

Pre-fix, uncapped: froze in the free kick above, idle reached 16 000 s.

Post-fix, same tree, 240-minute cap: `[determinism] c2c8b71f468db66d`, `[restarts] 69 stoppages, longest 23.2 s (1251 ticks), bound 32.5 s`, EXIT 0, 184.5 s wall.

Post-fix, 60-minute cap with `[ticks]`: `[determinism] c2c8b71f468db66d`, `128912 sim ticks to full time`, EXIT 0, 188.1 s wall. The hash repeats across the two runs.

Final acceptance smoke on the committed tree `4f7358b`: `[determinism] c2c8b71f468db66d`, `128912 sim ticks to full time`, `[restarts] 69 stoppages, longest 23.2 s`, EXIT 0, wall 184 s (`[ticks]`/`[idle]` as above).

Main's old baseline `7407dc1ab4162b83` is from before A2; this hash is expected to differ, and I did not compare the two.

## Open items

1. PES VA for the touch-reach test (`humanoid.cpp:3156-3157`) is not decoded. OPEN.
2. Whether PES lets a tripee stand past the touchline at a foul, or clamps the foul position to the pitch, is not decoded. The fold is a GF stand-in until then.
3. The 60-minute cap is a judgement. The owner may prefer a different backstop.
4. The pre-fix 1758-tick goal stoppage is not root-caused; it was not a freeze.
5. Not verified: that the restart watch flags the touchline freeze on its own. The capped run exited at the goal overrun before reaching it.
