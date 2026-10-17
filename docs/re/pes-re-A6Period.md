# A6Period: PES period record and time-up gate (RE + port report)

Branch `A6Period`: red `c437690` (tests), green `b7943a5` (port). `git ls-remote origin A6Period` = `b7943a5`.
Install: `<workdrive>/PES21/PES2021.exe`, image base 0x140000000. Live reads read-only (`/proc/<pid>/mem`); no writes, no pins.

Verdict tags: **CONFIRMED** (decompile, raw asm, or live read whose checksum verifies), **INF(conf)**, **OPEN**.

## 1. Decode

| Item | VA | Verdict |
|---|---|---|
| `FUN_140A3C070`: slot = `ring[idx&3]` (`MI+0x1184`, slots `+0x1188..+0x1194`), key `MI+0x1180`, checksum `MI+0x1178` | 0x140A3C070–0x140A3C16A | CONFIRMED (raw asm) |
| K_i = byte i of `key*3 + 4i`; decoded byte = `((L_i + K_i) mod 256) ^ ((K_i*63) mod 256)` | same | CONFIRMED (raw asm) |
| Checksum = FNV-1a 64 over the four decoded bytes (offset `0xcbf29ce484222325`, prime `0x100000001b3`); mismatch calls `FUN_1415A1714` (trap) | 0x140A3C151–0x140A3C16F | CONFIRMED |
| Return = the four bytes as f32 bits (`MOVSS` at 0x140A3C15F) | 0x140A3C15F | CONFIRMED |
| `FUN_140A3C050`: clock = stamp + `F` (`MI+0x1174`) | 0x140A3C050–0x140A3C068 | CONFIRMED |
| Period record `MI+0x1168` period, `+0x116C` play state, `+0x1170` LIMIT (f32) | `FUN_14050B9B0` head | CONFIRMED (offsets) |
| Live: decode with checksum passes on the live record (period 1, state 2) | live | CONFIRMED |

## 2. Time-up gate

- `FUN_14050B9B0` (0x14050B9B0): `clock = FUN_140A3C050(MI+0x1168)`; `flag = ((period-5) & ~2) == 0`, i.e. periods 5 and 7; `FUN_140A6BEF0(clock, LIMIT, flag)`.
- `FUN_140A6BEF0`: threshold = `(flag ? [0x142647C64] : [0x142647C68]) + float(u16(int64(LIMIT / [0x142598590]))) * [0x14259A764]^2`; returns `threshold <= clock`.
- Install values (read by `LoadPesClockConstants`, checked by `PesClockInstall.ConstantsComeFromTheExe`): 162000 (periods 1, 3), 54000 (5, 7), 3600 (units per minute), 60 (`0x14259A764`).
- **CONFIRMED** that the units are 1/60 game-second: 162000 = 45 game-minutes, and LIMIT = 21600 live = 6 min x 3600.
- Live process: both gate constants read `999999995904.0` (1e12). **The endless patch is active in this session**, so the stock gate cannot be observed live now.

## 3. Live stoppage and play rates (read-only, PID 2510857)

- Play (state 1): stamp +9.94 per frame over 2359 frames, so about 10 per frame. F constant. Frames 56.3/s wall.
- Stoppage (states 2 and 3): stamp frozen. F +19.5 over 39 frames (exactly 0.5 per frame); 682.5 over 1398 frames (0.49); 0.43 to 0.47 over other windows.
- `116c` takes 1 (in play), 2 and 3 (stopped).
- **INF(high):** the per-frame play increment of 10 is PES's match-speed setting (`E+0x58` = 10.0 live). One sample. GF runs at game time (60 units per game-second); the speed mapping is open.
- **INF:** the stamp base (~7.2 M units) does not track the HUD clock. That agrees with `pes-re-clock-live.md` §8.2, and this report does not resolve it.

## 4. Port (GF)

- `pesclock.{hpp,cpp}`: `PeriodRecord` (PES layout), `PeriodStamp` (decode, throws on checksum), `PeriodClockUnits`, `StoreStamp` (inverse), `TimeUpReached`, `PeriodNumber` (1, 3, 5, 7, 9), `PeriodHasClock` (1, 3, 5, 7), `PesClock` (record per period; `Advance`: play adds 60/s, stoppage adds F at 0.5 per frame; `AnnounceAddedTime` once per period).
- Referee whistles when `TimeUpGateOpen()`. LIMIT is announced at the length. The neutral band and the 3-minute stoppage cap are gone.
- Deleted: `ShouldEndPeriod`, `neutralBandHalfWidth`, `maxStoppageTime_ms`, `PeriodEndMinutes/Ticks`, `HalfLengthMinutes`, the percentage clock, `IsPeriodDue`, `PeriodEndWithAdded`, `addedTimeAnnouncedPhase`, and the in-match percentage tests.
- Consequence: tier 0 earns no added time (`AddedTimeUnits` is 0 for tier 0), so GF's whistle is still at the length, as before.

## 5. Tests and mutations

- `pes_clock_test` 20/20, `pes_clock_match_test` 2/2, `pes_period_end_test` 3/3 with `PES21_ROOT` set (none skipped).
- Red: `c437690` (new APIs absent on main: compile-level red). **Not re-run on main.**
- Mutations (each rebuilt, run, reverted; all caught by the named test):
  - key multiplier 3→2: `DecodesTheLivePeriodClockAsPesDoes`
  - checksum check disabled: `ASlotChangedUnderItsChecksumIsRefused`
  - penalties has a clock: `ThePenaltyShootoutHasNoTimeUp`
  - extra-time flag off: `ExtraTimePeriodsUseTheShorterLength`
  - gate 1 s early: `GateOpensExactlyAtTheHalfLength`, `AddedMinutesHoldTheGateByWholeMinutes`, `StoppageTimeCountsTowardsTheTimeUp`, `ExtraTimePeriodsUseTheShorterLength`
  - period reset removed: `ANewPeriodStartsAFreshRecord`
  - announce-once guard removed: `AddedTimeIsAnnouncedOnce`
  - stoppage F = 0: `AStoppageHoldsTheStampAndAddsHalfAUnitPerFrame`, `StoppageTimeCountsTowardsTheTimeUp`, `AStoppageMovesTheClockOnlyThroughF`
  - stamp advances in stoppage and rate 1/s: `InPlayTheStampAdvancesAtTheUnitRate`, `AStoppageHolds…`, `AStoppageMovesTheClockOnlyThroughF`
  - phase change not starting a record: `EachTransitionStartsItsOwnPESPeriod`
  - referee whistle disabled: `TheRefereeWhistlesWhenTheGateOpens`
- Not mutated: `PastPeriodLength` vs `TimeUpGateOpen` (equivalent at tier 0).

## 6. Smoke (2 min, `gfsim_run --seed 7 --minutes 2`, no lock, per contract)

- Two runs, both exit 0, `[determinism] 8a6f941897eceab4` both times.
- `[restarts] 12 stoppages, longest 16.9 s, bound 32.5 s`. `[ticks] 33894 to full time (cap 194400)`. `[idle] 29 episodes, stalls 16`.
- Not run on main (no baseline line).

## 7. Open

1. **Ring rotation and key policy** of PES's writer. GF rotates `idx` by one per store and keeps key 0. Any key round-trips, so this only matters if PES's policy is observed.
2. **Writers of LIMIT and F in PES** (where and when LIMIT is set; the F 0.5-per-frame writer VA is not traced). GF announces at the length.
3. **Post-gate code of `FUN_14050B9B0`** (the 60-second counter `d0` against `rate*60`, the ball-state branches, the `FUN_140A57800` tests). GF whistles on the gate alone.
4. **`116c` transitions:** the half-time writer (`FUN_140A115A0`, `116c += ...`) disagrees with `116c` as play state. Not traced.
5. **Breaks** (periods 2, 4, 6, 8) are not modelled; the walk-off is GF's set piece.
6. **Match speed:** PES plays 10 units per frame at `match_time` 10; GF runs at game time. Needs the setting-to-speed map.
7. **Endless exhibition knob** (plan scope): not ported.
8. **HUD clock source** (`pes-re-clock-live.md` §8.2): still unresolved; GF's display keeps its own elapsed seconds.
9. Dropped: the in-match "clock counts the match's game time" check, which depended on the percentage model.
