# pes-port-StoppageFlow4: the R1 "deferred card" was a foul-hold lane error; stall and restart reds open (28-10-26)

**Status.** Branch `StoppageFlow` at `bc1df87c` (pushed; `ls-remote` confirmed). Not landable yet: `pes_stall_test` is red, and `pes_restart_delay_test` has one red test. R1 is fixed and was not a deferred card. The foul hold read the wrong lane of PES's hold table, so every foul stop ran 3.0 s long. Named oracle: `AMeasuredStop…` green, `ACarriedThrowIn…` green, `ALiveBall…` (stall) red, `pes_restart_delay_test` no longer times out (one red test).

## 1. Commits
- `d827d348`: merge of `origin/main` (`e9687d7c`) into StoppageFlow. `merge-tree` was clean; the `extern/godot-cpp` symlink was swapped for the merge and restored. Pushed; `ls-remote` confirmed.
- `bc1df87c`: `pesstoppage`: table[1] is lane 1 of the 0x14259ce60 vector. Pushed; `ls-remote` confirmed. Diff: `pesstoppage.cpp` (1 line), `pesstoppage.hpp` (comments and the field), `pes_stoppage_timing_test.cpp` (+3 lines, `LoadsFromTheExe` pin).

## 2. R1: the 10330 ms free-kick takes (CONFIRMED)

### 2.1 The card hypothesis is refuted
- Temporary trace prints (reverted; `referee.cpp` has no diff). In the two-minute seed-1000 match, `IssueDeferredCards` never runs, in switch-off and in PES-loop alike (no `[card-issue]` line in either run).
- Every advantage expiry in that match is foul type 1. `CardBook::ShouldDefer` (foulType ≥ 2) drops those, so the book stays empty at every free kick.
- The card condition in the port matches PES and was not changed: a yellow-level foul let go under advantage books the fouler at the next stoppage (`docs/re/pes-re-duels-fouls.md` §3, lines 361–366: `FUN_1405103e0` stores it, `FUN_14050de50` writes it at the next stop; `docs/re/pes-re-A5Judge.md` row J5). Red-level fouls never get advantage (duels §3: "level 4 → never").
- StoppageFlow2 §4 and StoppageFlow3 §2 attributed the 3 s to a deferred card. That reading is wrong.

### 2.2 Where the 3 s comes from
- Switch-off `ScheduleStop` for a free kick gives `[fk-sched] pesLoop=0 prepare+8100 take+10330`. Prepare = stop + `holdKind1_s` + wipe (0.1 s), so the loaded `holdKind1_s` is 8.0 s, not the 5.0 s the test assumes (PES 9.2, no scene).
- The loader reads `HOLD_KIND1_VA = 0x14259ce60`. Values read from the unpacked image `pes_full.bin` (offset = VA − 0x140000000), by script:
  ```
  0x14259ce50 [7.0, 2.5, 0.0, 0.0]
  0x14259ce60 [8.0, 5.0, 3.0, 0.0]
  ```
- `FUN_14044e460` (kind in `r8d`, `movsxd rbp,r8d`) copies both vectors: `movaps xmm0,[0x14259ce60]` at 0x14044e475 and `movaps xmm1,[0x14259ce50]` at 0x14044e47c, to `rsp+0x20` and `rsp+0x30`. It returns `movss xmm0,DWORD PTR [rsp+rbp*4+0x20]` at 0x14044e56b.
- So kind 1 is lane 1 = **VA 0x14259ce64 = 5.0** (table[1]). Kind 5 = 0x14259ce54 = 2.5 (already right). Kind 2 (offside) = 0x14259ce68 = 3.0. The loader's VA 0x14259ce60 is lane 0, table[0] = 8.0: a four-byte neighbour error, present since commit `93d808ef`.
- Install check: `LoadsFromTheExe` reads `PES2021.exe` under `PES21_ROOT`. It returned holdKind1_s = 8 before the fix and 5 after.
- Hold consumers: `FoulSequence::HoldEndsAt_ms` (switch-off) and `pes::RunProgramme` (PES loop) both read `holdKind1_s`. Both were 3 s long.

### 2.3 Fix and evidence
- Fix: `HOLD_KIND1_VA = 0x14259ce64`; header comments updated. Test: `LoadsFromTheExe` pins `holdKind1_s = 5.0` with the 9.2 citation.
- Red (before): `LoadsFromTheExe` 8 ≠ 5; `AMeasuredStopRunsPesBracketNotFifteenSeconds` three free kicks at 10330 ms.
- Green (after): `LoadsFromTheExe`; `PesStoppageTiming.*` 9/9 (bracket test 48.6 s, free kicks 7330 ms, out-of-play 4830 ms).
- Mutation: the pre-fix VA 0x14259ce60 is the red state above, observed in both tests before the fix.
- Not verified on this tree: the StoppageFlow2/3 phase tables (W→ready 276–308 frames for free kicks). With an 8.0 s hold they cannot be reproduced. PES live FK whistle→ready is 313 frames (StoppageFlow §1), which fits the 5.0 s hold. Re-measure before using them.

## 3. Late-card mutation (item 4; CONFIRMED)
- Mutation: `LateCardStep_ms` returns `holdBase_ms + 0 * scene_ms` (scene term dropped). Red: `PesStoppageTiming.LateCardShowsAtSceneTickFortyAndRunsTheLongerOfTheHoldAndTheScene`, `pes_stoppage_timing_test.cpp:414` (2500 vs 5666).
- Reverted. `git diff` on `src`, `tests`, `apps` is empty. The reverted build passes the test in the full suite (`pes_stoppage_timing_test` rc=0).

## 4. Remaining reds (item 3)
- **`pes_stall_test` `ALiveBallNeverLiesStillLongEnoughForTheBreaker`: RED, not fixed.** The breaker forces a live ball for 147 ticks on a seed-7 two-minute PES-loop match (`liveFirings`, line 122). The R1-fixed tree gives 147, and the count was not measured on the pre-R1 tree in this session.
  - Split on `OppNear` (`1472df11`, Cutover7, `active`-only filter): filter as committed gives 147; filter removed (scratch, restored, `git diff` empty) gives 147. `OppNear` is not the cause on this trajectory.
  - Late card: no card-level expiry or `[card-issue]` in that match, so the late-card change is not involved.
  - Cause: OPEN.
- **`pes_throwin_release_test` `ACarriedThrowInIsHeldAtItsRowsKeyOffsetUntilTheTouch`: GREEN** on the R1 tree (132 s). It was red in StoppageFlow3 §2. The `OppNear` split was not run because the test is green without any change to `OppNear`.
- **`pes_restart_delay_test`: no longer times out** (completes in 1725 s; StoppageFlow3 recorded exit 124 at 1800 s). 22 pass. One red: `AThrowInIsNotAwardedAgainOnItsOwnSpot`, `pes_restart_delay_test.cpp:564`, `throwIns > 0` with actual 0 ("no throw-in was released", 282 s). Not localised; `OppNear` split not run. 4 tests are `DISABLED_` and were not run.

## 5. Match length, seed 7, 10 min, `--pes-loop` (item 5; not tuned)
- R1 tree (stall red, late card unchanged): **54,227 ticks**, determinism `d337a806ecd47e37`, full time.

| run | ticks | vs PES 48,708 |
|---|---|---|
| PES live, one match (H1 inferred) | 48,708 | 0 |
| StoppageFlow2 `999c300d` | 46,973 | −3.6% |
| merged with Cutover, before R1 (`421dbbf0` code) | 58,645 | +20.4% |
| R1 fixed (`bc1df87c`) | 54,227 | +11.3% |

- Every GF run on this lineage used the 8.0 s foul hold (the VA dates from `93d808ef`), so the 46,973 and 58,645 figures are not like-for-like with the fixed tree. The remaining gap is open; it is not attributed.

## 6. Acceptance counts (`build/tests/**/pes_*`, sim slots 2–3, 1800 s limit)
- 96 binaries. 94 exit 0. Red: `pes_stall_test` (1 of 2 tests; `ALiveBall…`), `pes_restart_delay_test` (22 passed, 1 failed, 4 disabled).
- Passed test cases: 1,981 (1,959 in the 95-binary script run, 22 in restart-delay). Failed: 2.
- The script ran every binary except restart-delay, which was run separately. Three logs mention skipped tests; not inspected.
- Gate: not run (Main gates). Smoke: the 10-minute run in section 5.

## 7. Mutations run (each run, rebuilt, reverted)
- M1: VA 0x14259ce60 (pre-fix lane 0) → `LoadsFromTheExe` red (8 ≠ 5) and the bracket test red (10330 ms). Reverted by the fix.
- M2: `OppNear` `active` filter removed → stall 147 ticks, unchanged. Restored.
- M3: `LateCardStep_ms` scene term dropped → late-card test red (2500 vs 5666). Restored.

## 8. Open items (numbered)
1. Stall: breaker forces a live ball for 147 ticks (seed 7, 2 min). Ruled out: `OppNear` active filter. Next probe: at the first firing tick, log the holder's command and action state (pass, shot or dribble choice) and compare with PES's holder release.
2. `AThrowInIsNotAwardedAgainOnItsOwnSpot`: zero throw-ins in its run. Next: check whether the trajectory has any touchline out (StoppageFlow2 §1 and §6 found the same pattern).
3. Four `DISABLED_` tests in `pes_restart_delay_test` are not run.
4. Re-measure the StoppageFlow §2/§6 phase tables and the match lengths on this tree; they were taken with the 8.0 s hold.
5. Docs (Main, `docs/re` rule): `pes-re-foul-choreography.md` 9.3 table and the row at line ~712, and `pes-re-A5Judge.md` line 233, cite 0x14259ce60 = 5.0. Correct to 0x14259ce64 (lane 1) and cite the `movss` at 0x14044e56b.
6. Direct foul card stand-in `kGfCardOnScreen_ms` (3.0 s, StoppageFlow3 §7.8) is still open.
7. 10-minute seed 7: 54,227 ticks vs PES 48,708 (+11.3%). Not tuned; the gap is unexplained.
