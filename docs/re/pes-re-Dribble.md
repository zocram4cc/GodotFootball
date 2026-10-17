# Dribble status (current): branch `refs/heads/Dribble` = `1a3ea8e`

- `1a3ea8e` is the dribble-heading commit (same content as `5a91d88`) rebased onto `6c77c00`. Full gate on that tree: 694 tests, 100% passed, only the two standing install skips. Pushed (force-with-lease over my own `5a91d88`).
- origin/main is now `1ff9c3b` (Main). Rebase before the next commit; the gate above is not valid for `1ff9c3b`.
- Base seed 7 on `6c77c00` (export, fresh build): passes 56-47, forced 11305, determinism 95515192750cd51b. Identical to the pre-PoseVeto base, so the humanoid change did not move seed 7.
- Wired seed 7 on `1a3ea8e`: NOT re-measured. The run died when its shell job ended (no summary). The last wired numbers (on `5a91d88`, pre-rebase base) were passes 50-48, forced 8493, determinism d106da357dba863b. The content is identical; re-running needs the owner's go, since gfsim_run is one-at-a-time fleet-wide.
- Items 1-4 of the decode plan (image state, list, busy, prediction) are not ported: the owner put new gate ports on hold until the foundation inventory lands. Decode notes are in `pes-re-Dribble-inventory.md`.

# Dribble: PES dribble direction wired (handover item 7)

Commit `5a91d88` on `refs/heads/Dribble` (on `2f081e3`). Not merged. The
attacking output drops on both seeds, so the acceptance "smoke not degraded"
is not met. Do not merge to main until the open item 1 below is explained.

## Done

- **Heading = `pes::DribbleDirection`** (c95b525's wiring, re-applied). It is
  PES's bearing from +z: the step is `FUN_140a8f7c0` (decompile:
  `x += r·sin θ`, `z += r·cos θ`), so desired is 90 for attack +x and 270 for
  −x; output `(sin θ, cos θ)` on GF (x, y). The earlier port used cos on x: a
  self-consistent frame 90° off PES's raw, now literal.
- **R_o from the install.** `opponentReach_m` = 10 (`0x142588210`, the 10 of
  `R_o = 10/f`, f = 1 at image state 0) and `keeperReach_m` = 5 (`0x14259222c`,
  `FUN_14212e110`'s keeper branch). Loaded with `PeImage::ReadVa`; checked
  against the PE image (10.0, 5.0). `pes::DribbleOpponentReach`.
- **Speed = GF's force-field pace**, unchanged: `AI_GetBestDribbleMovement` is
  now `AI_GetDribbleVelocity`, the same function minus its direction output.
  PES's dribble speed is not decoded (`pes-re-locomotion.md` O3; carrier-ai 5.4
  gives direction and DRIBBLE mode only).
- Stale comment in `playingstyles.hpp` updated. `veloExpFactor` (dead before
  this change) left alone. `dribble_centermagnet` is still read (force field).

## Verification (the committed tree; diff +85/−23, 7 files)

- Carrier unit suite 28/28, incl. install relations.
- Mutations: literal 5/10 for R_o fails the reach test (9/4 synthetic). Swapping
  the step's axes (cos on x) fails 3 tests: touchline 1 vs 0.45; lone man
  along +x 45° vs 90°; mirror +z 45° vs 90°.
- Target tests PASS: `PesChallengeCounters.SlidesAreCountedPerCommitmentNotPerContact`
  (114 s), `SimMatch.SameSeedPlaysTheSameMatch` (281 s),
  `SimMatch.FullMatchRunsToTheWhistle` (210 s). The c95b525 breakage was the
  throw-in stall, already fixed in `f69aa74`; no root-cause repair was needed.
- Full gate: 693 tests, 100% passed, the two standing install skips.
- PII check clean. Base binary reproduced the handover's seed-7 numbers exactly.

## Smoke vs base (20 min)

| | seed 7 base | seed 7 final | seed 11 base | seed 11 final |
|---|---|---|---|---|
| score (HOM-AWA) | 2-5 | 3-2 | 7-6 | 2-1 |
| passes | 56-47 | 50-48 | 55-52 | 69-79 |
| forced stalemate | 11305 | 8493 (−25%) | 17945 | 8991 (−50%) |
| stall diagnostic lines | 70 | 57 | 97 | 58 |
| shots | 11-15 | 12-9 | 24-18 | 13-9 |
| on target | 3-5 | 2-2 | 8-4 | 2-1 |
| xG | 0.16-0.66 | 0.22-0.12 | 0.74-0.54 | 0.62-0.22 |
| goals (sum) | 7 | 5 | 13 | 3 |
| chance runs | 564-464 | 297-466 | 413-779 | 405-604 |
| determinism | 95515192750cd51b | d106da357dba863b | b45bbb184434eba1 | ed6e227a1d924c31 |

Files: `<workdrive>/gf-Dribble-scratch/smoke-*.txt`, `final-*.txt`, `gate-final.txt`.

## Not done / open

1. **Attacking output drops on both seeds** (shots −19%/−48%, xG −59%/−34%,
   goals down). Cause not established. Refuted: R_o from image state 3 (`10/2.5`
   = 4 m; live dribbles run at states 1–3, carrier-units §1.2). Its smoke:
   seed 7 forced 13436 and shots 16-20; seed 11 forced 15593, shots 5-10, xG
   0.50. Not kept. Hypothesis (INF): the search's goal pull is weak (`0.6·W`
   per step vs threat ~1 per opponent in reach), so the carrier faces off the
   goal and the shot gate (`ShootSubKind` `aimVsFacing`) fails.
   **Next:** log `ShootSubKind` rejections by gate (facing angle, band) for
   seed 11, base vs wired, and check the facing share before changing any term.
2. **Image state is not tracked in GF.** `PassForward` uses state 0; W = 1.0 is
   the states 2–4 value; R_o uses state 0 (f = 1). W and R_o should come from
   one state, so this needs an owner decision on a GF stand-in.
3. Busy opponents (R_o 1.0 at `0x14256ae1c`) are not told apart; needs the
   decode of `0x140a6e3c0`.
4. Opponent `pred = pos`; the list is the 5 nearest, not 0x14212e110's
   distance-sorted list stopping at R_o + 10 m.
5. Docs not edited (docs/re is archive-only). These still say the wiring is
   reverted and must be updated after the decision: `docs/re/pes-re-carrier-port.md`
   §3 Dribble row, `docs/re/pes-ai-map.md` carrier row,
   `docs/re/pes-re-carrier-units.md` §1 "GF replaces", `tasks/14-10-26.md` item 7.
