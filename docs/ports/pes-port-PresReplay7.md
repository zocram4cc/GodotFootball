# pes-port-PresReplay7: the region retry is fixed; the copy tests now stop at the allocator's TLS gate (round 6)

Branch `Presentation`, pushed `552985d8..d3b8b5f3` (`git ls-remote origin Presentation` = `d3b8b5f30c02`). Worktree `<workdrive>/gf-PresReplay2`: build dir removed; decompressed captures removed from scratch (the boot captures are zstd-compressed in place, `mem.bin.zst`). Labels: CONFIRMED = a replay of PES's bytes, a live stack, a gdb register dump, or the layer; INF = inferred; OPEN = not settled.

## 1. Result

- **Done: the region retry (the round-5 blocker).** Replaying PES's own bytes from the captured `1403B4EB0` entry (`seq002`: `rcx = rdx = 0x400000`), the first region call now returns, CONFIRMED: `1403B4FC0` returns `0x2600400000`, the block end. The VirtualAlloc call inside it passes `lpAddress = rcx` as a **commit hint** (`rcx = rbx = block end`), and the model returned the bump cell instead (ignoring `rcx`), so `1403B4FC0` returned 0 and `1403B4EB0` retried forever. The model now returns `rcx` when `rcx != 0` (`code_hex` 37 bytes, checked by capstone and by unicorn in the test); the xlate mirror (`do: bump`) matches it register for register.
- **Done: the boot window and TEB.** `replay_human.cpp`: the boot window is 16 MB (the first region `0x7F0000` plus the 4 MB commit at its end). The TEB window was `0x1000` bytes, but the TLS slots sit at `TEB+0x1480`, so every slot access fell outside the window; it is now `0x2000`.
- **Not done: the copy tests.** Both still fail (`CopiesThePaletteEntryWhenTheHumanIsPresent` segfaults in `1403ABDE0`; `ASequence…` was not run to completion since the same path is taken). The blocker is now identified (section 3).
- **Mutation (one):** the pre-hint `code_hex` (`48a1…c3`, ignores `rcx`). `test_create_path_models.py::test_a_commit_hint_returns_itself_and_leaves_the_cell` fails (1 failure, the hint test); the hint code_hex was restored and the 12 model tests are green.

## 2. Checks

- `test_create_path_models.py`: 12/12 green (after the mutation was reverted). `test_import_keys.py`, `test_native_services.py`, `test_prove_bounded.py`: OK.
- `gen_translated --check`: byte-identical, 5874 functions (3769 recorded bit-exact, 2105 closure). Run after the last regeneration (bg_469), as a tracked job.
- `ledger.py --check`: current (44459 rows).
- `pes_replay_human_test`, the five non-copy tests: 5/5 green. `pes_hud_clock_test`: 4/4 green.
- Copy tests: red (SIGSEGV in `pes_fn_1403ABDE0`, rd of `0xFFFFFFFFFFFFFFFF`), not a hang.

## 3. Where the copy test stops now (CONFIRMED)

The one-time allocator init `14038E3E0 → F230 → F320` (the `else` branch of line 733) reads the TLS slot of the current thread:

- `F320`: `idx = dword [0x143AC2814]`; `slot = [gs + 0x1480 + idx*8]`; if `slot == 0`, call `1403AB660` (the list path), else skip it.
- PES: `idx = 10` (`seq008`, CONFIRMED: `0x143AC2814 = 0x0A`), and `TEB+0x1480+0x50 = 0x181CB60` (CONFIRMED, `seq008` at TEB `0x7FC0000`).
- GF port: `idx = 0`, slot 0 = 0 → `AB660` → `ABA40` → `ABDE0`, which reads `[0x14254DED0]` (the static `0xFFFFFFFF` of `docs/re/pes-re-stamina-live.md`) as a list head and faults. A gdb dump shows `rcx = 0xffffffffffffffff` and `rax = 0x14254ded0` at `ABDE0`.
- PES's own replay from the `seq008` state (complete memory, call 100001) does not reach this path: it enters `1403AE320` from `E3E0` with different arguments and completes without a fault (CONFIRMED by a dump replay), so the port's divergence is the TLS state, not the list code.

Sources of the state PES has and the port does not:
- `0x143AC2814 = 10` is written by `pes_fn_143E114E0` from `gpr0`, the result of the process's TLS index allocation (the 11th index in PES; the port's `TlsAlloc` model returns 0 for every call). The value depends on the process's earlier TLS allocations (loader, CRT, other DLLs), which the port does not run. INF.
- Slot 10 (`0x181CB60`) is the per-thread object the `F320` else-branch stores (`s.wr(gs+0x1480+i*8, gpr2)`); it is created by a thread-start path the port does not run. INF.

## 4. Open items and next step

1. **TLS index and per-thread object (the blocker).** Two honest options, neither done: (a) run PES's own TLS setup (`1403B08E0 → 143E114E0`, plus the thread-start code that stores slot 10) inside the port's create path, if it can be reached from the boot code without the loader; (b) a model `TlsAlloc` with PES's process count, which is a captured runtime value and would need a comment saying so. Needs the capture below to choose.
2. **Rig request (ONE; for Main to arm):** hook `0x14038F320` (`F320`, TLS gate) and `0x1403AB660` (list path) at entry, `FIRST 1 EVERY 1`, cap 2, FULL, on the first call of a launch, plus `0x143E114E0` at entry, `FIRST 1 EVERY 1`, cap 2. This shows the TLS index and the slot value at the first gate, which decides (a) or (b).
3. Captures in `live2/pid2998562` are now `mem.bin.zst` (the earlier plain `mem.bin` files were compressed in place). The capture reads this round used decompressed copies in scratch (removed).
4. `replay_human.cpp` still declares three unused `pes_fn_*` prototypes (cosmetic).
