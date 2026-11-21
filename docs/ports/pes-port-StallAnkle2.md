# pes-port-StallAnkle2: the throw-in ankle failure is NOT a PES SetMotion start time; the failing clip is an authored GF clip (StallRun, 21-11-26)

Status: NO FIX LANDED. Nothing pushed. Worktree `gf-StallRun` at **a2439354**, clean (no src/tests diff vs HEAD; build dir deleted). The ankle test is still RED on StallRun. The premise of StallAnkle (section 3 of pes-port-StallAnkle.md: "the taker plays a PES SetMotion clip with start 0.1 s") is WRONG and is corrected here.

## 1. The failing clip, measured (not inferred)
Run: a2439354 + temporary print at the failing tick (reverted). Log: `gf-StallRun-scratch/ankle/diag2.log`.

| field at the one persistent short tick | value |
|---|---|
| anim id / file | 1203, `assets/gf_anims/pass/idle/000.anim_mirror` |
| `type` variable | `shortpass` (authored GF clip) |
| `specialvar1` / `specialvar2` | empty (not a PES special, so NOT a SetMotion issue) |
| functionType | 4 |
| originatingInterrupt | 7 |
| frame | 10 / 73, rate 1.0 |
| shortfall | 0.0040 m at that tick (worst 0.00923 m, the reported number) |

## 2. Why frame 10 is not a start time
- `PlayIssuedClip` (the only place the SetMotion start time is applied, `humanoidbase.cpp:547`) sets frame only on PES special clips, and the clip here has no specialvar. So the start-time line is not on this clip's path.
- The authored clip is selected at frame 0 (`humanoidbase.cpp:1311`, `currentAnim->frameNum = 0`) and then advances one frame per tick (`humanoid.cpp:400-402`). Frame 10 = 10 ticks into that clip, at 10 ms per frame (= 0.10 s of ordinary playback, not a skipped wind-up).
- So the start-time fix (a2439354's line) changes the trajectory: PES special clips now begin mid-clip, the match diverges (the earlier anchor attempt showed 137 vs 61 taking ticks), and in that different match the taker is on this authored `pass/idle` clip at its frame 10 when the ankle reads 9 mm short. The clip's own joint geometry sets that reading; the start-time line only changes which clip is on the body.

## 3. Consequences for the decode asked for
- "Motion 1203's SetMotion start-time source" has no PES motion behind it: 1203 is a GF authored clip id, not a PES motion number. There is no FUN_1406b97d0 entry to compare for this issue.
- FUN_1406b97d0's table (docs/re/pes-re-anim-select.md §10.3) covers step-loop motions only; this clip is not one of them as far as the measurement shows.
- The open question is therefore not the start-time unit or scale. It is: why does this taker's ankle, on GF's own authored `pass/idle` selection (no PES issue), sit short of F[1] + 0.1 m at frame 10 in the trajectory the start-time fix produces, and what does PES's taker do on the same tick (his PES joint block at that moment). That needs PES's trajectory for this taker, and I did not get there in this round.

## 4. Not done (time box)
- No change landed; PesStallQuick, the full ALiveBall, PesHang and gen --check were not run because nothing changed. gen_translated --check passed earlier on the tree with the reverted anchor (10,150 byte-identical; pes-port-StallAnkle.md §4).
- The ALiveBall count stays 92 on a2439354 (pes-port-StallRun2.md).

## 5. Next highest-value step
Compare the taker's ankle on the failing tick with PES: one PES capture of the same taker's joint block (joints 3 and 6, P+0x538) during a throw-in hold, next to GF's joint block at frame 10 of `pass/idle/000`. If PES's taker holds the ankle at the line on this clip, the gap is the taker's own push/hold on this authored clip (FUN_141EB5BB0 path), not the start time. Owner: StallAnkle's next round, with a capture request to Main.

## 6. Blockers
None for tooling. For the next step PES's joint block on this taker needs a capture (live2 hook on the throw-in hold, P+0x538), which Main must arm.
