# PES mentality lane line, post items: special predicate, distance branch, three post-step A-writers, recompute A-rank, band rows, gate helpers (LanePostRE, 23-10-26)

Image `pes_full.bin` (file offset = VA − 0x140000000). Labels: **CONFIRMED** = read from bytes (checker) or a decompile with named lines; **INF** = inference; **OPEN** = not decoded, blocker named. Checker: `<workdrive>/gf-LanePostRE-scratch/checker_lane_post.py` (47 checks: instruction bytes + float constants; `RESULT OK`; `--selftest OK`).

Closes open items §4/§6/§7/§8 of `docs/re/pes-re-lane-writers.md` and §7 of `docs/re/pes-re-blockA-forward-pull.md`, except where marked OPEN below.

## §0 Constants (script output, pasted; each paired with its operand VA in §1–§6)

```
0x140a645a2->0x142588218 = -1.0
0x140a645b1->0x14256ae1c = 1.0
0x1405cce4f->0x142565600 = 0.5
0x1405cce59->0x14257a190 = 0.6 (0.6000000238418579)
0x1405cced6->0x14259b89c = 0.4 (0.4000000059604645)
0x1405ccee0->0x142565600 = 0.5
0x14063f7be->0x14256ae1c = 1.0
0x14063f825->0x14259be18 = 18.0
0x14063f841->0x14259bdf4 = 8.0
0x14063f886->0x142588218 = -1.0
0x14063f8e1->0x14259be04 = 12.0
0x14063f915->0x142565604 = 2.0
0x14063f956->0x14253ee30 = 100.0
0x14063f966->0x14253ee30 = 100.0
0x14063f9a8->0x14259a760 = 35.0
0x14063f9bb->0x14259d614 = 17.0
0x14063f9c3->0x14253ee30 = 100.0
0x14063f9d7->0x14253ee30 = 100.0
0x14063f9ef->0x142588210 = 10.0
0x14063f9f7->0x1425c48c4 = 24.0
0x14063f500->0x14259be34 = 50.0
0x14063f508->0x14253ee30 = 100.0
0x14063f52c->0x14256ae1c = 1.0
0x14063f537->0x14259222c = 5.0
0x14063f545->0x14259be0c = 15.0
0x14063f5bb->0x142588218 = -1.0
0x14063f600->0x142565604 = 2.0
0x14063f60b->0x14259bdec = 6.0
0x14063f631->0x142565600 = 0.5
0x14063f689->0x14253ee30 = 100.0
0x14063f699->0x14253ee30 = 100.0
0x14063f6c2->0x142565604 = 2.0
0x14063f6e1->0x14259bdf0 = 7.0
0x14063f706->0x142565600 = 0.5
0x14063f70e->0x142565600 = 0.5
0x1405ce23b->0x14259d634 = 10000.0
0x1405ce247->0x1425c80e8 = -10000.0
0x1405cf48f->0x142565d30 = NaN-mask (abs via ANDPS, not a threshold)
0x1405cf49c->0x14258c264 = 3.0
0x1405cf4bb->0x14259be1c = 20.0
0x1405cf4c9->0x14259be04 = 12.0
0x1405cf4f4->0x14259bddc = 1.5
0x1405cf82b->0x14259bdf0 = 7.0
0x1405cfa1e->0x1425c84bc = 21.0
0x1405cfa30->0x14259bdec = 6.0
0x1405d005a->0x14259be34 = 50.0
0x1405d0066->0x14253ee30 = 100.0
0x1405d0078->0x14259be0c = 15.0
0x1405d0085->0x14259a768 = -2.0
0x1405d008e->0x14258c264 = 3.0
0x1405d0097->0x14259bdf4 = 8.0
0x1405d00a0->0x14259222c = 5.0
0x140687c53->0x14259be80 = -7.0
0x140687c63->0x14259be04 = 12.0
0x140687c6f->0x142565604 = 2.0
0x140687c78->0x14259bdf4 = 8.0
```

Every float above was read by a script from the image; each instruction→operand pair was re-checked against the disassembly text. Neighbouring constants differ by 4 bytes (e.g. 0x14259BE1C = 20.0 vs 0x14259BE18 = 18.0 vs 0x14259BE04 = 12.0); the pairs above are the exact ones.

## §1 The special predicate FUN_140A64410 (SMF/WF special) — CONFIRMED

VA 0x140A64410. Signature `FUN_140A64410(team, uVar15)` where `uVar15 = side·11 + k` (called from CE150 with the same index used for designated/restart tests). Returns 1 = special (CE150 takes the CCD50 branch), 0 = normal (CE150 takes `ReclassifyLine(B[k])`).

Rule, in order (decompile, asm):

1. `side = (uVar15 in 11..21) ? 1 : 0` (decompile l.27–29; INF: out-of-range indices read side 0's tables). **CONFIRMED** asm `TEST EDX,EDX; JS / CMP EDI,0xB / JL / CMP EDI,0x16 / SETL` at 0x140A6441C–2D.
2. Scan 4 instruction-table rows `MA+0x25CF8+4·(j+4·side)` for `== 0x10` with partner `MA+0x25D38+… == 0xFF`: absent all 4 → return 0 (decompile l.34–40; asm 0x140A64447 `CMP [RDX+R8*4+0x25CF8],0x10`, 0x140A64452 `CMP …,0xFF`; checker bytes). **CONFIRMED.**
3. Scan the same 4 rows for `(0xF, uVar15)`: found → return 0 (decompile l.42–46; asm 0x140A64487 `CMP …,0xF`, 0x140A64492 `CMP …,EDI`; checker bytes). So an instruction-15 slot is excluded from the special (CE150 already forced it to B = 2). **CONFIRMED.**
4. `role = T+0x4C00[slot]` via `FUN_140A48A30(MA, side)` (the side's team record; see §1a). `role ∉ {5, 8}` → return 0. **CONFIRMED.** Roles 5/8 are the SMF/WF band (role-class table: 5, 8 ∈ midfield line 1; the "SMF/WF" label is INF from formation usage).
5. Slot active (`MI+0x1748+uVar15 != 0`) else return 0 (decompile l.63; asm 0x140A64528). **CONFIRMED.**
6. Count side slots with `role−1 < 2` (roles 1, 2: SB): `≥ 5` → return 1 (decompile l.66–84, `bVar14 < 5` inverted; asm 0x140A64578 `DEC EAX; CMP EAX,1; JA`, 0x140A6458F `CMP R8B,4; JA`). So the special needs **fewer than 5 SB on the side**. **CONFIRMED.**
7. If own role ≠ 5 (i.e. role 8): return 1 (decompile l.85; asm 0x140A64599 `CMP ESI,5; JZ`). **CONFIRMED.**
8. Else (own role 5): scan own-side active slots `m ≠ uVar15` with `role == 5`; for the first found, compare signs: `s1 = sign(slotPoint f0 of k)`, `s2 = sign(slotPoint f0 of m)`, each `+1.0` unless `(f0 ≤ 0 && f0 ≠ 0)` → `−1.0` (decompile l.98–108; asm 0x140A645A2 `MOVSS XMM3,[-1.0]`, 0x140A645B1 `MOVSS XMM4,[1.0]`, 0x140A6461C/0x140A6462B `COMISS XMM2,[mem]; JBE` = `f0 ≤ 0` arm, checker bytes). Equal signs → return 0 (`UCOMISS; JZ` at 0x140A6463E); scan ends with no equal-sign role-5 mate → fall through to return 1. Note `f0 == 0.0` reads sign +1. **CONFIRMED.**

Slot-point reads (which table, which float):
- The scan's own-side pointer is `R15 = team + 0xC48` (asm 0x140A64551 `LEA R15,[RAX+0xC48]`; checker bytes), i.e. the plan block `S = T+0xC48`.
- Per-slot stride 12 (`IMUL R11,RAX,3` → `R15+R11*4`, l.99: `(slot%11 + 0x4E3)·0xC`); `0xC48 + 0x4E3·0xC = 0x3A9C`: **the f0 float of slot k's row `S+0x3A9C+12k`** (GF: `slotPoints[k].x`).
- The partner float is `[R8]` with `R8 = R15 + 0x3AA4` (asm 0x140A645AA `LEA R8,[R15+0x3AA4]`; checker bytes): `0xC48 + 0x3AA4 = 0x46EC`: **the second table `T+0x46EC+12m`** (the mate's float at the same stride; only offset +0 of each 12 B row is read). GF has no `T+0x46EC` (see §8).

§1a `FUN_140A48A30(MA, side)` (VA 0x140A48A30): returns `MA + (side+0xA1)·0x28` for side < 2 else `MA + 0x1928` (10-line decompile; **CONFIRMED**). Its `+0x4C00` array is the side's role table. `T+0x46EC` and `T+0x4C00` are the same team record reached through MA.

## §2 The distance branch FUN_1405CCD50 — CONFIRMED

VA 0x1405CCD50. `CCD50(LX, MI, uVar15) → {0, 1, 2}` and appends one debug record (slot, kind, distance) to `LX+0x1B4/0x1BC/0x1C4` while `LX+0x1CC < 2` (decompile l.20–25, 38–43, 52–58; kind 1 + 1.0 for early/back arms, kind 0 for the forward arm). The B-word is the return value.

Pre-branch (decompile l.18–27; asm 0x1405CCD83 `CMP [RAX+0x116C],1`, 0x1405CCD8C `CMP [R9+0x54],0`, 0x1405CCD93 `MOV EAX,[RAX+0x1554]`, 0x1405CCD99 `SUB EAX,3; CMP EAX,2`; checker bytes): if `MI+0x116C ≠ 1` **and** `MI+0x54 == 0` **and** `MI+0x1554 − 3 < 3` (restart kinds 3, 4, 5 out of play) → record kind 1 + 1.0 and **return 0**. **CONFIRMED.**

Otherwise:
- `line = ReclassifyLine(B[k])` (`FUN_1405CD540`, CONFIRMED §2a).
- If `line ≠ 2` → `d = FUN_14063F760(...)`; threshold `t = (B[k] == 0) ? 0.4 : 0.5` (decompile l.33–36; asm 0x1405CCED6/0x1405CCEE0 `MOVSS XMM1,[0.4]/[0.5]` with the `CMP [RBX+RAX*8+0xB8],0` select; pairs in §0). Return `d ≤ t ? 0 : 1`. **CONFIRMED.**
- If `line == 2` **and** `dir·MA.x·T+0x2482C < 0` (decompile l.29–31: `T+0x254`-signed `MA+0x2482C` product, `COMISS XMM0,XMM1; JA` = skip when positive; the forward arm is taken when the product is **negative**) → `d = FUN_14063F490(...)`; threshold `t = (B[k] == 1) ? 0.5 : 0.6` (asm 0x1405CCE4F/0x1405CCE59 `MOVSS XMM1,[0.5]/[0.6]`; pairs in §0). Return `(d ≤ t) + 1` ∈ {1, 2}. **CONFIRMED.**
- If `line == 2` but the product is ≥ 0 → the back arm (`F760`, thresholds 0.4/0.5). **CONFIRMED** (asm `JA 0x1405CCEB9` rejoins the back arm).

§2a `FUN_1405CD540(MI, k)`: B[k] 0 → 3; 1–2 → 0; 3–6 → 1; 7–9 → 2 (46 lines; **CONFIRMED**).

§2b `FUN_14063F760` (back distance, VA 0x14063F760, 125 lines) and `FUN_14063F490` (forward distance, VA 0x14063F490, 115 lines) both take `(ctxthing, S, uVar15)` and return a 0..1 score. Inputs they read: slot-point f0 signs (`S+0x3A9C`-area rows; F760 also reads `S+0x7B14`/`S+0x7B24` band rows), `T+0x254` (side sign), `MA+0x2482C/0x24834` (analysis-ball x/z-ish), `S+0x3AA4`-area row floats, `DAT_…+0x2AA8` extras. Constants are all in §0 with operand pairs. The internal ramps are transcribed in the scratch decompiles but are **INF** as a port rule (not reduced to a GF formula). Status: **entry/exit, thresholds and call targets CONFIRMED; interior score formula INF**.

## §3 The CE150 sort key and the B-rank — CONFIRMED (extends lane-writers §3)

- CE150 builds two 11-slot lists keyed by `fVar18`: default `fVar1 = S+0x3AA4+slot·0xC`-area float (asm 0x1405CE407 `MOVSS XMM0,[R12+RAX+0x3AA4]`, R12 = S; checker bytes); when the slot took the CCD50 arm **and** `fVar1 > 0.0`, the key is replaced by `fVar4 = −10000.0` (decompile l.131–134; asm 0x1405CE414 `COMISS XMM0,XMM8; JBE` / `MOVAPS XMM0,XMM6` with XMM8/XMM6 = ±10000.0 from 0x1405CE23B/0x1405CE247; pairs §0). So special-branch slots with positive key sort last; `fVar5 = +10000.0` is the matching init. **CONFIRMED.**
- B-rank `LX+0x64+8k` = count of earlier same-B-word slots in that sorted order (decompile l.150–182; `FUN_140A79640` sort itself OPEN). **CONFIRMED** (structure; the sort comparator stays OPEN).

## §4 The three post-step A-writers — CONFIRMED (fires/inputs/values)

Order inside CE4F0's recompute path (decompile l.65–140): `CFFE0` → `CF310` → `CFC80` → designated/B loop → copy loop. Each post-step only ever writes 0/1/iVar22 over the recompute values; the later designated/B loop can still force a slot to 2.

### 4a FUN_1405CF310 (A := 0) — CONFIRMED write; fire condition CONFIRMED at the predicate level, two leaf inputs INF/OPEN

VA 0x1405CF310 (401 lines). Store: decompile l.170 `*piVar16 = 0` with `piVar16 = LX+8` strided 8 B (asm 0x1405CF6F7 `MOV [R12],0`, R12 = A-word; checker bytes).

Per-slot gate (decompile l.106–121): slot `uVar19 = side·11+i`, active (`MI+0x1748`), on-pitch class (`P+0x3FB8 ≠ 0`), B-rank `LX+0xB8+8i == 0`, A-word `≠ 0`, `FUN_1405CCD00(MI,uVar19) == 0` (not a 0x3A-state man, §4d), slot ≠ outlet `T+0x4B40`, and `FUN_1405CD780(MI,uVar19) == 0` (no instruction-15 claim, §4e). Then a fail-fast arm (l.121–152): if any of three sub-tests fails, the slot is **skipped**, else (l.155–170) the A-word is set to 0 and the tail counter breaks when `local_1F4 ≤ bVar15`.

The three sub-tests (l.121–168):
1. `local_1D0 ≠ 0` (i.e. `CD460(side, otherSide) > CD460(otherSide, otherSide)` evaluated with `iVar21 == 0`, decompile l.84–90) selects the `FUN_140A57800(T+0x10)` arm (l.155–160): fires (writes 0) unless `|P+0x564[man] − T+0x8| ≤ fVar1 + fVar2` where `fVar1 = 3.0` (0x1405CF49C→0x14258C264) and `fVar2 = 20.0` (0x1405CF4BB→0x14259BE1C). **CONFIRMED** except `FUN_140A57800` and `T+0x8` (**OPEN**, not decoded).
2. Else (`local_1D0 == 0`) with `local_1E0 ≠ 0` (`MI+0x1554 ∈ {3,4}`, l.96–99): the `FUN_140A4C720`-vs-`FUN_140A714C0` arm (l.129–152, calls at 0x1405CF6A0/0x1405CF6D8). **CONFIRMED** structure; opponent-line inputs per §7.
3. Else (in-play arm): `FUN_140A4C720(x·fVar22, P+0x55C[man], local_1F7)` with `x = (int)local_1F7`, `fVar22 = 12.0 ± ({3 if B==3} / {+3.0 if B∈{5,6}} / {−20.0 if B<3})` (l.263–297; 12.0 = 0x1405CF4C9→0x14259BE04, 3.0, 20.0 as above). If the 4C720 test fails, a second chance via `FUN_140A714C0(T+0x20, slot)` rank-vs-`bVar3` (l.146–152). **CONFIRMED** structure; `T+0x4B40`/rank inputs per §4d/GF notes.

Second half (l.198–397): rebuilds two sorted lists by key `fVar22 − (A≠0 ? 0 : fVar1)` and zeroes further A-words while `bVar15 + local_1F5 < bVar3` etc. (calls CD220/CD7C0/A79640/4C720). This tail is **INF** (not reduced; needs CD220/CD7C0).

Tail constants: 1.5 (0x1405CF4F4→0x14259BDDC), 7.0 (0x1405CF82B→0x14259BDF0), 21.0 (0x1405CFA1E→0x1425C84BC) / 6.0 (0x1405CFA30→0x14259BDEC). Operand pairs in §0.

### 4b FUN_1405CFC80 (A := 1) — CONFIRMED

VA 0x1405CFC80 (126 lines). Stores: `*puVar11 = 1` at l.80 (first loop) and l.115 (second loop) with `puVar11 = LX+8` strided 8 B (asm 0x1405CFE4D `MOV [RSI],1`, 0x1405CFFAA `MOV [RSI+RDI*8+8],1`; checker covers the first). No floats (asm has no MOVSS/COMISS — confirmed by grep).

- `bVar2 = FUN_1405CD320(MI, LX+0x60)` (forward target count, §4f); `bVar16` = active A==1 count; `bVar14` = active A==0 count; `cVar15 = max(0, CD0F0 − bVar14)` (decompile l.23–53). **CONFIRMED.**
- First loop (l.58–87): per on-pitch slot with `CCD00 == 0`, `CD780 == 0`, slot ≠ outlet `T+0x4B40`, `10 < uVar12%11 || T+0xA2A0[slot%11] ≠ 0x3A`, and `T+0xB8A2 == 0 || B ≠ 2`, and `(A > 1 && (B == 1 || (T+0xB8A4 < 2 && B ≠ 0)))`: set A := 1 until `(cVar15 − bVar16 + bVar2) ≤ bVar3` (count reached). **CONFIRMED.**
- Second loop (l.88–123): same with the sorted-slot order `FUN_140A71500(T+0x20, side)` (slot table; §4d) and `T+0xB8A3/T+0xB8A4` gates; sets A := 1 until `(bVar2 − bVar16 − bVar3) ≤ bVar17`. **CONFIRMED.**

### 4c FUN_1405CFFE0 (A := iVar22) — CONFIRMED write and value rule; one leaf (A71500 table) is a pure selector

VA 0x1405CFFE0 (215 lines). Store: l.203 `piVar21[−0x16] = iVar22` with `piVar21 = LX+0x60`, `−0x16·4 = −0x58` B = `LX+8` = the A-word (asm 0x1405D04E6 `MOV [RBX−0x58],EDI`; checker bytes). A second store in the loop-B tail (l.110 `*(LX+8+8·slot) = iVar2`, asm 0x1405D0540 `MOV [RCX+RAX*8+8],ESI`; checker bytes) covers the `iVar2 == 3` early case. **CONFIRMED.**

Per-slot gate (l.56–66): active, on-pitch, `uVar17%11 < 11` (always), and **`T+0xA2A0[slot] == 0x3A`** (0x3A = 58; GF `pesoffball.hpp` names 0x3A states). Plus an early `FUN_141F9C3B0() ≠ 0` arm (l.67–99): counts on-pitch men; if `≤ FA70(side,0)` → A := 0 and return; else `bVar5 = count ≤ FA70(side,0)+FA70(side,1)`. **CONFIRMED** structure; `FUN_141F9C3B0` OPEN (not decoded; called with no args).

Value rule (l.106–203), with `iVar2 = B-word`, `iVar18 = B[k]` (l.139), `fVar6 = dir·MA.x·sideSign`-ish (`T+0x2482C·T+0x254`, l.121–124), band edges `fVar24/28/29` from `T+0x38/0x3C/0x40 + 0x14·side` scaled by side sign (l.125–127), `local_f0` = ball depth (l.121):
- `iVar2 == 3` (keeper B) → iVar22 := iVar2 (3), unless the man is 0x3A-state with `|MI+0x1198 − *(T+0x10)| < 10` frames and `(B − B[other]) < 2·fps` — then falls through (l.106–117; `FUN_141E532E0` = fps, 10 = frames). **CONFIRMED.**
- Else band compare of `local_f0` vs `fVar24/28/29` picks `cVar16 ∈ {0,1,2,3}` (l.128–138). **CONFIRMED.**
  - `cVar16 == 0` (behind back edge): 1 unless (`!bVar5 && B == 1 && B[k] == 1`) then `iVar22 = (fVar24 + 5.0 ≤ local_f0) + 1` (l.141–147; 5.0 = 0x14259222C). **CONFIRMED.**
  - `cVar16 == 1` (mid band): `!bVar5` → 1; else `iVar22 = (((fVar24−fVar28)·fVar29)/100 + fVar28 ≤ local_f0) + 1` with `fVar29 = 50.0`, or `100 − T+0x254[opp]` when `B == 2 && B[k] == 2`, or `100.0` when `B == 1 && B[k] == 1` (l.149–164; 50.0 = 0x1405D005A→0x14259BE34, 100.0 = 0x1405D0066→0x14253EE30). **CONFIRMED.**
  - `cVar16 == 2` (forward band): deep-narrow arm → 0/1 by `local_f0` vs `fVar9 − T+0x18[opp]` (15.0 = 0x1405D0078→0x14259BE0C) and `T+0x8770 − (−2.0 or 0.0)` (−2.0 = 0x1405D0085→0x14259A768); else 1 via the 4C720 test (l.166–194). **CONFIRMED** structure.
  - else → 3, or 0 when `cVar16 == 3` (l.196–201). **CONFIRMED.**

### 4d–4f Helpers used by the three post-steps — CONFIRMED (short decompiles)

- `FUN_1405CCD00(MI, slot)`: nonzero iff `T+0xA2A0[slot%11] == 0x3A` (the man's action state is 0x3A; side-0/side-1 table select at `MI+0x50`; VA 0x1405CCD00, 21 lines). **CONFIRMED.**
- `FUN_1405CD780(MI, slot)`: nonzero iff in play (`MI+0x116C == 1` → false) and restart kind 4 (`MI+0x1554 == 4`) and `FUN_140A63950(side-team, side, 0xF, slot)` (instruction-15 claim on the slot; VA 0x1405CD780, 13 lines). **CONFIRMED.**
- `FUN_140A63950(team, side, id, slot)`: nonzero iff instruction table `MA+0x25CF8+4·(j+4·side) == id` with partner `MA+0x25D38+… == slot` for some j < 4 (side < 2, id < 17; VA 0x140A63950, 23 lines). **CONFIRMED.**
- `FUN_1405CD0F0(MI)` (VA 0x1405CD0F0, 53 lines): back-line target count = `FA70(side,0)`, +1 when `T+0xB710 ≠ 0` and < 5; then `max` with the forward count `CD050` + 1 when `T+0xB8A4 < 1`; capped so the `T+0xB8A4 > 2 && CD050 < FA70−1` case returns `FA70 − 1`; hard cap 5; when `T+0xB8A2 ≠ 0` returns `FA70>2 ? 3 : FA70`. **CONFIRMED** (needs `FUN_1408C5900`, `FUN_1405CD050`, `FUN_14063FA70` — CD050 and FA70 decoded here, 8C5900 OPEN).
- `FUN_1405CD320(MI, B-words)` (VA 0x1405CD320, 50 lines): forward target count = `FA70(side,1)`, +2 when `T+0xB8A4 == 0`, +1 when `== 1 && T+0x87CD > 1`; minus 1 per special-predicate (`A64410 ≠ 0`) line-1 man (`B ∈ {3..6}`) whose B-word is 0. **CONFIRMED.**
- `FUN_1405CD460(MI, otherSide)` (VA 0x1405CD460, 30 lines): counts other-side active slots whose `|T+0x44[side] − P+0x55C[man]| < 10.0` (10.0 = DAT_142588210). **CONFIRMED.**
- `FUN_140A71500(T+0x20, flag)`: returns `T+0x229B4 + (flag == 1 ? 0 : 0x1E8)` — a slot-order table selector (VA 0x140A71500, 13 lines). **CONFIRMED.**
- `FUN_14063FA70(side, MI, classes, line)`: counts active on-pitch slots whose reclassified class == line (VA 0x14063FA70, 52 lines). **CONFIRMED.**

## §5 The recompute-path A-rank writer — STILL OPEN (narrowed)

CE4F0's own code writes no A-rank (re-read: the only `param_1+0xC`-adjacent traffic is the final copy loop reading A-words and ranks for the debug log; no store to `+0xC`). The post-step A-rank stores were not found either: CF310/CFC80/CFFE0 asm contain no `+0xC`-strided A-rank store (their only A-record stores are the checker-covered `+0x8` word stores). `FUN_140682320` (the wrapper's list builder, 95 lines, re-read) writes list positions to its **own** buffer (`param_4+0x58/0x59` counts and `P+0x7B20/0x7B21` line counts), never to `LX+0xC`. So the recompute path's A-rank writer is none of CE4F0/CF310/CFC80/CFFE0/682320. Next: the CE4F0 callees not yet read in full (`FUN_1405CDF30`, `FUN_1405CD7C0`/`FUN_1405CD220`, `FUN_1405CE930`, `FUN_1405CC250`, `FUN_1405CDA40`, `FUN_1405CDBD0`). Status: **OPEN (narrowed)**.

## §6 Band rows S+0x7B10/7B14 — WRITERS FOUND (prior doc's OPEN closed by pes-re-defensive-shape)

The lane-writers doc §6 said OPEN ("not in the 116 functions"). `docs/re/pes-re-defensive-shape.md` §6 + §14 decodes the writers since: `FUN_1405FBCB0` (phase-0 `FUN_1405F9EB0` back-line front → `S+0x7B14`, forward rear → `S+0x7B38`; phase-1 variants), `FUN_1405FA990` (mid band `S+0x7B24/0x7B28`, back upper `S+0x7B10`, forward lower `S+0x7B3C`), `FUN_1405F4F50` (cover drop), `FUN_1405EFBD0` (catch-up shift), `FUN_140687FB0` (shape-pass pull). The remaining band work is the open list in defensive-shape §12 (passes 2–7, cover chain, per-slot template), not the row writers. On split displacements: no split-displacement search was needed — the writers were already decoded under `T+0xC48` with the shape builder `FUN_1405EC600` path. `FUN_141FC1590` (the setup builder named in the ticket) fills record B of the **sheet** (`S+0xA54`), not the bands; its keeper row (−50.0, 0, 0) is decoded in pes-re-sheet-triple §5. Status: **CONFIRMED (by reference)**.

## §7 Back-line gate FUN_140687A80 and helpers — CONFIRMED except one leaf

VA 0x140687A80 (186 lines, 3 args: `param_1` = ctx-ish, `param_2` = list, `param_3` = bitset; 682770 passes one arg in the decompile because the other two are inlined temporaries — INF).

- Returns 0 when `*(ctx+0x1E·8+0xB0) == 0` (l.41) or `MI+0x116C ≠ 1 && MI+0x11D4 == 0` (l.42). **CONFIRMED.**
- Else `FA70(side)` = back-line target; per active lane-1 slot (`lane−1 ∈ {0,1}` arm of the jump table at 0x140687CDD/CE4, i.e. lanes 1–2 — in practice lane 1 since lanes are 1/4/7) runs **four** `FUN_140A4C720` tests (asm 0x140687D0A/0x140687D27/0x140687D5A/0x140687D6E, all `CALL 0x140A4C720`; checker covers #1): the XMM3 threshold cycles `−7.0` (0x140687C53→0x14259BE80), `12.0` (0x140687C63→0x14259BE04), `2.0` (0x140687C6F→0x142565604), `8.0` (0x140687C78→0x14259BDF4) with XMM0 alternating `MA+0x2482C` and XMM12 (= `MA+0x2482C·−1` via `XORPS XMM6,[−0.0]` at 0x140687C40/0x140687C9F) and XMM1 = `P+0x55C[man]`. Pass = at least one of each pair (l.127–130). **CONFIRMED.**
- Survivors sorted (`FUN_140A79640`), listed, `FUN_140688300` + conditional `FUN_140640C80`; returns `count < FA70(side)` (l.153–178). **CONFIRMED** (sort comparator and the two tail calls OPEN as behaviour, called out in lane-writers §7 already).
- `FUN_140A4C720(x, y, c, z) = (c·z + x)·c < c·y` (VA 0x140A4C720, 10 lines; float `(int)c` sign-selects the side). **CONFIRMED.**
- `FUN_14063FA70` = per-line on-pitch headcount (§4). **CONFIRMED.**
- Player fields `+0x55C/+0x564`: `+0x55C` = lateral (z) position of the man (used as `y` in every 4C720 call here and in CD460/CF310; GF `PlayerData` lateral is the carrier), `+0x564` = depth (x) position (used in CF310 l.157 `P+0x564 − T+0x8` and the gate's `|P+0x564 − MA.x|` key). **INF (high)** — consistent across all five call sites in this decode plus CD460/CF310, but no struct layout was recovered.

## §8 GF carriage (read-only grep over src/sim + src/pes, 23-10-26)

| PES input | GF carries 1:1 | New state needed |
|---|---|---|
| `MI+0x116C` (play flag), `MI+0x1554` (restart kind 3/4/5), `MI+0x54`, `MI+0x11D4`, `MI+0x1198` (frame), `MI+0x1748` (active) | PARTLY: `PesPlayState`, `PesRestartKind`, `TeamTick.t80` exist (`pesteamtick.hpp`); `MI+0x54/0x11D4/0x1198/0x1748` have no GF word | 4 bytes + frame counter + active mask |
| `P+0x3EF8` (designated), `P+0x3FB8` (class), `T+0x4B40` (outlet), `T+0xA2A0` (action state incl. 0x3A), `T+0xB8A2/B8A3/B8A4`, `T+0xB710` | PARTLY: roles B (`pesroles.hpp`), `b8a2/b8a3/b8a4` (`TeamTick`), action states (`pesoffball.hpp`, `teamAIcontroller.hpp:64`); `P+0x3EF8/T+0x4B40/T+0xB710` missing | designated slot, outlet slot, B710 flag |
| `MA+0x25CF8/0x25D38` (instruction table ids 15/16), `MA+0x2482C/0x24834` (analysis ball) | NO: GF instruction check is a stand-in false (`A63950`); no analysis-ball word | instruction table (4 × (id, slot) per side), analysis-ball x/z |
| `S+0x3A9C+12k` f0/f2 rows | YES (landed): `slotPoints` (`formation::SlotPointRows`, `teamAIcontroller.cpp:3131`) | — (f1 unused here; f2 is the CE150 key when the slot is not special) |
| `T+0x46EC+12m` second float table | NO | one float per slot (name TBD; reader at A64410 l.99) |
| `T+0x4C00` role table, `T+0x254` side sign, `P+0x55C/0x564` man coords, `FUN_140A71500` slot order | PARTLY: roles B 1:1; side sign trivially derivable; man coords via GF player positions (not the PES words); slot order table missing | slot-order table (or confirm identity with lineup) |
| `S+0x7B10..0x7B3C` band rows | NO (not in `src/`: grep confirms only comments) | 6 floats × 2 phases (defensive-shape §6/§14 is the writer spec) |
| A-words `ctx+0x58+8k`, B-words `ctx+0xB0+8k`, ranks | NO (`TeamTick` has b8a4/b8a2/b8a3/t80 only) | 11 × (A word, A rank, B word, B rank) + flag byte `ctx+0x200` + debug ring `LX+0x1B4/0x1BC/0x1C4/0x1CC` |

## §9 Port spec (exact lane line; replaces DefensiveLineOf's stand-in)

```cpp
// All words below are per side, 11 slots, updated on the team tick.
struct LaneLine {
  int a[11];        // ctx+0x58+8k: 0 back, 1 mid, 2 forward, 3 none
  uint8_t aRank[11];// ctx+0x5C+8k (copy path + CC1A0 decrements; recompute writer §5 OPEN)
  int b[11];        // ctx+0xB0+8k
  uint8_t bRank[11];// ctx+0xB4+8k (CE150 sorted-list counts)
  bool copyFlag;    // ctx+0x200 (CE4F0 l.41-50 reset, l.145-147 set)
};
// CE4F0(ctx, MI):
//   CE150(LX, MI) first (B-words §3 + B-ranks; special arm §1–§2).
//   CC1A0 (A-rank decrement; CONFIRMED).
//   if (!copyFlag) { A = B (all 11); }
//   else {
//     A = 3;
//     CFFE0(ctx, MI);   // §4c (0x3A-state men)
//     CF310(ctx, MI);   // §4a (A := 0)
//     CFC80(ctx, MI);   // §4b (A := 1)
//     for each active on-pitch slot with A > 1
//       && (slot == designated || B == 2): A = 2;
//     for each slot: inactive -> 3; else if (A > 2) A = B;
//     CDF30, D05A0-gate, CE930(0), CC250, CDA40, CE930(1), [CDBD0 if T+0x2CC];
//   }
//   copyFlag = true; backup A-records; debug-log non-3 words.
// Wrapper maps A -> lane {0:1, 1:4, 2:7, else 0-skipped} (blockA-forward-pull §1).
```

Order matters: CFFE0 → CF310 → CFC80 is the decoded CE4F0 call order (not CF310-first). The B-word special arm (§1–§2) must precede the A recompute. The A-rank recompute writer (§5 OPEN) blocks an exact A-rank port but not the A-word/lane port.

## §10 Evidence ledger

| ID | Claim | Verdict | Evidence |
|---|---|---|---|
| LP-01 | A64410 rule (16/FF gate, 15-exclusion, roles 5/8, <5 SB, sign match on S+0x3A9C f0 vs T+0x46EC) | CONFIRMED | 0x140A64410 decompile l.27–108; asm 0x140A64447/0x140A64487/0x140A64492/0x140A64528/0x140A64551/0x140A645AA/0x140A645A2/0x140A645B1 (checker) |
| LP-02 | CCD50 rule (restart-gate → 0; line≠2 → F760 vs 0.4/0.5; line 2 + neg product → F490 vs 0.5/0.6) | CONFIRMED | 0x1405CCD50 decompile l.18–60; asm 0x1405CCD83/93/99/0x1405CCE3D/0x1405CCEC4/0x1405CCE4F/0x1405CCE59/0x1405CCED6/0x1405CCEE0 (checker) |
| LP-03 | F760/F490 entry, thresholds, call targets; interior ramps INF | CONFIRMED/INF | 0x14063F760, 0x14063F490 decompiles; operand pairs §0 |
| LP-04 | CE150 sort key (S+0x3AA4-area float; −10000.0 override for positive-key specials) | CONFIRMED | 0x1405CE150 decompile l.131–134; asm 0x1405CE407/0x1405CE414 (checker) |
| LP-05 | CF310 A:=0 store + per-slot gate + three arms | CONFIRMED (two leaf inputs OPEN: A57800/T+0x8, CD220/CD7C0 tail INF) | 0x1405CF310 l.106–170/198–397; asm 0x1405CF6F7 (checker) |
| LP-06 | CFC80 A:=1 rule (two loops, counts, gates) | CONFIRMED | 0x1405CFC80 l.23–123; asm 0x1405CFE4D/0x1405CFFAA |
| LP-07 | CFFE0 A:=iVar22 store + band-value rule | CONFIRMED (141F9C3B0 OPEN) | 0x1405CFFE0 l.56–203; asm 0x1405D04E6/0x1405D0540 (checker) |
| LP-08 | Recompute A-rank writer | OPEN (narrowed: not CE4F0/CF310/CFC80/CFFE0/682320) | §5 |
| LP-09 | Band-row writers | CONFIRMED (by reference) | pes-re-defensive-shape §6/§14 (FBCB0/F9EB0/FA990/F4F50/EFBD0/687FB0) |
| LP-10 | Gate 687A80 + FA70 + 4C720 + P+0x55C/0x564 roles | CONFIRMED (sort/tail OPEN as before; +0x55C/0x564 layout INF high) | 0x140687A80 l.41–178; asm 0x140687B85/0x140687D0A/0x140687C40/0x140687C53 (checker); 0x140A4C720; 0x14063FA70 |
| LP-11 | GF carriage table | CONFIRMED (grep) | §8 paths |
