# CutLane2: goal anchor split into PES's two ticks (recompute, then anchor); the wide test is RED on a named blocker

Branch CutLane (worktree <workdrive>/gf-CutLane). Labels: CONFIRMED = tool output this session; INF = inference; OPEN = blocker named.

## 1. What is CONFIRMED

- The recompute 0x140471A80 is already proven bit-exact on the real call seq203 (recompute_471A80 hook): `results_snapdiff_recompute.json`, 1/1 replayed, mismatches []. So the translated T writer is not the gap.
- The DemoInfo tick 0x14047B460 (decompiled, Ghidra headless, <workdrive>/gf-CutLane-scratch/dec_7b460.txt): its only T-relevant effect is the EA-gated call to 0x140471A80 (lines 161-163). The rest is per-frame sound, sanction and sync (about 200 lines). Not translated: no proof needs it. Main agreed no hook is needed.
- The anchor 0x140474F00 (decompile dec_474f00.txt): reads A0+8 (scene), A0+0xC (radius), the scene row (cut mode bytes +2, +3, +4), the field info, and the scorer's man record +0x55C/+0x564 at its own frame. It does NOT read the run target. Mode 3 on z (row +3) gives z from 0x140A7C9F0(row, x, z): z = 34.0 is the row's float (0x42080000 in the scene row). So pos depends on tick B's man position, not on the celebration spot.
- 0x140476010 (the scene-0x3013B T source): threshold constant 0x14253EE30 = 100.0 (install, read from pes_full.bin). Not on the BC=2 path for scene 0x300D9.
- BC<6 scene pick 0x1404B6400 (dec_b6400.txt): returns the u32 at +0x28 of the cut group record FUN_141F4FC30(ctx, 0x30005) for BC 2 (case 2 -> 0x30005). A0+8 after the recompute = that value.
- seq203 entry state (streamed from mem.bin.zst, nothing unpacked): A0 = 0x31C53DB8, G = 0x2DB0C420, A0+8 = 0x300AB, BC 2, C0 8, EA 1, DC 200, A0+0x114 = 1124239360 (the generator state pinned in the test's facts).

## 2. Code (fixdemo_select.cpp/.hpp)

- `ArrangeGoal(facts)` = tick A: the arrangement, zone decision, recompute (else arm), celebration spot, radius, scene, timers. Arms `armed` (the target and scene) and `gateSlot`.
- `AnchorGoal(facts)` = tick B: writes the scorer's facts for its own frame (`WriteMatchFacts`), runs 0x140474F00 and the flip 0x140478FF0 on the armed arena. Returns nullopt before ArrangeGoal for the same slot.
- `PlaceGoal(facts)` = ArrangeGoal then AnchorGoal on the same facts (production and the existing tests; behaviour unchanged).
- `Arrange` split: `WriteMatchFacts` (both ticks) and the tick-A defaults.
- ponytail kept: the run object's +0x18 is NOT written by the translated chain; mutation (below) shows the T-writer test fails without GF's manual write.

## 3. Tests (pes_goal_anchor_test.cpp, pes_fixdemo_select_test.cpp)

- `pes_fixdemo_select_test`: 8/8 green.
- `pes_goal_anchor_test`: 23/24 green. RED: `GoalAnchorKindTwo.WideChannelScorerTakesTheElseArmAndEqualsPes`.
  - Its T assertions now pass (the celebration spot (-41.6211472, 0, 35.56) from the seq203 man).
  - The armed scene assertion (A0+8 = 0x300D9) fails: GF gives 1. Blocker below.
  - The pos assertions fail: GF pos[0] -57.5796 (PES -41.5502167), pos[2] 0 (PES 34.0). Consequence of the scene: the row mode differs.
- New green: `GoalAnchorKindTwo.RecomputeTickWritesTheCelebrationSpotIntoTheScorersRunObject` (seq203 state, run+0x18 == PES T).
- Mutation (run, rebuilt, reverted): removing GF's manual run+0x18 write -> RecomputeTick test KILLED (run+0x18 stays 0). Reverted; the line is back.
- Not run: a mutation on AnchorGoal (the wide test is red, so no test is sensitive to it yet).

## 4. Next blocker (OPEN, named)

The scene pick. PES's 0x1404B6400 for BC 2 reads the u32 at +0x28 of the cut group record FUN_141F4FC30(ctx, 0x30005). GF's record gives 1 (PES: 0x300D9 = 196825). Next step: decode FUN_141F4FC30 (the cut-group lookup) and check GF's cut library entry for group 0x30005 (built by the cut library builder 0x141E5C8F0 from constant_* data, see `kCutGroupPaths`). Until that matches, the anchor's row is the wrong row and pos cannot match.

## 5. Other items not done

- The brief asked to translate 0x14047B460. Not done: the tick's only T-relevant effect is covered by the recompute's real call (see 1). Recorded here so Main can decide.
- No production timing change: gf_match still calls PlaceGoal in one frame. Splitting it into ArrangeGoal (event) and AnchorGoal (a later frame) changes goal camera timing; not done.
- Disk: <workdrive> was at 148 GiB free (below the 150 floor). Main approved incremental rebuilds down to 140 GiB; no capture was unpacked (streamed reads only).

## 6. Checks

- `gen_translated.py --check`: 10959 functions (3900 recorded bit-exact, 7059 closure) regenerate byte-identical, rc 0.
- Pushed: CutLane `73406ffb` (ls-remote confirmed). Committed with explicit paths (the commit namespace refuses this worktree: extern/godot-cpp is a symlink).
- `ledger.py --check`: `LEDGER.md current (47749 rows)`, rc 0.
- PII: grep of the three changed files for `<workdrive>` and `<home>`: none.
