# Speed inputs for clip selection (S1Watch, rounds 9–10)

> **Main audit 19-10-26:** clamp bytes `0x142613CAE`=40, `0x142613CAF`=99 (this is PES's general player-ability range 40..99, shared by every ability id via the `0x142613C60 + 2*id` table; expect it at every stat site, it is not Speed-specific); vtable `0x1425E3508`+0x60 = `0x1406E7EB0` (clip duration, not a band speed): match. PARTIAL: the base-row writer and the band-speed source (11-knot table at `0x1425DE010`, INF) are open. Supersedes the earlier "vt+0x60 = FUN_1406B9B20" reading in the anim-select docs.
> **Main audit 19-10-26 (round 10):** band table `0x1425DDE10` = [0.15, 0.3, 0.6, 1.0] and vtable `0x1425E35B0`+0x38 = `0x1406B9E50`: match. **Main disputes the "stock fill" reading of the base row:** the live match was BNT vs W (4cc teams), whose squads carry one stat value per player on tiers, so a uniform 77/82/88/93/99 ability row is most likely the team's real database data [INFERENCE, not checked against the team file]. Check on a stock-team match before calling it a fill.

Tags: **CONFIRMED** = decompile, image read, or live read with VA; **INF** = inference; **OPEN** = unresolved. Constants script-read from `pes_full.bin` (offset = VA − 0x140000000). Operand VAs are Ghidra labels or raw addresses.

## Summary

1. **Speed byte (id 0x27): writer and formula CONFIRMED.** `Speed = clamp((u8) trunc(base[0x27] · A2·A1·A3·A4·A5), 40, 99)` in `FUN_14092AF10`. The 40–99 clamp is the shared ability clamp table at `0x142613C60 + 2·id` (per Main), so it is not a Speed finding.
2. **Band speed (round 10): the PES value is a dimensionless rate fraction, not km/h (CONFIRMED).** The anime-interface slot `+0x38` returns `{0.15, 0.3, 0.6, 1.0}` for bands 0–3. The 11-knot table `0x1425DE010` is the speed→rate curve used by the steady-run path. GF's `BandSpeedKmh` stand-in (CurrentSpeedKmh) is the wrong quantity.
3. **Roster base row (round 10): a per-player uniform fill, not a per-stat DB row (CONFIRMED layout, loader OPEN).** Every ability byte of a player's row carries the same value. The values form a stock ladder `77 + 5k`. The loader is not located.
4. **Live checks:** the base row is read live; the band-speed rule is not checked against a running player (OPEN, reason in §4).

## 1. Speed byte (id 0x27)

### 1.1 Writer and formula (CONFIRMED)

`FUN_14092AF10(P, Q)` (519 lines; caller `FUN_1409255F0`, reached through a vtable, no direct E8 caller) rebuilds the ability bytes for one player per tick.

- **Roster.** `roster_t = *(G + (0xA3 + t)·0x28)` (`FUN_140A489D0`; `0x1978` team 0, `0x19A0` team 1). Live: team 0 at `0x1D48A0D8`, team 1 at `0x1D4A81E8`; `roster[0]` = team flag.
- **Slot → record.** `slot = P+0x18`; `e = roster[4 + 4·(slot % 11)]` (`FUN_140A3AEE0`/`FUN_140A3AF90`); record = `roster + e·0x128 + 0xA8`.
- **Base.** `local_1B8 = FUN_141B59E20(record) + 8 = record + 0x38`, so **`base[id] = roster + e·0x128 + 0xE0 + id`**. Speed: `roster + e·0x128 + 0x107`.
- **Multipliers** (ids `< 0x3C`). Per-player float arrays in `P`: A1 `+0xC798`, A2 `+0xC888`, A3 `+0xC978`, A4 `+0xCA68`, A5 `+0xCB58`, entry `4·id`. Product `f = A2·A1·A3·A4·A5`.
- **Value.** `v = (u8) trunc( (float) base[id] · f )`.
- **Penalty.** None for id `0x27` (breath penalty only for `{0x1A,0x1B,0x1C,0x2A}`).
- **Clamp.** Shared table: `lo = byte[0x142613C60 + 2·id]`, `hi = byte[0x142613C61 + 2·id]` (for id 0x27: 40 and 99). `out = (v ≤ hi) ? max(v, lo) : hi`.

### 1.2 Output block (CONFIRMED address; live OPEN)

`out_block = FUN_1408C7210(FUN_140A3E8A0(*(P+0x7E0) + 0xE0), t, e) + 0x70`, with `FUN_1408C7210(X, t, e) = X + e·0x2134 + t·0x59650 + 0x64D8` and `FUN_140A3E8A0(x) = x + 0x750D0`. Speed output = `out_block + 0x27`. Not verified live: the per-player object for `P` did not verify (`verifymgr.py`, 4 of 22).

### 1.3 Consumers (OBS, `pes-re-anim-select-2.md` §1.2)

- `FUN_1406F9A2C` rate cap: `q = clamp((S − 20 − 20·fatigue)/79, 0, 1)`.
- `FUN_14096D2D0`, `FUN_14096E284`: speed differential of two players.
- `FUN_14211CBEF`: `(S − 40)/59` term.

## 2. Band speed (round 10)

### 2.1 The band reader (CONFIRMED)

- The movestep callers `FUN_1406D2030` (lines 62 and 83 of its decompile) and `FUN_1406C1070` (line 31) read a band through the **anime-interface dispatch object** (vptr `0x1425E35B0`; slot `+0x38` = `0x1406B9E50`, CONFIRMED from the image). Call form: `(**(code**)(*param_1 + 0x38))(param_1, req[0x1F])`, used when `req[0x1C] != 0`.
- `0x1406B9E50` (no Ghidra function header; it is a leaf in the `FUN_1406B9D80` region):
  - `cmp edx, 3; ja → 0` (band > 3 gives 0);
  - `if edx == 0 && (r8d == 3 || r8d == 6) → 0` (kind 3 or 6 at band 0 gives 0);
  - else `return *(float*)(0x1425DDE10 + 4·edx)` (`lea rcx,[rip+0x1F23FA1]` at `0x1406B9E68`, target `0x1425DDE10`).
- **Band table at `0x1425DDE10`** (script-read): `[0.15, 0.3, 0.6, 1.0]` for bands 0..3 (VAs `0x1425DDE10`, `…14`, `…18`, `…1C`). These are dimensionless rate fractions.
- The result feeds `req[0x4A]` (the parametric rate, a float in the request record, per `pes-re-a3helpers.md`). The caller's `int iVar8` is a float-bit pass-through.

**So the band value is a rate fraction, not a km/h speed.** GF's `BandSpeedKmh(band, g)` returning `CurrentSpeedKmh()` is therefore wrong in both unit and quantity.

### 2.2 The 11-knot speed→rate table (CONFIRMED)

- `FUN_1406B9E80(P, speed_kmh, kind)` (105 lines; callers `FUN_1406B9CEF`, `FUN_1406C10F5`, `FUN_1406C3781`, `FUN_1406CC1D3`, `FUN_1406D211F`, `FUN_1406D219C` — each passes `req[10]` or `req[0x1D]` as the km/h input).
- `p = speed_kmh · DAT_1425C48A8 (= 1/3.6 = 0.27777779)`, then `p /= FUN_141AF3180(P+0xA08)` (the physique scale G).
- Table at `0x1425DE010 … 0x1425DE0A8` as 16-byte blocks `[x_k, x_{k−1}, y_k, y_{k−1}]` (script-read). Knots `(x m/s, y rate)`:

| knot | x (m/s) | y (rate) |
|---|---|---|
| 0 | 1.15348 | 0.150 |
| 1 | 3.056 | 0.295 |
| 2 | 3.85 | 0.484 |
| 3 | 4.3 | 0.555 |
| 4 | 5.0 | 0.660 |
| 5 | 5.8 | 0.750 |
| 6 | 6.5 | 0.820 |
| 7 | 7.1 | 0.871 |
| 8 | 7.3 | 0.887 |
| 9 | 8.3 | 0.960 |
| 10 | 9.16 | 1.000 |

- **Interpolation (CONFIRMED from the decompile):** find the first segment `k` with `x_{k−1} ≤ p`; `t = (p − x_{k−1}) / (x_k − x_{k−1})`, `t` clamped to ≤ 1; `rate = y_{k−1} + (y_k − y_{k−1})·t`. Special case: segment 9 with kind 3 or 6 multiplies `t` by `_DAT_1425DDF5C = 0.295`. Result clamped to ≤ 1.0 and ≥ 0.0.
- So the table is the **speed→rate** map (1.0 ↔ 9.16 m/s ÷ G = 33 km/h at G = 1). Its inverse gives a km/h for a rate fraction, but no code reads it that way (INF).

### 2.3 Live check (OPEN)

Not done. The band value is read from the request record `req[0x1F]`, which I did not locate in memory. The speed→rate table needs the `P+0xA08` physique scale. Neither could be checked against a running player's speed within this round.

## 3. Roster base row (round 10): a fill

### 3.1 Live layout (CONFIRMED)

Read live in play (`roster + e·0x128`, all 22 slots, both teams). The ability area `0x14–0x2D` of each record holds **one value repeated for every id**. Index 18 (`0x26`) is `01` or `03` (the conditioning byte from `pes-re-stamina-live.md`); the rest is the single fill value.

| record (team, slot, e) | fill value (hex, dec) |
|---|---|
| (0,0,11) | 4d, 77 |
| (0,1,14) | 4d, 77 |
| (0,2,2) | 4d, 77 |
| (0,3,1) | 52, 82 |
| (0,4,7) | 52, 82 |
| (0,5,6) | 4d, 77 |
| (0,6,8) | 63, 99 |
| (0,7,4) | 5d, 93 |
| (0,8,21) | 4d, 77 |
| (0,9,18) | 4d, 77 |
| (0,10,22) | 4d, 77 |
| (1,0,0) | 4d, 77 |
| (1,1,16) | 4d, 77 |
| (1,2,18) | 4d, 77 |
| (1,3,22) | 4d, 77 |
| (1,4,5) | 4d, 77 |
| (1,5,11) | 52, 82 |
| (1,6,6) | 58, 88 |
| (1,7,3) | 52, 82 |
| (1,8,19) | 52, 82 |
| (1,9,8) | 58, 88 |
| (1,10,10) | 63, 99 |

Values are `77 + 5k` for k = 0..4 (77, 82, 88, 93, 99): a stock ladder, one step per player class.

### 3.2 Verdict (INF high)

The base row is a **per-player fill with one stock value**, not a per-stat DB row. Speed, Accel, Jump and Stamina cannot differ from one another in this row. So the base values (the inputs of §1.1) are not the player's own ability bytes. The loader is not located (OPEN). The fill is written before kick-off, since the row is populated at that time per `pes-re-stamina-live.md` §2.3.

## 4. Open

1. The loader of the roster row (§3). The fill path or the DB record source is not located.
2. The live check of the band value against a running player's speed (§2.3).
3. The per-player output block for the live Speed check (§1.2).
