# pes-port-PresReplay14: the region allocator and the pool entries are boundary services; the copy tests stay red (round 13)

Branch `Presentation`, pushed `09ff2a631e5a..2e76b84c` (`git ls-remote origin Presentation` = `2e76b84c5fae`). Build dir removed; scratch 25 MB. Labels: CONFIRMED = a test run, gdb, a dump or a file read; INF = inferred; OPEN = not settled.

## 1. What the boundary is (CONFIRMED from the bodies)

- **Region allocator `0x14038E3E0`** (malloc entry; its only caller is the E6B0 wrapper). Inputs: rcx = out slot, rdx = alignment (must be a power of two and at least r9), r8 = size, r9 = the minimum alignment (E6B0 passes 1). Output: rax = 0 with the block at `*out`, 160 for a bad alignment, 8 when no memory. Alignment check: lines 49-60 of the body; out write: line 452 and 875.
- **Pool alloc `0x1403FB410`**: rcx = `&poolId`, rdx = size, r8 = alignment; rax = block or 0 (pool table at `[0x1436EF2C8]`, vtable `+0x10`).
- **Pool free `0x1403FB5B0`**: rcx = `&poolId`, rdx = block; releases through `0x1403FB4F0` (vtable `+8`).
- **Free counterpart on the copy path**: the pool free (and its vtable `+8`, already the native `heap_free`). The copy path's release `14217DE20` calls `1403FB710/FB730` (TLS last-status writers) and virtual slots `+0x28`/`[obj]`; it does not reach `1403FB5B0` in the hit log, so the release is not the pool free on this path. OPEN: which vtable slot frees the palette object.
- **Realloc**: none on the copy path; not modelled.

## 2. Implementation (committed `2e76b84c`)

- `stubs.json` `native_services`: `14038E3E0 region_alloc`, `1403FB410 pool_alloc`, `1403FB5B0 pool_free`, each with `code_hex` (its entry bytes). `stubs.check_boundary_code` refuses a layer built from an image whose entry bytes differ; `gen_translated.build` calls it (flat image reader).
- `xlate`: a call or tail jump to a service goes through the dispatch table (`native_call_lines`); `gen_translated.layer_stubs` passes the services as `do: native`.
- `gen_translated`: services are kept out of the roots, the closure and the members. The layer regenerated: 5871 functions (was 5874).
- `pes_native.cpp`: `pes_native_region_alloc` (PesHeap block, zero contents), `pes_native_pool_alloc` / `pes_native_pool_free` (the heap services; the pool id is not needed: one arena).
- `replay_human.cpp`: the boot window (1 GB calloc, the VirtualAlloc bump cell) and its `boot` member are deleted; the thread window is back to `0x1000` (the TLS slots at `+0x1480` were only the region manager's); the threshold note is replaced by a boundary note; unused includes are dropped.

## 3. Checks

- `pes_native_service_test`: 5/5 (fresh non-overlapping blocks, alignment, out slot, bad alignment 160, exhaustion 8, pool free accepts what pool alloc returned).
- Mutation (region allocator hands back the previous block): `TwoCallsReturnDistinctBlocksThatDoNotOverlap` and `AnExhaustedArenaAnswersNoMemory` fail; reverted.
- `pes_replay_human_test` non-copy: 5/5. `pes_hud_clock_test`: 4/4.
- `test_native_services` (routing test updated for the services), `test_create_path_models`, `test_import_keys`, `test_prove_bounded`: all OK.
- `gen_translated --check`: byte-identical, 5871 functions. `ledger.py --check`: current (44459 rows).
- **Copy tests: still red.** The run now stops loudly (`PES_DISPATCH_UNTRANSLATED 0x14239EF20`), not in a hang.

## 4. Why the copy tests are still red (CONFIRMED, with one OPEN link)

- `14239EF20` is called from `1423A0DE0` on the branch taken when `[r14+0x60] == 0` (the `OUT-OF-SET CALL` at line ~440; `[r14+0x60] != 0` goes to `pes_fn_14239EFD0`, a layer member). `r14` = the first argument of `1423A0DE0`, passed by `1423A2090` from `141AFF8E0`. PES takes the other branch, so PES's object has `+0x60 != 0` when this code runs.
- The layer holds no translation of `14239EF20`: its call site was never reached by a proof record (the layer's gap policy: a callee only a closure holds stays a gap). The port now reaches it because the boundary lets the create path proceed.
- OPEN: where `r14`'s object comes from. If it is the `E3E0` block, its fill matters; if it is a registry object built on the create path, the gap is a missing proof record for that branch.

## 5. A premise that does not hold: zero-fill

- The owner's contract says zero-fill exactly as `E3E0` does. The captured block at `0x1810010` (`live2/pid2402670/seq001`, AB470 entry) holds `0xFF` qwords beyond a short header (`+0x1810018 = 0xFFFFFFC000000000`, `+0x1810020.. = 0xFFFF...`), and `+0x60` of that block is `0xFF` (CONFIRMED). So `E3E0`'s carve does not produce zeros.
- Trial: filling the block with `0xFF` (a PES value from that capture) did not change the copy path's outcome (same `14239EF20` abort). Reverted; the model keeps `PesHeap`'s zero contents, which is not what PES does. OPEN: PES's exact fill and header for a carved block; it needs a capture of a freshly carved block with its header.

## 6. Scaffolding that is now dead, and what is still there

- Deleted: the boot window and its bump cell (only `VirtualAlloc` used them), the `boot` member, the TEB slot window.
- Still there, by design: the TLS array and `gs:[0x58]` (used by `1403FB710/FB730`), the idle-state seeding (the record state the replay reads), the import models (`VirtualAlloc`, `TlsAlloc`, `GetSystemInfo`, `InitializeCriticalSectionAndSpinCount`, `SetLastError`, `getenv`, `timeGetTime`, `VirtualFree`) in `stubs.json` and their tests. Those models are now reached only from layer members that remain from older proof closures (`F230`, `F320`, `AB660`, `AD8A0`, `AC320`, `AC460`, `AC600`, the one-time init `EE80`) — the layer still contains them (5871 functions, a drop of 3 only), because membership comes from the proof closure files, which this round did not regenerate. Pruning them needs new snapdiff closure records (not done). The import models are therefore not yet dead; I did not delete them.

## 7. Process note

- Four lines were appended to `tests/CMakeLists.txt` with a shell `cat >>` (not the edit tool). The content is the registration of `pes_native_service_test`; it is in the commit.

## 8. Ruling and next step

1. Decide `r14`'s origin (a `1423A2090` / `141AFF8E0` trace: which object `[+0x60]` is read from) and whether `14239EF20` belongs to the proven closure. If the branch needs PES's carve state, the next input is a capture of a freshly carved block with its header (the fill question in section 5).
2. Prune the dead region-manager members by regenerating the closure records (snapdiff), then delete the import models that no layer member reaches.
3. The copy tests stay red and are not skipped.
