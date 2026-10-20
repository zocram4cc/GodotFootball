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
