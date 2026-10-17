# CarrierGainFix — handed-over state (no commits, no push)

Worktree: `<workdrive>/gf-CarrierGainFix` (detached at `c21fa11` = origin/main,
"docs: verify the COM styles write-up against the binary; log the GF carrier-port
mismatches"). `extern/godot-cpp` initialised as a real submodule clone at
`507ed9d8`. `build/` configured (`cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo`,
Makefile generator). **No source edits were made; nothing staged, nothing committed.**

Stopped by the orchestrator: the owner wants this pair restarted as
`tdd-task-implementer` agents. This file is the handover; the next agent should
re-do the red tests against `origin/main` head `c21fa11` in the worktree above
(reuse it, don't delete).

## Confirmed bug sites (all read from the binary-verified reports)

| # | Site | File:line | PES VA | Status here |
|---|---|---|---|---|
| 1 | `breakthroughStatGain` never assigned in production | declared `src/sim/onthepitch/pescarrier.hpp:165`, read `pescarrier.cpp:584`, never written in `LoadCarrierConstants` (only `tests/sim/pes_carrier_test.cpp:128` sets it) | gain 15 @0x14259BE0C (com-styles §3.17, F11) | not started |
| 2 | `crossStatGain` never assigned in production | declared `pescarrier.hpp:173` (comment wrongly says `0x14259be2c` = 40.0), read `pescarrier.cpp:586` | gain 20 @**0x14259BE1C** (com-styles §3.17, F12) | not started |
| 3 | Plan priority table missing 7 | `pescarrier.hpp:386` `kPlanPriorityOrder[] = {9,8,2,3,1,4,5,6}`; test pins the 8-entry size at `tests/sim/pes_carrier_test.cpp:537` | `[9,8,2,3,1,4,5,6,7]`, table @0x142BF02C0 + **inline 7 in 0x14211E2D0** (com-styles §4.12 / F5) | not started |
| 4 | Tried-before order: Breakthrough admitted before MiddleShot | `src/sim/onthepitch/player/controller/elizacontroller.cpp:2044-2056`, the `choices[]` initialiser lists `kPlanBreakthrough` first | dispatcher order 2 (MiddleShot) before 3 (CurveShot) before 1 (Breakthrough), 0x14211E2D0 (com-styles F6) | not started |
| 5 | Truncation order per evaluator | `pescarrier.cpp:590` `statTerm` is one float term truncated once at the call site | Breakthrough/MiddleShot **per-stat integer** (`CVTTSS2SI` then `ADD EBX`): 0x14211A9B3-AA7C / 0x14211B6D4-D9; Cross/CurveShot/Long **per-stat float then truncate per step** (`ADDSS` then `CVTTSS2SI`): 0x14211AD0E/AD4D (com-styles §3.18 / F27) | not started |

Supporting facts read this session (verified in `docs/re/pes-re-com-styles.md` §3.17-3.20):
- stat ramp floor 70 @0x14259BE4C; divisor 29 @0x1425A9460 for MiddleShot/Cross/CurveShot/Long,
  **20 @0x14259BE1C for Breakthrough** (F10/F26 — deliberately **out of scope** for this wave;
  CarrierImagePort was told not to touch it either).
- Breakthrough cap 70 @0x14211AA7E, no floor; MiddleShot cap 80 / floor 5; Cross cap 80 /
  floor 40; Long none/none.

## Scope boundary agreed with peer CarrierImagePort (over IRC)

I take: the two gain loads, the dispatcher priority table (add 7), the tried-before
order in `elizacontroller.cpp`, and the truncation order. CarrierImagePort takes:
`kPlanCurveShot` in `PlanStartProbability` (+ `curveShotBase/StatGain/Floor/Cap`,
seed K 1300 already loaded at `pescarrier.cpp:254`), the Intention state, and the
arbiter persistence. Because both waves touch `PlanStartProbability`/`choices[]`,
the next agent should re-read this file after CarrierImagePort's branch lands.

## Assignment (unstarted) — what the next agent must do

Strict TDD per repo rules, one red test per bug first (must fail against
`c21fa11`), then the smallest fix, then one mutation check per branch:

1. `tests/sim/pes_carrier_test.cpp`: a test that the gains are **loaded**, not
   merely defaulted — e.g. a synthetic-k P-table test that fails when
   `breakthroughStatGain`/`crossStatGain` are 0 (they are today at runtime, and the
   current synthetic() masks the bug by assigning them itself). The install test
   path (`LoadCarrierConstants`, `pes::PeImage::ReadVa`) must be exercised so the
   load itself is covered; check the VA comments too (item 2's is wrong).
2. `kPlanPriorityOrder` grows to 9 entries ending in 7; update the `static_assert`
   and loop bound at `tests/sim/pes_carrier_test.cpp:537-538`.
3. `elizacontroller.cpp`: MiddleShot before Breakthrough in `choices[]` (CurveShot
   and SideChange are CarrierImagePort's/not ported — leave the array shape as-is,
   only reorder; do not add plans here).
4. Per-stat truncation order inside `PlanStartProbability`/`statTerm`.

Then: carrier suite (`tests/sim/pes_carrier_test.cpp`) → full gate under
`flock <workdrive>/gf-sim.lock` with `PES21_ROOT=<workdrive>/PES21` and the
EDIT00000000 save, `ctest --test-dir build -j3 --timeout 2400` → seed-7 smoke
(`build/apps/gfsim_run --db-sql assets/fixtures/teams.sql --home 1 --away 2
--seed 7 --minutes 20 --pes21-root <workdrive>/PES21`, range reported against
main's standing seed-7 line in `tasks/14-10-26.md`). Report every seed run.

## Mutation checks: none run.

No code changed, so no mutation was applied and no test was run against this
worktree (build was configured only — the `grep` job `bg_170` that was still
running was read-only). The red/mutation evidence for the four bugs above is
entirely uncollected.

## Not verified by tools this session (state as inference, marked)

- I did not independently re-read the bytes at 0x14259BE0C / 0x14259BE1C from the
  install; the values 15/20 and the VA for Cross's gain are taken from
  `docs/re/pes-re-com-styles.md` §3.17 (§"CONFIRMED" rows F11/F12/F27, plus
  `pes-re-carrier-port.md` §3.6). The next agent must confirm against
  `<workdrive>/PES21` via `PeImage::ReadVa` before trusting them.
- The exact `CVTTSS2SI` site lists for item 5 come from the same report.
