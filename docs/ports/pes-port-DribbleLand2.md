# DribbleLand2: DribbleProposal merged with main c45fbd57 (batches BD, BE, BF); GATE fast RED on exactly one test, the resting-ball stall that RestingBall fixes; smoke moves attributed by ablation (05-11-26)

Branch `DribbleProposal` at `916e755f` (`git ls-remote origin DribbleProposal` confirmed), worktree `gf-DribbleProposal` (Release `build`), ablation worktree `gf-DribbleAblate` (removable), scratch `gf-DribbleLand2-scratch`. CONFIRMED = observed this session.

## 1. Merges (CONFIRMED)
- `92fd5b32` = merge of main `23aed782` (BD). One conflict, `tests/sim/pes_throwin_release_test.cpp` (`kThrowInSeeds`/`kCeremonySeeds`): main's side taken whole (`{6,11,10,5}`, `{8,3}`).
- `1b7c9930` = merge of main `c45fbd57` (BE + BF). One conflict, `tests/sim/pes_restart_delay_test.cpp` (ceremony seed list): main's side taken whole (the ceremony probes are `CeremonyProbe(13u), (8u), (4u)` in the shared-match structure).
- `916e755f` = re-staging commit (section 3).

## 2. Phase 1 results (unchanged)
- The three `PesThrowInRelease` timeouts (`AThrowInIsReleasedFromInsideTheLine`, `ACarriedThrowInIsHeldAtItsRowsKeyOffsetUntilTheTouch`, `TheLiftTurnsACarriedThrowInTakerToFaceThePitch`) passed alone through simslot on the BD tree (1038, 1252, 1025 s wall): **load, not real**. On the BE tree (shared matches) the whole `pes_throwin_release_test` binary passes in 1760 s.
- Smoke ablation (seed 7, 2 min, both rc=0). Ablation tree = `92fd5b32` + `git revert --no-commit 6e851f03`:

| Tree | switch-off | pes-loop |
|---|---|---|
| main `23aed782` baseline | `399e180f39459c2b` | `85c7d7c515cf92ae` |
| DribbleProposal `92fd5b32` | `f7cafd50fed02b33` | `5da302b9db51f82c` |
| `92fd5b32` + revert `6e851f03` | `399e180f39459c2b` | `85c7d7c515cf92ae` |

Both moves are attributed to `6e851f03` (the deletion of the GF playing-style / COM-inference paths): reverting it reproduces main's baselines bit-for-bit. On the BE+BF-merged tree (`916e755f`) the gate's smokes give the same hashes: **switch-off `f7cafd50fed02b33`, pes-loop `5da302b9db51f82c`** (25 / 26 stoppages, longest 23.1 s / 23.6 s, 31783 / 35562 ticks; both rc=0, reach full time). The ablation was run on the BD tree; it was not repeated on the BE+BF tree (BE/BF cannot move the smoke hashes: BE is test structure, and the gate's hashes on the merged tree equal the pre-BE ones).

## 3. Census on the BE structure (CONFIRMED; assertions untouched)
After the BE+BF merge I ran the families once, unmodified, as the gate runs them (`pes_restart_delay_test` incl. PesStall as one process 7497 s; `pes_forfeit_test` 1705 s; `pes_throwin_release_test` 1760 s, all via simslot, concurrently):
- `pes_throwin_release_test`: all pass, no re-stage needed (main's lists hold).
- `pes_forfeit_test`: both pass (`ARedCardRemoves...` 371 s, `AYellowThenRedStillOnThePitchAtFullTimeForfeitsThreeNil` 788 s): **no drift on the merged tree, `SEED`/`kRedCardSeed` unchanged.**
- `PesRestartDelay.AThrowInTakerWalksToHisSpotAndIsNotPlacedOnIt` failed (`placedOnTheWay` 1 on `SEAMLESS_SEED = 3`). Census over `SEAMLESS_SEED` (temp `GF_CENSUS_SEED` override, reverted before commit; the seven tests that share the seamless match, serially through simslot): **seed 1 passes all seven** (822 s). `SEAMLESS_SEED = 1` staged, comment states it is a census result (`916e755f`). Seeds 2..10 were not needed.
- `PesStall.ALiveBallNeverLiesStillLongEnoughForTheBreaker`: **NOT re-staged, per Main's rule.** The test alone (386 s) shows one breaker episode, 67 ticks: ball at rest at (-25.9, 12.7), piece 3 started, taker H11 in `mid_jocky_standby_noloop`, forced man H10 0.1-0.5 m from the ball in `dribblerun_3_3_000_out_far` fr 10/91. A temporary per-firing print (reverted) shows on every firing tick: previous designated man H10 (the same man), `HasPossession()==1`, `GetBallRetainer()==null`, ball at rest at his feet. This is exactly the RestingBall shape (designated man + possession + no retainer + ball at rest, `docs/ports/pes-port-RestingBall.md` section 1), not a different drift-explained one. It waits for RestingBall's fix on main. Caveat from RestingBall's own report: with its current fix `b5a416b4` this test is red on main seed 7 for other episodes (167 ticks), so the landing needs those diagnosed.

## 4. GATE fast on `916e755f` (gate.sh with GATE_SLOTS unset, `GATE_TIER=fast`, 14,170 s ctest wall) = **RED pes_restart_delay_test**
`99% tests passed, 1 tests failed out of 2321`; the only failing gtest is `PesStall.ALiveBallNeverLiesStillLongEnoughForTheBreaker` (inside `pes_restart_delay_test`, the shared process). Everything else passed, including the walk test (seed 1), forfeit, all throw-in release, `diff_gen_*`. Recorded re-check 0 selected of every set; no unrecorded status regression (18,635 rows); 125 off-path ports allow-listed; ledger current (38,116 rows). `GATE fast: RED pes_restart_delay_test`.

## 5. Left (numbered)
1. `PesStall.ALiveBallNeverLiesStillLongEnoughForTheBreaker` (seed 7, 67 live ticks, shape = resting ball under a man with possession) waits for RestingBall's fix on main; no re-stage was made. Once that lands, merge main and run `GATE fast`: expected GREEN if the fix clears this episode.
2. Main's rule 6 (one census per batch, in the merge lane) reached me after the `SEAMLESS_SEED = 1` census was done: the result is kept (`916e755f`); Main's merged-batch census may supersede it.
3. The BE+BF-tree ablation was not repeated (section 2).
4. Comments in the re-staged tests cite `docs/ports/pes-port-StoppageFlowLand.md`/`CeremonySeeds.md` from main's side as-is.
