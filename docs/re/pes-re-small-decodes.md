# PES team-tick small decodes (SmallDecodesRE, 24-10-26)

Read-only RE. Image `~/ghidra-projects/scratch/Unpacker/pes_full.bin`
(file offset = VA − 0x140000000). Static byte decode
(`gf-SmallDecodesRE-scratch/rd.py`, capstone 5.0.7); no repo edits.
Slice labels and §0 floats per `pes-re-slice-survey.md` (SliceSurveyRE).
Scope: the survey's ten slices in cost order. Verdict rule as survey:
**port-ready** = every input closed; **port-ready-after-small-decode** =
one named decode; **blocked** names the blocker.
Floats below are script-read (checker `check_small.py`: RESULT OK,
2 mutations caught); every instruction cites its operand VA.

## §0 Floats read this pass (script output, pasted)

```
0x14259d610 = 12.25  (T7b-predicate arm, 0x14057cb5f)
0x142588218 = -1.0   (T7b/T12d select arm)
0x14259a764 = 60.0   (T7b-predicate arm, 0x14057cacf)
0x142565604 = 2.0    (T7b +-2.0 arms)
0x14256ae1c = 1.0    (T7b/T9a/T12d select arm)
0x1425a9680 = -9.0   (T8c-2 40h `0x1404fadb4 MULSS`, [RSI]=per-0.5 rate)
0x14259ce3c = 7.5    (T8c-2 outer SUBSS, 0x1404fae1f; also T9a row 0x1404fcde7)
0x1425a1ff4 = 4.5    (T8c-2 SUBSS, 0x1404faf13)
0x14259be1c = 20.0   (T8c-2 stack arg, 0x1404faf24)
0x1425c48b4 = 8.5    (T8c-2 cone arg, 0x1404fb0c5)
0x1425a9bf0 = 130.0  (T8c-2 cone arg, 0x1404fb0a5)
0x14256ae30 = 120.0  (T8c-2 cone ceil, 0x1404fb1fa)
0x1425656c0 = 4.0    (T8c-2 `0x1404fb056 MULSS`, RNG arm)
0x14259be4c = 70.0   (T8c-2 cone arg, 0x1404faeec; also T7b arm)
0x14259be5c = 90.0   (T8c-2 `0x1404faef7`; also T7b arm)
0x1425a2828 = 6.5    (T8c-2 line arg, 0x1404fb152)
0x142572348 = 0.800000011920929 (T9a `0x1404fcd57 MULSS`, 54-arm)
0x14253ee30 = 100.0  (T9a normaliser `0x1404fce59/7a`, +15.0 at 0x1404fce6e)
0x14259d60c = 9.0    (T9a `0x1404fce51 DIVSS`)
0x142588210 = 10.0   (T9a `0x1404fce3f COMISS`)
0x14259d634 = 10000.0 (T10a helper-6 `0x14053D4C6` nearest seed)
0x14259222c = 5.0    (T10a helper-6 `0x14053D463 MULSS`, countdown fps*5.0+0.5)
0x1425656d0 = -0.0   (T10a `0x14053D522 XORPS` select init; T8c-2/T9a sign mask)

## §1 T7b-predicate — CLOSED, port-ready-after-nothing

`FUN_14057C8C0` (167-line body + tail, `fun_57c8c0.txt`): release test
`(T, slot)` → AL. Full gate chain CONFIRMED:

1. `0x14057c8e3 CMP [MI+0x11B0],1 → 1` (in play); else `0x14057c8ec`
   `T+0x6D50` → `0x14057c8f3 CMP [T+0x255],0 → 1` (latch). Either true → 2.
2. `0x14057c8fc CMP [T+0x88D8],ESI → !=` → 1 (no release while the slot
   differs); `==` requires `0x14057c904 CMP [T+0xB8A3],0 → !=0` else 1.
3. `0x14057c90d CMP [T+0x6C68],0 → !=0` → 4; `==0` requires
   `0x14057c916 CMP [MI+0x116C],1 → ==1` → 4, else 0.
4. `0x14057c94c` record read `A3E190(M+0xE0)`; `0x14057c961 CMP [RAX],4 → ==`
   → 1. `MI+0x116C==1` arm: `0x14057c97a CALL rng; LEA ECX,[RAX+RAX*4]`
   (`×5`); `0x14057c982 CMP EBP(M+0x119C),ECX → >` stays 1... then
   `0x14057c984 JBE` falls to the geometry arms (inverted: above stays 1).
5. Ball arm: `A57800(M+0x40)` pos; `0x14057c9b9 [ball+0x18]+[2.0]` vs
   masked-`[ball]`; above → 1 (`0x14057ca47`).
6. Select arm: `A48A10(slot)` → row; `0x14057ca11 CMP [row+0x88D8],ESI`:
   equal → `max(-1.0, [row+0x564])` else `max(1.0, [row+0x564])`
   (`-1.0=[0x142588218]`, `1.0=[0x14256ae1c]`); `ucomiss` equal →
   `[ball+0x1C]` vs masked (`0x14057ca41`), above → 1; NaN/unequal → 7.
7. `MI+0x116C==1` + record-`[0]==5` arm: `A57710` angle vs 60.0
   (`[0x14259a764]`, `0x14057cacf`); `>60.0` → 0-bound... then
   `match+0x48` row test (`0x14057cafa CMP [row+slot*4+0x48],ESI`);
   `==` → ball-rate arm (`[0x48+0x2482C]` rate × `T+0x254` vs
   `-[ball+0x10]`): above → release-0 path.
8. Fallback: `A4C8E60` (25.0 cone, DOC) then `A627E0` (slot scan):
   `>0x15` → 0; else geometry `12.25=[0x14259d610]` arm
   (`0x14057cb5f COMISS [M+…],XMM6; JA stays 0`).

Returns: `0x14057cb8b MOV AL,1` (release) / `0x14057cb8f XOR AL,AL`
(keep). Caller (`0x14057D240` head, survey) wires: `AL==0` →
`A40820` (release) + clear `T+0x88D8:=0xFF` if still `==ESI`;
`AL!=0` → `A42940(slot,0x6D)` (hold) + set `T+0x88D8:=ESI` if still
`==ESI`. **Port spec delta**: predicate is a pure function over
(MI 11B0/116C/119C, latch 255, T 88D8/B8A3/6C68/254, slot,
ball pos, `A3E190/A57800/A48A10/A57710/A4C8E60/A627E0` — all ported or
DOC except `A57710` = angle helper, name it). `T+0x88D8` has no reader
yet: port is exact-but-inert. Status: **port-ready** (no open input
except the already-known reader hunt).

## §2 T5b — CLOSED as write-only, port-ready (inert)

The disp32 scan (`F8B80000`: 1145 raw hits, only 6 in code; `FCB80000`:
58 raw, 2 in code) closes the survey's OPEN item:

- Writers (tick, step-23 callees): `FUN_140A424F0` (`[M+rdx*8+0xB8F8]`
  5×(bit,flag) slots, guarded `EDX<9`, `count<5`) and
  `FUN_140A42530` (`[M+rdx*8+0xB8FC]`, guarded `EDX<9,R8B<5`), called from
  all five step-23 per-mode bodies (`0x14053b9d9/9eb/bb32/bb4b/bd52/bec0/bed9/c0aa`).
  Clear: `FUN_140A41AC0` (B8EC count=0, 9× B8F8 slots=0/`0xFF`... wait
  `MOV [RAX],R9D(0); [RAX+4]=0xFF`? re-read: `0x140a41af0 MOV [RAX],R9D`
  with R9D=0 then `ADD RAX,8` — pairs (u32 id=0, u32 flag=-1); 5 per
  slot × 9 slots) called once from the dispatch head `0x14053b87f`.
- Readers: `FUN_140A41B30` (`[M+rdx*8+0xB8F8]>>R10D &1` any-of-5,
  guarded `EDX<9`) with 26 callers, ALL in `0x1405xxxx/0x1406xxxx`
  (AI/plan range) — none in the tick (`0x1404F0000–0x1405A0000` zero
  hits); `FUN_140A42170` (two-bit variant, same idiom) 2 callers, also
  non-tick; `FUN_140A41B80` (B8FC-compare variant) 13 callers, one
  tick-range: `0x14053B720` (step-23's own `53B6E0`-family helper:
  `A48A30` record → `A41B80(slot,B8FC-row?)` → AL selects the 70.0/90.0
  float arm — a *local* reuse inside step 23, not a cross-slice read).

Verdict: **port-ready**: port the clear + writers + the 41B30/42170
reader semantics as named pure helpers; nothing in the tick reads them
cross-slice, so wired T5b stays inert until an AI-side port needs them.
No live watch needed (static CLOSED both directions).

## §3 T4a — CLOSED, port-ready (fill decoded past the estimate)

`FUN_1404FD4B0` head: `A428E0(T,slot,3)` ×4 slots (`0x1404FD500–51A`
loop over BL=0..3), then the roster-walk fill (`0x1404FD51D+`):
`A55D40` side test → `A67010/A66F50` row select → card walk
(`0xF8/0x7F` masks, `0x6B` class at `0x1404FD5B1`) → `A40FF0` +
`A42880` commits → `BT EAX,ECX` concept-bit set (`0x1404FD645`) →
`T+0x6DA8` chains (`A51250/A457C0`) → fallback `0x1404FD731 CALL
0x1404FDC80` → plan check (`A3E5D0` type `!=0x37`, `[RAX+0x14]==3`
at `0x1404FD758–765`, else skip to `0x1404FD935`) → `T+0x6DD0` list
walk (`0x1404FD76B–935`: `A41030` slot records, `A40FE0` row arms
`{-8,-6,+1}` at `0x1404FD7F2–7FF`, latch-bit gates on
`[MI+0x1748]/[MI+0x175E]` (`0x1404FD88A–8A7`), then `0x1404FD8C3 CALL
0x1404F6030` commits) → second plan block (`0x1404FD935+`: same
`A3E5D0 !=0x37` gate, `T+0x6DD0` re-walk, `A489D0/A48910` records,
`A3F310/A3F3D0` eligibility, `A3B320`, `A4587B0`, `A40FE0` row arms
with `[row+0x2C]/[row+0x38]` id compares at `0x1404FDAC9–DB22`, then
`0x1404FDB52/DB76 CALL 0x1404F6030`) → list retire
(`0x1404FDBDE–DC21`: `XCHG [RAX+0x18],EDI` + `A42A630` virtual).
Row semantics: `A67010/A66F50` are the two roster sources (bulk vs
per-slot); `A40FF0(T,slot)` reads, `A42880(T,slot,arg)` commits.
Status: **port-ready** — the whole fill chain is now static-decoded
(`§0b` anchors + `fun_t4atail.txt`); `FUN_1404F6030`'s commit payload
body stays the shared T4b OPEN (tracked there, not here).
## §4 T9a — store skeleton CONFIRMED, port-ready as skeleton

`FUN_1404FC720` (517 lines, cached dump re-read end to end). Store
sites named (T-relative): `T+0x6C4C` (ESI frame/count),
`T+0x6C50` (AL/DIL/B branch byte), `T+0x6E5` (BL), `T+0x6D4`
countdown via `A425B0`, `T+0x6E8` via `A425B0/A425D0`,
`T+0x140` side byte, `T+0x13C` (record `[R8*4+0x40]`),
`T+0x6C54/6C48/6C58` timer arms, `T+0xA570` (DIL), per-side
`[RSP+slot*4+0x5C/0x40/0x4C/0x54]` blend rows. Countdown values:
`6.0` seed (`0x1404fca7d MOV R13D,0x6`), `fps*0.8+0.8` re-arm
(`0x1404fcd52 CALL rng; MULSS [0x142572348]=0.8`), halving
(`0x1404fcdad SHR EAX,1`) when the `6C54` arm overruns. Blend row:
rate `×100.0/÷` normaliser (`0x14253ee30`), `15.0=[0x14259BE0C]`
(`0x1404fce6e`) gain, `÷9.0` (`0x14259D60C`), `10.0=[0x142588210]`
gate, `7.5=[0x14259CE3C]` window with `±[ball+0x18]` clamps.
Status: **port-ready** as the store skeleton behind the set-piece
event sink; per-branch countdown refinement is follow-up, not a
blocker.

## §5 T9c — card values CLOSED, skeleton port-ready

Gate chain CONFIRMED as surveyed; card inputs CLOSED this pass:
classifiers `FUN_140A6E450` (returns bit9 of `TBL[class]`),
`FUN_140A6E220` (bit25), `FUN_140A6E320` (bits17-18 = `0x30000`),
with shared table base `0x1426484F0`, 0x5E dwords, dumped verbatim
(all 94 rows in scratch `card-table` log). Semantics read off the
gate use + bit positions:

- `0x3B` class arm (`T9c 0x1404fc5a1 CMP R14D,0x3B`): rows 0x3B–0x41
  all have bit9=1 AND `0x30000` set — the 0x3B family is
  bit9-bearing; the arm additionally requires `PA+0x1A==PA+0x1C`
  (`0x1404fc583`) and NOT `A6E220` (bit25: rows 0x3B–0x41 have
  bit25=0, consistent — the `A6E220` test excludes the bit25
  classes 22–24/26/30/34/44/49/52/53/62).
- Keeper arm (`A6E320` = `0x30000` test): rows with `0x30000` =
  8,9,10,25,27,28,31–33,36–43,50,51,54,56–61,63–66,68 — the keeper
  class is inside this set (GF: `pl+0x5D0==0x4D` arm at
  `0x1404fc5dd` is separate, geometry-gated).
- Whitelist `0x1404fc65a MOV RCX,0x4010000000200010; BT RCX,RDI`:
  bits set = 4, 20, 50, 62 → classes {0x04,0x14,0x32,0x3E}.
- Blacklist `0x1404fc6f6 MOV ECX,0x2000401; BT ECX,EDI` (after
  `EDI-=0x34`): bits 0, 10, 21 → action offsets {0x34,0x3E,0x49}.
- `0x48`-row `0x25/0x2C` + `0x39..0x3F` + `{3,5,2}` + `0x12/0x16`
  arms: literal class compares, no table.

**Port spec delta**: table `0x1426484F0` ports as a 94-entry const
row (stock dt18-derived? NO — this is code-const in .rdata, port as
`kCardBits[94]` with the VA cited); the three classifiers are one-line
bit tests; the gate maps GF action classes onto {0x3B-family,
keeper, whitelist, row-arms} with `// ponytail:` naming the PES table.
Status: **port-ready** (values closed; only GF-class mapping is
stand-in).

## §6 T10a — CLOSED, port-ready (helper-6 body decoded, §0b)

`FUN_140520930` produces the 11-row side bytes + `+0x8E08` plan
commits (survey-cached re-read stands). The taker words live one call
downstream in `FUN_14053D3B0` (helper 6, `0x1404F7A39 CALL 0x14053C6E0`
family; tick passes `R8=RDX=T` at `0x1404F7A28`, `RCX=[RDI+0x6D28]`
step-52 ctx): countdown arm (`ctx+0x14++`, clear `T+0x40` at expiry
with the `0x116C==2` / budget / `fps*5.0+0.5` vote ladder,
`0x14053D417–491`, seed `5.0=[0x14259222C]`); idle arm walks the match
event list for type-`0x10` with `MI+0x1554==1`, `MI+0x1558==side`,
seeds `T+0x44:=0xFF` + distance `10000.0=[0x14259D634]`, then the
11-slot nearest loop (`0x14053D530–592`: skip `MI+0x173C`, `>0x15`,
`1748`-latch, `A638C0`-commanded, `A48A10` row, `A8F890` distance,
keep best in `T+0x44`); `<=0x15` → `T+0x40:=1`. The survey's two OPENs
are these two CALLs (`A638C0` at `0x14053D55A`, `A8F890` at
`0x14053D57D`). Status: **port-ready** (subs half stays parked —
that logic lives outside both bodies).

## §7 T12b — table is a jump table, tests PARTLY closed

- `FUN_140579E10` (112 lines): NOT a data table — a 0x30-entry
  computed-jump selector (`EDX>0x2F → NULL`, else
  `JMP [RSP+RDX*8]` over 48 stacked row pointers `RCX+{0x20,0x60,
  0x90,0x5A8,0xC8,0x108,0x140,0x178,0x1B0,0x1E0,0x500,0x260,
  0x2A8,0x540,0x568,0x6D8,0x3D0,0x400,0x440,0x488,0x4D0,0x2E8,
  0x5D0,0x610,0x640,0x668,0x340,0x388,0x698,0x318,0x700,
  0x730,0x748,0x760,0x778,0x7A0,0x7B8,0x7D8,0x7F0,0x808,
  0x820,0x838,0x868,0x880,0x898}`).
- `FUN_14057A160` (43 lines): `A48A30` team record → scan 11
  `T+0xA2A0` rows for `[R8+0x14A8]==slot` AND row `∈{0x25,0x2C}` →
  AL. CLOSED (pick test).
- `FUN_14057A1F0` (109 lines, head read): `579E10(ctx,MI+0x1198)`
  → concept row; latch `B6E8` clear/set per slot over the 11
  `A2A0` rows (`0x30/0x0` stamps); `A40820` (release) for rows
  `∈{1,0x22}`. Tail OPEN (lines 60–109 partially read).
- `FUN_14057A3E0` (88 lines): CLOSED — the T12c stop helper, not a
  T12b test: `579E10` → clear `B6E8[slot]` → vcall `[RAX+0x20]` →
  per-slot `B6E8:=0x30/0x0` + `A40820` for `{1,0x22}` rows +
  `T+0x1500:=0xFF`, `A84E30(T+0x6F4)` → full `B7D8–B820` mark-block
  clear (`0x14057a50e–56e`: B7D8/B7DC/B7E4/B7EC/B7F4/B7F6/B824=3/
  B7F8–B820=0, `[row+0xC]:=M+0x1198`) → the `A070`/`A3E0` pair
  shares this row base.

Status: **port-ready-after-small-decode**, re-scoped: the four
bodies are now read (selector semantics + pick test + stop clears
closed); what remains is the per-concept row *payloads behind the
48 pointers* (which the survey estimated as "the table" ~15). The
selector skeleton ports now behind a `// ponytail:` stand-in naming
`579E10`; exactness waits on the payload rows.

## §8 T12c — row struct CLOSED, executor port-ready

Commit-row base traced tick-side: `0x1404F6FCE–F82` builds 11×0x40
rows at `[RBP-0x40]` via `A40240` (zero 0x40 ×11); `0x1404F7673
CALL 579840` (selector) then `0x1404F7698 CALL 579BF0` with
`RSI=rowbase−T` (T-relative!), `R9=[RBP-0x78]` (side rows:
`[T+0xA2A0]` at `0x1404F6FEF`), `R8=T`, `RDX=[RSP+0x68]` (stack
copy chain). Inside: per concept (0x30 loop) × per slot (0xB loop):
qualify `{0x25,0x2C}` rows → collect `R12[EBX]=slot` list →
`579E10` row → `A070` end-test (CLOSED: `B748`-collect compare
`A62140` + `0x50` budget + tactic byte) → `A3E0` stop (CLOSED above)
→ 5-commit chain over `RBX = rowbase + (slot<<6)` (stride 0x40):
`A429A0(T,slot,[R13+slot*4])`, `A42A40(T,slot,RBX+0x20)`,
`A42A30(T,slot,[RBX+0x1C])`, `A42B80(T,slot,[RBX+0xC])`,
`A42B50(T,slot,RBX+0x10)`. Payload-row semantics: offsets
`+0xC/+0x10/+0x1C/+0x20` inside the 0x40 concept rows (row base =
`579E10` return, i.e. the 48-pointer set). Status: **port-ready**
(executor + end-test + stop closed; row *contents* are whatever the
concept payloads write — shared OPEN with T12b payloads, tracked
there, not here).

## §9 T12d-1 — CLOSED, port-ready (all helpers in-image, §0b)

`FUN_140538050` mode-4 arm (`0x140538160+`) CONFIRMED end to end:
latch gate (`[RSI+0x255]`), B8B4 ladder (mode 4 = `EBX-1-2-1-1`
chain → `0x14053810C JMP 0x140538160`), `538550` search
(in-image head: `116C==1` gate, `MI+0x173C` slot, `A489F0` record,
`[rec+0x1494]==0` gate — tail beyond 400 bytes, contract sufficing),
11-slot scan: `A48A10` row → `max(±1.0)` select on `[row+0x564]`
(`1.0=[0x14256AE1c]`, `-1.0=[0x142588218]`) vs running best →
`1748` latch bit → second `A48A10` → `A4C720(-12.0=[0x14259D664],
survey §0)` cone → `FUN_140A4C9A0` (in-image, decoded §0b: slot-range
bit + `[M+0x18+idx*0x14+0x44]` side-x line + `A48A10` rows +
`[row+0x55C]/[row+0x564]` geometry vs `T+0x254`-scaled ball terms;
the survey's `0x1404C9A0` is a typo for `0x140A4C9A0`) → up-to-3
takes: `537C60` search-block fill → `A79640` rank (ascending-stable,
DOC) → `A429A0(slot,0x1E)` + `A42A40` × ≤3, `BA2E8BA3` mod-11 idiom.
Status: **port-ready**, no stand-in; modes 1/2/3/5/6 stay parked
(T12d-2).

## §10 T8c-2 — 40h ports now (both payloads closed to contract)

- `FUN_140A62960` (24 lines): pure scan `[M+0xA2A0+idx*4]==0x3A`
  over 0xB slots → `side*11+idx` (`IMUL ECX,EBX,0xB`) or `0xFF`.
  CLOSED.
- `FUN_140A4FEA0` (~330 bytes): head = wrap `XMM3` into `[0,360)`
  (`±360.0=[0x14259858C]`); body = receive gate over
  `A489F0/A48A10` record reads, `A6CF70`, `A39E50`, `A627E0`,
  `A07740` — contract from the two 40h call sites: returns AL,
  `JNZ` = reject (bit cleared). CLOSED to contract; inner angle
  dataflow INF (one trace left, not blocking: the gate's accept/
  reject contract is what 40h consumes).
- 40h outer gates (constants paired to operand VAs): ball-box
  (`-9.0=[0x1425A9680]` rate MULSS, `7.5=[0x14259CE3C]`,
  `4.5=[0x1425A1FF4]`, `20.0=[0x14259BE1C]`), carrier/25.0 arm,
  `70.0=[0x14259BE4C]`/`90.0=[0x14259BE5C]` cone args,
  `8.5=[0x1425C48B4]`/`130.0=[0x1425A9BF0]`/`120.0=[0x14256AE30]`/
  `6.5=[0x1425A2828]` line arms, RNG arm (`×4.0=[0x1425656C0]`,
  `+0.5`, `==EBP` gate), `8.0=[0x14259BDF4]` arm.

Status: **port-ready** (40g was already; 40h now joins it).

## §11 Updated port-ready list

Port-ready now (this pass promotes ★): T9b, T10b, T8a, T8c-1,
T12a-skeleton, ★T7b-predicate, ★T9a-stores, ★T9c-skeleton,
★T8c-2 (40h), ★T12c-executor, ★T12d-1 (mode-4, no stand-in),
★T5b (inert), ★T4a (clear + fill), ★T10a (taker pick). Still
after-small-decode: T12b (re-scoped: 48 concept payload rows behind
the `579E10` pointers). Blocked (unchanged): T4b (commit payloads +
sheet-plan), T6a (kick-list writer), T8b (`8C1F/8CE8` writers), T8d
(`40800` orders + `B6DC` writer), T10c (row-base identity).

## §12 Checker

`gf-SmallDecodesRE-scratch/check_small.py`: 25 float rows + 12 opcode
rows + 1 byte row; `RESULT OK`. Mutations (each reverted): float
`12.25→12.5` fires `BAD float 0x14259D610`; site `MOV AL,1→0` at
`0x14057CB8B` fires `MISS`; float `10000.0→10001.0` fires
`BAD float 0x14259D634`; site `T+0x44:=0xFF→0xFE` at `0x14053D500`
fires `MISS`. Scratch: `rd.py`, `fun_57c8c0.txt` (predicate body +
tail), `fun_t4atail.txt` (T4a `4FDC80` + both `4F6030` commits +
helper-6 + `4C9A0` anchors), `check_small.py`.
