# PES port CardLevel7: FoulSanction fast gate: both reds were seed drift, not a regression; the smoke-hash move is the translated judge (31-10-26)

Branches `FoulSanction` and `CardLevel2Cutover`: same head (`git ls-remote` confirmed after every push). Worktree `<workdrive>/gf-FoulSanction`, Release dir `build-rel`. CONFIRMED = observed this session.

## 1. Merge (CONFIRMED)
`git merge origin/main` (`7fca7d2c`, CardLevel6's report commit): conflict-free, merge `6232ae83`, pushed.

## 2. Ceremony throw-ins: seed census 1..40, Release, 2-minute `--pes-loop` matches (CONFIRMED)
A temporary census test (`PesThrowInRelease.ZzProbeCensus`, removed; counts each throw-in whose release starts, classified by `IsPieceTakerHeld` = ceremony / path B, else `IsCarryingBall` = carried / path A) ran seeds 1..40 on the translated-judge tree, nice 19 (all four sim slots were held by other gates, so run directly):

| | count |
|---|---|
| matches | 40 |
| throw-ins released | 80 |
| ceremony (path B) | 9 |
| carried (path A) | 71 |

Seeds with a ceremony throw-in: **2, 6, 15, 16, 17, 18, 26, 27, 30**. The old list (3, 13, 11) has none: seed 3: 4 carried, 0 ceremony; seed 7: 3 carried; seed 5: 3 carried. So ceremony throw-ins still occur (about one in nine throw-ins), the translated judge only moved the matches: it is seed drift, not a regression, and no bisect of `origin/main..3e0dd4f8` was needed.

Re-staged (assertions untouched, commit `a30de5e1`):
- `pes_throwin_release_test.cpp`: new `kCeremonySeeds = {2, 15, 16}` used only by `ACeremonyThrowInIsReleasedFromTheHandOverTheSpot` (the carried tests keep `kThrowInSeeds`: they have carried throw-ins in every census seed they use).
- `pes_restart_delay_test.cpp`: `kCeremonySeeds = {2, 15, 16}` (was `{SEED, 3, 5, 11, 13}`).

Direct runs (Release, load 20+): `PesThrowInRelease.ACeremonyThrowInIsReleasedFromTheHandOverTheSpot` PASSED (186 s, seed 2); `PesRestartDelay.AThrowInCeremonyTakerIsPlacedOutsideTheLineAndReleasedWithTheBallInHand` PASSED (602 s; main's gate took 629 s).

## 3. The 1800 s timeout (CONFIRMED)
Same cause. The restart-delay test searches 20-minute matches in turn for the first ceremony throw-in and stops at it; with no ceremony in seeds 7, 3, 5, 11, 13 it ran the five 20-minute matches back to back (about 1800 s under load) and hit the ctest timeout. With seed 2 first it stops after the first ceremony and takes 602 s.

## 4. Why the smoke hashes moved off main's (CONFIRMED by ablation, seed 7, 2 minutes, Release)
Moved: switch-off `e663d1e917d6c4f4` -> `2802089ed1b4da32`, `--pes-loop` `980c54e27f93ced2` -> `e7c1462426cef558`.

Experiments, each a rebuilt `gfsim_run` (binaries kept in scratch, tree restored afterwards):

| tree | switch-off | `--pes-loop` |
|---|---|---|
| HEAD (translated judge decides, sanction runs) | `2802089ed1b4da32` | `e7c1462426cef558` |
| HEAD, `DecideSanction()` call disabled | `2802089ed1b4da32` | `e7c1462426cef558` |
| HEAD with main's `referee.cpp/.hpp`, `pesjudge.cpp/.hpp`, `foulseverity.*` (the hand-ported judge) | `e663d1e917d6c4f4` | `980c54e27f93ced2` |

So the move comes only from `Referee::JudgeContact` deciding with the translated `0x1405B7470` (`PesWorld::DecideContact`) instead of GF's hand-ported scorer: the first contact the two call differently diverges the 2-minute match. The sanction tree (writes the window, feeds the cutscene request) does not move either hash, and nothing else in the branch's source delta (the 17-style run table of `3e0dd4f8`, the man-record writer export, the ring wiring) moves them: with the hand port restored the hashes are main's exactly. The source change entered with `5eefd011` (CardLevel2Cutover: `ContactFacts` + `DecideContact`; the `git log -S` hit for `world.DecideContact(facts)`), carried onto this branch by the merge fold `debffa69`; a per-commit build bisect was not run because the delta is that single logical change and the ablation isolates it.

## 5. Fast gate
Gate 1, tree `a30de5e1` (origin/main `7fca7d2c` merged, ceremony seeds re-staged), Release `build-rel`, absolute path, `GATE_TIER=fast GATE_SLOTS="3 2"`, gate env set (`PES_EXE`, `GF_DIFF_PYTHON`, `PES21_ROOT`, `GF_IMPURE_BANKS=banks_union9l`): **`GATE fast: GREEN`** (ctest `100% tests passed out of 2163`, 15,570 s, recorded re-check 3,763 of 3,763 selected; ledger current; smokes seed 7 / 2 min `2802089ed1b4da32` and `e7c1462426cef558`, deterministic, 25 and 28 stoppages, longest 23.2 s / 23.7 s). Both former reds Passed inside it: `PesThrowInRelease.ACeremonyThrowInIsReleasedFromTheHandOverTheSpot` 148 s, `PesRestartDelay.AThrowInCeremonyTakerIsPlacedOutsideTheLineAndReleasedWithTheBallInHand` 545 s. The `diff_pilot/cutscene/presentation/impure_recorded` and the four `diff_gen_*` lines Passed (the earlier relative-path artefact is gone with the absolute build dir).

Merge of batch AR (`d4fff751`, landed while gate 1 ran), commit `61b18c51`: conflicts only in generated files (`pes_dispatch_gen.cpp`, `translated.hpp`, `LEDGER.md`), resolved by regenerating on the merged tree (`gen_translated.py`: 4,478 functions, 3,700 recorded bit-exact, 778 closure; `--check` byte-identical; `ledger.py`: 38,068 rows). One semantic conflict: main's DribbleProposal made the CFG check thunk `0x1415A01DC` a stand-in spec (`specs/dribble/1415A01DC.json`) while this branch carries an `engine-noop` stub model of it (`stubs.json`, CardLevel3). The generator refuses both ("a callee is one or the other"). Dropping the stub (my first resolution, `61b18c51`) made `pes_fn_14159FA0C` abort at runtime (`PES_DISPATCH_UNTRANSLATED: 0x1415a01dc`, gdb backtrace through `DecideContact`; `pes_judge_decide_test` aborted in its second test). Final resolution: keep the stub model (the judge's closure needs the no-op) and delete main's stand-in spec file (it has no record in `results_dribble.json`; a `ret` thunk either way). Layer regenerated (4,478 functions, `--check` byte-identical), ledger regenerated. On that tree: `pes_judge_decide_test` 9/9, `pes_sanction_test` 10/10, `pes_judge_test` 12/12 PASSED.

**Gate 2 (merged tree) did NOT complete.** It queued ~3 h behind the CutsceneTiming and StoppageFlow gates on every slot (the build finished; ctest never started) and I killed it when the thunk problem showed the tree needed a fix. The AR-merged head has therefore NOT been through the fast gate; `a30de5e1` (pre-AR) has (GREEN, above). Main's gate on the merged head is the remaining check; the one area the AR merge can move is the dribble set (`diff_dribble` is not in the fast tier's list) because main's `1415A01DC` stand-in spec is removed.

## 6. Open items
1. Switch cases 6..14 of the judge's event step still need staged ring-0 records (CardLevel6 item 2).
2. Card proof (level >= 2): no `judge_commit` capture yet (CardLevel6 item 3).
3. Main's batch smoke baselines move to `2802089ed1b4da32` / `e7c1462426cef558` when this branch lands (the hand port is gone; section 4).
