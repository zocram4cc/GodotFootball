# PES mentality lane line: the four leaves (LaneLeavesRE, 24-10-26)

Image `~/ghidra-projects/scratch/Unpacker/pes_full.bin` (file offset = VA − 0x140000000).
Bridge `http://127.0.0.1:8080`, one request at a time.
Checker: `<workdrive>/gf-LaneLeavesRE-scratch/checker_lane_leaves.py`
(`python3 checker_lane_leaves.py`: RESULT OK, 33 instruction-byte checks + 11 float rows;
`--selftest` OK: byte-flip + 20.0/18.0-neighbour mutations caught).
Decodes: `d_A79640.txt`, `d_A79440.txt`, `d_A79240.txt`, `d_A79050.txt`, `d_CE150.txt`,
`d_CE4F0.txt`, `d_CF310.txt`, `d_4FB2C0.txt`, `d_4FB780.txt`, `d_4FB380.txt`,
`d_A63020.txt`, `d_4F8AD0.txt`, `d_A23C10.txt`, `xref_A48A30.txt`, sweeps
`t78_store_hits*.txt`, `t78_lea_hits.txt`, `t78_xmm_hits.txt`.

Read-only RE. No repo edits, no files written outside the scratch dir.
Verdicts: **CONFIRMED** = decompile + asm (byte-checked where the checker covers it);
**INF** = inference; **OPEN** = not decoded, blocker named.

Closes the four items in the ticket, extending `docs/re/pes-re-lane-writers.md`,
`docs/re/pes-re-lane-post.md` (LanePostRE) and `docs/re/pes-re-blockA-forward-pull.md`.

## §0 Constants (script output, pasted; each paired with its operand VA in §1–§4)

```
0x14259d634 = 10000.0      (CE150 +10000 key init; CF310 tail reuses the same word)
0x1425c80e8 = -10000.0     (CE150 -10000 override; CE930 fVar24 init)
0x14258c264 = 3.0          (CF310 fVar1; CFC80-adjacent band edge arithmetic)
0x14259be1c = 20.0         (CF310 fVar2; neighbour 0x14259BE18 = 18.0 differs — checker mutation row)
0x14259be04 = 12.0         (CF310 fVar22 base)
0x14259bdf0 = 7.0          (CF310 tail key adjust: B<3 subtract)
0x142565d30 = NaN           (abs-mask via ANDPS, not a threshold; CF310 l.101-102, CE930 l.116-117)
0x14259be84 = -10.0        (CF310 tail -10.0 arm; CE930-adjacent depth tests)
0x14259222c = 5.0          (CFFE0 band edge +5.0; CE930 fVar29 side-sign scale base)
0x14259bddc = 1.5          (CF310 depth-per-level; CE930 fVar28)
0x1425c84bc = 21.0         (CF310 tail rank-vs-threshold)
0x14259bdec = 6.0          (CF310 tail B==5/6 bonus; CFFE0-adjacent; 4FB380 6.0 gate)
0x14259be34 = 50.0         (CFFE0 fVar29 mid-band ramp; 4FB380 range test)
0x14253ee30 = 100.0        (percent divisor; CFFE0 100.0 arms)
0x14259be0c = 15.0         (CFFE0 deep-narrow arm)
0x14259a768 = -2.0         (CFFE0 T+0x8770 arm)
0x14259bdf4 = 8.0          (gate thresholds; CE930-adjacent)
0x14259d664 = -12.0        (CD7C0 opponent-line test constant)
0x14259be74 = 500.0        (CD7C0 nearest-mate distance init)
0x1425656d0 = -0.0         (CE930 sign-flip XOR mask for opponent side)
0x142565604 = 2.0          (CE930 fVar24-side scale; 4FB380 range test)
0x1425c8824 = 576.0        (= 24.0²; CE930 second-sort radius test, R=24 m)
0x1425c48c4 = 24.0         (CE930 fVar5 radius)
0x14259d62c = 144.0        (= 12.0²; CE930 radius compare)
0x142598580 = 180.0        (4FB780 angle wrap)
0x14259858c = 360.0        (4FB780 angle wrap)
0x142588218 = -1.0         (4FB780 side-sign select)
0x14256ae1c = 1.0          (4FB780 side-sign select)
0x142598598 = -360.0       (4FB780 angle wrap)
0x14259a764 = 60.0         (4FB380/4FB780 angle gate; neighbour 0x14259A760 differs)
0x1425656c0 = 4.0          (4FB380/4FB780 range arg)
0x1425a9be0 = 2.25         (= 1.5²; 4FB380 close-range test)
0x14259d5d0 = 0.6499999761581421 (461C10 rank-scale; not lane-line, cited for completeness)
0x1425c65fc = 100000.0     (CE930 fVar22 init: +100000.0 scan guard)
```

Operand pairs (instruction VA → operand VA, decoded RIP-relative; checker byte-covers the
instruction, script reads the float):
`0x1405CE23B→0x14259D634`, `0x1405CE247→0x1425C80E8`, `0x1405CE407→S+0x3AA4` (struct, not a
constant), `0x1405CF49C→0x14258C264`, `0x1405CF4BB→0x14259BE1C`, `0x1405CF4C9→0x14259BE04`,
`0x1405CF82B→0x14259BDF0`, `0x1405CFA1E→0x1425C84BC`, `0x1405CFA30→0x14259BDEC`,
`0x1405CEA5D→0x1425C65FC`, `0x1405CEA69→0x1425C80E8`, `0x1405CEE27→0x14259BDDC`,
`0x1405CEE30→0x14259222C`, `0x1405CEDF1→0x14259BE0C`, `0x1405CEFD→0x14259D62C`,
`0x1405CEE0D→0x1425C48C4`, `0x1405CEDB→0x1425C8824`.

## §1 The sort comparator `FUN_140A79640` — CONFIRMED (key, tie rule, stability, exact)

VA 0x140A79640. Signature `FUN_140A79640(uint *param_1, char param_2)`.
`param_1` = two 11-slot (slot, float-key) lists back to back:
words `[count0, entries0×11, count1, entries1×11]` where each entry is 8 bytes
(slot dword + float key). `param_2` is observed 0 at every lane call site
(CE150, CF310-second-half, CE930 ×2); the `≠0` arm is described last.

What it does (decompile 249 lines + asm 319 lines, checker bytes):

1. `param_2 == 0` arm: for each half (offsets 0 and 0x16 words):
   compact the entries whose slot `< 0x16` into `local_84[]` in **slot order**
   (asm: unrolled `CMP [RBX+8k],0x15; JA skip` / `MOV [RBP+RCX*8+..],RAX` chain,
   checker rows; each kept entry's source word is then set to `0xFF`),
   then sort that run.
2. First half sorts with `FUN_140A79440` (thin wrapper over `FUN_140A79240`);
   second half sorts with `FUN_140A79240` directly with an explicit temp arena.
   `FUN_140A79240` = introsort-style split (recurse while `n < 0x21` is false,
   `FUN_140A79B90` pivot test, `FUN_140A78530` / `FUN_140A788C0` partition/merge);
   base case `FUN_140A79050` (37 lines, asm 57 lines, checker-adjacent).
3. Base-case comparator (the actual key order), `FUN_140A79050`:
   entries are `(slot, key)` with the key in the **high dword** (`float key =
   (u64 >> 0x20)`, decompile l.13–14; asm `MOVSS XMM0,[RSI+0x4]` vs
   `MOVSS XMM1,[RSP+0x44]` then `COMISS XMM0,XMM1`).
   - `COMISS first_key, new_key; JBE equal-or-greater arm` (asm 0x140A790A6/0x140A790A9):
     when `first_key > new_key` (strictly), the block is memmoved right
     (`DAT_14252FD68` memmove call at 0x140A790B5) and the new entry goes first.
   - Else (first_key ≤ new_key): insertion scan from the end while
     `old_key > new_key` strictly (`COMISS XMM0,XMM1; JBE done` at
     0x140A790D1/0x140A790D4, shift loop `JA shift` at 0x140A790F5).
   - **Equal keys take neither shift arm**: the new entry is appended **after**
     all equal keys. So the sort is **ascending by float key, stable**
     (insertion sort; equal keys keep their pre-sort relative order, which is
     slot order from the compact step). **CONFIRMED** from the two `JBE`
     (≤, not <) fall-throughs plus the strict-`>` memmove gate.
4. Write-back: the sorted run is copied back over the half's entries in order
   (decompile l.119–123, 161–165; asm copy loops).
5. Rank-fill tail (both halves): `0xA0`-fill of `param_1+0xB2..0xC7`
   (asm `MOV RAX,0xA0A0...; MOV [RBX+0xB2],RAX ...`, checker row), then for
   each half, for position `i` in 0..count-1: `param_1[entry_slot] = i`
   (asm `MOVSXD/MOV [RAX+RBX+0xB2],CL` loop at 0x140A79AD0, checker row).
   So after the call, `param_1[slot]` = the slot's **rank** (0-based position
   in its half's sorted order). CE150/CE930/CF310 then read ranks by slot.
6. `param_2 != 0` arm: generic sort `FUN_141E53540(buf, count, 8,
   &LAB_140602040)` per half (quicksort-ish, 95 lines). Comparator
   `LAB_140602040` not decoded; **no lane caller uses this arm** (all pass 0).
   Status: OPEN, out of scope (dead for the lane line).

Key, tie rule, stability (exact):
**key = the per-slot float built by the caller** (CE150: `S+0x3AA4+slot·0xC`
with the −10000.0 override, §3 of lane-post.md; CE930: `|P+0x564−ballX|`
negated form and the `T+0x22E50`-area depth, see §3 below; CF310 tail: same
`A71500`-table form, §2). **Direction ascending; ties keep slot order
(stable); rank = index in the sorted half.** Nothing in A79640 itself reads
game state.

## §2 `FUN_1405CF310`'s second half — CONFIRMED (structure, keys, calls, stores)

VA 0x1405CF310 (401 lines). First half (l.60–197): per-slot A:=0 pass, already
CONFIRMED in lane-post.md §4a — unchanged. Second half (l.198–397):

1. Gate (l.198–206): `if (MI+0x294-flag byte ≠ 0 && (bVar14 =
   FUN_1405CD220(ctx, MI)) < bVar3)` → `bVar3 = bVar14` (cap the zero-target
   by the CD220 count). Then `if (bVar15 + local_1F5 < bVar3)`:
   `local_1F4 = bVar3 − local_1F5 − bVar15` (remaining zeroes to place),
   `local_1C8 = T+0x2482C` (analysis-ball x pointer),
   `local_1F6 = FUN_1405CD7C0(local_1E8, MI)` (nearest-mate flag: 1 = some
   slot's A-word already 0 near the ball; CD7C0 returns `A[k]==0` at its best
   slot, see below).
2. List rebuild (l.207–318): two fresh 11-slot lists, one per side, same
   layout as CE150 (`slot` + float key). Per active on-pitch slot
   (`MI+0x1748`, ≠ outlet? no — the outlet exclusion is first-half only;
   here the exclusions are: inactive skip; `MI+0x116C≠1 && MI+0x1554==4 &&
   A63950(15)` skip; `T+0xB8A3≠0 || B≠2` pass):
   - `cVar5 == 0` arm (normal): `p = FUN_140A71500(T+0x20, sideSign)`;
     `key = p[4 + ((side11 + p[u])·8)]` (the slot-order table's float at the
     same `(side11 + table[u])` indexing CE930 uses), then class adjust with
     `fVar1 = 3.0` (asm 0x1405CF49C→0x14258C264), `fVar2 = 7.0` (tail reload,
     asm 0x1405CF82B→0x14259BDF0): `B==3 → key −= 3.0; B∈{5,6} → key += 3.0
     (decompile `fVar22+fVar1`); B<3 → key −= 7.0`.
     (Lane-post.md §4a said `fVar1` only; the three-way adjust is the exact rule.)
   - `cVar5 != 0` arm: `FUN_140A714C0(); key = FUN_1415A1780()` (sqrt of an
     engine float; inputs OPEN — same OPEN as lane-post.md §4a arm 3).
   - Final key stored: `key − (A[k]≠0 ? 0 : fVar1)` i.e. `fVar22 − fVar24`
     with `fVar24 = (A[k]==0 ? 3.0 : 0.0)`. **So A=0 slots sort 3.0 lower
     (earlier) than their raw key; A≠0 slots sort at the raw key.**
     Two sorted lists via `FUN_140A79640(&local_1B8, 0)` (asm CALL at
     0x1405CFA19, checker row).
3. Zero pass (l.319–397): walk this side's sorted list (`bVar15` = position);
   per slot `< 0x16`, active, class≠0, `A[k]≠0`, `B<6`, not the 0x3A-state
   (`T+0xA2A0≠0x3A` when `slot%11 ≤ 10 — always true, so the check is the
   0x3A compare itself):
   - `fVar22 = (A[k]≠0 ? 3.0 : 0.0)` (21.0/6.0: `fVar2 = 21.0`
     asm 0x1405CFA1E→0x1425C84BC; threshold compare `fVar2 − fVar22 ≤ |ball −
     MA.x|` uses the NaN-abs idiom; the `6.0` word at 0x1405CFA30→0x14259BDEC
     is the second radius in the same function, used by the shared depth arm).
   - If `local_1F6 == 0` (CD7C0 found no zeroed near slot) **or**
     `21.0 − fVar22 ≤ |P+0x564[man] − T+0x2482C+8|` **or**
     `FUN_140A4C720(T+0x2482C, P+0x55C[man], sideSign) == 0` (asm CALL at
     0x1405CFBB3, checker row): set `A[k] := 0`, `bVar4++`, stop when
     `bVar4 ≥ bVar3` (the capped target). Else stash the slot in `local_E8[]`.
   - Flush: if `local_1F6 ≠ 0 && bVar4 < bVar3`, zero the stashed slots in
     order until the target is met, then return.
   Tail constants: 1.5 = depth-per-level (asm `MOVSS XMM6,[0x1425CBDDC]`
   region — same word as CE930 fVar28); 7.0/21.0/6.0 as above; operand pairs
   in §0. Helpers: `FUN_1405CD220` (count, CONFIRMED below),
   `FUN_1405CD7C0` (nearest-mate flag, CONFIRMED below), `FUN_140A71500`
   (slot-order table selector, CONFIRMED in lane-post.md §4d),
   `FUN_140A4C720` (CONFIRMED), `FUN_140A79640` (§1).

Helpers (short decompiles, CONFIRMED):
- `FUN_1405CD220(MI)` (VA 0x1405CD220, 48 lines): counts own-side slots
  (`A714E0(T+0x20, sideSign)` order, 11) passing `FUN_140A4C720(ballX-ish,
  P+0x55C[man], sideSign, −12.0)`; returns count+1. Ball word:
  `T+0x40+(side·0x14)` (param_2[4]+0x40 slice); constant −12.0 =
  `DAT_14259D664` (script-read; the only new float in this function).
- `FUN_1405CD7C0(A-words, MI)` (VA 0x1405CD7C0, 81 lines): nearest-mate
  scan. Per active on-pitch slot: depth = `P+0x564·sideSign − MA.z-ish
  (T+0x24834)`, lateral = `P+0x55C·sideSign`, skip when `lateral > MA.x-ish
  (T+0x2482C)`; dist² vs 500.0 init (`DAT_14259BE74`); keeps the nearest
  slot passing the 0x3A/`|slot%11|>10` gate and `R = FUN_1415A1780(...) <
  500.0`. Returns 1 iff the winning slot's A-word == 0, else 0. Radius word:
  `DAT_1425C8824` is NOT used here (that is CE930's 576.0); CD7C0's only
  float reads are `S+0x7B14` (band edge), `T+0x2482C/0x24834`, `500.0`.

## §3 The recompute-path A-rank writer — CONFIRMED (it is `FUN_1405CE930`)

The OPEN in lane-writers.md §7 / lane-post.md §5 is closed: the writer is
`FUN_1405CE930`, called twice in CE4F0's recompute path
(`CE930(ctx,MI,0)` then `CE930(ctx,MI,1)`; decompile CE4F0 l.148–151).
The earlier sweep missed it because the `+0xC` stores use the alias
`local_2B8` (= param_1) with per-slot `+8k` strides, not a literal `+0xC`
token in every line. Both calls write A-ranks; the second call's tail
additionally resolves one slot (`local_2DC`).

VA 0x1405CE930 (425 lines). Exact rule:

1. Head (l.60–102): build two 11-slot lists keyed by
   `key = |P+0x564[man] − ballX|`, `ballX = *(T+0x2482C)·sideSign` with the
   NaN-abs idiom (`DAT_142565D30` pair, decompile l.114–117; `sideSign =
   T+0x254`, l.68–70; when `sideSign > 0`, `fVar29 ^= −0.0` flips the ball
   word's sign, decompile l.111–114). Per active slot with
   `actionState ≠ 0x3A` (else substituted by `local_2DC`, l.126–134):
   if `A64410(team,slot) ≠ 0` **and** `S+0x3AA4-area depth > 0.0`
   (`*(S+0x3AA4+slot·0xC) > 0`, asm 0x1405CEB0B `MOVSS XMM0,[R15+RAX]` /
   0x1405CEB11 `COMISS XMM0,XMM15` with XMM15 = 0.0), use the override key
   `fVar22 = +100000.0` (asm 0x1405CEA5D→0x1425C65FC); else the depth key
   `fVar24 = −10000.0` init is replaced by the plain depth
   (init `fVar24 = DAT_1425C80E8`, asm 0x1405CEA69→0x1425C80E8; the stored key
   is the `|Δdepth|` value, decompile l.104–110). Sort with
   `FUN_140A79640(&local_1B8, 0)` (asm 0x1405CEB82, byte-checked pattern
   `e8` CALL — same opcode as CE150/CF310 sites).
2. Rank write-back (l.103–122): walk this side's sorted list; per active
   on-pitch slot with class ≠ 0: `rank = local_2E4[A[k]]`,
   `A_rank[k] := rank`, `local_2E4[A[k]]++`, where `local_2E4[4]` counts
   starts at 0 and `A[k]` is the **current** A-word (post CFFE0/CF310/CFC80 +
   designated/B loops — CE930 runs after them in CE4F0). So the A-rank is
   the slot's position among same-A-word slots in depth order. **This is the
   recompute-path A-rank writer.** Store form: `*(local_2B8+0xC+8k) = cVar6`
   (decompile l.118).
3. Tail (only when `param_3 ≠ 0`, i.e. the second call): nearest-depth slot
   search (l.123–178): per active slot, `d² = (P+0x564−MA.z)² +
   (P+0x55C−MA.x)² ≤ 576.0` (= 24.0², asm 0x1405CEDB→0x1425C8824;
   `MA.x = T+0x2482C`, `MA.z = T+0x24834`); sort, take the first of this
   side → `local_2DC` (the "nearest" slot). Then the radius/count pass
   (l.179–424): constants 24.0/144.0/15.0/1.5/5.0/−1.0/1.0
   (asm 0x1405CEDF1→0x14259BE0C, 0x1405CEDFD→0x14259D62C,
   0x1405CEE0D→0x1425C48C4, 0x1405CEE27→0x14259BDDC,
   0x1405CEE30→0x14259222C, 0x1405CEE16→0x142588218,
   0x1405CEE1E→0x14256AE1C; pairs in §0) with `FUN_1408C5900`,
   `FUN_1405CD5E0`, `FUN_140A76240`, `FUN_1404C8EA0` gates; writes
   `A_rank` again at l.342 (`*(local_2B8+0xC+8uVar15) = local_2E0`).
   Interior gate order is transcribed in `d_5CE930.txt`, not reduced here.

`FUN_1405CC1A0` (46 lines, re-read) only **decrements** stale ranks and zeroes
records of inactive slots — it never assigns order; it runs before the
recompute and its output is overwritten by CE930's write-back. `FUN_1405CC390`
(CC390, 136 lines, read in full for this ticket) writes **B-words**
(`LX+0x60+8k := 0/1`) and the debug ring (`LX+0x1B4/0x1BC/0x1C4/0x1CC`), never
`LX+0xC` — not an A-rank writer. `FUN_1405CDF30/D05A0/CC250/CDA40/CDBD0`:
no `LX+0xC`-strided store (grep over the full decompiles; CDBD0's only
`param_1+0x1A4` store is its own count word).

## §4 The `T+0x78` writer — CONFIRMED (was OPEN in pes-re-holder-leaves.md §2)

`T = FUN_140A48A30(G, side)` (side < 2 → `G+(side+0xA1)·0x28`, else
`G+0x1928`; 10-line body). `T+0x78` is a dword player id (`< 0x16` valid,
`0xFF` = none). The writer was found by scanning image-wide `C7 disp8 0x78`
stores (`C7 4? 78 FF000000`) and filtering to non-frame bases in team-tick code.

Writer chain (all CONFIRMED decompiles):

- `FUN_1404FB2A0(Y)` (VA 0x1404FB2A0, 27 lines; team-tick region):
  `T = *(Y+0x6D50)` (asm `MOV RAX,[RBX+0x6D50]` at 0x1404FB2F3, byte pattern
  in checker context). If `MI+0x1775 == 0x0C` → return (keep). If
  `MI+0x116C == 1` (in play): needs `FUN_1423E7FD0` and `FUN_140A431E0`
  true, else `T+0x78 := 0xFF` (asm `C7 40 78 FF000000` at 0x1404FB2FA,
  checker row). If `MI+0x116C == 2` → `FUN_1404FB780(Y)`. If `≠ 3` →
  `T+0x78 := 0xFF` (asm at 0x1404FB34A, checker row). Else
  `FUN_1404FB380(Y)`. **So in every non-play phase except 2/3 the id is
  cleared to 0xFF here.**
- `FUN_1404FB780(Y)` (VA 0x1404FB780, 151 lines; the `MI+0x116C == 2` arm):
  - `MI+0x11D4 == 2 && A627E0(Y+0x6D48,0) > 0x15 && MI+0x1558 == Y+0x8`
    and (`T+0x78` invalid or the `A48910`-side check): copy
    `MA+0x24840[side·0x16]` → `T+0x78` (decompile l.34–44); if the copied
    id's role (`T+0x4C00[id%11]`) == 0, copy the +2 slot instead
    (l.46–62). Return 1.
  - `MI+0x11D4 == 3 && MI+0x1558 == Y+0x8`: if `T+0x78` invalid,
    `T+0x78 := MI+0x11CC` (decompile l.64–69). Return.
  - Else (in-play fallthrough): angle-gated keep-or-clear. Reads
    `T+0x78`, `MA+0x247D4+id·4 + 180.0` wrapped to [0,360)
    (180.0 = 0x142598580, 360.0 = 0x14259858C, −360.0 = 0x142598598;
    script-read), minus the man's angle `FUN_140439E50(...)`; abs via the
    NaN mask; `> 180.0 → −= ±360.0·sign`; `≥ 60.0`
    (0x14259A764, the same word as 4FB380's gate) → run the
    `FUN_140A3C950` / `FUN_140A8FF10` keep tests (with `1.5 =
    0x14259BDCC`·fps-scaled range and `4.0 = 0x1425656C0`); fail → `T+0x78
    := 0xFF` (decompile l.147). Angle path CONFIRMED at the predicate
    level; the keep-test interiors (`8FF10`, `57800`, `439EF0`) are OPEN
    leaves, not needed for the lane line.
- `FUN_1404FB380(Y)` (VA 0x1404FB380, 143 lines; the `MI+0x116C == 3` arm):
  `u = FUN_140A627E0(Y+0x6D48, 0)` (holder), `w =
  FUN_140A62690(Y+0x6D48, Y+0x8)`; if `u > 0x15` or `FUN_140A57940` false →
  `T+0x78 := 0xFF` (+ clear `Y+0x6D04/0x6CFC/0x6D00`, l.38–42). Else a
  decision tree over `FUN_140A51320` (count), action id `*(FUN_140A3E190)`,
  `T+0x6E7`, range `FUN_14044E0D0 < 6.0` (0x14259BDEC), the current
  `T+0x78`, role-5/angle tests (`60.0 = 0x14259A764`, `50.0 =
  0x14259BE34`, `2.25 = 0x1425A9BE0`, `2.0 = 0x142565604`), ending with
  `T+0x78 := FUN_1404F0E50(Y, iVar1)` (the computed kick-man, l.138) or
  `0xFF` on the early arms (l.55, l.65). Return value: whether the id
  changed.
- 3-level fallback `FUN_140A63020(Y, side)` (VA 0x140A63020, 25 lines,
  re-fetched): carrier `MA+0x2601C` (asm `MOV ECX,[R9+0x2601C]` at
  0x140A6302A, checker `41 8B 89` + disp32) → holder `MA+0x2483C` (asm at
  0x140A63050) → `T+0x78` (asm `MOV ECX,[RAX+0x78]` at 0x140A63076,
  checker row `8B 48 78`). Each level needs `id < 0x16` and
  `sideMatch(id) == side`; else `0xFF`. **CONFIRMED** (matches the
  holder-leaves doc's read sites; the writer above is what makes the
  third level live).
- Other read sites (unchanged from holder-leaves §2, re-verified):
  461C10 l.105–107 (per-side copy to `param_3[0..1]`, then
  `FUN_140A72A60` holder-setback on mismatch), 4F8AD0 (`[T+0x78]==slot`
  steal-gate, decompile l.99), A23C10 l.410–423 / 712–759 (own/other-side
  select on `T+0x255`, fallback `FUN_140A62960`).

Depth-2 sweep result (why this closes the OPEN): all 174 `FUN_140A48A30`
callers' disassemblies contain **zero** register-indirect `[T+0x78]` stores
(the 59 `+0x78]` memory-dest hits are all `[RSP/RBP+0x78]` frame locals;
the 57 spaced hits are all `−0x78` spill slots or `[RCX+0x60]` 16-byte
XMM fills that end at 0x6F and never reach 0x78; the 36 `LEA …,0x78` hits
are stack-address takes or `&T[0x78]` passed to readers like
`FUN_14047DE80`, which stores through its own `RCX` out-param, not the
`RDX` id pointer). The writer is not a caller-frame store at all — it is
the team-tick setter above, found by the image-wide `C7` scan. No
split-displacement search was needed (the disp8 `C7 40/41/43/46/47 78`
forms are the emitted encodings).

## §5 GF carriage (read-only grep over `src/sim`, origin/main 24-10-26)

| PES input | GF carries 1:1 | Exactly what is needed |
|---|---|---|
| A79640 sort (key/tie/stability) | NO (GF has no lane sort) | New pure function `SortLaneSlots(slots, keys)`: insertion/ascending/stable, ties → slot order; rank = index. Test: keys `{1,0,0,2}` → order `{1,2,0,3}`, ranks `{2,0,1,3}`. |
| CE150 key (`S+0x3AA4` f2; −10000 override) | PARTLY: `slotPoints` rows exist (`teamAIcontroller.cpp:3160-3175`) | The f2 float of each slot + the special-branch flag per slot (CCD50 arm). No new struct: reuse `slotPoints[k].z`-equivalent + a bool. |
| CF310 2nd half (CD220 cap, CD7C0 flag, A71500 key ±3.0/7.0, 21.0-vs-depth, 4C720) | PARTLY: `DefensiveLineOf`/`ReclassifyLine` (line), `teamTick.b8a4/b8a2/b8a3` (counts/flags), `P+0x55C/0x564`-equivalent man coords via `PlayerData` | `MI+0x294` flag byte, `MI+0x11CC`/`0x11C8` words, `T+0xB8A3`, outlet `T+0x4B40`, `A71500` slot-order table, analysis-ball `T+0x2482C/0x24834`. Man coords already carried (lateral/depth). |
| CE930 A-rank (depth order, rank = same-word position) | NO (`TeamTick` has b8a4/b8a2/b8a3/t80 only) | 11 A-rank bytes per side + the `A71500`-table float reads + analysis-ball x. |
| T+0x78 writer (4FB2A0/4FB780/4FB380; `FUN_1404F0E50` computer) | PARTLY: `kickMan/kickValid` stand-in (`teamAIcontroller.cpp:2446-2448`, ponytail names the OPEN writer — now closable) | Per-side dword id + `MI+0x116C/0x11D4/0x1558/0x11CC/0x1775` words + `FUN_1404F0E50` decode (the final computer; still OPEN) + `FUN_140A431E0/1423E7FD0` gates. |
| A63020 3-level rule | PARTLY: `GetBallRetainer()` (carrier) + `GetDesignatedPossessionPlayer()` (holder) + `kickMan` (T+0x78 leg) | Same as above; the rule itself is 25 lines and ports verbatim. |
| `FUN_1405CD220/CD7C0` | NO | Their inputs are all above (ball words, man coords, side sign, 0x3A states). |
| A-rank copy path + CC1A0 decrement | NO | 11 A-rank bytes (same struct as CE930). |
| CFFE0/CFC80/CF310-first-half inputs | As lane-post.md §8 (unchanged) | Unchanged: instruction table, band rows (writers known), `T+0x46EC` float, `T+0x4B40/T+0xB710`, `A57800/T+0x8`, `141F9C3B0`. |

Nothing in §1–§4 changes the lane-post.md §8 verdict that the full exact lane
word is not yet portable (band-row consumers, `T+0x46EC`, `A57800`,
`141F9C3B0`, `FUN_1404F0E50` remain OPEN), but items 1–4 each shrink:
the sort is now an exact spec with no PES input at all; the CF310 tail needs
no new decode, only state; the A-rank writer is found (CE930) with inputs
already listed; the T+0x78 writer is found with the exact chain above.

## §6 Port spec (exact rules; replaces the four `// ponytail:` leaves)

```cpp
// Sort: ascending by float key, stable (ties -> slot order). Rank = index.
// FUN_140A79640(param_2=0): compact slots < 0x16 in slot order, insertion-sort
// (strict-> shifts; equal keys append), write back, rank-fill per half.
void SortLaneSlots(std::array<uint8_t, 11>& slots, const float* keys);  // pure
```

```cpp
// CF310 second half (FUN_1405CF310 l.198-397):
//   cap = CD220(); if (done + alreadyZero < target) {
//     target -= ...; nearestZero = CD7C0(A, MI);
//     build two sorted lists by key = A71500float(slot) + adjust(B) - (A?0:3.0),
//       adjust: B==3 -> -3.0; B in {5,6} -> +3.0; B<3 -> -7.0;
//     sort (SortLaneSlots); walk order: zero A[k] while
//       (noNearestZero || 21.0-(A?0:3.0) <= |depth-ballX| || !A4C720(...)),
//       else stash; flush stashed while target unmet (only if nearestZero).
//   }
```

```cpp
// A-rank writer (FUN_1405CE930, the recompute-path writer):
//   order slots by |depth - ballX| (special slots with A64410 && f2>0 use +100000.0);
//   A_rank[k] = position among same-A-word slots in that order (both CE930 calls;
//   second call also resolves the nearest-depth slot local_2DC, radius 24 m / 576.0).
```

```cpp
// T+0x78 writer (FUN_1404FB2A0/4FB780/4FB380):
//   if (MI+0x1775 == 0x0C) keep;
//   else if (MI+0x116C == 1) { if (!(431E0 && 23E7FD0)) T+0x78 = 0xFF; }
//   else if (MI+0x116C == 2) T+0x78 = FB780();   // MA+0x24840 copy / MI+0x11CC / angle keep-or-0xFF
//   else if (MI+0x116C != 3) T+0x78 = 0xFF;
//   else T+0x78 = FB380();                       // holder/count tree -> F0E50(Y, iVar1) or 0xFF
// A63020(Y, side): carrier (MA+0x2601C) -> holder (MA+0x2483C) -> T+0x78,
//   each gated by id < 0x16 && sideMatch(id) == side, else 0xFF.
```

Order inside CE4F0's recompute path (unchanged, now with the writer named):
`CFFE0 → CF310 → CFC80 → designated/B loop → copy loop → CDF30 → [D05A0-gate] →
CE930(·,0) → CC250 → CDA40 → CE930(·,1) → [CDBD0 if T+0x2CC]`.

## §7 Evidence ledger

| ID | Claim | Verdict | Evidence |
|---|---|---|---|
| LL-01 | A79640 layout (two 11-slot lists, count+entries, param_2=0 path) | CONFIRMED | 0x140A79640 decompile l.52–249; asm LEA/CMP/MOV chain 0x140A796CD–0x140A7986A (checker) |
| LL-02 | sort key ascending float in high dword, stable (JBE fall-throughs), rank-fill | CONFIRMED | A79050 decompile l.13–30; asm 0x140A79096–0x140A790F5 COMISS/JBE/JA; tail 0x140A79A93/0x140A79AD0 (checker) |
| LL-03 | param_2≠0 arm = 1E53540×2, unused by lane callers | CONFIRMED (structure) / OPEN (comparator) | asm 0x140A796DF–0x140A7971A; all lane call sites pass 0 |
| LL-04 | CF310 tail gate/list/key/zero-pass rule | CONFIRMED | 0x1405CF310 l.198–397; asm 0x1405CF76F (CD220), 0x1405CF7C4 (CD7C0), 0x1405CFA19 (A79640), 0x1405CFBB3 (4C720), 0x1405CF49C/4BB/4C9/82B/A1E/A30 (checker + §0) |
| LL-05 | CD220 count rule; CD7C0 nearest-mate flag rule | CONFIRMED | 0x1405CD220 (48 lines), 0x1405CD7C0 (81 lines); consts −12.0/500.0 script-read |
| LL-06 | A-rank recompute writer = CE930 (both calls), rank = same-word position in depth order | CONFIRMED | 0x1405CE930 l.60–122 + tail l.123–424; CE4F0 l.148–151 call order; +0xC stores l.118/l.342 |
| LL-07 | CC1A0 = decrement-only; CC390 = B-words, not A-rank | CONFIRMED | 0x1405CC1A0 (46 lines); 0x1405CC390 (136 lines, B-word + debug-ring stores only) |
| LL-08 | T+0x78 writer chain 4FB2A0/4FB780/4FB380 | CONFIRMED | 0x1404FB2A0 (27 lines) + asm C7 stores (checker); 0x1404FB780 (151 lines); 0x1404FB380 (143 lines) |
| LL-09 | A63020 3-level rule (carrier→holder→T+0x78) | CONFIRMED | 0x140A63020 (25 lines); asm 0x140A6302A/50/76 (checker) |
| LL-10 | A63020/461C10/4F8AD0/A23C10 read sites | CONFIRMED | decompile lines cited in §4; xref_A48A30.txt (174 callers, zero frame stores) |
| LL-11 | GF carriage table | CONFIRMED (grep) | §5 paths (origin/main 24-10-26) |
| LL-12 | §0 floats | CONFIRMED (script-read) | checker float rows; §0 table |

## §8 Open items (numbered, for Main)

1. `FUN_1404F0E50` (the T+0x78 computer used at the end of 4FB380) — not decoded.
2. `FUN_140A714C0` + `FUN_1415A1780` inputs in the CF310-tail `cVar5≠0` arm — same OPEN as lane-post.md.
3. CE930 tail interior gates (`FUN_1408C5900`, `FUN_1405CD5E0`, `FUN_140A76240`, `FUN_1404C8EA0`) — transcribed, not reduced.
4. 4FB780 keep-tests (`FUN_140A8FF10`, `FUN_140439EF0`, `FUN_140A3C950`) and 4FB2A0 gates (`FUN_140A431E0`, `FUN_1423E7FD0`) — predicate-level CONFIRMED, interiors OPEN.
5. `LAB_140602040` (A79640's param_2≠0 comparator) — dead for the lane line, not decoded.
6. Prior OPENs unchanged: `T+0x46EC` float, band-row consumers in the port, `A57800`/`T+0x8`, `141F9C3B0`, `F0E50`.
