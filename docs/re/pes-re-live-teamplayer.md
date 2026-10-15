# Live verdicts: team- and player-owned rows (LiveTeamPlayer-2), revision 3

Revision 3 supersedes revision 2. It corrects my holder-trust check (a reference-point error), adds the MI+0x1554 structure of the 116c gate, and details the four flips at MI+0x116c == 2.

## Corrections to earlier text

1. "Countdown falls exactly 1 per frame (890 of 890 intervals)": withdrawn as stated. The clean test (consecutive clean pairs, windows of at most 12 frames) gives 13 nonzero drops, all equal to the frame count, 0 violations. The fractional ratios I reported came from torn pairs.
2. "Countdown re-arms coincide with a newest-event frame (22 of 23)": retracted. It came from long windows, where events are frequent by chance. At windows of at most 12 frames there is one rise pair, with no event.
3. T+0x78 "kick-gated / set after a touch": not measured. No kick was captured (the AnimePlayer chain reads 0).
4. T+0x210 "init 2 at kickoff": not observed. Only the constant value 2 is measured.
5. T+0xa2cc "stamps the frame a slot entered a state": not supported. Stamps change with a state change in 212 of 2,211 cases (10%).
6. T+0xa548 "all 255 in play": wrong. Two short press episodes in run10 set team-1 player ids.
7. Keeper claim field "only 0 and -1": wrong. Countdown segments exist (see Extras).
8. Chance ends: 47 in runs 9-12 (not 24). Possession changes: run9 5, run10 5, run11 10, run12 2. The event log missed some changes; the valid-sample series is authoritative.
9. Holder trust ("about 11% of ticks", "UNVERIFIED because the fit fails") was a reference-point error in my sampler. The press-table check used the ball-line point (MA+0x2482c/34) instead of the sim ball. LivePoke's findma fits the sim ball. Offline re-test on 90 snapshots: the sim-ball fit is typically 8-16 of 22 (18 or more in 3 snapshots); the ball-line fit is 0-3. The remaining shortfall is probably read timing (INFERENCE: a snapshot's reads span several frames). The holder status is therefore UNKNOWN, not failed. A clean-burst test is armed (see Run status).

## Scope and method

- Match: the new match, G = 0x371d5090. Data: runs 9-12, all recorded after ClockRestore's threshold raise (epoch 1792064121.9). The raise pins no sim field, so these runs are treated as normal play. Runs 1-8 are excluded (half-time, ClockFreeze pins, dead-ball stoppage). Run7 is INVALID from 5:00 (run7/INVALID.txt).
- Exact reads only, one sampler at a time, every sampler stopped by PID. No writes to the game. Per-run headers (G, launch epoch, first valid sample) are in run*/HEADER.txt under the scratch dir.
- Time base: the frame counter (54 Hz). The HUD clock is not read.
- Torn samples: 3,874 of 3,905 consecutive valid pairs straddle a frame boundary, so a sample's frame is uncertain by about 5 frames. Rate claims use clean pairs only.
- MA (analysis object) comes from LivePoke's chain: M = *(0x1436F3F78); W = *(M+0x118); blk = *(W+0x10); MA = blk+8. It is re-resolved every 2 s. Holder, carrier and MA-table values are UNVERIFIED until a clean burst fits the press table to the sim ball at 18 or more of 22 (see item 9).

## Verdicts (17 rows)

Key: CONFIRMED = measured, coverage stated. CORRECTED = the row's premise is wrong. OPEN = measured, meaning unresolved. NOT MEASURED = no usable reading.

1. T+0x78 (fallback/last-touch id): CONFIRMED scope. 255 in 5,870 readings; a player id in 1,948, always of the same team (team 0 ids 0-10, team 1 ids 11-21; 0 cross-team). Trigger (kick or touch): NOT MEASURED.
2. T+0x210 (manual/target level): CONFIRMED constant 2 across about 3,900 consecutive valid pairs (runs 9-12) and the run7 snapshots. Init at kickoff: NOT OBSERVED. No in-match writer was seen in these windows; the menu feeder is not located.
3. Instruction pairs (row says G+0x25cf8/0x25d38): CORRECTED base. G-relative reads are out of range (for example 1052688 in every id slot). The table is read through MA (FUN_140a63950 reads ctx+0x18). MA values: ids [8,5,0,16,4,8,14,16], params [10,255,255,255,255,21,10,255] (255 = team-wide). Index 2 switches between id 0 and id 11 (4, 2 and 4 changes in runs 9, 10 and 11). Values UNVERIFIED (see item 9). Activator: OPEN.
4. T+0x28c (44-byte block): OPEN. It changes about 1,170 times across runs 9-11. The float reading is an inference from byte patterns, not a measured fact.
5. T+0xf1: OPEN. 0 in every reading; no change in about 3,900 pairs; no press arming seen.
6. T+0xa2cc (state ring): CORRECTED. The values are frame numbers. Age (current frame minus stamp): p10 = 1, p50 about 8,200, p90 about 120,600 frames. A stamp changes with a T+0xa2a0 state change in only 212 of 2,211 cases. Meaning: OPEN.
7. T+0xc12b (supporter-wanted flag): OPEN. 0 in every reading; no change.
8. T+0xc12c (supporter request): OPEN. 0; no change.
9. T+0xc48 (shape-block pointer; S+0x7b38): OPEN. Pointer 0 in every reading, so S+0x7b38 is unreachable.
10. T+0x4b40 (outlet slot): OPEN. 255 in all snapshots; no change in about 3,900 pairs; no restart with an outlet observed.
11. T+0xa548[4] (press-slot men): CONFIRMED populated by player ids in run10. Three slots were set to team-1 ids 12, 14, 16, 18 at frames 118383-118395 and 118400-118435; 255 otherwise. The 5-second snapshots miss these episodes. Link to the writer: OPEN.
12. T+0xa53c (press timer): OPEN. 0; no change.
13. T+0xa540 (press-rise stamp): OPEN. 0; no change.
14. T+0xa298 (defensive mode byte): OPEN. 0; no change.
15. T+0x29c + 0x40*s (kind bytes): CORRECTED. They toggle between 0 and 1 in play (8, 30 and 33 changes in runs 9, 10 and 11), so they are not clear in open play. T+0x2a3 = 0, T+0xc0 = 255, T+0x88d8 = 255; no change.
16. PI+0x48 (keeper-case trigger 0x18): CONFIRMED zero in all 806 readable entries at 47 Chance ends (keeper 0 readable at 46 ends, keeper 11 at 31). The 0x18 trigger was never observed.
17. rec+0xdc+4k (holder record f0-f2): NOT MEASURED. The holder is unverified and the record chain was not sampled.

## Extras

- Countdown T+0x6a4[0..10]: one decrement per frame on clean pairs (13 of 13 drops, windows of at most 12 frames, 0 violations; 29 zero-reaches consistent). The countdown is nonzero only when MI+0x116c == 1: 458 of 2,416 valid ticks at 116c = 1, and 0 of 1,493 at 116c = 2 or 3. Eight exits from 116c = 1 all read zero at the next valid tick. Re-arm values seen: 19 (run10, about frame 118516) and 25 (run11, about frame 125485).
- MI+0x116c and MI+0x1554 (new): in all 3,909 valid ticks, 116c == 1 occurs exactly when MI+0x1554 == 0 (2,416 ticks each way). Every 116c 2 or 3 tick (1,493) has 1554 in 1..5. So in these samples the 116c gate is the 1554 restart-situation gate. 116c == 1 coincides with ball motion on 2,415 of 2,416 ticks, but the reverse does not hold: 260 and 95 moving ticks have 116c = 2 and 3, and the countdown is zero on all 355 of them.
- T+0x255 possession changes (valid-sample series, frame at the change, +/-5). Team that gained possession, in order: run9 (5): 115364 T0, 116162 T1, 116834 T0, 116999 T1, 117956 T0. run10 (5): 118475 T1, 119196 T0, 120935 T1, 121015 T0, 121504 T1. run11 (10): 122160 T0, 123842 T1, 124320 T0, 124823 T1, 125488 T0, 125574 T1, 125802 T0, 126753 T1, 129332 T0, 129416 T1. run12 (2): 132196 T0, 134344 T1. Logged team-0 events by MI+0x116c: 14 at 1 and 4 at 2 (four changes exist only in the series).
- Team-0 flips at MI+0x116c == 2 (four events). In each the ball was moving and frames advanced (ph PLAY, bs MOVE), but MI+0x1554 was nonzero (a restart situation). run9 f=115369: 1554=5, 1558=0; ball about 3.5 m/s over valid ticks 115358-115381. run10 f=121504: 1554=3, 1558=1; about 18.6 m/s over 121493-121515. run11 f=124823: 1554=5, 1558=1; about 8.0 m/s over 124813-124836; newest event type 2 (team 0, slot 10) at frame 124792, 31 frames before the flip. run11 f=126753: 1554=1, 1558=1; about 15.4 m/s over 126742-126764; newest event type 5 at frame 126717. Speeds use 5 valid samples, so each carries roughly +/-25% from +/-5 frame timing.
- Holder and carrier: NOT YET VERIFIED (item 9). No holder or carrier agreement is counted as evidence. Four-step rule: one step-2 case (run11, flip samples 125483-125488, newest event type 1 by team 0 at frame 125485). MI.frame minus the event frame lies between -2 and +3, so it is 6 or more frames from 9, which clears the +/-5 rule. It agrees with the gaining team. One case is not evidence. Steps 1 and 3 are untested until the holder is verified.
- Half-time reset of T+0x255 and T+0x6a4: NOT MEASURED. No break with valid team data before and after was sampled; the team tables read 0 during the break.
- Hold effects on T+0x255 and T+0x6a4: NOT TESTABLE. No flip or nonzero countdown falls in a valid window overlapping a hold. My reads of MI+0x116c were 1, 2 or 3, and MI+0x1574 was 0, 4 or 5, so ClockFreeze's pinned tuple (1,1,0) was not reliably visible.
- Keeper claim countdown (PI+0x1308, keepers 0 and 11), six segments: 28, 16, 8, 1, 0 (run10, keeper 0); 19, 5, -1 (run11, keeper 0); 16, 10, 4, 1, stall at 1 for 289 frames, then 0 (run11, keeper 0); 11, -1 (run12, keeper 0); 25, 19, 13, 7, 2, 1, stall at 1 for 177 frames, then 0 (run12, keeper 0); 41, 35, 31, -1 (run12, keeper 11). Three segments end at 0 after a stall at 1; three end at -1 from values 5, 11 and 31. -1 is also the idle value between claims. The data do not show a clean "counts down to -1 at catch". Ball contact was not checked.
- MI+0x1554 / MI+0x1558 pairs (all valid ticks, not foul-conditioned): (0,2) 2,416; (1,1) 472; (4,0) 389; (3,1) 238; (5,1) 147; (5,0) 142; (2,1) 105. The pair (5,2) was never seen, but these windows are not foul-conditioned, so that absence is a weak constraint. MI+0x1588 was not logged.
- Lane-x (T+0x46EC + 12*slot): constant over 84 full snapshots. Team 0: [0, 9.73, -8.43, 23.36, -23.36, 0, 10.38, -10.38, 5.19, -5.84, 0]. Team 1: [0, 9.08, -9.73, 20.76, -20.76, 9.73, -9.73, 9.73, -9.73, 9.73, -9.73]. The lanes are symmetric and are not slot-ordered depth.
- Constant across about 3,900 pairs: T+0x40 = 0; T+0x44 = 2 (team 0) and 255 (team 1); T+0xc134 = 255 for both teams, including the team in possession; T+0xa298, T+0xa53c, T+0xa540, T+0xc12b, T+0xc12c and T+0xf1 = 0.
- P-AIM values (not among the 17; 74 play snapshots of 22 players, most common values): PI+0x14: 0 (1,042), 9 (6). PI+0x1c: 255 (1,043). PI+0x20: 0 (1,042), with official ids 22 and 24 seen. PI+0x1490 (receiver): 255 (1,041), 4 (4), 6 (1). PI+0x1494: 0 (753), 3 (155), 1 (106), 2 (35). PI+0x13ac: 0 (1,020), 3 (28). PI+0x13c4: 255 (1,048). PI+0x14a8: 0 (810), with frame-like stamps 95,075 (49) and 23,572 (47). PI+0x45: 0 (1,024), 1 (18). PA+8: 0 (519), 2,048 (459). PA+1 (action byte): 2 (607), 3 (269), 79 (90), 74 (42). PA+0x11c: 4.5 to 359.8 degrees.
- Kick and AnimePlayer: PM+0x7D8 reads 0 in every snapshot, and a probe over PM offsets 0x0-0xFFF found no AnimePlayer vtable pointer for any player. MOVESTEP (P+0x245c), kick aim and kick captures are therefore NOT MEASURED.

## Not measured

Half-time reset (T+0x255, T+0x6a4); hold effects on T+0x255 and T+0x6a4; kick-gating of T+0x78; rec+0xdc and holder/carrier flags (trust pending the clean-burst test); MOVESTEP and kick aim; MI+0x1588.

## Needed to close

(a) A half-time break and a kick-containing play window, with the AnimePlayer chain re-derived statically. (b) A clean-burst fit of the press table to the sim ball at 18 or more of 22, then holder and carrier per flip. The burst test is armed and waits for a live match. (c) ClockFreeze holds visible in MI+0x116c, for the hold test.

## Run status

- No samplers running. The clean-burst test (tp_burst.py, background job bg_429) waits up to 6 h for a live match, then records 150 s. Output: the scratch dir/burst2/bursts.jsonl, with per-burst frame-equality flag, sim-ball fit count, holder, carrier and team p255.
- Logs: the scratch dir/run9 through run12 (HEADER.txt per run). Run7 is INVALID from 5:00 (run7/INVALID.txt).

## Holder field check (added after PoseVeto's request)

- Raw MA holder and carrier at the two run11 flips (team-0 events). f=124823: holder 255, carrier 255; sim ball (13.66, -2.33); nearest player 7 at 1.94 m; ball-line point (18.72, 0.19). f=126753: holder 255, carrier 255; sim ball (53.75, -2.91); nearest player 13 at 16.4 m; ball-line point (0.0, 0.0). If MA is valid, neither step 1 nor step 3 has a holder or carrier to decide at these ticks. MA validity is not established.
- Holder field plausibility over valid ticks: holder is 255 in 2,468 of 3,909 ticks (63%); carrier is 255 in 2,575 (66%). Where the holder is an id, it equals the nearest player in 559 of 1,441 ticks (39%), and in 173 of 322 ticks with the nearest player within 2 m (54%). A valid ball-owner field would be expected to match the nearest player more often when the ball is within 2 m, so this is not yet validation of the holder and carrier reads.

## Lag-fit regrade of holder and carrier (live, added)

- Method: LivePoke's lag rule (tp_lagfit.py). Per frame, the press table at frame f is fitted to the positions at frame f-L (L = 0..40), per-player tolerance max(1, 2%). Sampling was per frame, about 34 Hz over the window; torn frames were skipped (413 of 5,104, 8%).
- Live window (the match came live after an 80 s wait; 150 s recorded): 4,639 fitted frames. Best fit: 22 of 22 on 3,475 frames; 18 or more on 3,749 (81%). Best lag: 0 on 2,634 frames, 1 on 926, 2 on 610, 3 on 306, 4 on 110, 5 or more on 35.
- The 671 zero-fit frames form one run (frames 187839-189057) with the sim ball at the sentinel position (0.0, 100.0), a stoppage in which the table is not fitted. Trust applies outside such stoppages.
- Holder and carrier on trusted frames (fit at least 18, n = 3,749): holder is 255 on 2,171 (58%); carrier is 255 on 2,335 (62%). Most frequent holder ids: 9, 4, 16, 5, 12, 21. Most frequent carrier ids: 6, 9, 21, 3.
- Team-0 possession flips in the window: 184355 to 184356 (team 0 gains): fit 22, lag 1, holder 1 (team 0), carrier 255. Trusted, and the holder's side agrees with the gaining team. 187575 to 187577 (team 0 loses): fit 14 at the flip frame, untrusted; holder 255, carrier 255.
- Only two flips fall in the window, so the flip-level check is not yet a test of the four-step rule. A 10 Hz capture cannot apply the lag rule: lag 0 alone is the best lag on only 57% of frames.

## Per-frame capture and restart-rule test (added)

- R-test on existing valid ticks (runs 9-12, PoseVeto decode sec. 29): R active on 1,493 ticks; p255_0 == (1558 == 0) on 1,491. Exceptions: run9 f=115364 and run11 f=124819, both torn and 4-5 frames before the flip (sampling lag at the boundary).
- Run11 f=126753 (116c 2, 1558 1, 1574 3): R active, predicts 0; observed 0 (torn tick). The other 116c == 2 flips also agree: 115369 (predicts 1, observed 1), 121504 (predicts 0, observed 0), 124823 (predicts 0, observed 0).
- Per-frame capture, 600 s live (lag2/frames.jsonl): 23,601 frames, 1,754 torn. Trusted share: 89.7% of fitted frames (fit at least 18; 22 of 22 on 82%). Flips: 26 team-0, all at fit at least 18. R-state and the ranked heads were not logged, so the four-step verdict per flip is undetermined.
- Conditional on R inactive (NOT verified): step 2 explains 5 flips (5 agree); step 1 explains 4 (3 agree, 1 disagrees); 3 flips have no step; 14 are step-3 candidates with unresolved cadence. For the 7 step-3 candidates with no carrier and no step-2 kind, (first new-value frame - newest frame - 2) mod 5 gives {1: 5, 2: 2}, and with the frame minus one it gives {0: 5, 1: 2}. No single residue.
- Needed: one more bounded per-frame capture with MI+0x116c, 0x1554, 0x1558, 0x1574 and both heads (u32 at MA+0x24840 + 0x58*t) logged per frame.

## Final per-frame capture (lag3) and four-step grading (added)

- Capture: tp_capture3.py, 10 min after the match went live (live after 326 s), one process, per-frame exact reads. Output lag3_frames.jsonl in the scratch root. 27,247 frames, 2,218 torn, 24,765 fitted; trusted share (fit at least 18) 88.8%.
- Fields per frame: MI+0x116c, 0x1554, 0x1558, 0x1574, raw MI+0x119c; both heads (u32 at MA+0x24840 + 0x58*t); carrier; holder; both p255; the LivePoke fit; the newest event (kind, team, slot, frame, sub-kind) and raw words (u32 at +0x1c, u32 and byte at +0x2d, u32 at +0x40).
- Team-0 possession changes: 48, of which 47 at fit at least 18. Flip table: lag3_flip_table.md; full fields: lag3_flips4.json (scratch root).
- Convention A (state at the first new-value frame): restart-rule R 5 flips, all agree; step 1 6, all agree; step 2 4, all agree; step 3a 0 (gate never true); step 3b candidates 30, all agree (the gaining team's pre-flip p255 is 0, so the other head's +0x1470 flag applies and is not evaluable); no step 3 flips (256722 heads-invalid, 258344 and 278762 no holder or carrier).
- Convention B, exact state (record at f-1 present; 22 flips): all 22 agree (R 1, step 1 3, step 2 4, step 3b candidates 14).
- Convention B, gap state (record at f-1 missing; 26 flips, state from the previous record 2-4 frames earlier): 6 disagreements at a gate-evaluable 3b: 255510, 271925, 275423, 276286, 278762, 282733. Unresolved: the holder or the R-state can change within the gap.
- Step 3a gate (raw MI+0x119c < 162): true in 5.9% of valid frames, but in no decision state of the 48 flips (minimum raw value 231). Step 3a is not graded in this capture.
- Step 1 disagreements: none. Confirmed defects: none. Candidate defects pending exact-state coverage: the six gap-state disagreements above.
- Limits: the 2-4-frame gaps are missed or torn frames. The 3b verdicts rest on the untestable +0x1470 flag. The 3a result needs flips where the raw MI+0x119c is below 162.

## Correction to the grading (supersedes the six candidate defects above)

- The six "gap" disagreements were a grader error. The earlier grader discarded non-torn records whose team p255 read was empty, so it used a state 2-4 frames older than the decision state. Regraded with every non-torn record (tp_flips5.py; lag3_flips5.json; table lag3_flip_table.md).
- Exact f-1 state: 41 of 41 flips agree. Seven flips use the nearest non-torn record instead: six agree; 276286 disagrees on that record, but its torn f-1 frame (276285) already shows R active (116c 2, MI+0x1554 4), which predicts the observed value.
- The six, with the state that decides each (offset from the new-value frame):
  - 255510 (T1 gains): holder 12 (team 1) at 255509, one frame before. 3b-candidate; agrees.
  - 271925 (T0 gains): carrier 0 (team 0) at 271924, one frame before. Step 1; agrees.
  - 275423 (T0 gains): holder 2 (team 0) from 275420, three frames before. 3b-candidate; agrees.
  - 276286 (T1 gains): R active from torn frame 276285, one frame before (116c 2, 1554 4, 1558 1, 1574 4). R predicts team 0 = 0; agrees.
  - 278762 (T1 gains): holder 12 (team 1) and newest event kind 1, team 1, slot 11, frame 278761, one frame before. Step 2 applies (D = 0); agrees. The no-step result at the flip frame comes from the newest event being overwritten by kind 13 at 278762.
  - 282733 (T1 gains): R active from 282731, two frames before (116c 2, 1554 1, 1558 1, 1574 3). R predicts 0; agrees.
- Candidate defects: none. Each flip follows its deciding input (holder, carrier, event or R) by 1-3 frames. That is consistent with the latch reading state written in the previous tick (a holder set later in the same tick would give a one-frame offset). The 2- and 3-frame offsets need the latch cadence, which is not established.
- Step 3a: not graded, gate never true (raw MI+0x119c < 162 at no flip state; minimum 231).
- Non-exact flips (nearest record 2-4 frames older; all agree): 255656, 260596, 264207, 264270, 272920, plus 276286 and 282733 above.
