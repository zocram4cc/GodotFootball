# CutsceneAnchor2: the recompute's else arm is not a closure gap; the wide-channel kind-2 test stays RED on PES's DemoInfo object graph. The zone is now tested against PES's writer (zones 4, 5), with a mutation.

Branch `CutsceneAnchor`, pushed head `8dc367dc` (`git ls-remote origin CutsceneAnchor` = 8dc367dc). Previous head `b4d35fe0` (round 1 feed) on `6bb86f6b`. Labels: CONFIRMED = run or read this round; INF = inference; OPEN = named blocker.

## 1. The closure premise does not hold (CONFIRMED)

Main's round-2 premise: 0x140440FC0 and 0x140421F40 are indirect targets stored by 0x140451C10, to add to the recompute's closure. Checked against PES's own replay of seq203 (the proven record's snapshot, `snapdiff` `addrs.json` and `trace.json`, gen dir of `snap2`):
- 0x140451C10 is NOT executed by PES on that call (addrs.json: False). 0x140440FC0 is not executed (False). 0x140421F40 is not executed either (False, though it appears as a vtable slot of a table PES reads).
- trace.json (the indirect sites PES entered) has no site in 0x141B55730, 0x140451C10 or 0x1404511E0 targets.

So the snapdiff reached set does not contain them, and adding them to the closure would add code no replay runs: that breaks the rule "the layer holds what the proof ran". I did not add closure members. Recorded as a finding for Main.

The real divergence is GF's arena: the DemoInfo sub-object at A0+0x158 (inline object). PES holds, at the recorded entry of seq203 (CONFIRMED by dump):
- +0x00 vtable 0x14259C9E0; +0x08 byte = 1; +0x10 pointer 0x10E1EB90 (node N1); +0x18 0; +0x20 0; +0x28 pointer 0x1425A1FA8 (a table of function pointers: [0] 0x140470E70, [1] 0x140421F40); +0x30 1; +0x38 pointer 0x2DD81E00 (object H, vtable 0x1425A1F08).
- GF had +0x08 = 0 (my round-1 arena wrote only the vtable). With 0 the list builder 0x1404511E0 takes its vtable arm and reaches 0x140451C10. With 1, it takes the other arm.

## 2. Fix landed: the flag (CONFIRMED by run)

`PlaceGoal` writes A0+0x158+8 = 1 (`kSubFlagPes`, PES's value at the recorded entry). The else arm no longer reaches 0x140451C10. It now reaches `0x140445750` (translated, closure) and stops there: `PES_DISPATCH_UNTRANSLATED: 0x2400006550`. That is a dispatch on a pointer read from the unbuilt object graph: 0x140445750 reads [sub+0x10] (N1, not built in GF: zero) and [sub+0x20] (zero in GF, nonzero in PES).

## 3. Blocker for the wide-channel test (OPEN, named)

The else arm needs PES's DemoInfo sub-object graph: N1 (vtable 0x14259C940, with its own N2 at +0x10), the table at +0x28, and H at +0x38. GF builds none of them. They are built by the DemoInfo's constructor, which is not located. Next step: find the writers of the vtable 0x14259C9E0 (the constructor stores it at A0+0x158): `curl -s "http://127.0.0.1:8080/xrefs_to?address=0x14259C9E0"` (bridge up at round start), then decompile the constructor, translate it into the closure (or model its stores), and feed the nodes. Not started this round (it is a new object-building feed, not a closure addition).

`GoalAnchorKindTwo.WideChannelScorerTakesTheElseArmAndEqualsPes` stays RED (not disabled, not gated). Expected values: BC 2, radius 0, T (-41.6211, 0, 35.56), pos (-41.5502, 0, 34.0), rotY 0 for DC 5/100/161.

## 4. The zone is now tested against PES's writer (CONFIRMED)

Found a clean seam: `FixDemoSelector::ZoneDecision(facts, dc)` runs the writer 0x1404B88B0 on the arena that `PlaceGoal` arranges (`Arrange`, split out of PlaceGoal; no behaviour change for PlaceGoal). It returns A0+0xBC and does not go through the else arm, so the zone test is independent of the blocker above.

PES's values (oracle, CONFIRMED): replaying the writer on the captured seq003 entry (DC 1, the scorer's man = slot 19 = squad index 8) with only the scorer's man position poked (scratch tool `poke_bc.py`, which wrote into the scratch copy's mem.bin at the captured page's file offset and restored it):
- (-41.5, 30) -> 2; (20, 30) -> 2; (-41.5, -30) -> 2 (wide channel, as the decode says);
- (60, 0) -> 4; (-60, 0) -> 4; (0, 0) -> 5;
- (-49.5, -4) and the captured (-41.553, 3.039) -> 8.

Tests (`tests/pes_goal_anchor_test.cpp`):
- `GoalAnchorKindTwo.NarrowChannelScorerIsZoneEightAsTheDefaultIs`: PASSES, asserts BC 8 at the captured DC-1 position. Main asked for a discriminating NarrowChannel. It cannot be one: zone 8 IS the default (0x140478090 writes 8), so a writer-less arm gives the same answer. Kept as a regression of the zone-8 path, and named as non-discriminating.
- `GoalAnchorKindTwo.PastTheLineAndCentreScorersAreNotZoneEight`: PASSES (BC 4, 4, 5 at the three spots, default 8). This is the discriminating case.

Mutation (CONFIRMED, run, reverted): the writer call in `ZoneDecision` commented out. The past-line test fails: `8 != 4` at (60, 0), `8 != 4` at (-60, 0), `8 != 5` at (0, 0). Reverted; the test passes again.

## 5. Gates and checks

- `gen_translated.py --image PES --check`: rc 0, "5695 functions (3759 recorded bit-exact, 1936 closure) regenerate byte-identical". The translated layer is unchanged this round.
- `tools/dev/ledger.py --check`: rc 0, "LEDGER.md current (44459 rows)", 59 off-path ports (all allow-listed).
- Re-proof of the recompute on seq203 (CONFIRMED): `snapdiff.py --only 0x140471A80` on the current tree, snapshot = a scratch copy of pid2024691/seq203, trace = gf-CutsceneLine-scratch/recompute/trace203 (the round-4 trace). Result: calls 1, replayed 1, mismatches 0, own-block 0.3913, **proven 1 of 1**. Results file: `gf-CutsceneAnchor-scratch/snap2/results.json`. The recorded entry in `results_core.json` is unchanged (its proof stands: no translated text changed).
- Test binaries, after the split and the mutation revert (the binary was rebuilt from the committed source, `8dc367dc`): `pes_goal_anchor_test --gtest_filter='GoalAnchorKindTwo.NarrowChannel*:GoalAnchorKindTwo.PastTheLine*:GoalAnchor.*'` 9/9 PASSED; `--gtest_filter='GoalGate.*:FoulGate.*:GoalScenes.*:GoalAnchor*:-GoalAnchorKindTwo.WideChannel*'` 22/22 PASSED; `pes_fixdemo_select_test` 6/6 PASSED. The wide test is excluded from the filters because it aborts the process (PES_DISPATCH_UNTRANSLATED).
- Not run: the full gate and the fast gate (Main gates). The round-1 round also ran 21 goal-anchor tests before the split.

## 6. Open items (numbered)
1. DemoInfo sub-object graph (section 3): find the constructor via `xrefs_to 0x14259C9E0`; translate it; feed N1, N2, the table at +0x28 and H. Then `WideChannelScorerTakesTheElseArmAndEqualsPes` runs to its assertions.
2. `PlaceGoal` doc comment now sits above `Arrange` (the split left it there): move it back above `PlaceGoal` (cosmetic, next round).
3. The else arm's scene list (the list 0x1404B6BD0 writes) and the kind-2 flip (0x140478FF0) are not read (as round 1).
4. Quad 0x140A901E0 in the near-line strip: not in the predicate or the feed (as round 1).
5. A0+0xDC store (the writer's counter): not located (as round 1).
6. The corrected watcher predicate is armed (Main). No new capture this round.

## 7. Cleanup
- Deleted `gf-CutsceneAnchor-scratch/cap/` (seq003, seq203, seq204 unpacked copies, ~11.5 GB). The poke oracle (`poke_bc.py`) now has no snapshot to run on; its results are the values in section 4, and the captures it used are still in `live2/pid2024691/`.
- Kept `gf-CutsceneAnchor-scratch/snap2/results.json` (small); removed `snap2/gen` (compiled units).
- Build dir `build/` in the worktree is still there: remove it with the worktree.
