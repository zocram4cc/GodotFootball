# pes-port-StallAnkle: the throw-in ankle failure is a mid-clip issue at frame 10; a root anchor for it did NOT fix the test (StallRun, 21-11-26)

Status: NOT LANDED. Reverted. No commit pushed. Worktree `gf-StallRun` is at **a2439354** (= origin StallRun), clean: `git status` empty, `git diff HEAD -- src tests` empty. Build dir deleted. The ankle test is still RED on StallRun, so StallRun still cannot merge into StallLive.

## 1. Red, reproduced (on a2439354 + diagnostic)
- Binary: built from a2439354 with a temporary probe print (reverted). Run: `pes_throwin_release_test --gtest_filter=PesThrowInRelease.TheThrowInTakersAnklesStayBehindTheLine`, `PES21_ROOT=<workdrive>/PES21`, `PES_EXE=<home>` (**PES_EXE must be set at run time**, not only at configure; without it the test stops at "install constants unavailable" in 4 s).
- Result: "1 of 61 taking ticks kept the nearer ankle short ... worst by 0.00923 m", 537 s. Matches StallRun2.
- Failing tick (persistent short, 0.0040 m at that print): **anim id 1203 = `assets/gf_anims/pass/idle/000.anim`, frame 10 of 73, rate 1.0, root-jump(frame-0) 0.0298 m**. Frame 10 = 0.10 s at the 10 ms clip frame, i.e. the start time of this issue. The start time is the MotionStartSeconds table entry (constant at 0x142588200, value 0.1 s; the same address the ankle test reads as its 0.1 m margin).
- Log: `gf-StallRun-scratch/ankle/red.log`, binary `ankle/red_test`.

## 2. Hypothesis tested: the body is displaced by the skipped root offset (REJECTED)
- Mechanism (code, CONFIRMED by reading): `HumanoidBase::PlayIssuedClip` sets `startPos = spatialState.position` and `Process()` applies `startPos + positions[frameNum]`. A clip issued at frame k therefore moves the body by `positions[k]` (the clip's root offset at that frame). The idle path (`ResetPosition`) anchors instead at `newPos - positions[k]`. The frame-0 convention is `positions[0]` ~ origin.
- Change tried (on a2439354): `startPos += positions[0] - positions[k]` for k > 0, and the predicted end shifted by the same delta, so a mid-clip issue applies `pos + positions[0]` on its first frame, like a frame-0 issue.
- Result: **worse.** `persistent short`: 49 of 137 taking ticks (was 1 of 61), worst 0.960 m. The run diverged: 137 taking ticks vs 61. The failing clip became `highpass/idle/000_long.anim` (id 1022), whose root travels about 1.4 m over its frames (root-jump 0.08 to 1.38 m), so the taker drifts far off the spot. The diagnosis of the body jump at frame 10 is real but it is not the cause of the test failure: the same clip's own travel moves the body in both versions, and the anchor only removes a 0.03 to 0.08 m initial offset while changing the whole match.
- Verdict: the anchor is not the faithful fix, and the test does not pass with it. Reverted.
- Log: `ankle/green.log` (run with the anchor), binary `ankle/green_test`.

## 3. What this leaves open (for the next round; not decided here)
1. Which clip PES plays for this taker at that moment, and whether PES applies a 1.4 m root travel for `highpass/idle/000_long` (id 1022) at all. In GF the taker's idle-hold clips carry large root travel; PES's taker holds the spot. Decode PES's SetMotion for the motion (the table row that names 1022/1203) before touching positions.
2. Whether the 0.1 s start of motion X (`MotionStartSeconds` case 0x23/0x26/0x46/... -> `k.v588200`) is the right start for the throw-in taker's idle clip. The start constant and the ankle margin are the same float (0x142588200); confirm that PES reads it as a start time for this motion, not the ankle margin.
3. Unit check: the frame unit is 10 ms (`pesclips.hpp:60`, `humanoidbase.cpp:504` playback, `teamAIcontroller.cpp:3560`). Consistent; not the cause.

## 4. Gates that were run (on the tree before revert; the revert restores HEAD)
| Check | Result |
|---|---|
| Ankle test, red (a2439354) | FAIL: 1 of 61 ticks, 0.00923 m (537 s) |
| Ankle test, with root anchor | FAIL: 49 of 137 ticks, 0.960 m (751 s) |
| gen_translated.py --check (image pes_full.bin, tree with the anchor; no translated file changed) | PASS: 10,150 functions byte-identical (`gf-StallRun-scratch/ankle/gen_check.log`) |
| PesStallQuick, PesStall.ALiveBall, PesHang, check_pii, push | NOT RUN: nothing to land |
| ALiveBall count | not re-measured in this round; StallRun2 measured 92 on a2439354 |

`origin/FastStall` (88ae7714) was fetched and inspected, not merged (nothing to land).

## 5. Ownership note
Only `humanoidbase.cpp` (PlayIssuedClip) was touched, and only in the tried-and-reverted anchor. No `Think()`/`MovementDecision` (StallBusy's area) was changed.

## 6. Next highest-value step
Item 1 of section 3: decode PES's clip for motion 1022/1203's table row and its root travel. The start-time line itself is still the right mechanism (it moves ALiveBall 2,401 -> 92); the throw-in taker's clip choice or its root travel is the open part.

## 7. Blockers
None for tooling. The ankle test needs `PES_EXE` at run time; a run takes 9 to 13 min under load.
