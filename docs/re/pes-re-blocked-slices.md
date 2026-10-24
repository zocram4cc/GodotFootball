# PES team-tick blocked slices (BlockedSlicesRE, 24-10-26)

Read-only RE. Image `~/ghidra-projects/scratch/Unpacker/pes_full.bin` (file offset = VA − 0x140000000).
Bridge `http://127.0.0.1:8080` (lowercase hex `address=0x…`; endpoints used: `decompile_function`,
`disassemble_function`, `xrefs_to`, `get_function_by_address`). Static byte work with capstone 5.0.7
(`rd.py`: `dis`/`scan`; byte-anchored owner search for disp32 — linear sweep desyncs on data, so every
raw-pattern hit is resolved by decoding from each candidate start `h-15..h` and keeping the instruction
whose span covers the disp field and whose `CS_OP_MEM.disp` matches).
Scratch: `<workdrive>/gf-BlockedSlicesRE-scratch/` (bridge dumps `dec_*.txt`/`das_*.txt`, `checker.py`:
RESULT OK, 3 mutations caught). No repo edits. Slice labels/VAs per `pes-re-slice-survey.md` (SliceSurveyRE)
and `pes-re-small-decodes.md` (SmallDecodesRE); nothing re-derived except where noted.

Live PES (pid 2510857, read-only `pread`): in MENU the whole pass (`G=0x1eb942c0`, `MI=0x0`, `MA=0x0`);
no live values sampled. Live recipes below re-resolve from the globals every read
(`tasks/14-10-26.md` §Live chains).

## §0 Constants (checker output, script-read, pasted)

```
0x14253ee30 = 100.0   (T10c/A408C0 rate: MOVSS XMM7 at 0x140A40922; also 4FEA0 wrap context)
0x14259D628 = 110.0   (T12a 57A580 tail facing-bearing ceiling at 0x14057A83D)
0x14259D664 = -12.0   (T12d/T7b select arm; survey §0)
0x142588200 = 0.10000000149011612 (T12b 579840 select epsilon at 0x1405799F0)
0x142565600 = 0.5     (T12b addend at 0x1405799F8)
0x142647340 = 127.0   (T8d 65DD0 divisor: rec byte/127.0 at 0x140A65DF1)
```

Byte anchor: `0x140501845: 88 88 DC B6 00 00` = `MOV byte ptr [RAX+0xB6DC],CL` (T8d live writer store).

## §1 Per-slice verdicts (cheapest first)

### T4b — PORT-READY (commit payloads CLOSED, SheetTriple M1 caller-mapped)
`FUN_1404F6030` (T+0x6D50 row commit, 3 call sites in `FUN_1404FD4B0` at `0x1404FD8C3/DB52/DB76`):
body is (1) gated publish + (2) row clear. Decompile + disassembly agree:
- `0x1404F6059 CALL 0x140A489D0` (slot table: `p2<2 ? *(G+(p2+0xA3)*0x28) : *(G+0x1978)`),
  gated on `MI&&row&&param_2!=0`;
- `0x1404F607B CALL 0x140A3AEE0` (concept-bit test: returns `0xFF` unless record head matches,
  else `record[slot%0xB+1]`);
- `0x1404F60AC CALL 0x140A5BAE0` (plan store: guards `p2<2,p3<3,p5<2,p4-0xE<2`;
  stores `p4` at `dst+0x9C+l*0xC`, arg pair at `+0xA0/+0xA4`, with `l=(p5+((p2*3+p3)*2))`);
- `0x1404F60C0 CALL 0x140A40FE0` → `FUN_1408C6430(dst+0x8E08,slot,rec)` (row clear:
  `rec==3 → rec=*dst`; `rec<3&&slot<4` → `dst+(slot+rec*4)*0x1C+0x3BE`, else `dst+0x3BE`),
  then zero 0x40 bytes with `+0x2C=0xFF, +0x38=0xFF` sentinels (`0x1404F60EC MOV [RAX+0x2C],0xFF`).
- `FUN_1404FDC80` (fallback, 36 insns): `1F9C3B0/23E7FD0` virtual gates, then
  `A74E60 + 4FF7D0` sample and `4FABD0` gate; commits `T+0x6D1C` when `ECX-T+0x6D1C>=0xF`.

`FUN_140500B90` (step-25 41-row bulk publish, 352 insns) re-read end to end:
- Two plan-source arms (roster `A55CF0`+`A66F50` at `0x140500BE6/0x140500C03`, direct `A67010` at
  `0x140500CD4`), each with the `0xF8/0x7F` card filter and `0x2E`-class test, then the 41-iteration
  (`EDX=0x29`) 0x80-stride block copy `T+0x8E08 → [RSP+0x30]`, then the commit pair:
  `0x140500E92 CALL 0x1408C5E20` ([RSP+slot],R8B) + `0x140500EAA CALL 0x1404175A0` (T+0x8E08,[RSP]).
- `FUN_1408C5E20` (7 insns): `if (p2<3) *p1=p2` — plan-index vote. CLOSED.
- `FUN_1404175A0` (0x45D-byte body): 0x9C-word + 0x2D-u16 + 0x45-u16 bulk row publish
  (decompile: three counted loops 99/0x2D/0x2D + tail). CLOSED to contract (exact field map: INF detail,
  not blocking — the stores are verbatim copies `[dst+i]=[src+i]`).
- Change record: `0x140500EFF MOV [RBX+0x6D06],1` + `+0x6D08:=EBP` + `+0x6D0C:=EAX`
  (re-check arm at `0x140501016–032` clears when `EBP/EDI` drift ≥0xF). `T+0xBCC` ladder
  (`0x140501076`, cases 0/1/2/3+) CONFIRMED as surveyed.
- SheetTriple M1 (`FUN_1408C50E0`): `(p4,p2=slot,p3=sub)` → `p1+((p4*0xB+p2)*3+p3)*3+199`
  (`p4==3 → p4=*p1`; `p4<3&&p2<0xB&&p3<3` else `p1+199`). `FUN_1408C5010` = M1 check
  (`*(p1+0x1465)`). 9 callers of `8C50E0` (all outside the tick: `4A8930/4F5F10/51CF70/51CE50/…`);
  the tick-side caller `0x1404F5F9D CALL 0x1408C50E0` passes `(RCX=R15=T-side?, EDX=ESI=slot,
  R8D=EBP, R9D=3)`: **R9D=3 forces the `*p1` (head-word) record** — so each side's plan record is
  the head word of its own `T+0x8E08` block (record = `*T+0x8E08`, slot = side slot, sub = EBP).
  M1 verdict: CLOSED (head-word select, not an open source).
- Status: **port-ready**: commit pair + fallback + BCC ladder + change record all closed;
  no live read needed. SheetTriple has nothing left here.

### T8d — PORT-READY (orders CLOSED as counter-increments, B6DC writer found, tick-reachable)
- `FUN_140A40800` (9 insns): `if (p2<0x45A) (*(p1+0xA574+p2*4))++` — a per-order fire counter.
  The three 40e sites (`0x14057C301 EDX=0xB1`, `0x14057C332 EDX=0xB3`, `0x14057C36F EDX=0x172`)
  therefore only bump `T+0xA574+{0xB1,0xB3,0x172}`. Port spec delta: no order payload to port —
  the "orders" ARE the counter bumps; consumers read the counters (e.g. restart code).
- `FUN_140A42A30` (4 insns): `*(u8*)(p1+p2*0x40+0x288)=p3` — run-order byte. CLOSED.
- `FUN_140A62EA0`: `(p2<2) ? A3B320(A489D0(G), hi32(p2)) : 0xFF` — id test. CLOSED.
- `T+0xB6DC` writer: THREE writers, full-image byte-anchored owner scan (13 raw hits → 9 in-code):
  1. `0x140A4168A MOV [RBX+0xB6DC],ESI(0)` in `FUN_140A415A0` (T-init, called from `FUN_140A40580`
     reset; covers B6DC/B6E0/B6E4 + the B8A2/B8A4/B8AC/B8B4/B8BC/B8E4/B8E8/B8EC/B9xx family);
  2. `0x140417184 MOV [RDI+0xB6DC],AL` (copy loop `0x140417176–1C5`: copies B6DC–B6E0+ bytes
     RSI→RDI; called from `FUN_142155AA0/14214CC10/142160350` — match-setup, outside tick);
  3. `0x140501845 MOV [RAX+0xB6DC],CL` (bytes `88 88 DC B6 00 00`) in `FUN_140501260`
     (tick-reachable: called from the tick at `0x1404F6D0A`), gated on the 0x5D card arm:
     `A65DD0` record decode (`*p=float u16, p[1]=byte/127.0, p[2]=(u32>>0x18)&0x7F`) with
     `b32==0x5D` then `*(T+0xF3)=1, *(T+0xF4)=rec[8], *(T+0xFC)=rec[0xC], *(T+0xB6DC)=local_200`.
- Status: **port-ready**: 40e's state rewrite + order counters + `T+0xB6DC` tick writer all closed.
  The setup-time copier (writer 2) is init, not a tick input. No live read needed.

### T10c — PORT-READY (row-base identity CLOSED)
- `FUN_140A408C0` args at all 3 call sites (`0x1404F7A50`, `0x1404FA6F9`, `0x14051E2A3`):
  `RCX=[frame+0xBB8]` (heap row-table pointer), `RDX=[frame+0x6D48]` (M),
  `R8D=[frame+8]` (side), `R9=[frame+0x6D50]` (T). Inside: `R13=R9` used at
  `0x140A40A6B CMP [R13+0x8E00],EAX` (plan-index test) and `0x140A40AA4 … [R13+0x254]`
  (latch float): confirms R9 = T (T+0x8E00 plan word, T+0x254 latch).
- Row base = the `H+0xBB8` heap table (one per H-frame = per match side-context; H created by
  `FUN_14041E4D0` with 0x6DF8-byte alloc, side contexts by `FUN_1404F0530`).
  Rows written by `FUN_14051E400(H+0xBB8+0x2EE,side,…)` (RNG-seeded triple via `FUN_140A928A0`:
  `*(p1+8+idx*4)=value`, 3 ids) and consumed by the `A408C0` lerp
  (`dst+=(src-dst)*rate`, `rate=[RCX+0x3F88]`, `×100.0=[0x14253EE30]/÷` normaliser).
- Destination `FUN_140A422C0` (T+0x3A9C/T+0x3B20 slot copies, stride 0xC) DOC as surveyed.
- Status: **port-ready**: base = `*(H+0xBB8)` (new GF state: one row-table per side, written by
  the 51E400 triple with GF RNG, read by the lerp). No live read needed.

### T6a — INERT IN PES (no pusher in image; verdict: do not port until live shows count≠0)
- Full-image byte-anchored owner scan for `0x24D50` (7 raw hits): exactly ONE reader
  (`0x140537AE0 ADD RBX,0x24D50` + `[RBX]/[RBX+4]` count/list reads in `FUN_140537AC0`),
  one reset (`0x140A6F38F MOV [RDI+0x24D50],RAX(0)` + `+0x24D54:=ESI(0)` + `+0x24D58/5C`,
  in `FUN_140A6F240`, called once from `FUN_1404636B0` — match setup), and one vcall slot
  (`0x14326E5CF CALL [RAX+0x24D50]` — vtable slot, not a data write). Neighbouring fields
  `+0x24D60/64/68/6C` have live writers (`0x140A71DA1/71DB4/71DBE` — different feature),
  but the kick-list head/count (`+0x24D50/54`) has NO pushing store anywhere in the image:
  no plain disp32, no split displacement (`ADD r,0x8C00/0x8B00/0x8D00` sweeps: none),
  no REP MOVS, no LEA-taken bulk copy.
- Gate (re-verified `0x140537AE3–BBB`): `count=[RBX+4]`, `count==0 → ret`; `list[0]>0x15 → ret`;
  side fold; `count==2` required (`0x140537B12–B15`); `T+0x130:=[list]` via `A429A0(0x1E)` path.
  Caller: only `0x1404F6FB9` (tick step 29). Live (menu): `MA+0x24D50/54/58/5C = 0/0/0/0`.
- Status: **inert in PES exhibition unless a live match shows count≠0** — port is a 1:1 gate
  (`count==2 && side-own && list[1]<0x16 && pl+0x5D0!=0x1E → T+0xA2A0[slot]=0x1E, T+0x130=list[2]`),
  wired behind the (always-false-here) count gate. Live recipe if ever needed: with a live
  exhibition running, `pread MA+0x24D50` (8B), `+0x24D54` (u32) each second through a restart;
  a nonzero count names the pusher via live watch, not static.

### T8b — STILL BLOCKED (live-only): readers CLOSED, zero static writers
- Object pinned: T-side (`dec_4f8c70.txt:312 lVar15=*(param_1+0x6d50)` = T; line 315–316:
  `T+0x8C1F+slot` byte rows, `T+0x8CE8` word). Readers CLOSED (3 sites in `FUN_1404F8C70`:
  `0x1404F9308 CMP [RAX+RSI+0x8C1F]` + `0x1404F9312 CMP [RAX+0x8CE8],0x11`, plus the
  `0x1406682D7/E1` and `0x140614BDA` AI twins).
- Writers: full-image byte-anchored owner scan — `0x8C1F`: 9 raw → 3 in-code, ALL `CMP`
  (readers); `0x8CE8`: 36 raw → 19 in-code, ALL `CMP`/`MOVZX` (readers, values
  `{0xB,0xC,0xD,0xF,0x11,1,3,7}`); neighbours `0x8C2A/0x8D28`: readers only.
  T-reset (`A40580/A415A0`) does NOT initialise the range; T-copy (`A42A70`) copies
  `T+0xA2CC` rows, not `0x8C1F`; no split-displacement `ADD r,0x8Bxx/0x8Cxx` exists.
- Status: **still blocked (live)**: the writers are runtime AI-side (match AI fills them;
  no static store). Live recipe: live exhibition, `pread T+0x8C1F` (11B) + `T+0x8CE8` (u16)
  per tick (T = `*(G+(s+0xA1)*0x28)` per probe.py); first nonzero names the set tick, then
  a live watch on the writer. Port stays parked (cost skeleton without inputs keeps budget 50).

### T12b — STILL BLOCKED (live-only for the 48 row contents); selector + tests CLOSED
- `FUN_140579E10` (112 insns): 0x30-entry computed jump over 48 stacked row pointers at
  `param_1+{0x20,0x60,0x90,0x5A8,0xC8,0x108,0x140,0x178,0x1B0,0x1E0,0x500,0x260,0x2A8,
  0x540,0x568,0x6D8,0x3D0,0x400,0x440,0x488,0x4D0,0x2E8,0x5D0,0x610,0x640,0x668,
  0x340,0x388,0x698,0x318,0x700,0x730,0x748,0x760,0x778,0x7A0,0x7B8,0x7D8,0x7F0,
  0x808,0x820,0x838,0x868,0x880,0x898}` (`EDX>0x2F → NULL`). Call protocol CONFIRMED from
  `FUN_140579840` (decompile + disasm): `row=[579E10(ctx,concept)]`, then
  `[row+0x8](row,M)`, `[row+0x10](row,M,T)` + `A42A40` take, `[row+0x18](row,M)` (+A3E0 stop),
  `[row+0x28](row,M,T,R9)` commit, `[row]→[R14+0x8/0x10]` copy arms, `B6E8/B718` stamps.
- Base object: `param_1 = &H+0x62B8` (tick `LEA RCX,[RDI+0x62B8]`), i.e. 48 inline structs on
  the per-match H object (0x6DF8 alloc). H-init (`4F07A0`) covers `+0x2EE(BB8 rows)/+0x300/
  +0x3DE/+0x107E/+0x18AB/+0x62AD(identity stub)` but NOT the `+0x62D8..0x6B50` row vtables:
  no init function writes 48 RIP-relative vtable pointers there (full LEA/owner sweeps clean).
  The rows are match-setup state; their vtable pointers are runtime heap values.
- Status: **still blocked (live) for the 48 row payload contents only** — everything else
  (selector, offsets, protocol, `57A160` pick test, `57A3E0` stop, `57A1F0` head) is CLOSED.
  Live recipe (one match init or live tick): `H=*(0x1436F3F98)` (vtable `0x142597580`),
  read 8B at `H+0x62B8+off` for each of the 48 offsets → vtable addrs → bridge-decompile
  each vtable's `[+8/+0x10/+0x18/+0x28/+0x30]` methods. Until then the selector ports behind
  a `// ponytail:` stand-in naming `FUN_140579E10`.

## §2 Port spec deltas (ready slices)

- T4b: `PlanCommit(slot,rec,arg6,arg7,oddbit,u6,u7)`: gate `MI&&row&&slot`; `A489D0(G,arg4)` →
  `A3AEE0` (0xFF keeps row); `A5BAE0(dst,T_id,rec,arg6,oddbit,u6,u7; MI+0x1198)`; then
  `A40FE0(T,slot,rec)` + zero row (`+0x2C/+0x38=0xFF`). `PlanIndexVote(id,val<3)`.
  Caller (`PublishSidePlan`): two source arms → 41×0x80 block copy → vote+publish pair →
  `T+0x6D06/08/0C` change record → `8C5010` M1 check → BCC ladder. M1 =
  `record *T+0x8E08 head word` (R9D=3 arm), slot, sub=EBP.
- T8d: `OrderFire(order){ if(order<0x45A) T+0xA574[order]++ }` for orders `{0xB1,0xB3,0x172}`;
  `RunOrderByte(slot,v)`: `T+slot*0x40+0x288=v`; `IdTest`. `T+0xB6DC` tick writer (in `501260`):
  gated on the 0x5D card arm (`b32==0x5D` from `A65DD0` rec; `rec[8]<0x16`): sets
  `T+0xF3=1, T+0xF4, T+0xFC, T+0xB6DC`. No new GF state beyond the counters + `T+0xB6DC`.
- T10c: new GF state = `H+0xBB8` row-table per side (written by the 51E400 triple with GF RNG,
  read by the lerp `dst+=(src-dst)*[H+0xBB8obj+0x3F88]` with `×100/÷100` normaliser).
- T6a: `KickSeed`: `if(count==2 && sideOwn(list[0]) && list[1]<0x16 && tgt+0x5D0!=0x1E)
  { T+A2A0[list[2]%0xB]=0x1E; T+0x130=list[2] }`. GF: same gate over the (empty) kick list;
  inert until count≠0 observed live.

## §3 Slice status updates

| slice | old (survey/small-decodes) | new |
|---|---|---|
| T4b | blocked (commit payloads + sheet-plan M1) | **port-ready** (all payloads closed; M1 = head-word select) |
| T8d | blocked (40800 orders + B6DC writer) | **port-ready** (orders = counter bumps; B6DC writer tick-reachable via 0x5D arm) |
| T10c | blocked (row-base identity) | **port-ready** (base = `*(H+0xBB8)` heap table; writer + protocol closed) |
| T6a | blocked (kick-list writer) | **inert in PES** (no pusher in image; gate never passes; port the gate, park the push) |
| T8b | blocked (8C1F/8CE8 writers) | **still blocked (live)** — writers are runtime AI-side; exact live recipe §1 |
| T12b | after-small-decode (48 payload rows) | **still blocked (live)** — contents only; selector/offsets/protocol closed; exact live recipe §1 |

## §4 Open items (numbered, with cost)

1. T8b writers — live exhibition read of `T+0x8C1F` (11B) + `T+0x8CE8` (u16) per tick; first
   nonzero → live watch. Static is exhausted (full-image owner scan: 0 writers).
2. T12b row contents — one live read of 48 qwords at `H+0x62B8+off` (H from
   `*(0x1436F3F98)`); then ≤48 bridge decompiles of vtable methods (batch; ~2 requests each).
3. T6a liveness — one live-exhibition second-half read of `MA+0x24D50/54`; if nonzero, pusher
   hunt resumes live; if zero across a full match, lock in as inert (no port action ever).

## §5 Checker

`checker.py` (scratch): 6 float rows + 10 opcode/site rows + 1 byte row; `RESULT OK`.
Mutations (each reverted): float `100.0→100.5` fires `BAD float 0x14253EE30`;
site `…,CL→…,DL` at `0x140501845` fires `MISS`; bytes `…00→…01` at `0x140501845` fires `BAD bytes`.
All dumps + `rd.py` + `checker.py` in `<workdrive>/gf-BlockedSlicesRE-scratch/`.
