# pes-port-PresReplay16: the copy source comes from a runtime registry keyed by a game-loop record; the port's fixture has neither (round 15)

Branch `Presentation` unchanged: last pushed `2e76b84c` (`git ls-remote origin Presentation` = `2e76b84c5fae`). No code change this round. Copy tests still red. Closure regeneration (item 3) NOT done (section 5). Build dir removed; decompressed captures removed; scratch 30 MB.

Labels: CONFIRMED = a unicorn replay of PES's own bytes on `pid2024691/seq205` (the only capture that copies), a gdb read of the port, a layer line, or a dump of PES memory. INF = inferred.

## 1. `1402647D0` is not the source; it stores what it is given (CONFIRMED)

- `1402647D0(rcx = &slot, rdx = source)` does `xchg [rcx] <- rdx` (through `1403DB290`), then releases the old value. It stores 0 when it is given 0.
- In PES's real run it is called twice by `A0DE0`: clear (`rdx = 0`, trace index 745), then set (`rdx = 0x22e50570`, index 746, return into `pes_fn_14030E300` at `0x14030E334`). `14030E300` passes `rdx = [r15+0x28]` where `r15` = `A0DE0`'s `rdx` = the stack record of `AFF8E0`. So the source is whatever `AFF8E0` stored in that record.

## 2. The source is the result of a registry lookup in `AFF8E0` (CONFIRMED)

- `AFF8E0` stores the record's `+0x28` from `gpr14`, and `gpr14` comes from `141B03540` (call at `AFF8E0` line 107). `141B03540(rcx = table, rdx = key record)`: `table = 141B034B0(...)`, `key = [rdx]`, a linear search of `count = dword [table+0x38]` entries of 0x40 bytes at `[table+0x40]`, comparing `[entry+0x38] == key`, returning `[entry+0x18]`.
- PES, first copy: `table = 0x1501960` (count 14, entries at `0x22f50400`), key `0x90a26f0fb38a02` (from `[0x23e85bd0+0x28]`), match at entry 1, value `0x22e504d0`. Second copy: key `0x9080a74211eea4`, match at entry 2, value `0x22e50570` (both values are the sources that `1402647D0` stores).
- These 14 entries are not written during the frame (a write trace of `0x22f50000..0x22f51000` shows none): they exist before the captured frame, i.e. they are game-loop state.

## 3. The key record comes from `AFF8E0`'s own argument (CONFIRMED)

- `AFF8E0` line 82: `[rcx + 0x100] = rdx` (rcx = the replay object `X`, rdx = the key record). So `[X+0x100]` = the key record, and `141B03540`'s `rdx = [X+0x100] + 0x28`.
- In PES `rdx` at that store = `0x23e85bd0`, a **stack** address (the key record is on the thread's stack; `[0x23e85bd0+0x28] = 0x90a26f0fb38a02`, the key).
- The caller `141EB6840` passes `rdx = [0x1437F4B78]` (line ~116 of its layer body). PES: `[0x1437F4B78] = 0x23e85bd0` (CONFIRMED, seq205 memory). The install's raw `.data` ends at `0x1436CF800`; `0x1437F4B78` is in the zero tail, so GF reads 0.

## 4. The port's divergence (CONFIRMED by gdb)

- At `AFF8E0` entry the port has `rcx = 0x2400001840` (`X`, in the arena) and **`rdx = 0`**, so `[X+0x100] = 0` and `141B03540` runs with `table = 0`, `rdx = 0x28` (`gpr3 = 0`). The lookup finds nothing, `A0DE0` gets source 0, and the copy takes the zero branch.
- The fixture therefore lacks three pieces of game-loop state that PES holds before the frame: (a) the global `0x1437F4B78` (a pointer to the key record on the caller's stack), (b) the key record itself (`+0x28` = the key, computed by the caller chain from the human's data), (c) the registry table `0x1501960` returned by `141B034B0` (14 entries keyed by those keys).
- None of (a)-(c) is produced by the frame's own code: (a) is a runtime global in the zero tail; (b) and (c) are heap/stack objects written before `14217DEF0` (the frame's write traces show no writes to (c)). So the port's frame cannot build them from the install.

## 5. Owner's items

1. **Fixture vs translation:** the fault is the fixture (missing game-loop state), not a translated function: `1402647D0`, `141B03540`, `AFF8E0`, `A0DE0` all run PES's logic on what they are given. The faithful fix needs (a)-(c). Options:
   - (A) find and translate the game-loop code that builds (a)-(c) (the writer of `0x1437F4B78` and of the table `0x1501960`'s entries); not located yet (they are written before the captured frame; the captures do not start early enough).
   - (B) load the state from the capture into the fixture. This is the "as the other replay tests do" route, but the brief rules and the owner's "no baked PES heap state" rule forbid copying PES heap contents into the port.
   - I did not do either. Ruling needed.
2. **Closure regeneration (`closure_destr14217.json`) and dead-member pruning, dead import models:** NOT done. The closure is a snapdiff proof of `14217DEF0` over the real `seq567/960/1464/1954/1955` (no copy) and the synthetic states. Regenerating it with `seq205` requires the copy path to be proven, which needs (A) or (B) above. Without that, regeneration would drop or keep the wrong members. The region-manager members remain in the layer, so the import models stay referenced.
3. **gen --check, ledger --check, one mutation, presentation and HUD tests:** not re-run (no layer or test change this round).

## 6. Process

- Worktree clean; branch `2e76b84c` unchanged.
