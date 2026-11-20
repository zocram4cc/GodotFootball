# pes-port-StallLive12: the press-man census (172 classified), the seed-25 throw-in stall (located; a fix that broke PesHang was reverted), acceptance (StallLive, 20-11-26)

Status: PARTIAL. Branch `StallLive`, HEAD **08296496** (pushed). Goal not reached: `PesStall.ALiveBall` still red at 223. Build dir deleted.

Commits this round:
- `aa181e63` PES throw-in: the ThrowinLoop ends at the release (a fix for seed 25). Reverted in `08296496`: it broke PesHang seed 11 (stall at 1,758 ticks, stopped at frame 23,732).
- `08296496` Revert the ThrowinLoop release gate (src back to the pre-fix text; PesHang green again).
- Earlier this round's test diagnostics were committed in 12356234 (STALLMOVE) and 837fe21d (STALLPRESSER); the STALLPRESSER line now reads the MOVESTEP decision command (not the stale clip command) — UNCOMMITTED in tests/sim/pes_stall_test.cpp; commit it with the census numbers.

## 1. The 172 non-presser firings, by the press man's state (MEASURED, census with decision commands; 223 firings, 51 presser-is-forced, 172 not)
Source: `STALLPRESSER` (press man's distance to the resting ball, his MOVESTEP decision command: usetgt/tgtdist = target distance to the ball, cmdv, fn). The clip's originatingCommand was stale and was not used.

| Press man's state | firings | reading |
|---|---|---|
| commanded to the ball (usetgt 1, target on the ball), 2–5 m away, clip fn 14 (Special locomotion) | 98 | approaching, command 3.8–6.7 m/s, body only 0–2.1 m/s |
| commanded to the ball, >= 5 m away, fn 14 | 39 | approaching slowly; body 0.0–2.5 m/s against cmdv 2.2–6.1 |
| commanded to the ball, 1–2 m, fn 14 | 13 | close, not arriving |
| commanded to the ball, 2–5 m, fn 13 (Sliding) | 11 | |
| not commanded to the ball (usetgt 0), 2–5 m, fn 13 or fn 1 | 25 | no ball target; the press command is not applied |
| <1 m, usetgt 0, fn 5 / fn 8 | 36 | at the ball, standing (the 35 LongPass wind-up cases; command not applied) |
| <1 m commanded to ball | 1 | |

Reading (CONFIRMED by the census lines, INF for the mechanism): 161 of 172 have a press man who IS commanded to the ball but whose body does not close: body speed is 0–2.5 m/s against a commanded 2–7 m/s (the MOVESTEP speed gap of round 8's G2, now at the press man). So the press assignment is right in most cases; the loose ball stays loose because the presser's body does not reach it in 5 s. The 25 usetgt-0 cases are a separate gap (the press command not applied for a presser at 2–5 m).
- Probable PES-faithful cause (INF): GF's `Issue` stands in the blend group's nearest member for PES's SetParametric (pesmovestep.cpp, "ponytail: GF stands the group's nearest member in ... Removed by a SetParametric port"). The issued motion's own speed is then not the command's rate, which fits the body at 0–2 m/s under cmdv 3.75–6.7. Not verified: the issued motion number of these men was not logged (the STALLPRESSER line has no motion field).
- Not done: the press-selection port (item 1's "translate PES's selector"): the writer (`PesLooseWriterTick`/`RunPresserPass1`) already selects a presser in these cases; the failing part is the approach speed, so the selector is not the first thing to port.

## 2. Seed 25 throw-in stall: located, fix reverted (regressed PesHang)
- The stall (gfsim seed 25, 2 min): the restart is a throw-in (A9); the taker carries the ball, is lifted, and releases (`started` 1). The taker stays in place, `carrying 1 retainer 1 touchAnim 0`. The set piece never ends, the stoppage runs past 1,757 ticks, gfsim stops.
- Cause (MEASURED, one fix verified): elizacontroller.cpp's ThrowinLoop branch (PesLoop, restart ThrowIn, HoldsRoot, carrying) stands the taker still even after the release. Gating the branch on `!started` (aa181e63) makes seed 25 reach full time (35,025 ticks; determinism f2bede24e50292b2) and passes PesThrowInRelease ACeremony (896 s).
- The same gate breaks PesHang seed 11 (stall at 1,758 ticks at frame 23,732). PesHang is in the acceptance set, so the gate was reverted (08296496). With the revert: seed 25 stalls again; PesHang green (774.98 s).
- Open: the seed-11 and seed-25 throws differ in what follows the release (the retainer's ShortPass throw in seed 11 must not be cut off). Next step: log the throw command and touch of seed 11 after the release with the gate on, then keep the gate only for the case that stalls.
- The ceremony test now passes when seed 25 is not in its search (it fails at the tick cap, not by hanging) — the test is not green on the reverted tree.

## 3. Writer of the seed-25 restart gap (CONFIRMED, pinned, not the cause)
- Logged writes to `prepareTime` (referee.cpp, env-gated, removed): the only writer in the gap is `ScheduleStop` for new out-of-play and foul stops (stop 9574 kind 5, 33796 kind 1 foul by H10 on the taker A9 at ball y 22.9 while inPlay=1, 51462 kind 5, 62629 kind 5). `foulType` is not a card; the card ceremony and the goal-scene hold are not involved.
- The restart gap is therefore a sequence of real restarts (throw, foul free kick, corner, throw), each taken at a 7–12 s gap, not a single push. The 1,758-tick watch counts them as one stoppage (`IsInSetPiece` stays on until the taker's touch completes), which is why the watch fires at the chain's end.

## 4. Acceptance on 08296496 (final tree)
| Check | Result |
|---|---|
| PesStall.ALiveBall | FAIL: 223 (unchanged) |
| PesStall.TheWorldHolderFieldIsThePesHolderEveryTick | PASS |
| PesPassTarget.EveryPassInFlightHasItsOwnSidesReceiver | PASS |
| PesHang (seed 11) | PASS (774.98 s) |
| PesThrowInRelease 9/9 | NOT RUN on 08296496 (last run, with the gate on: 8/9; the ankle test 1 of 156 short, 0.0092 m; ACeremony PASS 896 s). With the gate reverted the ceremony test does not finish green. |
| Kick-clear mutation | not re-run this round (last run round 9: pass-target red) |
| gen --check | GREEN, 10,150 functions (on the same src/ except the reverted change) |
| ledger --check | GREEN, 47,642 rows |
| Seed-7 smoke | 29,597 ticks, determinism d5b32ac91262de67 |

## 5. Open items
1. Seed-25 / PesHang conflict in the ThrowinLoop release (section 2): log the throw and touch after the release for seeds 11 and 25, keep the gate only where it is faithful.
2. ALiveBall: the press men are commanded to the ball but do not close (section 1): the issued motion's speed vs the command (SetParametric stand-in) is the next thing to measure (log the issued motion number and the blend member's speed).
3. The 25 usetgt-0 pressers at 2–5 m: why the press command is not applied (elizacontroller's loose-presser block).
4. Ankle 1/156 (0.0092 m) with the gate, 4/154 without it: still red.
