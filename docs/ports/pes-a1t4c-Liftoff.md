# Gate fix: PesBallConversion.ASecondOfSimTicksIsASecondOfPesTicksWhileRolling, branch Liftoff

Result: Done. Test premise fixed (not the port). Branch `Liftoff` at `366bcf9` (pushed; `git ls-remote origin Liftoff` confirms). Port unchanged since `6e68e49`.

## Decision by decode
- The old test built a zero-spin BallState each tick. Under report 3.3 (FUN_140876360 0x140876c0f..0x140876f6e) that is a slipping ball: A_dot = 0, S3 = Hz'*RollRamp, A = -B*S3, so it loses speed toward rolling. The K-only reference does not model slip. Premise wrong under T4.
- A rolling ball has spin v/r (live convention `w = (v_z,0,-v_x)/r`, 13,518 rolling records, ratio 1.0). Below the spin cap (2 pi boundRollMax = 5.12 m/s on the ground, VA 0x14087704e) A_dot = Hz, S3 = Hz, B = 1, A = 0: the horizontal is K per tick (report 5.2). So a carried rolling state matches K^54.
- Above the cap the spin stops at 2 pi 7.5 rad/s, short of v/r: the contact slips and the reference is no longer the rolling keep. Ends below K^54.

## Probe (scratch `rolling_probe.cpp`, install constants, 54 ticks)

| v0 (m/s) | PES K^54 | GF rebuilt v/r each tick | GF carried, rolling start | GF carried, zero spin |
|---|---|---|---|---|
| 2 | 0.5813 | 0.5813 | 0.5826 | 0.5826 |
| 4 | 1.3569 | 1.3569 | 1.3573 | 1.1394 |
| 5 | 1.6673 | 1.6673 | 1.6670 | 1.4463 |
| 10 | 2.7057 | 124.9 (see open 1) | 2.4837 | 2.2700 |

## Tests changed
- `PesBallConversion.ASecondOfSimTicksIsASecondOfPesTicksWhileRolling`: state carried from a rolling 4 m/s start, 54 ticks within 1% of K^54, vz stays 0. Green.
- `PesBallConversion.ARollingBallAboveTheSpinCapLosesMoreThanItsRollingKeep` (new): rolling 10 m/s, state carried, must end below K^54 (cap slip). Green.

## Runs (WITH PES21_ROOT=<workdrive>/PES21; 0 skipped)
- pes_ball_track_test 14/14; pes_ball_physics_test 32/32; pes_ball_collision_test 11/11; pes_tick_a1_test 18/18.
- Gate not run (Main gates).

## Mutations (run, rebuilt, reverted)
1. Spin Z sign flipped (`newSpinZ = +newVx/r`): `ARollingBallKeepsItsRollingSpinConvention` and `ASecondOfSimTicks…` FAIL.
2. Spin cap removed (capScale = 1): `RollingSpinAboveTheCapKeepsTheCappedHorizontalFrequency` and `ARollingBallAboveTheSpinCapLosesMoreThanItsRollingKeep` FAIL.

## Open items
1. **Above-cap input spin runs away (port or decode, undecided).** A ball entering a tick with spin exactly v/r above the cap (10 m/s) gains speed: T_x > 0 from A_cap = A_dot - B*S3 with S3 = A_dot + RollRamp*(Hz' - A_dot) = 13.2 Hz vs A_dot 14.6 Hz. Over 54 ticks this goes 10 -> 124.9 m/s. A carried ball is fine (cap clamps spin each tick), so the runaway needs a non-capped input spin. Reachable? Unknown (kick/landing/Magnus spin not audited). Check: VA 0x140876d60..0x140876d76 (S3 over-removal and RollRamp weighting), and whether PES's rolling spin state is capped before it enters the block.
2. **Zero-spin slip magnitude is decode-driven, not live-checked.** At 4 m/s a zero-spin ball loses 16% over 54 ticks; per-tick tangential up to yRate*vmax ≈ 4.4 m/s. Sign is physically right; magnitude needs a live sample with zero spin on the ground.
3. **Cap not confirmed live above 5.12 m/s.** `spin_cap_live.py` on cap2.bin: the rolling rows (y == r for 3 records) only reach ~3 m/s; ratio |w_xz|/(|v|/r) = 1.000 to 3 m/s. The 2π·7.5 rad/s cap is a decode claim (VA 0x14087704e) and still unverified live.
4. Doc correction carried from earlier: contact-2 §2 places rollToSpeed scalars at BallBase 0x2AC..0x2B4 (ball.o 0xE0..0xE8); the port reads them at ball.o 0xE0/0xE4/0xE8.
