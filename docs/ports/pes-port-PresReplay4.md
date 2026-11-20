# pes-port-PresReplay4: the create path's imports through the translator's import models (round 3, 20-11-26)

Branch `Presentation`, worktree `<workdrive>/gf-PresReplay2`. Base `2c347d21` (round 2). Result: the two copy tests are **still red**; the route landed for four of the five imports; the fifth is a `VirtualAlloc` branch that the captured PES calls never take and that I could not explain in budget. Labels: CONFIRMED = read from the code, a decompile, a debugger trace or a test run; INF = inferred; OPEN = not settled.

## 1. Result

- The kernel32 native (`kernel32_cs_init`) is removed everywhere. Its replacement is an import model in the translator's own table (`import_stubs.json`), mirrored in `xlate.import_model_lines` (ruling 1).
- Four models added with `code_hex`: `InitializeCriticalSectionAndSpinCount` (new `do`: `cs_init`), `SetLastError` (existing `setlasterror`), `getenv` (return 0), `timeGetTime` (return 0).
- `test_create_path_models.py`: 6 of 6 green. Mutation M1 (LockCount written as 0 in the cs_init `code_hex`): exactly one failure (`test_fields_of_a_free_single_thread_section`); reverted.
- The copy tests now run past four imports and stop at the fifth: `VirtualAlloc` (`0x14252F6D8`), then `VirtualFree` (`0x14252F6D0`). The survey (`GF_PES_SURVEY`) lists exactly these two (`survey2.txt`).
- **Copy tests: red, not skipped.** Model stubs for `VirtualAlloc` would need a memory model, and the branch that reaches them (section 3) is one the captured PES runs never take. A model that returns NULL would make the test green by sending the allocator down its out-of-memory path, which PES does not run here. I did not do that.

## 2. What the survey found (CONFIRMED)

Survey on the copy test, before the batch (5 keys; the run segfaults after the no-op, so it is a lower bound):

| key | IAT slot | export | caller (GF test binary, `nm`) |
|---|---|---|---|
| `0x6FFFFF9CF314` | `0x14252F440` | kernel32 `InitializeCriticalSectionAndSpinCount` | `pes_fn_1403AB2B0` |
| `0x6FFFFED00840` | `0x1425300C8` | ucrtbase `getenv` | `pes_fn_14038E7B0` |
| `0x6FFFFF9D4E80` | `0x14252F6B8` | kernel32 `SetLastError` | `pes_fn_14038E6B0` |
| `0x6FFFF8F43610` | `0x14252FE88` | winmm `timeGetTime` | `pes_fn_1403E8800` |
| `0x6FFFFF9D10B4` / `0x6FFFFF9D10FC` | `0x14252F6D8` / `0x14252F6D0` | kernel32 `VirtualAlloc` / `VirtualFree` | `pes_fn_1403B4E00` |

After the batch the survey lists only the last two (`VirtualAlloc`, `VirtualFree`), called from `pes_fn_1403B4E00`.

Route check (CONFIRMED): `xlate.emit_branch` calls `import_model_lines` for a `CALL` through a named slot (`import_slots.json`, kind `export`) and falls back to the dispatch lookup when the export has no `do` entry. The five exports had no `do` entry, so their call sites went to the dispatch with the IAT value as the key. Adding the entries was enough; no change to the call-site route was needed.

## 3. The fifth import: why the copy tests stay red (CONFIRMED / INF)

- `pes_fn_1403B4E00` is reached through `14038E3E0 → 1403B00E0 → 1403AFB30 → 1403B4E00`. The branch in `14038E3E0` (line ~76-93) compares its `gpr3` (the caller's `rcx`, via the `14037FDA0` swap and `14038E6B0`) with `0x3F0000000000` (`[0x143AC2868]`) and then `gpr7` with `0x400000000000` (`[0x143AC2888]`). A value above the first bound takes `1403B00E0`.
- The bounds and the globals read at that point are the same in the install and in a real capture (`0x1436DCEE0 = 1`, `0x143AC2868`, `0x143AC2888` checked in `live2/pid2998104/seq567`). So the on-disk `.data` is not the cause.
- In the GF run the size argument reaches the bound. It is the output of the chain `1403CF740` (rounding of a size: `gpr2 = ~(size-1) & (size & ~0xF | 0x10)`), so the size is a huge value computed further up the create chain (INF on where). The harness proof (round 1, `closure_destr14217.json`) contains `14038E3E0` but not `14038EE80`, `1403AB2B0`, `1403AFB30`, `1403B4E00` or `14038E7B0`: the captured PES replays return from `14038E3E0` before the allocator.
- Human index does not change it (tested with 15, as in the captures: still `VirtualAlloc`).
- **OPEN:** why the GF create chain computes that size. Next step: trace the size at `1403434D0` (its caller) in the GF run and compare with the harness's inputs for the same call (the proven closure's replay inputs).

## 4. Changes (all in the branch; committed as listed in section 6)

| file | change |
|---|---|
| `tools/dev/translate/import_stubs.json` | four entries: `InitializeCriticalSectionAndSpinCount` (`cs_init`, code_hex 49 bytes, disassembly checked with capstone), `SetLastError` (`setlasterror`, the RtlSetLastWin32Error bytes; INF for the kernel32 export's own bytes), `getenv` (return 0, `31c0c3`; INF: the name is not decoded), `timeGetTime` (return 0, `31c0c3`; INF: a fixed clock) |
| `tools/dev/translate/xlate.py` | `import_model_lines`: `cs_init` mirror (the code_hex: LockCount -1, the rest 0, SpinCount = edx zero-extended, rax = 1, rcx kept) |
| `tools/dev/translate/stubs.json`, `stubs.py` | the `kernel32_cs_init` native entry and op removed |
| `src/pes/translated/pes_native.{hpp,cpp}` | `pes_native_kernel32_cs_init` removed |
| `src/pes/translated/pes_dispatch_gen.cpp` and 6 closure members | regenerated: the create path's calls now carry the inline models |
| `tools/dev/translate/test_create_path_models.py` | new: contract tests for the four `code_hex` models (unicorn), 6 tests |

## 5. Tests and checks (current tree)

- `test_create_path_models.py`: 6/6 OK. Mutation M1 (cs_init LockCount `ffffffff` → `00000000`): 1 failure (the field test); reverted; 6/6 OK after.
- `pes_replay_human_test` with the filter `-*CopiesThePalette*:*ASequence*`: 5/5 green (the absent-human stride test included).
- `pes_replay_human_test` copy tests: red (`CopiesThePalette`, `ASequence`); unfiltered the binary aborts at `PES_DISPATCH_UNTRANSLATED: 0x6fffff9d10b4` (VirtualAlloc).
- `test_native_services.py`: OK (the native removal is consistent with `KNOWN_NATIVE_OPS`).
- `test_prove_bounded.py`: OK (3/3).
- `pes_hud_clock_test`: 4/4.
- `ledger.py --check`: LEDGER.md current (44,459 rows).
- `gen_translated --check`: see the final message (run as a tracked job after the last regeneration; the import_stubs.json and xlate.py content is the one the layer was built from).
- Mutation M1 is the only mutation this round (ruling: one mutation).

## 6. Commits, cleanup

- Commits and push: see the final message.
- Build dir: removed at the end of the round. Unpacked captures (`cap567`, 4.2 GB) were deleted during the round. Scratch left: 9.7 MB (backups of the translated function, logs, survey files).

## 7. Open items (for Main)

1. **VirtualAlloc/VirtualFree on the copy path (blocker).** Either (a) find why the GF create chain feeds `14038E3E0` a size above `0x3F0000000000` (section 3; the harness never does), and fix the state so the branch matches PES; or (b) if the allocator path is real PES behaviour in some state, a model needs a memory region (the arena), which a `code_hex` model cannot allocate: a design call.
2. **The divergence is a fidelity issue, not a missing import.** The four models are the Win64 contracts and pass their tests, but the copy path that calls them is not one the captured PES replays take. Until (1) is explained, the copy tests would certify a path PES does not run. I did not skip the tests or model `VirtualAlloc` to make them pass.
3. The round-2 "named gap" question (skip or red) is unchanged: they stay red.

## 8. Branch and final checks

- Branch `Presentation`: `2c347d21..317585ac` (`git ls-remote origin Presentation` = `317585acd612`). Commit: "translate: create-path imports as translator import models".
- `gen_translated --check`: byte-identical (5874 functions, 3769 recorded bit-exact, 2105 closure), run as a tracked job after the last regeneration.
- `ledger.py --check`: current (44,459 rows).
- Tests: `test_create_path_models.py` 6/6; `pes_replay_human_test` 5/5 with `-*CopiesThePalette*:*ASequence*` (the two copy tests red); `pes_hud_clock_test` 4/4; `test_native_services.py` OK; `test_prove_bounded.py` 3/3.
- Cleanup: build dir `<workdrive>/gf-PresReplay2/build` removed; unpacked captures deleted; scratch 9.7 MB.
- Gate: not run (Main gates).
