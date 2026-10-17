# Offside port — PES 2021 detection into GameplayFootball

Implements `docs/re/pes-re-offside.md` (detection, closed by OffsideRE) and the
call path of `docs/re/pes-re-duels-fouls.md` 5.1-5.2. Worktree: a detached
worktree of GodotFootball. All constants are read from the user's own
PES2021.exe at the report's VAs; the pure rule functions are exercised with a
synthetic constant set, and one install test checks the loader through
relations.

## Changed GF functions

### `pes::JudgeConstants` + `pes::LoadJudgeConstants` (pesjudge.hpp / pesjudge.cpp)
The nine offside constants of the report, read into the existing struct (no new
plumbing):

| field | PES value | VA | role in the port |
|---|---|---|---|
| `offsideMarginTorso_m` | 0.15 | 0x1425c48a0 | the head/torso body-point margin, **and** the tolerance when no arm point leads (PES's one xmm8 float, two roles) |
| `offsideMarginArm_m` | 0.06 | 0x1425cc238 | an elbow/wrist point's margin |
| `offsideMarginToe_m` | 0.10 | 0x142588200 | a toe point's margin |
| `offsideArmTolerance_m` | 0.30 | 0x142590d98 | the tolerance when an arm point leads |
| `offsideRearShiftTorso_m` | -0.15 | 0x14259d64c | the defender rear point's shift towards his own goal (report 3) |
| `offsideRearShiftArm_m` | -0.06 | 0x14259d644 | as above, arm points |
| `offsideRearShiftToe_m` | -0.10 | 0x14259d648 | as above, toes |
| `offsideNearBand_m` | 1.0 | 0x14256ae1c | category 2 / category 1 boundary |
| `offsideClearBand_m` | 4.0 | 0x1425656c0 | category 1 / category 0 boundary |

The three VAs that PES shares with other handlers reuse the existing named
constants (`OFFSIDE_MARGIN_ARM_VA = SPOT_BIAS_VA`, `OFFSIDE_CLEAR_BAND_VA =
FOUR_VA`).

### `OffsideRule::OffsideLineX` (offsiderule.cpp) — report 3
`L = min(0, second-rearmost defender's rear surface, ball.x)` in the defending
team's own attack frame (`v = s*x`, own goal at `-halfLength`), with the two
running minima starting at 0 (a defender beyond halfway never deepens the
line), the goal line when fewer than two defenders are eligible, and the
goal-line clamp. Returns the world x. Callers pass the **current** ball centre
(`mentalImage->GetBallPrediction(0)`) and `pitchHalfW + lineHalfW` (= PES's
`pitch+0` = 52.56, PES's live goal-line clamp).

### `OffsideRule::OffsideCategory` (offsiderule.cpp) — report 4
`s*P.x + m > s*L + tol`, with `d = |P.x - L|`: category 2 when `d < 1.0`,
category 1 when `d <= 4.0`, category 0 beyond that, category 3 for a
near-level attacker who is not beyond, and -1 ("no bit") otherwise.

### `OffsideRule::CanCreateOffside` (offsiderule.cpp) — report 2.2
The staged-restart set is now PES's: kick-off, throw-in, goal kick, corner
never arm a flag; a free kick or a penalty can. (GF's `BallTouched` gates this
behind `!IsInSetPiece()` anyway, so this only makes the pure helper match the
report's gate.)

### `AI_GetOffsideLine` (AIfunctions.cpp, AIfunctions.hpp) — report 3/6.1
Rebuilt on `OffsideRule::OffsideLineX` over the mental image's player centres
(the defending team's images), filtered to those on the pitch laterally, with
the current ball centre. The `futureSim_ms` extrapolation parameter is gone
(PES's line is not a prediction), and with it GF's 0.01 m halfway offset; the
ball-capped line no longer uses the predicted ball. One call site dropped its
`240` argument (elizacontroller.cpp).

### `Referee::BallTouched` (referee.cpp) — report 2.3/4
At the ball event (never per frame), once `TouchResetsPhase` has cleared the
previous phase and `ShouldSnapshot` has passed the staged-restart test, the
attacking team is judged against the defending team's line, the player on the
ball is skipped (PES skips the event's player), and each player's category is
computed. Only categories 0..2 are stored (category 3 is onside near level and
never punishable), each with the position at the touch and its category.
When the toucher is himself flagged, PES's involvement test (duels-fouls 5.2
step 2) applies and a pending offside is set instead of an immediate whistle;
a pending contact foul blocks it ("every frame with no pending foul").

### `Referee::CommitPendingOffside` (referee.cpp, new) — duels-fouls 5.1
The whistle waits the offside whistle hold (gs/2 frames) from the involvement,
and a category-2 flag additionally waits while the ball player is shooting
(`DelayWhistle`). On commit it whistles PES's 0xb stop, the free kick at the
involvement spot, and the stats + late-card-eligible buffer arm as before. A
goal, a ball out or a staged restart drops it instead (PES's Judge only runs
while the ball is in play).

### `Referee::Process` (referee.cpp)
Calls `CommitPendingOffside` next to `CheckFoul(true)` in the in-play branch
and drops a pending offside as soon as play is off.

## `// ponytail:` approximations and their ceilings

- `offsiderule.cpp` `OffsideLineX`: no per-limb points; arms and toes not
  modelled. The torso shift stands in for every body point's rear radius
  (0.15 / 0.06 / 0.10), so a defender's rear surface is his centre plus the
  torso shift and no deeper. Ceiling: PES takes the rearmost of the root and
  nine body points, so its line can sit up to ~0.15 m (a toe or a swung arm)
  deeper than this engine's. Upgrade path: per-limb points from the body model.
- `offsiderule.cpp` `OffsideCategory`: no per-limb points; arms and toes not
  modelled. The leading point is the centre shifted by the torso margin, so no
  arm ever leads and the 0.30 m arm tolerance cannot apply - the centre itself
  must pass the line, exactly as PES's own head/torso point does. Ceiling:
  PES's per-limb strictness is not reproduced (a toe leading gives 0.05 m, an
  elbow 0.24 m, a hand tip 0.30 m at the centre), so tight calls that PES
  resolves with a limb this engine calls the other way.
- `offsiderule.cpp`: PES's `second > 0 -> 0` clamp is not written as a step -
  both running minima start at 0 and only ever go down, so it is unreachable.
  Pinned by the halfway test.
- `AIfunctions.cpp` `AI_GetOffsideLine`: eligibility is "active" (from the
  mental image) plus "on the pitch laterally" (`|y| <= pitchHalfH`). PES's
  other eligibility is its off-the-pitch action-state byte, which this engine
  does not carry; the lateral test stands in for it.
- `referee.cpp` `kPesOffsideWhistleHold_ms = 500`: PES's offside whistle hold
  is `gs/2` frames, a frame count rather than a .rdata float, so it is carried
  as a named constant derived from gs (54 frames per second, pes-re-foul-
  choreography.md 9.1). It is not one of the nine read at runtime.
- `referee.cpp` `CommitPendingOffside`: `DelayWhistle` is only "the ball player
  is in `e_FunctionType_Shot`". PES also holds the whistle for a ball event
  younger than 5 frames, for an event of type 5, and for its own key-player
  conditions (`FUN_14050d7f0`, AI state 0x19), none of which this engine
  models. Upgrade path: a ball-event frame counter.
- `referee.cpp` `BallTouched`: the involvement test is PES's step 2 only (the
  flagged player made the newest ball event). PES's other branches
  (interfering with the keeper, the near-to-ball races) are not ported, so an
  offside that PES calls on a passive flagged attacker is not called here.

## Verification

- `tests/sim/pes_offside_test.cpp` (new target `pes_offside_test`): 12 tests -
  the line (second-rearmost, ball cap, halfway cap, keeper as a defender,
  fewer than two defenders, goal-line clamp, the mirrored direction), the
  beyond test at the tolerance edge, the four categories and no-bit, the
  no-evaluation-while-staged gate, and the install loader's relations.
- Mutation checks: dropping the leading-point margin fails the beyond tests;
  flipping `d <= clearBand` to `<` fails the category edge; the clipped tests
  also caught a sign error in the rear shift for the defending team attacking
  -x. The halfway clamp was shown unreachable and deleted.
- 20-minute smoke run, seed 7, home 1 away 2, before and after on the same
  seed: offsides called went from 1 (0-1) to 23 (15-8). The score moved from
  3-4 to 2-3 with the extra free kicks. The PES test flags a player level with
  the line, where the old rule needed 0.20 m of slack, and GF's attackers do
  not keep themselves behind the line, so the jump is the rule change, not a
  regression in the detection.
- Gate: after the port 347/347 pass on the port's own base; rebased onto
  e18f443 (the set-piece port and the shared constants helper), 368/368 pass,
  with the same two pre-existing install skips. `tools/check_pii.sh` clean.
- Commit: `40eae92` (rebased; pre-rebase `f6b2076`).
