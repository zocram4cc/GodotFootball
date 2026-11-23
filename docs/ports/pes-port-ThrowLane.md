# ThrowLane: throw-in taker owns his body, pickup to release (HangFix branch)

## State
- Branch HangFix, pushed: `origin/HangFix` = 39898dc3 (confirmed by ls-remote). Worktree <workdrive>/gf-HangFix clean at HEAD.
- Commits: 261cf17b (HangFix WIP, previous agent's uncommitted work), 134fcf03 (merge of Integrate 8fa69b21; no input file was touched on both sides, so no json_union_merge was needed), 39898dc3 (the ownership change).
- check_pii: PASS (run from the main checkout). gen_translated.py --check: PASS on this tree.

## Change (39898dc3)
- One ownership rule: `TeamAIController::PieceTakerOwnsBody` = pieceWalk && taker && IsInSetPiece && (pieceWarping || pieceCarrying). It covers the taker from pickup to release (docs/re/pes-re-throwin-choreography.md 3.2, 4.2). The AI Movement and the MOVESTEP FSM do not reach him.
- `PieceTakerWarping` (old definition) keeps the clip-end hold for carry and lift warps only, so the loop's end still switches to the queued throw.
- Deleted: `ReleasePassPlaying` (MOVESTEP gate and humanoid selection special case), and the pickup-only MOVESTEP gate.
- When no throw is queued, an owned taker holds his last clip frame and the queue is asked again next tick (no RED ALERT reset).

## Verified (this tree)
- PesHang.SeedElevenTwoMinutePesLoopMatchReachesFullTime: GREEN (818 s). Was stopped at frame 5448, longest stoppage 1758 ticks.
- PesThrowInRelease.TheThrowInTakersAnklesStayBehindTheLine: GREEN, alone and in the suite. Was 415 of 629 taking ticks short, worst 2.03 m.
- PesThrowInRelease, 9 tests:
  - Green: ThePlayStateIsSetPlayReadyUntilTheReleaseTouch, TheMatchIsBuiltWithTheInstallsPlayerDatabase, ACarriedThrowUsesTheShortRowUpToThePesDistanceAndTheMidRowBeyond, ACarriedThrowInIsReleased, TheThrowInTakersAnklesStayBehindTheLine, EveryManHasHisDemoInfoRef (6 in one run), ACarriedThrowInIsHeldAtItsRowsKeyOffsetUntilTheTouch, TheLiftTurnsACarriedThrowInTakerToFaceThePitch (run separately).
  - RED: ACeremonyThrowInIsReleasedFromTheHandOverTheSpot. Zero ceremony (placed-taker) throw-ins in all six seeds (14, 25, 3, 26, 20, 24), 5034 s. Assertion: `ceremony > 0`.
- PesStall.ALiveBallNeverLiesStillLongEnoughForTheBreaker: RED by its own assertion `liveFirings == 0`. liveFirings = 2229 ticks in 160 episodes. Ratchet constant not touched (Main sets it against Integrate's number, which I did not measure).
- PesStallQuick.ALiveBallNeverLiesStillInTheQuickWindow: RED, forced_live = 53 over 12960 ticks.
- Not run: the full PesStall census beyond ALiveBall/Quick; no mutation runs.

## Open
1. ACeremony red. An A/B was started (Integrate's four files checked out, binary built, files restored to HEAD before the run, run cancelled at wrap-up). It was not completed, so it is unknown whether Integrate is 9/9 green on this test. If Integrate is green, the regression is in this lineage. Next step: run `PesThrowInRelease.ACeremony*` on Integrate's binary (about 85 min, one sim slot). If it fails there too, the brief's 9/9 claim is wrong for this test.
2. ALiveBall ratchet: Main compares 2229 (and quick 53) against Integrate's measured number before setting the constant.
3. Test runs need more than 4000 s in one simslot slot; run the suite split by test (the `timeout` inside simslot is not clamped; the Bash job deadline is).

## Scratch
- Kept: <workdrive>/gf-HangFix-scratch/*.log (run logs). Removed: the Integrate-binary copy.
- Build dir <workdrive>/gf-HangFix/build is regenerable; left in place.
