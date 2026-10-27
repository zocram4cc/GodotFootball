# PES port FoulOffside2: PES's match-info block holds the period record; the switch-over did not land

Branch `FoulOffside` (origin), head `b14514b8` (`git ls-remote origin FoulOffside` confirmed), on `c428d636` merged with `origin/main` (`59f9e89d`) and `origin/Cutover` (`b71c0a66`). Commits: `317ba045` (test fix), `b14514b8` (match-info block). Worktree `<workdrive>/gf-FoulOffside`. Labels: CONFIRMED = read from code, a test, or a run this pass; INF = inference; OPEN = blocker named.

## 1. Result

- **Landed (CONFIRMED):**
  - The match-info block MI sits in `PesWorld`'s window beside the player blocks (`src/sim/onthepitch/pesworldrecord.hpp`: `PesMatchInfo`, `kMatchInfoBytes` 0x15C0, `kMatchInfoPeriodOffset` 0x1168). The period record `PeriodRecord` (MI+0x1168..0x1194, `pesclock.hpp`) is inside it.
  - G+0x270 holds the block's VA (`PesWorld::MatchInfoVa()`), as `0x140A3A690` and `0x14047F170` read it (docs/re/pes-re-ref-record.md 1.1).
  - GF's own period copy is deleted. `PesClock::record` is now a reference to the block. Match builds the world before the clock. The clock's unit tests pass a local record.
  - Fix on the merged tree: `pes_team_tick_test.cpp` no longer compiled after Cutover4 moved `Candidate` and `Facts` onto man records. Ported; expectations unchanged.
- **Not landed (OPEN):** both switches (`0x140A3A690` officials' involvement, `0x14047F170` foul sanction), the referee records, the other MI fields' GF copies, and frames. Blockers in §4.

## 2. Verification (merged tree, Release)

- Full build: `cmake --build build -j2 -- -k 0`, rc 0.
- Unit suites, run directly (not via ctest; the gate is Main's): `pes_match_info_test` 2/2, `pes_team_tick_test` 26/26, `pes_clock_test` 51/51, `pes_world_test` 6/6, `pes_period_end_test` 11/11, `pes_mark_table_test` 19/19, `pes_scorerows_test` 12/12, `pes_offball_test` 76/76.
- **`pes_forfeit_test` segfaults (rc 139) in `PesForfeitMatch.ASideAtTheMinimumKeepsPlaying`.** Backtrace: `pes::offball::OppNear` <- `TeamAIController::ProcessRunManager` <- `Team::Process` <- `Match::Process`. The earlier case in that binary passes. This change touches neither the run manager, `OppNear`, nor `PesSlotOf`. INF: a short side's squad member gets no PES slot (0xFF) and `OppNear` indexes `w.p[255]`. Pre-existing on the merged tree (not measured on the pre-merge base). Not fixed here; open item 8.
- Mutation M1: `pesclock.cpp:317` `record.period = 0`. `PesMatchInfo.TheClocksPeriodRecordIsTheMatchInfoBlock` fails (0 vs 1). Reverted; green again.
- Red first: `pes_match_info_test.cpp` did not compile before the change (`MatchInfoVa`, `Period`, the block constants, the three-argument clock were undeclared).
- Repros, 5 min, `gfsim_run --seed S --minutes 5 --pes21-root <workdrive>/PES21` through `tools/dev/simslot.sh timeout 900`, one at a time:

| run | determinism | sim ticks |
|---|---|---|
| switch-off, seed 7 | `7ca3d08f68567016` | 31521 (the brief's value: unchanged) |
| switch-off, seed 3 | `6536a99ad3d2e34c` | 25212 (no pre-change seed-3 run) |
| `--pes-loop`, seed 7 | `45e830a988ff01cf` | 28663 (new) |
| `--pes-loop`, seed 3 | `2c0749ccc8ca6735` | 27548 (new) |

The FoulOffside doc's `--pes-loop` value (`67d115b984bab251`, 25398 ticks) was on the pre-merge base. Attribution of the new `--pes-loop` hashes to the merges versus this change is NOT measured. The change is storage-only: `PeriodRecord`'s defaults are all zero, so the block starts as the old clock record did. That is an argument, not a measurement.
- Frames: none. No switch landed, so a frame would show GF's current officials, not the target.

## 3. What the switches need (decoded; the spec inputs are in `tools/dev/diffharness/specs/stoppage/*.json`)

**`0x140A3A690` (officials' involvement).** Inputs: A0+4 (u32, the official's or involved player's index). Image global `0x1436F3DC8` (G; production must map 8 bytes there holding the window VA; not wired). G+0x270 (MI: 116C, 11B2, 1544, 1548, 154C, 1554, 155C, 1564, 1574, 1588). G+0x1928 (team record, +0x254 s8 attack direction; the side table is still null in production). G+0x19C8 and G+0x1DD8 (anime and man records; +0x1 class, +0x55C and +0x564 position), plus the E1EE table entries (other indices). It calls `0x140A48910`, `0x140A48A10`, `0x141E53520`, `0x140A3A500`, all translated.

**`0x14047F170` (foul sanction).** Inputs: A0, A1+0x18 (3 inputs), A1+0xE0 (about 23 inputs), G, G+0x270 (MI: 1554, 1574, 155C, 1564, 1588, 158C, 1590, 1594, 15B8), G+0x1928 and G+0x1978 (team records; side B is unbound), G+0x2AA8 (`[G+0x2AA8]+0x18`, goal-area half-width), E1EE entries. Its only PES caller `0x140480910` is untranslated (its selector `0x141F500D0` writes a global).

**Officials.** Official indices are 22..25 (`ref+0x18`, 0..25 valid). `BindSlot` covers only slots 0..21, so the translated accessors return null for officials. Their man records (position, facing) have no window storage yet.

**Writers still in GF.** `restartTeam` (`buffer.teamID`), `restartPos` (`RefereeBuffer::restartPos`, a `Vector3`), `foulType`, `foulPlayer` and `foulVictim` (`Foul`), the offside words (`offsidePlayers` map and `pendingOffside`), 11B2, 15A5, 15B8. These need migrating into the block at their writers, with the offside map reduced to PES's words.

## 4. Why the switches did not land (blockers)

1. **Involvement.** GF has no function for the officials' involvement at a stoppage (`grep -i involv src/sim`: only the offside test in `Referee::BallTouched`). The PES caller of `0x140A3A690` is not decoded, so the hand code it would replace is not identified. The candidates, `RefereeController::RequestCommand` (the referee's approach gate at lines 65-69, the linesman's `holdForStoppage` at line 180), are unverified against PES's call graph. OPEN.
2. **Sanction.** A1 is not GF's `EventRecord` (800 bytes; the tree reads A1+0x1900 and +0x1904 and A1+0xE0). The layout beyond +0x18 is OPEN, and the caller is untranslated. OPEN.
3. **Referee records (§2).** The official-pointer location is OPEN (RefRecord §5, items 1-3). The four action functions (`0x140A38F20`, `0x140A39A80`, `0x140A3A870`, `0x140A3A930`) are unproven: their corpus is empty (null record fields). No record was built, so no GF copy was deleted for them.
4. **Frames** depend on 1 and 2.

## 5. Open items (numbered)

1. Decode the callers of `0x140A3A690` and what its result gates; name the GF hand code it replaces. Blocks the involvement switch.
2. Layout of the sanction tree's A1 record (+0x18, the +0xE0 sub-record, +0x1900 and +0x1904) and its caller's inputs. Blocks the sanction switch.
3. Production binding: map `0x1436F3DC8` as a `PesMem` seg for the call; bind the side tables (team records, sides A and B) and the officials' blocks (22..25).
4. Move the remaining MI fields' GF copies (restartTeam, restartPos, foul fields, offside words, 11B2, 15A5, 15B8) into the block at their writers.
5. Referee records (at least 0x958 bytes each) once RefRecord §5 names the official-pointer location.
6. Agreement run (or per-call proof) on seeds 7 and 3 for each switch, then delete the hand code.
7. Frames of a foul and an offside stoppage in the PES loop, after 1-6.
8. Fix `pes_forfeit_test`'s segfault: `OppNear` on a player with no PES slot. Owner: the Cutover run-manager path.
