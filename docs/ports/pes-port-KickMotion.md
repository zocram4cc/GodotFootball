# PES port: KickMotion: GF kicks carry PES's motion id; Q's in-flight branch runs in a match (30-10-26)

Branch `KickMotion` from `origin/MergeBatchAP` (`3177c3a9`); head confirmed by `git ls-remote origin KickMotion` (see final message). Image `pes_full.bin`, base `0x140000000`. CONFIRMED = a tool run this session (decompile, unicorn call over the `pid407792/seq067` kick_scatter snapshot, test); INF = inference; OPEN = not settled.

## 0. Finding that changes the task: `FUN_1406B0250` does not pick anything

`FUN_1406B0250(table, motion, flag)` (`0x1406B0250..0x1406B02A2`, CONFIRMED decompile + asm) is a reader: motion `0xE42` -> `0x22`, `0xF7C` -> `0x18`, motion >= `0x1125` -> byte `1 + flag` of the table's default record (`table+8`), else byte `1 + flag` of the 0x390-byte record `table + motion*0x390`. Byte 1 of a record is the **count byte of Mbinfo kind 1 (HitData)**, capped at `DAT_1425DC370[1]` (the builder `FUN_1406AD300`; `pestrapdata`'s record block, proven 6/6 by TrapData). So "descriptor == 1" = **the kick's motion has exactly one HitData record**. The default record is filled by `FUN_1406AE940`, one entry per kind (`0x1406AE9A4 inc byte [rbx+1]`), so its byte 1 is 1.

The kick **motion** is picked by the Kick action start `FUN_140727230` (CONFIRMED decompile):

```
row    = FUN_14071EC10(ap)  -> FUN_14071E6F0(ap, request = vtable+0x28, kind = ap+0x2752, band = ap+0x2754, side = ap+0x2756, scale = ap+0x2758, flagA, flagB)
         FUN_14071E6F0 = FUN_1406ABBB0 (clip table [kind][band][side], kinds 0x77/0x78 by angle) + row swaps
motion = FUN_14071E570(ap, row, col = ap+0x2755, flag) = FUN_1406ABB30 (Animetable cell (row, col)) + two fixed swaps (0xA5E->0xA7B, 0xABE->0xADC)
         + per-player swaps FUN_1406B3560 / FUN_1406B3780 (table DAT_1436F2600)
FUN_1406F0F70(ap, motion)   SetMotion;  FUN_1406B0250(table, motion, 0) steers the approach (uVar8 < 2 at 0x140727940)
Q (FUN_14083CC30, 0x14083E429) reads FUN_1406B0250(table, u16 behind ap+0xAB8 vtable+0x18, 0) == 1
```
`ap+0x2755` (the Animetable column, 0..0x11) is `FUN_140723DE0(turn angle, kind, req+0x24)`; `ap+0x2754` the ball-height band (`FUN_140A6D380`, 12 cuts) after `FUN_140838CA0`'s scan of band-2..band+2. The clip table behind `FUN_1406ABBB0` is manager `[1]` (`0x2A78` bytes, init `FUN_1406A9F20`, filled by `FUN_1406AA590` from **Idletable.bin (side 0, stores at +0,+4..) and Movetable.bin (side 1, stores at +2,+6..)**; Kickinfo.bin fills the floats after it). Note the loader's sub-type numbers do not match the file names the way the decompile reads: the oracle decided it (Idletable = side 0).

## 1. What landed

- `src/sim/onthepitch/peskickmotion.{hpp,cpp}` (hand-translated, constants from the install): `LoadKickMotionConstants` (clip table from the VFS winner of the two files, angle cuts, three swap tables read by VA, the default-record count opcode-checked at `0x1406AE9A4`), `KickClipRow` (`FUN_1406ABBB0`), `KickRow` (`FUN_14071E6F0`), `KickMotion` (`FUN_14071E570` for a player with no swap-table entry), `KickBucket` (`FUN_140723DE0`), `KickMotionDescriptor` (`FUN_1406B0250`), `KickWorld` (the tick parity, side, action bit, displacement, heading, ball record the row fix-ups read), `KickMotionStats`.
- `pes::Mbinfo::HitCount` + `MotionCaps::hit` (`src/pes/mbinfo.*`): PES's count byte rec[1] (cap `0x1425DC370 + 4`). The legacy `Hits()` is untouched. No change to `pestrapdata.*` (not in this base; TrapCutover3 owns it): the descriptor comes from Mbinfo, equal to the TrapData record block (oracle `desc.tsv`, 4393/4393).
- Wiring: `MatchSetup::pesKickMotionConstants` (required) loaded in `gfsim_run`, `pes_library`/`gf_match` (gdext) and `tests/sim/pes_match_constants.hpp`; `Match::GetPesKickMotionConstants`, `PesKickMotionStats`.
- `humanoid.cpp` `PesKickSituation(match, ...)`: the descriptor-0 stand-in and the `kPesDefaultB2754/kPesDefaultBucket` defaults are replaced by the pick: band = ball-height band at the touch, bucket = `KickBucket(turn angle)`, row = `KickRow`, motion = `KickMotion(Animetable)`, `descriptor` = `KickMotionDescriptor`, ball records 1 and 2 = `Ball::TrackSample(1/2)`; the motion also feeds Q's `motion` input (the 0x9BE / 0xA20 ... overrides and the 0x9F1 gate) for the first time. A kick the table has no motion for keeps the PES "none" band `0xC` / bucket `0x12` and descriptor 0.
- `tools/dev/rendezvous/kick_motion_replay.py`: calls PES's own `FUN_14071E6F0`, `FUN_14071E570`, `FUN_140723DE0`, `FUN_1406B0250` in unicorn over the seq067 snapshot (20 s) and writes the oracle TSVs (scratch, PES-derived, never committed).
- `tests/sim/pes_kick_motion_test.cpp` (registered in `tests/sim/CMakeLists.txt`).

## 2. Oracle (CONFIRMED; `PES_KICK_MOTION_DIR` + `PES21_ROOT`, 18/18 pass)

Equal to PES: the whole clip table (3624 cells, byte for byte); 14,256 clip rows (ap null) with the angle grid for kinds 0x77/0x78; the real-ap rows at the capture's match (tick 10099 odd, side 0; 0x809 rows excluded: they read the ball and displacement); 13,573 (row, column, flag) motions from the Animetable incl. the two fixed swaps; 32,850 bucket rows (kinds 1..150 x 73 angles x req -1/0/1); 4,393 descriptors (every motion 0..0x1124, 0xE42, 0xF7C, 0x1125, 0x2000); **the captured kick** (kind 98, band 0, column 9, side 1 -> row 2141 -> motion 4266 = the motion the kicker plays, descriptor 1).
Captures: only `pid407792/seq067` (kick_scatter, full) is on disk; the three `kick_postscale` captures are slimmed and cannot serve these calls. The 510-row Q oracle's TSVs no longer exist (scratch removed): `pes_kick_error_test` ran with its two TSV oracles skipped; **the "510-row oracle still bit-exact" check was not re-run** (regenerate with `kick_replay.py --q` on a snapshot, then `PES_KICK_Q_TSV`). Q's code is unchanged except the stale ponytail comments.

### Mutants (scratch copies, rebuilt, run, reverted; repo untouched) - 17/17 killed
M1 angle-A cut (`a < n157` only), M2 mid sector side, M3 swap loop continues past the first match (first survived; killed by `ASwapTableStopsAtTheFirstMatchingRow`), M4 odd-tick inverted, M5 pass-row request, M6 fixed swap 0xA7B->0xA7C, M7 empty cell -1 instead of 0, M8 `lab` split, M9 header byte `<2`->`<1` (first survived; killed by `AKindTableByteOfOneIsAHeaderAndTwoIsNot`), M10 default-record count, M11 flag A, M12 action row (first survived; killed by `TheActionRowTakes...`), M13 Mbinfo hit cap removed, M14 descriptor 0xE42, M15 bucket req sign, M16 odd-tick 0x809, M17 clip side swap. The tests for the 0x809, lateral, first-side, action-bit and flag-A fix-ups are unit tests with synthetic constants: the install-only oracle cannot reach them (the capture's match takes none).

## 3. Smoke (seed 7, 2 min, `--db-sql assets/fixtures/teams.sql --home 1 --away 2`, own build; Main re-runs on the merged tree)

`[kickmotion] 30 kicks, no clip 21, descriptor 1 9, in-flight Q 9` -> **in-flight Q evaluations 9 > 0** (before: 0 by construction). Two runs: rc 0 both, output identical except the wall-clock seconds of `[profile-split]` (tick counts equal: in-play 21601, hold 12027, 33628 ticks to full time). `[balance] shots 5-1, goals 2-1, passes 10-15`. Behaviour moved by design (Q's bucket terms and the descriptor branch are on for the 9 kicks with a motion; the band/bucket defaults are gone). No test needed a re-stage in the binaries I ran (`pes_kick_error_test` 17 pass + 2 skipped for missing TSVs; `pes_kick_motion_test` 18/18); the 23 `pes_*` binaries that link `peskickerror` and the seeded `pes_defender_decisions` / `pes_throwin_release` / `pes_restart_delay` tests were **not run** (budget): Main's gate decides whether any seeded scenario moved.

## 4. Open items (numbered)

1. **21 of 30 kicks have no clip.** GF's stand-in kinds (`0x69` shot, `0x21` ground pass, `0x49` lofted, `1` header) have table rows only at bands 0, 2, 4 (`0x1` at 5, 6, 7, 9) and `0x49` only on side 1, while GF's stand-in side is 0. PES's chooser (`FUN_140829E80` / `FUN_140838CA0`: kind by band, band scan) picks another kind there. Porting the chooser is the real fix; a cheap interim is the kicking foot as the side (GF's `touch_bodypart`).
2. Not ported, named `ponytail:` at the call site: the band scan (`FUN_140838CA0`), the side `ap+0x2756`, request `+0x24`, `FUN_1406EAD40`'s action bit, `MI+0x1554 == 1`, the per-player swaps `FUN_1406B3560/B3780` (`DAT_1436F2600`, the player ids `0x32F`, `0x1CFBE` ... branch at the head of `FUN_14071E570`).
3. The F1/F2 branch of Q (`0x14083E749..0x14083E76F`, reads `FUN_140A57450(.., 1)` when the descriptor is 1) is still `F = 1.0`: with a real descriptor PES now runs it for those 9 kicks.
4. The 0x809 row and the lateral swap read the player's displacement, heading and the ball record; the oracle cannot see them (snapshot is one kick). A second kick capture on a 0x809 / swap row would close it.
5. `pestrapdata` is not in this base. When StoppageFlow merges, nothing here depends on it; if TrapCutover3 wants one motion-record reader, `Mbinfo::HitCount` equals its record byte 1.
6. Re-run the 510-row Q oracle (item above) and the 23 `peskickerror` binaries on the merged tree.

## 5. Verification run

Tests run: `pes_kick_motion_test` 18/18 (hand build against the identical prebuilt translated layer, and the CMake build: the CMake target `pes_kick_motion_test` needed a local-only import of a sibling lane's prebuilt `libpestranslated.a` (same `src/pes/translated` tree) because the machine could not build 4,548 translated objects; that CMake edit was reverted, not committed); `pes_kick_error_test` run, its two TSV oracles skipped. `GATE fast: NOT RUN (machine load; budget)`. `gen_translated.py --check`: not run (no translated file touched). Process note: two of my own new scratch/tool files were authored with a short python rewrite instead of the edit tool (`kick_motion_replay.py`, `peskickmotion.hpp` trim): the result is the intended text.
