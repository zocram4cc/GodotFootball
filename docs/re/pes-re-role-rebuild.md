# PES role rebuild: the dial-driven writer of `planObj+0x3FB8` (23-10-26)

> **Main audit 22-10-26 (bytes):** `FUN_140A42590` = `movsxd rax, edx; mov dword [rcx+rax*4+0x3FB8], r8d; ret`; the role rebuild calls the bulk writer at `0x14051DA5A` and `0x14051E2E7` (`call 0x1404F5F10`): match.

> **Source and status.** Read-only RE against `pes_full.bin` (file offset = VA - 0x140000000),
> decompile + disassembly via the Ghidra bridge, constants script-read from the image. No GF code
> edited. Every cited instruction is re-read byte-for-byte by the self-checking checker
> (`<workdrive>/gf-RoleWriterRE-scratch/checker_role_writer.py`, all checks pass).
> Confidence labels: **CONFIRMED** = byte-verified in this doc; **INF** = inferred from a verified
> fact, not directly demonstrated; **OPEN** = not yet determined.
>
> Resolves the open item `docs/re/pes-re-mentality-reclass-live.md` §6.3 ("the writer of the +2
> fullback promotion") and §6.5 (the discriminator log). Closes the §4 refinement in
> `docs/re/pes-re-mentality-blocks.md` (confirm the exact PES writer of `+0x3FB8`).
> Repo copy: `docs/re/pes-re-role-rebuild.md`.

## 1. The writer — CONFIRMED

The per-tick role array `planObj+0x3FB8` (11 slots, dword each) is written by a **computed-base
store**, which is why a plain disp32/LEA scan for `+0x3FB8` found 0 hits:

```
FUN_140A42590 :  48 63 c2            MOV  EAX, ESDX        (slot, sign-extended)
                 44 89 84 81 B8 3F   MOV  [RCX+0x3FB8+RDX*4], R8D   <- THE WRITER
                 00 00
                 C3                  RET
```

- `RCX` = `planObj`, `RDX` = slot (0..10), `R8D` = role.
- Companion array `+0x3F8C` is written with the **same** value by the sibling one-liner
  `FUN_140A425A0` (`MOV [RCX+0x3F8C+RDX*4], R8D; RET`). Both are called per slot with the identical
  role argument.

Both stores are called once per slot from the bulk per-slot writer **`FUN_1404F5F10`**, which is
called **twice per team tick** from the role rebuild **`FUN_14051D9E0`** (call sites at
`0x14051da5a` and `0x14051e2e7`, both `E8`-verified).

## 2. The per-tick rule — CONFIRMED (byte-verified chain)

```
role[slot] = JT[ matrix[10 + (family*11 + slot)*3 + level] ]
    family = matrix[0]                       (dword at T+0x8E08)
    level  = 2 - (F != 0)                     F = possession flag
    JT     @ 0x1408C52A4 (13 rel32 entries, image base 0x140000000)
```

**`FUN_1404F5F10`** (bulk per-slot writer), arguments `(planObj, matrix=T+0x8E08, globalRec, F)`:
- `level`: `NEG R9B; SBB EBP,EBP; ADD EBP,2`  ⇒  `level = 2` when `F=0` (out of possession), `1`
  when `F=1` (in possession).  Override: `CMP[MI+0x116c],1; JZ; CMP[MI+0x1554],1; CMOVZ EBP,0`  ⇒
  `level = 0` when `MI+0x116c != 1 && MI+0x1554 == 1` (a preplay/transition special case).
- Slot loop `ESI` = 0..10, per-slot buffer stride `0xC` (`ADD RDI,0xC`).
- Per slot: `role = FUN_1408C51F0(matrix, slot, level, 3)`; then
  `FUN_140A425A0(planObj, slot, role)` → `+0x3F8C`, and `FUN_140A42590(planObj, slot, role)` →
  `+0x3FB8` (THE STORE).

**`FUN_1408C51F0`** (role query), arguments `(matrix, slot, level, 3)`:
- `family = matrix[0]`  (`MOV R9D,[RCX]` @ `0x1408c5200`).
- `key = matrix[10 + (family*11 + slot)*3 + level]`  (index bytes @ `0x1408c5214`; key load
  `MOV EAX,[R11+RCX*4+0x28]`, the `+10` folded into the `0x28` base).
- `key > 12` (`CMP EAX,0xC; JA`) → default role 0.
- `JT[key]` (`MOV ECX,[RDX+RAX*4+0x8C52A4]; JMP RCX` @ `0x1408c5248`) dispatches to
  `MOV R10D, <role>; RET`.

**Jump table `@0x1408C52A4` — key → role (13 entries, script-read):**

| key | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 |
|-----|---|---|---|---|---|---|---|---|---|---|----|----|----|
| role| 0 | 1 | 2 | 2 | 3 | 4 | 5 | 5 | 6 | 8 | 8 | 7 | 9 |
| name| GK| CB| SB| SB| DMF| CMF| SMF| SMF| AMF| WF| WF| SS| CF |

Role names 0..9 = GK, CB, SB, DMF, CMF, SMF, AMF, SS, WF, CF. These role codes are the **input**
to the shared line reclassify (`0x1405ECBB0`, JT `0x5ED1F4`: roles 1-2 → line 0, 3-6 → line 1,
7-9 → line 2).

## 3. How the dial enters — CONFIRMED to the matrix boundary, INF/OPEN beyond

- The dial is the byte **`T+0xB8A4`** (0..4 = -2..+2), written by the dial setter
  **`FUN_1404F8350`** — store `MOV [RCX+0xB8A4],EAX` confirmed @ `0x1404f86e7`.
- **The per-tick writer does NOT read the dial.** `FUN_1404F5F10` contains no read of `0xB8A4`
  (scanned, 0 hits, checker-verified). It reads only `family` and the per-`(family,slot,level)`
  key from the **tactics matrix `T+0x8E08`**.
- **INF:** the dial's effect on `+0x3FB8` is carried *through the matrix* — i.e. when the dial
  moves (setter `4f8350`, or the auto-adjuster `51e970`), some routine rewrites the matrix
  (`family` byte + key cells) so the next per-tick rebuild picks up the new roles. The matrix is
  the only remaining dial→role path (the writer itself has no dial read). The +2 fullback→forward
  promotion therefore shows up as the fullback slot's **key** moving to 9/10/11/12 (WF/SS/CF →
  line 2) at dial +2.
- **OPEN:** the **matrix builder** — the routine that writes `T+0x8E08` from formation + dial.
  Not decoded in this pass. It is the single missing link for (a) the exact dial→key mapping and
  (b) *which slot is promoted, per formation*.

### Matrix layout (INF from the verified index formula + 99-dword span)
- `T+0x8E08`  `matrix[0]` = `family` (dword). Range `0..2` inferred from the 99-dword key span
  (3 families × 11 slots × 3 levels); the 99 value itself is CONFIRMED from the index math, the
  3-way split is INF.
- `T+0x8E0C .. T+0x8E27`  `matrix[1..9]` = 9 dwords, purpose **OPEN** (per-family metadata).
- `T+0x8E28 .. T+0x8EBB`  `matrix[10..108]` = 99 key dwords =
  `key[family][slot][level]`, `key = matrix[10 + (family*11+slot)*3+level]`.

## 4. Call graph (VAs)

```
team tick orchestrator (FUN_1404F6C30)
  └─ FUN_14051D9E0  (role rebuild, per team, twice/tick path)
       ├─@0x14051da5a─ FUN_1404F5F10 (planObj=[RCX+0xba8], matrix=R13+0x8E08, F=R8B)   [gated MI+0x1775==0]
       └─@0x14051e2e7─ FUN_1404F5F10 (matrix=R13+0x8E08, F=(T+0x255!=0) via SETNZ)    [gated MI+0x1775==0]
            └─ per slot 0..10:
                 ├─ FUN_1408C51F0  (role = JT[matrix[10+(fam*11+slot)*3+level]])
                 ├─ FUN_140A425A0  → planObj+0x3F8C+slot*4  (companion)
                 └─ FUN_140A42590  → planObj+0x3FB8+slot*4  (THE writer)
```

Downstream: `+0x3FB8` role → line reclassify `0x1405ECBB0` → class record `planObj+0x7B88+8*slot`.
(`+0x4C00` = persistent sheet role, never moves; distinct from this per-tick array.)

## 5. The live read that would settle the OPEN (matrix builder / which slot promoted)

The owner already holds both `T` (team record, = `*(unitctx+16)`) and `planObj` per think pass.
Add, per team per tick, alongside the existing log:

| field | address | size | note |
|-------|---------|------|------|
| dial   | `T+0xB8A4`     | 1 B  | 0..4 |
| latch  | `T+0x255`      | 1 B  | possession (F) |
| family | `T+0x8E08`     | 4 B  | matrix[0] |
| keys   | `T+0x8E28 + ((family*11+slot)*3+level)*4` | 4 B × 33 | slot 0..10, level 0..2 (level = `2-(latch!=0)`) |
| role   | `planObj+0x3FB8 + slot*4` | 4 B × 11 | the per-tick array (the writer's output) |
| class  | `planObj+0x7B88 + slot*8` | 4 B × 11 | reclassify output |

Sweep the dial 0..4 (coach-vs-coach) and log the table above each press. **Settles the OPEN when:**
(a) the fullback slot's key moves 2/3 (SB) → 9/10/11/12 (WF/SS/CF) exactly as the dial crosses +2,
and (b) the `family` byte / other key cells change in a dial-correlated way. The slot whose key
makes that jump is the promoted slot for that formation. Cross-check each tick against the rule
`role[slot] == JT[keys[slot][2-(latch!=0)]]`; any mismatch localizes the matrix-builder call.

## 6. Checker

`<workdrive>/gf-RoleWriterRE-scratch/checker_role_writer.py` — re-reads every cited instruction
byte-for-byte and script-reads the 13-entry jump table; fails (exit 1) on any single flipped byte.
Last run: **ALL CHECKS PASSED** (34 assertions + 13 JT entries).
