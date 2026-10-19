# Team mode (G4 operand), K+0x6E (DHB) and T+0x87CD (2-CF gate): writers and live values (S1Watch, run 3, 19-10-26)

> **Main audit 19-10-26 (rev 2):** `0x141052E30` reader, zeroing store `0x140A65E15`, T+0x87CD store `0x1405F9E5C` (+ `0x1405F9E51`, source `0x1405F9E2B`), K+0x6E flag store `0x14063AA5E` `mov [rdi+0x6e],r13b`, reset `0x14063A3F6` `mov [rcx+0x6c],rax` (qword covers +0x6E), copier `0x140AC0365` `mov [rbx+0xe6],al`, writer call `0x142110E78 -> 0x1421197B0`: all re-read by script and match. Port-ready: G4 mode writer (FUN_1421197B0 decision order), K+0x6E flag (=1 only when K+0x6C=0), T+0x87CD = on-pitch count of slot classes 7-9 (needs RoleArrays' class arrays).

Read-only. Live reads via `/proc/<pid>/mem` (`sudo -n`), `perf` execute and write watches on the JobGraph worker threads (no stop, no pin, no write), `perf` CPU sampling. No PES memory written. No GF edits. Scratch: `<workdrive>/gf-S1Watch-scratch/` (`modelog.py`, `gatelog.py`, `kwatch.data`, `entry.data`, `dec_1421197B0.c`, `dec_142110D30.c`, `dec_142128C10.c`, `dec_142128C10.c`, `dec_63A490.c`, `dec_1405F9CD0.c`, `dec_142110D30.c`, `dis_63A400.txt`, `scan32b0.py`, `scanA.py`).

Tags: **CONFIRMED** = live read, watch capture, raw asm or bridge decompile. **INF** = inference. **OPEN** = blocker named.

## 0. Summary

1. **G4 operand = `bp+0x32B0`, not the team-AI snapshot.** The driver `FUN_14210F110` calls `FUN_141052E30(param_1+0x32B0)` directly (decompile lines 211 and 447; `FUN_1421112C0` line 76). `bp` = BallPlayer object = **`0x1D6DA0F0`** in this run (live: `R8 = bp+0x1C`, `R9 = bp+0x78`, `CX = bp+0xEC` at the image-builder entry `0x142119390`). So the G4 mode is `bp+0x32B0+8 = 0x1D6DD3A8` (CONFIRMED live). The team-AI snapshot path (`TA+0x5A0 → src+0x48`) of run 2 is a different structure; it is not the G4 operand. **Run 2's "G4 mode = 0" was that wrong object.**
2. **Nonzero writer: `FUN_1421197B0`** (writes `bp+0x32B0+8` with 4, 3, 2, 1; caller `FUN_142110D30` at `0x142110E78`, which is only reached when `bp+0x10 ≠ 0`). Live values in active play: **1, 2, 3, 4** (4 seen in run 2 at 2.6 s; 3 and 2 in every window; 1 at `bp+0x10 = 0`).
3. **Decision (CONFIRMED by decompile of `FUN_1421197B0`, live-consistent):** see §2.
4. **K+0x6E (DHB) writers:** `0x14063AA5E` (`mov %r13b,0x6e(%rdi)`, flag store, watch IP `0x14063AA62`), reset to 0 in `FUN_14063A3B0` (store `0x14063A3F6`, watch IP `0x14063A3FA`, ~50 hits/s). The flag is 1 only when `K+0x6C == 0` (live: `K+0x6E = 1` only with `K+0x6C = 0` in 1,905 of 1,905 cases). The copier `0x140AC035E` was **not** hit in 25 s of active play (INF: rare). Live: `K = bp+0x78 = 0x1D6DA168` (CONFIRMED from builder entry `R9`).
5. **T+0x87CD (2-CF) writer:** `0x1405F9E5C` (`mov %r10b,0x7b21(%r15,%rax,4)`), watch IP `0x1405F9E64`. Value = **number of on-pitch players of role group 2 (classes 7–9)** in the team, per the function body (decompile, §3). `2` = exactly two such players (INF high). The row selector `[this+0x10C]` (`r9`) is OPEN.

## 1. G4 mode: operand chain and writer (CONFIRMED except the predicate `ctx[6]`)

- Reader: `FUN_141052E30(p) = *(u32*)(p+8)` (bridge decompile).
- Carrier driver: `FUN_14210F110` (bridge decompile) reads `FUN_141052E30(param_1+0x32B0)` at lines 211 and 447; `FUN_1421112C0` line 76. The image builder `FUN_142119390` is called with `(bp+0xEC, ..., bp+0x1C, bp+0x78)` by `FUN_142110D30` (decompile lines 49, 52).
- Writer: `FUN_142110D30(bp, team)` (`0x142110D30`, called from `0x142101AA7`) runs `FUN_1421197B0(bp+0x32B0, team, bp+0x1C, bp+0x78, bp+0xEC, ...)` at line 52, and only inside `if (*(char*)(bp+0x10) != 0)` (line 40-ish; decompile line 33 `if (*(char *)(param_1 + 0x10) != '\0')`). Only caller of the writer: `0x142110E78`.
- Live (run 3, active play):
  - `bp+0x10` (writer gate) toggles 0/1 with play. Mode values by gate, 42,897 samples over 40 s: `(bp+0x10=0, mode 1) 37,158`; `(bp+0x10=1, mode 1) 2`; `(bp+0x10=1, mode 2) 5,737`.
  - Run 3 window 1 (30 s): `mode 1: 937`, `mode 3: 709`, `mode 2: 105` samples (sample rate ~1.7 kHz; all three seen).
  - Run 2 window (8 s): values 3, 1, 4 seen; one 4 at 2.62 s, back to 1 at 3.54 s.
- Holding: when `bp+0x10 = 0` the value is not rewritten and holds its last value (INF from the gate; the mode-1 rows at `bp+0x10 = 0` are the held value).

## 2. Decision of `FUN_1421197B0` (CONFIRMED structure; live checks of the 3/2/1 branches)

Read in order (decompile lines 72–240, `dec_1421197B0.c`; Dribble inventory §2 agrees):

1. `prev == 4` → 4 (sticky) (line 72).
2. `IsClose(imageA+0x9CC)` (`FUN_140699820`) and `|wrap180(bp+0x2C − bp+0x4C)| < 100` → 4 (lines 77–93). **Not observed live in run 3** (the `IsClose` input is not read); observed once in run 2.
3. `prev == 3`, or `attackSign × ball.x > F[6]` (F[6] = penalty line = `*(G+0x2AA8+0x18)` = **36.14**, live), or `imageA+0xB3C ≤ imageA+0xB58 + 10` (`bp+0xC28`, `bp+0xC44`; threshold 34.0 with `B58 = 24`) → 3 (lines 98–103).
   - Live: all 709 mode-3 samples satisfy `(sign·x > 36.14) or (B3C ≤ 34)`; the `B3C` test alone accounts for 320 of them and the penalty test for 389 (both together 389).
   - `attackSign = bp+0x30` (= +1.0 in every live sample).
   - `ball.x` = `*(*(G+0x40)+0)` (`G+0x40` is the ball object; live `bx` values 0–42).
4. Else, if `team+4 < 2` (line 106) and `prev ≠ 2`: corridor branch (lines 106–230): origin = carrier position moved 30 m along `team+0x250`; corridor-1 count 0 → 2; goal and clock test; corridor-2 count ≤ 1 → 2; else 1 when `team+0x134 > 2` or the team-sheet slider ≠ 1 or `bp+0x88 ≥ 2·fps`, otherwise 2 (lines 221–225). **1 comes only from here.**
5. Else → 2 (lines 231–234). Live mode 2 with `B3C > 34` and no penalty is the common in-play case (5,737 samples, `bp+0x10 = 1`).

Verdict (CONFIRMED structure, INF on the corridor geometry): **4** = shoot-zone angle test (or sticky); **3** = penalty box (ball x beyond F[6]) or the `B3C` test; **1/2** = corridor test in play, else 2; **held** when `bp+0x10 = 0`. **OPEN:** the `IsClose` helper and corridor helpers (`FUN_14063D2F0`, `FUN_14063CD80/90`, `FUN_140A53DF0`, `FUN_140A90D80`) are not read through; `B3C`/`B58` writers (image-A updater `FUN_142119390`) are OPEN.

## 3. T+0x87CD: the table and what 1/2/3 are (CONFIRMED structure; INF on the class meaning)

Function `FUN_1405F9CD0(this, side, B, rec)` (bridge decompile `dec_1405F9CD0.c`; the loop at `0x1405F9E20..E68` is in this body, `int3` pad at `0x1405F9CC7`-ish, the head is `0x1405F9CD0`).

- Loop over the team's 11 slots (`iVar11 = 0..10`, slot class `iVar4 = *(this+0x128 + 4·slot)`). Group `lVar6 = 0` (class ∉ 3..9 or 0), `1` (classes 3–6), `2` (classes 7–9). Each slot appends a 8-byte entry to `rec[group]` (count byte `+0x58`); if the slot is on pitch (`MI+0x1748 + side·11 + slot`, flag `bVar14`), the on-pitch count byte `+0x59` is incremented too.
- Output: for `k = 0..2`, `*(B + 20·k + 60·r9 + 0x7B20) = rec[k][+0x58]` (total in group) and `*(B + 0x7B21 + 20·k + 60·r9) = rec[k][+0x59]` (**on-pitch in group**), with `r9 = [this+0x10C]`.
- For `k = 2`, `r9 = 1`: the byte `B+0x7B85 = T+0x87CD` (`B = T+0xC48`, CONFIRMED live `R12 = R15 = 0x1D47EB90`, `T0 = 0x1D47DF48`) = **on-pitch count of classes 7–9**, and `T+0x87CC` = their total count.
- Live: T+0x87CD = 1 (side 0) and 3 (side 1) over 6 s in run 2; never 2. The live recount through `this` (R14 = `0x1B65ED70` at the hit) was **not** possible: that object's class array reads zero outside the tick (INF: a per-tick copy).
- Caller and `this`: `this+0x110` is not the side (brain objects read 2 for both sides); `this+0x10C` is `r9` (39 byte-store sites in `0x1405EC095..0x1405FB2EE`, OPEN).

**Verdict:** T+0x87CD = count of on-pitch players whose role class is 7, 8 or 9 (INF high from role-writer classes 7/9 = SS/CF). **2 = exactly two such players on pitch** = the 2-CF gate (`0x14050117B CMP [RAX+0x87CD],2`). Sources: `this+0x128` (classes) and `MI+0x1748` (on-pitch). **OPEN:** the `this+0x10C` selector writer.

## 4. K+0x6E (DHB): writers, gate and live values (CONFIRMED)

`K = bp+0x78` (CONFIRMED live: builder entry `R9`). `K+0x6E = bp+0xE6`.

| VA of store | instruction | effect | watch IP | hits (25 s) |
|---|---|---|---|---|
| `0x14063A3F6` in `FUN_14063A3B0` | `mov %rax,0x6c(%rcx)` (clear) | zeroes K+0x6C..0x73 incl. K+0x6E | `0x14063A3FA` | 1,235 |
| `0x14063A5A9` | `movw $0x0101,0x6c(%rdi)` | K+0x6C = 1, K+0x6D = 1 | `0x14063A5AF` | 52 |
| `0x14063A610` | `movw $0,0x6c(%rdi)` | K+0x6C, K+0x6D = 0 | `0x14063A616` | 103 |
| **`0x14063AA5E`** | `mov %r13b,0x6e(%rdi)` | **K+0x6E = flag (r13b)** | `0x14063AA62` | 155 |
| `0x14063AB2F` | `mov %r13b,0x6f(%rdi)` | K+0x6F = flag (second) | `0x14063AB33` | 155 |
| `0x140AC0365` (copier, `FUN_140AC0280`, movzx at `0x140AC035E`) | `mov [rbx+0xE6],al` | K+0x6E = `*(src+0x90)` | not hit | 0 |

Function `FUN_14063A490(K, ?, carrier-index)` (bridge decompile `dec_63A490.c`): `K+0x6E = 0` at `LAB_14063aa5e`, then:
- if `K+0x6C == 0`: `K+0x6E = 1` when `(float)*(u8*)(player+0x31) / 100 < (float)K+0x44` (`player = FUN_14124DAD0(...)`, line 271); `K+0x6E = 1` also when `*(u8*)(ball+0x4C) ≠ 0` and the ball test (`FUN_140A6D0D0`, `FUN_140A8F890` distances) fails (lines 286–287).
- `ball = *(G+0x40)` (line 57).

Live (run 3, 40 s, 42,897 samples): `(K+0x6C=0, K+0x6E=0) 38,320`; `(K+0x6C=1, K+0x6E=0) 2,672`; `(K+0x6C=0, K+0x6E=1) 1,905`. **K+0x6E = 1 never occurs with K+0x6C = 1** (consistent with the gate).

Meaning (INF): "not at the carrier's feet and the carrier's height byte below the threshold" (volley/header-like). **OPEN:** `K+0x44` writers (lines 169/180/187/218/232), `FUN_14124DAD0`, and the copier's source `+0x90` (not hit in 25 s).

## 5. Corrections

- Run 2 (`pes-re-team-mode.md`, previous revision): the G4 operand was `TA+0x5A0 → src+0x48`; that was the team-AI snapshot path. The G4 operand is `bp+0x32B0+8` (§1). The writer `FUN_140A65E10` (zeroing `src+0x40/+0x48`) belongs to that other path and is not G4.
- Run 2 `T+0x87CD` writer instruction: the store is at `0x1405F9E5C`; the watch IP after it is `0x1405F9E64` (Main's correction). Run 2 also gave the first store (`mov %al`) as `0x1405F9E51` with IP `0x1405F9E55`: correct.
- `pes-re-lane-inputs.md` §4 and `pes-re-carrier-helpers.md` line 413: `0x141052E30` is a reader of `bp+0x32B0+8`, written by `FUN_1421197B0`.

## 6. Open

1. `IsClose(imageA+0x9CC)` and the corridor helpers, to finish the 4 and 1/2 decision (§2).
2. `B3C` (`bp+0xC28`) and `B58` (`bp+0xC44`) writers in `FUN_142119390` (§2).
3. `this+0x10C` selector writers for the T+0x87CD table (§3).
4. `K+0x44` threshold writers and `FUN_14124DAD0` (§4).
5. Copier `0x140AC0280` source byte `+0x90` and the copier's run window (§4).
6. Predicate `FUN_142128C10`: `param_1[6]` is 0 at entry in the one capture; the driver's direct `bp+0x32B0` read is the confirmed G4 operand.
