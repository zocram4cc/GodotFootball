# PES throw-in touchline re-award (item 1) and the two older restart-delay tests (item 2): RestartRef report and handover

> **Main note 22-10-26:** landed on main in batch B (`94b8f66`, gate GREEN 1708/1708; 20-min smokes off `464f398685b80578`, pes-loop `6a0fc698e4fd6fbe`). Its five open items are carried in tasks/22-10-26.md.

## 0. Status at handover

- Branch `RestartRef`: `origin/RestartRef` = `869a43ac303cf8175e7d96894df908ffb9a3c48f` (`git ls-remote`). Commits on base `d85b017`: `de2a9a5` (test reframe) and `869a43a` (zone gate).
- HEAD `869a43a` compiles: `cmake --build build --target pes_restart_delay_test`, rc 0 (libgfsim and the test relinked).
- DodgeGate merged it into DodgeGate2 as `9eaad1e` (ort, no conflicts). `869a43a` is an ancestor of `origin/DodgeGate2` (checked).
- Worktree clean: no tracked or untracked changes in `src`, `tests`, `docs`. No job of mine is running. Nothing unproven is left in the tree, so there is no patch to hand over.
- I stopped after the handoff to DodgeGate and Main. No step is half-done.
- Item 1 is not closed. The zone gate removes the same-spot re-award seen in the trace. Three hazards remain (sections 5.1, 5.2, 5.4).
- Next concrete action: a red test for the judge's missing out-of-play bit (section 5.1), then the bit check in `referee.cpp`.

## 1. Corrections to my earlier report and to commit 869a43a (read first)

1. Trace source. The re-award trace (section 2.2) was taken on `8e8a925`. `git diff --stat 8e8a925 0c1f7da -- src` is empty; only `tests/sim/pes_restart_delay_test.cpp` differs. So the one-stall red on `0c1f7da` ran the same sim code and seed. That the stall is the same release is INF-high. I did not re-trace `0c1f7da`.
2. Terms. "Release" was used for two events. Take start = `started` (the `ReleasesRestart` block in `referee.cpp`). Touch = the set piece end, which is when the ball leaves the hand. The trace puts take start 25 ticks before the touch (DodgeGate measured about 27). The `869a43a` message and the `matchevents.cpp` comment use "release" for both.
3. Reference point. The `869a43a` message says the throw was "awarded again two ticks later". That is two ticks after the touch. It was 27 ticks after take start.
4. Wrong argument. `869a43a`'s message and the `matchevents.cpp` comment cite `pes-re-throwin-choreography.md` 4.2 for "the ball is always released from behind the line" and for "a ball already over the line at the release raises nothing". 4.2 does contain the first phrase, but its own OBS numbers (5.3) put PES's ball inside the line at the first in-play frame (z = 33.87 and 33.81, touchline 34.06). So the phrase is not a reliable position claim, and the commit should not have used it. The second sentence is my inference; 4.2 does not say it. The gate's direction rests on `pes-re-out-side.md` §7 item 3, which the doc lists under "Not established". The message cites that as "7.3"; it is §7 item 3. The `matchevents.cpp` comment also lacks the `// ponytail:` note the brief requires: `buffer.started` is a stand-in for PES's `p+0x298` writer. Both go in the next commit (sections 5.1 and 5.5). The pushed messages are not amended (no force-push).
5. Repeats. My earlier statement "I did not reproduce them on any base" was wrong. On `8e8a925` the 2-minute trace shows a same-spot re-award (section 2.2). On `d85b017` the 5-minute repro shows no touchline repeats, with or without the change (section 4).
6. Outside releases on `d85b017`. "No outside release occurs in seed 7 on `d85b017`" was not measured; release positions were not logged there. What is measured: the 2-minute `AReleasedThrowInCarriesItsBallInsideTheTouchline` passes on `d85b017` without the gate change.
7. Other restarts. "The change leaves other restarts unchanged" is INF and untested. For spots inside the line the zone code is the same before and after, but any event raised during a take is now routed differently.

## 2. Item 1: the touchline re-award at the throw

### 2.1 Mechanism

- [CONFIRMED, GF] The zone event K=10 is raised on any change of the zone code (`matchevents.cpp`, `ProcessPesEventStage`, PES-loop block; classifier `OutOfPlayCode` in `pesevents.cpp`).
- [CONFIRMED, GF] The touchline judge in `referee.cpp` (`overSideline`) fires when `PesOutOfPlayEvent()` is true and `|x| <= F0` and `|y| > F1` on the raw ball.
- [doc] PES: zone writer `FUN_140872C10`, classifier `FUN_140A72E70`, K=10 `FUN_140A17810` (`pes-re-out-side.md` §2–5). Judge case 10 `FUN_14050E4A0`, gate `116C == 1 and event.flags & 2` (`pes-re-A5Judge.md` §1.4).
- [CONFIRMED, GF, before the fix] The code is forced to 0 while `buffer.active`. The touch sets `buffer.active = false`. So the code changes after the touch, and the judge reads that event.

### 2.2 The re-award [CONFIRMED, trace on `8e8a925`, seed 7, 2-minute pes-loop match; scratch `trace/run2.log`, ticks 3320–3347]

- Take start at tick 3320, ball at (−0.99, 35.13).
- Touch at tick 3345, ball at (−0.50, 34.47), taker root y 34.68.
- Zone code changes out at touch+1 (tick 3346): the ball is pulled in by one radius to 34.36, past F1 = 34.06.
- The judge fires at touch+2 (tick 3347). The raw ball is past F1 at that tick: y 34.14 in the tick's print and 34.36 in the previous one (the judge's read and the print can differ by one tick). The throw is awarded again at x ≈ 0.3 on the same touchline.

### 2.3 The fix (`869a43a`)

- Zone code = `buffer.active && !buffer.started ? 0 : OutOfPlayCode(...)`. The code is held at 0 only while the restart is armed. It opens at take start.
- [INF from the trace's positions and the code; the green run in 2.4 is consistent with it] On this release the code is out from take start +1 (the ball is at the spot, outside). It stays out through the touch, so there is no change at touch+1. The event from take start +1 is consumed while `IsInSetPiece()` keeps the judge closed, so nothing fires.
- Once the judge is open after the touch, the code flips to in when the pulled-in point crosses F1. That flip is the hazard in section 5.1.

### 2.4 Evidence

- `0c1f7da`, without the gate change: `AReleasedThrowInCarriesItsBallInsideTheTouchline` FAILED, 1 stall (scratch `base2/red_focus.log`).
- `0c1f7da`, with the gate change only: the same test OK, 0 stalls (scratch `fixC/green_focus.log`). The reframe was not yet applied in that run, so the 2a test failed there as expected.
- `d85b017`, clean vs with both commits: the test OK in both (scratch `base3/focus.log`, `fix4/focus.log`).

## 3. Item 2: the two older reds

### 2a. `AThrowInTakerPlaysABallActionWhenTheBallLiesOverTheLine`: premise, reframed (`de2a9a5`)

- The failing check was `onLocomotion > 0`: the taker is in a PES locomotion clip at release (a Special clip whose `specialvar1` is in the locomotion family).
- [CONFIRMED, GF] The family is `kLocomotion` in `pesclips.cpp`: prefixes `step_`, `goto_run_`, `run_`, `walk_`, `side_`, `back_`, `backslant_`, `near_`, and others.
- [doc] PES names the seamless carry and release motions `throwin_*` (`pes-re-throwin-choreography.md` §3.2, rows 0x9be and 0x9bf; §4.2). None matches a locomotion prefix. On this path the carry is a MOVE clip (movestep trace). So the premise has no PES basis; it described an older carry.
- Change: the counter, its branch and its check are removed, with the reason in the comment. Kept: `untaken == 0` (a ball action within the restart bound) and `throwIns > 0`.
- Before: FAILED, `Expected: (onLocomotion) > (0), actual: 0 vs 0`, on `0c1f7da` and on `d85b017`. The other checks passed. After: PASSED on `d85b017` + `de2a9a5`.

### 2b. `AThrowInIsNotAwardedAgainOnItsOwnSpot`: green on every tree I ran; no change of mine

- `0c1f7da`: OK (scratch `base2/red_focus.log`). `d85b017` clean: OK (`base3/focus.log`). `d85b017` + both commits: OK (`fix4/focus.log`).
- It is not an item this change needed.

## 4. Verification

Gtest XML and logs. `base3` = `d85b017` clean. `fix4` = `d85b017` + the two commits' code (the later edits are comments only).

- `pes_restart_delay_test`: 26/27 → 27/27. The only change: `AThrowInTakerPlaysABallActionWhenTheBallLiesOverTheLine` FAILED → PASSED.
- `pes_movestep_wire_test`: 27/27 → 27/27.
- `pes_setpiece_stand_test`: 8/9 → 8/9. `SetKickStand.ThePlacedKeeperKeepsHisHeadingWhileHeWaits` fails on both: "the placed keeper turned 50.50 degrees while he waited" (tolerance 5). Pre-existing on `d85b017`; not this change.
- Repro, seed 7, 5 minutes, `--pes-loop`, Release `gfsim_run`, identical before and after: `Full match complete: HOM 2 - 1 AWA`; `34447 sim ticks to full time (cap 194400)`; `22 stoppages, longest 23.2 s (1251 ticks), bound 32.5 s (1757 ticks)`; `determinism 86b864c44a733cc1`; `idle 30 episodes, stalls 3`.
- Restart records (temporary logger, not committed): 23 records, including the opening kick-off (6 kind 1 = kick-off, 4 kind 2 = goal kick, 9 kind 3 = free kick, 1 kind 4 = corner, 3 kind 5 = throw-in; kind numbers per the `e_SetPiece` enum in `src/sim/gametypes.hpp`). The record lists are byte-identical with and without the gate. The three throw-ins are at (−1.25, 34.31), (−47.43, −34.31) and (−9.67, −34.31): distinct spots, no repeats.
- Mutations, each built and run: gate off → release test red (1 stall, `0c1f7da`); gate on → green (0 stalls); premise restored → 2a red (`d85b017`); premise reframed → green (`d85b017`).

## 5. Open items: where I stopped and the next action

### 5.1 The judge takes any zone event; PES also requires the event's bit 2 [GF side, mine; next]

- [CONFIRMED, GF] `referee.cpp` `overSideline` and `endLineOut` test `PesOutOfPlayEvent()` only. The event is raised on any change of the code, in or out. The code is the record's first payload word (`pesevents.cpp`, `DetectOutOfPlay`: `out->Write(0x0a, …, in->code, 0)`).
- [doc] PES tests `116C == 1 and event.flags & 2` (`pes-re-A5Judge.md` §1.4). Which bit of the K=10 flags word that is: OPEN.
- [INF, untested] Hazard: a code that flips back to in-play while the raw ball is still past F1 fires the judge. It can fire when the inward speed is below K × 54 ≈ 5.9 m/s (K = 0.1087 m). The trace's throw moves about 0.22 m per tick (about 12 m/s), so it does not hit this. A slower ball could. This does not depend on the gate: on `d85b017` a ball resting in the band at the touch also fires (0 → in change).
- Next action: a red test in `tests/pes_outside_test.cpp` (or a match test in `pes_restart_delay_test.cpp`). An out → in flip with the raw ball in (F1, F1+K), and the judge open one tick later, must not stop play, because PES's bit 2 is clear. Expect RED on GF. Then decode the K=10 flags word (writer `FUN_140A17810`) to find PES's bit, and gate the judge on it. In the same commit, fix the citation and add the `// ponytail:` note (section 1.4).

### 5.2 A refused touch now leaves no event [UNTESTED; possible regression]

- The old gate turned a refused touch with the ball outside into a re-award (a code change after the set piece ends). The new gate makes no change, so no event follows. Then nothing restarts the throw while the ball lies out in play.
- [INF] Refusals should be rare for a carrying taker: `humanoid.cpp` `atOwnSpot` (about line 3224) exempts his touch while he carries the ball near his spot. Coverage not measured.
- Next action: build the case in a test (touch refused, ball out) and assert that a restart follows. If none does, the judge needs a raw-ball check at the set-piece end. PES's behaviour there is not decoded (OPEN).

### 5.3 In-play flag rises at take start; PES rises at the release event [DodgeGate's files; owner not assigned]

- [CONFIRMED, GF] The `ReleasesRestart` block in `referee.cpp` calls `StartPlay()` and `StartSetPiece()` at take start, 25 ticks before the touch (DodgeGate measured about 27).
- [doc] PES raises 116C 3 → 1 at the release event: `FUN_1406EC260` → `FUN_140445750` (`pes-re-throwin-choreography.md` §4.2, phase 1). The clip's release row is in §4.2.
- [INF] Moving it changes which stop or play branch each player runs during the take (`elizacontroller.cpp`, the `!IsInPlay()` branch near line 150). Not tested. The judge stays closed during the take (`IsInSetPiece`), so this does not decide the re-award.
- Next action: confirm the release-event call chain from the doc's VAs (ThrowinNormal `0x140792E10`, phase 1), then move `StartPlay` to the touch under a red test, checking the restart watch and the branch effects.

### 5.4 The release clip [DodgeGate's files; owner not assigned]

- [CONFIRMED, GF] The throw is a `ShortPass` command (`elizacontroller.cpp`, throw-in branch). The clip is a generic pass-family clip (function type 6 in the trace, touch frame 23). `kThrowInKeyMotion = 0xcfa` sets only the stand length (`teamAIcontroller.cpp`). No PES `throwin_release_*` row is played.
- [doc] PES plays the release rows 0x9b8–0x9bc (release frames 18, 27, 99 and 36 at 54 Hz; `pes-re-throwin-choreography.md` §4.2). At the first in-play frame its ball is 0.19–0.25 m inside the line (§5.3: z = 33.87 and 33.81).
- [CONFIRMED, GF trace] At the touch GF's ball is at y 34.47, 0.41 m outside the line and 0.21 m inside a taker root at 34.68. This is the release-geometry cause of the outside releases. The gate change covers only the judge side.
- Next action: port the release-row choice (distance ≤ 13 short, ≤ 25 mid, otherwise long; variants from the carry; §4.2) into `humanoid` and `movestep`, with a red test that the ball is inside the line at the touch.

### 5.5 Exact frame the zone gate opens [INF]

- Take start is GF's stand-in for PES's gate. `pes-re-out-side.md` §7 item 3 lists the meaning of `p+0x298` as "Not established"; `pes-re-A5Judge.md` §9.4 puts the armed exit on the taker's kick action, frame not decoded.
- Next action: find the writer of `p+0x298`. The doc does not name it; start from the zone writer `FUN_140872C10` and its caller `FUN_140425A90`, or read `p+0x298` live against 116C at a throw release. Until then the stand-in carries a `// ponytail:` note naming that site.

### 5.6 Pre-existing, not mine

- `SetKickStand.ThePlacedKeeperKeepsHisHeadingWhileHeWaits` fails on `d85b017` (50.50° vs 5°). Owner of `pes_setpiece_stand_test` to decide.

## 6. Commits and checks at handover

- `de2a9a5` test(sim): the throw-in taker test asserts a ball action, not a locomotion clip
- `869a43a` sim(judge): the touchline zone gate opens at the take, not at the set piece's end (its message: see section 1.4)
- HEAD `869a43a` compiles, rc 0.
- Not yet in any commit, for the next one: the citation fix and the `// ponytail:` note (sections 1.4 and 5.5).

## 7. Evidence files (scratch directory `gf-RestartRef-scratch`)

- `trace/run2.log`: widened trace on `8e8a925`, seed 7, 2-minute match. The re-award is at ticks 3320–3347.
- `base2/red_focus.log`: `0c1f7da` red (one stall; 2a FAILED).
- `fixC/green_focus.log`: `0c1f7da` with the gate only (release test OK).
- `base3/focus.log`, `fix4/focus.log`: `d85b017` clean vs with both commits, four tests.
- `base3/*.xml`, `fix4/*.xml`: full suite results (gtest XML).
- `base3/repro5.log`, `fix4/repro5.log`: the 5-minute repro, identical.
- `logs/base_inst.log`, `logs/fix_inst.log`: restart records, identical.
- `logs/head_build.log`: HEAD compile, rc 0.
- `my-changes-on-0c1f7da.patch`: the gate and reframe patch (both committed; kept for reference).
