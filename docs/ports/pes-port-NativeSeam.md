# PES port: the native heap seam, allocating translated code runs on GF memory (NativeSeam, 29-10-26)

Branch `NativeSeam` (worktree `<workdrive>/gf-NativeSeam`, from `origin/main` 2dfe069e, then `origin/CutsceneSelect` merged: it carries SpecGen, ImportModel and the evaluator/matcher/vector closure, none of which is on main). Gate not run (Main gates). Labels: CONFIRMED = read in tool output (decompile, snapshot read by script, test run); INF = inferred, reason given; OPEN = blocker named.

## 0. Summary

- **The seam exists and is general.** `PesHeap` (`src/pes/translated/pes_native.{hpp,cpp}`) rebuilds PES's allocator object graph in GF memory: the two globals, the 0x32-entry pool table, heap and forwarder pool objects whose vtables hold PES's real method VAs, and an arena. The translated allocator `0x1403FB410`, the free path `0x1403FB5B0`, the trim `0x1403FB860` and every container growth run **unchanged**; the vtable VAs dispatch to `pes_native_heap_{alloc,free,owns,trim}`. The selection is one table, `tools/dev/translate/stubs.json` `"native_services"` (8 VAs), emitted by `gen_dispatch.py` into the committed `pes_dispatch_gen.hpp`. No call site is touched. The harness path is unchanged (§5).
- **Found on the way (CONFIRMED): the committed layer's allocating chains never allocated.** `14047BCA0` (the `vector<u32>` grow) ends in `jmp 0x14047BB10`; the layer lacked `14047BB10`, and xlate emits a silent `return` for an out-of-universe tail jump. On main the evaluator pushed `end += 4` over a null buffer and no allocator call happened (my first test run: `Vec() == {0}`, `Allocs() == 0`, no abort). Fixed in `driver.runnable_extra` (follows tail JMPs past the function body); the layer regenerates with 14 new closure members (§4).
- **Proof:** red test aborts at the allocator dispatch (`PES_DISPATCH_UNTRANSLATED: 0x1403fbc10`); green 9/9 with the native heap; 12/12 mutants killed; recorded real calls (matcher 2/2) equal the translated run with the native heap, 2 allocs / 2 frees / 0 live (§6).

## 1. The allocator, decoded (CONFIRMED: `dec.sh` decompiles + `SnapMem` read of `live2/pid1694737/seq009`)

`FUN_1403FB410(uint* poolId, size, align)`:
- pool id = `*poolId`, 0 is read as 1. Pool object = `*( [0x1436EF2C8] + id*0x40 )` (the global holds the table VA `0x1436EF300`; 0x32 pools, 0x40 bytes each, entry +0 = the pool's heap object; `1403FB860` loops 0x32 times at stride 0x40). A null pool answers 0.
- alignment floor: `size < 0x10`: byte `[0x14258E270 + size]`; else 0x10. `align = max(align, floor)`.
- `rax = pool->vtable[+0x10](pool, size, align)`. If 0 and size != 0: `FUN_1403FB860` (calls `vtable[+0x50](obj, 0, 0)` on every non-null pool: trim), retry, then if `[0x1436EF2B8]` (a handler object, null in the live snapshot) is non-null call its `+0x10`. Returns the result (0 = failure).
- **Free** `FUN_1403FB5B0(uint* poolId, ptr)`: pool id != 0 and non-null pool: `vtable[+8](pool, ptr)`; if that answers true, done; id 0 (or a false answer): scan pools 1..0x31 with `vtable[+0x30](pool, 0)` to find a pool, then `vtable[+8](pool, ptr)`.
- **Live pool objects (snapshot, ids walked by script):** ids 1, 2, 4, 5, 6, 9 -> heap objects (vtable `0x14258E390`; ids 1, 2, 4, 5, 9 share `0x1436EFF80`, id 6 is `0x1437F1000`); ids 0xA..0x28 -> one forwarder object `0x1437F0110` (vtable `0x142AC0090`, `+8` = 1); ids 3, 7, 8 and above 0x28 are null.
- **Heap vtable `0x14258E390`:** `+0` `0x1403FBAC0` (dtor), `+8` `0x1403FBCB0` (free: `if ptr: [flags&5: 0x1403FBFA0]; 0x1403FD350(this+8, ptr)`; returns 1), `+0x10` `0x1403FBC10` (alloc: `0x1403FDB10(this+8, align, size)`, then a guard-header step `0x1403FBEE0` when flags `+0x48` & 5), `+0x30` `0x1403FBD00` (owns: returns 1 when flags bit 0 is clear), `+0x50` `0x1403FBEB0` (loop `0x1403FDD40(this+8, pad)`: trim). The object keeps flags at `+0x48` (0 in the snapshot). `0x1403FDB10` is dlmalloc's `mspace_memalign`/`malloc`: align > 0x10 takes the next power of two >= max(align, 0x20); size wrapping returns 0; blocks are 16-byte aligned (header 0x10).
- **Forwarder vtable `0x142AC0090`:** `+8` `0x141B34D80`, `+0x10` `0x141B34CB0`, `+0x30` `0x141B34C30`: each looks up pool `*(this+8)` (`0x1403FB600(id) = id*0x40 + [0x1436EF2C8]`) and tail-jumps to that pool object's same slot with the arguments unchanged. `+0x50` `0x141B34DB0` and `+0` `0x141B34DF0` (dtor, decoded) are INF by the same pattern (`0x141B34DB0` not decompiled: the live slot VA is CONFIRMED, its body is not).
- **Allocation contract (what PesHeap implements):** `alloc(pool obj, size, align)` -> pointer aligned to `align` rounded up to a power of two (16 minimum; over-aligned requests at least 0x20), or 0 when the heap cannot hold the request; size 0 still yields a block; `free(pool obj, ptr)` answers true; the contents are not specified (dlmalloc returns recycled bytes; PesHeap returns zeros).
- **Heap object used by the FixDemo chain:** the vectors in the evaluator/matcher (`0x14047BCA0`, `0x141F533D0`, `0x14047BB10`) pass pool id 2 on the stack (`auStackX_8[0] = 2`) -> the shared heap object `0x1436EFF80` (CONFIRMED: decompiles of `14047BB10`, `14047BCA0`, `141F533D0`; table entry id 2 from the snapshot). The std::map node path (`141056DF0` -> `140EBF370` -> `1403FB410`) is not exercised here (OPEN, §7).

## 2. Design

| piece | where | what |
|---|---|---|
| `PesHeap` | `src/pes/translated/pes_native.{hpp,cpp}` | Owns a page (`[0x1436EF000, +0x1000)`: `[0x1436EF2B8]` = 0, `[0x1436EF2C8]` = `0x1436EF300`, the table) and an arena at `0x2400000000` (heap object, forwarder object, both vtables, then blocks). `Attach(PesState&)` adds both as `mem.services` windows and sets `s.heap`. Pool occupancy mirrors the live table (1, 2, 4, 5, 6, 9 heap; 0xA..0x28 forwarder; rest null). |
| `PesMem::services` | `pes_state.hpp` | Host windows that win over every source, the snapshot included (`service_at` at the top of `find/wfind/rd/wr`). `window` (the single production window) is unchanged; a chain can have the world window and the heap together. |
| `PesState::heap` | `pes_state.hpp` | The native allocator behind the services windows. |
| natives | `pes_native.cpp` | `pes_native_heap_alloc/free/owns/trim`: registers as PES calls them (rcx = pool object, rdx = size or pointer, r8 = align, answer rax). An unattached state, or a free of a pointer the heap did not give out, aborts loudly (`PES_NATIVE_HEAP: ...`). |
| the table | `stubs.json` `native_services`, `stubs.natives()` | Global stub table, separate key: `stubs.load()` (the harness's view) does not see it. `KNOWN_NATIVE_OPS` is the closed set of ops. |
| emission | `gen_dispatch.py` | `text(vas, native)`: native VAs get `case ...: return {pes_native_<op>}` plus declarations; a VA both translated and native is `SystemExit`. `--native` adds them to a CLI-written header. `gen_translated.py` line 172 passes `gen_dispatch.natives()`; harness writers (`driver.py`, `snapdiff.py` default) pass nothing. |

Approximations (labelled): `// ponytail:` in `pes_native.hpp` (fixed 8 MiB arena, bump + exact-size free lists, no coalescing: raise or replace when a chain keeps more live) and `pes_native.cpp` (over-aligned blocks are not recycled). The zero-fill on reuse and on a fresh arena is a choice (dlmalloc returns recycled bytes); the contract does not promise contents.

## 3. Production use

```
PesHeap heap;                 // GF-owned, lives as long as the chains that allocate
PesState s; heap.Attach(s);   // services windows + s.heap; then the usual window/registers
pes_fn_141F4D2F0(s);          // allocates through PES's own allocator code, in the arena
```
The heap must outlive every pointer the chain stored; containers PES keeps across calls (the per-manager cache `std::map` at `+0x30`, history vectors) live in the arena for the heap's lifetime, so the production owner of such state owns one `PesHeap` per PES world (the wrapper `0x141F4FDA0` item in CutsceneHybrid3 §6). Several PesStates may attach one heap.

## 4. The layer gap (CONFIRMED)

- Cause: `driver.runnable_extra.direct_calls` followed only `CALL`. A chain's tail jump (`jmp 0x14047BB10` at the end of `14047BCA0`) left its target out of the universe and xlate emitted `/*OUT-OF-RANGE JMP 0x14047BB10*/return;`. Fix (`driver.py`): also follow a `JMP imm` whose target is not in the function's own decoded addresses.
- Effect measured: `runnable_extra` of the fixdemo evaluator spec = 14 functions, 0 blocked (`14047BB10`, `1404705B0`, `14047BCA0` in). `gen_translated.py --check` on the unfixed layer: 24 drifts (14 new closure files `1403FB4F0 14047BB10 1408C8FC0 1408C9190 1415A1750 1415A1756 1415A1AD4 1415A1EA0 1415A1EC0 141B34A30 141F4C830 141F4C8C0 141F53000 141F531A0`, 8 changed closure files, `pes_dispatch_gen.hpp`, `translated.hpp`). Regenerated and committed (4,011 functions: 3,683 recorded, 328 closure). Silent `OUT-OF-RANGE JMP` sites in the committed layer (grep): before 29 sites / 23 targets / 21 files, after 21 sites / 16 targets / 13 files. The remaining 16 targets (`141B37CE0`, `14417D0C0`, `1415A17CE`, `1403D2840`, ...) are in sets whose `runnable_extra` this change did not extend, and stay silent returns. **OPEN: a silent `return` for an out-of-universe tail jump is a hazard everywhere it remains; making xlate emit `pes_dispatch_missing` there is TrapChain3's file (told).**
- TrapChain3 regenerated the layer on `StoppageFlow` (4,039 functions); on merge, re-run `gen_translated.py` and take the regenerated files (their reply).

## 5. Harness unchanged (CONFIRMED by construction + test)

`stubs.load()`, `MODEL_MAP`, the driver and snapdiff default dispatch never see `native_services`; the harness builds still translate PES's bytes for the allocator targets (snapdiff's `reached` roots). `test_native_services.py` pins that the harness table has no `pes_native_` and the production table does. `--native` in snapdiff is opt-in.

## 6. Tests and results

| what | result |
|---|---|
| **Red (oracle)**: `tests/pilot/pes_native_heap_test.cpp` against the regenerated layer with a dispatch table built **without** natives (`gen_dispatch.py` no `--native`) | `NativeHeap.EvaluatorPushes...` and `FreeThroughEitherPoolKind...`: `PES_DISPATCH_UNTRANSLATED: 0x1403fbc10`, abort (CONFIRMED). Before the tail-jump fix the same chain did not even reach the dispatch (silent `return`, `Allocs() == 0`). |
| **Green**: same test, production table | 9/9 (`EvaluatorPushesThroughPesAllocatorOntoGfMemory`, `EvaluatorReusesTheVectorsCapacityAndFollowsTheCtx`, `VectorGrowthKeepsElementsAndFreesTheOldBlocks` (200 pushes, `Live()==1`, `Frees()==Allocs()-1`), `EveryOccupiedPoolAllocatesAndTheEmptyOnesAnswerNull`, `FreeThroughEitherPoolKindReturnsTheBlock`, `PoolZeroFreesThroughTheOwnsScan`, `AlignmentSizeZeroAndFailureFollowTheAllocationContract`, `ABlockIsReusedAfterItsFreeAndComesBackZeroed`, death test `FreeingAPointerTheHeapNeverGaveOutAborts`). Built as a standalone closure (13 + 14 layer functions, `g++ -O1`): CONFIRMED. The ctest target (`pestranslated` + gtest) is built separately (§8). |
| expectations | from the decompiles (evaluator `0x141F4D2F0` pushes 0x3F00 / 0x3F01 by `ctx+0x2A22`, resets `end = begin`, flag set = no push; `14047BE40` push_back; allocator pool-0 and empty-pool rules), not from the code |
| `tools/dev/translate/test_native_services.py` | 3/3 (table names known ops and stays out of `stubs.load()`; production vs harness table; clash refused) |
| **Mutations (run, rebuilt, reverted; scratch copy of `pes_native.cpp`, repo untouched)** | M1 over-alignment off, M2 zero size gets no block, M3 no size/wrap guard, M4 no fit guard, M5 no free-list reuse, M6 no zero-on-reuse, M7 free answers 0, M8 owns answers 0, M9 free(null) not a no-op, M10 forwarder range one short, M11 free of an unknown pointer silent, M12 alignment argument ignored. Result: **12 killed**. First run: M1, M3, M12 survived (the test's first block sat on the alignment already; the size guard was shadowed by the fit guard); the test got a misaligning first block, a 0x100 alignment and a wrapping size, and all three then died. |
| **Recorded real calls** (`snapdiff.py --native`, `--vec-arg 2`; snapshots `pid161465/seq003`, `pid407792/seq003`, `live/seq003`, `seq004` (evaluator); `pid161465/seq005`, `pid407792/seq005`, `live/seq006`, `seq007` (matcher); spec dir `RvReplay-scratch/hooks/specs`) | **Evaluator `141F4D2F0`: 4 calls, 3 replayed, native = translated (rax, xmm0, vector contents) 3/3.** These recorded calls return rax 0 with an empty vector and 0 allocations (no condition pushed), so they pin that the native heap changes nothing when no allocation happens; they do not exercise a push (the gtest does). **Matcher `141F4B1D0`: 4 calls, 2 replayed, native = translated on rax and xmm0 2/2 (rax `0x299`, `0x297`), 2 allocs / 2 frees / 0 live per call** (the evaluator's vector inside the matcher allocates and the matcher frees it). Translated = harness is the CutsceneSelect snapdiff result (the same translated run is compared with the harness corpus in the same invocation: `mismatches: []`). Own-block coverage 0.0168 (evaluator), 0.2895 (matcher), recorded not gated. Excluded: 1 + 2 calls, `case excluded: page not captured at 0x6fffffbc73b0` (the slim `live/seq004/006/007` captures; same cause as CutsceneSelect's exclusions). |
| recorded-set checks | see §8 |

## 7. Open items

1. **Evaluator pushes on a recorded real call.** The captured evaluator calls do not push. The matcher's inner calls do, and equal the translated run, but the vector contents are not dumped for the matcher (rax only; the history record write `14125A080` is not compared). A capture of a call that pushes (cond 0x3Fxx path, `ctx+0x2A22`) would close it: the new FixDemo wrapper hook (`pid1792161`, still empty at the time) or any evaluator capture with a push.
2. **std::map node allocation** (`141056DF0` -> `140EBF370` -> `1403FB410`): same allocator, so the seam covers it; no test runs it (the wrapper `0x141F4FDA0` needs captures). The tail-jump fix may add closure members to the wrapper's set when it is regenerated.
3. **Other silent tail-jump returns** (§4) and the xlate change that would make them loud: TrapChain3.
4. **Persistence:** who owns the `PesHeap` of the PES world in production (one per match) is the caller's choice; no GF code allocates through it yet.
5. **Pool 6** is a separate heap object in PES (`0x1437F1000`); the native heap shares one arena for all pools. INF: no flow depends on the separation (no pool-specific state is read by translated code); an allocation of pool 6 is served from the same arena.
6. `0x141B34DB0` (forwarder trim) body not decompiled (§1).

## 8. Results of my runs (CONFIRMED, tool output)

- `cmake --build build --target pes_native_heap_test`: 4020 steps, built (55 min at -j3 under load; the whole layer recompiles because `pes_state.hpp` and the dispatch header changed). `build/tests/pilot/pes_native_heap_test`: **9/9 PASSED** (ctest: 9/9). `translate_native_services` (ctest, `test_native_services.py`): **3/3**. `pes_world_test` 6/6 and `pes_team_sheet_test` 5/5 (the two tests that map GF memory into `PesState`): PASSED.
- `gen_translated.py --check` after regeneration: **4011 functions (3683 recorded bit-exact, 328 closure) regenerate byte-identical**.
- `driver.py --check` recorded sets: **cutscene 685/685 still bit-exact; presentation 546/546 still bit-exact** (the driver change only extends closures; the harness runs PES's bytes). Core (`diff_core_recorded`, 4 h) not run: Main's gate.
- `snapdiff.py --native` on recorded calls: §6.
- `GATE fast (interim): GREEN` for the tests above; not run: the other `pes_*` binaries (they do not read the changed members beyond `PesMem::services` being empty), core set, gfsim smoke (Main gates).

## 9. Not done

- **libm `powf` / `logf` models (Main's request):** not done; budget exhausted by the layer regeneration, the 55-minute layer build and the 40-minute recorded-set checks. Both remain `libm` kind in `stubs.json` with no emitter (`stubs.py` `MODEL_DOS`); the `native_services` mechanism is for dispatch targets, not direct callee models, so they belong with `MODEL_DOS` and `xlate.stub_lines`/`run.py`.
- Merged `origin/main` (bbd5dc7c) into the branch: clean, no layer drift.
