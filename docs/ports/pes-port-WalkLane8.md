# PES port: WalkLane8 (branch WalkLane; merged origin/Integrate c7fa9d01d; pushed 72c61d6df; NOTHING LANDED)

## 0. Verdict
- Item 1 (who drops strike adjuster A from state 2 to 0 mid-Trap): ATTRIBUTED. It is PES's own adjuster timeout `FUN_141EB3360`, run every tick by GF through `RunAdjusters -> FUN_141EB5E70 -> FUN_1406EEDF0`. GF already runs it. No missing writer, no port. The killed theory from WalkLane7 ("GF lacks the PES writer that drops A mid-trap") is KILLED.
- Item 2 (trap-only strike root lock): re-measured on the merged tree. ATrapsRoot is still red (worst 0.068035 m, bound 1 mm). PesStallQuick was not re-run with the lock; WalkLane7's 697 stands (cap 106 under M2). Fails M1 and M2. Lock reverted. Nothing landed. Scenario: none claimed.
- Item 3 (capture): not needed for item 1. A capture is requested below to check the timeout's inputs against PES.
- Gate not run (Main gates). ALiveBall not run (no behaviour change landed). PesStallQuick on the merged tree WITHOUT the lock: 53 (= ratchet, PASS; the gdb-instrumented run below).

## 1. Build blocker found (Integrate, reported to Main)
- origin/Integrate c7fa9d01d contains `src/pes/translated/out:_career/pes_fn_140400DF0.cpp`. The colon breaks the Makefile generator: `target pattern contains no '%'` in `pestranslated/build.make`, so no test target links under Make. Main confirmed the Ninja configure builds it, and is fixing `gen_translated.py` (sanitized dir names) on Integrate.
- Local build-only workaround: the dir was renamed to `out_career_LOCAL` for my builds and restored before the push. Not committed.

## 2. Attribution of A's 2 -> 0 (CONFIRMED, GF run with a hardware watch on P+0x430)
Method: `gf_diag_arm` breakpoint in `tests/sim/pes_stall_test.cpp` (scratch, reverted); `gdb` hardware watch on the host word `P+0x428+8` of the first man seen in Trap with state 2 (slot 9, t=189). Quick match, seed 7, 4 min, no lock. Log: `gf-WalkLane-scratch/watch_a2.log`; script: `watch_a.gdb`. Probe result under gdb: `PESSTALLQUICK forced_live=53` (no perturbation).

Transitions of A's state word for that man over the 4-minute match (31 changes):
| change | writer (first PES frame after libc memcpy) | driver |
|---|---|---|
| 0 -> 1 | `FUN_141EB39A0` (strike arm) | `FUN_14079C2D0` (trap start, GF tick) or `FUN_1407C2AE0` (trap start from a kick) |
| 1 -> 2 | `FUN_141EB3AE0` (finish: `A+8 == 1 -> 2`) | `FUN_14079C2D0` via `RunTrapTick` |
| 2 -> 0 (x10) | `FUN_141EB3360` | `FUN_1406EEDF0` via `FUN_141EB5E70` via `RunAdjusters` (once per tick) |
| 2 -> 0 (x1) | `FUN_141EB33E0` (reset; the vtable +8 reset) | `FUN_1406EB440` <- `FUN_1406EEBB0` (action switch) <- `FUN_14079E5C0` (trap end handler) |

Totals: 11 zeros = 10 via 3360 (mid-Trap timeout) + 1 via the trap-end reset. So the 2 -> 0 inside a Trap is PES's `FUN_141EB3360`.

`FUN_141EB3360` (translated `src/pes/translated/closure/pes_fn_141EB3360.cpp`, read from the generated code): compares `s16 [A+0xC]` (tick counter) with `s16 [A+0x10]` (ticks budget). If counter `le` budget, it does nothing. Otherwise (counter > budget) it clears the state and the rest of the adjuster (`A+8 = 0`, `A+0x10`, `A+0x18`, `A+0x20/0x24`, `A+0x38`), then calls `[A.vt+0x10]` (`FUN_141EB1A20`) and `[A.vt+0x20]`. So: state 2 is the adjuster's active window; it ends when the tick counter passes its budget.
- CONFIRMED: GF runs 3360 each tick with PES's code; the drop is not a GF stand-in.
- INF: PES's counter and budget values match GF's (same translated consumer `FUN_141EB1BF0`, mode 2 steps the counter, `FUN_141EB39A0` sets the budget). Not checked against PES; needs the capture in section 5.

Consequence for the lock: `B+0x81 = (A+8 == 2 && A+0x6D != 3)` (`FUN_1406EEDF0`) is already PES's window, so the lock's on/off timing is PES's. The residual is elsewhere (section 3).

## 3. Item 2: trap-only lock re-measured (tip 72c61d6df = Integrate + nothing; lock = scratch patch)
Lock: `LockRoot(frame.strikeMoved && ac8 == Trap)` in `StepAdjusters`; `CalculateSpatialState` folds the clip root out while locked (`lock-diag-*.patch`, reverted).
- ATrapsRootMovesByExactlyTheStrikeAdjusterStep: 573 samples, worst 0.068035 m (n=362, side 1, count 7), mean 0.00197 m, 281 of 573 over 1 mm. Red. Log `atraps_lock3.log`.
- Per-strike residual is a near-constant offset per run of counts, not one bad tick: e.g. 0.003347 m for counts 3-10, 0.001116 m for counts 12-18 of one strike; a second strike 0.007550 m (counts 3-7). Per count, mean 0.001-0.004 m (`count=6`: worst 0.0648; `count=7`: 0.0680).
- Split (new columns in the print): `ReadAdjusterFrame` warp input (x, z) is ZERO on most sampled ticks (rows at n=0-3 and n=361-362) while A+0x14 moves by 0.0004-0.0104 m per tick. In 461 of 573 samples the warp input differs from the A+0x14 step by more than 0.5 mm; in 194 the body's move differs from the warp input by more than 0.5 mm. All 573 samples had `strike=1`.
- So the residual has two parts: (a) B+0x90 - O2+0xD0 is not the A+0x14 step at sample time (the warp carries 0 while the root point moves), and (b) the body moves by terms other than the warp. The B+0x90 write vs the StepAdjusters read order in the tick is not yet checked (INF; `ReadAdjusterFrame` reads B after the tick).
- PesStallQuick with the lock: 697 (WalkLane7, not re-run). Census (WalkLane7): Trap +371, MOVE +233, bucket 13 +27, bucket 21 +4. Trap-end distances change with the held root, so trappers cycle (74 vs 56 Trap starts). Mechanism of the cycling not decoded (open item 3 of WalkLane7 still open).
- Verdict: fails M1 (ATrapsRoot red, Quick 697) and M2 (697 > 106). Reverted.

## 4. Changes in the branch
- Pushed: `72c61d6df` = merge of origin/Integrate (c7fa9d01d) into WalkLane. No code change. `git ls-remote origin WalkLane` confirmed 72c61d6df.
- Diagnostics reverted (`git checkout` of the five paths). Their diff is kept at `gf-WalkLane-scratch/wl8-diagnostics-lock-and-gdb-arm.patch` (101 lines: lock, gap print, gdb arm).
- Tests run this lane (none changed): `pes_restart_delay_test --gtest_filter=PesStallQuick.ALiveBall*` under gdb with the arm: PASS, forced_live=53 (no lock). `pes_pass_target_test --gtest_filter=PesPassTarget.ATrapsRoot*` with the lock: FAIL, worst 0.068035 m.
- Build dir `gf-WalkLane/build` holds binaries built WITH the lock/diagnostics (pes_pass_target_test). Rebuild before any gate.

## 5. Capture request (hook + predicate + what it must show)
Goal: check `FUN_141EB3360`'s inputs against PES (is the tick budget A+0x10 and the counter A+0xC the same in PES at the drop tick?).
- Hook: entry of `0x141EB3360` (A is rcx; PES A = P+0x428).
- Predicate: `d(rcx+0x8)==2` (state 2 at entry), EVERY 20 (per-man per-tick; never EVERY 1), cap 12, FULL (not slimmed).
- Must show: `s16 [rcx+0xC]` and `s16 [rcx+0x10]`, `d(rcx+0x6D)` at the entry of each call; the drop tick is the call where counter > budget (the function then clears `[rcx+8]`). Expect 2-3 captures per trap with counter <= budget and at least one with counter > budget.
- Also useful: the trap-end distance input of FUN_14079E5C0 is unchanged; not requested.

## 6. Open items (numbered)
1. PES's A+0xC / A+0x10 at the 2 -> 0 tick (section 5 capture). Decides whether the strike window length is PES's.
2. Warp input B+0x90 - O2+0xD0 is 0 while A+0x14 moves (section 3): find the PES writer of B+0x90 and O2+0xD0 and the tick order against `StepAdjusters`. Likely the real cause of the ATrapsRoot residual (INF).
3. Body motion not from the warp (194 of 573 samples): the clip root term not captured by `LockRoot` (smuggles / OffsetPosition), INF.
4. Trap cycling under the lock (74 vs 56 starts): which trap-end arm fires early (FUN_140797CD0 case 5/6 distance tests `FUN_1406E6CB0`).
5. Dribble series and action ids: not re-measured in this lane.

## 7. Next highest-value step
Open item 2: decode `B+0x90` and `O2+0xD0` writers and the tick order. The 0.068 m residual is in the warp input, not in the window timing, so the lock cannot be exact until this is fixed.
