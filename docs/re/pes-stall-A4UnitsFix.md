# Stall at pes_loop seed 7: goal-kick loop after the A4 carrier merge (A4UnitsFix findings)

Status: no fix. Branch `A4UnitsFix` is at `origin/main` `a337835`, no commits, worktree clean. Probes reverted. Main's ruling: do not patch the units; the stand-in goal kick is the fix point.

## Symptom

20-minute `gfsim_run --pes-loop --seed 7` on current main hits the tick cap:
`[cap] no full time after 194400 sim ticks`. Stalemate lines alternate `piece 2` (goal kick) at x = +/-47.8 with taker H1 or A1.

## Bisect (CONFIRMED)

| Build | Result |
|---|---|
| Current main (post-merge carrier files) | `[cap]`, no full time, 194400 ticks |
| Pre-merge carrier files (0ee3d7a versions of `pescarrier.*`, `elizacontroller.cpp`, `pes_carrier_test.cpp`) on current engine | `09e776a0332a8784`, 172763 ticks, full time |
| Off-path, current main | `96ce33d3b51d4199`, 108183 ticks, full time |

Engine files changed since the gate (`0ee3d7a`..`origin/main`): only `pescarrier.{cpp,hpp}`, `elizacontroller.cpp`, `tests/sim/pes_carrier_test.cpp`. The kick, placement, referee and humanoid code are unchanged.

The off-path 20-minute hash is preserved (`96ce33d3b51d4199`), as required.

## The loop (CONFIRMED, full 20-minute run, 97 goal-kick cycles)

Full log: `<workdrive>/gf-A4UnitsFix-scratch-full.log` (scratch, not in repo).

- 97 goal-kick preps (`piece=2`), 97 taken. The keeper does kick every time. The earlier premise that he never kicks is wrong.
- Cycle: keeper high pass, ball lands about 25 m beyond the mate, rolls out over the far goal line, next goal kick at the opposite end. Cycle is about 19 s (about 6.7 s from taken to next prep, about 3.5 s from whistle to taken, about 6 s prepare and hold).
- Launch-to-prep pairing: 88 of about 97 goal kicks follow a keeper HighPass (H1/A1, type 6, power 1.00–1.05). The rest follow outfield launches (A5, H11, A2, A10, A11).
- Keeper launches in the full run: 89 HighPass (type 6), 3 ShortPass (type 4). The 3 Shorts are post-catch distribution, not goal kicks.

Exits from the loop were all angled kicks into touch: prep `piece=5` (throw-in) after kicks at power 0.69–0.79 aimed at A9 or H6 (y about +/-20). Straight kicks at A10/A11/H10/H11 (y about +/-6–9) never produced a reception.

## Kick numbers (CONFIRMED from the launch probe)

- Target: keeper at (+/-48.5, ~2), mate at (-7.3, 6.3), so about 56 m.
- Power 1.02–1.05, touch vector about 45 m/s before the fiddling pass.
- Flight trace (A1 kick, power 1.02, t = taken + n/54 s): ball at (36, 3.5) at launch+0.5 s; peak about 8 m at x about 5; passes over the target at x = -8.5, z = 6.9, speed 27 m/s; lands about x = -32; rolls to x = -55 at 10 m/s; out over the goal line (x = -52.56) between samples; rests at -69 about 6 s after launch.
- Home keeper H1 sits designated from about 1 s after kick (time-to-ball frozen, see below), sidesteps laterally from y = -1 to y = +10 and never runs at the ball. Function stays Movement (#1) except briefly Special (#14).

## Mechanism chain (CONFIRMED by reading code and by trace; the step "trap gate blocks non-designated players" is CONFIRMED in code; the freeze is CONFIRMED by trace)

1. Goal-kick draw, `elizacontroller.cpp:263-272`: `random(0,1) > 0.4` gives HighPass with target x = `pitchHalfW * -side * 0.2` (about +/-10.5, midfield) and target y = `random(-pitchHalfH, pitchHalfH)`. ShortPass branch targets x = `player.x * 0.9`, near own box.
2. Pass power, `AIfunctions.cpp:1211-1222` (`AI_GetAutoPass`): HighPass power = `(clamp(len,0,60)/60)^1.4 * 1.15`. A 56 m target gives power about 1.04. Humanoid touch: `humanoid.cpp` `touchVec = ballDirection * 36 * (ballPower + 0.3)`. This is an overshoot: the ball travels about 80 m.
3. Designation freeze, `team.cpp:~580-600`: switch to another player only if `timeRating < 0.8`, with both times offset by +500 ms. A fast ball makes every candidate's time huge, so the ratio is about 1 and the home designation stays on H1 (keeper).
4. Trap gate, `playercontroller.cpp:379-390` (`_ReceiveCandidate`): returns false unless the player is match-designated or team-designated. Non-designated outfielders near the rolling ball cannot trap it.
5. Team PASS_GET (`team.cpp:~370-410`) designates the away target A11, but he starts about 25 m from the landing point and cannot arrive before the ball rolls out (INF, from trace distances).
6. Keeper H1 does not rush: keeper branch `elizacontroller.cpp:499-571` only chases when the race and lateral/area tests pass; with a ball 56 m away and a 20 m/s roller it does not (correct per that logic, not changed by my merge).

Net: no player who may trap is close enough, and the overshooting kick sends the ball out. The loop has no exit except angled kicks.

## Why the merge tips it (INF, not isolated)

Pre-merge and post-merge differ only in carrier decisions. Those decisions consume only the PES per-player generator and seeded rolls, not the global `random()`, so the global stream is not changed by my code directly. The differences that do change play are the Long (kind 9) and Cross (kind 7) answers and the wide-side list latch. They shift which balls leave the pitch and when, so the global RNG draws for kicks land on different values. Main's reading (the merge only shifted the RNG into an existing stand-in) is consistent with this. I did not isolate the one unit that enters the loop. A unit-level bisect (disable kind 9, then 7) would confirm it, about 3 minutes per run, but Main has ruled that the fix belongs in the goal-kick port.

## Draw check (CONFIRMED)

`random(0,1)` at the goal-kick branch, 851 samples over the run: 514 above 0.4 (High), 337 at or below 0.4 (Short). The split is about 60/40 as coded. Pairing with the next keeper launch: 76 High, 1 Short in the paired sample. The generator is fine; the goal-kick branch is simply rarely producing a Short kick in the loop window.

## Probes used (all reverted; nothing committed)

- `taker-probe` in `elizacontroller.cpp` (taker branch): taker, retainer, distance, touch flags each tick during goal kicks.
- `prep-probe`, `whistle-probe`, `taken-probe` in `referee.cpp`: when a restart is armed, released, and taken.
- `flight-probe` in `match.cpp` `ProcessBallStage`: ball position and speed every 27 ticks for 800 ticks after a goal-kick taken, with designation, keeper position and function, and the newest event.
- `launch-probe` in `humanoid.cpp`: pass launch power, type, target, kicker and target positions.
- `draw-probe` in `elizacontroller.cpp`: the goal-kick draw value.

Caution on my earlier readings: several probe runs were piped through `head` and killed early by SIGPIPE, so their tails are not evidence. The full-run results above are from complete logs.

## Open

1. Port PES's goal kick (taker placement, kick type and target, power). The PES VA is not decoded in this pass; blocked on that decode. The keeper branch and taker branch are outside the carrier path.
2. Unit-level bisect to name the carrier unit that enters the loop (kind 9 or 7).
3. Designation freeze (+500 ms time rating) on fast balls is shared code; it turns any unreceived fast ball into a dead ball. Not my file; flag to the team owner of `team.cpp`.
4. Trap gate requires designation; outfielders 10 m from a rolling ball cannot trap it. Shared (`playercontroller.cpp`), not my file.
