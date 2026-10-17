# PES 2021 Extra Frontman: live pin runs (EFPin), 17-10-26

Scope: owner ruling to pin side-0 `team+0xB8A2` = 1, and in run B also bit 15 on the CB (side-0 local slot 1), to test whether `P+0x3EF8` names the CB and whether the FC860 +0.2 matters. Never written: `MI+0x1168`, `MI+0x116C`, `MI+0x1574`. Full detail: `pes-re-extra-frontman.md` §6.8.

Tags: CONFIRMED = live read/write readback or raw decompile in this pass. INF = inferred. OPEN = blocked.

## Words and VAs (this match)

| Word | VA | Original | Written |
|---|---|---|---|
| side-0 `T+0xB8A2` (posture, full attack) | `0x1D47D6A6` (1 byte) | 0x00 | pinned to 0x01 (control, cb) |
| CB roster gate word `P+0xE0+0x48+r*0x128` (r=14) | `0x1D48B230` | 0x1000 | 0x9000 (cb only) |
| CB live copy word `copy+0xB8` | `0x1D4B3110` | 0x1000 | 0x9000 (cb only) |
| designated slot `TD+0x3EF8` | `0x1D481E40` | 0x00 | 0xFF once (des), 0x30 once (probe) |

The VAs differ from §6.4 (earlier match). All restores read back equal to the original (`ok=True` in every log).

## Runs (300 s, 300 s, 150 s, 180 s)

- **A control** (`b8a2` only): 130,657 samples; `b8a2` rewritten 1,848 times (the game reverts it about every 0.16 s); all pins held in 6.9% of samples. `P+0x3EF8` = 0 in all 9,226 valid samples, both sides. 5 set-piece entries, all with `P+0x3EF8`=0.
- **B cb** (`b8a2` + bit 15 on CB): 129,034 samples; rewrites `b8a2` 1,658, roster 1,636, copy 1,658; all held in 5.9%. `P+0x3EF8` = 0 in all 9,247 valid samples. CB bit 15 seen at 1 of 14 set-piece entries; `b8a2`=1 at 2 of 14.
- **D des** (`b8a2` + one write `P+0x3EF8`=0xFF): 0xFF in all 5,303 valid samples, so no winner was written while `b8a2`=1 was observed.
- **P probe** (no pin, one write `P+0x3EF8`=0x30): 0x30 in all 6,297 valid samples with `b8a2`=0. CONFIRMED (decompile of `FUN_1405FC860`, first gate): `b8a2`==0 writes 0xFF. So FC860 did not run on side 0 during ~3 min of play, dead ball and set piece.

## Answers

1. `P+0x3EF8` names the CB in B and not in A? **Neither.** It stayed 0 in every valid sample of A and B. CB designations observed: 0 of 19 set-piece entries and 0 of ~18k samples.
2. +0.2 effect attributed? **No.** No designation was written, so the FC860 +0.2 branch (L62–65) was never exercised.
3. Candidate list `FUN_14051B8B0` (SP[4..14]): side 0 unchanged in every run `(2047,1,0,1,2,3,6,4,5,8,9)` (181 cleared-to-255 rows in A, none for side 0 in B). Side 1 changed in B (SP[4] 2047↔143, SP[14] 8↔9). INF: the CPU store runs for side 1 and not side 0, so side 0 may be the human side. Not verified.
4. `F63C0` / `F7D00` outcomes: **not observed.** Their outputs are not in memory after return; no breakpoints used. OPEN.

## Decode added (CONFIRMED, raw decompile of `FUN_1405FC860`)

- Gate 1: `b8a2`==0 → `des=0xFF`, return. Gate 2: `MI+0x116C`!=1 → `des=0xFF`. Recompute runs only if `0x15 < des`.
- So `des`=0 can never recompute. The pin alone cannot produce a designation; the function must first see `des` = 0xFF (or a value above 0x15).
- The single caller is `FUN_1405EC600` (FC860 call at the end of an 11-slot loop). `FUN_1405EC600` is called from team tick `FUN_14051D9E0`: MI==1 branch and MI∈{2,3} branch.

## Open items

1. **Blocker:** why the team tick (or `FUN_1405EC600`) does not reach FC860 for side 0. `FUN_1405EC600` has no early return in the decompile, so either the tick is not entered for side 0 or the decompile misses a `goto`. The tick's own entry gate is not decoded.
2. Run the sentinel probe on side 1's `TD+0x3EF8` (VA via `H+0x450+8*1`, `+8`, `+0xBA8`, `+0x3EF8`) to see whether any side reaches FC860.
3. Only then repeat the CB pin, in a state where FC860 runs.
4. `FUN_140A55EF0` (side-type test) and its table not decoded live.
5. Unexplained cleared SP lists (all 255): 181 rows in A, 208 side-1 rows in B. Not attributed to the pins.

## Files

Scratch: `<workdrive>/gf-EFPin-scratch/` (`efpin_run.py`, logs `efpin_{control,cb,des,probe}.log`, `fc860.c`, `ec600.c`, `d9e0.c`). No repo writes.
