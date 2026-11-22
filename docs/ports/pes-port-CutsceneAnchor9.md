# CutsceneAnchor9 (CutLibrary): the cut library's boot builder 0x141E5C8F0 is proven, translated and run in the selector's world build; the wide test is still RED on T.x, pos.x, pos.z (the fill and the time-scale key are the next blockers)

Branch `CutsceneAnchor`, pushed `4d087301` (`git ls-remote origin CutsceneAnchor` = `4d0873012429a1d3a7307e00b071dd1c1ee10443`, fast-forward from `f6ffbad5`). Labels: CONFIRMED = run or read this round; INF = inference; OPEN = named blocker; SYNTHETIC = a state whose values were chosen over a real capture's memory. Static work only: no live reads, no writes to PES. The Ghidra bridge is down; decompiles came from the headless `dec.sh` (CONFIRMED, output in scratch `dec_cutlib*.txt`).

## 0. Outcome

- Wide test `GoalAnchorKindTwo.WideChannelScorerTakesTheElseArmAndEqualsPes`: still RED, same values as CutsceneAnchor6-8 (CONFIRMED by run, DC 5/100/161): `T[0]` got -41.5502167, want -41.6211472; `pos[0]` got 0, want -41.5502167; `pos[2]` got 0, want 34. T[1], T[2], rotY, bc, radius pass.
- Landed and pushed: step 1 (the builder), proven, translated, run at the world build, with a red-first test and a mutation.
- Not landed: step 2 (the fill: the boundary is decided as far as the decode goes, the loader is not decoded yet) and step 3 (the writer of [T+0x1C] is not found).
- Stopped at the 70% budget line with the blockers named (section 7).

## 1. Step 1: the builder 0x141E5C8F0 (CONFIRMED)

Decompile (`dec_cutlib.txt`):
- If `[0x1437F4808] == 0`: allocate 0x18 bytes (`0x1403FB410`), zero the three words, construct the sub-object at +0x18 (`0x14184D5C0`), `n = 0x141E5DE80()` (returns 0xEA = 234), `0x141E5D330(header, n)` (vector resize to n null slots), store the global, return 1. Else return 0.
- Boot caller: `0x140419D80` calls it at `0x140419D89`; `0x140419D80` is called at `0x140401C25` from `0x140401B50` (CONFIRMED by the call scan of the image; the boot-table entry is INF from the prior round).
- The builder only resizes: the slots are null after it returns (the fill is step 2).

Proof (`tools/dev/translate/results_core.json`, record for 0x141E5C8F0; `closure_cutlib.json`; `results_snapdiff_cutlib.json`):
- SYNTHETIC states from `synth.py` (static `as` spec, over seq321's memory, which is the worldctor capture of pid678718): case `null_library` (global 0) and case `filled_library` (global 55444544, a synthetic non-null value, not a PES pointer).
- `snapdiff --bounded`: 2 of 2 calls replayed, 0 mismatches (return value, every written byte), own-block 11/12 (the allocation-failure arm is not reached: recorded, not gated).
- Real calls: none in any capture (the builder runs at boot, before the captures).
- Closure, translated: `0x1403FB410` (PES allocator, its pool helpers `0x1403FB600`, `0x1403FD830`, `0x1403FDB10`), locks `0x1403FF430`, `0x1403FF490`, memset `0x1415A1720`, `0x14184D5C0`, `0x141E5DE80`, `0x141E5D330` with `0x141E5D270`, teardown helper `0x141E5C7E0`.
- Native services (stubs.json `native_services`, not translated in production): `0x1403FBC10` (heap_alloc) and `0x141B34CB0` (forwarder alloc). The proof ran their translated bodies; the production dispatch does not, so they are not closure members (the same rule as the cutanchor7 closure).

GF wiring (`src/pes/fixdemo_select.cpp`): the page `0x1437F4000` (holds the global; no other static maps it) is mapped; the Impl constructor calls `pes_fn_141E5C8F0` once after the engine singleton, as PES's boot step does. The slots stay empty.

## 2. Step 2: the fill, decoded and bounded (CONFIRMED where marked; the loader is OPEN)

- Name table (CONFIRMED, `0x141E5DE60` / `0x141E5DE80`): 234 records of 16 bytes at `0x142B48CA0 + i*0x10`: `+0` a `char*` cut name, `+8` a factory function pointer. The table is in the image's `.rdata`.
- `0x141E5D560` (CONFIRMED decompile; the fill method): builds a 0x204-byte list object, walks a cut-name list (the list accessors `0x1414A9800`, `0x141030AB0`, `0x1414A98D0`, `0x1414A9900`, `0x1414A98A0`), calls `0x141E5CB40` for each name in batches of at most 0x80, then `0x141E5BA70` with the second global `0x1437F4800` when that is set. Its vtable is `0x142B48C88` slot 0; who installs that vtable into the cut-library object is OPEN (no `lea` or qword reference found in the image).
- `0x141E5CB40` (CONFIRMED decompile): strips the directory and extension from the given name and compares it, case-folded, with the table's names; on a hit whose slot is empty it creates the entry with `0x141E5E0A0`, stores it in the vector, and writes the index to the caller's list.
- `0x141E5E0A0` (CONFIRMED): allocates a 0x18-byte entry (`0x1403FB410`), sets `+0x10 = 0xEA`, calls `0x141E5E210`.
- `0x141E5E210` (CONFIRMED): `rec = 0x141E5DE60(idx)`; `obj = rec->factory()`; `ok = obj->vtable[+0x10](obj, arg3, arg4)`; on success stores `obj` at entry `+8` and the index at `+0x10`. So the entry's `+8` is the loader object, and the node that `0x141E5CA20` returns is the loader's product.
- The factory: `rec+8 = 0x141E649B0` (a thunk to `0x15B35DBD0`, which allocates 0x30 bytes and sets vtable `0x142B502C8`). `0x15B35DBD0` is in `.impdata` (relocated code): per the owner's classification rule it is allocation plus vtable initialisation, which is game logic; the classification is INF until the body is checked against the rule.
- The read: the loader's vtable slot `+0x10` (vtable `0x142B502C8`) does the load; it is NOT decoded yet. The path-based stream initialiser `0x140387C60` (CONFIRMED decompile: stores the path into the stream at `+0x70`/`+0x78`, sets `+0x80..+0x90`, calls `0x14022E4C0`, `0x14028BC70`, `0x140387E10`, `0x140388A80`, `0x140388EA0`) and `0x141E5DF50` (CONFIRMED decompile: the other path reader, which calls `0x140387C60` and then the same record factory) are on the same path.

Boundary for Main (INF until the loader's slot `+0x10` is decoded):
- Native service: the file bytes of a named VFS file, read through `pes::PesVfs::Read` (the reader the FixDemo table decode already uses; `src/pes/vfs.hpp`). That replaces the stream's file read only.
- Translated above it: the stream object, the loader's slot `+0x10`, and the parse into the node of vtable `0x142B54228`.
- The factory thunk `0x15B35DBD0` is a `.impdata` classification case (section 6).

## 3. Step 3: the writer of [T+0x1C] (OPEN, not found)

- `T = [G+0x2C0]` (the object GF calls the time-scale object) holds `T+0x1C = 0x9E` at worldctor (seq321) and `0x71` at the anchor (seq204), from CutsceneAnchor7.
- Static scan (`keyscan.py`, the image's code, capstone): 106 loads of `+0x2C0`; a store with displacement `+0x1C` within 300 instructions after such a load gives 3 hits. All three REJECTED:
  - `0x1411014D3` (function `0x141100D00`): `mov eax, dword [r12+0x2C0]` then `mov dword [rbp+0x1C], eax`: a dword copy of a field of r12, not a load of T; not verified further.
  - `0x14216835A` (function `0x142167E60`): stores into a freshly allocated 0x38-byte object (the allocation at `0x142168334`), not T.
  - `0x142167E60`: `[rbp+0x2C0]` is a shared pointer in the function's own frame (`lea rbp, [rsp-0x358]`), not `G+0x2C0`.
- The cut-start region `0x141F2D000..0x141F3F000` (where `fdc.hpp` says the player's cut map key is placed) holds no store with displacement `+0x1C`.
- INF: `T` is the cut player and `+0x1C` the id of the cut in play (the fdc record's `id` is the player's map key). Unverified.

## 4. The wide test (CONFIRMED by run, unchanged)

- Same 9 EXPECT failures for DC 5/100/161 as CutsceneAnchor6-8. The scene row (A0+8 = 1, PES 0x300D9) and T's key are unchanged, so the T.x check (-41.6211472) is not reached.
- The builder runs in the selector but its slots are empty, so the else arm's lookup `0x141E5D4C0` returns the same 0 as before (the test values match the pre-change run exactly).

## 5. Tests, mutation, checks (CONFIRMED)

- New test `FixDemoSelect.TheCutLibraryHoldsOneEmptySlotPerCutName` (`tests/pes_fixdemo_select_test.cpp`): the global is non-null; `(end - begin) / 8` equals the count `pes_fn_141E5DE80` returns; every slot is 0.
- Mutation M1: the builder call removed (`// MUTATION-CUTLIB`). Rebuilt `pes_fixdemo_select_test`: the new test FAILS (`header != 0`: actual 0). Reverted; rebuilt; `pes_fixdemo_select_test` 7/7 PASS.
- Neighbours (rebuilt and run with `PES21_ROOT=<workdrive>/PES21`, `PES_EXE` = the unpacked image, `GF_DIFF_PYTHON` = the harness venv; `env -u GF_DEBUG_RECEIVE -u GF_DEBUG_ANKLE -u GF_PES_SURVEY`):
  - `pes_fixdemo_select_test` 7/7 (incl. the new test);
  - `pes_registry_test` 7/7;
  - `pes_camera_list_test` 5/5;
  - `pes_proc_camera_test` 6/6;
  - `pes_goal_anchor_test --gtest_filter=-GoalAnchorKindTwo.WideChannel*` 22/22;
  - `pes_goal_anchor_test --gtest_filter=GoalAnchorKindTwo.WideChannel*`: RED, the section 4 values.
- `gen_translated.py --image PES --check`: rc 0, `5758 functions (3765 recorded bit-exact, 1993 closure) regenerate byte-identical` (run twice: before and after the record note fix).
- `tools/dev/ledger.py` regenerated and `--check`: rc 0 (44498 rows).
- The "held out" lines of the regeneration (0x1403BB670, 0x1405549C0, 0x14202BA30: a mistranslated callee in their closure) are pre-existing and unchanged.
- Gate: not run (Main gates). Build: `flock /tmp/gf-build.lock nice -n 19 cmake --build build -j8 --target ...` for the five targets only.

## 6. Owner-rule applications (for Main)

- Standing boundary policy (rule 6): the PES heap allocator `0x1403FB410` and its pool calls `0x1403FBC10`, `0x1403FB5B0`, `0x1403FB860` are native-service boundaries (stubs.json); the builder's other callees are translated.
- `.impdata` classification (17-11-26): `0x15B35DBD0` (the factory thunk `0x141E649B0` jumps to it) allocates 0x30 bytes and sets vtable `0x142B502C8`. It is game logic (an object constructor), but it is not yet checked against the rule (no integrity, licence or environment call in its body: NOT CHECKED). Decode it before translating.
- Synthetic states (`synth.py`): labelled SYNTHETIC in the record; the `filled_library` value is a synthetic non-null, not PES's pointer.

## 7. Open items (numbered; the next blocker for each)

1. Decode the loader's vtable slot `+0x10` (vtable `0x142B502C8`, reached through the factory `0x15B35DBD0`) and its read calls. Settle the VFS boundary (section 2) on that list. Blocks step 2.
2. Find the installer of vtable `0x142B48C88` (the cut-library object whose slot 0 is `0x141E5D560`) and its caller (the cut-name list source). Blocks the fill's entry point.
3. Translate and prove the fill chain under bounded proofs: `0x141E5CB40`, `0x141E5E0A0`, `0x141E5E210`, `0x141E5DE60`, `0x140387C60` and its helpers, the loader slot, `0x141E5DF50`. The real case is seq321's filled vector (234 slots; slot 0x71's record is the anchor's node, seq204): compare the filled record byte for byte. Blocks step 2 and T.x.
4. Classify the `.impdata` factory `0x15B35DBD0` (item 1's thunk) under the owner's rule before translating it.
5. Find the writer of `[T+0x1C]` (0x9E at worldctor, 0x71 at the anchor). The static scan's three hits are rejected (section 3). Settling it needs a write watch on `T+0x1C` in a live match (not done: static only this round). Blocks the key.
6. After items 1-5, re-run the wide test and check T.x = -41.6211472 at the event tick (INF: the scorer's x at the event tick, not verified).

## 8. Files

- Commit `4d087301`: `src/pes/fixdemo_select.cpp`, `tests/pes_fixdemo_select_test.cpp`, `src/pes/translated/{pes_dispatch_gen.cpp,translated.hpp,cutlib/pes_fn_141E5C8F0.cpp,closure/pes_fn_141E5C7E0.cpp,closure/pes_fn_141E5D270.cpp,closure/pes_fn_141E5D330.cpp}`, `tools/dev/diffharness/specs/core/{141E5C7E0,141E5C8F0,141E5D270,141E5D330}.json`, `tools/dev/diffharness/specgen/sets/closure-cutlib.txt`, `tools/dev/translate/{closure_cutlib.json,results_snapdiff_cutlib.json,results_core.json}`, `tools/dev/ledger/{universe.json,history.csv}`, `docs/re/LEDGER.md`.
- Worktree: `<workdrive>/gf-CutsceneAnchor` (clean; the `extern/godot-cpp` symlink was removed only around the commit and restored). `build/` removed. Unpacked capture copies removed (the packed originals stay in `live2/pid678718/seq321`).
- Scratch kept: `<workdrive>/gf-CutsceneAnchor-scratch/cutlib/` (keyscan.py, dec outputs are in the scratch root as `dec_cutlib*.txt`, synth spec, proof logs).
