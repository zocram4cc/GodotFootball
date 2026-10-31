# PES port CardLevel6: AQ merged, the judge's ball-event ring wired to main's event store, `.impdata` bake test; fast gate RED on two ceremony throw-in tests (31-10-26)

Branches `FoulSanction` and `CardLevel2Cutover`: same SHA `d12d2be2` (confirmed by `git ls-remote`). Worktree `<workdrive>/gf-FoulSanction`. CONFIRMED = tool output this session; INF = inference; OPEN = blocker named.

## 1. Merge of origin/main (AQ, `15cc00c5`) (CONFIRMED)

`git merge origin/main` was conflict-free (`fd15e88c`). `gen_translated.py` regenerated the layer: 4,367 functions, **no file changed** (the merged tree was already byte-identical). `gen_translated.py --check`: `4367 functions (3697 recorded bit-exact, 670 closure) regenerate byte-identical`, rc 0. Pushed with the later commits (section 2-3). `docs/re/LEDGER.md` was stale after the merge (the gate's ledger check said RED); regenerated with `tools/dev/ledger.py` (`LEDGER.md current (37971 rows)`, no status regression vs HEAD) in `d12d2be2`.

## 2. Ball-event ring wired to main's event store (CONFIRMED)

Before: A1+0xE0 (the sanction's ANALYZE object) named a private five-ring `PesSanctionImage::events`, which the match never wrote, so the judge's "A made the newest ball event" reads (`0x140A3E190(E,0)` at `0x1405B7932`, `(E,1)` at `0x1405B7A..`, the ring-0 walk at `0x1405B7C..`) saw an empty store (CardLevel4 M8 survived).

Change (`pesworldrecord.cpp/.hpp`, clean cutover): `BindSanction` writes `EventsVa()` (the window's own `EventStoreImage`, the one the match's event stage writes) at A1+0xE0; `PushFoulEvent` pushes into ring 4 of that same store (`EventsVa() + 4*0x1908`); `PesSanctionImage::events`, `kSanctionEventsBytes` and the "E is the last member" static_assert are deleted. The card-count tables at E+0x750D0 / E+0xCE720 now lie in the store's unwritten middle and still answer zero (same ponytail as before).

Tests (`pes_judge_decide_test.cpp`, 9/9; decode of `0x1405B7470` read from the bridge, `judge.c` in scratch):
- `ABlockOnAManWhoJustKickedIsAFoul`: with B's action `0xE` (a block) the contact alone is no foul; with the newest event (E+0x180698, payload+0 = kind 1, 2, 3, 4) made by the victim ({team 1, place 1}, roster identity) it is pending at level 1 (step 3 cases 1..4 set the "victim made the event" byte; step 5's else-if raises 0 to 1); the same event made by the fouler leaves it no foul.
- `TheVictimsOwnShotClearsAFoul`: kind 5 by the victim turns a pending tackle into no foul (case 5 clears it); kind 5 by the fouler leaves it pending.

Mutations (each rebuilt, run, reverted; `git diff` of `pesworldrecord.cpp` shows only the intended change):
| mutation | killed by |
|---|---|
| M9 `A1+0xE0 = 0` | both new tests (7 old tests still pass) |
| M8 judge ctx cell (`judge+0x3B8` target) left zero | both new tests (the 7 old tests survive it, as CardLevel4 reported) |

Other binaries run: `pes_sanction_test` 10/10, `pes_judge_test` 10 passed + 2 skipped (install tests, no `PES21_ROOT` in that shell). Not run by me: kinds 6..14 of the switch (cases 6/7, 8/10..0xE read ring 0's second entry: staged ring-0 records are the next test).

## 3. `.impdata` bake test (CONFIRMED)

`tools/dev/translate/test_image_reads.py` (already ctest `translate_image_reads_test`) gains a synthetic `.impdata` section (rwx flags `0xE0000020`, page `0x1A000`, not in `IMPDATA_VOLATILE_RVA`) and function D (`mov eax,[rip+word]; ret`). `test_a_stable_impdata_word_is_baked_into_the_function` compiles the translated function with g++ and runs it with NO image view; it must return the word (`a1b2c3d4`). 9/9 green. Mutation (monkeypatched `Image.writable` without the `.impdata` clause, i.e. what the dropped merge left): 1 failure, `'0' != 'a1b2c3d4'`. Not covered: a volatile `.impdata` page stays live (needs a 64 MB sparse image).

## 4. Fast gate: RED, two ceremony throw-in tests (OPEN)

Interim line: **`GATE fast: RED (PesRestartDelay.AThrowInCeremonyTakerIsPlacedOutsideTheLineAndReleasedWithTheBallInHand timeout, PesThrowInRelease.ACeremonyThrowInIsReleasedFromTheHandOverTheSpot, ledger fixed in d12d2be2, gate rerun needed)`**.

What the runs showed:
1. **The worktree's `build/` is `-O0 -DNDEBUG`** (predecessor config); main's gate builds Release. At -O0 the pes-loop smoke took ~15 min against main's ~1.6 min and three 20-minute-match tests timed out at 1800 s. Gate run 1 (`build`, -O0) was killed. Run 2: `build-rel` (Release, configured with the gate env), full build 1 h 35, ctest 2,168 tests.
2. Run 2 failures: `diff_pilot/cutscene/presentation/core_recorded` failed in 0.1 s: **I passed a relative build dir** (`build-rel`) and `gate.select.json` is resolved from the ctest cwd (`FileNotFoundError: 'build-rel/gate.select.json'`). Rerun with an absolute path (`tools/dev/gate.sh <workdrive>/gf-FoulSanction/build-rel`). The recorded re-checks themselves have not run on this tree; the selection said all 3,763 recorded functions selected.
3. Real failures (Release): `PesThrowInRelease.ACeremonyThrowInIsReleasedFromTheHandOverTheSpot` fails after 930 s: `no ceremony throw-in was released in any searched two-minute match` (seeds 3, 3, 13, 11). `PesRestartDelay.AThrowInCeremonyTakerIsPlaced...` times out at 1800 s (five 20-minute seeds 7, 3, 5, 11, 13 searched in turn for one ceremony throw-in; the test itself says the first one "moves with every change to the match before it"). Both passed in batch AQ's gate on main (#1133 in 629 s). The translated judge now decides fouls, so matches differ from main's; whether ceremony (path B, `seamlessRestart == false`, `teamAIcontroller.cpp:2063`) throw-ins disappeared or only moved out of these seeds is **not established**: a probe (temporary test counting held/walking/other takers for seeds 3, 5, 7, 11, 13, 17) was queued but every sim slot stayed held by other gates, and I removed it unrun (probe code reverted; `git diff` of the test file is empty). Next step: run that probe (or `AThrowInIsReleasedFromInsideTheLine`-style counting) on seeds 1..20; if some seed has a ceremony throw-in in 2 minutes, put it first in `kThrowInSeeds` (`pes_throwin_release_test.cpp:36`) and `kCeremonySeeds` (`pes_restart_delay_test.cpp:503`); if none does in 20 seeds, that is a regression of the judge cutover (suspect the stoppage state at throw-in: `SeamlessBallGate` / `SeamlessThrowInTaker` inputs) and needs bisecting between `origin/main` and `3e0dd4f8`.
4. Everything else in ctest passed (the log's failure list has only those 6 entries, 4 of them the relative-path artefact).

## 5. Smoke hashes (CONFIRMED)

Seed 7, 2 minutes, gate's smokes on `3e0dd4f8`+ring (Release `build-rel`) and on the -O0 `build`:

| smoke | `[determinism]` -O0 run | Release run | CardLevel5 (-O0, run 1 and 2) | main (AQ baseline) | ticks / stoppages |
|---|---|---|---|---|---|
| switch-off | `2802089ed1b4da32` | `2802089ed1b4da32` | `2802089ed1b4da32` x2 | `e663d1e917d6c4f4` | 28,008 / 25, longest 23.2 s |
| `--pes-loop` | `e7c1462426cef558` | `e7c1462426cef558` | `e7c1462426cef558` x2 | `980c54e27f93ced2` | 44,687 / 28, longest 23.7 s |

Deterministic: the same hash at -O0 and -O3 and across three runs/trees (CardLevel5's two reruns and this session's two gates). The ring rewiring did not move either hash (this tree equals CardLevel5's, which lacked the ring). **Why they moved off main's** (INF, not bisected): the hand-ported judge/card scoring is gone and the translated judge `0x1405B7470` plus the sanction tree decide fouls and cards from PES's records, so the 2-minute match diverges at the first contact the old scorer and the new one call differently.

## 6. Captures replayed

New since CardLevel5 (`live2/`): no `judge_commit` (level >= 2) and no new `judge_decide`. New `judge_whistle_50E770`: `pid1572516/seq055`, `pid2122415/seq792`. `frame_trace.py recorded` rc 0 for both; `snapdiff.py --only 0x14050E770`: `calls=2 replayed=2 mismatches=0 own_block=0.3623 proven=True` (`sd_whistle6.json`). Still no card-level capture: the commit proof (level >= 2) stays open.

## 7. Open items (numbered)

1. Ceremony throw-in tests (section 4.3): decide seeds vs regression; rerun the Release gate with an absolute build dir and put its final line in the next report.
2. Switch cases 6..14 of the judge's event step need staged ring-0 records (the second entry via `0x140A3E190(E,1)`); only cases 1..5 are tested.
3. Card proof (level >= 2): no `judge_commit` capture; keep `--when 'judge_commit_43F546F0=d(rdx+0x18)>=2'` armed.
4. `build/` in this worktree is -O0 (fast unit iteration); gates must use a Release dir (`build-rel`, untracked, can be deleted with the worktree).
5. Machine: all four sim slots were held for hours; two direct probes never started.
