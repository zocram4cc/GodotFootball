# pes-port-PresReplay5: the allocator branch in the create chain (round 4, 20-11-26)

Branch `Presentation`, pushed `317585ac..e575a654` (`git ls-remote origin Presentation` = `e575a6542318`). Worktree `<workdrive>/gf-PresReplay2` (build dir removed; unpacked captures removed; scratch 15 MB). Labels: CONFIRMED = a PES run, a debugger trace, a decompile or a test; INF = inferred; OPEN = not settled.

## 1. Result

- **The decisive GF-supplied state is found and mostly fixed (CONFIRMED).** GF's `.data` is the install's section, which is zero past its raw size (`0x14339B000 + 0x334800 = 0x1436CF800`). PES's allocator one-time init lives in that zero part: `0x1436DCEE0` (flag, PES value 1), `0x143AC2818` (0xE000), `0x143AC2868` (0x3F0000), `0x143AC2888` (0x400000), written by `0x14038EE80`, `0x1403B07D0`, `0x1403AFBA0`, `0x1403AC690`. GF never ran that boot, so the create path ran it on the first copy, with zeroed inputs.
- **The branch is not taken by PES; it is taken by GF's missing boot (CONFIRMED, by a PES experiment).** Zeroing the flag on the PES bytes reproduces GF's call to `0x14038EE80`. Running the boot's initialisers in dependency order (`0x1403AFBA0`, `0x1403AC690`, `0x1403B07D0`) in the port then reaches `VirtualAlloc`, which PES's own boot calls from `0x1403B07D0 → 0x1403AF990 → 0x1403AFB30 → 0x1403B4E00`.
- **Port state now supplied (committed, `e575a654`):** the boot block window (`0x2600000000`, 4 MB, zeroed) with `VirtualAlloc` returning its base; `VirtualFree` TRUE; `TlsAlloc` index 0 (the captured `DAT_1437067A0` is 0 in every real capture); the idle record state every real entry holds (`+0x1C` = -1, `+0x21` = 1; CONFIRMED in 5 of 5 captures).
- **Copy tests: still red.** `CopiesThePalette` and `ASequence` now get through the palette copy and the create chain up to `TlsAlloc` (one import), and then abort at `PES_DISPATCH_UNTRANSLATED: 0x6fffff9d0eec`. The cause is a second IAT copy: `0x15B960E60` in `.impdata` holds the same `TlsAlloc` key (`0x6FFFFF9D0EEC`); `import_slots.json` names only the `.rdata` range, so the model is not routed there. Fixing it needs `import_slots.py` to cover `.impdata` (a snapshot-driven regeneration) or a translator lookup by IAT value. Not done in this round.
- **One mutation (CONFIRMED):** `VirtualAlloc` code_hex base `0x2600000000` → `0x2500000000`: `test_virtual_alloc_returns_the_boot_block` fails (`FAILED (failures=1)`). Reverted; 9/9 green.

## 2. The chain of evidence

### 2.1 PES reference (real bytes, the proof's state)
`run_recorded` on `live2/pid2998104/seq567` with the record-flip (`R+0x50` byte 0), the same state the round-1 proof used (`dump_pes.py`, scratch). 1,888 watched hits, no error. The create chain runs: `0x14217DB10` → `0x1403FB410` (pool alloc, 112 hits) → `0x1403AB...` path → `0x141EB6840 → 0x141AFEB40 → 0x14029D370 → 0x1403434D0 → 0x1403CF740 → 0x14037FDA0 → 0x14038E6B0 → 0x14038E3E0` (308 times, each). PES never reaches `0x14038EE80`, `0x1403B00E0`, `0x1403AFB30`, `0x1403B4E00`.

### 2.2 GF (gdb, the copy test, same entries)
`gf_bps.gdb` (breakpoints on the 20 chain entries, printing rcx/rdx/r8/r9/rbx/rsi/rdi/rax) on the copy test. Argument registers match PES at every chain entry `c1..c8` (rdx, r8, r9, rbx, rdi equal; only stack and heap addresses differ). The first difference is at `c8 → c9`: GF calls `0x14038EE80`.

### 2.3 Where the branch is decided (CONFIRMED by disassembly)
`0x14038E3E0` begins `cmp byte ptr [0x1436DCEE0], 0; jne …` (PES flag 1: skip). If the flag is 0 the function calls `0x14038EE80` (the one-time init). Further in: `cmp rbx, [0x143AC2868]` (= r8 at entry) → `ja` to the big-allocation path `0x1403B00E0`.

### 2.4 The data: install vs runtime (CONFIRMED)
- Install `.data` raw size `0x334800`; the four globals sit beyond it → zero in GF. Captured runtime values (`seq567`) and the unpacked image agree: `0x1436DCEE0 = 1`, `0x143AC2818 = 0xE000`, `0x143AC2868 = 0x3F0000`, `0x143AC2888 = 0x400000`.
- Writers in the layer: `0x14038EE80` (flag, line 411), `0x1403B07D0` (`0x143AC2818`, `0x143AC2868` read), `0x1403AFBA0` (`0x143AC2888`, `0x143AC2878`), `0x1403AC690` (`0x143AC2878` read, `0x143AC2868` written). The bridge (`:8080`) is down; the writers come from the layer.

### 2.5 Experiments on the PES bytes
- Zeroing the callee-saved entry registers (what GF's `Read` leaves zero): no change (1,888 hits, no `0x14038EE80`). So the entry registers are not the cause.
- Flag byte set to 0 at `0x14038E3E0` entry: `0x14038EE80` is entered 308 times (matches GF's `c9`), but `0x1403AFB30` / `0x1403B4E00` are still not reached.
- Human index 15 (as in the captures) in the GF copy test: same path (`VirtualAlloc`). Not the cause.

## 3. Changes (committed in `e575a654`)

| file | change |
|---|---|
| `src/pes/replay_human.cpp` | `Create`: host windows, the boot window (`kBootVa = 0x2600000000`, `kBootBytes = 0x400000`, `Impl::boot`), image/data/stack segments, then PES's initialisers `0x1403AFBA0`, `0x1403AC690`, `0x1403B07D0` in that order. `Read`: `+0x1C` = `0xFFFFFFFF`, `+0x21` = 1 at each entry (the idle state of the 5 captures). |
| `tools/dev/translate/import_stubs.json` | `VirtualAlloc` (return `0x2600000000`, code_hex `48b8…c3`), `VirtualFree` (return 1), `TlsAlloc` (return 0). |
| `tools/dev/translate/test_create_path_models.py` | `BootAllocatorModels`: 3 tests. Total 9 tests, all green. |
| `tests/pes_replay_human_test.cpp` | `ASequence…`: `+0x21` after a copy expected 0 (PES's post-call value; the round-1 `1` was the predecessor's guess). |
| translated layer | regenerated (dispatch + 9 closure members inline the new models). `gen_translated --check`: byte-identical (5874 functions). |

The copy tests' `word1c` expectation (`-1` after a copy) matches PES's post-call state (CONFIRMED: `R+0x1C = 0xFFFFFFFF`).

## 4. Checks

- `gen_translated --check`: byte-identical, 5874 functions (3769 recorded bit-exact, 2105 closure), tracked job after the last regeneration.
- `ledger.py --check`: current (44,459 rows).
- `test_create_path_models.py`: 9/9 OK (after the mutation reverted).
- `pes_replay_human_test`: copy tests red (see 1); the other 5 tests were green in the previous run and their code paths did not change (not re-run after the last build; the binary was removed).
- Presentation tests not re-run this round beyond the above (`pes_hud_clock_test`, `test_native_services.py`, `test_prove_bounded.py` were green in round 3; no change to their code).
- Mutation: 1 (VirtualAlloc base), caught and reverted.

## 5. Open items for Main

1. **`.impdata` IAT copy (blocker for the copy tests).** `0x15B960E60` holds `TlsAlloc`'s key but is not in `import_slots.json`. Options: (a) `import_slots.py` names `.impdata` slots by scanning the snapshot's `.impdata` (the table currently covers the universe's `.rdata` range); (b) the translator resolves an absolute slot whose key equals a named slot's key. (b) is smaller: the key is an image constant the translator already bakes.
2. **Surveys are lower bounds.** After `TlsAlloc`, the next missing key is the `.impdata` one. Further imports may follow (the closure has ~80 import keys; only a run shows which are reached).
3. **The boot is not PES's boot.** GF runs the three threshold initialisers and the model returns a block the port maps. PES's boot calls `VirtualAlloc` for its own region, and the first call's return is an address GF chooses. INF: the allocator's behaviour depends only on the returned block being committed, zeroed and large enough (4 MB); not verified past the copy.
4. The round-3 INF note on `getenv`/`timeGetTime` stays: those models were chosen by the path GF takes, not by a PES capture of the same call.
