# A5Merge: A5Tail-e906 x A5Tail2 merge (not landed)

Merge review of the A5Tail stack on DeadBall. Archived from the agent's final message (it wrote no report file).

## State
- Merge `9ca351e` = `--no-ff` of `origin/A5Tail2` (`4faa303`) into DeadBall `e906f25`, pushed as `origin/A5Tail2-confirmed`. Not merged to main.
- Tree clean; the A5Tail bulk `6b50897`, red `6ae2f4c` and fix `ce74a39` are ancestors.

## Finding: the fix is not in the tree
`4faa303` reverts `ce74a39`'s source (`git diff 6b50897 4faa303 -- src/` is empty; Main re-checked). A5Tail-2 found the mental-image assertion passes without the fix (docs/ports/pes-test-A5Tail2.md), so the mental-image theory is dead.

## The red is a merge-interaction stall
`ARestartPlacesTheBallInTheTakersMentalImage` fails on the merged tree at the harness stall checks (`RunMatchWithinTheBound`, 1758 ticks against a 1757 limit, throw-in chain), with or without `ce74a39`; its mental-image assertion passes either way. Deleting the republish call (mutation) changes nothing: the stall masks it.

|Line|pes_restart_delay_test|
|---|---|
|DeadBall `e906f25`|6/7 pass; the one failure is `ALooseDeadBallInPlayIsReachedAndPlayed` (known, loose-ball work open)|
|A5Tail2 `4faa303`|3/8 pass; 5 pre-existing failures (`EveryStoppageRestartsWithinTheInstallBoundWithPesLoop`, `PesLoopRestartsAreArmed...`, `AWhistleLeavesNoFlyingBallBehind`, `AThrowInKeepsItsWriterSpotThroughPrepare`, `ARestartTakerPlaysHisBallActionAfterRelease`)|
|Merged `9ca351e`|9/10 fail: the 5 above, `...AtTwentyMinutes`, `AThrowInIsReleasedFromItsSpotAndTaken`, `ARestartPlacesTheBallInTheTakersMentalImage`, plus DeadBall's known one|

The 1758-tick stall appears only on the merged tree: DeadBall's requeue gates against A5Tail's restart pieces (`Ball::Stop` at the whistle, release on reach, the broad throw-in OOB exemption). Cause not isolated; A5Close lands those pieces one at a time to find it.

## Review
Clean on the A5Tail2 side: no source-text tests, no output-chosen branches, no 4cc/pack pins; literals cited (`.rdata 0x142592228`/`0x14256ae1c`/`0x14259d5d0`, `FUN_14050e770`, `KickoffReceiver 0x140781fc0`, `FUN_14044d2e0`/`FUN_140a43750`).

## Open
1. The merged-tree stall (A5Close).
2. `ALooseDeadBallInPlayIsReachedAndPlayed` (disabled on main; PES first touch `FUN_1404f4be0` undecoded).
3. The full 44-binary sweep of the merged tree was not completed.
