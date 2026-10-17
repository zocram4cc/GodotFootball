# IdleEpisodes round 2 (branch `IdleEpisodes`, SHA d47e785, pushed; ls-remote confirmed)

Result: Partially done. Clamp port and stall split are committed. Group A is not fixed: no PES-faithful rule decoded, and both GF candidates tried made it worse.

## Committed (d47e785)
- `offsiderule.cpp/.hpp`: `RestartPosition(spot, attackSign, setPiece)` ports PES's restart writer FUN_14050e770 (explicit path, VA 0x14050e770, per TouchReach): goal-area push-out (F4 0.7 at 0x14257a194, z 2.6 at 0x1425ab138), clamp to F0/F1 with no margin, free-kick inset 2.0 at 0x142565604. Replaces the mirror fold. Offset constants are typed in from those VAs; ponytail marks them as not yet read from the install.
- `referee.cpp`: PrepareSetPiece passes the restart team's attack direction and set piece; offside spot is the flagged player's position.
- `restartwatch.cpp/.hpp`, `apps/gfsim_run.cpp`: `[idle] N episodes, stalls M`. Stall = episode starting with `IsInPlay() && !IsInSetPiece()`. Bound unchanged.
- `tests/sim/pes_offside_test.cpp`: fold tests replaced by clamp tests; `ATouchlineSpotPastTheLineStaysInsideThePitch` (A2Stall) now expects 32.06 for a free kick.
- Gate: pes_offside_test 22/22 (PES21_ROOT set).
- Mutation (free-kick inset disabled): ATouchlineSpot fails. Reverted.
- Smoke, 2-min seed-7 (tripwire only, per owner ruling): `[determinism] 4da6dc877f52abee`, 20 stoppages, longest 23.1 s (bound 32.5 s), `[idle] 24 episodes, stalls 10`. No 2-min baseline was run, so no comparison is claimed.

## Group A: not fixed. Inputs for A4
Stall shape (20-min baseline `6d0e1e05fa65998d`, 54 episodes, 20 starting in live play): designated man, HasPossession=1, ball at rest 0.4-1.0 m from him, play live, no set piece. He stands, or later drifts into a Special anim (fn 14).
GF numbers and sites:
1. `AI_HasPossession` radius 1.0 m (AIsupport/AIfunctions.cpp:1119-1135). No PES VA. PES writer for the holder: MB-B B2 `FUN_140a72a60` only stores; its value source is open.
2. Touch reach: `GetBestCheatableAnimID` (humanoid.cpp:3059-3393) accepts touches only at touch-point-to-ball distance of about 0.3 m (a few to 0.5 m). Heuristic (radiusFactor, bodyBallDistanceAdvantage >= 1). PES touch acceptance: clip picker `FUN_14081ff70`, no edge test found (TouchReach).
3. elizacontroller.cpp:452 `hasIt` = HasPossession, so the step-onto-ball branch never runs for a man 0.4-1.0 m off.

Tried and reverted:
- Touch-reach gate (kBallReach_m = 0.3, trap and ball control only) plus hasIt aligned to it: 103 episodes, 92 stalls, stoppages 53 to 11. Much worse.
- hasIt-only: 59 episodes, 27 stalls, 73 stoppages, 107149 ticks. Worse than baseline 20 stalls.

Reproduction for A4: `pes_touch_reach_test.cpp` (scratch `<workdrive>/gf-IdleEpisodes-scratch/`, not committed, red). Harness: wait for opening kick-off, park all active players at 40 m, designate after one tick, place designated man 0.5 / 0.9 m from ball at rest. Result: 0.5 m plays within nine ticks; 0.9 m never plays in 108 ticks. The harness is useful, but its designation and kick-off setup need review before it becomes a gate.

## Not done
- Group A count with a new hash: not measured for a fix (no fix kept).
- Constants 0.7/2.6/2.0 read from the install, not typed in (ponytail is in place).
- The clamp's sign(0)=+1 convention is INF: z=0 goal-kick spots go to z=2.6 in the goal area.

Checks for Main: this worktree's `extern/godot-cpp` symlink showed a self-loop in main's checkout for a while; it resolves now.
