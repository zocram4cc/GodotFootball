# A1 T4 (rolling block) on branch Liftoff

Result: Partially done. The rolling tangential is ported per the decode. Acceptance NOT met: the pooled 1 s median is above main's on both harnesses. Branch `Liftoff` at `c9a3404` (pushed; `git ls-remote origin Liftoff` confirms). Earlier: `86586d2` (pre-flight landing inputs), `5e1b023` (T2 lift).

## Ported (VA evidence in code comments)
- W = K*v + T on the horizontal (contact-2 §2: `Wx = K*vx + T_x`). Block skipped for bVar3 (pre-flight y == r and |v_xz| < 2.2222 m/s, VA 0x1425cc23c).
- T: heading e from pre-flight v_xz (VA 0x1425a9668 = 0.001); A_dot = (e_z wx - e_x wz)/2pi, B_dot = -(e_x wx + e_z wz)/2pi; S3 = (f - over - A_dot)*RollRamp + A_dot with f = |v_xz post|/(2 pi r), over = max(0, f - boundRollMax); B = 0.5 + 0.5*clamp01((4 - |A_dot - S3|)/3) (VA 4.0 and -3.0); A_ = yRate*clamp(A_dot - B*S3, +-8.5)*vmax/8.5, B_ = yRate*clamp(B_dot*RollRamp, +-8.5)*vmax/8.5; T = (A_ e_x + B_ e_z, A_ e_z - B_ e_x).
- RollRamp = BaseXZ + AddRateXZ*Ramp(-3.6 vy_pre; SpeedMinXZ, SpeedMaxXZ). yRate = base + (1-base)*clamp01((|vy_pre|/|v_xz_pre| - YMin)/(YMax - YMin)). vmax = rollToSpeedMaxKPH*1000/3600.
- Spin Y rebuilds to zero (W_y = 0, contact-2 §2).
- Constants: rollToSpeedMaxRoll, rollToSpeedSpeedYMax, rollToSpeedSpeedYMin loaded from ball.o disk 0xE0/0xE4/0xE8 = 8.5 / 0.5 / 0.1 (found by scanning the install record; BallBase +0x2AC/+0x2B0/+0x2B4 in the decode). Other rows from the record. VA literals read from the install (`vaprobe`): r = 0.108685933 (DAT_142598958, = ballRadius), -1, 0.5, pi, 3600, 1000, 1, 4, -3, 0.001, 2.2222.
- Doc corrections: DAT_142598958 is 0.108686 (contact-2's 0.142599 is wrong); the rolling flag's 2.2222 is 8 km/h.

## Tests
- `PesBallContact.SpinAboutTheSidewaysAxisPushesARollingBallAcrossItsHeading` (new): topspin about +x pushes vz > 0.05 (red before: vz = 0).
- `PesBallContact.ABallRollingSlowerThanTwoMetresPerSecondIgnoresTheTangential` (new guard, green before and after).
- Install test: the three scalars (red before: compile error, fields absent).
- `TheGrounderTrimsTheVerticalSpeedAndOnlyItsFrictionOnTheHorizontal`: assertion changed to the K-only bound (the old ratio assumed no tangential).
- Final: pes_ball_track_test 14/14, pes_ball_physics_test 27/27, pes_ball_collision_test 10/10, pes_tick_a1_test 12/12.

## Numbers (1 s median, e54, pooled; corrected starts unless noted)

| variant | recorded 253 | live 168 | live cap2 123 |
|---|---|---|---|
| main T1+T3 | 0.3325 | 0.4354 | 0.3030 |
| 5e1b023 T2 | 0.4372 | 0.5523 | 0.4356 |
| 86586d2 T2 + pre-flight | 0.4993 | 0.6263 | 0.4441 |
| c9a3404 T4 (this) | 0.4459 | 0.4516 | 0.5642 |

Uncorrected harness: main 0.3864 (reproduces the stated baseline); T4 0.4639 recorded. Acceptance (< 0.333 recorded, < 0.435 live): NOT met.
Sample f2409107: T4 error at record 14 is 0.001 m (T2: 0.0002 m at 12, 0.45 m at 35 without T4); by record 29 it is 0.06 m, and by 54 it is 0.35 m. The rolling velocity still differs.

## Next unported term (VA, decoded, NOT ported)
- Horizontal spin cap: `0x14087701e … 0x140877081` (contact-2 §2.4 area). PES caps the horizontal rolling frequency f = |W_xz|/(2 pi r) at boundRollMax (7.5 Hz, `comiss` of f^2 vs 7.5^2 at 0x14087704e) and scales only the spin X and Z components (`xmm13,xmm12 *= 7.5/f`, 0x14087708d). That is a horizontal cap of 2 pi * 7.5 = 47 rad/s. GF caps the 3-D spin at 7.5 rad/s (`SpinCapScale`, rad/s). A fast roller hits the GF cap at 7.5 rad/s, where PES's cap is 47 rad/s; the rolling spin then feeds the next tick's tangential. Not ported: the existing test `SpinRebuildMovesRawSpinTowardTheRollingSpin` pins GF's 7.5 rad/s cap and needs an owner decision.
- Also unresolved: the spin-vector convention. GF rebuilds spin_x = vx/r, spin_z = vz/r. The disasm (0x140876fc7 `xmm13 = Wx - Wz`) may index the spin differently. Check against a live spin read before porting the cap.

## Files
Scratch: `<workdrive>/gf-Liftoff-scratch/` (`vaprobe.cpp`, `recdump.cpp`, `peek_ball.py`, `a1_port_probe.cpp`, `capture2.py`/`cap2.bin`).
