# Offside awareness for GF's attackers (OffsideAwarenessPort)

## 1. RE (brief)

Tools: the OffsideAwarenessPort scratch dir. `dec.sh` for the decompiles (PESLive project, read-only),
`/tmp/pesmem.bin` for the `.rdata`/`.data` floats, `pdis.py` for the disassembly that resolved one ambiguous
argument. All addresses are PES2021.exe VAs.

### 1.1 `FUN_14045e0d0` (0x14045e0d0) — the per-team ball list

Builds each team's arrival list (`MA+0x24840`, 11 × `{id, frames}`) over its own 11 slots. Per slot, if the
per-side enable `*(G+0x2c0)+0x6e+team` holds, it walks the four flag words
`MA+0x253c4 + 0x10·team + 4·category` (`(0x94f1 + 4c)·4` for team 0, `0x94f5·4` for team 1) testing bit
`id & 0x1f`; if the bit is set in a category `iVar20 < 2` it jumps to `LAB_14045e914` with
`fVar58 = fVar53` = **1000000.0** @0x14259d640, the impossible score the loop otherwise seeds with 3600.0
@0x142598590.

**Rule (OBS): categories 0 and 1 keep a player off his team's ball list — he does not chase. Categories 2 and 3
keep him on it.**

The same list is the team's presser (defensive-shape §12), the involvement walker (duels-fouls §5.2 step 6),
the key-player fallback, the keeper's decision (goalkeeper §7) and the camera target.

### 1.2 `FUN_14050f580` (0x14050f580) — the team's ball candidate (key-player selector)

Returns the team's key player `team+0x78` unless he is flagged in a category `iVar5 < 3` **and** `param_3 == 0`;
otherwise walks `MA+0x2470c + team·0x58` (11 × `{id, dist}`) and returns the first entry that is active, not flagged
in 0..2, has `team+0x4bd4[slot] == 0`, is in a ball-seeking AI state (`player+0x5d0` ∈
{3, 4, 5, 0x12, 0x14, 0x16, 0x18, 0x34, 0x3e}) and is not falling/sliding (`human+1` not 0x11/0x57).

**Rule (OBS): the key player is dropped when flagged in 0..2 (`c < 3`); the list fallback the same.** Used by
the attack-chance test `FUN_14050f7e0` → the Judge's advantage (duels-fouls §3.1). **Not ported**: GF has no
key player, so only the ball list's branch has a counterpart here.

### 1.3 `FUN_142117c30` (0x142117c30) — the receiver enumeration's offside status 6

```
LAB_14211818f:
  fVar30 = s * table[+0x34]                         -- the carrier's offside line
  if  fVar30 < s * (0*vel + pos)   -> status 6      -- his position now
   or fVar30 < s * (T16*vel + pos) -> status 6      -- his position 16 frames ahead
```
with `T16 = table[+0x30] = int(fps · 0.30 @0x142590d98)` = **16** frames, and
`table[+0x34]` the running **maximum**, from 0, of the attack-signed x (predicted 16 frames ahead) of the first
**five** entries of `FUN_140a714e0(MA, s)` (the opponents' depth-sorted table), then of the ball (also 16 frames
ahead).

**Rule (OBS): a team-mate is an offside receiver when his current OR near-future position is beyond the deeper of
the five last defenders (predicted 16 frames ahead) and the ball (predicted the same).** This is the carrier's own
line, not the referee's `MA+0x44`: no second-last-defender rule, no goal-line clamp, and a halfway floor (the
running maximum starts at 0). It does **not** read the flag bits — the exclusion is geometric, not flag-based.

### 1.4 No run rule reads offside (and none exists to port)

`FUN_140688c00` (common stop test), `FUN_140607a60` (Chance stop), `FUN_140612830` (LineBreak stop),
`FUN_140620900` (PostPlay stop) and `FUN_14059ba60` (the Chance target search behind `spaceRun.o`'s score) were
all decompiled and read **neither** the flag words **nor** the referee's line `MA+0x44+0x14·t`. The only
longitudinal stop input is the **ball** line: `FUN_140a4c720(…, −1.8 @0x1425cdbe0)` for LineBreak and
`FUN_140a4c720(runner.x, MA+0x2482c, team+0x254, −2.5 @0x1425c74d0)` for PostPlay's lane 0 (the disassembly at
0x140620a7f resolves the second operand to `MA+0x2482c`, the ball x — the off-ball report's "offside-line test
−2.5" label was the ball line all along).

**⇒ PES's run selectors hold no run because of offside: PES runs players offside and the referee's flag stops the
play.** So no run rule is decoded, and GF's own run/line handling is not in conflict with anything decoded.
`spaceRun.o`'s `checkOffsideLineDistX 12` key still has no identified reader (off-ball O3).

### 1.5 The root cause of the 23 whistles

PES's `s` is `team+0x254`, the team's **attack** direction. GF's `Team::GetSide()` points at the team's **own
goal** (the reads `elizacontroller.cpp`'s `const int attackDir = -team->GetSide();` and `referee.cpp`'s
`const float attackDirSign = -tripee->GetTeam()->GetSide();` both take). The ported offside line and attacker test
were passed `GetSide()` directly, which is the negation of PES's `s`: the line landed on the ball and the test
flagged every attacker behind the ball at his team's touch. Mutation-checked — 23 whistles in a 5-minute match
(10 for one side, 13 for the other) against 0 with the negation in place.

## 2. What GF changed

| GF change | PES rule / VA |
|---|---|
| `AI_GetOffsideLine` passes `-GetSide()` to `OffsideLineX` | the defending team's attack direction; offside §3 |
| `Referee::BallTouched` judges with `-team->GetSide()` | the attacking team's attack direction; offside §4 |
| `Referee::GetOffsideCategory(Player*)` (new) | exposes the per-player category 0..2 (−1 unflagged) |
| `OffsideRule::OffsideBlocksChase(category)` (new, pure) | FUN_14045e0d0's `c < 2` ball list |
| `Team::GetBestPossessionPlayer` skips a category 0/1 player (with a "nobody left → closest goes anyway" fallback) | FUN_14045e0d0's ball list |
| `pes::CarrierOffsideLine` and `pes::ReceiverStatusOf` (new, pure; they replace `OffsideRule::ReceiverIsOffside` and `AI_ReceiverIsOffside`, deleted) | FUN_142117c30's whole enumeration: statuses 1, 2, 4, 5, 6, 7–10, 0x0B |
| the carrier's pass-candidate loop takes only the mates whose status is 0x0B; the GK skip and the ball-to-target cut are gone | carrier-ai §4.3 |
| `matchreport.cpp`: `[balance-offside] offsides N-N \| free kicks N-N` | telemetry so a smoke run can read the rate |

`elizacontroller.cpp`'s own offside heuristics (`GetSupportPosition`'s `offsideRating`,
`GetSupportPosition_ForceField`'s `forceNoOffside` clamp, `GetPoacherTargetX`'s 0.5 m cushion) were already
consistent with PES's line and were left alone — with the frame fixed they now clamp against the right line.

### ponytail: comments in code

- `pescarrier.cpp` `CarrierOffsideLine`: no per-limb points — a centre stands in for every defender's rear
  surface (the same approximation `OffsideLineX` already makes).
- `team.cpp` `GetBestPossessionPlayer`: PES lists category 2 as a chaser too; PES's list would read 0xff with
  everybody blocked, so with nobody left to send the closest man goes anyway.
- `elizacontroller.cpp`: the carrier's offside line is read over centre positions and is not clamped to the goal
  line, exactly as PES's running maximum is.
- `referee.cpp`'s pre-existing ponytails (the 500 ms whistle hold, the shot-only DelayWhistle) are unchanged.

## 3. Verification

- `pes_offside_test`: **19/19**. Mutation-checked: widening `OffsideBlocksChase` to `c < 99` red-dens the chase
  test; reading only the rearmest defender instead of the five last red-dens five receiver tests.
- `sim_match_test`'s `OffsideRateStaysInPessBand`: the frame read pinned end to end. With the negation reverted the
  same 5-minute match whistles **23** (10 + 13); with it in place the band holds.
- 5-minute fixture, seed 1000, with everything in: **0 offsides, 0 free kicks from offside** (the match was drawn
  0-0 and went to penalties, which the fixture does when nobody scores — the AI readers keep both sides onside).
- Gate: run after the final rebuild; see the commit message for the state at commit time.

## 4. What is open

1. PES's ball-event type table (offside §9 item 1) — `TouchResetsPhase` keeps GF's own deliberate-play split.
2. `FUN_14050f580`'s key-player branch and the receivers' record B ("into space") are not ported.
3. `spaceRun.o`'s `checkOffsideLineDistX 12` has no identified reader.
4. Category 3's "near-level onside" is detected but, as before the port, not stored — PES never reads it for
   anything the AI does, so nothing was lost.
