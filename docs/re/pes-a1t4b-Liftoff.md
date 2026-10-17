# A1 T4b: PES's horizontal spin cap and rolling spin convention, branch Liftoff

Result: Done for the cap and the convention. The 1 s median is now below main on both harnesses. Branch `Liftoff` at `6e68e49` (pushed; `git ls-remote origin Liftoff` confirms). Commits: `68bfc9e` (cap and convention), `6e68e49` (comment only).

## Spin-vector convention (live, read-only)
- Source: `cap2.bin` (live capture, 9,368 snapshots). 13,518 rolling records (y == r at records k-1, k, k+1, speed >= 1.5 m/s). Spin raw x 492.181 vs post-contact velocity from the next position step.
- Relative error, median: `w_x = +v_z/r` and `w_z = -v_x/r`: 0.000 / 0.000. The other three sign patterns give 2.0. Magnitude `|w_xz|/(|v|/r)` median 1.0.
- So PES's raw rolling spin is `w = (v_z, 0, -v_x)/r`. The old GF rebuild `(v_x/r, 0, v_z/r)` was a different axis: the bug the test pinned.
- Script: `<workdrive>/gf-Liftoff-scratch/spin_conv.py`.

## Ported
- Spin rebuild from the post-contact W: `w_x = W_z/r`, `w_y = 0`, `w_z = -W_x/r` (raw = w/492.181), times the cap scale on X and Z only (VA 0x14087708d). Blend k = 1 on the airborne path (`kRawSpinBlendFactor` = 1).
- Cap: `SpinCapScale(c, |W_xz|)`: f = |W_xz|/(2 pi r) in Hz, scale = boundRollMax/f above it (VA 0x14087704e: f^2 vs boundRollMax^2; f = |v_xz|/(2 pi r) at 0x140877013-0x140877046). The old 3-D rad/s cap is removed.

## Tests (TDD)
- `SpinIsCappedAtBoundRollMaxHertzOfHorizontalRolling`: SpinCapScale in Hz.
- `ARollingBallKeepsItsRollingSpinConvention`: a ball already rolling at 4 m/s: spin x = 0, y = 0, `w_z = -v_x/r`. Red on the old axis convention (mutant caught).
- `RollingSpinAboveTheCapKeepsTheCappedHorizontalFrequency`: 20 m/s: horizontal spin 2 pi * 7.5 rad/s, x = 0. Red on the old 3-D cap.
- The old `SpinRebuildMovesRawSpinTowardTheRollingSpin` (7.5 rad/s 3-D cap, `x > 0`) is replaced. Report 5.2's cap assertion uses the horizontal form.
- Mutants run and reverted: old axis convention (2 tests fail); cap in rad/s (3 tests fail).
- Final: pes_ball_track_test 14/14, pes_ball_physics_test 28/28, pes_ball_collision_test 10/10, pes_tick_a1_test 12/12.

## Numbers (1 s median, e54, pooled; live probe harness, same samples)

| variant | recorded 253 (corr) | live 168 (corr) | cap2 123 (corr) | recorded (uncorr) | live (uncorr) |
|---|---|---|---|---|---|
| main T1+T3 | 0.3325 | 0.4354 | 0.3030 | 0.3864 | 0.4720 |
| c9a3404 T4 (no cap) | 0.4459 | 0.4516 | 0.5642 | 0.4639 | 0.4730* |
| 68bfc9e T4b (cap, convention) | 0.1817 | 0.2326 | 0.1632 | 0.2338 | 0.2442 |

*uncorrected T4 live not measured; the figure is from the corrected row. Acceptance (below main's 0.333 recorded and 0.435 live, corrected): MET, 0.182 and 0.233.

Caveats: the mean error is still 0.61 m recorded and 0.60 m live (main 0.66 / 0.74), so the tail is not gone (max 5.2 m recorded, 4.6 m live). The 1 s median is what moved most. The landing velocity and the rolling-phase per-tick errors were not re-measured this step.

## Next
- Re-measure the landing metric (`A1_LAND`) and per-sample tails on the new branch before any merge.
- Remaining unported: the Y spin pump (contact-2 §2.1, `0x1408770a5`…`0x1408771cd`, raw-Y blend with RollRamp-like factor); the spin-vector convention for the flight Magnus is now PES's, which the flight tests may not cover.
