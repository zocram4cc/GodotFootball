# A1 measurements: paired sim (main vs ddc67cd) and live track error at 1 s

**Status.** Live measurement (§2): complete on two live stretches (253 samples) and a third capture (168 samples, §2.9). Paired sim table (§1): partial, see the seed list in §1. The A1 port of T1 and T3 is on branch `BallContact` (§2.9); T2 and T4 are not ported. Scripts and the report only.

## 0. Scope and corrections to the brief

- **The `ddc67cd` label is wrong.** `ddc67cd` is not pre-A1. A1 (`BallTick`: `00eb224`, `3199211`) merged as `6106aa8`, which is an ancestor of `ddc67cd`. So main − `ddc67cd` measures only what landed after A1 (among it `f96f2ed`, collision pushes on the 1/54 s tick; `d1c15c0`, carrier seed; `ed8a93c`, flat headings). The true pre-A1 commit is `70d323c`, the first parent of the A1 merge. An A1-only pair is `70d323c` → `6106aa8`.
- **Seed count.** The brief lists eight seeds, 7, 8, 9, 11, 1000, 2, 3, 4, each 20 minutes. I first queued a ninth seed, 20, by reading the trailing "20 minutes" as a seed. It never ran and was removed.
- **Scope cut (Main, mid-run).** Main limited the sim table to main vs `ddc67cd`, with seeds still missing. The `pre` (`70d323c`) and `a1` (`6106aa8`) trees were dropped from the table. Their seed 7 and seed 8 outputs exist in `runs/` but are not in §1. The A1-only comparison is therefore **not measured**.
- **Builds.** Four detached worktrees, `the worktree{main,pre,a1,base}`, each with `gfsim_run` built in the default configuration (`-O2 -g -DNDEBUG`). They are left in place for reruns.

### 0.1 Determinism and noise assumptions

- **Determinism.** Same build and seed gives the same hash. Evidence: main seed 7 gives `63286486ec6c84de`, the same value the tracker records as the seed-7 smoke for `f96f2ed` and the Heading2D merge, so a fresh build of a later commit reproduces it. `ddc67cd` seed 7 gives `2f6242c5556975df`; the tracker's commit attribution for that value is inconsistent, so it is not used as evidence. No same-build repeat was run (scope cut).
- **Seed noise.** The SD of each metric across the completed seeds of the base build. The paired delta is main − base per seed; its SE is SD(delta)/√n. A metric is flagged MOVED when |mean delta| > 2 SE. With few seeds this flag is weak; the per-seed rows are in the appendix so it can be read directly.

## 1. Paired sim table: main (`e6487f8`) vs base (`ddc67cd`)

Status: **partial, two paired seeds.** Seeds 7 and 8 are complete for both `main` and `base`. Seeds 9, 11, 1000, 2, 3, 4 were **not run**: the queue was stopped under Main's scope cut because each run waited on the shared lock for 15 to 20 minutes. With two seeds, the SE and the MOVED flag are very rough and no metric is flagged; read the per-seed rows. The table below was generated from `runs/` by `build_sim_section.py`.

### Seeds completed

- main (e6487f8): [7, 8]
- base (ddc67cd): [7, 8]
- paired (both complete): [7, 8]

### Per seed: score and determinism hash

| seed | main score (H-A) | base score (H-A) | main hash | base hash |
|---:|---|---|---|---|
| 7 | 4-2 | 3-2 | 63286486ec6c84de | 2f6242c5556975df |
| 8 | 2-0 | 3-2 | 1b503a829c6ce191 | 1eb98a93374b7bb1 |
| 9 | not run | not run | - | - |
| 11 | not run | not run | - | - |
| 1000 | not run | not run | - | - |
| 2 | not run | not run | - | - |
| 3 | not run | not run | - | - |
| 4 | not run | not run | - | - |

### Paired deltas (main minus base) over the completed seeds

| metric | base mean | base seed SD | main mean | delta | SE of delta | SD of paired delta | flag (abs delta > 2 SE) |
|---|---:|---:|---:|---:|---:|---:|---|
| passes (H+A) | 92 | 21.92 | 67 | -24.50 | 34.50 | 48.79 | within noise |
| pass completion % (mean) | 20.2 | 3.18 | 24.8 | +4.50 | 4.00 | 5.66 | within noise |
| shots (sum of bands) | 12 | 1.41 | 12 | -0.50 | 7.50 | 10.61 | within noise |
| offsides | 0 | 0.00 | 0 | +0.00 | 0.00 | 0.00 | within noise |
| free kicks | 13 | 2.83 | 9 | -4.00 | 3.00 | 4.24 | within noise |
| clearances | 8 | 4.95 | 8 | -0.50 | 0.50 | 0.71 | within noise |
| keeper holds | 4 | 3.54 | 4 | -1.00 | 3.00 | 4.24 | within noise |
| keeper out of reach | 2 | 1.41 | 4 | +1.50 | 1.50 | 2.12 | within noise |
| failed receptions | 44 | 22.63 | 63 | +19.00 | 33.00 | 46.67 | within noise |
| completions lost | 4 | 2.83 | 1 | -3.00 | 3.00 | 4.24 | within noise |
| total goals | 5.00 | 0.00 | 4.00 | -1.00 | 2.00 | 2.83 | within noise |

### Per-seed balance numbers (all completed runs)

| seed | build | passes (H+A) | pass completion % (mean) | shots (sum of bands) | offsides | free kicks | clearances | keeper holds | keeper out of reach | failed receptions | completions lost |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 7 | main | 86 | 18.5 | 18 | 0 | 10 | 5 | 4 | 6 | 80 | 2 |
| 7 | base | 76 | 18.0 | 11 | 0 | 11 | 5 | 2 | 3 | 28 | 2 |
| 8 | main | 48 | 31.0 | 5 | 0 | 8 | 11 | 3 | 1 | 46 | 0 |
| 8 | base | 107 | 22.5 | 13 | 0 | 15 | 12 | 7 | 1 | 60 | 6 |


## 2. Live track error at 1 s (PES's track vs GF's `pesball::BuildTrack`)

**Result: not met.** On 118 live samples from one continuous 240 s stretch of play, GF's track at 1 s (record 54) is 0.63 m from PES's record 54 at the median (mean 0.92 m, max 4.30 m). Only the 8 samples where the ball was in free flight for the whole second are inside 0.05 m (all of them to 0.0002 m). The target is ≤ 0.05 m.

### 2.1 Method (what was read, what was fed)

- **Process and chain (read-only).** `sudo` `pread` of `/proc/<pid>/mem` on PES2021.exe, nothing written. `G = *(0x1436F3DC8)`, `MI = *(G+0x270)` (valid only in part of each tick, so every read re-resolves `G`, `MI`, `B`), `B = *(G+0x40)` (sim ball), frame = `u32(MI+0x1198)`. Ball position = `B+0x88` (vec3).
- **Track layout (confirmed live, not from the docs).** Record `r` (r = 0..248) is 40 bytes at `B+0xB0+40r`: position at +0, unit quaternion at +12 (`B+0xBC+40r`), raw spin at +28. `docs/re/pes-re-gaps-match.md` §9.2b and `pes-re-officials-ball.md` §5.3 place the record at `+0xBC`; that is the quaternion, and the record starts at `+0xB0`. Record 249's slot holds no quaternion and is not used.
- **Alignment (exact).** Record `r` of the snapshot at frame `f` equals the sim position `B+0x88` read at frame `f+r+1`. For `r = 0` the difference is 0.0000 m in 97.1 % of snapshots, and the median is 0.0000 m at every `r` up to 54. The growing non-exact fraction (42 % at r = 54) is player touches, which the PES track does not model; those snapshots were not excluded from the table.
- **Initial state fed to `BuildTrack`.** Position = record 0. Velocity = `(record0 − pos88(f)) · 54` (the semi-implicit step that produced record 0). Raw spin = record 0's spin. `contactTicks` = 0 and `recentHit` = 0: GF's `GrounderKick` ignores `tickIndex` (`(void)tickIndex`), and no sample has |x| above 24.4 m (both stretches), so the ball never comes within 28 m of the goal line and the goal frame does not fire. Constants come from the install (`LoadPesBallConstants`); the goal frame is `ball.cpp`'s `kGoalFrame`.
- **Samples.** A snapshot is used when its next frame was captured and its record 0 equals the next sim position (< 1 mm), its quaternions are unit norm, and no record step exceeds 0.6 m. Samples are spaced ≥ 60 frames (≈ 1.1 s) and the ball moves ≥ 0.5 m/s. That gives 118 samples from 7,932 captured snapshots (6,886 verified). Nothing was chosen beyond that spacing.
- **Comparison.** For k = 0..54, error_k = |GF track[k] − PES record k|. Reported per sample: e54 (the 1 s error), max_k and mean_k over k = 1..54.

### 2.2 Per-sample result on current main (`e6487f8`, `BuildTrack` as shipped, contact mode 2)

Summary: n = 118; **e54 mean 0.917 m, median 0.626 m, max 4.300 m; e54 ≤ 0.05 m in 8 samples.** Per-sample table: appendix A (e1, e10, e30, e54, max_k, mean_k, contact steps, and the root-cause phase of the first divergent step).

Flight is exact: 3,242 flight steps, one-step position error 0.0000 m at every step. The 8 free-flight samples of the first stretch agree with GF to 0.0002 m at k = 54. In the second stretch, the other 17 free-flight samples agree to 0.0004 m or better at k = 54; the 18th lands in its last tick (§2.6).

### 2.3 Which term diverges

Attribution is per sample, by the first teacher-forced one-step error above 0.1 mm (each step starts from PES's own state, so there is no error accumulation). The first divergent step is in the phase that holds the term.

| # | Term | Evidence | Samples whose first error is here |
|---|---|---|---|
| T1 | **Landing tick order.** `BuildTrack` tests `y ≤ r` before the flight step, so a flight step that crosses the ground is handled one tick late. PES integrates, then resolves the contact in that same tick: y = r, velocity response, no position step on that tick (the landing record has x, z from the flight step). | Landing one-step error on main: median 0.0029 m, max 0.159 m. With the same-tick landing: max 0.00000 m over 488 landing steps. | 74 of 118 on main |
| T2 | **Lift tick after a landing (second bounce).** After the landing, GF applies a second `Bounce` to the upward ball; `min(−imp, vy)` clamps it to `F·imp`. PES's next tick lifts the ball by a different amount. Undoing one flight step from PES's lift gives an implied landing-out speed of 0.99× GF's (median; p10 0.72, p90 1.55), which is an inference, so the landing response is probably close and the lift is the wrong part. | 326 landings: PES lift median 0.39 m/s, GF 0.39 m/s, but \|Δ\| median 0.19 m/s, max 6.1 m/s. PES lift ≈ landing-out − one flight step for 63 of 326 (within 0.01 m/s). Ruled out: `F·imp` (GF); `F·landing-out`; "flight when moving up" (worsens the 1 s median 0.34 → 0.51 m). **Rule not identified.** | 90 of the 110 divergent samples once T1 is fixed |
| T3 | **Contact mode.** GF's launch value is mode 2 (`kContactModeDryNormal`). The live contact is mode 1: the per-contact-tick horizontal keep is PES/GF = 1.00304 (median) under mode 2 and **1.00000** under mode 1 (K = 0.972 vs 0.969, the `frictionRollRateMax` column). | 2,180 contact steps; the 90th percentile is 1.00317 under mode 2, so the bias is tight. Mode 1 alone does not lower the 1 s median (0.318 → 0.344 m with T1 fixed) because T2 then dominates. | every rolling tick (per-tick bias 0.3 %) |
| T4 | **Spin rebuild on contact.** GF always blends the raw spin fully to `v/r` (`airborne` is passed as true on every contact tick). The live blend is small on rolling ticks: a least-squares fit gives median 0.015, and about 400 of 2,191 steps are exactly 0. | Blend 0, 0.0135, 0.02 or 0.05 on rolling ticks moves the 1 s median between 0.345 and 0.354 m (from 0.344 m) and the mean by at most 0.05 m. A state divergence, not a 1 s error driver. | not a first-error term |

Other residue, unverified: the grounder-kick gate (about 10 % of horizontal steps show a PES/GF ratio up to 1.02 under mode 1; the grounder fires on 33 of 326 lift ticks).

Ablation summary (118 samples; e54 in m):

| GF variant | median | mean | max | ≤ 0.05 m |
|---|---:|---:|---:|---:|
| main as shipped (mode 2, GF order) | 0.626 | 0.917 | 4.300 | 8 |
| + T1 (same-tick landing), mode 2 | 0.318 | 0.663 | 4.159 | 19 |
| + T1 + T3 (mode 1) | 0.344 | 0.715 | 5.587 | 13 |
| + T1 + T3 + rolling blend 0 | 0.345 | 0.686 | 5.507 | 13 |

Not fixed by any of the above: T2. The RE evidence and the ruled-out rules are in §2.4.

### 2.4 Notes for the T2 follow-up

- The lift tick's input speed is the landing-out vertical speed. The implied landing-out speed is inferred, not read; the sim ball's own velocity field (not located, since the record holds position, quaternion and spin only) would settle it.
- Candidate lift rules tested in the probe (`a1_track_probe`, `A1_LIFT=1`, `A1_VARIANT=2`): `F·imp` (GF); `F·landing-out`; flight step from the landing-out velocity. None fits more than 20 % of the 326 events within 0.01 m/s.

### 2.5 Acceptance statement

The ≤ 0.05 m target is **not met** on live play. On the first stretch: median 0.63 m on main, 0.32 m after the same-tick landing fix, and no variant tested is inside 0.05 m for more than 19 of 118 samples. Pooled over both stretches (§2.6): 0.71 m on main, 0.35 m after the fix, 43 of 253 inside 0.05 m at best. The 0.045 m figure quoted in `pes-re-ball-track.md` §3 for the `livepes` diagnostic was measured on the sim's own ball run on the PES tick, not on PES's running game, so it does not test this.

### 2.6 Replication on a second live stretch

A second 240 s capture, from the match Main reported at about 10:52, gives 9,625 verified snapshots and 135 samples. The pipeline is unchanged. Pooled over both stretches (253 samples):

| configuration | match 1 (n=118): median / mean / ≤0.05 | match 2 (n=135): median / mean / ≤0.05 | pooled (n=253): median / mean / max / ≤0.05 |
|---|---|---|---|
| main as shipped (mode 2, GF order) | 0.626 / 0.917 / 8 | 0.751 / 0.888 / 17 | 0.711 / 0.902 / 4.300 / 25 (9.9 %) |
| T1 same-tick landing, mode 2 | 0.318 / 0.663 / 19 | 0.366 / 0.588 / 24 | 0.348 / 0.623 / 4.169 / 43 (17.0 %) |
| T1 + T3 (mode 1) | 0.344 / 0.715 / 13 | 0.437 / 0.702 / 21 | 0.386 / 0.708 / 5.587 / 34 (13.4 %) |

First divergent step per sample (one-step teacher-forced error above 0.1 mm; the phase of that step):

| configuration | landing | lift after landing (T2) | rolling contact (T3/T4) | exact (no error) |
|---|---:|---:|---:|---:|
| main as shipped, pooled | 163 | 25 | 40 | 25 |
| T1 fixed, mode 1, pooled | 0 | 193 | 34 | 26 |

Match 2 has one free-flight sample (f7698) that is not exact. PES lands at record 53; GF's flight step puts the ball below the ground at record 54, so GF resolves the landing one tick late. Its 1 s error is 0.106 m, all of it at record 54. It is T1, not a flight error. The other 17 free-flight samples in match 2 agree with GF to 0.0004 m or better, against 0.0002 m in match 1.

### 2.7 T1 as a proposed port (applied, §2.9)

The landing-order term is in two places with the same structure: `BuildTrack`, `src/sim/onthepitch/pesballtrack.cpp` (lines 24 to 35 on main), and the predictor loop in `src/sim/onthepitch/ball.cpp` (lines 182 to 213). Both test `y ≤ r` before the flight step. The proposed branch order for `BuildTrack` (the predictor needs the same change; its contact branch also advances `contactTicks` only on `tick == 1`):

```cpp
    if (IsResting(c, s)) {
      s.velocity = pes::Vec3(0.0f, 0.0f, 0.0f);
    } else if (s.position.y > r) {
      // Flight. A step that crosses the ground resolves its contact in the same tick: PES's landing record
      // has x, z from the flight step and y = r, with no position step after the response.
      FlightTick(c, &s, kPesTick_s, false);
      if (s.position.y <= r) {
        ContactTick(c, &s, contactTicks, true, false, kPesTick_s);
        ++contactTicks;
      }
    } else {
      // On the ground at the start of the tick: response, then the step on the response velocity.
      ContactTick(c, &s, contactTicks, true, false, kPesTick_s);
      ++contactTicks;
      s.position = s.position + s.velocity * kPesTick_s;
    }
```

Evidence for T1 alone:
- One-step, 488 landing steps: max error 0.00000 m with the same-tick landing, against median 0.0029 m and max 0.159 m on main. Flight steps are unchanged (0.00000 m).
- 1 s live error: median 0.711 → 0.348 m pooled, and within 0.05 m 25 → 43 of 253 (table §2.6).

T1 alone does not meet the ≤ 0.05 m target, and T1 with T3 raises the 1 s median (§2.9). After T1, the first error sits in the lift tick after a landing (T2), which is not identified.

### 2.8 Open items for the owner

- **T2, lift after a landing.** Not identified. Next evidence: the sim ball's velocity field, which is not located in the live object (the record holds position, quaternion and spin only). Candidate rules ruled out are in §2.4.
- **T3, contact mode.** The live contact table matches mode 1 (horizontal keep ratio 1.00000 median) in both stretches. The GF launch value is mode 2 (`kContactModeDryNormal`, documented as dry normal). Which pitch-condition index the live match uses is an owner call; with T2 unfixed, mode 1 slightly raises the 1 s median (§2.6).
- **T4, rolling spin blend.** GF blends fully on every contact tick; PES blends about 0.015 on rolling ticks. No effect on the 1 s error measured here; it does change the spin used for Magnus after a takeoff, which was not measured.
- **Sim table.** Partial (§1). The remaining runs are the seeds listed in §1 for `main` and `base`.

### 2.9 The port (branch `BallContact`)

T1 and T3 are in `pesball::BallTick` (`src/sim/onthepitch/pesballphysics.cpp`). `BuildTrack` and the live predictor in `ball.cpp` both call it, so the live ball and the sampler share one step. The live contact mode is `kContactModeLive = 1` (`pesball.hpp`). T2 and T4 are not ported.

Measured with `a1_port_probe` (scratch, not committed) on the worktree's `BuildTrack`, from each sample's own start state. The main-order, mode 2 row reproduces §2.6's main row exactly.

| configuration | recorded pool (253): median / mean / max / ≤0.05 m | live capture 3 (168): median / mean / max / ≤0.05 m |
|---|---|---|
| main as shipped (landing one tick late, mode 2) | 0.711 / 0.902 / 4.300 / 25 | 0.801 / 0.965 / 5.286 / 17 |
| port, T1 only (mode forced 2) | 0.349 / 0.623 / 4.169 / 43 | 0.357 / 0.637 / 7.025 / 29 |
| port, T1 + T3 (default, mode 1) | 0.386 / 0.708 / 5.587 / 34 | 0.472 / 0.759 / 8.067 / 27 |

One-step, teacher-forced from PES's own state. Landing steps are those where PES is airborne at record k and on the ground at k+1 (1 mm). The port's landing error is 0.000000 m at every landing step (max, 1092 recorded and 872 live). On main it is max 0.167 m, median 0.0030 m, on the same recorded steps. (§2.7 quotes 0.159 m from the probe's own rule.) The one-step max over all steps is 0.155 m recorded and 0.119 m live; those are contact steps, not landings.

Live capture 3: a 240 s read-only `pread` of `PES2021.exe` (`live/capture.py`), 11,306 snapshots, 168 verified samples (`analyze_samples.py`), with the ball in play (speeds 0.9 to 15 m/s in the first samples).

Adding T3 to T1 raises the 1 s median on both sets (0.349 to 0.386 m recorded; 0.357 to 0.472 m live). The ≈0.35 m target is met by T1 only, not by T1 with T3. Whether T2 is what makes T3 cost on the 1 s metric is not tested here.

## 3. Files and reproduction

Scratch root `the scratch dir/` (not committed):
- Sim: `build.sh` (builds, sequential, `-j3`, `nice 19`), `run.sh` (first queue, seed-major over all four trees; killed while `base-s8` was running, which was left to finish; it had queued a seed 20 by mistake, which never ran), `run2.sh` (current queue: `main` and `base` only, seeds 9, 11, 1000, 2, 3, 4 after `base-s8`), `summ_sim.py` (parser and paired metrics), `build_sim_section.py` (§1 generator), `runs/` (gfsim outputs, with `[gfsim]`, `[balance-*]`, `[determinism]` lines).
- Live: `live/capture.py` (read-only sudo `pread` capture: `sudo -n python3 capture.py SECONDS OUT.bin`), `live/cap-main.bin`, `live/cap-match2.bin`, `live/analyze_samples.py` (samples: `python3 analyze_samples.py CAP.bin samples.txt`), `live/analyze1.py` to `analyze6.py` and `survey*.py` (layout checks behind §2.1), `live/samples-main.txt`, `live/samples-match2.txt`.
- Probe: `probe/a1_track_probe.cpp` (compiled against main's `libgfsim.a`, `libpesrig.a`, `libpesfmt.a`, zlib and sqlite, flags from `ninja -t commands`), run as `A1_VARIANT={0,1,2} A1_MODE={1,2} A1_BLEND=x A1_ONESTEP=1 A1_LIFT=1 ./a1_track_probe the PES21 install SAMPLES.txt [ID]`. Variant 0 is `BuildTrack` as shipped; variant 1 is T1; variant 2 is an ablation (flight when moving up on the ground; ruled out). Outputs: `final-*.txt`, `m2-*.txt`, `onestep-*.txt`, `lift-m1.txt`, `live-table*.md`.
- Analysis: `probe/onestep.py` (per-phase one-step residuals), `probe/spinblend.py` (spin blend fit), `probe/attrib.py`, `probe/live_table.py`.

Per-sample tables follow (appendix A: match 1, appendix B: match 2). Columns: sample id, speed at record 0, GF contact steps in the 1 s window, main e1 / e10 / e30 / e54 (m), main max over k and mean over k (m), main root cause (phase of the first one-step error above 0.1 mm), e54 with T1 and mode 1, and that configuration's root cause.

## Appendix A: match 1 (118 samples)

| sample | speed0 m/s | contact steps | main e1 | main e10 | main e30 | main e54 | main max | main mean | main root cause | fix e54 | fix root cause |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---|---:|---|
| f2408607 | 3.11 | 4 | 0.0000 | 0.0126 | 0.1255 | 0.3923 | 0.3923 | 0.1285 | landing | 0.0982 | takeoff |
| f2408667 | 10.39 | 5 | 0.0000 | 0.0554 | 0.4354 | 1.2796 | 1.2796 | 0.4666 | landing | 0.7776 | takeoff |
| f2408727 | 1.59 | 6 | 0.0000 | 0.0682 | 0.3579 | 0.8046 | 0.8046 | 0.3423 | landing | 0.4734 | takeoff |
| f2408787 | 16.36 | 0 | 0.0000 | 0.0000 | 0.0000 | 0.0000 | 0.0000 | 0.0000 | none (exact) | 0.0000 | none (exact) |
| f2408847 | 13.55 | 1 | 0.0000 | 0.0000 | 1.1430 | 2.5438 | 2.5438 | 1.0363 | landing | 2.1284 | takeoff |
| f2408908 | 0.69 | 2 | 0.0000 | 0.0000 | 0.0818 | 0.0774 | 0.1858 | 0.0648 | landing | 0.1714 | takeoff |
| f2408971 | 22.75 | 4 | 0.0000 | 0.1532 | 0.5543 | 1.5966 | 1.5966 | 0.6006 | landing | 0.9414 | takeoff |
| f2409031 | 11.37 | 4 | 0.0000 | 0.1059 | 0.8026 | 1.6841 | 1.6841 | 0.7425 | landing | 1.4759 | takeoff |
| f2409107 | 21.51 | 4 | 0.0258 | 0.2620 | 1.3349 | 3.5712 | 3.5712 | 1.3907 | takeoff | 3.2108 | takeoff |
| f2409173 | 7.86 | 7 | 0.0036 | 0.0260 | 0.3015 | 1.1478 | 1.1478 | 0.3707 | takeoff | 0.1650 | takeoff |
| f2409233 | 3.56 | 14 | 0.0035 | 0.0122 | 0.0970 | 0.2998 | 0.2998 | 0.1028 | contact | 0.0532 | contact |
| f2409293 | 2.93 | 14 | 0.0025 | 0.0081 | 0.0794 | 0.3762 | 0.3762 | 0.1098 | contact | 0.1032 | contact |
| f2409353 | 12.09 | 4 | 0.0000 | 0.0787 | 0.2483 | 1.0257 | 1.0257 | 0.3314 | landing | 2.0115 | takeoff |
| f2409413 | 3.04 | 7 | 0.0000 | 0.0291 | 0.1937 | 0.6282 | 0.6282 | 0.2180 | landing | 0.2450 | takeoff |
| f2409473 | 3.54 | 14 | 0.0015 | 0.0144 | 0.1023 | 0.3018 | 0.3018 | 0.1062 | landing | 0.0164 | contact |
| f2409534 | 3.99 | 14 | 0.0051 | 0.0796 | 0.3111 | 0.6575 | 0.6575 | 0.2929 | takeoff | 0.3785 | takeoff |
| f2409594 | 15.74 | 1 | 0.0000 | 0.0000 | 0.1225 | 0.9226 | 0.9226 | 0.2298 | landing | 0.6692 | takeoff |
| f2409654 | 9.81 | 2 | 0.0000 | 0.0000 | 0.0729 | 0.1785 | 0.1785 | 0.0618 | landing | 0.4659 | takeoff |
| f2409714 | 2.80 | 14 | 0.0021 | 0.0200 | 0.1585 | 0.5362 | 0.5362 | 0.1814 | contact | 0.2540 | contact |
| f2409782 | 0.74 | 14 | 0.0021 | 0.0111 | 0.0797 | 0.2096 | 0.2096 | 0.0811 | contact | 0.0901 | contact |
| f2410371 | 11.94 | 5 | 0.0000 | 0.4009 | 1.6133 | 3.3015 | 3.3015 | 1.5186 | contact | 4.2297 | takeoff |
| f2410431 | 1.44 | 6 | 0.0000 | 0.0181 | 0.1695 | 0.4922 | 0.4922 | 0.1794 | landing | 0.2343 | takeoff |
| f2410493 | 4.15 | 14 | 0.0004 | 0.0135 | 0.1182 | 0.3205 | 0.3205 | 0.1214 | landing | 0.0256 | takeoff |
| f2410553 | 2.04 | 18 | 0.0032 | 0.0202 | 0.1651 | 0.4542 | 0.4542 | 0.1707 | contact | 0.2424 | contact |
| f2410613 | 0.51 | 14 | 0.0019 | 0.0083 | 0.0567 | 0.1476 | 0.1476 | 0.0577 | contact | 0.0503 | contact |
| f2411013 | 26.46 | 0 | 0.0000 | 0.0000 | 0.0001 | 0.0002 | 0.0002 | 0.0001 | none (exact) | 0.0002 | none (exact) |
| f2411073 | 18.23 | 0 | 0.0000 | 0.0000 | 0.0001 | 0.0001 | 0.0001 | 0.0000 | none (exact) | 0.0001 | none (exact) |
| f2411133 | 14.01 | 1 | 0.0000 | 0.0000 | 0.1211 | 1.6954 | 1.6954 | 0.4077 | landing | 2.2045 | takeoff |
| f2411195 | 10.26 | 1 | 0.0000 | 0.0000 | 0.1136 | 0.2213 | 0.3722 | 0.1203 | landing | 0.1873 | takeoff |
| f2411255 | 7.24 | 3 | 0.0000 | 0.0000 | 0.0347 | 0.0845 | 0.1049 | 0.0423 | landing | 0.6671 | takeoff |
| f2411315 | 3.09 | 14 | 0.0035 | 0.0108 | 0.0785 | 0.3496 | 0.3496 | 0.1049 | contact | 0.1128 | contact |
| f2411378 | 4.71 | 11 | 0.0070 | 0.1271 | 0.5278 | 1.0493 | 1.0493 | 0.4859 | takeoff | 0.6542 | takeoff |
| f2411443 | 18.00 | 3 | 0.0000 | 0.0386 | 0.8471 | 2.1260 | 2.1260 | 0.8183 | landing | 1.6589 | takeoff |
| f2411503 | 0.53 | 2 | 0.0000 | 0.0000 | 0.0766 | 0.0618 | 0.1597 | 0.0536 | landing | 0.1738 | takeoff |
| f2411563 | 17.87 | 1 | 0.0000 | 0.0000 | 0.0000 | 0.1621 | 0.1621 | 0.0068 | landing | 0.2121 | takeoff |
| f2411623 | 9.58 | 1 | 0.0000 | 0.0000 | 0.0425 | 0.3235 | 0.3438 | 0.0991 | landing | 0.1237 | takeoff |
| f2411683 | 6.03 | 5 | 0.0000 | 0.0000 | 0.1953 | 0.6957 | 0.6957 | 0.2253 | landing | 0.1099 | takeoff |
| f2411743 | 23.25 | 2 | 0.0000 | 0.0000 | 0.3917 | 1.4170 | 1.4170 | 0.4372 | landing | 0.5590 | takeoff |
| f2411803 | 13.04 | 4 | 0.0000 | 0.1753 | 0.9394 | 2.0378 | 2.0378 | 0.8906 | landing | 2.5490 | takeoff |
| f2411871 | 3.31 | 5 | 0.0012 | 0.1593 | 0.7023 | 1.5936 | 1.5936 | 0.6829 | landing | 1.0998 | takeoff |
| f2411937 | 3.66 | 14 | 0.0039 | 0.0415 | 0.1832 | 0.4466 | 0.4466 | 0.1802 | contact | 0.1978 | contact |
| f2411997 | 4.08 | 14 | 0.0035 | 0.0236 | 0.1425 | 0.3527 | 0.3527 | 0.1416 | takeoff | 0.0702 | takeoff |
| f2412061 | 4.40 | 14 | 0.0051 | 0.0604 | 0.2748 | 0.5742 | 0.5742 | 0.2576 | contact | 0.2040 | takeoff |
| f2412121 | 10.21 | 7 | 0.0415 | 0.4152 | 1.4032 | 2.9630 | 2.9630 | 1.3620 | takeoff | 1.7785 | takeoff |
| f2412181 | 2.34 | 6 | 0.0000 | 0.0273 | 0.1705 | 0.5725 | 0.5725 | 0.1945 | landing | 0.1939 | takeoff |
| f2412241 | 18.09 | 3 | 0.0000 | 0.0388 | 0.3345 | 1.1823 | 1.1823 | 0.3839 | landing | 0.8807 | takeoff |
| f2412307 | 6.62 | 7 | 0.0000 | 0.0491 | 0.3203 | 0.8458 | 0.8458 | 0.3263 | landing | 0.0665 | takeoff |
| f2412367 | 3.05 | 14 | 0.0035 | 0.0109 | 0.0828 | 0.3702 | 0.3702 | 0.1111 | contact | 0.1317 | contact |
| f2412653 | 20.79 | 5 | 0.0000 | 0.0000 | 0.6222 | 2.2359 | 2.2359 | 0.7226 | landing | 1.4702 | takeoff |
| f2412715 | 8.53 | 7 | 0.0000 | 0.0216 | 0.1746 | 0.5390 | 0.5390 | 0.1859 | landing | 0.3894 | takeoff |
| f2412775 | 2.13 | 10 | 0.0000 | 0.0144 | 0.1188 | 0.4575 | 0.4575 | 0.1446 | landing | 0.1070 | takeoff |
| f2412835 | 9.45 | 7 | 0.0000 | 0.0526 | 0.3773 | 0.7959 | 0.7959 | 0.3486 | landing | 0.0793 | takeoff |
| f2412895 | 10.75 | 5 | 0.0000 | 0.0350 | 0.2398 | 0.5981 | 0.5981 | 0.2294 | landing | 0.8602 | takeoff |
| f2412955 | 1.45 | 8 | 0.0000 | 0.0320 | 0.1177 | 0.2978 | 0.2978 | 0.1191 | landing | 0.1878 | takeoff |
| f2413015 | 18.64 | 5 | 0.0000 | 0.0669 | 0.7808 | 2.3021 | 2.3021 | 0.8294 | landing | 5.5874 | takeoff |
| f2413075 | 4.45 | 9 | 0.0141 | 0.1668 | 0.8749 | 1.8762 | 1.8762 | 0.8245 | takeoff | 1.4673 | takeoff |
| f2413135 | 13.83 | 6 | 0.0000 | 0.1286 | 0.4317 | 1.2012 | 1.2012 | 0.4599 | landing | 0.3320 | takeoff |
| f2413197 | 14.20 | 5 | 0.0000 | 0.1151 | 1.2630 | 3.0931 | 3.0931 | 1.2298 | landing | 0.6729 | takeoff |
| f2413257 | 3.21 | 14 | 0.0021 | 0.0098 | 0.0781 | 0.3303 | 0.3303 | 0.1001 | contact | 0.0590 | contact |
| f2413317 | 21.41 | 1 | 0.0000 | 0.0000 | 0.0000 | 0.5545 | 0.5545 | 0.0954 | contact | 0.3147 | takeoff |
| f2413379 | 11.95 | 5 | 0.0000 | 0.0439 | 0.4682 | 1.6232 | 1.6232 | 0.5421 | landing | 0.8292 | takeoff |
| f2413439 | 23.06 | 0 | 0.0000 | 0.0000 | 0.0001 | 0.0002 | 0.0002 | 0.0001 | none (exact) | 0.0002 | none (exact) |
| f2413502 | 13.81 | 3 | 0.0000 | 0.0000 | 0.2134 | 0.8553 | 0.8553 | 0.2686 | landing | 0.2915 | takeoff |
| f2413562 | 4.68 | 9 | 0.0000 | 0.0675 | 0.5603 | 1.2449 | 1.2449 | 0.5224 | landing | 0.7499 | takeoff |
| f2413622 | 14.29 | 0 | 0.0000 | 0.0000 | 0.0000 | 0.0000 | 0.0000 | 0.0000 | none (exact) | 0.0000 | none (exact) |
| f2413682 | 10.32 | 1 | 0.0000 | 0.0000 | 0.0000 | 0.8279 | 0.8279 | 0.0724 | landing | 1.1718 | takeoff |
| f2413742 | 8.02 | 1 | 0.0000 | 0.0000 | 0.0000 | 0.1226 | 0.1226 | 0.0084 | landing | 0.4740 | takeoff |
| f2413804 | 6.16 | 1 | 0.0000 | 0.0000 | 0.0000 | 0.0649 | 0.0649 | 0.0070 | landing | 0.5301 | takeoff |
| f2413864 | 12.83 | 2 | 0.0000 | 0.0000 | 0.5996 | 1.3726 | 1.3726 | 0.5518 | landing | 0.6653 | takeoff |
| f2413924 | 7.49 | 4 | 0.0300 | 0.0783 | 0.2813 | 0.7714 | 0.7714 | 0.3000 | landing | 0.3444 | takeoff |
| f2413984 | 22.41 | 0 | 0.0000 | 0.0000 | 0.0000 | 0.0000 | 0.0000 | 0.0000 | none (exact) | 0.0000 | none (exact) |
| f2414044 | 15.59 | 1 | 0.0000 | 0.0000 | 0.0000 | 1.1824 | 1.1824 | 0.2378 | landing | 1.3908 | takeoff |
| f2414104 | 7.86 | 1 | 0.0000 | 0.0000 | 0.0000 | 0.2082 | 0.2082 | 0.0405 | landing | 0.3921 | takeoff |
| f2414164 | 5.98 | 3 | 0.0000 | 0.0251 | 0.0804 | 0.1347 | 0.1862 | 0.0861 | landing | 0.7078 | takeoff |
| f2414233 | 2.97 | 10 | 0.0000 | 0.0253 | 0.2492 | 0.7371 | 0.7371 | 0.2649 | landing | 0.4407 | takeoff |
| f2414293 | 11.61 | 6 | 0.0000 | 0.0350 | 0.1912 | 0.5524 | 0.5524 | 0.2108 | landing | 0.0652 | takeoff |
| f2414353 | 2.71 | 14 | 0.0037 | 0.0281 | 0.2099 | 0.6233 | 0.6233 | 0.2257 | contact | 0.3627 | contact |
| f2414413 | 16.69 | 4 | 0.0000 | 0.1293 | 0.4873 | 1.5417 | 1.5417 | 0.5442 | landing | 0.5799 | takeoff |
| f2414473 | 12.03 | 3 | 0.0000 | 0.1405 | 1.6532 | 4.2996 | 4.2996 | 1.6379 | landing | 4.5778 | takeoff |
| f2414537 | 15.94 | 7 | 0.0124 | 0.1735 | 0.7957 | 1.8946 | 1.8946 | 0.7927 | takeoff | 2.5893 | takeoff |
| f2414597 | 1.04 | 4 | 0.0000 | 0.0000 | 0.1960 | 0.5814 | 0.5814 | 0.2095 | landing | 0.4697 | takeoff |
| f2414657 | 2.48 | 11 | 0.0035 | 0.0143 | 0.1869 | 0.6034 | 0.6034 | 0.2069 | contact | 0.2741 | contact |
| f2414717 | 12.12 | 5 | 0.0000 | 0.0332 | 0.6779 | 1.6755 | 1.6755 | 0.6577 | landing | 0.1033 | takeoff |
| f2414777 | 3.15 | 14 | 0.0028 | 0.0400 | 0.1941 | 0.5815 | 0.5815 | 0.2129 | takeoff | 0.3459 | takeoff |
| f2414837 | 5.35 | 11 | 0.0000 | 0.0790 | 0.4815 | 1.0267 | 1.0267 | 0.4498 | landing | 0.5218 | takeoff |
| f2414897 | 21.25 | 2 | 0.0000 | 0.0000 | 0.2263 | 0.9908 | 0.9908 | 0.2900 | landing | 0.2968 | takeoff |
| f2414958 | 2.07 | 18 | 0.0032 | 0.0203 | 0.1664 | 0.4583 | 0.4583 | 0.1720 | contact | 0.2449 | contact |
| f2415018 | 0.52 | 14 | 0.0019 | 0.0084 | 0.0575 | 0.1497 | 0.1497 | 0.0585 | contact | 0.0510 | contact |
| f2416414 | 13.70 | 6 | 0.0000 | 0.2955 | 1.5048 | 3.3147 | 3.3147 | 1.4337 | landing | 4.8158 | takeoff |
| f2416474 | 5.02 | 10 | 0.0000 | 0.0093 | 0.1046 | 0.3786 | 0.3786 | 0.1236 | landing | 0.0651 | takeoff |
| f2416535 | 13.38 | 8 | 0.0117 | 0.1667 | 0.7869 | 1.9162 | 1.9162 | 0.7885 | takeoff | 2.0184 | takeoff |
| f2416595 | 1.58 | 18 | 0.0028 | 0.0173 | 0.1359 | 0.3629 | 0.3629 | 0.1387 | contact | 0.1900 | contact |
| f2416655 | 3.76 | 14 | 0.0011 | 0.0118 | 0.1035 | 0.2966 | 0.2966 | 0.1064 | landing | 0.0073 | takeoff |
| f2416715 | 4.53 | 11 | 0.0048 | 0.0787 | 0.3396 | 0.7243 | 0.7243 | 0.3220 | takeoff | 0.3194 | takeoff |
| f2416777 | 3.28 | 14 | 0.0019 | 0.0100 | 0.0800 | 0.3158 | 0.3158 | 0.0975 | contact | 0.0440 | contact |
| f2416837 | 15.03 | 2 | 0.0000 | 0.0448 | 0.8621 | 1.8084 | 1.8084 | 0.7764 | landing | 0.7052 | takeoff |
| f2416899 | 19.01 | 3 | 0.0000 | 0.0000 | 0.3691 | 1.4989 | 1.4989 | 0.4587 | landing | 0.7813 | takeoff |
| f2416959 | 9.45 | 6 | 0.0163 | 0.1633 | 0.7068 | 1.9986 | 1.9986 | 0.7619 | takeoff | 0.8375 | takeoff |
| f2417019 | 15.00 | 3 | 0.0000 | 0.2768 | 0.9651 | 2.0541 | 2.0541 | 0.9142 | landing | 1.1733 | takeoff |
| f2417088 | 12.95 | 3 | 0.0000 | 0.0000 | 0.2382 | 1.0004 | 1.0004 | 0.3021 | landing | 2.3996 | takeoff |
| f2417148 | 1.01 | 10 | 0.0000 | 0.0036 | 0.0510 | 0.2157 | 0.2157 | 0.0667 | landing | 0.0175 | takeoff |
| f2417208 | 20.71 | 0 | 0.0000 | 0.0000 | 0.0000 | 0.0001 | 0.0001 | 0.0000 | none (exact) | 0.0001 | none (exact) |
| f2417271 | 14.04 | 1 | 0.0000 | 0.0000 | 0.0000 | 0.7605 | 0.7605 | 0.0862 | landing | 1.2243 | takeoff |
| f2417331 | 9.11 | 1 | 0.0000 | 0.0260 | 0.5188 | 0.9560 | 1.0121 | 0.4665 | landing | 0.5684 | takeoff |
| f2417391 | 4.97 | 2 | 0.0000 | 0.0000 | 0.0425 | 0.0807 | 0.1110 | 0.0354 | contact | 0.3443 | takeoff |
| f2417459 | 3.98 | 8 | 0.0000 | 0.0075 | 0.0705 | 0.3023 | 0.3023 | 0.0943 | landing | 0.2020 | takeoff |
| f2417519 | 2.20 | 18 | 0.0033 | 0.0210 | 0.1733 | 0.4814 | 0.4814 | 0.1798 | contact | 0.2588 | contact |
| f2417835 | 21.43 | 0 | 0.0000 | 0.0000 | 0.0000 | 0.0001 | 0.0001 | 0.0000 | none (exact) | 0.0001 | none (exact) |
| f2417897 | 13.98 | 1 | 0.0000 | 0.0000 | 0.0001 | 1.1070 | 1.1070 | 0.1896 | landing | 1.4025 | takeoff |
| f2417959 | 7.71 | 1 | 0.0000 | 0.0604 | 0.8803 | 1.8850 | 1.8850 | 0.7991 | landing | 1.1260 | takeoff |
| f2418020 | 3.64 | 2 | 0.0000 | 0.0000 | 0.0187 | 0.0505 | 0.1351 | 0.0356 | landing | 0.1672 | takeoff |
| f2418080 | 19.33 | 1 | 0.0000 | 0.0000 | 0.0406 | 0.9985 | 0.9985 | 0.2363 | landing | 0.6397 | takeoff |
| f2418143 | 10.43 | 6 | 0.0000 | 0.1018 | 0.4225 | 1.0992 | 1.0992 | 0.4370 | landing | 0.3197 | takeoff |
| f2418203 | 5.43 | 7 | 0.0000 | 0.0564 | 0.4889 | 1.1791 | 1.1791 | 0.4735 | landing | 0.5471 | takeoff |
| f2418277 | 6.61 | 8 | 0.0054 | 0.1465 | 0.6532 | 1.3852 | 1.3852 | 0.6187 | landing | 0.9480 | takeoff |
| f2418339 | 0.87 | 12 | 0.0023 | 0.0127 | 0.0980 | 0.2653 | 0.2653 | 0.1009 | contact | 0.1070 | contact |
| f2418408 | 4.45 | 11 | 0.0069 | 0.1275 | 0.4988 | 1.0106 | 1.0106 | 0.4634 | takeoff | 0.6149 | takeoff |
| f2418476 | 1.09 | 18 | 0.0024 | 0.0133 | 0.0993 | 0.2580 | 0.2580 | 0.1003 | contact | 0.1329 | contact |

main: n=118 e54 mean 0.9168 median 0.6258 max 4.2996 <=0.05: 8
fix : n=118 e54 mean 0.7146 median 0.3443 max 5.5874 <=0.05: 13

## Appendix B: match 2 (135 samples)

| sample | speed0 m/s | contact steps | main e1 | main e10 | main e30 | main e54 | main max | main mean | main root cause | fix e54 | fix root cause |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---|---:|---|
| f3477 | 7.37 | 9 | 0.0000 | 0.0158 | 0.1241 | 0.3022 | 0.3022 | 0.1190 | landing | 1.1893 | takeoff |
| f3537 | 5.15 | 5 | 0.0000 | 0.0033 | 0.0533 | 0.2683 | 0.2683 | 0.0757 | landing | 0.3234 | takeoff |
| f3600 | 5.87 | 2 | 0.0000 | 0.0000 | 0.1164 | 0.4512 | 0.4512 | 0.1359 | landing | 0.1099 | takeoff |
| f3662 | 2.55 | 4 | 0.0000 | 0.0765 | 0.2669 | 0.5872 | 0.5872 | 0.2570 | landing | 0.3853 | takeoff |
| f5386 | 17.66 | 5 | 0.0000 | 0.3601 | 1.8587 | 3.5650 | 3.5650 | 1.6665 | contact | 5.2306 | takeoff |
| f5446 | 8.04 | 8 | 0.0000 | 0.0449 | 0.4069 | 1.0237 | 1.0237 | 0.4000 | landing | 1.6797 | takeoff |
| f5506 | 18.28 | 6 | 0.0000 | 0.0555 | 0.6870 | 1.9596 | 1.9596 | 0.7228 | landing | 0.2703 | takeoff |
| f5566 | 7.76 | 7 | 0.0047 | 0.0260 | 0.3410 | 0.8056 | 0.8056 | 0.3198 | landing | 0.4367 | takeoff |
| f5626 | 4.48 | 11 | 0.0048 | 0.0780 | 0.3355 | 0.7105 | 0.7105 | 0.3173 | takeoff | 0.3233 | takeoff |
| f5686 | 12.72 | 5 | 0.0000 | 0.0212 | 0.6102 | 1.9959 | 1.9959 | 0.6826 | landing | 0.7149 | takeoff |
| f5746 | 2.98 | 14 | 0.0035 | 0.0104 | 0.0873 | 0.3877 | 0.3877 | 0.1166 | contact | 0.1357 | contact |
| f5806 | 3.11 | 14 | 0.0035 | 0.0114 | 0.0804 | 0.3524 | 0.3524 | 0.1065 | contact | 0.1093 | contact |
| f5866 | 1.73 | 18 | 0.0029 | 0.0184 | 0.1463 | 0.3943 | 0.3943 | 0.1499 | contact | 0.2077 | contact |
| f5928 | 8.38 | 8 | 0.0000 | 0.0349 | 0.2680 | 0.7535 | 0.7535 | 0.2824 | landing | 0.0324 | takeoff |
| f5988 | 10.69 | 4 | 0.0000 | 0.1279 | 0.8114 | 1.7679 | 1.7679 | 0.7612 | landing | 0.6836 | takeoff |
| f6048 | 1.27 | 18 | 0.0026 | 0.0149 | 0.1136 | 0.2982 | 0.2982 | 0.1152 | contact | 0.1545 | contact |
| f6108 | 20.93 | 0 | 0.0000 | 0.0000 | 0.0000 | 0.0001 | 0.0001 | 0.0000 | none (exact) | 0.0001 | none (exact) |
| f6168 | 12.66 | 0 | 0.0000 | 0.0000 | 0.0000 | 0.0001 | 0.0001 | 0.0000 | none (exact) | 0.0001 | none (exact) |
| f6228 | 9.58 | 1 | 0.0000 | 0.0000 | 0.3138 | 0.9676 | 0.9676 | 0.3278 | landing | 1.4801 | takeoff |
| f6288 | 11.83 | 1 | 0.0000 | 0.0000 | 0.0000 | 0.0824 | 0.1073 | 0.0066 | landing | 0.3380 | takeoff |
| f6348 | 8.23 | 1 | 0.0000 | 0.0000 | 0.0000 | 0.2436 | 0.2436 | 0.0528 | landing | 0.2912 | takeoff |
| f6409 | 22.06 | 3 | 0.0000 | 0.0899 | 0.7714 | 2.2818 | 2.2818 | 0.8205 | landing | 0.7620 | takeoff |
| f6469 | 18.21 | 6 | 0.0000 | 0.1243 | 0.8133 | 2.1543 | 2.1543 | 0.8314 | landing | 1.2458 | takeoff |
| f6529 | 7.30 | 6 | 0.0000 | 0.0471 | 0.2422 | 1.0333 | 1.0333 | 0.3248 | landing | 1.5152 | takeoff |
| f6589 | 11.82 | 7 | 0.0000 | 0.0962 | 0.6686 | 1.5727 | 1.5727 | 0.6432 | landing | 0.4769 | takeoff |
| f6649 | 5.47 | 11 | 0.0066 | 0.1061 | 0.4765 | 0.9938 | 0.9938 | 0.4471 | takeoff | 0.5680 | takeoff |
| f6709 | 3.07 | 14 | 0.0023 | 0.0108 | 0.0833 | 0.3670 | 0.3670 | 0.1105 | contact | 0.0934 | contact |
| f6772 | 2.52 | 3 | 0.0000 | 0.0000 | 0.1834 | 0.6835 | 0.6835 | 0.2169 | landing | 0.4662 | takeoff |
| f6832 | 17.33 | 4 | 0.0000 | 0.1616 | 0.6379 | 2.0575 | 2.0575 | 0.7237 | landing | 0.5719 | takeoff |
| f6892 | 4.86 | 5 | 0.0135 | 0.1329 | 0.4615 | 0.9956 | 0.9956 | 0.4473 | takeoff | 0.6845 | takeoff |
| f6952 | 12.89 | 4 | 0.0195 | 0.1430 | 0.6037 | 1.7629 | 1.7629 | 0.6611 | landing | 0.4636 | takeoff |
| f7012 | 21.44 | 4 | 0.0000 | 0.0000 | 0.5694 | 2.0980 | 2.0980 | 0.6800 | landing | 1.1878 | takeoff |
| f7072 | 10.17 | 7 | 0.0062 | 0.0899 | 0.5349 | 1.2700 | 1.2700 | 0.5194 | takeoff | 0.0772 | takeoff |
| f7132 | 22.48 | 2 | 0.0000 | 0.0000 | 0.2794 | 1.1426 | 1.1426 | 0.3400 | landing | 0.5732 | takeoff |
| f7192 | 12.52 | 3 | 0.0000 | 0.0000 | 0.2527 | 1.1076 | 1.1076 | 0.3286 | landing | 0.2019 | takeoff |
| f7253 | 17.99 | 3 | 0.0000 | 0.0000 | 0.2869 | 1.1606 | 1.1606 | 0.3584 | landing | 2.2888 | takeoff |
| f7313 | 6.77 | 7 | 0.0000 | 0.0411 | 0.5367 | 1.4331 | 1.4331 | 0.5401 | landing | 1.1190 | takeoff |
| f7373 | 16.47 | 2 | 0.0000 | 0.1388 | 0.4751 | 1.1675 | 1.1675 | 0.5140 | landing | 1.4199 | takeoff |
| f7638 | 2.87 | 1 | 0.0000 | 0.0000 | 0.0000 | 0.3131 | 0.3131 | 0.0444 | landing | 0.6225 | takeoff |
| f7698 | 21.42 | 0 | 0.0000 | 0.0000 | 0.0000 | 0.1060 | 0.1060 | 0.0020 | contact | 0.0001 | none (exact) |
| f7758 | 3.73 | 3 | 0.0000 | 0.0000 | 0.1092 | 0.3862 | 0.3862 | 0.1177 | landing | 0.2335 | takeoff |
| f7818 | 20.96 | 1 | 0.0000 | 0.0000 | 0.0000 | 0.1192 | 0.1192 | 0.0056 | landing | 0.1403 | takeoff |
| f7878 | 12.21 | 2 | 0.0000 | 0.0000 | 0.1759 | 0.5925 | 0.5925 | 0.1840 | landing | 2.0410 | takeoff |
| f7938 | 0.73 | 11 | 0.0060 | 0.0051 | 0.0668 | 0.1966 | 0.1966 | 0.0717 | landing | 0.0505 | takeoff |
| f7998 | 11.53 | 6 | 0.0000 | 0.0090 | 0.3059 | 1.0952 | 1.0952 | 0.3584 | landing | 0.3282 | takeoff |
| f8058 | 15.50 | 6 | 0.0000 | 0.0532 | 0.5608 | 1.5578 | 1.5578 | 0.5734 | landing | 2.6796 | takeoff |
| f8118 | 3.51 | 9 | 0.0097 | 0.0112 | 0.1056 | 0.3608 | 0.3608 | 0.1178 | contact | 0.0465 | takeoff |
| f8178 | 19.22 | 0 | 0.0000 | 0.0000 | 0.0001 | 0.0001 | 0.0001 | 0.0001 | none (exact) | 0.0001 | none (exact) |
| f8238 | 12.20 | 0 | 0.0000 | 0.0000 | 0.0000 | 0.0001 | 0.0001 | 0.0000 | none (exact) | 0.0001 | none (exact) |
| f8298 | 5.55 | 1 | 0.0000 | 0.0000 | 0.0001 | 0.1224 | 0.1360 | 0.0105 | landing | 0.5318 | takeoff |
| f8358 | 5.10 | 1 | 0.0000 | 0.0000 | 0.0001 | 0.0686 | 0.0686 | 0.0069 | landing | 0.4641 | takeoff |
| f8418 | 10.53 | 2 | 0.0000 | 0.1850 | 0.6346 | 1.2101 | 1.2101 | 0.5673 | landing | 0.4529 | takeoff |
| f8478 | 3.06 | 6 | 0.0076 | 0.0759 | 0.2708 | 0.6693 | 0.6693 | 0.2736 | takeoff | 0.3969 | takeoff |
| f8538 | 10.13 | 5 | 0.0000 | 0.0585 | 0.5609 | 1.5190 | 1.5190 | 0.5803 | landing | 0.8589 | takeoff |
| f8598 | 2.59 | 14 | 0.0030 | 0.0066 | 0.1336 | 0.4930 | 0.4930 | 0.1581 | contact | 0.1985 | contact |
| f8658 | 11.08 | 7 | 0.0099 | 0.1464 | 0.7887 | 1.8732 | 1.8732 | 0.7793 | takeoff | 1.0835 | takeoff |
| f8718 | 3.08 | 5 | 0.0000 | 0.0545 | 0.3011 | 0.8382 | 0.8382 | 0.3160 | landing | 0.4361 | takeoff |
| f8778 | 16.26 | 2 | 0.0000 | 0.0374 | 0.7161 | 1.6941 | 1.6941 | 0.6829 | landing | 0.5010 | takeoff |
| f8838 | 7.93 | 7 | 0.0000 | 0.0483 | 0.6593 | 1.6256 | 1.6256 | 0.6380 | landing | 0.3782 | takeoff |
| f8898 | 17.03 | 1 | 0.0000 | 0.0000 | 0.2157 | 1.1522 | 1.1522 | 0.3291 | landing | 0.6276 | takeoff |
| f8958 | 9.12 | 3 | 0.0000 | 0.0000 | 0.1362 | 0.5439 | 0.5439 | 0.1667 | landing | 0.2696 | takeoff |
| f9018 | 25.50 | 0 | 0.0000 | 0.0000 | 0.0000 | 0.0001 | 0.0001 | 0.0000 | none (exact) | 0.0001 | none (exact) |
| f9078 | 17.04 | 0 | 0.0000 | 0.0000 | 0.0000 | 0.0000 | 0.0000 | 0.0000 | none (exact) | 0.0000 | none (exact) |
| f9138 | 13.81 | 1 | 0.0000 | 0.0000 | 0.6037 | 2.1931 | 2.1931 | 0.7005 | landing | 2.2256 | takeoff |
| f9198 | 9.16 | 1 | 0.0000 | 0.0000 | 0.4490 | 1.0211 | 1.0211 | 0.4122 | landing | 0.8405 | takeoff |
| f9258 | 2.20 | 1 | 0.0000 | 0.0000 | 0.2049 | 0.7513 | 0.7513 | 0.2379 | landing | 0.6226 | takeoff |
| f9318 | 18.47 | 5 | 0.0000 | 0.0572 | 0.7115 | 1.8414 | 1.8414 | 0.7269 | landing | 4.9323 | takeoff |
| f9378 | 8.05 | 6 | 0.0041 | 0.1054 | 0.8425 | 2.0994 | 2.0994 | 0.8396 | landing | 0.2594 | takeoff |
| f9438 | 2.33 | 11 | 0.0034 | 0.0257 | 0.2222 | 0.6446 | 0.6446 | 0.2351 | contact | 0.2898 | contact |
| f9498 | 12.66 | 4 | 0.0000 | 0.0271 | 0.7262 | 2.3211 | 2.3211 | 0.7885 | landing | 1.2383 | takeoff |
| f9558 | 22.04 | 3 | 0.0000 | 0.0000 | 0.4251 | 1.6093 | 1.6093 | 0.4880 | landing | 1.0723 | takeoff |
| f9618 | 4.03 | 1 | 0.0000 | 0.0000 | 0.6582 | 1.8340 | 1.8340 | 0.6530 | landing | 1.4744 | takeoff |
| f9678 | 13.95 | 4 | 0.0000 | 0.0313 | 0.2502 | 1.0061 | 1.0061 | 0.3125 | landing | 0.1995 | takeoff |
| f9738 | 9.42 | 7 | 0.0132 | 0.0826 | 0.4754 | 1.4781 | 1.4781 | 0.5380 | takeoff | 1.9224 | takeoff |
| f9798 | 15.89 | 5 | 0.0169 | 0.2069 | 0.8594 | 2.0586 | 2.0586 | 0.8589 | takeoff | 1.9004 | takeoff |
| f9858 | 5.10 | 6 | 0.0000 | 0.0000 | 0.1719 | 0.6171 | 0.6171 | 0.2005 | landing | 0.1395 | takeoff |
| f9918 | 15.45 | 4 | 0.0000 | 0.0825 | 0.7095 | 2.0774 | 2.0774 | 0.7471 | landing | 0.4874 | takeoff |
| f9978 | 3.96 | 14 | 0.0007 | 0.0583 | 0.2606 | 0.5675 | 0.5675 | 0.2455 | landing | 0.2512 | takeoff |
| f10038 | 7.87 | 5 | 0.0000 | 0.0260 | 0.4334 | 1.5019 | 1.5019 | 0.5056 | landing | 0.6684 | takeoff |
| f10098 | 3.11 | 14 | 0.0035 | 0.0114 | 0.0816 | 0.3554 | 0.3554 | 0.1076 | contact | 0.1125 | contact |
| f10158 | 15.02 | 3 | 0.0000 | 0.0895 | 0.6489 | 1.6397 | 1.6397 | 0.6534 | landing | 0.5163 | takeoff |
| f10218 | 15.46 | 5 | 0.0000 | 0.0808 | 0.3697 | 1.0252 | 1.0252 | 0.3913 | landing | 0.3503 | takeoff |
| f10278 | 3.93 | 5 | 0.0000 | 0.0751 | 0.4534 | 1.1874 | 1.1874 | 0.4625 | landing | 0.6999 | takeoff |
| f10338 | 9.31 | 7 | 0.0000 | 0.0674 | 0.5459 | 1.5522 | 1.5522 | 0.5798 | landing | 3.1353 | takeoff |
| f10398 | 18.91 | 2 | 0.0000 | 0.0000 | 0.0779 | 0.7747 | 0.7747 | 0.1865 | landing | 1.1033 | takeoff |
| f10458 | 3.56 | 4 | 0.0000 | 0.0000 | 0.2084 | 0.7191 | 0.7191 | 0.2345 | landing | 0.3722 | takeoff |
| f10518 | 2.28 | 5 | 0.0000 | 0.0155 | 0.1707 | 0.4692 | 0.4692 | 0.1760 | landing | 0.3864 | takeoff |
| f10578 | 21.00 | 3 | 0.0000 | 0.1654 | 0.8386 | 1.8329 | 1.8329 | 0.7849 | landing | 0.6359 | takeoff |
| f10638 | 5.58 | 10 | 0.0000 | 0.0361 | 0.2978 | 0.7345 | 0.7345 | 0.2926 | landing | 0.2779 | takeoff |
| f10698 | 2.65 | 14 | 0.0035 | 0.0086 | 0.1357 | 0.4981 | 0.4981 | 0.1605 | contact | 0.2413 | contact |
| f10758 | 0.84 | 12 | 0.0022 | 0.0123 | 0.0947 | 0.2558 | 0.2558 | 0.0974 | contact | 0.1031 | contact |
| f11114 | 26.01 | 0 | 0.0000 | 0.0000 | 0.0001 | 0.0002 | 0.0002 | 0.0001 | none (exact) | 0.0002 | none (exact) |
| f11174 | 17.36 | 0 | 0.0000 | 0.0000 | 0.0000 | 0.0001 | 0.0001 | 0.0000 | none (exact) | 0.0001 | none (exact) |
| f11234 | 13.72 | 1 | 0.0000 | 0.0000 | 0.5278 | 2.1082 | 2.1082 | 0.6539 | landing | 2.2649 | takeoff |
| f11294 | 7.80 | 1 | 0.0000 | 0.0000 | 0.0613 | 0.2880 | 0.2880 | 0.0667 | landing | 0.3052 | takeoff |
| f11354 | 6.28 | 3 | 0.0036 | 0.0486 | 0.0984 | 0.2057 | 0.2143 | 0.1155 | landing | 0.7861 | takeoff |
| f11414 | 4.36 | 14 | 0.0035 | 0.0130 | 0.0137 | 0.1506 | 0.1506 | 0.0411 | takeoff | 0.1678 | takeoff |
| f11474 | 4.16 | 11 | 0.0061 | 0.0860 | 0.4194 | 0.8624 | 0.8624 | 0.3876 | takeoff | 0.5789 | takeoff |
| f11534 | 10.59 | 6 | 0.0000 | 0.0213 | 0.2411 | 0.7551 | 0.7551 | 0.2622 | landing | 0.0427 | takeoff |
| f11594 | 27.72 | 0 | 0.0000 | 0.0000 | 0.0000 | 0.0002 | 0.0002 | 0.0001 | none (exact) | 0.0002 | none (exact) |
| f11654 | 20.35 | 0 | 0.0000 | 0.0000 | 0.0000 | 0.0000 | 0.0000 | 0.0000 | none (exact) | 0.0000 | none (exact) |
| f11714 | 15.10 | 1 | 0.0000 | 0.0000 | 0.0745 | 1.6317 | 1.6317 | 0.3922 | landing | 1.9138 | takeoff |
| f11774 | 7.11 | 1 | 0.0000 | 0.0000 | 0.0091 | 0.2424 | 0.2424 | 0.0569 | landing | 0.3955 | takeoff |
| f11834 | 10.52 | 3 | 0.0000 | 0.0229 | 0.4808 | 1.3689 | 1.3689 | 0.5012 | landing | 1.3281 | takeoff |
| f11894 | 3.28 | 14 | 0.0042 | 0.0506 | 0.2098 | 0.5897 | 0.5897 | 0.2251 | contact | 0.3563 | contact |
| f11954 | 23.01 | 1 | 0.0000 | 0.0000 | 0.3939 | 1.5386 | 1.5386 | 0.4907 | landing | 1.2552 | takeoff |
| f12014 | 17.65 | 0 | 0.0000 | 0.0000 | 0.0001 | 0.0003 | 0.0003 | 0.0001 | none (exact) | 0.0003 | none (exact) |
| f12074 | 4.73 | 2 | 0.0000 | 0.0000 | 0.1324 | 0.2815 | 0.3138 | 0.1242 | landing | 0.2697 | takeoff |
| f12134 | 12.09 | 0 | 0.0000 | 0.0000 | 0.0001 | 0.0001 | 0.0001 | 0.0001 | none (exact) | 0.0001 | none (exact) |
| f12194 | 10.55 | 1 | 0.0000 | 0.0000 | 0.0001 | 0.3971 | 0.3971 | 0.0249 | landing | 0.8338 | takeoff |
| f12254 | 7.48 | 0 | 0.0000 | 0.0000 | 0.0001 | 0.0002 | 0.0002 | 0.0001 | none (exact) | 0.0002 | none (exact) |
| f12314 | 5.67 | 1 | 0.0000 | 0.0000 | 0.1320 | 0.4363 | 0.4363 | 0.1457 | landing | 1.6834 | takeoff |
| f12374 | 2.88 | 1 | 0.0000 | 0.0000 | 0.0495 | 0.2871 | 0.2871 | 0.0810 | landing | 0.7599 | takeoff |
| f12599 | 12.11 | 0 | 0.0000 | 0.0000 | 0.0001 | 0.0001 | 0.0001 | 0.0001 | none (exact) | 0.0001 | none (exact) |
| f12659 | 5.05 | 1 | 0.0000 | 0.0219 | 0.4378 | 0.8880 | 0.8880 | 0.3907 | landing | 0.4591 | takeoff |
| f12719 | 3.88 | 6 | 0.0000 | 0.0681 | 0.4476 | 1.2109 | 1.2109 | 0.4626 | landing | 0.6994 | takeoff |
| f12779 | 18.25 | 0 | 0.0000 | 0.0001 | 0.0002 | 0.0004 | 0.0004 | 0.0002 | none (exact) | 0.0004 | none (exact) |
| f12839 | 12.11 | 1 | 0.0000 | 0.0000 | 0.0229 | 0.5810 | 0.5810 | 0.1379 | landing | 0.8232 | takeoff |
| f12899 | 5.90 | 2 | 0.0000 | 0.0000 | 0.3967 | 1.0466 | 1.0466 | 0.3794 | landing | 0.5039 | takeoff |
| f12959 | 1.46 | 5 | 0.0000 | 0.0589 | 0.2114 | 0.4513 | 0.4513 | 0.2032 | landing | 0.3174 | takeoff |
| f13296 | 25.93 | 0 | 0.0000 | 0.0000 | 0.0001 | 0.0004 | 0.0004 | 0.0002 | none (exact) | 0.0004 | none (exact) |
| f13356 | 17.28 | 0 | 0.0000 | 0.0000 | 0.0000 | 0.0001 | 0.0001 | 0.0000 | none (exact) | 0.0001 | none (exact) |
| f13416 | 13.71 | 1 | 0.0000 | 0.0000 | 0.6002 | 2.1683 | 2.1683 | 0.6935 | landing | 2.2802 | takeoff |
| f13476 | 7.00 | 1 | 0.0000 | 0.0000 | 0.0579 | 0.3362 | 0.3362 | 0.0931 | landing | 0.2278 | takeoff |
| f13536 | 13.00 | 3 | 0.0000 | 0.0000 | 0.1608 | 0.8520 | 0.8520 | 0.2232 | landing | 0.2565 | takeoff |
| f13596 | 3.24 | 5 | 0.0000 | 0.1218 | 0.6862 | 1.6416 | 1.6416 | 0.6736 | landing | 1.2047 | takeoff |
| f13656 | 11.52 | 4 | 0.0000 | 0.0138 | 0.3344 | 1.2147 | 1.2147 | 0.3890 | landing | 1.6868 | takeoff |
| f13716 | 2.71 | 14 | 0.0028 | 0.0073 | 0.0998 | 0.4296 | 0.4296 | 0.1295 | contact | 0.1406 | contact |
| f13776 | 12.02 | 5 | 0.0080 | 0.1086 | 0.6580 | 1.6346 | 1.6346 | 0.6594 | takeoff | 3.0796 | takeoff |
| f13836 | 2.67 | 14 | 0.0035 | 0.0084 | 0.1212 | 0.4707 | 0.4707 | 0.1480 | contact | 0.2126 | contact |
| f13896 | 0.87 | 12 | 0.0023 | 0.0126 | 0.0977 | 0.2645 | 0.2645 | 0.1006 | contact | 0.1067 | contact |
| f13956 | 4.88 | 7 | 0.0000 | 0.0479 | 0.4376 | 1.0310 | 1.0310 | 0.4201 | landing | 0.5160 | takeoff |
| f14016 | 13.97 | 5 | 0.0062 | 0.0825 | 0.7346 | 1.9319 | 1.9319 | 0.7373 | takeoff | 0.5317 | takeoff |
| f14076 | 2.47 | 6 | 0.0157 | 0.1078 | 0.3315 | 0.4644 | 0.4657 | 0.2834 | landing | 0.9568 | takeoff |
| f14136 | 13.48 | 7 | 0.0000 | 0.0969 | 0.3592 | 0.7515 | 0.7515 | 0.3366 | landing | 1.2721 | takeoff |

main: n=135 e54 mean 0.8883 median 0.7513 max 3.5650 <=0.05: 17
fix : n=135 e54 mean 0.7024 median 0.4367 max 5.2306 <=0.05: 21
