# pes-port-StallContact: the contact probe read its clock one tick late; the grant already touches (branch StallContact, 46aaf971)

## Verdict

Every granted trap touch touches the ball in the tick it is granted. The red test `PesStall.ATrapTouchIsTheBodyScansGrantAndNothingElse` measured a clock, not a touch. The grant path in `Humanoid::Process` needed no change. The fix is in the probe (`tests/sim/pes_stall_test.cpp`), and no simulation code changed.

Note on the brief's premise: the grants do not "lose" their touch. Read against the probe's own time base, the stamp is one tick earlier than the probe believed.

## Evidence (CONFIRMED)

Order inside one tick:

- `Match::Process` (src/sim/onthepitch/match.cpp, function starts l.861) runs `RunPesTrapTicks`, then `RunPesBodyScans` (l.899). The scan fills the grant (`Match::GrantPesTouch`, match.hpp l.~333).
- The team tick then runs `Humanoid::Process`. There `Match::TakePesTouch` (humanoid.cpp l.758) hands the grant over, and the touch block (l.759 onward) runs unconditionally: `Ball::Touch`, then `Team::SetLastTouchPlayer` (team.cpp l.377), which stamps `player->SetLastTouchTime_ms(match->GetActualTime_ms())`.
- `actualTime_ms += kTick_ms` is the last statement of `Match::Process` (match.cpp l.1052).
- The probe's `OnTick` runs after `match.Process()` (tests/sim/pes_shared_match.hpp l.195). It compared `GetLastTouchTime_ms()` with `GetActualTime_ms()`, which by then is the next tick's start. A touch could never equal "now".

Per-grant trace, original probe, seed 7, 2-min pes-loop match (`<workdrive>/gf-StallContact-scratch/run_diag1.log`, lines `DIAGGRANT`): 11 grants, all 11 with `now - last = 18.5 ms` (one tick), all on the trap contact frame (`frame=33/…`, fn 14 = Trap). Example: `now=3870 last=3851 slot=9 clip=trap_0_3_135_y0_in_ver12 frame=33/101 pesTrap=1`.

Consequences: `grantsWithoutTouch` was 11 of 11 by the clock alone, and `touchesWithoutGrant` could never count (`touched` was always false), so the test's second assertion was blind.

## Fix (test only)

`TrapContactProbe` records the tick start (`OnStart`, and the end of `OnTick`) and compares a touch's stamp with it. No source file changed. The sim binaries do not link the probe, so trajectories are unchanged.

Commit `46aaf971` on `StallContact` (pushed; `git ls-remote origin StallContact` = `46aaf971791881c9387aec02973130cbab599ec2`).

## Results (CONFIRMED)

| Run | Tree | Result | Log |
|---|---|---|---|
| Original probe (diagnostic) | f2458223 | FAIL: grantsWithoutTouch 11 of 11 (clock) | run_diag1.log |
| Fixed probe, unmutated | 46aaf971 | PASS (611,793 ms) | run_fix1.log |
| Mutation M1: take the grant, skip the touch (humanoid.cpp l.759 `pesTrapTouch && false`) | fixed probe | FAIL: grantsWithoutTouch 13 of 13 | run_m1.log |
| Mutation M2: also touch at GF's contact frame with no grant (l.759 `pesTrapTouch \|\| touchFrame==frameNum`) | fixed probe | FAIL: touchesWithoutGrant 34 | run_m2.log |

`PesStall.*` on 46aaf971 (run_pesstall_all.log): 6 pass, 1 red: `ALiveBallNeverLiesStillLongEnoughForTheBreaker` census.liveFirings = 606 (want 0). That is the f2458223 value in the brief (not worse; the breaker is StallLive's). `ATrapTouchIsTheBodyScansGrantAndNothingElse` green.


Both mutants were reverted; `git diff` of src/ is empty on the branch. M1 and M2 are the proof the probe now detects both faults.

Smoke, pes-loop seed 7, 2 min (`gfsim_run --seed 7 --minutes 2 --pes-loop`): full time reached, `[ticks] 36252 sim ticks to full time (cap 194400)`, `[determinism] dd235817a7383b5c`, `[restarts] 22 stoppages, longest 23.6 s`, `[idle] 7 episodes, stalls 7`. Log: smoke_pesloop.log. No source changed, so this equals f2458223's smoke.

## Ball response to a granted touch (named gap, not ported)

The scan's output record carries PES's ball velocity (kind 4, `+0x9C`). GF applies its own `GetTrapVector` instead (humanoid.cpp, trap block, `ponytail:` label). PES's kick application `FUN_140870A40` (sole caller `FUN_140425010`, calls `FUN_140874860`) is not translated. Upgrade: translate `FUN_140870A40` and `FUN_140874860` with a real-call proof, then feed the record's `+0x9C` vector and kind to it. Out of scope for this ticket; it is the ball's response, not the grant.

## Open items

1. `PesStall.*` full run: done, see Results (6 of 7 pass; the breaker census 606 is the f2458223 value, StallLive's).
2. `GATE_TIER=fast` not run (Main gates; this change is test-only).
3. The grant path has no PES-side mismatch found in this pass; the seed-7 grants all land on the contact frame (33). Drifted seeds not censused.
