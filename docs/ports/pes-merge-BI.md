# Batch BI: RestingBall3 (RestingBall6's tree) on main, with the ceremony seeds re-staged

## 1. Contents and base

- Branch `RestingBall3` (`14d47a87`, the tree reported in `pes-port-RestingBall6.md`) merged onto `origin/main` `dcf1e2e9` in `gf-MergeLane` as `MergeBatchBI`. Merge commit `aca7a141`.
- Seed re-stage `cc540ae8`: `kCeremonySeeds` re-staged from the census in section 3. **This is the gated commit.**
- `bba6687b`: merge of `origin/main` `8d266e3c`, which had moved on by four docs/ports archive commits after `dcf1e2e9` (`docs/ports/README.md` and their own reports). No code, no ledger, no smoke input.
- What the branch brings: the trap family and carrier gate (`pes::CarrierMayTouch`, gate 3), the 661-member closure of `0x1407A38E0` (layer 5652), `0x1406E9F10` and `0x1406E9F90` translated and proven, the CVTPS2DQ lowering, `PesReceiveCarrier.*` and `PesStall.*`. VAs and proofs: `pes-port-RestingBall6.md` sections 2 to 5.
- Main since the branch's base `406ab7e2`: eight commits (docs archives and hook_plan targets). The 539 files this branch changed since `406ab7e2` and the 10 files main changed in `406ab7e2..dcf1e2e9` share none.

## 2. Conflicts

None. The merge applied with no unmerged path.

- RestingBall3's earlier merge with `406ab7e2` (`512398f8`) conflicted in `tests/CMakeLists.txt` and `tools/dev/README.md`. Main has changed neither since, so the auto-merge took the branch side for both.
- `tests/CMakeLists.txt`: the branch's only change there is whitespace (one blank line moved). The merge commit carries main's bytes (`git checkout origin/main -- tests/CMakeLists.txt`), so the diff against main is empty.
- `tools/dev/README.md`: the branch's synth-tool bullet is kept. Its extra blank line before `### The branch tier` is kept too (whitespace only).
- Generated files (`src/pes/translated/**`, `tools/dev/ledger/universe.json`, `docs/re/LEDGER.md`, `tools/dev/translate/ro_tables.json`) came from the branch unchanged. The gate's layer drift test and ledger check confirm them (section 4).

## 3. Census (rule 6)

**Scenario:** the ceremony test's own (`PesThrowInRelease.ACeremonyThrowInIsReleasedFromTheHandOverTheSpot`): one two-minute pes-loop match per seed (`RunMatch`). A ceremony throw-in is one whose taker is `IsPieceTakerHeld` when the throw-in starts; it counts as released when the set piece ends. Carried throw-ins (for `kThrowInSeeds`) are `IsCarryingBall` at the start. The key check is the test's: the held ball against `Hits(0xcfa)[1]` through `ClipOffsetMetres`, tolerance 0.05 m.

**Method:** a scratch-only test `CensusScratch` (never committed; reverted before the re-stage; source in `gf-MergeLane-scratch/BI/census_probe.txt`) reads one seed from `GF_CENSUS_SEED` and prints one `CENSUS` line. One process per seed, each run under `tools/dev/simslot.sh` with `GF_SIM_PRIORITY=0`. Seeds 1 to 9 ran serially. Seeds 10 to 24 ran as a queued batch through simslot (Main's rate ruling; the slot pool bounds it). Seed 10's first serial attempt was cancelled and is not used. `first` is `Match::GetPesFrame()` at the throw-in's start.

| seed | ceremony releases | first ceremony frame | key mismatches | carried releases | first carried frame |
|---:|---:|---:|---:|---:|---:|
| 1 | 0 | - | 0 | 2 | 10843 |
| 2 | 0 | - | 0 | 2 | 14678 |
| 3 | 0 | - | 0 | 1 | 29317 |
| 4 | 0 | - | 0 | 6 | 3993 |
| 5 | 0 | - | 0 | 1 | 30381 |
| 6 | 0 | - | 0 | 2 | 6358 |
| 7 | 0 | - | 0 | 3 | 8438 |
| 8 | 0 | - | 0 | 1 | 26910 |
| 9 | 0 | - | 0 | 3 | 9993 |
| 10 | 1 | 42655 | 0 | 1 | 10797 |
| 11 | 1 | 11290 | 0 | 2 | 10223 |
| 12 | 1 | 7525 | 0 | 1 | 14229 |
| 13 | 0 | - | 0 | 3 | 7781 |
| 14 | 0 | - | 0 | 0 | - |
| 15 | 0 | - | 0 | 1 | 6081 |
| 16 | aborted: `PES_DISPATCH_UNTRANSLATED 0x14079E0B0`, core dump (rc 134) | | | | |
| 17 | 0 | - | 0 | 5 | 4803 |
| 18 | 2 | 7237 | 0 | 2 | 2531 |
| 19 | 0 | - | 0 | 5 | 1511 |
| 20 | 0 | - | 0 | 2 | 13869 |
| 21 | 0 | - | 0 | 5 | 2925 |
| 22 | 1 | 31775 | 0 | 0 | - |
| 23 | 1 | 27409 | 0 | 4 | 2950 |
| 24 | 0 | - | 0 | 2 | 27920 |

**Results:**

- `kCeremonySeeds`: the old `{8, 3}` drifted. Seed 8 and seed 3 now release no ceremony throw-in. Re-staged to `{18, 12, 11, 23, 22, 10}`, fastest first. Assertions unchanged. Every ceremony release on the list matched the key.
- `kThrowInSeeds {6, 11, 10, 5}`: no drift. Each still releases a carried throw-in, first at frames 6358, 10223, 10797 and 30381, in that order, so the list is kept. Seed 7 (first carried 8438) is now earlier than 5 and 10; the four fastest on this tree are 6, 7, 11, 10. Not re-staged, since nothing drifted. Candidate for the next batch that touches the carried tests.
- `pes_restart_delay_test` seed lists: not censused (a different scenario). They pass in the gate (section 4).

## 4. Gate

`GATE_TIER=fast GF_SIM_PRIORITY=0 tools/dev/gate.sh <lane>/build` on `cc540ae8`, with `PES21_ROOT`, `PES_EXE`, `GF_DIFF_PYTHON`, `GF_IMPURE_BANKS` and `ulimit -S -v unlimited`, run with no deadline (52 min wall). Output: `gf-MergeLane-scratch/BI/gate.out`.

```
GATE fast: GREEN
```

- ctest: 100% of 2333 (`ctest rc=0`). `pes_throwin_release_test` passed (1084 s, all cases in the binary). `diff_gen_band`, `diff_gen_strategy`, `diff_gen_cone` and `diff_gen_cond_arrow` passed (not skipped). `diff_core_recorded`, `diff_cutscene_recorded` and `diff_presentation_recorded` passed.
- `select_recorded`: base `dcf1e2e9`, 575 changed paths.
- Ledger: `LEDGER.md current (38338 rows)`.
- Smokes (seed 7, 2 min): both rc 0, with determinism lines (section 5).
- Build: Release, `cmake --build -j3` with ccache, no errors. One LTO warning, open item 2.

## 5. Smoke attribution

| smoke (seed 7, 2 min) | main `dcf1e2e9` | RestingBall6 (tree on `406ab7e2`) | MergeBatchBI `cc540ae8` |
|---|---|---|---|
| switch-off | `399e180f39459c2b` | `a0fbe2d5cd445a52` | **`a0fbe2d5cd445a52`** |
| pes-loop | `85c7d7c515cf92ae` | `17941efc16b4a6a5` | **`17941efc16b4a6a5`** |

- Both hashes equal RestingBall6's section 8. The switch-off move is the pass-target stand-in `Team::PassGetReceiver` (kept until TeamTick). The pes-loop move is the trap family, the carrier gate and the translated leaves, not split (RestingBall6 section 8).
- Main's commits since the base, and the four docs-only commits after `dcf1e2e9`, do not feed the smokes. The seed re-stage is test-only and changes no match.

## 6. Open

1. **Seed 16 aborts.** A two-minute pes-loop match on seed 16 reaches `PES_DISPATCH_UNTRANSLATED 0x14079E0B0` and core-dumps (`census3/seed_16.log`). That address is not in RestingBall6's closure roots. Not run on main, so not attributed to this batch. Survey its callees (`GF_PES_SURVEY`) before any cutover (rule 1).
2. **LTO warning `-Wstringop-overread` at `src/sim/onthepitch/pestrapactor.cpp:152-153`** (`WriteActorP` reads 12 bytes from the man record; "source object is likely at address zero"). `ManRecord(slot)` (`pesworldrecord.cpp:415`) is `InWindow(Bytes(), pes_fn_140A48A10(...))` and can return null when the PES answer falls outside GF's window. `WritePesTrapActors` (`teamAIcontroller.cpp:3541-3564`) does not check it. This batch does not touch those files; neither smoke nor any test reaches the null. Open: can an active player's `PesSlotOf` produce a window miss?
3. Carried list: seed 7 now releases earlier than 5 (section 3). Re-stage at the next batch that touches `pes_throwin_release_test`.
4. RestingBall6's open items (its section 9) are unchanged: gate 3's second clause (`MA+0x247BE`), the keeper claim flag `MA+0x251FA`, TeamTick5's resting-ball shape, `ALiveBall` walk-vs-run, the pass-target stand-in until TeamTick, and the real `rb5_*` captures.
