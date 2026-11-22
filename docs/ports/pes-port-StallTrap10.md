# pes-port-StallTrap10: touch-frame fix landed (56ef1e91, red-first); PES's counter-B store is not only at touch ticks (12 captures); GF's ball samples are already filled; counter B is still 0 at 7,832 touch-tick writer calls in the real match, so the feed is not landed (StallTrap, 22-11-26)

Status: PART 1 LANDED (56ef1e91 on origin/StallTrap; ls-remote confirmed; parent 8e782040). PART 2 (samples) found already filled: no change needed. PART 3 NOT LANDED: the feed still writes 0 at every measured touch tick. Feed and probe reverted from the worktree; saved as patch (section 6).

## 1. Touch-frame fix: verified against PES, then red-first and landed (CONFIRMED)

PES evidence (12 counter-B captures, pid678718 seq392, 394-397, 399, 401, 402, 404, 407-409; each replayed under unicorn over PES's own memory; script rs/gatecheck.py):
- The writer's frames-since routine 0x140A51320 computes MI frame (+0x1198) minus the record that 0x140A3E190 returns, read at +0x10. The record is the newest kind-7 copy (E+0x180698, payload frame at E+0x1806A8). CONFIRMED by disassembly (objdump of the image) and the reads in the trace.
- Newest kind-7 frame in ALL 12 captures: 167. The MI frame at those writer calls: 168, 172, 174, 177, 179, 181, 184, 186, 188, 193, 195, 197. So the newest stamp is the frame of the last touch (its emission tick) and is not re-stamped per tick. The gap (MI minus stamp) is therefore a clock difference, not a stamping offset. CONFIRMED.
- Consequence: PES stamps a touch with the frame it is emitted in. The 1-tick difference GF had (queue-time stamp, touch emitted one tick later) was a real mismatch with that rule. 

Red-first test (pes_event_wiring_test, AQueuedTouchIsStampedWithTheFrameOfTheTickThatRoutesIt): play to live, queue a touch in tick N, advance the frame, run Process (tick N+1). Expected newest kind-7 frame == tick N+1's frame.
- Before the fix: FAILED, newest stamp 173 (the queue frame), expected 174. (Output: "Which is: 173".)
- After the fix (matchevents.cpp: in.frame and snap.frame = the event stage frame; PendingPesTouch loses its unused frame field, match.hpp / matchevents.cpp): PASSED. pes_event_wiring_test: 5/5 pass.
- Smoke: 2-min seed-7 --pes-loop match with the fix only: rc 0, full time, determinism b0fa6734dd6accf4. Balance and idle lines are not targets (owner rule).
- Commit 56ef1e91 (3 files: tests/sim/pes_event_wiring_test.cpp, src/sim/onthepitch/matchevents.cpp, src/sim/onthepitch/match.hpp).

## 2. Ball prediction samples: already filled in GF (CONFIRMED)

- PES's FUN_140A6CF70 is an address calculator (B+0x88 + 0x28*j, j >= 250 clamped), translated in src/pes/translated/ai/pes_fn_140A6CF70.cpp. It writes nothing. The samples are written in GF: Ball::CalculatePrediction (ball.cpp:135-267) calls SetTrackSample(tick, nextPos) for each tick and SetTrackSample(0, positionBuffer). It runs on every Ball process/reset.
- Match probe (touch ticks, 30 logged): ball sample 0 = (0.00, 0.11), sample 1 = (-0.03, 0.14), sample 249 = (-3.21, 0.11). The samples are populated and move.
- So the team.cpp comment "the prediction samples 1..249 stay zero" is stale. Main's premise (GF fills only sample 0) does not hold in the match.

## 3. Counter B in the real match (CONFIRMED, probe output, match seed 7, pes-loop)

Feed under test (applied, not landed): RunCounterB per on-pitch slot after the ball is staged, gated to IsInPlay, BindSlot binds man+0x18 = slot and man+0x7E0 = ctx. Frames-since read with rcx = the man's ctx (LoadU64 man+0x7E0), as the earlier probe needed.
- 1,060,000 in-play writer calls: 7,832 at touch ticks (framesSince 0 or 2). counter B nonzero at those calls: 0.
- Logged touch-tick calls (slot 0..11 of one tick): framesSince 0, counterB 0, writer result rax 249 (the same value as PES's counter A, SR+0x1430 = 0xF9), ball samples populated, men spread across the pitch (slot 1 at 0.1 m of the ball, counterB 0).
- The match timed out at 900 s because of the probe (1M calls through the frames-since routine); the feed alone had no timeout trouble before.

Reading: the frames-since gate passes at touch ticks, so the writer is exiting or not storing on a later branch. PES's store also happens without a touch (12 captures: stores at gaps 1, 12 and 17; the gate's second flag MA+0x249D0 is 1 only at gap 1). So "store only at touch ticks" (Main's premise, round 9) is false for PES. In GF the store never happens in the first 40 touch-tick calls, which isolates the failing branch to the store path after the frames-since test.

## 4. Gate facts decoded this round (CONFIRMED by disassembly of the image)

- 0x14092EDB0 calls 0x140A51320; the result feeds `test al,al; jne 0x14092F2F9` (exit when the low byte is nonzero) at 0x14092EDC5. Then `sil` is set to 1 at 0x14092EE2E when r12d == 0, at 0x14092EE46 when r12d == 2 and team flag [*(rbp+0x77)+0x255] == 0, and at 0x14092EE5F (cmovne) from the byte at [*(rbp+0x7F)+0x249D0] (= MA(side)+0x249D0; 1 in seq392 only). The store 0x14092F204 `mov [r15+0x1438],eax` follows `test sil,sil; je 0x14092F2F9` at 0x14092EF6F/EF72.
- Open conflict (NOT resolved): the seq392 ordered trace shows the byte stored at 0x14092EDCB as 0, which implies the call's low byte is 0, but my reading of 0x140A51390 gives MI 168 minus stamp 167 = 1. One of these reads is misattributed; unresolved.

## 5. Not run / not landed

- Race test (unit world): still red (counterB 0). Not re-run after the touch fix.
- Feed not landed: it writes 0 in the match, so landing it would zero the arrival base.
- PesStallQuick, full ALiveBall, PesHang, PesPassTarget: not run (no feed to measure; the touch fix was smoke-tested by a 2-min match only).
- man+0x18: not measured in the match; writer-of-record not found. Still INF: slot key.

## 6. Artifacts (scratch, <workdrive>/gf-StallTrap-scratch/)

- round10-feed-inputprobe.patch: feed (gated) + input probe + race test, 175 lines. Probe only; not for landing as-is.
- round10-touch-fix-final.patch: the landed fix (51 lines, already in 56ef1e91).
- rs/gatecheck.py + rs/gate/: 12-capture replay (gate inputs per capture).
- sim9..sim12.out: match logs. sim11.out: the smoke run.

## 7. Next steps (exact)

1. Resolve the store path after the frames-since test: the writer's exit at 0x14092EDC5 uses the low byte of 0x140A51320's result, so check that GF's result at touch ticks is 0 (the probe prints since=0). Then find which later branch skips 0x14092F204 for GF (trace the writer in the match with a gated, bounded read log; the header read hook from round 9 forces a full layer rebuild, ~20 min).
2. Compare that branch's inputs with seq392 (captured reference): team flag [*(rbp+0x77)+0x255], MA+0x249D0, the ball/MA lists the loop at 0x14092EEB5 walks.
3. Only then re-land the feed; race test; PesStallQuick, ALiveBall, PesHang, PesPassTarget.

## 8. Confidence

- Touch stamp equals the emission frame; newest kind-7 stamp constant 167 across 12 captures: CONFIRMED.
- Touch-frame fix red-first and green: CONFIRMED (test output, 5/5 wiring tests).
- Smoke of the fix (2-min pes-loop, full time): CONFIRMED.
- Ball samples already filled in GF: CONFIRMED (probe values).
- PES stores counter B without a touch tick: CONFIRMED (12 captures, gap 12 and 17 stores).
- Counter B stays 0 at touch ticks in GF: CONFIRMED (0/7,832). Cause: OPEN (store branch after the frames-since test).
