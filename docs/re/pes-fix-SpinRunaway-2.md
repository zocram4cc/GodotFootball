# Resting test, kick-writer spin, collision cap, and the 124.9 probe re-run (branch SpinRunaway)

Result: Partially done. Item 3 is fixed (TDD). Item 2 is decoded, and the decode says PES's kick writer applies no cap, so I did NOT clamp `Ball::SetRotation`; this needs Main's decision. Item 1 (collision cap site) is decoded. The probe is re-run on the final tree.

Commit: `aa66686` on `SpinRunaway`, pushed. `git ls-remote origin SpinRunaway` = `aa6668677a5f87b7268b40afe37f75229348887b`. Previous commit `859047a` is the parent.

## 1. Item 3: IsResting squares the 3-D speed (red then green)

Decode (FUN_140876360): `0x140876488` compares `[r9+4]` (y) with `r` (`0x142598958`). `0x14087648e..0x1408764c8` sum `vx^2 + vy^2 + vz^2` from the pre-velocity (`[rsi]` = `r9+0xc`, saved at `[rsp+0x58..0x60]`) and compare against 0. The spin sum is compared at `0x140876494`. So the test is 3-D, not horizontal. CONFIRMED.

Test added: `PesBallContact.ABallMovingUpAtRestHeightIsNotResting` (ball at rest height, velocity (0, 2, 0), zero spin).
- Red (before the change): `IsResting` returned true, the ball was zeroed (`velocity.y 0 vs 1.5`, `position.y` not lifted).
- Green: `IsResting` uses `vSq = vx^2 + vy^2 + vz^2`. The test passes: `pes_ball_physics_test` 36/36.
- Mutation (horizontal-only `vSq`): `ABallMovingUpAtRestHeightIsNotResting` fails. Rebuilt, run, reverted; the restored file is the committed one.

## 2. Item 1: the kick/touch spin writer. Verdict: uncapped. No clamp applied.

Chain (all raw disasm from the Ghidra bridge; no decompiler names used for the decisions):
- `FUN_140870a40` is the kick application for an intended touch. It sets the intended-touch flags (`+0x294 |= 0x13`, `+0x298`), builds a candidate state with `FUN_140874860` (return buffer at `rsp+0x80`), and copies it into the ball at `0x140870cef` (`FUN_140731bb0`: pos, vel, spin at `+0x18`). Then `0x140870d28` computes the spin magnitude into `simBall+0x20` via `FUN_140875550` (raw to rad/s, `x 492.181`).
- `FUN_140874860` zeroes the candidate spin (`[rdi+0x18]`), then, inside the gate `[rbx+0xec] > 0` (`0x1408749f4..0x140874a00`), calls `FUN_140875580` once (`0x140874a2a`).
- `FUN_140875580` (80 lines) calls `FUN_140875170` once (`0x1408756e8`).
- `FUN_140875170` is the per-record response R(p5, p6, n, n', T, E) of `pes-re-ball-collision.md` 5. It returns `v_out` and `ω_out` with no cap. Its `ω_out = (n' × u)/(2r·1.1) + 1.1·(w1 × n)/(2r)`.
- The cap constant `0x14259bdf4` (8.0) does NOT appear in `FUN_140870a40`, `FUN_140874860`, `FUN_140875580` or `FUN_140875170`.

So PES's kick writer sets the ball's spin to R's `ω_out`, uncapped. The one-sided cap sits in the accumulator, not on this path (see §3).

Why I did not clamp `SetRotation`: `Ball::SetRotation` is GF's stand-in for this writer, and PES's writer has no clamp. A clamp at 8 rev/s would impose a rule PES's kick writer does not have. Left as is. I added a `// ponytail:` comment at `ball.cpp:85..89` naming these PES sites, which will be replaced when R's foot response is ported.

Magnitude is [INFERENCE]: R's `ω_out` for a kick at about 30 m/s (u ~ 0.75·v) is order 90 rad/s (~14 rev/s), which exceeds 8 rev/s. Not measured: it needs the foot kinematics.

What clamps it afterwards: the next contact's rebuild, `SpinCapScale` in `ContactTick` (`0x14087704e..0x140877081`), caps the horizontal spin to boundRollMax. The probe in §4 (mode 3) shows a one-off 2x v/r spin is bounded.

## 3. Item 2: the collision spin cap's PES site

`FUN_14086d6f0` (the unintended-contact accumulator, doc §5):
- `0x14086efb4`: `ω' · 0.5 / π` (`0x142565600`, `0x142578480`), which is `|ω'|/2π`.
- `0x14086efbd`: `MOVSS XMM1, [0x14259bdf4]` = 8.0 (`spinCap_revPerS`).
- `0x14086efd2..0x14086efd5`: `COMISS`/`JBE`; if above 8, `ω' *= 8/(|ω'|/2π)`, i.e. `ω' *= 8·2π/|ω'|`, written back at `0x14086efe8..0x14086efed`.

The GF `pesballcollision.cpp` cap (`Length(wo) > spinCap_revPerS·2π`, scale to the cap) uses the same constant and the same 3-D form. CONFIRMED for body contacts. Whether the goal-frame path (`FrameContact`) shares this accumulator is [INFERENCE]: the doc's §3 static objects are weighted into the same `Σ w_c` form, but I did not decode that site separately.

Note: `pes-re-ball-collision.md` §5 says the cap is 50.27 rad/s in 3-D. The horizontal block cap (boundRollMax = 7.5 Hz) is lower, so a collision can leave horizontal spin above the rolling cap by at most 0.5 Hz. The next contact's `SpinCapScale` removes it.

## 4. The 124.9 probe, re-run on the final tree

Same method as Liftoff's `rolling_probe.cpp`: install constants, `ContactTick`, `step = 1/54`, 54 ticks, start at rolling spin v/r along x.

Liftoff's binary, run as built on its own tree: `v0 = 10`: rebuilt = 124.9149 (reproduces the report). The current tree differs by the s4 band fix (rad/s to Hz), which changes the restitution as the ball slows. That explains 124.9 to 94.6 without touching the runaway.

Final-tree results (m/s, horizontal speed after 54 ticks):

| v0 | PES K^54 | 0: rebuilt v/r each tick | 1: rebuilt, capped 8 rev/s each tick | 2: carried, rolling start | 3: carried, one-off 2x v/r spin |
|---|---|---|---|---|---|
| 2 | 0.5813 | 0.5824 | 0.5824 | 0.5824 | 0.5824 |
| 4 | 1.3569 | 1.3572 | 1.3572 | 1.3572 | 1.6081 |
| 5 | 1.6673 | 1.6670 | 1.6670 | 1.6670 | 1.9381 |
| 10 | 2.7057 | **94.5597** | 2.5557 | 2.4838 | 2.7763 |

Reading:
- Mode 0 (re-supplied uncapped v/r every tick) still runs away at 10 m/s. Its only source is a writer that re-supplies uncapped spin every tick, which PES does not have. The kick is one-shot, and the collision cap is 8 rev/s.
- Mode 1 (the PES collision cap applied to re-supplied spin) is bounded: 2.56 m/s.
- Mode 2 (carried) is bounded: 2.48 m/s.
- Mode 3 (one-off over-cap spin, the kick case) is bounded: 2.78 m/s at 10 m/s, 1.61 at 4 m/s, against 1.36 with no over-cap spin.

Verdict: PES's input clamps are (a) `SpinCapScale` at every contact tick (the carried spin is capped each tick), and (b) the 8 rev/s accumulator cap for collisions. The kick input is uncapped but one-shot, and mode 3 is bounded. So the 124.9 runaway needs a per-tick uncapped re-supply, which I found no PES writer doing. [INFERENCE for GF's `SetRotation` callers: they run per touch or per dribble frame, not per contact tick; I did not trace their call rate.]

The probe was temporary and is removed from the tree. Nothing in the commit depends on it.

## 5. Verification

Build: `nice -n 19 cmake --build build -j3 --target pes_ball_physics_test pes_ball_track_test pes_ball_collision_test pes_tick_a1_test`, exit 0, 0 warnings. Run with `PES21_ROOT=<workdrive>/PES21`, 0 skipped:
- `pes_ball_physics_test` 36/36 (35 + `ABallMovingUpAtRestHeightIsNotResting`)
- `pes_ball_track_test` 14/14
- `pes_ball_collision_test` 11/11
- `pes_tick_a1_test` 18/18

Gate not run (Main gates). No path or PII leaks in changed files (grep for machine paths).

## 6. Open items

1. **Decision for Main: `SetRotation` clamp.** PES's kick writer is uncapped (§2). I did not clamp it. If you want a clamp anyway, it is a deviation and needs a named reason. The real fix is porting R's foot response (`FUN_140875580`) into `SetRotation`'s caller.
2. Kick spin magnitude is not measured. Needs a live kick sample with foot kinematics.
3. Whether the goal-frame path shares the 8 rev/s accumulator is not decoded (§3).
4. GF's resting path still zeroes `velocity` (`BallTick`). I did not check PES's resting path for velocity handling. Out of this block.
