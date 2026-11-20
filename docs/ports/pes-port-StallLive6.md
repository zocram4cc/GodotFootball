# pes-port-StallLive6: PES's live carrier rule (MA+0x2601C) measured over a 10-minute pass sample (StallLive, 20-11-26)

Status: the measurement answers Main's question. No behaviour change is committed by me this round. The two hunks that implement the rule are in StallContact's files (humanoid.cpp touch gate, match.hpp kick clear), and StallContact is landing them on its own branch. Branch `StallLive` stays at **93ad88d1bfc404a10079aab01e4f3fd8686bec6f** (ls-remote confirmed earlier this round). The build dir is deleted (none rebuilt after the revert).

## 1. Inputs (CONFIRMED)
- MA = 0x7ff5fe280018 (the rdi of the W1 hook, seq2595 of pid 2998562; the capture's memory holds `*(0x2d27be70+0x508)` as that value; the live process read `MA+0x2601C = 0xb`, `MA+0x2600C = 0xff`, consistent with Main).
- Event store E = 0x7ff5fe0f0018: found offline as `*(ctx+0xE0)` for the ctx objects whose `+0x18` holds MA (33 holders in seq2595; both rings full, count 8). Ring r at E+r*0x1908: count +0x1900, head +0x1904, 800-byte records: kind +0, frame1 +4, actor team +0x18, actor slot +0x1C; newest = head mod 8.
- Ball: the ball record B = *(G+0x40), sample 0 at B+0x88 (x, y, z); velocity at B+0x2948 (x, y, z). Check: consecutive-frame position change x 0.024 m per frame * 54 = 1.31 m/s, matching the recorded velocity 1.3138 m/s. Units are metres and seconds. MA+0x2482C is NOT a live ball: it held the same value across frames and produced the 600-4300 m/s "speeds" of round 5. That was a wrong-offset artefact, not a carrier signal.
- Sample: `pass_sampler.py` (read-only), 600 s, 11,148 frames, 9,170 after dropping rows with an unreadable ball (the play state is 1 for 5,519 of them, 2 for 3,315, 3 for 336). Full timeline in `analysis600.txt`.

## 2. The PES rule (CONFIRMED from the sample)
Carrier MA+0x2601C (0xFF = 255 = none) and the pass target T+0x78 (per side) against the ball speed and the newest ring-1 key (r1):

1. **Kick -> 0xFF.** Every fast ball (kick) clears the carrier: the carrier goes to 255 on the kick frame, and the ball speed jumps to 14-26 m/s. Examples: f19970 (14 -> 255, speed 14.6), f20204 (20 -> 255, speed 17.5), f20469 (1 -> 255, speed 22), f20609 (9 -> 255, speed 24), f20901 (7 -> 255), f22077 (21 -> 255, 13.8), f22343 (13 -> 255, 26.6). The newest r1 key on those frames is the none key (team 0, slot 2) for the whole flight.
2. **Pass target set at the kick.** On the kick frame (or within 2-3 frames) T+0x78 of the kicking side is set to the chosen receiver: e.g. f19972 pass1 255 -> 15 (kick from carrier 14); f20067 pass1 14 -> 20 on the kick at f20063 (speed 17.5).
3. **During the flight: carrier = 0xFF.** Confirmed for every episode in the sample (carrier 255 from the kick until the receiver's touch).
4. **Receiver's touch -> carrier = receiver, pass target cleared, same frame.** f20155: carrier 255 -> 20 and pass1 20 -> 255 on the same frame; the ball is slow (3.4 m/s). The r1 key at that frame is the toucher (team 1, slot 1), not the none key.
5. **Slow touches keep the carrier.** The carrier stays on the last slow toucher through control (e.g. 20155 -> 20181 -> 20204: carrier 20 while the ball is slow; 19 from f20271 to f20420 while the ball is slow and dribbled).
6. **A kick clears it again.** 20204: 20 -> 255 at the kick (speed 17.5), i.e. the carrier is cleared by the next fast ball, not by a loose ball.

So the answer to Main's questions:
- **During a pass flight MA+0x2601C holds 0xFF (none)**: not the passer, and not the receiver until the receiver touches.
- **At the receiver's touch it becomes the receiver's id** (and T+0x78 is cleared on that same frame).
- **T+0x78 is set at the kick** (receiver chosen by the kicker's side selector), and **cleared at the receiver's touch**. While T+0x78 is set, MA+0x2601C is 0xFF.
- **Ring-1 newest key ↔ carrier**: key (team 0, slot 2) -> 255 in 6,926 of 6,926 frames. Other keys map to ids, but not by a fixed formula: (0,0) -> 10 (97%), (0,1) -> 21 (97%), (1,0) -> one of 1/4/10 (mixed). The map of key to id is not decoded (FUN_140A62E40 / FUN_140A3B320 are the PES reads; not decoded here, OPEN). The carrier flips to none on kicks, as seen in the timeline.

## 3. What GF does now (CONFIRMED by reading the code)
- The MA carrier is written from GF's sticky `pesHolder` (team.cpp `SetMaOwner`, elizacontroller.cpp), which only changes when a touch sets `pesKeyActor` (humanoid.cpp:1501-1503, `CarrierFromTouch(touchedThisTick, AI_HasPossession(...))`). Nothing clears it on a kick.
- GF's first touch by the receiver often fails `AI_HasPossession` (the ball is not at his feet at the touch), so no key is pushed, and after a kick clear the holder stays null (the 662 firings of round 4).

## 4. The change that implements the rule (handed to StallContact; not committed by me)
Two hunks in StallContact's files (StallContact confirmed it will land them and run PesStall):
- humanoid.cpp ~1501: `pes::CarrierFromTouch(touchedThisTick, AI_HasPossession(match->GetBall(), CastPlayer()) || team->PassGetReceiver() == CastPlayer())` — a touch by the pass target takes the carrier (PES point 4).
- match.hpp `RecordPassEvent`: `pesKeyActor = nullptr; pesHolder = nullptr;` — a kick clears the carrier (PES points 1 and 3).
I applied both locally as an experiment and reverted them (the worktree is clean on 93ad88d1). The experiment's numbers were not measured (the rebuild and the census were not run, because StallContact lands the change). Expect `ALiveBall` and the pass test to move, but this is unmeasured here.

## 5. Not done in this round (CONFIRMED)
- No commit from me this round. No mutation. No PesStall, PesHang, throw-in, gen --check or smoke run on a changed branch (the branch is still 93ad88d1, measured in round 5: PesStall 36 firings, PesHang pass, pass test red, throw-in 2/109 ankle short by 0.0155 m, gen --check clean, smoke 37,524 ticks).
- The key-to-id map (ring-1 key -> carrier id) is OPEN; the none key (team 0, slot 2) is confirmed.

## 6. Open items
1. StallContact's branch: verify the two hunks give PesStall.ALiveBall and the pass test the expected result (not measured by me).
2. Decode the ring-1 key -> carrier id mapping (FUN_140A62E40, FUN_140A3B320) so GF can write MA+0x2601C exactly, not only the sticky holder.
3. The first touch of a pass receiver that does not pass AI_HasPossession: the rule in point 4 covers it only through PassGetReceiver; a PES-faithful gate (the ball-at-feet test on the receiver's touch) is OPEN.
