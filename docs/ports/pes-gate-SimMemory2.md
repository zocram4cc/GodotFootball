# SimMemory2: PES clip keys released after prepare (RSS 2.3 GiB to 0.3 GiB per sim)

Branch `SimMemory2`, commit `812e0da5ee6dea3709a9c4d4f930d0237f7fb764` (on top of origin/main `784ffd5d`). `git ls-remote origin SimMemory2` returns the same SHA.

## Change (CONFIRMED by build, tests, smoke hashes)

- `Animation` (src/sim/utils/animation.hpp/.cpp): `ReleaseKeys()` caches every derived value (translation, movements, velocities, angles, body directions) from the keys, then drops the keys. `SetKeyReloader()` gives the clip a rebuild function. `EnsureKeys()` rebuilds the keys before any key read: `GetKeyFrame`, `Apply`, `SetKeyFrame`, `DeleteKeyFrame`, `Shift`, `Invert`, `Save`, `ConvertToStartFacingForwardIfIdle`, `Mirror`, the copy constructor, and inside each cache getter's dirty branch. A clean cache never touches the keys.
- `AnimCollection::AddPesClips` (animcollection.cpp): every clip is prepared exactly as before (same order of `_PrepareAnim`, same variables, same `animations` indices). Then both sides get a reloader and `ReleaseKeys()`. The reloader parses `PesClipAnimText(clip, rig)` into a temporary `Animation`, mirrors it for the mirrored side, and swaps its key lists into the released nodes (same node order). It touches no variables and does not rerun `_PrepareAnim`, so the shared `playerNode` state is never replayed.
- `Match::GetAnimPositionCache` (match.hpp): built on first request, not for every animation in the constructor (match.cpp loop removed). A clip that is never played never gets its keys back.
- The clip set outlives the collection: `MatchSetup::pesClips` is already required to live for the match (gf_match.cpp: the library's `pesClips_`; gfsim_run: a local). The reloader holds a reference to it.

## Measured (CONFIRMED)

Probe (scratch only, reverted, patch kept at `<workdrive>/gf-SimMemory2-scratch/probe.patch`), seed 7, 2 min, switch-off, before the change:

| item | value |
|---|---|
| PES clips / sides (as authored + mirrored) | 4,419 / 8,838 |
| keys per side (sum over clips) | 18,413,787 (x2 sides) |
| FootballAnimationExtension keys, PES clips (both sides) | 0 (the extension maps are empty for PES clips; not an owner) |
| decoded GANI (`pes::Gani`, all clips) | 153,655,432 B (146.5 MiB), 66,285 units |
| heap in use: after LoadPesClips / after AddPesClips | 187 MiB / 2,243 MiB (AddPesClips +2,027 MiB) |
| eager position cache (match.cpp loop) | +40 MiB |

Key size: 56 B per key (heap), so keys are about 1.9 GiB of the 2.26 GiB sim. The earlier 36 B estimate was low.

Sampler (`/proc/<pid>/status`, 0.5 s, `simslot`), seed 7, 2 min:

| mode | before, VmRSS peak | after, VmRSS peak | after, end |
|---|---|---|---|
| switch-off | 2,366,052 kB (2.26 GiB) | 312,616 kB (305 MiB) | 301,120 kB (294 MiB) |
| pes-loop | 2,382,420 kB (2.27 GiB) | 353,544 kB (345 MiB) | 283,684 kB (277 MiB) |

Reduction: -2.05 GiB switch-off (-87%), -2.03 GiB pes-loop (-85%). The pes-loop median in the sampler is not usable (the series includes a long low-RSS stretch); peaks and end values are the figures.

Clips actually used (rebuilt sides, from the release counter, same run): switch-off 3 of 8,838; pes-loop 184 of 8,838.

Smoke hashes (CONFIRMED, identical to baseline):
- switch-off `399e180f39459c2b` (30,213 ticks), same as baseline and as the probe build.
- pes-loop `85c7d7c515cf92ae` (30,080 ticks), same as baseline and as the probe build.

## Tests (tests/sim/pes_clips_test.cpp, install-gated; `pes_clips_test`)

- `PesClipsInstall.ReleasedClipsHoldLessHeapThanTheirKeys` (RED on origin/main: `AddPesClips holds 459243 B per clip (both sides); playing one costs 0 B`; GREEN after). Relation: the metadata of a clip costs less than the keys of its two sides.
- `PesClipsInstall.RebuiltKeysEqualAFreshLoadOfTheMotion` (guard, passes before and after): every rebuilt key list (as authored and mirrored) equals a fresh `LoadText` (+`Mirror` for the mirror).
- `PesClipsInstall.RebuiltKeysDoNotDependOnAccessOrder` (guard): two collections touched forward and backward give the same keys, velocities, angles and variables.
- Run on the final tree: `PesClips.*` and the three new tests, 7 of 7 PASSED. Earlier full `PesClipsInstall.*` run on the implementation: 13 of 13 PASSED.

## Mutations (run, rebuilt, reverted)

- M1: drop the mirror reflection in the reloader. Result: RebuiltKeysEqualAFreshLoad fails on every mirror side (for example `avoidjump_1_1_slide (mirror)`, `tacklefoot_parallel_3_3_f090 (mirror)`). Reverted.
- M2: remove the key guard in `GetKeyFrame`. The test binary produced no PASS/FAIL line in the captured output (the run was killed at its 1,400 s timeout or crashed; the exit status was not captured). Not re-run within budget: treat this check as NOT CONFIRMED. Reverted.

## Not done (open)

- Lazy GANI decode: 146.5 MiB of the new 0.3 GiB is the decoded `pes::Gani` set (kept in the library's `PesClipSet`). The next owner, not started.
- `FootballAnimationExtension` per-clip `std::map`: zero keys for PES clips; GF clips' extension maps not measured separately.
- Gate: see the last line.
- Slot count: unchanged (simslot.sh untouched), per the assignment; a sim now uses about 0.3 GiB, so Main decides the count.
- Probe counters are not in the commit (removed before commit).

## Gate

GATE fast: PENDING (log `<workdrive>/gf-SimMemory2-scratch/gate.out`).
GATE fast: NOT YET GREEN at hand-off. The gate was still running about 3 h after launch (load ~26 on 16 cores); its ctest summary had not printed. Its `select_recorded` step selected 0 of about 3,700 recorded functions. Check `<workdrive>/gf-SimMemory2-scratch/gate.out` for `GATE fast: GREEN|RED` and `GATE_EXIT`; the ctest log is `<workdrive>/gf-SimMemory2/build/gate.ctest.log`.
