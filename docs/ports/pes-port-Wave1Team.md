# Wave 1 team/AI line (Wave1Team): report

Branch `Wave1Team`, pushed at `b00949cc` (base `origin/OneWorld-series-wip` `1d93bdb8`; `git ls-remote origin Wave1Team` confirmed). Worktree `<workdrive>/gf-Wave1Team`, godot-cpp symlink restored, `git diff -- src tests` empty.

## Results

| Item | Result |
|---|---|
| T3 nearest-list contract (`tests/sim/pes_nearest_contract_test.cpp`) | red on base (2 of 3 arms), green after the writer fix: 3/3 |
| Mutation M1 (absent rank back to 0xFF) | fails the rank-byte test only |
| Mutation M2 (empty d² set to NaN) | fails the empty-entry test only |
| `pes_scorerows_test` | 12/12 |
| `pes_offball_test` | 81/81 |
| `pes_nearest_opponent_test` | 2/2 |
| Team tick `0x1404F6C30` proof, 6 captures (`pid2998104` seq132, 264, 395, 648, 778, 907) | 6/6 proven: 1 call each, 0 mismatches, own-block 0.7877-0.7933 |
| Selector `0x1404FB2A0` proof, 6 captures (seq024-030) | 6/6 proven: 1 call each, 0 mismatches, own-block 0.4375 |
| Arrival `0x14045E0D0` proof, 6 captures (seq031-113) | 6/6 proven: 1 call each, 0 mismatches, own-block 0.22-0.35 |
| `gen_translated.py --image pes_full.bin --check` | exit 0; 10,150 functions byte-identical (3,824 recorded bit-exact, 6,326 closure). One held-out note, `0x14202BA30` (closure reaches mistranslated `0x14202C250`), predates this change |
| Seed-7, 2-minute smoke (`gfsim_run --seed 7 --minutes 2`, PES_EXE set) | reached full time: 29,807 sim ticks (cap 194,400), rc 0 |
| `PesStall` (`pes_restart_delay_test --gtest_filter=PesStall*`) | 6 pass, 1 fails: `PesStall.ALiveBallNeverLiesStillLongEnoughForTheBreaker` (line 239, `census.liveFirings` expected 0) |
| `PesStall...ALiveBall...` on base `1d93bdb8` writer | **fails the same assertion at the same line**; `liveFirings` 606 (branch: 1366). Pre-existing failure, not introduced here |
| `pes_pass_target_test` | both tests skipped (named-gap skip; "no match ran" without `PES21_ROOT`). Same as base; file untouched |

Gate: not run (Main gates). Binaries run: `pes_nearest_contract_test`, `pes_scorerows_test`, `pes_offball_test`, `pes_nearest_opponent_test`, `pes_restart_delay_test` (PesStall filter), `gfsim_run` (smoke), `gen_translated --check`, and the snapdiff proofs.

## Open item: the breaker's firing count moved

The PesStall test fails on base and on the branch, so it is not a pass-to-fail regression. But the number of live breaker firings moved from 606 (base) to 1366 (branch) on the same test. The only sim-code change on the branch is the nearest-list fix, which the breaker's consumers read through the rank and empty-entry bytes. That is the intended effect, but I did not verify that it explains the increase. Treat the increase as unexplained (INF: the nearest-list change). Next step: compare the breaker's inputs on the two trees for the first tick where the counts diverge.

## T3 detail (CONFIRMED against `docs/re/pes-re-teamtick.md` section 3.1, live)
- Per team an ascending d² list; the k-th entry has `rank[id] = k`; an id not listed has rank byte 10; an empty entry is id 0xFF with d² 0.0; counts at +0xB0/+0xB1.
- Red arms on base: absent rank read `0xFF` (not 10); empty id read `0xFFFFFFFF` and empty d² read NaN.
- The first fix attempt compared `RankOf` against 11 (`kSideSize`), so 10 read as listed. Caught by the consumer suites; corrected to `kRowAbsentRank`.

## Code changed and deleted
- `src/sim/onthepitch/pesscorerows.cpp`: `SortedRow::Clear()` writes PES's fill (id `kNoSlot`, d² 0.0, rank 10) instead of a `0xFF` memset; `RankOf` uses `kRowAbsentRank`; the local `kMaNearestOffset` moved to the header.
- `src/sim/onthepitch/pesscorerows.hpp`: `kRowUnlisted` deleted (dead); `kRowAbsentRank` and `kMaNearestOffset` added.
- `tests/sim/CMakeLists.txt`: one target added. `tests/sim/pes_nearest_contract_test.cpp`: new.

## Not done (blockers)
- **T1 pass-target hold: not written.** The selector's counter B (SR+0x1438) is GF's time-to-ball stand-in. PES's writer `0x14092ED30` (man think) is translated and proven only on `origin/PressCut` (`src/pes/translated/gameplay/pes_fn_14092ED30.cpp`, `closure_press_manthink.json`), not landed. The existing `PesPassTarget` named-gap skip is unchanged.
- **T2 mark words: not written.** Base does not wire the press dispatcher `0x140535C60` (translated in `closure/`, proven only on PressCut). The run words T+0xA2A0[slot] have no GF writer. The test needs the dispatcher run on the window with inputs staged from press captures.
- **Side-record growth: no change, no test.** Base already has `kSideRecordBytes = 0xB900` (`pesworldrecord.hpp:383`), past 0xB760. The read extent of the proven roots is not measured yet.

## Captures and disk
- Registry (`run/capsets.json`): `ai_team_tick_pid2998104`, `teamsel_pid2998104` (6 on disk; the watcher cap was 35), `ai_arrival_pid2998104`, `press_dispatch_pid2998104`. Status `cited`. The press dispatcher set was not replayed (no spec on base).
- Captures were unpacked one at a time with `sudo -n capz.py unpack` and packed back. All 18 replayed captures are packed (`rc=0`).
- Disk fell to 114 GiB free during the batch (floor 300 GiB, not mine alone); 168 GiB after packing. Proof scratch: `<workdrive>/gf-Wave1Team-scratch/proof` (results and logs only).

## Open items
1. The breaker firing count (606 base, 1366 branch): explain or accept (above).
2. T1 and T2: need PressCut's `0x14092ED30` and `0x140535C60` translations, specs and proofs landed or ported.
3. Side-record growth: measure the proven roots' read extent, then add the test the measurement supports.
