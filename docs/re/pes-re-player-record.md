# PES player slot record: identity, field table, snapshot check, GF cutover, per-function A0 (RE, 25-10-26)

Read-only static RE plus the 25-10-26 16:23 live snapshot (`<workdrive>/gf-LiveState-scratch/live/`, pid 2876063, taken mid-match). No PES bytes are quoted; values are ranges, offsets, widths, VAs. Scripts and joined tables: `<workdrive>/gf-PlayerRecord-scratch/` (`snap.py`, `dump.py`, `scan.py`, `owner.py`, `ctor.py`, `stores2.py`, `fields.py` -> `slot_fields.md`, `argmap.py`, `classify.py` -> `argmap.md`).

Labels: **CONFIRMED** = disassembly bytes or snapshot read this pass (or a cited doc that this pass re-checked); **INF** = inference from layout or shape; **OPEN** = not decoded, blocker named.

## 0. Verdict

1. The "slot record" in `pes-re-record-layouts.md` §2 and in `fieldmap.json` ("slot" type) is **three records, not one**: an anime/AI record (G+0x19C8 table, A48910), a man/player-object record (G+0x1DD8 table, A48A10: positions, velocity, action), and the slot record proper (G+0x25F8 table, A489F0: counters and the u16 triple). They sit in one per-player block at fixed offsets. The class byte `+0x1` that RL §3 assigns to the slot record is an anime-record byte (snapshot: 0 in all 22 slot records, 2/3/4/91 in the anime records).
2. The field map's "slot" root also covers team offsets (T+0x8E08 sheet, T+0xE0 lanes, T+0x6D48/0x6D50), G-table reads (G+0x19C8 / 0x1DD8 / 0x25F8 used as 8-byte entries), and the table accessors themselves, which take **G** in rcx.
3. **Constructor / allocator: OPEN.** The block is not allocated with a visible size immediate (section 1.4).

## 1. Identity

### 1.1 Table bases (G = the world object; `G` = 0x2BF4E090 in the snapshot; the G header blob is 0x2E58 bytes)

| Table | Accessor | Base | Stride | Count | Index | Fallback | Evidence |
|---|---|---|---|---|---|---|---|
| slot record | `A489F0(G,slot)` = VA 0x140A489F0 | G+0x25F8 | 0x28 (pointer entries) | 22 | slot+0xF3 (`CMP EDX,0x16`) | `*(G+0x25F8)` | **CONFIRMED** disassembly: `CMP EDX,0x16; JNC; MOVSXD RAX,EDX; ADD RAX,0xF3; LEA RAX,[RAX+RAX*4]; MOV RAX,[RCX+RAX*8]`; 22 non-null entries in snapshot |
| man / player object | `A48A10(G,i)` = VA 0x140A48A10 | G+0x1DD8 | 0x28 | 26 | i+0xBF | `*(G+0x1DD8)` | docs `pes-re-team-helpers.md:159` CONFIRMED; snapshot 25 non-null (indices 0-24) |
| anime / AI record | `A48910(G,i)` = VA 0x140A48910 | G+0x19C8 | 0x28 | 26 | i+0xA5 | `*(G+0x19C8)` | docs `pes-re-press-eval.md:97` CONFIRMED; snapshot 25 non-null |
| side / team pointer | `A489D0(G,side)` = VA 0x140A489D0 | G+0x1978 | 0x28 | 2 | side+0xA3 | `*(G+0x1978)` | docs `pes-re-press-eval.md:100`; snapshot 2 non-null |

The table entries are **pointers** (8 bytes at the entry base). The slot record is the object the pointer names, not the table entry.

### 1.2 Per-player block layout (CONFIRMED by snapshot)

For slot index i in 0..21 let B_i = the anime pointer of index i (G+0x19C8+0x28·i). Then:

- man(i) = B_i + **0x32C** (checked for all 25 man entries; the same delta holds for indices 22-24)
- slot(i) = B_i + **0x10F0** (checked for all 22 slot entries); equivalently slot(i) = man(i) + 0xDC4 (checked for all 22)

The stride B_{i+1} − B_i is **0x3F40** for i = 0..21 (22 consecutive pairs). From index 22 onward the stride is **0x1220** (22→23 and 23→24 checked). Indices 22..24 have man records (positions present) but no slot-table entry, so they form a second group of non-slot players (INF: bench or extras).

So one 0x3F40 block per on-pitch slot holds: the anime record at +0x0, the man record at +0x32C, the slot record at +0x10F0.

### 1.3 Size bounds (INF from the layout; exact sizes OPEN)

| Record | Start in block | Lower bound | Upper bound | Basis |
|---|---|---|---|---|
| anime | +0x0 | max accessed offset + width | **0x32C** | man starts at +0x32C |
| man | +0x32C | **0x6BC** (`+0x6B8` repeat word, RL-07 writer) | **0xDC4** | slot starts at man+0xDC4 |
| slot | +0x10F0 | **0x144C** (store at slot `+0x1448`, RL §2 CONFIRMED bytes) | **0x2E50** | next block at +0x3F40 |

Field offsets beyond the upper bound cannot belong to that record for on-pitch indices. Concretely, the map's `slot` fields at 0x3FB8 (read on the anime table) lie in the next block, and 0x6C40 / 0x6D48 / 0x6D50 / 0x8E08 / 0x271F0 are outside every per-player record.

### 1.4 Constructor (OPEN)

Negative results, so the next pass does not repeat them:

- **No allocation-size immediate.** Image scan for `0x3F40` and `0x18D00` as mov/imul/stack immediates: all hits are data, unrelated struct fields, or state codes. The `0x3F40` hits in `FUN_1403B1020` are a **state word** (`[R13+8]` is the bitstream state machine; `0x3F40` is a state code, not a size). Checked by disassembly; a false lead.
- **Table store sites** (opcode-decoded, disp32 = 0x19C8 / 0x1DD8 / 0x25F8, SIB and non-SIB):
  - `FUN_140921B20` (VA 0x140921B20) writes `[G+0x19C8]` at 0x140921B75 (zero) together with other G fields, and calls `0x140A85820` on G+0x874, G, G+0x10E8. A G-level reset, **not** the population loop.
  - `FUN_140EF6BE0` (VA 0x140EF6BE0) is a G-level zeroing initialiser: writes G+0x4 … G+0x2588 and a 0x12C-entry loop at G+0x44; stores `[G+0x25F8]` at 0x140EF6E3F (value unverified). Candidate G-init.
  - `FUN_140F02FB0` (0x140F02FB0, store at 0x140F031AA), `FUN_14133CCE0` (0x14133CCE0, store at 0x14133CF8F, base RDX), `FUN_141193490` (0x141193490, byte stores at 0x14119360E / 0x141193632), `FUN_144B264D0` (imm store at 0x14B57D855): store entry 0 or a fixed field, not the 22-entry loop.
  - `FUN_1420C0200` (0x1420C0200): 20-byte record rotate; false positive for the store scan.
- The population of entries 1..21 uses a computed base (`LEA`/`IMUL` then `[reg]`), which the disp32 scan cannot see.

Blocker: a caller-frame walk of the writers of G+0x25F8 (and G+0x19C8 / G+0x1DD8), or a call-graph search for the per-player block allocator (a heap call whose size is computed, e.g. `26·0x3F40`). Not done within budget.

Also: the bridge `decompile_function` endpoint returned "Decompilation failed" for 0x140A489F0 and 0x1404F0E50 (both decode fine in `disassemble_function`). Disassembly was used for the accessors.

## 2. Field table (per record; full joined table at `slot_fields.md`)

Widths and types from the decoded docs and the snapshot. "Writers/readers" are the VAs the docs give; blank = not named in the docs.

### 2.1 Slot record (G+0x25F8, block +0x10F0)

| Off | W | Type | Name (provisional) | Snapshot (22 records) | Writer(s) | Reader(s) | Label |
|---|---|---|---|---|---|---|---|
| +0x1430 | 4 | s32 | counter A (need blend) | 0..249 | `FUN_14092ED30` @0x14092F1CF (`41 89 87 30 14 00 00`) | F0E50 `+iVar11`, `rnd·1.5`, `57450` | CONFIRMED writer (RL-06); **not in fieldmap** |
| +0x1438 | 4 | s32 | counter B | 21..500 (22 distinct) | `FUN_14092ED30` @0x14092F204 | F0E50 `0x1404F3632` (`A-B`), `928120` gate | CONFIRMED writer (RL-06); fieldmap has this one |
| +0x1414 | 4 | f32 | AI target point (x) | not read this pass | `FUN_14092ED30` block copy | `0x1404F225C/228A/33DB/340E` | docs (RL §2, impure-state); **snapshot not checked** |
| +0x1A | 2 | u16 | lo (need ladder) | 15785..17311 | `FUN_14092ED30` | F0E50 ladder `< 0x1C` | docs; **shape mismatch** (F5) |
| +0x1C | 2 | u16 | hi | 255 (all 22) | `FUN_14092ED30` | F0E50 ladder | docs; **fieldmap says f32** (F4) |
| +0x22 | 2 | u16 | cap | not read | `FUN_14092ED30` | F0E50 `cap != 0` | docs; not in fieldmap |
| +0x2C | 4 | s32 | flag | 0..4 (int, not float) | record build | F0E50 `0x1404F379D` | docs; **fieldmap says f32** (F3) |
| +0x8 | 4 | u32 | bits | 4 (all 22) | OPEN | F0E50 `>>0x16`, `>>9` | RL-INF-02 |
| +0x1 | 1 | u8 | class byte | **0** (all 22) | OPEN | 6E150-family | **doc mis-attribution**: class is anime +0x1 (F1) |
| +0x1464 | 1 | u8 | (unnamed) | 0..251 | OPEN | fieldmap slot/sheet reader `A0+0x1464` @0x1408C5000 | INF |
| +0x12A4 | 1 | u8 | (unnamed) | 0..4 | OPEN | fieldmap | INF |
| +0x38 | 8 | ptr | sub-record pointer | in captured maps 22/22 | OPEN | F0E50 `+0x38` byte arms (`0x3`, `0x4`) | **width/kind conflict** (F7): fieldmap 8-byte ptr; RL says s8 |

### 2.2 Man / player-object record (G+0x1DD8, block +0x32C)

| Off | W | Type | Name | Snapshot (25) | Writer(s) | Reader(s) | Label |
|---|---|---|---|---|---|---|---|
| +0x0 | 4 | u32 | (header) | u32 values, not pointers | OPEN | | fieldmap says 8-byte ptr (F8) |
| +0x55C | 4 | f32 | pos x (PES x) | finite, −36.9 … +49.6 | tick movement (VA OPEN) | `0x1404AB8E0` (fieldmap man-field) | **shape CONFIRMED**; writer INF |
| +0x564 | 4 | f32 | pos z (GF y) | finite, −35.4 … +35.4 | tick movement (VA OPEN) | `0x1404AB8E0` | shape CONFIRMED |
| +0x574 | 4 | f32 | vel x (m/frame) | finite, small | tick movement | `0x140A6C6D0`, `0x140A6C990` | **width conflict**: fieldmap says 8-byte int (F2); pesmover.hpp:26 says f32 |
| +0x578 | 4 | f32 | vel y | not in fieldmap | | | docs (pesmover.hpp:26) |
| +0x57C | 4 | f32 | vel z | finite | tick movement | `0x140A6C6D0`, `0x140A6C990` | shape CONFIRMED |
| +0x5B0 | 4 | s32 | (unnamed) | 0..5 | OPEN | `0x140458740`, `0x140A6C6D0` | INF |
| +0x5D0 | 4 | s32 | action word | ints (1, 3, 30, 33, 54, 61, 63…), 0 for some | `FUN_140A6CA10` (sites `0x14092D607` copy `+0xB5AC→+0x5D0`, `0x140A36D6F` writes 1/0x72) | F0E50 `== 5 / 0x15 / -3U<3`; carrier `== 0x25` (pescarrier.hpp:1166) | CONFIRMED writer (RL-07, mark-slot §2). **Not in fieldmap** (F9) |
| +0x5D4 | 4 | s32 | action prev | equals +0x5D0 for most | `FUN_140A6CA10` | (none in F0E50) | RL-07; **not in fieldmap** |
| +0x661 | 1 | u8 | touching | 0/1 (2 records = 1) | OPEN | GF `match.hpp:520` (`pl+0x661`) | **not in fieldmap** (F9); shape CONFIRMED bool |
| +0x6B8 | 4 | s32 | repeat count | 10..4503 | `FUN_140A6CA10` | | RL-07; **not in fieldmap** |

### 2.3 Anime / AI record (G+0x19C8, block +0x0)

| Off | W | Type | Name | Snapshot (25) | Writer(s) | Reader(s) | Label |
|---|---|---|---|---|---|---|---|
| +0x1 | 1 | u8 | class (anime kind) | 2, 3, 4, 91 | OPEN (lane-units2 S12) | `0x140A6E150/340/050/4B0/580/5E0/1C0` via `A48910` | CONFIRMED reader (first-touch doc); the byte RL §3 puts on the slot record |
| +0x8 | 4 | u32 | flags | 0, 16384, 268435456 … | OPEN | F0E50 | RL-INF-02 |
| +0x18 | 8 | ptr | sub-record ptr | in captured maps 21/25 | OPEN | | fieldmap "ptr" consistent |
| +0x5D0 | 4 | f32 | (unnamed) | −1.0 in all 25 | not traced | | anomaly (F10) |

## 3. Snapshot check (25-10-26 16:23, mid-match)

Counts: anime 25 non-null (indices 0-24), man 25 (0-24), slot 22 (0-21). All 47 pointers (anime, man, slot) resolve into one captured RW heap mapping (`0x1B9F2000`-`0x1C000000`). Offsets verified in §1.2.

Mismatches and findings (report, not fixed here):

- **F1** Slot class byte `+0x1` = 0 in all 22 slot records. Anime `+0x1` = 2/3/4/91. The doc's §3 class byte lives on the anime record (A48910). **Mis-attribution in RL §2 and §3.**
- **F2** Man `+0x574`: fieldmap says 8-byte int. Snapshot: 4-byte values finite and small (f32 velocity); the 8-byte read adds the `+0x578` word. **Width/type wrong.**
- **F3** Slot/anime `+0x2C`: fieldmap says f32 (w4). Values are small ints 0..4 (subnormal as f32), i.e. s32 flags, as RL §2 says. **Kind wrong.** (Man `+0x2C` is a real float, ±35.5.)
- **F4** Slot `+0x1C`: fieldmap says f32. Value 255 in all 22 (as f32 this is 3.57e-43). RL §2 says u16 `hi`. **Kind wrong.**
- **F5** Slot `+0x1A` (u16 lo, RL ladder) = 15785..17311, while `+0x1C` (hi) = 255. Every slot record has lo > hi, so the RL `lo < hi` ladder cannot fire for any on-pitch slot in this snapshot. **Either the pair is mis-decoded or the ladder is never taken; OPEN, needs a re-decode of `FUN_14092ED30`'s store width.**
- **F6** Slot `+0x10`, `+0x18`, `+0x20`, `+0x28` (8-byte, fieldmap ptr): non-zero in 20-22 of 22 but **not in any captured mapping**. `+0x38` is in-map 22/22. **Pointer kind wrong or target not captured; OPEN.**
- **F7** Slot `+0x38`: RL says s8 (`== 3`, `== 4` arms); fieldmap says 8-byte ptr. In-map 22/22 supports ptr. **Conflict between RL and fieldmap; the snapshot favours the fieldmap.**
- **F8** Man / anime `+0x0`: 4-byte u32 (e.g. 0x01000000-type values), not pointers. Fieldmap says 8-byte ptr, 66 functions. **Kind wrong.**
- **F9** Missing from the fieldmap `slot` type entirely: man `+0x5D0`, `+0x5D4`, `+0x661`, `+0x6B8`; slot `+0x1414`, `+0x1430`, `+0x1A`, `+0x1C`, `+0x22`. The fieldmap has 0x5B0, 0x5EC, 0x5F4, 0x1438 instead. **The field map does not describe the action word, touching flag or counter A.**
- **F10** Anime `+0x5D0` = −1.0 float in all 25 (`0xBF800000`). Source not traced: either the anime record carries a constant there or the offset means something else on that object. **Anomaly, OPEN.**
- **F11** Slot `+0x14` (8-byte) takes 22 distinct values in the 4.4e18..4.9e18 band; `+0x30` (8-byte) takes 2 values. Neither is a counter or pointer. **Unexplained, OPEN.**
- **F12** Fieldmap offsets beyond the record bounds (§1.3): anime `+0x3FB8` (beyond +0x32C, read as 4-byte int in 23/25 anime records), slot `+0x6C40` (non-zero 22/22, beyond +0x2E50), `+0x271F0/4`, `+0x8E08…` are team or G. **Root conflation confirmed by value shape.**
- **F13** Man `+0x55C`, `+0x564`, `+0x57C`, `+0x5D0`, `+0x6B8`: shapes match RL/GF (positions in pitch range, action ints, repeat counts up to 4503). **CONFIRMED plausible.**
- **F14** Man `+0x661` is 0/1 in every record (bool). Matches `match.hpp:520`.

## 4. GF cutover list (for the task agent that moves the record into PES layout)

Grep of `src/` for each offset (large offsets exact; small offsets noted). "1:1" = a GF field carries the same meaning; "partial" = a GF stand-in exists; "none" = nothing found.

| PES field (record) | GF state that carries it | Status |
|---|---|---|
| man `+0x55C`/`+0x564` pos | `pescarrier.hpp:1012` (`CarrierPoint pos`, `// record +0x55C/+0x564`); `pespress.hpp:215` `posX/posZ` (`// player +0x55c/+0x564`), `:312`; `pesmentality.hpp:228/229` `manLateral/manDepth`; `pesscorerows.hpp:30`; `Player::GetPosition()` (`player/player.hpp:44`, humanoid) | partial: GF keeps position in the humanoid, not in a PES-layout record. GF y = PES z (`pesdodge.hpp:270`). |
| man `+0x574/+0x578/+0x57C` vel | `pesmover.hpp:13,26` (`pes::Vec3 velocity`, `// +0x574, +0x578, +0x57c`), `pesdodge.hpp:271,286` (`moveX` from +0x574) | 1:1 by meaning; no PES-layout storage |
| man `+0x5D0` action | `pescarrier.hpp:1015` (`int action`, carrier only, `// record +0x5D0`), `pescarrier.hpp:1166` (`oneTwoRun`, `== 0x25`); `elizacontroller.cpp:3216` says "no GF state" for the one-two case | partial (carrier only); no per-player storage |
| man `+0x5D4`, `+0x6B8` (prev, repeat) | none (`grep 0x5D4/0x6B8` in `src/` empty) | none |
| man `+0x661` touching | `match.hpp:520` (`bool touching; // pl+0x661`) | 1:1 by meaning |
| man `+0x5B0`, `+0x5EC/+0x5F4` | not searched | open |
| slot `+0x1430`/`+0x1438` counters | none (`grep 0x1430/0x1438` in `src/` empty) | **none** |
| slot `+0x1414` AI target point | `elizacontroller.cpp:661, 903, 3021` (`his base (AI+0x1414) is GF's adapted formation position`; ponytail stand-ins) | partial (stand-in, ponytail-marked) |
| slot `+0x1A/+0x1C/+0x22` u16 triple | none. Note: `pescarrier.hpp:685` `finishing = +0x1A` is a PlayerData skill from the database record, **not** this slot field (do not conflate) | **none** |
| slot `+0x2C`, `+0x38`, `+0x8`, `+0x1457`, `+0x1470` | not searched (small offsets collide with unrelated code) | open: needs names from the RL docs |
| anime `+0x1` class byte | `pesroles.hpp` class map (different vocabulary, RL §6) | none 1:1 |
| anime `+0x8` flags | none | none |
| team `T+0x78` kick word | `teamAIcontroller.cpp:2509-2510, 2597, 2635` (`kickValid`, `kickMan`, `mi.teamKickMan`, `g.teamKickValid`) | partial (stand-in, RL §4) |

Cutover needs: (a) a PES-layout block per on-pitch slot (0x3F40 stride, anime +0x0, man +0x32C, slot +0x10F0, with the sizes in §1.3), plus the three G tables as pointers; (b) fields from §2; (c) a `// ponytail:` at each stand-in naming the PES site (the writer VAs in §2).

## 5. Per-function A0 evidence (for the field map)

Data: 302 pilot + impure specs (`tools/dev/diffharness/specs/pilot|impure/*.json`). Columns: argument register (`args` in each spec), A0 offsets from the spec's input names, accessor callees from `universe.json`, and a provisional class (INF, from the offsets in §2).

Summary: 94 specs have **no A0 in rcx** (A0 is in another register or unbound), 34 touch no A0 offsets, 132 are ambiguous (only small offsets), 42 carry evidence:

| Class (INF) | Count | Functions (VA: A0 offsets) |
|---|---|---|
| **G** (table accessors take G as A0; offsets are G-table bases) | 5 | `140A48910` (0,19C8), `140A489D0` (0,1978), `140A489F0` (0,25F8), `140A48A10` (0,1DD8), `140A48A30` (0,1928). **Field map: these are G, not slot.** |
| **man** | 8 | `140458740` (554,574,57C,5B0); `1404AB8E0` (55C,564,5EC,5F4); `140A6C6D0` (554,574,57C,5B0); `140A6C990` (574,57C,5B0); `140682320` (5C, man+team); `14064DE60` (10..28, anime/man/team); `140978E70` (20,24,28, team); `14063C9F0` (0..C, 10..58: mixed, all roots) |
| **slot / sheet** (1438 and 1464 are slot-counter offsets but also team-sheet roster offsets; root OPEN) | 3+1 | `1408C5000` (1464), `1408C5020` (1438; sheet or slot, OPEN), `140A3C9B0` (1486,1488,1554); `140A3C950` (116C,1574 + team) |
| **team** (T, lanes, sheet) | 24 | `1404F6C30` (6AA0…6CEC, 8, BA8, C7C: T), `140A3E150/190/5D0/7D0/840` (T+0xE0 lane block: 1900/1904/271F0/1908/191C), `140A422C0`, `140A42430` (8E08…9D6C: S), `140A40820`, `140A42940`, `140A429A0` (21C, A2A0…), `140A41BE0` (7D8C/7D90), `140A74EC0`, `1405008E0`, `1405EBCA0` |
| **far-G / MA?** (offsets outside the G header and outside the player block) | 6 | `140439F50` (247D4), `140439F90` (22A7C,22C64), `140439FE0` (23F7C), `1404C8E60` (24710), `14061E7C0` (228F0,22AD8), `140A71520` (228D4,228D8) |

Reading of the table: the fieldmap labels every rcx-A0 spec as `slot` (`bindings`, `objects.A0`). The offset evidence says at least 5 are G (table accessors), about 24 are team/S/lane, and about 8 read the man record. Only a handful read slot-counter offsets, and those (`1408C5020`, `1408C5000`) also match team-sheet roster offsets, so their root is OPEN. The full 302-row list is `argmap.md`.

## 6. Corrections to docs and the field map

1. `fieldmap.json` type `slot` (A0 root, size 160252) must be **split**: slot record (block +0x10F0), man (+0x32C), anime (+0x0), G (table accessors), team (T, T+0xE0 lanes, S = T+0x8E08). Apply the §1.3 bounds to assign fields.
2. `fieldmap.json` kind/width errors: F2 (0x574 width), F3 (0x2C kind), F4 (0x1C kind), F8 (0x0 kind), F7 (0x38 conflict).
3. `fieldmap.json` misses +0x5D0, +0x5D4, +0x661, +0x6B8, +0x1414, +0x1430, +0x1A/+0x1C/+0x22 (F9).
4. `pes-re-record-layouts.md` §2 and §3: the class byte `+0x1` belongs to the anime record (A48910), not the slot record (F1). The "slot record" in §2 merges man (+0x55C…+0x6B8), slot (+0x1414…+0x1438) and anime (+0x1, +0x8). §6 cutover table must be read per object.
5. `pes-re-record-layouts.md` §2: the u16 `lo/hi` ladder pair does not fit the snapshot (F5).

## 7. Open items and blockers

- **OPEN-1 constructor / per-player block allocator.** Not found; negative results in §1.4. Needed: a caller walk of the code that fills G+0x19C8, G+0x1DD8 and G+0x25F8 for entries 0..21 (computed-base stores), and the heap allocation that yields the 0x3F40 blocks. Blocker: budget. Next step: search for `IMUL reg,reg,0x28` followed by a store through `[base+reg*8+disp]` with the table base in a register (not disp32), within functions that also call an allocator.
- **OPEN-2 exact record sizes** (§1.3). Needs the allocator or the ctor.
- **OPEN-3 slot `+0x1A/+0x1C` pair** (F5): re-decode the store width at `FUN_14092ED30` (0x14092F1CF/0x14092F204 neighbourhood).
- **OPEN-4 slot pointers `+0x10/+0x18/+0x20/+0x28`** (F6) and **`+0x14/+0x30`** (F11): not in captured mappings; need a field-name pass.
- **OPEN-5 anime `+0x5D0` = −1.0** (F10).
- **OPEN-6 A0 root for the 94 specs without rcx=A0** (§5). Needs the spec `args` for their A0 register.
- **OPEN-7 `+0x1414` value shape** not read this pass; `+0x2C`, `+0x38`, `+0x8` GF citations not searched.
- Bridge: `decompile_function` returns "Decompilation failed" for 0x140A489F0 and 0x1404F0E50; `disassemble_function` works. Cause not checked.

## 8. What was verified, what was not

- Verified this pass: accessor disassembly (A489F0), table bases/strides/counts (snapshot + docs), per-player block offsets (22 slot and 25 man/anime entries), field shapes for 159 fieldmap rows (joined to the snapshot), per-function arg registers and A0 offsets (302 specs), GF citations for the large offsets.
- Not verified: the writers of table entries 1..21 and of the block allocation (OPEN-1); exact widths of `+0x1A/+0x1C` (OPEN-3); small-offset GF citations; the `+0x1414` value shape. No GF build or tests were run (decode-only assignment).
- Budget: about 90 bridge and tool requests used of about 150.
