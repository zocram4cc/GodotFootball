# CutsceneAnchor1: the bcwriter predicate selects the keeper (wrong slot); corrected and verified on captures. Kind-2 feed: writer fed, else arm reaches a further untranslated callback; the kind-2 test stays RED

Branch `CutsceneAnchor`, pushed head `b4d35fe0` (`git ls-remote origin CutsceneAnchor` = b4d35fe0), on `6bb86f6b` (red test) and `0aebc8cd` (main). Labels: CONFIRMED = run or read this round; INF = inference; OPEN = named blocker.

## 1. Predicate verdict (CONFIRMED): wrong, the scorer slot. Corrected predicate verified on the captures

The predicate in `watch_current.sh` (`bcwriter_4B88B0=...`) gets the scorer slot from MI+0x1590 only when MI+0x1588 == 1. In every capture MI+0x1588 == 0, and then the predicate uses slot 0: the keeper's man record. The scorer comes from the squad table (`G+0x1978+side*0x28`, word 1+i = key of index i; slot = side*11 + i where key == MI+0x15CC).

| capture (pid2024691, goal 1) | MI+0x1588 | side 0x15C8 | place 0x15CC | scorer slot | slot 19 (x, z) | slot 0 keeper (x, z) |
|---|---|---|---|---|---|---|
| seq003 (writer entry, DC 1) | 0 | 1 | 10 | 19 (index 8) | -41.553, 3.039 | -49.542, -4.040 |
| seq204 (anchor call, DC 200) | 0 | 1 | 10 | 19 | -41.550, 29.557 | -48.895, -2.226 |

- The side and place are already set at DC 1. `pes-re-goal-anchor-feed.md` section 2 (MI+0x15C8 = 0; scorer at (-49.54, -4.04)) is wrong: that position is the keeper. The replay result (writer returns 8 on seq003) stands and agrees with the decode for slot 19 at z 3.04.
- Old predicate, evaluated with the watcher's own method (eval, empty builtins, GPRs + b/d/q/f): FALSE on seq204 even with DC forced to 100. Its structure is correct (AST: top-level And, the Z disjunction is an inner Or), so the defect is the slot only. With mode 0 in every goal, it reads the keeper for every scorer, so no decision tick could be captured. This explains the 11 goals without a capture (INF: no bcwriter cap line in the watch log since 07:24).

### Corrected predicate (file `gf-CutsceneAnchor-scratch/pred_new.txt`, generator `gen_pred.py`; 4219 chars)

Slot = `MI+0x1590` if `MI+0x1588 == 1`, else `side*11 + i` over an explicit 11-term sum (no generator expressions or lambdas: rv_watch evals with no nested scopes). Then the zone rule's wide channel on that man (x = +0x55C, z = +0x564): `-45.56 < x < 45.56` and `(z > 26.06 or z < -26.06)`, with `5 <= d(rcx+0xdc) <= 171` and `b(rcx+0x131) == 0`.

Results (`pred_check.py`, CONFIRMED):
- seq003 (DC 1): FALSE (DC below 5; slot 19 narrow).
- seq204 (DC forced 100): TRUE. The real DC there is 200, so FALSE at DC 200 by design.

Limit (OPEN): zone 2 also holds for a man inside the quad 0x140A901E0 in the near-line strip (|x| in [45.56, 52.56], |z| <= 26.06). Not selected. Decode the quad's corners before selecting that strip.

### Is a capture needed?
- Not for the feed: the decision's result is evidenced by the recompute capture seq203 (BC 2 at DC 200) and the anchor seq204 (scorer slot 19 at z 29.56). The feed tests take DC values in 5..161 (below clock x 3 = 162, where the writer's 7-arm is closed).
- For the exact decision tick (DC and z at the flip): one full bcwriter capture at the first TRUE call. Use the corrected predicate with `--hook-max bcwriter_4B88B0=20` (8 on disk). Not run this round.

## 2. The kind-2 feed (landed; test still RED)

`PlaceGoal` with `GoalAnchorFacts::decisionCount` set (CONFIRMED by run):
1. A0+0xDC = the count; translated writer `0x1404B88B0(A0, event)` runs on the arena; A0+0xBC -> `GoalAnchor::bc`. Team counters and presence flags are written at arena setup (the writer reaches `0x140471590 -> 0x140A80EF0 -> 0x140A80D10`, which aborts on an unwritten protected block: found by the run).
2. Arm gate (0x140471AD2): else arm iff `(BC-6) & 0xFFFFFFFD != 0`. Default arm keeps GF's builder path.
3. Else arm: translated recompute `0x140471A80(A0, event, r8 = A0, r9 = zeroed 0x100 object)`; radius = A0+0xC, T = A0+0x30 read after it. The scene list is not read (ponytail).
4. DemoInfo sub-object at A0+0x158 gets the install's vtable `0x14259C9E0` (read in `Create`; slots +0x8 = 0x140419D40, +0x10 = 0x14044E0B0, both translated).
5. Native camera registry (`PesCameraRegistry`, as the camera rig keeps it) is built in the selector arena; its singleton page 0x1437F0000 is mapped. `0x141B55730` (registry) then runs.

Where the else arm stops now (CONFIRMED, gdb backtrace): `0x140471A80 -> 0x1404B6BD0 -> 0x14047AB20 -> 0x1404511E0 -> 0x140451C10 -> 0x141B55730 -> dispatch 0x140440FC0 (untranslated)`. `0x140451C10` writes 0x140440FC0 (and 0x140421F40 in r9) as code-pointer constants into the registry call frame; the closure walker sees no direct CALL, so the function is not in the layer. The translation needs it as a closure member of a proven record (the `closure_file` mechanism in `gen_translated.py`: results entry `0x140471A80` in `results_core.json`, record proved by snapdiff on seq203). Not done: it needs the member's translation, a regeneration (`gen_translated.py --image`), and a re-proof of the record. Further members may follow (0x140421F40, then whatever 0x140440FC0 reaches).

## 3. Tests (`tests/pes_goal_anchor_test.cpp`)

- `GoalAnchorKindTwo.WideChannelScorerTakesTheElseArmAndEqualsPes`: RED, not disabled. Asserts BC 2, radius 0, T (-41.6211, 0, 35.56), pos (-41.5502, 0, 34.0), rotY 0 for DC 5/100/161 (PES's anchor for goal 1, pid2024691 seq204). Before the feed: BC 0, default-arm T. Now: aborts on the untranslated 0x140440FC0 (inside the test process; under ctest each test runs alone).
- `GoalAnchorKindTwo.NarrowChannelScorerKeepsTheDefaultArm`: PASSES (bc 8 at DC 1, slot 19 at z 3.04). Caveat (CONFIRMED by reading the defaults): A0+0xBC is 8 without the writer too, so this test does not discriminate the writer; the wide test is the discriminating one.
- Red-first record: commit `6bb86f6b`.
- Regression: `pes_goal_anchor_test --gtest_filter=-GoalAnchorKindTwo.Wide*` 21/21 PASSED; `pes_fixdemo_select_test` 6/6 PASSED.
- Mutation: not run (the narrow test would pass with the writer removed, so the mutation would show nothing; the wide test cannot run past the else arm yet).

## 4. Gates
- `gen_translated.py --check` and `ledger.py --check`: not run this round (no translated file changed; the gate is Main's).
- Builds: `pes_goal_anchor_test`, `pes_fixdemo_select_test` rc 0 under `/tmp/gf-build.lock` (lock contention delayed builds 5+ min).

## 5. Open items (numbered)
1. Kind-2 else arm: translate 0x140440FC0 (and 0x140421F40 if reached) as closure members of the recompute record; regenerate; re-prove 0x140471A80 with snapdiff on seq203. Then the wide test should run to its assertions.
2. Decision-tick capture with the corrected predicate (section 1). Needed only for the exact DC/z at the flip.
3. Quad 0x140A901E0 in the near-line strip (zone 2 by the quad): predicate and feed do not cover it.
4. DC (A0+0xDC) store: not located (captures show DC 1, 2, 3, 4 across successive writer calls, so it is probably a tick count).
5. Scene list of the else arm (0x1404B6BD0's list) not read; the kind-2 `scenes` is empty (ponytail).
6. Flip for kind 2 (0x140478FF0 on this goal) not captured.
7. Correct `pes-re-goal-anchor-feed.md` section 2 (slot 19, not slot 0; the (-49.54, -4.04) man is the keeper).

## 6. Cleanup
Scratch copies in `<workdrive>/gf-CutsceneAnchor-scratch/cap/` (seq003, seq203, seq204, ~11.5 GB unpacked) to delete when this report is archived. Build dir in the worktree is to be removed by Main with the worktree.
