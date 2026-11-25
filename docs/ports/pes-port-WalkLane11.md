# PES port: WalkLane11 (branch WalkLane, base and tip 66f2930d7; NOTHING LANDED)

## 0. Verdict
- No change lands. Scenario claimed: none. Every candidate fails the ratchet and the M2 caps:
  - Ids (2f8fcca3, re-applied as 943f20fd8): PesAnimeAction 4/4 green, but PesStallQuick 82 (bound 49, cap 98) and ALiveBall 985 (bound 479, cap 599). Rise on the SLIDING (0xB) and TACKLE (0xD) rows, whose PES action ticks GF does not translate.
  - Walker fix (PlayIssuedClip yaw + clip-start anchor): SetKickStand walker GREEN, PesStallQuick 567. Split: yaw-only 444, anchor-only 343 (walker RED). Rise is on non-walker clips of every clip start, not on the taker.
  - Ids + walker: walker GREEN, PesStallQuick 602.
  - Dribble member loop (WalkLane4 `humanoid.cpp` hunk) on the tip: PesStallQuick 62 (loop only, no ids; same 62 with ids), PesStall.APesDribblerNeverHoldsHisClipsLastFrame 2 vs 1 (red by one tick; WalkLane4 measured 343 before the loop), PesHang GREEN. ALiveBall with ids + loop: 2,839 (cap 599). REJECTED.
- Task 2 (dribble-series advance): decoded (section 3). PES does not advance a member sequence; a queue is not PES. The loop reproduces PES's record flag but is a GF stand-in with an unmeasured wrap law, and it raises ALiveBall. Not landed.
- Task 3 (O1, the other man's push): formula decoded (section 4). Not ported; multi-round.

## 1. Measurements (66f2930d7 base; seed 7; `GF_STALL_DUMP=1`; Release, Ninja, ccache)

Tree states (built and measured in the lane worktree; binaries copied before rebuilds):

| state | definition | SetKickStand walker | PesAnimeAction | PesStallQuick (bound 49, cap 98) | ALiveBall (bound 479, cap 599) |
|---|---|---|---|---|---|
| base | 66f2930d7 | RED (WalkLane10) | RED (ABall) | 49 (re-run here: 49) | 479 (WalkLane10; not re-run here) |
| A | ids (943f20fd8) | RED (62 of 71 far ticks) | 4/4 GREEN | 82 | 985 |
| B | walker only (humanoidbase.cpp PlayIssuedClip hunk) | GREEN | not re-run | 567 | not run (over cap) |
| Y | walker yaw only (`clipAngle` = startAngle) | GREEN | not run | 444 | not run |
| X | walker anchor only (`startPos = position - positions[frame]`) | RED (166 of 222 far ticks) | not run | 343 | not run |
| C | A + walker | GREEN | not run | 602 | not run (cancelled, over cap) |
| L | base + dribble member loop (WalkLane4 `humanoid.cpp`) | not run | not run | 62 | not run |
| D | A + dribble member loop | not run | dribbler tests: NeverHoldsLastFrame 2 vs 1 (red) | 62 | 2,839 |

PesHang (`PesHang.SeedElevenTwoMinutePesLoopMatchReachesFullTime`) on D: GREEN, full time, 1,747,549 ms.
PesStall.APesDribblerGoesToABallAtRestOrLeavesTheDribble (D): OK (2,315,166 ms). PesStall.APesDribblerPlaysTheMotionItIssued (D): OK. PesStall.APesDribblerTouchMovesTheBall (D): OK.

Census, quick window (STALLDUMP lines):
- base: 91 dump lines. Top: `trip/trip_t2/idle/000_180.anim` (ac8 2, 21), `dribblerun_3_3_000_mid_out_ver2` (ac8 7, 7 + 7), `avoidjump_3_3` (ac8 21, 7), `trap_0_0_000_y0_sole` (ac8 7, 6).
- D: 117 dump lines. Top: `trap_0_0_000_y0_in_mirror` (ac8 2, 24; ac8 7, 18), `trap_0_0_f045_y0_sole` (ac8 7, 7), `avoidjump_3_3_mirror` (ac8 21, 7).
- B (walker only): 1,075 dump lines vs A's 156: `movement_special/idle/special/000_stand_up_from_back.anim` (113), `dribble_0_3_f090_out_ver1` (78), `sliding/idle/000.anim` (60), `sliding/sprint/000.anim` (51). Not the taker's walk: the clip-start change is GF-wide.
- The M2 rule (every rise on men driven by named GF stand-ins) is NOT met by the loop: its rise lands on trap men (ac8 7 and MOVE) near the dribbler, with no named stand-in on those men.
- A (ids) ALiveBall census: top rows `sliding/idle/000.anim` (ac8 11, 114 lines), `tacklehold_waist_0_0_020` (ac8 13, 90), `dribble_0_3_000_in_mirror` (ac8 7, 76), `traprun_3_3_s_000_y0_near_in_mirror` (ac8 7, 59). The sliding and tackle rows dominate; PES's sliding tick `0x140786D60` is untranslated (WalkLane3 section 3).

Per-bucket pass lines print only `armed=0 live=N unarmed=0`, so bucket deltas come from the dump lines above.

## 2. Walker (SetKickStand.AFarSetPieceWalkerIsCommandedAtTheWalkDownSpeed)
- The walker test is green whenever the yaw change is in (Y yaw only, B yaw + anchor, C ids + yaw + anchor). The anchor alone is red (X).
- Both halves are GF-wide: they change every PlayIssuedClip start (`humanoidbase.cpp` ~586-599). The clip-start rule PES uses (the blend from the previous motion, `docs/re/pes-re-anim-blend.md` 7.1) is not ported, so no narrow, PES-faithful form is available.
- Not landed. Next: port PES's clip-start blend (the anime player) instead of changing PlayIssuedClip.

## 3. Task 2: the dribble-series advance (decode, VA-cited)
Source: headless decompile (`dec.sh`, `dec11/series.txt`), translated closure sources.
- CONFIRMED `FUN_1406F14E0` (0x1406F14E0): the series setter fills P+0x3B90 (count) and P+0x3B98 (data, 0x38-byte entries: motion +8, seconds +0xC, mirror +0x10), and sets **P+0x3B80 = 0** (its body, decompile line 175). So at issue the blend parameter is 0.
- CONFIRMED `FUN_1406E5FC0` (0x1406E5FC0, decompile lines 1171 and 1187): `FUN_141AF61C0(anim, P+0x3B48, P+0x3B80, ...)` and, when P+0x375C == 2 and P+0x3BA0 != 0, `FUN_141AF52B0(anim, P+0x3B90, P+0x3B80)`. The blend call's site is 0x1406E6325 (WalkLane3).
- CONFIRMED `FUN_1407B37D0` (dribble tick, decompile lines 215-345): no series index and no member advance. It issues requests 0x52/0x53 through `FUN_1406B4580`, then runs its phase switch on the secondary's byte. The clip-end arm is `FUN_1407B05F0`.
- CONFIRMED (docs, WalkLane3 2.1; TrapContact10 14): the member record carries the loop flag (walk_1_1's gani: loop on all 15 units).
- INF: PES plays member 0 looped while the Dribble action owns the body. The blend weights (P+0x3B80) are the only per-member selector, and the translated engine maps position 0 to member 0 alone (WalkLane3 2.1).
- OPEN: which code writes P+0x3B80 after issue (the blend position). Not found. The series-position writer is the next decode.
- Consequence: "play the next member at a member end" (WalkLane2's plan) is not PES, and WalkLane6's queue (PesStallQuick 253) is a GF stand-in, rejected. The PES-side fact is the loop flag, which section 1 measures (L/D: 62 quick, dribbler hold 2 vs 1, ALiveBall 2,839).

## 4. O1: the other man's push (decode, VA-cited)
- CONFIRMED (`docs/re/pes-re-duels-fouls.md` §1.2, OBS): the per-man share `FUN_140A6D670(ctx, P, Q)` = `0.5 − 0.35·ds − 0.3·(1 − 0.5·|ds|)·d`, `d = clamp(pcP − pcQ, −30, 30)/30` (±15 by heading and bearing), `ds = sQ − sP` (speed classes `s = clamp((v−5)/8.5, 0, 1)`, v in km/h), with special cases: action 0x15, flag bits 13, collision class 6, the class-pair table 0x142648900, and share = pcQ'/(pcP'+pcQ') for key players.
- CONFIRMED (`docs/re/pes-re-contact.md` §1.2): `FUN_1408C48C0` adds `Σ normal·(depth − allowance)·share` to P+0x624/+0x62C and the root offset `ap+0x248C..0x2494`.
- Not ported. It needs the full contact gather (`FUN_1408C31C0`) and the per-man PC and class state, so it is multi-round. GF keeps its bounce for the other man (`match.cpp` ~1435, ponytail).

## 5. Tests and gate
- Test binaries run (counts): pes_receive_test PesAnimeAction 4/4 (A); pes_setpiece_stand_test SetKickStand.AFarSetPieceWalker* (A red, B green, Y green, X red, C green); pes_restart_delay_test PesStallQuick.* (base 49, A 82, B 567, Y 444, X 343, C 602, L 62, D 62); PesStall.ALiveBall* (A 985, D 2,839); PesStall.APesDribbler* (D: 3 OK, 1 red 2 vs 1); PesHang (D: green).
- Gate: not run (nothing landed). gen_translated.py --check: not run. Mutations: none (nothing landed).

## 6. Repo state
- origin/WalkLane = 66f2930d700e (ls-remote; unchanged). No push, no commit on WalkLane.
- Local probe branch `WalkLane11-ids-probe` = 943f20fd8 (the ids cherry-pick). Not pushed; it is WIP that breaks the ratchet.
- Patches (scratch, `gf-WalkLane-scratch/`): `walker-humanoidbase-only.patch` (walker B), `dribble-loop-humanoid.patch` (loop, D/L). The yaw-only and anchor-only variants are the two halves of the walker hunk (section 1).
- Logs (scratch): `A_quick.log`, `A_aliveball.log`, `B_quick.log` (walker only, 567), `Base0_quick.log` (base 49), `Base_quick.log` (state L, 62), `B_walker.log`, `Y_quick.log`, `X_quick.log`, `C_quick.log`, `C_walker.log`, `D_quick_dribbler.log`, `D_aliveball2.log`, `D_hang.log`.
- Log names: `B_quick.log` is the walker-only run (PesStallQuick 567). `Base_quick.log` is state L (base + dribble loop, no ids; 62). `Base0_quick.log` is the true base (49).

## 7. Rig requests
None. No live PES reads, no hooks, no captures.

## 8. Open items (numbered, in order)
1. Clip-start rule (walker, 567/444/343): port PES's anime-player blend from the previous motion (`AnimMovePlugin::apply`, `docs/re/pes-re-anim-blend.md` 7.1) rather than changing `PlayIssuedClip`. Re-measure the walker and PesStallQuick after it.
2. Ids (82/985): port the SLIDING tick `0x140786D60` (exit `FUN_1406EEBB0`, requests 2 and 0x11, WalkLane3 section 3) and the TACKLE action tick before the 0xB and 0xD rows can land. The 2f8fcca3 rows for 8, 9, 0xA, 0x38 and 0x3A need their own census.
3. Dribble freeze: decode the writer of P+0x3B80 after issue (the blend position; `FUN_1406E5FC0` line 1171 callers) and port the PES blend, not a loop stand-in. The loop (L/D) raises ALiveBall to 2,839 and is rejected as it stands.
4. Dribbler red (2 vs 1): one-tick hold at the cycle end (a normal frame at 120/121, INF). Decide whether the test counts a normal frame (test change needs M3 scenario evidence) or whether the wrap holds a frame too long.
5. O1 push share `FUN_140A6D670` + `FUN_1408C48C0` (section 4): port the share and the root offset path for the other man (multi-round).
6. Next highest-value step: item 2 (the sliding tick). It is the largest single ALiveBall term and it is a PES tick GF lacks, not a stand-in tune.
