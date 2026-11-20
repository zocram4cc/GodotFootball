# pes-port-PresReplay8: the TLS object chain is PES's own allocator path; the port's object lists are not linked (round 7)

Branch `Presentation`, pushed `d3b8b5f3..09ff2a63` (`git ls-remote origin Presentation` = `09ff2a631e5a`). Worktree: build dir removed; the 2 GB decompressed capture removed from scratch (scratch 20 MB). Labels: CONFIRMED = a gdb run, a memory dump, or the layer source; INF = inferred.

## 1. Result

- **Done: the boot window is big enough.** The boot's `VirtualAlloc` calls (8 MB regions) overran the 16 MB window: the bump cell was at `0x26017E1000` before the first allocator walk (CONFIRMED, gdb read of the window). The window is now 1 GB of calloc'd, lazily committed address space (`replay_human.cpp`). Non-copy replay tests 5/5 green.
- **Effect:** the copy tests no longer fault in `1403ABDE0` (the `-1` at `0x14254DED0` was a read of unmapped memory). They now loop in `1403AC600`, the circular-list find, on a zero link (CONFIRMED by a gdb backtrace: `AC600 ← AC460 ← AC320 ← AD8A0 ← E3E0`). Copy tests remain red.
- **No mutation this round:** the change is a window size, not a unit-testable contract; the round-6 hint mutation stands.

## 2. The per-thread object: who creates it (CONFIRMED static)

- **Slot writers:** the only writers of the TLS slots `gs+0x1480+idx*8` are get-or-create paths inside the allocator (`14038E230`, `14038F1C0`, `14038F320`, `1403AB570`, `1403AB740`, `1403AB8B0`, `1403B0300`, `1403B0950`, `1403B0C70`, `1403B0F60`), each storing the object it creates. `TlsGetValue`/`TlsSetValue` are not called for these slots.
- **Index:** `0x143AC2814` is written only by `143E114E0` (via the trampoline `1403B08E0`, called once from the one-time init `14038EE80`). It sets the index from `TlsAlloc`'s result; the port's `TlsAlloc` model returns 0, which the ruling accepts as a consistent key. PES's 10 is a value of the process's earlier allocations (INF).
- **Creation chain (the port runs it):** `14038E3E0 → 14038F230 → 14038F320` (slot empty) `→ 1403AB660 → 1403AB470 → 1403AD8A0 → 1403AC320 → 1403AC460 → 1403AC600`, and `1403AE320 → 1403ABA40 → 1403ABDE0`.
- **PES's object (seq008, slot 10 at TEB+0x14D0 = 0x181CB60, CONFIRMED by a dump):** `+0x00` count 1; `+0x08` 0x1581E000; `+0x10` count 1; `+0x18` 0x1405700; `+0x20/+0x28` a pair `0x0D7BC388 / 0x0D7BA358`; `+0x30/+0x38` `0x01B78E40 / 0x01BF169F98`; `+0x40/+0x48` `0x0D7BC3B0 / 0x0D7BA370`; `+0x50` `0x0181CBC0`; `+0x60` the ASCII string "Clear"; `+0x70/+0x78` `0x0D7BC3D8 / 0x0D7BA388`. The `+0x20`, `+0x40`, `+0x70` pairs are sentinel-style links (three nodes 0x28 apart, prev links 0x18 apart), so the object owns linked nodes in region memory.

## 3. Where the port diverges (CONFIRMED)

- `1403ABDE0` at its first entry: `rbx = 0x2601000000` (the window end in the old 16 MB layout) and `rdx = rbx+0x30` (unmapped then). With the window enlarged, the walk reads real memory; the list links are zero.
- `1403AC600` (AC460's find): `do { node = [node] } while (node != sentinel)`. The port's sentinel is never linked (`next` stays 0), so the find loops on `[0]` forever. PES's sentinels are linked into their own region (see the object's pairs above).
- INF: the linking is done by an init that the port does not run before the first creation: PES's creation ran before the hook window (seq001's flag is already 1), so the init that links the sentinels ran in PES before the captures. The port's `14038EE80` one-time init runs `1403B08E0`, `1403AFBA0`, `1403AC690`, `1403B07D0`; none of them writes a sentinel link (grep of the closure: no `wr` of a self-pointer in those).

## 4. Next step (no hook needed yet)

1. Find the writers of the sentinel links statically: any `s.wr` in `AB660`/`AB470`/`AD8A0`/`AC320`/`AC460` that stores a node or sentinel address into a field the find then follows (`AC600` follows `+0x00`; `AC460` stores `+0x18/+0x20`). The first creation in the port must link them, as PES's object does.
2. If the link is written by a function in the boot's pre-hook phase (before `E3E0` call 1), name it: the rig request would be a hook on that function's entry (not the gate). I have not found it statically yet.
3. Copy tests stay red; the round's goal (copy tests green, one mutation, gen --check, ledger) is not met.

## 5. Checks

- Non-copy replay tests 5/5. Not run this round: HUD (4/4 last round; no change since), `gen --check` (no translator change this round; last run byte-identical, 5874 functions), ledger (44459 rows, last check).
