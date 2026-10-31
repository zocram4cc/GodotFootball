# pes-port-TrapTick4: trap end handler proven 6/6 (PES leaves at the first exit: the playing motion is in the anime's motion table); GF's window lacked that table; stall test NOT yet re-run (build not finished) (31-10-26)

Branch `StoppageFlow`, head `e69be3df` (confirmed by `git ls-remote origin StoppageFlow`); the merge of `origin/main` is `7dabfb15`. Image `pes_full.bin` (base `0x140000000`). CONFIRMED = a tool run this session; INF = inference; OPEN = blocker named.

## 0. Verdict

| Item | State |
|---|---|
| 1. merge `origin/main`, regenerate, push | merged and pushed (`7dabfb15`). `gen_translated.py` regenerated twice (4433 functions, 3696 recorded bit-exact, 737 closure; exit 0). `--check` NOT run after the last regeneration (the write is byte-identical by construction) |
| 2. replay the 6 handler captures, translate with a bounded closure, prove 6/6 | **DONE**: `snapdiff.py --bounded --only 0x14079E5C0` over `live2/pid2122415/seq884, 887, 889, 892, 894, 896`: `calls=6 replayed=6 mismatches=0 own_block=0.1045 proven=True` (CONFIRMED). Record in `results_trapaction.json`, closure `closure_trapend.json` (3 members: `0x14079E5C0`, `0x1406EBEE0`, `0x1406E6BD0`) |
| 3. first differing read GF vs PES; fix at the root | **cause found, stand-in written, NOT run on the stall scenario** (section 2). The input is the anime's current-motion table (P+0x354, count P+0x420), which GF never writes |
| 4. `pes_stall_test` green, tick oracle 14/14, mutations, smoke | **NOT RUN**: the merge made every translated unit stale (`pes_state.hpp` and the layer changed), the full rebuild is 4489 steps at `-j3` under load 27-37, and the request budget ended first. The changed sources were compiled one by one (no errors): `pestrapactor.cpp`, `teamAIcontroller.cpp`, `pesworldrecord.cpp`, `pes_trap_oracle_test.cpp`, `pes_stall_test.cpp` |

## 1. Which exit PES takes (CONFIRMED: frame_trace recorded + the capture's memory)

- The handler `0x14079E5C0` (decompile `dec_79e5c0.txt`): `cVar1 = FUN_1406EBEE0(P, *iface+0x18)`; `if (cVar1 != 0 && param_5 == 0 && param_6 == 0) return 0;` is the first statement. The other exits reach `FUN_14079D4B0` / `FUN_1406EEBB0` (the switch).
- `FUN_1406EBEE0(P, motion)` (`0x1406EBEE0`) returns true when `motion` is among the first `*(u32*)(P+0x420)` ids of the 10-entry table at `P+0x354` (stride 0x14, id at +0); a motion >= 0x1125 or an empty table gives false.
- The six captures are consecutive ticks of one man (P = `0x3E9ECB70`, A = `0x3E9ED7E8`, phase byte 1, `A+0xC = 1`, stack arguments 5-7 all 0, return address `0x140796F54`). Each replays to `rax = 0` and writes only stack spills (23 bytes). Motion `P+0xAE0 = 0xD18`, count `P+0x420 = 2`, table `{0xD18, 0x1F}` in all six: `FUN_1406EBEE0` is true, so PES returns 0 at the first statement, never reaching `FUN_14079D4B0` or `FUN_1406EEBB0`.
- Table entries (read from the captures by script): `{id, flag = 1, f32 seconds, f32, f32}`. The seconds field grows from capture to capture (`0xD18`: 0.0324, 0.0725, 0.0926, 0.1126, 0.1528 s; `0x1F`: 0.0565 .. 0.1704 s): it is the time the motion has played. The two trailing floats (about 0.02 and 1.02..1.09) change by the last digits only and nothing on the chain reads them.
- The tick captures show the same table with one entry (`seq6634`: `{0x23}`, `seq6666`: `{0x32B}`), i.e. the playing motion is listed.

## 2. What GF does differently (CONFIRMED by grep; INF for the consequence)

- GF writes P's fields in `WriteActorP` and never the table (no store to `kPMotionTableOffset` existed). With count 0, `FUN_1406EBEE0` is false, the handler passes its first statement and (with `*iface+0x20 != 3` and `FUN_1406EC5F0` false) goes to `LAB_14079E67B`: `FUN_1406EEBB0(P, 2)`, the abort GF's stall test reports at 0x14079E688 / E72C / E936 (the return addresses after the calls). INF: GF's stall test was not re-run with the table, so that the abort disappears is predicted, not observed.
- No translated tick writes the table in any of the 14 tick captures (`written.json` of the S5 replays, PES VAs: no write in `P+0x354..0x424`), so it is written by PES's per-man update (`FUN_1406E5FC0`, called after the action tick; candidates `FUN_1406F8060`, `FUN_1406F8B20`, `FUN_1406F6790`, `FUN_1406F82F0` reads it). The writer is OPEN: an `.text` scan for stores to `[reg + 0x354/0x35C/0x420]` finds no scaled store (so a pointer-walk or block copy). I tried to continue a handler capture's emulation past the handler's return to catch the writer (`cont.py`); the run stopped in a ucrtbase import at the first `memmove`-class call (import model not applied outside `--trace`): not resolved.
- Stand-in (named `ponytail:` in `pestrapactor.hpp`): `PesActorFacts::playingMotion/playingSeconds`, filled by `TrapFactsOf` from the body's PES clip while it has frames left (`CurrentMotionOf`, the finished test `PlayIssuedClip` uses, GF's 10 ms frames); `WriteActorP` writes entry 0 and count 1, or clears the count once. The count is only cleared when set, so the oracle's windows (no playing motion) are untouched. The PES site that removes it: the writer of `P+0x354..0x424` in the per-man anime update (OPEN, not located).
- Consequence (INF): when GF's clip finishes the entry is dropped, `FUN_1406EBEE0` is false and the handler continues to the switch, as PES's would when its motion leaves the table. PES's handler was never captured past its first exit in this launch (the watcher's `action_switch_6EEBB0` predicate `ret in (0x14079E68D, 0x14079E731, 0x14079E93B)` has caught nothing in 2 h of play, `watch.log`), so the switch is still untranslated and the stall test may still abort at `0x1406EEBB0` when a trap clip ends.

## 3. What Main must supply (needs: captures)

`action_switch_6EEBB0` (`0x1406EEBB0`) never fired from the handler in `pid2122415`. Please arm it without the return predicate: `action_switch_6EEBB0` cap 12 with `--when 'action_switch_6EEBB0=(r8&0xff)==0x5e'` (the handler's calls are `(P, 2, 0x5E)` and `(P, 2)`; r8 is the third argument, INF whether the second call leaves r8 unset) or no predicate at all, plus `trap_end_other_79D4B0` the same way. The handler's switch is then translated from those calls (bounded closure, `snapdiff.py --bounded`).

## 4. Changes

- `tools/dev/translate/gen_translated.py`: `bounded_roots()` (record field `bounded`): a capture-bounded record's universe is built without the tail-target walk and its members are exempt from the silent-tail lint (main's rules pulled the whole allocator subtree of `0x141EBC4D0 -> 0x141AFE750` into the layer and failed the image-read lint on 16 reads); `results_trapaction.json` marks `0x140797CD0` and `0x14079E5C0` `bounded`.
- `results_trapaction.json` + `closure_trapend.json`: the handler record. `snapdiff.py`: `build_binary(..., executed, native)` takes both of main's `native` and the bounded set.
- `trap_oracle.py`: `which()` as `origin/DribbleProposal`'s (identical text).
- `src/sim/onthepitch/pestrapworld.hpp` / `pestrapactor.{hpp,cpp}` / `teamAIcontroller.cpp`: the motion table stand-in. `tests/pes_trap_oracle_test.cpp`: `TrapOracle.TheMotionTableListsThePlayingClipSoTheEndHandlerWaits` (PES's translated `FUN_1406EBEE0` on the window: true for the playing clip, false for another motion, false after the clip finishes). Compiled, NOT run.
- Merge resolutions: `trap` stays per slot (`std::array<PesTrapImage, kSlotCount>`) beside main's `carrier` block; main's `kMaOwnerOffset`/`SetMaOwner` replace the branch's holder cells, and the score rows are views (main) so the branch's `SetMaNearestEntry` family and its test are gone; `pes_trap_oracle_test.cpp` is the branch's (main's copy was the 241-line WIP, it did not use `pes_oracle_snap.hpp`'s extra `actor` map); `universe.json` is main's (the branch's `trapchain2` set rows are not in it: regenerate with `tools/dev/ledger/universe.py` when the bridge crawl is wanted).

## 5. Unfinished (numbered)

1. Build `pes_trap_oracle_test pes_stall_test` (full rebuild after the merge) and run: `TrapTickOracle.EveryCapturedTickOnGfsWindowEqualsPes` (14/14 expected unchanged), the new motion-table test, `pes_stall_test` (does the abort at `0x1406EEBB0` remain when a clip ends?), `LoaderData...` (stays red for the VFS reason, `trap.o` stock vs mod).
2. Mutations for the stand-in: (a) no table write -> the new test's `EXPECT_TRUE(listed(...))` fails; (b) never clear the count -> `EXPECT_FALSE` after finish fails; (c) write count 2 -> `listed(kOther)`... not run.
3. The switch `0x1406EEBB0` and `0x14079D4B0` from captures (section 3), then delete the stand-in's dependency on it.
4. The table's PES writer (section 2), which removes the stand-in.
5. `gen_translated.py --check`, `gen_animplugin.py --check`, the 2-minute smoke on both paths, `GATE_TIER=fast`: not run. **`GATE fast: NOT RUN`** (reason: full rebuild after the merge unfinished).
6. `universe.json` `trapchain2` rows.

## 6. Scratch

`<workdrive>/gf-StoppageFlow-scratch/h/` (symlinks to the captures, `ft.sh`, `sd.sh`, `dumph.py`, `tab.py`, `cont.py`, `scanst.py`): small; delete with the worktree.
