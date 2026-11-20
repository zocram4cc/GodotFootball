# pes-port-PresReplay9: the size-class table is written; the sentinel-init branch is never taken (round 8)

Branch `Presentation` unchanged this round: last pushed `09ff2a631e5a`. No code change this round (investigation only). Worktree: build dir removed; decompressed capture removed; scratch 21 MB. Labels: CONFIRMED = a gdb hit log, a memory dump or a layer source line; INF = inferred.

## 1. Result

- The copy tests are still red. The sentinel-init writer is `1403AD090` (CONFIRMED static: the only self-link pair in the layer, line 176, `head.next = head.prev = head`). The port never reaches it (CONFIRMED by a hit log over 150 s): `AC320` is entered, but `AD520 → ADE90 → ADBB0 → AD090` never run.
- The static size-class table is written by the port (CONFIRMED: `1403AEF50` runs, from `AC690` in the one-time init, after `14038EE80`).
- No code change this round, so no mutation and no gen/ledger run.

## 2. Static structure (CONFIRMED)

- `1403AEF50` (from `AC690`, tail call) writes the size classes into the static table at `0x143AC28E0 + class*0x78`: the first qword of each record is the class size (8, 16, 32, ..., 4096).
- `1403B07D0` loops over 44 records at `0x143AC2900` (stride `0x78`), reading the first dword, capping at 200, and summing: the table is an input there.
- `1403AC320`: reads the class from `gpr2`, computes `gpr7 = class*120` (`0x78`), reads `object+0x28`, calls `AC460` (which calls `AC600`, the free-list walk). `AC320`'s branch on `object+0x28` decides whether `AD520` (and the sentinel init chain) runs.
- Callers of the sentinel init (exact greps): `AC320 → AD520 → ADE90 → ADBB0 → AD090` (also `AD520` ← `AC320`, `AD440`; `ADE90` ← `AD090`, `AD520`, `AD690`, `AD6E0`, `AEC80`, `AED50`).
- PES's seq008 table (CONFIRMED by a dump): the class records hold sizes, limits and counts only; no free-list heads or pointers sit in the static table. The lists are in region memory reached through the object.

## 3. Dynamic facts in the port (CONFIRMED)

- Hit log (`rbreak` with auto-continue, 150 s): `AC320` 2 hits, `AC600` 2 hits (`g1 = g2`: the empty check returns at once), `AD520`/`ADE90`/`ADBB0`/`AD090` 0 hits.
- The copy-test hang is a later `AC600` walk that reads a zero link (from an earlier gdb backtrace).
- `AC600` first entry: `rcx = rdx = 0x2600800278` (boot-window object), `rax = 0x143AC28E0` (static record). The static record is a size-class record, not a list head.
- PES's `seq008` object (slot 10): `+0x28 = 0x0D7BA358` (non-zero, a region pointer). The port's `object+0x28` is zero at `AC320`, so the branch goes to the zero path. That field is written in `AC320`'s own branch from `AC460`'s return value (`gpr3 = gpr0`), so PES's `AC460` returned a block and the port's did not.

## 4. The blocker, stated exactly

- `AC460`'s return (`gpr0` after `AC600`) is zero in the port; in PES it is non-zero. `AC460` returns the head of a free list it walks from `gpr2`; the list must already contain blocks in PES. The port's list is empty.
- The blocks are placed in region memory by `AB660`/`AB470` before `AC320` (INF: `AB470 → AD8A0 → AC320` is the creation chain, and `AD8A0` calls `AC320` with the list head in `gpr1`).
- What is unknown: the writer of the first list blocks (before the first `AC320`). Not found statically yet (no self-link triple beyond `AD090`).

## 5. Next step and hook

1. Static: find the writers of the free-list blocks (the stores that link a fresh region block into the list `AC460` walks): search the layer for stores whose address is a region block (`gpr` from `VirtualAlloc`/`AB660` allocation) and whose value is another block of the same list; the candidate set is `AB660`, `AB470`, `AD8A0`.
2. Hook (if static search fails, because the creation runs before the capture hooks): the first `1403AB660` entry (`FIRST 1 EVERY 1`, cap 2, FULL) and the first `1403AC460` entry (`FIRST 1 EVERY 1`, cap 2, FULL), installed at process start. Those two show the list state PES has at its first creation. `1403AC320` is entered at the creation too.
3. Copy tests stay red; the round goal is not met.
