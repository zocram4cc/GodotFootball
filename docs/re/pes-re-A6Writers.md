# A6Writers: PES period-clock writers, time-up post-gate, breaks, match speed, HUD

Scope: decode of the open A6 items in `pes-re-A6Period.md` §7. Read-only: Ghidra bridge (PESLive / pes_full.bin, base 0x140000000), install `.rdata` read from `<workdrive>/PES21/PES2021.exe`, live reads via `/proc/2510857/mem` under `sudo` (no writes, no pins; `MI+0x1168/0x116c/0x1574` never touched). Scratch: `<workdrive>/gf-A6Writers-scratch/`.

Tags: **CONFIRMED** (decompile, asm, install data, or live read with checksum/OCR), **INF(conf)**, **OPEN** (what would settle it).

Naming: `DRV` = driver object of `FUN_140445E90`/`FUN_14044CE80`; `rec_ = DRV+0x2790` (period record copy, same layout as `MI+0x1168`); `MI = *(G+0x270)`; `E = *(G+0x2c0)`; `MB` = match brain (dispatcher `FUN_140A0D190` param_1, event ring at `MB+0xBD08`); `S = (MB+0xBD08)+0x180C60` = the MB-E event block (A2 naming). Record layout (`+0` period, `+4` state, `+8` LIMIT, `+0xC` F, `+0x10` FNV checksum u64, `+0x18` key, `+0x1C` idx, `+0x20..` ring) is CONFIRMED by `FUN_140A3C070`/`FUN_140A3CBC0`.

## 0. Corrections to earlier reports

| Earlier claim | Now |
|---|---|
| A6Period §3 INF: "stamp base (~7.2 M) does not track the HUD clock" | Refuted: `HUD minute = (decode+F)/3600 − 2000` (§7). |
| clock-live §5/§6: `MI+0x118c/0x1190/0x1194` are wall-µs stamps; `MI+0x1180` layout dead | The `118c/1190/1194/1168/116c/1170/1174/1178/1180/1184` writers in the dispatcher act on `S` (MB-E), not `MI` (§4). |
| A6Period §4/§7.2: LIMIT/F writer not traced | Traced: `FUN_14044CE80` (§1). |
| A6Period §7.1: ring rotation/key policy open | Traced (§2). |

## 1. Who writes LIMIT (`MI+0x1170`) and F (`MI+0x1174`), and when

| Write | Site (VA) | Verdict |
|---|---|---|
| Init: LIMIT, F, state, period zeroed, ring encoded as stamp 0 | `FUN_140A3C560` (record reset; calls `FUN_140A3CBC0`) | CONFIRMED (asm: `+0x1170` zeroed as u64) |
| Per-tick driver (the only per-frame writer) | `FUN_14044CE80` (DRV) | CONFIRMED |
| LIMIT setter | `FUN_140A3CBB0` (`rec+8`), callers `0x14044CF78`, `0x14044D0DC` | CONFIRMED |
| F setter | `FUN_140A3CD70` (`rec+0xC`), caller `0x14044D0B9` | CONFIRMED |
| Publish of DRV copy into the match record, end of tick | `FUN_140445E90` tail: `FUN_140443390(*(DRV+0x17858), DRV+0x2790)` | CONFIRMED (call); INF(high) that the destination is `MI+0x1168` (live: the stamp in `MI` advances by the driver's rule) |

Driver logic (`FUN_14044CE80`, CONFIRMED from decompile):

1. `sVar6 = FUN_140A6BA90()` (added-time code, §1.1). If `sVar6 != 0`: `LIMIT += (100·E60·E5C/M) · sVar6` (`FUN_140A6BFC0`), then clamp: `if LIMIT > 21600.0 → LIMIT = 21600.0` (`FUN_140A6BFA0`; `21600.0` at `.rdata 0x142647C60`, install-read).
2. Gate `FUN_140A6BF50(MI)`: returns 1 when `MI.116c` is 1 or 3, or 2 with `MI.11d4 != 0`, or 2 with `MI.11d4 == 0` and `MI.11b0 == 0x16` and not `FUN_140A866E0(...)`; else 0. CONFIRMED.
3. If gate and `mode == 1` (DRV+0x2794): **stamp advances** `FUN_140A6C050(rec_, E)` = decode + `100·E60·E5C/E58`, re-encode (`FUN_140A3CBC0`). CONFIRMED.
4. If gate and `mode ∈ {2,3}` and `rec_.period ∈ {1,3,5,7}`: **the 0.5-per-frame stoppage writer**: `F += 0.5` (`0.5 = DAT_142565600`; the code computes `54·0.5/54`, exact), and `LIMIT += 0.5` with the same clamp. CONFIRMED (code); live F +0.5/frame (A6Period §3). The stamp is frozen in this branch. CONFIRMED.
5. Period 2/4/6/8 are outside step 4's period list, so F and LIMIT do not move there. CONFIRMED.

Live (read-only, 473 valid samples, 9 s, state 1, period 1): F = 178845.5 constant; LIMIT = 21600 (clamped); stamp +10.0 per frame over 280 frames (`2800/280`). CONFIRMED. F is the accumulated stoppage time of this match (INF(high)): it grows only in stoppage.

### 1.1 Added-time code `FUN_140A6BA90` (CONFIRMED structure; case meanings INF)

- Base code from `MI.1574`/`MI.1554`/`MI.11b0 == 0x13`/`MI.1378` (values 1, 3, 5), and from the two-team flags `FUN_140A3F350` (`MI.11bd`). Near-end test: `LIMIT <= 21600 && clock > 0` and `clock < 162000` (or 54000 for periods 5/7), with `MI.1168` in {1,3,5,7}.
- Period scale: ×20 for periods 1, 5, 7; ×35 for period 3; ×1 otherwise.
- Return: `floor(54 · code · scale / 60)` (ushort). The `LIMIT` increment per tick is therefore `(100·E60·E5C/E58) · floor(54·code·scale/60)`.
- Live: `E58 = 10.0`, `E5C = 1.0`, `E60 = 1.0` (see §6), so the increment is `10 · code·scale·0.9` per tick. LIMIT is always at the 21600 cap live (INF(high): the cap is reached in any match with added-time codes).

### 1.2 Ring encode and key

See §2 (the ring encoder is the same function that the stamp writer calls).

## 2. Ring rotation and key policy (`FUN_140A3CBC0`, `FUN_140A3C070`)

- Encoder `FUN_140A3CBC0(rec, stamp)`: checksum `rec+0x10` = FNV-1a 64 over the four stamp bytes (`0xcbf29ce484222325`, prime `0x100000001b3`). Encoded byte `L_i = ((b_i ^ (63·K_i)) − K_i) mod 256` with `K_i = byte i of (key·3 + 4i)`; inverse of the decoder. CONFIRMED (decompile + asm match of `FUN_140A3C070`).
- Key: the encoder reads `rec+0x18` (`MI+0x1180`) and **never writes it**. The key's origin at match start is OPEN (not traced). Live: constant at `2280871786` across all 473 samples of this match (CONFIRMED for one match).
- Ring slot: `idx = FNV-1a 32 of the four low-dword bytes of _DAT_7FFE0008`, then `·0x1b3` (32-bit). `_DAT_7FFE0008` is the KUSER_SHARED_DATA InterruptTime low dword, i.e. the wall clock. `idx` is stored at `rec+0x1C`; the slot is `rec+0x20 + 4·(idx & 3)`. CONFIRMED (decompile). Live: `idx & 3` cycles through 0, 1, 2, 3 (CONFIRMED).
- Consequence: the rotation is wall-clock driven and not reproducible. The decoder reads `slot[idx & 3]` and `idx` is stored with each write, so **any** slot policy round-trips and the decoded clock is unchanged (CONFIRMED by construction: `FUN_140A3C070` reads the slot the encoder wrote). The port may use a deterministic rotation.
- Stamp advance: `FUN_140A6C050` = decode + inc → `FUN_140A3CBC0` (CONFIRMED).

## 3. `FUN_14050B9B0` after the gate

Entry: `R` = Judge object (caller `FUN_14050C3E0` at `0x14050C425`); `MS = *(R+0x3b8)` match state; `B = G+0x40` ball object; `cfg = *(G+0x2aa8)` float table; rate `= FUN_141E532E0() = DAT_14351AE50 = 54` (CONFIRMED live: 54 ticks per game-second). The gate is `FUN_140A6BEF0(clock, LIMIT, flag)`, `flag = period ∈ {5,7}` (A6Period §2; CONFIRMED).

Post-gate order (CONFIRMED structure, branch semantics as labelled):

1. **Penalty hold** (`G+0x2920 == 4` and `MS+0x251f8|0x251f9 != 0`): if the value from `FUN_140A6C6D0(...)` / `(DAT_14259BDD0 / speed)` exceeds `DAT_14256AE18 = 0.01`, return false. INF(med): penalty-taker hold.
2. `R+8 != 0` → return false. OPEN (flag never set by the code above; writer not traced).
3. **60-second counter**: `R+0xd0 += 1`; if `R+0xd0 > rate · 60` (= 3240 ticks = 60 game-seconds at 54 Hz) → return true. CONFIRMED (arithmetic). Persistence (CONFIRMED): the counter runs only while the gate is open and `R+8 == 0`; it is **not** reset when the gate closes; it is reset (`R+0xd0 = R+0xd8 = 0`, `FUN_14050C3E0`) only after a true return or `R+0xdc != 0`. So the 60 s is "gate-open ticks since the last whistle", not wall-clock since the gate.
4. **Ball-zone counter** `R+0xd8`: `cVar6 = team attack sign` (`FUN_140A48A30(G, side)+0x254`). When `sign · FUN_140A57800(B) < cfg[0] / 3.0` (`3.0 = DAT_14258C264`), `R+0xd8 += sign`; a sign change resets it to 0; if `R+0xd8 > rate · 4` (216 ticks = 4 s) → return true. CONFIRMED arithmetic; INF(med) that it is the "ball held in the attacking zone" test.
5. **Event timeouts** (`piVar19 = R+0x3c0` = last event record: `[0]` kind, `[1]` pid, `[4]` frame stamp, `[8]` flag): kind 4 with `R+0xd0 > rate·5` (5 s) / `rate·10` (10 s) → return true; kind 0 tests (ball speed `|B+0x2948 x,z|` against `cfg`, frames since event `MI.1198 − piVar19[4]` against `30·rate`) → return true. CONFIRMED arithmetic; semantics OPEN.
6. Final block (`FUN_140A57800(B)`, `FUN_140A60530`, `FUN_140A62EE0` player/team indices `< 0x16`, team split `10 < idx`): return true on the out-of-zone pattern. OPEN.

Consequence (CONFIRMED from the code): **PES does not whistle on the gate alone.** The gate is a precondition; the whistle needs one of steps 3–6. GF whistles on the gate (A6Period §4), which is wrong.

## 4. The half-time writer `FUN_140A115A0` and its 116c transitions

- Dispatch (CONFIRMED): `FUN_140A0D190` → `FUN_140A13060(MB, ctx)` → for each event: `FUN_140A115A0(MB, S, event, ctx)` with `S = (MB+0xBD08)+0x180C60`; then `FUN_140A11290(MB, S, ctx)`.
- **`S` is not `MI`** (CONFIRMED by consistency, three independent checks):
  1. The same functions write `S+0x1178`, `S+0x1180`, `S+0x1184` as counters (`+1`, `+1`, float add). `MI+0x1178` is the FNV checksum of the period record; it verifies on every live sample (473/473, and 50/51 in the earlier probe). A counter there would break the checksum and trap (`FUN_1415A1714`).
  2. The function reads the frame count from `MI+0x1198` explicitly (`local_188 = MI`), not from its own `param_2`.
  3. Live: `MI − 0x180C60` has no ring pointer at `+0xBD08` (0 in 35 samples), so `S` is a separate object.
- **Case 7** (kind 7, subkind `param_3[5] ∈ 1..5`, `param_3[0xc] == 0`, valid pid `< 0x16`): `S+0x116c += MI.frame − saved_frame`; `S+0x1168 += 1` (if `!= -1`); `S+0x728 = S.116c / S.1168` (mean). This is a per-restart statistic. INF(high). It does **not** write `MI+0x116c` or `MI+0x1168`.
- `FUN_140A11290` (same dispatcher): `S+0x1170 += 1`, `S+0x1174 += S+0x11ac` (int), `S+0x72c = mean`; `S+0x1178`, `S+0x730..0x738` counters. Also statistics, not F or LIMIT (correcting the A6Period F hypothesis).
- **The play state `MI+0x116c`** is written through the record-state setter `FUN_140A3CD90` (`rec+4`, the driver copy). Callers (xref census): `FUN_1404491D0`, `FUN_140453890`, `FUN_14044B8A0`, `FUN_14044BB40`, `FUN_1404A6D20`, `FUN_1404A6EC0`, `FUN_1404A56C0`, `FUN_140453530`, `FUN_1404514F0`, `FUN_1404A62B0`, `FUN_140454570`, `FUN_140454C00`, `FUN_1404A42E0`, `FUN_1404A5DE0`, `FUN_14050E770`, `FUN_1404A3F50`. The per-restart rule (which value on which event) is OPEN; it is in the sequencer, not the half-time writer.

## 5. Breaks (periods 2, 4, 6, 8)

- Period writer (CONFIRMED): `FUN_1404494E0` (match driver), under `MC+0x49C0 == 7`, sets `rec_.period` via `FUN_14102CAD0(MC+0x2790, next)`:
  - from 1: `next = 2` (fixed);
  - from 3: `next = FUN_1404493D0(MC)` ∈ {4, 8, 10};
  - from 5: `next = 6` (fixed);
  - from 7: `next = FUN_1404499D0(MC)` ∈ {8, 10}.
- `FUN_1404493D0`: `1` if no state; tie test on team tallies (`0x6a`, `0x6b` of the two teams); if tied: flag `0x66` → `FUN_140443E30` and return 4; flag `0x67 == 0` → 10; else 8; not tied → 10.
- `FUN_1404499D0`: tie test (second tally pair); if tied (with flag `0x69` doubling) → 8 if flag `0x67`, else 10; not tied → 10. CONFIRMED (code); INF(med) meanings: 2 = half-time break, 4 = break before extra time, 6 = extra-time break, 8 = break before shoot-out, 9 = shoot-out (cutscene doc), 10 = full time.
- `MC+0x49C0` is the sequencer's phase copy. `FUN_1404491D0`: if `MI.1554 == 7`, it sets `MC+0x49C0 = 1` and copies `MI.1558` / `MI.155c` into it (CONFIRMED). So the "period end" switch runs when the phase is 7.
- Breaks and the clock: F and LIMIT do not move in break periods (§1 step 5). The stamp advances in `mode == 1` for any period (CONFIRMED: no period test in the stamp branch), so whether the break mode is 2 or 3 (no stamp) is OPEN.
- The 2→3, 6→7, 8→9 writers (the break-end transitions) are not among the `FUN_14102CAD0` callers I read; OPEN (candidate: the `MI.1554/1558` phase block).

## 6. Match-speed setting → units per frame

- Per-tick stamp increment (CONFIRMED formula, `FUN_140A6C050` and `FUN_140A6BFC0`):
  `inc = 90 · E60 · E5C · (60 / rate) / E58 = 100 · E60 · E5C / E58`,
  with `90.0 = DAT_14259BE5C`, `60.0 = DAT_14259A764` (install-read), `rate = 54` (`FUN_141E532D0`).
- Live (read-only): `E58 = 10.0`, `E5C = 1.0`, `E60 = 1.0` → **10.0 units per frame** (CONFIRMED; `2800/280`).
- `E58` = the match-time setting as a float (`settings+0xd5a`, A6Period/clock-record §3): INF(high). Only the 10 setting has been observed live.
- Interpretation (INF(high)): a 90-game-minute match (2 halves of 162000 units) takes `3240 · M` ticks at 54 Hz, i.e. `M` wall minutes, with `M = E58`. So the units-per-frame map is `100 / M`. At M = 10 that gives the observed 10. Other M values are OPEN until a match at another setting is read.
- Stoppage rate: 0.5 units per frame regardless of M (CONFIRMED code).
- Game-time per tick at M = 10: `(10/60) / 1` game-seconds per tick, i.e. 9 game-seconds per wall second at 54 Hz (matches the live HUD rate 9.4–9.5).

## 7. HUD clock source

- Live + OCR (CONFIRMED for period 1, state 1; four captures):
  | HUD (OCR) | capture mid (monotonic) | decode+F, min | minus HUD |
  |---|---|---|---|
  | 749:15 | 596170.32 | 2749.313 | 2000.063 |
  | 749:40 | 596172.96 | 2749.732 | 2000.066 |
  | 750:00 | 596175.64 | 2750.154 | 2000.154 |
  | 750:32 | 596178.33 | 2750.582 | 2000.049 |
  Result: `HUD_minute:second = (stamp + F)/3600 − 2000.0` (residual ±0.15 min incl. OCR; capture-midpoint error ≈ 0.02 min). The HUD also ran at the same rate: 2.66 wall-s gave 25 HUD-s. A frame at 744:56 was checked visually (`now_rgb.png`). The other three frames were read by OCR only; not visually checked.
- The HUD path `FUN_1420EFEC0` (reads `E+0xa0`/`E+0x11c`) cannot be the source: live `E+0xa0 = E+0x11c = 0.0` while `E+0xa4 = 117440535 ≠ 0` (CONFIRMED live), which that path would turn into minute 0.
- `FUN_140A3C180` (decode + F → `FUN_140A6BD00` minute and second, `+45`, `+90`, `+105` by period 3/5/7) is the same decode and the nearest code path, but it returns 2749 for this clock, not 749. The display subtracts 2000 minutes somewhere (CONFIRMED by the measurement; the code site is OPEN).
- OPEN: the origin of the 2000-minute base. Three readings fit: a constant base (HUD = clock − 2000); a modulo-2000 wrap (`clock mod 2000`, indistinguishable until clock > 4000 min); or a kickoff origin set when the match started. The endless tool (`tools/dev/pes_endless_match.py`) writes only the two gate constants (CONFIRMED), so it does not set the origin. A kickoff sample in a fresh match would settle it (not possible in this session: the endless patch is active and no new match was started).
- Stock-consistency note (INF(med)): the stock gate compares the absolute clock with 162000, so in stock the origin at kickoff must be near 0. The 2000-minute base therefore cannot be the stock kickoff origin, unless the stamp was not reset when the match started. Unresolved.

## 8. Live status and limits

- `MI` is non-null in about 4–9% of polls, at 2 ms cadence. The endless patch keeps period 1 and state 1–3 cycling. Half-time and break transitions cannot be observed live. Some frames show the goal replay (no HUD).
- Reads: `sudo -n python3` on `/proc/2510857/mem` (plain `os.open` fails: `EACCES`). Scripts: `probe_clock.py`, `probe_frame.py`, `sampler.py` (samples in `samples.jsonl`). Frames: `hud_0..3.png` (OBS, `magick -colorspace RGB`), `now_rgb.png`.
- Static: Ghidra bridge decompiles (`dec_*.txt`); xrefs `xrefs_to?address=`; displacement census `finddisp.sh` (scalar match; misses writes through a record base at small offsets such as `+0xC` and `+0x8`, which is why the record writers were found through the setters instead).

## 9. GF map and port order

| PES (VA) | GF today | What the port changes |
|---|---|---|
| Stamp advance `FUN_140A6C050` (`100·E60·E5C/E58` per tick, mode 1) | `PesClock::Advance` play branch adds 60 units per game-second | Use `100/M` per tick (`M` = match-time setting, read from the install/settings, not a PES number table). The GF rate is 1× game time; PES at M = 10 runs 9× game time. |
| Stoppage F `+0.5`/tick (mode 2/3, periods 1/3/5/7) | GF F `+0.5` | Keep; also `LIMIT += 0.5` the same tick, clamp at 21600 (`DAT_142647C60`, install-read). |
| LIMIT codes `FUN_140A6BA90` | GF `AddedTimeUnits` tier approximation | Replace with the code table; approximation labelled `// ponytail: tier table until FUN_140A6BA90 states are ported`. |
| Time-up post-gate `FUN_14050B9B0` (60 s, 4 s zone, event timeouts) | GF whistles on the gate alone | Whistle = gate AND one of the post-gate branches. The 60 s counter persists across gate closure and resets only on whistle. |
| Period writers `FUN_1404494E0`/`FUN_1404493D0`/`FUN_1404499D0` (2, 4, 6, 8, 9, 10) | GF has no breaks (walk-off is GF's set piece) | Add the period table; `PeriodHasClock` stays (1, 3, 5, 7). |
| Record encode `FUN_140A3CBC0` (ring by wall clock) | GF rotates `idx` by one per store | Keep GF's deterministic rotation; document that PES's slot is wall-clock driven and the decoded clock is unchanged. |
| HUD `(stamp+F)/3600 − 2000` | GF shows its own elapsed seconds | Port the HUD only after the origin is settled (§7). |
| MB-E event stats (`S`) | not ported | Not needed for the clock (statistics only). |

Port order:
1. Clock arithmetic (§1, §6): per-tick increment `100·E60·E5C/M`; stoppage F and LIMIT `+0.5`/tick; LIMIT clamp. Tests from the formulas, constants read from the install (`0x14259BE5C`, `0x14259A764`, `0x142565600`, `0x142647C60`, `0x142598590`).
2. Period table (§5): 1→2, 3→{4, 8, 10}, 5→6, 7→{8, 10}; 9 shoot-out; 10 full time; `PeriodHasClock` for 1/3/5/7.
3. Time-up post-gate (§3): 60 s counter with the persistence rule; 4 s ball-zone counter; the gate-only whistle removed. The event-timeout and final branches need the OPEN items below first.
4. LIMIT codes (§1.1).
5. HUD origin (§7) once settled.

## 10. Open items (what would settle them)

1. Ball-state and event branches in §3 steps 2 and 5–6 (`R+0x3c0` event record fields, `R+8` writer, `FUN_140A57800/57450/57740`, `FUN_140A3E190`): read `FUN_140A3E190`, the `R+8` writer (xref), and one live whistle outside the endless patch.
2. Origin of `MI+0x1180` (key) and `MI+0x1184` at match start: xrefs of `FUN_140A3C560` callers (`FUN_1404494E0`, `FUN_14044BB40`) and the match creation path.
3. Mode `DRV+0x2794` (1 play, 2/3 stoppage): the writer `FUN_141194330` (`0x1411944CF`) is the only disp-`0x2794` writer; confirm its object and source (likely `MI.116c`).
4. Per-restart `116c` rule: the sequencer callers of `FUN_140A3CD90` (§4); live check on stoppage transitions (state 1 → 2 → 3 → 1).
5. Break transitions 2→3, 6→7, 8→9 and break duration: the phase block `MI.1554/1558` writers; needs a match that reaches half-time (not possible under the endless patch; static only).
6. Match-time setting map beyond M = 10 (`settings+0xd5a` → `E+0x58`; `E+0x60` writer): one match at another setting.
7. HUD origin (2000 min): a kickoff sample in a fresh match, or the `mod` test at clock > 4000 min.
8. Stock gate consistency: the origin of `MI+0x1170/0x1174` at kickoff in a stock match (absolute compare against 162000 implies origin near 0).

## 11. Verdicts summary

- LIMIT and F: writer `FUN_14044CE80` (per tick), setters `FUN_140A3CBB0`/`FUN_140A3CD70`, published by `FUN_140443390`; F stoppage +0.5/tick and LIMIT +0.5/tick clamp 21600 — CONFIRMED.
- Ring: wall-clock slot, constant key read-only in the encoder — CONFIRMED (key origin OPEN).
- Post-gate: 60 s counter `R+0xd0 > rate·60`, 4 s ball-zone `R+0xd8 > rate·4`, plus event timeouts; no gate-only whistle — CONFIRMED structure; branch semantics INF/OPEN.
- Half-time writer: its counters are on MB-E `S`, not `MI`; play state `116c` written by the sequencer — CONFIRMED.
- Breaks: 2 (HT), 6 (ET break) fixed; 4 and 8 from the tie helpers; 9 shoot-out, 10 full time — CONFIRMED values, INF meanings.
- Match speed: `inc = 100·E60·E5C/E58`; live 10.0 at E58 = 10 — CONFIRMED formula and live point; mapping `E58 = minutes` INF(high).
- HUD: `(stamp + F)/3600 − 2000` minute in period 1 — CONFIRMED on four OCR points; origin OPEN.
