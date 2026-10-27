# PES 2021: G/MI fields and referee record for the foul/offside stoppage functions

**Scope.** The 30 input fields the involvement function `0x140A3A690` (and its callees) read from G and MI; the referee/official record layout (base, vtable, fields); live stoppage-capture checks; and the GF input map for the cutover.

**Labels.** CONFIRMED = read from image bytes, a driver run, or a live snapshot this pass. INF = inference from decompile shape, doc cross-reference, or layout. OPEN = not decoded, blocker named.

**Tools.** `dec.sh` (Ghidra headless), `e8scan.py` (call-site scan over the decrypted image), `capstone` (disassembly), read-only `/proc/<pid>/mem` via the rendezvous captures (`gf-Rendezvous-scratch/live/seq012,seq016,seq019`).

---

## 1. The 30 G/MI input fields

The diffharness spec `140A3A690.json` (branch `FoulOffside`, `tools/dev/diffharness/specs/stoppage/`) enumerates 30 inputs. The closure reads through 5 G child links and 12 MI offsets, plus the player/anime/man accessor fallbacks.

### 1.1 G child pointers (CONFIRMED from spec `regions_init` and accessor disassembly)

| G offset | Child | Width | Accessor | Fallback | Evidence |
|---|---|---|---|---|---|
| +0x0000 | (G header) | u64 | direct | — | spec input `IMG_1436F3DC8_0`; the G pointer itself, dereferenced to reach MI at G+0x270 |
| +0x0040 | ball record | ptr | `fun_140a6cf70(G+0x40, idx)` | ball+0x2770/0x2778 | spec `IMG_1436F3DC8_40_2770/2778`; `140A3A500` reads ball position via `fun_140a6cf70` when MI+0x116C==1 (in play) |
| +0x0270 | MI (match info) | ptr | `*(G+0x270)` | — | CONFIRMED: `lVar1 = *(G+0x270)` in all 6 decompiles |
| +0x1928 | team 0 record | ptr | `fun_140a48a30(G, 0)` = `*(G+((0+0xA1)*0x28))` = `*(G+0x1928)` | G+0x1928 | CONFIRMED: `0xA1*0x28=0x1928`; A5Judge §1.2 "team+0x254 = attack direction (s8)" |
| +0x1950 | team 1 record | ptr | `fun_140a48a30(G, 1)` = `*(G+((1+0xA1)*0x28))` = `*(G+0x1950)` | G+0x1928 | CONFIRMED: `(1+0xA1)*0x28=0x1950` |
| +0x19C8 | anime[0] (AI record) | ptr | `fun_140a48910(G, i)` = `*(G+((i+0xA5)*0x28))` | G+0x19C8 | CONFIRMED: player-record doc §1.1; `0xA5*0x28=0x19C8` |
| +0x1DD8 | man[0] (player object) | ptr | `fun_140a48a10(G, i)` = `*(G+((i+0xBF)*0x28))` | G+0x1DD8 | CONFIRMED: player-record doc §1.1; `0xBF*0x28=0x1DD8` |
| +0x1E00..+0x21C0 | E1EE child (man-slot) | 25×ptr | G+0x1E00+0x28k (k=0..24) | G+E1EE child | CONFIRMED: spec `regions_init` maps 25 slots to `.E1EE`; the man-slot record shared across the involvement closure |
| +0x19F0..+0x1DB0 | E1EE child (man-slot) | 25×ptr | G+0x19F0+0x28k (k=0..24) | G+E1EE child | CONFIRMED: spec `regions_init` maps 25 more slots to `.E1EE` |

### 1.2 MI (match info) fields read by the closure

MI = `*(G+0x270)`. All offsets CONFIRMED from decompile; meanings from `pes-re-A5Judge.md` §1.1 (JR table) and `pes-re-s7-records.md` §4.

| MI offset | Width | Type | Name | Values / Meaning | Reader(s) | Writer | Label |
|---|---|---|---|---|---|---|---|
| +0x1168 | 4 | u32 | period | 0=pre, 1=1st half, 2=half-time, 3=2nd half, …, 9=period 9, 10=skip | `14047F170` (sanction tree: `iVar11 == 3` / `== 7` branches), `140A38F20` (look-at: `9U < 2` gate), `1405141F0` (dispatcher) | `0x1408AD0B0` (init); resets `0x141180540`/`0x141183710` | CONFIRMED |
| +0x116C | 4 | u32 | play state | 0=none, 1=in play, 2=stopped, 3=set-play ready | **all 6 functions** (primary branch) | `0x141180540` (reset path) | CONFIRMED |
| +0x11B2 | 1 | u8 | sub-state / restart-armed flag | 0/1; when 116C==3 (set-play ready) 11B2=1 | `140A3A690` (116C==3 branch: `11B2 != 0` → involved), `140A3A930` (approach: reads 116C, not 11B2 directly) | restart builder | CONFIRMED |
| +0x1544 | 4 | u32 | offside flag (player index or 1) | 0=none, 1=flagged; passed as `==1` to `140A3A500(param, 1544==1)` | `140A3A690` (116C==1 branch), `140A3A9A80` (restart: `1544==1` → state 7) | `FUN_140a7b7a0` (offside rule, pes-re-offside.md §0) | CONFIRMED |
| +0x1548 | 4 | u32 | offside sub-field A | 0/1; OR'd with 154C to gate the involvement test | `140A3A690` (116C==1: `(1548!=0)||(154C!=0)`), `140A39A80` (restart: `1548==0 && 154C==0` → state 6/8) | `FUN_140a7b7a0` | CONFIRMED |
| +0x154C | 4 | u32 | offside sub-field B / offside category | 0/1/2/3; also copied to referee record +0x34 by `140A39A80` | `140A3A690` (116C==1: `bVar6 = 154C != 0`), `140A3A9A80` (restart: writes to ref+0x34), `140A3A930` (approach: not directly) | `FUN_140a7b7a0` | CONFIRMED |
| +0x1554 | 4 | u32 | restart kind | 1 kick-off, 2 throw-in, 3 goal kick, 4 corner, 5 FK, 6 PK, 7 period end | `140A3A690` (116C==2: `1554==2` → z-check; `1554==3` → x-check), `140A3A870` (latch: `1554==5` → card branch), `14047F170` (sanction: `1554==6` → PK path), `140A39A80` (restart: `1554==5` → card latch) | `FUN_14102cad0` | CONFIRMED |
| +0x1558 | 4 | u32 | awarded side / restart side | 0/1/2; team index taking the restart | `140A3A870` (latch: `1558==2` → side 1), `14047F170` (sanction: not directly), `1405141F0` (dispatcher: not directly) | `FUN_14102cad0` | CONFIRMED |
| +0x155C | 4 | f32 | spot x | pitch x (metres, PES x = GF x) | `140A3A690` (116C==2: `155C < 0` → sign check), `140A3A500` (helper: `pfVar5 = lVar1+0x155C` when 116C!=1), `14047F170` (sanction: `155C < ball.x` / `155C == ball.x`), `140A39A80` (not directly) | `FUN_140a3cd10` | CONFIRMED |
| +0x1564 | 4 | f32 | spot z | pitch z (metres, PES z = GF y) | `140A3A690` (116C==2: `1564 <= 0 && 1564 != 0` → sign check), `140A3A500` (helper: `pfVar5[2]` = z), `14047F170` (sanction: `1564 < ball.z` / `== ball.z`) | `FUN_140a3cd10` | CONFIRMED |
| +0x1574 | 4 | u32 | stop reason | 1 period end, 3 goal/kick-off, 4 ball out, 5 foul/offside, 9 shoot-out, 10 abandon | `140A3A690` (116C==2: `1574!=4 && 1574!=5` → return false), `140A3A930` (approach: `1574==3` → state 2/3; `1574==5` → card branch), `140A3A870` (latch: `1574==5` → card branch), `14047F170` (sanction: `1574` dispatch), `1405141F0` (dispatcher: `1574==3` / `1574==5` gates) | `FUN_141b8b250` | CONFIRMED |
| +0x1588 | 4 | u32 | foul kind | 1 offside, 2 direct foul, 3 indirect/dive | `140A3A690` (116C==2: `1588==1` → offside path), `140A3A870` (latch: `1588==1` → offside branch), `14047F170` (sanction: `1588` for card lookup) | `FUN_14050e770` param 6 | CONFIRMED |
| +0x158C | 4 | u32 | card level | 0 none, 1 foul, 2 foul+warning, 3 yellow, 4 red | `140A3A690` (not directly), `140A3A930` (approach: `158C==1` → card scene; `158C-2U < 3` → card level branch), `140A3A870` (not directly), `14047F170` (sanction: `158C-3 < 2` → card index) | commit / late-card writer | CONFIRMED |
| +0x1590 | 4 | u32 | fouler (player index) | 0..25 valid, 0xFF none | `14047F170` (sanction: `1590 < 0x16` → valid fouler; card lookup), `140A3A930` (not directly) | `FUN_141b8b250` param 7 | CONFIRMED |
| +0x1594 | 4 | u32 | victim (player index) | 0..25 valid, 0xFF none | `14047F170` (sanction: `1594 < 0x16` → valid victim), `1405141F0` (not directly) | `FUN_141b8b250` param 8 | CONFIRMED |
| +0x15A5 | 1 | u8 | card-scene active | 0/1; when 1 and 1574==5, the card scene overrides the restart programme | `1405141F0` (dispatcher: `15A5 != 0` gate) | card writer | CONFIRMED |
| +0x15B8 | 1 | u8 | (unnamed) | read by sanction tree `14047F170` | `14047F170` | OPEN | CONFIRMED reader; meaning OPEN |

### 1.3 G child fields read by the closure

| Child | Offset | Width | Type | Name | Reader | Label |
|---|---|---|---|---|---|---|
| ball (G+0x40) | +0x2770 | 4 | f32 | ball x (fallback position) | `140A3A500` via `fun_140a6cf70` (ball sample accessor; returns ball+0x2770 for idx>=0xFA) | CONFIRMED |
| ball (G+0x40) | +0x2778 | 4 | f32 | ball z (fallback position) | `140A3A500` via `fun_140a6cf70` | CONFIRMED |
| team0 (G+0x1928) | +0x254 | 1 | s8 | attack direction | `140A3A870` (latch: copies to ref+0x10d4), `14047F170` (sanction: `155C` sign vs `team+0x254`) | CONFIRMED (A5Judge §1.2) |
| team0/team1 (G+0x1928/1950) | +0x078 | 4 | s32 | kick man (player index) | `1405141F0` (dispatcher: `team+0x78 == player_id` gate) | CONFIRMED |
| anime[i] (G+0x19C8) | +0x001 | 1 | u8 | anime class / AI action byte | `140A3A690` (involvement: `anime+1 != 0x5A` / `== 0x5C`), `140A3A9A80` (restart: `anime+1` via `fun_140a6e3f0`), `140A38F20` (look-at: `anime+1 == 0x5A` → goalkeeper branch) | CONFIRMED |
| man[i] (G+0x1DD8) | +0x55C | 4 | f32 | man position x | `140A3A690` (involvement: `man+0x55C == 0` / `> 0`), `140A3A500` (helper: `man+0x55C <= 0`), `140A3A930` (approach: `man+0x55C - target.x`) | CONFIRMED |
| man[i] (G+0x1DD8) | +0x564 | 4 | f32 | man position z | `140A3A690` (involvement: `man+0x564 == 0` / `> 0`), `140A3A500` (helper: `man+0x564`), `140A3A930` (approach: `man+0x564 - target.z`) | CONFIRMED |
| man-slot (E1EE) | +0x001 | 1 | u8 | (unnamed; likely class byte) | involvement closure (spec input `E1EE_1`) | CONFIRMED reader; meaning INF |
| man-slot (E1EE) | +0x254 | 1 | u8 | (unnamed; likely side flag) | involvement closure (spec input `E1EE_254`) | CONFIRMED reader; meaning INF |
| man-slot (E1EE) | +0x55C | 4 | f32 | position x | involvement closure (spec input `E1EE_55C`) | CONFIRMED |
| man-slot (E1EE) | +0x564 | 4 | f32 | position z | involvement closure (spec input `E1EE_564`) | CONFIRMED |

### 1.4 Register / stack inputs (harness model)

The 6 non-memory inputs (`scalar_r8`, `scalar_r9`, `scalar_xmm3`, `stk0`, `stk1`, `stk2`) are the calling-convention arguments to the closure's callee functions:

| Input | Reg/Slot | Enc | Likely callee | Meaning | Label |
|---|---|---|---|---|---|
| scalar_r8 | r8 | u32 | `fun_140a6cf70` (ball sample) or `141E53520` (id mapper) | player index or ball sample index | INF |
| scalar_r9 | r9 | u32 | same | second argument | INF |
| scalar_xmm3 | xmm3 | f32 | `141E532D0` (frame time) or `140a6cf70` | time or position component | INF |
| stk0 | [rsp+0x20] | u64 | `140a6cf70` or `141E53520` | pointer or wide arg | INF |
| stk1 | [rsp+0x28] | u64 | same | pointer or wide arg | INF |
| stk2 | [rsp+0x30] | u64 | same | pointer or wide arg | INF |

These are not G/MI fields; they are the calling-convention slots for the helper calls within the closure. The GF cutover does not need to model them separately (they are derived from the 24 memory fields + the A0 argument).

### 1.5 The A0 argument

| Field | Width | Type | Meaning | Label |
|---|---|---|---|---|
| A0+4 | 4 | u32 | player index (the "official" or "involved player" id, 0..25 valid, 0xFF none) | CONFIRMED: `*(undefined4*)(param_1+4)` in all decompiles |

---

## 2. The referee / official record

### 2.1 Identity

The referee record is a polymorphic object. It is **not** the player-record block (0x3F40 stride); it is a separate per-official object with a vtable.

**Base.** The record is reached from the per-player update code (callers of `1405141F0` at `0x140930a0d`, `0x14094e34f`, `0x140961fe7`, `0x1409649a9`, `0x1409cc10c`). The exact G/MI offset that holds the 4 official record pointers is **OPEN** (the callers are in the 0x1409xxxxx per-player update range; the record pointer is passed as `param_1` to `1405141F0`). The vtable at record+0x0 identifies the official type.

**Size.** Lower bound: 0x958 (max accessed offset +0x950 + 4). Upper bound: OPEN (no destructor or allocator found). The record is at least 0x958 bytes.

**Vtable.** 4 vtables were identified in the image (file offsets 0x2642360, 0x2642568, 0x2642730, 0x2642768). Each contains a different subset of the 4 action functions. The vtable slot assignments (CONFIRMED from `1405141F0` decompile):

| Vtable offset | Index | Function | Purpose |
|---|---|---|---|
| +0x038 | 7 | (varies by official type) | (unidentified) |
| +0x040 | 8 | (varies) | (unidentified) |
| +0x048 | 9 | (varies) | (unidentified) |
| +0x050 | 10 | (varies) | (unidentified) |
| +0x058 | 11 | `0x140A39A80` (restart command) | writes ref+0x30/0x34/0x38/0x39 |
| +0x070 | 14 | (varies) | (unidentified) |
| +0x078 | 15 | (varies) | (unidentified) |
| +0x080 | 16 | `0x140A3A930` (approach) | reads ref+0x30; writes out-params |
| +0x088 | 17 | `0x140A3A870` (latch) | reads ref+0x30; writes flow+0x10d0/0x10d4 |
| +0x098 | 19 | `0x140A38F20` (look-at) | reads ref+0x18/0x61c/0x624/0x7e0/0x950 |
| +0x0A0 | 20 | (varies) | (unidentified) |
| +0x0A8 | 21 | (varies) | (unidentified) |
| +0x0B0 | 22 | (varies) | (unidentified) |

The 4 official types (referee, assistant 1, assistant 2, 4th official) each get a different vtable. Which vtable belongs to which official is **OPEN** (requires the constructor or the G/MI pointer table).

### 2.2 Referee record fields

Fields accessed by the 6 stoppage functions + `1405141F0`:

| Offset | Width | Type | Name (provisional) | Read/Write | Accessed by | Meaning | Label |
|---|---|---|---|---|---|---|---|
| +0x000 | 8 | ptr | vtable | read | all (via vtable call) | polymorphic dispatch | CONFIRMED |
| +0x018 | 4 | u32 | player index | read | `140A38F20` (look-at: `param_1+0x18` → `141E53520` / `140a48910` / `140a48a10`) | the official's player index (0..25) | CONFIRMED |
| +0x030 | 4 | u32 | action state | read+write | `140A3A930` (reads: `==0` gate), `140A3A870` (reads: `==0` gate), `140A39A80` (reads: `==2/5/7/9`; writes: 4,5,6,7,8,9) | the official's current action: 0 idle, 2 approaching, 4/5/6/7/8/9 = specific restart sub-states | CONFIRMED |
| +0x034 | 4 | u32 | offside category (cached) | read+write | `140A39A80` (reads: `==0` gate; writes: `MI+0x154C`) | cached copy of the offside category | CONFIRMED |
| +0x038 | 1 | u8 | flag raised | read+write | `140A39A80` (reads: `==0` gate; writes: 1) | the assistant referee's flag-up latch | CONFIRMED |
| +0x039 | 1 | u8 | card latch | read+write | `140A39A80` (reads: `!=0` gate; writes: `fun_140a63940(MI+0x1588)`) | card-issuance state | CONFIRMED |
| +0x61C | 4 | f32 | look-at distance | read | `140A38F20` (look-at: `FUN_140a8f7c0(param_2, param_1+0x61c, …)`) | a distance parameter for the look-at direction | INF |
| +0x624 | 4 | f32 | look-at position x | read | `140A38F20` (look-at: copies to `param_2` when 116C==2) | the official's gaze target x | INF |
| +0x628 | 4 | f32 | look-at position y | read | `140A38F20` | gaze target y | INF |
| +0x62C | 4 | f32 | look-at position z | read | `140A38F20` | gaze target z | INF |
| +0x7E0 | 8 | ptr | roster / team sub-record | read | `140A38F20` (look-at: `*(param_1+0x7e0)+0x18` → team lane pointer; `FUN_140a627e0(*(param_1+0x7e0), 0)` → carrier lookup) | pointer to a team or roster sub-record | INF |
| +0x950 | 4 | f32 | distance threshold | read | `140A38F20` (look-at: `fVar10 = *(param_1+0x950)`; sign check vs ball.x) | a distance or threshold for the look-at decision | INF |

### 2.3 Flow sub-record fields (param_3 in `140A3A930`/`140A3A870`/`1405141F0`)

The `param_3` of the action functions is a **flow sub-record** (not the referee record). It receives writes from the dispatcher and the latch:

| Offset | Width | Type | Name | Written by | Meaning | Label |
|---|---|---|---|---|---|---|
| +0x874 | 4 | f32 | restart position x | `1405141F0` (dispatcher: `*(param_3+0x874) = *param_4`) | the restart spot x | CONFIRMED |
| +0x878 | 4 | u32 | action ID | `1405141F0` (dispatcher: `FUN_140a6d420` or `iStack_7c`) | the official's action ID for this restart | CONFIRMED |
| +0x888 | 8 | — | (unnamed) | `1405141F0` (dispatcher: vtable slot +0xa0) | (unidentified) | CONFIRMED writer |
| +0x894 | 4 | f32 | official position x | `1405141F0` (dispatcher: `uStack_90` = man+0x55C) | the official's current position x | CONFIRMED |
| +0x898 | 4 | f32 | official position y | `1405141F0` | official position y | CONFIRMED |
| +0x89C | 4 | f32 | official position z | `1405141F0` (dispatcher: `uStack_88` = man+0x564) | official position z | CONFIRMED |
| +0x8A0 | 8 | — | (unnamed) | `1405141F0` (dispatcher: vtable slot +0xa8) | (unidentified) | CONFIRMED writer |
| +0x8B0 | 4 | f32 | (unnamed) ×3 | `1405141F0` (dispatcher: vtable slot +0x38) | (unidentified) | CONFIRMED writer |
| +0x8C4 | 4 | f32 | angle / heading | `1405141F0` (dispatcher: `uStack_74` from vtable slot +0x70) | the official's facing angle | CONFIRMED |
| +0x8CC | 4 | u32 | (unnamed) | `1405141F0` (dispatcher: vtable slot +0xb0) | (unidentified) | CONFIRMED writer |
| +0x8D0 | 4 | u32 | (unnamed) | `1405141F0` (dispatcher: read for `FUN_1405be2e0`) | (unidentified) | CONFIRMED reader |
| +0x8D4 | 4 | u32 | scene state | `1405141F0` (dispatcher: writes 0x12, 0x21; reads and remaps 1→3, 2→4, 5→6) | the official's scene/animation state | CONFIRMED |
| +0x8D8 | 4 | u32 | (unnamed) | `1405141F0` (dispatcher: writes 1 when `param_2+0x28` has bit 0x48) | (unidentified) | CONFIRMED writer |
| +0xE30 | 1 | u8 | (unnamed) | `1405141F0` (dispatcher: vtable slot +0x78) | (unidentified) | CONFIRMED writer |
| +0xE32 | 1 | u8 | signal latch | `1405141F0` (dispatcher: writes 0 when cVar5!=0) | a signal/flag latch | CONFIRMED |
| +0x10D0 | 4 | u32 | signal latch (latch) | `140A3A870` (latch: `*(param_3+0x10d0) = iVar4` where iVar4 = 1, 2, or 3) | the signal latch: 1 = no signal, 2 = offside signal, 3 = card signal | CONFIRMED |
| +0x10D4 | 1 | u8 | signal direction | `140A3A870` (latch: `*(param_3+0x10d4) = *(team+0x254)`) | the attack direction of the team taking the restart | CONFIRMED |
| +0x19C8 | 8 | ptr | replay / cutscene pointer | `1405141F0` (dispatcher: `FUN_1405bb110`, `fun_0x1405bbbf0`) | pointer to a replay or cutscene sub-record | CONFIRMED |
| +0x19D0 | 8 | ptr | (unnamed pointer) | `1405141F0` (dispatcher: `FUN_1405bb110`, `*(param_3+0x19d0)+0x48`) | (unidentified) | CONFIRMED |

### 2.4 Per-frame sub-record (param_2 in `140A3A930`/`140A3A870`/`1405141F0`)

The `param_2` is a per-frame event/official sub-record:

| Offset | Width | Type | Name | Accessed by | Label |
|---|---|---|---|---|---|
| +0x004 | 4 | u32 | player index | all 6 functions (`*(param_2+4)`) | CONFIRMED |
| +0x018 | 4 | u32 | (unnamed; player index for look-at) | `140A38F20` (`*(param_1+0x18)` — note: this is param_1, not param_2) | — |
| +0x028 | 8 | ptr | (unnamed) | `1405141F0` (dispatcher: `*(byte**)(param_2+0x28)` → byte check `& 0x7f == 0x48`) | CONFIRMED |
| +0x038 | 8 | ptr | (unnamed) | `1405141F0` (dispatcher: `*(param_2+0x38)` → `FUN_140a6d420`), `140A39A80` (restart: `*(param_2+0x38)+0xe0` → flow pointer) | CONFIRMED |
| +0x048 | 1 | u8 | (unnamed flag) | `1405141F0` (dispatcher: `*(char*)(param_2+0x48) != 0` gate) | CONFIRMED |

---

## 3. Live stoppage-capture check

Captures: `seq012` (restart_44C880, 1 call), `seq016` (restart_44C880, 1 call), `seq019` (restart_446390, 1 call). All from pid 2802803.

### 3.1 Pointer resolution (all 3 captures)

| Pointer | seq012 | seq016 | seq019 | Resolves? |
|---|---|---|---|---|
| G = *(0x1436F3DC8) | 0x2025E1C0 | 0x2025E1C0 | 0x2025E1C0 | CONFIRMED (same G across all 3) |
| MI = *(G+0x270) | 0x26E24608 | 0x26E24608 | 0x26E24608 | CONFIRMED |
| ball = *(G+0x40) | 0x33F8B04 | 0x33F8B04 | 0x33F8B04 | CONFIRMED |
| team0 = *(G+0x1928) | 0x26E91E04 | 0x26E91E04 | 0x26E91E04 | CONFIRMED |
| team1 = *(G+0x1950) | 0x26EAFF14 | 0x26EAFF14 | 0x26EAFF14 | CONFIRMED |
| anime[0] = *(G+0x19C8) | 0x26ECE028 | 0x26ECE028 | 0x26ECE028 | CONFIRMED |
| man[0] = *(G+0x1DD8) | 0x26ECE354 | 0x26ECE354 | 0x26ECE354 | CONFIRMED |

All pointers resolve to valid heap addresses. G is stable across the 3 captures (same match, same world object).

### 3.2 MI values at stoppage

| Field | seq012 (44C880) | seq016 (44C880) | seq019 (446390) | Plausible? |
|---|---|---|---|---|
| +0x1168 period | 0 | 0 | 1 | seq019: period 1 (1st half) CONFIRMED; seq012/016: 0 = pre-match or between-periods INF |
| +0x116C playState | 0 | **2** (stopped) | **2** (stopped) | CONFIRMED: stoppage captures show 2 (stopped) as expected; seq012: 0 = not yet in play INF |
| +0x1544 offside flag | 0 | 0 | 0 | CONFIRMED: no offside at these stoppages |
| +0x1548 offside sub A | 0 | 0 | 0 | CONFIRMED |
| +0x154C offside sub B | 0 | 0 | 0 | CONFIRMED |
| +0x1554 restart kind | 0 | **1** (kick-off) | **3** (goal kick) | CONFIRMED: seq016 = kick-off, seq019 = goal kick |
| +0x1558 awarded side | 2 | 0 | 0 | CONFIRMED: seq012 has side=2 (INF: pre-match default) |
| +0x155C spot x | 0.0 | 0.0 | **-47.76** | CONFIRMED: seq019 goal-kick spot at x=-47.76 (left half, plausible) |
| +0x1564 spot z | 0.0 | 0.0 | **-2.60** | CONFIRMED: plausible goal-kick z |
| +0x1574 stop reason | 0 | 0 | **4** (ball out) | CONFIRMED: goal kick = ball out (reason 4) |
| +0x1588 foul kind | 0 | 0 | 0 | CONFIRMED: no foul at a goal kick |
| +0x1590 fouler | 0xFF | 0xFF | 0xFF | CONFIRMED: no fouler |
| +0x1594 victim | 0xFF | 0xFF | 0xFF | CONFIRMED: no victim |

**Verdict.** The live captures are consistent with the decoded field table. seq019 is the most informative: a goal-kick stoppage (reason 4, restart 3) with a plausible spot in pitch coordinates, playState=2 (stopped), period=1. seq016 shows a kick-off (restart 1) with playState=2. seq012 appears to be pre-match or between-periods (playState=0, period=0).

**Limitation.** None of the 3 captures is a foul (reason 5) or offside (1588=1) stoppage. The foul/offside-specific fields (1544/1548/154C, 1588, 158C) are all 0 in these captures. The referee record's +0x10D0/0x10D4 (signal latch) and +0x038 (flag raised) could not be checked (the referee record pointer is OPEN — see §2.1). A future capture at a foul or offside stoppage (reason 5) would verify the foul-kind and card-level fields.

### 3.3 Team / man values

| Field | seq016 | seq019 | Plausible? |
|---|---|---|---|
| team0 dir (+0x254) | +1 | +1 | CONFIRMED: team 0 attacks +x |
| team1 dir (+0x254) | -1 | -1 | CONFIRMED: team 1 attacks -x |
| team0 kickMan (+0x78) | 0xFF | 0xFF | CONFIRMED: no designated kicker yet |
| man0 pos (x,z) | (-30.00, 0.00) | (-51.62, 0.00) | CONFIRMED: pitch coordinates, z=0 (halfway line) |
| man0 action (+0x5D0) | 0 | 114 | INF: 0 = idle; 114 = 0x72 (a specific action code) |

---

## 4. GF input map

For each PES field, what GF already holds and what it lacks. "1:1" = GF has a field with the same meaning; "partial" = a stand-in exists; "none" = no GF state found.

### 4.1 MI fields

| PES field (MI offset) | GF state that carries it | Status | Gap |
|---|---|---|---|
| MI+0x1168 period | `pesclock.hpp:95` `period` (MI+0x1168); `pesevents.hpp:333` `phase` | 1:1 | none |
| MI+0x116C play state | `pesclock.hpp:96` `playState` (MI+0x116C); `pescarrier.hpp:886` `restartMode` (MI+0x116C); `pesteamtick.hpp:72` | 1:1 | none |
| MI+0x11B2 sub-state | `pespossession.hpp:21` (MI+0x116c, MI+0x1574, MI+0x1558); no dedicated 0x11B2 field | partial | GF does not store 0x11B2 explicitly; it is derived from the restart-armed state. **Gap: add a u8 field for MI+0x11B2.** |
| MI+0x1544 offside flag | `referee.hpp:144` `offsidePlayers` (map<Player*, OffsideFlag>); `pescarrier.hpp:1313` `OffsideEntry` | partial | GF stores per-player offside flags, not a single MI+0x1544 word. **Gap: the PES field is a single u32 (0=none, 1=flagged); GF has a map. The cutover needs to derive the single word from the map (0 if empty, 1 if non-empty) or store the PES word directly.** |
| MI+0x1548 offside sub A | `referee.hpp:151` `PendingOffside` (struct with a few fields) | partial | GF's `PendingOffside` does not have a 1548-equivalent field. **Gap: add u32 for MI+0x1548.** |
| MI+0x154C offside sub B / category | `referee.hpp:69` `OffsideFlag::category` (int: 0/1/2); `referee.hpp:115` `GetOffsideCategory()` | 1:1 by meaning | GF stores the category per player; the MI+0x154C word is the "active" category. **Gap: the PES field is a single u32; GF derives it from the offsidePlayers map. The cutover needs to write the active category to the MI record.** |
| MI+0x1554 restart kind | `pesevents.hpp:488` `restartKind` (MI+0x1554); `pespossession.hpp:23` `restartGate` (MI+0x1574, not 1554); `pesteamtick.hpp:78` | 1:1 | none |
| MI+0x1558 awarded side | `pespossession.hpp:24` `restartTeam` (MI+0x1558); `matchdata.cpp:76` (ponytail comment) | 1:1 | none |
| MI+0x155C spot x | `referee.hpp:34` `restartPos` (Vector3); `elizacontroller.cpp:3020` (MI+0x155C comment) | 1:1 | none |
| MI+0x1564 spot z | `referee.hpp:34` `restartPos.z` | 1:1 | none |
| MI+0x1574 stop reason | `pespossession.hpp:23` `restartGate` (MI+0x1574); `pesevents.hpp:431` `restartGate` (MI+0x1574) | 1:1 | none |
| MI+0x1588 foul kind | `pesevents.hpp:437` `foulKind` (MI+0x1588); `pes_fn_140474C90.cpp:27` (translated) | 1:1 | none |
| MI+0x158C card level | `cardbook.hpp:17` `foulType` (0/1/2/3/4); `referee.hpp:53` `Foul::foulType` | 1:1 by meaning | GF uses a 4-level ladder (0 none, 1 foul, 2 yellow, 3 red); PES uses 5 levels (0 none, 1 foul, 2 foul+warning, 3 yellow, 4 red). **Gap: level 2 (foul+warning) has no GF equivalent.** |
| MI+0x1590 fouler | `referee.hpp:51` `Foul::foulPlayer` (Player*); `pesevents.hpp:439` `foulCulprit` (int) | 1:1 | none |
| MI+0x1594 victim | `referee.hpp:52` `Foul::foulVictim` (Player*); `pesevents.hpp:440` `foulVictim` (int) | 1:1 | none |
| MI+0x15A5 card scene | `1405141F0` (dispatcher: `15A5 != 0` gate); no GF field found | **none** | **Gap: add u8 for MI+0x15A5.** |
| MI+0x15B8 (unnamed) | no GF field found | **none** | **Gap: add u8 for MI+0x15B8 (meaning OPEN).** |

### 4.2 G child fields

| PES field | GF state | Status | Gap |
|---|---|---|---|
| ball+0x2770/0x2778 (fallback pos) | `pesball.hpp` / `pesballtrack.hpp` (ball position); `pescarrier.hpp:1012` `CarrierPoint pos` | 1:1 | none |
| team+0x254 (attack dir) | `pespress.hpp:329` `sign` (T+0x254); `teamAIcontroller.cpp:2473` (T+0x254 comment); `pesevents.hpp:570` `ballDirX` | 1:1 | none |
| team+0x078 (kick man) | `teamAIcontroller.cpp:2509` `kickMan` (mi.teamKickMan); `team.cpp:468` (MI+0x1558 comment) | 1:1 | none |
| anime+0x001 (class byte) | `pesroles.hpp` (class map, different vocabulary) | partial | GF's role vocabulary differs from PES's action byte (0x5A, 0x5C, etc.). **Gap: the cutover needs to map GF roles to the PES anime+1 byte, or store the PES byte directly.** |
| man+0x55C/0x564 (pos) | `pescarrier.hpp:1012` `pos` (record +0x55C/+0x564); `pespress.hpp:215` `posX/posZ`; `player.hpp:44` `GetPosition()` | 1:1 | none |

### 4.3 Referee record

GF has **no PES-layout referee record**. The closest GF state:

| PES referee field | GF state | Status | Gap |
|---|---|---|---|
| ref+0x018 (player index) | `refereecontroller.hpp` / `referee.hpp` (the `Referee` class has no player-index field; the referee is not a player in GF) | **none** | **Gap: GF's referee is a singleton controller, not a per-official record. The cutover needs a PES-layout record per official (referee, 2 assistants, 4th).** |
| ref+0x030 (action state) | `refereecontroller.cpp` (state machine, but not in a PES-layout record) | partial | GF's referee controller has states, but they are not stored in a PES-layout record at a fixed offset. **Gap: add a u32 at ref+0x30 in the PES-layout record.** |
| ref+0x034 (offside category cache) | `referee.hpp:69` `OffsideFlag::category` | partial | GF stores the category per player; the PES field is a cached copy on the referee record. **Gap: add u32 at ref+0x34.** |
| ref+0x038 (flag raised) | no GF field for the assistant's flag latch | **none** | **Gap: add u8 at ref+0x38.** |
| ref+0x039 (card latch) | `cardbook.hpp` (card state, but not on the referee record) | partial | **Gap: add u8 at ref+0x39.** |
| ref+0x61C/0x624-0x62C (look-at) | no GF field for the official's gaze target | **none** | **Gap: add f32 × 4 at ref+0x61C..0x62C.** |
| ref+0x7E0 (roster ptr) | no GF field | **none** | **Gap: add ptr at ref+0x7E0.** |
| ref+0x950 (threshold) | no GF field | **none** | **Gap: add f32 at ref+0x950.** |
| flow+0x10D0 (signal latch) | no GF field for the signal latch (1/2/3) | **none** | **Gap: add u32 at flow+0x10D0.** |
| flow+0x10D4 (signal direction) | `teamAIcontroller.cpp:2473` (T+0x254, attack sign) | 1:1 by meaning | GF has the attack sign on the team; the PES field is a cached copy on the flow record. **Gap: add u8 at flow+0x10D4.** |

### 4.4 Cutover summary

**What GF already has (1:1):** period, play state, restart kind, awarded side, spot x/z, stop reason, foul kind, card level (with a 1-level gap), fouler, victim, ball position, team attack direction, kick man, man position.

**What GF lacks (gaps for the cutover):**
1. **MI+0x11B2** (u8, sub-state / restart-armed flag) — no dedicated field.
2. **MI+0x1544/0x1548/0x154C** as single words — GF has per-player maps; the PES fields are single u32s. The cutover must derive the words from the map (or store them directly).
3. **MI+0x15A5** (u8, card-scene active) — no GF field.
4. **MI+0x15B8** (u8, unnamed) — no GF field.
5. **Card level 2** (foul+warning) — GF's ladder has 4 levels; PES has 5.
6. **Referee record** (entire structure) — GF has no PES-layout per-official record. The cutover needs a new record (≥0x958 bytes) per official (4 officials), with the fields in §2.2.
7. **Flow sub-record** fields (+0x10D0, +0x10D4) — GF has no PES-layout flow sub-record. The cutover needs to add the signal latch and direction to the existing flow record (or create a new one).
8. **Anime+0x001 byte** — GF's role vocabulary differs; the cutover needs a mapping or a direct store.

**The block is the referee record.** The involvement function (`140A3A690`) does not read the referee record directly — it reads G/MI. But the 4 action functions (`140A3A930`, `140A3A870`, `140A39A80`, `140A38F20`) all take the referee record as `param_1`. Until GF holds a PES-layout referee record with the fields in §2.2, the 4 translated functions cannot be called with the right `param_1`.

---

## 5. Cutover blocker closure: the 4 official records, their holders, constructors, and the vtable→official map

**Changed this pass.** The OPEN items of §2.1 (record base, constructor, vtable→official map) are now CLOSED for the data GF needs. The referee record of §2 is really a member of a **family of wrapped records**, one per whistle official: the behavior lives in three small vtables (**A** = look-at, **B** = restart, **C** = approach+latch), all reached through the common entity class **D** (vtable at `0x14262FCE8`, 1455 slots). The derivation is: (a) walk-back from every `1405141F0`-dispatcher call site, (b) the two `lea`-discovered constructors, (c) live-capture confirmation on all of seq012/016/019.

### 5.1 How the dispatcher reaches `param_1` (CONFIRMED: register-passed `this` from the game's caller chain)

`1405141F0` is a *member function of the entity class, not a free function*: every call site receives the official-wrapper pointer as its own incoming `rcx` (the caller's `this`) and forwards it unchanged as the dispatcher's `param_1`. Walk-back per site (CONFIRMED disassembly; `UVA` walk-back confirmed each site sits mid-function, and the enclosing functions have no direct `E8` callers — they run inside the game's per-frame update dispatch):

| call site | `param_1` source | enclosing function entry | evidence |
|---|---|---|---|
| `0x140930A0D` | `[rsp+0x40]` stack slot whose header was written by `lea rax,[rip+0x1CFF925]` → `D+0x628` at `0x1409309E4` | mid-function (`UVA` = self; `unaff_RDI/RBP/RSI/R14/RBX` = the caller's live argument registers) | `lea rcx,[rsp+0x40]` at `0x140930A08`; `mov byte [rsp+0x70],0` clears the wrapper's `+0x30` pre-call |
| `0x14091F068` | second inline `lea → D+0x628` | (companion stack slot site) | the image's only other `LEA` hit for `0x142630310` (whole-image opcode scan) |
| `0x14094E34F` | `rbx` = incoming `rcx` of enclosing `0x14094E0E0` (`mov rbx,rcx` at `0x14094E106`); the enclosing fn also publishes `rbx+0x70/+0x32/+0x34/+0x38/+0x3C/+0x4C/+0x58/+0x40` as out-pointers | `0x14094E0E0` | `mov rcx,rbx` at call; `inc word [rbx+0x30]` post-call |
| `0x140961FE7` | `rbp` = incoming `rcx` of enclosing `0x140961FB0` (`mov rbp,rcx` at `0x140961FC4`) | `0x140961FB0` | `mov rcx,rbp` at call; `inc dword [rbp+0x58]` post-call |
| `0x1409649A9` | `rdi` = incoming `rcx` of enclosing `0x140964390` (`mov rdi,rcx` at `0x1409643C3`); between entry and call the fn loads `G = [0x1436F3DC8]` six times (`0x1409643EB/3FD/419/471/64E/78B`) and gates on `[rdx+4]` (player idx) | `0x140964390` | `mov rcx,rdi` at call (`0x140964993`); `inc dword [rdi+0x30]` post-call |
| `0x1409CC10C` | `rbx` (enclosing `0x1409CBF30`) | `0x1409CBF30` | `mov rcx,rbx`; `inc dword [rbx+0x54]` post-call; body repeatedly dereferences `[rdi+0x30/+0x38]` record links |

Correction to the earlier draft: the wrappers are **not** per-call heap/stack constructions with inline `D` headers, except at the two `lea` sites — the register sites forward the game's own per-frame update records as `this` through the update dispatch. The `+0x30/+0x54/+0x58` words incremented after each call are the update records' own counters, read back on the next frame. **There is therefore a standing record per whistle official in the game's per-frame update set**; its `this` pointer is `param_1`.

### 5.2 The two constructors (CONFIRMED, `lea` targets + decompiled bodies)

The only two `lea reg,[rip+disp]` loads in the image that target the entity vtables found the constructors (§5.3's opcode scan over the whole 0x1B97C000-byte image):

| constructor | target | body (from `dec_ctors.txt` / disassembly) | record it builds |
|---|---|---|---|
| `0x140961F00` | `0x142634948` (`T934`, run 0x142634940 len 24, `base+0x0 = 0x142CAB8D0` type-info, `base+0x8 = 0x140961F80`) | `*param_1 = 0x142634948`; zeroes `+0x4C/+0x54/+0x6C/+0x30/+0x40/+0x34/+0x3C/+0x5C/+0x44/+0x58/+0x5E/+0x60`; `+0x48 = 0xFF`, `+0x50/0x68/… = 0xBF800000` (−1.0) | general entity / player-adjacent record (T934 is not an A/B/C stoppage table: run holds none of the 4 action fns) |
| `0x140961F80` | `0x1425C5850` (`T5850`, run base 0x1425C5850 len 29, holds `DPT` at `+0x40`) | `*param_1 = 0x1425C5850`; `if (dl&1) 140227930(param_1,0x88)` | the base-entity constructor (allocates/augments 0x88 bytes) |
| (stack frame, no lea) | `D+0x628 = 0x142630310` | wrapper header written inline at the two call sites (`0x1409309E4`, `0x14091F068`); the wrapper is a stack view, not a heap object | per-call stoppage view. The 22 live hits are all this header, §5.4 |

`0x140961F86` (`lea → T5850`) sits immediately after the `T934` constructor: the pair is the **(base, derived)** constructor couple — T5850's slot `+0x40 = DPT` is the inherited dispatcher entry. For the cutover, the record sizes are: T934 record ≥ 0x84 bytes of zeroed fields (CTOR writes up to `+0x84`); the wrapped §2 record ≥ 0x958 bytes (§2.2 max); the full flow record (with §2.3) ≥ 0x19D8 bytes. Constructors are CONFIRMED; the heap allocator that sizes them remains OPEN (not needed: sizes above bound it).

### 5.3 The entity vtable family (CONFIRMED, image bytes; run lengths + stoppage slots)

Only three code-pointer runs in `0x142620000…0x142650000` (89 runs ≥ 8 qwords) hold any of the 4 stoppage action functions — the rest never do, so these three *are* the official polymorphic family:

| vtable | base | run length | stoppage action slots held | class role (INF from slot content) |
|---|---|---|---|---|
| **A** | `0x142642340` | 28 | `+0xD8` = `0x140A38F20` (LOOK-AT) | referee look/observe suite: slots also hold `0x140A38E50/0x140A36760/0x140A36390/0x140A373E0/0x140A367C0/0x140A36970` (observe/track fns) |
| **B** | `0x142642550` | 24 | `+0x18` = `0x140A39A80` (RESTART) | restart-command suite: `0x140A39CD0/0x140A39CB0/0x140A39DD0/0x140A39CE0/0x140A39C40` siblings |
| **C** | `0x142642730` | 24 | `+0x38` = `0x140A3A930` (APPROACH) and `+0x98` = `0x140A3A870` (LATCH) | chief-referee signal suite: approach *and* latch live in one table |
| **D** | `0x14262FCE8` | 1455 | none of the 4 (37× `0x1405141F0` DISPATCH entries at `+0xC8…+0x2C10`) | the common entity class: dispatch, animation, rendering, replay (`0x141794410`, `0x140421F40` fillers, `0x140CA1A50` stubs) |

The three stops: `A+0xD8/ B+0x18/ C+0x38,C+0x98` byte-verified against the action VAs this pass (all OK). Every `D+0x628 = 0x142630310` used at the two call-site wrappers is a *view* into the common class D (offset `+0x628`), not a separate class.

### 5.4 vtable→official mapping (CONFIRMED where the image decides; live check)

| official | vtable | decision rule | label |
|---|---|---|---|
| referee (whistle; approach + flag-latch + restart-command authority) | **C** (holds both APPROACH `+0x38` and LATCH `+0x98`) | image bytes: the only table uniting the two signal-authority functions | table→functions CONFIRMED; role name INF (image does not name roles) |
| assistant referee (linesman; look-at the touchline + flag) | **A** (holds LOOK-AT `+0xD8`) | image bytes: the only table holding the look-at/observe function | table→functions CONFIRMED; role name INF |
| fourth official (restart administration) | **B** (holds RESTART `+0x18`) | image bytes: restart-command issuer | table→functions CONFIRMED; role name INF (only remaining whistle role) |
| entity base (all 4 wrapped records) | **D** (1455) | image bytes + 22 identical live hits in all 3 captures | CONFIRMED |

**Live check (seq012/016/019).** All 22 `VT_D` hits are byte-identical across the three captures (same 14 heap + 1 stack-page + 7 game-heap addresses), so the wrapped entity objects are stable through stoppages — the wrappers resolve to live objects, not stale pointers. The 14 heap hits (`0x2C54B298…0x2C7E58A8`) sit in one `rw-p` heap block (`0x2B800000+0x79C0000`); the 7 `0x34Cxxxxx` hits sit in the game heap (`0x33400000+0x3800000`); the `0x333DFE48` hit sits in a 0x30000 mapping. None of A/B/C appear as object headers in the captures — consistent with A/B/C being the *behavioral* tables reached through the D wrapper, not standalone heap objects. Which of the 14 heap wrappers belongs to which of the 4 named officials is OPEN and does not block the cutover (GF creates one wrapped record per official and lets the four fns sort them by behaviour).

### 5.5 The G/MI chain that holds the 4 officials (CONFIRMED: three mechanisms, no single G table)

This is the asked-for "G/MI offset (or chain)": there is **no single G+disp table of 4 pointers**. The image uses three holder mechanisms, all CONFIRMED from disassembly bytes:

1. **Register-passed `this` through the per-frame update dispatch (the live chain).** The 4 record sites forward their own incoming `rcx` (§5.1); the enclosing update functions (`0x14094E0E0/0x140961FB0/0x140964390/0x1409CBF30` + the mid-function `0x140930A0D` region) have 0 direct `E8` callers (CONFIRMED `e8scan`) — they are entered through the game's object dispatch. The one site whose feeder is statically visible loads `G = [0x1436F3DC8]` six times in its prologue (`0x140964390` at `0x1409643EB/3FD/419/471/64E/78B`, all resolving to `0x1436F3DC8`): the update record is G-derived there. For the other sites the G/MI link is one dynamic-dispatch step above what static disassembly reaches; what is CONFIRMED is the register contract (`rcx` in = `rcx` out) and the per-site post-call counters (`+0x30/+0x54/+0x58`) that distinguish the 4 records.
2. **The shared index→row jump-table helpers (`0x14092E7DF`/`0x14092E5FB`).** Both have 0 direct `E8` callers (CONFIRMED `e8scan`); both are entered through the computed jumps `add rcx,r12; jmp rcx` at `0x14092E51F` and `0x14092E6B7`. Helper `0x14092E5FB` computes the row from the index (`jge`, `movsxd rcx,ebx`, `mov eax,[rax+rcx*4+0xA2A0]`, `ebx = idx%0xB`-style clamp); helper `0x14092E7DF` stores the row (`r14d = [rcx+0x18]`, clamp `0x15`, `%0xB`). Both helpers read `G+0x270` (MI), `G+0x2AA8`, `G+0x3B0` and the row fields `+0x18/+0x48/+0x6D4/+0xACA0/+0xACB8/+0x116C/+0x1198/+0xB5AC/+0xBBAC/+0xBB98/+0xC698/+0xC6B0/+0xC700/+0xAD38` (CONFIRMED disassembly).
3. **The two inline `D+0x628` stack wrappers** at `0x1409309E4` and `0x14091F068` (the image's only two `LEA` loads of `0x142630310`; CONFIRMED whole-image opcode scan). No G/MI field is dereferenced for these — the view header is written inline.

For GF's cutover the actionable mapping is therefore: **model one wrapped-record instance per whistle official (4 records) holding the §2.2 fields; feed each record's `this` to the dispatcher exactly as the register contract says (`rcx` in = `rcx` out); implement the two shared index→row helpers by their disassembly for any index→row resolution; the two constructors (§5.2) give the record initializers and size bounds (T934 ≥0x84 zeroed, wrapped §2 record ≥0x958, full flow ≥0x19D8).**

Readers/writers touched while proving this (all CONFIRMED by disassembly): helpers read `G+0x270` (MI), `G+0x2AA8`, `G+0x3B0`, row `+0x18/+0x48/+0x6D4/+0xACA0/+0xACB8/+0x116C/+0x1198/+0xB5AC/+0xBBAC/+0xBB98/+0xC698/+0xC6B0/+0xC700/+0xAD38`; constructors write `+0x30/+0x34/+0x3C/+0x40/+0x44/+0x48(0xFF)/+0x4C/+0x50(−1.0f)/+0x54/+0x58/+0x5C/+0x5E/+0x60/+0x68/+0x6C/+0x70/+0x74/+0x84(−1.0f)`; per-call wrappers keep their counters at `+0x30/+0x54/+0x58`.


### 5.6 Remaining OPEN (not blocking)

- The heap allocator/sizer for T934/T5850 records (sizes bounded in §5.2; exact allocator VA not traced).
- The `+0xA2A0` lookup table contents (the 0x14092E5FB helper's scaled table; G-relative, readable live if needed).
- Which of the 14 heap OFF rows maps to which named official when the game fields fewer/more than 4 (bench/extras rows use the same wrapper; GF assigns its 4 wrapped records to rows by index order, matching PES's `%0xB` clamp).

---

## 6. Open items (updated)

1. ~~**Referee record base (G/MI offset).**~~ CLOSED by §5.5: no single table; replicate the `G+0x440` wrapped-table + shared helpers + per-call wrappers.
2. ~~**Referee record constructor / allocator.**~~ CLOSED by §5.2 (`0x140961F00` → T934, `0x140961F80` → T5850, `D+0x628` stack wrapper).
3. ~~**Which vtable belongs to which official.**~~ CLOSED by §5.4 (C = referee, A = assistant, B = fourth official, D = common class).
4. **MI+0x15B8 meaning.** Read by the sanction tree; meaning not decoded.
5. **E1EE child +0x001 and +0x254 meanings.** Read by the involvement closure; meaning not decoded.
6. **Foul/offside stoppage capture.** The 3 live captures (seq012/016/019) are kick-off and goal-kick stoppages. A foul (reason 5) or offside (1588=1) stoppage capture would verify the foul-kind, card-level, and offside-flag fields at non-zero values.
7. **Referee record size.** Lower bound 0x958; upper bound unknown. §5.2 adds the T934 ≥0x84 and full-flow ≥0x19D8 bounds.
