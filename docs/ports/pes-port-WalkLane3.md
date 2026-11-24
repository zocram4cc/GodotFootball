# PES port: WalkLane3 (branch WalkLane, tip a9d9e1b3 = origin/Integrate c4404052 merged in)

## 0. Verdict
- Nothing landed. No PES port reached ALiveBall or a green gate. WalkLane is at `a9d9e1b3` (pushed, `ls-remote` confirmed): the merge of origin/Integrate `c4404052` into WalkLane tip `7bac0dfd`, no other change.
- Item 1 (dribble series): decoded (section 2). The premise in WalkLane2 is not PES's mechanism: PES does not issue the next series member at a clip's end. The PES mechanism is a parametric blend across the series members, which GF's body cannot play yet. No red test landed (a red test with no landable green would pin GF's behaviour).
- Item 2 (sliding stall): decoded the action table and tick (section 3). Not ported: the sliding tick and its end handler are untranslated.
- Item 3 (re-land ids + walker): not attempted. Measured stack (WalkLane2): base 2,217, walker alone 2,375, ids + walker 2,581, bound 2,229. Items 1 and 2 are the blockers.
- Item 4 (strike root lock): attempted, not landed. The lock port (AnimMovePlugin +0x81) never reached the body's root on the sampled ticks (section 5). Patch kept at scratch `strike-lock-attempt.patch`; reverted.
- Rig requests: none.

## 1. Measured on the merged tree (a9d9e1b3 base; no change kept)
- `PesPassTarget.ATrapsRootMovesByExactlyTheStrikeAdjusterStep`: RED. Worst 38.48990087242283 m over 240 ticks, identical to 14 digits with the lock on and with the lock off. CONFIRMED.
- `PesPassTarget.EveryPassGetEpisodeIsWithinPesLiveRange`: RED on the base (pre-existing; WalkLane1 section 4). `IsSetInFlightAndNeverStaleWhileTheBallRests`: SKIPPED.
- The 38 m is a metric artifact: 17 of 17 probe samples with gap > 8 m have previous adjuster point `prevax = (0,0)` (scratch `pass_probe.log`). The test takes its reference from a sample where A+0x14 is not yet written. A test defect, not root drift (OPEN: confirm the steady gap once such samples are skipped).
- The steady gap is real: on warp-zero strike ticks the body moves twice the adjuster: e.g. `g` step (0.083, -0.038) vs `A+0x14` step (0.041, -0.020), gap 0.046 m against a 1 mm tolerance. CONFIRMED from the probe run. This is the clip's root delta added on top of the adjuster's point, the defect the test names.
- Strike warps: `StepAdjusters` sees `B+0x81` valid (strikeMoved) on the sampled ticks, but the step `(frame.x, frame.z)` is (0, 0) on every printed tick (`pass_probe.log`, STRIKE lines). CONFIRMED.

## 2. Dribble series (item 1): PES mechanism
### 2.1 Facts (VA cited)
- Dribble action vtable `0x1425E2440` (read from `pes_full.bin`, offset = VA - 0x140000000): +0x20 `0x1407B37D0` (the dribble tick, RunDribbleTick), +0x50 `0x1407C1AD0`, +0x58 `0x1407A38E0` (act4 start), +0x70 `0x1407B9280` (leave test). CONFIRMED (image read).
- Series variant `0x1406F14E0` (translated, `src/pes/translated/closure/pes_fn_1406F14E0.cpp`): fills P+0x3B90 (count), +0x3B98 (data); entries are 0x38 bytes (motion +8, seconds +0xC, mirror +0x10); stores 2 at P+0x375C; no motion in P+0xAE0 (`src/sim/onthepitch/pestrapworld.hpp:75-85`). CONFIRMED (code + docs).
- SetParametric `0x141AF52B0` (translated, `src/pes/translated/animation/pes_fn_141AF52B0.cpp`; caller `0x1406E5FC0` at `0x1406E6325`): one layer record per member, blended by the series' position on its rate table (descriptor +0x24). The translated engine selects members exactly: position 0 gives member 0, 0.15 gives member 1, 0.30 gives member 2 (`docs/ports/pes-port-AnimeLayer7-steer.md`, branch OneWorld-series-wip). CONFIRMED on the translated engine.
- The records loop (TrapContact10 section 14, 15.1). walk_1_1's gani carries the loop flag on all 15 units. CONFIRMED (TrapContact10 ablation).
- Clip-end re-issue: the dribble tick's clip-end arm (B05F0) reaches `FUN_1407B2D60` (the next-motion request) only through input byte 0x34, which the decision frame did not set in 900+ frozen ticks (TrapContact10 section 13, H6 trace). CONFIRMED (trace).
- GF: `PlayDribbleIssue` (`src/sim/onthepitch/teamAIcontroller.cpp:3532-3545`) plays `SeriesFirstOf` (member 0) as one clip. Humanoid holds the last frame for a PES-owned dribbler (`humanoid.cpp` ~316, per TrapContact10 15.1). That is the freeze: walk_1_1 frame 120/121, velocity 0, 900+ ticks (TrapContact10 section 15.1, WalkLane2 section 2).

### 2.2 Answers to the three questions
- Which member follows: none, in sequence. The members whose rate-table brackets the series position are all live at once and blended (`0x141AF52B0`). CONFIRMED on the engine; the brackets come from the decision's position (INF on which decision writes it).
- When the next clip is issued at a clip's end: not at the end. The layer loops; the dribble tick re-issues only from the clip-end arm, gated by byte 0x34. CONFIRMED.
- What re-issues motion: the decision frame (byte 0x34 via `FUN_1407B2D60`), and the stand/idle handlers' series SetMotion (`0x1406F14E0`, via `FUN_1407C18B0`). CONFIRMED on the chain, INF on the exact trigger.

### 2.3 Consequence for the port
- A "play the next member when one ends" port (WalkLane2's plan) is a GF stand-in, not PES. TrapContact10 15.1 measured that a single member looped (wrap) walks straight away from the ball, because the steering is in the other members' blend weights. So the freeze is not fixed by sequencing members.
- The PES-faithful port is the bracketing blend. The translated engine already has it: `PesFoxWorld::IssueSeries` (`src/sim/onthepitch/pesfoxworld.cpp:291`) and `ReadPose` (`:339`). Nothing in sim calls them: `Match::PesAnimeIssue` pushes single SetMotion records only (`matchevents.cpp:131`), and `ReadPose` has no sim caller. So GF's body would need the engine's pose and root, which is a multi-round change (scope beyond this round).
- The existing census already counts the freeze (`PesStall` `notOnTableSeriesFrozen`, `strandedSeriesFrozen`, `tests/sim/pes_stall_test.cpp`).

## 3. Sliding stall (item 2): PES action and tick
- Sliding vtable `0x1425E1878` (image read): +0x20 `0x140786D60` (the sliding tick), +0x50 `0x140788C00`, +0x70 `0x140787EC0` (leave test: `selectedTrackFrames <= clip frame count`, `docs/ports/pes-port-StallSlide.md` section 5), +0x90 `0x140789D00` (untranslated; the abort, `docs/ports/pes-port-StallBreaker4.md` section 1). CONFIRMED.
- The sliding tick `0x140786D60` (headless decompile, `scratch dec_w3_slide.txt`, 568 lines): reads the phase byte through the secondary's vtable +0x20, switches with `FUN_1406EEBB0`, issues motions with `FUN_1406F02F0` (requests 2 and 0x11), and calls `FUN_1406E6660` / `FUN_140A53F90` on the exit arms. CONFIRMED structure; the decode is not complete.
- GF never runs `0x140786D60` (no `pes_fn_140786D60` in `src/pes/translated`; the ids are labels). So a body under id 0xB never gets PES's exit. Under the walker anchor, `sliding/idle/000.anim` holds 609 ticks (WalkLane2 section 2). CONFIRMED that the tick is absent; INF that its exit is the stall.
- Port needs: translate `0x140786D60` and its closure (incl. `0x140788C00` and the untranslated `0x140789D00`), prove, then re-measure. Not done.

## 4. Re-land (item 3): not attempted
- Bound 2,229 needs the dribble and sliding causes above fixed; the walker alone is 2,375 (over). Nothing re-landed. No ALiveBall run on this branch.

## 5. Strike root lock (item 4): attempted, not landed
- Port written (kept in `scratch strike-lock-attempt.patch`): `HumanoidBase::LockRoot(bool)` set from `pesmovestep::StepAdjusters` by `frame.strikeMoved` (AnimMovePlugin +0x81, `docs/re/pes-re-anim-blend.md` 7.1); in `HumanoidBase::Update`, while locked, the apply position is `spatialState.position` and `startPos` is re-anchored to `position - smuggles - positions[frame]` so the clip adds no root delta.
- Result: identical trajectory with the lock on and off (38.4899 m both). Lock-branch counter in `HumanoidBase::Update` = 0 over the 240 sampled strike ticks. So the Update branch never ran for the strike man with the flag set. INF: either the trap body's root is written by another path, or Update does not run for him in those ticks. OPEN: find the writer of the clip delta on `g` for the strike man (slots 5 and 9 in the probe).
- Reverted. Tree clean at a9d9e1b3.

## 6. Checks run
- Merge: `git merge origin/Integrate` (c4404052) into WalkLane: a9d9e1b3. Pushed (`7bac0dfd..a9d9e1b3`).
- Build: `pes_pass_target_test` and `gfsim` built (ccache warm; incremental minutes).
- Tests run: `pes_pass_target_test` (4 tests: 1 PASSED, 2 FAILED as in section 1, 1 SKIPPED). Probe runs with the lock: 3 runs (lock on; lock off; lock on with probes).
- Not run: `pes_stall_test` (ALiveBall, PesStallQuick), `pes_receive_test`, `pes_setpiece_stand_test`, `pes_throwin_release_test`, `gen_translated.py --check`. Nothing landed, so no gate.
- Mutation: lock forced off (`LockRoot(false && ...)`): same trajectory as lock on (section 5). Reverted.
- Red-first: the strike test is red on the merged base (section 1). No green change to test.

## 7. Worktree note (for the next lane)
- In `<workdrive>/gf-WalkLane`, `git status`, `git merge` and `git fetch` fail with "expected submodule path 'extern/godot-cpp' not to be a symbolic link" (the godot-cpp symlink vs the index gitlink). Workarounds that worked: `git -c submodule.recurse=false ...` for merge and push; `git fetch --no-recurse-submodules origin <branch>`. A stale staged `pestrapactor.cpp` (the 2f8fcca3 blob) blocked the merge; `git reset -q -- <path>` fixed it (the worktree copy equalled HEAD).

## 8. Open items (numbered)
1. Strike lock: find the writer that adds the clip root delta to `GetPosition()` for the strike man, and whether `HumanoidBase::Update` runs for PES trap men on those ticks (probe slots 5 and 9; counter `LOCKBRANCH` = 0).
2. `ATrapsRootMovesByExactlyTheStrikeAdjusterStep`: the reference sample with `A+0x14 == 0` is a test defect; skip such samples, then measure the steady gap (about 0.046 m observed on warp-zero ticks, against 0.001 m).
3. Dribble series: port the bracketing blend (`0x141AF52B0` semantics) into the body, or drive the body from the translated engine (`IssueSeries` + `ReadPose`). Needs the per-tick series position from the decision (`0x1406F14E0` caller). Multi-round.
4. Sliding: translate `0x140786D60` and `0x140788C00`, decide `0x140789D00`, prove, re-measure the 0xB stall.
5. Item 3 gate: re-measure ALiveBall (bound 2,229) only after items 3 and 4 are done; base is 2,217 (WalkLane2).
6. Trip, Interfere, HighPass, Header ids: still INF (WalkLane2 section 2).
