# PressLane4: the two PesPressApproach reds, root cause, and why the fix that turns them green breaks the ratchets

Branch `PressLane4` (worktree `<workdrive>/gf-PressLane4`). Pushed tip: `6f982593` (`git ls-remote origin PressLane4` confirms). That is the merge of origin/Integrate `82ad8564` into the old PressLane tip `0f6fafc5`. Both reds are STILL RED on the pushed tip. No fix is landed. The working-tree fix is saved as `<workdrive>/gf-PressLane4-scratch/rethink_fix.patch`; the tree is clean at `6f982593`.

## 1. Landed: the Integrate merge (6f982593)
- Conflict in `tools/dev/translate/results_trapaction.json` resolved with `json_union_merge.py` (base merge-base, ours PressLane, theirs Integrate). rc 0.
- Layer regenerated with `gen_translated.py --image <local>/pes_full.bin`: 10,987 functions (3,906 recorded bit-exact, 7,081 closure). `--check`: byte-identical.
- Tick `0x140928FC0` is still held out ("closure reaches a mistranslated callee 0x1409F6960"). Not a regression; this is the row Main's re-record must clear.
- Worktree: `extern/godot-cpp` is an empty dir for git and a symlink only during builds (the index gitlink refuses a symlink).

## 2. Red-first explanation (CONFIRMED by a 265k-line decision dump and the test's own frame prints)

A PRESS man's (or designated man's) decision is made in a tick, then the ball stage, the designation recompute or the humanoid contacts change his state. `pesmovestep` reads the decision the next tick, so the stale command steers him for one tick. The test samples the decision after `Process()`, so it sees the stale command.

- ALoosePresser, 21 of 10,022 (test frames 3334, 3347, 3352, 3482, 3506, 3609, 3744, 8623 to 8719, 12468, 12515, 13014 to 13192). At the previous Think each man was the ball retainer, designated and `HasPossession()`, and his queue held an unnamed Movement from the "must be ball retainer" branch (`elizacontroller.cpp` ~612). At the sample he was a loose PRESS man. PES's holder (MA+0x2601C) was also him, so the press writer armed PRESS on the tick's holder before the ball stage gave him the ball. All 21 match this pattern.
- ADesignatedManWithNoHolder, 2 of 369 (frames 4859 and 12375). 4859 is the same retainer-to-loose transition one tick earlier. 12375: the man was not designated at his think and became designated after it. `RecomputePesDesignatedOwner` runs after `teams[*]->Process()` (`match.cpp` ~960).
- Order (CONFIRMED, `match.cpp` 863-1041): ball stage, event stage, referee, RecomputePesHolder, team ticks (PesPressTick, then players' Think), RecomputePesDesignatedOwner, CheckHumanoidCollisions. Contacts can move the retainer after Think.

Killed theories (CONFIRMED against the dump):
- Keeper presser (the `manualMovement` else-branch): no role-0 man among in-scope presser ticks.
- Set-piece walk (`elizacontroller.cpp` ~603): set-piece block only; the test excludes set pieces.
- `_BallControlCommand` walk (`playercontroller.cpp` 286-300): pushes BallControl, which `MovementDecision` drops for a man without the ball. It cannot produce an unnamed Movement.
- Press-word timing: PesPressTick runs before the players' Think in the same team tick.
- Dodge, tackle, trap, slide: non-Movement, they win in `MovementDecision`.

What PES does instead (CONFIRMED, `docs/re/pes-re-press-life.md` section 8): ActionPress runs every frame for a PRESS man from that frame's state and always writes a target (ball-now or base). There is no think to go stale.

## 3. Fix variants and measured numbers (all CONFIRMED runs)

Fix: `pesmovestep::ThinkInputs` (bits: retainer, PES holder, designated, in play, set piece, PASS_GET receiver, loose presser) stored in `PesDecision::inputs`; `RethinkStaleDecisions(match)` after `CheckHumanoidCollisions()` re-thinks an active man whose inputs changed.

| variant (tree) | PesPressApproach.ALoose (<- 21) | PesPressApproach.ADesignated (<- 2) | PesStallQuick (<=53) | PesStall.ALiveBall (<=2229) | ankle |
|---|---|---|---|---|---|
| 6f982593, no fix | RED (21 of 10,022) | RED (2 of 369) | not run on this tip | not run on this tip | not run on this tip |
| broad re-think | GREEN (373 s) | GREEN (347 s) | 1124 RED | not run | timed out, 900 s slot, rc 124, no verdict |
| presser-only re-think | GREEN (484 s) | not run | 1242 RED | not run | not run |
| presser-only, time-to-ball preserved | GREEN (822 s) | timed out, 1200 s slot, rc 124, no verdict | 1242 RED | 2340 RED (126 episodes) | not run |

The last row is the tree in `rethink_fix.patch` (built into the test binaries, reverted in source). Its ADesignated run did not finish inside the slot, so it is not green.

Conclusion: re-thinking a man in the same tick (in any scope I tried) is what breaks both stall ratchets. Preserving the time-to-ball did not help, so that side effect is not the cause. Cause not decoded (INF: the newly loose presser's `_PressApproach` target moves him where the breaker's count differs). Not checked.

Mutation checks: not run (no fix landed).

## 4. Next exact step
1. Measure the tip baseline for `PesStallQuick` and `PesStall.ALiveBall*` (no fix) so the regression is against a measured number, not the 53/2229 ceilings.
2. Decode the stall breaker's forced-live ticks with the re-think (which man, which tick; `pes_stall_test.cpp` ~766) to see whether the re-thought presser's command draws him off the ball.
3. Do not land a re-think. The PES-first fix is RunActionPress through the tick, which needs `0x1409F6960` in the layer (Main's record). That names the PRESS man's target from the frame's state with no re-think, and may cover the designated case too.
4. Ankle test: rerun at >= 1800 s slot timeout; it did not finish in 900 s.

## 5. Budget and rig
- Rig requests: 0 (no live PES use). Sims only through `simslot.sh`.
- Commits pushed: `6f982593` (merge + layer). No fix commit.
- check_pii: not run on `6f982593` (run from `<workdrive>/GodotFootball`).
- Gate: not run (Main gates).
- Test binaries run: `pes_press_approach_test` (ALoose, ADesignated, ANoMan), `pes_restart_delay_test` (PesStallQuick, PesStall.ALiveBall*), `pes_throwin_release_test` (ankle, timed out).
