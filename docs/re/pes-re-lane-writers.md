# PES out-of-possession lane line: the writers of block A's lane word, slot points, band rows, and the floor gate (BlockAPull, 23-10-26)

Image: `pes_full.bin` (file offset = VA − 0x140000000). Checker: `gf-BlockAPull-scratch/checker_block_a.py`, RESULT OK: 36 instruction-byte checks and 11 RIP-relative operand pairs; selftest OK. Verdicts: **CONFIRMED** = read from the decompile or asm and byte-checked, or a direct decompile read; **INF** = inference; **OPEN** = not decoded, with the blocker named.

## 0. Correction to the earlier report (`pes-re-blockA-forward-pull.md` §2, §5)

The lane and rank words are not at `T+0x58`. They sit in `ctx`, where `ctx` is the wrapper's `param_1`:

- `ctx = Y+0x10`, where `Y` is `FUN_1404F6C30`'s `param_1` (CONFIRMED: `FUN_14051C840(param_1+0x10, …)` and `FUN_14051D9E0(param_1+0x10, …)`, decompile 4F6C30 l.175, l.243). `T = *(Y+0x6D50)` (l.156, l.242).
- The lane-record builder takes `LX = ctx+0x50` (CONFIRMED: `LEA RCX,[RBX+0x50]` at 0x14068205C, RBX = ctx, checker-verified; the call is `FUN_1405CE4F0(LX, local_148)` at 0x140682065, checker-verified).
- Inside `FUN_1405CE4F0`, `param_1+8` is `ctx+0x58`. The record layout (8-byte stride) is:

| field | LX offset | ctx offset | writer |
|---|---|---|---|
| lane word A[k] (dword) | LX+0x08+8k | ctx+0x58+8k | CE4F0 (copy or recompute), A-sub-steps |
| rank A[k] (byte) | LX+0x0C+8k | ctx+0x5C+8k | CE4F0 via CC1A0 (decrement) |
| B-word B[k] (dword) | LX+0x60+8k | ctx+0xB0+8k | CE150 |
| rank B[k] (byte) | LX+0x64+8k | ctx+0xB4+8k | CE150 (count of earlier same-word slots) |
| backup of A records | LX+0xB8+8k | ctx+0x108+8k | CE4F0 l.148–154 (copy) |
| flag | LX+0x1B0 | ctx+0x200 | CE4F0 l.41–50 (reset), l.145–147 (set back to 1) |

The wrapper's first loop reads A[k] at 0x140682090 (`MOV ECX,[R9-0x58]`, R9 = ctx+0xB0+8k) and B[k] at 0x1406820BB (`MOV ECX,[R9]`), as the earlier report said.

## 1. The lane word and its mapping (CONFIRMED, checker-verified)

- Wrapper lane (0x140682090–0x1406820B4): A=0 → lane 1; A=1 → lane 4; A=2 → lane 7; else → lane 0 (skipped by the pull). Lanes 1, 4, 7 are the canonical classes of lines 0, 1, 2 (reclassify table: 1 CB → line 0, 4 CMF → line 1, 7 SS → line 2).
- Pull switch (682770 decompile l.437–463): lane 1 → line 0, lanes 4–6 → line 1, lanes 7–9 → line 2, lane 0 → slot skipped.
- So **the out-of-possession line of a slot is its A-word** (0 back, 1 mid, 2 forward, 3 none).

## 2. Writers of the A-word (CE4F0 `FUN_1405CE4F0`, CONFIRMED decompile + asm)

Order inside one call (decompile l.41–150):

1. `FUN_1405CE150(LX, local_148)` writes the B-words and B-ranks first (l.52).
2. Flag LX+0x1B0 == 0 → **copy path** (l.54–64): A[k] := B[k] for all 11 slots.
   Flag set → **recompute path** (l.65–140):
   - l.70–74: every A-word := 3.
   - Loop 1 (l.92–101): if slot active (`[MI+0x1748+slot]`), class B[k] ≠ 0, A-word > 1, and (designated == slot (`P+0x3EF8`) or B-word == 2) → A-word := 2.
   - Loop 2 (l.106–123): inactive → A-word := 3. Active with class ≠ 0 and A-word > 2 (still 3) → A-word := B-word.
   - Sub-steps (l.65, l.75–77, l.129–140): see §4.
3. Flag reset (l.41–50): if `MI+0x11D4 == 0` and (`MI+0x116C == 2`, or `MI+0x116C == 3` and `MI+0x11A4 < 3·FUN_141E532E0()`, or `MI+0x11B0 < 0x15`) → flag := 0 (the copy path runs once). l.145–147: flag := 1 at the end.

## 3. The B-word (CE150 `FUN_1405CE150`, CONFIRMED decompile + asm for the index)

For each slot k in 0..10 (uVar15 = side·11 + k, EBX; the local k is ESI, checker-verified at 0x1405CE261, 0x1405CE293, 0x1405CE296):

| case | B-word | VA / line |
|---|---|---|
| B[k] == 0 (keeper) | 3 | l.109–111 |
| `P+0x3EF8 == uVar15` (designated) | 2 | l.113–116 |
| `MI+0x1554 == 4` and `MI+0x116C != 1` and `FUN_140A63950(team, side, 0xF, uVar15)` | 2 | l.113–115 |
| `FUN_140A64410(team, uVar15) == 0` | `FUN_1405CD540(local_148, k)` = line of B[k] (B 1,2 → 0; 3–6 → 1; 7–9 → 2; GK → 3) | l.119–122; asm 0x1405CE3B8–0x1405CE3EC (checker-verified: `MOV EDX,ESI` = k) |
| `FUN_140A64410(team, uVar15) == 1` | `FUN_1405CCD50(LX, local_148, uVar15)` (distance-based; returns 0 or 1 or 2) | l.123–129 |

- `FUN_1405CD540` is `ReclassifyLine(B[k])` with the keeper mapped to 3 (CONFIRMED, decompile).
- The B-rank (LX+0x64+8k) counts earlier slots with the same B-word in a sort order by the key `P+0x3AA4+12k` (slot-point f2, l.131) with overrides (INF); the sort is `FUN_140A79640` (not decoded).

## 4. A-word post-step writers (CONFIRMED that they write; conditions OPEN)

- `FUN_1405CF310` l.170 `*piVar16 = 0`, piVar16 = LX+8 (A-words, the stride is INF). Sets A := 0.
- `FUN_1405CFC80` l.80 `*puVar11 = 1`, puVar11 = LX+8 (A-words, stride INF). Sets A := 1.
- `FUN_1405CFFE0` l.203 `piVar21[-0x16] = iVar22`, piVar21 = LX+0x60 (B-words), `[-0x16]` = −0x58 bytes = LX+8 = A-word. Sets A := iVar22 (value OPEN).
- `FUN_1405CC1A0` (l.18–27) decrements the A-ranks (LX+0xC) only.
- `FUN_1405CC250`, `FUN_1405CDA40`, `FUN_1405CDF30`, `FUN_1405D05A0`, `FUN_1405CE930` (425 lines), `FUN_1405CDBD0`: no A-word store found by grep (OPEN: the grep covered `piVar`/`puVar` pointers and `param_1 + N` forms; `FUN_1405CE930` not read in full).

## 5. What this means for GF (the key verdict)

GF's `DefensiveLineOf` (PES path) returns `ReclassifyLine(roles.b[slot])`. For an in-play, out-of-possession team, PES's lane word equals that value for every active non-keeper slot that is not:

| # | case | PES lane word | GF `DefensiveLineOf` | blocker |
|---|---|---|---|---|
| 1 | designated slot (`P+0x3EF8`), active, class ≠ 0 | 2 | line of B | designation field: GF's publisher (UpdatePesDesignation) is INF for `P+0x3EF8` |
| 2 | `FUN_140A64410` == 1 (SMF/WF special) | `FUN_1405CCD50`: 0/1/2 by distance | line of B | OPEN: needs slot-point rows `S+0x3A9C+12k` (f0 signs), `T+0x46EC`, the instruction table `MA+0x25CF8`, `FUN_14063F760`/`F490` |
| 3 | restart kind 4 (`MI+0x1554==4`, not play) with instruction 15 | 2 | line of B | `FUN_140A63950` (GF stand-in false) |
| 4 | A-word post-step writers (§4) | 0 / 1 / iVar22 | line of B | conditions OPEN |
| 5 | inactive slots | 3 | line of B | none (GF returns a line for inactive slots) |
| 6 | keeper (B = 0) | 3 | 0 (`ReclassifyLine(0)`) | none; GF's keeper path not affected |
| 7 | flag 0 (stoppage copy path, §2 step 2) | A := B[k] (inactive included) | line of B | `MI` stoppage state |

So the stand-in is the exact lane line in the common case, and it differs in cases 1–7. Replacing `DefensiveLineOf`'s stand-in with the exact lane line needs cases 1–4 decoded and their inputs present. **Not portable this session (§7).**

## 6. Item 2: the slot-point rows and the band rows

**Slot-point rows `P+0x3A9C+12k` (f0, f1, f2) and `P+0x3B20+12k`** (the swap partner):

- `FUN_1404F5F10` (sheet bulk writer, decompile l.19–44; every team tick at the head and tail, gated `MI+0x1775 == 0`, per the role-writer doc): for each slot k, the triple `FUN_1408C50E0(sheet, k, f, 3)` is written to P+0x3A9C+12k and P+0x3B20+12k, with `f = 2 − (F≠0)`, and `f = 0` when `MI+0x116C != 1 && MI+0x1554 == 1`. CONFIRMED (decompile). The row values are the sheet's formation coordinates (INF that they are per formation and per in-possession level).
- `FUN_14051CF70` l.52–55 writes P+0x3A9C+lVar6 (set-piece path, the sheet triple, per the role-writer doc §4b). CONFIRMED (decompile).
- `FUN_1405F8430(ctx, phase)` (phase swap): copies the P+0x3A9C phase block `+0x84·phase` to P+0x3BA4 and back (`+0x3C04` block). CONFIRMED (decompile).
- `FUN_140A422C0` swaps the rows at P+0x3A9C/P+0x3B20 (role-writer §4b). CONFIRMED.
- `FUN_1404F9DC0` (non-restart team tick) writes the blend record `P+0x3BA4+12k` (decompile l.401–404: `pfVar7 = P+0x3BA4+uVar12`, two float stores). CONFIRMED.
- `FUN_1405EC600` reads the blend record (`lVar4+0x3BA4+lVar18·0x84`, l.139) and the slot-point rows.

**Band rows `S+0x7B10 / 7B14 / 7B24 / 7B28 / 7B38 / 7B3C`** (P = S here):

- Readers: `FUN_140681FF0` l.79–80 (`P+0x7B10`, `P+0x7B24`), `FUN_140682770` (`P+0x7B10..7B3C`), `FUN_140687FB0` l.22–66 (writes `param_3[]`, its locals, reads 7B10..7B3C), `FUN_1406855F0` l.45–60 (reads 7B14), `FUN_140684390` l.22–23 (reads 7B14, 7B28), `FUN_140685D70` l.40–41 (reads 7B38, 7B14), `FUN_1405FBCB0` l.53 (copies 7B14 → 7C3C).
- Writers: **OPEN.** Not among the 116 functions searched (100 team-tick callees of `FUN_1404F6C30`, `FUN_14051D9E0`, `FUN_1404F9DC0`, `FUN_1405EC600`, plus the 16 wrapper callees). The next search is the rest of the 4F6C30 call tree (the `FUN_1404FBCA0`, `FUN_140501260`, `FUN_1404F87D0` callees are in the list; their callees are not).

## 7. Item 3: the wrapper's post-steps and `FUN_140687A80`

**Post-steps** (681FF0 l.68–95, after 682770): `FUN_140685800` (reads list counts), `FUN_140686190` (reads lanes `ctx+0x58+8k` l.390 and ranks `ctx+0x5C+8k` l.412; a consumer, not a writer), `FUN_1406847C0`, `FUN_140683DB0` (reads lanes and ranks l.57–140; read loops), `FUN_140684390` (band rows), `FUN_140688300` (reads list counts), `FUN_140685D70`, `FUN_140685270`, `FUN_140682540`, `FUN_1406853F0`, `FUN_1406850E0` (slot points and band rows, reads). **No post-step writes the lane or B-word.** The lane writers are §2–4 only.

**`FUN_140687A80`** (the floor gate, `local_538` in 682770 l.270, returns a byte, decompile 186 lines):

- Returns 0 when `*(char*)(param_1[0x1E]+0xB0) == 0` (l.41), or when `MI+0x116C != 1 && MI+0x11D4 == 0` (l.42). CONFIRMED.
- Otherwise it scans the 11 slots of the side (`uVar12 = side·11 + k`, `param_1[0x22]` = side) and keeps those whose lane (the B copy at `param_1[0x25]`, i.e. local_458+0x128) minus 1 is 0 or 1 (l.112–115). **Only lane 1 (back line) passes**, since lanes are 1, 4, 7 (CONFIRMED from the switch).
- Each candidate passes four `FUN_140A4C720` tests (l.127–130, the opponent-line tests with player fields `+0x55C` and `+0x564`, INF). The survivors are sorted with `FUN_140A79640` (l.154) and written to a list (l.159–173). It returns 1 when the count of survivors `bVar13 < FUN_14063FA70(side)` (l.153, l.178), else 0. OPEN: `FUN_14063FA70`, `FUN_140A4C720`, and the call's arguments (682770 calls it with one argument in the decompile).
- Side effects: `FUN_140688300(param_1, list, 0, 1)` (l.174) and, when `T+0x254·param_1+0x21 > 0`, `FUN_140640C80(0, list)` (l.175–177), CONFIRMED.

## 8. Port verdict

**Not ported.** The exact lane line requires:

1. the slot-point rows: the sheet triple `FUN_1408C50E0` (not decoded; GF has no rows, `teamAIcontroller.cpp:3116–3118` uses the formation point as a stand-in);
2. the `FUN_140A64410` predicate and its inputs (slot points, instruction table `MA+0x25CF8` with ids 15/16);
3. `FUN_1405CCD50`'s distances (`FUN_14063F760`, `FUN_14063F490`);
4. the three A-word post-step conditions (CF310 l.170, CFC80 l.80, CFFE0 l.203);
5. the band rows (writers OPEN) for `FUN_140687A80`'s gate.

Items 1–5 are not GF inputs. Porting case 1 alone (designated → 2) would be a partial port, so I did not port it. No code changed; the branch is `origin/main` merged (e34bbda).

## 9. Next, in order

1. Decode the three post-step conditions (CF310 l.150–170, CFC80 l.60–80, CFFE0 l.180–203). They decide case 4.
2. Decode `FUN_1408C50E0` (the sheet triple) and port the slot-point rows (item 2 proper). This replaces the stand-in at `teamAIcontroller.cpp:3116–3118` and is the input for cases 2 and 5.
3. Find the band-row writers in the rest of the 4F6C30 call tree (`S+0x7B10/7B14`).
4. Decode `FUN_140A64410`'s remaining inputs and `FUN_1405CCD50` (case 2), then `FUN_140687A80` (gate).
5. Then port the exact lane line (A-word) in `pesmentality*` with `DefensiveLineOf` replaced on the PES path.

## 10. Evidence ledger

| ID | Claim | Verdict | Evidence |
|---|---|---|---|
| L1 | ctx = Y+0x10; LX = ctx+0x50 | CONFIRMED | 4F6C30 l.175, 243; 681FF0 0x14068205C (checker) |
| L2 | lane word A[k] at ctx+0x58+8k; rank at +0x5C | CONFIRMED | CE4F0 l.52–74 (param_1+8, +0xC) |
| L3 | B-word at ctx+0xB0+8k; B-rank at +0xB4+8k | CONFIRMED | CE150 l.72, l.110–128, l.176 |
| L4 | B-word: keeper 3, designated 2, else CD540/CCD50 | CONFIRMED | CE150 l.109–129; CD540 decompile; asm 0x1405CE3B8–EC (checker) |
| L5 | CD540 index is the local k | CONFIRMED | asm 0x1405CE261 `MOV ESI,R8D`, 0x1405CE296 `ADD EBX,ESI`, 0x1405CE3E7 `MOV EDX,ESI` (checker) |
| L6 | A-word recompute: 3 default; designated/B==2 → 2; else B | CONFIRMED | CE4F0 l.70–123 |
| L7 | A-word writers beyond the recompute: CF310→0, CFC80→1, CFFE0→iVar22 | CONFIRMED (writes); conditions OPEN | CF310 l.170, CFC80 l.80, CFFE0 l.203 |
| L8 | stand-in equals the lane line for in-play, active, non-designated, non-special, non-keeper slots | INF (high) | §5 table from L4, L6 |
| L9 | slot-point rows written from the sheet triple by F5F10 | CONFIRMED | F5F10 l.19–44 |
| L10 | band-row writers | OPEN | §6 |
| L11 | FUN_140687A80 gate: back-line count vs FUN_14063FA70 | CONFIRMED (structure); helpers OPEN | §7 |
