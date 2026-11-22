# PES ActionPress inputs: what each one is, and who writes it (PressWire, 22-11-26)

Status: input map only. ActionPress is NOT wired into GF: its presser action object (A0) and dispatch context (A2) have no proven GF
builder yet, so no call was made. The provisional walk (`playercontroller.cpp`, `_BallControlCommand`) is still in place. Census:
unchanged, 151 (last measured on d4919cc4, `census3.log`; no code changed this round, not re-run).

Tags: **CONFIRMED** = read from a real capture by `tools/dev/rendezvous/press_inputs.py`, or decompiled from the image (headless, `dec.sh`),
or scanned in the image; **INF** = inference, reason given; **OPEN** = not settled, blocker named. VAs are PES's (`pes_full.bin`, base 0x140000000).

## 1. Entry registers (CONFIRMED, 3 of 3 captures)

`call.bin` words: index 0 = rax (the action vtable, loaded for the indirect call), 1 = rcx = A0, 2 = rdx = A1, 8 = r8 = A2.
The dispatcher calls ActionPress through the vtable (INF from rax = the vtable in `call.bin`: `call [vtable + k]`). CONFIRMED: no
direct `E8` call to 0x140962440 exists in the image (scanned), so the entry is reached only indirectly. The return address (`[rsp]`) is 0x140518E5B.

| captures | A0 (rcx) | A2 (r8) | slot (A2+4) | A0 vtable | ctx (A2+0x38) | ctx+0 vtable | owner (A2+0x40) | owner+0 vtable |
|---|---|---|---|---|---|---|---|---|
| pid1502356/seq10193 | 0x326F94E0 | 0x326F4358 | 6 | 0x142634948 | 0x324A8AF0 | 0x14259D590 | 0x3DB01EE0 | 0x1425E34D8 |
| pid1502356/seq846 | 0x33844A60 | 0x3383F8D8 | 17 | 0x142634948 | 0x324AD060 | 0x14259D590 | 0x3D8F05E0 | 0x1425E34D8 |
| pid1458145/seq1961 | 0x35093C30 | 0x3508EAA8 | 18 | 0x142634948 | 0x27601CF0 | 0x14259D590 | 0x35B88510 | 0x1425E34D8 |

## 2. Each input: what it is, and its writer

**A2 (dispatch context), a heap object, held from the stack and the static area (CONFIRMED layout, writer OPEN):**
- `A2+0x04` u32 = slot id (6, 17, 18). Read as `A2_4` by the spec. CONFIRMED.
- `A2+0x38` = the team AI ctx. Its vtable at ctx+0 is 0x14259D590 in all three captures. GF's `PesWorld::CtxVa(side)` is the
  side's ctx (`pesworldrecord.hpp`); whether it is the same object is OPEN (GF's ctx has no vtable model). INF: the same object.
- `A2+0x40` = the planner owner: an AnimePlayer P (P+0 vtable 0x1425E34D8 in all three). CONFIRMED.
  - The P vtable is stored at P+0 by `FUN_1406E0408` (headless decompile), which is inside P's constructor. It also stores the two
    interface vtables at P+0xAB8 (0x1425E3508) and P+0xAC0 (0x1425E35B0), the same offsets as GF's
    `kPInterfaceVtable0Offset` / `kPInterfaceVtable1Offset` (`pestrapworld.hpp`). CONFIRMED.
  - So the owner is a GF `PesTrapImage` P (`PesWorld::PVa(slot)`). OPEN: check owner == PVa(A2+4) in a GF run; the capture
    seq1961 holds 25 P objects (`pvalues`), one per slot.
- Who builds A2 (the caller frame): OPEN. The return 0x140518E5B is inside a function whose start is not identified (the
  headless decompile of that address is a fragment: Ghidra shows only the block after the call).

**A0 (the presser action object), vtable 0x142634948 (CONFIRMED vtable; builder OPEN):**
- Constructor block: `FUN_140961F0E` (headless decompile; no direct caller in the image: a fallthrough or jump target of a larger function).
  Its stores (CONFIRMED from the decompile): +0x00 vtable 0x142634948; +0x30 u8 0; +0x40 u8 0; +0x34 u64 0; +0x3C u32 0;
  +0x44 u32 0; +0x48 u32 0xFF; +0x4C u64 0; +0x50 u32 0xBF800000 (-1.0); +0x54 u32 0; +0x58 u32 0; +0x5C u16 0; +0x5E u8 0;
  +0x60 u64 0; +0x68 u32 0; +0x6C u64 0; +0x70 u32 0; +0x74 u8 0; +0x78 u64 0; +0x80 u32 0xBF800000 (-1.0); +0x84 u8 0.
- Not inside any P: A0 is not in any of the 25 P objects of seq1961 (`pvalues`). A0 is held by one heap cell (0x3509DDF0), by
  two static cells (0x22A0108, 0x22A0138) and by stack copies. OPEN: the holder 0x3509DDF0 is the table that names the press
  action (the team's action table or the man's action record); find its writer.

**A0 fields ActionPress reads and writes (CONFIRMED values at entry, 3 of 3; writers OPEN):**

| field | seq10193 | seq846 | seq1961 | reading |
|---|---|---|---|---|
| +0x30 u8 | 0 | 0 | 0 | planner re-plan flag (`n + 10 < eta`, press-life 8.6) |
| +0x48 u32 | 15 | 6 | 4 | marked peer; `< 0x16` means a peer exists (press-life 8.7). Ctor stores 0xFF |
| +0x58 u32 | 24 | 14 | 18 | `== 0` means no stored point yet (press-life 8.3, 8.5) |
| +0x5C u8 | 0 | 0 | 0 | has-base flag (press-life 8.4) |
| +0x68 f32 x3 | (3.851, 0.110, -6.406) | (-9.592, 0.110, -8.538) | (32.216, 0.117, -20.485) | the stored press point (x, y, z); y is 0.11 in all three |
| +0x74 u8 | 0 | 0 | 0 | zeroed by ActionPress (press-life 8.3) |
| +0x78 f32 | 0 | 0 | 0 | arrival-writer result (press-life 8.7) |
| +0x7C f32 | 0 | 0 | 0 | not decoded |

**Globals:** `G = *(0x1436F3DC8)`. The dispatcher's headless decompile loads `G+0x270` (the match info), CONFIRMED. GF models the same
cell as `kWorldGlobalVa` (`pesworldrecord.hpp`); the ActionPress reads (G+0x40 ball, G+0x3B0 skip, G+0x270 MI) are GF's
`PesWorldTables` offsets (static_asserts). The spec also lists `IMG_1436F3DC8_270_1198` (the MI frame counter); its value was not
read in this round (OPEN).

**A1 (output, float[3]):** the spec writes it as the three floats `w_A1_0`, `w_A1_4`, `w_A1_8`. INF: the target point (press-life 8.8,
`return out`). OPEN: which caller consumes it, and where the speed comes from (the planner's ETA only sets A0+0x30).

## 3. What GF has and what it does not

| PES input | GF has | gap |
|---|---|---|
| G (0x1436F3DC8) and its tables | yes: `kWorldGlobalVa`, `PesWorldTables` | none for the reads |
| ctx (A2+0x38) and MA (ctx+0x18) | yes: `CtxVa(side)`, `MaVa(side)` | the ctx's vtable is not modelled; INF same object |
| owner P (A2+0x40) | yes: `PVa(slot)` (P's constructor is `FUN_1406E03D0`) | owner == PVa(A2+4) not yet checked |
| dispatch context A2 | no | builder OPEN (caller frame of 0x140518E5B) |
| presser action A0 | no | builder OPEN (ctor block 0x140961F0E; holder 0x3509DDF0) |
| A0 fields (+0x30..+0x7C) | no | writers OPEN; the census needs the stop/arrival arm |
| A1 consumer | no | OPEN |

## 4. Next blockers, in order

1. The caller frame of the dispatcher: decompile the function that contains 0x140518E5B from its entry (find the start by the
   `.pdata` range of the image, not by the return address). It builds A2 and makes the `call [vtable+k]`. Settles the A2 writer.
2. The holder 0x3509DDF0 of A0 (a heap cell in capture seq1961): read its writer. It is the press action's owner table. Settles
   the A0 writer and whether A0 is per man.
3. The constructor block 0x140961F0E: its enclosing function (fallthrough from the function above 0x140961F0E). Settles who
   calls the press action's constructor.
4. The A0 field writers (+0x48, +0x58, +0x5C, +0x68 point, +0x78): the press writer `FUN_140536FC0` and ActionPress itself
   (press-life 8). Settles the per-tick state.
5. The A1 consumer: the caller that receives the target. Settles the command the designated man gets.
6. Only then wire ActionPress in GF (the red test: a designated man without possession beside a loose ball gets ActionPress's
   command, target = ball). Until 1-5 are proven, a call would read zeroed or stale memory, which is the ponytail this task removes.

## 5. Tools and evidence

- `tools/dev/rendezvous/press_inputs.py fields|holders|pvalues CAPDIR`: reads the entry registers, A0/A2/owner/ctx with vtable checks,
  the holders of any value, and the P objects. Streams `mem.bin.zst` through zstd; nothing unpacked.
- Captures (not committed, PES bytes): `<rendezvous>/live2/pid1502356/seq10193`, `.../seq846`, `<rendezvous>/live2/pid1458145/seq1961`
  (`<rendezvous>` is the rig's scratch folder; pass it to the tool).
- Headless decompiles (`dec.sh`): 0x140961F0E (+ 0x1406E0408, 0x1406E162C) and 0x140518E5B (+ 0x140518DFF). The Ghidra bridge was down.
- Image scans (`pes_full.bin`): RIP-relative `lea` to 0x142634948 at 0x140961F0E only; to 0x1425E34D8 at 0x1406E0408 and 0x1406E162C;
  direct `E8` call to 0x140961F0E and to 0x140962440: none.
