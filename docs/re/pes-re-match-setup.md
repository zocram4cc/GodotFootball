# PES match-setup path: the constructors that build the impure sample's state (StateSynth, 25-10-26)

Read-only static RE + a snapshot-emulation design. Image `pes_full.bin`
(offset = VA − 0x140000000); `.data`/`.bss` hold dump-time values. The match
heap (G lived at 0x35a31cd0 at dump time) is **not** in the flat image (the
file covers 0x140000000..0x15B97C000 only), so the object graph must be
rebuilt by running the constructors, not read back. Verdicts: CONFIRMED
(decompiled this pass), OBS (prior doc, VA named), INF (inference), OPEN.

## 0. The objects the impure sample reads

Scanned all 50 impure specs' `regions_init` (the G child links they restore):

| G child | offset | funcs | what it is |
|---|---|---|---|
| MI (match info) | `G+0x270` | 26 | the match clock/phase/scene record (E-block) |
| PA slot-record table | `G+0x1928` (+0x28·i) | 34 | `*(G+(slot+0xA5)·0x28)` (record-layouts §2) |
| PM slot-record table | `G+0x1DD8` (+0x28·i) | 34 | the move/kinematics slot record |
| slot table (0x1950/0x1A8..) | `G+0x19xx..0x1D38` | 25 | second slot table (side-folded) |
| PI pass-aim table | `G+0x25F8` | 13 | `*(G+(slot+0xF3)·0x28)` |
| `G+0x2AA8` | 17 | a ball/field sub-object (fieldinfo §G) |
| BallInfoRef | `G+0x40` | 6 | ball record ref (foundation-matchball F1) |
| table base (stride 0x74) | `G+0x180` | 6 | per-slot table (impure-map spec fix) |
| setup (match load) | `G+0x2AD0` | 1 | `+0xc400+side·0x2804` team blocks (H1) |

So the impure functions branch on **MI**, the **slot-record tables**
(PA/PM/PI), `G+0x2AA8` and the `G+0x180` table. Synthetic/zeroed versions of
these make them exit early (their ~6% coverage). The setup path must build
these.

## 1. The match-context (G) creation and construction — CONFIRMED

Creation: `FUN_140a48650(name, ctx, out)` (0x140a48650), called from
`FUN_141fb1a50` at 0x141fb2808 (a state-machine step, the `cVar3==0` arm):

```
if DAT_1436f3dc8 == 0:
    ptr = FUN_1403fb410(&cls0x18, 0x2e58, 0)   // 11864 bytes, class-0x18 allocator
    DAT_1436f3dc8 = FUN_140a44940(ptr)          // init G fields, return ptr
    FUN_140a4a300(DAT_1436f3dc8)                // construct G's sub-objects
```

- **`FUN_1403fb410(cls*, size, align)`** (0x1403fb410) — the class allocator.
  Reads the per-class allocator object `*(PTR_DAT_1436ef2c8 + cls·0x40)` and
  calls its `vtbl+0x10` alloc (=`FUN_1403fbc10` for the classes that are
  live at dump time, vtbl `0x14258e390`). Retry path calls `FUN_1403fb860`
  then the secondary allocator `PTR_PTR_1436ef2b8`. This is the real PES
  heap; the emulator replaces it with a bump allocator (stated in §4).
- **`FUN_140a44940(G)`** (0x140a44940) — G field init. Sets the 16 embedded
  "registration" sub-object vtables at the 40-byte stride (G[0]=&PTR_LAB_
  142599b58, G[5]=&PTR_LAB_14259afb0, G[10]=&PTR_FUN_14259cb80, … up to
  G[75]), zeroes every inited-flag byte (G[1], G[6], G[0xb], …), and zeroes
  the `+0x2aa0`/`+0x2aa8` fieldinfo block. Pure — no external calls.
- **`FUN_140a4a300(G)`** (0x140a4a300) — G construction. For each of the 32
  registration sub-objects (the 40-byte stride set in 44940, then a second
  batch at +0x52a..+0x557): if its inited flag is 0, calls the sub-object's
  `vtbl[+0x10]` (alloc/init) + `vtbl[+8]`, then the concrete initializer:
  obj0→`FUN_14042aa70`, +0x28→`FUN_140433930`, +0x50→`FUN_140451b50`,
  +0x78→`FUN_1404233f0`, +0xa0→`FUN_1404234b0`, +0xc8→`FUN_140423630`,
  +0xf0→`FUN_140423570`, +0x118→`FUN_140423930`, +0x140→`FUN_1404237b0`,
  +0x168→`FUN_14041f010`, +0x190→`FUN_140451c10`, +0x1b8→`FUN_140a49dd0`,
  +0x1e0→`FUN_1404236f0`, +0x208→`FUN_140451d90`, +0x230→`FUN_140451cd0`,
  +0x258→`FUN_14041bab0`, then the +0x52a batch →`FUN_140458a60`,
  `FUN_140427b00`, `FUN_14042abf0`, `FUN_1404269c0`, `FUN_1404640f0`,
  `FUN_14041bc30`, `FUN_1404258b0`, `FUN_140a4a040`, `FUN_1408720b0`,
  `FUN_140a4a150`. Then:
  - a **3×28 camera-target** loop (G+0x418.., name `"CAMERA_TARGET__%02d__%02d"`);
  - a **2-side** loop (G+0x322, name `"ORDER__%02d"`);
  - a **22-entry player loop** (G+0x3b8, name `"PLAYER_%02d"`, 0..21) and a
    **25-entry loop** (G+0x426, `"PLAYER_R_%02d"` for ≥0x16).
  Each player entry calls three name/label builders (`FUN_140433870`,
  `FUN_140433ab0`, `FUN_14047a4a0`) and (for the 22 loop) `FUN_1404339f0`/
  `FUN_140923200`. These build the per-slot name records that the slot tables
  (`G+0x1928`/`+0x1DD8`/`+0x25F8`) index.
  External calls in this path (must be stubbed, §4): `FUN_1403cf8b0` (string
  format, tail → `*DAT_142530438`), `FUN_141b55730` (logging/message
  registration; Denuvo, will not decompile), `FUN_140a41170` (name lookup;
  Denuvo). Their results feed name strings / a log, not the match state the
  impure fns read.

## 2. The match-analysis (MA) object — CONFIRMED (alloc) + OBS (ctor)

- **`FUN_1404636B0(base, a, b, c)`** (0x1404636b0): `if *(base+0x10)==0`:
  `ptr = FUN_1403fb410(&cls6, 0xeae28, 0)` (966,408 bytes, class-6 allocator);
  `*ptr = 1; FUN_140a6f240(ptr+2); *(base+0x10) = ptr`. The doc's MA is
  `*(G+0x18)`; foundation-matchball notes the `G+0x18` pointer path was found
  dead live and MA was located by scan, so the exact base that owns the
  `+0x10` MA slot is OPEN, but the alloc + ctor are confirmed.
- **`FUN_140a6f240(MA)`** (0x140a6f240) — MA constructor. Clears
  `+0x2482c/+0x24834` (ball analysis point, E5d), `+0x2601c` (carrier id, E5b),
  `+0x2483c` (holder id, E5c), inits the `+0x48` id and the ball rank lists.
  OBS from foundation-matchball E5d/E5b/E5c.

## 3. Team / player-slot / MI constructors — OBS (prior docs)

- **Team record ctor `FUN_140a415a0`** (0x140a415a0) — writes `T+0x255`
  (possession latch), `T+0xb8a4`=2 (attack level), `T+0xa53c`=0, `T+0xa298`=4
  (defensive mode), `T+0xa508`=3 (counter state), `T+0xb6e0`, `T+0xa558`,
  `T+0x210` (target level) init. Init sites 0x140a41656/0x140a416ae/
  0x140a41636/0x140a41604 (constant-team §, team-ai-leftovers).
- **Slot-record build `FUN_14092ED30(slot, out)`** (0x14092ed30) — writes the
  slot record's `+0x1404..+0x1457` rows, the `+0x1A/+0x1C/+0x22` u16 ladder,
  and the `+0x1430/+0x1438` counters (record-layouts §2.1, byte-confirmed
  stores 0x14092f1cf/0x14092f204).
- **MI (match info) writers** (foundation-matchball E5): init
  `FUN_1408ae72b` (`+0x11a0`=10, detector pair, `+0x11a4`=2, `+0x1198`
  vtable), copy `FUN_14100ce84`, reset `FUN_14118221c` (`+0x11a4..+0x11f8`
  zero). The MI base (≥0x1900 bytes, offsets up to `+0x1775`/`+0x1900`) is
  allocated during match load; its full constructor is OPEN (the E-block rows
  cite the per-field writers, not one named MI ctor).
- **Event-ring writer `FUN_140a3dc40`** (0x140a3dc40) — builds the `T+0xE0`
  800-byte ring records (record-layouts §1).

## 4. Emulation design (the snapshot)

Run the setup from PES's own bytes in unicorn:

- **Image**: map the code + `.rdata` + `.data`/`.bss` at their VAs (as
  `run.py` already does). `.data` starts at dump-time values.
- **Allocator**: replace `FUN_1403fb410` (and its retry/secondary) with a
  **bump allocator** in a reserved heap window — stated, per the task. The
  real per-class free-list state is heap-resident and not reconstructible
  statically; a bump allocator gives every ctor a fresh, distinct,
  page-aligned region so vtable pointers and cross-refs stay valid.
- **Stubs** (results feed name/log, not match state): `FUN_1403cf8b0`
  (string format → return 0, write fmt into the out buffer),
  `FUN_141b55730` (logging reg → noop, return 0), `FUN_140a41170` (name
  lookup → return a fixed name buffer). Existing `stubs.json` engine models
  (`sqrt_xmm0`, `noop`) apply as-is.
- **Sequence**: fresh G buffer (0x2e58) → `FUN_140a44940(G)` →
  `FUN_140a4a300(G)` → `FUN_1404636B0` (MA) → team ctor(s) → slot-record
  build. Then snapshot the heap window + the `.data` it wrote.
- **Snapshot → specs**: for each impure function, rebind its `G`/`MI`/
  `PA`/`PM`/`PI` regions to the snapshot's real objects and derive
  per-field case-variation from the snapshot values (class bytes, MI phase,
  slot counters) instead of from nothing.

## 5. What could not be run statically (gaps for a live capture)

- The **match-load entry** that decides which teams/players are loaded (the
  `FUN_141fb1a50` state machine past step 0 calls `FUN_14149a470`,
  `thunk_EXT_FUN_1443a83e0` = a UI/menu external, and `FUN_141794410`) — menu
  / VFS / competition config driven, not reproducible from the image alone.
- **MI's full constructor** — only the per-field writers are named (E5); no
  single MI ctor is CONFIRMED.
- **The `+0x10` MA-slot base** (is it `G+0x18` or a separate match-management
  object?) — foundation-matchball found `G+0x18` dead live.
- **Roster / player-DB load** (Player.bin via VFS) — needs the install VFS and
  the chosen teams; the image has no roster.
- **RNG seed origin** (the LCG `FUN_140a924e0` slot-2 state, the
  RandomInfo producer `FUN_140435EF0`) — set per match from the seed; the
  dump-time value is one session's.
- **Denuvo** (`FUN_141b55730`, `FUN_140a41170`, the 0x141b.. VM range) —
  will not decompile; runnable in unicorn only if it does not anti-tamper
  against the emulated env (must be stubbed for the snapshot).
