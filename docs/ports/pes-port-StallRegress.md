# PES stall regress: the 5876 live firings on c6f0f8e3 (StallRegress)

Branch `StallLiveRun` pushed at `68872384ea` (parent `c6f0f8e3`), confirmed by `git ls-remote`. Result: 5876 -> 151. The <=118 target is NOT met (33 over).

## Census (CONFIRMED, run on this machine)

Full `PesStall.ALiveBallNeverLiesStillLongEnoughForTheBreaker` on c6f0f8e3 with `GF_STALL_DUMP=1`: 5876 live firings, reproduced (the test fails with 5876). Episodes (gap > 1 s), match time:

| start | end | ticks | forced man | clip | note |
|---|---|---|---|---|---|
| 52.6 s | 53.7 s | 58 | team 0 id 8 | idle_0_3_run | the PesStallQuick window (unchanged) |
| 264.2 s | 355.4 s | 4930 | team 0 id 0 (keeper H1) | gkmovelow_guard_0_0 | THE episode |
| 438.7 s | 450.9 s | 659 | team 1 id 26 | run_3_0_045 | |
| 483.3 s | 484.3 s | 52 | team 1 id 26 | idle_0_3_run_045 | |
| 775.5 s | 778.8 s | 177 | team 0 id 4 | 000_180 | |

## Mechanism of the 4930-tick episode (CONFIRMED from per-tick trace)

1. 240.6 s: team 1 free kick prepared (piece 3 = e_SetPiece_FreeKick). Taker A2 (id 17) placed 2.3 m from the ball (`PrepareSetPiece`, the FreeKick branch).
2. 247.8 s: restart started; 248.8 s: taker's highpass touch (touchAnim, `lastTouch=248814`), `inSet` drops to 0 at 248.85 s. The ball leaves and comes to rest near team 0's goal.
3. 256.1 s: keeper H1 (team 0 id 0) touches the ball (`ret=0`), loses it at 256.2 s. From then on the ball has no retainer for the rest of the episode, but `PesHolder` stays on H1: `RecomputePesHolder` (match.hpp) is the only writer, and the comment there states PES's MA+0x2601C persists on a loose ball by design. So this is faithful.
4. 264.2 s: the breaker (5 s idle) designates H1 (nearest by time to ball, 2.76 m). H1's decision is a `BallControl` (fn=2) with velocity 0 and no target, queued by `PlayerController::_BallControlCommand` at the raw input velocity (0 for the keeper). `MovementDecision` keeps it because `holdsBall` is true (PES holder).
5. MOVESTEP reads that decision: target = his own position, `kDistToTarget = 0`, so `StayDecision` (FUN_1406b6bb0, GF port in `src/pes/movestep_group_a.cpp`) is false. `Idle` (FUN_1406bf0c0) and `IdleOther` (FUN_1406bf7f0) alternate between states 1 and 17 each tick. Body speed ~0 for 91 s. The breaker fires every tick (about 54 Hz), so the census counts about 4930.

Other episodes at 438 s, 483 s and 775 s are not traced in detail; after the fix they are gone or shortened.

## Fix (CONFIRMED by the census run)

`src/sim/onthepitch/player/controller/playercontroller.cpp`, `_BallControlCommand`: a designated man without possession now takes `AI_GetBallControlMovement` toward the ball (walkVelocity, the same walk the set-piece taker path already uses), instead of his raw input velocity. Named ponytail: PES's loose-ball approach is not decoded.

## Numbers

- ALiveBall census: c6f0f8e3 = 5876; with the fix = 151. Remaining 151 = 58 (52.6 s, same as the quick window, predates the fix) + 25 (288.8 s, team 1 id 25 approach, 0.89 m -> 0.84 m) + 29 (338.2 s, keeper approach, gkmovelow_guard_0_0_mirror) + 39 (440.4 s, team 1 id 19 approach). The 25 to 39 tick windows are approaches to a loose ball, not a stuck state.
- `PesStallQuick`: 58 (unchanged; the target was already red at 58 on c6f0f8e3, before this fix).
- `pes_throwin_release_test` (ankle suite): 9/9 PASSED.
- `pes_hang_test`: 1/1 PASSED.
- `gen_translated.py --image pes_full.bin --check`: exit 0, 10150 functions regenerate byte-identical.

## Mutation

Reverting the fix on c6f0f8e3 gives 5876 in the same census (run before any change). With the fix: 151. The red test is the existing ALiveBall test; the stall episode fails it at 5876 (red) and at 151 (still red; <=118 not met).

## Bisect (NOT done)

Not measured: a2439354 on this machine. The only behavioural diff between a2439354 and c6f0f8e3 is the throw-in lift warp (`810de3b5`). The stalled restart is a free kick, and `PrepareSetPiece` resets every piece flag, so the warp cannot leak into it. The warp changes the timeline and so the chance of reaching this state. The root (a designated man without possession gets a zero-velocity BallControl) does not depend on the warp, so the bisect would not change the fix. OPEN: whether a2439354 also reaches this keeper state on this seed.

## Open items

1. Target <=118 not met: 151. The three approach windows (25/29/39 ticks) and the 58-tick window at 52.6 s are unresolved. Each is a few hundred ms; reducing them would be tuning.
2. PesStallQuick (`forced_live=58`) fails its `EXPECT 0` on c6f0f8e3 and on this branch. Pre-existing.
3. Keeper PES mover (KP_PRESS, action 0x3e) is still a GF stand-in (`ponytail` in `pesmovestep.cpp` and `elizacontroller.cpp:814`).
4. The loose-ball approach has a ponytail: PES's approach is not decoded.

## Hygiene

- Diagnostic trace (`GF_RESTART_TRACE`, DIAG comments) removed before the commit. Test file equals c6f0f8e3 apart from the census tooling already on `StallLive`.
- Commit identity: zocram4cc, through the commit namespace (the worktree's godot-cpp symlink was swapped for an empty dir for the commit and restored).
- No new constants; `walkVelocity` from `velocitystate.hpp`.
