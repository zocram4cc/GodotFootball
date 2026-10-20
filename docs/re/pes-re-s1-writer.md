# PES S1 rows: writer, per-bit contents, fits and row-age cause (S1Watch, run 2, 19-10-26)

> **Main audit 19-10-26 (rev 2):** call `0x140457119 -> 0x14045B420`, `rep stos` at `0x14045B582`, the stores at `0x14045B6AC` and `0x14045BA11/1B`, and all 7 floats and the u64 were re-read by script and match. The -0.0 load is `movss xmm8,[0x1425656D0]` at `0x14045B4D5` (F3 prefix); Main's earlier "4D6" correction was a misdecode and is withdrawn. S1 runs from the listener step-5 path, NOT the team tick `FUN_1404F6C30`. A_tab (`MA+0x22F7C`) is unresolved: do not port it.

Read-only. Live reads via `/proc/<pid>/mem` (`sudo -n`), `perf` hardware write watches on JobGraph worker threads (no stop, no pin, no write), `perf` CPU sampling. No PES memory written. No GF edits. Scratch: `<workdrive>/gf-S1Watch-scratch/` (`pesmem.py`, `s1live.py`, `fitrun.py`, `lagrun.py`, `agerun.py`, `atabscan.py`, `intick.py`, `active.py`, `tids.sh`, `dis_45B420_full.txt`, `dec_45B6AC.c`, `dec_457070.c`, `dec_45B090.c`, `bp*.data`, `modew*.data`, `t87*.data`).

Tags: **CONFIRMED** = live read or hardware-watch capture, raw disassembly, or bridge decompile, with the evidence named. **INF** = inference. **OPEN** = blocker named.

## 0. Summary

1. **Writer:** `FUN_14045B420` (entry `0x14045B420`). Called from the team-AI tick `FUN_140457070` at `0x140457119` (`rcx = this`, `edx = mask`). CONFIRMED.
2. **Mask (CONFIRMED):** `0xFF` when the flag argument is non-zero. Otherwise `mask = {MI+0x1198 & 3 → 0: 0x01, 1: 0x02, 2: 0x24, 3: 0x10}`. Only bits `0x01`, `0x02` and `0x04` are tested in the body (`test $1/$2/$4`, decompile lines 68, 162, 257). Bits `0x08`, `0x10`, `0x20` gate nothing.
3. **What each frame writes (CONFIRMED live, 4-frame histogram of block changes over 12 s):**
   - `mask 0x01`: score0, list0, ang0, NL.
   - `mask 0x02`: score1, list1, ang1, NL.
   - `mask 0x24`: pairA, pairB, NL.
   - `mask 0x10`: NL only.
   - NearestList (NL) and A_tab are rebuilt on every call (ungated).
4. **Every block except A_tab is an exact function of the goal point and the on-pitch positions, at the row's own write time** (§2). The 4–15 d² error on `MA+0x228EC` is **row age**: the row is refreshed every 4 frames, and the positions move between refreshes. At age 0 the error is 4e-4 d²; at age 3–4 it is 19–29 d² median (§3). The pinned cause is therefore not a formula error.
5. **Pair rows (`MA+0x22CBC`, `MA+0x22D84`) are `|Y − z_i|` and `|Y + z_i|`**, with `Y = goal.y` (script-read 34.06). The four tested points were the wrong shape: these are 1-D distances in z, not 2-D points. Exact to 2e-6.
6. **NearestList `MA+0x22E4C + 200·i`:** owner `i`'s row = all other on-pitch players `j` with `d²(i,j)`, sorted ascending, team split (`j ≥ 11` → team 1). Exact (max 1.2e-3 over 272 entries).
7. **`MA+0x22F7C` (A_tab) is dead or transient (INF high).** It lies entirely inside NearestList rows 1–11 (§5). The live bytes there are NL data. No bearing table exists in memory. Port: do not port A_tab; keep NL and the angle rows.
8. **The "0x18 disagreement" is an addressing error in my earlier report, now closed.** `0x7FFFFB022AD8` is `MA+0x22AC0`, not `MA+0x22AD8`. The angle block starts at `MA+0x22A7C` (rep-stos start `0x7FFFFB022A94`, CONFIRMED from the register dump). The store `0x14045B6AC` with `R15 = 0x7FFFFB022AD8` is angle index 17 (`0x22A7C + 4·17`).
9. **`-0.0` load: instruction at `0x14045B4D5`** (`movss 0x210A1F2(%rip),%xmm8`, bytes `F3 44 0F 10 05 F2 A1 10 02`; `F3` prefix at `4D5`, REX at `4D6`). Operand `0x14045B4DE + 0x210A1F2 = 0x1425656D0` = `-0.0` (objdump comment; image bits `0x80000000`). Main confirmed 4D5 and withdrew the 4D6 correction.

## 1. MA resolution (CONFIRMED, re-run this session)

`M = *(0x1436F3F78) = 0x32E5D5F0`; `W = *(M+0x118) = 0x0A3EFD70`; `blk = *(W+0x10) = 0x7FFFFB000010`; `MA = blk+8 = 0x7FFFFB000018`. `ma_valid` (per-team rank bytes at `+0xB2` are permutations of `0..cnt-1`, `10` absent) holds.

The team-AI object `TA = X+0x10` (listener `P`, `X = *(P+0x40)`) is `0x1C698E70` (vtable `0x14259D5B8`, unique in memory). `TA+0x508 = MA` **only inside a tick**; outside it reads 0. Same for `TA+0x5A0` (§mode in `pes-re-team-mode.md`).

## 2. Blocks (CONFIRMED formulas; live fits)

Goal point: `gp = *(G+0x2AA8)` (`G = *(0x1436F3DC8)`), `xg = f32(gp+0) = 52.5600`, `yg = f32(gp+4) = 34.0600`. `zg = f32(gp+8) = 3.6600` is not used by any S1 block. Player `i`: `pl = *(G+0x1DD8+0x28·i)`, `x = f32(pl+0x55C)`, `z = f32(pl+0x564)`. On-pitch flag: `MI+0x1748+i`.

Entry layout (all 200-byte rows, `FUN_1404587F0`/`FUN_140458810` are row helpers): team `t = (i ≥ 11)`; `k`-th entry at `+0x58·t + 8·k`: `u32 id`, `f32 v`; count at `+0xB0+t`; rank byte at `+0xB2+id`; sorted by `FUN_140A79640`.

| block | MA-rel | mask | content (formula) | live fit at write time (age 0) |
|---|---|---|---|---|
| score0 | `0x228EC` | 0x01 | `v = (−xg − x_i)² + z_i²` | median 4e-4 d² (n = 3102) |
| score1 | `0x22AD4` | 0x02 | `v = (+xg − x_i)² + z_i²` | median 8e-4 d² (n = 3347) |
| list0 | `0x229B4` | 0x01 | `v = |−xg − x_i|` | 4e-6 |
| list1 | `0x22B9C` | 0x02 | `v = |+xg − x_i|` | 4e-6 |
| ang0 | `0x22A7C + 4·i` (22 f32) | 0x01 | `v = deg(atan2(x_i + xg, z_i))`, wrapped to `[0,360)`; `0` if `z_i = 0 ∧ x_i = −xg` | median 0.05° |
| ang1 | `0x22C64 + 4·i` | 0x02 | `v = deg(atan2(x_i − xg, z_i))` wrapped; `0` at `(xg,0)` | median 0.07° |
| pairA | `0x22CBC` | 0x04 | `v = |yg − z_i|` | 2e-6 |
| pairB | `0x22D84` | 0x04 | `v = |−yg − z_i| = |yg + z_i|` | 2e-6 |
| NL row i | `0x22E4C + 200·i` | every call | `v = d²(i,j) = (x_i−x_j)² + (z_i−z_j)²` for on-pitch `j ≠ i`, sorted | max 1.2e-3 over 272 entries |
| A_tab | `0x22F7C + 0x58·i + 4·j` | every call | transient, overwritten by NL (§5) | **dead / transient** |

The angle wrap (constants `180`, `360`, `−360`, §4) belongs to the owner-loop second pass that writes A_tab (§5), not to bit 1.

Note on bit 0 vs bit 1: the decompile's `Q` for bit 0 is `(−xg, 0)` (the XOR with `−0.0` at `0x14045B4D5`); bit 1 uses `+xg`.

## 3. Cause of the 4–15 d² error on `MA+0x228EC`: row age (CONFIRMED)

`agerun.py` (15 s, MI≠0): for each score row sample, error vs the positions of the **same** sample, grouped by position-states since the row last changed:

| age (position states since row write) | score0 median d² (n) | score0 max | score1 median d² (n) | score1 max |
|---|---|---|---|---|
| 0 | 0.0004 (3102) | 8.9 | 0.0008 (3347) | 14.0 |
| 1 | 7.8 (2313) | 2331 | 11.1 (2319) | 3808 |
| 2 | 9.6 (2842) | 2479 | 14.2 (2435) | 6863 |
| 3 | 18.7 (2177) | 49.9 | 25.2 (2544) | 51.9 |
| 4 | 21.2 (1921) | 53.4 | 28.9 (2002) | 69.7 |
| 5 | 26.1 (945) | 66.4 | 34.7 (770) | 69.2 |
| 6 | 20.5 (346) | 70.2 | 36.4 (227) | 71.7 |

The max values at age ≥1 come from players that moved far between refreshes. The 4–15 d² in the earlier report is the age-1..3 median. `lagrun.py` (12 s): at every row-change event, the best position lag is **0** for every block, and the error at lag 0 is exact (`s0` median 6e-4, `s1` 6.5e-4, `l0`/`l1` 4e-6, `pA`/`pB` 2e-6). ~85 position states/s observed.

## 4. Floats (script-read from `pes_full.bin`, `offset = VA − 0x140000000`, `floats.py`)

| VA | bits | value | operand instruction (VA, asm) |
|---|---|---|---|
| `0x14259BE40` | `0x42652EE0` | `57.2957763671875` (180/π) | `0x14045B4F5 movss 0x2140942(%rip),%xmm12` (operand VA `0x14259BE40`); used by `mulss` at `0x14045B6A7` and at the second loop |
| `0x1425795E4` | `0x40C90FDB` | `6.2831854820251465` (2π) | `0x14045B4E8 movss 0x211E0F3(%rip),%xmm11` (operand `0x1425795E4`) |
| `0x1425656D0` | `0x80000000` | `-0.0` (sign XOR for `−Q.x`) | `0x14045B4D5 movss 0x210A1F2(%rip),%xmm8` (operand `0x1425656D0`); instruction starts at `4D5`, `4D6` is REX |
| `0x142565D30` | u64 `0x7FFFFFFFFFFFFFFF` (abs) | — | `0x14045B4C6 movsd 0x210A862(%rip),%xmm7` (operand `0x142565D30`) |
| `0x142598580` | `0x43340000` | `180.0` (wrap fold) | `0x14045BB2D movss 0x213CA4A(%rip),%xmm10` (operand `0x142598580`) |
| `0x14259858C` | `0x43B40000` | `360.0` (wrap) | `0x14045BB3D movss 0x213CA47(%rip),%xmm7` (operand `0x14259858C`) |
| `0x142598598` | `0xC3B40000` | `-360.0` (wrap) | `0x14045BB4A movss 0x213CA45(%rip),%xmm8` (operand `0x142598598`) |
| `0x14260B394` | `0xBFC90FDB` | `-π/2` | `FUN_140A92D40` zero-x branch |
| `0x1425A35A0` | `0x3FC90FDB` | `+π/2` | `FUN_140A92D40` zero-x branch |
| `0x142609F98` | `0xC0490FDB` | `-π` | `FUN_140A92D40` quadrant fix |
| `0x142578480` | `0x40490FDB` | `+π` | `FUN_140A92D40` quadrant fix |
| `0x142565600` | `0x3F000000` | `0.5` | (not used in S1 blocks; listed for the pair-row path check) |

Goal point values are live reads: `*(G+0x2AA8)+0 = 52.56`, `+4 = 34.06`, `+8 = 3.66`.

Wrap rule (owner-loop second pass that writes A_tab; CONFIRMED by constants): `v = A + 180`; `if v ≥ 0 { if v ≥ 360 { v += −360 } } else { v += 360 }`.

## 5. A_tab `MA+0x22F7C`: settled as dead or transient (run 3, INF high)

- Layout (asm `0x14045BB36 lea 0x16A8(%rsi),%rcx`, `rsi = MA+0x228D4`; writes `0x14045BD43` owner loop): A_tab = `MA+0x22F7C + 0x58·i + 4·j`, 22 owners × 0x58 = `0x7D0` bytes, i.e. `MA+0x22F7C..MA+0x2374C`.
- NearestList rows (stride 200, 22 rows, `0x22E4C..0x23F7C` = `0x1130`) cover that range: row 1 starts at `0x22F14`; row 11 spans `0x236E4..0x237AC`. A_tab lies **entirely inside NL rows 1–11**. Overlap is arithmetic, not a guess.
- Live: the bytes at `MA+0x22F7C` are NL data at every sampled call (NL exact, §2). No 22×0x58 bearing table exists anywhere in `MA+0x10000..0x28000` (`atabscan.py`, 80 pairs, 3°). No absolute immediate `0x22F7C` in the image (scan: 0 hits). `disp32 0x16A8` occurs 114 times in `0x1400..0x1426`; the two in `FUN_14045B420` are the only writers of this base (the others are other objects; not all checked).
- The second loop reads A_tab for the wrapped value and writes it back into the same bytes; no output of the function depends on it (NL and rows carry the only stored results). The rows the second loop reads (`A[j][i]` for `j ≠ i`) are overwritten by the NL rebuild in the same call.
- **Verdict (INF high): A_tab is a transient scratch area that the NearestList rebuild overwrites. It is dead as a stored table.** Port statement: do **not** port a 22×0x58 A_tab; the bearing values exist only inside the call. The port keeps NL (`d²`, sorted, team split) and the angle rows (`ang0`/`ang1`, `MA+0x22A7C`/`MA+0x22C64`), which are stored and exact.
- Residual risk: if a reader outside `FUN_14045B420` uses `MA+0x228D4+0x16A8` after the call, A_tab would be live; the scan did not check the 112 other disp `0x16A8` sites.


## 6. Tick placement and call chain (unchanged; CONFIRMED)

```
listener P (vt 0x14259A350) slot 2 = FUN_14042B9F0      [record type 2]
  step 5 = FUN_140426760(P+0x40)                          [pes-re-tick-order.md]
    FUN_1404582C0(TA)                                     [call 0x14042678E]
      FUN_140458880(TA)                                   [call 0x14045832B]
        FUN_140457070(TA, flag)                           [call 0x140458940]
          FUN_14045DF70()   if flag != 0                  [call 0x14045708B]
          FUN_14045B420(TA, mask)                         [call 0x140457119]   <- writer
          FUN_14045B090(TA)  time-sliced                  [call 0x140457568]   <- 0x2470C, 0x247D4
          FUN_14045E0D0(TA)  gated                        [call 0x1404575FE]
```

The team tick `FUN_1404F6C30` does not write these rows (unchanged).

## 7. Hardware-watch notes (CONFIRMED)

- Writes to `MA+0x228EC..0x22CBC` seen at `0x14045B582` (`rep stos`, start `MA+0x22A7C`), `0x14045B6AC` (angle store), `0x14045BA11/BA1B` (rank `0x0A` fill), `0x140A79AA4..AE1` (sorter).
- Cadence: `MA+0x22AC0` (angle index 17) written every ~70 ms (4 frames).

## 8. Open

1. **A_tab (`MA+0x22F7C`)**: settled as transient/dead (§5). Residual: one unchecked outside reader of base `MA+0x228D4` disp `0x16A8`.
2. **Mask bits `0x08/0x10/0x20`** are not tested in the function. The `0x10` frame writes only NL and A_tab. Confirm the caller passes no other bit.
3. **Zero-argument flag path** (`mask 0xFF`): not traced live (not observed in this window).
4. **`G → listener P`** and **`TA+0x508` setter**: still OPEN (unchanged from run 1).
