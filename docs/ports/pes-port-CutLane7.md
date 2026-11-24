# CutLane7: 0x1419CB300 is a record-tree writer; nothing landed; the goal group rows match PES, so the 0xDE/0xD9 cause is not the loader

Branch CutLane, worktree <workdrive>/gf-CutLane, base tip **5b608ffb4** (unchanged on origin; nothing committed or pushed this round). Labels: CONFIRMED = read or run this round; INF = inference; OPEN = blocker.

## 0. Rig requests (top)

- **Hook 0x1419CB300** (cut DB slot-2 writer; CutLane6's `cdbw_1419CB300`): FIRST 4, EVERY 1, cap 12, FULL, any call. Capture must show: rcx = the record with vptr 0x142A92F88 at +0; q(rcx+0x8) = the class-data struct A (its fields +0x8/+0x10 and vectors at +0x68/+0x70/+0x78 are read by the helper below); q(rcx+0x10) = owner O; O's child vector begin/end at O+0x18/O+0x20; rdx = the output block.
- **Hook 0x1419B7930** (the writer's first call, the helper that walks the record's children): FIRST 4, EVERY 1, cap 12, FULL, any call. Capture must show the helper's rcx (the same record), the child vector A+0x8..A+0x28 (16-byte entries: child pointer, then a vptr with slots +0x8 and +0x10 called per child), and the vectors at A+0x68/A+0x78 (byte strings).
- Keep the parent hook 0x1423D4A80 from CutLane6's request (FIRST 2 EVERY 1, cap 6).
- Without A (the class-data struct) and the child vptrs, no synthetic state of 0x1419CB300 runs (section 2). The capture is required, not optional.

## 1. Scenario and verdict

- Scenario: **none landed.** No M-scenario is met. Steps 1 and 2 are not done; step 3 is answered (section 4); step 4 is cited, not re-measured (section 5).
- Layer unchanged: 0x1419CB300 and 0x1419C9C60 are still untranslated (the fixed-block model in the stash still aborts at 0x1419CB300 by construction, CutLane6's survey, not re-run).
- Working tree: the stash is re-applied as uncommitted changes (`stash@{0}` = CutLane6 r3, 45 tracked files plus new closure files; patch copy `gf-CutLane-scratch/cutlane6.patch`). The stash itself is untouched. My own uncommitted additions: `tools/dev/diffharness/specs/cutdb/` (8 specs), `tools/dev/diffharness/specgen/sets/closure-cutdb.txt`, the universe rows closure.py wrote, and the hand-edited spec `1419CB300.json`. `gen_translated.py` SETS entry: added then reverted (no results file exists, so it was not kept). `results_cutdb.json`: written by a full driver run, then removed.

## 2. Step 1: translate 0x1419CB300 and 0x1419C9C60 (NOT DONE: static record blocked)

Method: `closure.py --image pes_full.bin --spec-set cutdb --tag cutdb 0x1419CB300 0x1419C9C60` (CONFIRMED: 8 members, specs in `specs/cutdb/`, set file `closure-cutdb.txt`):

| VA | kind | note |
|---|---|---|
| 0x1419CB300 | seed | writer (slot 2 of vtable 0x142A92F88) |
| 0x1419C9C60 | seed | walker's next target (from 0x1423D4D10) |
| 0x1419B7930 | callee | helper: walks the record's children with vtable calls (CONFIRMED from the headless decompile, `gf-CutLane-scratch/dec_cutdb.txt`: `(**(code**)(*plVar6+0x10))(plVar6,plVar9)` and `(**(code**)(*plVar6+8))()` in a loop over A+0x8 entries) |
| 0x1419B7CD0 | callee | returns the output offset |
| 0x1419CB340 | callee | |
| 0x1419C9CA0 | callee | |
| 0x1415A1708 | callee | memcpy-like (restart set) |
| 0x15B050120 | callee | `tail` kind; lies outside the image (file 462,929,920 bytes at base 0x140000000; INF: packer or heap thunk). Not investigated |

Driver (CONFIRMED): full run over `specs/cutdb` gave **0/64 valid cases for all 8 functions** (`class = harness cannot run`, corpus empty). The first failure is `unmodelled read at 0x8 (size 8)`. I hand-completed the root spec (`specs/cutdb/1419CB300.json`: A0+0x8 to region A0.8; A0+0x10 to owner A0.10; A0.10+0x18 to A0.18; A0.10+0x20 to A0.20 = A0.18+0x1000, so 512 children; rdx to A1). It still gives 0/64 with the same error: the helper reads class data through A+0x8, and its vectors hold vtable-dispatched children that the spec cannot model. CONFIRMED that the spec generator's pilot mode saw one level of the record only.

Verdict: no diffed record, so nothing goes into the layer (closure members are emitted only under a diffed root). The writer's bit-exactness is UNPROVEN. The translation needs either (a) a modelled record tree (child objects with real vptrs into the image) for synthetic states, or (b) the real-call captures in section 0. I chose not to ship a translation without a record (M5: coverage would be 0 of the root's blocks).

## 3. Step 2: re-apply the stash and run the sim (NOT DONE)

- Stash re-applied (section 1). Sim not run: the abort at 0x1419CB300 is the known one, and the translation it needs does not exist yet. A run would only re-confirm CutLane6's survey.

## 4. Step 3: GF's cut DB group rows for variant 0x30005 vs seq203 (ANSWERED: the rows agree)

- Variant 0x30005 = category 3 (cut id >> 16 & 0xFF), group index (0x30005 & 0xFFFF) - 1 = **4** (CONFIRMED: `0x1404B6400` case 2, decompile `dec_b6400.txt` line 75).
- **PES**: snapshot `gf-CutsceneLine-scratch/recompute/seq203` (launch pid2024691, after the No-Mod pin of 30-10-26, so stock constants; read-only, no copy made). Read with `gf-CutLane-scratch/pesdb.py`: DB slot 0x1436F4FB8 = 0x228465F0; category 3 base = DB + 0x60; groups at +0x168 (count 362 at +0x178); rows at +0x180, stride 0x78.
- **GF**: diagnostic gtest (`FixDemoDiag.DumpGoalGroups`, scratch only, reverted) decoding `common/demo/fixdemo/goal/table_goal.bin` through the VFS (`PES21_ROOT=<workdrive>/PES21`), i.e. the same table `BuildDb` writes.
- Result, groups 0 to 6 (CONFIRMED, both sides):

| group | firstRow, count (PES = GF) | keys (row 0) | b0 | f (PES = GF) | b1 | layers (PES = GF) |
|---|---|---|---|---|---|---|
| 0 | 0, 3 | 3584, 256, 0... | 7 | 1586.39099 | 3 | 5 |
| 4 | 11, 2 | 3584, 256, 0... (row 1: 3585, 256) | 7 | 1080.18091 | 3 | 3 |
| 5 | 13, 3 | 3584, 256, 0... | 7 | 904.668335 | 3 | 5 |

  Groups 1, 2, 3, 6 agree the same way. Group 4's two rows (keys 3584 and 3585) are identical in b0/f/b1, so they differ only in key 3585 and in their 3 layer names (name text not compared).
- **Conclusion (CONFIRMED for groups 0 to 6): the loader and the data source are not the 0xDE/0xD9 difference.** The row contents GF builds equal PES's in-memory rows. Nothing to port from the loader; no poked value.
- What remains (OPEN): the pick itself. `0x1404B6400` calls `0x141F4FC30`, which calls `0x141F4ACE0` (the row choice, with the ctrl/pick histories at 0x1437F6080 + cat*0x40 and 0x1437F5DC0 + cat*0x40, and the Random1000 objects in the statics window). seq203's cat-3 ctrl history is empty; its cat-3 pick history holds one node (key 1, value 0x3C001), not keyed by 0x30005 (INF: a different map). GF's `InitHistory` starts every history empty. Whether this changes the row for 0x30005 is not shown. Next experiment: dump PES's Random1000 state and the 0x141F4ACE0 inputs at seq203 and compare with GF's statics at the same call.

## 5. Step 4: measurements (NOT RE-MEASURED this round; cited from CutLane6 on 5b608ffb4)

- My worktree carries the stash (fixed-block model), so any test run now is not the tip. I did not measure it.
- CutLane6 (CONFIRMED by its own runs, `pes-port-CutLane6.md` section 5): gen --check on 5b608ffb4 rc 0, 10,999 byte-identical; pes_goal_anchor_test 25 pass, 1 fail (WideChannelScorerTakesTheElseArmAndEqualsPes); pes_fixdemo_select_test 8/8; pes_fixdemo_table_test 7/7; pes_fox_world_test 15/15; PesStallQuick 53 (bound 53, PASS).
- My own run this round: `pes_fixdemo_table_test` built (ccache) and the scratch diagnostic run (reverted). It is not a gate result.
- ALiveBall: not run (Main's 20-minute run is still requested).
- Fast gate: not run (agents do not run it; Main gates).

## 6. Open items (numbered)

1. Main: hooks in section 0 at the next relaunch (rig is Main's).
2. Writer and helper: model the record tree for synthetic states (vtable children, A-class vectors) so the driver gets valid cases, or wait for the captures. Then the root's own diff, and only then the layer.
3. 0x1419C9C60 (walker's next target): same record-tree shape (it reads two more class-data vectors at owner+0x10/+0x18 and +0x20/+0x28/+0x38/+0x40); spec not yet hand-completed.
4. 0x15B050120: a `tail` callee outside the image. Check where the writer's tail jump leads before any translation.
5. Pick: compare 0x141F4ACE0's inputs (Random1000 objects, pick and ctrl histories) between seq203 and GF's statics (section 4).
6. The stash's fixed-block model (gfx device block) stays unpushed until the writer is translated and proven.

## 7. Next exact step

Main: queue the two hooks in section 0 (the helper hook is new). Lane: model the record tree from those captures (or a synthetic one from the capture's bytes via `synth.py --spec`), get 0x1419CB300 to valid cases in the driver, then record it and regenerate the layer.

## 8. Round 2 (Main's IRC): step 3 is the lead. Scenario M4 (test re-staged); production NOT landed

**Correction to section 4.** The scene that differs (A0+8, 0x300DE vs 0x300D9) is read from the goal's **control table (T3)** by
`0x141F4FC30` (variant 0x30005 -> ctrl row -> its +0x28 scene id), not from the cut-library rows (T1) I compared in section 4. The
T1 rows do agree with PES (section 4 stands for T1 only). The T3 choice writes its node into the ctrl history 0x1437F6080 + 3*0x40.

### 8.1 Test-input change (scenario M4, assertions unchanged)

`tests/pes_goal_anchor_test.cpp`: `LoadSeq203PickState(sel)`, called in `GoalAnchorKindTwo.WideChannelScorerTakesTheElseArmAndEqualsPes`
after `MakeInstalled` and before `ArrangeGoal`. It loads seq203's own pre-state (CONFIRMED, read with `gf-CutLane-scratch/histdump.py`,
`ctxdump.py`, `pesdb.py`; snapshot `live2/pid2024691/seq203`, unpacked copy of seq204 in `gf-CutLane-scratch/seq204`):
- category table 0x1437F5D58 = {2,13,2,12,9,13,14,3,9,2,2};
- Random1000 0x1437F6340: +0x10 seed 0x13CC6EFD, +0x20 phase 0x280;
- cat-3 pick-history node (0x1437F5DC0 + 0xC0): payload range 2..3, used bytes {0,0,0,0}, key word +0x20 = 0x3C001 (verified byte-equal
  to PES's node at +0x20 and +0x28); PES's +0x1C word is 1, GF's restored node reads 0 (UNEXPLAINED, OPEN; not needed, see ablation);
- the event's ctx facts through `BeginEvent`: stadium 0x52 (ctx+0x50), subject side 1 (+0x5204), event X -37.56 (+0x5208), Y 0,
  event Z 32.0 (+0x5210), event player side 1 / slot 8 (+0x5214/+0x5218), goal difference -1 (+0x5160), 11/11 players (defaults).
The cat-1 history node is NOT loaded (its key encoding is unverified; the scene does not read it).

Result: `GoalAnchorKindTwo.*` 6/6 green; `pes_goal_anchor_test` full binary **26/26** (was 25/26 with the wide test red).

Ablations (same assertions, measured this round; each reverted):
| variant | wide test | scene |
|---|---|---|
| as landed | PASS | 0x300D9 |
| no BeginEvent (no ctx facts) | FAIL | 0x300DE |
| no RNG and no category table | FAIL | 0x300DE |
| no cat-3 history node | PASS | 0x300D9 |
| event player 0/0 (not 1/8) | PASS | 0x300D9 |
| no history and event player 0/0 | PASS | 0x300D9 |
| builder inputs set (SceneBuild{}) | PASS | 0x300D9 |
| baseline (no loader, stash applied) | FAIL | 0x300DE (as CutLane6) |

So: the facts and the session RNG/category table are what this scene needs. The histories, the event player and the builder inputs are not.

### 8.2 Production check (Main's question 2): what matches PES and what does not

Measured by reading GF's arena after `ArrangeGoal` (the test's state, with 8.1 loaded) against PES's seq204 (the anchor call, after the
recompute tick):

| item | PES seq204 | GF after ArrangeGoal | verdict |
|---|---|---|---|
| Random1000 seed / phase (0x1437F6340 +0x10/+0x20) | 0x6657165F / 0x39F | 0x6657165F / 0x39F | **MATCH** (the tick draws exactly PES's count) |
| ctx pick cache (ctx+0x30), size / key / row | 1 / 0x300D9 / 0 | 1 / 0x300D9 / 0 | **MATCH** |
| A0+8 scene, T, radius, bc | 0x300D9 (test expects PES's) | 0x300D9, 2, 0 | MATCH (test) |
| ctrl (T3) cat-3 history node: key(+0x20), range, used | 0xFC06, 2..3, 3 bytes | 0xF706, 4..5, 5 bytes | **DIFFERS** |
| cat-3 pick history node range | 0..0 (was 2..3 at seq203) | 2..3 (unchanged) | **DIFFERS** |
| ctx stadium / subject / event player / goal diff (+0x50/+0x5204/+0x5214/+0x5218/+0x5160) | 0x52 / 1 / 1,8 / -1 | 0 / 0 / 0,0xFF / 0 | **DIFFERS** |

What the last row means (CONFIRMED by a scratch test, reverted: the facts survive `ZoneDecision` and are zeroed inside the rest of
`ArrangeGoal`. INF (call chain CONFIRMED by grep, the write not traced): the writer is probably the recompute 0x140471A80 (translated, `cutscene/pes_fn_140471A80.cpp:1007`), which calls the event
builder 0x14047C500 -> the ctx setter 0x141F514D0 -> 0x141F51630, writing the facts from the **event object** it is given. GF passes a
zeroed `event` (`fixdemo_select.cpp`, `event = Zalloc(kEventBytes)`), so the goal facts are lost after the pick. PES's event object
carries them. INF: this is why the ctrl node differs (the pick's inputs differ, or the node's key/range come from the event object)
although the RNG and the scene agree; NOT shown. The pick runs before the zeroing (the scene needs BeginEvent's facts, 8.1), so the
difference is in the pick's own state or in the object the recompute reads later.

Production facts (`gf_match.cpp` PollCutscenes -> BeginEvent -> PlaceGoalScene): BeginEvent IS called before the goal's PlaceGoal, with
stadium from `set_cutscene_environment` (the GDScript: `library.team_stadium(home)`, unverified against the capture's home team),
weather/time-zone args (default 0 = PES's 0), subject side, event position (ball prediction), goal difference, players per side.
Production does NOT set the event player (side/slot = 0/0); 8.1 shows the scene does not need it, PES has 1/8 (scorer side 1, place 8)
on this goal, but the rule for which man PES writes there is not decoded (goal A's capture had 0/0).
Production seeds from the match seed (`fixdemo_->Seed(seed % 10000, seed / 10000 % 10000)`, gf_match.cpp:160); PES seeds from its clock
(0x141B34AD0, comment at gf_match.cpp:158). The seq203 RNG seed 0x13CC6EFD is therefore NOT reproduced by production: the seed port is
OPEN (needs 0x141B34AD0's clock input, not decoded).

**Production verdict: no M1 port landed.** The three DIFFERS rows are real, but no red-first test could be written for them yet:
the event-object writer is decoded (0x140471A80 -> 0x14047C500) but the object's field layout and the pick's use of the stale facts are
not, and the seed port needs 0x141B34AD0. Landing either without a PES-proven red test would be a guess (ALWAYS REJECTED).

### 8.3 Step 3 (Main): 0x1419B7930

Noted: a pdata-less leaf, unhookable. The cdb translation waits (section 7); the writer's record needs the helper's callers
to be modelled, not the helper's own hook. No change this round.

### 8.4 State of the tree and what to do next

- Uncommitted in `<workdrive>/gf-CutLane` (on top of CutLane6's stash `stash@{0}`, which is also uncommitted): the M4 loader and call
  site in `tests/pes_goal_anchor_test.cpp` (my lines), nothing else of mine. Not committed: the wide test is green only with the stash's
  fixdemo code (its 0x1C key and the recompute call arguments), so a HEAD-only commit would be red. Main or the stash's owner decides
  when to commit. My scratch closure artifacts (specs/cutdb, closure-cutdb.txt, results_cutdb) were removed; universe.json restored to
  the stash's version.
- Scratch (not committed): `gf-CutLane-scratch/` (histdump.py, ctxdump.py, ctxmaps.py, ctrldump.py, pesdb.py, seq204/ unpacked copy of
  pid2024691/seq204, 3.9 GB: delete when this report is archived).
- Gates: not run by the agent (fast gate is Main's). Test binary run: `pes_goal_anchor_test` 26/26, CONFIRMED this round.

### 8.5 Next exact steps (numbered)

1. Decode the event-object layout the recompute reads (0x14047C500's param_1, at the recompute entry of seq203: capture the event
   pointer's bytes from `call.bin` and the snapshot) and feed GF's `event` with PES's bytes (a capture pre-state, policy 6). Then
   re-measure the ctrl node and the post-state against seq204. If they match, the next gap is the seed.
2. Seed: decode 0x141B34AD0 (the clock input) so production's `Seed` gets PES's session/match seeds for the same match.
3. Event player: decode which man 0x14047C500's caller writes into ctx+0x5214/0x5218 (goal A's capture held 0/0).
4. Cat-3 history +0x1C word (PES 1, GF 0) and the range update PES makes at the tick (2..3 -> 0..0): find the writer (the tick's
   pick, or the event object).
5. Then the M1 red test: "a goal's ctrl node and post-state equal PES's seq204" on production facts (not the test's), red first.
6. Step 3 (cdb 0x1419CB300): unchanged; needs the capture hooks in section 0 (the helper 0x1419B7930 hook is the one Main says
   is unhookable: its callers, not itself, must be captured).
