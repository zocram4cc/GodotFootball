# Live read-out: MatchBall rows (LiveMatchBall-2)

Status: event-ring histogram COMPLETE (run 5: 912.3 s in play, 10,694 records). One-off row probes done. W-A3 run (run 6, 1500 s) COMPLETE: no receiver id observed (see Run 6). Reads are exact and read-only; one sampler process at a time (lmb2.py, 10 Hz).

## Runs (what is valid)
- Runs 1 to 4: discarded (run 1 had a skip-branch bug giving 0 records; runs 2 to 4 restarted to add fields). Their data is not used.
- Run 5 (out/lmb-owner-5.jsonl, 1500 s, valid): histogram, census, K=27 gate, mask, rows, MI fields.
- Run 6 (out/lmb-widen-6.jsonl, 1500 s, valid; 797.7 s in play): team Chance (0x1e) states (346 change events; 134 episodes, duration min 0.10 s, median 1.30 s, max 10.80 s; team 0 on 103, team 1 on 158) and G+0x180 rows with +0xd and +0x20 at Chance on. Result: 288 readable row entries in Chance snapshots, all (+0xd, +0x20) = (0, 255). No row with +0x20 = receiver id in any snapshot. rows_u32 (+0x10) unchanged (0, 1, 0xFFFFFFFF x22). 95 samples had unreadable rows.
- Run 7 (out/keep-7.jsonl, 1500 s, all phases, keep-tagged): the 24 G+0x180 rows never changed from (+0xd, +0x20) = (0, 255) (one snapshot at start, 0 later changes); 0 keep candidates. Also logged: team action-state changes (3410), Chance on/off (460 changes), carrier byte MA+0x2601c (528 changes; 255 = none 261 times; player ids 6..20 otherwise), MI phase/period/1554/1574/1588 (75 changes; phases 1/2/3 = 4285/135/54 events).

## Chain (verified live, exact reads)
- M = *(0x1436F3F78) (global). Holder H = *(0x1436F3F98) with vtable 0x142597580 is at M+0x788 (verified).
- Event owner D = *(X+0x18) with X = *(M+0x790). D+0x2048 = 0. D+0xb600: 223 detector slots, all non-null in run 5. D+0xbd00 tick advances about 56/s.
- Event store E = *(D+0xbd08) = 0x7ffffae70018. Constant across run 5 (1498 seconds); owner never changed.
- MA (LivePoke's chain, validated by LivePoke): W = *(M+0x118), blk = *(W+0x10), MA = blk+8 = 0x7ffffb000018. MA+0xbd08 = 0, so MA is NOT the store owner. X+0x38 is NOT MA.
- H children (+0x450/+0x458/+0x460/+0x488) are not stores.

## Event-ring histogram (run 5, phase 1 = in play, 912.3 s; thresholds-raised samples counted as normal play)
Records in play: 9,488 (all phases 10,694). Kind: count (rate per second in play)
9: 5923 (6.5/s); 7: 960 (1.05); 10: 683 (0.75); 8: 491 (0.54); 24: 361 (0.40); 25: 228 (0.25); 14: 212 (0.23); 6: 146 (0.16); 12: 82; 13: 75; 11: 54; 44: 49; 15: 48; 45: 45; 29: 35; 43: 15; 37: 20; 26: 13; 0: 21; 23: 11; 27: 11; 28: 5.
Kind census (sticky seen flags, 45 kinds seen before and during the run; K=55 never): 0,1,2,3,6..17,19,21..32,34..39,43..45,47..51,53,54. Not pushed in this store's life: 4, 5, 18, 20, 33, 40, 41, 42, 46, 52, 55.
Routing observed (A5): 7->0, 8->1, 11->2, 14->7, 23->3, 30->4, 37->5, 43->9, 31 (forced)->10; all other observed kinds ->10. K=4 (table ->6) and K=55 (table ->0) were not pushed in this window.
Write-log count E+0x271f0 range 0..5 (within 0..64). Ring counts within caps; heads advanced in every ring.

## Per-row verdicts
| Row | Verdict | Evidence |
|---|---|---|
| A4 ring-5 restart records (kind 37) | CONFIRMED MODE, team, player; point UNCLEAR | 20 kind-37 pushes in play, all ring 5. Two logged payloads: MODE(+0x18) = 1 (t=53.25) and 3 (t=92.87), equal to MI+0x1554 at the stoppages before them. Team(+0x14) 0/1; player(+0x20) 0/1; +0x1c is 0 and 4292281084 in the two cases, so not a clean point. Record written at the phase 2->1 resumption. |
| A5 K->ring table | CONFIRMED for observed kinds | 7->0, 8->1, 11->2, 14->7, 23->3, 30->4, 37->5, 43->9, 31->10 (forced), others ->10. K=4->6 and K=55->0 not exercised. |
| A6 payload layout | CONFIRMED partly | w0 kind; w1 = frame (+4); w2 = frame (+8); w3 = state byte (+0xc, 157..205 over the run); w4 = 1 (+0x10) for every kind logged; w5..w7 (+0x14, +0x18, +0x1c) kind-specific. |
| A7 busy predicate | NOT MEASURED | Not sampled as a predicate. |
| C2 MA+0x247d4 | LIVE, meaning unchecked | Probe (one-off, 1 s): 272.98 -> 262.28 -> 257.84. Changing, so the field updates in this match. Mapping to PI+0x38 heading not checked. |
| D1 MA+0x3c/0x40/0x44 line block | LIVE, mapping unchecked | Probe (one-off, 1 s): (-28.81, -46.50, -46.85) -> (-28.20, -45.34, -45.83) -> (-28.21, -43.38, -43.80). Updating. Outfield/line mapping not checked. |
| E5 MI+0x11a0 | CONFIRMED per-frame increment; NOT an event counter | 11,881 of 11,980 steps equal the frame delta; 99 unequal (not localised). Kind 27 does not write MI. |
| E6 MI+0x118c/0x1190/0x1194 stamps | MEASURED: three accumulators, NOT wall-clock | One-off probe, 58 frames in 1 s: +4656 (0x118c, ~80/frame), +4576 (0x1190, ~79/frame), +5600 (0x1194, ~97/frame). Not microseconds; the earlier ~1e5/s drift is not supported. |
| E7 MI+0x11b4 detector enable | CORRECTED: not constant; gate UNVERIFIED | 119 changes in run 5. Values 0x100, 0x101, 0x10100, 0x1000100, 0x1010100 (never u32 0). Low byte alternates 0x00/0x01. Offline: low byte 0x01 for 7 s, with ring 10 still pushing 13 records (1.86/s) against 5.66/s at 0x00. Not a hard gate on ring-10 producers. |
| E9 MI+0x1775 mode | CONFIRMED constant in play | No change in 912 s. |
| E10 MI+0x14c8/0x14cc kickoff type | CONFIRMED constant | No change in 912 s. Kickoff not separately observed. |
| E11 MI+0x11d4 (==2 staged restart) | CORRECTED | Values 0, 1, 3 (6 changes). 2 never seen over about 21 restarts. |
| E12 foul/restart scene fields | CONFIRMED values; mapping PARTIAL | MI+0x1554 (restart kind): 0..5 seen (43 changes); MI+0x1574 paired (0,3,4,5). Stoppage onsets (t, 1554/1574): 1=(9.3,3),(144.1,3),(329.2,3),(488.9,3),(647.4,3),(798.7,3); 2=(448.9,4); 3=(74.1,4),(239.3,4),(595.4,4),(711.0,4); 4=(294.9,4),(390.2,4); 5=(564.2,5). MI+0x1588 (foul kind) 0/1: 1 at 564.17 (kind-30 foul, ring 4, 1554=5), 0 at 580.28. 6 (PK) never seen. |
| E14 period MI+0x1168 | CONFIRMED 1 in play | No change in 912 s. |
| F3 ball prediction FUN_140a57800 | NOT MEASURED | Not sampled. |
| F7 BallInfo aux (+0x2798 change-int, +0x27a0 set-piece point) | MEASURED: change-int not constant; point constant | One-off probe: change-int 145 -> 81 within 0.5 s; set-piece point (-10.746, 0.109, 8.242) constant over 1 s. Earlier match 2 had 97 constant; not constant now. |
| G6 FieldInfo F9..F11 | CONFIRMED values (this match) | Block via G+0x2aa8 (block 0x1d45f024), F[9..11] = 1.109375, 2.339844, 51.171875 at block +0x28/+0x2c/+0x30 in 12 of 12 windows. (The G+0x2a90 chain returned None in the probe.) |
| G+0x2aa8 timing (FoundationWiden / FieldInfo) | CORRECTED | Not null in valid windows: alternates 0 and the block pointer per sample. The null reading was a sampling artifact. |
| H3 team tick placement | OPEN | Static: holder slot +0x10 (0x14041ecf0) calls team ticks on H+0x450/H+0x458. The tick's this T is unresolved (X = *(T+0x40)). |
| H4 detector loop -> ring writers | CONFIRMED | Detectors push into the rings live (table above; K=9 dominant). |

## PoseVeto items
1. Kind 27: NOT on MI (the count and sum are at E+0x181e00 and E+0x181dfc). Kind 27 was pushed 11 times in 912 s of play (ring 10). All 11 have +0x30 = 24 (no player), so T1 (+0x30 < 24) rejects every push. E+0x181e00 = 0, E+0x181dfc = 0.0, and the frame-like counter E+0x181df8 = 0 all run. T3 passes on every push (+0x20 in {0,1}, +0x24 in {4,8,9,12,22}). Mask u32 at P+0xc8 = 1 constant (index 0). E+0x21ab0 (seen flag) = 1 throughout (sticky; no kickoff reset observable). Rate about 0.012/s in play; no per-frame kind 27.
2. Holder vtable 0x142597580: CONFIRMED (H = *(0x1436F3F98), M+0x788 == H). Owner and store chain CONFIRMED (above).
3. K=55: flag E+0x27310 = 0 and head E+0x271f4 = 0 all run; never pushed. Write-log count 0..5.

## Corrections to earlier notes
- Kind 31 is NOT the 64-deep qword log at E+0x26ff0. That log is a per-pass (ring, slot) write log, count E+0x271f0, zeroed each pass.
- Kind-27 counts go to store block S = E+0x180c60 (count +0x11a0 = E+0x181e00; sum +0x119c = E+0x181dfc), not MI.
- The MI+0x11a0 per-frame writer is NOT the kind-27 arm.
- MA is not at X+0x38 (null).
- 0x140a6f29e is the MA constructor's zero-init (FUN_140a6f240, rdi = MA), not a per-tick writer.
- The MI+0x11b4 "constant 0x100" (match 2) does not hold; it changes 119 times in run 5.

## Open
- Restart-kind mapping (FoundationAreas): only two kind-37 payloads logged; 6 (PK) and offside (1588) not seen. Logging all kind-37 payloads would settle it.
- E7 low-byte gate: unverified (offline evidence weak, 7 s at 0x01).
- W-A3 (G+0x180 rows +0xd/+0x20): NOT OBSERVED in runs 6 and 7 (about 2,300 s, all phases in run 7). Open: a write shorter than 0.1 s would be invisible at our 10 Hz limit; or the 5a7fe0 keep does not write this table in this build. Next step if needed: static writer search for the +0x20 field of the G+0x180 entries.
- F3, A7, H3: not measured in this window.
- E6 stamps: measured in one probe only; no series.
- Unequal MI+0x11a0 steps (99): cause not localised.
