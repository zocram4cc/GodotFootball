# PES port CardLevel5: the `--pes-loop` abort was a 64-word style table, not a judge state; FoulSanction merged with AP and SwitchTables (30-10-26)

Branches `FoulSanction` and `CardLevel2Cutover`: same SHA (confirmed by `git ls-remote`, see the end). Worktree `<workdrive>/gf-FoulSanction`, scratch `<workdrive>/gf-FoulSanction-scratch`. CONFIRMED = tool output this session; INF = inference; OPEN = blocker named.

## 1. Merges (CONFIRMED)

| step | result |
|---|---|
| `merge origin/main` (AP, 682b52b7) | hand-resolved: `pesworldrecord.hpp/.cpp`, `referee.cpp`; JSON data files `universe.json` (closures `cardlevel` + `danger` both kept) and `results_core.json` (main's file + this branch's `0x1405B7470`, `0x143F546F0` entries and the `0x14050E770` proof, text-level script `scratch/merge_results_core.py`; main's file has a malformed `}` / `,{` pair at line 98522 that the script normalises); generated `pes_dispatch_gen.hpp`, `translated.hpp` taken from main and regenerated |
| `merge origin/SwitchTables` (8bc504dc) | `xlate.py` (two additive hunks, SwitchTables' side taken: `live_ro` lint, `reaching_defs`); its `sw_tabs` bake is the same one this branch introduced in `b4bcd543`, so it supersedes it; 31 generated files regenerated, not hand-merged |
| layer | `gen_translated.py` 4,367 functions (3,697 recorded bit-exact, 670 closure); `--check` byte-identical after both merges |

### Defects the merge exposed (all fixed on the branch)

1. **The SwitchTables merge dropped the `.impdata` constants bake** (`d4784b07`, CardLevel3). I resolved `xlate.py` through the conflict tool without staging it, then `git checkout --theirs` over the unmerged list took SwitchTables' whole file. Effect: the judge builder's `0x144293D10` (`0x140A81AC0 -> 0x140A81940 -> 0x144293D10`, reached from `0x1405B8000`) read the protector's xor-masked constant `0x153F740B0` live (0 at GF runtime), its copy loop never terminated, and `pes_judge_decide_test` hung on the first foul that reaches the pending-foul builder (gdb: `PesMem::find <- pes_fn_144293D10 <- pes_fn_140A81940 <- pes_fn_140A81AC0 <- pes_fn_1405B8000 <- pes_fn_1405B7470 <- PesWorld::DecideJudge`). Fix: three-way `git merge-file` of `xlate.py` (base `f445018e`, ours `bf66af30`, theirs SwitchTables), `.impdata` logic present again (2 hits), layer regenerated (971 files re-baked: the constants are `static constexpr` again), commit `1e158360`.
2. **Name and layout collisions with main's world window** (`3b51b2da`): main's pitch record F at G+0x2AA8 *is* the sanction's field info (one object; `SetFieldInfo` now writes F0/F1 of `image_->pitch`, the duplicate `fieldInfo` slot and `kFieldInfoPtrOffset` are gone, `competition` G+0x2AD0 kept); main's `teamInfos` are the sanction's team infos (same layout `+0 side, +4 map, +0xA8 players at stride 0x128`; the duplicate record and constants are gone, `SetPlayerInfo`/`BindSanction` use `TeamInfoRecord`/`kSidePlayerInfoOffset`); `kMiFrameOffset` was defined twice (same value 0x1198); `kRequestBytes` collided with `pestrapworld.hpp` (renamed `kSanctionRequestBytes`), `kEventStoreBytes` with `pesevents.hpp` (renamed `kSanctionEventsBytes`). The sanction keeps its own small E (ring 4 plus the zeroed tail, `A1+0xE0`); main's full E is not wired to the judge (OPEN, below).
3. **Ball record**: main's ball record is a 250-sample table whose sample 249 is the old fallback (`+0x2770`, x/y/z; the old `SetBall` wrote x and z there). `Ball` publishes the samples every tick, so the referee's `SetBall` write is deleted (clean cutover) and `pes_judge_decide_test` stages `SetBallSample` for all 250 samples (as `pes_involvement_test` does).
4. `StagePesField` now stores the attack angle as well as the sign (main's `PublishPesStoppage` did both; the loop moved into `StagePesField`, which `PublishPesStoppage` ends with).

## 2. The `--pes-loop` abort (CONFIRMED, reproduced, root cause fixed)

Reproduction: gfsim_run `--seed 7 --minutes 2 --pes-loop` on the merged tree with a temporary `fprintf` before `movestep_run.cpp:2452` (removed again): rc 134, libstdc++ assertion `std::array<int,64>::operator[]`, preceded by

```
REPRO styleGroup index 66 (style 16 band 2) size 64      (scratch/repro_pesloop.log:307)
```

First `[stalemate] 5.0s idle` at log line 180; the abort comes after `[stalemate] 8.0s idle ... forcing H1 (0) from 10.0 m`, i.e. when the forced player runs into the taker.

**Producer**: not a judge state. `RunLoop` arm 5 (the style block of PES `FUN_1406c0170`, `movestep_run.cpp:2437-2452`) takes the style from `JostleStyle`, whose override returns `0x10` ("jostle loop, turn past 157.5 degrees, or past 112.5 with a class under 2"; PES `0x1406c0f..` stores `styleGroup[style*4+band]` = `mov eax,[rdi+rdx*4+0x25ddcf0]` at `0x1406c0f3f`, with no bound). Two players closer than the jostle threshold facing apart reach it; the stalemate puts two together. The 64-word table (`16 styles x 4`, loaded from `0x1425ddcf0`) had no row for style 16. The ladder `FUN_1406ba2f0` returns 0..0xf (`movestep_style.cpp`), so style 16 is the only out-of-range one.

**Fix (PES-faithful data, no clamp)**: `RunTables::styleGroup` holds `kRunStyleCount = 0x11` rows (`std::array<int32_t, 68>`), loaded from the install (`STYLE_GROUP_VA`). Row 16 at `0x1425dddf0` is real data (`2277, 2279, 2285, 2289`, read by script from `pes_full.bin`); `0x1425dde00` (the next table, `RATE_CLASS_VA`) starts right after it, so 17 rows is the table's extent. Nothing else indexes it past 0xF (`movestep_step.cpp:694` takes the ladder's style).

**Red-first**: on the merged tree with `kRunStyleCount = 0x10` the existing `RunLoopJostle.PastOneHundredTwelvePointFiveWithAShortPlayerIsStyleSixteen` already aborts (`Assertion '__n < this->size()'`, `_Nm = 64`), and the added `RunLoopJostle.TheStyleSixteenMotionIsTheTablesRowSixteenAtTheBand` (style 16's motion = synthetic table word `0x10*4 + band`, the band the change stored) cannot pass with 64 words. The install test now loops `kRunStyleCount` styles. Green with 17: `pes_movestep_run_test` 100/100. Mutation: `kRunStyleCount = 0x10` rebuilt and run: aborts in the jostle tests (reverted).

## 3. Tests (CONFIRMED, merged tree)

`pes_judge_decide_test` 7/7, `pes_judge_test` 12/12, `pes_sanction_test` 10/10, `pes_movestep_run_test` 100/100; translator: `test_switch_tables` OK, `test_image_reads` OK, `test_extent` 19, `test_closure_walk` 18, `test_setcc_store` OK.

## 4. Captures replayed

New in `live2/pid1010053`: `seq23596` judge_decide_5B7470 (31 calls), `seq21775` judge_whistle_50E770 (14 calls); `seq028` whistle (1 call, already proven). No judge_commit capture (level >= 2) exists in `pid1010053` or `pid1330921` (the latter holds only kick_chooser, trap_action_tick, trap_kind0).

Snapdiff (`sd.sh`, the merged layer, the capture's recorded call replayed against the translated function): `0x1405B7470` `pid1010053/seq23596` calls=1 replayed=1 mismatches=0 own_block 0.175 proven; `0x14050E770` `pid1010053/seq21775` calls=1 replayed=1 mismatches=0 own_block 0.3623 proven. (A capture's hook count, 31 and 14, is the number of hook entries; the snapshot is the one sampled call.) Neither reaches a level >= 2 commit.

## 5. Smoke and gate

Fast gate (`GATE_TIER=fast GATE_SLOTS="3 2"`, the gate env of the brief) on `3e0dd4f8`: build clean; selection 1,187 changed paths, all 3,763 recorded functions selected (`results.json` 245, `cutscene` 685, `presentation` 546, `core` 2,253, `impure` 34); `smoke-off rc=0`, `smoke-pesloop rc=0` (no assertion in either log); ctest was at 1,097 passed of 2,165 with no failure and the oracle tests skipped (their capture env is not set; not a gate condition) when this report was written. **Interim line: `GATE fast: INCOMPLETE` (ctest still running, ~2 h more at load 33-39); no failure seen so far.**

| smoke (seed 7, 2 min) | rc | `[determinism]` run 1 (gate) | run 2 (rerun, same binary) | main (AP baseline) | `[ticks]` / `[restarts]` |
|---|---|---|---|---|---|
| switch-off | 0 | `2802089ed1b4da32` | `2802089ed1b4da32` | `e663d1e917d6c4f4` | 28,008 / 25 stoppages, longest 23.2 s |
| `--pes-loop` | 0 | `e7c1462426cef558` | `e7c1462426cef558` | `980c54e27f93ced2` | 44,687 / 28 stoppages, longest 23.7 s |

Both deterministic. The hashes differ from main's because the translated judge now decides fouls and cards here (INF; not bisected). The pes-loop run's `[stalemate]` lines (38 s idle at a restart) are the same restart-watch tripwire as before; it reaches full time.

## 6. Open items

1. Card proof (level >= 2): still no `judge_commit` capture with `d(rdx+0x18)>=2`; ask Main to keep the predicate armed (the 49-hook watcher released every commit on "disk under 400 GiB").
2. The judge reads "A made the newest ball event" through A1+0xE0; GF's sanction E (ring 4 only) never holds a ball event. Main's `EventStoreImage` is now in the window; wiring A1+0xE0 to it (and a test per event kind) is the follow-up (M8 of CardLevel4 survives).
3. The `.impdata` bake and the live-read lint share one hazard: a future merge of `xlate.py` must keep `Image.writable` (`.impdata` non-volatile pages bake). `test_image_reads.py` covers read-only reads, nothing covers `.impdata`; a one-case test (synthetic image with an `.impdata` section) would have caught defect 1.
4. Machine load 33-39 and all four sim slots busy for ~50 min: two direct smokes never started; the numbers come from the gate.
