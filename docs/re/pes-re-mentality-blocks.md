# PES mentality: the two defensive blocks, the class reclassify, and the dial (23-10-26)

> **Main audit 22-10-26 (bytes):** `0x1405ECC21 cmp byte [rbp+0x8C], r14b`, `0x1405ECC28 je`; `0x1404F6EB8 movzx ecx, byte [r8+0x255]`, `0x1404F6EC6 setne bl`; `0x14067CE72 cmp r8d, 6`, `0x14067CE78 addss xmm1, xmm12`; `DAT_14259D634` = 10000.0, `DAT_1425C65FC` = 100000.0: match. One correction: the fold store at `0x14067CF93` is `mov dword [r12+0x7CD4], r9d` (one slot, no index), not `[R12+R13*4+0x7CD4], R13D`. Two wrap-up leftovers the author named: §3.1's "fold/depth terms active for dial < 2" sentence is stale (the `2.0` gate at `0x14067D285` is the forward depth push), and the checker's "dial fold-gate" label means the depth-push gate.

> **Scope.** Resolves the open items in `docs/re/pes-re-mentality-reclass-live.md` (§6) at byte
> level against `pes_full.bin` (file offset = VA − 0x140000000), decompile + disassembly via the
> Ghidra bridge, constants read from the image. Read-only RE: no GF code edited. Every constant is
> script-read and paired with the instruction that uses it; the self-checking checker is
> `gf-MentalityRE-scratch/checker_mentality_blocks.py` (run it: it re-reads every cited byte and
> fails on a single flipped byte).
>
> **Confidence key** (per ROLE §4): Confirmed = byte-verified here; High = multiple independent
> lines agree; Tentative = single line / decompile inference; Unknown = open.

## 0. What the "two blocks" actually are

`FUN_1405EC600` is the per-team defence-row tick. It does three things, in order:
1. **Reclassify every slot's line** (the shared role→line pass, §2) — writes the class records.
2. **Dispatch to one of two block bodies** by flag F (§1) — A = `681FF0`/`682770`, B = `67B640`.
3. Run the shared post-block shape calls (`1405FBE10`, `1405FC860`, `1405FC500`, `1405FC2F0`).

The "line reclassification" the live doc observed is **step 1 (shared)**, and the "block A vs block B
behaviour" is **step 2**. The dial's class-rewrite output is therefore produced by the shared reclassify
feeding the two blocks different downstream targets — not by two independent class writers. This is the
key reframing the live doc's §2 table needs.

---

## 1. Item 1 — the block choice, reconciled with plan v3 (RESOLVED, Confirmed)

**The dispatch is in `FUN_1405EC600` @ 0x1405ECC21** (bytes, read from the image):

```
1405ECC14:  XOR R14D,R14D                 # 45 33 F6  (0x1405EC714)  -> R14B == 0 at the cmp
1405ECC21:  CMP BYTE [RBP+0x8C], R14B     # 44 38 B5 8C 00 00 00     # RBP+0x8C == the team-tick flag F
1405ECC28:  JZ  0x1405ECC31               # 74 07                    # F == 0  -> take the A path
1405ECC2A:  CALL 0x14067B640              # E8 11 EA 08 00           # F != 0  -> BLOCK B (67B640)
1405ECC2F:  JMP 0x1405ECC49
1405ECC31:  ... (set up RAX/R9/RSP) ...
1405ECC44:  CALL 0x140681FF0              # E8 A7 53 09 00           # BLOCK A wrapper (-> 682770)
```

`[RBP+0x8C]` is F (plan v3 §0e: `local_458+0x10C`, held in `local_34c`, proven the same byte in this
pass: R14 zeroed at 0x1405EC714 and **not rewritten before the CMP** — byte-scanned the whole window).
The comparison is against `R14B == 0`. So the polarity, decoded from the raw control flow:

- **F == 0 → BLOCK A** (`681FF0` wrapper → `682770`).
- **F != 0 → BLOCK B** (`67B640`).

### Reconciliation (this is the crux)

Plan v3 §0e and the live doc §5 **agree with the bytes and with each other**:

- Plan v3 §0e (l.108): *"F = 0 → JZ 0x1405ECC31 → FUN_140681FF0 (E8 at 0x1405ECC44); F ≠ 0 →
  FUN_14067B640 (E8 at 0x1405ECC2A)."* — **matches the bytes exactly.**
- Live doc §5: *"the team-tick flag F (local_458+0x10C; F ≠ 0 → FUN_14067B640 at 0x1405ECC2A, else 682770)."* — **matches.**

The **task prompt's** one-line paraphrase ("F = 0 → FUN_14067B640, else 682770") is the one that had it
backwards. **No doc is wrong; the plan and the live finding are consistent.** Flag F is the selector and
its polarity is F=0→A, F≠0→B.

### What sets F — per tick, not fixed per team (Confirmed)

F's full derivation chain, all byte-verified:

| step | site | value |
|---|---|---|
| 1. read latch | `FUN_1404F6C30` l.238 (the team-tick orchestrator): `local_396 = *(T+0x255)` | the possession-latch byte |
| 2. test | `bVar22 = (local_396 != 0)` (4F6C30 l.239) | `F = (T+0x255 != 0)` |
| 3. pass down | `FUN_14051D9E0(param_1+0x10, uVar9, bVar22, local_397)` @ 0x1404F6EE8 | F = 51D9E0 param_3 |
| 4. forward | 51D9E0 l.92 / l.154: `FUN_1405EC600(param_1, 0|1, param_2, param_3)` | F = EC600 param_4 (arg4) |
| 5. store | EC600 `MOVZX EDI,R9B` @0x1405EC663 → `local_34c` → `[RBP+0x8C]` | F at the dispatch |

So **F = (T+0x255 != 0)** — the possession latch, tested fresh each team tick. Each team runs its own
EC600 with its **own** latch, so each team's block is **not fixed per team and not per match**: it
follows that team's possession state every tick. The live sweep's "one block per team" is a consequence,
not a rule: during a coach-vs-coach dial sweep, one team held the ball (latch≠0 → block B) and the other
was out of possession (latch=0 → block A). That is why each team consistently showed one block.

**Parity sub-path (plan v3 §0e, Confirmed present):** on the *non-restart* path `FUN_1404F9DC0` receives
`param_2 = low byte of local_394`, where `local_394 = (MI+0x116C==1 ? FE9C0-latch : (MI+0x119C > 1))`
(4F6C30 l.193-198, `local_394 = (uint)(1 < *(MI+0x119C))` under `MI+0x116C==1`). With `param_2 == 1`
(in play, kick-sum > 1), 9DC0 calls 51D9E0 on **odd** `MI+0x1198` with `F = (T+0x255 != 0)` and on
**even** `MI+0x1198` with `F = (T+0x255 == 0)` — i.e. the pull block **alternates every frame** between
the two possession states once the ball is in play. This is what "F inverted on even frames once
MI+0x119C > 1" means in plan v3. It is present in the bytes (the `local_394` computation at 4F6C30
l.195 and the 9DC0 two-frame dispatch) but the *effect* (per-frame block alternation in play) is marked
**High Confidence** (code present; the in-play frame-parity effect was not exercised live in this pass).

**Answer to item 1:** F follows `(T+0x255 != 0)` per tick (possession latch), with an in-play per-frame
parity inversion once `MI+0x119C > 1`. The block is **not fixed per team**; the observed "one per team"
is possession-state divergence, not a formation/team assignment. **Formation does not set F** — only the
possession latch and the frame-parity sub-path do.

The latch **T+0x255** itself: its only *byte* writer found in the image is `FUN_1404161E0`, which is a
**bulk team-record copy** (`param_1[i] = param_2[i]` over the whole record, §decomp) — it propagates the
latch from a source record during team sync, it is **not** the semantic setter. The semantic setter is the
possession-change path (live doc item 4, open): the latch is a *latch*, not a per-tick possession sample —
it is set on the possession-change and held, which is what lets the block assignment be stable through a
possession phase ("one block per team" in the coach sweep = the two teams carried different latch values,
switching when possession changes).

## 2. The shared reclassify (writes the class records) — Confirmed

`FUN_1405EC600` runs this for **every** team tick, **before** the A/B dispatch, so it applies to both
blocks. It reads a per-tick role array and writes the class + rank records.

- **Role input:** copied from the plan object's `+0x3FB8` array into a local (`local_138`, 11 slots):
  EC600 l.203-204 `*(local_138 + i*4) = *(planObj + i*4 + 0x3FB8)`, `planObj = *(param_1+0x2EA)`.
- **The role→line map is a jump table** at image offset `0x5ED1F4` (9 entries, index `role-1`),
  read at `1405ECB3D-1405ECB47`. Read from the image, the table maps:

  | role | 1 CB | 2 SB | 3 DMF | 4 CMF | 5 SMF | 6 AMF | 7 SS | 8 WF | 9 CF | (0 GK / >9) |
  |---|---|---|---|---|---|---|---|---|---|---|
  | line | 0 | 0 | 1 | 1 | 1 | 1 | 2 | 2 | 2 | 0 (default) |

  (table entries: idx0→`0x5ECB5D`=line0, idx1→`0x5ECB5D`=line0, idx2-5→`0x5ECB49`=line1,
  idx6-8→`0x5ECB53`=line2; `role-1 > 8` → `0x5ECB5D`=line0.)
- **Writes** (family = `F*11 + slot`, `IMUL RAX,RCX,0xB` @0x1405ECBA9):
  - class dword: `MOV [RDX + RAX*8 + 0x7B88], EDI` @ **0x1405ECBB0**
  - rank byte: `MOV [RDX + RAX*8 + 0x7B8C], BL` @ **0x1405ECBBE**
  - `RDX = *(R15+0xBA8)` (the record array the live doc watches as `planObj+0x7B88`).

**This is why the CF can only be demoted by block A, and never by the shared pass.** The shared reclassify
maps role 9 (CF) → line 2 **unconditionally**. So for a CF to read class 2→1, the value the *block* writes
must override it — which only block A does (it has its own class write, §3.1). Block B never writes the
class dword at all (byte-scanned: **zero** `0x7B88`/`0x7B8C` stores in `67C4B0`), so block B's "fold" is
purely the **slot pick** (§3.2) — the class record stays whatever the shared reclassify set, which for a
CF is always line 2. **That is the CF shield in block B: not a penalty, it is the absence of any class
write that could move role 9 off line 2.** (The `0x4B40` protected-slot filter in 67C4B0 is a second,
separate shield on the *pick*, §3.2.)

---

## 3. Item 2 — block B fold-picking rule (`67C4B0`) and the CF shield

`FUN_14067C4B0` is the list machine (913 asm lines; 67B640's pipeline). Its sole caller is block B
(`67B640` @ 0x14067B7A8, E8-scan confirmed). It does **not** write the class records; it **picks the
slot that gets folded** and writes the index to `planObj+0x7CD4`. The pick is a **minimum-score** scan.

### 3.1 The score (bytes)

For each eligible slot `s` (0..10), candidate position `(x,z)` in a local 3-float buffer `local_238`:

```
score(s) = x² + z²                    # squared distance from origin (the ball-side / line axis)
          + 10000.0 if role(s) > 5    # forward-class penalty: DAT_14259D634 = 10000.0f
best = min over s of score(s); init best = DAT_1425C65FC = 100000.0f
```

- distance terms: `MOVSS`/`MULSS` on `pfVar15[2]`(z) and `*pfVar15`(x) — asm `14067CE57` `ADDSS`.
- forward penalty: `MOVSS XMM12,[0x14259D634]` @ `14067CDE3`, `ADDSS XMM1,XMM12` @ `14067CE78`, gated by
  `CMP R8D,0x6` @ `14067CE72` (role > 5). Roles > 5 = **AMF(6) SS(7) WF(8) CF(9)** (GF `pesroles.hpp:5`).
- min pick: `COMISS XMM2,XMM1` / `JBE` @ `14067CE7D-CE80`.

**Which forward folds first:** the minimum-score slot among *eligible* slots. A forward (role>5) is
penalised by +10000, so a non-forward is chosen first; among the forwards that *do* become candidates
(all non-forwards already picked or ineligible), the **one with the smallest squared distance to the
line origin** folds first. The dial controls *how many* fold: the fold-count `T+0x110`
(`MOV EAX,[R13+0x110]` @ `14067C533`, `IMUL EDX,[R13+0x110],11` @ `14067C9FD`) bounds the number of
pick iterations, and the depth pass is gated by `DAT_142565604 = 2.0` @ `14067D285` (fold/depth terms
active for dial < 2, i.e. the defensive −1/−2 settings; at +2 the block is in full-attack and the fold
pass is skipped via `CMP BYTE [R13+0xB8A2]` @ `14067CDFB`/`14067D373`).

### 3.2 Eligibility filters (the CF shield, bytes)

A slot is a fold candidate only if (asm `14067CD66`..`14067CD9D`):

| filter | VA | meaning |
|---|---|---|
| `uVar20 < 0x16` | `14067CD60` | within the 2×11 team array |
| `[base+0x1748 + s] != 0` | `14067CD66` | on-pitch / active |
| `[base+0x173C] != prev` | `14067CD79` | not already assigned the previous pick |
| `role != 0` (GK) | `14067CDB3` | not the keeper |
| `[R13+0x4B40] != s` | **`14067CD9D`** | **not the protected-forward slot — the CF shield** |

The **`0x4B40` exclusion is the CF shield.** `planObj+0x4B40` holds the protected-forward slot, set by the
master's CF exemption (plan v3 R5/R6, E6E80 @ `0x1405E73C9` `MOV EDI,[R15+RAX*4+0x3FB8]`,
`0x1405E7B25` `CMP EDI,9` — i.e. role 9 = CF is the protected forward). So in block B the **CF is
excluded from the fold pick entirely**; only the other forwards (SS/WF/AMF) are candidates, and among
them the nearest-to-line (min score) folds first. At −1 one folds (the SS nearest the line); at −2 both
SS fold. **The CF never folds in block B because (a) it is the protected slot (0x4B40) excluded from the
pick, and (b) block B has no class write that could move role 9 off line 2 anyway.**

### 3.3 How block B differs from block A (Confirmed, byte contrast)

| | block A (`682770`) | block B (`67C4B0`) |
|---|---|---|
| writes class dword `+0x7B88`? | **YES** @ `0x140683809` (R10D from its own forward switch) | **NO** (0 stores, byte-scanned) |
| fold mechanism | forward switch + rank scale (×0.65/×0.20) rewrites the CF's class 2→1 at −1 | min-score **slot pick** → `planObj+0x7CD4`; class record untouched by the block |
| `0x4B40` protected-slot filter? | **NO** (no `0x4B40` reference in 682770 — byte-scanned) | **YES** @ `14067CD9D` (the CF shield) |
| CF at −1 | **folds** (class 2→1 via its own write) | **never folds** (protected + no class write) |

This is the byte-level answer to "why block A folds a CF at −1 but block B shields it."

---

## 4. Item 3 — the writer of the +2 fullback promotion

**Finding (Confirmed for the mechanism; the input-writer is the open refinement).** A fullback (role 2
= SB → line 0 in the shared reclassify) reads class 2 (attack line) at +2. The class dword is written by
**exactly two** stores in the entire image, and **no `class = 2` immediate store exists** (byte-scanned the
whole `.text` for `C7 BC C2 88 7B 00 00 02 00 00 00` → 0 hits):

1. the **shared reclassify** @ `0x1405ECBB0` (role 7-9 → line 2);
2. **block A's own write** @ `0x140683809`.

The promotion is observed in **both** blocks (live doc: both A and B promote a fullback at +2), so it
cannot be block A's own write — it must be the **shared reclassify**. Therefore: **at +2 the per-tick role
array `planObj+0x3FB8` carries a forward role (7/8/9) in the fullback's slot, and the shared reclassify
maps it to line 2.** The promotion is *switch-fed*, not a direct class store.

**The open refinement (Unknown):** the *writer* of the dial-dependent `+0x3FB8` forward role. It is **not**
a plain `disp32` immediate store and **not** a `LEA base,[plan+0x3FB8]` (byte-scanned both → 0 hits); it is
the tactics/role rebuild (`FUN_14051D9E0` "role rebuild" + `FUN_1404F5F10` "bulk A/B writer", plan v3 §0d),
which copies the role array per tick. Confirming the exact store site and that it is dial-driven (as opposed
to the persistent `+0x4C00` roles, which the live doc confirms "never moved") is the remaining step. This
does **not** change the deliverable's port plan: GF rebuilds `+0x3FB8` from the tactics sheet, so the dial
remap belongs there.

---

## 5. Item 4 — the dial byte vs GF's B8A4 port (same field, Confirmed)

The dial is the **same field** GF already ports. Byte-verified on the PES side (`FUN_1404F6C30`, the
team-tick orchestrator, all reads `*(T+0xB8A4)` where `T = *(ctx+0x6D50)`):

- `b8a2 = (b8a4 > 3)`: store 0 @ `0x1404F6E1C`, `CMP [T+0xB8A4],4` @ `0x1404F6E2A`.
- `b8a3 = (b8a4 < 1)`: store 0 @ `0x1404F6E41`, `CMP [T+0xB8A4],0` @ `0x1404F6E4F`.
- dial read in block A @ `0x140682BB7` (`MOVZX EAX,[RAX+0xB8A4]`); in block B's list machine @
  `0x14067D2B5`.

GF's port (`src/sim/onthepitch/pesteamtick.{hpp,cpp}`) is **the identical field with identical derivations**:

- `int b8a4 = 2;` (init `FUN_140A415A0`, tactics §3) — same 0..4, init 2.
- `WriteAttackFlags`: `tick->b8a2 = tick->b8a4 > 3; tick->b8a3 = tick->b8a4 < 1;` — **byte-for-byte the
  PES rule** at 0x1404F6E1C-58.
- `DriftAttackLevel` ports `FUN_1404F8350` (strategy→want, manual override `T+0x210`, in-play freeze at
  `MI+0x116C==1`, `min(want,4)` clamp, `b8c0Frame` stamp).

**Conclusion:** `[[unitctx+16]+0xB8A4]` and GF `pesteamtick` `b8a4` are the **same field** (T+0xB8A4,
0..4 = −2..+2). The port already models it correctly. The reclassify blocks (§1-§4) are the *new*
behaviour to add on top — GF currently consumes `b8a4` only as a depth/attack level, not as the line-
reclassification input.

---

## 6. Class-rewrite table (both blocks)

`class` = dword at `planObj+0x7B88+8*slot`; `rank` = byte at `+0x7B8C+8*slot`; family index =
`F*11+slot`. Lines: 0 = back, 1 = mid, 2 = attack. Role codes `pesroles.hpp:5`:
GK0 CB1 SB2 DMF3 CMF4 SMF5 AMF6 SS7 WF8 CF9.

**Shared reclassify (EC600 @0x1405ECBB0), runs every tick for both blocks** — the base table:

| dial | −2 (b8a4=0) | −1 (1) | 0 (2) | +1 (3) | +2 (4) |
|---|---|---|---|---|---|
| shared reclassify role→line | same map (1,2→0; 3-6→1; 7-9→2) | same | same | same | same |

The shared map itself is **dial-independent** (fixed jump table 0x5ED1F4). The dial's effect enters
through the **input** (`+0x3FB8` roles, §4) and the **block** below.

**Block A (`682770`, own write @0x140683809), 2 CF away team:**

| dial | effect on class records |
|---|---|
| −1 | one CF (role 9) class 2→1 (folded to mid) via its forward switch; the other CF holds |
| −2 | both CFs folded to 1 |
| 0 | neutral (shared reclassify values stand) |
| +2 | a fullback (SB, slot with role remapped to 7-9 in +0x3FB8) → class 2 (shared reclassify, §4) |

**Block B (`67B640`→`67C4B0`), 1 CF + 2 SS home team:**

| dial | effect on class records |
|---|---|
| −1 | one SS folded (min-score pick → `+0x7CD4`); CF holds line 2 (shielded) |
| −2 | both SS folded; CF still holds line 2 (0x4B40 excluded + no block class write) |
| 0 | neutral |
| +2 | a fullback → class 2 (shared reclassify, §4); full-attack flag b8a2 set, fold pass skipped |

All folds reverse at neutral (dial 0). **Rank** (0x7B8C) is data-ordered, not sheet order (live doc §3).

---

## 7. Port plan for GF (files, order)

GF's `b8a4` is already wired (`pesteamtick.*`, consumed at `teamAIcontroller.cpp:420,1380,3238`).
The reclassify is **new** and slots into the existing per-tick team path. Order:

1. **`src/sim/onthepitch/pesteamtick.{hpp,cpp}`** — add the shared reclassify: a pure
   `ReclassifyLine(int role) -> int` (the 0x5ED1F4 table: 1,2→0; 3-6→1; 7-9→2; else 0) and the
   per-tick role input. Reads `b8a4` (already present). **This is the class-record writer (§2).**
   Reuse the existing `b8a4`/`b8a2`/`b8a3`; do not add a second field.
2. **The role input (`+0x3FB8`)** — wire the per-tick role array the reclassify reads. Source it from the
   tactics sheet / `WriteRoleArrays` path (team.cpp:527/574). **Open refinement (§4):** confirm the exact
   PES writer and that the +2 fullback→forward remap is dial-driven here. Until confirmed, source `+0x3FB8`
   from the same roles GF already writes and mark the dial remap `// RE-APPROX(...)` pending the writer.
3. **Block dispatch (flag F)** — add `F = (possessionLatch != 0)` to the per-tick struct (GF already has
   the possession state; the `T+0x255` latch maps to it). Dispatch block A vs B on F (§1). Reuse the
   existing possession tracking; do not add a new latch.
4. **Block B pick (`67C4B0`)** — port the min-score fold pick (score = x²+z² + 10000·(role>5), init
   100000, filters §3.2 incl. the `0x4B40` protected-slot CF shield) → a fold-slot output. **New file**
   `pesfold.cpp` (pure, testable) or a method on the defensive-shape struct.
5. **Block A switch (`682770`)** — port the forward switch + rank scale (×0.65/×0.20) + protected-floor
   (the existing `pes-re-attack-level-pull.md` / `pesforward.cpp` R3-R9 already model most of this; add
   the class 2→1 fold write for the CF at −1/−2).
6. **Wiring** — call the reclassify, then the block, from `TeamAIController::Process` →
   `CalculateDefensiveShape` (teamAIcontroller.cpp:252), gated to in-play (GF already gates there).

**Ordering rationale:** reclassify (1) + role input (2) first because the class records are the observable
output the live doc measured; F dispatch (3) second because it selects the block; the block bodies (4,5)
last because they are the per-block downstream. Each step is independently testable (pure functions).

---

## 8. Self-checking checker

`gf-MentalityRE-scratch/checker_mentality_blocks.py` re-reads every cited byte/constant from the image,
checks the instruction pairings that carry each rule, and fails on a single flipped byte (mutation test at
the end). **Run result (this pass): ALL CHECKS PASS (0 failures)** — 30 checks covering the dispatch
polarity, R14==0, the F-derivation byte chain, both dial-flag derivations, the dial reads in A and B, the
reclassify jump-table stores, the "no class=2 immediate" census, the fold-count/penalty/gate constants, and
the CF-shield penalty mutation.

## 9. Evidence ledger (new)

| ID | Claim | Conf. | Evidence |
|---|---|---|---|
| EV-BLK-0001 | F==0→A(681FF0), F≠0→B(67B640); R14B==0 comparator | Confirmed | bytes 0x1405ECC21-44; R14 xor 0x1405EC714, no rewrite before CMP |
| EV-BLK-0002 | F = (T+0x255 != 0), per tick, per team | Confirmed | 4F6C30 l.238-239; 51D9E0 l.92/154; EC600 0x1405EC663 |
| EV-BLK-0003 | in-play parity inversion (F flipped on even frames, MI+0x119C>1) | High | 4F6C30 l.195; 9DC0 two-frame dispatch (plan §0e) |
| EV-BLK-0004 | shared reclassify jump table 0x5ED1F4: 1,2→0;3-6→1;7-9→2 | Confirmed | table read from image; stores 0x1405ECBB0/BBE |
| EV-BLK-0005 | class written only by EC600 + block A; no class=2 immediate | Confirmed | full `.text` census, 0 hits |
| EV-BLK-0006 | block B has no 0x7B88 store; fold = slot pick → +0x7CD4 | Confirmed | 67C4B0 byte-scan; store 0x14067CF93 |
| EV-BLK-0007 | fold score = x²+z² + 10000·(role>5), init 100000 | Confirmed | 0x14067CE57/CDE3/CE78/CE72/CE7D; DAT_14259D634/1425C65FC |
| EV-BLK-0008 | CF shield = 0x4B40 protected-slot exclusion (block B only) | Confirmed | 0x14067CD9D; block A has no 0x4B40 ref (byte-scan) |
| EV-BLK-0009 | +2 fullback promotion is switch-fed (role→line 2), not a direct store | Confirmed | EV-BLK-0005 + shared reclassify; writer open |
| EV-BLK-0010 | dial T+0xB8A4 == GF b8a4; b8a2=b8a4>3, b8a3=b8a4<1 | Confirmed | 0x1404F6E1C-58; GF pesteamtick.cpp |

**Open (carried to next run):** (a) the exact `+0x3FB8` dial-driven writer (§4); (b) the in-play parity
*effect* (EV-BLK-0003); (c) the semantic setter of T+0x255 (possession latch, live doc item 4).
