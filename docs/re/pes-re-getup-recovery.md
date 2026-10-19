# Get-up recovery: when may a defender slide or tackle again? (S1Watch, round 19)

> **Main audit 19-10-26:** `0x14072EEFD` is the call inside FUN_14072EA00 (the rise blend set): matches. No fixed get-up timer: the next slide/tackle is gated by the current clip's +0x70 frame test (FallDown FUN_14072E360, Sliding FUN_140787EC0, Tackle FUN_140790160).

Status: static. **The slide predicate has no explicit get-up gate.** Its refusals are the state, class and geometry tests of defender-decisions §3. The get-up's own timing is a rise-up gate in the contact layer (below). The one place a recovering man's transition answer depends on his clip is the anime `+0x70` frame test. Tags: CONFIRMED = decompile or image read; INF = inference; OPEN = not resolved.

## 1. The slide predicate (CONFIRMED structure, defender-decisions §3)

Entry `FUN_140971B60 → FUN_140971BA0` (`pes-re-defender-decisions.md` §3; decompile not re-read in full this round). The refusals listed in the doc are:
- open play (`MI+0x116C == 1`), own anime not in the class `FUN_140A6E4B0`, `MI+0x1198` odd;
- no newest-4 event of type 0xB within `10·gs + 0.5` frames;
- team capability 8 (`FUN_140A604D0`), CPU team (`T+0x255 == 0`);
- own anime not in classes `FUN_140A6E0D0`, `FUN_140A6E3C0`, `FUN_140A6E5E0`; not `FUN_140A6E300` (class test) and ball not > 0.5 m high 10 frames ahead;
- distance `d ≥ 4.0 + 0.8·L`, heading and bearing tests, teammates in SLIDING/SLIDING_KEEP, opponent capsule test, `FUN_140A539D0`, `FUN_140A53690`.

**No term in this list reads the man's fall/stagger/stand-up state directly.** The only anime-state refusals are the class tests above.

## 2. The class tests the slide predicate uses (CONFIRMED, image read)

Each is a bit test on the per-class word at `DAT_1426484F0 + 4·class` (class = the signed clip-class byte of the current clip; the test is `class ≤ 0x5D`):

| function | test | bit |
|---|---|---|
| `FUN_140A6E300` | `word >> 15 & 1` | 15 |
| `FUN_140A6E0D0` | `word & 0x2C0 != 0` | 6, 7 or 9 |
| `FUN_140A6E5E0` | `word >> 5 & 1` | 5 |
| `FUN_140A6E340` | (not read this round) | — |
| `FUN_140A6E3C0` | `(anime+8) bit 13` (any class), else `word >> 13 & 1` | 13 |

Class words (from the table, CONFIRMED): class 4 `0x4008` (bits 3,14); 5/6 `0x400C`; 7 `0x4020` (bits 5,14); 11/12 `0x4080` (bits 7,14); 13 `0x4040` (6,14); 14 `0x4100` (8,14); 15 `0x2000` (13); 16 `0x3002` (1,12,13); 17 `0x3000` (12,13); 72 `0x8000` (15); 91 `0x2` (1). Under the slide test the **bit-15 classes** (the `FUN_140A6E300` test) are the only ones this table sets bit 15 for, at class 72 (`0x8000`).

**The bit meanings are not labelled in the docs (OPEN).** Which of these classes is a stand-up/fall class is not decoded: the clip-class-to-action mapping (the class byte, not the action id) is not named.

## 3. The get-up rise gate (CONFIRMED, contact doc §4.3, FUN_14072C5C0)

- Rise-up starts at the fall clip's end frame (window +4/60 s) only if the ball, predicted 1 s ahead, is within 10 m, or the fall kind is 4, or `FUN_140A638C0`. Never during a stoppage or a pending Judge decision (`FUN_140A53F90`).
- Rise-up rate `0.95·(1−b) + 1.1·b` with `b = clamp((Balance−40)/60)`, blend 0.3 s (`FUN_14072EA00` @0x14072EEFD).
- After the rise the ball owner goes to DRIBBLE; others to MOVE; an injury check may switch to `DEMO_INPLAY_CONTACT` (0x50).

So the rise is gated by ball proximity, not by a slide cooldown.

## 4. The transition answers for a man still in a fall/stand (CONFIRMED)

The action objects have `+0x70` "may I go to action 7" answers (`pes-re-trap-windows.md` §6). For the slide/tackle entry, the relevant ones:

- **FallDown** (action 17, vt `0x1425DF250`, `+0x70 = FUN_14072E360`): allow iff the clip's frame count at speed 1 with key 5 (`FUN_1406AFBC0`/`FUN_1406AF8E0`) ≤ `sec+0x38` (the current clip's remaining frames). If the anime's state byte `+0x20` is 2, the frame count uses `iVar1 = 0`. CONFIRMED.
- **Sliding** (action 11/12, vt `0x1425E1878`, `+0x70 = FUN_140787EC0`): the same frame-count test; the state-byte branch uses `4` instead of `2`. CONFIRMED.
- **Tackle** (action 13, vt `0x1425E1A48`, `+0x70 = FUN_140790160(action, anime, target)`): if the target is not a trap/slide-class clip (`FUN_140A6E090`), a range test on `FUN_1406B03D0` frames (`iVar4 < frames>>1`), with the target `0xD` special case (speed/heading test on `0x2480/0x2488`, `0x247C`); otherwise the frame-count test.
- **Stagger** (action 16, `+0x70 = FUN_14072E450`): not read this round.

So a man in FallDown is allowed the next action only once his current fall clip has at most the remaining frames left. That is a frame test, not a fixed recovery timer.

## 5. Answer to the question

- **Refusal during a get-up:** not by the slide predicate itself. The predicate has no get-up term. A man on the ground is refused through the anime class tests of §2 (if his clip's class has the bit) and through the `+0x70` frame test of §4 (which refuses while the clip still has more frames than the remaining time).
- **After the get-up:** the man is free to slide again once the rise has finished (the anime is no longer in the fall clip) and his current clip allows the transition (§4 frame test), and the predicate's geometry and team tests pass. There is no fixed post-get-up timer in the slide predicate.
- **Not established:** which clip classes are the stand-up classes (so which bit in §2 marks a get-up); the stand-up clip ids (`stand_up_from_back`/`front` are not named in the install docs I read); and the Stagger `+0x70`.

## 6. Open

1. The class-to-bit meaning table (§2) and which class is the get-up.
2. The Stagger action's `+0x70` (`FUN_14072E450`).
3. The live test: a defender on a stand-up clip, the anime state byte and class at each frame, and the first frame a slide is allowed.
