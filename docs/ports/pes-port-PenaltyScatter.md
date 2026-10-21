# PenaltyScatter: PES's penalty aim scatter (partial)

Branch `PenaltyScatter` (agent PenaltyScatter, 21-10-26; note written by Main from its final message, since the agent wrote none).

## Done
- `src/sim/onthepitch/pespenaltyscatter.{hpp,cpp}`: FUN_1406B4000's kick-kind-54 scatter radius and FUN_1406E3210's kick-kind-54 jitter, every constant read from the install through pes::PeImage::ReadVa (0.6 @0x14257a190, 0.4 @0x14259b89c, 40 @0x14259be2c, 99 @0x1425c48cc, 59 @0x1425cb2fc, 0.5 @0x142565600, 0.2 @0x142598578, 0.3 @0x142590d98, 0.8 @0x142572348, 0.1 @0x142588200, 0.15 @0x1425c48a0, 3 @0x14258c264, 0.25 @0x142592228, 0.9 @0x14258c260, 1000 @0x14256ae34).
- Rule: acc = (uint)(placeKicking·0.6 + finishing·0.4) & 0xff, q = ramp(acc, 40, 99); base 0.5 (0.4 with PK Specialist), a chip adds 0.4 (0.2 with Chip Shot Control); radius = (base − 0.2q) + ramp(charge, 0.2, 1.0)·(0.8 − 0.3q) + ramp(|aim.lateral|, post/3, post·0.9)·(0.15 − 0.1q) + ramp(aim.height, bar·0.25, bar·0.9)·(0.3 − 0.2q). post = 3.66 m, bar = 2.44 m: the pitch/goal geometry object *(*(0x1436f3dc8)+0x2aa8) (+8, +0xC), read live. Jitter: two signed draws in [−radius, radius] (mm) from the taker's RNG slot 2, lateral first, height second (FUN_1406e3210 case 0x36, re-decoded in PenaltyIntegrate: four slot-2 draws in all); the HEIGHT is clamped at 0 (fixed in 9d42629).
- `tests/sim/pes_penalty_scatter_test.cpp`: 8 tests green (red → green on the ramp pairing and the clamp).

## Integration (PenaltyIntegrate, branch `PenaltyIntegrate`)
- Done: the shootout's aim is `PenaltyShootout::AimPenaltyKick` (the scatter around the goal's centre, drawn from the taker's own generator). `CalculateAim`, `IsOnTarget`, `GetSaveChance`, `ResolveKick` and the controller's planned outcome are deleted; the ball and the keeper decide.
- Done: the constants reach the shootout through `KickErrorConstants::penaltyScatter` (`match.cpp` untouched). The draw count is re-checked.
- Partial: the base point's height is bar/2, a stand-in for FUN_1408abc30 (the kind-54 tuning entry and geometry +0x3C are runtime; not yet read). Lateral is zero (the penalty branch's scale).
- Open: the height has no carrier. `elizacontroller.cpp` (busy) builds the touch direction with z = 0; `GetAimHeight()` is stored and unread.
- Stand-ins (each a `ponytail:` comment): charge = `physical_shotpower`, Place Kicking = `technical_setpiece`, post/bar = GF's `goalHalfWidth`/`goalHeight`.
