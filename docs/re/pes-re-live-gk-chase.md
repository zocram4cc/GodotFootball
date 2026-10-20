

> **Main note 20-10-26:** live, partial (rounds 32-33). Settled: no single-frame teleport of the keeper during restart holds (max 0.34 m/frame), consistent with SetKick writing only a target (pes-re-goalkick-taker.md). Killed: the guess that the man word +0x5D0 = 0x36 comes from the team setter FUN_140A42940 / T+0xA2A0 (they agree in 38/181 far man-frames; 0x36 never appears in the table). Open: the 0x36 writer, the goal-kick ladder unit object, the stand-point distance.
## Round 33 (S1Watch)

### (A) Keeper walk vs teleport, from gk2/restart.jsonl (man 0, two goal-kick restarts, 31 rows each)

Method: consecutive recorded rows, speed = step / frame gap (rows are not every frame; gaps 1–760 frames).

Restart 0 (exit ball at the minus line, frame 1130667):
- Keeper row-pairs with motion: 3 of 30. Pair 1130667→1131427 (760 frames): 2.09 m, 0.003 m/frame (no evidence of continuous walk). Pair 1131442→1131455 (13 frames): 1.17 m, 0.09 m/frame. Pair 1131474→1131477 (3 frames): 0.13 m, 0.043 m/frame.
- Maximum measured speed 0.09 m/frame (about 4.9 m/s at 54 fps). No step over 2.1 m between frames close enough to read as a single frame.
- Distance to ball when the ball moves: ball starts 27.0 m from the keeper at frame 1131427 and moves away (ball z −28.2 → −21.9 by 1131442, 0.6 m/frame); keeper stays static at (−52.06, −1.22) until 1131442.

Restart 1 (exit ball at (−52.8, −4.9), frame 1135148):
- Keeper row-pairs with motion: 4 of 30. Pair 1135148→1135571 (423 frames): 7.98 m, 0.019 m/frame. Pair 1135580→1135586 (6 frames): 2.04 m, 0.341 m/frame (the fastest measured step, the strike run). Pair 1135586→1135607 (21 frames): 0.23 m, 0.011 m/frame. Pair 1135633→1135651 (18 frames): 0.16 m, 0.009 m/frame.
- Distance to ball when the ball starts moving (1135571): 1.68 m. The keeper is near the ball in this window, not 27 m away.

Reading: the keeper is static for long spans and then moves in short bursts. Speeds of 0.09–0.34 m/frame are walk-to-run rates, and the 2.1 m and 2.0 m steps each span 6–760 frames. The restart rows are too sparse to call the 2 m step a per-frame teleport or continuous walk; the data does not show a single-frame jump of more than 0.34 m/frame of measured motion. Verdict: NO teleport observed in these rows; the step-by-step walk cannot be resolved below the row gaps. [INFERENCE: consistent with the clip-root carry in pes-re-goalkick-taker.md, but not confirmed.]

### (B) Team-state table and the 0x36 entrant (ts1/far.jsonl 60 far frames, ts1/events.jsonl 486 entries, 600 s recorder)

- **T+0xA2A0[slot] does NOT match the man's +0x5D0 in general.** Over 181 man-frames on far frames: equal 38, different 143. Table values on far frames: 0 (161), 54 (37), 58 (7), 50 (5), 37, 30, 34, 1 (1 each). **0x36 never appears in T+0xA2A0** in these far frames, while man +0x5D0 = 0x36 occurs many times. So under my mapping (side = `G+(side+0xA1)·0x28`, slot = man % 11, team = man // 11), the 0x36 value on `+0x5D0` is not the table entry. Either the side/slot mapping is wrong or +0x5D0 is written from a different source (the 0x14095E890 TeamData copy). This contradicts the premise that +0x5D0 = 0x36 reflects FUN_140A42940's T+0xA2A0 write. Not resolved here.
- **Who gets 0x36:** 0x36 entries per the events log: 486 total (`+0x5D0` changing into 0x36). Entry distance to ball: median 20.4 m, min 0.8, max 75.4; 130 entries at ≤13 m, 356 at >13 m. Entries come mostly from state 1 (264) and 17 (152), then 51, 50, 61, 52. The entrant is NOT the nearest man in general: on far frames, several men hold 0x36 at once (for example at frame 1143810 men 1–5 at 20–28 m while the nearest was man 7 at 17.6 m). Only 1 of 2 first-entry events on far frames was the nearest man.
- **Stable nearest man (5 frames):** rarely holds in play (False on 59 of 60 far frames). So "the designated man" cannot be reliably chosen by nearest-with-stability in this match.
- **Does the 0x36 man run at the ball?** Not measured: far frames are too sparse (60 in 600 s) for a per-man distance trend across consecutive frames, and the events log stores distance only at entry. Verdict: OPEN.
- Ball state word B+0x4C at entries: 0 (354), 1 (132).

Writer named? No. The live hook for FUN_140A42940 was not attempted: this round is read-only (no breakpoint and no code patch; a perf watch on T+0xA2A0 is possible but the side/slot mapping must be fixed first).

### (C) Goal kick ladder and stand-point distance: NOT MEASURED

- Ladder documented (pes-re-goalkick-taker.md): `sub_142136F00(unit, ·, ·)`; `unit+0x40` = elapsed counter, `unit+0x44` = start tick, `unit+0x48` = phase word. Phase entries at F+54, F+81, F+82 (default MI+0x14C4 == 0).
- **Unit object not located.** The recorder does not hold the unit pointer, and `sub_142135C50` could not be decompiled by address (the bridge returned "no function found"). Without the unit I cannot read the ladder words.
- No goal kick occurred in the team-state window (ts1 ran 600 s with no restart window logged), so no ladder or keeper stand-point data was recorded.
- Stand-point distance (~4.63 m behind the ball along the spot heading): not computed; spot heading not recorded.

### Status
- (A): done from existing data; no teleport observed; walk/run not resolvable from sparse rows.
- (B): partially done; table/`+0x5D0` link contradicted under current mapping; entrant identity is not "nearest"; running behaviour unmeasured.
- (C): blocked on unit-object location and on a goal-kick occurrence.

Next: fix the side/slot mapping by checking which side index holds the 0x36 men (a read of both sides' table for 0x36 across a longer run), then a per-man distance trend recorder for the entrant; locate the unit by a write watch on the phase word once a goal kick starts.
