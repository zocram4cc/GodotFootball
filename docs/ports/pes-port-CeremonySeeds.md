# PES port CeremonySeeds: batch AV's two ceremony throw-in reds were seed drift from StyleCutover2, not a regression (01-11-26)

Branch `DribbleProposal`, worktree `<workdrive>/gf-DribbleProposal`, Release dir `build` (absolute path passed to the gate). CONFIRMED = observed this session.

## 1. Merge
`git merge origin/main` (`6918e7de`, ProcCamera6 report): conflict-free, merge `e87227d5`, pushed (`git ls-remote` confirmed). Re-staging commit `491ba167`, pushed and confirmed.

## 2. Cause of both reds (CONFIRMED)
`PesRestartDelay.AThrowInCeremonyTakerIsPlacedOutsideTheLineAndReleasedWithTheBallInHand` and `PesThrowInRelease.ACeremonyThrowInIsReleasedFromTheHandOverTheSpot` both search `kCeremonySeeds = {2, 15, 16}` (staged by `pes-port-CardLevel7.md`) for a throw-in whose taker is placed over the spot (path B, a ceremony taker). StyleCutover/StyleCutover2 delete the width tuck, poacher spot and dribble style terms (`pes-port-StyleCutover2.md` section 6: switch-off smoke `3b73c968b4a377e0`, pes-loop `0e480b158cf80bd4`), which moves every 2-minute match; none of seeds 2, 15, 16 has a ceremony throw-in any more. The throw-in test then fails on `EXPECT_GT(ceremony, 0)`; the restart-delay test runs three 20-minute matches back to back, finds none, and fails on `EXPECT_GT(throwIns, 0)` (one old-binary run took 1569 s before failing). Ceremony throw-ins still occur (9 of the 40 seeds), so this is the same seed drift as CardLevel7, not a regression: no ablation of the StyleCutover2 deletions was needed.

## 3. Census (CONFIRMED)
Seeds 1..40, Release, 2-minute `--pes-loop` matches (`PesThrowInRelease` probe, removed; counts each throw-in whose release starts, ceremony = `IsPieceTakerHeld`, else carried = `IsCarryingBall`), run through `simslot.sh`, nice 19, three at a time:

| | count |
|---|---|
| matches | 40 |
| throw-ins released | 79 |
| ceremony (path B) | 9 |
| carried (path A) | 70 |
| neither | 0 |

Seeds with a ceremony throw-in: **5, 7, 9, 18, 19, 25, 32, 35, 36** (one each). Seeds 1, 22, 26, 27, 39 released no throw-in in two minutes. Seeds 2, 15, 16 (the old list): 1, 2, 3 carried, 0 ceremony. (Raw per-seed lines: `gf-CeremonySeeds-scratch/census.txt`.)

## 4. Re-staged (assertions untouched)
`kCeremonySeeds = {5, 7, 9}` in `tests/sim/pes_throwin_release_test.cpp` and `tests/sim/pes_restart_delay_test.cpp`, comments updated. The first seed in the list has a ceremony in its first two minutes, so the restart-delay test stops after one match (370-390 s, three runs) instead of searching all three.

Direct runs: `PesThrowInRelease.ACeremonyThrowInIsReleasedFromTheHandOverTheSpot` PASSED (147 s); `PesRestartDelay.AThrowInCeremonyTakerIsPlacedOutsideTheLineAndReleasedWithTheBallInHand` PASSED three times (370 s, 384 s, 391 s; the logs differ only in the final timing lines). A fourth run that failed after 1569 s was my shell chaining: it started before the rebuild finished and ran the old `{2, 15, 16}` binary, which is the red itself.

## 5. Fast gate (CONFIRMED)
Tree `491ba167` (origin/main `6918e7de` merged), Release `build` reconfigured with `PES_EXE` and `GF_DIFF_PYTHON` set, absolute path, `GATE_TIER=fast GATE_SLOTS="1 0"`, gate env `PES21_ROOT`, `PES_EXE`, `GF_DIFF_PYTHON`, `GF_IMPURE_BANKS=gf-Captures-scratch/banks_union9l`:

**`GATE fast: GREEN`**: ctest `100% tests passed out of 2235` (3,204 s); `diff_gen_*` and `diff_pilot_recorded` Passed (none skipped); recorded re-check 0 selected of every set (no recorded function changed since base `6918e7de`); no unrecorded status regression (18,587 rows); `LEDGER.md` current (38,068 rows); off-path ports 125 (all allow-listed). Smokes seed 7, 2 min, deterministic: switch-off `3b73c968b4a377e0` (26 stoppages, longest 23.2 s of bound 32.5 s), pes-loop `0e480b158cf80bd4` (21 stoppages, longest 15.9 s). Both match `pes-port-StyleCutover2.md` section 6. Both former reds Passed inside the gate: throw-in 157 s, restart-delay 402 s.

## 6. Left
1. Every ceremony-seed list in the suite is a seed census result and moves with any change to the early match; re-run the census (method in section 3) whenever a style/AI cutover moves the smoke hashes.
