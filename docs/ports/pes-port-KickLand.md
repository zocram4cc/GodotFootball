# pes-port-KickLand: KickSoftMin made landable on merged main (28-10-26)

**Status.** Branch `KickSoftMin` pushed at `5c47a082` (`git ls-remote` confirmed). 7 of the 8 named red tests are green. `PesStall.ALiveBallNeverLiesStillLongEnoughForTheBreaker` stays red and is attributed below (not weakened, not disabled). No production code changed on this branch in this session; only tests.

## 1. Commits (pushed to `origin/KickSoftMin`)
- `a8561ea3`: merge of `origin/main` (`b3665b02`). Docs, `tools/dev/rendezvous/*` only. The `extern/godot-cpp` symlink was swapped for an empty directory for the merge and restored after.
- `bfd46bb6`: cherry-pick of Main's `d344516c` (`tests: goal-kick ladder test searches seeds for its goal kicks`). Unchanged.
- `14be50b0`: `tests/sim/pes_restart_delay_test.cpp`, three tests re-staged (§2).
- `5c47a082`: `tests/sim/pes_throwin_release_test.cpp`, three tests re-staged (§2).

Src diff against `origin/main`: `src/sim/onthepitch/peskickerror.{cpp,hpp}` only (the PES kick-quality fold). The breaker (`match.cpp:941`) and the pickup path are byte-identical to main, so every red here is a trajectory change, not a changed stall path.

## 2. The eight named tests

Red on the merged tree, observed by running each binary. Staging for seed-dependent scenarios uses the file's existing seed-search pattern; assertions are unchanged.

| Test | Red assertion | Cause | Action | Now |
|---|---|---|---|---|
| `PesRestartDelay.AGoalKickIsReleasedOnThePesLadder` | (no goal kick, seed 7, 20 min) | moved | Main's `d344516c` | green |
| `PesRestartDelay.PesLoopRestartsAreArmedAtTheirPesSpotsSidesAndReleaseFrames` | `no goal kick was armed` (seed 7, 20 min) | moved | seed search `{7,3,5,11,13}`, stops when goal kick and throw-in are both armed | green |
| `PesRestartDelay.AReleasedThrowInCarriesItsBallInsideTheTouchline` | `no throw-in was released` (seed 7, 2 min) | moved | seed search `{7,3,13,11}`, stops at first match with a release | green |
| `PesRestartDelay.ABallRestingPastTheTouchlineIsJudgedOutWhenTheGateReleases` | `no throw-in was released to place the ball` (seed 7, 2 min) | moved | same search, stops at first armed throw-in | green |
| `PesThrowInRelease.AThrowInIsReleasedFromInsideTheLine` | `no throw-in was released within two minutes` (seed 7) | moved | `kThrowInSeeds {7,3,13,11}`; `RunMatch` takes a seed | green |
| `PesThrowInRelease.ACarriedThrowInIsHeldAtItsRowsKeyOffsetUntilTheTouch` | `carried > 0` (0 vs 0, seed 7) | moved | same | green |
| `PesThrowInRelease.TheLiftTurnsACarriedThrowInTakerToFaceThePitch` | `lifts > 0` (0 vs 0, seed 7) | moved | same | green |
| `PesStall.ALiveBallNeverLiesStillLongEnoughForTheBreaker` | `liveFirings == 0`, got 80 | see §3 | none | **red, attributed** |

Why these were moved, not regressions: seed 7's two-minute match has no throw-in on this tree (first throw-in on the older StoppageFlow tree was at tick ≈15577, `docs/ports/pes-port-StoppageFlow5.md` §3). Each re-staged test found a matching match in the search and passed on it. The search seed that matched was not logged, so I do not name it. Seed 3 leads the throw-in searches because the file's census puts its first walker at tick 2461 (`pes_restart_delay_test.cpp` comment, `SEAMLESS_SEED`). INF until a trace records the matching seed.

Test runs: `pes_restart_delay_test` filtered to the four changed tests: 4 passed (1965 s). `pes_throwin_release_test`: 4 passed (1122 s).

Red-then-green: no production change, so no mutation run. The red state is the failing run on the merged tree, listed above.

## 3. `PesStall.ALiveBallNeverLiesStillLongEnoughForTheBreaker`: attributed, left red

Measured (CONFIRMED): seed 7, two-minute PES-loop match, `liveFirings = 80` (two identical runs: `st2` and the sweep). The StoppageFlow tree recorded 147 on the same seed (`pes-port-StoppageFlow5.md` §2). Main's gate reports this test green on main; I did not re-measure main.

Samples from the breaker's own rate-limited report (`[stalemate]`, 2 lines of the 80 firings):
1. In play, no restart armed (buffer inactive), ball 0.08 m/s at (21.8, -13.4). Forced A7 (id 22) from 0.5 m, movestep 14←3, clip `gkcatch_b00` frame 3/301, function 14. The taker A4 (19) in movestep 12←3.
2. Next interval, ball 0.08 m/s at (-1.1, -4.7), in play, no restart armed. Forced A4 (19) from 2.8 m, movestep 7←1, clip autogen mirror frame 4/25, function 1 (Movement).
- Neither sample shows a TRAP (function 3, PES command 7) on the forced man.

Verdict: not a regression from this branch's code (§1: the stall path is unchanged). The kick fold moved seed 7's trajectory into a loose-ball episode with a presser who does not make contact. That is the class `pes-port-StoppageFlow5.md` §2 attributes to the unported first-touch pickup (TRAP from reaction entry 3, `docs/re/pes-re-first-touch.md` §7.2 gate 4c). CONFIRMED: live, nothing armed, ball at rest, no TRAP in the samples. INF: that this episode is the same mechanism. Two of 80 firings were sampled, and sample 1 is a goalkeeper-style clip (`gkcatch`) that §2 did not record.

The test is not re-stageable: its assertion is the defect detector, and moving the seed would only hide it. Not weakened, not disabled.

**Decision for Main:** hold this branch until the pickup port lands (OwnerPickup, `StoppageFlow`), or disable with a reason as `AGENTS.md` requires. I did not make that call. A per-firing probe (who is forced on each of the 80 firings) would need a temporary `match.cpp` edit; I skipped it, since a rebuild mid-sweep would change the binaries under test. OwnerPickup's branch should run this test on its merged tree.

## 4. Sweep: every `pes_*` binary (96)
Run through `tools/dev/simslot.sh` (slots 2-3 in effect), two lanes, timeouts 1800 s (3600 s for `pes_restart_delay_test`). Each binary printed its gtest summary; none timed out.

- Binaries: 96. Test cases: 1988 ran, 1984 passed, 1 failed, 3 skipped.
- Red: `pes_stall_test` (1 of 2; §3).
- Skipped (environment gates, `GTEST_SKIP`): `EditSaveInstall.DecryptsAndMatchesTheRunningGame` (`PES21_EDIT_SAVE` not set), `StadiumInstall.EveryTeamResolvesItsStoredStadium` (`pes_stadium_test.cpp:236`), `VfsInstall.NestedOriginalsSurviveWithoutDownloads` (`pes_formats_test.cpp:513`). Skip reasons for the last two not read.
- Gate: not run (Main gates). Smoke: not run (the sweep is the test binaries; `gfsim_run` below is the only sim run).

## 5. Determinism hashes (`gfsim_run`, 5-min, seed 7, tree `5c47a082`, sim source unchanged from `a8561ea3`)
Run at Main's request (assignment item 4); the brief's no-`gfsim_run` rule is for agents, so this is recorded as an exception.
- Switch-off (GF tick order, no `--pes-loop`): `be600b677b1de572`, 24040 sim ticks.
- PES loop (`--pes-loop`), run twice: `380554ebded0d3ee` both times, 21123 sim ticks. Same hash on rerun.

Hashes are reported, not tuned. The `[balance*]`, `[idle]` and `[stalemate]` lines in these runs are not acceptance (per the brief).

## 6. Run notes (for the record)
- First stall run was launched without a sim slot; killed after 60 s, nothing kept.
- A broken shell redirect made the first throw-in and stall logs unusable. Stall ran without `PES21_ROOT` and skipped both tests; that run is discarded. Stall figures above come from the valid runs `st2` and the sweep.
- `extern/godot-cpp` symlink swapped for merges and commits and restored after each; `git -c status.submodule=none` used for status.

## 7. Open items
1. `PesStall.ALiveBallNeverLiesStillLongEnoughForTheBreaker`: red until the first-touch pickup port lands (or Main disables it with a reason). Per-firing probe would name who is forced on each firing.
2. Matching seed of each re-staged test not logged; add to the test comment or a trace if it matters.
3. Skip reasons for `StadiumInstall` and `VfsInstall` not read.
