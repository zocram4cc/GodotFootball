# HitIndex: GF's HitData lookup was mis-indexed — PES files at `(w0 & 0x1fff) + 0x11`

## 1. PES evidence (CONFIRMED, decompile + asm)

- `FUN_1406ad300` is the HitData builder (kind 1 in `FUN_1406aef60`'s dispatch: `case 1 → FUN_1406ad300(*param_1,param_2,1,param_3)`, record size `param_4 / 0x6c` = /108). Filing loop:
  `uVar3 = (*puVar12 & 0x1fff) + 0x11; param_1 = (ulonglong)uVar3 * 0x390 + param_1;`
- Exact VAs (disassembly of `FUN_1406ad300`): `AND AX,R8W` with `R8D = 0x1fff` at `0x1406ad42a..0x1406ad434`, `ADD AX,0x11` at `0x1406ad438`.
- Contrast: the Animation builder `FUN_1406ac4d0` (kind 0) files at `uVar2 & 0x1fff` with **no add**; the CancelData builder `FUN_1406ac990` also files at `(ushort)*puVar4 & 0x1fff` with **no add**. Sibling bases confirmed too: `FUN_1406ae670` (SubHitData) `(w0 & 0x7ff) + 0x61`, `FUN_1406ad170` (FootType) `(w0 & 0x1fff) + 0x10`, `FUN_1406ae350` (PivothitInfo) `(w0 & 0xfff) + 0x20b` — HitData's `+0x11` fits the family pattern.
- Install cross-check (2,925 HitData records, raw words 0..4371): 2,896/2,925 raw words have an Animation record at the same raw word, but **2,925/2,925** have one at raw+0x11; Animation records exist at 4,345 raw words, and the per-motion `+0xc8`-style reader conflict in `pes-re-gaps-anime.md` gap 2/round 4 is unaffected (different list). Raw words 0..13 each carry exactly one HitData record whose Animation twin sits 0x11 above.

## 2. Every GF site that indexes HitData

Grep for `HitData|hitData|\.Hits(|hits_` over `src apps tests godot tools` finds exactly two production sites, both fixed by the single loader change:
1. `src/pes/mbinfo.cpp:183` (`ParseMbinfo`): `Slot(m.hits_, w & MOTION_MASK)` → `Slot(m.hits_, (w & MOTION_MASK) + HIT_MOTION_BASE)`. **This is the only write site; every reader routes through it.**
2. `src/sim/onthepitch/pesdodge.cpp:390` (`KindRow`): `mb.Hits(motion)` — read-only consumer, correct once the loader files at the PES motion. No edit needed (verified: no `+0x11`/`-0x11` compensation belongs there; the motion it takes is the Animetable cell's motionNo, and PES's `FUN_140739f90` passes its looked-up motion straight into the `+0x20`-list reader `FUN_1406b0250`, i.e. the shifted table).
- `elizacontroller.cpp:1342,1403` call `KindRow`, not the index — out of scope and untouched. `ball.cpp`, `gfsim_run`, `match.cpp` untouched per the brief. `movestep_run.cpp`'s `ReadClipHits` reads `MotionRecord` (Animation/Cancel/Rot lists), never `hits_` — unaffected.

## 3. Fix

- `src/pes/mbinfo.cpp`: new named constant `HIT_MOTION_BASE = 0x11` with provenance comment (`FUN_1406ad300: (w0 & 0x1fff) + 0x11`), used at the one filing site. No other file changed in production.
- Note: GF's `Motion()` table (`motions_`) is fed by Animation/Cancel/Rot/Sub/Foot/Pivot only — HitData never entered it, so the fix moves only the `hits_` legacy index into alignment with PES's per-motion slots.

## 4. Tests (red before, green after)

- New `PesMbinfoInstall.HitDataLandsOnTheMotionsOwnAnimationRecord` (`tests/pes_mbinfo_test.cpp`): for each of the first 64 install HitData records whose shifted motion has an Animation record, asserts `Mbinfo::Hits(raw + 0x11)` is non-empty and carries the record's frame. Red on the old code (`ASSERT_FALSE(hits.empty())` fails, e.g. raw motion 2769), green after.
- Updated `PesMbinfo.DecodesTheThreeLayouts`: synthetic raw-5 record now expected at `Hits(5 + 0x11)` with all field decodes, plus `Hits(5)` empty. (The oracle-style install test `TheMotionTableHoldsTheFirstRecordsOfEachMotionAsTheBinsSay` covers RotData only — unaffected.)
- Mutation (run, rebuilt, reverted): dropping `+ HIT_MOTION_BASE` fails exactly the two HitData tests (`DecodesTheThreeLayouts`, `HitDataLandsOnTheMotionsOwnAnimationRecord`); restoring passes 9/9.
- Runs: `pes_mbinfo_test` 9/9, `pes_dodge_test` 45/45 (links the loader; its install `KindRow` path uses `ballOwner=false`, unaffected). Gate not run (Main gates). No ctest, no `gf-sim.lock`.

## 5. SHAs

- Branch `HitIndex`: red-test commit `99b3209`, fix commit `e99e7af` (confirmed via `git ls-remote origin HitIndex` → `e99e7af…`). Pushed; not merged.

## 6. Open items

1. Behaviour note for Main's batch smoke: every dodge `KindRow` ball-owner fit previously read the wrong motion's hits (mostly empty → row rejected); after the fix those rows can pass the fit. Smoke drift, if any, is the fix taking effect, not a regression.
2. `docs/re/pes-re-gaps-anime.md` gap 2 (`+0xc8` list identity) is untouched by this change but adjacent; the `+0x11` base belongs in that report's builder table when round 6 writes it up.
3. Scratch probes live in `<workdrive>/gf-HitIndex-scratch/probe*.cpp` (not committed); remove with the scratch dir at batch close.
