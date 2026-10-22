# PES port: set-piece takers, throw-in carry and the clip frame

Status: work in progress on branch `DodgeGate2`, not for merge until the release-path item is closed. Head when written: merge commit `414513c` (`origin/HeldStandby` 0698c6e merged into the branch), plus the commit that adds this note.

## Acceptance at 414513c (full rebuild)

| Check | Result |
|---|---|
| 5-minute seed-7 `--pes-loop` repro | Full time, HOM 2-1 AWA, 47198 ticks, longest stoppage 1251 ticks (bound 1757), determinism `74b70dd49a3a9522` |
| Stand suite (`SetKickStand.*`) | 10 of 10 |
| Wire suite (`pes_movestep_wire_test`) | 27 of 27, `TheKickoffTakerClosesToTheBall` green |
| Restart suite (`pes_restart_delay_test`) | 22 of 23. The red is `AThrowInTakerPlaysABallActionWhenTheBallLiesOverTheLine`, which is also red on the base commit `7db5e59` |

The 5-minute repro is the ticket's gate. Its earlier numbers were taken with a stale binary; the figures above are from a rebuilt tree.

## What changed, by PES rule

### 1. Corner SetKick heading (restart kind 4)

- The corner taker's SetKick heading is `wrap360(wrap360(side) + restartYaw)`. `side` is 315 for the straight run-up and 45 for the mirrored one (`CornerSetKickHeadingDeg`, `src/sim/onthepitch/pesgoalkick.cpp`).
- `restartYaw` is the bearing from the restart spot to `(sign(spot.x) * F8, 0)`, where F8 is `pesPenaltySpotX` (41.56). The chain is FUN_14092E7E0 (taker branch), then FUN_1408B4560 (restart kind 4). The record's +0xC is stored in FUN_1406B3DB0.
- The mirror is `FUN_1406E6660(player, 0x34) == 1`, the preferred foot in playerdata +0x34. The stronger foot now comes from the install's Player.bin record (`Match::LeftFootedOf`); the right foot stands in only for a player with no record (pes-port-PlayerDB).
- Checked against two live kickers (331.9 and 297.2 degrees): `TheCornerHeadingMatchesTheLiveKickerYaws`.
- Not ported: the field clamp FUN_141E5D4C0 (a wrapper over FUN_141E5CA20).

### 2. Goal-kick SetKick placer at yaw 0

- `GoalKickStandby::SetKick` (0x140774AE0) places the keeper through the shared placer at yaw 0 (`docs/re/pes-re-goalkick-taker.md`, "He doesn't walk to the ball"). `PrepareGoalKickStand` uses yaw 0 for the stand point and the contact root.
- The keeper's body facing is still the stand-in `KeeperFacingPitch` (90 degrees times the side). The restart record's heading for goal kicks is not decoded.
- A placer heading does not change the root-to-ball distance at contact, since a rotation keeps lengths. So this is the PES-consistent placement, not the cause of the goal-kick stalls (section 5 is).

### 3. Pickup contact-root gate (phase 1)

- While the walk is pending, the taker warps his root onto the pickup clip's contact root (clip 0xCF5), once that root is within 5 m of him (`throwInPickupReach_m`, DAT_14259222C).
- The contact root is the predicted ball minus the clip's ball offset, turned by the taker's heading (`PickupContactRoot`, `FacingHeadingDeg`, `src/sim/onthepitch/pesgoalkick.cpp`).
- The walk's target stays the predicted ball until the gate passes.

### 4. Carry lift (phase 4)

- Inside 0.65 m of the carry spot (`throwInCarryStop_m`, DAT_14259d5d0), and with |x| inside F0 minus `throwInCarryEndInset_m`, the root warps onto the spot over 14 frames (clip `throwin_seamless_liftupball`, 14 frames).
- The release waits for the lift (`pieceLifted`), and `PieceTakerArrived()` returns it.
- Not ported: the yaw-warp to 180 degrees (or 0 when S.z < 0) that the lift clip carries.

### 5. Body-frame clip cache (the root cause of the stoppage freezes)

- An anim's root cache (`GetAnimPositionCache`) is in the anim's own frame, where forward is (0, -1).
- The positions of a `SelectAnim`-chosen clip are built by the physics model in world frame (`positions_ret`, `CalculatePhysicsVector`). `CalculateSpatialState` and `HumanoidBase::Process` apply `positions` as world.
- MOVESTEP-issued clips (`PlayIssuedClip`) and the placed idle clip (`ResetPosition`) installed the raw cache. A turned body's root therefore travelled in the anim's frame. Placed corner takers ran away from the ball, and the seed-7 stoppages froze.
- Fix: `TurnCacheToBody` (`src/sim/onthepitch/player/humanoid/humanoidbase.cpp`, anonymous namespace) turns the cache by the body's angle at install: `spatialState.angle` in `PlayIssuedClip`, `startAngle` in `ResetPosition`. The non-cache branch of `CalculateSpatialState` already rotated by `startAngle`, so one convention now holds.
- Effect: the five full-match stall tests (1758-tick stoppages) pass, the throw-in walker's run after start is gone, and the release tests pass.

### 6. Carrier approach: kept at the 2 x gap stand-in, and why

- Tried: a sprint snap for far gaps (`RangeVelocity` above 3.5 m/s). The sprint overshot the spot with the body's momentum, and the throw was awarded again at its own spot (`AThrowInIsNotAwardedAgainOnItsOwnSpot`).
- Tried: the walk level (5 m/s) above the dribble floor. It fixed the re-award, but a released ball stayed outside the touchline for more than 27 ticks (`AReleasedThrowInCarriesItsBallInsideTheTouchline`), and seed 23's goal-line throw-in stalled for 1758 ticks.
- Reverted to `FloorVelocity(kPieceTakerApproachGain * gap)`. With that, the four affected tests pass. The stand-in is named in the code: above 4 m the command is off the movement table, and MOVESTEP reads that as TURN_STOP.
- The decode of PES's writer of the desired speed (cmd+0x172) is the item that replaces this stand-in (open items, below).

## Stand-ins

| Stand-in | PES site it replaces | Status |
|---|---|---|
| Right foot for the corner and goal-kick mirror | playerdata +0x34, FUN_1406E6660 | Closed (PlayerDB): `Match::LeftFootedOf`, Player.bin stronger foot (bit 514); right foot for a player with no record |
| Physique 1.0 | player +0x3B18, FUN_1406E8CC0 | Open |
| Carrier approach at 2 x gap | the writer of the desired speed (cmd+0x172) | Open: decode in progress |
| Keeper facing at 90 degrees times the side | restart record heading +0xC | Open |
| Ball pinned to the taker's root during the carry | release-key offset, 0.8 m ahead at 1.85 m height | Open |
| Carry yaw-warp not ported | phase 4 yaw-warp to 180 / 0 degrees | Open |
| Field clamp FUN_141E5D4C0 not ported | corner heading | Open |

## State inherited from the integration branch

- A held set-piece taker is not steered by MOVESTEP (PES standby slot 10 skips the default locomotion driver), commit `4a8aa56`.
- `ResetPosition` anchors its mid-clip idle frame on the placement point, commit `0698c6e`.

## Open items

- Release path: the in-play timing at the release event, and the ball re-attaching after release.
- `AThrowInTakerPlaysABallActionWhenTheBallLiesOverTheLine`: red on the base commit. Release-path item.
- The writer of the desired speed (cmd+0x172) for the set-piece walker.

## Notes for the next reader

- The tests assert behaviour, not source text.
- The stoppage bound (1757 ticks at 54 Hz) is the install's bound. Any change to the sim changes the determinism hash.

## Handover 22-10-26 (DodgeGate's stopping report)


Branch: `DodgeGate2`. Head: `9eaad1e` (merge of `origin/RestartRef` 869a43a). Pushed; the remote confirms it.
Tree: clean. Nothing uncommitted, so there is no patch to carry over.

## Commits on the branch (oldest first, on 7db5e59)
- `8e8a925` WIP: pickup contact-root gate, carry lift, body-frame clip cache, goal-kick placer yaw 0.
- `0c1f7da` WIP: the two carry tests measure the taker's root (PES lift semantics).
- `d85b017` WIP: carrier approach back to the 2 x gap stand-in (the walk-level cap and the sprint snap are reverted).
- `414513c` merge of `origin/HeldStandby` 0698c6e (held taker not steered by MOVESTEP; ResetPosition idle anchor).
- `a1f75b8` docs: `docs/ports/pes-port-DodgeGate.md` and the throw-in note's section 14.
- `9eaad1e` merge of `origin/RestartRef` 869a43a (judge gate; reframe of AThrowInTakerPlaysABallActionWhenTheBallLiesOverTheLine). Approved by the integration owner.

## Verified
- On `414513c` after a full rebuild: 5-minute seed-7 `--pes-loop` repro reaches full time (HOM 2-1 AWA, 47198 ticks, longest stoppage 1251 ticks against the 1757 bound, determinism 74b70dd49a3a9522). Stand 10/10. Wire 27/27. Restart 22/23; the red was the reframed test, which `9eaad1e` now covers.
- `9eaad1e` is not verified by this branch. The integration owner is verifying it in the integration branch.

## Open
1. Approach speed. `src/sim/onthepitch/player/controller/elizacontroller.cpp`, the walk branch, uses `FloorVelocity(pes::kPieceTakerApproachGain * gap)`. Above about 4 m that is off the movement table, and MOVESTEP reads it as TURN_STOP. PES's writer of `cmd[0x172]` (desired speed) and `cmd[0x17]` (class) for the set-piece walker is NOT decoded. A bridge-bounded attempt (20 requests) found the MOVE-family callers `0x1407824A0`, `0x140789DD0`, `0x1407D1E60` (which reach FUN_1406B8340), but no writer and no formula.
2. Release path (RestartRef's items 1 and 2): `StartPlay` at `started` against PES's phase-5 release event (116C 3 to 1), and the generic ShortPass release clip against the throwin_release rows. The owner is not assigned.
3. The carry yaw-warp (phase 4, 180 / 0 degrees) is not ported. The field clamp FUN_141E5D4C0 is not ported.
4. Stand-ins are listed with their PES sites in `docs/ports/pes-port-DodgeGate.md`: right foot, physique 1.0, approach speed, keeper facing, ball pinned to the root during the carry.

## Next concrete action
- Decode the `cmd[0x172]` / `cmd[0x17]` writer for the set-piece walker. Start from the throw-in sequencer `FUN_140791EC0` (phase 1 walk and phase 4 carry) and from the MOVE command builder that `FUN_1406B8340` consumes. Search the decompiles for stores at command word `0x172` (desired speed) and `0x17` (class). One bridge request at a time.
- Then replace the walk branch's `FloorVelocity` rule with the decoded one, and verify: `AThrowInIsNotAwardedAgainOnItsOwnSpot`, `ACarriedThrowInReachesItsSpot`, `AReleasedThrowInCarriesItsBallInsideTheTouchline`, `TheSeedTwentyThreeKickOffRestarts`, and the 5-minute seed-7 repro. Do not replace the rule without these.
