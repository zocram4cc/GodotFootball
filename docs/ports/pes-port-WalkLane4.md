# PES port: WalkLane4 (branch WalkLane, tip 158aadf5, pushed; NOT landable)

## 0. Verdict
- Nothing lands. The gate is red: PesStallQuick measures 319 to 452 against the ratchet of 53 (base at a9d9e1b3 was 28; WalkLane1 section 1).
- Item 1 (dribble series): partly done. A PES dribbler's member now loops under PES's Dribble action instead of holding its last frame; the red test is green. The blend itself (0x141AF52B0 weights for positions between rate-table points) is NOT ported: its position input (FUN_1406F14E0's request filler) is untranslated.
- Item 2 (re-land ids + walker, ALiveBall/Quick): measured once with the dribble loop and the strike lock (ids reverted, base ids); Quick red. Measured with the ids and walker, lock off: Quick 319, red. Sliding (0xB) tick not ported.
- Item 3 (ATrapsRoot): the test is fixed; the red value on current code is 0.0480 m (lock off), as WalkLane3 predicted (0.046). The strike lock (AnimMovePlugin +0x81) cuts it to 0.0114 m but does not reach the 1 mm bound. Still red.
- Rig requests: none.

## 1. Commits (branch WalkLane, pushed; `git ls-remote origin WalkLane` = 158aadf5)
- fced68dd tests: `PesPassTarget.ATrapsRootMovesByExactlyTheStrikeAdjusterStep` pairs only consecutive strike ticks past the reference load; `PesStall.APesDribblerNeverHoldsHisClipsLastFrame` (new, red then green with the loop).
- 158aadf5 WIP production: dribble member loop (humanoid.cpp), strike root lock (humanoidbase.hpp/.cpp, pesmovestep.cpp), PES ball action ids restored (pestrapactor.cpp, the 2f8fcca3 rows), walker clip-start anchor (humanoidbase.cpp PlayIssuedClip, from walker-clipstart.patch).

## 2. Item 1: the dribble freeze and the loop
### 2.1 Facts
- The freeze is at the clip end: `Humanoid::Process` (humanoid.cpp, end-of-clip switch, formerly line 419-425) does not switch a PES-owned dribbler's body, so the clip's last frame holds. CONFIRMED (code).
- The frame advance clamped at the last frame (`std::min(frame + steps, count - 1)`). CONFIRMED (code).
- PES: the dribble series member's record carries the loop flag (WalkLane3 section 2.1; TrapContact10 section 14). PES's layer does not hold a finished member. CONFIRMED (prior lanes' trace; not re-decoded here).
- PES's SetParametric (0x141AF52B0, caller 0x1406E5FC0 at 0x1406E6325) selects members by the position on the rate table: a position at a table point gives that member alone, between two points two members blend. CONFIRMED on the translated engine (docs/ports/pes-port-AnimeLayer7-steer.md section 4; the translated function is `src/pes/translated/animation/pes_fn_141AF52B0.cpp`). The position is the request filler's param_7 (FUN_1406F14E0), which GF does not translate: so the between-points blend has no input here. INF on which decision writes it.
- The translated engine's `PesFoxWorld::IssueSeries` and `ReadPose` exist; nothing in the sim calls them (`src/sim/onthepitch/pesfoxworld.cpp:291`, `:339`).

### 2.2 The port (GF stand-in for the record loop; mapped to PES)
- While PES's Dribble action owns the body (`PesOwnsDribble`), the frame wraps to the first when it passes the last. The wrap carries the cycle's root travel into `startPos` (`startPos += positions[N-1] - positions[0]` per wrap), so the body's position `startPos + positions[frame]` is continuous across the wrap. CONFIRMED arithmetic: at the wrap, `startPos' + positions[0] = startPos + positions[N-1]`.
- INF: the wrap law. PES adds each frame's root delta (pes-re-anim-blend.md 7.1), so a looped member's per-cycle travel is the cycle's own root travel. Not measured against a PES capture.
- Other clips hold their last frame as before. The end-of-clip switch gate is unchanged for them.

### 2.3 Measured
- Red before the port (dribble + lock, base ids): `APesDribblerNeverHoldsHisClipsLastFrame` worst hold 343 ticks in a row (`Failure: 343 vs 1`, `stall_dribble_red.log`). Red test first, on the pre-port code; the test sets `worstLastFrameHold` in the contact census.
- Green after the loop: the same test passes (census run, `census_p1.log`).
- NOT green: `PesStall.APesDribblerPlaysTheMotionItIssued`: `dribbleMotionNotOnTable = 26` (expected 0), with dribble + lock. Not investigated (budget).

## 3. Item 2: ids, walker, sliding, and the ratchet
### 3.1 Measured (`pes_restart_delay_test`, seed 7; PesStall census)
| tree | ALiveBall | PesStallQuick | note |
|---|---|---|---|
| a9d9e1b3 (base, ids reverted) | not re-run | 28 (WalkLane1) | ratchet 53 holds |
| dribble loop + strike lock, ids reverted | PASS (<= 2229; exact count not printed by the test) | 452 | `census_p1.log` |
| dribble loop + ids + walker, lock off | not run | 319 | `quick_m1.log` |
| dribble loop + ids + walker + lock | not run | not run | the landing tree |

- The quick increase comes from the dribble loop, the lock, or both: the two measured trees differ in the ids as well, so the split is not isolated. INF: the loop keeps a PES dribbler moving where GF's body froze, which changes the breaker's quick-window firings.
- ALiveBall: PASS on dribble + lock; the count itself was not printed. Brief asks for it: NOT reported as a number (the test prints only on failure).
- Walker alone (WalkLane1: 2,375 over base) and ids alone (WalkLane2: 4,965) are unchanged by this round; the combined measure is not in hand.

### 3.2 Sliding (0xB)
- Not ported. `0x140786D60` (sliding tick, WalkLane3 section 3) and its end handler `0x140788C00` remain untranslated; `0x140789D00` (abort) untranslated. The 0xB id stays in the tree and the sliding clip still holds its idle loop (`sliding/idle/000.anim`).

## 4. Item 3: ATrapsRoot
### 4.1 Test fix (red on current code)
- The old test compared the body step with the step of `A+0x14` from a sample where the reference was unwritten (the 38.49 m).
- PES: `A+0x14` is loaded from the layer's root point when the adjuster's counter reaches 1 (`FUN_141AF3CA0` via `FUN_141EB3AF0`; docs/ports/pes-port-RootMotion.md section 1). Before that the field keeps its unwritten zeros. The sample rule was tightened in two steps: (a) only counter >= 2 (after the load); (b) a pair only when the counter advanced by exactly one (the same strike). Step (a) alone left 37.47 m: a new strike's reference restarts the counter, so the pair straddles the reference jump.
- Final rule measured: lock off 0.0480 m over 254 pairs (RED, `pass_m1.log`). Lock on 0.01135 m over 642 pairs (RED, `pass_p4.log`). Target 0.001 m.

### 4.2 Strike lock
- Placed in `HumanoidBase::CalculateSpatialState` (humanoidbase.cpp, the `positions` branch): while `rootLocked`, `startPos = spatialState.position - smuggles - positions[frame]`, so the clip adds nothing and the body moves only by `WarpRoot`. The flag is set in `StepAdjusters` (pesmovestep.cpp) from `frame.strikeMoved` (AnimMovePlugin +0x81, pes-re-anim-blend.md 7.1).
- The earlier attempt put the lock in `HumanoidBase::Process`; the body runs `Humanoid::Process` (humanoid.cpp:352), so that branch never ran (its LOCKBRANCH counter 0). That explains the old zero counter. CONFIRMED.
- Residual 0.0114 m (lock on): INF. The warp is `B+0x90 - O2+0xD0` (pesworldrecord.cpp:1228-1231) and the test compares with `A+0x14`'s step; the two coincide only if `O2+0xD0` equals the previous tick's point (WalkLane1 section 3, INF). Open: check `O2+0xD0` against the previous `A+0x14` per tick.

## 5. Mutations (run, reverted)
- Root lock forced off (`if (false && rootLocked)`): gap 0.0480 m vs 0.0114 m with the lock. Reverted before commit.
- Dribble loop: red (343 ticks) before, green (0) after. Not separately mutated (red-first run is the pre-port measure).

## 6. Checks run / not run
- Run: `pes_restart_delay_test --gtest_filter=PesStall.APesDribbler*:PesStall.ALiveBall*:PesStallQuick.*` (census_p1: 1 failure in APesDribblerPlaysTheMotionItIssued, 1 in PesStallQuick, ALiveBall passed); `PesStallQuick.*` lock off (quick_m1); `pes_pass_target_test` (pass_fix_red, pass_p3, pass_p4, pass_m1); `pes_restart_delay_test` red dribble test (stall_dribble_red).
- Not run: `pes_receive_test` 60/60, `pes_setpiece_stand_test` 12/12, `pes_throwin_release_test` (ankle, carried), `PesAnimeAction.*`, `gen_translated.py --check` (no translated file changed), `tools/check_pii.sh` (run from the main checkout; my changed files grep clean for `<workdrive>` and `<home>`), GATE fast (not run: not landing).
- Build: `pes_restart_delay_test`, `pes_pass_target_test` (ccache warm, minutes each).

## 7. Worktree note
- In `<workdrive>/gf-WalkLane`, plain `git status`/`add`/`commit` fail with "expected submodule path 'extern/godot-cpp' not to be a symbolic link". `git -c submodule.recurse=false ...` works for add, commit, push, ls-remote.

## 8. Open items (numbered)
1. Dribble + ids + walker + lock together: run `PesStallQuick` and `ALiveBall` (print the ALiveBall count on pass). The quick ratchet is 319-452 now vs 53.
2. Split the quick regression: dribble loop alone (ids reverted) vs lock alone.
3. `PesStall.APesDribblerPlaysTheMotionItIssued` 26 off-table ticks after the loop: find which clip leaves the table (the loop's frame wrap vs motion table).
4. The wrap law and root carry of the record loop: measure against a PES dribble capture (INF now).
5. Blend (0x141AF52B0): translate FUN_1406F14E0 (the series position writer) so the between-points weights have an input; then drive the body's two-member blend.
6. Sliding: translate 0x140786D60, 0x140788C00 and decide 0x140789D00, then re-measure the 0xB stall.
7. Strike lock residual 0.0114 m: check `O2+0xD0` against the previous tick's `A+0x14` / `B+0x90`.
8. Walker re-measure with the lock off and on (walker alone was 2,375 over base in WalkLane1).
