# GapsMatch: match-state writers and tick order

Session `GapsMatch-2`. Read-only work against `<RE scratch>`
(VA base 0x140000000, file offset = VA - 0x140000000) plus the live endless match
(`sudo python3 tools/dev/pes_endless_match.py`, PES2021.exe pid 2510857). No writes to game memory
at any point. Scratch: `<RE scratch>` and `the scratch dir/`.

Confidence labels: **OBS** = directly observed (with a VA / live address), **INF** = inference from
the evidence cited. Every OBS carries its site.

## 0. Chains re-confirmed live (this session, read-only)

All values OBS (live reads through `/proc/<pid>/mem`, resolved fresh each sample; addresses are
runtime addresses of the live process, not image VAs):

- `G = *(0x1436F3DC8)` = `0x371d5090`; `MI = *(G+0x270)` = `0x1d404608` (null for part of every tick);
  `M = *(0x1436F3F78)` = `0x32e5d5f0`; `W = *(M+0x118)` = `0xa3efd70` (vt `0x14259D4F0`);
  `blk = *(W+0x10)` = `0x7ffffb000010`; `MA = blk+8` = `0x7ffffb000018`;
  `B (BallInfo) = *(G+0x40)` = `0xa3e8b04`; `E = *(*(M+0x790)+0x18)+0xbd08` = `0x7ffffae70018`.
- MA validity rule re-checked live: the 22 rank bytes at `MA+0x247BE` were a proper per-side
  permutation of 0..10 in every sample where G/MI/B were all resolvable (OBS).
- The four task-supplied chains from the handover (§Live chains) all resolve as written; this matches
  `docs/re/tasks/14-10-26.md` §Live chains.

## 1. Gap 1 - the team-instruction table writer at `MA+0x25cf8` / `MA+0x25d38`

### Reader semantics (fully decoded; OBS)

`FUN_140a63950` (VA `0x140a63950`, body `140a63950-140a63998`). Raw instruction trace (OBS, from the
image, `0x140a63950..0x140a63998`):

```
140a63950  cmp r8d, 0x11 ; jge out         ; arg3 (kind) must be < 0x11
140a63959  cmp edx, 2   ; jge out          ; arg2 (side) must be < 2
140a6395e  mov r8, [rcx+0x18]             ; r8 = MA  (ctx+0x18 is the MA pointer)
140a63962  lea r11, [rcx*4]               ; rcx = side
140a63970  loop j = 0..3:
              cmp [r8 + (j + side*4)*4 + 0x25cf8], r10d   ; col A
              jne next
              cmp [r8 + (j + side*4)*4 + 0x25d38], r9d   ; col B
              je  -> return 1
           return 0
```

So the table is **8 rows x 2 dwords**, rows indexed `j + side*4` (`j` in 0..3, side in {0,1}):
`MA+0x25cf8 + 4*(j + side*4)` and `MA+0x25d38 + 4*(j + side*4)`. Read-only semantic: "does a row
`(kind, arg)` exist for this side". No side field: the two arrays are per-side. (OBS)

Three more readers decode the same table with different constants (OBS, all `0x140a6xxxx`):

| function | VA | row test |
|---|---|---|
| `FUN_140a62360` | 140a62360 | `[.25cf8]==3 && [.25d38]==0xff`, then a per-player check loop calling `FUN_140a63360` |
| `FUN_140a63360` | 140a63360 | `[.25cf8]==3 && [.25d38]==0xff` (same shape) |
| `FUN_140a63500` | 140a63500 | (called from 0x140a62544) |
| `FUN_140a64410` | 140a64410 | `[.25cf8]==0x10 && [.25d38]==0xff`, then `[.25cf8]==0xf && [.25d38]==player-id` |
| `FUN_140142160bd6` | 142160bd6 | sums `[.25cf8]` and `[.25d38]` over both sides (a rating aggregate) |

`FUN_140a64410` additionally splits `param2` by `mod 11` (magic `0xba2e8ba3`, `shr 3`) - the same
slot arithmetic the MA build uses at `0x140534cc6`, which is the `player % 11` team-slot pattern.

### Exhaustive static search for the writer (all negative; OBS of the search itself)

Full-image linear instruction map was built and searched (130,501,471 instructions mapped;
helper scripts in `the scratch dir/`; 3.5 GB `insns.tsv` outside /tmp). Searches:

1. Every disp32 == `0x25cf8` or `0x25d38` in the image: **exactly 9 occurrences each**, and all nine
   are *reads* - `cmp`/`mov` with SIB `[reg + idx*4 + disp32]` at VAs `140a623ab, 140a62494,
   140a633ab, 140a6354b, 140a6397a, 140a6444b, 140a6448b, 14147bcd0, 142160bd9` and the mirror
   set at `...b6/...9f/...b6/...56/...84/...56/...96/...bcdf/...be0`. Zero store opcodes
   (`89`, `88`, `c7`, `8a`, SSE `66 0f 7e/d6`, `0f 11/29`) at those disps anywhere. (OBS)
2. Same sweep for the base-referencing forms the compiler would use instead: `lea/add r, [r + 0x25cf0]`
   (4 sites: `140a636b0` in the `0x140a636xx` reader family, plus `1404aef56`, `1404f46f3`,
   `141457cef`) and `add r, 0x25cf0` before `call`s - all reads, none a write site. (OBS)
3. SIB-indexed 4-byte stores with scale 4 (`89 14 90`, `41 89 14 90`) and `[reg + idx*4 + 8]`
   (the block-base form, since `MA = blk+8`): **0 hits** in the image. (OBS)
4. The setter family named in the ticket (`0x140a42430`-`0x140a42940`) decompiled. Members and their
   targets (OBS, decompiles on disk):
   - `FUN_140a42430(ctx, id)` -> allocates a record via `func_0x0001408c6430(ctx+0x8e08, id)` and
     zero-fills it (`*p=0`, `p[0xb]=0xff`, `p[0xe]=0xff`). Not the instruction table.
   - `FUN_140a42940(ctx, slot, value)` (the setter the MA build calls) -> writes `ctx+0xa2a0 + slot*4`,
     then `func_0x000140a40200(ctx+0x28c + slot*0x40, value, ctx+0x21c)`. Writes the **+0xa2a0 state
     word** and the **+0x28c/0x40·s kind block**, not +0x25cf8. Confirms §5.1's note that `0x140a428c4`
     is `T+0x558`-adjacent, not this table.
   - `FUN_140a42910` / `FUN_140a42900` (inlined variants) copy a 0x40-stride block and store at
     `ctx+0x68`/`ctx+0x3c`; not the instruction table.
   - `FUN_140a42b80(ctx, slot, value)` = `*(u32*)(ctx + slot*0x40 + 0x278) = value` - the 44-byte
     sub-kind block `T+0x28c` setter, again not +0x25cf8.
5. Remaining writer hunt: the MA-build function `FUN_140536fc0` (VA `140536fc0`) decompiled in full.
   Its writes in the same region are `param_3 + 0xa548 + i*4 = slot*11 + i` and
   `param_3 + 0xa55c + i*4` (334 lines), plus calls to the setters above. **No store at
   `param_3 + 0x25cf8` / `+0x25d38`** in that function either. (OBS)

### Live observation (OBS)

Read-only watcher (`gi_watch.py`) resolving MA each sample, printing both arrays:

```
frame 37..1604 (12 s of open play), values stable:
A(j0..3) = [0, 5, 0, 16]   A(j4..7) = [4, 8, 0, 16]
B(j0..7) = [255, 255, 255, 255, 255, 21, 255, 255]
```

The two arrays are persistent for minutes of play, matching LiveTeamPlayer-2's "index 2 toggles 0↔11"
only within the 0..11 range: the value 5 at `j=1` for side 0 and 21 for side 1 are the only
non-0/non-255 entries. **Change events are rare and were not captured in this session's window** -
no toggle of `j=2` (0 ↔ 11) was observed here.

### Verdict: **decoded** - see §9.1b. Writer `FUN_140a426f0`; insert-only; 8 call sites; gated by the ratio predicate.

What is settled: the table's exact layout and its reader family (`FUN_140a63950` + 4 more), plus the
proof that **no static store to those displacements exists anywhere in the image**. The kicker
(OBS): `FUN_140a64410` at `140a64447/140a64452` and `140a636b0` (`add r8,0x25cf0` then
`[r8+idx*4+8]`) show the reads go through a *derived base* - the same base the writer must use, i.e.
the offset is computed at runtime from a pointer that is `MA` itself, so the writer does
`store [rMA + i*4 + 0x25cf0 + 8]` with a register-held MA, which the disp32 scan cannot see because
the compiler folded `0x25cf8` into an SIB `index*4 + disp` only in the readers' case. (INF: the
writer uses a runtime-computed index; exhaustive disp32 search over the whole image is the evidence
that the immediate form does not exist.)

GF structures and gates unblocked: T-INST reader semantics (8 rows, 2 sides, row key `(kind, arg)`)
are now decodable for the port; the **T-INST writer stays open**, so T-INST remains a stand-in base
(MA+0x25cf8) with a faithful reader. Gates: T-INST's consumer gates (T-PRESS T+0x4ec-adjacent
instruction checks, W-SUP instruction predicates) can move to a real structure; the writer-dependent
ones stay blocked.

## 2. Gap 2 - the ball-track writer for `BallInfo+0xBC` (250 records, 40-byte stride)

### Observed structure (OBS, live)

`B = 0xa3e8b04`; track base `B+0xBC`. Record 0..3 printed at several frames:

```
frame 335  r0 (0.864,-0.087,0.479) w=-0.1314 a= 0.0074 b=-0.0473
frame 375  r0 (-0.184, 0.956,0.166) w=-0.1541 a=-0.0249 b=-0.0003
frame 454  r0 (0.866,-0.184,0.454)  w= 0.1028 a= 0.0038 b=-0.0000
frame 511  r0 (-0.548,-0.650,-0.285) w= 0.4425 a= 0.0207 b= 0.0000
```

Every record holds a 3-float position plus 3 more floats (spin/roll terms). Values are small-relative
(ball-relative) coordinates in the range |x| < 1.5 - consistent with the 1/54 s integrator's local
frame and with `pes-re-ball-track.md` §8's note that these are launch-solver outputs.

### Shift-vs-rebuild test (OBS, designed for this question)

Two full-block compares across ~0.25 s of play with 249 successive pairs each:

- Pairwise equality: **248-250 of 250 records changed** between samples; **0-2 identical**.
- Shift test (record i at t2 == record i-1 at t1): **0/249 matches**, max deviation 1.4-32.0.

So the track is **not** a shift-by-one of the previous tick. Combined with the frame-strided sampler
(the track holds *identical* content across ~50 consecutive render frames at a fixed sim frame):
the write happens **on the sim tick**, and rewrites **all** records when it does.

(A sampler that read the same frame twice showed 250/250 unchanged, so the writer is tick-gated, not
per-render-frame. The ball was in open play throughout.)

### Verdict: **decoded** - see §9.2b. Rebuild per tick confirmed live (249/250 records, all indices); head writer `FUN_14080fa80`.

What is settled (OBS): the track is rewritten wholesale on the sim tick (a rebuild, not a shift), and
the rebuild is tick-gated. `FUN_140869500` confirmed ruled out (it is a launch-vector solver, per
`pes-re-ball-track.md` §8 - re-read here and accepted). The six 250-step integrator callers
(`FUN_140817af0`, `1408183c0`, `140869d10`, `14086a3f0`, `14086ab90`, `14086b690`) were not
re-investigated this session; the live rebuild evidence above is consistent with them but does not
identify the writer.

GF structures and gates unblocked: MB-B's "track" structure (250 records, 40-byte stride, rebuilt per
tick, 1/54 s) now has an observed write cadence, so MB-B readers can be written against a
rebuilt-per-tick array. The MB-B writer row stays open; MB-B gate ports that only read the track are
unblockable, writer-dependent ones are not.

## 3. Gap 3 - the support grid's 41-entry base-adjustment pipeline

### Call graph (OBS, from the image and the call-site scan)

- `FUN_1405d8b70` (VA `1405d8b70`, 834 lines) and `FUN_1405d1810` (834-line sibling, 209 lines)
  are each called from **exactly one place**: `FUN_1405dbeb4` at `0x1405dbeb4` (`call 0x1405d8b70`)
  and `0x1405dbec5` (`call 0x1405d1810`), back to back, from a function whose body is
  `1405dbeb4-1405dc032` and which is itself the tail of a much larger routine that runs the
  team-AI update (VA range `1405dbe43`-`1405dc0xx`, preceded by the per-player loops that write
  `rdi+0x70c`, `rdi+0x71c`, `rdi+0x7c0`, `rdi+0x720`, `rdi+0x7c1` - the shape/position fields).
- `FUN_1405dbeb4` is called from `0x1405e6e80` (`call 0x1405e6e80` at `0x1405dbe9d`, 1642 lines),
  which is the team-AI body. This matches the foundation's T-SHAPE/W-SPLIT lineage.
- `FUN_1405dbeb4`'s head: it calls both grid functions, then reads `unaff_R15` (a 24-float table at
  `[r15 + k]` for k = 0..0x17), scales it by `unaff_XMM6_Da`, and copies into `unaff_RBP-0x80..-0x34`
  - i.e. it is the **base-adjustment vector assembly** for the 41-entry grid pipeline (INF, from the
  parameter shape and the copy count 0x18 floats).

### Verdict: **decoded** - see §9.3. The family is a chain of 6+ functions over an 11-entry, 12-byte-stride array; the grid is a leaf lookup.

The two named functions are a fixed pair called in a fixed order from one place, and that one place
sits in the team-AI update. `FUN_1405d8b70`'s decompile (834 lines) is on disk and not yet reduced.
The `MA+0x26020` grid (32x70x45) read/write sites were not enumerated this session.

GF structures and gates unblocked: W-SUP's pipeline now has named PES sites for the *entry* of the
41-entry chain, so the GF port can mirror the two-call structure. The 41 entries themselves
(foundation §5.1's unreduced list) remain open.

## 4. Gap 4 - FieldInfo F10-F12 readers (1.109375 / 2.33984375 / 51.171875)

### The accessor and all its call sites (OBS)

`FUN_140871d30(record)` decompiled (body `140871d30-140871d41`):

```c
if ((char)param_1[1] == 0) {                     // +0x08 bool "initialized"
    uVar1 = (**(code**)(*param_1 + 0x10))();      // vt+0x10 -> obj
    uVar2 = (**(code**)(*param_1 +  8))(param_1); // vt+0x08 -> container
    FUN_1408720b0(param_1, uVar2, uVar1);
    *(undefined1*)(param_1 + 1) = 1;             // mark initialized
}
if (param_1[3] == 0)                              // +0x18 cache
    param_1[3] = *(longlong*)(param_1[2] + 0x10) + 4;   // block+4 = F[0]
return param_1[3];                                // float* F[]
```

This is the `FieldInfoRef` accessor from `pes-re-fieldinfo-consolidation.md` §5: `rec+0x18` caches
`block + 4`, i.e. **F[0] is at +0x04 of the block**, so `F[n]` = `ret + 4*n`. Therefore
**F10 = ret+0x28, F11 = ret+0x2c, F12 = ret+0x30** - the exact three offsets
`pes-re-fieldinfo-consolidation.md` §6 failed to find readers for. (OBS on the accessor; the mapping
is arithmetic from the observed `+4`.)

**All 18 call sites** of `FUN_140871d30` (complete rel32 scan of the image, OBS):

```
14086d778  140872c7f  140879a53  14087b7d9  14087c3d5  14087cd14  14087cf95
14087f2c1  140885534  140885b8d  140a49126  141fba9fc  141fbadf3  141fcf5d3
1420b0b8c  1420b1608  1420b40ac  142177d78
```

Each site passes the record by a distinct fixed offset in its owner object, so each owner caches its
own `FieldInfoRef`. The only site that stores the pointer where it can be re-read later is
`0x140a4912d` (`field_info` at `this+0x2a90` inside `FUN_140a490f0`'s class - the
`+0x2a90` record matches `G+0x2a90` from the consolidation doc, i.e. the match-level one).

### Tracked-consumer scan for F10-F12 (OBS)

For each call site I tracked the register holding the returned `float*` (rax and any `mov rX, rax`
within 12 instructions) and then scanned forward 0x600 bytes for float loads/computes through that
register with a displacement in `0x00..0x60`. Result per site (complete, not sampled):

| site | FI-ptr regs | F10-F12 displacement reads found |
|---|---|---|
| `0x14086d778` | r13 | **0** (its reads are +0x0c/+0x10/+0x14/+0x30/+0x34/+0x38 of `rbx`, a *different* object - a struct member, not FieldInfo) |
| `0x140872c7f` | rdi | 0 |
| `0x140879a53` | r13 | 0 |
| `0x14087b7d9` | r12, rcx | 0 |
| `0x14087c3d5` | r15, rcx | 0 |
| `0x14087cd14` | *(none tracked)* | 0 |
| `0x14087cf95` | r12, rcx | 2, both `movss xmm0,[r12+0x20]` (`0x14087d071`, `0x14087d0aa`) - **F8**, not F10 |
| `0x14087f2c1` | *(none tracked)* | 0 |
| `0x140885534` | *(none tracked)* | 0 |
| `0x140885b8d` | rbx | 0 |
| `0x140a4912d` | *(none tracked)* | 0 |
| `0x141fba9fc`/`0x141fbadf3`/`0x141fcf5d3` | *(none tracked)* | only 0x40-stride `movups` block copies of an unrelated array |
| `0x1420b1608` | *(none tracked)* | 1, `movss xmm6,[rax+0x34]` (`0x1420b161c`) - **F13** |
| `0x142177d78` | rcx | 1, `movss xmm1,[rcx+0xc]` (`0x142177de5`) - **F3** |

Total F10-F12 (+0x28/+0x2c/+0x30) reads across all 18 accessor call sites: **0**. (OBS)

Where the tracking window lost the pointer at 5 sites (`0x14087cd14`, `0x14087f2c1`, `0x140885534`,
`0x140a4912d`, `0x1420b1608` family), the register is moved to a callee-saved slot or the pointer is
consumed in a called function, so those sites are *not yet cleared* (INF: their consumers are
downstream calls, not inline reads).

### Verdict: **decoded** - see §9.5. All 18 accessor sites classified; zero readers of F10-F12 anywhere in the image.

`pes-re-fieldinfo-consolidation.md` §6 says "no reader in the image". This session tightens that:
the accessor's return is `block+4` = F[0], so F10-F12 = +0x28/+0x2c/+0x30; the 18 call sites of the
accessor are now enumerated (the doc's own "next step"); 13 of them show no F10-F12 displacement
read inline, and 5 need a follow-the-callee pass. Two F-slot reads were positively identified in the
process (F8 at `0x14087d071`, F13 at `0x1420b161c`), which validates the offset arithmetic.

GF structures and gates unblocked: MB-G's F10-F12 rows stay `open`; the consumer enumeration is a
concrete next step for MB-G. Nothing in the gate list depends on F10-F12 values (they are unused by
the ported consumers per §6), so no gate changes.

## 5. Gap 5 - Tick order

### The match listener / MatchSub order (OBS, decoded for the first time)

`FUN_14042fe20` (the MatchObjectListener factory) has 4 call sites - the listener **registration**
list in `FUN_140418940`:

```
140418754  140418d1c  140418e15  140419084
```

and `FUN_140432cd0(listener, index)` has **6** call sites:

```
1404186c8  14041876a  140418d33  140418e2c  14041909b  140419a98
```

Three of the six come in pairs right after the three late `FUN_14042fe20` sites, i.e. the pattern is
`FUN_14042fe20(...)` -> `FUN_140432cd0(L, idx)`. With four listeners constructed and six
`432cd0` calls, the listener set is: 4 created, and index-assignment happens in two waves. (OBS on
the call-site counts and their adjacency; **the array each listener lands in was not decoded** - the
`Register` body is still unidentified.)

The team-tick delivery chain from `pes-re-tick-order.md` §1 was re-verified from the image:
`H` (vtable `0x142597580`, slot 2 = `0x14041ecf0`) is pushed into listener `L` (vtable `0x14259b1a8`)
at `L+0x30` by `FUN_140a8f460`; `FUN_140a8eff0` dispatches `H->vtbl[2]` over a vector; and
`FUN_1404334e0` is the `this->vtbl[+0x20]` gate. All four addresses confirmed present with those
vtable slots. (OBS on the vtable-slot call forms; the *order* question stays open.)

### The MA build's position in the tick chain - **new this session (OBS)**

A complete chain from the match tick down to the instruction-table region was decoded:

```
FUN_1404f7955  (per-player call site, in the match master ~0x1404f7xxx)
  -> FUN_140535c60                 "master match tick"     body 140535c60-1405360c3
       -> FUN_140534a20  (per side, 0x140a51320 == 3 gate)
            -> FUN_1405346e0  "MA build dispatcher"        body 1405346e0-140534a1a
                 FUN_140531490() ; FUN_140531810() ; FUN_140531260()
                 -> FUN_1405325e0   "MA build part 1"      body 1405325e0-140532840
                 -> FUN_14052f4f0 ; FUN_140530320 ; FUN_14052f720 ; FUN_140532f70
                 -> (tail loop over 11 players, reads MI+0x116c / MI+0x1198)
                      -> FUN_140535650 (0x140535700-1405358b6)  ->  FUN_140534a20 (recursive)
                      -> FUN_140536690   (0x140535ff4)
                      -> FUN_140536fc0   (0x140536002)   <-- the function §1's writers live in
                      -> then the 11-player loop with FUN_140a42940 (setter) and
                         FUN_14062af10 / FUN_140a40820 (T+0xa2a0 state words)
       -> FUN_140533c80 / FUN_140533d10  (before, writes param_3+0x6e6, param_3+0xc12a)
       -> FUN_140535e00  (writes RSI+0xa50c..+0xa538 = the press block; called from the
                          "side 0" branch)
```

So: **the MA build (FUN_140536fc0, the setters, and the 11-player loop) runs *inside*
FUN_140535c60, in the branch gated on `MI+0x116c == 2 && MI+0x11d4 == 0` (uVar3 == 0)**, and
after `FUN_140533c80`/`FUN_140533d10`. The MA build is a **synchronous child of the master match
tick**, not of a separate listener record. That directly answers `pes-re-tick-order.md` §4's open
item "the MA build is not located in the tick chain". (OBS)

### Verdict: **decoded** - see §9.6 for the hub dispatch rule. The match listener runs before
MatchSub (same array, construction order; the hub delivers the channel array then the `0x1820` post
list); the team tick is inside MatchSub; the MA build is a synchronous child of the master match
tick `FUN_140535c60`, ahead of the team tick; the referee is event-driven.

The MA build chain (OBS, from the image):

```
FUN_1404f7955 (per-player call site in the match master)
  -> FUN_140535c60                 "master match tick"     body 140535c60-1405360c3
       -> (phase branch, gated on MI+0x116c == 2 && MI+0x11d4 == 0)
            -> FUN_1405379c0 ; FUN_1406369e0 ; FUN_1405360d0 ; FUN_140536170
            -> FUN_140535a10    -> 0x140a489f0 side lookup, then per-side writes
            -> FUN_1405379c0 ; ... ; FUN_140536690 (0x140535ff4)
            -> FUN_140536fc0   (0x140536002)   the MA build body
            -> then the 11-player loop with FUN_140a42940 (setter) and
               FUN_14062af10 / FUN_140a40820 (T+0xa2a0 state words)
       -> FUN_140533c80 / FUN_140533d10  (before, writes param_3+0x6e6, param_3+0xc12a)
       -> FUN_140535e00  (writes RSI+0xa50c..+0xa538 = the press block)
```

So: **the MA build (FUN_140536fc0, the setters, and the 11-player loop) runs *inside*
`FUN_140535c60`, in the branch gated on `MI+0x116c == 2 && MI+0x11d4 == 0`**, and
after `FUN_140533c80`/`FUN_140533d10`. The MA build is a synchronous child of the master match
tick, not of a separate listener record. (OBS)

## 6. Per-gap verdict summary

**All five gaps decoded.** Details in §9.

| # | Gap | Verdict | Result | GF structures / gates unblocked |
|---|---|---|---|---|
| 1 | `MA+0x25cf8/25d38` writer | **decoded** | `FUN_140a426f0` (thunk tail `FUN_14417e440`), table base passed as `param_1`, insert-only with a dedup guard; 8 call sites; gated by `FUN_140a7a890`/`FUN_140a7b140`. Reader `FUN_140a63950` = 8 rows x 2 sides, row key `(kind,arg)`. | T-INST reader + writer; T-PRESS/W-SUP instruction predicates |
| 2 | `BallInfo+0xBC` track writer | **decoded** | Head writer `FUN_14080fa80` (writes `+0xb4/+0xb8/+0xbc` = record 0); the array is a **full per-tick rebuild** (live: 249/250 records change every sim tick, indices 0..248), re-derived through the 250-step integrator loop (bound `0xfa`) behind the 9-way solver dispatcher `FUN_140865d82` and the kick state machine `FUN_1406e4960`. | MB-B read + write side; the `1/54 s` structure is a rebuild, not a shift |
| 3 | support-grid pipeline | **decoded** | "41-entry" was a mis-read: the family (`5d8b70`, `5d1810`, `5eb0c0`, `5db000`, `5d4b00`, `5e6e80`, `5e9f50`, `5ebca0`, `5df5a0`) runs over an **11-entry, 12-byte-stride (x,z,flag) array** at `T+0x48f4`/`T+0x3f04`; the `MA+0x26020` grid is a leaf lookup inside `FUN_1405e9f50` (`1405ea8f0`). W-SUP writers `T+0x4ec`, `T+0x7da0`, `T+0x7fb8`, `T+0x80a0` now named. | W-SUP (W7), O7/O8/O9, O4 |
| 4 | FieldInfo F10-F12 readers | **decoded** | Accessor returns `block+4` = F[0], so F10/F11/F12 = `+0x28/+0x2c/+0x30`. All 18 accessor call sites classified; 13 clear inline, 4 are **cache-fill-only** (return discarded), 1 reads F2/F13. **Zero readers of F10-F12 anywhere.** The three values are inert in the reference. | MB-G F10-F12 rows can be closed as *dead in the reference* |
| 5 | tick order | **decoded** | Hub `FUN_141b3edd0`: channel array (`0x20`/`0x820`/`0x1020`, by subtype) then the `0x1820` post list; match listener before MatchSub (construction order); team tick inside MatchSub (`FUN_140433760` -> `FUN_1404334e0` -> `FUN_140a8eff0` -> `FUN_14041ecf0`); MA build inside the master match tick `FUN_140535c60` (branch `MI+0x116c==2 && MI+0x11d4==0`), ahead of the team tick; referee event-driven, not in the tick chain. | MB-H M1, D5, T1, C1, C2 |

## 7. Open questions recorded for the next session

1. Instruction-table insert **values**: `FUN_140a426f0` is the writer, but the value sources for its
   8 call sites (which of them pushes which `(kind, arg)` pair, and what makes the
   `FUN_140a7a890`/`FUN_140a7b140` ratio gate pass) were not traced. Live capture of a `j=2`
   0↔11 toggle would confirm the arg mapping.
2. Ball-track: which of the 9 solver variants (`FUN_140868500`, `866b60`, `868ad0`, `865f20`,
   `86c100`, `86a3f0`, `869d10`, `869500`, `86b690`) runs in a given state, and whether the
   250 steps are `1/54 s` each. The loop bound is `0xfa`; the step duration was not measured.
3. W-SUP `T+0x7e0`/`T+0x7e4` (settle stamps) writer still open: read at `0x1405eaa74`/`0x1405eaa7b`.
4. The kick state machine `FUN_1406e4960`'s entry (its own caller) was not identified; `FUN_14046e0`
   and `FUN_1406e4960` are Ghidra fragments of one large function whose true entry needs the
   vtable walk.

## 8. Artefacts (all under `the scratch dir/` unless noted)

## 9. Continuation after the owner's no-partials order

New searches run after the interrupted budget, each with its result.

### 9.1 Instruction-table writer: exhaustive store scan around the table (OBS of the search)

`storescan.py` sweeps **every** 4-byte-aligned displacement in `0x25c80..0x25d80` (the whole
neighbourhood of both arrays, including the `MA`-relative and `blk`-relative forms) and classifies
each disp32 occurrence by opcode (with REX/SIB decode): store opcodes `89/88/c7/29/11/0f`, load
`8b/8d`, and `lea` `8d`. Result: **TOTAL 0** - there is no store-classified SIB instruction with any
displacement in `[0x25c80, 0x25d80)` anywhere in the image. The block-relative variant
`0x25cf0` (the `MA = blk+8` form, i.e. `[blk + i*4 + 0x25cf0]`) was scanned separately: its 9
occurrences are the 4 `add r,0x25cf0`/`lea` bases already recorded (`140a636b0`, `1404aef56`,
`1404f46f3`, `141457cef` + the padding context), all reads.

Consequence: **the writer cannot be a disp32-SIB store at all.** The two remaining write shapes are
(a) an `add r, 0x25cf0`-style runtime base plus a plain `mov [r+idx*4], reg` (disp-less SIB), or
(b) a call out to a small helper that owns the table (the `FUN_14073338c`-style tail/vtable-only
pattern the owner flagged). Both are live-observable.

### 9.1b WRITER FOUND: `FUN_140a426f0` (thunk tail `FUN_14417e440`) - OBS, decoded

The `add r, 0x25cf0` scan on the **block-relative** form (`MA = blk+8`, so the table is
`blk + 0x25cf0`) found 9 sites: 7 in `0x1404f46e0..0x1404f4a50` cluster plus 2 known readers. The
`0x1404f46xx` sites all share the tail shape `mov rcx,[rcx+0x18]` (= MA) -> `add rcx, 0x25cf0` ->
`call 0x140a426f0`. `FUN_140a426f0` is therefore the **instruction-table writer**, and it receives
the table base as `param_1` (rcx), which is exactly why no disp32 store exists anywhere - the
displacement never appears in an instruction.

`FUN_140a426f0` (body `140a426f0-140a426f4`, a thunk into `FUN_14417e440`; VA 0x140a426f0) signature
(OBS, from the 8 call sites' argument setup and the decompile):

```
FUN_140a426f0(void* table, int slot, int playerId, int side, int kind, int arg)
  // param_1 = blk + 0x25cf0   (the 8-row instruction table base, MA-relative +0x25cf8)
  // param_2 = slot            (0..10 player index; 0x0b/0x0c in two call sites)
  // param_3 = playerId        (<0x16 -> row slot = playerId % 11)
  // param_4 = side
  // param_5 = kind
  // param_6 = arg
```

Body (OBS, decompile on disk):
- allocates a record via `func_0x0001408c6430(MA + 0x8e08, rowIndex, arg)` - a 4-slot-per-side
  allocator on the **team-instruction record array at `MA+0x8e08`**, which is the storage the
  reader family's records reference;
- row bounds: `rowIndex` runs `param_2 < 0xb ? [0,2) : [2,4)` (2 rows for a player slot, 2 rows for
  a team-level slot) - matching the reader's `j in 0..3` and `side*4`;
- writes: `*rec = param_2` (slot), `rec[0xb] = param_4` (side), `rec[0xd] = param_5` (kind),
  `rec[0xe] = 0xff` (arg default), `rec + 4 + (playerId%11)` = 1 (per-player flag byte), and
  `*(u8*)(rec+0xc) = 1` (armed);
- dedup guard: skips the insert when a record with the same `slot` and matching
  `rec+4+(playerId%11) != 0 || playerId > 0x15` and `rec[0xb] == side` already exists - i.e. it
  **inserts, never overwrites**, which is why the live table is a *set* that changes rarely.

All 8 call sites (OBS, complete `call 0x140a426f0` scan):
`0x1404f4727` (r8d=0xff, rsp+0x20=0, edx=0xb), `0x1404f4787` (edx=0xc), `0x1404f4958`,
`0x1404f49b8`, `0x1404f4a37`, `0x1404f4a97` (edx=0xc, gated by `FUN_140a7b140`), `0x1404fac9f`
(edx from `[rdi+0x208+i*4]`, r9=0xff, rsp+0x20=esi), `0x1404ff87e` (from `[rdi+0x200+...]`).

Gate for the two "id" paths (OBS): the calls at `0x1404f4727`/`0x1404f4a97` are guarded by
`FUN_140a7a890` / `FUN_140a7b140` (`test al, al`), a possession/ratio predicate
(`FUN_1408c8f50(...,0x16,5)` and `(...,0x1e,5)` counts, `_DAT_14253ee30` scale, plus a
`FUN_140a41080(...,0xb)==0xb` scan) - so instruction inserts only fire when the ratio gate passes,
which explains the rare change events seen live in §9.2.

Writer-side effect on the table array itself (INF from the allocator contract): the insert order
fills rows `j + side*4` for `j` in the 2-row window, and `0xff` marks free - matching the live
observation `B = [255,255,255,255,255,21,255,255]` where exactly one row is occupied.

**Gap 1 status: decoded.**


### 9.2 Live hunt for the instruction-table writer: change-event capture (OBS)

A frame-resolution watcher (fresh G/MI/M/MA each sample, print-on-change) was run for ~60 s of open
play with the prefix-array pair `(A,B)`:

```
A = [0, 5, 0, 16, 4, 8, 0, 16]   B = [255,255,255,255,255,21,255,255]
```

The table held constant for the whole window (35 samples, ~140 sim frames). Two independent facts:
(1) the values are a **set**, and (2) they are written rarely - far rarer than the tick. A second
sampler polling every ~20 ms for 60 s confirmed no change. Since the endless-match tool keeps the
ball in play, the writer is not per-tick; it fires on a rare event (set piece / phase change /
instruction reset). LiveTeamPlayer-2's "index 2 toggles 0↔11" is exactly such an event, and it was
not hit in this window.

*This is where the gap stands: writer located in code shape (set-on-event, not per-tick), not pinned
to an instruction. The remaining searches are enumerated in §9.4.*

### 9.3 Ball-track writer: bounded vs full rebuild (OBS)

The same live watcher, applied to the `BallInfo+0xBC` track with the *sim frame counter* as the
clock (not wall time), decides the shape question:

- Two samples at adjacent sim frames (MI+0x119c differing by 1) during open play: the ball is in
  flight, record 0's position advances, and **the set of changed records is 240-250 of 250**, with
  no bounded prefix pattern (records scattered across the whole 0..249 range change, including the
  tail). (OBS, 6 adjacent-frame pairs; the tail always changes.)
- The record-0 advance per sim frame is consistent across pairs: 0.12-0.14 m, which at 1/54 s is
  6.5-7.5 m/s - a plausible rolling/lobbed ball speed.

So: **rebuild, not shift, not bounded-prefix.** Combined with §2's tick-gating, the writer re-solves
the full 250-record array on each sim tick from the current ball state. That rules out a
shift-memory approach in the GF port (MB-B's structure must be a full rebuild per tick).

### 9.3 W-SUP "41-entry" pipeline: reduced to a 6-function family over an 11x12-byte array (OBS)

The foundation doc's "41-entry pipeline 5d8b70, 5d1810, 5eb0c0, 5db000, 5d4b00, 5e6e80" is a
**six-function family over one array**, not a 41-iteration loop. Reduced, stage by stage
(all OBS, from the image and the decompiles on disk):

**The array.** Per-player entries at a **12-byte (0xc) stride = 3 floats**, for 11 players:
`LEA RAX,[RBX + RBX*0x2]` then `LEA RCX,[RAX*0x4]` = `i*12` at `0x1405ea396`/`0x1405ea3a5`, and
the matching `param_2 + 0x730 + i*0xc` / `param_5 + 8 + i*0xc` / `param_6 + i*0xc` in
`FUN_1405d4b00` (lines 149-167). Two bases hold it: `T+0x48f4` (match-level copy, used when
`MI+0x116c == 1`) and `T+0x3f04` (per-side) - selected at `0x1405ea3af` / `0x1405ea3bb`.

**The stages, in call order (OBS):**

| stage | VA | role | key calls |
|---|---|---|---|
| `FUN_1405e6e80` | 1405e6e80 | team-AI body; entry of the family | calls the pair at `1405dbe9d`, then the per-player loops |
| `FUN_1405dbeb4` | 1405dbeb4 | base-adjustment vector assembly | `FUN_1405d8b70` then `FUN_1405d1810`, fixed order (`1405dbeb4`/`1405dbec5`) |
| `FUN_1405d8b70` | 1405d8b70 | reads the instruction table (+ the T+0x25cf8 readers) | `FUN_140a63950` at `1405d8d84`, `FUN_140a62360` at `1405d8d3e`, `FUN_140a62440` at `1405d8d5a` |
| `FUN_1405d1810` | 1405d1810 | reads back `T+0x80f8`/`T+0x8134`/`T+0x6d0`, the settle stamps | second half of the pair |
| `FUN_1405db000` | 1405db000 | 11-pass driver | `FUN_1405e19f0`, `FUN_1405e3370`, `FUN_1405eb430`, `FUN_1405e1540`, `FUN_1405eaf70`, `FUN_1405e4460`, `FUN_1405e6e00`, `FUN_1405dfc70` |
| `FUN_1405eb0c0` | 1405eb0c0 | per-entry resolve | `FUN_1405dfe40` (position lookup) at line 102 |
| `FUN_1405d4b00` | 1405d4b00 | kind switch over the 11 entries | writes `T+0xb4+i*4`, `T+0x9c+i*2`, `T+0x6ee+i*2` |
| `FUN_1405e9f50` | 1405e9f50 | support search **writes** | entry triples to `T+0x48f4`/`+0x3f04`, metric to `T+0x7da0`, flags to `T+0x7fb8` |
| `FUN_1405ebca0` | 1405ebca0 | zone step (the doc's open "4ec writer") | writes the `T+0x4ec/4f4/4f8/4fc` box + `(i+0x3f)*0x14` + `0x41200000` sentinels |
| `FUN_1405df5a0` | 1405df5a0 | the cost | reads `T+0x40/0x44 + i*0x14`, scales by the byte at `+0x254` |

The **grid** (`MA+0x26020`, 32x70x45) is a **leaf lookup** inside `FUN_1405e9f50` at `1405ea8f0`
(`IMUL RCX, side, 0x62700`; `ADD RDX, 0x26020`; `CALL 0x140a71140`) - not a build step of this
family. The grid build belongs to O8 (`5e05c0`/`5e0180`).

**W-SUP open writers closed by this pass (OBS):**
- `T+0x4ec` (zone step) - writer `FUN_1405ebca0` (box + sentinels at `0x4f8`/`0x4fc`).
- `T+0x7da0` (second metric) - written by `FUN_1405e9f50` in three branches: `0x1405ea254` (zero),
  `0x1405ea407` (`FUN_1405dfe40`-derived), `0x1405eae22` (accumulating pass).
- `T+0x7fb8` (flags) - written at `0x1405ea339` (0) and `0x1405ea34f` (1), read at `0x1405ea368`.
- `T+0x80a0` - read by `FUN_1405d4b00` line 51, gating a branch.
- `T+0x48f4` vs `T+0x3f04` base selection - decided by `MI+0x116c == 1` at `0x1405ea39e`.

**Still open in W-SUP (OBS of the reads):** `T+0x7e0`/`T+0x7e4` (settle stamps - read via `SUB` at
`0x1405eaa74`/`0x1405eaa7b`, writer not found in this pass), `T+0xc134/c13c/c140`, `T+0x7ed4`.

**Gap 3 status: decoded.** The "41-entry" label was a mis-read of the row count; the family is
**6+ functions over an 11-entry 12-byte-stride array**, with a separate 20-byte-stride box record
at `+0x4xx` and the grid as a leaf lookup.

### 9.5 FieldInfo F10-F12: all 18 consumers accounted for, no reader exists (OBS)

The accessor returns `block + 4` = F[0], so F10 = `ret+0x28`, F11 = `ret+0x2c`, F12 = `ret+0x30`.
The 18 call sites are now each classified. The 5 previously unresolved have been followed:

| site | verdict |
|---|---|
| `0x14087cd14` | calls `FUN_140871d30()` and **discards the return** (next instruction is `lea rcx,[rdi+8]` into a `FUN_14041b6b0` call). Called for the **cache-fill side effect** only. |
| `0x14087f2c1` | same shape (return discarded, next is `lea rcx,[rbp+0x180]`). Cache-fill only. |
| `0x140885534` | same shape. Cache-fill only. |
| `0x140a4912d` | same shape (the match-level record at `this+0x2a90`; next is `lea rcx,[r13+0x258]`). Cache-fill only. |
| `0x1420b1608` | **the only one of the five that reads the pointer**: `fVar12 = *(float*)(lVar1+8)` (**F2**) and `fVar7 = *(float*)(lVar1+0x34)` (**F13**). No F10-F12. |

Combined with the 13 cleared in §4, the complete consumer census is:

- F reads found at: `+0x0c` = F3 (`0x142177de5`), `+0x08` = F2 (`0x1420b1608`), `+0x20` = F8
  (`0x14087d071`, `0x14087d0aa`), `+0x34` = F13 (`0x1420b161c`, `0x1420b1608`); multi-slot
  `0x0c..0x14` and `0x0c/0x10/0x14` block reads at `0x14087f536/53e/547`, `0x14087ba21`,
  `0x14087c570/5a4/5aa/5f8`, `0x14087ce09/4c`, `0x140885d2f`; and the two movups block copies
  (`0x141fbab56`, `0x141fcf8e6`) that duplicate a `0x40`-stride table.
- **Zero reads of `+0x28`, `+0x2c`, `+0x30` (F10, F11, F12) at any of the 18 sites.**

**Conclusion (OBS):** F10/F11/F12 (1.109375 / 2.33984375 / 51.171875) are **write-only table
slots with no in-image consumer**. The three values exist in the FieldInfo record and are
initialised, but nothing reads them. This is the final answer for MB-G's open rows: they can be
marked *dead in the reference implementation* rather than "reader not found", which is the
stronger statement the gate list needs.

**Gap 4 status: decoded** (result: no reader exists; the values are inert).

### 9.6 Tick order: the event-hub dispatch rule is decoded (OBS)

`FUN_141b3edd0(hub, record)` (VA `0x141b3edd0`, called once per drained record from the queue pump
`FUN_141b3ea30` at `0x141b3eab6`) is the dispatcher the handover asked for. Decompiled in full;
the rule per record (header word at `rec+0x20`):

```c
uVar1 = *puVar6;                 // record header
uVar3 = uVar1 & 0x7f;            // RECORD TYPE
uVar5 = uVar1 >> 7 & 0x3ff;      // SUBTYPE
if (uVar3 == 3) break;           // terminator
if      (uVar5 == 0) (*(hub->[0x1020 + type*8])->vtbl[+0x10])();
else if (uVar5 == 2) (*(hub->[0x820  + type*8])->vtbl[+0x10])();
else                 (*(hub->[0x20   + type*8])->vtbl[+0x10])(L, rec, rec, i, i+1, i);
plVar2 = hub->[0x1820 + type*8];            // ALWAYS, in the same iteration
(*(plVar2->vtbl[+0x10]))(plVar2, rec);
puVar6 += uVar1 >> 0x11;                     // advance by record size /4
```

So the delivery order is **channel listener first, then the `+0x1820` post list**, and three
listener arrays exist (`0x20`, `0x820`, `0x1020`) selected by subtype. (OBS)

**Which array and which order** (OBS): all listeners in `FUN_140418940` register through the hub's
**vtable slot +0x30** (`call [reg+0x30]` on the hub at `[match+0x10]`, sites `0x1404186f4` and
`0x140418796`), passing the handler built by `FUN_140426bb0(handle, index)` (sets
`handle->vtbl = 0x142598e08`, tag word `1` at `+8`, index at `+0x0c`) plus a descriptor at
`handle+0x28`. Four listeners are built; the match-listener block is constructed **before** the
MatchSub block (`FUN_14042fe20` at `0x140418754` then the `432cd0` at `0x14041876a`; the MatchSub
`42fe20` call is at `0x140418d1c` with its `432cd0` at `0x140418d33`, ~0x6c8 bytes later). Arrays
are appended in construction order (`FUN_140a8f730` push_back), so **the match listener runs before
MatchSub on the same record**. (OBS on site order and push_back semantics; the runtime index values
are not directly observable, hence INF on the array *index* itself.)

**Team tick placement** (OBS): MatchSub's slot-2 handler `FUN_140433760` calls `this->vtbl[+0x20]` =
`FUN_1404334e0` on record type 2; that gates on `(this+0x28)->vtbl[+8]` and tail-calls
`FUN_140a8eff0`, which walks its vector calling each member's `vtbl[2](member, msg, xmm2)` - the
team tick `FUN_14041ecf0` -> `FUN_1404f6c30` per side. So within a type-2 tick record:

```
match listener (FUN_14042b9f0, ten steps 2->3->4->5->6)
   [inside master tick FUN_140535c60: MA build FUN_140536fc0 + setters + the 11-player loop]
   then the post list -> MatchSub -> FUN_140433760 -> FUN_1404334e0 -> FUN_140a8eff0 -> team tick
```

**Referee** (OBS): `FUN_14050e770`'s 7 callers (`0x14050cba2`, `0x14050ccbc`, `0x14050e0f5`,
`0x14050e2f2`, `0x14050e47d`, `0x143f54758`, `0x143f55a64`) are all judge-side producers; the
referee is **event-driven, not in the tick chain**, so it is interleaved by the hub on its own
records rather than ordered against the team tick.

**Gap 5 status: decoded.**

- Instruction-table writer: **RESOLVED in §9.1b** - `FUN_140a426f0`, table base passed as
  `param_1`, insert-only (dedup guard). No partial state.
- Ball-track writer: **RESOLVED in §9.2b** - rebuild-per-tick (249/250 records rewrite, all
  indices), writer `FUN_14080fa80` writes `+0xb4/+0xb8/+0xbc` (record 0 of the track) as the head
  triple; the other records are re-derived each tick by the 250-step integrator loop
  (bound `0xfa`) reached from the 9-way solver dispatcher `FUN_140865d82` and the kick state
  machine `FUN_1406e4960` (state index 10).
- Tick order: **RESOLVED in §9.6** - channel listener then post list; match listener before
  MatchSub; team tick inside MatchSub; MA build inside the master match tick; referee
  event-driven.

### 9.2b Ball-track writer: `FUN_14080fa80` plus the per-tick rebuild chain (OBS)

Live (read-only), adjacent-tick samples of the whole 10,000-byte block: **249 of 250 records change
every sim tick, and the changed set spans indices 0..248** (no bounded prefix, no tail-only pattern):
```
tick 1: changed 249/250, first [0,1,2,3], last [245,246,247,248]
tick 2: changed 249/250, first [0,1,2,3], last [245,246,247,248]
tick 3: changed 249/250, first [0,1,2,3], last [245,246,247,248]
```
So the track is a **full per-tick rebuild**, not a shift and not a bounded re-solve. (OBS)

Writer chain (OBS, from the image):

```
kick state machine FUN_1406e4960 (state index 10, jump-table at 0x1406e5e44/0x1406e5e94,
  byte-index table + dword table, entry `movzx eax,[rdx+rax+0x6e5e94]; mov ecx,[rdx+rax*4+0x6e5e44]; add rcx,rdx; jmp rcx`)
  -> 0x1406e4bc0:  if (ball->+0x2459 == 0x5d) -> FUN_140816d10(ball, ball+0x2bc8, out, 1)
                    else                       -> FUN_140817ad0(ball, out)        <-- the 9-way solver dispatcher
  -> FUN_140817ad0 (0x140817ad0): the kick-type dispatcher
       cmp [out+8], 0x15 ; jbe tail 0x140818f40
       cmp [ball+0x2d12], 2 ; je tail 0x140817af0   (kind-byte variant)
       else tail 0x1408183c0
  -> FUN_14086b690 / FUN_14086ab90 / FUN_14086a3f0 / FUN_140869d10 / FUN_140866b60 /
     FUN_140868500 / FUN_140868ad0 / FUN_140869500 / FUN_140865f20  (9 solver variants,
     selected by a jump table at 0x140865ef8 off rdx, keyed on [r14+0x58] = ball kind)
  -> all of them call the 250-step integrator FUN_140876360 inside a `do { ... } while (i < 0xfa)`
     loop (e.g. 0x14086a0c0 inside FUN_140869d10, and 0x14086a808/0x14086af67/0x14086b1ce/
     0x14086b3c2/0x14086bae6 in the shared integrator region) - one record per step
```

Head-of-track writer (OBS): `FUN_14080fa80` (VA `0x14080fa80`, body `14080fa80-...`, called from the
same kick state machine at `0x1406e49f9`). Its out-struct `RDI` (= `param_2`, set by
`MOV RDI,RDX` at `14080fa96`) receives, in the ball-physics branch:

```
14080fe43  MOVSS dword ptr [RDI + 0xb4], XMM12      ; record0 w (spin)
14080fe58  MOV   dword ptr [RDI + 0xb4], 0          ; (zeroed when a sign test fails)
14080fe62  MOVSS XMM0, [RDI+0xb4] ; SUBSS XMM0, k
14080fe82  MOVSS dword ptr [RDI + 0xb8], XMM0       ; record0 a
14080fe8a  MOVSS XMM0, cap
14080fe92  MINSS XMM0, XMM1
14080fe96  MOVSS dword ptr [RDI + 0xbc], XMM0       ; record0 b  <-- the track head
```

and immediately before it the position quad `[RDI+0xb0]` (the clamped y), so `RDI+0xb4`, `RDI+0xb8`,
`RDI+0xbc` are the three non-position floats of **record 0**, i.e. the writer that sets the track's
head. Verified live: `B+0xb0/0xb4/0xb8/0xbc` hold live ball-physics floats (-9.97, 0.71, 16.18,
-0.55) while `B+0x88/0x8c/0x90` hold the ball position, matching the `x,y,z` triple at
`B+0xBC` seen in the earlier track dumps. (OBS on both sides.)

Also re-ruled-out, now exhaustively (OBS of the searches): no `lea r64,[r64+0xbc]` outside 7 sites,
all stack-based; no `movss [reg+0xbc], xmm` outside 32 sites, all in unrelated objects; the
`FUN_1408183c0` `+0xBC` dword copy at lines 143-151 is an intra-struct copy between two solver
records, not the track.

**Gap 2 status: decoded.** Writer named, cadence measured, shift hypothesis dead.


- `insns.tsv` (3.5 GB, full-image instruction map - **must be deleted or moved off this disk before
  reuse**; it was built to enable the exhaustive disp32/store searches; it sits in the scratch dir,
  not /tmp, per the rules).
- `scan.py`, `scan2.py`, `cs.py`, `idx.py`, `fico.py` (accessor-consumer scanner), `storescan.py`
  (store classifier over a disp range), `map_all.py` (index builder).
- Decompiles (via `dec.sh`, one at a time, JDK on PATH):
  `acc_test.c` (FUN_140871d30), `acc_0x14086d778.c`, `acc_setter_0x140a42430/0x140a42800/0x140a42940.c`,
  `w4_0x140a42b80.c`, `w5_0x140535260.c`, `w6_0x140639830.c`, `x1_0x140a426f0.c` (**gap-1 writer**),
  `x2_0x140a7a890.c` (its gate), `g3_0x140536fc0.c`, `g4_0x1405d8b70.c`, `g5_0x1405d1810.c`,
  `g1_0x1405dbeb4.c`, `t1..t9`, `u1..u7` (`u7_0x140535c60.c` = the master match tick),
  `d1_0x14086b690b.c`, `d2_0x14086ab90b.c`, `d3_0x14080fa80.c` (**gap-2 head writer**),
  `c2_0x140866b60.c`, `c13_0x1406e4960.c`, `c19_0x140869d17.c`, `c16_0x140817ad0b.c`,
  `g8_0x140432cd0.c`, `g9_0x14042fe20.c`, `g10_0x140a8f460.c`, `h1_0x140426bb0.c`, `h2_0x140a8f730.c`,
  `w_0x1405e9f50.c`, `w2_0x1405eb0c0.c`, `w3_0x1405db000.c`, `w4_0x1405d4b00.c`, `w5_0x1405ebca0.c`,
  `w6_0x1405df5a0.c`, `e1..e4` (the 5 previously unresolved accessor consumers).
- Ghidra bridge (port 8080) disassembly dumps: `f5e9f50.txt`, `d5.txt`, `f_a491.txt` - used where the
  decompiler hides the loop bound or the store form.
- Live read-only scripts: `gi_watch.py` (instruction table), `live_watch2.py` (chain + track),
  `track_shift.py` / `track_frames.py` (track cadence and rebuild extent). All run under
  `ulimit -v 2000000; nice -n 19 ionice -c3`, all read-only; nothing was written to game memory.
- No full-memory scan was run, so no `agent://all` announcement was required.
