# pes-port-TrapTick5: the motion table's writer located (FUN_141EB4650) and its table block ported; the action switch 0x1406EEBB0 proven 9/9 on its same-action exit; pes_stall_test now stops at 0x14081E340 (the real trap -> MOVE transition has no capture) (31-10-26)

Branch `StoppageFlow`, head `de3efaf0` (confirmed by `git ls-remote origin StoppageFlow`; parent `e69be3df`). Image `pes_full.bin` (base `0x140000000`). CONFIRMED = a tool run this session; INF = inference; OPEN = blocker named.

## 0. Verdict

| Item | State |
|---|---|
| 1. full build of `pes_trap_oracle_test`, `pes_stall_test` | built (4492 steps at -j3, ~38 min, twice: the regeneration touches `translated.hpp`, which every unit includes) |
| 2. the PES writer of `P+0x354` / count `P+0x420` | **LOCATED**: `FUN_141EB4650` (section 1). Not translatable as a bounded closure into GF's window (section 2); its table block is ported by hand from PES's instructions, replacing TrapTick4's single-entry stand-in |
| 3. tick oracle, motion test, kind-0 oracle | `TrapTickOracle`: 3 pass (incl. 14/14 `EveryCapturedTickOnGfsWindowEqualsPes`), `LoaderDataTheTickReadsEqualsTheCapture` red (known: `trap.o` stock vs mod); `TrapOracle`: 9/9 pass incl. the new `TheMotionTableAdvancesEachEntryFromTheOldTable` and `TheMotionTableListsThePlayingClipSoTheEndHandlerWaits` |
| 3. `pes_stall_test` | **NOT GREEN**: aborts `PES_DISPATCH_UNTRANSLATED: 0x14081e340` (was `0x1406eebb0` before this work). It reaches the switch's real action change (section 3) |
| 3. the action switch | translated, bounded, proven 9/9 on the only calls the watcher caught (all same-action early exits) |
| 4. mutations | 2 run (section 4); `gen_translated.py --check` and `gen_animplugin.py --check`: see section 5; smoke: section 5 |
| 5. `universe.json` `trapchain2` rows | NOT regenerated: nothing in the commit's checks reads them (see 5) |

## 1. The writer (CONFIRMED: PES's bytes run by unicorn on the live snapshot, not guessed)

- `SetMotion` `FUN_1406F0F70(P, motion, ...)` only stores the request into P (+0x3B50..+0x3B88) and calls the anime interface's vslot +0x10 (`P+0xAB8`; vtable `0x1425E3508`, slot `0x1406EF320`), which stores the motion id at iface+0x28 (`P+0xAE0`) and sets the dirty byte iface+0x2CA6 = `P+0x375E`. `FUN_1406E5FC0` (per-man update, `dec_e5fc0`) then hands the request to the AnimePlayer `*(P+0xA08)` (`FUN_141AF61C0`, `FUN_141AF6E40`) at `0x1406E619A`. None of this stores into `P+0x354`.
- A scan for scaled stores had missed the writer because the stores are `mov [rsi + rcx*4 + 0x354], ebp` with the index computed first. I continued the six-capture machinery past the handler's return: `seq884` (FULL capture, hook `trap_end_handler_79E5C0`, return `0x140796F54`) run with unicorn on its own memory, the host imports modelled (`sinf`/`cosf`/`memmove`/`memset`, thread `SetEvent` as no-ops; scratch script `cont2.py`). The first writes to `P+0x354..0x424` land at `0x141EB474E..0x141EB488E`, 14 stores, **`FUN_141EB4650`** (pdata extent `0x141EB4650..0x141EB49A8`), called from `FUN_1408BE100` (return `0x1408BE1AE`), once per man per tick (22+ calls in the run; callers also `FUN_1409F1BB0`). The run wrote `{0xD18, 0x1F}` into P `0x3E9ECB70`, the same ids as the snapshot's table.
- The function is the anime update: `FUN_141AF4230(*(P+0xA08))`, `FUN_141EB3BD0`, `(*P)[+0x20]`, then **the table block** (`0x141EB46B7..0x141EB4899`), then the root-motion tail (`FUN_141AF7880` etc.). The table block, in my words:
  1. copy the old 10 entries and the old count (`0x141EB46B7..0x141EB4742`, stack `+0x40`); `memset(P+0x354, 0, 0xC8)` and count = 0 (`0x141EB4749`, `0x141EB474E`);
  2. `n = FUN_141AF34B0(player, 1)` (the number of motions in the player's layers); for i < n: id = `FUN_141AF3070(player, i, 1)`, flag byte = `FUN_141AF4160`, seconds = `FUN_141AF3100` (`f32(entry+0xC) * DAT_142AAAFA0`), rate = `FUN_141AF3330`; the advance field starts as `P+0x41C` (the member rate, `0x141EB47E3`), and is replaced by `seconds - old.seconds` when the id is among the old entries (`0x141EB4819..0x141EB4827`);
  3. while count < 10 (`0x141EB4833`): store id (+0, `0x141EB483C`), flag byte (+4, `0x141EB484D`), seconds (+8, `0x141EB4863`), advance (+0xC, `0x141EB4872`), rate (+0x10, `0x141EB4885`), count + 1 (`0x141EB488E`).
- `P+0x41C` is the member rate `[0x14256AE1C] / tickRate` (`ConstructActorP`, 0x141EB4333); in the run it reads `0x3C97B426` = 1/54.

## 2. Why the whole writer is not translated into GF (CONFIRMED sizes, INF consequence)

- One writer call for P is 4,277 instructions over 33 functions (`1403EFEB0 1403F0030 1406AFF30 1406B03D0 1406B0ED0 1406E68C0 1406E6960 1406F82F0 140A581C0 140A58550 14159F3E0 14159FB70 141AF2C20 141AF3070 141AF3100 141AF3190 141AF3330 141AF34B0 141AF4160 141AF4230 141AF4650 141AF7880 141B12620 141B12680 141B12D80 141B19A80 141B2F060 141EB3BD0 141EB4650 141EB8310 141EB9A30 1423A2260` + `1408BE100`), all through the AnimePlayer's layer tables (`*(player+0x88)+0x10`, `+0xF8/+0x100` double-buffered lists, blend arrays) and bone data GF's P window does not hold (`P+0xA08`'s target in GF is a two-cell physique object). No capture hooks the function (the hook list has none), so a bounded closure would be proven on a synthetic call, and then needs GF to build the player's layer tables: the anime engine, not a missing input.
- What GF can feed is the list (`PesActorFacts::playing`: id, flag, seconds, rate; the body's own clip while it has frames left is the one entry, `teamAIcontroller.cpp`). **`WriteActorP` now runs PES's table block on it** (`RebuildMotionTable`, `pestrapactor.cpp`; hand-written from `0x141EB46B7..0x141EB4899`, every term cited): the old-table advance rule, the cleared cells, the member rate. The TrapTick4 single-entry stand-in and `playingMotion/playingSeconds` are deleted. The remaining ponytail names the sites that remove it: `FUN_141AF34B0/3070/4160/3100/3330` over the player's layer tables (writers: `FUN_141AF61C0` SetMotion, `FUN_141AF7A90` engine update).
- Not a translated function: the block is hand-ported semantics, proven by the behavioural test below, not bit-exact against a PES replay (no replayable call exists for the function).

## 3. The action switch `0x1406EEBB0` and why the stall test still stops

- `action_switch_6EEBB0` captures landed (watcher rvwatch28, `(r8&0xff)==0x5e`): `live2/pid2122415/seq26383, 26384, 26385, 26388, 26389, 26391, 26399, 26402, 26411`, all returning to `0x1406EEB7D` (not the handler's return addresses). Every call is `(P, sub = 2 or 3, 0x5E)` with `sub == P+0xAC8` (the man is already in that action): the switch takes the same-action exit (`param_3 == 0x5E`, `0x1406EED96`), rax = 0, writes only stack spills (25..29 bytes).
- Translated with `snapdiff.py --bounded --only 0x1406EEBB0` over the nine: `calls=9 replayed=9 mismatches=0 own_block=0.3864 proven=True` (CONFIRMED). Closure (5): `0x1406D9010 0x1406E66B0 0x1406EEBB0 0x140A6E340 0x141794410` (`closure_trapswitch.json`; record in `results_trapaction.json`, spec `specs/trapaction/1406EEBB0.json`). Layer regenerated: 4434 functions (the end handler's unit `trapaction/pes_fn_14079E5C0.cpp` had not been added to git by TrapTick4; it is in this commit).
- GF's handler call is `(P, 2 = MOVE, 0x5E)` while P+0xAC8 is the Trap id 7 (`trapActive`: the tick owns the action ids). `uVar11 != uVar6` (`sub != current action`) so the switch leaves the early exit and calls `FUN_14081E340(P)` (`0x1406EEBB0` body: then `FUN_1406D9010(P+0xC40, action)` exit vtable +0x28, iface vslot +0x78, `FUN_1406EB440`, enter...). `FUN_14081E340` is outside the bounded closure, so `PES_DISPATCH_UNTRANSLATED: 0x14081e340` aborts (stall test, `gfsim_run --pes-loop` 2 min, both CONFIRMED). **No capture of a real transition exists**: PES leaves the handler at its first statement while the clip plays (6 captures), and the switch captures that landed are same-action calls.
- **Needs (Main):** `action_switch_6EEBB0` with `--when 'b(rcx+0xac8)!=(rdx&0xff)'` (the switch changing the man's action; any action pair covers the shared exit/enter path: rdx = the new action, r8 = 0x5E or any), cap 12, never slimmed; and `trap_end_other_79D4B0` unfiltered. `rcx` is P, `[P+0xAC8]` the current action id (CONFIRMED in the nine captures: 2/2 and 3/3). Then `snapdiff --bounded` over them translates `0x14081E340` and the per-action objects' exit/enter (the table `P+0xC40`), and GF must build that table in P's window (not in it today).

## 4. Tests and mutations (CONFIRMED runs)

- `pes_trap_oracle_test` (`GF_PES_TRAP_TICK_ORACLE=gf-StoppageFlow-scratch/oracle`, `GF_PES_TRAP_ORACLE=gf-TrapOpen-scratch/oracle`): `TrapTickOracle` 3 OK + `LoaderData...` FAILED (known, VFS); `TrapOracle` 9/9 OK (`EachWindowBindingChangesTheCall`, 225 s, passed in the first run).
- New `TrapOracle.TheMotionTableAdvancesEachEntryFromTheOldTable`: a new motion advances by the member rate and carries flag, seconds and rate; the same motion advances by `new - old seconds`; a motion that left the list is gone; empty list zeroes cells and count. `TheMotionTableListsThePlayingClipSoTheEndHandlerWaits` now also drives `playing`/`playingCount`.
- Mutation A (`oldCount = 0`: ignore the old table): `TheMotionTableAdvancesEachEntryFromTheOldTable` FAILED at `pes_trap_oracle_test.cpp:647, 656`; reverted, rebuilt, green. Mutation B (no memset of the cells): FAILED at `:657, :661`; reverted, rebuilt, green. Not run: the member-rate default, the flag byte (covered by the same assertions, not mutated).

## 5. Generator checks, smoke, gate

- `gen_translated.py --check`: `4434 functions (3697 recorded bit-exact, 737 closure) regenerate byte-identical`, rc=0 (CONFIRMED, after the commit's tree). `gen_animplugin.py IMAGE --check src/sim/onthepitch/pesanimplugin_stores.inc`: rc=0 (CONFIRMED).
- 2-min smoke, seed 7 (`gfsim_run`): switch-off `[determinism] 4a350d095284288e` (main's AP baseline `e663d1e917d6c4f4` is for main's head; this branch carries StoppageFlow's other changes, so the two are not comparable and I did not run this branch's pre-change head). `--pes-loop`: aborts at `0x14081e340` in the 2-min match (the same stop as the stall test), so no determinism line.
- **`GATE fast: NOT RUN`** (reason: two 38-minute full rebuilds under load 30 used the time, and the stall test is red for the open reason above, so a fast gate would be red on `pes_stall_test`).
- `universe.json` `trapchain2` rows: not regenerated; the bridge crawl (`tools/dev/ledger/universe.py`) was not run.

## 6. Unfinished (numbered)

1. Captures of the real switch transition (section 3 needs), then translate `0x14081E340` + the per-action exit/enter closure with `snapdiff --bounded`, and give P the action table `P+0xC40` in GF's window; `pes_stall_test` stays red until then.
2. The player's layer tables (`FUN_141AF34B0/3070/4160/3100/3330` inputs): translate `FUN_141EB4650` as a root once the anime engine is in the window, then delete `PesActorFacts::playing`.
3. `GATE fast`, the `pes-loop` smoke line, `universe.json` rows.

## 7. Scratch

`<workdrive>/gf-TrapTick5-scratch/` (`cont2.py`: the continuation emulator with host import models; `scan*.py`; decompiles `dec_w.txt`, `q_*.txt`) and `h/` (snaps symlinks, traces): small; delete with the worktree.
