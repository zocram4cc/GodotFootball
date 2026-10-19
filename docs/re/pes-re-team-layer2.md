# pes-re-team-layer2: closing Slice C open items 1, 2, 3 + §5 item 8

Decode pass on `FUN_140A79640` (sorter), `FUN_140A40CA0` (bucket+lateral sort),
its 8 callers, and the `FUN_1405E6E80` support-count arms. Base `0x140000000`,
image `~/ghidra-projects/scratch/Unpacker/pes_full.bin`. Raw disassembly is the
authority throughout; decompile line refs are given only as finders.

## 1. `FUN_140A79640` sort direction, key, and tie rule

**Verdict: CONFIRMED — ascending by the float key at element+4, stable (ties keep insertion order).**

`FUN_140A79640(buf, flag)` sorts 8-byte elements `[slot u32][key f32]`. It is a
front for MSVC `std::sort`-family machinery, but the ORDER is pinned by the
two raw compare sites, not by the library shape:

- Insertion-sort base `FUN_140A79050` (`dis_140A79050.txt`):
  - `0x140A79096 MOVSS XMM0,[RSI+4]` (key of the already-sorted predecessor),
    `0x140A790A0 MOVSS XMM1,[RSP+0x44]` (key of the current element; `[RSP+0x40]`
    holds its saved 8 bytes, so `+0x44` is its key).
  - `0x140A790A6 COMISS XMM0,XMM1 / 0x140A790A9 JBE 0x140A790C0`: predecessor
    key `<=` current key falls through to the in-order path (store, no move).
    The move path (`0x140A790AB–BB`, `memmove` via `[0x14252FD68]`) runs only
    when `prev > cur`.
  - Inner walk (`0x140A790E0–F5`): `COMISS XMM0(current key),XMM1` per stepped
    element, `0x140A790F5 JA 0x140A790E0` keeps shifting left while `scanned >
    current`; `0x140A790D4 JBE 0x140A790F7` stops at the first `scanned <=
    current`. Shifts are strict-greater only: **equal keys never move**.
- 2-element fast path in merge `FUN_140A788C0` (`dis_140A788C0.txt`,
  `0x140A78B70–84`): `COMISS [RDX+4],[RBX+4]; JBE skip; swap`. Swaps only when
  `a > b`: ascending.
- Lower/upper partition helpers agree: `FUN_140A79120`
  (`0x140A7914D COMISS pivot,[elem+4]; 0x140A79155 JBE 0x140A79167`) and
  `FUN_140A79340` (`0x140A79376 COMISS [elem+4],pivot; 0x140A79379 JA 0x140A7938B`)
  both implement `lower_bound`/`upper_bound` with the same key.
- Merge `FUN_140A78640` (`dis_140A78640.txt`, `0x140A78675 COMISS
  [RCX+4],[RDI+4]; 0x140A78679 JBE 0x140A78690` take-left): ties take the LEFT
  (earlier) run first: **stable**.

The `param_2` flag of `FUN_140A79640` does NOT select direction. `TEST DL,DL /
JZ` at `0x140A796D4` selects WHICH bucket pair is gathered/sorted (first
`param_1[0..0x15]` when flag==0, second `param_1[0x16..0x2B]` when flag!=0 via
the `FUN_141E53540` scatter path at `0x140A796F2/71A`); both sorted paths use
the same ascending machinery (`FUN_140A79440` at `0x140A7987B` for pair 1,
`FUN_140A79240` at `0x140A79A51` for pair 2). CONFIRMED raw dis.

**Port statement (Slice C-C2):** within each class bucket, emit slots ordered
by lateral-x ascending, ties in slot order (stable). Replace the Slice C
stand-in "`FUN_140A79640` direction = ascending-until-decoded" with this.

## 2. Lateral-x base: `+0x46EC` vs the `+0x4E3` family

**Verdict: CONFIRMED — the same field. `S+0x3AA4 == T+0x46EC`; the `+0x4E3`
family is the index-domain spelling of the same address.**

Sort-key load (`dis_140A40CA0.txt`):
- `0x140A40E91 MOV EAX,0xBA2E8BA3; 0x140A40E9C MUL R8D; 0x140A40E9F SHR EDX,3;
  0x140A40EA2 IMUL EAX,EDX,0xB; 0x140A40EA5 SUB ECX,EAX` — magic-divide:
  `ECX = R8D % 11` (slot id mod 11; `R8D` is the designation-slot id, range
  checked `<0x16` at `0x140A40EBF`).
- `0x140A40EAC MOVSXD RAX,ECX; 0x140A40EAF ADD RAX,0x4E3; 0x140A40EB5 LEA
  RAX,[RAX+RAX*2]; 0x140A40EB9 MOVSS XMM0,[R14+RAX*4]` — byte offset
  `(slot%11 + 0x4E3) * 3 * 4 = 0x3AA4 + 12*(slot%11)` from `R14`.
- `R14` is `param_1` (`0x140A40CCC MOV RDI,RDX; 0x140A40CE0 MOV R14,RCX`; role
  array read `[R9]` with `R9 = RCX+0x3FB8` at `0x140A40D19/30`), i.e. the
  team-context block `S` (role arrays at `S+0x3FB8` per role-writer §1).
  So the key is the float at **`S+0x3AA4+12*(slot%11)`**.

Identity with `T+0x46EC`:
- `S+0x3AA4 = T+0x46EC` because `S+0x3FB8 = T+0x4C00` (role-writer §1, `0x3FB8
  + 0xC48 = 0x4C00`), so `S = T+0xC48` and `0x3AA4+0xC48 = 0x46EC`. Arithmetic
  CONFIRMED (`python3`: `hex(0x3AA4+0xC48) = 0x46EC`).
- Internal consistency: `S+0x3AA4 = (S+0x3A9C)+8`, the +8 float of the
  12-byte slot-point row. The writer in `FUN_1404A8620`
  (`dis_1404A8620.txt`, `0x1404A87E9 LEA RDI,[R14+0x3AA4]` then
  `0x1404A8854 LEA RAX,[RDI-8]` / stores `[RAX]=f0,[RDI-4]=f1,[RDI]=f2`) fills
  rows at `(S+0x3A9C)+12*i` from the `FUN_1408C50E0(sheet,slot,0,3)` formation
  triple `(f0,f1,f2)`, slot loop `0x1404A87F0–9B` (writes scratch `S+0x3A9C/0x3B20`
  rows immediately before calling `FUN_140A40CA0`).
- In-tick: `FUN_14051D9E0` copies slot-point rows forward
  (`dis_14051D9E0.txt`: `0x14051DADE–DC71` copies `P+0x3B20+i → P+0x3C28+i` and
  `0x14051DC33–71` copies `P+0x3A9C+i → P+0x3BA4+i` for i in `0xE4..0x108`
  step 12 — a 3-row shift of the 12-byte rows), and `FUN_140A422C0` at
  `0x14051E31E` swaps two 12-byte rows. The `8C50E0`-triple fill of the
  in-tick `P` rows happens on the set-piece paths (`FUN_14051CF70`/
  `FUN_14051D100`, role-writer §4b). Which writer last touched the in-tick
  rows before the `0x14051E380/3C2` publisher calls is INF (needs the `P`
  row-fill call chain above `FUN_14051D9E0`); the field identity and the
  picker/set-piece writers are CONFIRMED.

**Port statement (Slice C):** bucket-by-class (flag mapping §3), then lateral
order = ascending f2 (`S+0x3AA4+12*(slot%11)`, GF side: adapted-x stand-in
stays until the row-fill chain is ported — keep the existing
`// ponytail: FUN_140A40CA0 lateral load` but re-point it: the stand-in is for
the triple's f2, not "formation-x" generally).

## 3. `FUN_140A40CA0` R9B flag (`TEST BL,BL / JZ 0x140A40D3E`) + all 8 callers

**Verdict: CONFIRMED raw bytes + disassembly.**

Flag branch (`dis_140A40CA0.txt`):
- `0x140A40CD7 MOVZX EBX,R9B; 0x140A40D3C TEST BL,BL; 0x140A40D3E JZ 0x140A40D73`.
- Flag==0 (JZ taken): skips the class remap; `EAX` = raw class byte
  (`0x140A40D30 MOV EAX,[R9]`, `R9 = S+0x3FB8+i`), bucket index = raw class
  0..9.
- Flag!=0: jump table via `0x140A40D47 MOV ECX,[R13+RAX*4+0xA40FA8]` for
  `EAX<=9` (`0x140A40D42 CMP EAX,9 / 0x140A40D43 JA 0x140A40D70` raw-passthrough
  `EAX=R15D=0`... precisely `0x140A40D70 MOV EAX,R15D` zeroes >9):
  `0x140A40D54→1, 0x140A40D5B→3, 0x140A40D62→6, 0x140A40D69→9`
  (dec lines 54–74 agree: `{1,2}→1, {3,4}→3, {5,6}→6, {7,8,9}→9`, else 0).
  So flag 1 folds 10 classes into 5 buckets: 0, 1, 3, 6, 9.

All 8 call sites (E8 scan of `pes_full.bin`, rel32 CONFIRMED; R9B setup from
raw bytes around each site):
| call-site VA | R9B passed | context / why |
|---|---|---|
| `0x1404A88B4` | `R15B` (register: `45 0F B6 CF MOVZX R9D,R15B`) | picker `FUN_1404A8620` (head `0x1404A8620` per bridge dec) case-4 designation pick; R15B = live-flag/phase selector for scratch remap |
| `0x1404A8A59` | `1` (`41 B1 01 MOV R9B,1`) | picker `FUN_1404A8930` (head `0x1404A8930` per bridge dis) case-5 path via `FUN_1404A8AD0`; register/const twin-call pair in that wrapper |
| `0x1404A9330` | `R14B` (`45 0F B6 CE MOVZX R9D,R14B`) | picker `FUN_1404A90D0` (head `0x1404A90D0` per bridge dis) case-6 path; register-carried flag |
| `0x1404A97AB` | `1` (`41 B1 01`) | picker `FUN_1404A93A0` (head `0x1404A93A0` per bridge dis) case-2 path |
| `0x14051E380` | `1` (`41 B1 01`) | in-tick publisher `FUN_14051E350` 1st call → `[ctx+0xBA8]+0x7CDC` table (dec: `MOV R9B,1`, `LEA RCX,[RAX+0xC48]`) |
| `0x14051E3C2` | `1` (`41 B1 01`, visible in the `51E380`-function byte run) | in-tick publisher 2nd call → `+0x7D0C` table |
| `0x140A59BAF` | `1` (`41 B1 01`) | bulk setup `FUN_140A59B20` (`R8B = SETGE(EDX>=11)`, `RCX += 0x44/0x8204` table select); designation compact after bulk fill |
| `0x140A59D2D` | `1` (`41 B1 01`, same SETC pattern) | bulk setup twin (`FUN_140A59CA0`-family) |

So: every in-tick and bulk/setup caller passes 1 (5-bucket fold); only the
picker first-calls pass a register flag (`R15B`/`R14B`) that selects raw (0)
vs folded (1) per picker case. The two `FUN_14051E350` publisher calls both
pass 1 — the in-tick designation tables are ALWAYS 5-bucket. CONFIRMED.

**Port statement (Slice C-C1):** bucket by folded class with map
`{1,2}→1, {3,4}→3, {5,6}→6, {7,8,9}→9, else 0`; classes 0 stays 0. The flag is
a port-time constant 1 for the tick path (no GF branch needed); the picker
register-flag behavior is NOT ported (picker is out of Slice C scope).

## 4. Per-branch support-licence counts at attack level 0 and 1 (`FUN_1405E6E80`)

**Verdict: CONFIRMED raw dis (`dis_1405E6E80.txt`); decompile `bVar26/bVar10/bVar9` cross-checked.**

Inputs (CONFIRMED):
- `bVar10` = attack level `T+0xB8A4` byte (dec line 363: `*(puVar2[2]+0xB8A4)`).
- `bVar9` = `FUN_1405DFFD0(T,1,0)` = `*(T+0x7B5D)` char (dec of `5DFFD0`:
  `param_3==3`? no — `(1*3+0)*0x14` offset → `*(param_1+0x7B21+0x3C)`).
- `bVar26` = working support count; `local_450+0xB8A2` = B8A2 flag byte.

Arms, with raw sites (DIL = bVar26, EBX = bVar9, R14B = bVar10):
1. Base from count (`0x1405E8421 CALL 5DFFD0; 0x1405E8426 MOVZX EBX,AL`):
   `bVar9<=1 → 0` (`0x1405E8444 XOR DIL,DIL`); `<=3 → 1` (`0x1405E843F MOV
   DIL,1`); `==4 → 2` (`0x1405E8435 MOV DIL,2`); else `→ 3` (`0x1405E843A MOV
   DIL,3`). So base = {0, 1, 2, 3} for count {0–1, 2–3, 4, 5+}.
2. B8A2 arm (`0x1405E844B CMP [RAX+0xB8A2],0; 0x1405E8459 ADD DIL,-1`, gated
   `0x1405E8452 JZ skip / 0x1405E8454 TEST DIL,DIL / JZ skip`): if B8A2 set
   and count>0, count−1.
3. `0x1405E85A1` region: `*(T+0x8134)==slot` else-branch
   (`0x1405E8576 JNZ 0x1405E8598`): if `DIL>1` (`0x1405E8586 CMP R14B,1;
   0x1405E858A JA` + `0x1405E858C CMP DIL,1; JBE skip`) and forward-scan flag
   (`0x1405E857E TEST BL,BL; JZ skip`), `DIL−1` (`0x1405E8592 ADD DIL,-1`).
4. Level<2 arm (`0x1405E8598 CMP R14B,2; 0x1405E859C JC 0x1405E85A1` falls into
   `0x1405E85A1 CMP R14B,1; JNC skip`): if `bVar10<1`, i.e. **level 0 only**,
   and `DIL>1` (`0x1405E85B0 CMP DIL,1; JBE skip`), `DIL−1` (`0x1405E85B6`).
   At level 1 this arm does NOT fire.
5. Level==0 arm (`0x1405E85A1` target block: `CMP R14B,1 / JNC 0x1405E85BA
   skip`; `0x1405E85A7 CMP [T+0x8134],slot; JNZ skip`; `0x1405E85B0 CMP DIL,1;
   JBE skip`; `0x1405E85B6 ADD DIL,-1`): if `bVar10==0` (level 0 only) and
   carrier-slot match and `DIL>1`, `DIL−1`.
6. Companion site 2 (`0x1405E94CE / 0x1405E9527 CMP [RAX+0xB8A2],0` regions in
   the second `5DFFD0` block at `0x1405E94CE...`): `bVar20 = bVar9>4`
   (dec 967) with the `bVar10<2 && bVar20 → bVar20=false` clamp — same shape,
   second slot-table head.

Enumerated counts (base from arm 1, then decrements, each floored at
`DIL>1` i.e. minimum 1 after each decrement... precisely: each decrement arm
fires only if `DIL>1`, so count never drops below 1 via arms 2–5; arm 1 can
yield 0):
- count 0–1 → base 0 → arms 2–5 cannot fire (DIL==0): **0 at every level**.
- count 2–3 → base 1 → no arm fires (all need DIL>1): **1 at every level**.
- count 4 → base 2 → arm 2 (B8A2, in-play only... B8A2=(L≥4) so never at
  L0/L1): no; arm 3 (needs fwd-scan): −1 if scan; arm 4 (L<1): −1 only at L0;
  arm 5 (L==0 + carrier): −1 only at L0. So count-4 at **L1: 2, or 1 if the
  forward-scan fired; at L0: 1, or 0... no — floor: arm 4 fires if DIL>1
  (2>1 yes → 1); arm 5 needs DIL>1 (1>1 no) → stays 1 without scan, or with
  scan: arm3 →1, arm4 needs >1 (no), arm5 no → 1.** Net L0/L1 for count-4:
  **1–2, level makes no difference except via scan interplay**.
- count 5+ → base 3 → L1: 3 (no scan) / 2 (scan); L0: 2 (no scan: arm4
  3→2, arm5 2>1? yes if carrier-match → 1) — exact L0 value depends on
  carrier-slot match and scan: **1–2 at L0, 2–3 at L1**.

Settled wording: at L0/L1 the mechanism is 2–3 stacked −1 decrements (B8A2 is
dead at L<4, so: forward-scan, level<2... precisely level==0, carrier-match
at level==0), each floored to keep ≥1. **"Zero" is wrong** (no arm can take a
base-1+ count to 0; only base-0 stays 0); **"two non-forwards" is the right
shape for the top branch** (count 5+ at L1 = 3, at L0 = 1–2 after the two
level-gated decrements). The auditor's "floor switches off" stays INF/OPEN
as before; the concrete L0 effects are arms 4+5 above plus the `ef910` skip
(V1).

## 5. Corrections to `docs/re/pes-re-team-layer.md`

| doc:line | old | new | evidence |
|---|---|---|---|
| §3 Slice C stand-ins (~line 178) | "`+0x4E3`-family x = GF adapted x (ponytail `FUN_140A40CA0` lateral load); `FUN_140A79640` direction = ascending-until-decoded (ponytail)" | "lateral key = f2 of the slot-point row at `S+0x3AA4+12*(slot%11)` (= `T+0x46EC+12*(slot%11)` lane-x); GF adapted-x stand-in kept only for the f2 value (ponytail: `FUN_140A40CA0` row-fill chain); `FUN_140A79640` = ascending, stable (§1 above)" | dis `0x140A40E91–EB9`, `0x140A79096–F5`, `0x140A78B70–84`; `dis_1404A8620` writer `0x1404A87E9–892` |
| §5 item 1 (~line 226) | "sort direction (ascending vs descending) — decode the sorter body. Blocks C-C2 default" | "CLOSED: ascending by key at element+4, stable (ties keep slot order). Port: lateral-ascending within bucket" | dis `FUN_140A79050` `0x140A790A6 JBE` + `0x140A790F5 JA`; `FUN_140A78640` `0x140A78679 JBE` |
| §5 item 2 (~line 227) | "Lateral-x base (`+0x46EC` vs `+0x4E3`-family) — resolve the indirection. Blocks C exactness" | "CLOSED as field identity: `+0x4E3`-domain `(slot%11+0x4E3)*3*4` = `S+0x3AA4+12*(slot%11)` = `T+0x46EC+12*(slot%11)` (f2/lane-x). OPEN sub-item: last in-tick writer of the `P` rows before `0x14051E380/3C2`" | dis `0x140A40E91–EB9`; arithmetic `0x3AA4+0xC48=0x46EC`; `dis_1404A8620` writer |
| §5 item 3 (~line 228) | "`FUN_140A40CA0` R9B flag (0 vs 1...) — read the flag branch. Blocks C/D" | "CLOSED: flag 0 = raw class buckets, flag 1 = 5-bucket fold `{1,2}→1,{3,4}→3,{5,6}→6,{7,8,9}→9,else 0`. In-tick publisher passes 1 both calls; 6 of 8 sites pass 1, 2 picker sites pass a register. Tick port uses constant fold" | dis `0x140A40D3C–70`; E8 scan (8 sites) + raw R9B bytes |
| §5 item 8 (~line 234) | "Per-branch licence counts at L0/L1 ... 'zero' vs 'two non-forwards'" | "CLOSED: table in §4 above. 'Zero' refuted (decrements floor at 1; only base-0 is 0). Top branch: 2–3 at L1, 1–2 at L0" | dis `0x1405E8421–5B6` arms |
| §2 V4 (~line 144) | "Sorter call `0x140A40F13 CALL 0x140A79640`" (no direction) | add: "sorts ascending-stable by lateral key (§1); flag arg is 0 at `0x140A40F13` (first bucket pair)" | dis `0x140A40F0C–13` (`XOR EDX,EDX` flag 0) + §1 |
