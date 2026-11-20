# pes-port-PresReplay17: the outermost allocator entries are boundary services; the copy tests take pre-state from a capture (still red) (round 16)

Branch `Presentation`, pushed `2e76b84c..2f14f4c4` (`git ls-remote origin Presentation` = `2f14f4c48951`). Build dir removed; decompressed captures removed; scratch 31 MB.

Labels: CONFIRMED = a gdb stack or read, a unicorn replay of PES's bytes on `pid2024691/seq205` (the copy capture), a layer line, or an xref sweep of the image. INF = inferred. OPEN = not settled.

## 1. What changed (committed `2f14f4c4`)

- **Allocator boundary moved outward.** The copy path reaches the region manager through the free entry, not only through the E3E0 allocation: `14022C690 -> 1403CF770 (free) -> 14037FDB0 -> 14038E6F0 -> 14038DF60 -> 14038F320 (TLS slot get-or-create) -> 1403AB660 (region create)` (CONFIRMED by gdb backtrace at `1403AB660` on the port). Two outermost entries are now services, each with a code_hex check (`stubs.json native_services`):
  - `0x1403CF740` `malloc`: rcx = size, rdx = requested class x; alignment `((x & 0xFFFFFFE0) | 0x10) & ~(x-1)` (computed from its bytes); answers the block or 0.
  - `0x1403CF770` `free`: rcx = block; releases GF-arena blocks, ignores null and any pointer the arena did not hand out (PES state the port never allocated). Its rax is not written (the PES callee's return value on the nonzero path is not modelled; OPEN).
- `xlate`: conditional tail jumps to a service go through the dispatch table (as the direct and unconditional forms already did).
- **Pre-state plumbing.** `ReplayHumanFrame::Create(exe, error, pre)` maps caller pages (`PreStatePage`) after the frame's own windows. `tests/pes_replay_human_test.cpp`: `GF_PES_COPY_CAPTURE` names an unpacked capture; `LoadCopyCapture` reads its `maps.txt`, `mem.bin` (via `SnapView`) and `livein.tsv`. `tools/dev/translate/copy_livein.py` writes those two files from a capture (PES-derived, never committed).
- Layer regenerated: 5869 functions (was 5871). `gen_translated --check`: byte-identical.

## 2. The pre-state (CONFIRMED)

- `copy_livein.py` on `pid2024691/seq205` (the only capture that copies): 161 pages the frame reads before writing; the copy sources stored by `1402647D0`: `0x22e504d0`, `0x22e50570`, `0x22e50570` (three xchgs in the frame).
- With the pre-state loaded, the port's `AFF8E0` now reads `[0x1437F4B78] = 0x23e85bd0` and the registry lookup `141B03540` finds the entry (the trace path is the same as PES's up to the create's record read).

## 3. Where the copy tests stand (CONFIRMED)

- Non-copy replay tests: 5/5. HUD clock tests: 4/4. Native service tests: 5/5. Ledger: current (44459 rows).
- `CopiesThePalette` with the seq205 pre-state: no longer faults in the allocator; it now reads address `0x10` (a null field) at `pes_fn_141E96380` line 58, reached `14217DB10 (line 165) -> 141EBF220 -> 141E96700 -> 141E96380`. That is a different create branch from PES's (PES's create goes `14217DB10 -> 141EB6840`), so the port diverges in the create path before the copy. OPEN: which pre-state field the branch tests (`141EBF220`'s predicate) and whether the frame's other inputs (the human record, `kM`/`kU` links the port builds) differ from PES's at that point.
- Test expectations are not yet updated: `CopiesThePalette` still expects `rec.bytes == in.entry` (the synthetic entry), whereas the copy source is the registry's entry (the capture's bytes at `S`). The relation to assert is `rec.bytes ==` the capture's 0xA0 bytes at the first copy source. Not done because the copy does not run to the record yet.

## 4. Step 3c requirement: who writes the pre-state globals (CONFIRMED by an xref sweep of the image, `pes_full.bin`)

- `0x1437F4B78` (the key-record pointer the frame's caller passes into `AFF8E0`; zero in the install's tail):
  - writers: `141EB8290` (stores 0, at `141EB82BE`), `141EB8770` (stores `rsi`, at `141EB8B0C`), `141EB8E80` (stores `rdi`, at `141EB91BE`); the reader `141EB6840` (create caller, reads at `141EB694C` and `141EB6C84`).
  - the callers of `141EB8290/8770/8E80` are not direct calls in the layer (no `call` sites found in translated code): they are reached indirectly (vtable or dispatch). OPEN: their callers up to a recognisable setup (match setup / replay manager init).
- `0x1437CEAA0` (the registry table pointer returned by `141B034B0`, the table of 14 palette entries keyed by `[key record + 0x28]`):
  - writers: `141B01230` (stores `rbx`, at `141B0128C`), `141B01500` (stores 0, at `141B01562`).
  - readers: `141B02800` (`141B0280D`), `141B034B0` (the getter), `141B03640`.
  - OPEN: the callers of `141B01230` and `141B01500`, to the replay manager / palette-load setup.

## 5. Owner's items not done

1. **Copy tests green:** not done (section 3). The next step is the create branch at `141EBF220` (its predicate on the pre-state) against PES's trace of `seq205` at the same point.
2. **Closure regeneration (`closure_destr14217.json`), dead-member pruning, dead import models:** not done. The layer still holds the region members (`F230`, `F320`, `AB660`, `AD8A0`, `AC320`, `AC460`, `AC600`, `EE80`) because the closure file lists them; the regeneration needs a snapdiff proof of `14217DEF0` on `seq205` plus synthetic states, which is blocked on item 1.
3. **One mutation, presentation tests:** not run this round (no mutation of the new services was made; the native service test is unchanged and green).
4. **Ledger, gen --check:** ledger current; gen --check byte-identical (5869 functions).

## 6. Process

- `tests/CMakeLists.txt` is untouched this round.
- Note for Main: the copy tests are red and committed red, as in earlier rounds; they now run to a real divergence rather than aborting.
