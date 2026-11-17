# AnimeLayer7: the anime singleton, reconstructed by meaning

Branch `OneWorld-series-wip`. Owner ruling (17-11-26): PES builds the anime singleton inside its copy-protection shell; GF never runs
that path. GF reconstructs the object from the game code that reads it and from the game's own stores.

Evidence classes: CONFIRMED (disassembly, a captured value, or a decode), INF (inference from the cited readers), OPEN (what blocks it).

## 0. Status

- Committed in this change: the wrapper as a gf-service stub (`0x141AEB7C0`, stubs.json), `pes_service_anime_singleton` (pes_native.cpp,
  builds the object by meaning), and the regenerated caller `FUN_141AE77B0` (one line: the out-of-set call becomes the service call).
  Nothing in committed code reaches the service yet: the caller's new branch needs `sil = cl = 1`, which no committed path passes
  (the boot passes 0).
- Held (uncommitted, in the worktree): the match-load call in `PesFoxWorld::LoadBody` (`Call(pes_fn_141AE77B0, {1, 1, 0, 1, 0})`) and the
  five-argument `PesMemory::Call` it needs. Reason (section 5): with the call, the layer's extra-plugin block runs and reaches 27
  untranslated callees. `AnimeEngine.TheConstructorBuildsAPlayersControlAndItsParts` (green on the committed tree) then aborts with
  `PES_DISPATCH_UNTRANSLATED: 0x14035df70`.
- Reds: `AnimeEngine.TheMatchLoadCreatesTheAnimeSingletonPesHolds` passes with the held call (global set; vtable 0x142AB91C0).
  `AnimeEngine.APlayersLayerHoldsTheElevenPluginsPesBuildsAtTheMatchLoad` aborts at `0x14035df70` (section 5). Both stay red on the
  committed tree.
- State diff: not yet a verdict (section 6). The comparison point is the ConstructAnime capture (`anime_ctor_141EB5250`, armed by Main).

## 1. The creation path (CONFIRMED)

- The match load calls `FUN_141AE77B0` with rcx = 1, dl = 1, r8 = 0, r9 = 1 and the fifth argument 0 (caller `0x141FE4A30`, call at
  `0x141FE4AB3`; `mov r9b,1` at `0x141FE4AA6`). The boot calls it with rcx = 0 and never reaches the singleton.
- In `FUN_141AE77B0`, `bpl = r9b` (`0x141AE77CC`). With `sil = cl = 1`, the branch at `0x141AE7832` calls the wrapper `0x141AEB7C0`.
  After it, the getter (`0x141AE783E`) and the layer's gate OR bit 3 into `+0x2C0` (`0x141AE7848`, `bpl = 1`). The second OR
  (`0x141AE78AD`) needs `edi != 0` and is skipped.
- The wrapper guards on the global `0x1437CEA40` (`0x141AEB7D1`), allocates `0x2E0` bytes (`0x141AEB7E6`) and calls the setter
  `0x141AEA910`, which publishes the global at `0x141AEACB7`.

## 2. What the setter does that GF reproduces (CONFIRMED by the setter's disassembly)

On the setter's own path (its Steam-dispatched calls and protection thunks are excluded):

- vptr `0x142AB91C0` at `+0x00` (`0x141AEA935`); count cell `+0x10` = 0 (`0x1403DB2B0`); `+0x18` = 0.
- Slots `+0x20`, `+0x30`..`+0x58`, `+0x70`, `+0x2D0` start empty. `+0x28` takes the driver record (below).
- Vector at `+0x60` (count), `+0x64` (capacity), `+0x68` (data): two elements reserved (`0x14022C690` at `0x141AEAA3B`), then two null
  elements stored by the loop at `0x141AEAC10`. GF: count 2, capacity 2, data = two zeroed slots.
- `+0x80..+0x17F`: sixteen 16-byte blocks copied from the unit vectors at `0x14253EE50..0x14253EE80` (xmm0..xmm3 = e0..e3, cycled).
- Records at `+0x190`, `+0x1B8`, `+0x1E0`, `+0x208` (stride `0x28`): `0x1418801F0` writes the header `0x30` at `+0`, `+0x18 = 0x80000004`,
  `+0x1C = 0x700`. Each record's `+0x20` is zeroed, except the last: `+0x228 = -1`, `+0x230 = -1`.
- `+0x180`, `+0x188` = 0; `+0x238..+0x258` = 0.
- `+0x260..+0x268`: three dwords zeroed (`0x14184D5C0`). List: `+0x288 = 0`, head `+0x290 = +0x2A0`, `+0x298 = 0`, node `+0x2A0 = +0x288`,
  `+0x2A8 = 0`, `+0x2B0 = 0`, `+0x2B8` dword = 0 (`0x141AEABBA..0x141AEABBE`).
- `+0x2C0` dword = 0 (`0x141AEABC3`); `+0x2C4` = the immediate of the store at `0x141AEABCA` (1/60 s, `0x3C888889`); `+0x2C8` = the
  immediate of the store at `0x141AEABD5` (60.0, `0x42700000`); `+0x2CC` = mode (the wrapper's ecx).
- Driver record at `+0x28` (vptr `0x142AB9188`, `0x141AEB04A`), 0x6D0 bytes (`0x141AEB020`): `+0x8` dword 1, `+0x10` dword 0, `+0x18..+0x38`
  = -1 (five qwords), `+0x40` dword = -1, `+0x6A4..+0x6C4` = -1 (five qwords), `+0x6CC` byte 1. Its zero fill `[+0x44, +0x6A4)` is the
  setter's call `0x1415A1720(+0x44, 0, 0x660)` at `0x141AEB096`, which is Steam-dispatched (INF from its 83 indirect call sites):
  GF models it by its arguments (zeros).
- The immediates in the list above are read from the install at their stores (`pes_native.cpp`: `kTickImmVa`, `kRateImmVa`,
  `kRecordHeaderImmVa`, `kRecordFlagsImmVa`, `kRecordCountImmVa`); their values were checked against the image.

## 3. Reader map: the singleton's fields and what reads them (CONFIRMED by a static dataflow over the image)

Roots: every function that calls the getter `0x141AEBC10` or references the global; closure through singleton-argument callees and
vtable methods: 44 functions, 108 analysed. Meanings are decoded from the readers. The full table is `reader_map.json` (scratch).

| field | meaning (readers) | in-play (pid661338 seq001, mid-play) |
|---|---|---|
| `+0x00` | vptr 0x142AB91C0; vtable slot 0 = 0x141AEB5B0 (read by 0x141AEBE90, 0x141AEBFF0) | vptr matches |
| `+0x10`, `+0x14` | count cell of the refcount helpers (0x1403DB210/240/250/280) | not compared |
| `+0x18` | pointer, written by the match branch (`0x141AE78A9`); read by 0x141AEB240, 0x141AEA260 | 0 |
| `+0x20` | slot; read by 0x141AEB240 and 0x1403EDC90 (reads `+0x30` of its object) | object |
| `+0x28` | driver record: its vtable slot +8 is called from 0x141AECDAC, 0x141AEBE00/E30/E60, 0x141AEBC20; dword `+8` and `+0x10` read; an indexed table at `+0x18` (0x141AEB9A0) | object (vptr 0x142AB9188) |
| `+0x30`, `+0x38`, `+0x40`, `+0x48` | slots filled through `0x141AEB750` and the Steam-dispatched `0x14164FB10`; not read through the getter | objects |
| `+0x50`, `+0x58`, `+0x70` | named records (names inside, e.g. MTR_BILLBOARD, test, 1_anim, xAnim); read by 0x141AEC8A0, 0x141AECA90, 0x141AEC2C0 | objects |
| `+0x60`, `+0x64`, `+0x68` | vector count, capacity, data; written by 0x141AEB680 | 2, 2, heap block |
| `+0x80..+0x17F` | unit-vector table (setter); rewritten at runtime by 0x141AEC8A0 | not identity |
| `+0x188`..`+0x238` | records and flags; 0x141AEC8A0 writes; 0x141B20313 and 0x141B1AE62 read `+0x228..+0x238` | `+0x188` = 0x100000A08 (runtime) |
| `+0x258`, `+0x268` | read by 0x141AF72B6, 0x141B0E69C, 0x1403F7F40 | object; dword |
| `+0x290`, `+0x2A0` | list head and node; 0x141AEBEC0 links and unlinks | heap pointers (runtime) |
| `+0x2B8` | dword read by the helpers 0x1403DB240/250 (INF: a refcount cell) | 0x165 |
| `+0x2C0` | flags: bit 3 gates the layer's extra plugins (0x141AFB090 lines 910, 1504; 0x141AFBA15, 0x141AFC15D); bit 4 read by 0x141B0E1A0 and 0x141B0E250 | `0x8` (bit 3 set, bit 4 clear) |
| `+0x2C4` | time step 1/60 s; read by 0x141B14F90, 0x141B1E792, 0x141B1FE53 | `0x3C888889` |
| `+0x2C8` | 60.0 per second; read by 0x141B0DD50 | `0x42700000` |

## 4. Not reconstructed (not part of GF, or game code not yet translated)

- The Steam-dispatched calls and protection thunks of the setter (`0x1415A1720`, `0x14164FB10` at four sites, and the thunks E1..E5 with
  their 19 edges): excluded (owner ruling 17-11-26).
- `S+0x20`, `S+0x30..S+0x58`, `S+0x70`, the vector's element objects, and the heap copies of the list head and node: built by
  `0x141AEB750`, `0x141AEB810` and `0x14164FB10`. GF leaves them null or as the setter stores them.
- The context global of `0x141861D60` (the setter's tail, `0x141AEACBE..0x141AEAD08`: the word at `+0xA8` = 0x101 at `0x141AEACD5`,
  `0x1418631E0` sets the byte at `+0x113`). Not reconstructed; readers not mapped (OPEN).

## 5. The layer block's untranslated callees (the reason the match-load call is held)

With the call, the layer setup runs its extra-plugin block. Walking the translated layer from `0x141AFB090` and `0x141EB5250`
(`tools/dev/translate/untranslated_callees.py`) reaches 27 untranslated callees, first `0x14035DF70` (called from `0x14029C800:55`):

0x140317720, 0x14035DF70, 0x140388760, 0x141AE8210, 0x141AEF2E0, 0x141AEF800, 0x141AF3D90, 0x141B04830, 0x141B11280, 0x1423977F0,
0x142398720, 0x142398760, 0x1423987A0, 0x142398800, 0x142398950, 0x142398990, 0x142398F00, 0x14239EF20, 0x1423A2770, 0x1423A2B70,
0x1423A3080, 0x1423A30A0, 0x1423AFD20, 0x1423B5FB0, 0x1423B7AD0, 0x1423B8A10, 0x1423CCB10.

Translating them is a closure task (closure.py roots, then gen_translated.py, then one full build). It is not done here.

## 6. State diff (not yet a verdict)

- Method: `diff_singleton.py` (scratch) compares the GF dump (test `ZZDumpSingleton`, scratch-only, removed before this commit) with an
  in-play capture. Pointers are classified by pointee (null, image vtable, inside the same object, heap).
- The only in-play singleton captures so far are mid-play (`pid661338` seq001, `pid855705` seq009), taken after players, layers and
  registrations have run. Comparing them with a fresh reconstruction shows the runtime-changed fields, not reconstruction errors: `+0x20`,
  `+0x30..+0x58`, `+0x68` (heap block), `+0x70`, the `+0x188..+0x250` records and `+0x290`/`+0x2A0`, and the driver's `+0x18..+0x40` and
  `+0x6A0..+0x6C8` words. The verdict needs the ConstructAnime capture (`anime_ctor_141EB5250`, armed by Main), which is the first
  state after the match load. Its `+0x2C0` should already be 0x8: FUN_141AE77B0 returns before any ConstructAnime call. If it shows 0,
  the ordering assumption is wrong and that is a finding.

## 7. Havok sub-object (narrow reconstruction) and status

The lazy object at `[0x1436D0030]` is built by the havok subsystem init, which reaches protection through the `.rdata`-slot thunks
`0x142293C38` and `0x1415A18FA` (`local/pes-havok-stop-AnimeLayer7.md`); the boot does not run that init. The fresh-player path
(ConstructAnime, replayed clean from `caps/pid3199291_001`: 305,365 instructions, no protection event) reads exactly two dwords of
the object, through the lazy getter `0x14029C800` and its only consumer `0x1418632F0`: `sub+0xC` and `sub+0xF0`, where `sub` is
`obj+0x220`. No vtable and no other field is read. `0x1418632F0` stores `sub+0xC` at `this+0xB0` and `sub+0xF0` at `this+0xAC`
(the product test against the constant at `0x14202FB30` picks `sub+0xF0` for this capture).

`BuildHavokSubobject` (`pesboot.cpp`, labelled `ponytail:`) writes those two values, from game code and game data: `sub+0xC` = 1.0f
(the constant `0x14035DF70` stores), `sub+0xF0` = the float of the double 1001/60000 s at `0x143445278`. The state check: both
values are identical in the boot capture (`pid900228` seq001), the ConstructAnime capture, and the in-play and late captures
(`pid661338` seq001/seq005, `pid855705` seq015). No pointer is read, so no pointee class needs checking.

Status: the match-load call (`pesfoxworld.cpp`) is restored and the three tests pass (`APlayersLayerHoldsTheElevenPluginsPesBuildsAtTheMatchLoad`,
`TheMatchLoadCreatesTheAnimeSingletonPesHolds`, `TheConstructorBuildsAPlayersControlAndItsParts`). The rest of the suite passes (13 tests)
except the steering red `AnimeEngine.ASeriesTurnsMoreThanItsFirstMemberAlone`, which aborts with `PES_DISPATCH_UNTRANSLATED: 0x1423B2340`
(kind-2 head, item 4): it was already excluded from the earlier committed runs.

Open: items 2–4 (`+0x94`, the L2 bit, the kind-2 head `0x1423B2340`) and the steering red. The static protection closure under-counts
`.rdata`-slot thunks (`closure_check.py`); `closure_check2.py` lists them as candidates (946 in the havok closure, mostly imports), and
the replay (`root_check2.py`) decides.
