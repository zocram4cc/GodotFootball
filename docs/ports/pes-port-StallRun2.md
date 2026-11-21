# pes-port-StallRun2: the clip-lookup census is not the GOTO/STEP driver; SetMotion's start time is now honoured (ALiveBall 2,401 -> 607 on 8ef8ae87, 92 on the merged tree); the merged tree regresses TheThrowInTakersAnklesStayBehindTheLine (StallRun, 21-11-26)

Status: PARTIAL. One change landed with a red-first test. It moves the stall count a lot, but it breaks a throw-in test on the merged tree. Branch `StallRun`: **8ef8ae87** (the fix) and **a2439354** (merge of StallLive 43e96723 into StallRun). Both pushed; `ls-remote` confirmed `a2439354bf5a88f88fc116680a189bc6c051c62`. The worktree equals a2439354 (fix restored after the ankle isolation in section 4).

## 1. Census: PlayIssuedClip misses (temporary instrumentation, reverted)

One seed-7, two-minute PES-loop match (temporary `PesCensus` test, 3,600 s limit, 31,218 issues). Printed at `Issue` (group, motion, rate, entry state) and in `PlayIssuedClip` (miss-empty, miss-filter).

| population | issues | misses | note |
|---|---|---|---|
| all | 31,218 | 496 (1.6%) | |
| GOTO_RUN entry (state 7) | 3,102 | 6 (0.2%) | not the GOTO driver |
| STEP_LOOP entry (state 4) | ~7,600 | ~45 (0.6%) | 3509/3541 misses are `miss-filter` (body on a ball action) |
| RUN_LOOP entry (state 3) | 2,684 | 207 (7.7%) | all motion 2891: locomotion family 102 has no clip; trap family 103 has it |
| IDLE_OTHER entry (16) | 1,979 | 207 | motion 2891 again |

Reading: the clip lookup is not what keeps the GOTO_RUN or STEP_LOOP body slow. The one real gap is motion 2891 (a RUN_LOOP and IDLE_OTHER issue, present only as a trap-family clip). A test asserting that the locomotion family holds *every* Animetable cell motion failed on dozens of motions (dodge, tackle and kick cells are not locomotion), so it was dropped. The right invariant needs MOVESTEP's locomotion row tables, which the port does not have yet. OPEN.

GOTO_RUN issues (3,102) are mostly SetMotion (`group=-1`). Motions 4152/4154/4155 (goto_run) and 2221/2237/2220 change between ticks about 350 times, and each change restarts the clip at frame 0. OPEN: whether PES flips the same way.

## 2. The fix: SetMotion's start time (CONFIRMED structure, MEASURED effect)

- `PlayIssuedClip` dropped `MotionRequest.startTime` (req[0x33], the SetMotion argument that FUN_1406b8340's tail passes, pes-re-anim-select.md 1). Every issued clip began at frame 0, so a step start replayed the wind-up that MotionStartSeconds (FUN_1406b97d0) is meant to skip.
- The fix: the issued clip's start frame is `startTime / pes::kGfAnimFrame_s` (0.01 s), clamped to the clip's frame count. Re-issues of the playing motion are unchanged. Trap callers keep the default 0.
- Files: `src/sim/onthepitch/player/humanoid/humanoidbase.{cpp,hpp}`; test `tests/sim/pes_movestep_wire_test.cpp::AnIssuedClipStartsAtItsStartTime`.
- **Red first (done):** with `frameNum = 0` restored, the test failed (got 0, expected 50) for motions 4152 and 1747 on two men. With the fix restored, it passes.

## 3. Measurements

| check | HEAD 1b56b726 (StallLive16 §1) | 8ef8ae87 (fix) | a2439354 (fix + StallLive 43e96723) |
|---|---|---|---|
| PesStall.ALiveBall (forced ticks) | 2,401 | **607** | **92** |
| PesStall other 7 | PASS | PASS | PASS |
| PesHang seed 11 | PASS | PASS (1,013 s) | PASS |
| PesPassTarget | PASS | PASS | PASS |
| Throw-in: TheThrowInTakersAnklesStayBehindTheLine | PASS | PASS | **FAIL**: 1 of 61 taking ticks, nearer ankle 0.009 m short, persistent |
| Throw-in: the other tests | PASS | 6 PASS; ceremony test not finished (exit 124 at the 3,600 s limit) | 5 PASS plus the ankle FAIL; ceremony not finished |
| Seed-7 smoke (2 min) | 50,435 ticks, determinism dae72e72a5e831e2 | 29,807 ticks, determinism c2016bf08fd0bc98 (rerun identical); shots 4-7, goals 3-5, passes present | identical to 8ef8ae87 |
| gen_translated --check | PASS | PASS (10,150 functions byte-identical) | PASS (10,150 byte-identical; `<workdrive>/gf-StallRun-scratch/gen_merged.log`) |
| ledger --check | PASS (47,642 rows) | PASS (pre-merge tree) | PASS (47,642 rows) |

Smoke tripwire: no crash, hang or zero-pass case; determinism holds. Its tick count moved a lot (50,435 -> 29,807). That is the trajectory change, not tuned to.

## 4. The regression: the ankle test (isolated)

- Test: `PesThrowInRelease.TheThrowInTakersAnklesStayBehindTheLine` (PES rule FUN_140792E10: the taker's nearer ankle stays at least F[1] + 0.1 m behind the line, persistently across taking ticks).
- On a2439354 it fails: 1 persistent short reading of 61 taking ticks, worst 0.009 m.
- **Isolation (one variable):** on a2439354 with the start-time line restored to `frameNum = 0` and nothing else changed, the test passes (636 s). So the StallThink merge is not the cause; the start-time change is.
- StallLive13 reports 43e96723 (StallThink, no StallRun change) passes the same test. Consistent.
- Interpretation (INF): the throw-in taker's walk or step clips now start mid-clip, which moves the ankle about 9 mm at one tick. The rule is PES's and the test is correct. A proper fix is needed, not a weaker test. Next: find which clip the taker plays at the persistent tick and PES's start time for it.

## 5. Gate status of the pushed tree

- `gen_translated.py --image <unpacked pes_full.bin> --check` on a2439354: PASS (10,150 functions byte-identical). On 8ef8ae87 also PASS.
- Ledger check: PASS on a2439354 (47,642 rows).
- Untracked files (`git status --porcelain --untracked-files=all -- src tests tools docs`): clean.
- `git diff HEAD -- src tests` on the worktree: empty.

## 6. Killed or unsupported

- K1. "The RUN entry is a clip-lookup miss": killed (1.6% overall, 0.2% GOTO, 0.6% STEP).
- K2. "A locomotion family holding every Animetable motion is the fix": not supported (dodge, tackle and kick cells are not locomotion); needs MOVESTEP's row tables.
- K3. "The StallThink merge breaks the ankle": killed (section 4).

## 7. Open items (numbered)

1. The ankle regression (section 4): find the clip the taker plays at the persistent tick and PES's start time for it; fix the stand-in, do not weaken the test. Blocks merging into StallLive.
2. Motion 2891 (locomotion family gap, 207 RUN_LOOP and 207 IDLE_OTHER issues): decode MOVESTEP's locomotion row tables so the family holds the motions those rows name.
3. GOTO_RUN motion flips (about 350 changes restart the clip): compare PES's goto column choice (AngleBucket hysteresis, the `negHalf` facing test) with GF's facing error.
4. GOTO issue duration and start: `EnterGotoRun` issues the default duration and start 0 (movestep_run.cpp tail). PES picks 0.4 or 0.1667 s and a 0.15 s start from the anime state bytes (LAB_1406cfa8f..cfb18). Playback is the stand-in 1.0; PES reads the anime vt+0x50 into req[0x35] (dec_goto.txt, line 253). Port these.
5. CommandIdFor returns 2 for every movement (pes-re-press-cmd.md §8.1). It feeds derived precise (req[0xe]) for classes 1..3, so it affects StayDecision and the stop arm. PES's writer of cmd[0] is not decoded, so it cannot be ported yet.
6. Throw-in ceremony test (about 30 min; it timed out at 3,600 s on 8ef8ae87 under load): run with a longer limit before merging.

## 8. Next highest-value step

Item 1. The start-time fix is correct in mechanism and moves ALiveBall 2,401 -> 92 on the merged tree, but it breaks the ankle rule at one tick. Find that tick's clip and its PES start time first.

## 9. Blockers

None for tooling. Under load (about 15-22) a full throw-in suite takes over an hour; the ceremony test needs a limit above 3,600 s.
