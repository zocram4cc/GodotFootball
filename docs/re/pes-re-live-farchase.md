# Far chase, LIVE (FarChaseLive, 21-10-26)

> **Main note 21-10-26:** live, one match (/biz/ vs /xs/, 2226 in-play frames). Headline: a far loose ball is chased by a man already near it, designated PRESS (0x34) by the coordinator arm (108/109) via its target eval; the 13 m gated writer refuses. DELAY (0x33) is a +0x5D0 behaviour state of MATCH_UP men, never a table write. Caveat: the team point read as a match constant (the MA gate object was not pinned).

Live read-only measurement on the running match (pid 2510857, /biz/ vs /xs/), answering the open
items in `pes-re-far-chase.md` §3/§6: who goes to a loose ball beyond 13 m of the team point,
which slot and which state (0x34 PRESS / 0x33 DELAY), from which arm, how many frames until
someone reaches it, and how often mode == 38.

**Window:** 2226 in-play frames recorded (1837 loose), frame span ~11600 (~4 min of play). The
match then ended (MI → 0) and is now unloaded; the recording is final. A concurrent positional
recorder (per-man xz) captured 0 rows (its window hit stoppage); the main recorder's rows already
carry per-man distance-to-ball, which is what the chase questions need.

## Method and object validation (re-discovered this match)

- `G = *(u64*)0x1436F3DC8`, `MI = G+0x270`, state = `MI+0x116C` (1 = in play), frame = `MI+0x1198`.
- Ball `B = G+0x40`; position = `B+0x88` (x) / `B+0x90` (z), predicted point sample 0 (the same
  sample the gate uses, per S1Watch round 11). Holder word `B+0x4C` (0 = loose, 1 = held, per
  S1Watch round 33). Ball x-range observed −52.2…52.8, z −29.1…34.2.
- Teams `T_s = G+(0xA1+s)*0x28` (s = 0,1); read per frame:
  - **team point** `T_s+0x2482C` (x) / `T_s+0x24834` (z) — see §1.
  - **assignment table** `T_s+0xA2A0[0..10]` (u32, the setter `FUN_140A42940` target word).
  - **mode field** `T_s+0xBD48[0..10]` (u32; == 38 arms the ungated sweep at 0x140536059).
- Men `pl(i) = G+(0xBF+i)*0x28`, i = 0..21 (`i%11` = slot, `i//11` = team); pos `pl+0x8` (x) /
  `pl+0x10` (z); **behavior state** `pl+0x5D0` (u32).

**Validation:** before recording, each man pointer was checked for a vtable in the image range,
position on the pitch (|x| < 60, |z| < 50), and a plausible state word. All 22 men validated. The
team records were validated two independent ways: (a) the same `T = *(G+(side+0xA1)*0x28)` formula
S1Watch's round-33 `teamstate.py` used, and (b) the recorded `T+0xA2A0` value distribution
(54/MARK and 58/MATCH_UP dominant) matches S1Watch's own `ts1/far.jsonl` `teams` histogram
(54×107, 58×21, 50×10, …) — so the team mapping is **correct** for this match.

## State names via the table at 0x1434A4D50 (script-read from the image)

8-byte string-pointer table. The full set of indices that actually appear in the `T+0xA2A0`
assignment table or the men's `+0x5D0` behavior state this match:

| idx | name | idx | name | idx | name |
|---|---|---|---|---|---|
| 0x00 | NOTHING | 0x1D | SPACE_RUN_SECOND_LINE | 0x29 | BALL_DODGE |
| 0x01 | BASE_POSITION | 0x1E | SPACE_RUN_CHANCE | 0x2A | GOAL_GET |
| 0x02 | FREE_RUN | 0x1F | SPACE_RUN_COUNTER | 0x2B | POST_PLAY |
| 0x03 | DRIBBLE | 0x20 | LINE_BREAK | 0x32 | COVER |
| 0x04 | TRAP | 0x21 | DIAGONAL_RUN | 0x33 | **DELAY** |
| 0x05 | PASS_GET | 0x22 | PASS_SUPPORT | 0x34 | **PRESS** |
| 0x07 | MOVE_KEEP | 0x23 | PASS_SUPPORT_THROWIN | 0x35 | SAND |
| 0x10 | MOVE_ON_PASS | 0x24 | PASS_AND_GO_RUN | 0x36 | MARK |
| 0x11 | WAIT_TIMER | 0x25 | ONE_TWO_PASS_RUN | 0x3A | MATCH_UP |
| 0x12 | LONG_PASS | 0x26 | CENTERING_GET | 0x3D | PASS_COURSE_CUT |
| 0x16 | SHORT_PASS | 0x27 | OVERLAP_SB | 0x3E | KP_PRESS |
| 0x18 | THROUGH_PASS | 0x28 | OVERLAP_CB | 0x3F | KP_BASE_POSITION |
| 0x19 | SHOOT | 0x1C | SWITCH_PASS | 0x40 | KP_PUNTKICK |
| | | | | 0x4E | KP_AFTER_CATCH_MOVE |
| | | | | 0x6D | POST_MAN |

Two distinct words share this enum, and this match's data keeps them apart:

- **Assignment table `T+0xA2A0[slot]`** — the team's per-slot *assignment*. Dominated by
  MARK(0x36) and MATCH_UP(0x3A), plus run-moves (DIAGONAL_RUN 0x21, PASS_SUPPORT 0x22, ONE_TWO 0x25,
  SPACE_RUN_* 0x1D/0x1E/0x1F, POST_PLAY 0x2B, POST_MAN 0x6D) and COVER(0x32). **It never contains
  DELAY(0x33) — 0 occurrences in 48,972 slot-reads.** It contains PRESS(0x34) on 156 man-frames,
  all of them the designated presser.
- **Behavior state `+0x5D0`** — the man's *live state*. Here DELAY(0x33) is frequent (1251
  man-frames total, 1028 on far-loose frames), as are BASE_POSITION, PASS_GET, DRIBBLE, MARK.

A man whose assignment is MATCH_UP(0x3A) shows up in the behavior state as DELAY(0x33) most of the
time (1015 of 1332), sometimes PRESS(0x34) (244), sometimes WAIT_TIMER(0x11)/PASS_GET. So "the
nearest man on a far ball is in DELAY" is a *MATCH_UP-assigned* man delaying, not the press writer's
DELAY. This is what S1Watch round 32-33 saw as the +0x5D0 toggling.

## 1. The team point `T+0x2482C/+0x24834` is a match constant, not a moving point

Read every frame for both teams, it never changed: **T0 = (0.0, 0.0), T1 = (90.0, 0.0)** (x, z).
So the gate's "team point", as recorded here, is a per-side **fixed** point (T0 at the ball's
zero-x end, T1 at the +90 x end, 38 m past the +52 goal line) — **not** a moving team centroid.

Consequence (honest): I could not pin the *actual* MA object the gate reads at runtime. The decompile
chain (`local_230 = *(param_2[0]+0x18)` inside `FUN_140536FC0`) is a different object of the same
record layout, and its live base was not resolvable this session (a heap scan for `team+0x6d50 == T_s`
found nothing, because `T_s` are the record pointers, not a value stored at +0x6d50). The task named
`MA+0x2482C/+0x24834` as the points to record, so I recorded `T+0x2482C/+0x24834` for both teams, and
the **beyond-13 m / ≤13 m classification below is measured against those points** (each team's own
point). The ball's distance to the pitch center is also logged (`dpt`): median 58 m on far frames,
max 96.9 m.

## 2. Who goes to a loose ball beyond 13 m

Definitions: **far-loose frame** = loose ball (`B+0x4C` == 0) with ball > 13 m from *both* sides'
points (1628 of 2226 frames, 73%). A **designated presser** = a slot in `T+0xA2A0` set to
PRESS(0x34) on that side (the coordinator's / sweep's press assignment). The visible "chaser" is
tracked two ways: the designated presser (table == 0x34) and the man actually nearest the ball
(`+0x5D0` state).

**Result: a designated presser (table 0x34) is present on far-loose frames 109 man-frames, and the
coordinator arm armed it 108 of 109. The visible men closing a far ball are usually
MATCH_UP-assigned men in DELAY(0x33) behavior state, not the designated presser.**

- **Designated pressers (table == 0x34) on far-loose frames: 109 man-frames.**
  - Arm attribution by the slot's mode: **coordinator (mode ≠ 38) 108 (99.1%), sweep (mode == 38) 1.**
  - The 13 m gated writer (arm 1) is refused by its own gate on these frames — that is the
    definition of "far" — so the press comes from an ungated arm. The coordinator (arm 2,
    sub_140634470 → target eval sub_140633940) is the source, consistent with doc §6 (the gate
    stops only arm 1's candidates).
- **Most common designated-presser slots (side, slot):** (1,8)×37, (0,7)×19, (1,10)×12, (1,4)×11,
  (0,9)×9, (1,9)×8, (1,7)×8, (0,5)×4 — side 1's high/wide slots and side 0's slot 7.
- **The designated presser is usually already near the ball** (table==0x34 man's distance to ball,
  109 man-frames): **median 3.3 m, p75 5.1, max 21.7 m; <3 m in 45, 3–6 m in 42, 6–10 in 14, >10 in 8.**
  The coordinator's target eval (max score over role-filtered candidates) picks the man *already
  nearest the ball*. So "loose ball beyond 13 m" (from the team point) is usually **not** a long
  sprint — the presser is typically within ~3–5 m.
- **The presser is the nearest man only 42 of 109 times.** More often the nearest man is a
  MATCH_UP-assigned man in DELAY(0x33) behavior state. Nearest-man `+0x5D0` state on far-loose
  frames: DELAY 337, BASE_POSITION 288, PASS_GET 286, DRIBBLE 164, PRESS 122, MARK 111, WAIT_TIMER 89,
  NOTHING 87, KP_PRESS 36, COVER 26, …
- **DELAY(0x33):** never in the assignment table (0/48,972), but frequent in the behavior state
  (1251 man-frames total, 1028 on far-loose, median 5.9 m from the ball). These are MATCH_UP-assigned
  men delaying, i.e. the sweep's / coordinator's "hold and time the press" — the sweep's DELAY
  *outcome* is visible here, even though the table word that produces it is MATCH_UP(0x3A), not a
  literal 0x33 in 0xA2A0.

### Per-episode table (chaser = designated presser, table == 0x34; "reach" = presser < 3 m of ball)

An episode = contiguous far-loose frames on which a side has a designated presser, following that
side's presser. 18 episodes.

| side | start frame | len (frames) | presser d at start (m) | frames to reach <3 m | slot |
|---|---|---|---|---|---|
| 0 | 429650 | 15 | 4.7 | 2 | S7 |
| 1 | 434145 | 14 | 3.5 | 4 | S8 |
| 1 | 434236 | 14 | 7.2 | 5 | S8 |
| 1 | 438047 | 11 | 3.0 | – (held ~3) | S7, S10 |
| 0 | 437272 | 9 | 15.8 | 7 | S9 |
| 1 | 437852 | 8 | 2.4 | 1 | S10 |
| 1 | 434113 | 6 | 3.0 | 2 | S8 |
| 1 | 427104 | 5 | 16.5 | – | S9 |
| 1 | 436741 | 5 | 0.8 | 1 | S4 |
| 1 | 428653 | 4 | 0.8 | 1 | S4 |
| 0 | 429596 | 4 | 1.5 | 1 | S7 |
| 1 | 427865 | 3 | 21.7 | – | S9 |
| 1 | 434319 | 3 | 3.7 | – | S8 |
| 0 | 428502 | 2 | 8.6 | – | S5 |
| 0 | 428510 | 2 | 7.0 | – | S5 |
| 1 | 428676 | 2 | 1.9 | 1 | S4 |
| 0 | 428289 | 1 | 3.6 | – | S6 |
| 1 | 437850 | 1 | 2.6 | 1 | S10 |

**Frames until someone reaches it:** 11 of 18 episodes reached <3 m; **frames-to-reach median 1,
max 7**. The two genuinely long ones (16.5 m and 21.7 m starts) were never closed by the
designated presser within the window — the ball was won or moved by another man first. Because the
coordinator picks the nearest eligible man, "reach" is fast; a true long chase (>10 m) is rare
(8/109 presser man-frames) and, when it happens, is resolved by a *different* man closing the ball.

## 3. How often mode == 38 (the sweep's arm condition)

**178 / 2226 in-play frames = 8.0%;** 201 slot-frames total, side 0 = 119, side 1 = 82. By slot:
(0,10) 69, (1,7) 24, (1,8) 17, (0,7) 15, (1,9) 12, (0,9) 10, … So the sweep's condition is *met*
~1 in 12 frames, but on far-loose frames it produced **1** designated-presser slot-frame (the other
108 being coordinator) and is not correlated with the DELAY-state men (of the 1028 far-loose
DELAY-state man-frames, only 1 sat in a mode==38 slot). In this match the sweep's mode==38 slots
mostly overlap the coordinator's own picks (both write the same PRESS word, so the table cannot
disambiguate the winner); the sweep's DELAY outcome, where it occurs, lands in MATCH_UP-assigned
men's behavior state rather than as a literal 0x33 in the table.

## 4. Direct answers

1. **Who goes to a loose ball beyond 13 m?** A man *already near the ball*, designated into PRESS
   (0x34) in his side's `T+0xA2A0` — the coordinator arm's target-eval pick (sub_140634470 →
   sub_140633940). Chaser start distance median 3.3 m; it is almost never a long sprint from
   formation. Separately, the men actually closing the ball are often MATCH_UP-assigned men whose
   behavior state is DELAY(0x33).
2. **Which slot?** Concentrated in the high/wide slots: side 1 (8), (10), (4), (9), (7); side 0
   (7), (9), (5).
3. **Which state, 0x34 PRESS or 0x33 DELAY?** The **designated** presser is PRESS(0x34) in the
   assignment table (109 far-loose man-frames). **0x33 DELAY never appears in the table**; it
   appears only as a *behavior state* (+0x5D0) on MATCH_UP-assigned men (1251 man-frames, 1028 far).
   So "0x33 DELAY" is the sweep/coordinator's *timing* outcome, not a distinct table assignment.
4. **From which arm?** **Coordinator (arm 2): 108 of 109 designated pressers (99.1%).** The 13 m
   gated writer (arm 1) is refused by its own gate (that is "far"); the sweep (arm 3) armed 1.
5. **Frames until someone reaches it:** median 1 frame for a presser already near the ball; the
   few true long-ball episodes (>15 m) were closed by another man, not the designated presser.
6. **How often mode == 38:** 8.0% of in-play frames (178/2226), side 0 more than side 1, dominated
   by side0 slot 10.

## 5. What this settles / leaves open

**Settled (CONFIRMED, live):**
- On far loose balls the designated presser (table 0x34) is overwhelmingly the **coordinator** arm,
  and that presser is typically already ~3–5 m from the ball — "far from the team point" ≠ "a far
  run."
- The assignment table `T+0xA2A0` is the **general per-slot assignment** (MARK/MATCH_UP/run-moves
  dominant); **0x33 DELAY is never a table value.** DELAY(0x33) is a *behavior state* on
  MATCH_UP-assigned men — resolving S1Watch round 32-33's +0x5D0 BASE_POSITION↔MARK(0x36) and the
  DELAY observation: those are MATCH_UP-assigned men's live states, not press-table writes.
- This corroborates S1Watch round 32-33: their far-ball sample's nearest men were plain men
  (MATCH_UP→DELAY / MARK), not pressers.

**Left open (could not pin this session):**
- The live base of the MA object the gate reads (`local_230 = *(param_2[0]+0x18)`). The recorded
  team points are the record-constant (0,0)/(90,0), so "beyond 13 m" here is measured against
  those named points, not a moving centroid. (Resolves the doc §3.1 "far-chase writer" only for the
  coordinator arm; the sweep-vs-coordinator winner is indistinguishable from the table because both
  write the same 0x34 word.)
- Why a literal 0x33 never lands in the table despite frequent DELAY behavior: the sweep
  (`sub_14062AF10`) chooses 0x33 vs 0x34 by trap/interruptible checks, but the *table* word that
  persists for a delaying man reads as MATCH_UP(0x3A) — i.e. the 0x33 is a transient per-frame
  behavior, while the slot's standing assignment stays MATCH_UP. (The far-chase doc §6 "sweep sets
  0x33 or 0x34 via the setter" would then write a 0x33 to the table only on the frames it picks;
  in this match those were 0, or were immediately overwritten.)

**Caveat:** single match, one ~4-minute in-play window, both teams' default-ish tactics. The match
ended and is now unloaded, so a second window was not possible. Reproduce with `fcl_rec.py`
(scratch dir `<workdrive>/gf-FarChaseLive-scratch/`, `out/rows.jsonl` = the 2226 rows).
