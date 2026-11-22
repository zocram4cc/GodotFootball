# pes-port-StallTrap2: the race's time-to-ball input is GF's stand-in, and it is wrong for a standing man 1.9 m from the ball (1980 ms); PES's counter-B writer is not decoded; nothing landed, probes reverted (StallTrap, 21-11-26)

Status: REVERTED, NOTHING LANDED, NOTHING PUSHED. Branch `StallTrap` local at `646cc259` (StallRun a2439354 + FastStall 88ae7714). Tree clean. Probe patches kept at `gf-StallTrap-scratch/race-probe.patch` and `race-probe2.patch` (not for landing). Baseline unchanged: PesStallQuick 58 forced ticks. Not run: full ALiveBall, PesHang, PesPassTarget, gen --check (nothing to land).

## 1. Mapping the race's inputs (CONFIRMED, census5.log)

- The race test (`FUN_14079FBC0`) calls the arrival `FUN_140A4D340` at three sites (`170`, `310`, `390`). Each call passes the same `g1` (constant across a race) and a per-site `g2`. `g2` at site 310 is the presser's own block +0x888 (slot 8 block), so the arrival compares the presser's ball point with a record base that is not a roster block (`g1` lies beyond the 22 blocks: a team-side record, not a man). The mapping of `g1` to a side table was not finished (it needs `MaVa(side)` printed).
- Counter B (`SR+0x1438`) equals GF's `timeNeededToGetToBall_ms` converted to frames (×54/1000): man 8 counterB 10 (= 200 ms), man 10 counterB 106 (= 1980 ms). GF's stand-in feeds the race (`team.cpp` RunPesPassTarget writes it; ponytail there says it stands in for `FUN_14092ED30`).
- Arrival x0 values at the refusal: site 170 = 3.54, site 310 = 1.51, site 390 = 1.51 (units not decoded).

## 2. The stale/wrong time-to-ball (CONFIRMED mechanism, INF for the PES comparison)

Man 10 stands 1.88 m from a resting ball and is given 1980 ms. Mechanism, from `AIfunctions.cpp` `AI_GetTimeNeededForDistance_ms` (lines ~589-720):
- A standing man's reach radius grows only by `adaptedMaxVelocity * bias * dt`, where `adaptedMaxVelocity = 0.94 * GetMaxVelocity()`. The loop breaks when the target distance is under that radius.
- `GetMaxVelocity()` (`playerbase.cpp:103`) = sprintVelocity × velocity multiplier × fatigue factor × `ReserveTopSpeedFactor` (dash reserve) × slip factor. A drained reserve or fatigue lowers it, so the radius grows slowly and the search runs to the cap `ms` (the search returns the first ms where `timeNeeded <= ms`, so the value tracks the search cap, here ~1980).
- Man 8 at 1.23 m gets 200 ms, so his max velocity is high; man 10's is low. Which factor (reserve, fatigue or slip) drives man 10's value is **not measured** (INF).

This is GF's own estimate, not PES's: PES's counter B comes from `FUN_14092ED30` (`pes-re-player-record.md` 2.1, counter B 21..500 frames captured). A 1.9 m, 1980 ms answer (106 frames) is in PES's range only at the top, so it is suspicious but not proven wrong for PES.

## 3. Killed or ruled out

- K1. Self inside the NearestList: killed (`pesscorerows.cpp:201` skips `j == i`).
- K2. Reach gate refuses these men: killed (out 1 absent for men 8/10 in the window).
- K3. Admission refuses: killed (`admit140798A70=0`).
- K4. A faster mate wins the race on time to ball: killed for the counted mates (mate 8 is fastest in GF's own tt, yet refused).

## 4. Open (what settles it, in order)

1. Name `g1`: print `MaVa(0)`, `MaVa(1)` and the team-side records next to the arrival probe; then read which field of the record `g1` addresses at `+0x22E4C + 200*id` (the NearestList row) is the teammate candidate. This tells whether the race reads a teammate row or the presser's own arrival.
2. Measure `GetMaxVelocity()` parts for men 8 and 10 at the firing tick (sprintVelocity, fatigue factor, `ReserveTopSpeedFactor`, slip). If man 10's reserve is drained, the stand-in is wrong for a standing man, and PES's arrival (not this sim) decides.
3. Decode PES's counter-B writer `FUN_14092ED30` (store site 0x14092F204, `pes-re-player-record.md` 2.1) to see whether PES's counter B uses the reserve/fatigue factors at all. This is the port that replaces the stand-in (`team.cpp` ponytail). It is a decode round first (Main's OK needed, per the 21-11 rule: diagnosis-only rounds need Main's OK).
4. Red-first test once 3 is decoded: a resting ball at ≤1.5 m, a standing man with full reserve gets a counter B from PES's formula and wins the race.

## 5. Why nothing landed

No PES writer for counter B is decoded, so any change to `timeNeededToGetToBall_ms` or to the sim's `AI_GetTimeNeededForDistance_ms` would be a guessed stand-in, and changing the shared function moves every AI decision. Land-or-revert: reverted. The next round needs the decode (step 3) before a fix.

## 6. Process

- The `sed` on `playercontroller.cpp` from the previous round was reverted, and this round used the edit tool only (including includes).
- `extern/godot-cpp` symlink intact; `build/` deleted.
