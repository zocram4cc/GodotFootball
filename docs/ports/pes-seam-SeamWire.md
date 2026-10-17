# SeamWire: MOVESTEP team seam, NearestOpponent from the RunWorld

Branch `SeamWire`, pushed. `git ls-remote origin SeamWire` = `0a2f71bc3f548c3ebe68ff935ffc7d047e264c9a`. Base `origin/main` d5f3065. Worktree `<workdrive>/gf-SeamWire`. Gate not run (Main gates).

## Changes
- `src/sim/onthepitch/teamAIcontroller.{hpp,cpp}`: keeps the last RunWorld and its roster (`lastRunWorld`, `lastRunIds`, set in `ProcessRunManager`). New: `RunSlotOf`, `RunPlayerAt`, `NearestOpponentOf`.
- `src/sim/onthepitch/pesmovestep.cpp`: `NearestOpponent` answers from `NearestOpponentOf`; the stand-in and its ponytail are gone. The seam's own slot (`OwnSlot`, used by `ActionIndex`) is the RunWorld slot, not `GetID()`. Opponent `Position`, `Velocity`, `Heading` read live players via `Who(slot)`.
- `tests/sim/pes_nearest_opponent_test.cpp` (new), `tests/sim/CMakeLists.txt`.

## Build-order finding (no reorder)
The RunWorld is not per tick. `ProcessRunManager` builds it on the team's parity frame (`(frame & 1) == team id`), only in play and not in a set piece. `Team::Process` runs the team controller before that team's players, so a same-team build precedes its MOVESTEP. The other team's build comes later in the tick. The seam reads the last build, so it is at most one frame old in play, and stale through stoppages.

## Tests (red → green)
- `PesNearestOpponent.RankZeroIsTheNearestOpponentNotATeammate`. Red: the stub returns none, so the test fails at `ASSERT_NE(opp, nullptr)` (exit 1). Green: passes.
- `PesMoveStepJostle.AwayStarterJostlesTheNearOpponentInRunLoop`: away starter (match id 22), home opponent 0.57 m off at 45°, `ProcessMoveStep` in RUN_LOOP, compared with the opponent far away. Passes; the jostled style is non-zero, the base is 0.

Mutations, each rebuilt, observed red, then reverted to the same blob:
- M1 seam answers none (main's behaviour): jostle `0 vs 0`.
- M2 own slot = `GetID()` (the Main-requested red): jostle `0 vs 0`.
- M3 `RunSlotOf` ignores team block: both tests red (slot 0 for away starter, expected 11; 6 for own, expected 17).
- M4 farthest opponent: both tests red.
M1 and M2 ran in the -O2 build; the final runs are Release.

## Verification
- `pes_nearest_opponent_test`: 2/2 pass (Release, PES21_ROOT=<workdrive>/PES21).
- `pes_movestep_test` 91/91, `pes_movestep_run_test` 40/40, `pes_movestep_turn_test` 42/42, `pes_movestep_step_test` 13/13.
- `ninja -t deps | grep -c "#deps 0"` = 0 after the header layout change.
- Smoke (2 min, seed 7, direct, no lock): exit 0, `[determinism] d4a22e3211c6940e` on two runs, 32125 ticks to full time, passes 7-11 and shots 3-4 per side, 16 stoppages with longest 14.4 s. No baseline run of main's binary, which the brief rules out; the away-starter comparison belongs in Main's batch smoke.
- Not run: other sim test binaries that include `teamAIcontroller.hpp` (Main gate).

## Open items
1. The proximity cap still cannot fire: `ActionBit27(opp)` (hu+8 bit 27, A-CLS, writer not decoded) is still a named stand-in, as Main ruled. The ramp-target acceptance therefore cannot pass until A3Helpers decodes it.
2. Opponent PC (0x28) reads 0. GF has `physical_contact`, but the mapping is not decoded, so I did not guess one. Own PC is also 0.
3. Away starters now get their RunWorld slot (11..21) instead of `GetID()` (16..31). Their acceleration ability, factor branch and jostle gates change, so the 2-min and 20-min smokes will differ from main. This is the decision Main approved.
4. Staleness: a substitute has no slot until the next in-play build; the ranking is stale through stoppages and set pieces.
5. Build hygiene: a fresh configure without `CMAKE_BUILD_TYPE` (-O2) fails to link `gfsim_run` with undefined `pes::movestep::LocomotionStyle` (`pesfmt` ↔ `pesmovestepcore` archive order). With `Release` (main's setting) it links. Not investigated further.
6. `extern/godot-cpp` is a symlink in the worktree and was not committed. Staged paths only.
