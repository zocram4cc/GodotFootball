# PES block A (FUN_140681FF0 wrapper → FUN_140682770 forward pull): decode, checker, GF readers (BlockAPull, 23-10-26, final)

Image: `pes_full.bin` (file offset = VA − 0x140000000). Checker: `<workdrive>/gf-BlockAPull-scratch/checker_block_a.py` (`python3 checker_block_a.py`: RESULT OK, 36 instruction-byte checks + 11 RIP-relative operand pairs = 47; `--selftest` OK). Verdicts: CONFIRMED = read as bytes by the checker, or in asm/decompile; INF = inference; OPEN = writer or input not decoded.

**Correction (final).** The lane and rank words are at `ctx+0x58+8k` and `ctx+0x5C+8k`, where `ctx = Y+0x10` (`Y` = `FUN_1404F6C30`'s `param_1`). Earlier drafts wrote `T+0x58`; that base was wrong. Their writers are decoded in `pes-re-lane-writers.md`. Also corrected: the B class does reach the lane word (§2), so the earlier "the B class does not enter the family-0 line" is withdrawn.

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

## §2 The lane word that block A reads (CONFIRMED)

- EC600 points local_458+0x128 at its B copy: `LEA RAX,[RBP+0x2A0]` (0x1405EC9FE), `MOV [RBP+0xA8],RAX` (0x1405ECA0A). The copy is P+0x3FB8 (loop 0x1405EC9C4–EC9FC).
- EC600's shared reclassify reads that copy before block A runs (`MOV RAX,[RBP+0xA8]`, 0x1405ECB20). It writes the family-F records: class at 0x1405ECBB0, rank at 0x1405ECBBE.
- Block A is then called with the same buffer as param_5 (`LEA RAX,[RBP+0x2A0]`, 0x1405ECC31; `MOV [RSP+0x20],RAX`, 0x1405ECC3F). The wrapper overwrites that buffer for the 11 slots (loop 0x140682090–0x1406820F9):
  - lane(k) = v(ctx[+0x58+8k]), with v(0)=1, v(1)=4, v(2)=7, else 0. Bytes: `MOV ECX,[R9-0x58]` (0x140682090); `MOV [R8],1` (0x140682098); `MOV [R8],4` (0x1406820A6); `XOR EAX,EAX` (0x1406820AF); `CMOVZ EAX,R15D` with R15D = 7 (0x1406820B4; `LEA R15D,[R10-4]` at 0x140682088).
  - P+0x7D60+4k = w(ctx[+0xB0+8k]): w(0)=0, w(1)=1, w(2)=2, w(3) = no store, else 0. Bytes: `MOV ECX,[R9]` (0x1406820BB); `CMP ECX,3; JZ` (0x1406820C1); `MOV [RCX+R8],EDX` (0x1406820E9), with RCX = P (`MOV RCX,[RBX+0xBA8]`).
- The switch in the pull (decompile, 682770 l.437–463) reads the lane, not the B class: lane 1 → line 0; lanes 4–6 → line 1; lanes 7–9 → line 2; lane 0 → slot skipped.
- **Result: the out-of-possession line of an on-pitch slot is its lane word ctx[+0x58+8k] ∈ {0,1,2} (3 = none).** The B class reaches that word through its writers (`pes-re-lane-writers.md` §3). In the common in-play case the lane word equals `ReclassifyLine(B[k])`, which is GF's `DefensiveLineOf` PES-path value; a designated slot is forced to 2 (lane-writers §2, §5).
- The mentality doc's "CF 2→1 at −1" (`pes-re-mentality-blocks.md` §3.3) is not reproduced by the decoded writers: the designated slot gets 2, and a 2→1 needs a post-step A-word writer (CF310 → 0 or CFC80 → 1), whose condition is OPEN.

## §3 Per-slot writes of block A (CONFIRMED stores; semantics from decompile)

| Field (planObj P = [ctx+0xBA8]) | Store VA | Value | Verdict |
|---|---|---|---|
| +0x7B88 + 8·(F·11+k), class dword, family F = 0 | 0x140683809 `MOV [R11+RAX*8+0x7B88],R10D` | line of §2 | CONFIRMED |
| +0x7B8C + 8·(F·11+k), rank byte | 0x140683818 `MOV [R11+RAX*8+0x7B8C],DIL` | DIL = bVar18, slot k's list position (ctx[+0x5C+8k], §5) | store CONFIRMED; index is the list position |
| +0x7B20 + 0x14·line + 0x3C·F (count), +0x7B21 (active) | 682320 final loop (called 0x14068210B) | per-line counts | CONFIRMED (decompile) |
| +0x7D60 + 4k | 0x1406820E9 | w(ctx+0xB0+8k) | CONFIRMED |
| +0x7C40 | 0x140683CCB `MOVSS [R11+0x7C40],XMM10` | line-2 band top (fVar4 + 5.0 per decompile l.761) | store CONFIRMED; operand OPEN |
| +0x7C44 | 0x140683CED `MOVSS [R11+0x7C44],XMM1` | line-1 band top (decompile l.762) | store CONFIRMED; operand OPEN |
| +0x3A9C + 12k (float 0 score, float 2 = Y key) and the target triple param_3 + 12k | decompile l.398–430, 640–690 | per-slot score and depth | structure CONFIRMED; inputs OPEN (§6) |

## §4 Forward depth (CONFIRMED constants; the mapping of band rows to the GF frame is INF)

- Factor (0x140682BD5–0x140682BEC): `COMISS XMM7,[4.0]`; below it, `MULSS XMM7,[0.25]`; otherwise `MOVAPS XMM7,XMM9` with XMM9 = `MOVSS [1.0]` (0x140682B59). So factor = level·0.25 below level 4, else 1.0. Matches `pes::forward::DepthFactor`.
- Rank scale: ×0.65 (0x1406832D9) under the predicate arms in decompile l.491–496. The ×0.2 operand is not byte-checked here.
- CF bonus +30.0 (0x14068305C), count bump −10.0 (0x140683089), scan guard −100000.0 (0x140682C81), +10 m (0x140682CE3), window 5.0 (0x1406828E0).
- Band clamp in the wrapper (0x1406821E5–0x14068223B): slot depth ≤ ((S+0x7B10 − S+0x7B24)·70.0)/100.0 + S+0x7B24, applied to slot P+0x3F00 % 11.
- deep / high: local_4a0[line] and local_4a0[line+3], filled by FUN_140687FB0 (reads S+0x7B10+0x14·line and S+0x7B14+0x14·line; l.22–25) and FUN_1406855F0 (l.14–62). Their mapping to GF's frame: INF.

## §5 Rank (CONFIRMED structure; the rank byte's writers are partly OPEN)

- The wrapper's list builder, FUN_140682320 (0x14068210B), places slot k's entry at list position ctx[+0x5C+8k] in its line list (`pbVar12 = param_1+0x5C`, decompile l.40–79). Count at +0x58, active at +0x59 (of each line record).
- The pull scans that list for slot k and takes the index it finds (l.480–482). That index is the rank stored at 0x140683818.
- EC600's own rank (BL, 0x1405ECBBE) comes from FUN_1405F9CD0's list, which appends in slot order and sorts with FUN_141E53540 by a float key (comparator LAB_1405F6380, not decoded).
- The rank byte ctx+0x5C+8k: writers are the copy path (B-rank → A-rank, CE4F0 l.59–60) and FUN_1405CC1A0 (decrement, CONFIRMED). **The recompute path's A-rank writer is OPEN**: CE4F0's own code does not store it, and the A-rank stores of the post-steps were not checked.
- The docs' "rank = team-sheet order" and pesforward's "rank = slot order" are not confirmed. The rank is the list position written at ctx+0x5C+8k.

## §6 What GF reads of these fields (grep over src/, 23-10-26)

- P+0x7B88, +0x7B8C, +0x7B20, +0x7C40, +0x7C44, +0x7D58, +0x7D60, +0x7CD4, +0x4B40: no reader in src/ (only comments in pesforward.hpp, teamAIcontroller.cpp:444/514, teamAIcontroller.hpp:307).
- ctx+0x58, ctx+0x5C, ctx+0xB0 (the lane and B-word records): not in GF. `TeamTick` (pesteamtick.hpp) has b8a4, b8a2, b8a3, t80, b8c0Frame only.
- Slot-point rows +0x3A9C/+0x3BA4 and band rows +0x7B10/+0x7B14/+0x7B24/+0x7B28/+0x7B38/+0x7B3C: not in GF (pesroles.hpp:60 says so; teamAIcontroller.cpp:3116–3118 uses the formation point as a stand-in).
- The only GF reader of the slot line is `TeamAIController::DefensiveLineOf` → `ReclassifyLine(roles.b[slot])` (MentalityPort). For in-play, active, non-designated slots this equals the lane word (§2). The cases where it differs are listed in `pes-re-lane-writers.md` §5.
- GF's forward depth is its own stand-in (teamAIcontroller.cpp:668–675: `line+len`, `max(line+len−5, line+midGap)`). PES's forward depth is `lo + (hi−lo)·f` (§4), and GF has no band rows for lo and hi.

## §7 Not decoded (OPEN), with VAs

1. Lane and rank writers: **decoded** (`pes-re-lane-writers.md` §2–4). Still OPEN: the recompute path's A-rank writer; the A-word post-step conditions (CF310 decompile l.150–170, CFC80 l.60–80, CFFE0 l.180–203) and the value iVar22 stored at CFFE0 l.203.
2. Band rows S+0x7B10/7B14/7B24/7B28/7B38/7B3C: readers listed in `pes-re-lane-writers.md` §6; writers OPEN (not in the 116 functions searched). Slot points P+0x3A9C/P+0x3B20: writers decoded (F5F10 from the sheet triple FUN_1408C50E0, not decoded; FUN_14051CF70; FUN_1405F8430; FUN_140A422C0). Blend record P+0x3BA4: FUN_1404F9DC0 (decompile l.401–404).
3. FUN_140687A80 (the floor gate, `local_538`): decoded in structure (`pes-re-lane-writers.md` §7). Its helpers FUN_140A4C720 and FUN_14063FA70 are OPEN, and 682770 calls it with one argument in the decompile.
4. [[ctx]]+0x44 (the floor reference); FUN_1406405C0 (RE-2); FUN_140A63950 (instruction 15); FUN_140A627E0 (holder); FUN_14043A0A0 (fold angle); FUN_140A64410 (the special predicate) and FUN_1405CCD50 (its distance-based B-word), see `pes-re-lane-writers.md` §3; FUN_140A79640 (the sort).
5. The wrapper's post-steps (681FF0 l.68–95) were searched: none writes the lane or the B-word (`pes-re-lane-writers.md` §7).

## §8 Port verdict

Block A is not ported. Its lane word has a decoded writer chain, but the exact value needs the slot-point rows (sheet triple FUN_1408C50E0), FUN_140A64410's predicate with FUN_1405CCD50's distance inputs, and the A-word post-step conditions. The forward depth needs the band rows (writers OPEN), the slot-point scores, the floor gate FUN_140687A80 and [[ctx]]+0x44. A forward-depth port now would need stand-ins: a look-alike. See `pes-re-lane-writers.md` §8.

## §9 Corrections to docs/re/pes-re-mentality-blocks.md

- §2 and §3.3: block A's class write comes from the lane word ctx+0x58+8k, not from the B class directly. The wrapper writes the lanes over the B copy (0x140682090–0x1406820B4) before the pull's switch reads them. The lane word is ReclassifyLine(B[k]) in the common case (designated → 2), so the B class does reach block A's class write, through the writers.
- §3.3, "forward switch + rank scale rewrites the CF's class 2→1 at −1": not reproduced by the decoded writers (§2 above).
- EV-BLK-0005 ("class written only by EC600 + block A") holds. The "no class = 2 immediate" census is unchanged.

## §10 Next steps for Main

1. Queued by Main: FUN_1408C50E0 (the sheet triple). It gives the slot-point rows, which the FUN_140A64410 predicate, the CCD50 case and the rank sort need.
2. The A-word post-step conditions (CF310, CFC80, CFFE0) and the A-rank writer of the recompute path.
3. Band-row writers (S+0x7B10/7B14).
4. Then port the lane word in `pesmentality*` and replace DefensiveLineOf's PES-path value (`pes-re-lane-writers.md` §9).
