# Speed inputs for clip selection (S1Watch, round 9)

> **Main audit 19-10-26:** clamp bytes `0x142613CAE`=40, `0x142613CAF`=99; vtable `0x1425E3508`+0x60 = `0x1406E7EB0` (clip duration, not a band speed): match. PARTIAL: the base-row writer and the band-speed source (11-knot table at `0x1425DE010`, INF) are open. Supersedes the earlier "vt+0x60 = FUN_1406B9B20" reading in the anim-select docs.

Tags: **CONFIRMED** = decompile or live read with VA; **INF** = inference; **OPEN** = unresolved. Constants script-read from `pes_full.bin` (offset = VA − 0x140000000). Operand VAs are Ghidra labels.

## Summary

1. **Speed byte (id 0x27): writer and formula CONFIRMED.** `Speed = clamp((u8) trunc(base[0x27] · A2·A1·A3·A4·A5), 40, 99)`, written by `FUN_14092AF10`.
2. **Base source (roster record byte): address CONFIRMED, writer OPEN.** `base[id] = roster_t + e·0x128 + 0xE0 + id`. The live bytes are not per-id ability values (§3).
3. **Live output check: OPEN.** The per-player object that holds the A arrays was not located live.
4. **BandSpeedKmh = AnimePlayer secondary vtable `+0x60`: NOT a band speed (CONFIRMED).** It returns the clip duration. The real band-speed source is the exe speed table (§4), which is INF on the stand-in mapping.

## 1. Speed byte (stat id 0x27)

### 1.1 Writer and formula (CONFIRMED)

`FUN_14092AF10(P, Q)` (519 lines; caller `FUN_1409255F0`, reached through a vtable, no direct E8 caller) rebuilds the ability bytes for one player per tick.

- **Roster.** `roster_t = *(G + (0xA3 + t)·0x28)` (`FUN_140A489D0`, t = team; `0x1978` for team 0, `0x19A0` for team 1). Live: team 0 at `0x1D48A0D8`, team 1 at `0x1D4A81E8`; `roster[0]` = team flag (0 / 1).
- **Slot → record.** `slot = P+0x18` (`FUN_140A3AEE0`/`FUN_140A3AF90`): `e = roster[4 + 4·(slot % 11)]`, record = `roster + e·0x128 + 0xA8`.
- **Base.** `local_1B8 = FUN_141B59E20(record) + 8 = record + 0x38`, so **`base[id] = roster + e·0x128 + 0xE0 + id`**. Speed: `roster + e·0x128 + 0x107`.
- **Multipliers** (ids `< 0x3C`). Per-player float arrays in `P`: A1 `+0xC798`, A2 `+0xC888`, A3 `+0xC978`, A4 `+0xCA68`, A5 `+0xCB58`, entry `4·id` (Speed: `+4·39`). Product `f = A2·A1·A3·A4·A5`. A1 and A3 are 1.0 in the live docs; A2 has a few varying slots; A4 is 1.0 except super-sub pairs; A5 is the fatigue drain (`pes-re-stamina-live.md` §10.4).
- **Value.** `v = (u8) trunc( (float) base[id] · f )` (the `(longlong)` cast then `byte`).
- **Penalty.** None for id `0x27`. The breath penalty `p` is applied only to `{0x1A,0x1B,0x1C,0x2A}` (decompile condition `(id − 0x1A) < 3 || id == 0x2A`, `local_1c7 != 0`).
- **Clamp.** `FUN_1408CAF60(id)`: `lo = byte[0x142613C60 + 2·id]`, `hi = byte[0x142613C61 + 2·id]`. For Speed: `lo = byte[0x142613CAE] = 40`, `hi = byte[0x142613CAF] = 99` (script-read). `out = (v ≤ hi) ? max(v, lo) : hi`. Stored only when changed.

### 1.2 Output block (CONFIRMED address, live OPEN)

`out_block = FUN_1408C7210(FUN_140A3E8A0(*(P+0x7E0) + 0xE0), t, e) + 0x70`, with `FUN_1408C7210(X, t, e) = X + e·0x2134 + t·0x59650 + 0x64D8` and `FUN_140A3E8A0(x) = x + 0x750D0`. Speed output = `out_block + 0x27`. The live verification (`mgr_ok.json`, `mgr_res.json`) gave a candidate that passed the `[40,99]` test for only 4 of 22 players (`verifymgr.py`), so this could not be confirmed.

### 1.3 Consumers (OBS, `pes-re-anim-select-2.md` §1.2)

- `FUN_1406F9A2C`: `q = clamp((S − 20 − 20·fatigue)/79, 0, 1)`, `rateMax = 1 − 0.085·u(G) − 0.1·(1 − q²)`.
- `FUN_14096D2D0` and `FUN_14096E284`: speed differential of two players.
- `FUN_14211CBEF`: `(S − 40)/59` term in an athleticism blend.

### 1.4 Source of the base (OPEN)

The roster block is documented as static metadata (`pes-re-stamina-live.md` line 783). Its writer (the load of Player.bin-style ability bytes into `roster + e·0x128`) was not located.

## 2. Live check of the base block (anomaly)

`speedrec1.json` (90 s in play, 22 players, roster read at start):

- Speed, Acceleration, Jump and Stamina read at `+0xE0 + 0x27`, `+0x2B`, `+0x2C`, `+0x2D` are **equal for every player** (e.g. 77/77/77/77).
- The raw record (team 0, `e = 11`, `0xF4–0x10F`) is a uniform run of `0x4D` (77) across the ability ids, and `0x63` (99) at `0x0E–0x27`.
- So the values at `+0xE0+id` are **not** per-id ability bytes in this record. Either the block is filled with a per-player constant, or the ability bytes live at another offset. The per-player values (77, 82, 88, 93, 99) do vary between players.

**Conclusion:** the base-byte address (§1.1) is the one the writer reads, but in this match its content does not look like Speed/Accel/Jump/Stamina. The live base is **not validated** and the Speed formula cannot be checked against it yet.

## 3. Live speed versus base (inconclusive)

The 90 s recording (filtered for teleports, step > 0.5 m/frame dropped) gives p95 speeds of 0–20 km/h per player, with median ≈ 0 for most players. Too few clean 12-frame windows (53–209 per player) and many players still for long periods. The base values (77–99) do not order these speeds. **No conclusion** on Speed-vs-speed is possible from this recording.

## 4. BandSpeedKmh (AnimePlayer secondary vtable `+0x60`)

- **Vtable.** `AnimePlayer` secondary (`+0xAB8`), vtable `0x1425E3508` (RTTI per `pes-re-animselect.md`). Slot `+0x60` = `0x1406E7EB0` (CONFIRMED, image read).
- **Function.** `FUN_1406E7EB0(this, clip, divisor)`: `if clip < 0x1125: return ((float) FUN_1406AF620(FUN_1406B0ED0(), clip, 1.0) / fps) / divisor; else 0`. Here `fps = FUN_141E532D0() = 54`. So vt+0x60 returns the **clip duration in seconds** divided by `divisor`. It is not a band speed.
- **Docs conflict.** `pes-re-anim-select.md` (table §2, row `[8]/[0xc]/[0xd]`) says vt+0x60 = `FUN_1406B9B20(angle,10)` (the 10-way angle bucket). That is inconsistent with the image: `FUN_1406B9B20` is not at this vtable slot (`FUN_1406B9B20` is an angle-range lookup over `DAT_1425DDEE0` / `DAT_1425DDEB0`). The angle-bucket attribution needs a separate check (**OPEN**).
- **GF stand-in.** `src/sim/onthepitch/pesmovestep.cpp:318`: `BandSpeedKmh(band, g) = CurrentSpeedKmh()`, with a comment naming vt+0x60. That comment is wrong per §4 above. The consumers are `movestep_run.cpp` lines 287, 804, 1589, 1804 (`kSpeedBand`, `kDesiredSpeedKmh`, × `speed3600`) and `movestep_step.cpp` lines 1145, 1384.
- **Candidate band-speed source (INF, med).** `pes-re-anim-select.md` §2: steady running uses an exe speed table with an 11-knot piecewise-linear curve at `0x1425DE010 … 0x1425DE0AC` (`rate = PL((km/h / 3.6) / G)`, `G` = the AnimMovePlugin scale). Knots: `3.056 m/s ↔ 0.295`, `3.85 ↔ 0.484`, `7.3 ↔ 0.887`, `9.16 ↔ 1.0`. The band speed in km/h would be the inverse of this table at the band's rate, but the mapping from `band` to a knot was not decoded (**OPEN**).
- **Speed class table (OBS).** `FUN_1406DEB30` buckets km/h against `{2.5, 8.0, 13.5, 22.0}` (table at `0x14344E250`): 0 still, 1 walk, 2 jog, 3 run, 4 dash (`pes-re-animselect.md` line 245).

## 5. Open

1. The writer of the roster record bytes (the `+0xE0` block) and the reason its ability ids read uniform (§2).
2. The per-player object for the live output check (§1.2).
3. The vt+0x60 angle-bucket attribution conflict (§4).
4. The band → km/h mapping (§4 candidate).
