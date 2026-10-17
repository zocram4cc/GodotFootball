# Rolling block audit: S3 weighting, spin writers, two decoded fixes (branch SpinRunaway)

Result: Partially done. The S3 weighting matches PES. Two decoded deviations in the same block are fixed. The over-cap runaway is not reproduced in the block itself. Its only reachable source in GF is an uncapped GF stand-in writer, left open (see Open items).

Commit: `859047a` on `SpinRunaway` (pushed; `git ls-remote origin SpinRunaway` confirms `859047a4ada2a95903532d0195ffb24d3240fd8c`). Based on `origin/main` `0a1784b` (Liftoff is merged there).

## 1. Decode of the S3 weighting (FUN_140876360, install `PES2021.exe`)

Constants read from the exe: `0x142565600` = 0.5, `0x142588218` = -1.0, `0x142578480` = pi, `0x1425656c0` = 4.0, `0x14259d654` = -3.0, `0x142598958` = radius.

| VA | Instructions | Meaning |
|---|---|---|
| 0x140876d05..0x140876d34 | sqrt((vxPost/r)^2 + (vzPost/r)^2) | \|v_xz\|/r |
| 0x140876d42..0x140876d55 | x0.5 then /pi | rollHz = \|v_xz\|/(2 pi r) |
| 0x140876d5d..0x140876d69 | SUBSS [rdi+0x68] (boundRollMax); MAXSS 0 | over = max(0, rollHz - cap) |
| 0x140876d69 | rollHz - over | Hz' = min(rollHz, cap) |
| 0x140876d6d..0x140876d76 | (Hz' - A_dot)*RollRamp + A_dot | **S3** |

CONFIRMED: the port's `s3 = (rollHz - over - aDot)*rollRamp + aDot` is identical. The rest of the block also matches: `|A - S3|` drives `s = clamp((4 - x)/3)`, `blend = 0.5 + 0.5 s`, `A = clamp(A_dot - blend*S3, ±rollToSpeedMaxRoll)`, `B = clamp(B_dot*RollRamp, ±cap)`, and `T = (A e_x + B e_z, A e_z - B e_x)` with `yRate*vmax/cap` scaling (0x140876dba..0x140876f66). Input spin is read raw (0x140876c0f..0x140876c4d): no clamp inside the block.

## 2. Spin writers: can any exceed the cap at the block input?

- Landing / contact rebuild (`ContactTick`): capped. `SpinCapScale` matches PES's `f^2` against `boundRollMax^2`, scaling X and Z (0x14087704e..0x140877081). After any contact tick, horizontal spin is at most the cap. CONFIRMED.
- The `k` blend (0x1408770b6..0x1408770c0): `k = 1` unless the ball is resting. R15B is 0 only for a resting pre-state (0x1408764c8..0x1408764cd), and BallTick returns before the contact in that case. So the port's `k = 1` is equivalent for every reachable tick. CONFIRMED.
- Magnus / flight: `FlightTick` writes velocity and position only. No spin write. CONFIRMED.
- Collision (`pesballcollision.cpp:153`): clamps |w| at `spinCap_revPerS * 2 pi` = 8 rev/s, which is 50.3 rad/s in 3-D. That is above boundRollMax (7.5 Hz horizontal), so this input can exceed the block cap by up to 0.5 Hz. The bounded push that gives is `A = RR*0.5`. The PES site of this clamp is not decoded in this session: INF.
- Kick / touch: GF stand-ins only. `Ball::SetRotation` (`ball.cpp:82..113`) writes spin from GF per-axis rates with no clamp. Callers: `humanoid.cpp:564, 660, 736, 840, 1051, 1167` (touch and shot, rates from animation) and `match.cpp:2078` (`random(-30, 30)` per axis, rad/s). `ball.cpp:171` reads it back into `fox.spinRaw` each track build. This is the only reachable route in GF for over-cap input to the block. The PES kick writer is not decoded: OPEN.

## 3. Runaway verdict

- Carried over-cap spin (state carried through `BallTick`): one bounded push, then the rebuild caps spin. Liftoff's carried probe ends at 2.48 m/s from 10 m/s over 54 ticks (bounded). I did not re-run that probe.
- The 124.9 m/s column re-supplies v/r every tick (per the Liftoff column header). No decoded PES writer does that, because the rebuild caps every tick. Re-supply comes from GF's `SetRotation` callers (above). I did not re-run that probe. The mechanism is by decode, not by measurement [INFERENCE].
- The block itself is PES-faithful for S3, so no runaway fix sits in S3.

## 4. Deviations fixed (decoded, same block)

1. **s4 band is in Hz.** `0x140876cd4` halves `hypot(wx,wz)` and divides by pi. The band `0x140876ec5` then tests that value against rollToBoundRollMin (2) and rollToBoundRollMax (7). The port tested rad/s. Effect: a horizontal spin of 8 rad/s (1.27 Hz, below the 2 Hz floor) added `rollToBoundAdd` to the restitution in the port (0.8316 vs 0.7616 at zero spin). Fix: `RollSpinRamp` uses `spinRad * 0.5f / kPi`.
2. **Zero heading takes PES's fallback e = (0, 0, 1).** `0x140876c28..0x140876c34` sets ez = 1.0, ex = 0 when the heading is under the epsilon (`0x140876c50` JC skips the divide). The port returned zero tangential, so an airborne ball with no horizontal speed and spin got no push. Fix: fallback in `RollingTangential`.
3. **Zero-heading Y rate uses the YMax bound.** `0x140876e1a..0x140876e37` skips the divide when the heading is 0 and takes `rollToSpeedSpeedYMax` as the slope, so t = 1. The port divided 0/0 once fix 2 was in place. A match crashed with `FLYING PLAYERS height -nan` until this guard was added (found by `pes_tick_a1_test`).

Note: flipping the fallback sign (ez = -1) is an equivalent mutant. The block is invariant under e -> -e, because A and B both flip and T is quadratic in e. It is not a usable mutation.

## 5. Tests

Added to `tests/sim/pes_ball_physics_test.cpp`:
- `PesBallContact.ALandingWithNoHorizontalSpeedPushesAlongItsSidewaysSpinBoundedly`: landing at zero horizontal speed with 10 Hz (over cap) spin about x. Asserts a push toward +z, `|push| <= rollToSpeedMaxKph`, vx = 0, and the value 4.64 m/s (hand-derived from synthetic column 1, the live mode).
- `PesBallContact.ASlowSpinBelowTwoRollingHertzAddsNoBounceRestitution`: 8 rad/s spin adds no restitution.
- `PesBallContact.ASpinningBallWithNoVelocityFallingFromTheAirStaysFinite`: no NaN at zero heading and zero vy.

Changed (intentional behaviour change): `PesBallContact.RollSpinRampAddsRestitutionThroughTheHorizontalSpin`. Its spin arguments were rad/s read as Hz. They are now Hz x 2 pi, which is the reading decoded in fix 1.

Red before the fix (`pes_ball_physics_test`, run before editing production code):
- `ALandingWithNoHorizontalSpeed...` failed at `s.velocity.x`: `0 vs -0.0486` (expected < -stop).
- `ASlowSpinBelow...` failed: `slow 0.8316 vs none 0.7616`, difference 0.07 = rollToBoundAdd.
- The sideways-push pin was added after the fix. At baseline it has zero tangential, so vz = 0 and it would fail its EXPECT_GT. This is inferred, not run.
- `ASpinningBallWithNoVelocity...` was added after the fix. It is red under the slope-guard mutation (below), not at baseline.

Mutations (each rebuilt and reverted; all restored):
- Fallback removed (original code): `ALanding...` and `ASlowSpin...` red, as above.
- Fallback axis swapped (ex = 1, ez = 0): `ALanding...` red on the pinned value (2.44 vs 4.64 expected; the test does not print the value).
- Fallback sign flipped (ez = -1): **not caught**. Equivalent mutant (see above).
- Slope guard removed: `ASpinningBallWithNoVelocityFallingFromTheAirStaysFinite` red (NaN).
- s4 divide changed to `/pi` (no 0.5): `RollSpinRampAddsRestitution...`, `ASlowSpinBelow...`, `ALanding...` red.

## 6. Verification

Build: `nice -n 19 cmake --build build -j3 --target pes_ball_physics_test pes_ball_track_test pes_ball_collision_test pes_tick_a1_test`, exit 0, no warnings in the final build log.

Run with `PES21_ROOT=<workdrive>/PES21`, skipped 0:
- `pes_ball_physics_test`: 35/35 (32 before + 3 new). Ran with the `RollSpinRamp` test changed as above.
- `pes_ball_track_test`: 14/14.
- `pes_ball_collision_test`: 11/11.
- `pes_tick_a1_test`: 18/18.

Gate not run (Main gates). Smoke not run.

## Open items

1. **GF kick / touch writers are the reachable over-cap source.** `Ball::SetRotation` (callers in `humanoid.cpp` and `match.cpp:2078`) writes uncapped spin every frame, and `ball.cpp:171` feeds it into the block. The PES kick writer is not decoded. Once it is, clamp at `SetRotation` to PES's rule. A clamp chosen now would be a guess.
2. **Collision cap is above the block cap.** The collision clamp is 8 rev/s in 3-D, and boundRollMax is 7.5 Hz horizontal. The block input can exceed the cap by up to 0.5 Hz. The PES site of that clamp is not decoded here.
3. **`IsResting` tests horizontal speed only.** The PES test (`0x14087648e..0x1408764c8`) uses the 3-D pre-velocity. Outside the block, not touched. A ball with y <= r, zero horizontal velocity, nonzero vy, and zero spin is treated as resting by the port and not by PES.
4. **124.9 probe not re-run.** I did not reproduce it. The runaway verdict is decode-based.
