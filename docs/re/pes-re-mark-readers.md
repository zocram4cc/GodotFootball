# PES mark-table readers (T7a `FUN_1404F61E0` slice): the three duel readers' payloads + the ramp base's source (MarkReaderRE, 23-10-26)

Read-only RE. Image `pes_full.bin`, file offset = VA − 0x140000000. Bridge `http://127.0.0.1:8080`.
Scratch: `<workdrive>/gf-MarkReaderRE-scratch/` (`r1/r2/r3.dec`, `r2b.asm`, `r3.asm`, `t7a.dec`, `a72fb0.dec`, `c1/c2.dec`, `check_markreaders.py`).
Prior: `pes-re-mark-table.md` (pair writer, T7a rule, 7.0 correction), `pes-re-mark-cone.md` (B7D8 word, cone, clear gate), `pes-re-mark-gate.md` (count gate with the 12.0 correction, head/action meanings, ramp OPEN, reader→GF INF map).

## 0. Constant table (script-read from the image, never typed)

```
0x14259BE5C f32= 90.0 bits=0x42B40000   (R1/R3 head-gate cone)
0x14259BE04 f32= 12.0 bits=0x41400000   (R1 rate arg; T7a/R2 cone outer)
0x14259BE44 f32= 64.0 bits=0x42800000   (folded v into the count gate)
0x14259BE20 f32= 25.0 bits=0x41C80000   (R1 entry d2 floor; R2 25.0 fallthrough d2)
0x14259BE10 f32= 16.0 bits=0x41800000   (R1 speed gate)
0x14259BE1C f32= 20.0 bits=0x41A00000   (R1 facing/pair floor)
0x14259BE30 f32= 45.0 bits=0x42340000   (R3 arm-1 pass band)
0x14259BE34 f32= 50.0 bits=0x42480000   (R2 age-gate alt band)
0x14259BE4C f32= 70.0 bits=0x428C0000   (R2 facing-vs-holder band)
0x14259A760 f32= 35.0 bits=0x420C0000   (R2 gate-A band)
0x1425C48DC f32=324.0 bits=0x43A20000   (R2 2-vs-4 d2 cut)
0x1425C6780 f32= 81.0 bits=0x42A20000   (R1/R2 holder-d2 refuse)
0x1425A9464 f32= 36.0 bits=0x42100000   (R1/R2 holder-d2 lenient band)
0x1425A9BF0 f32=130.0 bits=0x43020000   (R2 pair-head band)
0x14259D628 f32=110.0 bits=0x42DC0000   (R3 arm-2 pass band)
0x1425C74CC f32=484.0 bits=0x43F20000   (R1/R3 22 m d2)
0x1425C4C18 f32=196.0 bits=0x43440000   (R3 14 m d2 floor)
0x1425C48C0 f32= 22.0 bits=0x41B00000   (R3 bearing-vs-holder band)
0x14259D660 f32= -6.5 bits=0xC0D00000   (R3 4C9A0 rate arg)
0x142598578 f32= 0.2 bits=0x3E4CCCCD    (R3/R1 rate per frame)
0x142590D98 f32= 0.3 bits=0x3E99999A    (R3 rate per frame)
0x14259BDDC f32=  1.5 bits=0x3FC00000    (R1/R3 rate: ball-height limit / fps quotient)
0x142565600 f32=  0.5 bits=0x3F000000    (R1/R3 bias; cone half-scale)
0x14259BDEC f32=  6.0 bits=0x40C00000    (R1/R2 4C8E60 band; T7a/R2 cone radius arg)
0x142596E6C f32= 80.0 bits=0x42A00000    (R1 cand-facing band; T7a wrap 2+3)
0x14259BDF0 f32=  7.0 bits=0x40E00000    (ramp base; NOT 8.0)
0x14258C264 f32=  3.0 bits=0x40400000    (ramp top add)
0x14259BE0C f32= 15.0 bits=0x41700000    (cone offset)
0x142565604 f32=  2.0 bits=0x40000000    (count-gate width; 57800 z-sign scale)
0x1425C48E4 f32=1369.0 bits=0x44AB2000   (37^2 hard-exit ceiling)
0x142565D30 bits=0xFFFFFFFF (abs-mask low word, NOT a value)
```

Checker asserts every row above plus the byte shapes below. 48 checks green; mutations 5/5 killed (`be5c91/cap4/mod10/b7ed/commit0`).

## 1. Reader R1 — `FUN_1409696A0`: what it decides, set vs clear

Presence site (bytes CONFIRMED): `0x140969970: 41 80 BC 01 EC B7 00 00 00` = `CMP byte [R9+RAX+0xB7EC],0`, R9 = own-side team record (`48A30`), RAX = slot 0..10. `0x140969979 JNZ 0x140969D81` (out of the function region: restore + RET), loop `INC RAX; CMP 11; JL`.

- **Mark bit SET (any of the 11 set) ⇒ early return, NO verdict written.** The mark EXEMPTS this man from this duel path. Status CONFIRMED (branch target is the function epilogue).
- **Mark bit CLEAR (all 11 zero) ⇒ fall through to the payload (§1.1).**

Entry gates before the scan (decompile CONFIRMED, `r1.dec` lines 57–110): `param_1[7] < 0x16`; `6E340(class of +1 byte) == 0`; `d2(me, anchorRec) >= 25.0@[BE20]`; holder `627E0|2483C < 0x16`; stamp age `(MI+0x1198 − [param_1+0xC4]) * 1.5 >= u` frames; 439E50-vs-facing wrap within 25.0@[BE20]-family band (the `local_res10 − facing` fold at lines 97–110).

### 1.1 Payload when clear (CONFIRMED decompile + asm; operand VAs pair-checked)

1. Role-word scan: if any `T+b748 == 0x24` (PRESS-role word), take that slot's record, `439E50` bearing, verdict `4ECF0(ctx, slot, bearing)` → `+0x8D4`, stamp `[param_1+0xC4]`, RET.
2. Else the 11-scan above; marked ⇒ return (no verdict).
3. Speed gate: `vel = 6C6D0(pl, BDD0/fps)` (floored velocity, `F4`); `fps = 1E532D0()`; `v = fps*60*60/1000 * |vel|`; require `v < 16.0@[BE10]` AND `param_5[0] == 3`.
4. Facing gate: `P0 = 439FE0(MA,row=slot,col=param_1[7]) = P[slot][owner]`; `c = 481B00(&facing+0x554, P0)` = wrap(P0 − facing); require `c < 20.0@[BE1C]`.
5. 11-walk over the OTHER side's slots `cand = (side^1)*11 + i`: skip unless on-pitch (`MI+0x1748`), `cand != holder`, `cand != param_1[7]`.
   - `P1 = P[slot][cand]`; require `481B00(&facing, P1) <= 80.0@[96E6C]`.
   - `P2 = P[slot][param_1[7]]`; require `20.0@[BE1C] <= |43A0A0(P2-line …)|` — floor: the candidate's pair-bearing must differ ≥ 20° from the owner's (asm `0x140969B42..B5C`, `JA`-skip CONFIRMED). Note the row: **R1 reads `P[slot][·]` (the man's own row), not `P[cand][·]`** — 439FE0 disasm CONFIRMED (`LEA R9,[RCX+0x228D4]; row = R9 + A*0x58; val = [row + B*4 + 0x16A8]`, so `(MA, A, B) = P[A][B]`; fallback `P[0][B]` when `A >= 0x16`).
   - Ball-side test via `57800` z-sign arms (decompile lines 189–243); `43B8B0(row, slot, cand) < 6.0@[BDEC]` AND `43B8B0(row, holder, cand) < 10.0@[88210]` (squared arrival distances from the S1 rows).
   - `481B00(&pl+0x554, ·) < 80.0` twice with a `71520` between; then the other-side 11-walk with `71540` + `4E720(ctx, cand, bearing, 35.0@[A760], 4.0@[656C0], 0,1,0)`: break on the first cand that FAILS the cone (i.e. keep searching while inside).
6. Tail (`LAB_140969CE4`): `cand > 0x15 ⇒ return`; `pos = 76240(48A10(cand))`; `out = 439E50(plPos, pos)`; verdict `4ECF0(ctx, slot, out)` → `+0x8D4`; `[param_1+0xC4] = stamp`; RET.

Verdict word: `+0x8D4` = `4ECF0(...)` computed, or the early role-arm's `4ECF0`. Literals written nowhere else on this path.

### 1.2 GF counterpart

The engaging-defender reaction walk in `elizacontroller.cpp::_ReactionCommands` (ordered `DefenderReactionInput` → `ChooseReaction`, `pesdefenderdecisions.hpp`): `in.tackle = _TackleCandidate(k)` is the port of this family (standing-tackle entry 1, `FUN_14096FDA0`-line geometry: ball gate, height, from-behind, engage window, safety veto, no-double-tackle). R1's mark arm is a NEW predicate input: `markTable[slotOf(me)] != 0 ⇒ tackle = false` (exemption, not an OR-leg). GF state that carries each input today: positions/velocities/facings (`PlayerFacts`/player records), holder (`_Carrier()`), S1 rows (`nearestRows`/`RowD2` for 43B8B0), floored velocity (`GetMovement()` — no state-5 floor, `pesmover`), cone (`ConeOpp` for 4E720). None: `markTable` itself (P0 of the mark-table port), the `+0x8D4` verdict word (GF writes commands, not verdicts), `param_1+0xC4/+0x68/+0x6C` stamps, `T+b748` role words, `MI+0x1198/0x1748` bytes. Status: rule CONFIRMED; GF mapping CONFIRMED to `_TackleCandidate` + `ChooseReaction` order.

## 2. Reader R2 — `FUN_1409931C0`: what it decides, set vs clear

Presence site (bytes CONFIRMED): `0x140993250: 80 BC 08 EC B7 00 00 00` = `CMP byte [RAX+RCX+0xB7EC],0`, RAX = own-side team record. Hit (`JNZ 0x140993265`) sets frame flag `[RSP+0xF0] = 1` (`0x140993265: C6 84 24 F0 00 00 00 01`); loop `INC RCX; CMP 11; JL`.

- **Mark bit SET ⇒ `bVar6 = true`.** Consumed at `0x14099369C CMP [RSP+0xF0],0` inside the big stamp gate (line 198–212): `((!bVar5) || (72E70(F, pl, dir), bVar6) || (...)) || d2 <= 25.0` — marked is the TRUE-leg of the middle OR: **marked lets the man take the cheap `2/4` verdict instead of the `4ECF0` call**, all else equal. It is an OR-leg, not a gate and not an exemption. Status CONFIRMED.
- **Mark bit CLEAR (`bVar6 = false`) ⇒ that OR-leg is dead; the gate needs `!bVar5`, the `0x800` arm, or the 25.0 fallthrough.** Status CONFIRMED.

Payload when the earlier gates pass (decompile CONFIRMED, `r2.dec`):

1. `fVar20 = bearing(param_1+0x150/158 → pl)` (coincident 0.0); `fVar19 = bearing(MA+0x2482C/24834 holder point → pl)` (coincident 0.0).
2. If `[param_1+0x6E4] != 0`: fold facing `+0x554` and `fVar19` to [0,360), `|diff| < 70.0@[BE4C]` else `+0x8D4 = 3`, RET.
3. If `98EDA0(param_2, param_3)` ⇒ RET (no verdict).
4. Gate A: `|wrap(fVar20 − [T+0x250])| <= 35.0@[A760]` else two `43A0A0` sign-agreement calls decide return vs continue (asm `0x14099345F..51C`, decompile lines 124–166).
5. `cone = 72FB0(F, 0xB, pl.x, pl.z, 6.0@[BDEC])`; require `cone > 0.0`.
6. Require `643A30(ctx, side, slot)` (carrier-free-to-cross, run-targets-b1 §23) AND `646B30(ctx, slot)`.
7. Holder `627E0|2483C` (`bVar5 = true` default): if `> 0x15`, `d2 = 4C8E60(MA18, holder)`; `> 81.0@[C6780]` ⇒ RET; `> 36.0@[A9464]` ⇒ `bVar5 = false`.
8. Pair read: `XMM6 = P[side·22+slot …]` via `IMUL RCX,RAX,0x16; ADD RCX,R12; MOVSS [RDX+RCX*4+0x23F7C]` (asm `0x140993613..61D` CONFIRMED — direct pair-table word); `head = 43A0A0(facing+0x554, XMM6)`; require `<= 130.0@[A9BF0]`.
9. Stamp gate (line 198): `iVar4 = MI+0x1198`; pass iff `(!bVar5 || (72E70 flag, bVar6) || ((flag&0x800)==0 || age-gate || |43A0A0| >= 50.0@[BE34])) || d2(pl, param_1pt) <= 25.0@[BE20]`.
10. THEN: `d2 = 4C8E60(MA18, slot)`; `+0x8D4 = (d2 > 324.0@[C48DC]) ? 4 : 2`; ELSE `+0x8D4 = 4ECF0(ctx, slot, fVar20)`. `[param_1+0x6CC] = stamp`.

### 2.1 GF counterpart

Same reaction walk (`_ReactionCommands`): this is the **block/jostle-for-pass entries' duel core** — the `2/4`-vs-`4ECF0` choice is the cheap-challenge vs computed-challenge verdict, and the `643A30` + `646B30` + `72FB0`-cone + `72E70`-zone shape matches the entries GF ports as `_BlockCandidate` / `_PassDuelCandidate` (entry 8 / entry 11, `pesdefenderdecisions.hpp`). The mark flag is a NEW OR-leg input to that predicate: `marked ⇒ take the cheap verdict`. GF carries: positions/facings, holder (`_Carrier()`), `L643a30` (ported in `pesoffball.cpp:1148`), zone test via FieldInfo chain (port pattern exists), cone via `ConeOpp`/`A53da0`, S1 rows for `RowD2`/`FirstOpponent`. None: `markTable`, `+0x8D4`, `646B30`, `98EDA0`, `4C8E60/43B8B0`-row distances as PES tables (GF uses live `Dist2`), stamps/flags. Status: rule CONFIRMED; GF mapping CONFIRMED to the block/jostle predicate family (`_BlockCandidate`/`_PassDuelCandidate` + `ChooseReaction` order).

## 3. Reader R3 — `FUN_1409AD830`: what it decides, set vs clear

Presence site (bytes CONFIRMED): `0x1409ADB40: 41 80 BC 1E EC B7 00 00 00` = `CMP byte [R14+RBX+0xB7EC],0`, R14 = own-side team record, RBX = slot 0..10. Hit (`JNZ 0x1409ADB56`) sets `AL = 1` (`bVar14 = true`); loop `INC RBX; CMP 11; JL`.

- **Mark bit SET (`bVar14 = true`) ⇒ the holder-duel block is SKIPPED**: `if ((!bVar14 && action∈{1,2,3} && +0x5E5==0))` (decompile line 140–141, asm `0x1409ADBA6..BC5` CONFIRMED) — marked falls through to the `param_1+0x54`-dispatched verdict ladder (13 `+0x8D4` stores) instead of the `627E0/439FE0/4ECF0` holder path. Status CONFIRMED.
- **Mark bit CLEAR ⇒ the holder block runs when `action∈{1,2,3}` and `+0x5E5 == 0`.** Status CONFIRMED.

Payload (decompile CONFIRMED, `r3.dec`):

- Head: `48910`-class bit ⇒ `+0x8D4 ∈ {computed, 2}`; `+0x5F5 != 0` or `+0x54 == 0x19` ⇒ `4`; `+0x54 == 4` ⇒ return; `+0xE32` ⇒ return; `9AE390` ⇒ return; `+0x7C != 0` ⇒ return.
- Time/role pre-filters (lines 88–124): `+0x54 == 0x15` with `fps*2.2+0.5` frames; non-`MI+0x116C==1` with `fps*1.2+0.5`; `5D4C0`-object `+0xC8/+0xC9` with the `b6e8` 48-scan / `b748 == 0x30` return; `b748 == 10` with `fps*0.1+0.5` ⇒ `4`.
- The mark scan (line 126–133), then `d2(pl, MA+0x2482C/24834 holder point)` = `fVar18`.
- Holder block (clear only, lines 140–188): `holder = 627E0|2483C`; `P = 439FE0(MA18, holder, slot)`; if `+0x54 == 3`: `fVar18 <= 484.0@[C74CC]` ⇒ `4ECF0(ctx, slot, 439EF0(pl, param_1+0x3C))` else `4`; else age-gate (`MI+0x1198 − [param_1+100]` vs `fps*0.5+0.5`) + `|43A0A0| < 90.0@[BE5C]` + `45.0@[BE30] < |43A0A0| < 110.0@[D628]` ⇒ same `4ECF0` (via `76240`/`439EF0` bearings).
- Common tail: `439EF0(MA+0x2482C, pl)` + `43A0A0` vs `22.0@[C48C0]`; `196.0@[C4C18] <= fVar18` + `4C9A0(..., −6.5@[D660])` + `6C6D0` + `4C8DC0` ⇒ `4`; then the `+0x54` dispatch ladder: `2` (reach `760E0 < 9.1499…@[D5D4]`), `8` (refuse unless `4C8EA0(slot, +0x5F0) > 36.0@[A9464]`), `0x17` (`4ECF0` of `439EF0`), `1` (marked-gated `4` vs `5AA880`-gated `2`, `param_4 < 3` arm), else return; then `9AE1E0` tail call. All `+0x8D4` values CONFIRMED per arm.
- Caller: single CALL at `0x1409AA05F` from the post-dispatch merge `0x1409A9F83` (capstone walk CONFIRMED): the `+0x54`−1 jump table at `0x1409AA0A8` (26 arms, each `JMP 0x1409A9F83` or equivalent) means **R3 is the common duel core every action reaches**; the arms are pre-filters. Dispatch head `0x1409A89D4` gates on `T+b748`-family `0xa2a0 == 0x1F`, `4C720`, and the `G+0x2AA8+0x18` ramp shape (same bytes as T7a §4).

### 3.1 GF counterpart

Same reaction walk: R3 is the **holder-duel dispatch core** — GF's analogue is the `_ReactionCommands` dispatch itself (`in.receive/tackle/slide/block/jostleForPass/dodge…` → `ChooseReaction` → per-reaction command), and the `+0x54` action ladder maps to GF's `e_FunctionType` dispatch (`humanoid.cpp`/`playercontroller.cpp` current/desired function type), which is NOT 1:1 (PES `+0x54` values 1/2/3/8/0x15/0x17/0x19/4 vs GF's `e_FunctionType_*`). The mark bit is a NEW skip input on the holder-duel arm: `marked ⇒ skip the holder challenge, fall through to the action dispatch`. GF carries: holder (`_Carrier()`), d2 (`Dist2`), bearings (`BearingDeg`/`AngleBetweenDeg`), ball prediction (`_mentalImage`), verdict→command mapping exists. None: `markTable`, `+0x8D4`, `+0x54/+0x5E5/+0x5F5/+0x7C/+0xE32` record words, stamps, `T+b748/b6e8` words, `9AE390/5D4C0/4C8DC0/5AA880/5951B0/4C9A0/760E0/76240` helpers. Status: rule CONFIRMED; GF mapping CONFIRMED to the `_ReactionCommands`/`ChooseReaction` dispatch with `e_FunctionType` as the (non-1:1) ladder analogue.

## 4. The ramp base's source (`+0x2AA8` / `+0x18`): CLOSED as FieldInfo F5

Reader (bytes CONFIRMED): T7a `0x1404F633C MOV RCX,[R8+0x2AA8]`, `0x1404F6343 MOVSS XMM1,[RCX+0x18]`; R2 `0x140993560/690` same register shape; R3-dispatch head `0x1409A8B1B/826` same.

**The record is FieldInfo (the pitch-geometry record), and `+0x18` is F5 = the goal-area half-width, 9.16 m.** Evidence:

- `pes-re-fieldinfo.md` §1: the live FieldInfo block is `{refcount, f32 F[18]}`, floats at `block+0x04…+0x48`; index convention `F[k] = block+4+4k`. Consumers hold a pointer to the first float (`block+4`), so Ghidra `param[5]`-style indexing reads `F[k] = [ptr + 4k]`.
- `+0x18` from the FLOAT pointer = `block+0x04+0x18` = `block+0x1C` = `F[(0x1C−0x04)/4] = F[7]`? No — recompute: if the held pointer is `block+4`, then `[ptr+0x18]` = `block+0x1C` = byte offset `0x1C` in the block = `F[(0x1C−4)/4] = F[7]` = penalty-area half-width 20.12. BUT the sibling readers prove the float-pointer base differently: `FUN_14052EB6D` (mark-gate doc §4) uses `[c0+0x14]`/`[c0+0x18]`/`[c0+0x1C]` with `c0 = [DAT+0x2AA8]`, i.e. offsets from the OBJECT pointer, not the float pointer. `0x14/0x18/0x1C` from the object = `block+0x14/0x18/0x1C` = `F[(0x14−4)/4..(0x1C−4)/4]` = `F[4]/F[5]/F[6]` = goal-area line x / goal-area half-width / penalty-area line x.
- T7a/R2/R3 read `[obj+0x18]` = `block+0x18` = `F[(0x18−4)/4]` = **`F[5]` = goal-area half-width 9.15999985** (`0x41128F5C`, fieldinfo §1 table). The ramp is `base = F5 − 7.0`, `top = F5 + 3.0`, i.e. **2.16 m → 12.16 m**, the ladder on `sign·holderX` minus 15.0 cone offset (T7a decompile lines 64–77).
- Writers/value: FieldInfo is VM-built at match start (fieldinfo §0.3: NO authoritative on-disk constant — byte-exhaustive search over the exe finds none for F4/F5/F6; every field is a named constant with §4/§5 provenance). **No writer isolation is needed or possible in the image**: the value is read live at `F[5] = 9.16`. GF carrier: `pescarrier.hpp` already carries `pitchF5` (`F[5]`) in `BackSpaceInput/SafetyInput/TouchInput` and `pesjudge.hpp:176 goalAreaHalfWidth_m (F[5])`; `pesoffball.cpp` uses `pesGoalAreaHalfWidth`. So the input is NOT new — it is GF's existing F5 constant, loaded from the install's FieldInfo/record path, never transcribed.
- The `*(G+0x2AA8)`-is-null note (fieldinfo §2/§8 orchestrator note): null out-of-tick (same timing as `MI = *(G+0x270)`); the path is real in-tick (referee `pes-re-protection.md` §3.3 outputs matched live restarts). T7a/R2/R3 all run in-tick. No port impact.

**Status: CLOSED.** `rampBase = F[5]` (goal-area half-width, 9.16 m, GF `pitchF5` family). The mark-gate doc's "OPEN / needs rampBase X" is superseded: no new state, no new writer hunt — wire GF's existing F5.

## 5. Port spec for MarkTablePort2 (CONFIRMED pieces only)

- **P0 — state (no GF carrier, grep CONFIRMED empty):** per-team `markSlot` (`B7D8`: 0..10, `0xFF` empty) + `markTable[11]` bytes (`B7EC`). Writers: mark-cone doc §1 arms + §4 teardown (`0xFF` + zero). Clear together.
- **P1 — pair table (CLOSED):** `pair[22][22]` f32, S1 rebuild, do NOT port the wrap-back pass (`0x14045BD2D..0x14045BE35`).
- **P2 — mark build:** T7a rule with `rampBase = F[5]` (§4: `base = F5 − 7.0`, `top = F5 + 3.0`); cone `53DA0/53DF0`; count gate `4EAF0/4E7C0` (12.0 radius correction); side/head/action inputs per the gate doc.
- **R1 — tackle exemption:** `if (any markTable[0..10]) skip tackle-duel payload` in `_TackleCandidate` (new early-out; no other output changes). Wire when P0 lands.
- **R2 — block/jostle OR-leg:** `marked ⇒ take the cheap 2/4 verdict arm` in `_BlockCandidate`/`_PassDuelCandidate` (new OR-leg; thresholds `324.0/36.0/81.0/130.0/70.0/35.0/50.0/25.0` per §2).
- **R3 — holder-duel skip:** `if (marked && action∈{1,2,3} && rec+0x5E5==0) skip the holder block, fall through to the action dispatch` in `_ReactionCommands` (new skip; ladder values per §3).
- **Do NOT port:** the `+0x8D4` word itself (GF has no verdict word; commands are the output), the `+0x54` ladder values (non-1:1 with `e_FunctionType`), `param_4 < 3` (R3 call-site arg).

## 6. Status summary

- R1 payload: **CONFIRMED** (exemption + full gate chain + tail verdict; 439FE0/43A0A0/481B00/43B8B0 arg orders byte-verified). GF counterpart CONFIRMED (`_TackleCandidate`).
- R2 payload: **CONFIRMED** (flag OR-leg + 12-gate chain + 2/4-vs-4ECF0 choice; `643A30` contract from run-targets-b1). GF counterpart CONFIRMED (`_BlockCandidate`/`_PassDuelCandidate`).
- R3 payload: **CONFIRMED** (bool skip + holder block + `+0x54` ladder + common-core caller at `0x1409AA05F` via the `0x1409AA0A8` jump table). GF counterpart CONFIRMED (`_ReactionCommands` dispatch).
- Ramp base: **CLOSED** (`F[5]` = 9.16, GF `pitchF5`; supersedes the gate doc's OPEN).
- Verdict helper `4ECF0`, `627E0`, `6C6D0`, `4E720`, `4C8E60`, `43B8B0`, `57800`, `72E70`, `72FB0`, `643A30`, `646B30`, `98EDA0`, `9AE390`, `5D4C0`, `4C8DC0`, `5AA880`, `5951B0`, `4C9A0`, `760E0`, `76240`, `71520/71540`, `6E340`, `48910/48A10/48A30`, `4C720`, `439E50/439EF0`, `481B00`, `43A0A0`, `439FE0`: semantics as cited (ported or elsewhere-decoded); bit-exactness of `A92D40/2DC0` remains lane-core O3 (inherited, not re-proven here).

## 7. Open questions (no port impact)

1. `140A92D40/2DC0` bit-exactness (inherited O3).
2. `4E7C0` b80-slot aliasing (rule-neutral, gate doc §1.4).
3. `93CAE0` tick placement + `+0x5F4` hot-path writer (head-writer completion; port takes the bearing as input).
4. `+0xB5AC/+0xB5B0` split-displacement writers ({1,0x3F} provenance; port takes the action word as input).
5. `646B30`, `98EDA0`, `9AE390`, `5D4C0`, `4C8DC0`, `5AA880`, `5951B0`, `760E0`, `71520/71540` bodies (reader-internal; port cites their call contracts only).
