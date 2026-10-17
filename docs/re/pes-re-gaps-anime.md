# PES 2021 gap report: anime and locomotion (GapsAnime)

Decode only. No code in `src/`, no commits, no game writes. Labels: **OBS** = observed in a decompile, the install's PE bytes, or a live read (VA given); **INF** = inferred. Scratch: `the scratch dir/` (`dec/` decompiles, `pe_const.py` PE constant reader). Tools: Ghidra bridge (PES21 project), `dec.sh` (PESLive, JDK on PATH, no ulimit), read-only `/proc/2510857/mem` via `LivePoke/common.py` under `sudo -n`, `xd://mcp__ghidra_get_xrefs_to`, install PE bytes `the PES21 install/PES2021.exe`.

## Verdicts

| Gap | Verdict | Unblocks (foundation ids) |
|---|---|---|
| 1. Group A helpers | **decoded**: 11 of 12 fully; `FUN_1406d1390` partial | L1 (MOVESTEP group A), A-MOV |
| 2. Motion HitData/CancelData record | **partial**: per-motion table layout and all readers decoded; `+0xc8`/`+0x208`/`+0x110` field meanings not pinned; fill writer not traced | A-MOV (timing reads); K-ERR (kick timing, INF) |
| 3. D1 vtable at P+0xac0 | **resolved**. The handover's slot mapping was off by 0x20 | D1 / C22 (dodge speed term), A-MOV, P-KIN |
| 4. Owner of P+0x245c | **partial**: chain to the factory's caller decoded; live check inconclusive | A-MOV, P-KIN, D1 |
| 5. L4 (P+0x428) | **partial, high**: dispatcher, approach, strike, solver, and adjusters decoded. Per-tick consumer and the gate-bit writer open | L4 (A-MOV, P-KIN, K-ERR) |

## Corrections to the handover (OBS)

1. **The dodge's speed slot is `vt+0x20` of the table whose vptr is `0x1425e35b0`, which is `FUN_1406ea110`.** `FUN_1406eab00` is at `vt+0x40`, not `+0x20`. The handover took the body at `0x1425e35d0` as the vptr; the vptr is `0x1425e35b0`.
2. **`DAT_14351ae54` (returned by `FUN_141e532d0`) = 54.0 live** (one read). It is the sim's tick rate in Hz.
3. **The approach `FUN_1407344f0` runs the solver and `FUN_141eb3910` only when bit `0x40000000` of `P+0x2460` is set.** Otherwise it issues the clip and sets phase 1.
4. **`FUN_140a48a10(G, slot)` = `*(G + 0x28·(slot+0xbf))`** for `slot < 0x1a`, else `*(G+0x1dd8)`.

---

## Gap 1: MOVESTEP group A helpers

Constants below were read from the install by VA (OBS). `G = *(0x1436F3DC8)` (match context); `P` = AnimePlayer object (gap 4); `MI = *(G+0x270)`.

**Shared accessors (OBS):**
- `FUN_1406b0ed0()` returns the motion-table singleton `DAT_1436f23c0`, allocated by `FUN_1403fb410(…, 0xd8)` and initialised by `FUN_1406aecf0`. `+0` is the per-motion array base; `+8` is the default record (used for motion ids ≥ 0x1125). Every motion reader below takes this object as its first argument.
- Per-motion record: `base + motion·0x390`, valid for `motion < 0x1125`.
- `FUN_141e532d0()` returns `DAT_14351ae54` (54.0 live). `FUN_1415a1780` = `sqrtf` through the hook `*(0x1425301f0)` (matches LivePoke).
- `FUN_1406e6bf0(P, n)` = `(RNG slot 2 output) mod n`. The RNG is `P+0x32d0`, advanced by `FUN_140a92470(P+0x32d0, 2)`: LCG `s = (s·(-0x2d22b2e3) + 0x4513) & 0x7fffffff`, output `int(s · DAT_1426550d8 · tbl[2])`, with `tbl = DAT_1426548b0`. This is the PES generator (`pes::PlayerSeed`); INF that the table entry is the seed multiplier.

**Motion-timing readers (all on `FUN_1406b0ed0()`, OBS):**

| Fn | Reads from record | Returns |
|---|---|---|
| `FUN_1406af620(tbl, m, rate)` | `+0` byte (valid), `+0x18` pointer, u32 at `+4` bits 10..19 | `int(2v·(runtime/60)/rate + 0.5)`; `rate` ≤ 0 → 1.0 (INF: frames → ticks) |
| `FUN_1406af8e0(tbl, m, rate, k)` | `+5` count, `+0xa0` list of 8-byte pointers, `list[k]` u32 bits 13..22 | same formula over that `v` |
| `FUN_1406afbc0(tbl, m, off)` | byte at `rec+off` (default record if `m ≥ 0x1125`) | byte |
| `FUN_1406b1f00(tbl, m, rate, k)` | `+6` count, `+0xc8` list, `list[k]` u32 word 1 bits 10..19, minus 0x205 (517) | `int(2(v−517)/rate·(runtime/60) + 0.5)` (signed) |
| `FUN_1406b1d00(tbl, m, rate, k)` | same list, word 1 bits 0..9 | `int(2(v+1)/rate·(runtime/60) + 0.5)` |
| `FUN_1406b1c70(tbl, m, flag, k)` | same list, word 0 bits 13..23 (`&0x7ff`), minus 0x36e (878) | `(v−878) · (flag ? DAT_142588218 : DAT_14256ae1c)` = `(v−878)·(−1.0 or 1.0)` |
| `FUN_1406b1930(tbl, m, k)` | `+0xc` count, `+0x208` list, `list[k]` u32 bit 29 | 0 or 1; returns 0 at once for `m` in 0x834..0x852 when `(0x1804001 >> (m−0x834)) & 1` (OBS) |

**Helpers (OBS unless marked):**

- **`FUN_1406b93a0` (IdleRow).** `(P, int* req)`, returns a motion id. Case on `*req`:
  - `2`: if `FUN_140a638c0` is false, table `DAT_1425ddbf0` (n=4) via `FUN_1406e6bf0`; otherwise falls to case 5;
  - `5` → `0x5b1`; `4` → `0x5b0`; `7` → `0x5b2`;
  - `3`: uses `FUN_140a627e0`, `FUN_140a581c0`, `FUN_140a58550`, `FUN_1406ebf30` and tables `DAT_1425ddcc8`/`DAT_1425ddcd0` (n=2), `DAT_1425ddc00`/`LAB_1425ddc10`;
  - `6` → `DAT_1425ddc40` (n=2);
  - default → `DAT_1425ddbe0` (n=4). If the nearest-player test (`FUN_140a48a10(G, slot)` at `+0x564/+0x55c`, distance `< 10.0` `DAT_142588210`) and the facing test (`FUN_14043a0a0`, `> 135°` `DAT_14259be60`) both hold, `DAT_1425ddbd0` (n=4) instead.
  - Install table values (OBS): `DAT_1425ddbe0` = {0x59f, 0x59e, 0x59f, 0x59e}; `DAT_1425ddbd0` = {0x59d, 0x59c, 0x59d, 0x59c}; `DAT_1425ddbf0` = {0x5a4, 0x5a1, 0x5a2, 0x5a3}; `LAB_1425ddc10` (n=12) = {0x5a5…0x5ad, 0xcb, 0xcf, 0xd0}; `DAT_1425ddc40` = {0x5ae, 0x5af}; `DAT_1425ddc48` and `DAT_1425ddc58` = {0x5a5…0x5a8}; `DAT_1425ddcc8` = {0x665, 0x666}; `DAT_1425ddcd0` = {0x663, 0x664}.
- **`FUN_1406b6bb0` (StayDecision).** `(P, int* req)`, returns 1 to stay. `req[9] == −1` → 0. Threshold `DAT_142598578` (0.2), or `DAT_142588200` (0.1) when `req[0xe] != 0`. Cases on `req[0x16]−1`: 0 and 1 use 0.2 unless `lVar5 +0x5d8`/`+0x5dc` are both 5 (`lVar5 = FUN_140a48a10(G, P+0x2458)`); `0x3e`/`0x4c` with `req[0]−4 ≤ 1` and `0x51` → 0.1; `0x50` → 0.2 on a `req[0]` test. Compares `req[0x11]` against the threshold; the bearing from `P+8` to the ball sample (`FUN_140439e50`, ball ring `FUN_140a6cf70(G+0x40, 1)`) is folded by 360 (`DAT_14259858c`) and 180 (`DAT_142598580`). Returns 1 when `req[0xe] != 1` or `req[7]² ≤ thr²`. Meaning of "stay": INF (facing-to-ball test).
- **`FUN_1406b6ea0`.** Returns `(req[0x58] != 0x79)` when `req[0x34]` is not 0 or 5 and `|req[0x1c]| ≥ 33.75` (`DAT_1425cc688`); else 0. A keep-state predicate that excludes motion 0x79 (INF).
- **`FUN_140a638c0(x, side)`.** `side = (slot > 10)`; checks `0 ≤ slot ≤ 0x15` and that `side·11 + FUN_140a62610(…)` equals `x`. Predicate: is the player in this row (INF).
- **`FUN_140a48a30(G, side)`** = `G + (side+0xa1)·0x28` for side < 2, else `G+0x1928`.
- **`FUN_140a54130(x, slot, flag)`.** Checks `FUN_140a489f0(G, slot)`: `+0x1490` equals `x` and `+0x1494` equals `flag` (unless `flag == 4`). Pass-aim record test (P-AIM, `PI+0x1490/0x1494`).
- **`FUN_140591a10(vec, yaw)`** rotates a 2D vector by yaw (sin/cos via `FUN_140a92c80`, the 512-step table).
- **`FUN_14042d470(x)`** = `x · runtime · 3.6` (since `60·60/1000 = 3.6`). With runtime 54 this converts metres per tick to km/h (INF on the unit).
- **`FUN_140581ce0(angle, out, scale)`**: folds the angle to [0, 360) (`DAT_14259858c`), multiplies by `scale`, folds again.
- **`FUN_141eb3320(A)`** = `A+8 == 2` (done).
- **`FUN_1406d1170(P, req)`** (calls group A): `motion = IdleRow()`, row = `FUN_1406abb30(tbl, motion, 9)`, then `vt+0x10` of the `P+0xac0` interface (`FUN_1406d8620`, gap 3), then `FUN_1406af620` into `req[0x1b]` (frames). Connects IdleRow to the request (OBS).
- **`FUN_1406d1390`** (partial): tests phase `P+0x116c == 1` and `req[0xd] ∈ {4, 9}`, reads `req[7]` against `DAT_1425ddfcc` (168.75), sets `req[0xd]` to 4 or 9, and `req[7]` to `DAT_1425ddfc8` or `DAT_1425ddff8`. The rest is not read.

**Constants (install, OBS):** `DAT_14256ae1c` 1.0; `DAT_142565600` 0.5; `DAT_14259a764` 60.0; `DAT_14256ae34` 1000.0; `DAT_14259be5c` 90.0; `DAT_14259858c` 360.0; `DAT_142598598` −360.0; `DAT_142598580` 180.0; `DAT_142598578` 0.2; `DAT_142588200` 0.1; `DAT_14256ae18` 0.01; `DAT_14259be20` 25.0; `DAT_14259bdd4` 0.75; `DAT_142592228` 0.25; `DAT_14259222c` 5.0; `DAT_142588218` −1.0; `DAT_142588210` 10.0; `DAT_14258d694` −0.5; `DAT_14258c260` 0.9; `DAT_1425ddfcc` 168.75; `DAT_1425ddfc8` 167.5; `DAT_1425ddff8` −167.5.

**Gate:** group A's handlers call no anime-class predicate, so L3 is not a prerequisite. Status: **decoded**. Porting them needs the motion table filled (gap 2).

---

## Gap 2: Motion HitData / CancelData record layout

**Per-motion table (OBS):**
- `FUN_1406b20e0` allocates **one block of `0x3d13d0` bytes = `0x1125 × 0x390`** (4,002,768 bytes).
- It opens **22 Mbinfo files** from the path table at VA `0x14344dc40` (read from the exe): 0 Animation, 1 HitData, 2 SubHitData, 3 HoldData, 4 JumpData, 5 CancelData, 6 RotData, 7 DemoConnectData, 8 PersonalizedData, 9 FootType, 10 DangerData, 11 PivotInfo, 12 PivothitInfo, 13 BallData, 14 MoveAdjustInfo, 15 DivertInfo, 16 JointPosInfo, 17 EqualPartsPosInfo, 18 CameraPickupInfo, 19 NoballMotionInfo, 20 NoballFrameInfo, 21 NoballFootprintInfo. Each gets a `0xa0` reader object (vtable `0x1425dc588`, index in `+0x98`). **GF loads 3 of the 22** (`src/pes/mbinfo.cpp`: Animation, HitData, CancelData).
- Per-motion record fields as the readers use them (OBS):

| Offset | Use | Reader |
|---|---|---|
| `+0` byte | validity | `af620` |
| `+5` byte | count of the `+0xa0` list | `af8e0` |
| `+6` byte | count of the `+0xc8` list | `b1f00`, `b1d00`, `b1c70` |
| `+0xc` byte | count of the `+0x208` list | `b1930` |
| `+0x18` pointer | one Animation record (u32 word 1 bits 10..19) | `af620` |
| `+0xa0` list (8-byte pointers) | word bits 13..22 (`&0x3ff`) | `af8e0` |
| `+0xc8` list (8-byte pointers) | word 0 bits 13..23 (`&0x7ff`); word 1 bits 0..9 and 10..19 | `b1c70`, `b1d00`, `b1f00` |
| `+0x208` list (8-byte pointers) | u32 bit 29 | `b1930` |

- **The brief's `+0x110` list is not read by any reader decoded here.** It may belong to a reader I did not trace (INF). Open.
- **Mapping to the files (INF, with reasons):**
  - `+0xa0` ↔ **CancelData**: bits 13..22 is GF's `CancelData` frame (`CANCEL_FRAME_MASK 0x3ff`).
  - `+0x18` ↔ **Animation**: word 1 bits 10..19 is GF's `blendStart`.
  - `+0xc8` ↔ **HitData or SubHitData** (unresolved). Its word 0 field is 11 bits minus 878, which does **not** match GF's HitData frame (8 bits, 13..20). Conflict, see below.
  - `+0x208` ↔ unresolved (bit 29 flag).
- The 0x390 stride is per motion. The lists hold pointers, so the file records (HitData 108 B, CancelData 12 B, Animation 36 B) are likely referenced in place, not copied (INF).

**Writer:** the fill writer is not decoded. Searched: `FUN_1406b20e0` is the only allocator found for the 0x3d13d0 block; its per-file fill `FUN_1414a9570` is called from the loop and not decoded. Not searched further.

**How GF could load them:** `LoadMbinfo` already reads the three files from the VFS (`common/anime/Mbinfo/bin/…`; the exe's table uses `cpk_dat/…`, so the VFS root is `cpk_dat/` (INF)). To match PES's in-memory semantics GF needs: (a) the other Mbinfo files the readers reach (SubHitData at least), (b) the per-motion counts `+5`/`+6`/`+0xc`, and (c) the true HitData word-0 frame width.

**Status:** partial. Readers and offsets decoded. `+0xc8`/`+0x208`/`+0x110` field meanings and the fill writer open. Check still to run: HitData word 0 bits 21–23 and word 1 fields against the file.

**Gates unblocked:** A-MOV (timing reads), K-ERR (kick timing, INF).

---

## Gap 3: D1, the vtable at P+0xac0

**Resolved (OBS).**

1. Constructor `FUN_1406e03d0(P, slot)` writes:
   - `*P = PTR_LAB_1425e34d8`;
   - `P+0xab8 = PTR_LAB_1425e3508` (the anime interface, gap 4);
   - **`P+0xac0 = PTR_FUN_1425e35b0`**;
   then calls `FUN_1406eb670(P, slot)`. `FUN_1406e1610` writes the same vptrs (re-init).
2. Raw search (OBS): RIP-relative `lea` (`48/4c 8d`, modrm `05/0d/15/1d/25/2d/35/3d`, 7 bytes) over the memory image for targets `0x1425e3580…0x1425e3640` gives **2 hits, both vptr stores**: `0x1406e0420` (in `FUN_1406e03d0`) and `0x1406e1644` (in `FUN_1406e1610`), target `0x1425e35b0`. Target `0x1425e35d0` has **no** `lea` and no 8-byte absolute reference (0 hits each); Ghidra `get_xrefs_to 0x1425e35d0` returns none.
3. **Vtable at `0x1425e35b0`** (read from the exe, OBS):

| vt+ | Target | Note |
|---|---|---|
| 0x00 | `FUN_1406d2030` | EnterRun, group B (INF from the MOVESTEP notes) |
| 0x08 | `FUN_1406b9c80` | not decoded |
| 0x10 | `FUN_1406d8620` | motion request: reads `(this−8)` vt+0x50 (`P+0xab8`, `FUN_1406e7920`) for timing; partly decoded |
| 0x18 | `FUN_1406eadf0` | `*(u32*)(this+0x2cbc)` = P+0x377c (corrected 17-10-26 from pes-re-teamctx.md §8.1; was "P+0x297c") |
| **0x20** | **`FUN_1406ea110`** | **km/h of the root displacement at index `idx`**, the dodge's read |
| 0x28 | `FUN_1406ea750` | not decoded |
| 0x30 | `FUN_1406e9f90` | not decoded |
| 0x38 | `FUN_1406b9e50` | not decoded |
| 0x40 | `FUN_1406eab00` | acceleration ramp `(this, float, float)`, void (OBS) |
| 0x48 | `FUN_1406b9aa0` | not decoded |
| 0x50 | `FUN_1406e9f10` | turn playback (INF) |
| 0x58 | `FUN_1406b8de0` | not decoded |

4. The dodge (`FUN_140736d30`, OBS): `fVar2 = (**(P+0xac0)+0x20)(P+0xac0, 1)`, so `vt+0x20` = `FUN_1406ea110(this = P+0xac0, idx = 1)`.
5. `FUN_1406ea110` (OBS): `P = this − 0xac0`; calls `FUN_141eb50b0(P, buf, idx)`. For `idx < 2` (OBS), `FUN_141eb50b0` gives `Δ = (P+8 − P+0x14, P+0xc − P+0x18, P+0x10 − P+0x1c)`, the root's current minus previous position. Result: `|(Δx, Δz)| · (runtime·60·60/1000)`, with `runtime = DAT_14351ae54` (54.0 live).
6. **So the dodge's speed is `speed_kmh = |Δxz| · 54 · 3.6`**, with `Δxz` the last-tick root displacement in metres (formula OBS; the unit reading INF). The `FUN_1406eab00` concern in the handover does not apply: `eab00` is `vt+0x40`, and the dodge does not call it.

**What GF must provide for D1:** the root history `P+8…P+0x1c` (current and previous root). GF feeds its own physical speed today (per the handover). The D1 fix is to feed `3.6·54·|Δxz|` from the root history, or to show the two are equal. Not tested in this pass.

**Gates unblocked:** D1 (and C22), with its input defined. Also A-MOV and P-KIN (the root-history words).

---

## Gap 4: owner of P+0x245c

**The anime interface at `P+0xab8`** (vptr `PTR_LAB_1425e3508`, read from the exe, OBS). Offsets below are relative to the interface base `this = P+0xab8`:

| vt+ | Target | Effect (OBS) |
|---|---|---|
| 0x00 | `FUN_1406eaaf0` | returns `*(u64*)(this+0x3008)` |
| 0x08 | `FUN_1406ea810` | returns `(int)*(char*)(this+0x19a0)` (the player's match index, 0..21 range) |
| 0x10 | `FUN_1406ef320` | sets `*(u16*)(this+0x28)` = `P+0xae0` (current motion key), marks `this+0x2ca6` dirty |
| 0x18 | `FUN_1406e6bd0` | returns `this+0x28` (`P+0xae0`, the key read as u16) |
| **0x20** | **`FUN_1406eade0`** | **returns `this+0x19a4` = `P+0x245c`**, the phase byte |
| 0x28 | `FUN_1406e66b0` | returns `(int)*(char*)(this+0x11)` |
| 0x30 | `FUN_1406ec700` | returns `*(u8*)(this+0x19ba)` = `P+0x2472` (the flag byte the approach reads) |
| 0x38 | `FUN_1406e6960` | calls `vt+0x18` and `FUN_1406e68c0(P, key)` (refresh; void) |
| 0x40 | `FUN_1406e6a50` | returns `this+0x19b9` = `P+0x2471` |
| 0x48 | `FUN_1406e7910` | returns `this+0x2a2c` = `P+0x34e4` |
| 0x50 | `FUN_1406e7920` | per-motion timing reader (`motion, float, frame`) |
| 0x58 | `FUN_1406e7730` | per-motion root speed at a frame: `|root delta|` × scale, 0 when `motion ≥ 0x1125` (OBS) |

**Chain (OBS):**
1. **Factory** `FUN_1408be740(PM, src, flag)`: allocates `0x3bc0` bytes (`FUN_1403fb410(…, 0x3bc0)`), calls `FUN_1406e03d0(block, PM[3])`, stores the result at **`PM[0xfb]` = `PM+0x7d8`**; then `P+0x3ac0 = PM[0xfc]`, `P+0x3ad8 = vt+0x90(PM)`.
2. **Caller:** `FUN_1408be840(PM, …)` (one xref; builds a 160-byte block with `FUN_1408be270`, then calls the factory). Its own callers not traced.
3. **Gate:** `FUN_1408be3c0(PM, …)` must return nonzero first. It creates sub-objects at `PM+0x7d0` (`0x410`), `PM+0x938` (`0x418`), `PM+0x940` (`0x18`), and tests `FUN_141ebf1b0(PM+0x7d0, …)` and `FUN_1408bdf70` (body only partly read).
4. **Player lookup:** `FUN_140a48a10(G, slot)` = `*(G + 0x28·(slot+0xbf))`; `FUN_140a489f0(G, slot)` = `*(G + 0x28·(slot+0xf3))` (OBS).
5. `FUN_1406eb670` writes `*(u32*)(P+0x245c) = 0` and seeds RNG `P+0x32d0` (OBS).

So the owner of `P+0x245c` is the `0x3bc0` block `P`, reached from `PM+0x7d8`. PM's type is not decoded (INF on the role).

**Live check (read-only, OBS, game running, PID 2510857):**
- `G = 0x371d5090`; `MI = *(G+0x270) = 0` at the sample; `M+0x788` valid.
- `*(G + 0x28·(s+0xbf))` for `s = 0…21` read **0** in three samples. One earlier read showed `0x1` at `G+0x1f58` and a heap pointer `0x1d4d5ba0` at +8; that pointer's vptr at +0 is `0x14259b0b0`, not the P vptr, so it is not an AnimePlayer.
- `PM+0x7d8` reads 0, consistent with the handover. The sample is inconclusive: either the factory gate (step 3) is false, or the match is not in play (`MI = 0`). **Not refuted, not confirmed.**

**Status:** partial. Factory, allocation, vptrs, interface slots, and offsets decoded. The PM identity and the live gate are open. Blocked on a live sample with `MI ≠ 0`.

**Gates unblocked:** A-MOV (owner of `P+0x245c`), P-KIN (owner of root and RNG), D1 (vptr of P+0xac0 now known).

---

## Gap 5: L4, P+0x428 adjusters and solver

**Dispatcher `FUN_140732e30` (OBS):** phase = `*(char*)(P+0xab8 vt+0x20())` = `P+0x245c`.
- `1` → `FUN_140733a30` (strike);
- `2` → `FUN_1407335e0` (not decoded);
- otherwise, **including phase 0** → `FUN_1407344f0` (approach).
- `FUN_1407344f0` has one caller, `FUN_140732e30` at `0x140732e7a`. `FUN_140732e30` has no direct caller in the xref list (INF: indirect).

**Approach `FUN_1407344f0` (OBS):**
1. `flag = (P+0xab8 vt+0x30())` (= `P+0x2472`); `side = FUN_1406abba0()`.
2. `row`: if `(vt+0x28)() == 0x33`, row `0x50a` when `|ball − P+0x2bf4/0x2bfc| < 25.0` (`DAT_14259be20`) else `0x509`; otherwise `0x509`. Motion `sVar4 = FUN_1406abb30(side, row, 0)` (the Animetable lookup).
3. `(vt+0x10)(sVar4)` (= `FUN_1406ef320`, sets the motion key `P+0xae0`); then `FUN_1406f0f70(P, motion, 0, flag, 1.0, 0.2, 0)` (issue, OBS: `DAT_142598578` = 0.2 is the sixth argument).
4. `P+0x245d = P+0x245c`; `P+0x2471 = 0`; `P+0x2478 = 0`; **`P+0x245c = 1`**, so the next tick dispatches to the strike.
5. **Only if bit `0x40000000` of `P+0x2460` is set**: `FUN_1406b03d0` → ticks `iVar6`; `FUN_1406b00d0` → contact offset; `FUN_1404bef90(P+0x20, …)`; **`FUN_1406e85b0(P, target, motion, sample, …)` (solver)**; `FUN_141eb50b0(P, buf, 1)` (root velocity); **`FUN_141eb3910(P+0x428, target, P+8, iVar6+1, velocity)`**; `FUN_141eb3ae0(P+0x428)`.

**Strike `FUN_140733a30`** (archived decompile, `SimMotionRE/o_actions.txt` ~930–945 and ~1256–1260 in the scratch archive; not re-read this session): solver, then `FUN_141eb39a0(P+0x428, target, P+8, ticks+1, velocity, …)`, `FUN_141eb3900(P+0x428, …)`, `FUN_141eb3ae0(P+0x428)`. Yaw: `FUN_141eb35a0(P+0x4d0, …)` then `FUN_141eb3ae0(P+0x4d0)`.

**Solver `FUN_1406e85b0` (OBS):**
- `local = FUN_1406e8b70(P, out, motion, …)`: `local = (FUN_1406aff90(tbl, motion, …) − FUN_1406b05e0(tbl, motion, …)) · DAT_14256ae18 (0.01) · P+0x3b18` (`P+0x3b18` = 1.0 in the constructor). Two motion-table root positions, differenced: the root offset in the motion's frame.
- `c = FUN_140a92c80(yaw + 90°)` (cos), `s = FUN_140a92c80(yaw)` (sin).
- Output (`FUN_1406e85b0`'s `param_2`): `x = ball.x − (c·local.x + s·local.z)`, `y = 0`, `z = ball.z − (c·local.z − s·local.x)`. The root position at which the contact offset lands on the ball (INF on the physical reading; the arithmetic is OBS).
- 42+ callers (xref); only the two L4 callers traced.

**Adjusters on object `A = P+0x428` (OBS; `A+8` is the state, `A+0x6d` the mode):**
- **`FUN_141eb3910(A, target, cur, ticks, vel)`** (AdjustTransXZ setup): zeroes `A[1..3]`, `A+0x20`, `A+0x24`; calls `vt+0x10(A)`; stores `cur` (P+8) at `A+0x28/0x2c`, target at `A+0x30/0x34`, `vel` at `A+0x38/0x3c` and `A+0x4c/0x50`; `ticks` (u16) at `A+0x10`; `A+0xc = 1`; `A+8 = 1`; **`A+0x6d = 2`**.
- **`FUN_141eb39a0(A, target, cur, ticks, vel, …)`** (strike adjuster), for `ticks ≠ 0`: stores the same fields; velocity `A+0x30 = (target.x − pred.x)/ticks`, `A+0x34 = (target.z − pred.z)/ticks`, with `pred = FUN_141eb30c0(…)`; `A+0x48 = FUN_141eb3220(…)` (the alpha, clamped to `[0.25, 0.75]`); `A+0x10 = ticks` (u16); `A+0x6d = 1`; `A+8 = 1`.
- **`FUN_141eb30c0(out, …, n, vA, vB)`** (predictor, OBS): `out = p0 + (vA+vB)·(n/2) + (vA−vB)·((alpha−0.5)·(n−1)/2)`, per axis.
- **`FUN_141eb3220(…)`** (alpha, OBS): ratio of the position and velocity terms over `n`, clamped to `[0.25, 0.75]`; 0.5 when the displacement is zero.
- **`FUN_141eb3ae0(A)`**: if `A+8 == 1`, set `A+8 = 2` (finish). **`FUN_141eb3320(A)`** = `A+8 == 2`.
- **`FUN_141eb3900(A, u16)`**: `A+0x56 = A+0x58 = value` (u16), `A+0x64 = −1.0` (OBS).

**Open:**
1. The per-tick consumer of `A` (the code that reads `A+0x28…0x58` each tick and moves the root) is **not decoded**. It is the step that moves the root.
2. The writer of **bit 30 of `P+0x2460`** was not found. Not searched further.
3. `FUN_1407335e0` (phase 2) not decoded.

**Status:** **partial, high confidence** on dispatch, the approach gate, the solver arithmetic, and the adjuster formulas. Consumer and gate writer open.

**Gates unblocked:** L4 (kick approach root alignment, `P+0x428`). Inputs: A-MOV and P-KIN (root P+8 and yaw), K-ERR (kick row and distance). Still needs the consumer, the gate bit, and the motion table reads.

---

## Searches

| Target | Search | Result |
|---|---|---|
| `0x1425e35d0` as vptr | Ghidra `get_xrefs_to`; memory image `lea` (7 B) and 8-byte absolute | 0 hits each |
| `0x1425e35b0` vptr | same, window `0x1425e3580–0x1425e3640` | 2 `lea` hits: `0x1406e0420`, `0x1406e1644` |
| `FUN_1406eb670` callers | `get_xrefs_to` | 1: `FUN_1406e03d0` at `0x1406e04d3` |
| `FUN_1406e03d0` callers | `get_xrefs_to` | 1: `FUN_1408be740` at `0x1408be7a4` |
| `FUN_1408be740` callers | `get_xrefs_to` | 1: `FUN_1408be840` at `0x1408be89b` |
| `FUN_1407344f0` callers | `get_xrefs_to` | 1: `FUN_140732e30` at `0x140732e7a` |
| `FUN_140732e30` callers | `get_xrefs_to` | 0 (indirect, INF) |
| 0x390 motion fill writer | `FUN_1406b20e0` decompile | allocator and file loop found; `FUN_1414a9570` not decoded |
| bit 30 of `P+0x2460` | not searched | open |

## Files and state

- Scratch: `the scratch dir/` (`dec/`, `xref/`, `pe_const.py`).
- Nothing in `src/`, nothing committed, no game memory written.
- Live reads: `sudo -n` `/proc/2510857/mem` only, under `ulimit -v 2000000; nice -n 19 ionice -c3`, read-only (`LivePoke/common.py`).

---

# Round 2 (Main's order): every pending item

Scratch added: `the scratch dir/dump_mbinfo` (C++20, links `src/pes/vfs.cpp`+`cpk.cpp`, dumps raw bins via `PesVfs`), `Animation.bin`, `HitData.bin`, `CancelData.bin`, `SubHitData.bin`.

## Corrections to round 1

- **PM chain (round 1, gap 4) was wrong.** `G+0x28·(slot+0xbf)` reads zero even with `MI ≠ 0`. The G table is not the owner list. The live owner is found below.
- **`FUN_1406ea110` (dodge speed) and the `0x1425e35b0` vtable:** still correct (VA `0x1425e35b0`, slot `+0x20`).
- **`FUN_1414a9570` (round 1 said "fill writer"):** it is a reader-state initialiser, not the 0x390 fill (see item 8).

## 1. Per-tick consumer of A = P+0x428: BLOCKED (searched, not found)

- **Class of A (OBS):** `FUN_141eb4500` stores vtable `0x142b67768` at `param[0x85]`, `param[0x93]`, `param[0x9a]`, i.e. `+0x428`, `+0x498`, `+0x4d0` (OBS). That is the root-advance adjuster (A), the height adjuster and the yaw adjuster. The receiver is P (INF: the offsets are the P fields that the L4, height and yaw code use).
- **Vtable `0x142b67768` (OBS):** `+0x00` `FUN_141eb19c0` (ctor/dtor: sets vptr, optional free); `+0x08` `FUN_141eb33c0` (reset: zeroes `A[1..3]`, `A+0x20`, `A+0x24`, then `vt+0x10`); `+0x10` and `+0x18` `FUN_1415a171a` (thunk through `*0x14252fd80`); `+0x20` `FUN_141794410` (ret). **No tick virtual.**
- **Per-tick call (OBS):** `FUN_1406eb440` (per-player tick, from CollisionRE/d9): clears the phase (`P+0x245d = P+0x245c; P+0x245c = 0`), then `(A.vt+8)(A, P+0xa08)` and `(P+0x4d0.vt+8)(…)`, so A and yaw are **reset every tick**. The adjusters' state is re-armed by the dispatchers when they run.
- **Searches, none conclusive:**
  - Writers of A's state set: `FUN_141eb3910` (mode 2), `FUN_141eb39a0` (mode 1), `FUN_141eb26f0` (mode 3, OBS, body read), `FUN_141eb3ae0` (2 when 1).
  - Reads of mode byte `P+0x495`: 48 hits in the image. The 41 that match the `0x6d` offset form were checked; the containing functions are switch tables or unrelated (e.g. `FUN_1407ddd10` has `case 0x6d`).
  - Callers of the setters: `FUN_1407344f0` (approach), `FUN_140733a30` (strike), `FUN_1406b8340` (MOVESTEP entry, sets modes 3 and 2 at lines 222–228), `FUN_141eb26f0`.
  - `FUN_1406fa7c0` (after the reset): no A access.
- **Not done:** the consumer that reads `A+0x28…0x58` into `P+8`. It must be reached through a base pointer (lea of P+0x428 passed in a register), which the field-offset scan does not catch. Needs a targeted decompile of the callers of `FUN_1406b8340`.

## 2. Writer of P+0x2460 bit 30: writer found, its caller is NOT (BLOCKED)

- **Writer (OBS):** `FUN_14073338c` (decoded with `dec.sh`). It does `P+0x2460 |= 0x40000000` (two sites, `0x14073338c` and `0x1407333a2`), and, when a float argument differs from the stored one, writes it to `RSI+8` (RSI = a caller register, not decoded) and sets the bit again. Calls `FUN_141794410` (ret) and returns 1.
- **Callers: none found.** Scans over `pes_full.bin`: `call`/`jmp` rel32 to `0x14073338c`: 0. RIP-relative `lea` to it: 0. 8-byte absolute: 0 (also not in any exe data table). Ghidra `get_xrefs_to`: none.
- **Not searched:** switch jump tables with 32-bit offsets (the likely route for an action-dispatch function).
- **Bit 30 live:** clear in all 12 sampled players (`P+0x2460` values seen: 0, `0x80000`, `0x1000000`).
- **Adjacent, OBS:** `FUN_140733000` (`0x140733000`, a kick starter for motion `0x1ba`) sets phase 1 and never touches bit 30. It sits in vtable slot `0x1425df758`.

## 3. Phase 2 `FUN_1407335e0`: DECODED (high)

- Dispatch: phase byte 2 (via `FUN_140732e30`).
- Body (OBS): samples the motion (`FUN_1406b0250`/`FUN_1406b03d0`); if `FUN_1406ec260(P, ticks, P+0x2468)` then `P+0x2460 |= 0x400`. When `(vt+0x28)() == 0x33` and `FUN_141eb3320(P+0x4d0)` is false, it runs the yaw correction: `FUN_140439e50` (angle difference to the target) and `FUN_141eb34b0(P+0x4d0, …)` + `FUN_141eb3ae0(P+0x4d0)`.
- Tail: when `vt+0x38() < ticks` it may set `FUN_1406eebb0(P, 2, 0x5e)` (action setter, OBS). That is phase bookkeeping, not the root advance.

## 4. Solver `FUN_1406e85b0` callers: ENUMERATED, not each decoded (partial)

- xref (OBS): **44 call sites in about 33 functions**. L4 callers: `FUN_1407344f0` (approach) and `FUN_140733a30` (strike). Others include `FUN_1406ec710`, `FUN_1406ec940`, `FUN_14073d7d0`, `FUN_14073da40`, `FUN_1407423e0`, `FUN_140744850`, `FUN_140774c80`, `FUN_140780560`, `FUN_140782e00`, `FUN_140795bc0`, `FUN_1407965e0`, `FUN_14079c2d0`, `FUN_1407abbf0`, `FUN_1407c3690`, `FUN_1407c9a10`, `FUN_1407cc760`, `FUN_1407cac20`, `FUN_1407d0390`, `FUN_1407ce5b0`, `FUN_1407d29a0`, `FUN_1407dfac0`, `FUN_1407d5ae0`, `FUN_1407d5eb0`, `FUN_1407e0970`, `FUN_1407ed3a0`, `FUN_140829e80`, `FUN_140831ea0`, `FUN_140824ce0`, `FUN_140797cd0`, `FUN_140791ec0`, `FUN_14073cd80`.
- Not done: decoding each of the 33 for its use of the solver output.

## 5. Live owner chain of P+0x245c and PM identity: RESOLVED (live, read-only, OBS)

- **Live AnimePlayers (OBS, PID 2510857):** a heap scan (bounded, 2.85 GB of anonymous rw regions, `pread` only) found **25 objects** whose `+0`, `+0xab8`, `+0xac0` vptrs are `0x1425e34d8`, `0x1425e3508`, `0x1425e35b0`.
- **Per player (12 read):** slot `+0x2458` = distinct values 0…23; phase `+0x245c` ∈ {0, 1, 3, 9}; `+0x245d` (previous phase) set; bit 30 of `+0x2460` clear.
- **Owner (OBS):** each P has two holders, spaced exactly `0xa550`. The first holder is a pointer field of a per-player record. The candidate `PM = holder − 0x7d8` is a live object with **vtable `0x1426329a8`** and `PM+0x7d8 == P`. Confirmed for three P: `P=29ca340 → PM=3b777220`, `P=2c081d50 → PM=41400010`, `P=31fd2030 → PM=37191d80`. This matches the factory (`FUN_1408be740` stores P at `PM[0xfb] = PM+0x7d8`).
- **Not the G table:** `PM` is not referenced from the first `0x8000` bytes of `G`. Who holds PM is not found (not searched beyond the holder scan).
- **Caveat:** P+0x7d8 itself is float 1.0 (`0x3f800000`), a field of P, not the owner link. The owner link is PM+0x7d8.

## 6. HitData word-0 width against the file: DECODED (high)

- Dumped `HitData.bin` (315,900 bytes = 2,925 × 108, OBS).
- Word 0 bits 13–20 (8-bit frame, GF's mask): values **0 to 136**, median 10. Fits GF's 8 bits.
- Word 0 bits 21–23: non-zero in **1,288** of 2,925 records (values 1–7). These are **not** part of the frame. GF's 8-bit mask is right.
- The decompiled reader `FUN_1406b1c70` takes bits 13–23 (11 bits) minus 878 (`0x36e`): range **−878 to 941**, median −857. **Not a HitData frame.** So that reader is not applied to HitData records.
- `FUN_1406b1f00`/`b1d00` word 1: bits 10–19 minus 517 gives **−517 for every record** (constant). Bits 0–9 plus 1 give 1–26 (plausible). So `b1d00` fits HitData word 1 bits 0–9; `b1f00`'s field is not in HitData word 1.
- **Open:** which file the `+0xc8` list points to. `SubHitData.bin` (1,408 bytes) was dumped but not checked against these readers.

## 7. The `+0x110` list: NOT FOUND

- The 26 motion-table readers decoded (`FUN_1406aff90`, `b05e0`, `af620`, `af8e0`, `afbc0`, `b1f00`, `b1d00`, `b1c70`, `b1930`, `afcf0`, `b0720`, `b14f0`, `b10d0`, `b00d0`, `b16b0`, `b0840`, `b03d0`, `b0250`, `b15e0`, `b1400`, `b1360`, …) read record offsets `+0x1, +0x2, +0x3, +0x4, +0x5, +0x6, +0x7, +0xb, +0xc, +0x18, +0x20, +0x48, +0x78, +0x90, +0xa0, +0xc8, +0xe0, +0x1b8, +0x208`. **None reads `+0x110`.**
- Corpus grep for the 0x390 indexing (`* 0x390 + *param`): all hits are in these readers.
- Not done: any reader that is not one of these.

## 8. Fill writer `FUN_1414a9570`: PARTIAL

- `FUN_1414a9570(reader)` (OBS): `kind = (FUN_14149ab50() == 0 ? 2 : 8)` (`FUN_14149ab50` returns `DAT_143704e40`), then `FUN_1414a9680(reader, kind)`.
- `FUN_1414a9680` (OBS): sets `reader+8 = kind` and `reader+0xb`; for kind 2 it sets `reader+0x59 = 1`, calls `FUN_1423d90e0` and `FUN_1414b06c0(FUN_1414b0850(), reader)`; for kind 5 `FUN_1414b0e80`; for kind 1/8 it clears `+0x59` and calls `FUN_141b38a10(+0x60, 1)`.
- `FUN_1414b06c0` (OBS): appends to a list at `reader+0x48` (count `+0x50`, "list<T> too long" guard at `0xaaaaaaaaaaaaaa9`), then `FUN_141b37e50(+0x88)`. **So the fill is a list push, not a copy into the 0x390 block.**
- Not done: where the `0x390` table's pointer lists (`+0x20`, `+0x48`, `+0xa0`, `+0xc8`, `+0x208`) are written from these lists. `FUN_1414b0850` and `FUN_141b37e50` are not decoded.

## 9. `FUN_1406d1390` in full: DECODED (partial on two branches)

- First branch (round 1): phase `P+0x116c == 1` and `req[0xd] ∈ {4, 9}` gate a ball-distance test against `DAT_1425ddfcc` (168.75) and sets `req[0xd]` to 4 or 9 and `req[7]`/`req[3]`.
- Tail (OBS): `req[0x18] = req[3]`; `req[0x2d] = 0`; `req[0x2e] = 0x3e2aaaab` (≈0.1667); `req[0x38] = 1`; `req[0x39] = 2`; `req[0x1e] = req[0]`; `req[0x3c] = 0`; `(P+0x4d0.vt+8)(P+0x4d0, P+0xa08)` (the yaw reset).
- Middle (OBS): `motion = FUN_1406abb30(tbl, {row 0x54c or motion}, …)`, then `FUN_1406abd60`, then `(P+0xac0.vt+0x10)(P+0xac0, req, motion, speed, 0, DAT_1425ddf50, …)` (the motion issue).
- Not decoded: the row-selection branch that depends on `FUN_1406ebf30`/`FUN_1406e6bf0`.

## 10. Leaf callees

| Fn | Result |
|---|---|
| `FUN_1406aff90(tbl, out, motion, k, flag)` | reads the record's `+0x20` list (count `+1`, flag 0) or `+0x48` (count `+2`, flag 1); special case `motion 0xe42` (table `0x14344dcf4`); returns `out` (2 words) |
| `FUN_1406b05e0(tbl, out, motion, k, flag)` | same two lists: `flag != 0` → `+0x48` (word 2 and 4 of the entry); else `+0x20` (count `+1`) |
| `FUN_1404bef90(a, out, d)` | `out = wrap360(a + d)` (OBS) |
| `FUN_140a57800(G)` | `FUN_140a6cf70(G, 0)` (ball ring slot 0) |
| `FUN_140a62610(side, G, team)` | scans slots `side·11 + k` (k ≤ 10) for the first with `G+0x1748` flag set and the `team+0xc48` table entry non-zero; returns the index (OBS) |
| `FUN_140a581c0(p)` | reserve percent (`max(20, FUN_140a58550)·0x780/100`, quadratic in `+0x110` with `+0x118` words; partly read) |
| `FUN_140a58550(p)` | **not decoded** (3,955 bytes) |
| `FUN_1406ea750` | vt+0x28 (see §11) |
| `FUN_1406b9e50` | vt+0x38 (see §11) |
| `FUN_1406b9aa0` | vt+0x48: clamps its first arg to ≥ `DAT_1425c48a0` (0.15) and does a table lookup; rest **not decoded** |
| `FUN_1406b8de0` | vt+0x58 (see §11) |
| `FUN_1415a171a` | `(*DAT_14252fd80)()`: thunk through an import pointer (`+0x10`/`+0x18` of the adjuster class) |
| `FUN_141794410` | `ret` (used as a no-op vtable slot, many classes) |

## 11. Remaining `0x1425e35b0` slots: DECODED

| vt+ | Function | Result (OBS) |
|---|---|---|
| 0x08 | `FUN_1406b9c80(this, req, flag)` | calls `FUN_1406ba2f0(this-0x158, …)` (LocomotionStyle, a group A helper); the value comes from `req[0xb]` or `FUN_1406b9e80`. Connects the request to the style. |
| 0x28 | `FUN_1406ea750` | returns `*(float*)(this-0xb8 + 0x1b4)` if `*(char*)(this-0xb8 + 0x1b2) != 0`, else `−1.0` (`DAT_142588218`). `this-0xb8` = P+0xa08, the physique object. A speed limit from the physique (INF on the name). |
| 0x30 | `FUN_1406e9f90(this, flag)` | reads the anime-state byte `this+0x1998`, the motion record via `FUN_140a489f0` → `FUN_14124dad0`, byte `+0x2b` (or `+0x15` when `flag`), then `FUN_140a581c0` (reserve percent) below 11 (offset `DAT_142588210` = 10.0). A rate or stamina term (INF). |
| 0x38 | `FUN_1406b9e50(this, idx, motion)` | `idx < 4` and not (`idx == 0` with motion 3 or 6) → float table `UNK_1425dde10` = **{0.15, 0.3, 0.6, 1.0}**; otherwise 0. |
| 0x48 | `FUN_1406b9aa0(this, a, b)` | clamps `a` to ≥ 0.15 (`DAT_1425c48a0`), then a table lookup; rest **not decoded**. |
| 0x58 | `FUN_1406b8de0(this, k)` | `k ∈ 1..5` → `UNK_1425ddcd8[k]` = **{0, 1, 2, 3, −1}** (i.e. `k−1` for `k ≤ 4`, else −1); otherwise −1. |

## Open after round 2 (exact)

1. **Consumer of A (§1):** not found. Next: find the callers of `FUN_1406b8340` (the MOVESTEP entry, which sets A's modes) and read which function reads `A+0x28…0x58` into `P+8`.
2. **Caller of `FUN_14073338c` (§2):** not found by call, jump, lea, absolute or vtable scans. Next: 32-bit jump-table scan.
3. **Who holds PM (§5):** not found. The holder scan covered anonymous rw regions only.
4. **`+0xc8` source file (§6):** test against `SubHitData.bin`.
5. **`+0x110` (§7):** no reader found.
6. **Fill of the 0x390 pointer lists (§8):** `FUN_1414b0850` and `FUN_141b37e50` not decoded.
7. **Solver callers (§4):** enumerated; 31 of 33 functions not decoded.
8. **`FUN_140a58550`, `FUN_1406b9aa0` body (§10):** not decoded.

## Verification

- Live reads: `sudo -n`, `/proc/2510857/mem`, `pread` only, under `ulimit -v 2000000; nice -n 19 ionice -c3`. Scans bounded by time. No writes.
- Nothing in `src/`, nothing committed.

---

# Round 3 (Main's order)

## 1. A consumer (A = P+0x428): FOUND (predictor family), partial on callers

- **Consumer `FUN_1406e9800(P, out, n, speed)` (OBS, decompiled):** the root-displacement predictor. It reads the ring velocity (`FUN_141eb50b0(P,·,1)`), then calls `FUN_141eb3330(P+0x428)` (OBS: `A+8 == 2` and `A+0x48 > 0.1`, `DAT_142588200`). When true and `P+0x470 (= A+0x48) > 0.1`, it subtracts A's velocity `P+0x458/0x45c` (`A+0x30/0x34`) from the ring velocity. It then rotates by yaw (`FUN_14046f230`, sin/cos `FUN_140a92c80`) and writes `out = P+8 + vel·n·0.5` per axis (OBS, the `param_3·0.5` factor and `speed·DAT_14256ae34/(DAT_14351ae54·DAT_142598590)` scaling).
- **Family (OBS, same pattern, each reads `FUN_141eb3330/3320(P+0x428)` and `P+0x458`):** `FUN_1406e9050`, `FUN_1406e99b0`, `FUN_1406e9ac0`, `FUN_1406ecbb0`, `FUN_140837fc0`, `FUN_1406e9800`.
- **Callers (xref, OBS):**
  - `FUN_1406e9800`: `FUN_1406ecbb0` (×2), `FUN_1407476c0`, `FUN_140761890`, `FUN_140761060`, `FUN_140763560`, `FUN_140829e80` (×2), `FUN_1406ec940`.
  - `FUN_1406ecbb0` (called from the strike): `FUN_140733a30` at line 261 (OBS).
  - `FUN_1406e9050`: called from `FUN_140783860` (OBS, `lea_140783fa8.c` line 128).
- **L4 relevance (INF):** the strike `FUN_140733a30` reaches the family through `FUN_1406ecbb0`, so the root advance of the kick reads A through this predictor. Whether the approach `FUN_1407344f0` also does is not checked.
- **Searches:** `lea reg,[base+0x428]` then `[reg+0x28..0x58]` in code range → 8 sites, of which the adjuster-relevant ones are `FUN_1406ef200` (setter, calls `FUN_141eb39a0`), `FUN_140783860` (setter), and `FUN_141eb4650` (other class). Float accesses at `P+0x450…0x464` in code range: the predictor family above. **Caller set of `FUN_1406e9800` is only partly decoded** (out of scope beyond L4 per Main).
- **Callers of `FUN_1406b8340` (MOVESTEP entry) (OBS):** `FUN_1407824a0` (builds the request; checks anime classes via `FUN_1406b4580` for 0x52/0x53, else MOVESTEP), `FUN_140789dd0` (same for 0x53), `FUN_1407d1e60` (velocity integrator, calls MOVESTEP at line 221 then `FUN_1406eebb0` if the stop flag). These are per-action tick functions.
- **`FUN_1406eb440` (per-tick driver, OBS, full callees):** it is the action-entry reset, called only from the action setter `FUN_1406eebb0` (xref, OBS, at `0x1406eecc1`). Callees, in order: `FUN_1406eb440` clears phase, calls `(A.vt+8)(A)` and `(yaw.vt+8)`, clears `P+0x2460` bits, loops 19 slots `FUN_14081d920(P+0x3aa8, k)` and sets `P+0x2704…` to 0x11, `FUN_14081d930` (×2, conditional), `FUN_141af4eb0(P+0xa08, P+0xa00)`, `FUN_14058f0f0(P+0x25c8)`, `FUN_1406fa7c0(P)` (clears bit 17, sets `P+0x2890=4`), then predicates `FUN_140a6e300/580/340/5e0` on the phase.

## 2. Bit-30 writer `FUN_14073338c`: NOT a separate entry; it is a fall-through tail

- **Bytes (OBS, disk):** `0x14073336c…0x14073338b` ends with `f3 0f 11 36` (`movss [rsi], xmm6`). The writer `81 8f 60 24 00 00 00 00 00 40` (`or [rdi+0x2460], 0x40000000`) starts at `0x14073338c`, directly after that store. No prologue exists in `0x140733130…0x14073338b`.
- **Entry into the tail (OBS):** `FUN_140733000`'s phase test is `cmp byte [rax],1` at `0x140733027`, then `je rel32` at `0x140733029` → `0x140733130`. `FUN_140733032` has `je rel8` at `0x14073310a` → `0x140733130`. So the tail `0x140733130…0x1407333dc` is the **phase-1 (strike) continuation** of `FUN_140733000`; Ghidra's body for `FUN_140733000` stops at `0x140733031`.
- **Switch-table scan:** not needed for this writer. Its only entries are the two `je` sites above (intra-function). I did not run a 32-bit switch-table scan.
- **Callers of `FUN_140733000`:** vtable slot `0x1425df758` (OBS, found in round 2). That table's base is not determined.
- **Callers of `FUN_140733032`:** not searched.
- **Verdict:** the writer is reached each tick the phase is 1 (strike) through `FUN_140733000`'s vtable slot. Caller of the table not established.

## 3. 0x390 list fill (`FUN_1414b06c0` / `FUN_1414b0850`): PARTIAL

- `FUN_1414b06c0(reader, rec)` (OBS): appends `rec` to the reader's list at `+0x48` (`+0x50` count, guard `0xaaaaaaaaaaaaaa9`), then `FUN_141b37e50(+0x88)`. This is a list push, not a store into the 0x390 table.
- `FUN_1414b0850` (OBS by name in `FUN_1414a9680`): returns the list head used as the push target. Body not decoded.
- **Store into `+0xc8` / `+0x208` / `+0x110`:** not found. A scan for `imul …, 0x390` (`69 … 90 03 00 00`) found 104 sites in `0x1406ac…0x1406b0…`. The first decompiled (`FUN_1406ac4d0`, `0x24`-byte records, a different table) is **not** the motion table, so most are other structs. The readers decoded earlier are among the hits.
- **Verdict:** the copy of reader records into the 0x390 block is not located. Blocked on a store-writer search over those 104 sites.

## 4. `+0xc8` source vs `SubHitData.bin`: NOT CONFIRMED

- `SubHitData.bin` = 1,408 bytes (OBS). No record stride gives a clean fit:
  - 11-bit word 0 minus 878 spans −878…1133 (44-byte records), or −878 for most (64-byte: −878…−875). The reader's offset does not produce a plausible value on any stride.
  - Word 1 bits 10–19 in [500, 540]: at most 2 of 32 records (44-byte stride), 0 for 176+.
- **Verdict:** no evidence that `+0xc8` is SubHitData, and no stride makes its fields match the readers. Source stays open.

## 5. `FUN_140a58550` and `FUN_1406b9aa0`: DECODED

- **`FUN_140a58550(p)` (OBS):** a **reserve percent**. It reads two protected 32-bit words from the record: the slot at `p+0xf8[(p+0xf4) & 3]` and the slot at `p+0xd8[(p+0xd4) & 3]`. Each is decrypted by `byte ^ (counter·3 + k)·0x3f` with a counter from `p+0xf0` / `p+0xd0`, then checked against an FNV-1a 64-bit hash stored at `p+0xe8` (first) and `p+0xc8` (second). A mismatch calls `FUN_1415a1714` (integrity failure). A zero first word returns 0. Result: **`(D·100) / N`**, D = the `p+0xd8` value, N = the `p+0xf8` value (both current/max pairs, INF on the meaning).
- **`FUN_1406b9aa0(_, x, s)` (OBS):** a three-segment curve. Clamp `x ≥ 0.15` (`DAT_1425c48a0`). Test segments from the top: thresholds `thr = {0.15, 0.3, 0.5}` (`0x1425dde30/34/38`, index 0/1/2), `span = {0.15, 0.2, 0.5}` (`0x1425dde40`), `base = {1.15348, 3.05600, 4.04072}` (`0x1425dde50`), `slope = {1.90252, 0.98472, 4.25334}` (`0x1425dde60`).
  - For the first `k` (2, 1, 0) with `thr[k] ≤ x`: `out = s·base[k] + ((x − thr[k]) / span[k]) · slope[k] · (k < 2 ? s : 1)`.
  - It always matches at `k = 0` after the clamp, so the 0.0 fallthrough is unreachable.
- Meaning of `x` and `s` (INF): the vt+0x48 call passes a speed or rate and a scale.

## Remaining open (exact)

1. **Predictor family callers (§1):** only partly enumerated; out of scope beyond L4.
2. **Bit-30 table caller (§2):** `FUN_140733000`'s vtable table base not established; `FUN_140733032` callers not searched.
3. **0x390 record store (§3):** not located; 104 `imul 0x390` sites, mostly other structs; reader-record copy path `FUN_1414b0850` body not decoded.
4. **`+0xc8` source (§4):** SubHitData does not fit; source not established.
5. **Thunk `_DAT_14252fd80` (vt+0x10 of the adjuster class, §round 2):** the global holds `0x6ffffa666be0` live (not a canonical address, disk value `0x3398a3a`). The thunk's real target is not resolved.

## Verification

- Live reads only (`sudo -n`, `/proc/2510857/mem`, `pread`), no writes.
- Nothing in `src/`, nothing committed.
- Scratch: `the scratch dir/dec/` (round-3 decompiles: `1406eb440`, `1406eebb0`, `1407824a0`, `140789dd0`, `1407d1e60`, `1406e9800`, `c_*`, `lea_*`, `f32.txt`, `bit30.txt`, `140a58550`, `1406b9aa0`).

---

# Round 4 (Main's order: thunk, §3 fill, §4 SubHitData)

## 0. The thunk target: RESOLVED (corrects round 2 and 3)

- Live `*0x14252fd80 = 0x6ffffa666be0` is a **function address**, not an encoded pointer. The thunk `FUN_1415a171a` calls it directly. Round 2's dereference was wrong.
- Module (OBS, `/proc/2510857/maps`): `vcruntime140.dll` at `0x6ffffa660000` (header line 823), with its `.text` mapped as an anonymous `r-xp` region at `0x6ffffa661000` (LivePoke's "module name missing" case). RVA = `0x6ffffa666be0 − 0x6ffffa660000 = 0x6be0`.
- Export table on disk (`…/eFootball PES 2021/drive_c/windows/system32/vcruntime140.dll`, OBS): RVA `0x6be0` = **`_purecall`**. Disk bytes at `0x6be0` equal the live bytes (`48 83 ec 28 e8 e7 ff ff ff …`).
- **Consequence:** the global `0x14252fd80` holds the MSVC pure-virtual handler. The base vtable `0x142b67768` has `+0x10` = `_purecall`, so the base is abstract. The **live** adjuster vptrs are derived classes (below).

## 0b. Live adjuster vtables (OBS; corrects rounds 2–3)

| Object | Live vptr at P+… | slot +0x08 | +0x10 | +0x18 | +0x20 |
|---|---|---|---|---|---|
| A, `P+0x428` | `0x142b678f0` | `FUN_141eb33e0` (reset, calls +0x10, clears `+0x4c..+0x6c`) | `FUN_141eb1a20` (clears `+0x28..+0x48`, `+0x6d = 0`) | **`FUN_141eb1bf0` (per-tick step)** | `FUN_141794410` (ret) |
| height, `P+0x498` | `0x142b67920` | `FUN_141eb3440` | `FUN_141eb1a40` | `FUN_141eb2450` | ret |
| yaw, `P+0x4d0` | `0x142b67950` | `FUN_141eb33c0` | `FUN_141eb1a10` | `FUN_141eb1a50` | ret |

- All 12 live P read the same three vptrs (OBS).
- Dtor of base: `FUN_141eb45e0` (sets `0x142b67768`, frees 0x70).

**A's per-tick consumer = `vt+0x18` = `FUN_141eb1bf0(A, P, buf)`** (OBS, decompiled, ~200 lines). It branches on `A+0x6d` (mode):
- mode 3: `A+0xc += 1` (tick counter).
- mode 2: reads P's root and yaw (`FUN_141af3070/3100/3330`, `FUN_141afdf10` ring sample), sin/cos (`FUN_140a92c80`), rotates the A velocity (`A+0x30/0x34`, `A+0x4c/0x50`) by yaw, and steps over `A+0xc..A+0x10` ticks (`FUN_140581ce0` angle wrap). This is the root advance in the L4 approach/strike.
- Callers: indirect (`call [vptr+0x18]`). The root-motion predictor family of round 3 (`FUN_1406e9800`) reads the same `A+0x30/0x34` and `A+0x48` (alpha) and is the non-virtual reader.

## 3. The 0x390 fill: TRACED (corrects round 3)

Round 3 said `FUN_1414b06c0` pushes records. **Wrong:** it pushes the **reader object** onto a global queue.

Chain (OBS, each step decompiled):
1. `FUN_1406b20e0` (allocator, round 1): for each of the 22 files, if `FUN_1414a8790`-initialised: `FUN_1414a9570(reader)` → `FUN_1414a9680(reader, 2 or 8)`.
2. `FUN_1414a9280(reader)` (file open): `FUN_1414a9680(reader, 3)` (state only), reads the file via `FUN_1415a2fd0`, schedules the parse with `FUN_1415a71b8`; the callback is `FUN_1414a8f00`.
3. `FUN_1414a8f10(reader)` (parse): state 6; `FUN_1414abd80` tests a magic (the reader's name against `DAT_142975cd4/cd8`); if flagged, XOR-decrypts the buffer with a 32-bit xorshift keystream (`0x3ade68b1…`); `FUN_1414a8a50` validates (`FUN_1403b29d0`, `"1 2 11 14"` pattern); state 1/2 → `FUN_1414b0dd0(FUN_1414b0e80(), reader)`; then `FUN_1403fb5b0` releases; at the end **`(reader.vt+8)(reader, data, size)`**.
4. Reader vtable `0x1425dc588`: `+0x00 = FUN_1406aede0`, `+0x08 = FUN_1406aee20` (OBS).
5. `FUN_1406aee20` → `FUN_1406aef60(table, data, size, kind = reader+0x98)`, then `reader+0x98`'s flag `table+0xc0+kind = 1`.
6. `FUN_1406aef60` is a switch on `kind` (0..21) to the per-file **builder** (table in `param_1[2+kind]`).

**In-memory buffer (OBS):** the data pointer is `buffer+0x10` (`FUN_1414abd70`), size `u32 @ obj+8`, key `u32 @ obj+0xc`, flag byte `@ obj+2` (bit0 = decrypt, bit1 = `FUN_1414abdb0`). That 16-byte block is **not in the file**: it is the loader's in-memory object. So there is no header on disk to skip (SubHitData's first 16 bytes are record data, §4).

**Builders (OBS, `dec/bld/`).** Each builder takes `(table, data, kind, size)`, sets per-motion record pointers into lists, and does not copy:
- Record size: `size >> n` or `size / K` (`K` per builder).
- Motion id: `(w0 & mask) + offset` (e.g. `(w0 & 0x7ff) + 0x61` for kind 2; `(w0 & 0x1fff) + 0x11` for kind 13/14).
- Count byte: `record[kind]` **per motion**, bounded by `DAT_1425dc370[kind]` (the capacity).
- Pointer stored at `list(kind)[count] = record_ptr`, where `list(kind) = FUN_1406ae810(motion_record, kind)`.

**Kind → file → list (OBS):** from `FUN_1406ae810`'s 22-entry offset table (`local_c8[k]`, k = kind), joined to the path table (`0x14344dc40`, index = kind), the record sizes, and the caps `DAT_1425dc370` (`[1,5,6,3,2,5,3,4,2,20,1,10,1,1,1,1,3,10,1,1,10,20]`):

| kind | file | record | list off | count byte | cap | builder |
|---|---|---|---|---|---|---|
| 0 | Animation | 36 | `+0x18` | `+0` | 1 | `FUN_1406ac4d0` (`/0x24`) |
| 1 | HitData | 108 | `+0x20` | `+1` | 5 | `FUN_1406ad300` (`/0x6c`) |
| 2 | SubHitData | 32 | `+0x48` | `+2` | 6 | `FUN_1406ae670` (`>>5`) |
| 3 | HoldData | 16 | `+0x78` | `+3` | 3 | `FUN_1406ad4d0` (`>>4`) |
| 4 | JumpData | 4 | `+0x90` | `+4` | 2 | `FUN_1406ad810` (`>>2`) |
| 5 | CancelData | 12 | `+0xa0` | `+5` | 5 | `FUN_1406ac990` (`/0xc`) |
| **6** | **RotData** | **8** | **`+0xc8`** | `+6` | 3 | `FUN_1406ae4f0` (`>>3`) |
| 7 | DemoConnectData | 4 | `+0xe0` | `+7` | 4 | `FUN_1406accb0` |
| 8 | PersonalizedData | 4 | `+0x100` | `+8` | 2 | `FUN_1406ae020` |
| **9** | **FootType** | 4 | **`+0x110`** | `+9` | 20 | `FUN_1406ad170` |
| 10 | DangerData | 4 | `+0x1b0` | `+10` | 1 | `FUN_1406acb20` |
| 11 | PivotInfo | 8 | `+0x1b8` | `+11` | 10 | `FUN_1406ae1c0` (`>>3`) |
| **12** | **PivothitInfo** | 16 | **`+0x208`** | `+12` | 1 | `FUN_1406ae350` (`>>4`) |
| 13–15 | BallData, MoveAdjustInfo, DivertInfo | | `+0x210/0x218/0x220` | | 1 | |
| 16 | JointPosInfo | | `+0x228` | | 3 | |
| 17 | EqualPartsPosInfo | | `+0x240` | | 10 | |
| 18 | CameraPickupInfo | | `+0x290` | | 1 | |
| 19 | NoballMotionInfo | | `+0x298` | | 1 | |
| 20 | NoballFrameInfo | | `+0x2a0` | | 10 | |
| 21 | NoballFootprintInfo | | `+0x2f0` | | 20 | |

- Kinds 13–21 list offsets are from the table; their builders are not all decoded.
- Kind 17 is `thunk_EXT_FUN_143fafa70` (external, not decoded).

**Store into `+0xc8`/`+0x208`/`+0x110` (OBS):** the builders store via `list(kind)` (computed offset), so no builder has a literal `0xc8`/`0x208`/`0x110` (confirmed by grep). The three lists are:
- `+0xc8` = RotData (kind 6), count `+6`, cap 3.
- `+0x110` = FootType (kind 9), count `+9`, cap 20.
- `+0x208` = PivothitInfo (kind 12), count `+0xc`, cap 1.

**Round-1/2 readers, rechecked:** `FUN_1406b1c70/b1d00/b1f00` read `+0xc8` = RotData, and the field ranges fit RotData (§4). `FUN_1406b1930` reads `+0x208` (bit 29 of PivothitInfo word 0; PivothitInfo records are 16 bytes, the builder's `0xfff`/`+0x20b` motion offset).

## 4. SubHitData: every stride the file size allows, no header

- Builder for kind 2 (`FUN_1406ae670`): `count = size >> 5` (32-byte records), motion = `(w0 & 0x7ff) + 0x61`, data from `buffer+0x10` = file start (the file has no header; see §3).
- File: 1,408 bytes = **44 × 32**. Motion range 97–2104, 7 distinct motions, **max 14 records per motion** against the kind-2 cap 6 (the builder drops the excess beyond the cap). Other strides were tested in round 3 and do not fit the builder.
- **So `+0xc8` is not SubHitData.** Its source is **RotData** (kind 6, below). SubHitData is the `+0x48` list.

**RotData (`+0xc8`), dumped via VFS:** 34,760 bytes = **4,345 × 8**, motion ids `w0 & 0x1fff` 0–4388 (the `0x1125`-slot table). The round-1 readers fit it:
- `w0` bits 13–23 minus 878: range −878…638; 63% in [−50, 50] (`FUN_1406b1c70`).
- `w1` bits 10–19 minus 517: range −517…231 (`FUN_1406b1f00`).
- `w1` bits 0–9 plus 1: range 1…598 (`FUN_1406b1d00`).

## Open after round 4

1. **Live A owner:** the derived vptr set is verified (25 P, 12 read), but the root-advance callers of `FUN_141eb1bf0` (indirect) are not enumerated.
2. **Builders for kinds 13–21 (incl. the external kind 17):** not decoded; only the list offsets and caps are.
3. **Decrypt flag:** `FUN_1414abdc0`/`abdb0` (flag bits of the in-memory object) are decoded; whether the install's Mbinfo files set them is not checked. The dumped bins parse as plausible plain records (frames 0–136, motion ids in range), which suggests no decryption, but that is inferred.
4. **Cap overflow:** SubHitData has 14 records on motion(s) with cap 6 (8 dropped in the builder). Whether PES intends this is not established.

## Verification

- Live reads only (`sudo -n`, `/proc/2510857/mem`, `pread`), bounded; no writes.
- Dumps: `the scratch dir/{SubHitData,RotData,FootType,HitData,CancelData,Animation}.bin` (via `dump_mbinfo`, the PesVfs path).
- Decompiles: `dec/bld/*.c` (22 builders), `dec/rd_*.c` (reader/parser chain), `dec/fill_1406aef60.c`, `dec/rv_*.c`, `dec/vt_*.c`, `dec/vtslots.txt`.
- Nothing in `src/`, nothing committed.

---

# Round 5 (Main's order: adjuster callers, kinds 13–21, decrypt flag, SubHitData drops)

## 1. Callers of `vt+0x18` on the adjusters: FOUND (per-tick driver chain)

Chain (OBS, decompiled):

1. **Message handler `FUN_1409f1bb0`** (3,373 bytes; corrected 17-10-26 from the bridge decompile). It returns unless the message kind `*param_2 == 0x14`. Otherwise it updates one slot's team record (slot from `param_1−0x938` mod 11). When bit `(param_2[5] >> 23)` is set (`uVar7 = 0x3c`), it runs 60 iterations over the same object `P = *(param_1−0x178)`: `FUN_1406e5fc0(P, sample, 0)`, `FUN_141eb5e70(P, tick)`, `FUN_141eb4650(P, 0)`. It is not a per-player loop over 22 players; `uVar7` is a 0/60 count, not a player count. The 22-player path is the slot-2 tick (pes-re-teamorder.md §2).
2. **Owner tick `FUN_1408bfcf0(PM, param)`** (487 bytes). With `P = *(PM+0x7d8)`, it calls `FUN_141eb5e70(P, *param)` when `PM+0xa07 == 0 || PM+0xa06 != 0` and `PM+0x7c8 != 0`, then `FUN_141ebc820(PM+0x940, …)`. This is the PM tick of round 2 (the owner with vtable `0x1426329a8`).
3. **Adjuster driver `FUN_141eb5e70(P, tick)`** (OBS): `FUN_141eb3af0(P+0x428)`, `FUN_141eb3af0(P+0x498)`, `FUN_141eb3af0(P+0x4d0)`, then `(P.vt+0x18)(P)` (on P itself), `FUN_141af7a90(P+0xa08, tick)`, `(P+0xa08.vt+0x38)()`, `P+0x41c = tick`, `P+0xa05 = 1`.
4. **Per-adjuster tick `FUN_141eb3af0(adj, P)`** (OBS, decompiled): if `adj.state == 2`:
   - `adj.ticks_now` (`+0xc`) `< adj.ticks_end` (`+0xe`): `+0xc += 1`, return;
   - `if adj.ticks_end < adj.ticks_now` (`(short)adj[2] < cnt`): finish: `vt+0x10` (reset), `vt+0x20` (finish hook, `FUN_141794410` = ret for A);
   - else **`(adj.vt+0x18)(adj, P, adj+0x14, adj+0x20)`**, the step.
   - Before the test: `FUN_141af3ca0(P, adj+0x14, adj+0x18, adj+0x1c, 1)` samples the body.
5. **Step slots (derived classes, OBS, §0b of round 4):** `vt+0x18` = `FUN_141eb1bf0` (A, `P+0x428`), `FUN_141eb2450` (height, `P+0x498`), `FUN_141eb1a50` (yaw, `P+0x4d0`).

So the per-tick driver for the three adjusters is `FUN_141eb5e70` → `FUN_141eb3af0` → `vt+0x18`, reached from the owner tick `FUN_1408bfcf0` (once per player, slot 2 of the 22) and from the message handler `FUN_1409f1bb0` (60 ticks on one object). The driver is closed; the 22-player caller stays OPEN (corrected 17-10-26: `FUN_1409f1bb0` is not that caller).

Caveats:
- A **direct** `call [reg+0x18]` on the adjuster from other code (a `lea reg,[P+0x428]` then `call [reg+0x18]` within 16 bytes) matches 3 sites: `0x14073d173` (FUN_14073cd80), `0x14073dc2a` (FUN_14073da40), `0x140792d67` (FUN_140791ec0). Each decompile shows the `+0x18` call on the anime interface `P+0xab8` at the same place, so **these are not verified as A calls** (the lea is for another argument). Treat them as unconfirmed.
- The 2 `Y` matches (`0x140733919` in FUN_1407335e0, `0x141eb5ec0` in FUN_141eb5e70) are calls on `P` itself or on the anime interface, not the yaw adjuster.
- `FUN_1409f1bb0`'s caller is not decoded (bridge xrefs return 404). Its `uVar7` is 0 or 0x3c (decoded 17-10-26), not a player count.

## 2. Builders for kinds 13–21 (OBS, decoded through `FUN_1406aef60`)

Dispatch (OBS): kind 13 → `FUN_1406ac660`, 14 → `FUN_1406ad9a0`, 15 → `FUN_1406ace40`, 16 → `FUN_1406ad670`, 17 → `FUN_143fafa70`, 18 → `FUN_1406ac7f0`, 19 → `FUN_1406ade70`, 20 → `FUN_1406adcd0`, 21 → `FUN_1406adb30`.

| kind | file (path idx) | record | list (`FUN_1406ae810` off) | count byte | cap (`DAT_1425dc370`) | motion id | file on disk | records |
|---|---|---|---|---|---|---|---|---|
| 13 | BallData | 20 (`/0x14`) | `+0x210` | `+13` | 1 | `w0 & 0x1fff` | 86,900 B | 4,345 |
| 14 | MoveAdjustInfo | 4 (`>>2`) | `+0x218` | `+14` | 1 | `((w0>>4)&1) + 0x909` | **0 B (empty)** | 0 |
| 15 | DivertInfo | 4 (`>>2`) | `+0x220` | `+15` | 1 | `((w0>>2)&1) + 0x909` | **0 B (empty)** | 0 |
| 16 | JointPosInfo | 16 (`>>4`) | `+0x228` | `+16` | 3 | `(w0 & 0x7ff) + 0xbe` | 11,968 B | 748 |
| 17 | EqualPartsPosInfo | 12 (`/0xc`) | `+0x240` | `+17` | 10 | `(w0 & 1) + 0x909` | 120 B | 10 |
| 18 | CameraPickupInfo | 4 (`>>2`) | `+0x290` | `+18` | 1 | `(w0 & 0x1fff) + 0xee` | 980 B | 245 |
| 19 | NoballMotionInfo | 40 (`/0x28`) | `+0x298` | `+19` | 1 | `(w0 & 0x1fff) + 0x11` | 41,440 B | 1,036 |
| 20 | NoballFrameInfo | 32 (`>>5`) | `+0x2a0` | `+20` | 10 | `(w0 & 0x1fff) + 0x11` | 331,520 B | 10,360 |
| 21 | NoballFootprintInfo | 32 (`>>5`) | `+0x2f0` | `+21` | 20 | `(w0 & 0x1fff) + 0x11` | 153,792 B | 4,806 |

Every non-empty file's size is an exact multiple of its record size. Kinds 14/15 are empty on the install.

**Kind 17 source (OBS):** `FUN_143fafa70` is **real code**, not an external. It starts `push rbx … mov eax,0x1190; call __chkstk` (MSVC). Its section is **`.impdata`** (RVA `0x3d14000`, VA `0x143d14000`, vsize `0x17c3d94f`, flags `0xe0000020` = code + read + write). The PE section named `.impdata` is executable, so Ghidra could not name the body. The "EXT" name in the decompile is the analysis label for that section, not an import. Its builder is the same shape as the others (`/0xc` records, 2-slot motion `+0x909`).

**Kinds 13–21 record-count check:** every non-empty file divides exactly by its record size, so the sizes above are the on-disk layout.

## 3. Decrypt flag: STATIC PARTIAL, plain bytes test POSITIVE

- **Gate (OBS):** the parser `FUN_1414a8f10` reads the buffer header at `buffer+0..0xf`: byte `+2` bit 0 (`FUN_1414abdc0`) → XOR keystream; bit 1 (`FUN_1414abdb0`) → a second predicate (not decoded). Size is `u32 @ +8`, key `u32 @ +0xc`. Data is at `buffer+0x10` (`FUN_1414abd70`).
- **Keystream (OBS, from the decompile):** `u9=0x3ade68b1, u16=0x297c9cb4, u11=0x25822e49, u3=(key<<16)|size`; for each dword: `u9 ^= u9<<11; u10 = u3 ^ (((u3>>11) ^ u9)>>8) ^ u9; data ^= u10; (u9,u16,u11,u3) = (u16,u11,u3,u10)`. Implemented and run on the install bytes.
- **Flag writer: NOT FOUND.** `FUN_1414a8790` writes `reader+2` (the reader, not the buffer). `FUN_1415a2fd0 → FUN_1415a2894 → FUN_1415a5358` fill the 0x30-byte descriptor (`param[0] = buffer`, `param[1]` and `param[2]` sizes, `+0x28` a flag word); no store to the buffer's `+2` appears in their bodies. Not searched: writes of `byte [reg+2]` across the image.
- **Install test (OBS):** `RotData.bin` raw = 4,345 × 8 bytes, motion `w0 & 0x1fff` in `0…4388`, **100% ≤ 0x1124**. After the keystream (key `0`, `1`, `0x10`, size = file length) the same field falls to **≈53%** (random-like: min 0, max 8191). So the install bytes are **plain**, not XOR-encrypted, for the tested keys. (INF: the true key is unknown; plain bytes fit the table range fully, which random keystream output would not.)
- **Verdict:** flag bit 0 is 0 for these files (INF); writer not located.

## 4. SubHitData cap overflow (PES as written; not fixed)

- Builder (kind 2, `FUN_1406ae670`): the count byte `record[+2]` of each motion is checked against cap `DAT_1425dc370[2] = 6` before storing. Records past the cap are not stored and the count stays at 6. Order is file order.
- Motions: 7 distinct (ids `(w0 & 0x7ff) + 0x61`, range 97–2104). **One motion overflows: motion 599 (`0x257`)** — 14 records; first 6 kept (file indices 0–5 of that motion), **8 dropped**: file record indices **36, 37, 38, 39, 40, 41, 42, 43**.
- Totals: 44 records, **36 stored, 8 dropped**; all other motions fit under the cap.

## Open after round 5

1. **Direct `vt+0x18` calls on A from other code:** the 3 strict sites are unverified. The per-tick chain (`FUN_141eb5e70` → `FUN_141eb3af0`, called from `FUN_1408bfcf0`) is the verified driver.
2. **Caller of `FUN_1409f1bb0`:** not decoded. `uVar7` is decoded (0 or 0x3c).
3. **Kinds 14/15:** the builders are decoded, but the files are empty on the install.
4. **Flag writer:** the `buffer+2` store is not located by static search.
5. **Flag bit 1 (`FUN_1414abdb0`):** not decoded.

## Verification

- Live reads: none this round. Static decompiles, the install's PE sections, the VFS dumps (`dump_mbinfo`), and a Python port of the keystream for the bit-field test.
- Nothing in `src/`, nothing committed.
- Scratch: `dec/k17.txt`, `dec/cs_*.c`, `dec/af0.c`, `dec/cl_*.c`, `dec/drv_5e70.c`, `dec/hd_*.c`; dumps `BallData.bin`, `JointPosInfo.bin`, `NoballFrameInfo.bin`, `RotData.bin`, `FootType.bin`, `PivothitInfo.bin`, `PivotInfo.bin`, `SubHitData.bin`, and the empty `MoveAdjustInfo.bin` / `DivertInfo.bin`.
