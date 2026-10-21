# Far chase: the 13 m loose-ball gate in FUN_140536FC0 (S1Watch, round 18)

> **Main audit 19-10-26:** `0x140537332` `movss xmm0,[0x1425C6788]` (169.0), `comiss xmm0,xmm1`, `jb 0x14053798a` after the squared ball-vs-MA+0x2482C distance: matches. The 13 m gate is ball-to-team-point, applied to fresh picks and renewals alike (resolves the press-life/press-layer conflict). PARTIAL: the far-chase writer is unidentified (no live match loaded).

Status: **static decode CONFIRMED; live part NOT DONE** (no match loaded during the 4-minute recorder window, `MI = 0`). Tags: CONFIRMED = decompile or image read this round; INF = inference; OPEN = not resolved.

## 1. The 13 m compare: one site, one gate (CONFIRMED)

- Constant `0x1425C6788` = **169.0** (image read, f32 `0x43290000`), referenced at exactly one site, `0x140537332`, inside `FUN_140536FC0` (decompile line 156: `if (DAT_1425c6788 < fVar30*fVar30 + fVar28*fVar28) return;`).
- Operands at that site: `fVar28 = ballSample.x − (local_230+0x2482C)` and `fVar30 = ballSample.z − (local_230+0x24834)`. So the gate is **squared horizontal distance from the team point `MA+0x2482C` (x) and `MA+0x24834` (z) to the ball sample, compared with 169 (13 m)**. The ball sample is `FUN_140A57800(local_228)`, the predicted ball point used by the press writer (not the man).
- Guard for that branch (decompile lines 149–152): `FUN_140A627E0(lVar27,0) > 0x15`, i.e. **only when the carrier slot is 0x16 or higher (no carrier)**. With a valid carrier, the branch is skipped (press-life §3.1 item 12 agrees).
- Failure action: `return` before any candidate work. So the gate stops the whole writer; nothing is written and no man is picked (the `T+0xa548`/`0x34` writes are below it).

## 2. Fresh pick vs renewal: the same gate (CONFIRMED structure)

- `FUN_140536FC0` is called once, from `0x140536002` (caller `FUN_140535FE4`), and runs every frame for the team. The renewal of an existing PRESS is not a separate call: the candidate loop (`FUN_140A42940(team, iVar14, 0x34)` at the table write, decompile lines ~200) re-writes PRESS for each score-0 entry on every pass that reaches it.
- `FUN_140536690` (called from `0x140535FF4`) is the reset path. It clears a PRESS man to 0 when he is absent from `T+0xA548[0..3]` (`FUN_140A41190 < 0`), and rewrites PRESS or flips to DELAY at its tail (`FUN_140A42940(slot, 0x34 − (gate != 0))`, line 354).
- So for an existing presser: while the ball is within 13 m of the team point and the gate passes, the writer renews him; once the ball is beyond 13 m, `FUN_140536FC0` returns before renewing, and `FUN_140536690` resets the unlisted men to 0. **Both readings in the docs describe the same single gate.** Press-life §10's "renewal stops" and press-layer-port §1.7's "body gate" are one test, applied to renewal and to fresh picks alike. The gate has no separate fresh-pick branch.
- Consequence for a presser picked at 14 m (the flip-flop Main describes): the test is applied before the candidate fill, so a man cannot be picked at 14 m in the first place if the gate blocks the whole pass. A pick at 14 m that appears in a table could only come from a pass where the ball was within 13 m of `MA+0x2482C`, and the next tick the ball has moved beyond it (INF). The gate is on the **ball vs team point**, not the man vs ball, so an ordinary man-to-ball distance does not decide it.

## 3. What is not established

1. The 13 m gate is the press writer's own gate. It does **not** send a man to a loose ball beyond 13 m. For a man beyond 13 m with a loose ball, the sender is some other writer. I did not find it statically in this round (the candidates are the ball-chase states of the reaction entries, press-life §6 PASS_GET / `0x34` seek; OPEN).
2. The state words (`+0x5d0`) of the nearest men over time, and which writer sets them, need the live recording. Not measured.
3. Team point `MA+0x2482C` vs K+0x40 point: both are the team point in `FUN_14063A490` (default `MA+0x2482C`), so the press gate and the K+0x40 chain share the same point (CONFIRMED addresses, INF on the meaning).

## 4. Live part (NOT DONE, blocker)

Wanted: loose ball, nearest man > 13 m, the state word `+0x5d0` of the nearest men per frame, and the writer of the state that takes the chase. The recorder (`far/live.py`) ran 240 s with `MI = 0` (no match loaded), so zero rows. Retry needs a match in play with a loose ball away from all men.

## 5. Rule for FirstTouchPort2

- The press writer (`FUN_140536FC0`) is gated by `|ball − MA+0x2482C|² ≤ 169` (13 m) when there is no carrier. Fresh picks and renewals both pass through this gate. It is not a man-to-ball distance.
- Beyond 13 m, PRESS (0x34) is neither picked nor renewed; `FUN_140536690` resets it. A loose ball beyond 13 m is therefore not chased by PRESS. The sender of the far chase is outside this writer (OPEN; §3.1).

## 6. The ungated press arms (external Q&A, 20-10-26)
> **Main audit:** `0x1404f7955` = `call 0x140535C60` (the team think tick sub_1404F6C30 → the press pass): match. The state name table at `0x1434A4D50` (8-byte string pointers) reads: 0x1 BASE_POSITION, 0x11 WAIT_TIMER, 0x33 DELAY, 0x34 PRESS, 0x35 SAND, 0x36 MARK, 0x3A MATCH_UP. **Two corrections to the answer below:** its "SAND (0x33)" is DELAY (SAND is 0x35), and its "state 58 = chaser" is 0x3A MATCH_UP (the decimal state numbers 52/53 = PRESS/SAND are right).

The press pass sub_140535C60 (one call per team think tick) runs three arms:
1. **The 13 m gated assignment** sub_140536FC0 (§1). When it refuses, only its own candidates are lost.
2. **BallPlayer_PressingCoordinator** sub_140634470 (the setter sites 0x1406345ad / 0x140634993 / 0x1406349e0 / 0x140634a3a): per-slot maintain or release via sub_140632E70. Release unless status == 1, slot < 11, state == 52 (PRESS), the keep-behind-line check passes and the record timing holds. Ball grounded (h <= 1.5): cross-check the holder of state 58 (MATCH_UP per the table; the agent called it the chaser) via sub_140A62960; the presser survives only if he is that man or none exists. Ball airborne: needs the carrier id +120. Survivors get setter(team, slot, 0x34); releases go through sub_140A42B90(..., 0xFF), clearing the target word T+0xB88C + 2·slot. Its target eval **BallPlayer_PressingTargetEval** sub_140633940 picks the man: a scan over roles {1, 2, 16}, scored by sub_1404C8EA0 against dword_1425C73A4, keep-max, then the setter and the target word.
3. **The ungated sweep** at 0x140536059: for each slot whose mode field [T+0xBD48 + 4·slot] == 38 and whose state is not already 52/53, sub_14062AF10(player) chooses 0x33 (DELAY, per the table) or 0x34 (PRESS) by opponent-side trap/interruptible checks, then the setter. It runs regardless of the 13 m refusal.

So when the 13 m gate refuses, the same pass's ungated arms (the sweep and the coordinator) still arm men; picking happens in the target eval (max score over role-filtered candidates). Assignments and the man word +0x5D0 share this one enum (the 96-entry table above).

**Live cross-reference (§ rounds 32-33, pes-re-live-gk-chase.md):** on far loose-ball frames the men's +0x5D0 toggled 0x1 ↔ 0x36, which is **BASE_POSITION ↔ MARK**: in that sample nobody was in PRESS or DELAY while the ball was far.

Open (the agent's): the semantics of mode == 38; the press-target word's content for a loose ball; the sweep's side scope (slot < 11).

## 7. Live answer
See pes-re-live-farchase.md: the coordinator arm (sub_140634470 → sub_140633940) supplies 99% of far-ball pressers; the ungated sweep almost never arms one on far loose frames.
