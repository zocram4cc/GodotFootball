# Gate speed, SeedShare2: one process for the restart, throw-in, holder and press families

## Result

- Branch `SeedShare2` at `cd1379dec4da80fc5d285de78dafb462b50561aa` (base `784ffd5d`), pushed. `git ls-remote origin SeedShare2` returns that SHA.
- `GATE fast: GREEN`: 2317 of 2317 ctest entries passed (`<workdrive>/gf-SeedShare2-scratch/gate.log`).
- Verdicts unchanged: 33 of 33 gtests OK before and after. All 35 old probe stops reproduced exactly.
- Summed wall time of the four family entries fell 16.5% (5,571 s to 4,652 s). The critical-path entry grew 21.5% (3,829 s to 4,652 s). **Wall time did not improve.** Open item 1 is the cause and the fix; the fix is not in this branch.

## What changed (tests/sim only)

- `pestest::CallbackProbe` takes `shared` (default false, so existing callers are unchanged).
- `pes_holder_latch_test.cpp` (PesHolderLatch, PesHolderPersist) and `pes_press_approach_test.cpp` (PesPressApproach) became shared probes on `{pes_loop, 2 min, seed 7}`, the key the restart family plays. Their state moved to file-scope structs written by the probes.
- `pes_throwin_release_test.cpp`: the ceremony test's unshared runs (seeds 8, 3) became `CeremonyHandProbe`s. The other throw-in tests already used shared probes.
- `pes_restart_delay_test.cpp`: `ThrowInArmSpeculation` probes observe the ball-at-rest search's seeds in the shared runs. A seed whose match reaches a throw-in is then replayed alone with the original perturbing callbacks, unchanged.
- `tests/sim/CMakeLists.txt`: the throw-in, holder and press sources compile into `pes_restart_delay_test`. Their separate ctest entries are gone.
- Unchanged: every assertion and seed, PesStall, the cross-seed and cumulative tests, PesForfeitMatch.

## Proof: same stops, same verdicts

- Old: the four origin/main test binaries (`784ffd5d`), each its own process. New: the merged `pes_restart_delay_test`. Both with `PES21_ROOT` and `PES_EXE` set (`PES_EXE` is required by `FillMissingPesConstants`).
- Temporary `[proof]` prints (frame, `GetDeterminismHash()`, judged, game over) at each probe stop. They are removed; `grep PROOF-TEMP` on the branch is empty.
- 35 of 35 old stops reproduced exactly in the merged process. No frame has two hashes across the runs.
- 33 of 33 gtests OK in both (old 24 + 6 + 2 + 1; merged 33).
- 3 stops appear only in the merged process. They are riders on runs other tests need, not verdicts: ABallResting seed 7 stopped at frame 24994, its throw-in start, and the test then replayed seed 7 alone as before; ABallResting seed 3 (frame 32697) and ceremony seed 3 (frame 33124) ride runs other tests play.
- Same proof as SeedShare: an identical stop hash at the same frame means an identical sim state. Each probe's observations, and the values its test asserts from them, are therefore the old ones. The asserted values were not printed side by side.

## Measured family wall times (ctest entries, seconds)

- Before: BE gate `<workdrive>/gf-MergeLane/build/gate.ctest.log` (main `d0f98581`, slots 1 0, load about 26).
- After: this gate `<workdrive>/gf-SeedShare2/build/gate.ctest.log` (`cd1379de`, slots 1 0, load about 16 to 18).

| family | before | after |
|---|---:|---:|
| pes_restart_delay_test (24 gtests, incl. PesStall) | 3,829.2 | |
| pes_throwin_release_test (6 gtests) | 877.5 | |
| PesHolderLatch.ALastToucherWithLivePossessionBecomesTheHolder | 288.2 | |
| PesHolderPersist.ASetHolderIsNeverClearedWhileTheBallIsInPlay | 287.5 | |
| PesPressApproach.ALoosePresserCommandNamesTheActionPressTarget | 288.8 | |
| **family entries, sum** | **5,571.2** | **4,651.7** (one entry, 33 gtests, Passed) |
| **critical-path entry** | **3,829.2** | **4,651.7** |

- ctest total: 8,797.9 s before, 8,690.8 s after. Not like-for-like. The BE gate re-checked the recorded diff suites (`diff_core_recorded` 756.6 s, `diff_cutscene_recorded` 335.2 s, `diff_presentation_recorded` 224.6 s). This branch's diff against `784ffd5d` touches test sources only, so `select_recorded` chose 0 of 245/685/546/2253/34 recorded checks and those entries took 0.1 s each.
- Noise: untouched entries in this gate ran 1.02 to 1.28 times their BE times (PesForfeitMatch 1.11 to 1.17, GoalSceneHold 1.02, diff_translated_drift 1.28; small MoveStepWire entries up to 4). Treat per-entry times as about plus or minus 20%.
- Paired, same session (proof runs): the restart-only process took 5,168 s; the merged process took 6,404 s (+24%). The four old processes summed to 7,404 s, so the merge saves about 1,000 s of summed process wall time (-13.5%).

## Gate: `GATE fast: GREEN`

- `100% tests passed out of 2317`; `Total Test time (real) = 8690.76 sec`.
- Smokes: switch-off `[determinism] 399e180f39459c2b`, pes-loop `[determinism] 85c7d7c515cf92ae`. Both equal the main baselines. This branch changes no product code.
- `no unrecorded status regression vs HEAD (18635 rows compared)`; `off-path ports: 125 (all allow-listed)`; `LEDGER.md current (38116 rows)`.
- Job wall 34,081 s (11:38 to 21:06). About seven hours of it was the wait for a sim-slot pair; the smoke and ctest logs begin near 18:40. The job wall is queue time, not the branch's cost.

## Open

1. **Critical path regressed (CONFIRMED by key analysis and by the paired timing).** The merged process also runs the throw-in test's own keys `{true,2,6}` (InsideLine, CarriedHold, Lift: seed 6) and `{true,2,8}` (ceremony hand: seed 8). These are full two-minute runs that share no key with the restart family. Before the merge they ran in parallel in their own entry.
   - Fix candidate (INF, not built): restore `pes_throwin_release_test` as its own executable and ctest entry. Keep holder and press in the restart process: their key `{true,2,7}` is the restart family's, and their 19,440-tick cap ends before that run does, so they add nothing to its critical path.
   - Expected (INF from the BE timings): the restart entry returns to about its BE value, and holder and press save about 0.6 k s of summed wall time. Needs a CMake change and a second fast gate.
   - Recommendation to Main: land after this fix rather than as pushed.
2. **Fork or snapshot sharing not attempted.** The first sharing rule requires a bit-exact copy of the sim, and I did not try to copy a `Match`. PesForfeitMatch (six tests; their common prefix is the kickoff) and the goal-scene hold family therefore remain whole runs.
3. **Not converted.** The cross-seed tests (seeds 23, 42, 57, and `pes_loop` off) have unique keys. The cumulative searches (PesLoopRestartsAreArmed, AGoalKickIsReleasedOnThePesLadder, and the ceremony, not-awarded and natural-goal loops) stop on results from earlier seeds, so sharing would change what each one sees. For the other families at 120 s or more, I read only the match builders, by grep, not each test's perturbation before its first tick. BE totals: MoveStepWire 1,002 s; pes_playerdb_wiring_test 821 s; pes_tick_a1_test 525 s; pes_period_end_test 393 s; StyleRunDepthSite 600 s; pes_setpiece_stand_test 301 s; PesRestartTaking 237 s; PesChallengeCounters 232 s; SimSubstitution 231 s; pes_tick_order_test 150 s; PesClipsInstall 147 s; FoulWhistle 145 s; PesEventWiring 139 s. Observation-only candidates, such as PlayerDB wiring (seed 1000, five minutes, one key per `pes_loop` value), need a per-test read.
4. **Mutations:** none run and none reported.
