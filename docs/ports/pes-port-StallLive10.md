# pes-port-StallLive10: the kick-tick hook, the 145 locomotion census, and the ceremony hang (StallLive, 20-11-26)

Status: PARTIAL. Branch `StallLive`, HEAD **a0a290a8** (pushed; ls-remote not re-run after the push). Commits this round:
- `a0a290a8` Ceremony search: a stalled match fails at 100,000 ticks instead of running forever; the stall names the throw-in taker. (Tests: `pes_throwin_release_test.cpp` cap; `apps/gfsim_run.cpp` stall detail.)
- Census diagnostics (movement fields, STALLMOVE) are in the working tree of `tests/sim/pes_stall_test.cpp` and are NOT committed yet (see section 4).

## 1. Item 1: the kick-action tick 0x140789D00 (blocked; the target is probably not a kick)
- Translating 0x140789D00 needs a proof record (snapdiff, captured states) and a closure json; neither exists. `untranslated_callees.py` refuses an untranslated root, so the closure cannot be listed until it is translated.
- Static (CONFIRMED): no direct E8/E9 caller. Its only reference is one qword slot, 0x1425E1908 in `.rdata`, slot 0 of a run of per-action function pointers (neighbours 0x140787690, 0x140787C00). It runs only through an indirect call from a per-action table.
- The ledger (row 9360) and `pes-re-contact-port.md` line 16 decode it as the slide rule: "slide phase 4, first 6/60 s", in the forced-fall path `FUN_140844540`. So it is most likely SLIDING (11), not a kick. Not verified: the table's index-to-action mapping is not decoded.
- Hook: LEN 16 prologue `488974241048897c241841564883ec20` was generated for 0x140789D00 (`hooks_for_image`). Main's EVERY-20 arm got 0 calls in ~8 min of live play, consistent with COM kicks not reaching it.
- Better hook (armed by Main, EVERY 100, predicate d(rcx+0xac8) in {8,9,10,11,13,56,58}): the dispatch point 0x1406E0540 (caller of the table builder FUN_1406DF630; header of pesactiontable_stores.inc says the table is at P+0xC40). Its prologue `48894c2408574883ec3048c7442420feffffff` spills rcx (the first argument). I could not prove rcx is P: if the predicate never fires, check the first argument's type from the capture first.
- Consequence for the kick ids: no action id has been carried to P+0xAC8 for GF clips. The f6a0ad18 attempt aborted on this same table path. Until the table index is decoded, this item cannot be closed.

## 2. Item 2: the 145 locomotion firings (CONFIRMED classes; cause not fully chased)
Census with the new STALLMOVE line (seed 7, 2 min, `PesStall.ALiveBall*`, GF_STALL_DUMP=1; 223 firings, 223 STALLMOVE lines).

| Class (clip fn 14 Special, command fn 1 Movement, P+0xAC8 = 2 MOVE) | firings | command | target | body speed |
|---|---|---|---|---|
| command speed 8.00, no target (usetgt 0), target 21 m from the man, MOVESTEP issued (motion 3014) | many (run of 12+ ticks shown) | 8.00 m/s | 21 m away | 0.6-2.7 m/s |
| command speed 5.00, no target, target 42 m away, motion 2222 | many | 5.00 m/s | 42 m away | 0.07-0.14 m/s |
| command 1.76 m/s, use target, target 0.75 m from the man (the one man that is an actual approacher) | 1 shown | 1.76 | 0.75 m | 1.11 m/s |

Counts by (command speed, body speed, motion issued): cmd>0 body 0.3-1.5 issued 71; cmd>0 body>=1.5 issued 35; cmd>0 body<0.3 issued 34; cmd>0 body>=1.5 not issued 20; cmd0 body>=1.5 issued 18; cmd0 body 0.3-1.5 not issued 18; cmd0 body<0.3 not issued 17; others 10.

Reading (INF, from the fields): the 145 are NOT men pressing the ball. Their movement commands target points 21-42 m away (formation moves, no ball target). They stand 1-1.5 m from a resting ball because no approacher was commanded to the ball, not because MOVESTEP fails to arrive. The "non-arrival" was a misreading: they were never sent to the ball. That is the same press-assignment gap as the 172 non-presser firings of round 8. The one real approacher (target 0.75 m, cmd 1.76) is at 0.75 m, inside the 0.1-0.2 m stop radius? No: outside it, and speed 1.11, so that single man is still closing.
- PES-faithful cause (open): which man PES sends to a resting ball (the press writer, pes-re-approach.md 1.1). Not decoded further here. The MOVESTEP speed/heading formula was not checked: the census shows the command speed is not reached (8 m/s command, 1-2.7 m/s body), which is a separate question for the movement port.

## 3. Item 3: the ceremony test hang (FOUND; fixed as a hang, the stall itself not fixed)
- Not slow: a hang. `ACeremony...` searches seeds 14, 25, 3, 26, 20, 24 for a ceremony throw-in and had no tick cap (`kNoTickCap`).
- Seed 14: no ceremony, full time at 34,975 ticks (test ran it to the end: 31 progress lines, 17 at seed 14).
- Seed 25 STALLS its first throw-in: gfsim stalls at 1,758 ticks (limit 1,757). At the stall: set piece 1 (ThrowIn is e_SetPiece 5; 1 is KickOff: the print's `piece 5` is the buffer's desiredSetPiece = ThrowIn), in-play 1, buffer active 1 prepared 1 started 1, taker A9. Taker line: fn 1 (Movement), clip `movement_special/dribble/angled/special/D135_decel_holdball.anim_mirror` frame 1/25, MOVESTEP state 2 (prev 1), 0.18 m from the resting ball, speed 1.38, ball 0.00 m/s at (-2.6, 33.3, 0.9), held 0, walking 1, pickingUp 0. The taker stands at the ball in a walk and never releases the throw: a stall in the throw-in release, the same family as the throw-in release test.
- The test's progress (cer2) shows seed 25 at 26,000+ frames still in play with the set piece armed (inPlay=1 setPiece=1 from frame 4,000).
- Fix applied: `kMatchTickCap = 100000` per match (about twice a full match: 34,975 and 49,980 ticks), so a stalled seed fails instead of hanging. Not verified by a full run (a run takes about 40 min per stalled seed).
- Not fixed: the seed-25 throw-in release stall (the taker in Movement walking at the ball never takes the throw). Needs the throw-in taker path (PES's SEAMLESS_THROWIN / the take), not started.

## 4. Item 3b: the ankle (4 of 154) — not diagnosed this round
The ankle failure is unchanged from round 9 (4 of 154, worst 0.0269 m). No work this round.

## 5. Acceptance state
| Check | State |
|---|---|
| PesStall fully green | NOT MET (ALiveBall 223) |
| Holder test, pass-target, PesHang | last measured round 9 (f6a0ad18 tree); not re-run on a0a290a8 (no code change touched them) |
| Throw-in 9/9 | NOT MET (ankle fails; ceremony not verified after the cap) |
| Kick-clear mutation | last run round 9 (red); not re-run |
| gen --check, ledger --check | last run round 9 on the same src/ (green); not re-run after a0a290a8 (apps/tests only changed) |
| Seed-7 smoke | 29,597 ticks (round 9); not re-run |

Uncommitted: `tests/sim/pes_stall_test.cpp` (STALLMOVE census line and the MOVESTEP include). It is diagnostic; commit it with the census numbers above when the next round needs it.

## 6. Open items
1. Confirm which table index is action 8/11 (the dispatch point 0x1406E0540): the armed hook's logged action id answers it. Then the kick ids (item 1).
2. The throw-in taker in Movement at the ball (seed 25): what the throw-in taker path should be in PES (SEAMLESS_THROWIN, the take), and why GF never releases.
3. The 145: which man PES commands to a resting ball (the press writer). The command speed not reached (8 m/s command, 1-2.7 m/s body) is a separate movement question.
4. The ankle, 4 of 154.
5. Re-run the acceptance set on a0a290a8 (holder, pass-target, PesHang, the throw-in suite with the cap, mutation, gen, ledger, smoke).
