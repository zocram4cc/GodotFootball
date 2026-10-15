# Wave 1 MB-F: ball record and prediction. Multi-seed table, pass-drop diagnosis, merge decision

Branch `Wave1MBF` on origin, not merged. Base `453640f`. Commits:
- `7f61a5a` pesmover: F4 floored velocity, F6 mover velocity, heading.
- `5714da4` pesballtrack: PES's 250-record track (F5; F3 = record 0), read by the AI through a floor.
- `faae222` AI reads interpolate between PES's records (the floor read's defect removed). Gate green: 725 tests, 100%, two standing install skips.

Worktrees: `gf-Wave1MBF` (branch, clean), `gf-Wave1MBF-base` (reference 453640f), `gf-Wave1MBF-diag` (diagnostic only, never committed). Logs: `gf-Wave1MBF-scratch/runs/`.

## 1. Answer

- **The pass drop does not reproduce.** Seed 7's −22 was one of five paired differences. The branch as first pushed (`5714da4`, floor read) is +5.8 passes against base on average (t 0.74).
- **The fix leans down, not significantly.** `faae222` is −7.6 passes on average (paired −36, +16, +4, −27, +5; SE 10). The two large negatives are seeds 7 and 11.
- **The defect was real and large.** The floor read gave zero ball movement in the intercept optimiser for 52% of calls in a full match, against 12% on base. The exact-time read gives 9%. The defect is gone in `faae222`; the outcome is not measurably changed.
- **The residual cause is the tick mismatch, not the reader.** The live ball runs on GF's 10 ms integrator. The AI now predicts on PES's 1/54 s tick. In a match the interpolated track misses the ball by 0.175 m at 1 s; the GF grid it replaced misses by 0.113 m.
- **A consistent variant (live ball on PES's tick, diagnostic) reads at base or above, and removes the mismatch.** It is +9 passes on four seeds (t 1.25), with completion +9 points, goals 37 against 21, and forced stalemate 4085 against 10332. It is a sim-wide change and not part of MB-F.

## 2. Multi-seed table (20 min per match, serial under the fleet lock)

Passes = pass attempts, H-A. Completion = accuracy, mean of the two teams.

| variant | seeds | passes mean | completion | shots total | goals total | forced mean |
|---|---|---:|---:|---:|---:|---:|
| base 453640f | 7, 8, 9, 11, 1000 | 87.4 | 49.1% | 147 | 28 | 10891 |
| floor read 5714da4 | 7, 8, 9, 11, 1000 | 93.2 | 54.4% | 171 | 35 | 11226 |
| interpolated read faae222 | 7, 8, 9, 11, 1000 | 79.8 | 49.2% | 141 | 32 | 9419 |
| live ball on PES tick (diagnostic) | 8, 9, 11, 1000 | 92.2 | 57.1% | 134 | 37 | 4085 |

Same four seeds (8, 9, 11, 1000): base 83.0 passes, floor 95.8, interpolated 82.5, live-PES 92.2.

Per seed (passes total):

| seed | base | floor | interpolated | live-PES |
|---|---:|---:|---:|---:|
| 7 | 105 | 83 | 69 | — |
| 8 | 69 | 77 | 85 | 100 |
| 9 | 80 | 84 | 84 | 86 |
| 11 | 99 | 124 | 72 | 100 |
| 1000 | 84 | 98 | 89 | 83 |

Paired passes against base: floor −22, +8, +4, +25, +14 (mean +5.8, SE 7.8). Interpolated −36, +16, +4, −27, +5 (mean −7.6, SE 10.1). Live-PES on the four common seeds +31, +6, +1, −1 (mean +9.3, SE 7.4). Seed-to-seed spread is about ±20 passes for any change, so none of these reads as a result.

Determinism hashes (per seed) are in `gf-Wave1MBF-scratch/table-all.md`. They differ on every seed, as expected, since the AI reads change.

## 3. Diagnostic evidence (full 20-minute match, seed 8 unless stated)

Controls and non-interference:
- The diagnostic gf10 read path (the base's 10 ms reads, through the branch's code) reproduces base seed 8 exactly: hash `c20f0efee27df7d0`, same summary. So the live sim is identical to base, and all differences come through the AI reads.
- The probe build (`floor`) reproduces the branch's seed-8 hash `098b63b6ce2cad92`. The instrumentation does not alter the sim.
- `faae222` reproduces the diagnostic interpolated run's seed-8 hash `22c265d8d027b6c4`. The committed code is the diagnosed path.

Records read (mental image): the image copies 250 records per tick and reads **162** (indices 0–161, 0–2.98 s). Records 162–249 are computed each tick and never read. Reads past 3 s are 4.2% of all reads, with the horizon clamp.

Phase: the floor read is early by −8.2 ms on average and 18.5 ms at most (match probe). The harness puts 98% of reads off the grid.

Intercept finite difference (`AI_GetToBallMovement`, `ballMovementRough = (P(T+10) − P(T))·100`, AIfunctions.cpp:872):

| variant | zero movement |
|---|---:|
| base (gf10 control) | 12.2% |
| floor | 52.2% |
| interpolated | 9.3% |
| live-PES (interpolated reads) | 11.0% |

Other finite-difference sites with 10–60 ms spacing: `humanoid.cpp:2144` (180−120), `2859–2860` (250−240), `3148–3149` (touch frame +10). `elizacontroller.cpp:846` reads `int(f × frame_ms)`, which can truncate an exact frame to one record early. Interpolation removes that.

Prediction against what the ball did (passive ticks; touches and resets excluded), full match:

| variant | horizon | GF 10 ms grid | track, floor | track, interpolated |
|---|---|---:|---:|---:|
| branch ball on GF tick (seed 8) | 100 ms | 2 mm | 36 mm | 6 mm |
| | 300 ms | 11 mm | 38 mm | 26 mm |
| | 1000 ms | 88 mm | 151 mm | 151 mm |
| interpolated (seed 8) | 1000 ms | 113 mm | 175 mm | 175 mm |
| live-PES ball on PES tick (seed 8) | 100 ms | 85 mm | 14 mm | 53 mm |
| | 1000 ms | 619 mm | 45 mm | 45 mm |

The GF grid is the integrator the ball runs on, so its 2 mm at 100 ms is the probe's floor. It is 88 mm at 1 s, which I suspect is the grounder counter (section 6), not verified. In the live-PES ball the stair-step is the real motion, so the floor read is the faithful comparison at short horizons.

Blend: changes a read only when the mental and live predictions differ by more than 2.5 m. That happened in 90,653 of 7.7 M reads (1.2%). It is GF's reaction-delay construct, with no PES site (the foundation doc lists it as GF-only, DESIGNED). Under PES-wins it should be replaced by the per-tick MA copy (MB-B), not kept as a GF term. `faae222` leaves it unchanged.

Rule rate: the same PES rules at 1/54 s and 1/108 s differ by up to 4.8 m over 3 s (roll 20 m/s; harness). The per-tick snaps (bounce, spin rebuild, stop gate) are not rate-invariant, so the `k^0.54` conversion does not make the 10 ms integrator PES. Against PES's own track, the 10 ms rules are 0.6 m (kick), 2.2 m (lob), 0.6 m (roll 8), 2.3 m (roll 20) off on average over 3 s.

## 4. Tick equivalence: does not hold

1. Read mapping (floor): fixed by `faae222`.
2. Physics: the live ball runs on GF's 10 ms integrator, the track on PES's 1/54 s tick. Prediction misses the ball by 0.11–0.18 m at 1 s in a real match, depending on variant. Open; see section 7.
3. Rule rate-dependence (section 3): the GF integrator is not PES at any rate. The keep-factor conversion is exact (`KeepFactorPerStep(0.981, 0.01)` equals `pow(0.981, 0.01·54)` to nine digits), but the snaps are per tick.

## 5. The pass result, interpreted

- Floor read (`5714da4`): the defect (52% zero) did not show in the outcomes. The +5.8 mean is noise.
- Interpolated read (`faae222`): outcomes at base level in four seeds (paired mean −0.5 on 8, 9, 11, 1000), and −7.6 on five (seed 7 added). The lean comes from seeds 7 and 11. The named candidate is the tick mismatch: the AI predicts PES's ball, 0.06 m further from the real ball at 1 s than the GF grid was. The data cannot attribute an effect of that size: its SE (10) is larger than the effect.
- Live-PES (diagnostic): the mismatch is gone. Passes +9, completion +9 points, goals +16, forced stalemate −60%. Four seeds, t 1.25. The flow change is large enough to need its own gate.

## 6. Pre-existing findings (not changed here)

- `contactTicks` is advanced inside every prediction pass (`ball.cpp:210`), by the constructor, `Touch`, `SetMomentum`, `SetRotation` and `Process`. A rolling ball advances it by up to 299 per call. The grounder gate therefore runs on a count that is not PES's contact-tick count. Suspected source of the GF grid's 88 mm at 1 s; unverified.
- Dead-ball loop: in every variant, the stalemate log shows restarts prepared but never started (`prepared 1 started 0`, `start-in −1.0 s`). 19–43 lines per match on base and branch alike. Not investigated.
- AI reads that bypass the mental image call `GetBall()->Predict(ms)` directly (playercontroller 214–268, humanoid 504 and 1950). These stay on the GF grid in every variant. Under a PES-tick ball they are inconsistent (62 cm at 1 s on the grid).
- The live netting stand-in runs on the first prediction step only. The track has no netting (named ponytail). The live-PES diagnostic also drops netting.

## 7. Merge decision

Not mergeable on the criteria as they stand.

- The floor read (`5714da4`) has a real defect (52% zero movement in intercepts) and an unfaithful reader. Do not merge.
- `faae222` fixes the defect with a named stand-in (linear interpolation of PES's records). Its outcome is at base level on four seeds, and −7.6 on five (SE 10). Its prediction is PES-correct but inconsistent with the sim ball (0.175 m against 0.113 m for the grid it replaced). That is a named cause, but it sits in the live ball's tick, which the branch does not own. So it is not "noise with a faithful structure behind it". Do not merge alone.

Decision for Main, in order:
- **(A) Faithful path, recommended in principle.** Move the live ball onto PES's 1/54 s tick, as in the diagnostic. This also needs: (i) floor reads for a stair-stepped ball, not interpolation, (ii) the 10 ms finite differences (four sites) rewritten as record differences times 54, (iii) `Predict(ms)` direct reads moved to the track, (iv) netting on the PES tick. It is sim-wide. It needs its own ticket, a gate, and at least eight seeds.
- **(B) Hold.** Keep MB-F unmerged until (A) is decided. Base's AI reads stay in place. The branch's structures stay unread.

I did not commit a doc. The RE rule sends reports through the archive tool; this table lives in this local report.

## 8. Corrections

- `docs/re/pes-re-ball-contact.md` §4 says `FUN_140869500` is the BallInfo future-data writer. Refuted. It is a launch-vector solver: it loops the integrator up to `[matchctx+0xd0]` steps and writes two caller-supplied float vectors. It stores nothing into BallInfo.
- **Track writer not found.** Six integrator callers contain 250-step loops: FUN_140817af0, FUN_1408183c0 (lines 331–370), FUN_140869d10, FUN_14086a3f0, FUN_14086ab90, FUN_14086b690 (lines 167–183). They are kick and landing solvers that write their own outputs. None has a 40-byte-stride store into BallInfo. FUN_1408183c0 copies one dword at +0xBC between two structures (lines 143–151), not traced further; FUN_14086b690 reads +0xBC once. The other integrator callers (40 call sites in 21 named functions, plus two outside any named function) have no 250-step bound. Doc §5.3's "shifted every tick" is unverified.
- Earlier report, section "Stand-ins": "Track rebuilt in full each tick, equal to a shift when unperturbed [INFERENCE]" is not verified. The writer is not found, and a rebuilt track differs from a shifted one wherever the 54 Hz phase changes.
- Earlier report: "the 2.5 m blend is kept and named" is corrected. It changes 1.2% of reads, and it is GF's construct (section 3).
- Earlier report: the seed-7 "passes 49-56 → 47-36" is not a symptom of the floor branch. It is one of five paired differences.
- Earlier report: "the AI reads the PES prediction, faithfully" is corrected. The reader was broken (52% zero movement), now fixed in `faae222`. The prediction is PES's and is inconsistent with the sim ball.
- Earlier report: "the live numerics are unchanged". Confirmed: the seed-8 control hash equals base.
- The first report's "one-seed shift" framing is withdrawn; see section 2.

## 9. Artefacts

- Commits: `7f61a5a`, `5714da4`, `faae222` (on origin `Wave1MBF`; not merged).
- Gate (on `faae222`): full `ctest`, 725 tests, 100% (two standing install skips). PII scan clean.
- Mutation checks on `faae222`'s tests: floor read fails 3 tests; no horizon clamp fails 1 test. Earlier mutations on the mover and sampler are in the first report.
- Diagnostic-only (in `gf-Wave1MBF-diag`, not committed): `diag_grid_test.cpp` (tick-equivalence harness, PES 1/54 reference), `diagprobe.hpp` and its hooks (counters, read modes `DIAG_READ`, blend `DIAG_BLEND`, gf10 control `DIAG_SRC`, live PES `DIAG_LIVE`).
- Harness output: `gf-Wave1MBF-scratch/diag-grid2.txt`.
