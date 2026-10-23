# PES 2021: the player-keyed motion remap object `_DAT_1436F2600` (the FK / penalty run-up remap)

> **Correction 22-10-26 (TeamT1, FKPenaltyPlace):** the default free-kick result in §4.2, §5 and §7 is wrong. MI+0x1740 is the published second kicker, 255 = none (`docs/re/pes-re-setpiece-kicker.md`:116), so the FK player walk runs, and a default taker gets `fk_runup_robecal` (1261), not `fk_runup` (1248). The install test `ADefaultTakersRunUpsAreTheStockRows` pins this.

> **Main note 22-10-26:** RemapRE's decode, not yet re-audited by Main at the bytes. The records are Derivationdatatable.bin, which GF already loads (`src/pes/derivation.*`); the free-kick and penalty placement port needs the three key getters and the placer sequence (§6) and the six mutations (§7).

> Decode of the object the free-kick and penalty `SetKick` placers pass a base motion through, to
> pick the taker's run-up clip. Every constant below is read from `PES2021.exe`'s own instructions
> or the stock `Data/dt18_all.cpk`; nothing is a hard-coded PES table in GF code.
> Companion to `pes-re-kicker-standby.md` §12 and `pes-re-special-players-decode.md`.

## 0. What the object is, in one line

`_DAT_1436F2600` is a **global pointer** (not an inline object) to a **heap-allocated, 0x370-byte**
record table. The table holds **106 eight-byte "clip substitution" records** split into **six kind
lists** (counts 8, 35, 23, 15, 16, 9), loaded from the install file
`common/anime/playerID/bin/Derivationdatatable.bin`. The FK/penalty placers walk the list for their
**kind** (2 = free kick, 4 = penalty) with a **player key** (signature-card id) and a **stat** key,
and substitute the taker's run-up clip for the named player or the generic stat.

GF already loads and parses this file (`src/pes/derivation.cpp`, `DerivationTable` /
`DerivePlayerMotion` / `DeriveGenericMotion`, Main-audited, tests green). What is **not** ported is
the two **key getters** (card id, FK/penalty stat) and the **placer call sequence** around them —
that is the gap this doc closes.

---

## 1. The object: creator, size, the pointer-global

**Global.** `_DAT_1436F2600` is a 64-bit slot holding a pointer to the table. The accessor
`FUN_1406B3910` is `return _DAT_1436f2600;` (one instruction, `0x1406B3913` reads the slot and
returns it). A whole-image disp32 scan finds exactly five rip-relative references, all within the
`0x1406B30C0–0x1406B3920` family: reads at `0x1406B30FD`, `0x1406B34EC`, `0x1406B350D`,
`0x1406B3913` and the destructor's clear-store at `0x1406B3538` (in `FUN_1406B34C0`). There is **no
direct creation store** — the slot is written only by the destructor's clear and (the pointer is
stored) by the factory.

**Size 0x370.** The destructor `FUN_1406B34C0` frees exactly `0x370` bytes
(`func_0x000140227390(*_DAT_1436f2600, 0x370)` at `0x1406B3538`), so the object is a single
`0x370`-byte heap allocation. The reset/re-init `FUN_1406B3B70` does the same: it takes the address
of the pointer slot, frees `*slot` (`0x370`), re-zeroes the new allocation through the zeroing
constructor `FUN_1406B3380` → `0x143FB23D0`, then re-registers the two data files.

**Creator (High confidence on the path, Tentative on the exact call site).** The whole
`0x1406B3xxx` family is reached only through the manager/vtable (framework/static-init path), not a
direct `.text` `E8` call in a scanned range, so the exact allocating instruction is not a direct
E8-xref. The confirmed chain is:

1. Allocate `0x370`, run the **zeroing ctor** `0x143FB23D0` (thunk `FUN_1406B3380`) — zeroes the
   object and the per-kind pointer-array supersets.
2. Store the pointer into `_DAT_1436F2600`.
3. Register the two data files with the manager (vtable `0x1425DCAE0`); the records then flow in
   through the **registration entry** `FUN_1406B30C0` (lock `0x1436F2610` at `0x1406B30D1`,
   `FUN_1406B3210(obj, param_2, param_3, [param_1+0x98], −2)`, flags `obj+0x35F` / `obj+0x369`,
   unlock `0x1403FF490`), which parses the file via `FUN_1406B3210` → `FUN_1406B3AB0` (per-record
   tag dispatch into the typed per-kind arrays).

`FUN_1406B3B70` (the reset) is the closest directly-decompiled creator-adjacent function and is the
one to model if GF ever needs to re-init the table.

**Evidence:** EV-MOTION-0001 (pointer-global + 5 refs), EV-MOTION-0002 (0x370 free),
EV-MOTION-0003 (creator path).

---

## 2. Layout of the 0x370 object

The object is a **six-slot pointer array** (one heap pointer per kind) plus per-kind counts and
flags. The slot bases and counts are **byte-confirmed from `FUN_1406B3560`'s own jump tables**, not
the decompiler's `puVar3[0x26]`-style offsets (which mislabel the displacement base). `rdx =
param_1 + 0x48` in the walker; the slot table at `0x1406B375C` (kinds 0..5) and the count table at
`0x1406B3744` give:

| kind | slot base (`obj + …`) | count (stock) | kind name (by the placers that use it) |
|------|----------------------|---------------|----------------------------------------|
| 0    | `+0x08`              | 8             | (other set-piece / idle family)        |
| 1    | `+0x48`              | 35            | curve / placekick (long-kick)          |
| 2    | `+0xC0`              | 23            | **free kick** (`fk_runup`, `fk_idle`)  |
| 3    | `+0x178`             | 15            | (other family)                         |
| 4    | `+0x290`             | 16            | **penalty** (`pk_runup`, `pk_idle`)    |
| 5    | `+0x310`             | 9             | list-five placekick variants           |

The counts (8, 35, 23, 15, 16, 9) equal the stock file's per-kind record counts, so the six in-memory
pointer arrays hold the file's **106 records in kind order**, one 8-byte heap record per slot. The
walkers read `ptr = *(obj + slot)`, walk `count` pointers, each pointing at an 8-byte record.

After the six slots (last at `+0x310`, 9 records → `+0x310..+0x35F`), the tail `+0x35F` and `+0x369`
are the two registration flags set by `FUN_1406B30C0` (`obj+0x35F=1` when the registered file size
`[param_1+0x98]==0`, else `obj+0x369=1`).

> **Note for the loader:** GF does **not** need to reproduce this in-memory pointer-array packing.
> The loader reads the flat file and walks it positionally (as `ParseDerivationTable` does); the
> in-memory 6-slot layout is PES's own container detail and only matters if GF ever needs to model
> the live object (it does not — it parses the file once at load).
>
> **Minor open point (does not affect the loader):** the zeroing ctor `0x143FB23D0` zeroes the
> pointer-array supersets a bit conservatively; the exact per-kind zero-widths are not fully
> reconciled with the counts. This is cosmetic to the deliverable because the **walkers define the
> authoritative bases and counts** (the disassembly jump tables above), and the **file defines the
> record contents**.

**Evidence:** EV-MOTION-0004 (slot bases + counts from the two jump tables).

---

## 3. Source of the records: the install file

**File (Confirmed).** `common/anime/playerID/bin/Derivationdatatable.bin`, read through the **VFS
winner** (so a mod's `download/4cc_06_gameplay.cpk` override replaces it, exactly as the game
does). Present in the **stock install** `Data/dt18_all.cpk` (confirmed by the VFS walk and by GF's
own `PesVfs::Read`). Size = `106 records × 8 bytes = 848 bytes`.

**A second, different file** is also registered by the same factory:
`common/anime/playerID/bin/Definedatatable.bin` (132 bytes = 33 × 4). It fills a **separate 4-byte
record buffer at `obj+0x360`** and is **not** read by the FK/penalty walkers (`0x1406B3560` /
`0x1406B3780`). It is a second dataset (stat/definition table) for other consumers; the FK/penalty
run-up remap depends only on `Derivationdatatable.bin`. GF's existing loader reads the former only —
correct for this deliverable.

**Not built at runtime from player records.** The records are file data. The **keys** (card id, stat)
are computed at runtime from the player record (section 4), but the **records themselves** come from
the file.

**Evidence:** EV-MOTION-0005 (path + 848-byte size in stock VFS), EV-MOTION-0006 (second file).

---

## 4. Record format and the keys

### 4.1 The 8-byte record

Each record is two little-endian dwords. From `FUN_1406B3560`'s loop
(`0x1406B35D8..0x1406B3690`) and `FUN_1406B3780`'s, all values opcode-checked in
`src/pes/derivation.cpp`:

| field          | source                          | mask / shift                     | base | stock value |
|----------------|---------------------------------|----------------------------------|------|-------------|
| `playerId`     | `dword0 & 0x1FFFF` (`AND EAX,0x1FFFF` @ `0x1406B3662`) | bits 0..16 | −1 (stored `id+1`) | id 0 = generic |
| `motionOut`    | `(dword0 >> 0x11) & 0xFFF` (`SHR R9D,0x11` @ `0x1406B366D`) | bits 17..28 | `0x3A7` | motion index |
| `motionIn`     | `word1 & 0x7FF` (`MOV R14D,0x7FF` @ `0x1406B35E5`) | bits 0..10 | `0x3CA` | motion index |
| `sub`          | `(word1 >> 0xB) & 0x1F` (`SHR EAX,0xB` @ `0x1406B3878`, `AND AL,0x1F`) | bits 11..15 | — | 5-bit sub |

So: **`dword0 = idField (17 bits) | ((motionOut − 0x3A7) << 0x11)`** and
**`word1 = (motionIn − 0x3CA) | (sub << 0xB)`**. `idField == 0` ⇒ generic entry; otherwise the player
id is `idField − 1` (17 bits, up to 131070). Bits above the named fields are never read.

`motionIn` is the clip **replaced**; `motionOut` is the clip **substituted**. A record matches a
player when `idField−1 == playerId` **and** `motionIn == motionIn`; the **first** matching record in
the kind list wins (file order). A record with `idField == 0` matches **any** player by `motionIn`
alone (the card-0 / "no id" path).

### 4.2 The three keys (which player field each reads)

The placers pass three runtime-computed keys. Each is decoded from its getter's bytes:

- **Card id** — `FUN_1406EA7B0(player)` (`0x1406EA7B0..0x1406EA80A`). Reads the player's **anime
  interface** `player+0xAB8`: calls the vtable to get the record index (`(**vtable+8)(this)`). If the
  count is `≤ 0x15` (21) it resolves the record through the anime-record table
  (`func_0x000140a489d0` … `func_0x000141b59e20`) and returns **`*(record + 0xE8)`** — the player's
  **signature-card id** (a 32-bit card id from the anime interface). If the count is `> 0x15` it
  returns the fixed global `_DAT_14351AE34` (a special/anonymous card); otherwise `0`. For a **default
  player with no signature card**, the record index is 0 and `+0xE8` is `0` ⇒ **card id = 0**.
  *(The exact `_DAT_14351AE34` value is not read here; the card-0 "no signature" case is what the
  default player hits.)*

- **FK stat** — `FUN_1406E81F0(player)` (`0x1406E81F0..0x1406E831E`). Keyed by the player's **style
  index** `*(char*)(player+0x2458)`, looked up in the style record table
  (`func_0x000140a489f0(_DAT_1436f3dc8, style)`), reading **`*(rec + 0xE)`** and clamping to `0..0x13`
  (20). It is then gated by two float tests (the player's `style+0x254` × a global, and a distance
  test) which zero it when the player is "too far / too weak". For a **default player** (no
  signature, stock style) this resolves to **0**.

- **Penalty stat** — `FUN_1406EA760(player)` (`0x1406EA760..0x1406EA7A0`). Same style-index key
  (`player+0x2458` → style table), reading **`*(rec + 0xF)`** and clamping to `0..6`. For a
  **default player** this is **0**.

So: **card id = the player's signature-card id (anime interface `+0xE8`); FK stat and penalty stat =
per-player style-table bytes (`style+0xE` clamped 0..20 and `style+0xF` clamped 0..6), both 0 for a
default player.** The record's `idField` is matched against the card id; the record's `sub` is
matched against `stat + 1` (the generic walk uses `sub − 1 == stat`).

**Evidence:** EV-MOTION-0007 (record packing from the two walkers), EV-MOTION-0008 (the three key
getters and their player fields).

### 4.3 The two walks

`FUN_1406B3560(obj, cardId, motionIn, kind)` — **the player walk.** Walks the kind list; a record
matches when `motionIn` matches **and** (for `cardId != 0`) `idField−1 == cardId`. First match wins →
returns its `motionOut`. Then a **tail** rewrites two special cards that have no list row
(opcode-checked in `derivation.cpp` `TAIL_*`):
- card `0x20DA3` or `0x20D30`, `motionIn 0x4D1`, kind 2 → returns `0x4D4` (fk idle +3);
- card `0x20DA3` or `0x20D30`, `motionIn 0x4E0`, kind 2 → returns `0x4E3` (fk runup −3);
- card `0x20DA3` or `0x20D30`, `motionIn 0x94C`, kind 1 → returns `0x93F` (curve).
These are the **Beckham-2002** cards (they have no rows in the file; the tail hard-codes them).

`FUN_1406B3780(obj, stat, motionIn, kind)` — **the generic/stat walk.** Same list, but matches only
**generic** records (`idField == 0`) whose **`sub − 1 == stat`** and `motionIn` matches. First match
wins → its `motionOut`. No tail.

Both return `motionIn` unchanged when nothing matches. **GF has ported both** as
`DerivePlayerMotion` and `DeriveGenericMotion` (`src/pes/derivation.cpp`), with the tail in
`DerivePlayerMotion`.

**Evidence:** EV-MOTION-0009 (the two walks + the tail).

---

## 5. The exact remap the FK and penalty placers apply

### 5.1 The placers

- **Free kick** — `FUN_1407411F0` (`0x1407411F0..0x140741564`), `SetKick` for an FK.
- **Penalty** — `FUN_140782930` (`0x140782930..0x140782ACE`), `SetKick` for a penalty.

Both:
1. Read the **base run-up motion** from the animation table cell, via
   `func_0x0001406abba0()` (the animetable) + `func_0x0001406abb30(table, row, col, 0)` (a cell
   motion-number lookup):
   - FK: cell **`(row 0x509, col 9)`** (`auStack_e8[0]=0x509`, `auStackX_18[0]=9`) →
     **`fk_runup` (motion 1248)**.
   - Penalty: cell **`(row 0x8EB, col 9)`** (the two floats `0x43400000`=2283=`0x8EB`, `0x9`) →
     **`pk_runup` (motion 2926)**.
2. Compute the keys (section 4.2): card id = `FUN_1406EA7B0(player)`, stat = `FUN_1406E81F0` (FK) or
   `FUN_1406EA760` (penalty).
3. **Player walk** `FUN_1406B3560(obj, cardId, base, kind)`.
4. **If the player walk returned the base unchanged**, the **stat walk**
   `FUN_1406B3780(obj, stat, base, kind)`.
5. Place the taker at the ball using the final motion's root offset (`FUN_1406E8CC0`) and contact yaw
   (`FUN_1406B1C70`), then the stand composition `FUN_140732D40`.

**The one structural difference between the two placers** (byte-confirmed):
- **FK** gates the **player walk** on the player's anime-record count
  `uVar13 = *(uint*)(*(_DAT_1436f3dc8+0x270) + 0x1740)`: it runs **only when `0x15 < uVar13`**
  (`if (0x15 < uVar13)` at `0x1407411..`), and it also **zeroes the FK stat** when
  `uVar13 < 0x16`. A player with `uVar13 ≤ 21` (no signature anime records) **skips the player walk
  entirely** and goes straight to the stat walk.
- **Penalty** has **no such gate**: it calls the player walk **unconditionally**, then the stat walk
  only if the player walk was a no-op.

### 5.2 For a default player

"Default player" = a player with **no signature card**: card id `0` (EA7B0 → 0), FK stat `0` (E81F0 →
0), penalty stat `0` (EA760 → 0), and — for the FK gate — a small anime-record count (`uVar13 ≤ 21`,
High-confidence inference: a no-signature player has no anime records, and the FK code zeroes the stat
precisely in that branch).

**Free kick, default player:**
1. `uVar13 ≤ 21` ⇒ **player walk skipped**; FK stat zeroed to 0.
2. Stat walk `FUN_1406B3780(obj, stat=0, 1248, kind=2)` looks for a **generic kind-2 record with
   `sub−1 == 0`** (i.e. `sub == 1`) and `motionIn 1248`. **There is no kind-2 row with `sub 1`**
   (kind-2 `fk_runup` rows are subs 2, 8, 9, 12, 4, 11, 10, 6; the generic-`sub-1` rows are for
   `fk_idle`/other). **No match → returns 1248 unchanged.**

   **Net: the FK run-up is `fk_runup` (1248), NOT remapped.**

**Penalty, default player:**
1. No gate ⇒ player walk runs: `FUN_1406B3560(obj, cardId=0, 2926, kind=4)`. Card 0 matches **any**
   kind-4 record by `motionIn` alone; the **first** kind-4 row with `motionIn 2926` (pk_runup) is
   `pk_runup → pk_runup_neymar (2931)`. **Match → returns 2931.**
2. The player walk changed the motion, so the stat walk is skipped.

   **Net: the penalty run-up is `pk_runup` (2926) → `pk_runup_neymar` (2931), REMAPPED.**

**The asymmetry is real and is the answer to "the exact remap for a default player":**

| set piece | base motion | default-player result | remapped? |
|-----------|-------------|----------------------|-----------|
| **Free kick** (kind 2) | `fk_runup` (1248) | `fk_runup` (1248) | **No** — FK player walk gated off (`uVar13 ≤ 21`); stat walk has no `sub 1` row |
| **Penalty** (kind 4)   | `pk_runup` (2926) | `pk_runup_neymar` (2931) | **Yes** — player walk unconditional; card 0 matches the first generic `pk_runup` row |

The stock kind-4 row `pk_runup → pk_runup_neymar (2931)` is the **first** `pk_runup` row in the file,
so it is what a card-0 (no-signature) player picks up. The name `pk_runup_neymar` is the generic
penalty run-up clip as it happens to be filed first; that is the stock default.

For **signature** players the player walk matches their specific `idField` row (e.g. Ronaldinho card
1631 → `fk_runup_ronaldinho_2` (4145) / `pk_runup_ronaldinho_2` (4385); Beckham card 815/118718/123447
→ `fk_runup_beckham_2002` (1251)), and the stat walk is skipped.

**Evidence:** EV-MOTION-0010 (FK placer + gate, `0x1407411F0`), EV-MOTION-0011 (penalty placer,
`0x140782930`), EV-MOTION-0012 (the two stock rows: kind-2 has no `sub 1` fk_runup row; kind-4's first
pk_runup row is neymar/2931), EV-MOTION-0013 (the asymmetry, verified against GF's audited walkers).

> **Confidence caveat.** The mechanism (cells, keys, the two walks, the FK gate, the first-match-wins)
> is **Confirmed** from bytes. The single inference is that a **genuinely default** stock player has
> `uVar13 ≤ 21` (no anime records), which is the natural reading and is what makes the FK run-up
> unchanged; confirming it would require one runtime read of
> `*(*(_DAT_1436f3dc8+0x270)+0x1740)` for a default player. Marked **High confidence**.

---

## 6. What GF needs to load it (loader spec)

GF **already loads the table correctly.** `src/pes/derivation.cpp`:
- `TABLE_PATH = "common/anime/playerID/bin/Derivationdatatable.bin"` read through the **VFS winner**
  (`PesVfs::Read`), so a mod override wins.
- `LoadDerivationLayout` / `LoadDerivationRules` read every constant from `PES2021.exe`'s own
  instructions (opcode-checked `Site`s in section 4/5 VAs); both walkers must agree or the load
  fails loudly. No hard-coded PES numbers.
- `ParseDerivationTable` builds the six kind lists from the 8-byte records.
- `DerivePlayerMotion` (= `FUN_1406B3560` + tail) and `DeriveGenericMotion` (= `FUN_1406B3780`)
  implement the two walks.

**What is missing for the FK / penalty placement** (the actual port gap):
1. **The three key getters** — card id (`FUN_1406EA7B0`), FK stat (`FUN_1406E81F0`), penalty stat
   (`FUN_1406EA760`). These read the player's anime interface (`+0xAB8` record index → `+0xE8` card
   id) and style table (`player+0x2458` → `style+0xE` clamped 0..20, `style+0xF` clamped 0..6). GF
   must expose these three per the player-record layout it already decodes.
2. **The base motion** from the animetable cell: FK `Cell(0x509, 9).motionNo`, penalty
   `Cell(0x8EB, 9).motionNo`. GF's `Animetable` already does cell lookups (`LoadAnimetable`); these
   two cells are the stock `fk_runup` (1248) / `pk_runup` (2926).
3. **The placer call sequence** (per section 5.1), including the **FK gate**
   (`uVar13 = anime-record count; if (uVar13 > 0x15) player-walk` and the FK-stat zeroing when
   `uVar13 < 0x16`), which the penalty placer does not have.

**Call sequence GF must replicate to place an FK/penalty taker's run-up:**
```
base   = Animetable.Cell(row, col).motionNo        // FK (0x509,9)=1248 ; penalty (0x8EB,9)=2926
card   = CardId(player)                            // FUN_1406EA7B0 : anime+0xE8, 0 if no signature
m      = base
if FK:
    if (AnimeRecordCount(player) > 0x15)           // the FK gate (0x1407411..)
        m = DerivePlayerMotion(table, card, base, 2)
    stat = (AnimeRecordCount(player) < 0x16) ? 0 : FKStat(player)   // E81F0, zeroed when count<22
else:  // penalty: no gate
    m = DerivePlayerMotion(table, card, base, 4)
if (m == base):                                     // player walk was a no-op
    m = DeriveGenericMotion(table, stat, base, kind)  // kind 2 (FK) / 4 (penalty)
// place taker with m's root offset (FUN_1406E8CC0) and contact yaw (FUN_1406B1C70)
```

---

## 7. Self-checking checker and mutations

The table's self-check **already exists and is green** in `tests/pes_derivation_test.cpp`
(Main-audited). The install-based checkers (each reads the stock archive through the VFS, and skips
if a mod ships the table, per the brief's stock-vs-mod rule):

- `DerivationInstall.TheTableHoldsItsSixListsAsWholeStockEntries` — the six lists parse as whole
  stock entries (lengths 8/35/23/15/16/9, size exactly 848 bytes).
- `DerivationInstall.EveryEntryNamesAClipInTheInstallsMotionTable` — every `motionIn`/`motionOut`
  names a real clip in the install's motion table.
- `DerivationInstall.BeckhamsFreeKicksSubstituteByName` / `BeckhamsPlacekickSubstitutesByName` /
  `BeckhamsCurvedKickSelectsTheBeckham2002ClipByName` — the Beckham cards substitute by clip name.
- `DerivationInstall.NeymarsFeintIsItsOwnDoubleTouchSoleClip`,
  `DerivationInstall.ListFivesPlacekickVariantsResolveByTheirSub` — per-list invariants.

The synthetic (non-stock-value) tests catch packing/walk bugs independent of the install:
`DerivationPacking.*`, `DerivationPlayerWalk.*` (incl. `PlayerZeroMatchesEveryEntryByMotionOnly` and
`TheTailRewritesItsTwoCardsOverTheListResult`), `DerivationGenericWalk.*` (incl.
`MatchesTheStatPlusOneAgainstTheSubField`).

**Mutations the FK/penalty port must add** (one per branch, per the brief's TDD rule; each is a
consumer-visible behavior bug):
1. **FK gate dropped** — if the FK placer runs the player walk for a no-signature player, the FK
   run-up wrongly becomes `fk_runup_robecal` (1261) instead of `fk_runup` (1248). (Card 0 matches the
   first generic kind-2 `fk_runup` row, `sub 2` → robcchal/1261.)
2. **FK stat not zeroed when `uVar13 < 0x16`** — the stat walk would then use a non-zero FK stat and
   could substitute a `fk_runup` variant for a default player.
3. **Penalty gate added (wrongly)** — adding the FK gate to the penalty placer would leave the
   default penalty run-up at `pk_runup` (2926) instead of `pk_runup_neymar` (2931).
4. **Card-0 matched by id (not motion-only)** — if the player walk required `idField−1 == cardId` for
   card 0, the default penalty run-up would not remap (no `idField 0 == −1` row).
5. **First-match order reversed** — the first kind row with a given `motionIn` must win; reversing
   changes the default penalty result (e.g. to `pk_runup_ronaldinho` (2933), `sub 2`).
6. **Stat walk matches `sub == stat` (not `sub − 1`)** — off-by-one; would substitute the wrong
   generic row.

These pin the **exact default-player remap** (FK unchanged, penalty → neymar) and the key/gate
mechanism, all from the stock install, with no hard-coded PES numbers (the stock values are read via
the VFS; the synthetic tests use non-stock shapes so a constant that happens to match stock fails).

**Evidence:** EV-MOTION-0014 (existing install checkers + synthetic mutations).

---

## 8. Evidence ledger

| ID | Claim | Confidence | Evidence | Dependents |
|----|-------|-----------|----------|-----------|
| EV-MOTION-0001 | `_DAT_1436F2600` is a pointer global; 5 rip-relative refs (4 reads + 1 clear-store); accessor `FUN_1406B3910` | Confirmed | disp32 scan; `0x1406B3913` read; `0x1406B30FD`/`0x1406B34EC`/`0x1406B350D` reads; `0x1406B3538` clear | §1, §6 |
| EV-MOTION-0002 | Object is a single 0x370-byte heap allocation | Confirmed | `FUN_1406B34C0` free at `0x1406B3538` (`…390(ptr,0x370)`); `FUN_1406B3B70` re-zeros `*slot` | §1, §2 |
| EV-MOTION-0003 | Creator path: alloc 0x370 → zeroing ctor `0x143FB23D0` (thunk `FUN_1406B3380`) → store ptr → register files; records flow via `FUN_1406B30C0`→`FUN_1406B3210`→`FUN_1406B3AB0`; exact alloc call site is a vtable/framework path (not a direct E8) | High (path) / Tentative (exact site) | `FUN_1406B30C0` decompile (lock `0x1436F2610`, `FUN_1406B3210`, flags `+0x35F`/`+0x369`); `FUN_1406B3B70`; `FUN_1406B3380`; vtable `0x1425DCAE0` | §1 |
| EV-MOTION-0004 | Layout: 6 pointer slots at `+0x08`/`+0x48`/`+0xC0`/`+0x178`/`+0x290`/`+0x310`, counts 8/35/23/15/16/9, 106 records, kind order; flags at `+0x35F`/`+0x369` | Confirmed | `FUN_1406B3560` jump tables: slots `0x1406B375C` (kind 0..5), counts `0x1406B3744`; disassembly slot reads `0x1406B3615/61b/620/626/62f/638` | §2, §6 |
| EV-MOTION-0005 | Records from `common/anime/playerID/bin/Derivationdatatable.bin`, 848 bytes = 106×8, in stock `Data/dt18_all.cpk`, read via VFS winner | Confirmed | VFS walk; `src/pes/derivation.cpp:14` `TABLE_PATH`; `PesVfs::Read` | §3, §6 |
| EV-MOTION-0006 | Second registered file `common/anime/playerID/bin/Definedatatable.bin` (132 B = 33×4) fills a separate 4-byte buffer at `+0x360`, NOT read by the FK/penalty walkers | Confirmed (present) / High (not-read) | `FUN_1406B3210` second-file branch; `FUN_1406B3560`/`FUN_1406B3780` read only the 6 slots | §3 |
| EV-MOTION-0007 | 8-byte record: `dword0 = id(17b) | ((motionOut−0x3A7)<<0x11)`, `word1 = (motionIn−0x3CA) | (sub<<0xB)`; `idField 0`=generic, else `id−1` | Confirmed | `FUN_1406B3560` loop `0x1406B35D8..0x1406B3690`; `FUN_1406B3780`; `derivation.cpp` `Site`s (opcode-checked) | §4.1, §6 |
| EV-MOTION-0008 | Keys: card id = anime interface `+0xAB8` record index → `+0xE8` (0 if no signature, count>21 → `_DAT_14351AE34`); FK stat = style `+0xE` clamped 0..20 (+ two zeroing float gates); penalty stat = style `+0xF` clamped 0..6; all 0 for a default player | High | `FUN_1406EA7B0` (`0x1406EA7B0..80A`); `FUN_1406E81F0` (`0x1406E81F0..31E`); `FUN_1406EA760` (`0x1406EA760..7A0`) | §4.2, §5, §6 |
| EV-MOTION-0009 | Player walk `FUN_1406B3560` (first match by id+motionIn, then the Beckham-2002 tail); generic walk `FUN_1406B3780` (generic rows, `sub−1==stat`); both return motionIn on no match | Confirmed | `FUN_1406B3560` `0x1406B3560..6BC`; `FUN_1406B3780` `0x1406B3780..81C`; `derivation.cpp` `DerivePlayerMotion`/`DeriveGenericMotion` | §4.3, §5, §6 |
| EV-MOTION-0010 | FK placer `FUN_1407411F0`: base cell `(0x509,9)`=fk_runup(1248); player walk gated on `uVar13 > 0x15`; FK stat zeroed when `uVar13 < 0x16`; stat walk if no-op | Confirmed | `FUN_1407411F0` decompile: `auStack_e8[0]=0x509`, `auStackX_18[0]=9`, `if (0x15 < uVar13)`, `if (uVar13 < 0x16) uVar7=0` | §5 |
| EV-MOTION-0011 | Penalty placer `FUN_140782930`: base cell `(0x8EB,9)`=pk_runup(2926); player walk UNCONDITIONAL; stat walk if no-op; penalty stat from `FUN_1406EA760` | Confirmed | `FUN_140782930` decompile: the two cell floats (`0x8EB`,`9`), `FUN_1406B3560` then `if (sVar3==sVar2) FUN_1406B3780` | §5 |
| EV-MOTION-0012 | Stock rows: kind 2 has NO generic `fk_runup` (1248) row with `sub 1` (fk_runup rows: subs 2,8,9,12,4,11,10,6); kind 4's first `pk_runup` (2926) row is `→ pk_runup_neymar` (2931), `sub 4` | Confirmed | rowdump of the stock file through GF's audited parser (this session); `derivation.cpp` | §5.2 |
| EV-MOTION-0013 | Default-player asymmetry: FK run-up **unchanged** (fk_runup 1248); penalty run-up **remapped** to pk_runup_neymar (2931) | High (FK's `uVar13≤21` for a no-signature player is inferred) | §5.2 reasoning over EV-MOTION-0010/11/12 + GF's walkers; `statcheck` (this session): `DerivePlayerMotion(card0,2926,4)=2931`, `DeriveGenericMotion(stat0,1248,2)=1248` | §5.2, §7 |
| EV-MOTION-0014 | Self-check exists: install checkers + synthetic walk/packing tests, all green | Confirmed | `tests/pes_derivation_test.cpp` `DerivationInstall.*`, `DerivationPacking.*`, `DerivationPlayerWalk.*`, `DerivationGenericWalk.*` | §7 |

---

## 9. Open questions

1. **`_DAT_14351AE34`** — the card id returned by `FUN_1406EA7B0` when the anime-record count is
   `> 21`. Not read here; only matters for players with a very large anime record set (not the
   default case). Resolve if a signature player's card id appears wrong.
2. **The exact allocating call site** for the 0x370 object (EV-MOTION-0003) — a vtable/framework
   path, not a direct E8-xref. Resolve only if GF needs to model live re-init (it does not).
3. **`uVar13` for a genuinely default stock player** — inferred `≤ 21` (no anime records). One
   runtime read of `*(*(_DAT_1436f3dc8+0x270)+0x1740)` for a default player would confirm the FK
   "no remap" at Confirmed level.
4. **The two `Definedatatable.bin` consumers** — what reads the `+0x360` 4-byte buffer. Out of scope
   for the FK/penalty remap; note it if another placer needs it.

---

## 10. Report

**(a) Objective.** Decode `_DAT_1436F2600` (the FK/penalty run-up motion remap object): creator,
layout, record source, record format + keys, the exact default-player remap, and the GF loader spec.

**(b) Findings** (by confidence):
- **Confirmed:** pointer global + 5 refs (EV-0001); 0x370 heap object (EV-0002); 6-slot layout +
  counts 8/35/23/15/16/9 (EV-0004); file `common/anime/playerID/bin/Derivationdatatable.bin` 848 B
  (EV-0005); 8-byte record packing (EV-0007); the two walks + tail (EV-0009); FK placer + gate
  (EV-0010); penalty placer (EV-0011); stock rows (EV-0012).
- **High:** creator path (EV-0003); key getters' player fields (EV-0008); the default-player
  asymmetry — FK unchanged, penalty → neymar (EV-0013); second file not read by the placers (EV-0006).
- **Tentative:** the exact allocating call site (EV-0003).

**(c) Spec/interfaces defined.** The placers' call sequence (section 5.1/6) and the three key getters
(card id = anime `+0xE8`; FK stat = style `+0xE` clamped 0..20; penalty stat = style `+0xF` clamped
0..6), and the base cells (FK `(0x509,9)`=1248, penalty `(0x8EB,9)`=2926).

**(d) Code changed.** None — read-only RE deliverable. (GF already loads the table; the port gap is
the three getters + the placer sequence, not the loader.)

**(e) Tests added or run.** This session ran `statcheck` and `rowdump` (scratch, against the stock
install) to confirm the stock rows and the default-player walk results: `DerivePlayerMotion(card 0,
2926, 4) = 2931 (pk_runup_neymar)`, `DeriveGenericMotion(stat 0, 1248, 2) = 1248` (no `sub 1` row),
and the card-1631/815/118718/130523 substitutions. These are not repo tests; the repo's
`pes_derivation_test.cpp` (install + synthetic checkers) is the permanent self-check, green per its
last audit. The six mutations in section 7 are the tests the FK/penalty port must add.

**(f) Differential fidelity.** N/A (no GF FK/penalty placement exists yet to diff). The port, once
made, should diff the placed taker's motion number against these stock results (default FK = 1248,
default penalty = 2931; per-player = the named clip).

**(g) Unresolved.** See section 9 (4 items, none blocking the deliverable).

**(h) Next step.** Port the three key getters + the FK/penalty placer sequence into GF's set-piece
placement, driven by the call sequence in section 6, with the six mutations in section 7 as the red
tests.
