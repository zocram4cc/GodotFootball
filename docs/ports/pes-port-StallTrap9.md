# pes-port-StallTrap9: the kind-7 touch route is not the staleness (it routes every queued touch); the writer's gate was off by one tick, fixed in a patch; counter B still 0 at the gate's hit ticks; nothing pushed (StallTrap, 22-11-26)

Status: NOT LANDED. origin/StallTrap = 8e782040 (ls-remote confirmed, unchanged this round). Worktree reverted to 8e782040, build/ removed. The fix and probes are saved as patches (section 6), not committed, because the touch-frame fix changes event timing and PesStallQuick, full ALiveBall, PesHang and PesPassTarget were not run on it.

## 1. The "172 frames stale" reading was a sampling artefact (CONFIRMED)

- Earlier I logged the first 60 in-play writer calls. Those were at the start of play, right after the first touch (frame 171). framesSince 172 meant "172 frames after the first touch", not a route gap.
- Touch probe (GF_EV_LOG, 80 touches logged, match seed 7 pes-loop): every queued touch was routed; DetectBallEvent rejected 0. Touches continue through the match (frames 8893, 8913, 10252, 12491; many on consecutive frames). Action kinds 1, 2 and 7.
- So GF does emit kind-7 events for touches; PES's kind-7 writer is the same ball-event message (docs/re: kind 7 = FUN_140A3DC40 copy to E+0x180698).

## 2. The real gap: the writer's gate was one tick late (CONFIRMED by histogram after the fix)

- Process(): ProcessBallStage, then ProcessPesEventStage, then the team stage (RunPesPassTarget / the writer). GF queues a touch in the team stage (Team::SetLastTouchPlayer -> QueuePesTouch), so it is routed in the next tick's event stage.
- The event was stamped with touch.frame (the queue-time frame, one tick old). At the writer, MI+0x1198 is one more, so framesSince = 1, and the writer's gate (0 or 2) fails. The histogram before the fix: framesSince 0..6 all 0, every call "other" (that histogram was taken with the wrong gate argument; see 3).
- Fix (patch, 25 lines, matchevents.cpp): in.frame and snap.frame = the event stage's frame. After the fix, framesSince is 0 on 4,004 calls and 2 on 3,916 calls out of ~1.04 M in-play calls (probe, match seed 7, pes-loop). Those hits are the touch ticks.
- Consequence: the event is emitted one tick after the touch is detected (the queue). PES detects and emits in one tick. The fix only removes the stamp mismatch; the one-tick delay of the queue remains (OPEN: move the touch queue into the ball stage, or emit the touch in the team stage).

## 3. Probe bug found and fixed on the way

- framesSince must be read with rcx = the man's ctx pointer (man+0x7E0, LoadU64), as the StallTrap5 probe did. rcx = ManVa gives an out-of-range value (every call "other").

## 4. Counter B still 0 at the hit ticks (CONFIRMED)

- At framesSince 0 the logged calls (first 40, slots 0..7 of one tick) show rax 249 and counterB 0 after the writer. So the writer returns without storing counter B although its frames-since gate passes.
- The store (0x14092F204 `mov [r15+0x1438],eax`) is behind further gates (the AI+0x8D exit and the `sil` arm, StallTrap5). Not yet decoded for this path.
- Candidate (INF, not tested): GF's ball prediction samples 1..249 are zero (match.cpp: RunPesPassTarget sets only sample 0; "ponytail: the prediction samples 1..249 stay zero"). The writer's search reads them for frames-to-ball; with zeros it finds no ball and stores nothing. Test: set samples 1..249 from GF's Predict(j) and re-run.
- The probe run also hit the 900 s limit (1M probe calls), so no end-of-match counts.

## 5. man+0x18 (not measured this round)

- Not read in the match. The writer-of-record for man+0x18 (a constructor store) was not scanned. PES's event key: the touch key written from PesSlotOf (pesevents.cpp:715). Still INF: slot.

## 6. Artifacts (scratch, <workdrive>/gf-StallTrap-scratch/)

- round9-touch-frame-fix.patch: the event-stamp fix (matchevents.cpp), 25 lines. Candidate to land with its tests.
- round9-feed-frame-probe.patch: feed (gated to in-play) plus the bounded writer probe and CBHIST, 199 lines. Probe only; not for landing as-is.
- feed9-gated-probe.patch: the earlier feed (pre-round-9 probe).
- sim5..sim10.out: match logs; sim7.out: touch probe (80 touches).

## 7. Not run

- Race test (red in the unit world; not re-run after the fix).
- PesStallQuick, full ALiveBall, PesHang, PesPassTarget, gen --check (no translated change this round).

## 8. Next steps (exact)

1. Test the sample hypothesis: fill ball prediction samples 1..249 from GF's Predict(j) in RunPesPassTarget (as PES's FUN_140A6CF70 does), run the pes-loop match once with the frame fix and the feed, check counterB > 0 after touches (probe at framesSince 0/2, not 1M calls).
2. If counter B becomes nonzero and PES-shaped: land the frame fix + feed + sample fill, run race test, PesStallQuick, ALiveBall, PesHang, PesPassTarget, gen --check.
3. If not: decode the writer's store gate from 0x14092EDC5 / 0x14092EF72 on this path (with the captured pre-state of seq392 as the reference).

## 9. Confidence

- GF routes every queued touch as kind 7: CONFIRMED (touch probe, 80/80).
- Writer gate is framesSince 0 or 2: CONFIRMED (earlier probes; histogram after fix).
- The off-by-one is the stamp, fixed by the 25-line patch: CONFIRMED by the histogram (before 0 hits, after ~7,900 hits).
- Counter B stays 0 at the hit ticks: CONFIRMED (probe). Cause: OPEN (sample hypothesis INF).
- man+0x18 = slot: INF.
