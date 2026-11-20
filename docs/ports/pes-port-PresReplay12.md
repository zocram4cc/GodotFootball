# pes-port-PresReplay12: the CRT initialiser tables are located; the copy-path gap is allocator history (round 11)

Branch `Presentation`, last pushed `09ff2a631e5a`; no code change this round (the initialiser list and the state diff are the result). Copy tests still red. Build dir removed; decompressed capture and dumps removed; scratch 29 MB.

Labels: CONFIRMED = a file read, an instruction sweep, a gdb dump or a diff; INF = inferred.

## 1. The startup and the initialiser tables (CONFIRMED)

- The import calls in the unpacked image go through the packer's copies: `mov rax,[IAT]; mov [copy],rax` pairs at `0x15B95C000` (163 pairs; the CRT copies are in `.rdata` at `0x142530318..0x1425303E8`). The original code calls the copies through thunks `jmp [copy]` at `0x1415A1894..0x1415A18FA`.
- The CRT startup is `0x14159F150`. It calls `_initterm_e` (the `__xi` table) and `_initterm` (the `__xc` table), with bounds set by `lea` pairs:
  - `__xi`: `0x14253DE68..0x14253DE88`, 4 slots, 3 non-zero: `0x14159F068`, `0x14159F124`, `0x14159F6D8`.
  - `__xc`: `0x142530878..0x14253DE50`, 6,843 slots, 6,842 non-zero (C++ dynamic initialisers), first `0x14159F134`, last `0x140227900`.
- No translated function is among them (CONFIRMED: `closure`, `replay`, `pre_match`, `ai` checked).

## 2. Which initialisers write the missing state (CONFIRMED, direct references only)

- A sweep of every RIP-relative reference (read/write/lea) from every function to the `.data` tail found 58,982 functions with such references; 1,647 of them are initialisers.
- Diffing the port's `.data` (dumped at the first `AC600` entry) against PES's captured memory (`live2/pid2402670/seq001`): 28,091 qwords are non-zero in PES and zero in the port, and 188 differ. The initialisers' direct references cover 5,127 of the 28,279 differences; with their 0x40-byte blocks, 8,409.
- The copy path's statics mostly already match after the port's init: the record tables at `0x143AC2818/868/900/978`, `0x143AC2820`, the thresholds and the flag all equal PES's values (the port ran `EE80` and `AEF50`). The differing ones are pointers and the cookie (expected to differ between runs), and `0x143AC3DA0` (PES `0x375A50`, a heap pointer).

## 3. The actual copy-path gap is allocator history (INF, with CONFIRMED facts)

- `F230` calls `B0AE0` (the sentinel init) only when `[object+8] > 3` after `F320`. In PES's captured object `[+8] = 0x375A90`, a heap pointer, which only an earlier allocation can have stored.
- The port's first creation stores `1` at `+8` (line 83), and nothing later overwrites it: the port's copy test starts with a fresh allocator.
- PES's captured state is after at least 100,001 calls of `E3E0` (`seq008` is call 100,001, counted from hook install), i.e. after the game's boot has used the allocator heavily. The replay frame in the port does not run that history.

## 4. What is needed (stop point)

- The CRT initialisers are PES's own code and can be translated in table order (the `__xi` 3 and the `__xc` subset that touches the diff). That is the step Main asked for, and it is large: the 1,647 touching initialisers plus their callees.
- But the initialisers alone do not turn the copy test green: the copy path needs the allocator's history (the `[object+8]` pointer), which is game-boot state, not a CRT table. Making the copy test green faithfully means replaying the game's boot allocations (or a boot entry that performs them) before the replay frame.
- Ruling needed: (a) translate the CRT initialisers (large; the list is in this report) and then the game-boot allocator history, or (b) mark the copy tests as needing the game-boot history.

Nothing committed this round; the branch is at `09ff2a631e5a`.
