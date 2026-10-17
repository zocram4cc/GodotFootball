# A6Whistle: post-gate judge and stoppage LIMIT writer (behind pes_loop)

Branch `A6Whistle` (worktree `<workdrive>/gf-A6Whistle`, not merged). Commits: red `5075ddb`, green `6312385`, comments and rename `6fd6ab3`. `ls-remote` confirms `6fd6ab3`.

## What changed (pes_loop on only; off path unchanged by construction)
- `pesclock.{hpp,cpp}`: `TimeUpWhistle` (FUN_14050B9B0 post-gate: 60 s count R+0xd0 > 54*60, zone count R+0xd8 > 54*4 signed by the side in possession, touch timeouts under holder flag 4). Blowing clears both counts (FUN_14050C3E0). `PesClock::AccrueStoppageLimit` (FUN_14044CE80 step 4: LIMIT += 0.5/frame while stopped in a clock period, clamp 21600 from `.rdata 0x142647C60`). `AnnounceAddedTime` now adds (`+=`), identical in the off path because LIMIT is 0 there.
- `referee.{hpp,cpp}`: the gate blows only when `!IsPesLoop() || PesTimeUpBlows(ballPos)`. Carrier's team gives the side (FUN_140A627E0 arg, asm 0x14050BB4F-0x14050BB76: index >= 11 is side 1, none is side 0; GF uses `GetBallRetainer()`, INF).
- `match.{hpp,cpp}`: `IsPesLoop()`; stoppage accrual called only under pes_loop.

## Corrections to pes-re-A6Writers.md §3 (decompile of 0x14050B9B0)
- Line 227 is a literal `0x1e` (30 frames), not `rate*30`.
- The kind-4 check is `rate*10 < R+0xd0` inside the flag-4 block; kind 2 is `rate*5 < R+0xd0` (also flag-4).
- The final block returns `rate*30 < R+0xd4` (30 s count); the report omitted it.
- The judge runs in periods 0,1,3,4,5,7,8 and in dead ball (`FUN_14050C3E0` gates on period only, skips 2,6,9,10).

## Tests (red -> green)
- Red at `5075ddb` with stub bodies: `pes_clock_test` 10 judge/LIMIT tests FAILED on assertions (e.g. 60 s blows at tick 1 instead of 3241 expected). `pes_period_end_test`: `PesLoopTheRefereeWaitsSixtySecondsPastTheGate` FAILED (IsInPlay false at tick 1), `PesLoopAStoppageAddsToLimit` FAILED (LIMIT 0 vs 2700).
- Added: `PesTimeUpJudge.{TheSixtySecondCountBlowsOnTheTickAfterSixtyGameSeconds, ABlowClearsTheCountsSoTheNextBlowNeedsAFullCountAgain, TheBallZoneBlowsAfterFourGameSecondsInTheZone, TheBallZoneDoesNotCountWhileTheBallIsInTheAttackingThird, AZoneCountRunningTheOtherWayNeverBlowsOnTheZone, AChangeOfSideInPossessionClearsTheBallZoneCount, AHolderFlaggedThirdKindBlowsAtOnce, AHolderFlaggedFourthKindBlowsAfterTenGameSeconds, AHolderFlaggedSecondKindBlowsAfterFiveGameSeconds, WithoutTheHolderFlagTheTouchTimeoutsDoNotBlow}`, `PesClock.{TheStoppageLimitTakesHalfAUnitPerFrameAndCapsAt21600, TheStoppageLimitMovesOnlyWhileStoppedInAClockPeriod}`, `PesTimeUp.AnAnnouncementAddsToTheStoppageAlreadyOnLimit`, `PesPeriodLifecycle.{PesLoopTheRefereeWaitsSixtySecondsPastTheGate, PesLoopAStoppageAddsToLimit, WithoutPesLoopAStoppageLeavesLimitAlone}`; `PesClockInstall` asserts `limitCap == 21600`.

## Mutations (each applied, rebuilt, run, reverted; tree matched the commit after each)
| # | Mutation | Result |
|---|---|---|
| m1 | 60 s count `>=` | caught: TheSixtySecondCount..., ABlowClears..., WithoutTheHolderFlag... |
| m2 | no zone sign-change reset | caught: AChangeOfSide... |
| m3 | zone threshold `>=` | caught: TheBallZoneBlows..., AChangeOfSide... |
| m4 | zone test inverted | caught (6 tests incl. 60 s and zone) |
| m5 | holder flag ignored | caught: WithoutTheHolderFlag... |
| m6 | blow does not clear counts | caught: ABlowClears... |
| m7 | kind-4 timeout at 5 s | caught: AHolderFlaggedFourthKind... |
| m8 | zone count `+= 1` (ignores direction) | NOT caught: EQUIVALENT. A reversed count resets on the next tick, so it never passes +4 s either way; same whistle output. |
| m9 | LIMIT accrues in play | caught: TheStoppageLimitMovesOnlyWhileStopped... |
| m10 | LIMIT cap removed | caught: TheStoppageLimitTakesHalfAUnit... |
| m11 | announcement overwrites LIMIT | caught: AnAnnouncementAddsTo... |
| m12 | gate ignores pes_loop switch | caught: TheRefereeWhistlesWhenTheGateOpens (off path) |
| m13 | accrual call dropped | caught: PesLoopAStoppageAddsToLimit |

## Verification
- Built `pes_clock_test pes_period_end_test pes_clock_match_test pes_event_wiring_test` (`nice -n 19 cmake --build build -j3`). One pre-existing warning in `humanoid.cpp:2864` (not mine). `ninja -t deps` shows no `#deps 0`.
- Run with `PES21_ROOT=<workdrive>/PES21`: pes_clock_test 33/33, pes_period_end_test 6/6, pes_clock_match_test 2/2, pes_event_wiring_test 4/4.
- Gate not run (Main gates). Smoke not run: brief-rules §Verification split. Main runs the off and on smoke.
- Off-path argument: gate short-circuits to the old behaviour when pes_loop is off; accrual is pes_loop-only; announcement `+=` equals `=` at 0; the new VA is read in the constants load (present in this install).

## Ponytails (OPEN, named in code)
1. Event timeouts cannot fire yet. The holder flag (payload+0x20, `G+0x2914`) has no GF writer, so it is always 0. Ported exactly, unreachable until a writer lands.
2. Not ported: kind-0 ball-state branch (FUN_14050B9B0 lines 140-232), kind-2 x test on payload+8 (lines 239-240), zone 0x255 sub-branch (lines 88-119), penalty-taker hold (lines 52-66), R+8 flag (line 67), final block with R+0xd4 (lines 253-297).
3. Dead ball: GF calls the judge in play only, so the 60 s count skips dead-ball ticks. PES counts them (see call-site ponytail).
4. `AnnounceAddedTime` still uses GF's tier approximation (FUN_140A6BA90 codes not ported). Tier minutes can exceed 21600 units; the next stoppage tick then clamps them down, PES's clamp behaviour.
5. Field half length uses `pesFieldHalfLength` (gametypes F0), not the install's cfg[0] read at runtime. Ball x uses GF `coords[0]` as PES x (INF).

## Open items for Main
- Run the off smoke (`--minutes 2`, pes_loop off) and compare with main `96ce33d3b51d4199` behaviour expectations; run on smoke with `--pes-loop` (tripwire only).
- Items 1-3 above decide whether the whistle timing matches PES in real matches.
