# A1 T2 (lift after a landing) and landing vertical response, branch Liftoff

Result: NOT accepted. 1 s median is above T1+T3 on both harnesses. Branch `Liftoff` at `86586d2` (pushed, `git ls-remote origin Liftoff` confirms). Base `a47f5f2` (origin/main). Commits: `5e1b023` (T2 lift as flight step), `86586d2` (landing inputs from pre-flight state; grounder trims vy only).

## Item 1: live ball velocity (not found; B+0x94 refuted)
- Read-only capture `cap2.bin` (240 s, 9,368 snapshots, `capture2.py`): pos88 at B+0x88, candidate velocity at B+0x94, track block.
- Flight check, 5,332 airborne pairs: |54·(p[g+1]-p[g]) - v[g+1]| median 7.94 m/s, 0 exact. B+0x94 is not the velocity.
- Window scan (`capture3.py`, 120 s, 4,243 frames, B+0x40..B+0x1C0, lags -1..1, direction cosine and magnitude): best |cos| 0.48. No offset holds the velocity.
- Substitute, exact up to the flight term: PES's post-response velocity at a landing record k is v_resp = (P[k+1]-P[k])·54 - a(P[k],v_resp)/54. Used for contact starts (A1_V0) and as the landing reference (A1_LAND).

## Item 2: landing vertical response (VA decode)
- CONFIRMED (contact-2 §1.2 verbatim, and the disasm): vy_out = -F·min(-imp, vy_trim); the spin pump writes raw Y, not vy (`T_y ≡ 0` at 0x140876f45). No tangential term in vy.
- CONFIRMED: imp uses |v_xz| from entry state (0x140876824, xmm11 from [rsp+0x58/0x60]), not post-flight. The grounder gate reads old x (0x1408769c7), old z (0x1408769e6), and guards read old vy (0x1408769a4) and old |v_xz| (0x1408769ab). Port: `ContactTick(..., preFlight)`.
- CONFIRMED: grounder: K *= (f + 0.985) at 0x140876a57; vy -= |v_xz_pre|·0.09·(1-f) at 0x140876a5c. No horizontal trim instruction. GF had a horizontal trim; removed.
- CORRECTION: the gate divisor row (Ball+0x1d8 + m·4) is the loader's kGounnderprob (5 for mode 1), not frictionRollSpeedStop (0). GF already reads 5; the gate is live. `docs/re/pes-re-ball-contact.md` §2.5 "mode 1 = 0" is wrong.
- Not ported: F's s4 term is in GF (rollToBoundAdd); spin pump (raw Y only).

Landing velocity over 1,635 PES landings (exact v_resp reference, one BallTick from PES's pre-landing state):

| variant | median abs dvy | p90 abs dvy | median abs dv | within 0.02 m/s |
|---|---|---|---|---|
| main (T1+T3) landing | 0.0209 | 0.4696 | 0.2638 | 53 |
| 5e1b023 (T2) | 0.0209 | 0.4696 | 0.2638 | 53 |
| 86586d2 (pre-flight) | 0.0154 | 0.1397 | 0.2321 | 52 |

Horizontal error median 0.23 m/s is unexplained (not decoded; not the grounder trim).

## Item 3: mutation guards (TDD)
- `ABallExactlyOnTheGroundWithNoVerticalSpeedStaysAirborne` (gravity 0, vy 0, y = r, one BallTick): stays airborne. Mutant `>` at the r<=y test: FAILS (caught). Reverted.
- `TheGrounderGateReadsTheStateBeforeTheFlightStep`: mutant contact reads post-flight state: FAILS (caught).
- `TheGrounderTrimsTheVerticalSpeedAndOnlyItsFrictionOnTheHorizontal`: on 5e1b023 sources: FAILS (ratio 0.8865 vs 0.985), GREEN on 86586d2. Caught by red-on-HEAD run.
- Gate test on 5e1b023 sources was NOT red (the post-flight x passed the gate there). It was red only under mutation. The vx value changed 6.0 -> 5.8 to make the post-flight x fail; that was not re-run on 5e1b023.
- Tests green on 86586d2: pes_ball_track_test 14/14, pes_ball_physics_test 25/25, pes_ball_collision_test 10/10, pes_tick_a1_test 12/12. Build: 0 new warnings in touched files. Gate not run (Main gates).

## Acceptance numbers (1 s median, e54, pooled)

| variant | recorded 253 (uncorrected starts, BallContact harness) | live 168 (uncorrected) | recorded 253 (start-corrected) | live 168 (start-corrected) |
|---|---|---|---|---|
| main T1+T3 | 0.3864 | 0.4720 | 0.3325 | 0.4354 |
| 5e1b023 T2 | 0.5552 | 0.5922 | 0.4372 | 0.5523 |
| 86586d2 T2+pre-flight | 0.5725 | 0.6547 | 0.4993 | 0.6263 |

Uncorrected T1+T3 reproduces the stated 0.386/0.472 exactly, so the harness is the same. Target (< 0.386 / < 0.472) NOT met by any variant.

Live capture 3 (`cap2.bin`, 123 samples, start-corrected): main 0.3030, T2 0.4356, 86586d2 0.4441.

## Why T2 loses (f2409107, start-corrected)
- Flight part is exact: T2 err ≤ 0.00024 m for records 0–12. Main (T1+T3) err grows to 0.15 m by record 12. T2 is right on the lift.
- Rolling phase diverges: PES's velocity rotates (vz 7.98 → 9.24 while vx decays), GF rolling keeps horizontal by K only. Error 0.0006 at record 17 → 2.16 m at record 53.
- Not ported: the rolling block `0x140876c0f … 0x140876f6e` (decompile lines 283–370), which builds horizontal velocity deltas from raw spin × unit horizontal velocity. Its constants (DAT_142588218, DAT_142565600, DAT_142578480) are unresolved.

## Recommendation
- The lift (T2) is decoded and correct on the flight part, but e54 gets worse on both harnesses until the rolling block (above) is ported. Main decides whether to keep T2.
- `86586d2` is the VA-correct landing semantics (landing velocity error tail improved, p90 0.47 → 0.14 m/s). It makes e54 worse by ~0.05 m. Drop it if the branch must stay on the metric.

## Files (scratch, not committed)
- `<workdrive>/gf-Liftoff-scratch/`: `capture2.py`, `cap2.bin`, `analyze_v.py`, `samples-cap2.txt`, `capture3.py`, `cap3.bin`, `scan_v.py`, `a1_port_probe.cpp` (env A1_V0 start correction, A1_LAND landing reference, A1_DUMP, A1_EACH), `decomp.c` (Ghidra FUN_140876360).
