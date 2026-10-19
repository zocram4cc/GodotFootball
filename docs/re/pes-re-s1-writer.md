# PES S1 score rows: live writer found (S1Watch, 19-10-26)

> **Main audit 19-10-26:** call `0x140457119 -> 0x14045B420`, `rep stos` at `0x14045B582`, the angle store at `0x14045B6AC`, the rank fills at `0x14045BA11/1B`, and all 7 floats and the u64 were re-read by script and match. Correction: the -0.0 load (`movups xmm8,[0x1425656D0]`) is at `0x14045B4D6`, not `0x14045B4D5`. Main also notes: S1 runs from the listener step 5 path, NOT the team tick `FUN_1404F6C30`, so team-tick slices do not cover it.

Scope: writer of the S1 score/list/angle rows `MA+0x228EC..0x22CBC` and the NearestList `MA+0x22E4C`. Read-only: `/proc/<pid>/mem` pread via `sudo -n`, `perf` hardware breakpoints (no ptrace attach, no thread stop), `perf` CPU sampling. No PES memory written, no pins, no GF edits. Scratch: `<workdrive>/gf-S1Watch-scratch/` (`pesmem.py`, `resolve.py`, `chainM.py`, `e8scan.py`, `scanVT.py`, `scanMA.py`, `sample_rows2.py`, `corr2.py`, `verifyS1.py`, `checkQ.py`, `floats.py`, `dec_*.c` bridge decompiles, `dis_45B090.txt`, `bp1.data`, `bp2.data`).

Tags: **CONFIRMED** = live read, hardware-watch capture, raw disassembly or bridge decompile, with the evidence named. **INF** = inference. **OPEN** = blocker named.

## 0. Verdict

1. **Writer function (CONFIRMED by decompile and call-site disassembly): `FUN_14045B420`** (entry `0x14045B420`, frame `0x190`). It writes the S1 blocks for the bits of its `param_2` mask: score/list/angle triples at `MA+0x228EC` (bit 0), `MA+0x22AD4` (bit 1) and the pair/NearestList area `MA+0x22CBC`, `MA+0x22D84`, `MA+0x22E4C` (bits 2–5, decompile only). Its three inputs are the team-AI object `this`, the match context `ctx` (built from `G`), and the mask.
2. **Caller and mask (CONFIRMED, raw asm + decompile of `FUN_140457070`):** `0x140457119 call 0x45b420` with `rcx = this`, `edx = mask`. Mask = `0xFF` when the flag argument is non-zero; otherwise it rotates on the match frame counter `MI+0x1198 & 3`: **0→0x01, 1→0x02, 2→0x24, 3→0x10**. The hardware-watch hits fall every ~70 ms (≈ 4 ticks), which matches that rotation.
3. **Live row semantics (CONFIRMED numerically):** the score rows hold the squared distance from each on-pitch player to a goal-centre point on the z = 0 line: `MA+0x22AD4` (bit 1) to `(+52.56, 0)` (error 0.0 on one snapshot, 17 entries); `MA+0x228EC` (bit 0) to `(−52.56, 0)` (mean |error| 4.3 and 14.8 d² units over 17 entries on two snapshots, so the match is close but not exact; cause not pinned, INF timing). The point is `*(G+0x2AA8)`, i.e. `x = +52.56`, `z = +3.66` (script-read, §2).
4. **The pair row `MA+0x22CBC` does not match any of the four goal points tested** (`(±x, 0)`, `(±x, ±z)`): mean error ~3000 d². Its point is OPEN.
5. **MA resolved live**, by two chains that agree (§1).
6. **The team tick `FUN_1404F6C30` is not the writer's caller.** The writer is driven from the match listener's type-2 record (step 5 `FUN_140426760`), through the team-AI tick. Its position relative to the team tick is INF (§4).

## 1. MA resolution (script output)

Chain A (LivePoke chain, `pes-re-live-chains` in `tasks/14-10-26.md`), run by `chainM.py`:
```
M = *(0x1436F3F78)           = 0x32E5D5F0      (CONFIRMED)
W = *(M+0x118)               = 0x0A3EFD70      vt(W) = 0x14259D4F0 (CONFIRMED, matches doc)
blk = *(W+0x10)              = 0x7FFFFB000010
MA = blk+8                   = 0x7FFFFB000018  (CONFIRMED live)
```
Validation of the MA contents (`chainM.py`): per team, rank bytes `+0xB2` are a permutation of `0..cnt-1` with `10` = absent; ids ascend by d²; NearestList owner 0: `cnt=(10,11)`, owner 21: `cnt=(7,9)`. (My earlier whole-22 permutation test was wrong: ranks are per team, so it was discarded.)

Chain B (team-AI, traced through the match listener), run by `findP.py`, `scanVT.py`, `candTA.py`, `taWatch.py`:
```
G = *(0x1436F3DC8)                 = 0x41C14520        (CONFIRMED)
listener P: live instances with vt 0x14259A350 (one full-memory vtable scan) = 0x1FF54B70
X  = *(P+0x40)                     = 0x1CA69230        (CONFIRMED)
X+0x30 = W                         = 0x0A3EFD70        == Chain A W  (CONFIRMED)
TA = *(X+0x10)                     = 0x1C698E70        vt 0x14259D5B8, sub-vt(+0x4E8) 0x14259D590
TA+0x508 (MA slot)                 = 0x7FFFFB000018 in 18 of 764 samples over 8 s, 0 otherwise
```
- `TA+0x508` is **set only in active windows**. The rest of the time it is 0, and `G+0x270 (MI)` is 0 too.
- The builder's `this` is `TA`: in the hardware-watch register dump at the `rep stos` hit (`0x14045B582`), `RBP = 0x1C698E70` (CONFIRMED, `bp2.data`). The same dump shows `R8/R14/R15` at `MA+0x228EC/+0x22904/+0x22A94`.
- The H-brain path from the docs does **not** give MA: `brain_s = *(H+0x450+8s)` with `H = *(0x1436F3F98) = 0x2C025400` gives `brain+0x6D48 = H+0x488` (the frame struct S, CONFIRMED) but `brain+0x508 = 0xFF`. The docs' `H = M+0x788` is false in this session (`M+0x788 = 0x32E5DD78`).
- Open: how `G` reaches the listener `P` (only the vtable scan found it). This is the only unresolved link in chain B.

## 2. Inputs (script-read, CONFIRMED against the decompile)

The builder's `ctx` is built by `FUN_1404566A0` from `G` (CONFIRMED, decompile): `ctx[0] = *(G+0x270) = MI`, `ctx[2] = *(G+0x2AA8)` (the goal-point pointer), `ctx[4] = *(G+0x40)` (ball), `ctx[8+i] = FUN_140A48A10(G,i) = *(G+0x1DD8+0x28·i)` (player i), `ctx[0x4A+2s] = FUN_140A48A30(G,s) = *(G+0x1928+0x28·s)` (team record s).

- **Player table: `G+0x1DD8+0x28·i`, not `G+0x1EF8`** as `pes-re-teamtick.md` §1 says. The decompile of `FUN_140A48A10` gives `(i+0xBF)*0x28` (CONFIRMED); live: 22 valid players, `x = +0x55C`, `z = +0x564`.
- **Goal point** `P_g = *(G+0x2AA8)` (CONFIRMED live): `x = f32(P_g+0) = 52.5600`, `y = f32(P_g+4) = 34.0600`, `z = f32(P_g+8) = 3.6600`. Bits 0 and 1 use `(±x, 0)`; the z is not used there.
- **MI frame counter** `MI+0x1198 & 3` picks the mask (CONFIRMED, `0x4570E2..0x457100`).
- **On-pitch flag** `MI+0x1748 + i` (decompile `FUN_14045B420`, `*(char*)(lVar11 + 0x1748 + uVar15)`; INF that `lVar11` = MI).

Per-entry formulas (CONFIRMED, decompile of `FUN_14045B420` bit 0 and bit 1 paths):
- id store: entry index `idx = count[t] + 11·t`, `t = (i ≥ 11)`; `id` at `row + 8·idx`, `d²` at `row + 8·idx + 4`; `count[t]` at `row+0xB0+t` is incremented per entry. The row is the NearestEntry layout (`team0 11×{u32 id, f32 d²}`, `team1` at `+0x58`, counts `+0xB0/+0xB1`, rank `+0xB2+id`).
- d²: `(Q.x − x_i)² + (0 − z_i)²` with `Q = (−P_g.x, 0)` for bit 0 and `(+P_g.x, 0)` for bit 1 (the XOR with `−0.0` flips the sign).
- list: `|Q.x − x_i|` (double → float, abs by mask `0x7FFFFFFFFFFFFFFF`).
- angle: `atan2(x_i − Q.x, z_i)` via `FUN_140A92D40`, if `< 0` add `2π`, then × `57.29578`, zero if `(x_i,z_i) = Q`.
- rank: `FUN_140A79640(row, 0)` sorts the entries; the sorter stores are seen live at `0x140A79AA4`, `0x140A79AAB`, `0x140A79AE1`.

## 3. Writer VAs (hardware-watch captured, `bp1.data`, `bp2.data`; disassembly `dis_45B090.txt`)

Method: `perf record -e mem:<VA>:w -t <tid list>` on the four worker threads (`JobGraph::worker`, tids 2511634–2511637). The hits are the IPs after the writing instruction, except for `rep`, where the IP is the `rep` itself. No thread is stopped; the kernel takes only the debug-register slot. **Note:** `perf -p <pid>` (all 188 threads) fails with ENOSPC, so the writer thread was found first by CPU sampling (`samp1.data`, 92,980 samples), which put the writes on those four threads.

| VA (IP) | raw instruction | captured as | role |
|---|---|---|---|
| `0x14045B582` | `f3 ab  rep stos %eax,(%rdi)` (22 dwords) | watch `0x7FFFFB022AD8`, 42 hits; DI = `…22ADC` at capture | angle-run zero fill |
| `0x14045B6AC` | `f3 41 0f 11 07  movss %xmm0,(%r15)` | IP `0x14045B6B1`, R15 = `…22AD8`, 42 hits | angle store (xmm0 = `atan2·57.3`) |
| `0x14045BA11` | `4c 89 b3 b2 00 00 00  mov %r14,0xb2(%rbx)` | IP `0x14045BA18`, R14 = `0x0A0A0A0A0A0A0A0A`, 41 hits | rank `0x0A` fill (8 bytes) |
| `0x14045BA1B` | `4c 89 b3 ba 00 00 00  mov %r14,0xba(%rbx)` | IP `0x14045BA22`, 41 hits | rank `0x0A` fill |
| `0x140A79AA4..AE1` | sorter stores in `FUN_140A79640` | 41–82 hits each | rank/sort writes into the row |
| `0x14045B642` / `0x14045B652` | `mov %edi,(%r14,%rax,8)` / `movss %xmm1,0x4(%r14,%rax,8)` | **not captured** (disassembly only) | id / d² entry stores; d² from `(xmm1)` = `(Δx)²+(Δz)²` on `rsi` = player, `0x55C/0x564` |

Watch-to-row attribution (INF): the `0x22AD8` watch is written by an angle store (`0x14045B6AC`) and by a zero fill (`0x14045B582`). The decompile puts the angle run of bit 0 at `MA+0x22A7C`, not `…22A94`, and the register dump shows `R15 = MA+0x22A94` (k = 17 is `…22AD8`). There is an 0x18 offset disagreement between the asm base and the decompile base that I could not close. Which block owns `0x22AD8` is OPEN.

Cadence (CONFIRMED, watch timestamps): 0x22AD8 hits at 843471.267, .338, .407, .480, .548, .617, .687, .758, .829, .898 (≈70 ms apart, ~4 ticks at 54 Hz).

## 4. Call chain to the team tick

Each link is CONFIRMED by a decompile or raw call unless marked:
```
listener P (vt 0x14259A350), slot 2 = FUN_14042B9F0    [record type 2 = the tick]
  step 5 = FUN_140426760(X = P+0x40)                    [docs: pes-re-tick-order.md §2]
    FUN_1404582C0(TA = X+0x10)                          [call 0x14042678E]
      FUN_140458880(TA)                                 [call 0x14045832B]
        FUN_140457070(TA, flag)                         [call 0x140458940 inside 0x140458880]
          FUN_14045DF70()    if flag != 0               [call 0x14045708B]
          FUN_14045B420(TA, mask)                      [call 0x140457119]  <- S1 writer
          FUN_14045B090(TA)  time-sliced               [call 0x140457568]  <- ball list 0x2470C, angle 0x247D4, goal geometry 0x228DC..E8
          FUN_14045E0D0(TA)  gated                      [call 0x1404575FE]
```
- Team tick: `FUN_1404F6C30`, called from `H` (`FUN_14041ECF0`, vt `0x142597580`, slot 2) per side via `FUN_140A8EFF0`, delivered by the MatchSub listener in the same record (docs: `pes-re-tick-order.md` §1, §3, INF on the ordering).
- The team tick's body makes **no** store to `MA+0x22E4C` (`pes-re-teamtick.md` §2.2, re-read by me: no contradiction).
- **Position in the team tick (step index):** the writer is not one of the 56 team-tick steps. It runs in the listener's record-type-2 tick, step 5, before the team tick (INF; order of record delivery OPEN per `pes-re-tick-order.md` §3).

## 5. Floats (script-read from `pes_full.bin`, offset = VA − 0x140000000; `floats.py`)

| VA | bits | value | operand at the cited instruction |
|---|---|---|---|
| `0x14259BE40` | `0x42652EE0` | `57.2957763671875` (180/π) | `0x14045B4F5 movss 0x2140942(%rip),%xmm12` (= VA 0x14259BE40), used by `mulss` at `0x14045B6A7` |
| `0x1425795E4` | `0x40C90FDB` | `6.2831854820251465` (2π) | `0x14045B4E8 movss 0x211E0F3(%rip),%xmm11` (= VA 0x1425795E4) |
| `0x1425656D0` | `0x80000000` | `-0.0` (sign XOR) | `0x14045B4D5 movss 0x210A1F2(%rip),%xmm8` (= VA 0x1425656D0) |
| `0x142565D30` | u64 `0x7FFFFFFFFFFFFFFF` | abs mask (double) | `0x14045B4C6 movsd 0x210A862(%rip),%xmm7` (= VA 0x142565D30) |
| `0x14260B394` | `0xBFC90FDB` | `-1.5707963705062866` (−π/2) | `FUN_140A92D40` zero-x branch |
| `0x1425A35A0` | `0x3FC90FDB` | `1.5707963705062866` (+π/2) | `FUN_140A92D40` zero-x branch |
| `0x142609F98` | `0xC0490FDB` | `-3.1415927410125732` (−π) | `FUN_140A92D40` quadrant fix |
| `0x142578480` | `0x40490FDB` | `3.1415927410125732` (+π) | `FUN_140A92D40` quadrant fix |

The operand pairs are read from the objdump comments (`# 0x…`), which print image VAs without the `0x14` prefix. The goal-point float values are live reads, not image reads: `*(G+0x2AA8)+0 = 52.56`, `+8 = 3.66` (`goalpt.py`).

## 6. Verification evidence (raw)

```
checkQ.py  point x=52.560 z=3.660   (8 snapshots tried; 2 full ones)
  k1score  (-x,0): mean|err| 4.317 (17)     ; k0score (+x,0): 0.0 (17)
  pair     best (-x,-z): 3042.7 (17)         ; no candidate fits
verifyS1.py  k1score best vote -52.6, k0score +52.6, pair none (3 snapshots)
sample_rows2 (4 s): 431 change events; 0x228D4..0x23F7C written in bursts, min gap 0.24 ms, median 13 ms
```
The "mean |err|" for `k1score` is not zero, so the k1 point match is approximate. The most likely cause is that the row and the player positions were read in different ticks, but that is INF.

## 7. Not done / unconfirmed

- **`K+0x6E` (DHB) and `T+0x87CD`:** not attempted (time).
- **Block owner of `0x22AD8`:** OPEN (§3, 0x18 discrepancy).
- **Bits 2–5 (pair row `MA+0x22CBC/0x22D84`, NearestList `MA+0x22E4C`):** the builder code is decompiled (`dec_45B6AC.c` lines 334–467, `local_188 = lVar16+0x578 = MA+0x22E4C`), but I did not read it through. The point of the pair row (`P_g` with z = 3.66, or the ball) is OPEN, and the `0x14045BA18` hits are rank fills of the pair row, not NearestList entries (INF).
- **Link `G → listener P`:** OPEN (only found by vtable scan).
- **Why `TA+0x508` is 0 outside active windows:** the setter of `TA+0x508` is not decoded; the observed on/off is CONFIRMED, the cause is OPEN.
- **`MI` frame counter:** `MI` is null outside active windows (CONFIRMED live), so the rotation runs only then.
- **Corrections to existing docs:** `pes-re-teamtick.md` §1 player table is `G+0x1DD8+0x28·i`, not `G+0x1EF8`; `H = M+0x788` does not hold in this session.

## 8. Next step (highest value)

1. Decode the bit 2–5 block of `FUN_14045B420` (`dec_45B6AC.c` lines 257–467) and watch `0x22CC0` and `0x22E54` with the same perf method to attribute the pair row and NearestList.
2. Find the setter of `TA+0x508` (`MA`): one hardware watch on `TA+0x508` with `perf -t` on the four worker threads, and capture the IP at the first active window.
