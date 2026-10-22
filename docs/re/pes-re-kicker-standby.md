# PES kicker standby actions: Update, slot-10 hand-over, placement, and hand-over to the kick (decode, 21-10-26)

> **Main note 22-10-26:** DodgeGate's decode, not yet Main-audited. Headline: only the kickoff loops (22/23) let locomotion run; every other kicker standby holds one idle clip; the taker is PLACED at ready by SetKick, and the motion comes from the kick action's clip and root adjusters; hand-over by sub-id. GoalKickStandby's vtable is 0x1425E0BB0.
> **Main audit 22-10-26 (bytes):** the corner SetKick `0x1407331A0` reads 45.0 at `0x14259BE30` and 315.0 at `0x1425AA1C0` (`0x14073325B`/`0x140733265`), chosen by `FUN_1406E6660(AP,0x34) == 1`; §4 is right. §12's `DAT_1425CCA48/CCA34/CCA50/14259D618` hold 292.5/67.5/337.5/22.5 and are not read by that function. The heading composition is `FUN_140732D40`: heading = wrap360(wrap360(yaw) + record+0xC) (`FUN_1404BEF90`), stand = spot + RotY(heading)·(−hit.x, hit.y, −hit.z), sine table `FUN_140A92C80`. The record+0xC yaw is the bearing from the corner spot to that end's penalty spot (`FUN_1408B4560` kind 4, DodgeGate). The throw-in stand is 0.7958 m from the SetKick formula (live 0.773 m at body scale 0.972), not the 1.02 m row.

Tags: CONFIRMED = decompile (headless dec.sh, this session, `dec/standby.txt`, 31 functions) or image bytes (pes_full.bin, VA − 0x140000000) or the resolved vtable table (`FoxRegistryRE/actions.json`); INF = inference; OPEN = not resolved here.

## 0. Method and one correction

- Decoded with one headless `dec.sh` run (1 GB heap, `-noanalysis -readOnly`). Image checks by capstone on `pes_full.bin`. Vtables read from the image: slot k is at vt + 8k.
- **Correction to the audit line in `pes-re-goalkick-taker.md` (Main audit 20-10-26):** the GoalKickStandby vtable is **0x1425E0BB0**, not 0x1425E0BD0. The RTTI COL sits at vt−8 = 0x142C9D780 for BB0. At 0x1425E0BD0 the qword at vt−8 is the code pointer 0x141794410 (a `ret`), so BD0 is a mid-table pointer. Under BB0: slot 4 (+0x20) Update = 0x1407748F0, slot 10 (+0x50) skip-check = 0x140775730, slot 12 (+0x60) SetKick = 0x140774AE0, slot 2 (+0x10) CanEnter = 0x140782330, slots 3 and 5 = `ret` (0x141794410).

## 1. The per-tick frame (CONFIRMED)

- `FUN_1406F679E` (0x1406F679E–0x1406F6937) calls `FUN_1406F0C20` (0x1406F0C20–0x1406F0F61) first (call site 0x1406F67AB). Neither has a direct caller and neither has an 8-byte data reference, so the caller is OPEN (INF: the per-tick anime step, reached through a function pointer).
- `FUN_1406F0C20` is the **default locomotion driver**. Its first actions: `FUN_1406FB3B0(AP, ctx+0x2C)` (the steering vector from the caller's record; INF: the AI's move target), then `skip = GetAction(AP+0xC40, AP+0xAC8)->slot10(action, AP, ctx)` (call at 0x1406F0C93, the `+0x50` call at decompile line 2933). **If `skip` is true the driver returns**: no default steering. If false, it runs the steering branch (layers via `FUN_1406FA9F0`/`FUN_1406FAAA0`, MOVE and STEP_MOVE tested by sub-id, clip-id tests 0x1CD, 0x8D1, 0xC3A, 0xC3B, 0xBE0, 0xBC9, 0xC15, 0xC5, 0xC6, and predicates `FUN_140A6E4D0/FUN_140808EF0/FUN_140A6E290/FUN_140A6E2E0/FUN_140A606E0/FUN_140A73E20/FUN_140A6E260`; their meaning OPEN).
- So **slot 10 = "skip the default locomotion"** (CONFIRMED by that branch). Every standby below calls its slot 10 each tick, and the return value decides whether the body is steered by the default locomotion.

## 2. The standbys: vtable slots, Update, skip (slot 10), SetKick, CanEnter (CONFIRMED; vtables from the image and the resolved table)

Enter (slot 3) and Exit (slot 5) are `ret` (0x141794410) for every row here. CanEnter (slot 2) is the shared loop test `sub-id == 1` (0x140782330) for 62, 30, 49, 52, 53, 22 and 23; the other rows have their own (table).

| id | action (vtable) | Update s4 | skip s10 → return | SetKick s12 | CanEnter s2 |
|---|---|---|---|---|---|
| 62 | GoalKickStandby (0x1425E0BB0) | 0x1407748F0 | 0x140775730 → **1** | 0x140774AE0 | 0x140782330 |
| 30 | FreeKickLoop (0x1425DF908) | 0x14073E150 | 0x140742D90 → **1** | 0x1407411F0 | 0x140782330 |
| 34 | FreeKick2ndLoop (0x1425DFAD8) | 0x14073DE80 | 0x140742BC0 → **1** | 0x1407410A0 | 0x140741FE0 |
| 49 | CornerKickLoop (0x1425DF568) | 0x140732EB0 | 0x1407343A0 → **1** | 0x1407331A0 | 0x140782330 |
| 52 | PostManLoop (0x1425DF738) | 0x140733000 | 0x140734430 → **1** (both branches) | 0x1407333E0 | 0x140782330 |
| 26 | ThrowinLoop (0x1425E1B30) | 0x140793590 | 0x1407952D0 → **1** | 0x140794280 | 0x140794FA0 |
| 53 | PenaltyKickLoop (0x1425E14D8) | 0x140782770 | 0x140742BC0 → **1** | 0x140782930 | 0x140782330 |
| 22 | KickoffKickerLoop (0x1425E1050) | 0x1407819F0 | **0x141863350 = false** | 0x1406DEB60 (none) | 0x140782330 |
| 23 | KickoffReceiverLoop (0x1425E1220) | 0x140781FC0 | **false** | 0x1406DEB60 | 0x140782330 |
| 24 | KickoffOtherLoop (0x1425E1308) | 0x140781B30 | 0x1407823D0 → **1** | 0x1406DEB60 | 0x140782360 |
| 40 | QuickRestartReady (0x1425E0240) | 0x14073F750 (not decoded) | 0x140742F00 (not decoded) | 0x1406DEB60 | 0x140742180 |

Only the two kick-off loops (22, 23) leave the default locomotion running. Every other standby holds the body and skips it (CONFIRMED from the returns).

## 3. What each standby does (CONFIRMED from the decompile unless marked)

Clip plays go through the interface's play slot (`vt+0x10`, the MOVESTEP handoff path) and `FUN_1406F0F70(AP, clip, t0, mirror, rate, blend, 0)`. Usage values are read from the calls; parameter names of `FUN_1406F0F70` are INF from the anime-select doc.

**GoalKickStandby (62), Update 0x1407748F0.** Phase 0: row 0x84 (0x83 if mirrored; `FUN_1406E6660(AP,0x34)`), col 9, `goalkick_idle` (clip 1709, `gkgoalkick_idle_0_0`), rate 1.0 (DAT_14256AE1C), blend 0, → phase 1; sets the action byte +8 to 4. Phase 1: while request kind `req+0x774` < 2, `FUN_1407755D0` plays a coaching gesture (table DAT_1425E8F58[kind·2+mirror], not decoded: OPEN). Phase 2: `FUN_1407754C0`, back to phase 1 at clip end (OPEN body). **Moves the body: nothing** (no move target; the slot-10 call writes only the layer target ctx+0x2C).
  - In play (`MI+0x116C == 1`, INF: the doc's in-play flag): speed class → AP+0x245A (FUN_14081E340), foot band → AP+0x245B (vt+0x78), `FUN_1406EB440`, AP+0xAD0 |= 1, then inline **MOVE hand-over**: AP+0xAC8 := 2, AP+0xAC9 := 2, `GetAction(2)->Enter`.
- Slot 10 `0x140775730`: `FUN_1406FB3B0(AP, ctx+0x2C)`, `FUN_1406FA9F0(AP,0)`, `FUN_1406FAAA0(AP,0)`, `FUN_1406E6620(AP,1)`, `FUN_1406FB440(AP,0)`, `FUN_1406FB340(AP,1)`, return 1.
- **SetKick 0x140774AE0** (the placement, below): clip 0x6B1 (`gkgoalkick_instep_0_0`), mirror from `FUN_1406E6660(AP,0x34)`, yaw 0.0, returns 1.
- **CanEnter of the kick (63/64) 0x140774C50**: returns `AP+0xAC9 == 0x3E` (decompile).

**FreeKickLoop (30), Update 0x14073E150.** Phase 0: stores the spot (ctx+0x20 → the action's own +8..+0x10), plays `fk_idle` (row 0x507, col 9, style-remapped by `FUN_1406B3560/3780` kind 2), mirror by `FUN_1406E6660`; → phase 1, or phase 2 when a second kicker exists (MI+0x1740 < 0x16). Phase 1: aim/turn loop with re-spot (clips 0x4D1–0x4DB by a switch, per-clip blends, not fully decoded: OPEN). In play → MOVE. **Moves the body: nothing while held.**
- Slot 10 `0x140742D90`: if clip id `AP+0xAE0 == 0xE29`, the target is the designated spot `*(G+0x2AA8)` (sign-flipped by the team record's +0x254 flag), written to AP+0x3760 and the layer (`FUN_1406FA9F0(AP,1)`); otherwise `FUN_1406FB3B0(AP, ctx+0x2C)`. Always returns 1.
- **SetKick 0x1407411F0**: clip 0x509 (`fk_runup`, style-remapped), offset negated, yaw = clip yaw at contact (`FUN_1406B1C70`) folded by DAT_14259858C / DAT_142598598 (±360), placer; clamp to |x| ≤ F[0] + DAT_14259BDDC (1.5), |z| ≤ F[1] + 1.5.
- **CanEnter of the FK kick (31-33, 37-39) 0x140742030**: returns `sub == 0x1E || sub == 0x22` (disassembly: cmp 0x1E, cmp 0x22).

**FreeKick2ndLoop (34), Update 0x14073DE80.** Phase 0: plays clip 0x50D or 0x50B (by a test at the start, not decoded), rate 1.0, blend 0, → phase 1. In play → MOVE. Slot 10 `0x140742BC0`: ball position → AP+0x3760, flag AP+0x2460 bit 26 (0x4000000), layer off, return 1. CanEnter `0x140741FE0` (not decoded: OPEN).

**CornerKickLoop (49), Update 0x140732EB0.** Phase 0: plays **row 0x507 col 9** (`fk_idle`), mirror = (`FUN_1406E6660(AP,0x34) == 1`) → `AP+0x2472`, bit 36 of AP+0xAD0; `FUN_1406F0F70(AP, clip, 0, mirror, 1.0, 0, 0)`; AP+0x2471 = AP+0x2478 = 0; phase → 1. In play → MOVE (inline, as GK). **Moves the body: nothing while held.**
- Slot 10 `0x1407343A0`: ball position `FUN_140A6CF70(G+0x40, 1)` → AP+0x3760 (vec3 copy), AP+0x2460 |= 0x4000000, `FUN_1406FB440(AP,0)`, `FUN_1406FB340(AP,1)`, return 1.
- **SetKick 0x1407331A0**: run-up clip 0x509 (`{0x509, 9}`), offset negated, yaw = 315.0 (DAT_1425AA1C0) or 45.0 (DAT_14259BE30) by side, placer; clamp x/z to the field bounds (`FUN_141E5D4C0`, margin DAT_142565600 = 0.5); AP+0x2460 bit 30 (0x40000000) set when clamped.
- **CanEnter of the corner kick (50/51)**: `0x1407335C0` is `jmp 0x143FB8D40`; the target (in `.impdata`, bytes `sub rsp,0x28; lea rcx,[rdx+0xAB8]`) returns `sub == 0x31`. Note the vtable entry points at the thunk, not at the function body.

**PostManLoop (52), Update 0x140733000.** Plays clip row 0x1BA (postman_loop, the doc's m2934), col 9, rate 1.0, blend DAT_142598578 (0.2), → phase 1. In play → `FUN_1406EBB0(AP, 2)` (ChangeAction to MOVE). Slot 10 `0x140734430`: `FUN_1406FB3B0(AP, ctx+0x2C)`, AP+0x3760 := ctx+0x2C (with ctx+0x30/0x34), bit 26; if `ctx+0x38` is set: layers off, return 1; otherwise layer on, `FUN_1406FB440(AP,1)`, return 1. **Moves the body: only through the layer target (OPEN what the layer does).**

**ThrowinLoop (26), Update 0x140793590** (phases 1–6, phase byte AP+0x245C; sub-id tests in CanEnter). Phase 3 (turn start): plays `0x9C2 − mirror` (row 4 or 0xD) through `FUN_1406F0F70(AP, clip, DAT_14259D5CC (0.42), 0, DAT_14259D5D8 (1.4), DAT_1425C48A0 (0.15), 0)`; → phase 4. Phase 4 (turn): `FUN_1406AF620` frame test, then phase 1 or 3; yaw adjuster `FUN_141EB5B30` when the position error exceeds DAT_1425A8A28 (0.05). Phase 5 (release start): plays **clip 0xCFA** with rate DAT_1425AB120 (0.85) and blend DAT_142590D98 (0.3); **position adjuster** `FUN_141EB37B0(AP+0x428, AP+8, frames)` with frames = fps·0x11/0x36 (17 at 54 Hz), `FUN_141EB3AE0` (apply); yaw adjuster `FUN_141EB35A0(AP+0x4D0, facing, angle, 0xD, 1, 0)` (angle ±DAT_14259BE34 = 50.0 about the facing, sign by the aim); → phase 6. Phase 6: wait (fps·0xF)/0x36 = 15 frames, then phase 1. Phase 1/2 (idle/aim): `FUN_1406F1280(AP, 0x5E, 0, mirror, 1.0, DAT_142598578 (0.2), DAT_142565600 (0.5), 0)` (the aim group, INF), rate from `AP+0x14` via `FUN_141AF5100`.
- Slot 10 `0x1407952D0`: in phases 3–4 it takes the record point (`FUN_140A48A10(G, slot)+0x5B4`, the taker's team record, INF), the facing rotated by ±DAT_14259BE34 (50.0), and places an aim target 5.0 m ahead (DAT_14259222C) through `FUN_1406FB3B0`; returns 1 always.
- In play → `FUN_1406EBB0(AP, 2)` (MOVE).
- **SetKick 0x140794280**: clip 0xCFA's contact offset (`FUN_1406E8CC0(AP, out, 0xCFA, 0, 1, …)`), x and z negated, y zeroed, yaw 0.0, placer. **The stand point is over the spot with the release point over R** (matching the doc's "the taker stands so the ball at release is over R").
- **CanEnter of the throw-in kick (27/28) 0x140794F20**: `sub == 0x1A`, or `sub == 0x1D` with a further test at 0x140794F48. **CanEnter of ThrowinLoop itself 0x140794FA0**: `sub == 1`, or `sub == 0x1D` with a further test at 0x140794FCA.

**KickoffKickerLoop (22), Update 0x1407819F0.** Phase 0: plays **row 0xC2** (`kickoff_loop_kicker`, col from the stack, doc col 9), `FUN_1406F0F70(AP, clip, 0, 0, 1.0, 0, 0)`, `FUN_1406F02F0(AP, 6, 0xC)` and `(AP, 5, 0xC)` (layers 6 and 5 set to 0xC), AP+0x2472 = 0, bit 36 cleared, phase → 1. In play → MOVE. **Skip (slot 10) is false: the default locomotion steers him.** CanEnter of KickoffKicker (25) 0x140782310: `sub == 0x16`.

**KickoffOtherLoop (24), Update 0x140781B30; KickoffReceiverLoop (23), Update 0x140781FC0** (833 bytes, not read in full: OPEN). Receiver's slot 10 is false (default locomotion runs).

**PenaltyKickLoop (53), Update 0x140782770.** Phase 0: plays `pk_idle` (row 0x8EA, style kind 4, doc), `FUN_1406F0F70(AP, clip, 0, mirror, 1.0, 0, 0)` → phase 1. Phase 1: waits; in play → speed class, foot band, `FUN_1406EB440`, MOVE (`AP+0xAC8/AC9 := 2`), Enter. Slot 10 `0x140742BC0` (shared with 34): ball position → AP+0x3760, bit 26, return 1. **SetKick 0x140782930**: `pk_runup` (0x8EB, style kind 4): stand = ball − the clip's rotated root offset at contact, yaw = the clip's contact yaw (doc; the offset is `FUN_1406E8CC0`). CanEnter of PenaltyKick (54) 0x140782AF0: `sub == 0x35`.

## 4. Placement (SetKick) arithmetic (CONFIRMED)

- `FUN_1406E8CC0(AP, out, clip, mirror, …)`: out = the clip's contact root offset (`FUN_1406AFF90` on the clip; cm record scaled by DAT_14256AE18 = 0.01), times the body scale `AP+0x3B18` clamped to [DAT_1425DDF78 = 0.9166, DAT_1425DDF80 = 1.0833], and x negated when `mirror` (DAT_142588218 = −1.0).
- `FUN_140732D40(this, headingOut, posOut, offset, yawPtr, spot, spot+0xC)`: heading from the yaw (`FUN_1404BEF90`), then `stand = spot + Rot(heading)·offset`, with the 90° pair from DAT_14259BE5C (90.0) and `FUN_140A92C80`. The callers negate the offset first (`^ DAT_1425656D0`, the sign mask).
- Yaw per standby: GK 0.0; throw-in 0.0; FK = clip yaw at contact (wrapped ±360); corner 315/45 by side; penalty = clip contact yaw.

## 5. Hand-over to the kick action (CONFIRMED tests)

Every kick action admits the **sub-id of the standby that issued it** (AP+0xAC9), so the kick command is the standby's own anime id:

| standby | kick action | CanEnter (kick) | admits |
|---|---|---|---|
| 62 GoalKickStandby | 63 / 64 GoalKick* | 0x140774C50 | sub == 0x3E |
| 30 FreeKickLoop, 34 FreeKick2ndLoop | 31–33, 37–39 FreeKick* | 0x140742030 | sub == 0x1E or 0x22 |
| 49 CornerKickLoop | 50 / 51 CornerKick | 0x1407335C0 → 0x143FB8D40 | sub == 0x31 |
| 26 ThrowinLoop | 27 / 28 Throwin* | 0x140794F20 | sub == 0x1A (or 0x1D + test) |
| 22 KickoffKickerLoop | 25 KickoffKicker | 0x140782310 | sub == 0x16 |
| 53 PenaltyKickLoop | 54 PenaltyKick | 0x140782AF0 | sub == 0x35 |

The kick's own entry is the restart wrapper: `GetAction(AP+0xC40, id)->slot12(…)` (SetKick above), called from `FUN_14092E7E0` by the restart's MODE byte (`MI+0x1554`); the wrappers are skipped while `MI+0x116C == 1` (in play). So **the placement happens at the restart's ready moment, before play**. The kick is then issued by the AI command with the standby's sub-id.

## 6. Where the body moves during a restart (CONFIRMED summary)

- While a standby holds (its Update and slot 10 both return 1): the body is held in one idle clip; no default steering; no MOVESTEP (MOVESTEP runs only in MOVE, STEP_MOVE, DRIBBLE_STEP_MOVE, DEMO_MOVE: the 2026-10-21 caller map). **The body does not walk to the spot.** Its position is the SetKick placement.
- The motion comes from the kick action's clip with its root adjuster: the GK kick Update's root warp `FUN_141EB39A0(AP+0x428, contact, AP+8, frames, …)` (standby.txt line 558, GK kick body 0x140774890), the yaw adjuster `FUN_141EB35A0(AP+0x4D0, …)`, and the contact flag `FUN_1406EC260(AP, ticks, …)` → AP+0x2460 |= 0x400. The GK strike phase 0 (0x140775730 is its slot 10; the strike is 0x140775780) plays the strike clip and the yaw adjuster, then phase 1.
- The throw-in loop moves the body only through its release clip 0xCFA and the position adjuster AP+0x428 (toward AP+8, a hold over 17 frames) and its yaw aim.

## 7. The GF port, as read in this session (CONFIRMED by reading the code)

- **Goal kick:** the keeper **walks** to his stand point by MOVESTEP (`TeamAIController::PrepareGoalKickStand`, `pieceWalk = true`, teamAIcontroller.cpp:1791–1796). PES **places** him at the stand point (SetKick, §4) and holds him. The strike is `AdvanceGoalKickStrike` (a stand-in for the root warp).
- **Free kick, corner, penalty:** GF sends the taker **running to the ball from prepare** (elizacontroller.cpp:498–515, a Movement command `kPieceTakerApproachGain · gap`, comment: "PES's approach to a free kick, corner or penalty is not decoded; the run to the ball stands in for it"). PES places him (SetKick) and the run-up clip (0x509 / pk_runup) carries him at release. Release is by frame counts (`pessequencer.cpp:70–90`: corner 365, FK 355, penalty `kPenaltyReleaseFrames`), with no arrival requirement.
- **Throw-in:** path A (seamless, `pieceWalk = seamlessRestart`) walks to the ball by MOVESTEP (elizacontroller.cpp:150–190, `IsPieceTakerWalking`, `useDesiredTarget = spot`, velocity `FloorVelocity(2·gap)`) and must pick the ball up within `throwInPickupReach_m` (AdvancePieceWalk, teamAIcontroller.cpp:1148–1154) before `PieceTakerArrived()` releases it (referee.cpp:243). Path B (ceremony) is placed "outside the line with the ball, and does not walk" (teamAIcontroller.cpp:1797–1803).
- **Kick-off:** the formation places the kickers; PES holds `kickoff_loop_kicker` with the default locomotion **on** (slot 10 false).

## 8. What this means for the stall (INF until the probe in §10 confirms)

- All stalled runs (rows 1–4 in `pes-port-DodgeGate.md` §8) stop at **restart 5, the throw-in**. `ReleasesRestart` for a throw-in needs `PieceTakerArrived()`, which needs `pieceCarrying`, which needs the taker within 1 m of the ball (AdvancePieceWalk). In row 1 the taker stood at 10.8 m in MOVESTEP IDLE. So the **path-A walk**, not a standby, is the blocker for the throw-in. In PES the seamless approach is a MOVE walk (INF: no standby precedes the pickup; ThrowinLoop's CanEnter admits only sub 1 or 0x1D), so the walk is MOVESTEP in PES too. The standby port alone does not repair this.
- The **gate-on stall** (row 3, A10 in `near_delay_standby_goalline_noloop`, `frame 2/208`, MOVESTEP IDLE) is the same walk failing, not a standby being held.

## 9. Open items

1. Semantics of the layer and anime helpers: `FUN_1406FB3B0`, `FUN_1406FA9F0`, `FUN_1406FAAA0`, `FUN_1406FB440`, `FUN_1406FB340`, `FUN_1406E6620`, `FUN_1406F02F0`, `FUN_1406F0F70` (parameter names), `FUN_141EB37B0/35A0/39A0/3AE0`, `FUN_141AF5100`. OPEN.
2. The caller of `FUN_1406F679E` (reached through a pointer), and whether `ctx+0x2C` / `ctx+0x38` are the AI's move target and stop flag. INF.
3. GK phase 1/2 gestures `FUN_1407755D0`, `FUN_1407754C0`; FK phase 1 aim/re-spot (lines 790–1138 of the dump, not read); FK2 clip choice (0x50D/0x50B); receiver and other kick-off bodies; quick restart (40) body.
4. The kick action bodies (CornerKick 0x140732E30, FreeKick 0x14073DFC0, ThrowinNormal 0x140792E10, PenaltyKick 0x1407826F0, KickoffKicker 0x140781780). Only the GK strike (0x140775780) and the GK kick Update (0x140774890) were read here.
5. The corner CanEnter resolves into `.impdata` (0x143FB8D40), a `sub == 0x31` test. Confirm it is a real function in the executable, not an import-table artefact.
6. Which clock PES uses for the free-kick and corner release (the standby holds; the release is by the AI command), since GF's 355/365-frame timers are "live" measurements (pessequencer.cpp:22–23, "DOC 189 not reproduced").

## 10. Port implication (INF; for the decision)

- A faithful standby port means: (a) placement at ready by SetKick for every taker, (b) holding the standby clip with the default locomotion skipped (the gate's rule, already in the port), and (c) at release, the kick action's run-up or strike clip with its root adjuster to the contact. (c) needs set-piece clip families, root-displacement playback, and contact timing, which the port does not have. This is a multi-commit port.
- The throw-in stall (§8) is a MOVESTEP walk problem before any standby is involved. I propose a print-only probe on the throw-in path-A walk before choosing the port: taker distance, command velocity and target, MOVESTEP state, each second of a restart.

## 11. Throw-in walk probe (21-10-26): the MOVESTEP loop under the throw-in taker

Probe: print-only at three sites (referee Process top, elizacontroller walk branch, ProcessMoveStep tail), reverted; tree clean at 28f87c7. Log: `<workdrive>/gf-DodgeGate-scratch/run_trace5.log` (Release, no gate, 5-min seed 7, `--pes-loop`). The set-piece sequence matches row 1 exactly (seven events), so the probe did not change the simulation.

- Taker H6, restart 5, armed at ms 144,000. The controller's walk command is class 3, 8.00 m/s, precise target (CONFIRMED, the walk branch).
- ms 143,888 to 150,000: IDLE (1) and GOTO_RUN (7) alternate every two ticks. Each arm issues a motion (`issued=1`), the clip restarts at frame 0, and the speed stays at 0 to 1.4 km/h. Each arm selects a different idle clip. The dwell that ends each state is 0.03 s: `SLOW_DWELL` (movestep_run.cpp:556), set by the live-seconds arm (lines 553-557) when `GroundSpeedKmh < kmh5`; the arm then calls `RequestState(IDLE)` (line 562). Below 5 km/h the arm fires again, so the loop feeds itself.
- Class -1 appears only on the walk's first tick (ms 143,888). It is not the loop's driver.
- ms 152,000: pickup (carrying = 1), carry target (-16.32, 34.76). The carry cycles RUN_TURN_STEP (16), RUN_LOOP (3), ACCELERATE (9), STEP_LOOP (4), TURN_STOP (6), GOTO_RUN (7), STOP (8), IDLE (1). STOP is entered at 153,574 with the first dwell at 0.04 against a zeroed limit. The taker drifts to (-23.5, 34.6) by 173.8 s, 7.2 m from the spot, while the command still asks for 8.00 m/s.
- Stall: the restart is not started by ms 173.8 s (1758-tick limit), exit 1, as in row 1.
- Correction to section 8 of `pes-port-DodgeGate.md`: row 1's restart stall is at match time 173.8 s. The "26.0 s" there is the stalemate idle counter, not the match clock.
- Verdict (INF until the fix): the throw-in stall and every MOVESTEP walk (the free-kick, corner and penalty run, the keeper walk) fail through this loop. The loop belongs to ClipWords' MOVESTEP work and is handed over as its acceptance case. The standby port (section 10) does not repair it.

## 12. Placement port and the run-up chain (DodgeGate2, db5ae8a)

Decoded this session (CONFIRMED: `dec/style.txt`, `dec/standby.txt`, Ghidra bridge xrefs and decompile of 0x1406AB270 and 0x1406AB310).

- **Offset source.** `FUN_1406E8CC0(AP, out, motion, mirror, index, flag)` reads the record through `FUN_1406AFF90(table, motion, index, flag)`. With flag 0 the output is the record's (+0x2C, +0x28, +0x24) fields in cm, which are ballX, ballY, ballZ (mbinfo.hpp). It scales by 0.01, clamps the body scale `AP+0x3B18` to [0.9166, 1.0833] for x and z (y takes the scale itself), and negates x for mirror. **The index is an argument**: goal kick 0x6B1 at index 0; throw-in 0xCFA at index 1 (the call in `FUN_140794280`).
- **Motion remap.** `FUN_1406ABB30(table, &motion, &kind, flag)` is the anime cell at row = motion, column = kind (18 columns), returning the cell's motion or −1 (0 with flag 0) when empty; (0x7C5, kind 2) → 0x9B4. GF already ports this table (`src/pes/animetable.cpp`, 0xBE5 rows, 0x12 columns, the same patch). Its base is `*(_DAT_1436F2378)`. The only WRITE to that global is the clear in `FUN_1406AB270` (unload); the loader is OPEN.
- **Player remap.** FK (kind 2) and penalty (kind 4) then pass the motion through `FUN_1406B3560` and `FUN_1406B3780` over the object at `_DAT_1436F2600` (`FUN_1406B3910`), keyed by `FUN_1406EA7B0` / `FUN_1406EA760`. Records map an original motion to a remapped one; hard-coded keys 0x20DA3 / 0x20D30 (0x4D1→0x4D4 and 0x4E0→0x4E3 at kind 2; 0x94C→0x93F at kind 1). **GF does not load this object.** The corner calls only `FUN_1406ABB30`, with no player remap.
- **Contact yaw.** `FUN_1406B1C70(table, motion, flag, index)` = bits 13..23 of the record's first word, minus 0x36E, scaled by DAT_142588218 (flag ≠ 0) or DAT_14256AE1C.
- **Corner constants.** The corner SetKick (lines 1472–1491 of `standby.txt`) reads the motion 0x50C (0x509 when the restart record's byte +0x11 is set), kind 9, mirror = the taker's preferred foot, and the heading yaw DAT_1425CCA48 (315) or DAT_1425CCA34 (45) by side (DAT_1425CCA50 / DAT_14259D618 under the flag). **CONFLICT:** section 4 above lists DAT_1425AA1C0 / DAT_14259BE30 for the same placer. One reading is wrong; resolve before the corner port.
- **Hit-record data (install, `Hits` read by a diagnostic test).** Motion 0x6B1: record 0 ball (−240.00, 11.55, 370.00) cm = 4.410 m; root (−169.85, 340.92) cm. Motion 0xCFA: record 0 ball (−1.06, 193.76, −2.51) cm = 0.027 m, root (0, 0); record 1 ball (2.24, 184.69, 79.55) cm = 0.796 m, root (1.10, 14.07) cm.
- **Throw-in correction.** Path B's SetKick reads record **1**, not record 0 (record 0 is the hand at the root). At body scale 1 the stand is 0.7958 m outside the line. The live observation in section 4.1 of the choreography doc (|z| = R.z + 0.773 at body scale 0.972, 4 of 4) is what this formula gives. The 1.02 m placement in the doc's table (line 48) and at line 568 is **not** reproduced; that claim is OPEN.
- **Old goal-kick note.** The earlier walk in section 7 is gone (port below).

**Port status (db5ae8a, tests in `tests/sim/pes_setpiece_stand_test.cpp`):**

- Goal kick: the keeper is placed at prepare (`PrepareSetPiece` ResetPosition to the stand point), held, and the walk branch in `elizacontroller.cpp` is deleted. Test `SetKickStand.TheGoalKeeperIsPlacedOnTheStandPointAtEverySpot` is green. Mutation (placement removed): red, 0.96 m off at spot (−47, 3).
- Throw-in path B: placed at prepare at the record-1 stand, held; `kThrowInStandIn_m` is removed. Record-level test `SetKickStand.TheThrowInStandIsTheReleaseKeyOffsetAtTheLiveBodyScale` is green. The end-to-end test `PesRestartDelay.AThrowInCeremonyTakerIsPlacedOutsideTheLineAndReleasedWithTheBallInHand` now follows the first ceremony stoppage over `kMaxSimTicks`. On seed 7 it finds **no** ceremony before the walk stall, so it is red.
- Free kick, corner, penalty: **not ported.** Blockers: the corner heading composition (the placer's yaw argument, `FUN_140732D40`, not decoded this session) and the corner constants above; the free kick and penalty also need the player remap object `_DAT_1436F2600` (loader not decoded).
- Hold: `IsPieceTakerHeld` (ponytail: an idle stand, not the PES standby clip). It ends at the release, when the match turns in play (`referee.cpp:695`).

**Baseline and stall (5-min and 20-min seed 7, same test file).** On the committed controller (28f87c7), `AThrowInCeremonyTaker…` finds no ceremony, and `AGoalKickIsReleasedOnThePesLadder` stalls at the keeper's walk (H4, `near_delay_standby_goalline_noloop`, frame 1/208, MOVESTEP IDLE) and releases no goal kick. With the placement, the keeper's walk stall is gone. The run then stops at a kickoff restart (piece 1, H4) and a throw-in walk (piece 5, H2, forced from 1.1 m); the throw-in walker A9 is forced from 15.0 m at (36.2, 34.3) in the ceremony run. **No goal kick is released in either run, so the strike is not exercised.** Both tests are red before and after; the walk stall is ClipWords' loop (section 11).

**Verification run (this session):** `pes_setpiece_stand_test` 2/2 green on db5ae8a. Build of `pes_restart_delay_test` and `pes_setpiece_stand_test` clean. The release repro (5-min seed 7, gate on) is held per Main.
