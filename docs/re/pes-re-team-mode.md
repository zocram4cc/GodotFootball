# Team mode (G4 operand), K+0x6E (DHB) and T+0x87CD (2-CF gate): live writers (S1Watch run 2, 19-10-26)

> **Main audit 19-10-26:** `0x141052E30` = `mov eax,[rcx+8]; ret` (a reader, confirmed). Zeroing store `0x140A65E15` `mov [rcx+8],rax` confirmed. The T+0x87CD writer instruction is `mov [r15+rax*4+0x7b21],r10b` at **`0x1405F9E5C`** (`0x1405F9E64` is the next IP, the `cmp r11b,3`); source load `movzx r10d,byte [rbx+1]` at `0x1405F9E2B` and the byte-0 store at `0x1405F9E51` confirmed.

Read-only: `/proc/<pid>/mem` pread (`sudo -n`), `perf` hardware write watches on the four JobGraph worker threads (no stop, no pin, no write), `perf` CPU sampling. No PES memory written. Scratch: `<workdrive>/gf-S1Watch-scratch/` (`intick.py`, `active.py`, `findta.py`, `tids.sh`, `dec_42A4B2.c`, `dis_45B420_full.txt`, `modew*.data`, `srcw.data`, `t87w.data`).

Tags: **CONFIRMED** = live read, hardware-watch capture, raw disassembly or bridge decompile. **INF** = inference. **OPEN** = blocker named.

## 0. Summary

1. **Premise correction:** `0x141052E30` is a **reader**: `return *(u32*)(p+8)` (bridge decompile, CONFIRMED). The G4 operand `p` is `ctx[6]` of the carrier-unit context, not the creator object `M = *(0x1436F3F78)`. `M+8` (`0x32E5D5F8`) reads 29, with 57/58 transients in one run. It is **not** the G4 mode.
2. **Live G4 mode = 0** in every in-tick sample taken (9,871 samples over 15 s). The only store to the mode slot observed in 20 s is a **zeroing** store, so the window never reaches `{3,4}`. Nonzero writers: **not observed** (OPEN; needs an attack/OneTwo window).
3. **T+0x87CD (2-CF byte) is written by `0x1405F9E64`** (`mov %r10b,0x7b21(%r15,%rax,4)`), inside the function that contains the loop at `0x1405F9E20..E68`. The source byte is `[rbx+1]` of a 0x68-byte record. In the sampled window the byte is 1 (side 0) and 3 (side 1), never 2, so the 2-CF gate is not armed. **The source record pointer is OPEN.**
4. **K+0x6E (DHB): not reached this run.** The K instance was not located live. Plan in §4.

## 1. G4 mode: operand chain (CONFIRMED except where noted)

- Reader: `FUN_141052E30(p)` returns `*(p+8)`.
- Context: `FUN_1404566A0(ctx, S, T)` (bridge decompile) sets `ctx[6] = *(S+0x68)`. The call site in `FUN_1404582C0` is `FUN_1404566A0(local_298, TA+0x538, uVar1)` (dump of the call; decompile line `FUN_1404566a0(local_298,param_1 + 0x538,uVar1)`). So **`ctx[6] = *(TA+0x5A0)`**.
- Live (in-tick, `TA+0x508 = MA` nonzero): `*(TA+0x5A0) = 0x1D44AA14`, and `+8` of it is 0. Outside a tick both are 0 (`intick.py`: 9,871 in-tick mode reads, all 0).
- The buffer `0x1D44AA14` is a **per-frame snapshot**: its `+8` is written by `0x14042A575` (`movups %xmm0,-0x80(%rcx)` family) from `FUN_14042A4B0` (CONFIRMED: `perf` on `0x1D44AA1C:w`, about 52 hits per second across the four workers; the copy loop is in `dec_42A4B2.c`).
- `FUN_14042A4B0(this)` copies `*(this+0x168)+0x40` (0x23 blocks of 0x80 bytes, `dec_42A4B2.c`) into a buffer returned by `FUN_14042A820(this+0x330)`. Live: `RDI = this = 0x3060B590` (vtable `0x142599D18`), `src = this+0x168 = 0x3714D5D0` (vtable `0x14260D060`). The mode slot is therefore **`src+0x48`**.
- Live `src+0x48`: 0 in 3,704,871 samples over 8 s.

## 2. Writers of the mode slot (CONFIRMED, 20 s watch on `src+0x48`)

| writer VA | instruction | effect | hits (20 s) |
|---|---|---|---|
| `0x140A65E15` (IP after = `0x140A65E19`) | `mov %rax,0x8(%rcx)` with `rcx = src+0x40` | zeroes `src+0x48` | 2,280 |

Function `0x140A65E10`: `xor eax,eax; mov [rcx],rax; mov [rcx+8],rax; andb $0x80,(%rcx)`, i.e. zero a 16-byte record and keep bit 7 of byte 0. Its 15 E8 callers: `0x140427E03, 0x140499A18, 0x1408B14EC, 0x1408B498A, 0x140A58063, 0x140A66CFB, 0x140A676D2, 0x140A67739, 0x140A67755, 0x142164F90, 0x14216733B, 0x142167425, 0x14216EE23, 0x14216F0BC, 0x144062AF4` (E8 scan, CONFIRMED). Which of them zeroes the mode in play: **OPEN**.

Interpretation (INF): in this window the mode slot is reset to 0 at each zeroing call, and no nonzero write happened. G4 is false (`mode ∈ {3,4}` fails). The `{3,4}` writer must run in a window the sample did not cover.

## 3. T+0x87CD (2-CF gate byte): writer (CONFIRMED)

`T = brain+0x6D50` inside the team tick (brain = `H[0x450+8s]`, `H = 0x2C025400`). Live: side 0 `T = 0x1D47DF48`, side 1 `T = 0x1D49C058` (in-tick samples 5,534 and 4,274; `T+0x8C = 1` on both, matching teamtick step 5).

- `T+0x87CD`: side 0 = **1**, side 1 = **3**, constant over 6 s. Never 2 in this window; the 2-CF gate (`0x14050117B CMP [RAX+0x87CD],2`) is false.
- Watch on the aligned word `T0+0x87CC` (`0x1D486714`, 4 bytes) for 20 s: 566 hits, all from two IPs (283 each):
  - `0x1405F9E55`: after `0x1405F9E51 mov %al,(%r15,%rdx,4)`. Address = `B + 0x7B20 + 20·n` with `n = 5` → **`T+0x87CC`** (byte 0 of the record).
  - `0x1405F9E64`: after `0x1405F9E5C mov %r10b,0x7b21(%r15,%rax,4)`. Address = `B + 0x7B21 + 20·n`, `n = 5` → **`T+0x87CD`** (byte 1 of the record).
- Registers at the hit (`perf --user-regs`): `R12 = R15 = 0x1D47EB90` = `B = T+0xC48`; `CX = 4/5`; `BX = record pointer`.
- Byte sources (CONFIRMED asm): `0x1405F9E28 movzbl (%rbx),%eax` (byte 0 → `T+0x87CC`) and `0x1405F9E2B movzbl 0x1(%rbx),%r10d` (byte 1 → `T+0x87CD`); `lea 0x68(%rbx),%rbx` at `0x1405F9E30` (records stride `0x68`, loop `r11 = 0..3`, `cmp $3,%r11b` at `0x1405F9E64`).
- Element index: `n = r8 + 3·r9` with `r8 = r11`, `r9 = byte [r14+0x10C]` (CONFIRMED asm `0x1405F9E34..E5C`). `n = 5` gives `T+0x87CC/87CD`.
- Function: the loop is in the body that starts at `FUN_1405F9CD0` (bridge head; INF that the loop at `0x1405F9E20..E68` belongs to it, the decompile was not read through).

Verdict: **T+0x87CD is written from record byte 1 at `0x1405F9E2B`, by the store at `0x1405F9E64`, once per loop pass with `r11 = 2`, `r9 = 1`.** INF: the same loop writes side 1's byte (its watch was not run). The record's source table and the meaning of byte 1 are **OPEN**.

## 4. K+0x6E (DHB): not done this run (OPEN)

- Static (from `pes-re-lane-inputs.md` §4, not re-checked): `K = ctx[4]` of the DHB unit = `BP+0x78`; the copier `0x140AC0280` writes `K+0x6E ← *(*(param_1+0xC8)+0x90)`.
- Live: `K` was not located (it is a per-carrier object inside the carrier-unit context, built per tick, not a listener/team object). The `ctx[4]` route needs the carrier-unit context pointer, which this run did not resolve.
- Next step: find the carrier-unit context by its vtable or by a hardware watch on the copier's store (`0x140AC0280` `MOVZX/MOV [RBX+0xE6],AL`, §4.2 of lane-inputs), with `RBX` read from the registers (as done for T+0x87CD). Then watch `K+0x6E` for the writer.

## 5. Corrections to earlier docs

- `pes-re-lane-inputs.md` §4 and `pes-re-carrier-helpers.md` line 413 call `0x141052E30` the "team mode int" with writer `*(M+8)`. The reader is correct; the writer is **not** `M+8`. The operand is the per-frame snapshot `ctx[6] = *(TA+0x5A0)`; its `+8` is `src+0x48`, zeroed by `0x140A65E10`.
- `pes-re-carrier-helpers.md` `T+0x87CD`: the 2-CF writer is now pinned to `0x1405F9E64` (§3).

## 6. Open

1. Nonzero writer of `src+0x48` (values 3 and 4 needed for G4). Requires a window with OneTwo or an attack in play; the sampled window had mode 0 throughout.
2. Which E8 caller of `0x140A65E10` runs in play (§2).
3. Source record table for `T+0x87CD` (`BX`, stride `0x68`, byte 1) and the meaning of `[r14+0x10C]`.
4. `K+0x6E`: locate the carrier-unit context and watch the copier's store (§4).
