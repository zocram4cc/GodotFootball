# pes-port-StallLive11: the seed-25 throw-in stall (located, not fixed), the press-assignment question (not answered), the census lines (StallLive, 20-11-26)

Status: PARTIAL. Branch `StallLive`, HEAD **837fe21d** (pushed). Commits this round:
- `12356234` PES stall census: STALLMOVE line (command speed, target, body speed, issued motion). Test-only.
- `837fe21d` PES stall census: STALLPRESSER line for the side's press man. Test-only.
Not fixed: ALiveBall (223, unchanged), the seed-25 throw-in stall, the ankle (4/154). Build dir deleted.

## 1. Item 1, press assignment: NOT answered
- GF's loose writer is `TeamAIController::PesLooseWriterTick` (teamAIcontroller.cpp:2513): `pes::RunPresserPass1` builds candidates, `pes::PressWinner` returns the first candidate inside `PressPickRadius_m(sqrt(score))`, else -1. Its side reference and enrolment are ponytails (side reference FUN_140a71540 not decoded; ball-to-goal bearing stands in).
- The STALLPRESSER line (committed, 837fe21d) gives the census what it needs: the side's press man's distance to the ball, his command's target distance to the ball, command speed, body speed and function. I did not run it in this round (budget); the next round should run `PesStall.ALiveBall*` with GF_STALL_DUMP=1 and classify the 172 non-presser firings by whether the press man exists and is closing (tgtdist to the ball, usetgt).
- No PES capture of the press writer was used; the decode in `docs/re/pes-re-approach.md` §1.1 (candidates d² < 81 m², top < 8 m, the press writer gated on no carrier) is the reference. GF's `PressWinner` requires the winner inside the pick radius, so a press man 8-9 m away is never picked.

## 2. Item 2, seed-25 throw-in stall: FOUND (the stall is a restart that never arms in time)
Measured (gfsim seed 25, 2 min, pes-loop; the instrumentation was removed from source after measuring):
- Frame 700: throw-in armed (prepared 1, started 0). The taker A9 walks to the ball, which lies on the touchline at (14.3, -34.3).
- Frame ~1000: pickup. The taker carries the ball (carrying 1); the carry spot is (15.2, -34.8).
- Frames ~1050–3400: the restart's `prepared` is 0. `prepareTime` jumps from 12,174 ms to 65,229 ms (+53 s); `startTime` 13,904 -> 66,959. `inPlay` 0, `inSetPiece` 0, `foulType` 0, referee function Movement (1). The ball is carried across to (-1.7, 36.1) and (4.2, 41.0), i.e. outside the far touchline.
- Frame ~3550: `prepared` back to 1; the taker reaches the carry spot (-3.05, 34.76), lifts (`lifted` 1). gfsim's restart watch fires at 1,758 ticks (limit 1,757) before the lift can release.
- Ruled out (MEASURED): the card ceremony (foulType 0 in the window; AlterSetPiecePrepareTime needs a yellow/red card); the goal scene hold (`HoldRestartForScene` is only raised by GfMatch in the viewer, never in gfsim); the referee's `ScheduleStop` is not logged in the window.
- Not yet pinned: which writer sets `prepareTime` to 65,229 during 1,050–3,400 with `prepared` 0. Candidates still to log: referee.cpp:509 (`prepareTime = now + 500`), ScheduleStop (referee.cpp:126/130) on a new stop, and the re-arm at referee.cpp:1003 (`AlterSetPiecePrepareTime`'s non-PES branch, not reached in PES). The next step is a `prepareTime` write log at those five sites (one run, ~2 min).
- PES-faithful reading (INF): PES's throw-in does not stop being armed while the taker carries the ball to the spot; a 53-second push inside the carry is GF's (a new stop or re-arm), not PES's. Until the writer is pinned, the fix is not identified.
- The ceremony test (`ACeremonyThrowInIsReleasedFromTheHandOverTheSpot`) searches seeds 14, 25, 3, 26, 20, 24 and hangs on seed 25 at this stall. Committed earlier (a0a290a8): a per-match tick cap of 100,000, so the test fails instead of hanging. The test does not finish green until this stall is fixed.

## 3. Item 3, ankle (4/154): not started.

## 4. Action-dispatch table (Main's question, for the record)
- The kick-tick candidate 0x140789D00 is reached through the table slot 0x1425E1908 (slot 0 of a run of per-action pointers; neighbours 0x140787690 and 0x140787C00). The builder is FUN_1406DF630 (stores header: the table at P+0xC40); its caller chain is 0x1406E0540 <- 0x1406E03D0 <- 0x1408BE740. Per Main's latest finding, 0x1406E0540 is a setup-time function, so the per-action call site is elsewhere: find the indirect calls that load from [reg+0xC40] (or [reg+0xC40+idx*8]) and call through them. Not done this round.

## 5. Open items
1. Log the `prepareTime` writers during the seed-25 gap (section 2), then fix the writer that is not PES's.
2. Press assignment: run the census with STALLPRESSER and classify the 172 (section 1).
3. The per-action table call site (section 4): a hook on the indirect call with the action id as the predicate.
4. Ankle 4/154.
