# PES team tick, MA nearest lists, and the team/player interface (A4TeamTick)

> **Main correction 19-10-26 (FirstTouchPort2 raw decode, audited):** the "per-player think every 25 frames" reading of §5.1 is WRONG. The driver call `0x140925769` (`call 0x140925470`) is unconditional; the `counter % 25 == index` flag (`cmp r8d,[rbx+0x18]; sete r8b` at `0x14092578B/8F`) only goes to `0x1408BFCF0` (`0x140925793`), which never reads r8. The decoded per-frame cadence is instead the slide predicate's odd-frame gate in `FUN_140971BA0` (`mov eax,[rbx+0x1198]; and eax,1; test al,al; je 0x140971C9D` at `0x140971CB6..CC1`, `0x140971C9D` = `xor al,al`).

Tags: **CONFIRMED** = decompile (Ghidra MCP), raw disassembly of the running `PES2021.exe`, or live read (`/proc/<pid>/mem`, pread only, pid 2510857). **INF(h/m/l)** = inference. **OPEN** = not settled; the blocker is named.

Scratch (read-only, no writes to PES): `<workdrive>/gf-A4TeamTick-scratch/` (`probe_ma.py`, `probe2.py` (live), `store_scan.py`/`store_scan.txt`, `g_scan.py`, `player_loop_scan.py`/`player_loop_hits.txt`, `player_table_scan.py`, `binder_stores.txt`). The 120 M-line `pes_text.asm` (objdump of the running exe) was deleted after use.
Builds on: `pes-re-teamctx.md` (rev2), `pes-re-gaps-a2.md`, `pes-re-off-ball.md`, `pes-re-team-plans.md`, `pes-re-possession-latch.md`, `pes-re-counter-trigger.md`.
Pins: none. Live reads only. MI+0x1168/0x116c/0x1574 not touched.

---

## 0. Verdicts (after round 2)

| # | Question | Verdict |
|---|---|---|
| 1 | Relative order of team tick and per-player chain in one logic frame | **Team tick order inside its caller H: CONFIRMED** (§2.1). Team tick runs **before** H's second group (`FUN_1404405d0`). The per-player chain's caller (the `match::Player` vtable dispatch) is **not found**, so its position relative to H is **OPEN** (§5.3). |
| 2 | `G+0x180` / MI binder | **Partly closed.** MI must be non-null when H is entered (CONFIRMED: H reads `[G+0x270]` with no null test). MI is null in ~91% of live samples, so it is cleared again later in the frame (CONFIRMED live). The clearing or setting site is **OPEN**. The two G-table writers found (`FUN_140a48210` unbind, `0x140a49c3b` bind) are **match teardown and match setup, not per tick** (§4.1). |
| 3 | `MA+0x22e4c` writer | **OPEN.** Layout, sort key, rank and count CONFIRMED live (§3). No writer found; the 200-stride consumers are listed (§3.5). |
| 4 | `T+0x78` at `0x14093a089` | **Refuted as a T+0x78 writer** (INF h): the containing function `FUN_140939cc0` has no decompiled store to `T+0x78`; the `0x78(%rbp)` store is to its own frame. T+0x78 writer **OPEN**. |

Also corrected: the `match::Player` vtable chain top (§5.1), and the H routine's team-tick loop (§2.1).

---

## 1. Method and live checks

- Ghidra MCP via the `xd://mcp__ghidra_*` routes (the `curl` bridge on :8080 returns 404 for xrefs).
- Live probes (LivePoke `Mem`, pread only, `sudo -n`): `probe_ma.py` (51 valid samples), `probe2.py` (35 valid). "Valid" = MI non-null. MI is non-null in ~9% of samples (35/400).
- Live vtable read: `0x1426329a8` is the vtable of `match::Player` (RTTI name `.?AVPlayer@match@@`, CONFIRMED from the COL at `0x142ca8538`).

Accessors (CONFIRMED, decompile): team record `T_s = *(G+0x1928+0x28·s)` (`FUN_140a48a30`); player `pl(i) = *(G+0x1ef8+0x28·i)` for `i<26`, else `*(G+0x1dd8)` (`FUN_140a48a10`); carrier `FUN_140a627e0(X,0) = *(*(X+0x18)+0x2601c)` (teamctx §1).

---

## 2. Team tick `FUN_1404f6c30` (CONFIRMED)

### 2.1 Its caller H: the per-frame order (CONFIRMED, raw `0x14041ecf0`–`0x14041ef25`)

H = the team-tick owner, vtable `0x142597580`, slot 2 = `0x14041ecf0`. Its body, in order:

| Step | VA | Action |
|---|---|---|
| H.a | `0x14041ecf8`–`ed02` | `S = H+0x488`; call `FUN_140a49c50(S)`: per-match init of a 40-entry flag block, guarded by `S+0x4b0` (CONFIRMED decompile). |
| H.b | `0x14041ed07`–`ed1c` | `MI = *(G+0x270)`; **`if MI+0x1168 == 0xa: return`** (whole frame skipped). MI is read with no null test, so MI is non-null here. |
| H.c | `0x14041ed22`–`ed76` | Per-side init: `T_s` = team record (`+0xc148` of the side's owner), stored at `H+0x970+0x28·s`. |
| H.d | `0x14041edc0`–`edd7` | **Team tick loop**: for `s = 0, 1`: brain = `H[0x450+8s]`; if non-null, `FUN_1404f6c30(brain, S, T_s)`. |
| H.e | `0x14041ee57`–`ee62` | If `H[0x460] != 0`: `FUN_1404405d0(H[0x460], S, {T_0,T_1}, H+0x940-side+0x1150)`: the **side planner / ball-phase writer** (CONFIRMED decompile). It reads `T+0x78` of both sides into the match array (`param_1+0x10..`), writes per-slot values into `T+0x3c…`, and calls the phase helpers `FUN_140a571b0/571e0/54fe0/55000`. Then `vt+0x88` callbacks on the `H+0x940` and `H+0x988` sub-objects (targets not decoded). |
| H.f | `0x14041ef06`–`ef25` | `FUN_1405069a0(H+0x18)`, then tail-jump `FUN_140a484f0(S)`: teardown of `S`'s pointer slots when `S+0x4b0` is set (CONFIRMED decompile). Together with H.a this brackets `S` once per frame (INF h). |

**Order in one frame, as far as H goes (CONFIRMED):** `S-init → (MI check) → T_s bind → team tick side 0 → team tick side 1 → side planner (FUN_1404405d0) → callbacks → S-teardown`.

### 2.2 Team tick steps (CONFIRMED order; `d_1404f6c30.c` lines)

| # | Lines | Step | Direct writes in tick body | Purpose / doc |
|---|---|---|---|---|
| 0 | 155–156 | `brain+0x6d48 = match`, `brain+0x6d50 = T` | brain fields | per-tick context; **`brain+0x6d50` is cleared at step 48**, so it is a live "inside the tick" flag for that side (INF h) |
| 1–4 | 163–176 | `FUN_141e5d4c0`, `FUN_1423e7fd0` (sets skip), `FUN_140a0c3e0`, `FUN_14051c840` | — | not decoded |
| 5 | 178–179 | `FUN_140501260`; **`T+0x8c = 1`** | T+0x8c | per-tick flag |
| 6–8 | 181–190 | `FUN_1404f6720`, `FUN_1404f7ad0`, `FUN_140a42580(T, MI+0x1198)`, `FUN_1405008e0`, `FUN_140502190` | — | not decoded |
| 9 | 192–196 | **`FUN_1404fe9c0` = possession latch** | **T+0x255** (via `FUN_140a428d0`) | `pes-re-possession-latch.md` |
| 10–11 | 198–201 | `FUN_1404fedc0`; **`T+0x134 = brain+0xbcc`** | T+0x134 | T-LVL |
| 12 | 203 | **`FUN_1404f8350`** | b8a4, b8c0 (via) | attack level |
| 13–14 | 204–214 | `FUN_1404f6680` (cond.), `FUN_1404fa840`, `FUN_1404fa720`, `FUN_1404fbca0` | — | not decoded |
| 15 | 215–222 | **`T+0xb8a2 = (b8a4 > 3)`, `T+0xb8a3 = (b8a4 < 1)`** | T+0xb8a2/a3 | full-attack/defence flags |
| 16–19 | 224–235 | `FUN_140500200` (sole caller `0x1404f6e62`), `FUN_1404ff8b0`, `FUN_1404f7ba0`, `FUN_1404fd4b0`, `FUN_14053b860`, `FUN_1404f87d0` | — | not decoded |
| 20–22 | 237–270 | `FUN_140500b90` → `T+0x255` read; restart branch (`MI+0x1775 == 0x17`) or else: `FUN_140581980` (counter, T+0xa508 via), `FUN_14057ac20` (stoppage pair, T+0xb6df/b6e0 via) | — | counter-trigger doc |
| 23 | 260–267 | `T+0x80 = 0xff`; `T+0x6e6 = 0` (cond.) | T+0x80, T+0x6e6 | — |
| 24–27 | 269–279 | `FUN_140537ac0`; (skip==0) 11× `FUN_140a40240`; stack snapshot of `T+0xa2a0`, `T+0x28c…`, `T+0x474…`; `FUN_140a84e30` | locals | — |
| 28–29 | 473–476 | **`FUN_140579840`** (concept selector) and `FUN_140579bf0` (executor) | T+0xb6e8 running flags (via) | `pes-re-tactics.md` §2.1 |
| 30–32 | 478–484 | `FUN_1404f61e0`, `FUN_14057d240`, `FUN_1404f9dc0` | — | not decoded |
| 33 | 485–537 | 11-slot loop: designated-supporter reset via `FUN_140a429a0(T, s, 1, rec)` | **T+0xa2a0[s] = 1** (via) | `pes-re-off-ball.md` §3.1 item 6 |
| 34–35 | 538–559 | `FUN_140501160` (cond.), `FUN_14057bbb0`, `FUN_14053b110`, `FUN_1404fb2a0` (→ `uVar5`), `FUN_14057b6b0`, `FUN_14057c190`, `FUN_1404f8c70`, `FUN_140500a00`, `FUN_1404fad30` | — | not decoded |
| 36 | 561–571 | (skip==0, `MI+0x1198 & 1 == side`) **`FUN_140525530`** (run manager); **`FUN_140538050`** (see §2.3) | run states (via) | `pes-re-off-ball.md` §1.2 |
| 37 | 572–580 | (`T+0x218 == 0`) 11× `FUN_140a40820(T,s)` | run states released | — |
| 39 | 588–591 | **`FUN_140535c60` = MA build** → `FUN_140536fc0` | MA list (§3), `T+0xa548/a55c` (via) | `pes-re-gaps-a2.md` a1 |
| 40–42 | 593–610 | `FUN_140ca1a50`, `FUN_1404f88d0` (cond.), `FUN_1404fc720` (set-piece/restart events per side; CONFIRMED decompile, its `vt+0x10` is on the team object, not a player), **`T+0xb6de = FUN_140509170(side)`**, `FUN_1404fc4d0`, `FUN_14057ba50`, `FUN_14057d240`, `FUN_140580630` | T+0xb6de | stoppage (F17) |
| 43–47 | 611–642 | `FUN_140539ed0` (cond.), `FUN_140421f40` ×2 (return-0 stubs), `FUN_140520930` (takers, subs), `FUN_14053c6e0`, `FUN_140a408c0`, `FUN_140a42a70` | — | `pes-re-taker-map.md` |
| 48 | 643 | **`brain+0x6d50 = 0`** | brain field | end of tick |

**Direct writes in the tick body (CONFIRMED):** `brain+0x6d48/6d50`, `T+0x8c`, `T+0x134`, `T+0xb8a2/a3`, `T+0x80`, `T+0x6e6`, `T+0xb6de`. `T+0xa2a0[s]` is written only through callees. **No store to `T+0x78`, `G+0x180`, `X+0x18`, or `MA+0x22e4c`** in the body.

### 2.3 Step 36 callee `FUN_140538050` (CONFIRMED decompile)

A per-team dispatcher on `brain+0xb8b4` (mode 1..6; only when `T+0x255 != 0`, i.e. the team has possession). Mode 4 (`0x140538550` …): finds a receiver for the pass by scanning the 11 slots in `MA`-order, computes `d²` from `pl+0x55c/0x564` (the same positions as the list), and for up to 3 candidates calls `FUN_140537c60(…)`, then `FUN_140a429a0(T, slot, 0x1e, …)` (**SPACE_RUN_CHANCE 0x1e** into `T+0xa2a0`) and `FUN_140a42a40`. It is a **consumer** of positions, not the list writer, and it writes run states, which is how mode-4 runs are seeded.

---

## 3. The per-player nearest list `MA + 0x22e4c + 200·i`

### 3.1 Layout (CONFIRMED live, 51 + 35 valid samples)

Owner `i` (0..21), base `B_i = MA + 0x22e4c + 200·i`:

| Offset | Field | Note |
|---|---|---|
| `+0x00 + 8k`, k=0..10 | `{u32 id, f32 d²}` team 0 | ascending `d²`; `id = 0xff, d² = 0.0` in empty slots |
| `+0x58 + 8k`, k=0..10 | `{u32 id, f32 d²}` team 1 | same |
| `+0xb0` / `+0xb1` | u8 count team 0 / team 1 | valid entries (0..11) |
| `+0xb2 + id`, id=0..21 | u8 rank | index of `id` in its own team's list; **10 = absent** |

### 3.2 Sort key and contents (CONFIRMED live)

- Key `d² = (x_j−x_i)² + (z_j−z_i)²` (xz plane), `x = pl+0x55c`, `z = pl+0x564`. The stored `d²` matches the recomputed value in the same sample for every pair outside a rebuild.
- Ascending within each team (owner 0 team 0: 1845.2, 1902.1, 2153.5, …). Self excluded (owner 0: `rank[0] = 10`). Off-pitch players absent (owner 5 lacks id 2; owner 13 lacks id 12): cause not established (INF h).
- Rank identity (CONFIRMED): the k-th entry has `rank[id] = k`.

### 3.3 Cadence (INF h)

Mismatches between stored `d²` and positions appear only in samples that straddle a rebuild, so the list is rebuilt once per frame inside the team tick's window, then positions move.

### 3.4 Writer and consumers: OPEN

Excluded as writers (decompile/raw, reads only): `FUN_1405346e0`, `FUN_140531490`, `FUN_140531260`, `FUN_140531810`, `FUN_1405325e0`, `FUN_140535c60`, `FUN_140536fc0`, `FUN_14052f4f0`, `FUN_14052f720`, `FUN_140532f70`, `FUN_140530320` (stack copy), `FUN_1406c0170`, `FUN_1406cbf70`, `FUN_1405b89f0`, `FUN_140587100` (candidate test; its `0x1405878c5` store is a read-side false positive), `FUN_140538050` (§2.3), `FUN_140a79b20`, `FUN_14045e030`.

Static search (CONFIRMED, `store_scan.txt`): no direct store at `0x22e4c`; 320 base-forming `add`/`lea` sites; 288 have no store through the base within 160 instructions; the 32 that do are record block copies, the reaction-record area, or unidentified `mov [rcx],eax` sites at `0x140835106` and `0x14083562f`.

Consumers (all read `+0x22e4c + 200·i`): `FUN_140530320`, `FUN_140532f70`, `FUN_1406c0170` (jostle), `FUN_1406cbf70` (ramp), `FUN_140587100`, `FUN_1405b89f0`, `FUN_1405e6e80`.

Next step (writer): the builder must write `{id, d²}` for all 22 owners in one pass with a 200-stride. Find it as a function whose loop over `i = 0..21` stores into `MA+0x22e4c+200·i`. The static search missed it because the builder uses a computed base (`imul 0xc8`, which appears 401 times), not a literal displacement. Decompile the functions in the MA-chain body that are not yet named: `FUN_140534a20`, `FUN_140536210`, `FUN_140536690`, `FUN_140634470`, `FUN_140635ab0`, `FUN_140628400`.

---

## 4. Writers of `G+0x180`, `T+0x78`, `X+0x18`

### 4.1 `G+0x180` (roster) and MI (`G+0x270`): binder OPEN, partly closed

CONFIRMED:
- H reads `[G+0x270]` (MI) at entry with no null test (`0x14041ed07`–`ed0e`). So MI is valid when H runs.
- Live: MI is null in ~91% of samples, and the roster (`G+0x180`) is zero exactly when MI is null (35/35 valid samples had `0x1e002354`; 12 printed null samples had 0). The team and player tables flip together.
- Teardown: `FUN_140a48210` (CONFIRMED decompile) clears `G+0x270`, `+0x180`, `+0x40`, `+0x2aa8`, `+0x310`, and others when `G+0x2e50` is set, then clears `G+0x2e50`. Its only caller is `FUN_140a45780`, which is the **match-object destructor** (CONFIRMED decompile). So it is **not** the per-tick clear.
- Setup: the only store of `1` to `G+0x2e50` is `0x140a49c3b` (`mov BYTE PTR [r13+0x2e50],0x1`), the end of a match-setup function that initialises 0x28-stride records (CONFIRMED raw). Also a match-setup function, not per tick.

INF h: the per-tick set/clear pair for MI and the roster is elsewhere, in the code that calls H (the listener path). Next step: find the function that writes `[G+0x270]` with a register value obtained from the listener's record, and the function that writes zero to it after H returns. My qword-store scan (`binder_stores.txt`, 2642 hits) was not filtered by G-base; the filter needs G tracked through callee-saved registers.

### 4.2 `T+0x78` (designated pass target): writer OPEN

- Not written by the team tick body (CONFIRMED). Read by the side planner `FUN_1404405d0` (CONFIRMED) and by `FUN_140536fc0`, `FUN_1406cbf70`.
- Live: `T_s+0x78` (u32) changes during play (side 0: 255, 7, 5, 4, 0; side 1: 255, 16, 18). 255 = none.
- Candidate `0x14093a089` in `FUN_140939cc0` is **refuted** (INF h): the decompile of `FUN_140939cc0` (the receiver/position-hold function, reads `brain+0x59c…`, `param_1+0x5b8/0x5dc`) has no store to `T+0x78`; the `0x78(%rbp)` store is to the frame. Next step: a live write-watch on `T+0x78` at a kick (read-only probe already reads it), or find the kick code that writes the pass target id.

### 4.3 `X+0x18` (MA pointer on the player context): setter OPEN

- teamctx §1's `X = *(P+0x3ac0)` is **refuted** for `P = pl(i)`: `*(pl+0x3ac0)` is garbage (`0xc19c345841665884`) and no pointer under `pl+0x0…0x8000` has `+0x18 == MA`.
- Round 2: the per-player object is `match::Player` (RTTI CONFIRMED). Its vtable slot 2 (`0x1426329b8` → `0x140925750`) is the per-player tick. That wrapper uses `obj+0x18` as the player index (`cmp [rbx+0x18]` against `counter % 25`, CONFIRMED raw), so `X` is not at `P+0x3ac0` for this object. The `X` context must be found on `match::Player` at another offset. Next step: a live scan of `match::Player` for a pointer whose `+0x18` equals MA, using the vtable base `0x1426329a8`'s objects as the anchor.

---

## 5. Team tick ↔ per-player chain

### 5.1 Chain (CONFIRMED, round 2 correction)

```
match::Player vtable slot 2 (0x140925750; caller = vtable dispatch, NOT FOUND)
   wrapper: counter = obj+0x10 (inc); gate = (counter % 25 == obj+0x18)   [CONFIRMED raw 0x140925750–0x1409257a6]
   → FUN_140925470  (player driver; rbx = player object; [rbx+0x18] = player index)   [CONFIRMED raw]
       → FUN_140928fc0 (AI decision entry; call at 0x140925724)                     [CONFIRMED]
           → FUN_1409257b0 (0x1409294e4)                                            [CONFIRMED xref]
               → FUN_140929a00 (0x140926580; gate MI+0x11b0 == 0x16; writes AI+0x8d4 request code 0x13..0x1d)   [CONFIRMED]
                   → FUN_14092a340 (0x140929aa9 / 0x14092a7b9)                       [CONFIRMED xref]
                       → FUN_14092c9f0 (per-player action decision; reads T+0xa2a0[slot]; returns action id)   [CONFIRMED]
                           → FUN_1421019d0 (BallPlayer update; xref 0x14092ce53)
                               → BallPlayer::MainEval 0x142110d30 (0x142101aa7)    [CONFIRMED xref]
```

Round-1's "unnamed routine at 0x140925769" was the **wrapper's caller inside FUN_140925470's neighbourhood**; the corrected top is the `match::Player` vtable slot 2.

The team tick `FUN_1404f6c30` is **not** on this chain (no call into it from the body, §2.2).

### 5.2 Handoff fields

Team → player: `T+0xa2a0[s]` (run/concept state; written by step 33, step 36 (`FUN_140525530`, `FUN_140538050`), `FUN_140536fc0`), read by `FUN_14092c9f0`. `T+0x28c+0x40s` and `T+0x278` (sub-kind and queued state) written by `FUN_140a429a0`/`FUN_140a42b80`, read by `FUN_14092c9f0` and `FUN_1406cbf70`. `T+0x26c+0x40s` (targets), `T+0x78` (pass target; writer OPEN), `T+0x255` (latch, step 9; read by `FUN_14092c9f0` and `FUN_140929a00`), `T+0x254` (attack sign, read by `FUN_140929a00`).

Player → team: `pl+0x5d0` (AI action; COVER 0x32, MATCH_UP 0x3a; read by steps 33, `FUN_1406cbf70`, `FUN_140536fc0`); `AI+0x8d4` request code (written by `FUN_140929a00`; consumer not decoded); `P+0xc6bc` stamp = `MI+0x1198` (written by `FUN_14092c9f0`).

### 5.3 Relative order of the two ticks: OPEN (narrowed)

CONFIRMED: inside H, the team tick (H.d) and the side planner (H.e) run **before** H.f. The per-player chain's entry, the `match::Player` vtable dispatch, is **not inside H**: H contains no `call [reg+0x10]` on a `match::Player` (checked: H's only vtable calls are on `H+0x968`, `H+0x940`, `H+0x988`, all non-Player). So the per-player tick is called from outside H, in the same match tick (the caller of H is the listener path, `FUN_1404334e0 → FUN_140a8eff0 → H->vt+0x10`, per `pes-re-gaps-a2.md`).

Two facts constrain it:
- `FUN_140929a00` gates on `MI+0x11b0 == 0x16` and `FUN_140925470` reads `T+0x254`/`T+0xa2a0`, which the team tick writes in the same frame.
- A `match::Player` ctor (`FUN_14091fff0`, `FUN_14091e9f0`) installs the vtable; the caller of slot 2 is a loop over the 22 players.

INF(m): the per-player loop runs after H (it reads the latch and run states H wrote). Not confirmed. Next step: find the loop that calls `[player_vt+0x10]` for the 22 players. The `G+0x1ef8` stride-0x28 table is read through `FUN_140a48a10` (scanned: 6 sites, none on `match::Player`) and by a computed index (no literal-`0x1ef8` load hits). So the loop is either inline with `imul 0x28`/`lea` on the table, or calls the player object through a list pointer.

---

## 6. GF map and port order

GF anchors (this checkout): `pes/movestep.hpp:495–520` `TeamAi` (the team reads of `AccelerationRamp`, matches PES); `pes/movestep_ramps.cpp:215` `AccelerationRamp` (gate `:228`); `pes/movestep_run.cpp:1793–1862` RUN_LOOP jostle override and `RunLoop`; `movestep_style.cpp:65` `LocomotionStyle`; `sim/data/matchdata.hpp:337` `AddTeamTick` (a counter). The team tick, the MA list builder, the per-player driver and the H frame owner have **no GF counterpart** in this grep; GF's `OppByDist` stands in for the list (teamctx).

Port order, each gated by its open item:
1. **MA list layout** (§3.1), rebuilt once per frame from `pl+0x55c/0x564`, with on-pitch and tie rules marked `ponytail:` until the builder is decoded.
2. **H frame order** (§2.1): S-init → MI check (`MI+0x1168 == 10` skip) → per-side T bind → team tick ×2 → side planner → callbacks → S-teardown. This order is CONFIRMED and can be ported now.
3. **Team tick skeleton** (§2.2): 48 steps, fixed order, callees not decoded as named stubs with their VAs.
4. **Per-player driver** (§5.1): wrapper gate `counter % 25 == index` → `FUN_140925470` → … → MainEval handoff (§5.2). The frame position of the Player loop (§5.3) is needed before this can be wired.
5. **`X+0x18`, `T+0x78`, `G+0x180` / MI binder**: after §4's items.

---

## 7. Open items, ranked

1. **Caller of `match::Player` slot 2** (§5.3): the loop that gives the team-tick-vs-player order. Find the inline loop over the 22 players that calls `[vt+0x10]`.
2. **Per-tick MI/roster binder** (§4.1): the set and clear sites on `G+0x270`/`+0x180` per frame (not the setup at `0x140a49c3b` or the teardown `FUN_140a48210`).
3. **MA list builder** (§3.4): the 200-stride loop over 22 owners writing `{id, d²}`.
4. **`T+0x78` writer** (§4.2): a live write-watch at a kick, or the kick code that writes the pass target.
5. **`X+0x18` context on `match::Player`** (§4.3): the offset on the player object that holds MA.
6. **Off-pitch rule for the list** (§3.2).

---

## 8. Scope notes

- No GF code changed. No PES write, pin, or input. Live reads only.
- Round 2 budget used: about 200 requests in total for the ticket. The static scans were run once each; the three read-only probes were run in parallel with the decompiles.
- Scratch kept: `<workdrive>/gf-A4TeamTick-scratch/` (scanners and hit files). The 120 M-line `pes_text.asm` is deleted.
