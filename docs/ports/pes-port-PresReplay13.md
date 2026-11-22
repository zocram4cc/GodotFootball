# pes-port-PresReplay13: the heap boundary is on the copy path but is not the failing allocator; the creation is not reached from the CRT initialisers (round 12)

Branch `Presentation`, last pushed `09ff2a631e5a`; no committed change this round. The copy tests are still red. Build dir removed; scratch 24 MB.

Labels: CONFIRMED = a gdb hit log, a stack sample, a static sweep or a file read; INF = inferred.

## 1. The allocator's public entry points (CONFIRMED from `pes_fn_1403FB410/FB5B0/FB860` bodies)

- **pool alloc `0x1403FB410`**: `rcx = &poolId` (u32), `rdx = size`, `r8 = align`; returns `rax` = block or 0. It reads the pool table at `[0x1436EF2C8]` (0x40 bytes per pool, the pool's heap object at +0), calls the object's vtable `+0x10` (alloc). On failure it trims (`0x1403FB860`, vtable `+0x50`), retries, then calls the fail handler at `[0x1436EF2B8]` if set. Alignment: the table at `0x14258E270` gives the rounding (`1,1,2,2,4,4,4,4,8...`) for small aligns; default 16 (CONFIRMED from the body).
- **pool free `0x1403FB5B0`**: `rcx = &poolId`, `rdx = ptr`; reads the pool object, then `0x1403FB4F0` (vtable `+8`).
- **trim `0x1403FB860`**: called from alloc's retry path only (no GF effect in the native heap).
- The native heap (`PesHeap`, `pes_native.cpp`) already implements the vtable targets (`heap_alloc/free/owns/trim`, `native_services` in `stubs.json`).

## 2. Is the heap on the copy path? (CONFIRMED)

- Yes: a breakpoint log of the copy test shows `14217DE20 → 14217DB10 → 1403FB410 (×2) → 14038E3E0 → 1403AB660 → 1403AC460 (×2)`. So the pool alloc runs twice before the one-time allocator init.

## 3. Experiment: route the pool entries to the native heap (REVERTED)

- Hand edit (not translator-generated, reverted; worktree clean): `pes_fn_1403FB410` → `pes_native_heap_alloc(s)` and `pes_fn_1403FB5B0` → `pes_native_heap_free(s)`. It builds, runs, and the copy test still times out.
- Stack sample with the experiment in place: `pes_fn_1403AC600 ← AC460 ← AC320 ← AD8A0 ← 14038E3E0 ← …` (the same circular-list walk as before).
- So the pool heap is on the path but **not the failing allocator**. The walk that never ends belongs to the region manager that `14038E3E0` drives: `AB660 → AB470 → AD8A0 → AC320 → AC460 → AC600`. That manager is not `0x1403FB410`.

## 4. Why a fresh boundary does not turn the copy green (INF with CONFIRMED facts)

- `14038F320` (the TLS-slot get-or-create) returns the slot's object when TLS slot `10` is non-zero, and creates one (`1403AB660`) only when it is zero. In the port the slot is zero, so it creates a fresh object; its `+8` is 0 at the gate `F230 line 68`, the sentinel init `B0AE0` is skipped, and the list walk reads zero links.
- In PES the slot is already non-zero when the replay starts: the object (`+8 = 0x375A90`) was created earlier. Making the heap a boundary gives fresh zeroed blocks, which does not reproduce that state: the object's `+8` and sentinels come from the earlier creation, not from the heap contents.

## 5. Reachability from the CRT initialisers (CONFIRMED by a direct-call graph of the unpacked image, 125,455 functions with direct calls)

- The `__xi` (3) and `__xc` (6,842) initialisers, plus the two thread-start callbacks (`0x14159FE68`, `0x14159FEC8`, which are the `_initterm_e`/`_initterm` loops), reach `1403FB410` (the pool alloc) in 138 cases at depth 1. So the CRT initialisers do allocate from the pool heap during start-up: that is the heap history the owner's ruling refers to.
- They do **not** reach the creation path: `14038F320`, `1403AB660`, `14038E3E0`, `14217DB10`, `14217DEF0`, `14038F230`, and the allocator's static writers `1403AEF50`, `1403AC690`, `1403B07D0`, `14038EE80` are unreachable from any initialiser by direct calls at depth ≤ 16.
- So the object that PES holds in TLS slot 10 is created by a path the CRT initialisers do not reach directly. It is reached from the game's boot through indirect calls (function-pointer tables or vtables), which the direct call graph cannot see.

## 6. What the owner's boundary model would need (not done)

- A boundary is only useful if the object it stands for is created with PES's contents. The pool entries do not create it. The region object's contents (`+8 = 0x375A90`, the records, the self-linked sentinels) come from a creation whose callers are indirect. Finding that caller needs the indirect-call sites (tables at `.rdata`/`.data` that the initialisers read) resolved against the unpacked dump.
- TLS callbacks: the PE TLS directory has one callback, `0x15B9560E0`, a packer wrapper that walks the table `0x14253DE98`; the table holds the two CRT runners above (CONFIRMED).

## 7. Stop point and rulings needed

1. Resolve the indirect calls that reach the creation path from the game's boot (vtable/function-pointer tables in the dump). This is PES's own code and fits the "translate PES's boot" plan, but it is a larger graph search.
2. Or model the TLS-slot object as a boundary with contents produced by PES's creation code run once in the port's boot. That needs the creation's inputs, which depend on (1).
3. Or mark the copy tests as blocked on the game-boot creation.

Recommendation: (1). The pool heap boundary itself (`native_services` entries for `0x1403FB410`/`0x1403FB5B0`, code_hex-checked, with a unicorn test) is cheap and correct for the heap history, but it does not by itself turn the copy tests green, so I have not committed it.
