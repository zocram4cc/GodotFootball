# PES mark-table gate inputs (T7a `FUN_1404F61E0` slice): RE report (MarkGateRE, 23-10-26)

Title: the four open T7a inputs decoded to port spec. Count gate CONFIRMED (with one
operand-mapping correction vs the cone doc). Head vector CONFIRMED as the candidate's
stored facing word, writer INF (narrowed to two producer call sites). Action word writer
CONFIRMED (two call sites + init), value-set {1, 0x3F} provenance INF (one scan left).
Ramp base OPEN (struct shape CONFIRMED, allocating writer unidentified). Reader verdict
branches mapped to GF consumers as INF (near-miss, not port-blocking).

Read-only RE. Image `~/ghidra-projects/scratch/Unpacker/pes_full.bin`
(file offset = VA − 0x140000000). Bridge `http://127.0.0.1:8080`
(`/decompile_function?address=`, `/disassemble_function?address=`).
Scratch: `<workdrive>/gf-MarkGateRE-scratch/`
(`t7a.dec/asm`, `r1/r2/r3.dec/asm`, `w5ec.asm`, `w5ec2.asm`, `caller1/2.asm`,
`4C4E0.dec`, `check_markgate.py`).
Prior: `pes-re-mark-table.md` (pair writer, T7a rule, 7.0 correction),
`pes-re-mark-cone.md` (B7D8 word, cone 53DA0/53DF0, clear gate, reader TEST roles).
Slice note: MarkTablePort owns the exact port of everything else (B7D8, 4C720, cone,
ramp shape, wrap tests, pair reads, %11/cap-3/hard-exit); this report specs only the
four named inputs as caller-supplied facts.

## 0. Constant table (script-read from the image, never typed)

```
0x14259BE5C f32= 90.0 bits=0x42B40000   (coneDeg; T7a 0x1404F653C MOVSS XMM0)
0x14259BE04 f32= 12.0 bits=0x41400000   (RADIUS; T7a 0x1404F63BF MOVSS XMM15 / 0x1404F6558 stack slot)
0x14259BE44 f32= 64.0 bits=0x42800000   (folded v; T7a 0x1404F656C MOVSS XMM3)
0x142565600 f32= 0.5 bits=0x3F000000    (cone half-scale)
0x1425795E4 f32= 6.2831855 bits=0x40C90FDB (2pi)
0x14259BE40 f32= 57.29578 bits=0x42652EE0 (180/pi)
0x14259858C f32= 360.0 bits=0x43B40000  (fold)
0x142598580 f32= 180.0 bits=0x43340000
0x14256AE1C f32= 1.0 bits=0x3F800000
0x142588218 f32= -1.0 bits=0xBF800000
0x142565D30 bits=0xFFFFFFFF (abs-mask low word, NOT a value)
0x142565604 f32= 2.0 bits=0x40000000   (count-gate width; 4E7C0 0x140A4E905)
0x14259BDD0 f32= 0.6944444 bits=0x3F31C71C (velocity rate; 4E7C0 0x140A4E89A)
0x1425C48E4 f32= 1369.0 bits=0x44AB2000 (37^2 hard-exit ceiling)
0x1425C48D0 f32= 105.0 bits=0x42D20000
0x142596E6C f32= 80.0 bits=0x42A00000
0x14258C264 f32= 3.0 bits=0x40400000   (ramp top add)
0x14259BE0C f32= 15.0 bits=0x41700000  (cone offset)
0x14259BDF0 f32= 7.0 bits=0x40E00000   (ramp base; NOT 8.0)
```

## 1. Count gate `FUN_140A4EAF0` / `FUN_140A4E7C0`: CONFIRMED (one correction)

**Correction vs cone doc §2/§3:** the radius is **12.0, not 64.0**. The 64.0 is the
folded `v` arg; the 12.0 reaches the core through the stack float slot.

Call chain (bytes CONFIRMED):
- T7a sets (asm `t7a.asm` 0x1404F6544–0x1404F6598): `RCX=TA`, `EDX=cand`,
  `XMM2=head P[cand][holder]` (0x1404F6584 `MOVAPS XMM2,XMM6`), `XMM3=64.0`
  (0x1404F656C `MOVSS XMM3,[0x14259BE44]`, pair-checked §0), `[RSP+0x20]=12.0`
  (0x1404F6558 `MOVSS [RSP+0x28]...` — full-float store of XMM15 from
  0x1404F63BF `[0x14259BE04]`, pair-checked), `[RSP+0x28] low byte = 0`
  (0x1404F6587, **no-op**: 12.0 = 0x41400000 already has low byte 0x00 —
  script-verified, checker asserts), `[RSP+0x30]=1` (0x1404F657B/580).
- `FUN_140A4EAF0` prologue (dis CONFIRMED): single-folds XMM2→`[RSP+0x68]` (u),
  XMM3→`[RSP+0x60]` (v) by ±360 (`[0x14259858C]`, 0x140A4EAF4–0x140A4EB39);
  tail call 0x140A4E6A `CALL 0x140A4E7C0` with
  `(R8=[RSP+0x68]=u, R9=[RSP+0x60]=v, XMM0=[RSP+0x70]=12.0, [RSP+0x28]=1, [RSP+0x30]=b80)`.
  `[RSP+0x70]` aliases the caller's `[RSP+0x28]` slot = the 12.0 float.
- `FUN_140A4E7C0` (dis + decompile CONFIRMED, `0x140A4E7C0–0x140A4E7ED`):
  `param_1=TA, param_2=cand, param_3=u, param_4=v, param_5=radius=12.0,
  param_6=sideGate=1 (OFF), param_7=scaleArg=0.0`.

Core rule (decompile CONFIRMED, operand VAs pair-checked):
1. `cand > 0x15` → return count 0 (`0x140A4E7D2 CMP EDX,0x15; 0x140A4E7D7 JBE`).
2. `side = cand>=0 ? (cand>10) : 0` (`0x140A4E871–87A`); base = holder-side rows
   (`uVar8 = side*11`, `0x140A4E90E IMUL EDI,R12D,0xB`).
3. charPos = `6C6D0(48A10(side?DAT:DAT), velBuf, BDD0/rate)` + `pl+0x55C/0x564`
   (`0x140A4E8D7–8FC`, BDD0 at `0x140A4E89A [0x14259BDD0]` pair-checked).
   `param_7=0.0` ⇒ `fVar17=0.0` ⇒ charPos = plain position (XMM13=0 path,
   `0x140A4E87C MOVZX EAX,[RSP+0x138]` = b80 low byte = 0; T7a stack above the
   call holds 0 there — INF micro-item: exact b80 slot aliasing, does not move
   the rule since any 0 low byte gives scale 0.0).
4. Side gate OFF (`param_6=1` → `0x140A4E931 TEST R13B; 0x140A4E934 JNZ 0x140A4E965`
   skips the `48A30` slot test). The `R14=0x3F8C / lVar10` loop rig
   (`0x140A4EDB/94B/957/94F`) is dead for T7a calls.
5. Per opponent slot `o` (11 slots of the holder's side, `EDI`, skip `o==cand`,
   skip `o>0x15`): oppPos likewise via `6C6D0` with the same rate; admit iff
   `d²(charPos,oppPos) < radius²` (`0x140A4E9D5 COMISS; 0x140A4E9D8 JNC skip`
   — strict; touching the 12 m circle FAILS).
6. Bearing `b = bearingDeg(opp−char)` with coincident-0.0 arm
   (`0x140A4EDA–EA11`, `92D40 + 2π + 180/π`, pair-checked constants);
   threshold `t = 4C4E0(v, out, width=2.0)` = single-folded `v/2.0`
   (`0x140A4EA26 MOVAPS XMM2,XMM12` with XMM12=2.0 from `0x140A4E905
   [0x142565604]` pair-checked; 4C4E0 decompile CONFIRMED in scratch);
   corrected `c = 481B00(u, b)` (fold `b` to [0,360), `c = wrapTo180ish(b−u)`;
   481B00 decompile CONFIRMED shape); count iff `|c| ≤ t`
   (`0x140A4EA42 COMISS XMM0,[RBX]; 0x140A4EA45 JA skip` — soft, touching counts).
   With T7a's `v=64.0`: `t = fold(64.0/2.0) = 32.0`.
7. Return count in AL (`0x140A4EA66 MOVZX EAX,R15B`). T7a commits iff
   `count ≤ (coneResult != 0)` (`0x1404F659D CMP AL,BL; 0x1404F659F JA skip`
   where BL = cone boolean).

What it counts: teammates of the holder's side within 12 m of the holder whose
bearing-from-holder agrees with `u = P[cand][holder]` to within 32.0° after the
`481B00` correction. Threshold: count ≤ (cone?1:0) — i.e. 0 when the cone test
fails, ≤1 when it passes.

GF carrier: **none 1:1**. Needs: per-man position (have: `PressPickInput.posX/posZ`),
plus (a) the `6C6D0` shifted position (rate BDD0=0.6944 × velocity-sample gate;
GF `RunWorld.vel` + `pescarrier.hpp velZero` cover the inputs — the shift itself
is new, ~5 lines), (b) the slot-side walk (have: onField arrays), (c) wrap/bearing
helpers (have: `PesBearingDeg`/`WrapDeg`). No ball/attributes/RNG. Status: CONFIRMED.

## 2. Head vector at `+0x5EC`: CONFIRMED meaning, writer INF (narrowed)

Meaning (bytes CONFIRMED): T7a reads the candidate's **stored facing word**
`[cand+0x554]` (`0x1404F6498 MOVSS XMM7,[RBX+0x554]`), then calls
`FUN_1404AB8E0(candRec, [RSP+0x40])` (`0x1404F64A5/4A8`, target verified
`0x1404F64AD+rel = 0x1404AB8E0`). `4AB8E0` = `439E50(out, rec+0x55C, rec+0x5EC)`
(decompile CONFIRMED). `439E50` (decompile CONFIRMED): coincident
(`[5EC]==[55C] && [5F4]==[564]` → 0.0) else
`bearing = wrap360(deg(atan2([5EC]−[55C], [5F4]−[564])))`.
The call's result **overwrites the local copy** the three wrap tests consume
(T7a decompile: `uVar2=[cand+0x554]` is only the `43A0A0` subtrahend; the head
entering all three tests is `4AB8E0`'s output). So the head is
**bearing(candPos → point at cand+0x5EC)** — neither the facing word itself, nor
motion direction: it is the facing word *as recomputed through* the stored point.
When the stored point coincides with the position it is 0.0.

Writer (INF, narrowed to two producer sites): the only player-record scalar store
is `0x14093CB6C MOVSS [RBX+0x5EC],XMM0` in `FUN_14093CAE0`, value =
`FUN_14093C930(R8rec+0xC8-ish, RDI, RSI)` (dis CONFIRMED `0x14093CB5D–6C`:
`R8=[EAX+RBX]`, `RDX=RDI`, `RCX=RSI`, `CALL 93C930`, store). Gate:
`[RBX+0x5E9]!=0`, point `[EAX+RBX]+{0x5D0|0x5C4}` by `[RBX+0x5BF]/[RBX+0x5E0]`
(0x14093CB37–5A). `93C930` returns a forward-component scalar:
`dot(dirToPoint, facing)*3.6` (`[0x14259CE38]=3.6` script-read), clamped at 5.0
(`[0x14259222C]`), with a rate-decay arm (`fps × 60²/1000`, `1.5 [0x14259BDDC]`
script-read) — decompile CONFIRMED. Sibling adjust `FUN_140934E10([RBX+0x5F4])`
(dis CONFIRMED `0x14093CB7D–84`). Init zeroes both (`FUN_140A6C550`,
decompile CONFIRMED — same fn owns the +0x5D0 zeroing, fixing record identity:
player record). What is NOT established: which tick calls `93CAE0`, and whether
its scalar (forward distance, metres-ish) is the `x` the head bearing reads
(the `+0x5F4` companion writer on the hot path is unidentified; the two MOVSS
`+0x5F4` sites in `w5ec.asm` belong to other records). **Do not port the writer.**
Port the head as a named caller-supplied bearing input; the writer is `needs <head-point X>`.

GF carrier: `PlayerFacts.facing` (`pesoffball.hpp:187`, `+0x554` body facing) is the
facing word, but NOT the head: GF has no `+0x5EC` point and no `4AB8E0` recompute.
**None 1:1; needs new `headBearing_deg` per candidate** (computed by the caller
from its own facing/point state; ponytail names `0x1404AB8E0` + `0x14093CB6C`).
Status: meaning CONFIRMED; writer INF.

## 3. Action word at `pl+0x5D0`: writer CONFIRMED, {1, 0x3F} provenance INF

Reader (bytes CONFIRMED): T7a `0x1404F6484 MOV EAX,[RBX+0x5D0]`,
`0x1404F648A CMP EAX,1 / 0x1404F648F CMP EAX,0x3F` — admit iff `==1 || ==0x3F`.

Writer (decompile CONFIRMED): `FUN_140A6CA10(rec, newVal)`:
`[rec+0x5D4]=[rec+0x5D0]; [rec+0x5D0]=newVal; same ? [rec+0x6B8]++ : ([rec+0x6B8]=0)`.
Init `FUN_140A6C550` zeroes `+0x5D0/+0x5D4/+0x6B8` (same fn — record identity fixed).
Exactly 2 CALL sites (raw CALL scan CONFIRMED):
- `0x14092D607` in `FUN_14092D580`: `6CA10(R8rec+0xC8, [R8rec+0xB5AC])` — value from
  the outer record's `+0xB5AC` word; same function compares `+0xB5AC == 0x36`
  (decompile line 88) and switches on `+0xB5B0 ∈ {8,9,0x1B,0x1C,0x1F,0x20,0x25,
  0x26,0x29,0x2A,0x32,0x33,0x3F,0x40}` (line 570) — the action vocabulary the
  value comes from. The `+0xB5AC/+0xB5B0` writers are **not found by direct
  disp32 scan** (0 hits — split displacement like the mark-table rows, OPEN).
- `0x140A36D6F` in `FUN_140A36B30`: `6CA10(param_1+0x19, 1 or 0x72 by
  [MI+0x116C]==2)` (decompile lines ~124–132) — match-phase script values
  (1/114), gated on the slot dispatch `param_1[0x578+val]!=0`.
Neither site is observed carrying 1/0x3F on player records in open play; the live
{1, 0x3F} path is site (a) with `B5AC∈{1,0x3F}` or a third path (OPEN).

GF carrier: **none 1:1**. `TeamPressState.role[]` is NOT it (press slots 0x33/0x34
family). Closest: `pescarrier.hpp:1014 action` (record+0x5D0, 0x1E/0x25 arms) and
`PlayerFacts.passGet` (action 5 only). **Needs new per-player `actionWord`**
(fed from the carrier/command layer; ponytail names `0x140A6CA10` + both call sites).
Status: writer CONFIRMED; value provenance INF.

## 4. Ramp base (`+0x2AA8` / `+0x18`): struct shape CONFIRMED, writer OPEN

Reader (bytes CONFIRMED): T7a `0x1404F633C MOV RCX,[R8+0x2AA8]`
(`49 8B 88 A8 2A 00 00`, exact-pattern scan: 20 hits), `0x1404F6343
MOVSS XMM1,[RCX+0x18]`; ramp `base=[X]−7.0`, `top=[X]+3.0` (operands §0,
pair-checked), ladder + `−15.0` cone offset (cone doc §2.2).
Sibling readers prove a 3-float struct: `FUN_14052EB6D` (team-slice code, decompile
CONFIRMED) uses `[c0+0x14]`, `[c0+0x18]`, `[c0+0x1C]` with `local_c0=[DAT+0x2AA8]`.
`[DAT+0x2AA8]` is a **pointer** (all readers dereference then `+offset`); the
20 `49 8B 88` sites share one R8 base = `DAT_1436F3DC8` (T7a loads it at
0x1404F6325). The pointed-to object's allocating writer is unidentified
(many `+0x2AA8`-shaped hits are RSP-relative `0xA8` coincidences or other
records' fields; no init store isolated — OPEN).

GF carrier: **none**. GF `Ball::weatherWind`/`SetWeather` is match weather for the
ball trajectory, not this team-slice ramp scalar. **Needs new `rampBase` float**
(or the 3-float struct if sibling ports land); ponytail names `0x1404F633C`.
Status: OPEN (reader + struct shape CONFIRMED; writer unknown; value unknown —
never invent a wind number).

## 5. Presence-reader verdict branches → GF predicates (INF, budget-permitting map)

All three are per-slot `CMP byte [T+slot+0xB7EC],0` walks over 11 slots
(bytes CONFIRMED in `r1/r2/r3.asm`).
No GF consumer exists (grep for `B7EC/markTable` in `src/`+`tests/` empty —
re-verified this session); the duel-verdict word `+0x8D4` has no GF counterpart
either. Mapping is therefore to the GF predicate each branch would gate,
not to existing code:

- `0x140969974` (in `FUN_1409696A0`, decompile `r1.dec` CONFIRMED): two-phase.
  Phase 1: any-mark scan `do { if ([T+u12+0xB7EC]) return; ... } while (u12<11)`
  — marked ⇒ **early return, no verdict written** (the mark EXEMPTS the man from
  this duel path). Phase 2 (unmarked only): `439FE0`/`481B00` bearing gates +
  `48A30` role/formation checks, then verdict `+0x8D4 = 4ECF0(...)` written at two
  sites (`r1.dec` lines 132–135, 236–239) with `+0xC4 = [MI+0x1198]` timestamp.
  GF consumer: the duel-entry / exemption predicate in the press/duel branch
  (today `pesoffball.cpp` trap/press predicates, `pespress.cpp` picker gates).
  The mark input would be a new `markTable[slot]!=0 ⇒ skip` arm. Status: INF
  (branch payload = 4ECF0 verdict family, exact GF call site OPEN).
- `0x140993253` (in `FUN_1409931C0`, decompile `r2.dec` CONFIRMED): any-mark scan
  sets frame flag `bVar6 ([RSP+0xF0]=1)`; consumed deep in the tail:
  `(... || (!bVar5 || (72E70(...)&0x800)==0 ... || bVar6) || ...) || dist²≤25`
  then `+0x8D4 = (4C8E60(...)>324) ? 4 : 2` else `4ECF0(...)` (`r2.dec` lines
  ~118–231). So marked ⇒ the flag is one true-leg of a four-way OR that admits
  the cheap `2/4` verdict over the `4ECF0` call. GF consumer: same duel family;
  the flag is an OR-leg, not a gate. Status: INF.
- `0x1409ADB44` (in `FUN_1409AD830`, decompile `r3.dec` CONFIRMED): any-mark scan
  sets `bVar14`; consumed as `if ((!bVar14 && action∈{1,2,3} && +0x5E5==0))`
  (`r3.dec` `!bVar14 && (*(param_1+0x54)-1U & 0xFFFFFFFD)==0 && +0x5E5==0`) —
  marked ⇒ skip the `627E0/439FE0` holder-verdict block; fall through to the
  `param_1+0x54`-dispatched verdict ladder (`2/4/4ECF0`, 13 `8D4` stores).
  GF consumer: the holder-duel dispatch (GF has no `+0x54` action ladder —
  `e_FunctionType` is the analogue family, not 1:1). Status: INF.

Verdict-word note: all three write the duel-verdict word `+0x8D4`
(values: `4ECF0(...)` computed, or literals 2/3/4 with function-specific
meanings; `+0xC4/+0x6CC` timestamps). GF has no `+0x8D4` word; porting any reader
needs that word (or its GF predicate mapping) first. **Do not port readers yet**
(a wired T7a stays inert, as TeamT2 notes).

## 6. Port spec for MarkTablePort (the four inputs as named caller inputs)

- `countGate(TA, cand, u=headBearing_deg, v=64.0, radius=12.0, sideGate=OFF,
  scale=0.0) -> count`: §1 rule verbatim (strict `< radius²`, soft `|c| ≤ v/2`,
  coincident-0.0 arm, skip `o==cand||o>0x15`, return AL). Constants per §0
  (12.0/64.0/2.0/BDD0/360/2π/180π). Ponytail at `0x140A4EAF0/0x140A4E7C0`.
- `headBearing_deg` per candidate: §2 meaning (bearing to the stored point;
  0.0 when coincident). Writer excluded; caller supplies. Ponytail at
  `0x1404AB8E0` + `0x14093CB6C`.
- `actionWord` per candidate: admit iff `==1 || ==0x3F`. Writer `6CA10`
  semantics; value provenance open. Ponytail at `0x140A6CA10` + both call sites.
- `rampBase`: float feeding `ramp(sign·holderX)` (7.0/3.0/15.0 ladder per the
  mark-table doc §2 + cone doc §2.2). Value open. Ponytail at `0x1404F633C`.
- Readers (§5): not ported; GF predicates named, no existing consumer.

## 7. Checker with mutations (in scratch)

`<workdrive>/gf-MarkGateRE-scratch/check_markgate.py` — self-checking, no PES
bytes in repo: asserts the §0 script-read constants (incl. the 12.0-low-byte
no-op that fixes the radius correction), the `49 8B 88 A8 2A` T7a load shape is
present, the hard-exit `0F 87` at `0x1404F643D`, the cap-3 `41 80 F9 03` at
`0x1404F65D7`, the `%11` magic `B8 A3 8B 2E BA` at `0x1404F65AA`.
**29/29 green; mutations 5/5 killed** (`12.0→13.0`, `cap 3→4`, `%11→%10`,
`64.0→65.0`, `2.0→2.5` each fail exactly one check, reverted).

## 8. Status summary

- Count gate: **CONFIRMED** (rule, constants, threshold, radius correction 12.0).
- Head vector: **CONFIRMED** (meaning + call-site bytes); writer **INF**
  (two producer sites narrowed, hot-path companion writer + tick placement open).
- Action word: **CONFIRMED** (reader tests, writer fn + 2 call sites + init);
  {1,0x3F} provenance **INF** (+0xB5AC/+0xB5B0 split-displacement writers open).
- Ramp base: **OPEN** (reader + 3-float struct shape CONFIRMED; writer + value open).
- Reader verdicts: **INF** (TEST roles + verdict values CONFIRMED; GF predicate
  mapping is a near-miss by design — no consumer exists yet).
- GF state: no 1:1 carrier for any of the four (surveyed `pesoffball.hpp`
  `PlayerFacts`, `pespress.hpp` `PressPickInput.action[22]` (press-only, not
  player state), `pescarrier.hpp:1014 action`, `TeamPressState.role[]`,
  `Ball::weatherWind`). Each names its `needs <X>`.

## 9. Open questions / blockers

1. `140A92D40/2DC0` bit-exactness (inherited O3) — bearings match to ~1e-7, not bits.
2. `4E7C0` b80-slot aliasing (which stack word the scale byte comes from) — rule-neutral (any 0 low byte ⇒ scale 0.0), noted for completeness.
3. `93CAE0` tick placement + `+0x5F4` hot-path writer — head-writer completion.
4. `+0xB5AC/+0xB5B0` split-displacement writers — {1,0x3F} provenance completion.
5. `+0x2AA8` pointed-to object allocator + live value — ramp completion.
6. Duel-verdict `+0x8D4` word mapping — reader-port prerequisite.
