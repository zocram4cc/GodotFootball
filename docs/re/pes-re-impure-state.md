# Why impure functions reach so little of their code (StateSynth, 25-10-26)

Compiled by Main from StateSynth's final result (it ran out of budget before writing a report). Work in progress on
`origin/StateSynth` `6abf7e2`: `tools/dev/translate/snapshot.py`, `build_seed.py`, `live_snapshot.py` (none wired in or
run yet).

## Root cause (confirmed from the harness source and the specs)

`run.py` runs each case as: set inputs, apply `patch_addrs`, apply `regions_init`, link the context, snapshot, emulate,
restore. Impure specs declare `regions` (a shadow of G at `IMG.1436F3DC8` plus several 2 MiB child regions) and
`regions_init` (G's child slots re-pointed at those regions), but **every region starts zero-filled**: a spec gives a
region's base and size, never its content. So the sub-objects' vtable pointers, class bytes and "initialised" flags all
read 0, the functions' initialisation gates fail, and they exit before reaching the data fields the cases vary (for
example `.19C8` +0x1 u8 and +0x8 u32, `.1DD8` +0x554 f32, `.25F8` +0x1414 f32). Coverage-guided generation varied the
gate bytes but hit them at about 1 in 256, which is the measured ~6% median.

Seen on `0x140503430`: E1EE is linked at 51 G offsets (0x19F0, 0x1A18, 0x1A40, ... to 0x1950); `G+0x1928`, `G+0x1DD8`,
`G+0x25F8` link to their own child regions.

## Fix

Seed each region with real state before the case's inputs are applied, on both the harness side and the translated
side: G's own bytes, each child record's header (class byte at +0x1, initialised flag at +0x8), MI's phase words
(+0x11A0, +0x11A4), and the pointers G holds to its children. Per-case inputs then vary the data fields as before.
Seeds are PES-derived: build tree only, referenced by path, never committed.

Two sources:

- **Emulated setup** (`snapshot.py`): PES's own G constructor and initialiser (`FUN_140a44940`, `FUN_140a4a300`) run in
  the emulator give a real 0x2E58-byte G with genuine sub-object vtables and flags, but its child slots stay 0, because
  match load (`FUN_140886220`, which fills MI at `G+0x270`) never runs.
- **A live mid-match snapshot** (`live_snapshot.py`): every readable mapping while MI is non-null. All children are
  real. Available whenever the game is running.

Setup functions named in the docs (not re-verified): slot record `FUN_14092d230`; MA base and ladder `FUN_140b42350`,
`FUN_140b41c22`; setter `FUN_1404a2d80`; rule `FUN_140a3fbc0`.
