# pes-port-StallSlide: a committed slide or trip plays its clip to the last frame and re-decides there; the forced count is time-to-touch, not the clip end; no rule landed, tree reverted (StallSlide, 21-11-26)

Status: REVERTED, NOTHING LANDED, NOTHING PUSHED. Branch `StallSlide` (worktree `<workdrive>/gf-StallBusy`) was `a2439354` (StallRun), merged `origin/FastStall` 88ae7714 (local merge 745c7d73, test-only quick target), added a diagnostic trace (`census-trace.patch`, 53 lines, scratch only), then reset to `a2439354`. The reset dropped only my own unpushed merge and the trace. Build dir removed. `extern/godot-cpp` symlink restored.

## 1. Measurement (seed 7, tree = a2439354 + FastStall test, GF_STALL_DUMP=1)

| Run | Result | Log |
|---|---|---|
| PesStall.ALiveBallNeverLiesStillLongEnoughForTheBreaker (full match, with per-tick challenge trace) | **92** forced live-ball ticks, FAILED (identical to StallRun, StallBusy, StallBusy2) | `gf-StallBusy-scratch/census3-full.log` (11,140 lines) |
| PesStallQuick (first 240 s) | 58 (cited from `pes-port-StallBusy.md` on the same tree; not re-run this round, no change to measure) | `census-base.log` |

Build: `pes_restart_delay_test` (gate env set). The trace is env-gated under GF_STALL_DUMP and was not committed.

## 2. Census of challenge bodies (every tick a body is in Sliding or Trip, all men, full match)

- 10,682 challenge-body ticks; 200 challenge endings (STALLCHALEND).
- Endings by (from, to, decision at the tick the clip ends):

| from | to | decision | count |
|---|---|---|---|
| 12 Trip | 1 Movement | 1 Movement | 114 |
| 13 Sliding | 1 Movement | 1 Movement | 46 |
| 12 Trip | 1 Movement | 14 Special | 17 |
| 13 Sliding | 1 Movement | 14 Special | 15 |
| 12 Trip | 14 Special | 14 | 5 |
| other (12 to 6, 2, 11) | | | 3 |

160 of 200 (80%) end in Movement on the clip's last frame. No challenge ended before its last frame in the traced episodes.

## 3. The two forced episodes, tick by tick

**Slide, man 19 (team 1), t 513388 to 514481 ms.** Clip `assets/gf_anims/sliding/walk/000.anim`, 60 frames.
- Frame 0 starts with decision 13 (Sliding), body 2.44 m from a ball at rest. The reaction is the slide, not the breaker.
- Decision is Movement on frames 1 to 18, Special on 19 to 28, Movement on 29 to 58. No Sliding decision re-fires while he slides (0 of 58 ticks).
- The breaker fires on frames 0 to 28. Frames 8 to 28 are the 21 live-counted firings; frames 0 to 7 fell outside the live bucket.
- The breaker stops at frame 29 with the decision still Movement. INF: the idle timer resets when the ball moves more than 2 cm (`match.cpp:947`), so the slide's contact with the ball ended the burst at about 0.5 s, long before the clip ended.
- Frame 59 is the last frame. The next tick reads fn 1 (Movement), decision 1 (t 514481). No get-up action.

**Trip, man 8 (team 0), t 52703 to 53148 ms.** Clip `trip/trip_t1/walk/000_protectfront.anim_mirror`, 25 frames, issued by contact (decision 1 at frame 0).
- The breaker fires on every tick, frames 0 to 23. The ball never moved for the whole 24-tick clip (INF from the continuous firing count), so this trip ended no burst.
- Frame 24 is the switch: fn 1 Movement, decision 1, t 53148.

## 4. GF code (CONFIRMED by reading)

- `humanoid.cpp:414-417`: the clip switches (`e_InterruptAnim_Switch`) on its last frame, unless `PesPoseHeld` or a throw-in taker owns the body.
- `humanoid.cpp:478-504`: a requeue (mid-clip replacement) is allowed only over a movement clip (`IsMovementClip`) or a pending touch on a trap or ball-control clip. A Sliding or Trip clip is never requeued, so a Movement decision cannot interrupt it. This matches the census: 0 interruptions in 10,682 ticks.
- So GF's rule is: a slide or trip is committed to its last frame, and the next decision applies at the switch. There is no get-up action in GF.

## 5. PES decode

- **`FUN_140787EC0` (VA 0x140787EC0, Sliding action +0x70 transition answer; headless `dec.sh`, output `gf-StallBusy-scratch/dec_slide_end.txt`), CONFIRMED structure.** Reads the phase byte through the secondary's vtable slot +0x20 (`anime+0x245C`). If the byte is `0x04`, it takes track 0, otherwise the last track. Returns `selectedTrackFrames <= sec+0x38`, where `sec+0x38` is the current clip's frame count. So the slide's exit test is a frame comparison, not a timer and not a frame-of-clip test.
  - **Correction:** `docs/re/pes-re-getup-recovery.md` §4 and §7 say the Sliding test branches on `== 2`. This decompile says `== 0x04` for FUN_140787EC0. The doc needs the correction.
  - **OPEN:** this function answers "may the slide be left", but its caller (the per-tick action switch that calls the action's +0x70) is not decoded. So whether PES lets a Movement request end a slide before its last frame is not settled.
- **`FUN_140A6E0D0` (VA 0x140A6E0D0), CONFIRMED:** returns `(word & 0x2C0) != 0` for action < 0x5e, word = class table at `0x1426484F0 + 4·action`. This is the slide's own-action gate (bits 6, 7, 9).
- **Correction to `pes-port-StallBusy2.md` §2:** GF's `_SlideCandidate` does refuse its own Sliding and Trip. Its last line is `return current != e_FunctionType_Sliding && current != e_FunctionType_Trip;` (`elizacontroller.cpp:1461-1462`). What it lacks is PES's extra classes: tackle, stagger and fall (bit 13 via FUN_140A6E3C0), and the trap bit 5 via FUN_140A6E5E0. None of the 92 forced firings was in those classes, so this gap has no effect on the count.

## 6. Verdict and why nothing was ported

- The forced count is time-to-touch. A burst ends when the ball moves (`match.cpp:947`), so each forced episode lasts until someone touches the ball.
- The slide episode ended by touch at frame 29, well before its clip's last frame, so the committed slide did not waste the count. A Movement interrupt would only change the slide's time-to-touch, and I have no PES evidence for the direction.
- The trip episode is the one case where a committed clip carried 24 idle ticks to its end. Whether PES releases a trip early is OPEN (the caller of the Trip action's transition, §5, is not decoded).
- The predicted port (a Movement interrupt of a committed slide or trip) has no PES evidence behind it, so under the land-or-revert rule it is not landed. No red test written: a test would pin GF's current behaviour, not a PES one.
- The other 56 of the 92 forced ticks are MOVE/TRAP bodies beside a resting ball (StallBusy bucket 1). They are outside this round's scope.

## 7. Checks run

- ALiveBall full: 92 (identical to baseline).
- `PesHang`, `PesPassTarget`, `MoveStepWire`, `gen_translated --check`: not run. No change is pushed and the tree equals `a2439354`, which the StallRun round already ran.

## 8. Open items (numbered)

1. Decode the caller of the Sliding and Trip actions' +0x70 transition (`FUN_140787EC0`, the Stagger/Trip answer `FUN_14072E450`) to settle whether PES releases a slide or trip before its last frame. This is decisive for the 24-tick trip case.
2. INF: the slide's burst ends by the ball moving at frame 29. Confirming it needs a ball-position trace over the episode (a diagnosis-only round; needs Main's OK per the owner rule).
3. The own-action gate (FUN_140A6E0D0, §5) should still be ported for tackle, stagger and fall classes, but only as part of a round that has a count to move.
4. The per-tick challenge trace (`census-trace.patch`) is a diagnostic in `pes_stall_test.cpp`. It stays out of the tree unless Main asks for it.

## 9. Scratch

- `gf-StallBusy-scratch/census3-full.log`: full ALiveBall with the trace (92 forced, 10,682 challenge ticks, 200 endings).
- `gf-StallBusy-scratch/census-trace.patch`: the trace diff.
- `gf-StallBusy-scratch/dec_slide_end.txt`: `dec.sh` output for FUN_140787EC0, FUN_140A53690, FUN_140A6E0D0.
