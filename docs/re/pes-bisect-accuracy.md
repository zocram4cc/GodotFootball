# AccuracyBisect: attributing the 20% -> 33% pass-accuracy jump (6114ef3 -> a47f5f2)

No code changes. Branch `AccuracyBisect` `fc56ab7` (empty commit on `origin/main` `10edd6f`, confirmed by `ls-remote`).

## Trees (scratch worktrees, all on 6691a8a-code-identical bases)
- main+A2Stall: `9c92eb8` (A2 wiring + touchline fold + restart watch)
- +HitIndex alone: `e99e7af` (HitData `(w0&0x1fff)+0x11`; base code == `6114ef3`)
- +BallContact alone: `fc22bf2` (T1 same-tick landing + T3 mode 1; base code == `6114ef3`)
- main+A2Stall+HitIndex: `222dae7` (merge; == 9c92eb8 + e99e7af + report-only 508a48d)
- main+A2Stall+HitIndex+BallContact: `a47f5f2` (== 222dae7 + T1/T3)

Smoke: 20-min seed-7, `gfsim_run --db-sql assets/fixtures/teams.sql --home 1 --away 2 --seed 7 --minutes 20 --pes21-root <workdrive>/PES21`, exit 0 on all five.

## Results
| tree | passes H-A | accuracy H-A | shots H-A | goals H-A | determinism |
|---|---|---|---|---|---|
| 6114ef3 baseline (handover) | 36-43 | 19%-20% | — | — | 7407dc1ab4162b83 |
| main+A2Stall (9c92eb8) | 41-33 | 24%-9% | 16-6 | 4-2 | c2c8b71f468db66d |
| +HitIndex alone (e99e7af) | 36-43 | 19%-20% | 10-5 | 3-3 | 7407dc1ab4162b83 |
| +BallContact alone (fc22bf2) | 29-36 | 24%-22% | 2-4 | 1-4 | 9a7fbcfe864aa836 |
| +A2Stall+HitIndex (222dae7) | 41-33 | 24%-9% | 16-6 | 4-2 | c2c8b71f468db66d |
| +A2Stall+HitIndex+BallContact (a47f5f2, re-run) | 29-25 | 31%-36% | 5-10 | 0-5 | 6d0e1e05fa65998d |

A2Stall and the a47f5f2 re-run print `[restarts]`/`[idle]`; the older trees print `[stalemate] forced` (report-only 508a48d).

## Attribution
- Neither addition alone reproduces the jump: HitIndex alone is bit-identical to the 6114ef3 baseline (same hash, same 36-43/19%-20%); BallContact alone moves accuracy only to 24%-22% on a different trajectory (fewer passes: 29-36, shots collapse 10-5 -> 2-4).
- The full 19%-20% -> 31%-36% jump appears only when BallContact lands on the A2Stall trajectory (222dae7 24%-9% -> a47f5f2 31%-36%). A2Stall's event-store/latch changes move seed 7 onto a different trajectory (determinism hash changes, passes 36-43 -> 41-33); BallContact's effect is trajectory-dependent.
- Mechanism (from the report lines, not root-caused in code): BallContact alone drops pass volume (65 vs 79 attempts) and shots (6 vs 15) — fewer, shorter balls. On the A2Stall trajectory the same change drops attempts 74 -> 54 while completions concentrate (away trap fails 11 -> 65 is the away side keeping the ball; home out-of-bounds 15 -> 10). The jump is selection (which passes get attempted/completed on the new ball), not a passing-logic fix: no commit in range touches the passer, receiver, or interception code paths.
- HitIndex contributes nothing measurable on seed 7 in either trajectory (identical hash and lines with and without it on both bases). Expected per its report: the dodge `KindRow` ball-owner fit can now pass rows that were previously rejected, but seed 7 shows no pass/shots/goal movement from it.

## Faithfulness verdicts (with VAs)
- HitIndex +0x11: faithful, CONFIRMED. `FUN_1406ad300` files HitData at `(w0 & 0x1fff) + 0x11` (`AND AX,R8W` 0x1406ad42a..434, `ADD AX,0x11` 0x1406ad438); sibling builders confirm the family pattern (Animation/Cancel +0, SubHit +0x61, Foot +0x10, Pivot +0x20b). Install cross-check 2925/2925. It just does not move this smoke. (`docs/re/pes-re-hitdata-index.md`)
- BallContact T1 same-tick landing: faithful, CONFIRMED live (one-step landing error 0.000000 m over 1092 recorded + 872 live steps). (`docs/re/pes-re-ball-contact-port.md`)
- BallContact T3 default mode 1: PARTIALLY faithful, OPEN item attached. Mode 1 at table `0x14349f718` entries (0,1)/(1,1) and the horizontal-keep live fact are CONFIRMED (`docs/re/pes-re-ball-contact-2.md` §3.1, `pes-re-ball-track-live.md` §2.6); but which (pitch, weather) index the live match uses is OPEN — writer `FUN_1408723d0` at 0x140872403/0x140872411 not decoded — so every match plays mode 1 via a `ponytail:` until the index writer is ported. The port report itself notes T1+T3 overshoots the 1 s median (0.386/0.472 m vs ~0.35 m for T1 alone) with T2 unported.
- A2Stall touchline fold: GF stand-in, OPEN VA. The fold fixes the observed freeze (free kick spotted 0.2 m past the line; `humanoid.cpp:3156-3157` 34.17 m touch reach refuses every touch frame; `referee.cpp:579` waits forever), but PES's own touch-reach VA is not decoded. (`docs/re/pes-re-touchline-restart-stall.md`)

## The attributed change
The jump is BallContact's T1+T3 acting on the A2Stall-moved trajectory — an interaction, not a single commit. The ball change is faithful with the stated OPEN item (mode-1-for-every-match until the index writer is decoded); the trajectory it acts on includes A2 behaviour changes plus one GF stand-in (touchline fold). Nothing in the jump suggests an unfaithful tuning: no passing/AI constant was touched in range.

## Open items
1. Why BallContact cuts pass/shot volume (selection mechanism) is unread from these lines; a pass-attempt/trap breakdown per tree would say whether it is fewer long balls, more traps, or keeper collections.
2. HitIndex's expected dodge-fit effect is invisible on seed 7; other seeds may show it.
3. The mode-index writer (`FUN_1408723d0`) decode decides whether T3 stays unconditionally on.
