# CutsceneAnchor8: the cut-library singleton [0x1437F4808] is loaded at boot from the install's cut files through a vtable method; the chain is mapped, not built. Wide test still RED; no code landed.

Branch `CutsceneAnchor`, head `f6ffbad5` (unchanged; the tree is clean, no untracked files). Labels: CONFIRMED = read or run this round; INF = inference; OPEN = named blocker.

## 0. Outcome

- Wide test: RED (unchanged, values T.x, pos.x, pos.z).
- Not built: the builder chain for [0x1437F4808] needs VFS file loading and PES object layouts that GF does not model. The map below is what the build needs; it is not a one-round job.
- No commit, no push this round.

## 1. The singleton and what reads it (CONFIRMED, xrefs by a rip-relative scan of the image's pdata functions; the Ghidra bridge is down)

- Store sites of `[0x1437F4808]` (the 'cut-library' singleton, a 0x18-byte vector header: begin, end, capacity):
  - `0x141E5C964` in `0x141E5C8F0`: the **builder**. If the global is null: allocates 0x18 bytes (`0x1403FB410`), constructs its sub-object (`0x14184D5C0` at +0x18), reads the count `0x141E5DE80` (= 234, constant), resizes the vector (`0x141E5D330`), then stores the global.
  - `0x141E5CA0C` in `0x141E5C980`: the **teardown** (frees elements, clears the global).
- Callers of the builder: `0x140419D80` (CONFIRMED, one direct call at `0x140419D89`), which is called from `0x140401B50` (a function-pointer table entry at `0x143445460`: the boot init table, INF: the boot step).
- The builder only **resizes**: the element pointers are null right after it returns. The entries are filled later. seq321 (the worldctor capture of launch pid678718, pre-kick-off) holds the filled vector: 234 entries, begin 0x333B5C0, end 0x333BD10 (CONFIRMED by memory read).
- Element fill: `0x141E5D560` (CONFIRMED) is **a vtable entry**: its address is the value at `0x142B48C88` in the image (the vtable of the cut-library object). It calls `0x141E5CB40` (once, `0x141E5D668`) with `rcx = [S2]`. `0x141E5CB40` takes a **VFS path string** (the cut's `.fdc` name, reads it through the mounted VFS, parses it) and builds the entry object. Its only direct caller is `0x141E5D560`. (INF: the loop over the 234 indices is in `0x141E5D560`; not decoded.)
- Readers: `0x141E5CA20` (CONFIRMED): `entry = vector[key]`; returns `entry->+8` (a node of vtable `0x142B54228`) or 0 if the entry is null. `0x141E5D560`, `0x141E5D980`, `0x141E5DF50` and others read the global as the object for their method calls.

## 2. The writer of [G+0x2C0]+0x1C (key 0x71) (CONFIRMED values, writer OPEN)

- `T = [G+0x2C0]` (the object GF calls the time-scale object): PES holds T+0x1C = 0x9E at worldctor entry (seq321) and **0x71 at the anchor** (seq204, the goal under test). So the key changes per goal or per event; it is not a boot constant.
- Writer not found: the `[T+0x1C]` store is an indirect write through a register; a scan for writes to `[G+0x2C0]` plus offset is the next step (`0x1404B6400` reads it as the lookup key; the store is elsewhere).

## 3. The scene the key selects (CONFIRMED from the install)

- `scene_map.py` (repo tool, via the mounted VFS, `pesfmt_dump` built from the worktree): scene **0x300D9 = goal[216] GOAL_A_CELEBRATE_0243**; its step's cut is **0x300D9** (the same id), and the cut's variant keys `0100,0000,0000` select `goal_A_celebrate_0243.fdc` and `goal_Effect_h.fdc`.
- The goal table has 362 cut groups (scene_map), not 234. So the 234-entry library is not the goal cut-group list; its identity is OPEN (candidate: the cut *library* of the fdc files, T1 in `pes-re-cutscenes.md` §2.1; INF).

## 4. Chain for the else arm's scene (CONFIRMED by reading the translated code, see CutsceneAnchor7)

`0x1404B6400`: `T = [G+0x2C0]`; `key = u32[T+0x1C]`; `entry = 0x141E5CA20(singleton, key)`; the node at `entry+8` feeds the rest of `0x1404B6400` to the scene id. GF's singleton is null, so the lookup returns 0 and the fallback yields A0+8 = 1 (PES: 0x300D9).

## 5. What it would take (the plan, in order)

1. **Boot builder in GF**: `0x141E5C8F0` (vector of 234 null pointers, the vtable-object's count) and the init-table entry `0x140419D80`'s callees, as a bounded proof on seq321 memory (entry state: the global null, the rest as seq321). Proof: synthetic entry (labelled), like the factory and creators.
2. **Element fill**: the vtable method `0x141E5D560` (vtable 0x142B48C88) and `0x141E5CB40` (VFS read + parse of each `.fdc` into a PES object). Requires: a VFS read native (PesVfs exists in GF: `pes::PesVfs`), string handling (`0x140402370`, `0x1403DC850`), the .fdc object layout (`fdc.hpp` decodes records but not PES's in-memory node of vtable 0x142B54228). Biggest item.
3. **Key writer**: find the store to `[T+0x1C]`; decide whether it is a per-goal value (cut group index) and feed it from the goal's state.
4. Re-run the wide test; then T.x at the event tick (INF: PES's T.x -41.6211 is the scorer's x at the event tick, the anchor reads z 29.56 later; not checked).

## 6. Checks

- No code change: `gen --check`, `ledger --check` unchanged from CutsceneAnchor6 (rc 0).
- Neighbours unchanged (registry 7/7, fixdemo_select 6/6, camera_list 5/5, proc_camera 6/6, goal-anchor non-wide 22/22).
- No mutation (no green change).
- The Ghidra bridge was down for this round (scans done with a capstone pdata sweep, `gf-CutsceneAnchor-scratch/f5/xscan.py`, `cscan.py`).

## 7. Open items (numbered)

1. Build step 1 (boot builder) as a proof; the rest depends on it.
2. Decide the identity of the 234-entry library (not the goal cut groups: 362).
3. The VFS loader `0x141E5CB40` is the largest unknown: list its native calls before choosing between translating it and a native service (like PesVfs).
4. The writer of T+0x1C (key 0x71 at the anchor).
5. T.x at the event tick: check after the scene is right.
