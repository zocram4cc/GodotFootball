# Team mode (G4 operand), K+0x6E (DHB) and T+0x87CD (2-CF gate): writers and live values (S1Watch, run 3, 19-10-26)

> **Main audit 19-10-26 (rev 3):** round-4 constants re-read by script: `0x14253EE30`=100.0, `0x142588210`=10.0, `0x142591140`=30.0, `0x14259A764`=60.0. K+0x6E: `0x14063AA5E` stores r13b which is always 0 (`xor r13d,r13d` at `0x14063A61C`); the flag is SET by `movb [rdi+0x6e],1` at `0x14063AA91` and `0x14063AB06` (both confirmed). Round 3's "flag store = 0x14063AA5E" reading is superseded. B3C byte store `0x141193DEA` `mov [rcx+0xb3c],al` confirmed. Porting note: the G4 clock gate reads the period at `MI+0x1168`; a port reads GF's own match period, and MI+0x1168 is never pinned.

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

## 7. Round 4: helpers, K+0x6E condition, T+0x87CD selector (supersedes §2 and §4 open items where noted)

Constants are script-read from `pes_full.bin` (`consts.py`, offset = VA − 0x140000000) and listed with the operand VA of the instruction that uses them. Decompile names are the bridge's local names.

### 7.1 IsClose (`FUN_140699820`, VA `0x140699820`, 15-line decompile, CONFIRMED)

Input `p = imageA+0x9CC = bp+0xAB8`. Rule:

`IsClose(p) = 1` iff `f32(p+0x170) >= 0` AND `( (u32(p+0x16C) & 1) != 0 OR ( i32(p+0x17C) < 3 AND i32(p+0x178) < 3 ) )` AND `i32(p+0x17C) == 0`; else 0.

It is **not** the GF ShootZone. Its fields are written by the R4 writer `FUN_140698D10(p, ...)` (VA `0x140698D10`, 305 lines, called from the image-A updater `FUN_142119390` at its line 44). R4 writes: `0x170` (goal-line distance in the attack frame, `pitch.x − carrier.x·sign·…` in the no-projection branch, else `FUN_140A8F820`), `0x174` (`FUN_14043A020`), `0x178` (class 0..3 from `0x198/0x19C` and `0x174`), `0x17C` (0 or 1..3 from `0x188/0x18C/0x194`), and the accumulators `0x188`, `0x18C` (`+= ramp(carrier byte +0x1A)`, `+= count`). The R4 internals are **partly decoded**: the constants below are its operands.

| VA | value | use (operand instruction) |
|---|---|---|
| `0x14258C264` | 3.0 | divisor of the updater test `(F[6]·DAT_14259a768)/3.0` (`comiss` in `FUN_142119390`, `dec` lines ~63–66) and `local_188` of the corridor-1 scan |
| `0x14259BE4C` | 70.0 | ramp lower band of the R4 byte ramp |
| `0x1425C48CC` | 99.0 | ramp upper band |
| `0x1425A9460` | 29.0 | ramp span `(x−70)/29` |
| `0x14258C258` | 15.672 | neighbour constant in R4 (not decoded) |
| `0x142588210` | 10.0 | `B58 + 10` (`0x142119929: addss 0x2588210(%rip),%xmm0`) |

### 7.2 Corridor branch (in `FUN_1421197B0`, CONFIRMED structure; gates and picks)

Order inside the writer (`dec_1421197B0.c`, asm-matched where cited):

1. `*(int*)(p_out+8) == 4` (`p_out = bp+0x32B0`): keep 4 (sticky, `dec` line 72).
2. `IsClose(bp+0xAB8)` AND `|wrap(f32(bp+0x2C) − f32(bp+0x4C))| < 100.0` → 4. `wrap`: if `|d| > 180` then `d −= sign(d)·360`. Constants: `180.0` `0x142598580`, `360.0` `0x14259858C`, `100.0` `0x14253EE30` (`comiss` of `dec` line 91).
3. Else 3 if: `prev == 3`, OR `F[6] <= sign·K.x` with `sign = f32(bp+0x30)`, `K.x = f32(K+0x40)` (ctx4 ball x, copied per tick), `F[6] = f32(*(G+0x2AA8)+0x18) = 36.14` (`dec` line 98: `pfVar2 = pfVar2 + 6`), OR `B3C <= B58 + 10.0` (`B58 = f32(bp+0xC44)`, `B3C = f32(bp+0xC28)`, `0x142119931 comiss 0xb3c(%rbx)`).
4. Else, if `T+4 < 2` AND `prev != 2` (`T = FUN_140A48A30(G, team)`, `dec` line 106):
   a. origin = carrier player `(x,z)` (`FUN_140A48A10(G, bp+0x1C)`, `+0x55C`/`+0x564`) moved `30.0` m (`0x142591140`) along `T+0x250` (folded to `[0,360)` with `0x14259858C`/`0x142598598`, `dec` line 112).
   b. corridor-1 = `FUN_14063D2F0` → `FUN_14063CD90` (lane scan; output `[0]` nearest id, `[1]` count), side flag `(team == 0)`. `count1 = FUN_1421B7D40(out1)`. **`count1 == 0` → 2.**
   c. goal and clock gate (`dec` line 168–174): `goals_other <= goals_own` OR `((period != 3 OR minute < 40) AND (period != 7 OR minute < 10))`. `period = *(MI+0x1168)` (`MI = *(G+0x270)`), `minute = FUN_140A6BD60()`, `goals_x = FUN_1408C9DF0(FUN_140A3E8A0(*(team+0xE0)) + team_x·0x59650, 5)`. **Fails → 2.** Constants: `40` = `0x28`, `10` (immediate).
   d. corridor-2 = `FUN_14063CD90(out2, bp+0x1C index, team, param_8 = (count1 == 0))` (here `param_8 = 0`). Scan constants from the writer's locals: `width = 5.0` (`0x14259222C`), `length = 100.0` (`0x142C80000` immediate, `local_128`), `local_124 = 90.0` (`0x14259BE5C`), `local_190 = 60.0` (`0x14259A764`), `local_188 = 3.0` (`0x14258C264`), `tempo = fps/3` (`FUN_141E532E0() = 54`, so 18). `count2 = FUN_1421B7D40(out2)`.
   e. `count2 <= 1` → 2. Else **1** if `T+0x134 > 2` OR `slider(T) != 1` (`FUN_140A62180(team, field 0)`) OR `2·fps <= bp+0x88` (`*(K+0x10)`, `fps = 54`, so `108`); otherwise **2**.
5. Else → 2.

There is **no** `MI+0x116C` gate in the writer. The in-play gate is `bp+0x10` in the caller (`FUN_142110D30`).

Live (run 3, 40 s): the 2 is the common in-play value; the 1 occurred twice in one 40-s window (both with `bp+0x10 = 1`). Consistent with step 4.
The `call 0x51D9E0` sites `0x1404F9F24`, `0x1404FA237`, `0x1404FA6DB` (in `FUN_1404F9DC0`'s region) and `0x1404F6EE8` (tick) are mapped to branches by the decompile (lines 62, 69, 213 of `dec` for the 9DC0 calls); the address-to-branch map of the two `0x1404FA…` sites is INF.

### 7.3 "Sticky" and "held"

- Sticky 4 and sticky 3 test the stored int itself (`*(p_out+8)`), so the state is the previous value at `bp+0x32B0+8`.
- Held: the writer is called only from `FUN_142110D30` when `*(bp+0x10) != 0` (`dec` line 33). When the byte is 0 the int is not rewritten.

### 7.4 B3C and B58 (`bp+0xC28`, `bp+0xC44`)

- Consumers: `FUN_1421197B0` (step 3 above) and the updater `FUN_142119390` (`0x142119931`: `movups 0xb58(%rbx)` then `addss 10.0`, `comiss 0xb3c`).
- Block layout (imageA = `bp+0xEC`): `+0xB38` flag byte, `+0xB3C` (B3C), `+0xB40` i32, `+0xB44` byte, `+0xB48` i32, and `+0xB58` (B58, a float read by `movups`).
- **B3C writer: `FUN_141193DC0(dst, rec16)`** (VA `0x141193DC0`, 15 lines; asm confirmed): `movb $1,0xB38(dst)`; `movss rec[4] → 0xB40(dst)`; **`mov %al, 0xB3C(dst)` (only the low byte of rec[0] is stored, `0x141193DEA`)**; `mov %al, 0xB44(dst)` (rec[8]); `movss rec[12] → 0xB48(dst)`. The consumer reads B3C as a float, so the stored width is not a full float: **OPEN (type of B3C)**.
- Caller chain: `FUN_141159840(obj, …)` (wrapper: `dst = *(obj+0x48)`, guarded by `*(obj+0x38) != 0`; its caller is not identified). The call is at `0x141159884`.
- **B58: no nonzero writer found.** The image-A block is zeroed by three reset functions: `FUN_141183710` (651 lines, caller `0x1411FC3EF`), `FUN_1411825E0` (664 lines, caller `0x1411FC37F`), `FUN_141180540` (1228 lines, 7 callers). `FUN_1411FC230` is the allocator (size `0x29B8`, `FUN_1403FB410(…, 0x29B8, 0)`), which calls `FUN_1411825E0`. Live B58 = 24.0 constant, so the nonzero store is outside these reset functions: **OPEN**.

### 7.5 K+0x6E: full flag condition (CONFIRMED asm `0x14063A490..AB40`)

`K = bp+0x78`; `P = r12 at 0x14063AA6C` (the carrier player, `FUN_140A489F0(G, bp+0x1C)`); `B = [rsp+0x70]` (the ball, `*(G+0x40)`).

1. `0x14063AA5E`: `mov %r13b,0x6e(%rdi)`. `r13b = 0` (`xor %r13d,%r13d` at `0x14063A61C`; the only assignment on every path). So **K+0x6E is cleared here**.
2. `0x14063AA62`: `cmp %r13b,0x6c(%rdi); jne 0x14063AB0C`. **If K+0x6C != 0 the function skips everything below.**
3. `FUN_14124DAD0(P) = P+0x70`. Condition A (`0x14063AA8C comiss`): `K+0x44 (f32) > byte(P+0x70+0x31)/100.0` (`100.0` = `0x14253EE30`) → **`0x14063AA91: movb $1,0x6e(%rdi)`** (K+0x6E = 1).
4. Otherwise (no A): `r14b = byte(B+0x80)`. If `r14b == 0` AND `byte(B+0x4C) != 0` (`FUN_140A6D0D0(B) = B+0x4C`): `d1 = |pos(B) − FUN_140A6CF30(B,0)|²`, `d2 = |pos(B) − rsi|²` (`rsi` = the function's second argument, a point), `r14 = (d2 > d1)`.
5. `0x14063AB01`: if `byte(B+0x4C) != 0` AND `r14 == 0` → **`0x14063AB06: movb $1,0x6e(%rdi)`**.
6. `0x14063AB2F`: `mov %r13b,0x6f(%rdi)` (K+0x6F = 0). 

So `K+0x6E = 1` iff `K+0x6C == 0` AND (A: `K+0x44 > P.byte31/100`, OR B: `B+0x4C != 0` AND NOT(`d2 > d1`, computed only if `B+0x80 == 0`)). Live check of the stored bytes: K+0x6E=1 only with K+0x6C=0 (1,905 of 1,905).

The copier `FUN_140AC0280` (VA `0x140AC0280`, 50 lines) writes `bp+0xE6` (= K+0x6E) from `*(*(bp+0xC8)+0x90)`. It runs only through `FUN_140ABF720` (VA `0x140ABF720`, 43 lines), which returns 0 unless **all eight pointers at `bp+0x90, 0x98, …, 0xC8` have `+0xB4 != 0`** (`dec` lines 4–12), and then calls `FUN_140E3D790`, `FUN_140E40A20`, `FUN_140E52480`, `FUN_140E525A0` before `FUN_140AC0280(bp)`. Live: 0 hits of `FUN_140ABF720` and of the copier in 8 s of active play (`gate.data`, `copier.data`). The copier's source object (`*(bp+0xC8)`) is OPEN. So its K+0x6E write is rare, and **the per-tick flag is FUN_14063A490's**.

### 7.6 T+0x87CD selector `[this+0x10C]` and the role groups (CONFIRMED structure; INF on the class meaning)

Chain (decompile + asm):

- `FUN_1405F9CD0(this, side, B, rec)` writes `B+0x7B20 + 20k + 60·r9` (total) and `B+0x7B21 + 20k + 60·r9` (on-pitch), with `r9 = byte(this+0x10C)` (`0x1405F9E20`). **T+0x87CD = on-pitch count of group k=2 when r9 = 1** (`B+0x7B85`, `B = T+0xC48`).
- `this+0x10C` is stored by `FUN_1405EBFA0(ctx, p2, team, sel)` (`dec` line 43: `*(char*)(ctx+0x10C) = param_4`; VA `0x1405EBFA0`), called by `FUN_1405EC600(…, side, T, sel, …)` (`dec` line 116), called by `FUN_14051D9E0(param_1, p2, sel, p4, p5)` (VA `0x14051D9E0`, the role-rebuild routine; `FUN_1405EC600(param_1, 0/1, p2, sel)` for each side).
- Init: `FUN_14051F490` writes `+0x10C = 2` (`0x14051F5A7`, `movl $2`), so `r9 = 2` (rows 6–8) until the first build.
- **Selector values** (the four callers of `FUN_14051D9E0`, bridge decompiles):
  - Team tick restart branch `0x1404F6EE8`: `sel = (T+0x255 != 0)` (`tick.c` line 243, `bVar22`).
  - `FUN_1404F9DC0` (step 37, parity dispatcher), `param_2 == 0`: `sel = (T+0x255 != 0)` (`dec` line 62).
  - `FUN_1404F9DC0`, `param_2 != 0` and `MI+0x1198 & 1 == 0`: `sel = (T+0x255 == 0)` (`dec` line 69; asm `0x14051…` `sete`).
  - `FUN_1404F9DC0`, `param_2 != 0` and `MI+0x1198 & 1 != 0`: `sel = (T+0x255 != 0)` (`dec` line 213).
  
  So `sel` = **the team's in-possession latch `T+0x255`** (1 = possession), with the sense inverted on one odd-frame branch. Rows: `sel = 1` writes `n = k+3` (k = 0..2), `sel = 0` writes `n = k`.
- **Consequence:** `T+0x87CD` is the on-pitch count of group 2 **at the last build made while the team had possession**. The 2-CF gate reads that count.

**Role groups (cross-check with `docs/re/pes-re-role-writer.md` §5, lines 143–144):**

| formation role | class (role-writer) | group (`FUN_1405F9CD0` switch) |
|---|---|---|
| 0 GK | 0 | 0 |
| 1 CB | 1 | 0 |
| 2 LB, 3 RB | 2 | 0 |
| 4 DMF | 3 | 1 |
| 5 CMF | 4 | 1 |
| 6 LMF, 7 RMF | 5 | 1 |
| 8 AMF | 6 | 1 |
| 9 LWF, 10 RWF | **8 (WF)** | **2** |
| 11 SS | **7** | **2** |
| 12 CF | **9** | **2** |

So group 2 = {SS, LWF/RWF, CF} = the four forward roles 9–12. AMF (class 6) is group 1. This matches the stamina note (`SS=7`, `WF=8`).

The class array read by `FUN_1405F9CD0` is `*(this+0x128)` (11 ints, one per slot). `FUN_1405EBFA0` leaves `+0x128` = 0, so it is set after the ctx build (INF: array A `S+0x3F8C`, role-writer §3). The on-pitch flag is `MI+0x1748 + team·11 + slot` (CONFIRMED).

### 7.7 Corrections to earlier text

- §4 "K+0x6E writers": the store at `0x14063AA5E` writes **0** (`r13b = 0`), and the flag 1 is written at `0x14063AA91` and `0x14063AB06`. The earlier "flag store = r13b" is correct only as "r13b is 0".
- Constant `0x14259A764` = **60.0** (not 35.0).
- The `[this+0x10C]` selector was not the side index: `brain+0x110` = 2 for both brains. It is the in-possession latch (§7.6).
- Mode writer (`FUN_1421197B0`) values: 1, 2, 3, 4 only (no 0 store).

### 7.8 Still open

1. R4 (`FUN_140698D10`) internal branches and the `0x16C` bit-0 writer (`FUN_140699610`).
2. Corridor scan internals (`FUN_14063CD90`, `FUN_140A900E0`, `FUN_1421B7D40`); the lane geometry of the 'length 100 / width 5' scan.
3. The B3C record source (`FUN_141159840`'s caller) and B58's nonzero writer (§7.4).
4. K+0x44 writers (lines 169/180/187/218/232 of `dec_63A490.c`) and `FUN_14124DAD0`'s `P+0x70` object.
5. The copier gate's caller and `*(bp+0xC8)`.
6. The class array pointer `this+0x128` (which array, A or B).
