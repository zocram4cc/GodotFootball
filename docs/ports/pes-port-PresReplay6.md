# pes-port-PresReplay6: IAT copies route to the named models; the copy tests reach the region allocator (round 5, 20-11-26)

Branch `Presentation`, pushed `e575a654..552985d8` (`git ls-remote origin Presentation` = `552985d8666b`). Worktree `<workdrive>/gf-PresReplay2`: build dir removed; no unpacked captures left (scratch 18 MB: scripts and logs). Labels: CONFIRMED = a test run, a live stack, a decompile or the layer; INF = inferred; OPEN = not settled.

## 1. Result

- **Done (ruling): IAT copies route to the named models.** `xlate.ImportKeys`: a call through an absolute slot the import table does not name resolves to the named export whose key (the IAT qword the slot holds) it equals; a key two named slots share names nothing. The `.impdata` copy `0x15B960E60` now routes to `TlsAlloc`, and `0x15B960BA0` to `GetSystemInfo`. `test_import_keys.py`: 6 of 6 green. Mutation M1 (the key lookup returns the first named export) fails 2 tests; reverted.
- **Batched models (each with a code_hex and a unicorn contract test):**
  - `TlsAlloc`: return 0. The captured index `DAT_1437067A0` is 0 in the real capture (CONFIRMED).
  - `GetSystemInfo`: a `sysinfo` kind writing twelve SYSTEM_INFO dwords (Win64 values, processor count 16 = the PES host's, CONFIRMED by `0x143AC3DC8` in `live2/pid2998104/seq567`). The mirror and the code_hex come from one field table (`SYSINFO_FIELDS`).
  - `VirtualAlloc`: a `bump` kind over the boot window. Its cell is the window's first qword (`0x2600000000`); the port writes the first block (`0x2600001000`) into it; each call returns the old cell and adds `rdx`. The constant base model was wrong: a constant base made the region manager retry forever (live stack, CONFIRMED), because successive calls must return disjoint blocks.
  - `VirtualFree`: return 1.
- **Copy tests: still red.** The palette copy runs, the one-time init `0x14038EE80` runs once (the port's extra `Create`-time initialiser calls were removed; they ran the init twice), the import chain is models end to end, and the run stops in a retry loop in the region allocator (section 3). Unfiltered runs time out; they do not abort.
- **Other fixes:** `AnAbsentHumanLeavesTheRecordAlone` expected `R+0x1C = 0`; the PES idle state every real entry holds is `-1` (5 of 5 captures), and an absent human leaves it alone. Corrected. The sequence test's `R+0x21 = 0` after a copy is PES's post-call state (round 5).

## 2. Checks

- `gen_translated --check`: byte-identical, 5874 functions (3769 recorded bit-exact, 2105 closure). Tracked job, after the last regeneration.
- `ledger.py --check`: current (44,459 rows).
- `test_import_keys.py`: 6/6. `test_create_path_models.py`: 11/11 (after M2 reverted). `test_native_services.py`: OK. `test_prove_bounded.py`: OK.
- `pes_replay_human_test`, filter `-*CopiesThePalette*:*ASequence*`: 5/5 green. `pes_hud_clock_test`: 4/4 green.
- Copy tests: red (timeout at the retry loop, not an abort).
- Mutations this round: M1 (ImportKeys first-named fallback): `test_import_keys` 2 failures. M2 (bump advances by the cell, `lea rdx,[rax+rax]`): both bump tests fail. Both reverted; `grep MUTATION` = 0.

## 3. Where the copy test stops (CONFIRMED by live stacks, gdb SIGINT to the inferior)

Stack at the hang (the bump model in place):

`1403B4FC0 ← 1403B4EB0 (retry loop: L_1403B4EE0 … goto on zero) ← 1403B4E00 ← 1403AFB30 ← 1403AF990 ← 1403B07D0 ← 14038EE80 (one-time init, once) ← 14038E3E0 ← 14038E6B0 ← 14037FDA0 ← 1403CF740 ← 1403434D0 ← 14029D370 ← 141AFEB40 ← 141EB6840 ← 14217DB10 ← 14217DEF0`

- `1403B4EB0` calls `1403B4FC0`, which returns 0; the `goto L_1403B4EE0` retries while it is zero. The VirtualAlloc calls before it now return disjoint blocks, so the loop is not a VirtualAlloc retry. The function returns zero for a reason in its own bookkeeping: `1403B4FC0` reads the global `0x14350B340` (xor with its own frame address), the requested size (`gpr8 + gpr1`, `gpr2`), a block-end compare (`gpr0` against `gpr3`), and branches into `1403A6F10`/`1403A6F80`, which the trace does not reach in this run.
- With the constant base (before the bump), `1403B4E00` took its early return or the setup branch depending on alignment; the alignment move was a real fix for that branch (`block & (rdi - 1)`, rdi = 0x400000), but the retry is a separate problem.
- INF: the boot's region state (the first region's header and the `0x14350B340` chain) is what the function expects from PES's earlier boot code, which GF does not run. OPEN: which global or header the retry needs; nothing in the captures holds the boot-time state (captures start mid-game, after the boot).

## 4. Rig request (ONE; for Main to arm)

Hook the boot's first allocator call and its region function, entry only, with the first call of each launch:

- `0x1403B07D0` (threshold init, the boot's first VirtualAlloc caller), `FIRST 1 EVERY 1`, cap 2, FULL.
- `0x1403B4FC0` (region allocation, returns 0 in GF), `FIRST 1 EVERY 1`, cap 6, FULL.

Why: the boot runs once per launch, so a capture from the boot holds the region state GF lacks (`0x14350B340`, the first header). That is the one value that decides the retry. Mid-game captures cannot show it. The hook config is `hooks_for_image`; the existing 61 hooks should not be touched. If the boot cannot be captured, the alternative is to decode the region setup (`1403A6F10`, `1403A6F80`) from the image.

## 5. Open items for Main

1. Rig request above (one hook pair, boot entry).
2. The region retry (section 3): the port's allocator state at boot. Until it is known, the copy tests stay red (they do not abort; they hang inside the retry).
3. `replay_human.cpp` still declares `pes_fn_1403AFBA0`, `pes_fn_1403AC690`, `pes_fn_1403B07D0` (unused since the Create-time calls were removed). Harmless; remove with the next edit to that file.
4. The kernel32/CRT model batch (`TlsAlloc`, `GetSystemInfo`, `VirtualAlloc`, `VirtualFree`, `getenv`, `timeGetTime`, `SetLastError`, `InitializeCriticalSectionAndSpinCount`) is complete for the create path up to the region retry. The survey beyond it is not known: the retry stops the run before any further import.
