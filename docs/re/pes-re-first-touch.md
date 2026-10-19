# PES 2021 first touch on a loose ball: chase predicate FUN_1404F4BE0, touch path, GF replacement map

Read-only RE. No code edits, no commits. Scratch: `<workdrive>/gf-FirstTouch-scratch/` (`d_*.c` decompiles, `a_*.s` disassembly).
Tags: **CONFIRMED** = read in the decompile and checked against the disassembly bytes, or read from the install file. **INF** = inference, with the reason. **OPEN** = not decoded, with the blocker named.
VAs are PES2021 image VAs (base 0x140000000). File mapping, checked against the PE section table: `.trace`/`.rdata`: file = VA − 0x140000A00; `.data`: file = VA − 0x140000E00.

---

## 0. Answers

1. **Who calls FUN_1404F4BE0, and when.** CONFIRMED: one direct caller in the image. `E8` rel32 scan of `pes_full.bin` and of `PES2021.exe`, plus absolute-pointer and RIP-relative `lea` scans, found exactly one reference: **0x1404FED44, inside the possession latch FUN_1404FE9C0** (step 3b, called with the holder id `MA+0x2483C`). Nothing else references it (no vtable, no table, no `E9` jump). It runs once per team tick, and only when the ball has no carrier, on a 5-frame cadence (§4).
2. **So the predicate is not a per-player chase command.** CONFIRMED from §1. It answers "is this holder still contesting the ball?" for the latch's possession rule. Nothing in the image asks it "should player X run at the loose ball". The brief's premise that the designated chaser is decided by this predicate is wrong. What issues the loose-ball run is OPEN (§10, item 1).
3. **How a touch becomes possession.** CONFIRMED structure: a clip with a hit record makes contact. The contact is recorded as a **K=7 touch event** by the detector FUN_140A0DFD0 when a touch probe (`G+0x2A80`) is active. The **carrier** `MA+0x2601C` is then recomputed from the newest keyed event in ring 1 (FUN_140457070). The latch FUN_1404FE9C0 turns the carrier's side into the team-possession byte `T+0x255`. The probe writer and the K=8 key builder are OPEN (§5.6).
4. **What GF lacks.** The GF designated branch never touches a PES clip whose `touchFrame == -1`. On arrival, the only touch path (`humanoid.cpp:526-531` gate → `:545` branch) requires the man to be moving away from the ball, and its first alternative needs `e_FunctionType_Movement`, which PES locomotion clips are not (§6). GF has no K=7/K=8 touch record, no carrier from the event store, and no PES chase or possession ranks. §8 maps each step.

---

## 1. Method and evidence

- Decompiles and disassembly: Ghidra bridge `decompile_function_by_address` / `disassemble_function` (127.0.0.1:8080). The `/get_xrefs_to` endpoint is not used.
- Call scans: `pes_full.bin` (462 MB, file offset = VA − base, checked against `0x1404F4BE0` bytes `48 89 4C 24 08 57 48 81`) and `PES2021.exe` (same bytes at the mapped offset). Scans: `E8`/`E9` rel32 over the whole image, absolute `qword` `0x1404F4BE0`, RIP-relative `48/4C/49 8D` with `modrm & 0xC7 == 0x05`.
- Floats: read from the install bytes, not from the decompile. Ghidra drops XMM0 returns (`FUN_1415A1780` sqrt, the `FUN_14044E020` arrival value and `FUN_14043A0A0` angle value all appear as `extraout_XMM0` in the decompile), so every float test in §3 is taken from the asm.
- Not done: the rea provider (the Ghidra bridge answers directly); byte-level switch-table scan (see §10, item 6).

---

## 2. Constants (install bytes, CONFIRMED)

| VA | value (f32 bits) | used by |
|---|---|---|
| `0x14259BDDC` | `1.5` (`0x3FC00000`) | predicate: ball-height gate `> 1.5` (S0 and BallInfo), budget override |
| `0x142598580` | `180.0` (`0x43340000`) | `FUN_14043A0A0` wrap threshold |
| `0x14259858C` | `360.0` (`0x43B40000`) | `FUN_14043A0A0` wrap period |
| `0x142588218` | `−1.0` (`0xBF800000`) | wrap (negative branch) |
| `0x14256AE1C` | `1.0` (`0x3F800000`) | wrap (positive branch) |
| `0x142565D30` | abs mask `0x7FFFFFFFFFFFFFFF` (qword) | `|x|` in the predicate and in `FUN_14081FF70` |
| `0x14259A760` | `35.0` (`0x420C0000`) | predicate: `|wrap180(C2[s] − bearing(v))| > 35°` |
| `0x1425C48B0` | `2.89` (`0x4038F5C3`) | predicate: `d²(s, ball) > 2.89` (m², so 1.7 m) |
| `0x14259BE44` | `64.0` (`0x42800000`) | predicate: `pairDist(oth, s) < 64` (unit INF, §3.4) |
| `0x14259BE04` | `12.0` (`0x41400000`) | predicate: `dist(s) + 12 > dist(leader)` (m) |
| `0x1425C7BA8` | `−28.0` (`0xC1E00000`) | zone 2 cut: `v ≤ −28` |
| `0x1425C48C0` | `22.0` (`0x41B00000`) | zone 1 cut: `v < 22` |
| `0x14351AE50` (.data, u32) | `54` (`0x36`) | `gs` = `FUN_141E532E0` |
| `0x14256AE18` | `0.01` (`0x3C23D70A`) | hit record cm → m (picker) |
| `0x142598578` | `0.2` (`0x3E4CCCCD`) | picker: class 3..5 extra window |
| `0x142565600` | `0.5` (`0x3F000000`) | picker: row class 0x23 |
| `0x142590D98` | `0.3` (`0x3E99999A`) | picker: row class 0x32 |
| `0x142565604` | `2.0` (`0x40000000`) | picker and pesdodge (the 2.0 m post test) |
| `0x1425CC238` | `0.06` (`0x3D75C28F`) | picker: action 0xD extra |
| `0x14259B89C` | `0.4` (`0x3ECCCCCD`) | picker: intermediate |

`gs = 54` agrees with the docs. Action-id constants in the picker are the per-row values read from `DAT_14348DAC0`; those are data, not code constants.

---

## 3. FUN_1404F4BE0: chase / contest predicate

### 3.1 Identity

- VA `0x1404F4BE0`, frame `0xC0`. Signature (Ghidra): `undefined8 FUN_1404f4be0(longlong ctx, uint s)`. Returns AL, 0 or 1.
- `ctx`: match context. `MA = *(ctx+0x18)` (ball cluster), `E = *(ctx+0xE0)` (event store). `G = *(0x1436F3DC8)`, `MI = *(G+0x270)` (match-phase block).
- `s`: player id 0..21. Id `> 21` returns 0 (asm `4BF3`).
- Output 1 means "the caller's side is still contesting the loose ball" (latch use: `FUN_1404FE9C0` step 3b consumes it as "holder still contesting"). Name INF; the body decides it.

### 3.2 Pseudocode (checked against the asm, CONFIRMED)

Asm line references are the `0x1404F4xxx` offsets in `a_1404f4be0.s`.

```
chase(ctx, s):
  if s > 21: return 0                                              # 4BF3
  slot = s % 11;  team = (s >= 11)                                  # 4C3A magic /11, 4C5D
  T = team_rec(G, team)                                             # FUN_140A48A30
  if T.u8[+0x255] != 0: return 0                                   # 4C8D  team already has the ball
  z = zone(MA.+0x2482C, T.s8[+0x254])                              # FUN_140A4F1E0, 4CA0
  if z == 0: return 0                                              # 4CAF/4CB1
  budget = (z == 2) ? gs : gs >> 1                                 # 4CB6 / 4CC0
  S0 = FUN_140A57800(*(G+0x40))  -> sample ptr                     # = FUN_140A6CF70(BallInfo, 0)
  if S0.f32[+4] > 1.5: budget = gs                                 # 4CEF-4D08  (replace, not add)
  if T.i32[+0x4C00 + 4*slot] == 0: budget += 2*gs                  # 4D10-4D32  (LEA R14D,[R14+RAX*2])
  H = MA + 0x24840                                                 # R12
  oth = H.u32[22 * (team ^ 1)]                                     # 4D7A-4D9F  other team's head
  if oth > 21 or MI.u8[+0x1748 + oth] == 0: return 0               # 4DA2-4DB8
  ev = FUN_140A3E190(E, 0)                                         # newest event
  kind = ev.i32[0];  actor = FUN_140A62EA0(ctx, ev.u64[+4])        # 4DD2-4E35
  BI = *(G+0x40)
  if kind in {1,2,3,0x10} and S0.f32[+4] > 1.5 and BI.f32[+4] > 1.5
     and actor < 22 and team(actor) != team
     and FUN_1404C8EA0(MA, oth, s) < 64: return 1                  # 4E0A-4E8E
  F = ( cls_a(human(s).s8[+1]) and not (human(s).u32[+8] >> 22 & 1) )
   or ( cls_b(human(s).s8[+1]) and not (human(s).u32[+8] >>  9 & 1) )   # 4E82-4EC7
  if F: budget >>= 1                                               # 4EC7-4ECA
  if kind in {1,2,3,0x10}:                                         # 4ED2-4EEC
    dt = |MI.u32[+0x1198] - ev.i32[+0x10]|                         # 4EEF-4F0B
    if dt < gs and not F:                                          # 4F12-4F34
      if FUN_1404C8E60(MA, s) > 2.89: return 1                     # 4F98  (d² of s to ball)
      D = FUN_14043A0A0(bearing(BI.+0x2948), MA.f32[+0x247D4 + 4*s])   # 4F4F-4F7D
      if |D| > 35: return 1                                        # 4F8B
  if budget + FUN_14044E020(H, s) > FUN_14044E020(H, oth): return 1  # 4FA5-4FC0
  if S0.f32[+4] > 1.5 and not F:                                   # 4FC9-4FD7
    dS = dist(pl(s), ball2D)                                       # FUN_140A48A10, FUN_1415A1780
    L  = T_other.i32[+0x78]
    if L <= 21 and dS + 12 > dist(pl(L), ball2D): return 1         # 5026-5088
  return 0                                                         # 508C
```

Verdict on the body: **CONFIRMED** (asm). The bearing is `FUN_14046F230(BI+0x2948, out)` → `FUN_14046F260(out, 0, 0, vx, vz)`: the heading of `(vel.x, vel.z)`, read as `out[0]`; the doc says atan2(dx,dz) in degrees (`pes-re-defensive-shape.md:398`). Not re-decoded: INF on the unit (degrees, matches the 35° and 180/360 constants).

The pseudocode is ordered as the asm. The two early returns (other head off-pitch, `oth > 21`) are hard gates, not skips.

### 3.3 Inputs and outputs

| input | source | PES field |
|---|---|---|
| team possession byte | `T+0x255` | written by the latch (§4.2) |
| attack-direction sign | `T+0x254` (signed char) | ±1 |
| ball x (plane) | `MA+0x2482C` (f32) | sim-ball copy (`B1`) |
| ball height | `BI+4` (f32) and `S0+4` (f32) | INF high: the y axis (height). `pes-re-foundation-matchball.md` F7 names `+4 y` as "1.5 m high-ball tests" |
| registered position | `T+0x4C00[slot]` (i32) | role index 0..12, not a countdown (`pes-re-foundation-teamplayer.md:98`); `==0` gives `+2·gs` (INF: class 0 is a keeper-like position; not verified) |
| on-pitch | `MI+0x1748[id]` (byte) | flag |
| newest event | `FUN_140A3E190(E,0)` | kind `+0x00`, actor key `+0x04`, frame `+0x10`, sub `+0x40`, flag `+0x2D` |
| holder/head lists | `MA+0x24840` (per team 22 u32 entries, head at `+0x58·team`) | ranked list |
| rank bytes | `MA+0x24840+0xB2+id`; bucket `11·team(id)` | `FUN_14044E020` addressing |
| C1 d² | `MA+0x24710 + (MA+0x247BE[id] + 11·team(id))·8`, f32 at +0 | `FUN_1404C8E60` |
| C2 heading | `MA+0x247D4 + 4·id` (f32) | writer NOT FOUND (`pes-re-foundation-matchball.md` C2) |
| C3 pair | `MA+0x22E4C + id_a·200 + 4 + (byte[+0xB2+id_b] + 11·team(id_b))·8` | `FUN_1404C8EA0(MA, a, b)` |
| dynamic class | `human(s)+1` (anime action), `human(s)+8` (flags u32) | §3.4 |
| leader | `T_other+0x78` (i32) | the other team's nearest man |
| output | returns AL | 0/1 |

### 3.4 Helpers (all decompile-level unless noted)

| VA | name / role | verdict |
|---|---|---|
| `0x140A48A30(G, i)` | team record: `i<2 ? G+(i+0xA1)·0x28 : G+0x1928` | CONFIRMED (decompile) |
| `0x140A48A10(G, i)` | player/match record `pl(i) = G+(i+0xBF)·0x28` (x `+0x55C`, z `+0x564`, vel `+0x574/+0x57C`) | CONFIRMED (decompile); field offsets from `pes-re-contact.md` §Notation, read-only |
| `0x140A48910(G, i)` | anime record `human(i) = G+(i+0xA5)·0x28` (+1 class byte, +8 flags) | CONFIRMED (decompile) |
| `0x140A489F0(G, i)` | AI record `ai(i) = G+(i+0xF3)·0x28` (+0x1470 flag, +0x70 ability) | CONFIRMED (decompile) |
| `0x140A4F1E0(p, dir)` | zone: `v = dir·p.x`; `2` if `v ≤ −28`; `1` if `v < 22`; else `0` | CONFIRMED (decompile; call-site asm checked) |
| `0x1404C8E60(MA, id)` | C1 **squared** distance of id to ball (`MA+0x24710`); `id > 21 → 0` | CONFIRMED structure; unit m² CONFIRMED by `pes-re-foundation-matchball.md` B3/C1 live check ("(player−ball)²") |
| `0x14044E020(H, id)` | arrival value of id in its own team bucket (`+4`); `id > 21 → 0` | CONFIRMED structure; unit INF: frames (same unit as `gs` budgets) |
| `0x1404C8EA0(MA, a, b)` | C3 value of `b` in `a`'s sorted list (`+4+(rank+11·team(b))·8`); `a ≥ 22` uses base 0 | CONFIRMED structure; unit INF (C3 field "dist-or-frames") → the `< 64` test is m² or frames, not verified |
| `0x14043A0A0(x, y)` | `wrap180(y − x)`: both into `[0,360)`, then `d = y−x`; if `|d| > 180` add/sub `360` | CONFIRMED (asm and constants) |
| `0x1415A1780(d2)` | `sqrtf` through the IAT `_DAT_1425301F0` | CONFIRMED (asm `CALL [0x1425301F0]`) |
| `0x141E532E0()` | returns `gs` = `DAT_14351AE50` = 54 | CONFIRMED (.data) |
| `0x140A57800(BI)` | `FUN_140A6CF70(BI, 0)`: prediction sample 0 ptr; f32 `+4` = height (INF high, F7) | CONFIRMED call; `FUN_140A6CF70` body NOT decoded |
| `0x140A57720(BI)` | `BI+0x2948` (velocity) | CONFIRMED |
| `0x14046F230/F260` | bearing of `(vx, vz)`, degrees | doc-cited (`pes-re-defensive-shape.md:398`), not re-decoded |
| `0x140A6E5E0(cls)` | class bit: bit 5 of dword `0x1426484F0[cls]` (cls ≤ 0x5D) | CONFIRMED (asm table bit); name trap (doc `contact.md` §Notation) |
| `0x140A6E340(cls)` | class bit: bit 16 of dword `0x1426484F0[cls]` (byte `+2`, bit 0) | CONFIRMED (asm table bit); name kick 8-10 (doc) |
| `0x140A3E190(E, n)` | `n==0`: newest `E+0x180698`; else ring-0 record; empty sentinel `0x37` → `0x1436F3D60` | CONFIRMED (decompile) |
| `0x140A62EA0(ctx, key)` | key `{team, idx}`: `team<2 → player`, else `0xFF` | CONFIRMED (decompile) |

The `F` flag (`4E82-4EC7`) uses two class predicates and two bits of `human+8`. The class names in parentheses come from `pes-re-contact.md`. The bits (22 and 9) are unnamed; the behaviour is CONFIRMED.

### 3.5 Verdicts

- Structure of the body: CONFIRMED against the asm for every branch and return value.
- Meaning of `1`: INF (contest / chase). The latch consumes it as "holder still contesting".
- Unit of the `< 64` pair test and of the arrival values: INF (§10, item 3).

---

## 4. Callers, latch and cadence

### 4.1 Rel32 scan result (CONFIRMED)

| reference | site | owner |
|---|---|---|
| direct `E8` | `0x1404FED44` | `FUN_1404FE9C0` (latch), step 3b |
| absolute qword `0x1404F4BE0` | none | — |
| RIP-relative `lea` | none | — |
| `E9` tail | none | — |

`FUN_1404FE9C0` is called only from `0x1404F6D60` (team tick `FUN_1404F6C30`, CONFIRMED E8 scan). `FUN_1404F6C30` is called only from `0x14041EDD7` (routine `FUN_14041ECF0`, doc-cited, not re-verified). `FUN_140A428D0` (setter of `T+0x255`) is called only from `0x1404FEB8D`, inside the latch.

### 4.2 Possession latch FUN_1404FE9C0 (decompile; asm not re-read except the call site)

Signature: `byte FUN_1404fe9c0(longlong team_tick)`. `param+0x6D48` = ctx, `+0x6D50` = T, `+8` = team side. Returns `old != new` (changed flag).

Steps (decompile `d_1404fe9c0.c`, cadence from lines 60-130):

1. `MI+0x116C == 2` (restart): if `FUN_140A3C950` returns 0, side = `MI+0x1558 == team` and the latch stops there; if it returns nonzero, the normal steps run. `MI+0x116C == 3`: side = `MI+0x1558 == team` directly.
2. Carrier valid (`FUN_140A627E0(ctx,0)` < 22, i.e. `MA+0x2601C`): side = `team(carrier) == team`.
3. Carrier invalid and `frame − newest.frame < gs/6` (9 frames): newest kind in {1,2,3,0x10} and actor valid → side = `team(actor) == team`. The hysteresis detail is INF (`pes-re-possession-latch.md` §15).
4. Carrier invalid and `frame − newest.frame ≥ 9`:
   - **Cadence**: `(frame − newest.frame − 2) % 5 == 0` (decompile `uVar8 == (uVar8/5)*5`), newest kind ≠ 5, and if kind == 3 then `sub(+0x40) ∉ {3,4}` or `BI+0x80 ≠ 0`.
   - **3a**: `MI+0x119C < 3·gs` (162), `newest+0x2D ≠ 0`, newest kind ∈ {3,4,5}: side = `team(actor)`, actor = `FUN_140A62EA0(ctx, newest+4)`.
   - **3b**: `h = MA+0x2483C`; `FUN_1404F4BE0(ctx, h) == 0` and `h < 22` and (`T_side(h)+0x255 ≠ 0` or `T_other_head+0x1470 == 0`): side = `team(h) == team`.
5. Otherwise the old value is kept.

Then `FUN_140A428D0(T, new)`, `FUN_1404F4AE0`, `FUN_140A42570`, `FUN_140A71520`, `FUN_140A42480` run.

Consequence (CONFIRMED): the predicate runs **only** in step 4b. That is: no carrier, at least 9 frames after the newest event, on the cadence residue, once per team tick, with the holder as its argument. It never runs per player and never runs while a carrier exists.

Cadence source: the decompile line `uVar8 == (uVar8/5)*5` with `uVar8 = frame − newest.frame − 2` is CONFIRMED structurally. The asm of the cadence test was not re-read (INF on the exact opcode form only).

### 4.3 Team tick and frame cadence

Team tick `FUN_1404F6C30` runs once per team from the routine that calls it at `0x14041EDD7`. The latch is called at `0x1404F6D60`, inside the team tick. So the predicate is evaluated **per team, per tick, at most once, gated as in §4.2**.

---

## 5. How a touch becomes possession

### 5.1 Carrier write, `MA+0x2601C` (CONFIRMED structure)

The writer is **not** a standalone function. The store `mov [r13+0x2601C], edi` at `0x140457259` is inside `FUN_140457070` (the disassembly at `a_140457070.s:130`). This settles the "NOFUNC" note in `pes-re-foundation-matchball.md` B4/E5b.

Call chain: `FUN_1404582C0` (caller of `0x14045832B`) → `FUN_140458880` (`E8` at `0x14045832B`) → `FUN_140457070(param_1, flag)`. `FUN_140458880` is called only from `FUN_1404582C0`, and `FUN_1404582C0`'s own caller is OPEN (§10, item 4).

`FUN_140457070` carrier logic (decompile `d_140457070.c:129-150`):

```
k = FUN_140A3E7D0(E, ring 1, n = 0)      # newest ring-1 key {team, idx}; sentinel 0x37 -> FUN_140A60500()
if FUN_140A60530(k):                     # team < 2 and idx < 0x28
    carrier = FUN_140A62E40(team_rec(k.team), k)   # team_rec.i32[0] == k.team, then FUN_140A3B320(rec, idx)
else:
    carrier = 0xFF
MA.u32[+0x2601C] = carrier
```

Plus a second store in the same function: `*(G+0x2600C) = FUN_140A62E90(...)` from a ring-0 scan, the designated owner (INF: matches `pesdodge.hpp:195`, "designated owner", `+0x2600C`).

Verdict: the carrier is the actor of the **newest ring-1 key**. Ring 1 holds K=8 records (`FUN_140A3DC40` routing, `pes-re-possession-latch.md` §11: 8 → ring 1). So the carrier is the last K=8 key's player, and it persists until a key is invalid. K=8 is pushed by FUN_140A0DFD0 (§5.3).

### 5.2 Designated possession (CONFIRMED structure)

`FUN_140457070` also computes the `+0x2600C` designated owner from a 9-entry ring-0 scan with the `MI+0x1168 == 9` branch. Not needed for the touch itself; listed so the port keeps both fields.

### 5.3 Touch producer FUN_140A0DFD0 (CONFIRMED structure, asm not re-read)

- VA `0x140A0DFD0`. Signature `void (longlong D, int *rec)`. `D` = detector context, `E = *(D+0xBD08)`.
- Called **only through a vtable** (zero direct `E8` callers). The dispatcher is the detector loop (`FUN_140A0D3F0`, `FUN_140A0D520`, `FUN_140A0E400`, per `pes-re-possession-latch.md` §5). Those loops are called from the event stage of the match tick (`FUN_1404267F0`).
- Returns unless `rec[0] == 0x14` (input record type 0x14).

```
FUN_140A0DFD0(D, rec):
  if rec.i32[0] != 0x14: return
  G = *(0x1436F3DC8); MI = G+0x270
  if rec.u8[+0x18]:                              # branch A
     K8 record: frame = MI.+0x1198, state = FUN_140A3C050 .. (key from FUN_140A867A0/867D0/A60520)
     if FUN_140A60530(ring1 newest key): push K=8  (FUN_140A3DC40)
  if rec.u8[+0x19]:                              # branch B
     if ring1 newest key valid: push K=8
     probe = *(G+0x2A80)
     if probe.i32[+0x64] == 1 and probe.i32[+0x18] < 22:
        cand = FUN_140A62F20(team_rec(team(cand)), cand_id)       # key
        cmp  = FUN_140A60320(ring0_newest.+4, cand)               # semantics OPEN
        if cmp != 0 or ring0_newest.kind != 0xF: push K=7         # 0xF from the probe's kind
```

Both branch A and B build the K=8 record with `FUN_140A3D060(rec, 8, frame, frame)` and push it via `FUN_140A3DC40` only when the ring-1 newest key is valid.

Consequences (CONFIRMED structure):

- **K=7 (touch)** is pushed only from branch B, only when the probe `G+0x2A80` is active and names a player, and the comparison in `FUN_140A60320` passes or the newest kind is not 0xF.
- **K=8** is pushed from both branches whenever the ring-1 newest key is valid. So K=8 persists once valid.
- Which producer sets the probe's `+0x18` and `+0x64`: **OPEN** (§5.4).

The routing table for K → ring (`pes-re-possession-latch.md` §24) says 7 → ring 0 and 8 → ring 1. The newest-copy path (`FUN_140A3DC40`, `iVar1 == 7`) copies a K=7 record into `E+0x180698`, the newest slot. So the K=7 touch is the newest event, and the K=8 key is the carrier source.

### 5.4 Touch probe G+0x2A80 (OPEN)

The probe is a struct at `G+0x2A80` with `+0x18` = candidate player id (int) and `+0x64` = active flag (int, value 1 when live). Its writer was not found in this pass. It is the one input that turns a contact into a K=7. Method to close it: E8/absolute scan for `G+0x2A80` (offset `0x2A80` dereferenced off `DAT_1436F3DC8`).

### 5.5 Clip choice and acceptance

**The clip picker FUN_14081FF70 (CONFIRMED structure; semantics INF).**

- Signature (Ghidra): `int FUN_14081ff70(longlong ctx, float *ball, uint action, ushort *out_hit, int *out_clip, float *out_offset, ...)`. Returns the clip id, or `0x1125` for none.
- Row table `DAT_14348DAC0`, row stride `0x1C` bytes, row count from `DAT_1436F3314` (a runtime value; the static image value is not a row count, so the count is not usable from the install).
- Row layout (from the decompile's indexing, `local_110·7` dwords, so stride 0x1C): `+0x00` u32 clip id; `+0x05` u8 priority (the `bVar3` read, the value of `(&DAT_14348dac5)[lVar19]`); `+0x06/+0x07` u8 hit keys; `+0x08` f32 x-window base; `+0x0C` f32 z-window base; `+0x10` f32 x-window extension; `+0x14` f32 z-window extension.
- Per row, after the action gates: the predicted ball in clip space (`param_2`, normalised) must pass the x window `row.+8 ≤ x < row.+8 + fVar26 + row.+0x10` and the z window `row.+0x0C − fVar26 < z < local_138 + row.+0x14`. `fVar26` is 0 by default, or set by the picker's class switch: 0.2 (class 3..5), 0.5 (class 0x23), 0.3 (class 0x32), 0.06 (action 0xD). `local_138` is set by action. Then a 1-D test on the second axis against the row's distance. A row that passes sets the winner if its priority byte is ≤ the current one (last wins on tie). Hit point: `FUN_1406AFF90(FUN_1406B0ED0(), …, clip id, 0, 0)` scaled by `0.01` (`0x14256AE18`). The 3-D distance `fVar23 = |ball − hit|` (FUN_1415A1780) is compared with the best so far.
- No FieldInfo / edge test (confirmed by `pes-decode-TouchReach.md` §1, which read the same function).
- Axes (which of `param_2[0..2]` is height): **INF**. The hit record stores z, y, x in cm (`pes-re-anim-select.md` §6.1), and the picker's z-window reads `param_2[2]`. Mapping to height is not settled.

**Receive chain (doc-cited, not re-decoded here).** A defender who reaches a loose ball issues the same **TRAP, anime 7, contact frame 18** (`pes-re-receiving.md` §6, OBS live). The first-touch choice is `FUN_1409F8A50` (`pes-re-receiving.md` §5): kind 0 control, 2 running trap, 3 one-touch kick, 4 pad-forced. A stationary interceptor's touch is forced to kind 0 (no turning touch); a fast non-own-team ball gives a 90° turn allowance when running (`pes-re-receiving.md` §5 step 5). Speed threshold `neutralTrapAutoFront` = 100 km/h (live, same doc).

**Contact solver.** The physical contact of the hit frame is in `FUN_140843E60` (ball-hit test, `pes-re-contact.md` §1.1, returns `0xFF` for "hit by ball") and `FUN_1406B5C20` (two-body solver, `pes-decode-TouchReach.md` §1). Neither was decoded here (OPEN, §10 item 5).

**Verdict on acceptance.** For a loose ball, the decoded acceptance is: (a) a clip row admitted by the predicted ball position inside its windows, (b) the lowest priority byte wins, (c) the K=7 event fires only through the probe (§5.4). There is **no decoded height, speed or facing gate for a loose-ball touch** in the functions above. The receive-chain speed and turn rules (above) are the only decoded speed and facing inputs, and they apply to the receiver's first-touch kind, not to the chase.

### 5.6 What is not decoded (touch side)

- Probe writer `G+0x2A80` (§5.4).
- K=8 key builder `FUN_140A867A0`, `FUN_140A867D0`, `FUN_140A60520`, and `FUN_140A60320` (key compare semantics).
- `FUN_140A3B320` (roster index → id).
- Caller of `FUN_1404582C0`.
- Physical contact: `FUN_140843E60`, `FUN_1406B5C20`.
- Who chooses the chaser for a loose ball (OPEN, §10 item 1).

---

## 6. Why the GF designated man never touches a PES clip (CONFIRMED from code; behaviour INF)

Designated loose-ball branch: `elizacontroller.cpp:494-531`.

- `< 1 s` to the ball: if `!hasIt && ballSpeed ≤ kStalemateBallSpeed_ms`, push a sprint step onto the ball (`:507-521`). Otherwise `GetOnTheBallCommands` (`:522`). `hasIt = GetBallRetainer()==player || HasPossession()` (`:502`).
- `≥ 1 s`: sprint toward the ball if `> 0.5 m` (`:531-546`).

`HasPossession()` = `player.cpp:237` → `hasPossession` set at `player.cpp:404-416` = `AI_HasPossession(ball, player)` (except a shot/pass in flight).

`AI_HasPossession` (`AIsupport/AIfunctions.cpp:1119-1146`):
- ball prediction more than 5.0 m from the player → false;
- `|ball.z| > 0.5` → false;
- centre `pos + mov·0.05 + dir·0.1` more than 1.0 m from the ball's 2-D point → distance fails;
- `|ballVel_10ms − playerVel| > 6` → movement fails;
- true only when distance and movement both pass.

Touch path for a PES clip (`touchFrame == -1`):

1. `humanoid.cpp:526-531` (`TriggerControlledBallCollision`) fires only when (first alternative) `functionType == e_FunctionType_Movement`, `lastTouchBias ≤ 0.01`, `oppLastTouchBias ≤ 0.01`, **and** `ballDistanceNow < 0.6` **and** `ballDistanceFuture > 0.65` **and** `ballDistanceFuture > ballDistanceNow`; or (second alternative) `functionType == Trip`, `lastTouchBias ≤ 0.7`, `HasPossession`, and `ballDistanceNow < 0.4`. `ballDistanceFuture` is measured from `pos + 0.2·mov` to `Predict(200)`.
2. The first alternative needs the man to be moving **away** from the ball. An arriving man has `ballDistanceFuture < ballDistanceNow`, so the first alternative never fires on arrival.
3. The first alternative also needs `Movement`. `pes-port-DeadBall.md` (mechanism, measured) says PES locomotion clips are `e_FunctionType_Special` (clip 3343 under `SelectAnim`'s `Special` refusal at `humanoid.cpp:1972-1978`). So the first alternative is unreachable for PES locomotion clips at all. **CONFIRMED from code** (the Special classification is doc-measured).
4. `humanoid.cpp:589` (`touchFrame == frameNum`) needs `touchFrame ≠ −1`. PES clips have `touchFrame = −1`, so this branch never runs for them.
5. So on arrival, `hasIt` (AI_HasPossession true at ≤ 1 m) suppresses the step, `GetOnTheBallCommands` runs, and if it queues nothing playable the man stands. No clip with a hit frame is issued, so no contact is ever made. **INF (behaviour), consistent with the measured stall in `pes-port-DeadBall.md`.**

The disabled test `DISABLED_ALooseDeadBallInPlayIsReachedAndPlayed` (`tests/sim/pes_restart_delay_test.cpp:204-229`) measures only the longest loose episode in ticks. It does not assert that a touch occurs. A port needs a touch-occurred check as well (§7).

---

## 7. Port spec (what an implementer follows)

### 7.1 Predicate `ContestBallPredicate` (for the latch, step 4b only)

Input: `s` (player id), and a snapshot of: team record fields (possession byte, attack sign, registered-position array, leader id), ball `x`, `y` (height), `vel`, sample `S0.y` (predicted height, horizon 0), ball `2-D` point, on-pitch flags, newest event (kind, actor, frame, sub, flag), ranked head list and per-player lists (C1 `d²`, C2 heading, C3 pair values, arrival values), anime class byte and flags for `s`, per-player positions, and the match frame and `gs`.

Logic: §3.2, verbatim order. Returns bool.

Where GF has no source for an input, the stand-in is the nearest GF field, named `// ponytail:` with the PES VA (`brief-rules`). Gaps are in §8.

### 7.2 Latch step 4b (call site)

Inside the possession latch (§4.2 step 4b): only when carrier invalid, `frame − newest.frame ≥ gs/6`, cadence `(frame − newest.frame − 2) % 5 == 0`, newest kind ≠ 5 (and the kind-3 sub rule), and `MI+0x119C ≥ 3·gs` or `newest+0x2D == 0` or newest kind ∉ {3,4,5} (otherwise 3a). Call with `h = holder id`. `h ≥ 22` skips.

### 7.3 Touch record and carrier

- Touch event (K=7): when the contact probe (`G+0x2A80` equivalent) is live for a player and the newest event's actor is not that player (or the newest kind is not 15 with a different actor), push a touch record with the frame, team and player.
- Carrier (`MA+0x2601C` equivalent): the actor of the newest K=8 key, valid when `team < 2` and `idx < 40`; 0xFF otherwise. Recomputed each ball-action tick.
- Both need the event store with rings 0 and 1 (the `pes-re-foundation-matchball.md` ranked structure A). GF's 2-deep `ballEvents` with 4 kinds is not enough (§8).

### 7.4 First touch clip

PES first touch for an interceptor is TRAP anime 7, contact frame 18 (receive chain). The clip choice for the loose-ball touch uses the picker (§5.5) over the TRAP row. Port the picker's row acceptance and the lowest-priority rule. The receive-chain speed and turn rules (§5.5) gate the touch kind.

### 7.5 Tick order (decoded, INF on exact placement)

Event stage (detectors, K=7/8) → team tick (`FUN_1404F6C30`) → latch (`FUN_1404FE9C0`, §4.2 step 4b calls the predicate) → ball-action tick (`FUN_140457070`, recomputes carrier and designated owner). Placement of the ball-action tick is OPEN (`pes-re-possession-latch.md` §17 item 3 has the team-tick ordering question).

---

## 8. GF replacement map

| PES step | PES VA | GF today (file:line) | Replacement | Status |
|---|---|---|---|---|
| Chase/contest predicate | `0x1404F4BE0` | none | `ContestBallPredicate` in `pes::` namespace, called from the possession latch step 4b only | ABSENT; inputs listed |
| Latch (team possession) | `0x1404FE9C0` | `Team::HasPossession()` (`team.cpp:301`); `GetBestPossessionTeamID()` (`match.cpp:1130`) | the latch port, with step 4b calling the predicate | STAND-IN (`IsInPlay`) |
| Holder id | `MA+0x2483C` (`FUN_140A72A60`) | `Match::GetBallRetainer()` / `GetDesignatedPossessionPlayer()` (`match.hpp:195, 206`) | holder = designated man, until the PES holder writer is found | STAND-IN |
| Carrier (MA+0x2601C) | `0x140457070` | `SetBallRetainer` (`humanoid.cpp:1218, 1356, 1898`) | carrier = newest K=8 key actor (§7.3) | ABSENT (animation-driven in GF) |
| Touch record K=7 | `0x140A0DFD0` | `Team::SetLastTouchPlayer` (`team.cpp:365`) → `QueuePesTouch` (4 kinds) at `humanoid.cpp:571, 667, 743, 848, 1060, 1174, 1293, 1313, 1337` | touch from the contact probe (§5.4), 6+ kinds | PARTIAL (4 kinds, animation-driven) |
| Designated possession | `0x140457070` (+0x2600C) | `match.cpp:809-850` (time ratio 0.85 switch, stalemate pick) | ring-0 owner scan (§5.2) | STAND-IN |
| Designated loose-ball run | none decoded (OPEN) | `elizacontroller.cpp:494-546` (step/sprint to ball) | keep as stand-in, label `// ponytail:`; PES driver OPEN | STAND-IN |
| Touch possession gate | (none decoded) | `AI_HasPossession` (`AIfunctions.cpp:1119`); `player.cpp:411` | not a PES gate; drop from the touch decision, keep for the `HasPossession` read until replaced | STAND-IN (no PES VA) |
| First-touch clip | picker `0x14081FF70`, receive `0x1409F8A50` | `_TrapCommand` (`playercontroller.cpp:432`), called only from `elizacontroller.cpp:1017` (`kReceive`) | TRAP row from the picker; reached for the designated man too | ABSENT for designated men (INF: the `kReceive` block is in the defender branch; not verified) |
| Controlled collision | `touchFrame == −1` | `humanoid.cpp:526-600` (enabled, `:141`) | unreachable for PES locomotion (§6); replace with PES contact | WRONG PATH for PES clips |
| Touch frame | clip hit record | `humanoid.cpp:589-600`, `GetTouchPos` | keep for `touchFrame ≠ −1` clips | FAITHFUL for GF clips |
| Anim select | `0x1409F8220`, `SelectAnim` | `humanoid.cpp:1930-1985` (Special refusal `:1972-1978`) | the requeue gate is the stall cause (`pes-port-DeadBall.md`); out of scope here | DOCUMENTED |
| Ball height / sample | `FUN_140A57800`, `BI+4` | `Ball::Predict(0).coords[2]` (`humanoid.cpp:~536`) | height from `Predict(0)` (`coords[2]`) | STAND-IN (z is up in GF) |
| Registered position | `T+0x4C00[slot]` | `formation role` (`GetFormationEntry().role`) | role index 0..12 map | STAND-IN (doc says mapping, not PES value) |
| Frame / gs | `MI+0x1198`, `gs` | `actualTime_ms`, `kTick_ms` | frame = tick, gs = 54 | FAITHFUL (54 fps) |
| Newest event | `FUN_140A3E190(E,0)` | `pes::NewestEvent(GetPesEvents())` (`team.cpp:378`) | the 2-deep GF store is too small (rings 0/1 need 8 deep) | PARTIAL |

---

## 9. Corrections to the brief and to the docs

1. **Brief premise.** `FUN_1404F4BE0` is not called per player. Its only caller is the possession latch, step 4b. The loose-ball chaser must be found elsewhere (§10 item 1).
2. **`T+0x4C00[slot]`** is a registered-position index (0..12), not a countdown. `pes-re-foundation-teamplayer.md:98`; the doc `pes-re-run-targets-b2.md:9` agrees.
3. **Carrier writer.** `0x14045725A` is inside `FUN_140457070`, not a NOFUNC interior store (`pes-re-foundation-matchball.md` B4/E5b). The carrier is from the ring-1 newest key, not from a direct write of the holder.
4. **`pes-re-possession-latch.md` §18** calls `FUN_1404F4BE0` the "chase predicate"; in light of §4.1, rename to `ContestHolderPredicate` or similar.
5. **`FUN_1404F4BE0`'s `budget`** (`gs` vs `gs/2` by zone, `+2·gs` for registered position 0) is listed in `pes-re-possession-latch.md` §18 as "zone 1 gs/2, zone 2 gs, (4) +2·gs"; the decode above confirms it, with one addition: the `S0.y > 1.5` override replaces the budget with `gs` before the `+2·gs` step, and the flag `F` halves the result (`>> 1`).
6. **The "no PES touch-reach constant" note in `pes-decode-TouchReach.md` §1** stands; §5.5 confirms the picker has no edge test.

---

## 10. Open items (each with the method that closes it)

1. **Who runs at a loose ball.** The designated loose-ball run is not decoded. Method: find the caller that issues a movement toward the ball for a non-latch man. Candidates: the press writer `FUN_140536FC0` (loose ball within 13 m, `pes-re-press-layer-port.md:10`), reaction entry 3 `FUN_1409F8220` (receive, interception rule `FUN_1409FA0B0`).
2. **Probe `G+0x2A80`** (writer of `+0x18` and `+0x64`). Method: E8/absolute scan for the probe's base offset, then decompile each writer.
3. **Units**: the arrival values (`FUN_14044E020`) and the C3 pair (`FUN_1404C8EA0`). Method: read one live value against `GetTimeNeededToGetToBall_ms` and the distance of the same pair.
4. **Caller of `FUN_1404582C0`** (the ball-action tick). Method: E8 and vtable scan for the `0x1404582C0` address, then decompile the owner.
5. **Physical contact**: `FUN_140843E60` and `FUN_1406B5C20`, and the hit-frame velocity transfer. Method: decompile the callers of `FUN_140843E60` and check which of them feeds `ball->Touch`.
6. **Switch tables.** Only direct `E8`, `E9`, absolute and RIP-relative references were scanned. A computed-jump table (32-bit relative entries) or a call through a runtime pointer would not show. Denuvo VM stubs (`0x140A85F40` region) would hide a caller. Method: scan the image for 32-bit values equal to `0x1404F4BE0 − base` for each jump-table base, and check the VM bytecode sites.
7. **Bit names** of `human+8` bits 22 and 9 and the class predicates `FUN_140A6E5E0` / `FUN_140A6E340` names (only the table bits are CONFIRMED).
8. **`FUN_140A6CF70`** body (the ball sample `S0` and its `+4` = height, INF high).
9. **`BI+4`** (INF high = height, from F7). Method: compare with `Ball::GetPosition().coords[2]` for one live frame.

---

## 11. Session notes (evidence trail)

- Scratch files: `d_1404f4be0.c`, `a_1404f4be0.s` (the asm read for §3), `d_1404fe9c0.c`, `d_140a0dfd0.c`, `d_140457070.c`, `a_140457070.s`, `d_14081ff70.c`, and the helper decompiles listed in §3.4.
- Scans: the `E8` scan of `pes_full.bin` (2.2 s) and of `PES2021.exe` agree on the single caller `0x1404FED44`.
- GF files read (no edits): `AIsupport/AIfunctions.cpp:1119-1146`, `player/controller/elizacontroller.cpp:460-546, 1005-1020`, `player/humanoid/humanoid.cpp:500-600, 689-700, 1205-1222, 1350-1358, 1890-1900, 1930-1985, 3071-3393`, `player/player.cpp:237, 404-416`, `team.cpp:25-45, 355-385`, `match.cpp:809-850`, `playercontroller.cpp:432`, `tests/sim/pes_restart_delay_test.cpp:204-229`.
