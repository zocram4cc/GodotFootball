# PES port: WalkLane7 (branch WalkLane, base and tip 843a2e305; NOTHING LANDED)

## 0. Verdict
- Step 1 (FUN_1406EB440, the adjuster reset): decoded and CONFIRMED. GF already runs it on trap end. The WalkLane6 hypothesis "GF never runs the reset, so the lock stays on after the trap ends" is REFUTED by a per-tick census of all 119 Trap edges (section 3). No port, no new red test: the premise "A+8 leaves 2 when the trap ends" is already green on the tip (A is 0 at every trap end measured).
- Step 2 (trap-only strike root lock, re-applied on the tip with the diagnostic): PesStallQuick `forced_live=697` (ratchet 53, M2 cap 106). ATrapsRoot 0.068035 m (bound 0.001 m). Red on both. Reverted. Scenario: none (fails M1 and M2).
- Step 3 (dribble series, action ids): not re-run. WalkLane6 measured the series at 253 (M2 cap 106) and the ids at ALiveBall 4,965 (M2 cap 2,786). Neither can land without attributing its rise to named stand-ins; that attribution was not done in this lane (section 5).
- Tip unchanged: `843a2e305`. No push, nothing landed. Gate not run (Main gates).

## 1. FUN_1406EB440: what it resets (CONFIRMED)
Sources: headless decompile (`~/ghidra-projects/scratch/Unpacker/dec.sh`, output `gf-WalkLane-scratch/dec_eb440.txt`); translated closure `src/pes/translated/closure/pes_fn_1406EB440.cpp` (the two agree).
- `P+0x245D = P+0x245C; P+0x245C = 0` (the phase byte, `pes-port-ClipOwner.md` §1).
- `[A.vtable]+8(A, P+0xA08)` with A = P+0x428: the strike-adjuster RESET `FUN_141EB33E0` (translated `closure/pes_fn_141EB33E0.cpp`). It writes `A+8 = 0` (the state word), `A+0x10/0x18 = 0`, `A+0x20/0x24`, `A+0x4C..0x58`, then calls `[A]+0x10` = `FUN_141EB1A20` (`A+0x28..0x48 = 0`, `A+0x6D = 0`).
- `[P+0x4D0 vtable]+8(yaw, P+0xA08)` = the yaw reset `FUN_141EB33C0`.
- `P+0x2460 &= 0xFFFFF1FF`; a 19-entry loop writing `0x11` into `P+0x2704..`; the `P+0x2460` bit-18 arm; `FUN_141AF4EB0(P+0xA08, G)`, `FUN_14058F0F0(P+0x25C8)`, `FUN_1406FA7C0(P)`; the anime interface `P+0xAB8` (slot +0x28) reads for the `P+0x2814..0x2819` flags.
- Who calls it (CONFIRMED): ONLY the action switch `FUN_1406EEBB0` (translated `restart/pes_fn_1406EEBB0.cpp`, 12 call sites, all in its action-change arms). Its arm: `if (newAction != iface.slot+0x28 || (newAction < 0x3E && (0x3500000000000000 >> newAction) & 1)) { P+0x245A = FUN_14081E340(P); ...; FUN_1406EB440(P); P+0xAD0 |= 1; }`.
- GF calls the switch from the translated trap end handler `FUN_14079E5C0` (`trapaction/pes_fn_14079E5C0.cpp`), which `RunPesTrapTicks` reaches through `RunTrapTick`. So the reset runs in GF on the trap end. Other translated switch callers: `FUN_1407B3470` (switch to TRAP, sub 0x57, from the dribble path), `FUN_140792E10` (switch 2, sub 0x5C), `FUN_1407C44A0` (switch 0x5D), `FUN_1407A3A30`, `FUN_1407B05F0`, `FUN_1407B2D60`, `FUN_1407B37D0` (dribble tick), `FUN_1407A38E0` (act4 start).
- GF's OTHER action writer, `WriteActorP` (`pestrapactor.cpp:217`, the `!pesOwned` arm), stores P+0xAC8/AC9 from `ActionIdsAfterSwitch` and does NOT run the switch, so it does NOT run EB440 or the A reset. It covers non-Trap men and trap-ended men once `pesOwned` is false. Whether that matters for A: the census shows no A state stale across a trap end (section 3).

## 2. Diagnostic (scratch, reverted; patch kept)
- `tests/sim/pes_stall_test.cpp`, `BreakerCensusProbe::OnTick`: per man, prints `DIAGEDGE` on each Trap edge (ac8 7 <-> other) with A's state word before and after, the A mode `+0x6D`, the Trap ticks and the ticks with state 2 of that Trap.
- Patch: `gf-WalkLane-scratch/lock-diag-pes_stall_test.cpp.patch` (observation only).
- Probe check: baseline (tip, no lock) gives `PESSTALLQUICK forced_live=53`, the ratchet number, so the probe does not perturb the match.
- Run environment (for the record): `PES21_ROOT=<workdrive>/PES21 PES_EXE=<home>/ghidra-projects/scratch/Unpacker/pes_full.bin`; without `PES_EXE` the constants fail to load ("install constants unavailable") and the run aborts in 8 s.

## 3. Census of the Trap edges (CONFIRMED, one seed-7 quick match each)
Baseline (tip + diagnostic, no lock), `diag_quick.log`, 119 edges:
| edge (ac8 prev->now) | A state prev->now | count | reading |
|---|---|---|---|
| 2->7 (Trap start) | 0->0 | 56 | strike armed later in the trap |
| 7->2 (Trap end to MOVE) | 0->0 | 44 | A already 0 before the edge |
| 7->2 | 2->0 | 1 | reset on the edge |
| 7->4 (Trap end to Dribble) | 0->2, mode 1 | 10 | a NEW strike armed on the same tick (not stale) |
| 7->4 | 0->0 | 3 | A already 0 |
| 21->7 | 0->0 | 4 | Trap start from a kick |
- A is state 2 for about half of each Trap (median 17 of 31 Trap ticks). It goes 2->0 INSIDE the Trap, before the edge.
- So no stale state 2 survives a trap end in the sample: the reset runs (through the switch) on every trap end measured. The WalkLane6 "lock stays on after the trap" explanation for the rise is wrong.

Lock (tip + trap-only lock + diagnostic), `lock_quick.log`, 162 edges:
| edge | count (lock) | count (baseline) |
|---|---|---|
| 2->7 | 74 | 56 |
| 7->2 (MOVE) | 64 | 44 |
| 7->2 with 2->0 | 5 | 1 |
| 7->4 (Dribble) with 0->2 | 10 | 10 |
| 21->7 | 5 | 4 |
| 4->7 | 1 | 0 |
- The lock makes Trap men cycle: 74 Trap starts against 56, and 64 Trap-to-MOVE ends against 44. The trap ends early and restarts without the ball being played. This is the mechanism to attribute, not the reset.

## 4. Measurements (tip 843a2e305, trap-only lock: `StepAdjusters` sets `LockRoot(strikeMoved && ac8 == Trap)`, `CalculateSpatialState` folds the clip root into `startPos` while locked)
| run | target | result | bound | status |
|---|---|---|---|---|
| baseline (tip + diag) | PesStallQuick | forced_live 53 | 53 | green |
| lock | PesStallQuick | forced_live 697 | 53 (M2 cap 106) | red, fails M1 and M2 |
| lock | PesPassTarget.ATrapsRoot | worst gap 0.068035 m | 0.001 m | red (was 0.156 m with no lock, WalkLane6) |
| lock | ALiveBall | not run | 2,229 (M2 cap 2,786) | not run: the quick run already fails M2's caps |

Census delta per bucket (STALLDUMP, owned=0 on every firing; lock vs baseline; baseline buckets from `quick_base_dump.log`, lock from `s1_quick.log`, both from the tip with the same probe):
| bucket (ac8) | baseline | lock | delta |
|---|---|---|---|
| 7 (Trap) | 34 | 405 | +371 |
| 2 (MOVE) | 4 | 237 | +233 |
| 13 | 0 | 27 | +27 |
| 21 | 15 | 19 | +4 |
| total | 53 | 697 | +644 |
- The rise is on Trap men and MOVE men. Both sit behind the lock, which is a GF stand-in for PES's root replacement (FUN_141AFD400 replacing the layer root by A+0x14). That attribution makes it a stand-in rise (M2 clause 1), but the total still fails M2's cap (697 > 106), so M2 does not apply.
- Residual: ATrapsRoot is still 0.068 m with the lock. So the lock is not exact (WarpRoot does not account for all body motion, for example collision offsets or the smuggle terms). With no lock it is 0.156 m. A stand-in that is not exact cannot be the PES root.

## 5. Open items (numbered)
1. Mid-Trap 2->0 of A (A is state 2 for about half of each Trap, then 0 before the trap end). The only translated caller of EB440 is the action switch; the mid-Trap reset must come from a switch call with an action that differs from the iface action during a Trap tick. Decode the Trap tick `FUN_140797CD0` arms that reach the switch (ret addresses), and the indirect `[A]+8` calls (vtable, not visible by direct-call grep). INF until read.
2. Lock residual (0.068 m): find what moves the trapping body besides WarpRoot (`HumanoidBase::OffsetPosition` from collisions, the smuggle terms, `startPos` reset in the `found` branch of `Process`). Compare the per-tick body step with A's step in one capture.
3. Lock trap cycling (74 vs 56 Trap starts): which trap ends fire early under the lock (the trap end handler's arm, the ball distance). Needed before any M2 raise.
4. Dribble series (WalkLane6 item 2, 253): attribute the rise on Trap and locomotion clips to named stand-ins before retrying (M2). Not re-measured here.
5. Action ids `2f8fcca3` (ALiveBall 4,965, WalkLane6): split the walker and ids rises (M2) before re-landing. Not re-measured here.
6. A regression guard for the trap-end reset (the census edge table above, as a test) is possible; not added: it is green on the tip and a test written now would not be red-first.

## 6. Scenario and ratchet
- Scenario: none claimed. No change landed, so no M1/M2 evidence is claimed.
- Ratchet on the tip (unchanged): PesStallQuick 53 (baseline run in this lane: 53). ALiveBall: not re-measured in this lane (WalkLane6 recorded PASS on the tip).
- Tests run this lane: `pes_restart_delay_test --gtest_filter=PesStallQuick.*` (baseline 53 with diagnostic; lock 697), `pes_pass_target_test --gtest_filter=PesPassTarget.ATrapsRoot*` (lock: 0.068035 m, red). Gate not run (Main gates).
- Mutations: the lock was applied and reverted once (its effect is the 697 and 0.068 measured above). The diagnostic was applied and reverted.
- Rig requests: none. The census needs no capture; the open items 1-2 need a Trap-tick capture if static decode does not settle them (hook: the Trap tick `FUN_140797CD0` ret addresses, with a predicate `d(rcx+0x1d8)`-style on the action; to be specified by the next lane).

## 7. Next highest-value step
Open item 1: decode the Trap tick's switch calls and the indirect `[A]+8` callers, so the mid-Trap reset is attributed. It decides whether the 2->0 inside the Trap is PES's (then the lock's window is PES's, and the 0.068 m residual is the next thing to chase) or a GF stand-in (then the lock window is wrong).
