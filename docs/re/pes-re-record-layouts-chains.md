# PES record layouts for the four port chains (RecordMap, 28-10-26)

Branch `RecordMap` (from origin/main d9b80ea2). Tags: **CONFIRMED** = read from live captures (pin verdicts below) or the image; **INF** = inference (reason given); **OPEN** = not decoded (blocker named). Offsets are PES's (from the record's own start). Sizes are lower bounds (largest read or written offset + width) unless a pin states a readable extent. Captures: `<workdrive>/gf-Rendezvous-scratch/live2/pid407792/seqNNN` (PES pid 407792). Pins: `tools/dev/translate/recordmap_pin.py`. Access map: `tools/dev/translate/recordmap.py analyze`. Field map: `tools/dev/translate/fieldmap.json`, key `recordmap`. Headers: `src/pes/pesrecordmap.hpp`.

## 0. Method and what the pins can and cannot show

1. `recordmap.py analyze`: specgen's capstone walker on each chain's top level, with the direct-call closure of the top level (357 functions; sizes from `.pdata`, else the ledger). Every access is `(object root, pointer path, offset, width, r/w)`, relative to the top level's entry registers. 3146 memory operands are `unresolved-mem` and 522 are indirect (vtable) calls; the map does not cover them. Only resolved accesses appear below.
2. `recordmap_pin.py` reads each capture's `call.bin` (entry registers) and `mem.bin` through `full_snapshot_rv.SnapMem`. A check fails if the record identity or the offset is wrong against live bytes.
   Mutation check (module attribute overridden in one process, no file edit): `MAN_OFF` 0x32C to 0x330 fails the man-table check; `SLOT_OFF` 0x10F0 to 0x10F4 fails the slot-table check; `RING_HEAD_OFF` 0x1904 to 0x1905 fails the ring-head check. `CARRIER_TABLE_STRIDE` 0x74 to 0x78 does **not** fail: that check only proves readability, so the stride is **not pinned** (OPEN).
3. Capture counts: kick scatter 6, kick postscale 6, Clear unit 5, corridor angle 6 (`live2/pid407792`). No capture exists for the trap chain (no hook on 0x14079A470, 0x14079AFD0 or 0x14084DF20) or the sanction tree (`foul_sanction_47F170` and `foul_sanction_caller_480910` are in `hooks.cfg` with 0 captures in `live2/pid407792` and `live2/pid161465`).

## 1. G's record tables (G = `*(0x1436F3DC8)`; anime G+0x19C8, man G+0x1DD8, slot G+0x25F8; 0x28-byte entries)

Pins (live, 12/12 kick captures):
- **CONFIRMED** anime entries 0..20 are one slot block each: `anime[k+1] - anime[k] == 0x3F40` for k = 0..19 (the block size of `pesworldrecord.hpp`, `kPlayerBlockBytes`).
- **CONFIRMED** man entry k = anime entry k + 0x32C for k = 0..20.
- **CONFIRMED** slot entry k = anime entry k + 0x10F0 for k = 0..19.
- **CONFIRMED, irregular tail:** anime entries 21..25 and slot entries 20..21 do not follow the block (slot 20 minus anime 20 = 0xB2EF0, slot 21 = 0xB46D0). Which objects they are: **OPEN**.
- **INF:** GF's layout (`pesworldrecord.hpp`: `kSlotCount = 22`, `kAnimeCount = 26`, 22 uniform blocks) assumes the tail is blocks too. The live tables say 21 anime and 20 slot blocks are uniform. Any GF code that indexes slots 20 and 21 through the block layout is at risk.

| Record | Identity (analyzer root) | Fields read (offset: width, r/w, chains) | Verdict | GF source (file:line) or "GF has none" |
|---|---|---|---|---|
| anime block (entry k) | `IMG.1436F3DC8.19C8` | +0x1 u8 r (class, all chains); +0x2C u8 r (KICK); +0x8 4 bytes r (TRAP); +0x1A, +0x1C u16 r (CLEAR, TRAP); +0x94, +0x9C, +0xA0, +0xA8 f32 r (TRAP); +0x11C f32 r (CLEAR) | block layout CONFIRMED (entries 0..20) | +0x1: `pesworldrecord.hpp` `kAnimeClassOffset` (offset CONFIRMED). +0x1A/+0x1C: GF names them `kSlotLoOffset/kSlotHiOffset` in the **slot** record (`pesworldrecord.hpp`), but the live read is on the anime entry. **CONFLICT (INF):** GF's names or PES's reads are on the wrong record; OPEN. Rest: GF has none; the +0x2C writer is OPEN (kick doc). |
| man block (entry k = anime+0x32C) | `IMG.1436F3DC8.1DD8` | +0x554 f32 r; +0x55C, +0x560, +0x564 f32 r; +0x574 8 bytes r (x,y velocity); +0x578 f32 r; +0x57C f32 r; +0x5B0 4 bytes r; +0x5D0 4 bytes r (CLEAR); +0x610, +0x63C, +0x658 4 bytes r (KICK, not named) | offsets CONFIRMED (man entries 0..20) | +0x554 `kManFacingOffset` (writer `teamAIcontroller.cpp:3425`); +0x55C `kManPosXOffset` (writer :3418); +0x574 `kManVelXOffset` (writer :3422); +0x560, +0x564, +0x578, +0x57C: named in `pesworldrecord.hpp`, writers not in the grep; +0x5D0 `kManActionOffset` (named, no writer found); +0x5B0, +0x610, +0x63C, +0x658: GF has none. |
| slot block (entry k = anime+0x10F0) | `IMG.1436F3DC8.25F8` | +0x98 u8 r; +0xA1 u8 r (CLEAR); +0xB0, +0xC8, +0xE8, +0x108 8 bytes r; +0xD0, +0xD4, +0xF0, +0xF4, +0x110, +0x114 4 bytes r (TRAP); +0x12A4 u8 r (CLEAR); +0x1414 f32 r (CLEAR); +0x1418, +0x141C, +0x1420, +0x1424, +0x1428 f32 r (CLEAR) | offsets CONFIRMED (slot entries 0..19) | +0x1414 `kSlotAnchorXOffset` (named, `pesworldrecord.hpp`); +0x1430/+0x1438 counters are named there but not read by these chains. Others: GF has none. |

## 2. G+0x180 carrier table and G+0x40

- **G+0x180** is a pointer to a table with entries at stride 0x74, indexed by carrier id (EndClear2 port doc). **CONFIRMED** readable at `[id]*0x74 + 0x6E` for the id of each of the six angle captures. Fields: +0x6B u8 and +0x6D u8 read by the kick scatter (semantics OPEN). Stride **OPEN** (its mutation passes). GF: none.
- **G+0x40** is a pointer to a record readable to at least +0x2954 (6/6). Fields: 4-byte reads at +0x4, +0x34, +0x3C; 1-byte at +0x4C, +0x80 (CLEAR); f32 at +0x2770, +0x2774, +0x2778, +0x2920, +0x2944..+0x2950 (KICK, CLEAR, SANCTION). **INF:** this is the "carrier record" of the angle prologue (`mov rdi,[rcx+0x40]`). It is **not** equal to the Clear unit's carrier record (checked in both Clear captures, seq019 and seq047). GF has none; identity OPEN.

## 3. The kicker `ap` (rcx of 0x14083CC30; PES P)

- **REFUTED:** P is the anime record of an anime-table slot (block start). 0/12 captures. P is not inside any G-table block (scan of 0..0x4000 after every entry).
- **CONFIRMED:** P is readable to +0x32E8 in 12/12. P+0 holds an image address (0x1425E34D8; not decoded further).
- **CONFIRMED** field offsets, readable in 12/12 (values shown where pinned):

| Offset | Width | r/w | Live | GF source | PES writer / note |
|---|---|---|---|---|---|
| +0xAB8 | 8 | r | a pointer to an image-resident object (0x1425E3508 in the two captures printed; image-resident in 12/12); its vtable is in the image (0x1406EAAF0 printed; 12/12) | stand-ins: `pesmovestep.cpp:176,272,380` (vt+0x20/+0x28/+0x78 of the +0xab8 interface, ponytails) | writer OPEN |
| +0x2752 | 2 (s16) | r | 42, 97; in [-1,150] (12/12) | none (kick doc: `kSituationNotPorted`) | six writers: `0x1406EB6D7`, `0x1406F6B6B`, `0x140719DCF`, `0x14071BB69`, `0x140724F26`, `0x140725FFF` (kick doc) |
| +0x2754 | 1 | r | not pinned | none | OPEN (kick doc) |
| +0x2755 | 1 | r | not pinned | none | uVar14 writer OPEN |
| +0x2756 | 1 | r | not pinned | none | first-time flag writer OPEN |
| +0x2758 | 4 f32 | r | 0.0 (12/12 finite) | none | OPEN |
| +0x2764 | 4 f32 | r | 720.0 (seq017), 275.95 (seq022) (finite 12/12). The kick doc's gate `< 360` is false for 720.0: its meaning needs a decode | none | OPEN |
| +0x2768 | 4 f32 | r | 0.0, 0.037 (finite) | none | OPEN |
| +0x281C, +0x2BC8, +0x2BD0, +0x2BF8 | 4 f32 | r | 0.0 (finite) | none (contact vector per kick doc) | OPEN |
| +0x2D14 | 1 | r | not pinned | none | OPEN |
| +0x32E4 | 4 i32 | r/w | readable | none | OPEN |
| IDX+0x32D8 | 4 i32 | r/w | indexed (analyzer `IDX`) | GF `Player::GetPesRandom` (`player.cpp:448`) for P+0x32d0 slot 2 (kick doc) | OPEN (slot index) |

Lower-bound size 0x32E8 (`PesKickerRecord` in the header). Allocation size and owner: **OPEN**.

## 4. The corridor angle ctx, its E0 ring, and the Clear unit (EndClear2)

- **ctx** (A0 of 0x142108630; `unit+8` of 0x142109EB0; the sanction tree's A1 reads the same +0x18/+0xE0 pair): `+0x18` readable to +0x2601C+4 and `+0xE0` readable to +0x180698+4 (6/6). Identity A0 = A1 = unit+8: **INF**.
- **E0 ring** (`ctx+0xE0`): **CONFIRMED** `+0x1900` u32 count is 8 in 6/6 (saturated); `+0x1904` u32 head is 0, 2 or 5 in the captures printed (within 0..7, 6/6). GF has the same ring: `src/sim/onthepitch/pesevents.hpp:20` ("eight 800-byte slots with the count at +0x1900 and the head at +0x1904"). 800 = 0x320 = the kind-4 stride of the sanction doc §5.
- **Clear unit** (r8 of 0x142109EB0): **CONFIRMED** `+0` pointer (readable, 5/5), `+8` pointer (ctx, readable, 5/5), `+0x28` pointer (imageA, readable to +0x31C2, 5/5). GF has none (EndClear doc: "no danger writer, no FillUnitTable kind-1 wiring").
- **Carrier record** (`unit+0`): **CONFIRMED** `+0` u32 carrier id in 0..21 (5/5; ids 6 and 5 printed). **REFUTED:** it is not a G-table entry at `[id]` in the anime, man, slot, side-A or side-B tables (0/5). +4 u32 equals +0 in seq019 (OPEN). GF: none.
- **imageA** (`unit+0x28`): **CONFIRMED** `+0x31C1` u8 danger in {0,1} (5/5). GF reads the same byte as a `danger` field (`pescarrier.hpp:575`, `:1055`, `:1139`); the writer is not ported (`elizacontroller.cpp:2772`, ponytail; PES writer `0x140699960` per EndClear doc, OPEN).

## 5. Trap chain (TrapClipPort): anime and action objects

**OPEN, not pinned.** No hook on the chain's entries. The access map gives per-object offsets (`fieldmap.json` key `recordmap`):
- commit `0x14079AFD0` A0 (the action, INF): 37 fields; reads at `+0x0` u64, `+0x64`, `+0x68`, `+0x69`, `+0x6A`, `+0xB`, `+0xC`, `+0xD`, `+0x85`, `+0x86`, `+0x18`; writes at `+0xA`, `+0x10`, `+0x14`; `IDX+0x2FC` f64 read; `+0x2168`.
- commit A1 (the anime, INF): 16 fields; `+0xC`, `+0x10`, `+0x14`, `+0x18`, `+0x1C`, `+0x20` f32; `+0xA08`, `+0xAB8`, `+0xAC0` u64 pointers; `+0xB08` u8; `+0xB5C`, `+0x3B18` f32.
- query `0x14084DF20` A0: 22 fields written at `+0x64`..`+0x9C` (the query record of the kind-0 chain; `+0x6C` kind per the trap doc).
GF: none for these records (trap doc §4: "GF has no PES layout for the anime +0x70/+0xDC/+0xAB8/+0x3B18, action +0xC40, or query +0x6C/+0x20 records"). Hooks needed (section 8).

## 6. Sanction tree (SanctionSwitch): A1 and its E0 ring

**OPEN, not pinned** (no sanction capture). The access map of `0x14047F170` (A1 = arg 2): `A1+0xE0` (E0 ring, 13 fields: `+0x0`, 16-byte reads at `+0x10..+0x90`, `+0x1900` count, `+0x1904` head, `+0x42` u8, `+0x50`, `+0x60`), `A1+0x18` (2 fields, 16-byte reads). The ring domain is pinned through the angle ctx (section 4), which reads the same offsets. MI (G+0x270) reads for the tree: `+0x1168`, `+0x116C` (play state), `+0x1174`, `+0x1178` (8 bytes), `+0x1180`, `+0x1184`, `+0x1554`, `+0x155C`, `+0x1564`, `+0x158C`, `+0x1590`, `+0x1594`, `+0x15B8` u8. GF names `+0x116C` as the play state (`pesclock.hpp:96`) and `+0x1198` as the frame (`pesclock.hpp:159`, `pespossession.hpp:44`). The pilot map records a conflict on `+0x116C` (play state vs half-change counter) that must be settled before a domain is applied.

## 7. Field-to-GF work list (cutover)

| Quantity | PES record and offset | GF member today | Status |
|---|---|---|---|
| anime class | anime +0x1 | `kAnimeClassOffset` (pesworldrecord.hpp) | GF has it in PES layout |
| man facing, pos x, vel x | man +0x554, +0x55C, +0x574 | `kManFacingOffset`, `kManPosXOffset`, `kManVelXOffset` (written at teamAIcontroller.cpp:3418-3425) | GF has it in PES layout |
| man action word | man +0x5D0 | `kManActionOffset` (named, no writer found) | partial |
| slot anchor x | slot +0x1414 | `kSlotAnchorXOffset` | GF has it in PES layout |
| anime +0x1A/+0x1C | anime (live read) | named as slot `kSlotLo/HiOffset` | CONFLICT, OPEN |
| G+0x180 entry +0x6B/+0x6D | table, stride 0x74 | none | GF has none; stride OPEN |
| G+0x40 record | +0x2770..+0x2950 | none | GF has none; identity OPEN |
| kicker kick index, flags, window, contact | P +0x2752, +0x2754..+0x2756, +0x2764, +0x2BC8..+0x2BF8, +0x2D14 | none | GF has none; writers listed in section 3 |
| kicker interface | P +0xAB8 (image-resident interface, vtable) | stand-ins `pesmovestep.cpp:176,272,380` | stand-in |
| E0 ring count, head | E0 +0x1900, +0x1904 | `pesevents.hpp:20` ring (800-byte slots) | GF has the ring; E0 identity INF |
| imageA danger | imageA +0x31C1 | `danger` field (`pescarrier.hpp:575`) | field present, writer not ported (`elizacontroller.cpp:2772`) |
| Clear unit (+0, +8, +0x28) | unit | none | GF has none; PES writer OPEN |
| carrier id | carrier +0 u32 | none | GF has none |
| trap action, anime, query | action/anime/query (section 5) | `movestep.hpp` covers part of the anime interface only | GF has none for these offsets |
| sanction A1, E0, MI | section 6 | none (referee.cpp card mapping, FoulOffside doc) | GF has none |

## 8. Open items and hook requests (for Main; Main owns `hooks.cfg`)

1. Slot and anime tails: decode which objects are anime entries 21..25 and slot entries 20..21 (`G+0x19C8`, `G+0x25F8`). Settles the 22-block assumption in `pesworldrecord.hpp`.
2. The kicker `ap` (P): decode its owner and what its +0 pointer names. It is not a G block start.
3. Trap chain: hooks at `0x14079A470` (kind-0 entry) and `0x14079AFD0` (commit). Candidate windows (first 14 bytes from `pes_full.bin`; `hook_plan.py` must check them): `hook trap_kind0_79A470 0x14079a470 14 1 3000 488bc44c89482055415441554156` and `hook trap_commit_79AFD0 0x14079afd0 14 1 3000 488bc4554154415541564157488d`.
4. Sanction: a hook at `0x140A81230` (card kind reader, 14 bytes `40534883ec20418bc183fa020f84`), and a foul in the testbot match to fire `foul_sanction_47F170` (in `hooks.cfg`, 0 captures so far).
5. G+0x180 stride: a second entry pin (a table of ids whose bytes differ) before the stride is claimed.
6. ANALYZE identity: decide whether the angle's A0, unit+8 and the sanction A1 are one object (INF). A Clear call and an angle call in the same frame with equal ctx pointers would confirm it.
7. Clear carrier record (`unit+0`): decode the record's owner; it is not a G table entry.
