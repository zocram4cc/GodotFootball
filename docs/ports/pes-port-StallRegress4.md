# PES presser owner: A0 and A2 located, ActionPress NOT wired (PressWire round 2)

Branch `StallApproach`, pushed: `eeb8ca367ca7caadda94e7a0dbc5fd6cb1de9cdd` (parents `627d1475` input map, `d4919cc4`). Confirmed by `git ls-remote`.

**Result: blockers 1 and 2 are settled (CONFIRMED). ActionPress is not wired: the walk in `_BallControlCommand` is still there, and no red test was added. Census: 151, unchanged, not re-run (no code changed). Target <=118 not met.**

## What was settled (CONFIRMED)

From 3 captures (pid1502356/seq10193, seq846; pid1458145/seq1961), `tools/dev/rendezvous/press_inputs.py`, and the image:

- **A0 and A2 are members of one per-man object M.** M's constructor `FUN_14091EDB0` stores M's vtable `0x14262FD78` at M+0x10 (3 of 3). A0 (presser action, vtable `0x142634948`) is at M+0xF518; A2 (dispatch context) at M+0xA390; P3 = A2+0x50 (the r9 scalar) at M+0xA3E0. `A0 - M` and `A2 - M` are the same in all 3 captures. The offsets follow from the decompile's `undefined2*` arithmetic (`param_1 + N` is 2N bytes); the ctor's own stores match the capture bytes.
- **A2's fields are the ctor's**: A2+0 = 0, A2+4 slot = 0xFF (set later to the real slot), A2+0x38 ctx = 0, A2+0x40 owner = 0 (set later to P).
- **A0's constructor** `FUN_140961F00` (`.pdata` 0x140961F00) is called only from 0x14091F07D inside M's ctor. It calls the base ctor `FUN_140512CA0`, then stores vtable `0x142634948` and the presser defaults (+0x48 = 0xFF, +0x58 = 0, +0x80 = -1.0).
- **The holder cell `0x3509DDF0`** (seq1961) is M+0x11C4C, a pointer array inside M: blocker 2 resolved as "M's own action table".
- **The call chain at a press tick** (CONFIRMED by vtable slots, thunk bytes, and return addresses on the stack):
  driver `FUN_1405141F0` (`.pdata` 0x1405141F0..0x140514930) calls `A0.vt[0x88]` = thunk `FUN_140518DD0`, which calls `A0.vt[0x90]` = dispatcher `FUN_140518E20`, which calls `A0.vt[0x20]` = ActionPress `0x140962440` (`ff 50 20` at 0x140518E59). The driver's return address 0x140514320 and the thunk's (0x140518DFF) are both on the stack. The driver is called from G at 0x140963501..0x14096366C (return addresses 0x140963581 / 0x1409635E2).
- Image scans: E8 callers of the dispatcher (8 sites), of the ctor `FUN_140961F00` (1: 0x14091F07D), of `FUN_14091EDB0` (1: 0x14091EA27), of ActionPress (none).

## What blocks wiring (named)

1. **Translation and proof of `FUN_140961F00` and `FUN_140512CA0`.** Both are small, but no hook covers their entries, and a root needs a captured call to be proven. The capture bytes at A0 include later writes, so they cannot prove the stores. **Needs a hook**: `action_ctor_140961F00` (entry, FIRST 1 EVERY 1, cap 6). Main owns the rig; I did not add it.
2. **A window region for M's press part.** GF's player block is 0x3F40 bytes (`kPlayerBlockBytes`), and A0 is at +0xF518, so A0 cannot sit in GF's block. A new per-slot region (A0 0x100 bytes, A2 0x60, P3 0x40 at these offsets) changes `kWorldBytes`; the window static_asserts must still hold.
3. **Per-tick writers of A0 and A2 are not GF.** The driver and G write A2+4, A2+0x38, A2+0x40 and A0's fields (+0x48, +0x58, +0x5C, the stored point +0x68, +0x78) each tick. Calling ActionPress without them leaves those at their ctor values: a ponytail on every tick after the first.
4. **Command speed is not decoded.** ActionPress returns the target only; the planner's ETA sets A0+0x30 only. PES's speed comes from the movement solver `FUN_1406B9270` (in ActionPress's closure, unproven) and the speed class `FUN_140A75BE0`. Until it is wired, GF's speed for the designated man is open, and only a census run can test it.
5. **Red test** (designated man without possession beside a loose ball gets ActionPress's target, = ball): needs items 1 to 3.

## Why I did not wire it this round

Items 1 and 2 are not reachable with the tools and hooks I have. Wiring without them would call ActionPress on memory I built by hand from the decompile's immediates, which is the transliteration the brief forbids, and would not be proven. The honest step is the blocker list, not a call that cannot be checked.

## Asks of Main

- Add hook `action_ctor_140961F00` (FIRST 1 EVERY 1, cap 6) to prove item 1. Optional: `man_ctor_14091EDB0` (FIRST 1 EVERY 1, cap 3) to capture M's construction and its caller.
- Name the frame of 0x14091EA27 (the function that allocates M) if anyone has it; it decides M's lifetime.

## Next steps (in order)

1. Hook captures for 0x140961F00 arrive; translate `0x140961F00` and `0x140512CA0` with `closure.py` (set `trapaction`, tag `pressctor`) and prove them on those captures.
2. Add the per-slot press region and `PesWorld::RunActionPress(slot)`: call `pes_fn_140962440` with rcx = A0, rdx = out, r8 = A2, r9 = A2+0x50. Red test first (item 5), then the walk's removal in the same change.
3. Census (full ALiveBall), PesStallQuick, ankle 9/9, PesHang, `gen_translated.py --check`.

## Hygiene and verification

- Tests run: none (no code changed). `gen_translated.py --check`: not re-run.
- Doc: `docs/re/pes-re-press-owner.md` (the owner map, with VAs and tags); `docs/re/pes-re-press-inputs.md` (the input map, earlier round).
- `.pdata` is a flat table (file offset = VA - 0x140000000); the section-header mapping gave garbage, the flat mapping resolves every function start checked.
- Headless decompiles: 0x14091EDB0, 0x140512CA0, 0x140961F00, 0x1405141F0, 0x140518E20, 0x140518DD0 (bytes). The Ghidra bridge was down.
- Mistakes, corrected: one-byte placeholder in `<workdrive>/` (deleted); a session report written under `<workdrive>/.omp` (moved to `~/.omp`, tree removed); scratch decompile files written into the worktree (moved to `~/.omp/scratch-press`, not committed); a decompile output sent to `/tmp` (removed).
- Scratch: `~/.omp/scratch-press` (decompile dumps). `<workdrive>/gf-PressWire-scratch` removed earlier. Worktree on StallApproach, clean, godot-cpp symlink restored.
