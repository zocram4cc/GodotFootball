# PES presser owner M: where ActionPress's A0 and A2 live, and who builds them (PressWire round 2, 22-11-26)

Status: blockers 1 and 2 of `pes-re-press-inputs.md` are settled (CONFIRMED). ActionPress is NOT wired yet: the constructor that
builds A0 and the per-man constructor are not translated, and no hook proves them. The provisional walk stays. Census: 151 (unchanged).

Tags: **CONFIRMED** = read from 3 real ActionPress captures (`tools/dev/rendezvous/press_inputs.py`) or from the decompile / image
(`.pdata` begin, E8 scan, vtable slot read); **INF** = inference; **OPEN** = blocker named.

## 1. The per-man object M (CONFIRMED)

The presser action A0 and the dispatch context A2 are members of one per-man object M, not separate allocations:

| member | offset from M | checked |
|---|---|---|
| M vtable (ctor stores `0x14262FD78`) | +0x10 | 3 of 3 captures |
| M+0x18 u32 | 0 | ctor store, 3 of 3 |
| A2 (dispatch context) | +0xA390 | `A2 - M` = 0xA390 in 3 of 3 |
| A2+0x04 slot (ctor: 0xFF) | +0xA394 | slot 6/17/18 at entry |
| A2+0x38 ctx | +0xA3C8 | |
| A2+0x40 owner P | +0xA3D0 | |
| P3 = A2+0x50 (the r9 scalar) | +0xA3E0 | `P3 - A2` = 0x50 in 3 of 3 |
| A0 (presser action, vtable `0x142634948`) | +0xF518 | `A0 - M` = 0xF518 in 3 of 3 |
| action table pointer cell (holds A0's value) | +0x11C4C | seq1961 holder `0x3509DDF0` |

Scale: the constructor is written with `undefined2*` arithmetic (Ghidra), so its `param_1 + N` is a byte offset of 2N. That is why
`param_1 + 0x7A8C` is +0xF518 and `param_1 + 0x51C8` is +0xA390. The ctor's own stores match the capture bytes at these offsets.

## 2. Who builds M, A0, A2 (CONFIRMED structure, from the headless decompile of 0x14091EDB0)

- **The per-man constructor `FUN_14091EDB0`** (`.pdata` 0x14091EDB0..0x14091FA7E) builds M. It sets M's vtable (+0x10) and zeroes A2:
  `param_1+0x51C8` = 0 (A2+0), `param_1+0x51CA` = 0xFF (A2+4 slot "none"), `param_1+0x51E4` = 0 (A2+0x38), `param_1+0x51E8` = 0 (A2+0x40).
  It constructs P3 at `param_1+0x51F0` (= A2+0x50) through `FUN_14091E2B0` and `FUN_140921B20`.
- **A0 is constructed inside it**: `FUN_140961F00(param_1 + 0x7A8C)` at 0x14091F07D (+0xF518). CONFIRMED by the call site.
- **`FUN_140961F00`** (`.pdata` 0x140961F00..0x140961F7B): calls the base ctor `FUN_140512CA0` (`.pdata` 0x140512CA0..0x140512CC6,
  vtable 0x1425C5880), then stores the press class vtable `0x142634948` at +0 and zeroes the presser fields, with `+0x48 = 0xFF`,
  `+0x58 = 0`, `+0x80 = -1.0`. CONFIRMED (decompile). Its only direct caller is 0x14091F07D (E8 scan).
- The one direct E8 caller of `FUN_14091EDB0` is 0x14091EA27 (scan of the image; a second caller would be in the same match-setup
  function). OPEN: which function that is (the frame that allocates M), and M's lifetime.

## 3. The call chain at the presser tick (CONFIRMED from vtables, the thunk bytes, and the stack)

Per capture, the stack at ActionPress entry holds the return chain: ActionPress ← dispatcher ← thunk ← driver ← G.

1. **G**: the per-man tick. Return address 0x140963581 / 0x1409635E2 (both in `.pdata` 0x140963501..0x14096366C). Calls the driver with
   the action object from M's table (OPEN: G's start, the table index). Not decoded further.
2. **Driver `FUN_1405141F0`** (`.pdata` 0x1405141F0..0x140514930, decompiled): `param_1` = the action object (A0), `param_2` = A2,
   `param_3` = P3's owner record, `param_4` = the state int pointer. It calls `vt[0x88]` (the thunk), `vt[0x80]`, `vt[0x98]`,
   and more. Its call to the thunk is at 0x14051431x; the return 0x140514320 is in its body.
3. **Thunk `FUN_140518DD0`** (`.pdata` 0x140518DD0..0x140518E12): `r10 = [rcx]`, `call [r10+0x90]` with the same register args,
   then stores -1.0 through the out pointer passed on the stack (`mov rcx,[rsp+0x80]; mov [rcx],0xBF800000`). A0's vtable slot 0x88 = 0x140518DD0 (CONFIRMED).
4. **Dispatcher `FUN_140518E20`** (`.pdata` 0x140518E20..0x140518FE0): A0's vtable slot 0x90 = 0x140518E20 (CONFIRMED). Calls
   `vt[0x20]` (ActionPress) with `(A0, out, A2, P3)`, then copies the result to `param_5`, calls `vt[0x28]` and `vt[0x30]`, and
   writes `P3+0x8D4 = 0x1E` on a card/state test.
5. **ActionPress** `0x140962440` = A0's vtable slot 0x20 = 0x140962440 (CONFIRMED, `call [rax+0x20]` at 0x140518E59, bytes `ff 50 20`).

So the driver never calls ActionPress directly: each tick the man's action object is dispatched through the vtable, and the
dispatcher passes the same A2 and P3 that the driver received.

## 4. What GF would have to build, and what is missing

To call ActionPress for a designated man GF needs, in its world window, per slot:
- A0: the press action object, built by PES's own constructor (`FUN_140961F00`, plus the base `FUN_140512CA0`); persistent across ticks.
- A2: slot (+4), ctx (+0x38, GF's `CtxVa(side)`), owner P (+0x40, GF's `PVa(slot)`), zero elsewhere, as the ctor leaves it.
- P3 = A2+0x50: a VA only (ActionPress reads it as a 32-bit scalar); its content is not read by ActionPress.

Missing, with the blocker named:
1. **Translation and proof of `FUN_140961F00` and `FUN_140512CA0`.** Both are small, but no hook covers their entries, and a root needs a
   captured call to be proven (`prove_bounded`). The captures hold the ctor's bytes only after later writes, so they cannot prove the
   stores directly. Needs a hook: `action_ctor_140961F00` (entry, FIRST 1 EVERY 1, cap 6), which Main owns.
2. **The window region.** GF's player block is 0x3F40 bytes (`kPlayerBlockBytes`), and M is at least 0xF620 bytes, so A0 cannot sit
   in GF's block. A new per-slot press region in `PesWorldImage` (A0 0x100 bytes, A2 0x60, P3 0x40, at the offsets above) is needed.
   It changes `kWorldBytes`; every window static_assert must still hold.
3. **The per-tick writers of A0 and A2.** The driver (`FUN_1405141F0`, 0x740 bytes) and G write A2+4, A2+0x38, A2+0x40 and the A0
   fields (+0x48, +0x58, +0x5C, the stored point +0x68, +0x78) every tick. Calling ActionPress without the driver leaves these at
   their ctor values. That is a ponytail on the first tick only if GF keeps A0 across ticks; it is not PES's per-tick state.
4. **The speed.** ActionPress returns the target (A1) and the planner's ETA only sets A0+0x30. PES's command speed comes from the
   movement solver `FUN_1406B9270` (translated in the closure of ActionPress, unproven) and the speed class `FUN_140A75BE0`. GF's
   command speed for the designated man is still unset: OPEN, and a census run is the only test of it.
5. **The red test.** A PesWorld-level test: a designated man without possession beside a loose ball gets ActionPress's target (= the
   ball) through `_BallControlCommand`. It fails on d4919cc4 because the walk aims at the ball by `AI_GetBallControlMovement`, not
   by ActionPress's target. Writing it needs items 1 to 3.

## 5. Next steps (in order)

1. Main: add the hook `action_ctor_140961F00` (and `man_ctor_14091EDB0`, FIRST 1 EVERY 1, cap 3, to capture M's construction) to prove item 1.
2. Translate `0x140961F00` and `0x140512CA0` with `closure.py` (set `trapaction`, tag `pressctor`); prove on the hook's captures.
3. Add the per-slot press region (item 2) and `PesWorld::RunActionPress(slot)` calling `pes_fn_140962440` with the four registers,
   A1 in the window scratch. Red test (item 5) first, then the walk's removal in the same change.
4. Census (full ALiveBall), PesStallQuick, ankle 9/9, PesHang; `gen_translated.py --check`.

## 6. Verification

- Captures: `pid1502356/seq10193`, `pid1502356/seq846`, `pid1458145/seq1961` (PES bytes, not committed). Scratch: none kept.
- Image: `.pdata` read as flat file offsets (`file offset = VA - 0x140000000`; the section-header mapping gave garbage).
- Headless decompiles: 0x14091EDB0, 0x140512CA0 (base ctor), 0x140961F00 (ctor), 0x1405141F0 (driver), 0x140518E20 (dispatcher),
  0x140518DD0 (thunk, bytes read). The Ghidra bridge was down.
- Scans: E8 callers of 0x140518E20 (8 sites), of 0x140961F00 (1: 0x14091F07D), of 0x140962440 (none).

## 7. Correction: the per-slot object is X, not M (PressWire round 3, 22-11-26)

- `FUN_14091EDB0` (the proven ctor) runs on M = X+0x958. Its caller `FUN_14091E9F0` is X's ctor: it sets X's vtable 0x1426329A8 at X+0
  and the second vtable 0x142632A50 at X+0x950 (unit 0x12A), calls M's ctor, and stores X into the slot table at `0x1436F3FC0 + (side*11+slot)*8`.
- X is allocated by `FUN_1409211E0`: `FUN_1403FB410(0x18, 0x1A5E0, 0, ...)`, then X's ctor, then `FUN_1409227D0`, `FUN_140921F70`,
  `FUN_142101960(X+0x980)`. So PES's per-slot object is 0x1A5E0 bytes (CONFIRMED from the decompile of the allocator).
- A0 = X+0x10E70 (= M+0xF518) and A2 = X+0xACE8 (= M+0xA390) sit inside X. The A2+0x40 owner P (`0x3DB01EE0` in seq10193) is OUTSIDE X
  (X = 0x326E9670..0x32703C50 there): P is GF's trap P (`PVa(slot)`), not a member of X.
- GF's player block (`kPlayerBlockBytes` 0x3F40) is not a member of X: it is the world's per-slot anime/man/slot record block. Neither is P.
- Proven now: M's ctor only (6/6, `f704314b`). X's ctor (0x14091E9F0) and the driver are NOT proven: the hooks pw_14091E9F0,
  pw_1405141F0, pw_140518E20 were requested from Main (round 3).
