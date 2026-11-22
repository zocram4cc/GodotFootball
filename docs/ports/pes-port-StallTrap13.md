# pes-port-StallTrap13: kind-8 in-play producer reproduces the live ring-1 keys (replay of PES bytes, 3/3); GF never enters it because B+0x2918 bit 4 is never set; no translation, feed or push this round (CounterPath, 22-11-26)

Status: NOT LANDED. origin/StallTrap is 56ef1e91; the worktree is clean at HEAD (src, tests, tools). No translation of the producer or the decision, no red-first test, no feed, no race tests, no PesStallQuick / ALiveBall / PesHang / PesPassTarget, no gen --check. Those are the remaining work (section 6). What follows is what the captures settle.

## 1. Captures (CONFIRMED by index.json rv.hook and call.bin)

pid1971636: kind8_140430C30 x6, kind8_140872C10 x6, kind8_140A179C0 x11, kind8_140A19290 x24, kind8_140A3D060 x7, kind8_140A3DC40 x7. Full mem.bin, read in place. `scratch/trace/alllist.txt`.

## 2. The producer's two branches and which one the captures reach (CONFIRMED by replay)

FUN_140A179C0 chooses its builder on B+0x2918 bit 4 (`FUN_140A57950`, `(B.2918 >> 4) & 1`):
- bit 4 set (B.2918 = 0x12 in the captures): FUN_140A18810, the in-play builder. Returns ok = 1 except case 0x10.
- bit 4 clear (B.2918 = 0): FUN_140A19060, selector B.2920, then FUN_140A19290 (decision) for selector 1.

Capture split (fields read from mem.bin; `scratch/trace/fields.py`):
- The 11 kind8_140A179C0 captures: B.2918 = 0, B.291C = 1, B.2920 = 1, MI.116C = 1 (play). Every replay (`replay_cap.py`) returns false: no record. Two of them take the duplicate-suppression exit (0x140A60300) and one takes the decision's keep path.
- The 24 kind8_140A19290 captures: B.291C = 3, B.2920 = 1, MI.116C = 3 (23) or 2 (1). Every replay returns false (keep the holder, actor out = 0x0A, roster 10 of the holder). None returns true, so none covers the 12 loose-ball exits of FUN_140A19290 (true, actor 0xFF).
- The gate-on kind-8 records are only in the D060 and DC40 hooks (B.2918 = 0x12, B.2920 = 1, B.291C = 1): 6 captures, frames 473, 507, 640 (each seen by D060 and DC40).
- One selector-2 record: kind8_140A3D060 seq344 and kind8_140A3DC40 seq345 at frame 43 (B.2920 = 2, B.2928 = 1, MI.173C = 10). FUN_140A19060 selector 2 returns actor = MI.173C with ok = 1. That is the older roster-10 record found in round 11, so it is explained by selector 2, not by the decision.

## 3. In-play builder reproduces the live record keys (CONFIRMED, replay of PES's bytes; 3/3)

For the three gate-on captures, FUN_140A18810 was called over the capture's own memory with rdx = a scratch out pointer (`scratch/trace/replay_fn.py`), then FUN_140A62FE0 (the key builder) was called with the returned actor:

| capture | frame | replay 18810 actor | replay 62FE0 key | live record key at DC40 rdx+0x1C |
|---|---|---|---|---|
| seq1067 / seq1068 | 473 | 5 | 0x200000000 (team 0, slot 2) | 0x200000000 |
| seq1117 / seq1118 | 507 | 0xFF | 0xFF00000002 (none) | 0xFF00000002 |
| seq1310 / seq1311 | 640 | 3 | 0x400000000 (team 0, slot 4) | 0x400000000 |

So the in-play producer path is reproduced bit-exactly on its key output for these three states. It is PES's own code under unicorn, not a GF translation, so this is replay evidence, not a GF proof. The roster index to key mapping (5 -> slot 2, 3 -> slot 4) comes out of FUN_140A62FE0 itself (via FUN_140A3AEE0 and FUN_140A3D4F0), not from a table I wrote.

## 4. Where B+0x2918 bit 4 comes from (partly CONFIRMED)

- Writers of B+0x291C and the ball-state fields are the copies in section 1 of pes-port-StallTrap12. The 0x2914..0x291B copy is one 8-byte move inside FUN_140430C30 (`*(B+0x2914) = *(src+0x2914)`), so B+0x2918 is copied with it.
- FUN_140430C30 (kind8_140430C30 x6): param_1 = B (0xABE8B04); param_2 = 0xABEB470 = B + 0x296C, a sibling record in the same ball object. Its 2918 and 291C words equal B's in those captures (read: 2918 = 0, 291C = 1 or 3).
- FUN_140872C10 (kind8_140872C10 x6): param_1 is a different struct (0x33468EE0): its +0x2918 holds a float (0x40080000), so it is not a ball-state source. Its writes to B+0x291C come from elsewhere in that struct. Not decoded.
- What sets B+0x296C, and therefore the bit 4 that switches the producer, is OPEN. A GF value for bit 4 would have to come from that writer. GF has no B+0x2918 field: the GF ball (ball.cpp) has no counterpart, which is why `ballState` is hard-coded to in-play (matchevents.cpp:175) and the kind-8 detector's in-play path never runs.

## 5. Consequence for GF (CONFIRMED by source reading, unchanged from round 11)

- GF's kind-8 producers are FUN_140A179C0 (never called: GF's `BuildDefaultBallEvent` with `ballState == 4` is a different function) and DetectType14 (no call site). With B+0x2918 bit 4 never set and no selector-2 fields, the in-play path is unreachable and the decision's true exits are not modelled.
- So the frame-167 none-key record (roster none, team 2) in pid678718 is a true decision exit (the 0xFF path), and it is also not reachable from GF's current state. Its B-state at that moment is not in any of the 47 captures. OPEN.

## 6. What is left (not done, in order)

1. Translate FUN_140A18810 and FUN_140A62FE0 (small, the in-play key path; both translated already? checked: 140A62FE0 is in the translated set) and FUN_140A179C0 with its closure. Prove on the 3 gate-on captures plus the 6 D060/DC40 records. This needs a spec per root with the entry registers and the capture memory; the replay harness in `replay_fn.py` is the template.
2. Find the writer of B+0x296C's bit 4 (the B-state source of FUN_140430C30). Candidates: the caller of FUN_140430C30 at 0x1404335BB (a jmp, so its caller is one level up); not traced.
3. Only then GF: set the ball-state bit from GF's ball (once (2) is named), call the translated producer from GF's event stage, red-first test (GF writes a kind-8 record into ring 1 on the gate-on tick), lockstep, feed, race tests, the four match tests, gen --check, push.

## 7. Six stand-in closure bodies (answer to Main's question in round 12)

Unchanged from pes-port-StallTrap12 §7. None carries "hand-refined" in its first three lines, so gen --check does not skip them; gen --check not run this round.

## 8. Confidence

- Gate-on producer path reproduces live keys 3/3: CONFIRMED (replay, PES bytes; not GF).
- Decision returns false 24/24 in the captures, no true exit captured: CONFIRMED (replay).
- B+0x2918 bit 4 selects the in-play builder: CONFIRMED (disassembly + replay).
- Source of B+0x296C bit 4: OPEN.
- GF reproduces any of it: NOT DONE.

## 9. Scratch

`scratch/trace/`: replay_cap.py (producer and decision replay), replay_fn.py (call any VA with chosen registers over a capture), fields.py, alllist.txt, out19290b.tsv, out179C0.tsv, out_new.tsv. Repo untouched.
