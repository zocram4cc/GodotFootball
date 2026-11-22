# pes-port-PresReplay10: the first creation's list state is not reached in the port (round 9)

Branch `Presentation`, last pushed `09ff2a631e5a`; no code change this round (investigation only). The copy tests are still red. The uncommitted `extern/godot-cpp` symlink change is the worktree's submodule status, not mine. Build dir and unpacked captures removed; scratch 22 MB.

Labels: CONFIRMED = a replay, a gdb hit log, a memory dump or a layer line; INF = inferred.

## 1. Result

- The hook captures (`live2/pid2402670`) are **not** the first creation. PES's state at `seq001` (`AB470` entry) already has every class head set and the sentinels self-linked. So the first creation ran before the hooks were installed, as the caveat said.
- The port takes a different path at the TLS-object creation: `E3E0 → EE80 → F230 → F320 → AB660 → AD8A0 → AC320` (CONFIRMED hit order). The sentinel-init entries (`B0AE0`, `AD690`, `ADA90`, `AEC80`, `AED50`, `14038DF60`, `B0580`) are never hit.
- The port's TLS object is zero-filled at its first use (CONFIRMED: `obj = 0x2601018000`, `[obj]=0`, `[obj+8]=0`), while PES's object has its fields set (`+0 = 0x100000000`, `+8 = 0x375A90`, record tables and sentinels after it).

## 2. Structure (CONFIRMED from the `seq001` replay and dumps)

- Static chain: `[0x143AC3D98] = 0x1401C00` (written by `EE80`, lines 211/365), `[0x1401C00] = 0x14000C0` (the TLS-related object).
- The object at `0x14000C0`: `+0x00 = 0x100000000`, `+0x08 = 0x375A90`, `+0x20 = 0xFFFFFFFF`-style words, `+0x120 + class*0x98` = class records. Record `+0x28` is the class head (for class 0: `0x1920000`, a live block, which makes `AD8A0` take `AF8A0`, not `AC320`). Each record has a self-linked sentinel at the record stride (`0x14001C8`, `0x1400218`, … = `next = prev = self`).
- The one-time init sets the size-class records (`AEF50`, via `AC690`); the sentinels are set by `AD090` through `ADE90 → ADBB0`, which the port never reaches.
- `F230` calls `B0AE0` (→ `AD690 → ADE90 → ADBB0 → AD090`, the sentinel init) only when `[F320's result + 8] > 3` (CONFIRMED guard, line 74). The port's result has `[+8] = 0`, so the guard is false.
- No translated function stores `0x375A90`, `0x100000000` or `0x14000C0`/`0x1401C00` (grep of the layer, none). The object's first fields therefore come from data the allocator's heap already holds at PES's start (INF), not from a translated writer.

## 3. The blocker, stated exactly

- The TLS object is created by `AB660`/`AB470` into a block the port takes from its boot window, and its first fields (`+0`, `+8`, the records and sentinels) are never written: PES's object was initialised before the hooks, in a pre-hook step the port does not reproduce.
- The earliest PES creation that writes those fields is not in any capture (all captures are after hook install). INF: it is the `EE80` one-time init's allocation of `0x1401C00`/`0x14000C0` (both low-heap addresses in PES), whose contents are initialised before the first `E3E0` hook call.

## 4. Next step

1. Static: find the writes into the object's `+0x08` and the records (the `0x120`-based records) in the callers of `EE80` that run before `E3E0`; the candidates are the `EE80` allocation sites (lines 205–245 and 340–365 write `0x143AC3D90/94/98/DC8/DCC`).
2. Hook (if static fails): entry of `14038EE80` (the one-time init, `FIRST 1 EVERY 1`, cap 2, FULL) installed at process start, and entry of `1403AC690` (the size-class table writer), so the first creation's inputs are captured.
3. Copy tests stay red; the round goal is not met (no mutation, no gen/ledger run).
