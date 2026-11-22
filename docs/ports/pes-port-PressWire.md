# PES ActionPress inputs mapped from captures; not wired (PressWire, 22-11-26)

Branch `StallApproach`, pushed: `627d14752865d3cafc300d112415bfc775a07e82` (parent `d4919cc4`), confirmed by `git ls-remote origin StallApproach`.

**Result: the furthest proven step is the input map. ActionPress is NOT wired, the designated man's walk is still in place, and no red test was added. Census: unchanged at 151 (not re-run; no code changed).**

## What was proven (CONFIRMED)

From 3 real ActionPress captures (pid1502356/seq10193, seq846; pid1458145/seq1961), read with `tools/dev/rendezvous/press_inputs.py`:
- Entry registers: rcx = A0 (the presser action object, vtable 0x142634948), rdx = A1 (out float[3]), r8 = A2 (dispatch context).
- A2+4 = slot id; A2+0x38 = team ctx (ctx+0 vtable 0x14259D590); A2+0x40 = the planner owner, an AnimePlayer P (P+0 vtable 0x1425E34D8).
- The P vtable is stored at P+0 by 0x1406E0408, inside P's constructor; its interface vtables at P+0xAB8 (0x1425E3508) and P+0xAC0 (0x1425E35B0) match GF's `kPInterfaceVtable0/1Offset`.
- A0's constructor block is 0x140961F0E (its default stores decompiled; A0 is not inside any of the 25 P objects in seq1961).
- No direct E8 call to 0x140962440 in the image: the entry is reached only through the action vtable.
- The global G = `*(0x1436F3DC8)` is GF's `kWorldGlobalVa`.

## What blocks the wiring (OPEN, in order)

1. The caller frame that builds A2 and calls `[vtable+k]`: return address 0x140518E5B; the function's start is not identified (the headless decompile is a fragment).
2. The holder of A0 is heap cell 0x3509DDF0 (seq1961); its writer decides whether A0 is per man.
3. The enclosing function of the constructor block 0x140961F0E (no direct caller).
4. The A0 field writers (+0x48, +0x58, +0x5C, the stored point +0x68, +0x78): press writer FUN_140536FC0 and ActionPress itself.
5. The A1 consumer, which fixes the command (target and speed). The planner ETA only sets A0+0x30.
6. Then the wiring itself: a red test (designated man without possession beside a loose ball gets ActionPress's command, target = ball), the call in `_BallControlCommand`, and deletion of the provisional walk in the same change.

## Other

- Files: `docs/re/pes-re-press-inputs.md` (the map, with VAs and tags) and `tools/dev/rendezvous/press_inputs.py` (decoder: `fields`, `holders`, `pvalues`; streams the capture through zstd, reads only).
- Tests run: none (no code changed). `gen_translated.py --check`: not re-run (no translated code changed).
- Ghidra bridge was down: decompiles through `dec.sh` (headless). Some function starts were not identifiable from return addresses.
- Mistake, corrected: I wrote a one-byte placeholder to `<workdrive>/` and deleted it at once. Scratch `<workdrive>/gf-PressWire-scratch` removed.
- Worktree: `<workdrive>/gf-StallLive` clean on `StallApproach`; godot-cpp symlink restored.
