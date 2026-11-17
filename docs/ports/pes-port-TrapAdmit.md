# PES port: TrapAdmit: the twelve gate-4a slot +0x70 handlers and `0x1406B4C10` translated and proven (13/13), the body's PES action id derived, gate 4a called from `_ReceiveCandidate`; holder and refusal reds pass, the live-ball breaker red gets WORSE (633 ticks); no gate, no smoke (09-11-26)

Branch `TrapAdmit` (worktree `gf-TrapAdmit`), tip `add876c4` (pushed; `git ls-remote origin TrapAdmit`), base `origin/main` `2d0759c4` plus `origin/WhistleBall` `6f6e7ce0` (merged at Main's request, commit `9ec4fe34`, no conflict). Image `pes_full.bin`. CONFIRMED = a tool run this session (decompile, image script, capture, test run); INF = inference; OPEN = blocker named. **This run stopped at its request budget: the gate (`GATE_TIER=branch`), the smokes and the mutation runs were NOT run.**

## 0. Verdict against the acceptance
| Acceptance | State |
|---|---|
| translate and prove the twelve slot +0x70 handlers | **MET**: 12 handlers + the entry trampoline `0x1406B4C10`, 13/13 bit-exact (section 2); layer 5668 functions, `gen_translated.py` ran, ledger regenerated; the real calls cover only 3 of the 13 (the other 10 SYNTHETIC) |
| body's PES action id from GF's state, `MOVE` stand-in gone | **MET with a named limit** (section 3): by action NAME; MOVE vs STEP_MOVE not derivable |
| `0x14079BBE0` called from `_ReceiveCandidate` | **MET** (through `0x1406B4C10`, section 4) |
| red test: no PES trap start refused by `PlayIssuedClip` | **written and passes** on the merged tree, but its red state on main was not re-measured here (TeamTick7: 84 of 113); mutations NOT run |
| `GATE branch` GREEN | **NOT RUN**; one test is known red on the merged tree (section 5) |

## 1. What gate 4a is (CONFIRMED, decompile + image)
- `FUN_1409FA520` (called once, from gate 3 `FUN_1409FA400` at `0x1409FA4F7`) starts with `FUN_1406B4C10(anime, 7)`. `0x1406B4C10` (disassembled): `rcx = P`, `call 0x1406D9010(P+0xC40, edx)` names the action object, then `jmp [obj_vtable+0x10]` with `(obj, P)`. For id 7 the object's vtable `0x1425E1ED0` slot +0x10 is `0x14079BBE0` (image script).
- `0x14079BBE0` reads the CURRENT action through the anime interface (`P+0xAB8`, vtable `0x1425E3508`): **slot +0x28 = `0x1406E66B0` = `movsx eax, byte [this+0x11]` = `P+0xAC9`** (TeamTick7 §2 said `P+0xAC8`; the two differ only when an action change passes a sub-type, `FUN_1406EEBB0`'s third argument). The other slots the handlers use, all already translated: +0x18 `0x1406E6BD0` = `P+0xAE0` (motion id), +0x20 `0x1406EADE0` = `P+0x245C` (locomotion phase), +0x38 `0x1406E6960` = elapsed ticks of that motion in the motion table `P+0x354`.
- Admitted ids {2,3,4,5,6,8,9,0xA,0xB,0x10,0x11,0x15,0x44,0x48,0x50,0x51,0x53}: the id's own slot +0x70 must return true for `(obj, P, 7)`; 0x17 admits; 0x4A/0x4F admit under a flag; everything else refuses.
- **Slot +0x70 per id** (vtables from `pesactiontable_stores.inc`, `+0x70` read from the image by `gf-TrapAdmit-scratch/slot70.py`): MOVE(2) `0x140782570`; STEP_MOVE(3)/DRIBBLE_STEP_MOVE(6) `0x140789E60`; DRIBBLE(4) `0x1407B9280` (already in the layer); DRIBBLE_SIDE_BACK(5) `0x1407C6F40`; LONG_PASS/SHORT_PASS/SHOOT(8,9,0xA) `0x140724820`; SLIDING(0xB) `0x140787EC0`; STAGGER(0x10) `0x14072E450`; FALL_DOWN(0x11) `0x14072E360`; DODGE(0x15) `0x14073B680`; KP_DROP_BALL(0x44) `0x140744090`; JOSTLE(0x48) `0x140779DF0`; DEMO_INPLAY_CONTACT(0x50) `0x1407E8D20`; DEMO_INPLAY_KICK_FAILED(0x51) `0x141863350` (in the layer); DEMO_INPLAY_MOVE(0x53) `0x1407E8E10`.
- **Closure (CONFIRMED, `closure.py`):** 71 members from the 13 roots; only 15 are new: the 13 roots plus `0x1407245D0` and `0x140724620` (leaves of `0x140724820`). Every other callee was already in the layer.

## 2. Proofs (CONFIRMED, `prove_bounded.py --merge`, `snapdiff --bounded`: PES's bytes against the translated closure, return value and every written byte)
Base memory: `live2` was empty and the surviving oracle/`syn` states hold no P, so Main restarted the rig with my hook batch (13 `trapadmit_*` hooks, one message). Synthetic states: `tools/dev/translate/synth_trapadmit_specs.py CAPTURE [VA...]` writes one "static as" spec per handler (rcx = the action object the id names in P's table, rdx = P, r8 = third argument; pokes of motion `P+0xAE0`, phase `P+0x245C`, type `P+0xAC9`, motion-table entry 0 id and seconds); `gf-TrapAdmit-scratch/prove.sh VA CAP [REAL...]` runs synth + proof and deletes the states.
| root | calls | own-block | kind |
|---|---|---|---|
| `0x140724820` | 6/6 | 47/90 (0.52) | 1 REAL (`pid3332804/seq042`, `r8 = 0x48`) + 5 SYNTHETIC (seq042) |
| `0x140782570` MOVE | 6/6 | 10/19 (0.53) | 1 REAL (seq050) + 5 SYNTHETIC |
| `0x140789E60` STEP | 6/6 | 6/15 (0.40) | 1 REAL (seq051) + 5 SYNTHETIC (seq050) |
| `0x1407C6F40` | 5/5 | 14/51 (0.27) | SYNTHETIC (seq050); no hook call ever fired |
| `0x140787EC0` | 5/5 | 14/17 (0.82) | SYNTHETIC; never fired |
| `0x14072E450` | 5/5 | 14/34 (0.41) | SYNTHETIC; REAL calls landed later (see Left 2) |
| `0x14072E360` | 5/5 | 14/17 (0.82) | SYNTHETIC; REAL calls landed later |
| `0x14073B680` | 5/5 | 26/112 (0.23) | SYNTHETIC; REAL calls landed later |
| `0x140744090` | 5/5 | 3/12 (0.25) | SYNTHETIC; never fired |
| `0x140779DF0` | 5/5 | 20/71 (0.28) | SYNTHETIC; REAL calls landed later |
| `0x1407E8D20` | 5/5 | 3/14 (0.21) | SYNTHETIC; never fired |
| `0x1407E8E10` | 5/5 | 33/43 (0.77) | SYNTHETIC; 4 REAL calls landed later |
| `0x1406B4C10` trampoline | 8/8 | 2/2 (1.0) | SYNTHETIC (no hook on it; its target `0x14079BBE0` has real captures `seq049` and later) |
All 0 mismatches. Coverage is recorded, not a gate. No synthetic state is left on disk; captures untouched (pinned by Main).

## 3. The body's PES action id (CONFIRMED mapping source for BallControl; INF for the rest)
GF has no PES action machine. PES writes `P+0xAC8/0xAC9` only inside `FUN_1406EEBB0`, which about 80 call sites (one per action tick) call; no PES field holds a "clip → action" map, so any GF→id map is by action NAME. `pes::AnimeActionOfClip(function, specialVar1, carrier)` (`pestrapactor.hpp/.cpp`; names from the image table `0x1434A5470`): Movement and PES locomotion Special clips → MOVE; BallControl → DRIBBLE only for the PES holder, else MOVE (CONFIRMED: `FUN_1406EEBB0`'s 4/5 arm replaces a DRIBBLE request of a man who is not the carrier by MOVE); Trap → TRAP; ShortPass/LongPass+HighPass/Shot+Header → SHORT_PASS/LONG_PASS/SHOOT; Sliding → SLIDING; Trip, Interfere → TACKLE (INF); Catch/Deflect → KP_CATCH/KP_DEFLECT; Special by family: dodge DODGE, tackle TACKLE, locomotion MOVE, trap TRAP, any other NOTHING. **OPEN:** MOVE vs STEP_MOVE (entered from other actions' ticks, e.g. `FUN_140748390`; the MOVESTEP state does not decide it); the header's action (SHOOT is INF).
`TrapFactsOf` (`teamAIcontroller.cpp`) now derives action/sub from the clip and the holder; for a trap-family clip it keeps the id PES's own end handler wrote (`0x14079E5C0` → `FUN_1406EEBB0(P,4|2)` while the body is still on the trap clip, else the trap restarts); the previous pair follows `FUN_1406EEBB0`'s tail stores. The `MOVE` stand-in `ponytail:` is deleted. The man's motion is `CurrentMotionOf` (the PES motion of a PES clip, else `0xBE5`): for GF's own clips the handlers read an empty Animetable record.

## 4. The gate in GF
`PesWorld::RunTrapAdmit(slot, action, ext)` (`pesworldrecord.cpp`) runs `pes_fn_1406B4C10` on the window. `PlayerController::_ReceiveCandidate` calls it on the PES path after gate 3's carrier clause, except for a man already in the Trap action (his entry is held by `_TrapCommand`, an existing stand-in: PES's gate refuses TRAP itself, and letting the walk go on would queue later entries' commands over his clip). `HumanoidBase::PlayIssuedClip` no longer refuses a trap-family clip (the Trap action's slot +0x10 decides admission; PES's action switch replaces the clip); the locomotion refusal stays.

## 5. Tests on the merged tree (CONFIRMED runs: `pes_receive_test` 15/15; `pes_restart_delay_test --gtest_filter='PesStall.*'`, seed 7, 2 min, 432 s in a sim slot, `stall1.log`)
| Test | Result |
|---|---|
| `PesAnimeAction.*` (3 new, `pes_receive_test.cpp`) | pass |
| `PesStall.NoPesTrapStartIsRefusedByTheBody` (new red probe, `pes_stall_test.cpp`) | **pass** (0 refused) |
| `PesStall.AHolderIsNeverHandedANewTrap` (Main's WhistleBall red) | **pass** on the merged tree |
| `PesStall.ATrapClipIsNeverRestartedBeforeItsContactFrame`, `TheBreakerNeverForcesABallUnderAnArmedRestart`, `ANoManIsEverHandedNoCommandAtAClipEnd` | pass |
| **`PesStall.ALiveBallNeverLiesStillLongEnoughForTheBreaker`** | **FAIL: the breaker forced a live ball for 633 ticks** (Main reported 56 on WhistleBall alone): gate 4a makes the resting-ball stall worse. Not diagnosed (budget). INF: men standing at the ball in a GF ball-control or locomotion clip are mapped to MOVE/DRIBBLE and the handler (reading an empty Animetable record for GF's own clip, or a PES locomotion motion before its cancel point) refuses class 7, so nobody takes the ball. First probe: log per refusal the man's PES action, motion `P+0xAE0`, phase `P+0x245C` and which handler answered false. Do NOT hide it: if the handlers refuse a standing man for a real PES reason, the missing input is the elapsed-ticks table (`P+0x354`, written only for PES clips) or the MOVESTEP phase.|
Mutations (a) `carrier ? Dribble : Move` → `Dribble`, (b) restore the `PlayIssuedClip` trap refusal, (c) remove the `RunTrapAdmit` call, (d) Sliding → MOVE: **NOT run.**

## 6. Left (numbered, exact resume)
1. **Diagnose the 633-tick breaker red** (section 5) with the probe described there; fix at the root (the P inputs of the refusing handler), keep the red test.
2. Re-prove with the REAL captures that landed later (all full, `live2/pid3332804`, pinned by Main; names from `live2/watch.log` `-> ok`): STAGGER `0x14072E450`, FALL_DOWN `0x14072E360`, DODGE `0x14073B680`, JOSTLE `0x140779DF0`, `0x1407E8E10`, and `0x14079BBE0` itself (6 calls): `prove.sh VA CAP REAL...` then `gen_translated.py`, `ledger.py`. Release Main's pin afterwards. SLIDING, KP_DROP, DRIBBLE_SIDE_BACK and the demo `0x1407E8D20` never fired: they stay SYNTHETIC.
3. Run the four mutations; confirm the red probe's red state on `origin/main` (TeamTick7's 84/113).
4. `GF_PES_SURVEY` on the seed-7 match for handler indirect targets the closure walk cannot see.
5. Smoke (seed 7, 2 min, both paths, through `simslot.sh`; baselines switch-off `a0fbe2d5cd445a52`, pes-loop `17941efc16b4a6a5`; WhistleBall moves the pes-loop one), attribute the moves, list drifted seeds, `GATE_TIER=branch` on the final tip (env in the brief).
6. Main's pending item (RestingBall6): gate 3's second clause (teammate holds the ball and my rank byte `MA+0x247BE` refuses) is untouched; the holder test passes without it.
7. Scratch `gf-TrapAdmit-scratch` is small; `build/` in the worktree.
