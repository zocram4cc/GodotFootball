# PenaltyScatter: PES's penalty aim scatter (partial)

Branch `PenaltyScatter` (agent PenaltyScatter, 21-10-26; note written by Main from its final message, since the agent wrote none).

## Done
- `src/sim/onthepitch/pespenaltyscatter.{hpp,cpp}`: FUN_1406B4000's kick-kind-54 scatter radius and FUN_1406E3210's kick-kind-54 jitter, every constant read from the install through pes::PeImage::ReadVa (0.6 @0x14257a190, 0.4 @0x14259b89c, 40 @0x14259be2c, 99 @0x1425c48cc, 59 @0x1425cb2fc, 0.5 @0x142565600, 0.2 @0x142598578, 0.3 @0x142590d98, 0.8 @0x142572348, 0.1 @0x142588200, 0.15 @0x1425c48a0, 3 @0x14258c264, 0.25 @0x142592228, 0.9 @0x14258c260, 1000 @0x14256ae34).
- Rule: acc = (uint)(placeKicking·0.6 + finishing·0.4) & 0xff, q = ramp(acc, 40, 99); base 0.5 (0.4 with PK Specialist), a chip adds 0.4 (0.2 with Chip Shot Control); radius = (base − 0.2q) + ramp(charge, 0.2, 1.0)·(0.8 − 0.3q) + ramp(|aim.lateral|, post/3, post·0.9)·(0.15 − 0.1q) + ramp(aim.height, bar·0.25, bar·0.9)·(0.3 − 0.2q). post = 3.66 m, bar = 2.44 m: the pitch/goal geometry object *(*(0x1436f3dc8)+0x2aa8) (+8, +0xC), read live. Jitter: two signed draws in [−radius, radius] (mm) from the taker's RNG slot 2, height first, lateral second; the HEIGHT is clamped at 0 (fixed in 9d42629).
- `tests/sim/pes_penalty_scatter_test.cpp`: 8 tests green (red → green on the ramp pairing and the clamp).

## Not done (the integration)
1. Nothing calls the module: GF's PenaltyShootout::CalculateAim / IsOnTarget / GetSaveChance / ResolveKick and penaltyshootoutcontroller.cpp:188-217 are still GF logic.
2. Constants plumbing: the lazy route is a member of KickErrorConstants (peskickerror.*), reachable through match->GetPesKickErrorConstants() without touching match.cpp.
3. PES's base aim point comes from FUN_1408ABC30 via the per-kind tuning table FUN_141E5CA20(ctx, 0x36): not yet read.
4. GF's shot command has no height input (GetShotVector derives it; ClampShotArrivalVelocityZ holds 0.3-2.1 m), so the height term and draw have no carrier yet.
5. The draw count per jitter (4 Range(2) calls) is not re-checked against FUN_1406EAA10.
6. The in-match penalty taker (elizacontroller.cpp, its own random(−5, 5) aim) is out of scope.
