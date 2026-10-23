# PES block A (FUN_140681FF0 wrapper → FUN_140682770 forward pull): decode, checker, GF readers (BlockAPull, 23-10-26)

> **Correction (BlockAPull's later pass, `pes-re-lane-writers.md`):** every `T+0x58` / `T+0x5C` / `T+0xB0` lane offset below is **`ctx+0x58` / `ctx+0x5C` / `ctx+0xB0`**, with `ctx = Y+0x10` (`Y` = `FUN_1404F6C30`'s `param_1`), not the team record T.

Image: `pes_full.bin` (file offset = VA − 0x140000000). Checker: `gf-BlockAPull-scratch/checker_block_a.py`, 36 checks (25 instruction-byte checks, 11 RIP-relative operand pairs); `RESULT OK`; `--selftest` flips a byte and moves an operand, and both are detected. Verdicts: CONFIRMED = read as bytes by the checker, or in asm/decompile; INF = inference; OPEN = writer or input not decoded.

## §0 Constants (script output, pasted; the checker prints these from the image)

```
  0x14253ee30 = 100.0
  0x1425656c0 = 4.0
  0x14256ae1c = 1.0
  0x142588210 = 10.0
  0x142591140 = 30.0
  0x142592228 = 0.25
  0x14259222c = 5.0
  0x14259be4c = 70.0
  0x14259be84 = -10.0
  0x14259d5d0 = 0.6499999761581421
  0x1425c6614 = -100000.0
```

Operand pairs (instruction VA → operand VA, decoded RIP-relative): 0x140682BE4→0x142592228 (depth per level); 0x140682BD5→0x1425656C0 (level ceiling); 0x140682B59→0x14256AE1C (full factor); 0x1406832D9→0x14259D5D0 (rank scale, other); 0x14068305C→0x142591140 (CF bonus); 0x140683089→0x14259BE84 (count bump); 0x140682C81→0x1425C6614 (scan guard); 0x140682CE3→0x142588210 (+10 m); 0x1406828E0→0x14259222C (window); 0x14068220E→0x14259BE4C (band scale); 0x14068221C→0x14253EE30 (percent divisor).

## §1 Dispatch (CONFIRMED, bytes)

| VA | Bytes | Meaning |
|---|---|---|
| 0x1405ECC21 | `CMP BYTE [RBP+8C],R14B` | F = [local_458+0x10C] against 0 (R14B = 0 from 0x1405EC714) |
| 0x1405ECC28 | `JZ +7` → 0x1405ECC31 | F == 0: block A |
| 0x1405ECC44 | `CALL 0x140681FF0` | block A wrapper |
| 0x1405ECC2A | `CALL 0x14067B640` | F != 0: block B |
| 0x140682198 | `CALL 0x140682770` | the pull, from the wrapper |

local_458 = RBP−0x80 (`LEA RCX,[RBP-0x80]`, 0x1405EC6F6), so F = [RBP+0x8C] and the pointer below is [RBP+0xA8].

## §2 The lane table that block A reads (CONFIRMED by bytes; corrects pes-re-mentality-blocks §2 and §3.3)

- EC600 points local_458+0x128 at its B copy: `LEA RAX,[RBP+0x2A0]` (0x1405EC9FE), `MOV [RBP+0xA8],RAX` (0x1405ECA0A). The copy is P+0x3FB8 (loop 0x1405EC9C4–EC9FC).
- EC600's shared reclassify reads that copy before block A runs (`MOV RAX,[RBP+0xA8]`, 0x1405ECB20). It writes the family-F records: class at 0x1405ECBB0, rank at 0x1405ECBBE.
- Block A is then called with the same buffer as param_5 (`LEA RAX,[RBP+0x2A0]`, 0x1405ECC31; `MOV [RSP+0x20],RAX`, 0x1405ECC3F). The wrapper **overwrites that buffer** for the 11 slots (loop 0x140682090–0x1406820F9):
  - lane(k) = v(T[+0x58+8k]), with v(0)=1, v(1)=4, v(2)=7, else 0. Bytes: `MOV ECX,[R9-0x58]` (0x140682090); `MOV [R8],1` (0x140682098); `MOV [R8],4` (0x1406820A6); `XOR EAX,EAX` (0x1406820AF); `CMOVZ EAX,R15D` with R15D = 7 (0x1406820B4; `LEA R15D,[R10-4]` at 0x140682088).
  - P+0x7D60+4k = w(T[+0xB0+8k]): w(0)=0, w(1)=1, w(2)=2, w(3) = no store, else 0. Bytes: `MOV ECX,[R9]` (0x1406820BB); `CMP ECX,3; JZ` (0x1406820C1); `MOV [RCX+R8],EDX` (0x1406820E9), with RCX = P (`MOV RCX,[RBX+0xBA8]`).
- The switch in the pull (decompile, 682770 l.437–463) reads the lane, not the B class: lane 1 → line 0; lanes 4–6 → line 1; lanes 7–9 → line 2; lane 0 → slot skipped. The jump-table bytes are not byte-checked here.
- **Result: for an on-pitch slot k with T[+0x58+8k] ∈ {0,1,2}, the out-of-possession line is that word.** The B class does not enter the family-0 line. The mentality doc's "CF 2→1 at −1" is therefore a T+0x58 value for the CF slot (writer OPEN, §7).

## §3 Per-slot writes of block A (CONFIRMED stores; semantics from decompile)

| Field (planObj P = [ctx+0xBA8]) | Store VA | Value | Verdict |
|---|---|---|---|
| +0x7B88 + 8·(F·11+k), class dword, family F = 0 | 0x140683809 `MOV [R11+RAX*8+0x7B88],R10D` | line of §2 | CONFIRMED |
| +0x7B8C + 8·(F·11+k), rank byte | 0x140683818 `MOV [R11+RAX*8+0x7B8C],DIL` | DIL = bVar18, slot k's index in its line list | store CONFIRMED; index semantics OPEN (§5) |
| +0x7B20 + 0x14·line + 0x3C·F (count), +0x7B21 (active) | 682320 final loop (called 0x14068210B) | per-line counts | CONFIRMED (decompile) |
| +0x7D60 + 4k | 0x1406820E9 | w(T+0xB0+8k) | CONFIRMED |
| +0x7C40 | 0x140683CCB `MOVSS [R11+0x7C40],XMM10` | line-2 band top (fVar4 + 5.0 per decompile l.761) | store CONFIRMED; operand OPEN |
| +0x7C44 | 0x140683CED `MOVSS [R11+0x7C44],XMM1` | line-1 band top (decompile l.762) | store CONFIRMED; operand OPEN |
| +0x3A9C + 12k (float 0 score, float 2 = Y key) and the target triple param_3 + 12k | decompile l.398–430, 640–690 | per-slot score and depth | structure CONFIRMED; inputs OPEN (§6) |

## §4 Forward depth (CONFIRMED constants; the mapping of band rows to the GF frame is INF)

- Factor (0x140682BD5–0x140682BEC): `COMISS XMM7,[4.0]`; below it, `MULSS XMM7,[0.25]`; otherwise `MOVAPS XMM7,XMM9` with XMM9 = `MOVSS [1.0]` (0x140682B59). So factor = level·0.25 below level 4, else 1.0. Matches `pes::forward::DepthFactor`.
- Rank scale: ×0.65 (0x1406832D9) under the predicate arms in decompile l.491–496. The ×0.2 operand is not byte-checked here.
- CF bonus +30.0 (0x14068305C), count bump −10.0 (0x140683089), scan guard −100000.0 (0x140682C81), +10 m (0x140682CE3), window 5.0 (0x1406828E0).
- Band clamp in the wrapper (682770's caller, 0x1406821E5–0x14068223B): slot depth ≤ ((S+0x7B10 − S+0x7B24)·70.0)/100.0 + S+0x7B24, applied to slot P+0x3F00 % 11.
- deep / high: local_4a0[line] and local_4a0[line+3], filled by FUN_140687FB0 (reads S+0x7B10+0x14·line and S+0x7B14+0x14·line; l.22–25) and FUN_1406855F0 (l.14–62). Their mapping to GF's frame: INF.

## §5 Rank (CONFIRMED structure; the index byte's writer is OPEN)

- The wrapper's list builder, FUN_140682320 (0x14068210B), places slot k's entry at index byte T[+0x5C+8k] in its line list (`pbVar12 = param_1+0x5C`, decompile l.40–79). Count at +0x58, active at +0x59.
- The pull scans that list for slot k and takes the index it finds (l.480–482). That index is the rank.
- EC600's own rank (BL, 0x1405ECBBE) comes from FUN_1405F9CD0's list, which sorts entries with FUN_141E53540 by a float key (comparator LAB_1405F6380, not decoded). Block A's rank overwrites EC600's rank for the slots it matches.
- The docs' "rank = team-sheet order" and pesforward's "rank = slot order" are not confirmed; the rank is T+0x5C+8k, and its writer is OPEN.

## §6 What GF reads of these fields (grep over src/, 23-10-26)

- P+0x7B88, +0x7B8C, +0x7B20, +0x7C40, +0x7C44, +0x7D58, +0x7D60, +0x7CD4, +0x4B40: no reader in src/ (only comments in pesforward.hpp, teamAIcontroller.cpp:444/514, teamAIcontroller.hpp:307).
- T+0x58, +0x5C, +0xB0 (team-tick per-slot words): not in GF. `TeamTick` (pesteamtick.hpp) has b8a4, b8a2, b8a3, t80, b8c0Frame only.
- Slot-point rows +0x3A9C/+0x3BA4 and band rows +0x7B10/+0x7B14/+0x7B24/+0x7B28/+0x7B38/+0x7B3C: not in GF (pesroles.hpp:60 says so).
- The only GF reader of the slot line is `TeamAIController::DefensiveLineOf` → `ReclassifyLine(roles.b[slot])` (MentalityPort). That is EC600's family-F value. Block A changes it for out-of-possession teams (§2). **DefensiveLineOf is right in possession and a stand-in out of possession, and the size of the difference depends on T+0x58 (OPEN).**
- GF's forward depth is its own stand-in (teamAIcontroller.cpp:668–675: `line+len`, `max(line+len−5, line+midGap)`). PES's forward depth is `lo + (hi−lo)·f` (§4), and GF has no band rows for lo and hi.

## §7 Not decoded (OPEN), with VAs

1. Writers of T+0x58+8k (the lane, a line 0–2 per slot), T+0xB0+8k, and T+0x5C+8k (the rank index). Not in EC600's asm (the grep for 0x58/0xB0/0x5C found none), not in FUN_1404F8350 (the dial's DriftAttackLevel).
2. Writers of S+0x7B10/7B14 (band rows) and S+0x7B24–0x7B3C, consumed by FUN_140687FB0 and the wrapper's band clamp.
3. Slot-point writers: P+0x3A9C float 0 (the score the protected scan reads at 682770 l.398–430) and P+0x3BA4 (the blend v108, written by FUN_1404F9DC0 per pes-re-attack-level-pull §9).
4. FUN_140687A80 (floor gate, returns local_538; RE-4) and [[ctx]]+0x44 (floor reference).
5. Wrapper calls: FUN_1405CC070 (builds local_148), FUN_1405CE4F0 (T+0x50), FUN_140640C80 ×3 (when [T+0x108] > 0), FUN_140685800 (MI+0x116C != 2 and FUN_141B92460(2) > 1), FUN_140686190, FUN_1406847C0 (when [P2+0x2A8]), FUN_140683DB0, FUN_140684390, FUN_140685D70, FUN_140685270, FUN_140682540, FUN_1406853F0, FUN_1406850E0 (the last six when FUN_141B92460(2) ≥ 2). None is decoded.
6. FUN_1406405C0 (the mode-0/1 predicate, RE-2), FUN_140A63950 (instruction 15, the counter target), FUN_140A627E0 (holder), FUN_14043A0A0 (fold angle), FUN_141E53540 and LAB_1405F6380 (the EC600 sort and comparator).

## §8 Port verdict

Block A is not ported. Its outputs (the family-0 line and rank, the band tops, the forward depth) depend on inputs GF does not model (§6, §7). A forward-depth port would need stand-ins for the band rows, the slot-point scores, the floor reference, the rank index and the counter. That would be a look-alike, not a port. The one exact output, the family-0 line, is T+0x58+8k, which has no GF state and no located writer.

## §9 Corrections to docs/re/pes-re-mentality-blocks.md

- §2 and §3.3: block A's class write comes from the lane word T+0x58+8k, not the B class. The wrapper writes the lanes over the B copy (0x140682090–0x1406820B4) before the pull's switch reads them.
- §3.3, "forward switch + rank scale rewrites the CF's class 2→1": the switch input is the lane, so the CF's 2→1 at −1 needs T+0x58 of the CF slot to be 1 (writer OPEN).
- EV-BLK-0005 ("class written only by EC600 + block A") holds. The "no class = 2 immediate" census is unchanged.

## §10 Next steps for Main

1. Find the writers of T+0x58+8k, T+0xB0+8k and T+0x5C+8k (team-tick record stores; the dial path FUN_1404F8350 has none).
2. Find the writers of S+0x7B10/7B14 (band rows) and P+0x3A9C/+0x3BA4 (slot points; FUN_1404F9DC0).
3. Then port block A's out-of-possession line from T+0x58 (exact, no stand-in), and the forward depth with `pes::forward::ForwardLine`.
