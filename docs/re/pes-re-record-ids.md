# pes-re-record-ids (RecordIds, 28-10-26)

Read-only RE. Image `~/ghidra-projects/scratch/Unpacker/pes_full.bin`
(flat: file offset = VA − `0x140000000`, verified: `0x14259858C` reads
`360.0`/`0x43B40000` at its flat offset; code `0x14083CC30` = `48 8B BC
44 C8 19 07 C0` there). Captures: `live2/pid407792` (kick 6+6, Clear 5,
angle 6), `live2/pid463316` (kick_scatter 2, angle 3, trap_kind0 6,
trap_commit 6). Decompiles via `dec.sh` (`d_*.c` in
`gf-RecordIds-scratch`), one call at a time.

Tags: **CONFIRMED** = decode + capture read (operand-VA pairs below);
**INF** = inference (reason given); **OPEN** = not decoded, blocker named.

## §0 Constants (script-read, never typed; operand VAs paired)

```
0x14259D590 f32=3.084972381591797  bits=40457030  (P+0 vtable slot 2: code at 0x1406F82F0)
0x142BEF090 f32=36.21417236328125   bits=4210DB50  (P+0 vtable slot 4: image VA)
0x142588218 f32=-1.0  bits=BF800000  (angle 0x142108B81 MOVSS; scatter use)
0x14256AE1C f32=1.0   bits=3F800000  (angle 0x142108B70 MOVSS; Tier-3 blend)
0x14259BE0C f32=15.0  bits=41700000  (angle 0x142108992 COMISS [rip+0x493473])
0x14259BE2C f32=40.0  bits=42200000  (angle 0x142108AB6 MULSS [rip+0x49336E])
0x14259BE5C f32=90.0  bits=42B40000  (angle 0x142108AC1 ADDSS [rip+0x493393])
0x14259BE20 f32=25.0  bits=41C80000  (angle 0x142108C75 MOVSS [rip+0x4931A3])
0x14259858C f32=360.0 bits=43B40000  (angle 0x1421088B6 MOVSS [rip+0x48FCCE])
0x142598580 f32=180.0 bits=43340000  (angle 0x142108902 MOVSS [rip+0x48FC76])
0x142565600 f32=0.5   bits=3F000000  (Tier-3 blend; kick-doc gate)
0x142565D30 f32=nan   bits=FFFFFFFF  (angle 0x1421088DF MOVSD mask source)
0x14259A764 f32=60.0  bits=42700000  (shot-pitch term, kick doc (g))
0x14256AE34 f32=1000.0 bits=447A0000 (range /1000 divisor, kick doc (b))
0x142591140 f32=30.0  bits=41F00000  (angle 0x14210897F MOVSS)
0x14259BE4C f32=70.0  bits=428C0000  (angle 0x142108B9C MOVSS)
0x14256AE30 f32=120.0 bits=42F00000  (angle 0x142108B93 MOVSS)
0x14259BE30 f32=45.0  bits=42340000  (angle 0x1421089A6 MOVSS [rip+0x493482])
0x1425656D0 f32=-0.0  bits=80000000  (angle 0x142108A39 XORPS mask)
0x1425656C0 f32=4.0   bits=40800000  (angle 0x142108A84 MOVSS)
0x14259A760 f32=35.0  bits=420C0000  (angle 0x142108BA5 MOVSS [rip+0x491BB3])
0x14253EE30 f32=100.0 bits=42C80000  (angle 0x1421087AD DIVSS [rip+0x43667B])
0x14258C264 f32=3.0   bits=40400000  (angle 0x1421087B9 MOVSS [rip+0x483AA3])
0x142598598 f32=-360.0 bits=C3B40000 (angle 0x1421088AD MOVSS [rip+0x48FCE2])
0x1426484F0 u32=1 bits=00000001      (6E150 class table row 0; 6E340 reads row*4 byte 2)
```

Operand-VA pairs (site -> target; `?` = non-RIP form, bytes given):

```
0x14083CCB6 MOV RAX,[RIP+0x2EB710B] -> 0x1436F3DC8 (G)            | 488b050b71eb02
0x14083D448 MOV RAX,[RIP+0x2EB6979] -> 0x1436F3DC8 (G)            | 488b057969eb02
0x14083D452 MOV RBX,[RAX+0x180]     (G+0x180 table -> RBX)        | 488b9880010000
0x14083D48C IMUL RCX,RAX,0x74       (entry stride 0x74)           | 486bc874
0x14083D490 CMP [RCX+RBX+0x20],R8D  (entry+0x20 carrier id)       | 4439441920
0x14083D563 CMP BYTE [RCX+RBX+0xD],0 (entry+0xD flag)            | 807c190d00
0x14083CCAC CALL [RAX]              (P vtable slot 0 = 0x1406E2100)
0x14083CD0D CALL [RAX+0x28]         (P vtable slot 5)
0x1408414BC MOVSX EAX,WORD [RCX+0x2752] (kick kind short)        | 0fbf8152270000
0x1408414C6 MOVSX R12D,BYTE [RCX+0x2755]                         | 440fbea155270000
0x140841516 CALL 0x14071E520        (subclass getter)             | e805d0edff
0x14084151B LEA RDI,[R14+0xAB8]      (motion interface)           | 498dbeb80a0000
0x1406EB6D6 MOV WORD [RCX+0x2752],R15W (W1 init)                 | 664489b952270000
0x1406EB6FC MOV QWORD [RCX+0x2764],0x44340000 (W1 window=720.0f) | 48c7816427000000003444
0x1406F6B3A CMP WORD [RDI+0x2752],-1 (W2 guard)                  | 6683bf52270000ff
0x1406F6B6B MOV WORD [RDI+0x2752],AX (W2 set-kind)               | 66898752270000
0x140719DCF MOV WORD [RSI+0x2752],AX (W3)                        | 66898652270000
0x14071BB69 MOV WORD [RBP+0x2752],AX (W4, kind=0x96)             | 66898552270000
0x140724F26 MOV DWORD [RDI+0x2752],R13D (W5)                     | 4489af52270000
0x140725FFF MOV DWORD [R14+0x2752],ECX (W6)                     | 41898e52270000
0x142109EB0 PUSH RBX (Clear unit, r8=unit)                       | 4053
0x142109EC0 CMP BYTE [RAX+0x31C1],0 (imageA danger)              | 80b8c131000000
0x142109ED5 MOV RCX,[R9+8] (ctx = unit+8)                        | 498b4908
0x142109EDB CALL 0x142108630 (angle)                             | e850e7ffff
0x1421086E1/705/71B/C15/C45 MOV RCX,[RIP+...] -> 0x1436F3DC8 (G) | 488b0d...
0x1421086E8 MOV RDI,[RCX+0x40] (G+0x40 carrier record)            | 488b7940
0x14210872E MOVSS XMM8,[RAX+0x554] (man facing)                   | f3440f108054050000
0x142108C27 SUBSS XMM8,[RAX+0x55C] (man pos x)                   | f3440f5c805c050000
0x142108BB3 MOV R14,[RBP+8] (slot record latch)                  | 4c8b7508
0x142108BC0 MOVSXD RDI,[RBP+0x68] (arg carrier id)               | 48637d68
0x142108BCC CALL 0x140A714E0 (lane reader: RAX=[RCX+0x22B9C]/[RCX+0x229B4])
0x142108BDE IMUL RCX,RDI,0xB (lane stride 11)                    | 486bcf0b
0x142108BEB MOV EDX,[RBX] (ring +0 word = carrier id)            | 8b13
0x142108C51 LEA R8,[RAX+0x55C] (man pos base)                    | 4c8d805c050000
```

## §1 The kicker `ap` (rcx of kick scatter `0x14083CC30`)

**CONFIRMED: P is a heap C++ player object (its own vtable `0x1425E34D8`),
not a G-block record; its writers are six football-action setup paths
that store the kind short at `+0x2752` (and siblings).** The `+0xAB8`
interface is the sibling static object `0x1425E3508` (PES vt `0x1406EAAF0`
= `MOV RAX,[RCX+0x3008]; RET`).

- P entry (scatter head `0x14083CCA9..`, capstone): `MOV RAX,[R15]`;
  `CALL [RAX]`; `MOVSX R10D,WORD [R13+0x2752]`: R15/R13 = P at two
  call sites. P's vtable calls in the scatter body: `[RAX]` (slot 0),
  `[RAX+8]` (slot 1), `[RAX+0x20]` (slot 4), `[RAX+0x28]` (slot 5),
  `[RAX+0x30]` (slot 6) — the motion/action interface.
 - P+0 = `0x1425E34D8` in 14/14 kick captures (12 pid407792 + 2 pid463316):
   this address holds code pointers
   (`0x1406E2100, 0x1406F6CE0, 0x1406F82F0, 0x1406EEDF0, 0x141794410,
   0x142C9C220`), i.e. it IS P's vtable. P+0 points at P's own type's
   vtable; the class is a player/football-action object whose slot 0 is
   `0x1406E2100` (`FUN_1406E1610(); if (param_2&1)
   func_0x140227930(param_1,0x3BC0)` = guarded delete path), slot 1 is
   the `0x1406F6CE0` body, slot 2 the `0x1406F82F0` body.
 - P+0xAB8 = `0x1425E3508` = P+0 vtable + `0x30`, image-resident in 14/14;
  `[P+0xAB8]` = `0x1406EAAF0` = `MOV RAX,[RCX+0x3008]; RET`. So +0xAB8 is
  a fixed per-class interface singleton (the motion object), called via
  vt+0x18 (motion id), vt+0x28 (action), vt+0x30 (touch), vt+0x20.
- Owner/allocation: **OPEN.** No `LEA reg,[0x1425E34D8]` (vtable install)
  and no `LEA reg,[0x1425E3508]` site was found in the searched code
  spans, and P's constructor was not identified; the allocator is not
  decoded. Blocker: a full-image scan for the vtable-install `LEA`
  (needs the .impdata raw map for `0x14200000..0x14300000`, which the
  flat VA map does not cover) or a constructor xref from Ghidra.
 - Relation to the player's G block: **REFUTED as identity, OPEN as
   mechanism.** P is never a G anime/man/slot entry (0/14: 12 on
   pid407792 + 2 on pid463316) and no G-table entry points within
   `0x4000` below P. P is the kicker's player/action object; the G block
   is the per-slot record triple the engine indexes by carrier id. The
   scatter's caller is `0x14083998D` and the postscale's is `0x14083F03D`
   (call.bin return slot, 14/14 kick captures; same P persists across
   scatter/postscale pairs: seq017=seq018=seq113, seq022=seq084,
   seq048=seq099; kinds observed 1,14,28,42,43,57,97,98 — all in
   1..0x96). Which member links P to its slot is not decoded.
   Blocker: decode the caller at `0x14083998D` (how it sets up P in
   r15/r13) or a P->slot field.
- Writers of the kick-input fields (**CONFIRMED** sites, widths, values):
  - W1 `FUN_1406EB677` (init/reset path; `MOVSXD RSI,EDX; OR R15,-1`):
    `+0x2752 := -1`, `+0x2754 := -1`, `+0x2756 := -1`,
    `+0x2758/2760 := 0`, `+0x2764/276C := 0x44340000` = **720.0f**,
    `+0x2774/277C := 0`, `+0x27EC := -1`, `+0x27F0 := -1.0f`
    (`0xBF800000`); plus `+0xAC8 := 0x2020202`, `+0x3B18 := 1.0f`,
    `[RCX+4] := 0xC`, `[RCX+0x18] := 0x11` chain. This is the writer that
    sets `+0x2764 = 720.0` — the value the kick doc's `< 360` gate reads
    (seq017 live 720.0). So the gate's meaning: the *reset* window
    (720.0) vs a live first-time window (275.95 in seq022).
  - W2 `FUN_1406F6790` (kind-set path; guards
    `[RDI+0x2752]==-1 && +0x2754==0xFF && +0x2755==0xFF &&
    +0x2756==0xFF` before writing): `+0x2752 := AX` (the kind),
    `+0x2756 := AL`, `+0x2758 := 0`.
  - W3 `0x140719DCF`: `[RSI+0x2752] := AX; [RSI+0x2756] := AL;
    [RSI+0x2758] := R8D`.
  - W4 `0x14071BB69`: `[RBP+0x2752] := 0x96` (EAX=0x96); `[RBP+0x2758] :=
    R12D`; preceded by `FUN_1406EE640(RBP)` + `[RBP+0x428]` vtable call
    `[RAX+8]` and `FUN_1406FA7C0` — a motion-commit path.
  - W5 `0x140724F26`: `[RDI+0x2752] := R13D(kind);
    [RDI+0x2754/55/56] := [RBP+0x124/128/12C];
    [RDI+0x2758] := [RBP-0x28]` — copies a setup block into the player.
  - W6 `0x140725FFF`: `[R14+0x2752] := [RBP+0x110];
    [R14+0x2754/55/56] := [RBP+0x114/118/11C];
    [R14+0x2758] := [RBP-0x38]` — same shape, second setup block.
  - The six sites are CONFIRMED (capstone, widths: `66 89` word vs
    `44/41 89` dword stores). Which football action each enclosing
    function is (names/owners) is **OPEN**: enclosing-function starts and
    xrefs need Ghidra (the flat decompile gives no symbols; E8 callers
    were not found by raw scan). Blocker: Ghidra xrefs to the six VAs.

## §2 The Clear carrier record (`unit+0` of `0x142109EB0`)

**CONFIRMED: not a G-table entry (0/5 + no equal in 102/102 table
entries); a heap carrier record `cr+0 = carrier id`, `cr+4 = side/team
mirror of the id`, `cr+8 = id<<32` bitfield; owner OPEN.**

- `FUN_142109EB0` (body `0x142109EB0..0x142109EE9`, capstone): `r8`=unit;
  `RAX=[R8+0x28]` (imageA); `if ([RAX+0x31C1]==0) return`;
  `*(u32*)RDX=5`; `RAX=[R8]` (carrier); `RCX=[R9+8]` (ctx=unit+8);
  `EDX=[RAX]` (carrier id); `CALL 0x142108630`. So the carrier id IS
  `cr+0`, passed as angle arg 2 (edx).
- Live (5/5 Clear captures): `cr+0` u32 = 6,5,8,7,18 (all 0..21);
  `cr+4` u32 = 6,5,8,7,7 (equals cr+0 in 4/5; 7 vs 18 in seq116 —
  **REFUTED as "always equal"**); `cr+8` u64 = `0x500000000`,
  `0x200000000`, `0x800000000`, `0x700000000`, `0x1400000001` =
  `(u64)X << 32 | lowbit` (X = 5,2,8,7,20 = the *other* side's index in
  4/5: seq116 X=20 lowbit 1; seq019 X=5 lowbit 0). So `cr+8` is a packed
  word, not a pointer.
- `cr-0x40..cr` is the identical 64 B header in 5/5
  (`...78FD6242 01 00000000 00000000 FF000000 FF000000 90F0BE42 01
  00000003 00000000 00000001 01000001 0100001F 000000`), i.e. an
  enclosing record/slab header, not pointed to by any live register.
- Identity vs G tables: `cr` equals none of the 102 entries
  (sideA/sideB/anime/man/slot, 26/26/22/2/2) in any of the 5 captures;
  `cr`, `ctx`, `ia` sit in heap arenas (`+U` is `0x1Cn` stack-adjacent
  arena; `cr/ia` the `0x1Db2/0x1F60` anonymous rw arenas) while G/MI/G+0x180
  live in the `0x30D2/0x2050` arenas. G-table deltas are all in-subset
  (`<0x4000`); the triple is megabytes away.
- **OPEN: owner/allocator of `cr`.** `cr-0xD0 = ia` holds in 5/5
  (`ia-cr = 0xD0`: imageA sits 0xD0 above the carrier record in the same
  arena allocation). Who allocates the `cr..ia` slab and who writes
  `cr+0/+4/+8` is not decoded. Blocker: the writer of the `cr..ia`
  slab — a heap-write scan needs a live process or a snapshot with the
  allocator frame; static RE needs the constructor xref (Ghidra).

## §3 The G+0x180 table: stride CONFIRMED 0x74, count OPEN (bounded 22..25+)

**Stride CONFIRMED by two independent `IMUL r,0x74` indexing sites over
the same base + live 0x74-lattice; count OPEN.**

- Site A (kick scatter `0x14083D48C..0x14083D55F`, 8×, capstone):
  `MOV RAX,[RIP+0x2EB6979]` -> `0x1436F3DC8` (G) at `0x14083D448`;
  `MOV RBX,[RAX+0x180]` at `0x14083D452`; then
  `IMUL RCX,RAX,0x74` + `CMP [RCX+RBX+0x20],R8D` (6×, clamped indices
  `rdx-2..rdx+3`, clamp `0x17`=23 on `>0x18`) and a second loop
  (`IMUL RAX,RCX,0x74`, `CMP [RAX+RBX+0x20],R8D` over `edx` in
  `0..0x18`, clamp 23) + `CMP BYTE [RCX+RBX+0xD],0`. So the table base
  is `G+0x180`, the stride is the immediate `0x74` (bytes `48 6B C8
  74`), the id field is entry `+0x20` (u32), the flag is entry `+0xD`
  (u8). The scan domain is ids `0..0x18` (25) with clamp value 23
  (`0x17`): the code reads entries `[0..24]` but clamps out-of-range
  indices to 23 — count is at least 24 entries, likely 25 (0..24).
- Site B (function at `0x140478F30`, same shape):
  `MOV RAX,[RIP+0x327AE78]` -> G at `0x140478F49`;
  `MOV R8,[RAX+0x180]` at `0x140478F50`;
  `IMUL RCX,RAX,0x74` at `0x140478F79` + `CMP [RCX+R8+0x20],EBX`
  (loop `edx` in `0..0x18`, clamp `0x17`=23). Same stride, same `+0x20`
  id field — independent confirmation.
- Live lattice (6/6 angle captures): with stride 0x74 every entry
  `k=0..15` head-reads as `01 <k> ff ...` (`ent[k]+0=k+1? no:
  +0=`0x01`, +1=`k`, +2=`0xFF`) — the byte-1-is-index lattice holds
  exactly at 0x74 and breaks at 0x6E/0x70/0x78/0x80 (neighbour strides
  show `0xFF`-smeared or structured-different rows). The RecordMap
  0x78-mutation passed because the pin only proved readability; a
  lattice check (`ent[k]+1==k` for `k=0..15` at stride s) FAILS at 0x78
  and PASSES at 0x74 — that is the pin that settles it (Main: assert
  `ent[k]+1==k`).
- Entry `+0x6B/+0x6D` (kick scatter readers, EndClear2 port doc):
  live bytes in 6/6: `+0x6B=0x02`, `+0x6D=0x00` (all captures; the last
  two bytes of the entry are `01 01` tail: `...01 0002 0000 0101
  01010000`). Which instruction reads `+0x6B/+0x6D` was NOT found in the
  scatter/postscale bodies (no `[...+0x6B]` disp in `0x14083CC30` /
  `0x1408414A0`) — **OPEN**: the reader is a third function (EndClear2
  port doc's claim). Blocker: grep the doc's reader VA and re-check.
- Count: **OPEN.** Code scans `0..0x18` clamped to 23 (so >= 24 live
  entries); the lattice holds at least to k=15 (checked). Upper bound
  not probed (reads past entry 24 not sampled). Blocker: one lattice
  read to k=31 (Main: extend the check; count = first k with
  `ent[k]+1 != k`).

## §4 Anime `+0x1A/+0x1C` vs GF `kSlotLo/HiOffset`: GF is wrong, PES reads are on the anime entry

**CONFIRMED: PES's `+0x1A/+0x1C` u16 pair lives on the anime-table entry
(the `IMG.1436F3DC8.19C8` record); GF names them on the slot record
(`kSlotLo/kSlotHiOffset` in `pesworldrecord.hpp:93-94`) where no port
reader or writer touches them.**

- Live (6/6 angle captures, carrier id = edx): `anime[cid]+0x1A` =
  7,29,0,8,18,0 (varies per capture and per slot: entries 0..7 read
  22,32,18,0,33,5,4,4 — a per-slot need/counter ladder); `anime[cid]+0x1C`
  = 0 in 6/6; `slot[cid]+0x1A` = 17289,17283,17285,17120,17024,17037
  (stable ~17xxx = -24.0f low half = `0x4380/43xx` float bytes, i.e. the
  high half of a float, not a counter); `slot[cid]+0x1C` = 0x00FF in
  6/6 (float byte, not a u16 hi). The live chain's u16 reads can only be
  the anime entry's pair: small ints on anime, float-halves on slot.
- GF side: `kSlotLoOffset/kSlotHiOffset/kSlotCapOffset` are defined but
  **never read or written anywhere** in `src/` or `tests/` (grep: only
  the two definition lines). GF's own `SlotVa = BlockVa + 0x10F0` puts
  GF's "lo/hi" at `anime+0x110A/0x110C` — bytes that live-read as
  non-counter data in every capture. No GF behaviour depends on them,
  so the fix is a rename/move with zero cutover risk.
- The old layouts doc §2 (`+0x1A lo / +0x1C hi / +0x22 cap` on "the slot
  record", writer `FUN_14092ED30`) is therefore mis-rooted: `FUN_14092ED30`
  decompile shows only `+0x1430/0x1438/0x18` writes on its param_2, no
  `+0x1A/+0x1C/+0x22` write anywhere in the function — the `+0x1A`
  writer is a different, undecoded function (the byte-level `66 89/8B`
  scans found no word store to disp `0x1A/0x1C/0x22` in
  `0x140000000..0x141800000`, and no `MOVZX r,[r+0x1A]` load either:
  the reader likely addresses the anime entry through a computed base).
  Blocker: the true writer/reader VA (needs Ghidra xref on the anime
  entry's `+0x1A`, or the trap-chain captures now in pid463316 which
  read them per the chains doc §1).

## Open items (numbered; each names what settles it)

1. P's class name + allocator: Ghidra xref on `0x1425E34D8` (vtable
   install `LEA`) — one query. The six writer functions' identities:
   Ghidra names for `0x1406EB677/0x1406F6790` + the four mid-dispatch
   sites (W3-W6 need function starts from `.pdata`, unreadable flat).
2. Clear `cr` slab owner: Ghidra xref on the `cr..ia` slab shape
   (`-0x40` header / `+0xD0` imageA) or a live heap-write watch.
3. G+0x180 count: extend the lattice check to k=31; entry `+0x6B/+0x6D`
   reader VA from the EndClear2 doc, re-checked against the image.
4. `+0x1A` reader/writer VAs: Ghidra xref, or decode from the new
   pid463316 trap captures (6 kind0 + 6 commit) which the chains doc
   says read them.
5. Sanction A1/MI identity: `0x14047F170` reads `[RDX+0xE0]` (A1+0xE0 =
   the E0 ring, decompile CONFIRMED) — needs a sanction capture (still
   0 captures; needs a foul in the testbot match).
6. ANALYZE identity (angle A0 = unit+8 = sanction A1): a Clear call and
   an angle call in the same frame with equal ctx pointers — now
   checkable: pid463316 angle ctx vs Clear unit+8 are different objects
   (angle ctx is the caller's unit+8, r9==unit+8 in 6/6), so A0=unit+8
   is CONFIRMED by construction (`0x142109ED5 MOV RCX,[R9+8]`); A1=that
   still needs the sanction capture.
