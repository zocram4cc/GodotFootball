# Dodge eligibility gates

Closes the two gaps `pes-re-dodge-port.md` §5 left open (ball eligibility, net
geometry) and adds the human dodge's feature gate. Ported by the DodgeGates
pass; merged on main by the orchestrator, because the pass ended before
committing.

## What is ported

Each gate is a pure function in `src/sim/onthepitch/pesdodge.{hpp,cpp}` with
install-read constants and tests in `tests/pes_dodge_test.cpp` (31 tests),
wired into the matching predicate in `elizacontroller.cpp`.

|Gate|PES site|Tests|Mutations caught|
|---|---|---|---|
|Ball eligibility|`FUN_140a07bf0`: role-table class bits (`0x1426484f0`, read through `FUN_140a6e470`'s `LEA`), designated-owner shortcut, ball-holder half test, team latch `+0x255`, action slot `0x18`, last-action player, `(gs*5)/48`-frame window. Sub-tests `FUN_140a07570` (ball step, reach, 10°/90° bearing windows, time-to-ball) and `FUN_140a07880` (goal-mouth wedge, kick classes 8–10)|15|8/8|
|Net gate|`FUN_140a07e40`: `\|x\| ≥ halfW − 1` edge test, anime slot, `100/(gs·72)` step floor, post / behind-post bearing interval, 15° residual rejection|5|7/7|
|Human gate|`FUN_140a0b350`: feature / match-state, roster, half match, `gs·0.2`-frame window, team latch, roster-slot ball-owner test|4|6/6|

## Stand-ins (GF lacks the PES input)

- `AnimeClassOf`: GF's 15 `e_FunctionType`s mapped onto PES's 94 anime
  classes. The class table is the install's; the mapping is GF's.
- `designatedOwner` ← `GetDesignatedPossessionPlayer()` for PES's
  `+0x18->0x2600c`. Loosest term in the ball gate: GF recomputes it to the
  closest man.
- `perTeamActionAllowed` ← `IsInPlay()` for the team latch `+0x255`.
- `actionSlot` ← the player's own slot (GF runs one command per tick, so PES's
  `0x18` "none" never arises).
- `framesSinceEvent` ← `(simTime − lastTouchTime)/10`.
- `teamGateOpen` ← `!IsInPlay()`; `ballOwnerIsSelf` ← `GetBallRetainer() ==
  self`; `rosterSlotValue` ← 0 (GF has no per-team roster-slot array). The
  human gate is therefore effectively its frame window alone.
- Net gate bearings from GF's own-goal marks (`ownGoalX = side·pitchHalfW`,
  `goalHalfWidth`), PES's z read as GF's y.

## Measured (seed 7, 20 min, main)

|State|played dodge|`[dodge]` net|forced|passes|
|---|---|---|---|---|
|before the gates|1241-1174|1104-1522|21934|49-51|
|gates, GF veto on (merged)|1162-1129|9-9|11305|56-47|
|gates, GF veto off (not merged)|1753-1456|7-9|12093|39-35|

Before the gates, lifting the veto gave forced 57847 and passes 21-27; with
them the stall count holds. The veto stays on because passes fall to 39-35 on
this seed; the `ponytail:` note next to `pesClip` in `Humanoid::SelectAnim`
carries the numbers.

## Open

1. **Lift the GF pose veto for PES clips** (`pesClip` in `SelectAnim`): measure
   over several seeds; the stall count is no longer the blocker, passing is.
2. **The net gate nearly silences the net dodge** (~2,300 → 9 per side). The
   gate is faithful; check whether GF's `goalHalfWidth` (3.7) matches PES's
   `pitch[2]` and whether the predicate's own `|x − ownGoalX| ≤ 12` window is
   PES's.
3. The human gate's stand-ins never fire; only the `gs·0.2` window acts.
4. The human dodge kind (`FUN_14073a3d0`) still reads geometry only, not PES's
   per-part contact record.
