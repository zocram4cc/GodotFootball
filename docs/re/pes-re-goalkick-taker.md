# PES21 goal kick: how the keeper reaches the ball, and what releases the kick

Source: an outside RE agent (Q&A, 20-10-26), audited by Main against the image.

> **Main audit 20-10-26:** GoalKickStandby vtable `0x1425E0BB0` (corrected; 0x1425E0BD0 is mid-table): slot 0 = `0x1407748F0` (Update), slot 6 = `0x140775730` (clip-layer config), slot 8 = `0x140774AE0` (SetKick): match. `sub_142136F00` decompiled: phase word unit+0x48, elapsed = unit+0x40 − unit+0x44, phases 0-5 as below; constants `0x142565600` = 0.5, `0x14258c264` = 3.0, `0x142590d98` = 0.3, and `[+0x14c4] == 1` (5316): match. `0x14259d5d0` = 0.65; its compare at `0x140782f61` is in the penalty strike-power gate: match.

## The release is a timer ladder, not reach
ThinkUnitSetPlay's MODE 3 branch (sub_142135C50, gated on the designated taker [MI+5948]) runs sub_142136F00 (fps = FUN_141e532d0, i.e. DAT_14351ae54; each wait is `(uint)(fps·k + 0.5)` frames):
|phase|what|
|---|---|
|0|counts on-pitch opponents, but the count is dead: `0x142136FBB` stores 5 when none, then `0x142136FC2` stores 1 unconditionally (Main-verified), so phase 1 always follows|
|1|wait 1 s|
|2|0.5 s window: sub_142132660 (short target) every eval; a hit → command 2 (pass) to that receiver, type 0 (ground). Early-out to phase 3 when [+0x14c4] == 1 and a value < 0.3|
|3|wait 3 s (early-out on a valid special object; [MI+5316] == 1 → command 11)|
|4|0.5 s window: sub_142135230 (lofted target); a hit → command 2, type 1 (lofted)|
|5|forced: receiver 255, lofted|
fps = 54.0 (`0x14351AE54`). Phase-entry ticks from phase-0 tick F (ThrowInTaker's disassembly): default (MI+0x14C4 == 0): phase 2 at F+54, phase 4 at F+81, the first lofted emission at F+82, phase 5 at F+108, forced at F+109; special (MI+0x14C4 == 1 and tactics+0x18 < 0.3): phase 3 at F+81, phase 4 at F+243, the first emission at F+244. The live 83/245 = emission + 1 frame (inference: observation latency).

Placement (ThrowInTaker): SetKick stores into no keeper field; it writes through two out-pointers via the placer: position = spot.pos + R(spot.heading)·(−clip 0x6b1 root offset, mirror FUN_1406E6660(player, 0x34), x/z negated by the sign mask at `0x1425656D0`); heading = spot+0xC (not a literal 0). The teleport fits EV-SPC-0001 (live: placement in the 20-tick 0x14 window under the wipe). Open: the link from SetKick's out-pointers to the keeper's fields (the vt[8] caller is indirect).

**Reach is dead:** `0x14259d5d0` (0.65) is a compiler-folded constant with 40 consumers; its only compare uses are the penalty strike gate (`0x140782f61`) and sub_140A343E0. Nothing in the goal-kick chain reads it.

## He doesn't walk to the ball
- GoalKickStandby::SetKick (`0x140774AE0`) places him at the stand point: clip 1713 `gkgoalkick_instep_0_0`'s root offset (mirrored by foot stat 52), negated, via the shared placer sub_140732D40 at yaw 0. The stand point is exactly where that clip's root motion ends at contact.
- The standby Update (`0x1407748F0`) writes no move target; it only plays idle/gesture clips. A real move clamps req[0x1d] to >= 5 km/h; 0 means no locomotion intent. req[0x1f] and precise never enter this path; the slot-6 config (sub_140775730) writes a ctx+44 vec into anime-interface objects {2,3,4}+0xD0 (flags 0x100/0x200): clip-layer config, not MOVESTEP.
- When the ladder emits the kick command, CanEnter `0x140774C50` (kind == 62 only) swaps standby → 63/64. Phase 0 (sub_140775780) picks the strike clip and aims (a long pass projects the target +50 m along the yaw at ctx+1516) and arms the root-motion warp; **the clip's own root carries him the 4.63 m into the ball**, with contact at the clip event.
- The IDLE↔GOTO_RUN churn at 0 speed is the locomotion layer holding while an action clip owns the body (the agent's inference; the re-arm was not singly traced).
