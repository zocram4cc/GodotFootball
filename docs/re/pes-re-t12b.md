# PES RE T12b — concept-row table: corrected layout, live reconciliation, port spec

> **Correction (ConceptIndexRE, `pes-re-concept-index.md`):** the row labels 0x15-0x2D in the §3 table (and `rows_live.txt`) are each one low: the NULL arm 0x14 was conflated with true row 0x15 (+0x488), and true row 0x2E is missing. The suspected Chase/Press index mismatch with GF does not exist: GF's ids 0x21-0x25 bind PES's rows 0x21-0x25 exactly. True row 0x15 was never read live.

Title: `FUN_140579E10` is a row-address selector over 46 inline concept objects, not 48 row pointers; the live capture read the wrong base (missing `[H+0x450]` deref). True rows re-read live: all 46 vtables + methods captured.

## §0 Script-read float/constant table (image `pes_full.bin`, offset = VA − 0x140000000; pasted tool output)

```
0x142588200 = 0.10000000149011612 (T12b 579840 select epsilon at 0x1405799F0 MOVSS XMM7,[0x142588200])
0x142565600 = 0.5 (T12b addend at 0x1405799F8 MOVSS XMM8,[0x142565600])
0x142565D30 u64 = 0x7fffffffffffffff (T12b mask at 0x1405799E4 MOVSD XMM6,[0x142565D30]; lo=-1i32 hi=0x7FFFFFFF)
```

Operand-VA pairing (next-IP + disp32, verified from image bytes):

| instruction | next-IP + disp | target |
|---|---|---|
| `0x1405799E4 MOVSD XMM6,[RIP+0x1FEC344]` | `0x1405799EC+0x1FEC344` | `0x142565D30` |
| `0x1405799F0 MOVSS XMM7,[RIP+0x200E808]` | `0x1405799F8+0x200E808` | `0x142588200` |
| `0x1405799F8 MOVSS XMM8,[RIP+0x1FEBBFF]` | `0x140579A01+0x1FEBBFF` | `0x142565600` |

Row-offset table (from `das_579e10.txt`, 112 insns; index → `base+off`; arms `0x14`,`0x2F` are `NULL`):

```
00:+0x0 01:+0x20 02:+0x60 03:+0x90 04:+0x5A8 05:+0xC8 06:+0x108 07:+0x140
08:+0x178 09:+0x1B0 0A:+0x1E0 0B:+0x500 0C:+0x260 0D:+0x2A8 0E:+0x540
0F:+0x568 10:+0x6D8 11:+0x3D0 12:+0x400 13:+0x440 15:+0x488 16:+0x4D0
17:+0x2E8 18:+0x5D0 19:+0x610 1A:+0x640 1B:+0x668 1C:+0x340 1D:+0x388
1E:+0x698 1F:+0x318 20:+0x700 21:+0x730 22:+0x748 23:+0x760 24:+0x778
25:+0x7A0 26:+0x7B8 27:+0x7D8 28:+0x7F0 29:+0x808 2A:+0x820 2B:+0x838
2C:+0x868 2D:+0x880 2E:+0x898
```

## §1 Corrected layout (CONFIRMED)

- `FUN_140579E10(base, concept)` (VA `0x140579E10`, 112 insns, `das_579e10.txt`) never
  reads memory: it returns `base + OFF[concept]` (`LEA`s) for 46 concepts, `NULL` for
  `concept > 0x2F`, with `local_198[0x14] = 0` and `local_198[0x2F] = 0` as the two NULL
  arms (`dec_579e10.txt`). It is a row-address selector, not a pointer table: there are
  no pointers stored at `base+OFF` to read. CONFIRMED (disasm + decompile agree).
- Each row is an inline polymorphic object: `row+0` = vtable pointer; methods at
  `+0x8/+0x10/+0x18/+0x20/+0x28/+0x30`. Call protocol CONFIRMED from the three callers:
  `FUN_140579840` uses `+8` (test), `+0x10` (take-arm), `+0x18` (stop-arm), `+0x28`
  (commit) (`dec_579840.txt`); `FUN_140579BF0` (VA `0x140579BF0`, via tick
  `0x1404F7691`) and `FUN_14057A1F0`/`FUN_14057A3E0` families (callers `0x14057A255`,
  `0x14057A40E`) call `[row+0x20]` after the same `579E10` select.
- `base` is **not** `H = *(0x1436F3F98)`. The tick `FUN_1404F6C30` receives
  `RCX = [H+0x450+side*8]` — the two `0x6DF8`-byte per-side objects allocated and
  `FUN_1404F0530`-initialised at match setup (`0x14041E62A EDX=0x6DF8`,
  `0x14041E657 [R14+RAX*8+0x450]`, two iterations; tick fetch chain `0x14041EDC0
  RCX=[RSI+RBX+0x450]` with `RSI=R14−(RSP+0x20)`, i.e. `[H+0x450]`/`[H+0x458]`).
  The tick passes `RDI+0x62B8` (LEAs at `0x1404F7654`/`0x1404F7691`) to `FUN_140579840`
  / `FUN_140579BF0`. Only these two LEAs reference disp `0x62B8` in the image
  (56 raw hits collapse to the pair). CONFIRMED.
- Stride: rows are variable-spaced (gaps `0x30`–`0x40` in the low block, `0x18` in the
  `0x700`–`0x898` block with `0x7B8→0x7D8 = 0x20`, `0x838→0x868 = 0x30` anomalies);
  each offset is a distinct field/struct start, not an array. Element type: inline
  vtable'd object (8-byte vtable at +0, payload `+8` qword / `+0x10` qword observed
  live). INF: full per-row struct sizes need ctor disassembly (no static writer scan
  found; the 5 `579E10` callers are all tick/event-path, none construct).
- `H`-init (`FUN_1404F07A0`) covers `+0x2EE/+0x300/+0x3DE/+0x107E/+0x62AD` but not row
  vtables — consistent: rows live on the children, constructed at match setup
  (writer = match-setup placement-new, OPEN exact site).

## §2 Capture reconciliation (CONFIRMED: recipe's base was off by one deref)

- The script read `H+0x62B8+off` with `H=0x27473670`, i.e. the wrong object: true rows
  are at `[H+0x450]+0x62B8` (`=0x2747A2E8`) and `[H+0x458]+0x62B8` (`=0x4287AEA8`).
  Its 45 columns (OFF48 has 45 entries: idx 1–13,15–46; concept 0 at `+0x0` and the
  NULL arms `0x14`/`0x2F` correctly absent, though the "48" count was wrong) sampled a
  different `H`-resident table, not the concept rows.
- Hence its values are **not** row contents: `(−1.0,−1.0)` pairs at capture cols
  3,10,13,14,20,22–25,28, `(0,−0.552624)` at col 15, `(255,255)` i32 pairs at cols
  40,41,44, zeros, and one live float at col 32 (`0xC22E65xx00000000`, f64 ≈
  −6.5278e10, drifting across the hour — the only changing column, 100 distinct
  values) are whatever lives at `H+0x62B8+off`. Reinterpretation as concept-row
  payload is INVALID. The "pointer offsets were off" exactly as suspected: one missing
  dereference, `H+0x62B8` vs `[H+0x450]+0x62B8`.
- Correct live read (pid 2510857, open play, `MI=0x0` menu-adjacent/warm state,
  `H=0x27473670`, `[H+0x450]=0x27474030`, `[H+0x458]=0x42874BF0`): **all 46 row+0 are
  valid image vtables** (`0x1425C3D00`–`0x1425C4840`, `.rdata`), identical for both
  children (0 diffs), full 7-method vectors captured to
  `<workdrive>/gf-T12bRE-scratch/rows_live.txt` (92 lines). E.g. row 0:
  `vt=0x1425C3D00 m=1404F0D50,14053D730,141863350,14053D600,14053D5F0,141794410,140CA1A50`.
- The `0x141863350` false stub recurs at `+0x10`/`+0x30` across rows — the same stub
  GF's port already treats as "interruptible/slot-6 false" (teamAIcontroller.cpp).
  Row payload `p8=0x0` for all rows; `p10` holds per-row nonzero tags. CONFIRMED live.
- Live-match caveat: read taken while `MI=0x0` (no open-play tick running); vtables are
  setup-state (stable), but per-tick payload/counts need a re-read in open play.
  T6a/T8b/step items from that capture are unaffected by this correction.

## §3 Port spec (corrected) + GF inputs (`src/sim` read-only)

Full measured table (idx → vtable → `[+0,+8,+0x10,+0x18,+0x20,+0x28,+0x30]`), live:

```
00 1425C3D00 4F0D50,53D730,1863350,53D600,53D5F0,1794410,CA1A50
01 1425C3D40 4F0D20,53DE40,53D8B0,53D780,53D760,53E940,1863350
02 1425C3D80 4F0C90,543F40,53F710,53F600,53E950,53FEF0,1863350
03 1425C3DC0 4F0CF0,543F40,540780,5404A0,540440,5415B0,1863350
04 1425C46C0 4F0DE0,543F40,577A10,577800,576A20,577C30,1863350
05 1425C3E00 4F0D20,543F40,1863350,543890,542D30,544B80,1863350
06 1425C3E40 4F0CF0,54C3D0,5465B0,545E50,544BB0,546FB0,1863350
07 1425C3E80 4F0CF0,54C3D0,5485F0,5483E0,547850,5494B0,1863350
08 1425C3EC0 4F0CF0,54C3D0,54AD10,54A8A0,54A130,54B560,1863350
09 1425C3F00 4F0C90,54C3D0,54C080,54BF80,54BD40,54C850,1863350
0A 1425C3F40 4F0CC0,54C3D0,54D9B0,54D4B0,54D0F0,54E480,1863350
0B 1425C4380 4F0D20,56FBB0,56C5E0,56C490,56BAF0,56CB00,1863350
0C 1425C3FC0 4F0DB0,56FBB0,551900,5516F0,5516C0,552E90,1863350
0D 1425C3F80 4F0D20,56FBB0,550520,550220,54F170,550D30,1863350
0E 1425C43C0 4F0DE0,56FBB0,56EC30,56E520,56D580,56F5B0,1863350
0F 1425C4400 4F0D20,56FBB0,1863350,56FA60,56FA10,56FDE0,1863350
10 1425C4000 4F0DE0,56FBB0,553C40,553B30,553300,5541E0,1863350
11 1425C4080 4F0C90,558A10,556C40,5568A0,556880,557DD0,1863350
12 1425C40C0 4F0D20,558A10,5585F0,558400,558320,55A060,1863350
13 1425C4100 4F0DB0,558A10,55BC60,55BAB0,55AE80,55C260,1863350
15 1425C4240 4F0C90,56B630,565150,564FB0,563F10,5654B0,1863350
16 1425C4040 4F0C90,56B630,555730,555220,554CA0,556380,1863350
17 1425C4280 4F0D20,56B630,566C10,5666E0,565AF0,567F90,1863350
18 1425C42C0 4F0C90,56B630,568BB0,5688A0,568510,569FE0,1863350
19 1425C4300 4F0DE0,56B630,1863350,56A7A0,56D580,56AA40,1863350
1A 1425C4340 4F0C90,56B630,56B350,56B240,56B220,56BAB0,1863350
1B 1425C4180 4F0DB0,1863350,55ED70,55E910,562490,55FC20,1863350
1C 1425C41C0 4F0DB0,53DE40,561150,560C00,560510,561BA0,1863350
1D 1425C4440 4F0D20,53DE40,570970,570420,570380,572000,1863350
1E 1425C4140 4F0DE0,53DE40,55D3C0,55D170,1794410,55E240,1863350
1F 1425C4480 4F0C90,53DE40,1863350,573040,5728C0,5738E0,1863350
20 1425C44C0 4F0D80,574910,5747F0,574710,1794410,1794410,1863350
21 1425C4500 4F0D80,574910,574EA0,574B00,1794410,1794410,1863350
22 1425C4540 4F0D80,574910,575300,5752A0,1794410,1794410,1863350
23 1425C4580 4F0DE0,574910,575510,575410,579680,575DA0,1863350
24 1425C45C0 4F0D80,5767F0,575EA0,575DB0,1794410,1794410,1863350
25 1425C4600 4F0D50,5767F0,1863350,575EE0,1794410,1794410,1863350
26 1425C4640 4F0D80,5767F0,1863350,576350,1794410,1794410,1863350
27 1425C4680 4F0D80,5767F0,1863350,576650,1794410,1794410,1863350
28 1425C4700 4F0D80,5788D0,5781D0,578170,1794410,1794410,1863350
29 1425C4740 4F0D80,5788D0,1863350,578870,1794410,1794410,1863350
2A 1425C4780 4F0C90,5788D0,1863350,578930,578900,578DB0,1863350
2B 1425C47C0 4F0D80,579650,578EF0,578E90,1794410,1794410,1863350
2C 1425C4800 4F0D80,579650,5794E0,579480,1794410,1794410,1863350
2D 1425C4840 4F0DE0,579810,1863350,5796A0,579680,1794410,1863350
(VAs `0x14…`; prefix elided. `1863350` = `0x141863350` stub.)
```

What GF carries (all paths `src/sim/onthepitch/`, read-only):

- Selector skeleton: `teamAIcontroller.cpp` `RunConcepts` ports `FUN_140579840`'s walk
  (in-play test per concept, one-attacking-concept gate, `57A3E0`-equivalent
  `StopConcept`), with named `// ponytail:` stand-ins where PES has no GF counterpart
  (registry-15 stop flag, difficulty row `0x21` cap, `MI+0x116C==2` early-out).
- Gate VAs (all CONFIRMED against the live table's `+0x8` slot):
  SideCounter `0x03`→`+8=0x140543F40` ✓ (`pesteamconcepts.cpp` `ConceptGate`);
  ForeCheck triple `0x21/0x22/0x23`→`+8=0x140574910` ✓; RetreatBlock `0x25`→`+8=0x1405767F0` ✓.
- Concept constants GF reads from the image with opcode-checked `Code` pairs
  (`pesteamconcepts.cpp`): `END_TEST_AFTER 0x142588200`, `CHASE_*` family
  (`0x142588210/0x142565604/0x14253EE30/0x14259BE5C/0x1425A2670/0x14259BE0C/0x14259BDF4/0x14259BE6C`,
  window `LEA ECX,[RAX+RAX*2]` at `0x1405755C3` = 3×fps), `COUNTER_*`/`RUNNER_*` families.
- ids: `pesteamconcepts.hpp` `kConceptCount=48`, `kLastAttackingConcept=0x20`,
  `kConceptSideCounter=0x03/ForeCheckPress=0x21/LineDown=0x22/LineUp=0x23/
  TransitionChase=0x24/RetreatBlockCreate=0x25` — GF ids ARE `579E10` indices.

Port deltas after this decode:

1. No new GF state: the rows live on PES's per-side `0x6DF8` children; GF's
   `conceptRunning/conceptOrders/conceptStart_ms` already mirror the selector outputs.
   The `// ponytail:` stand-in naming `FUN_140579E10` stays until the two INF items close.
2. INF (one live + bridge session): TransitionChase `0x24` alignment. Its measured gate
   `+8=0x1405767F0` (All-out Defence) contradicts GF's Frontline-Pressure grouping, and
   GF's ported "Chase start" `0x140575510` sits at idx `0x23 +0x10` while idx `0x24 +0x10
   = 0x140575EA0`. Sibling idx `0x20`/`0x21` share gate `574910` and GF's ForeCheck
   "start/continue" (`5747F0/574710`) sit at idx `0x20`, not `0x21`. Decompile `575EA0`
   vs `575510` (and `574EA0` vs `5747F0`) to decide variant-vs-off-by-one, then either
   move GF's constants/ids or document the sibling split. All functions confirmed
   present (prologue bytes read from image).
3. Recipe to close: in open play, `H=*(0x1436F3F98)`, `C=[H+0x450+side*8]`,
   `row=C+0x62B8+OFF[idx]`; vtable=`[row]` → bridge-decompile `[vt+8/+10/+18/+28/+30]`
   per row of interest only (not all 48). Row payloads `p8/p10` + `B6E8/B718` arms in
   the same read give the live take/commit state.

## §4 Status ledger

| item | claim | verdict | evidence |
|---|---|---|---|
| selector shape | `579E10` = LEA selector, 46 rows + NULL `0x14/0x2F` | CONFIRMED | `das_579e10.txt` (112 insns) + `dec_579e10.txt` |
| row type | inline vtable'd objects, methods `+8..+30` | CONFIRMED | `dec_579840.txt` + callers `579BF0/57A255/57A40E` + live vtables |
| base | `[H+0x450+side*8]+0x62B8`, not `H+0x62B8` | CONFIRMED | `0x14041E657`, `0x14041EDC0`, `0x1404F7654/91` disasm (bytes) |
| capture values | floats/ints at `H+0x62B8` are NOT rows; one-deref error | CONFIRMED | chain math + live re-read (46/46 valid vtables at true base) |
| `-1.0`/`-0.553` meaning | unknown `H`-region init pattern, not row state | INF | capture cols mapped §2; writer not hunted (superseded) |
| 46 vtables + methods | captured both sides, 0 diffs | CONFIRMED | `<workdrive>/gf-T12bRE-scratch/rows_live.txt`, live pread 24-10-26 |
| float table §0 | script-read from image | CONFIRMED | pasted output above |
| operand VA pairs | next-IP+disp arithmetic | CONFIRMED | §0 table |
| Chase/Press id alignment | `0x24` gate/start vs siblings | INF | §3.2; needs `575EA0`/`574EA0` decompiles |
| row struct sizes/payload | ctor sites | OPEN | no static constructor found; live `p8=0`/`p10` tags only |

## §5 Open items / recommended next step

1. Bridge-decompile `0x140575EA0` vs `0x140575510` (and `0x140574EA0` vs `0x1405747F0`):
   decides whether GF's `0x24`/`0x21` ports ride the right rows. ~4 requests, then a
   6-line GF `ConceptGate`/constants fix or a sibling-split doc note.
2. OPEN (cheap, optional): identify `H+0x62B8` region owner (the capture's float table)
   from `H` ctor; only matters if some GF import wants those floats.
3. OPEN: row ctor (placement-new site writing `0x1425C3D00…` into children) — needed
   only if GF ever constructs rather than selects rows.
