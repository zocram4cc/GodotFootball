# Foundation inventory — widen pass (everything EXCEPT animation/anime, keeper, referee, kick/shot)

Owner: FoundationWiden. Read-only round. Seeds: decoded-but-unported PES fns from docs/re + local reports,
plus one call level out from ported fns. Round-1 rows (teamplayer 81 rows / matchball 54 rows) are cited,
never duplicated. Boundary with FoundationAreas: no new rows on anime bytes, keeper tables, judge/advantage,
PASS_GET arbitration, kick-error inputs (cited as deps only).
Conventions: T=`FUN_140a48a30(G,side)`, PI=`FUN_140a489f0(G,id)` (leaves' record A), PA=`FUN_140a48910(G,id)`
= hu, PM=`FUN_140a48a10(G,id)`, MI=`*(G+0x270)`, MA=`*(G+0x18)`, gs=54. OBS/INF per decompile.
Decompiles saved in `<workdrive>/gf-FoundationWiden-scratch/f*.c` (bridge, image VAs).

## 0. Ranked structures (new only; by pending gates unblocked)

| rank | structure | fields | gates unblocked | size/live |
|---|---|---|---|---|
| 1 | W-SEL: selector pins + claim records (sel+0x288/0x28c, 0x280/0x290/0x294/0x298, ring reader) | 5 | Counter/SecondLine/Diagonal admission, mask rank>1 sub-tests, sub-kind stops | M, N |
| 2 | W-AIM: pass-aim extension (PI A+0x60/0x1414/0x141c, G+0x180 route table, MA+0x23f7c, MA+0x2470c) | 6 | stop leaves 5a7fe0/5a7be0/5a7d10, 503430 base/k1/k2, reaction-delay writer A | M, Y(2) |
| 3 | W-SUP: support-work block (T+0x48f4/48fc cells, 0x4ec zone, 0x7b4/0x7c4/0x7c8, 0x7e0/0x7e4, 0x7da0, 0x7fb8/0x89e8/0x8b1c/0x3f04, c134/c13c/c140) | 9 | support search 5e9f50, designated supporter + distance, zone hysteresis | L, Y(3) |
| 4 | W-GRID: support grid MA+0x26020 (32×70×45) + build/clear sites | 3 | support cost 5e05c0/5e0180, interception layers | L, N |
| 5 | W-PRESSX: press-writer I/O extension (T+0xa55c arrival, T+0xb709/0xb70c/0xb712/0xb715 concept bytes) | 5 | press writer 536fc0 wiring (extends round-1 T-PRESS/T-CONC) | M, N |
| 6 | W-CHOOSER: reaction-chooser I/O (cmd+0x874/878/87c/880/fd5, ScriptBind tables, BallInfo+0x2920, pc record) | 4 | chooser caller a06dd0 port (5 ported entries' gating) | M, N |
| 7 | W-TMISC: team misc (T+0xb6f6, T+0x[slot+0x5e9]*0xc lane-x, T+0x40?, T+0x44?) | 4 | Overlap stop, Counter depth test, 593bd0/5a5400 geometry | S, Y(2) |

Totals: 6 new structures, 36 new rows: faithful 0 / stand-in 0 / absent 33 / pure-helper (no state) 3.
Effort: S 22 / M 10 / L 4. Needs live read: Y 7 / N 29.
Extensions to round-1 structures (counted above in W-PRESSX/W-TMISC placement): T-CONC +5 rows
(b709/b70c/b712/b715/b6f6), T-PRESS +1 row (a55c).

## 1. Field table

`field | meaning (OBS/INF) | PES writer VA(s) | PES readers (ported/pending) | GF status + symbol | blocking items | size | live?`

### W-SEL: selector pins + claim records

| W-S1 | sel+0x288 (u32 runner pin; Counter writer 60bdd0 writes id, 0x14060bdd0) | OBS | writer FUN_14060bdd0 @0x14060bdd0 (`*(param_1+0x288)=id`, f60bdd0.c) | readers: Counter mask 60ab00, Diagonal score head (sel+0x288 set → 100), 695800 lane sweep | absent (no selector record in GF; `runs` map only) | Counter/Diagonal admission | S | N |
| W-S2 | sel+0x28c (u32 runner pin; Counter writer 60b590 writes id, 0x14060b590 head `*(param_1+0x28c)=0xff` init) | OBS | writer FUN_14060b590 @0x14060b590 (f60b590.c) | readers: Counter mask, 60d210 (`+0x28c==id` → record-type-9 path) | absent (same) | same | S | N |
| W-S3 | sel+0x280/0x290/0x294/0x298/0x2a0..0x2c4 pins + claim gates 5ab180/5abcc0/5ab380/5ab990 (cnt<45, sec>=50/70/10) | OBS layout (attack-leftovers §1 dispatch; run-targets §2 lane-budget), INF thresholds | writers: selector take path (692cf0/claim gates; bodies walked, chance-lanes §2.5) | readers (p: 59ba60 budget, 664ee0 commit; P: 66d720 only) | absent | lane-budget + commit parity | M | N |
| W-S4 | state-ring reader FUN_140a401c0(T+0xa2a0+0x34·slot, 0/1) + rank>1 angle sub-tests 5a8290/5a5400 | OBS (chance-lanes §2.1: 5a8290 reads T+0xb8b8 + ring, `(end-cur)>11f` test; tail lane sweep cones 5/-14/10/70/2 + b748==0x30 + 655660) | writer: 42940/40200 ring push (press-layer-port) | readers (p: 694320, mask rank>1 tests) | absent | mask sub-tests | M | N |
| W-S5 | run sub-kind T+0x28c+0x40·slot (slot record byte; ==2 gate in 691300, ==0x13/7 early-outs in 60b590 loop) | OBS (chance-lanes §3.4; f60b590.c `+0x28c` compares) | writer: take path copies executor link byte (OffBallSelectors-inventory relay) | readers (p: 691300 sub-2 stop, 5a8290 sibling tests) | absent (no RunRecord sub-kind) | SecondLine sub-8, sub-2 stop | S | N |

### W-AIM: pass-aim extension (extends round-1 P-AIM; PI A = `FUN_140a489f0` record)

| W-A1 | PI A+0x60 (f32; mover-block value read in 503430 tail `*(lVar7+0x60)`, base lVar7=a489f0) | OBS addr, INF meaning | writer NOT FOUND (no scan run for +0x60; disp below useful floor near mover block) | reader: 503430 k-tail (p) | absent | writer-A k terms | S | Y (log A+0x60 per carrier live; reason: meaning ambiguous) |
| W-A2 | PI A+0x1414/0x141c (f32 pair; route/mover point read in 503430 `pfVar9=A+0x1414`, `*(A+0x141c)`) | OBS addr, INF meaning | writer NOT FOUND | reader: 503430 intersection test (p) | absent | same | S | Y (same read; reason: pair semantics ambiguous) |
| W-A3 | G+0x180 24-entry route table (stride 0x74; +0x20 u32 owner id, +0xd flag byte) | OBS (5a7fe0 loop over 0x18 entries; 503430 55d40/479470 probes) | writer NOT FOUND (route builder open) | readers: 5a7fe0 keep, 503430 bVar2 branch (p) | absent | stop leaves + writer A | M | Y (reason: writer unknown; confirming value = entry +0x20 holds receiver id q with +0xd==0 at a keep) |
| W-A4 | MA+0x23f7c angle table (f32 per ordered pair `(q*0x16+id)*4`; 5a7fe0 tail `MA+0x23f7c+(q*0x16+id)*4` vs a48910+0x11c bearing) | OBS addressing+use | writer NOT FOUND | reader: 5a7fe0 tail (p) | absent | stop leaf tail | S | Y (reason: writer unknown; confirming value = entry tracks bearing(q→id) within ~5°) |
| W-A5 | MA+0x2470c per-team mate list (side block; 5a7d10 `MA+0x2470c` mate enumeration, `|fold(bearing(c→j)-act+0x11c)|<25`, d²<400) | OBS use, INF layout | writer NOT FOUND | reader: 5a7d10 aimed-to-me STOP (p) | absent | carrier-aim stop | S | Y (reason: layout INF; confirming value = 11 ids of the side, stable across ticks) |
| W-A6 | MA+0x247d4 already round-1 C2 (cited, not duplicated). 5a7fe0's `MA+0x247d4+id*4` bearing read is the C2 reader. | — | — | — | — | — | — | — |

### W-SUP: support-work block (extends round-1 T-CONC/T-SHAPE family; bases on T unless noted)

| W-P1 | T+0x48f4+0xc·slot support cell (f32 x) / +0x48fc (f32 z); open-play store, off-play mirror at support+0x3f04 | OBS (f5e9f50.c:210,349; 695800 reads T+0x48f4/48fc) | writer FUN_1405e9f50 @0x1405e9f50 (f5e9f50.c) | readers: 695800 Diagonal body, slot targets param_4 (p) | absent | support search + Diagonal | M | N |
| W-P2 | T+0x4ec+0x14·slot zone rect (centre+half-sizes; supporter 10×10 m) | OBS (f5e9f50.c:349 `param_1+0x4ec+slot*0x14`) | writer: supporter-target 5ebca0 zone step (body open) | reader: 5e9f50 cell search (p) | absent | zone hysteresis | S | N |
| W-P3 | T+0x7b4 designated-supporter slot (u32; `*(local_248+0x7b4)==id` fast path) | OBS | writer: designated supporter 5eb980 (ported fn, pin write not ported) | readers: 5e9f50 every-call research (p); P: ProcessDesignatedSupporter sticky (own pin) | stand-in candidate only (GF pin exists, PES write site not wired) | supporter admission | S | N |
| W-P4 | T+0x7c8[11] research counters (u16; +1/call, reset at 5, research when `< iVar20`) + T+0x7c4 current slot | OBS (f5e9f50.c) | writer: 5e9f50 itself | reader: 5e9f50 cadence (p) | absent | search cadence | S | N |
| W-P5 | T+0x7e0/0x7e4 settle stamps (u32 frames; `10 < now-stamp` re-search gate) | OBS (f5e9f50.c:397,401) | writer open (set when mate settles; not traced) | reader: 5e9f50 (p) | absent | hysteresis | S | N |
| W-P6 | T+0x7da0[slot] second metric (f32; `FUN_1405df5a0 ≤ metric+3` hysteresis) | OBS use | writer: 5df5a0 write path (open) | reader: 5e9f50 replace test (p) | absent | replace hysteresis | S | N |
| W-P7 | T+0x7fb8/0x89e8/0x8b1c per-slot flags + support+0x3f04 off-play cell mirror | OBS addrs | writers open | reader: 5e9f50 (p) | absent | search parity | S | N |
| W-P8 | T+0xc134: LIVE-CORRECTED constant 255 even in possession (LiveTeamPlayer-2) — NOT the passer slot; 5ebca0 takes the else-branch (supporter keeps AI target +0x12b0) in the observed windows | OBS addr live | — | reader: 5ebca0 (constant input in observed play) | absent (constant 255; GF keeps AI target) | supporter target | M | done (re-open if a non-255 window is ever logged) |
| W-P9 | support object +0x6c5[c] flag, +0x7ed4/+0x80a0 slot words (f2/flag inputs to cost 5e05c0) | OBS addrs, INF meaning | writers: base-adjustment pipeline 5d8b70/5d1810 (41-entry; bodies saved f5d8b70/f5d1810, per-entry reduction open) | readers: 5e05c0/5e0180 cost (p) | absent | support cost flags | M | Y (reason: 41-entry pipeline unreduced; confirming value = +0x6c5 stable per player across a possession) |

### W-GRID: support grid (extends round-1 MA cluster; build OBS off-ball §4.2)

| W-G1 | MA+0x26020+0x62700·k grid (32 layers × 70×45; layers 0-10 mate presence, 12-22 screens, 24 opp, 27-31 intercept) | OBS (off-ball §4.2; 5e05c0/5e0180 readers f5e05c0/f5e0180) | writers `FUN_140a72a70` sites 0x14045906a/0x1404596e1/0x1404599cf/0x140459c82; clear 140a72150 (4 sites); layers 11/23/25/26 never written | readers: 5e05c0 cost, 5e0180 opp term (p) | absent | support cost | L | N |
| W-G2 | grid build cadence (possession team only via T0+0x255; step=MI.frame%11; 5f-ahead prediction) | OBS (off-ball §4.2) | writer: step driver 0x140458b71… (census open) | reader: grid freshness (p) | absent | grid parity | S | N |
| W-G3 | support-zone/adjust pipeline drivers 5d8b70/5d1810/5eb0c0/5db000/5d4b00/5e6e80 (41-entry base-adjustment pipeline) | OBS existence+order (off-ball §4.1; callees: 5d8b70→{5a4790,a62180,a63680,a63950,a64030,a627e0…}, 5d1810→{a63950…}, 5eb0c0, 5db000→{…12 callees}, 5d4b00, 5e05c0, 5e0180) | writers: team tick (placement open) | readers: all support state (p) | absent | whole support positioning | L | N |

### W-PRESSX: press-writer I/O extension (extends round-1 T-PRESS T+0xa548/a558/a53c/a540/a50c + T-CONC)

| W-R1 | T+0xa55c[4] press arrival records (from FUN_140535260 wrapped accumulator; written at 536fc0:195,328-330) | OBS | writer FUN_140536fc0 @0x140536fc0 (f536fc0.c) | readers: press loop (p) | absent (GF PressPickInput has no arrival array) | press wiring | S | N |
| W-R2 | T+0xb709 concept byte (PRESS-vs-DELAY branch `*(local_240+0xb709)`, f536fc0.c:288) | OBS addr | writer open (concept start region; 57a3e0 family) | reader: 536fc0 output path (p) | absent | press writer | S | N |
| W-R3 | T+0xb70c zero-gate byte (`T+0xb70c==0` required, f536fc0.c:150) | OBS | writer open | reader: 536fc0 gate (p) | absent | press gate | S | N |
| W-R4 | T+0xb712/b715 arm bytes (`f1||b712||b715` body gate, f536fc0.c:137) | OBS | writers: event-walk 501283/501468 set on event-type-7 (press-layer-port pass 8); ctor clear | reader: 536fc0 (p) | absent (GF has presserFlag=f1 only) | press gate + type-7 mapping | S | N |
| W-R5 | T+0x78 fallback id already round-1 T+0x78 (cited). 536fc0 head returns early unless `T+0x78<0x16` — the round-1 row's reader. | — | — | — | — | — | — | — |

### W-CHOOSER: reaction-chooser caller I/O (extends round-1 A7 busy + E-refs; anime predicates = Areas-owned, cited)

| W-C1 | cmd record +0x874 (AI id) / +0x878 (anime) / +0x87c (=0x18 prio) / +0x880 (taken) / +0xfd5 (block-clear) | OBS (fa06dd0.c table walk) | writer: chooser FUN_140a06dd0 @0x140a06dd0 + entry fns | readers: command build 9f4db0 (p) | absent (GF ChooseReaction returns enum, no cmd record) | chooser port | M | N |
| W-C2 | ScriptBind tables @0x14263ec60 (m=0, 17 entries) / 0x14263ed70 (m=1) / 0x14263edb0 (m=3) / dodge table @0x14263eca0 (16-B records) | OBS (fa06dd0.c; defender-decisions §1; net-dodge §49) | writer: static (reaction.json bind) | readers: a06dd0 walk (P: 5 entries' order; p: 12 skipped entries + m=1/3 lists) | stand-in (GF walks 5-entry order; tables not loaded) | reduced lists, dodge entries | S | N |
| W-C3 | BallInfo+0x2920 mode byte (==4 replay/special arm in bVar12 branch, fa06dd0.c:32) | OBS | writer: engine (ball mode) | reader: a06dd0 gate (p) | absent | chooser gate | S | N |
| W-C4 | pc record +0x38 (E-store holder: `*(pc+0x38)+0xe0` = E) / pc+4 (player id) / reaction.json+8 thinkWaitTimer (flagA skip) | OBS (fa06dd0.c) | writers: per-tick pc rebuild 928fc0 (open) | readers: a06dd0 mode/table walk (p) | absent | chooser cadence | S | N |

### W-TMISC: team misc

| W-T1 | T+0xb6f6 zero-gate byte (Overlap stop `+0xb6f6==0`, f6013d0.c) | OBS | writer open | reader: Overlap stop 6013d0 (p) | absent | Overlap stop | S | N |
| W-T2 | T+0x46EC+12·slot lane-x float (596470 depth test vs s·target+8, c596470.c:27; base T via a48a30; decompiler renders the index as slot+0x5e9) | LIVE-CORRECTED (LiveTeamPlayer-2, pes-re-live-teamplayer.md): symmetric, NOT slot-ordered — team 0 kickoff [0, 9.73, -8.43, 23.36, -23.36, 0, 10.38, -10.38, 5.19, -5.84, 0] | readers: 60bdd0/60b590 Counter writers (p) | absent | Counter depth gate | S | done |
| W-T3 | T+0x40: LIVE constant 0 (LiveTeamPlayer-2) — reader 593bd0 takes a constant; no writer needed | OBS addr live | — | reader: 593bd0 (constant input) | absent (constant; GF passes 0) | Counter/593bd0 | S | done |
| W-T4 | T+0x44: LIVE constant 2 (team 0) / 255 (team 1) (LiveTeamPlayer-2) | OBS addr live | — | reader: 5a5400 cone test (constant input) | absent (constant; GF passes side-dependent const) | mask sub-test | S | done |

### Pure helpers (no new state; recorded so seeds close)

| W-H1 | FUN_140a8ff10 capsule (pure geometry; fa8ff10: no struct reads) | OBS | — | reader: PostPlay lane gates (p); P: pesreceive capsule | — (pure; GF capsule covers) | — | — | N |
| W-H2 | FUN_140a4e720 cone wrapper → a4e3f0 (pure angle folds) | OBS | — | readers: 5a8290/5a5400/5a5240 tails (p); P: partial (pesteamconcepts ponytail) | — (pure) | — | — | N |
| W-H3 | FUN_140a62610 first-flagged-man scan (reads T+0x4c00 flags + MI+0x1748; fa62610) | OBS | — | reader: via a62690 (P: PressExcludedMan) | faithful scan rule (GF PressExcludedMan) | — | — | N |

### Seeds with no new fields (all reads already rows; edge-only)

- FUN_140a62960 (0x3a-state scan): reads T+0xa2a0 (round-1) only → no row.
- FUN_14060d210 SecondLine score: reads sel pins (W-S1/S2/S5), a50250-holder (round-1 P-IMG), a71500 rank table (round-1 C1), 5c0f00 _good (no state), a64030 skills (round-1 P-record) → no row.
- FUN_1406013d0 Overlap stop: reads T+0xb824 (round-1), T+0xb748 (round-1), 5a4830 run-state (ported), a4c9a0 line test (ported), a76240 route (ported) + W-T1 → edge only.
- FUN_140695800 Diagonal body: reads T+0x48f4/48fc (W-P1), MA ball (round-1 B1), T+0x78 (round-1), a50250 (round-1), a63950 instr (round-1 T-INST) → edge only.
- FUN_1405a5400: reads team-base +0x44 (W-T4), player pos (round-1 P-KIN), MA line (round-1 D1) → edge only.
- FUN_140a06dd0 caller: MI+0x116c/0x1775 (round-1 E1/E9), PM+0x5d0 (round-1 P-KIN), W-C1..C4 → edge only.

## 2. Seed → round-1-structures edge list (pending gates get their deps)

| seed | round-1 structures needed (cite, not duplicate) |
|---|---|
| 60bdd0/60b590 Counter writers | T-POSS (255/254), T-LVL (b8a4 via 60ab00 cap), T-CONC (b748), T-SHAPE (c48/S, 4c00), T-PRESS (a548/a558), MA B-cluster (2482c/34, 2483c), MA C1 rank, MA D-line (44), MI E1/E9/E5(11a0), P-KIN pos/facing, P-AIM (1490/1494), T-INST (a63950) |
| SecondLine mask/score (60ebf0/60d210) | T-POSS 255, T-CONC b748, P-IMG holder rec+0xd4 (a50250), MA C1, T-INST, sel pins W-S |
| Diagonal select/score/stop (614290/613910/615090/695800) | MI E1, T-POSS 255, T-SHAPE (4c00/c48), T-CONC b748, P-KIN, MA B-cluster, W-P1 cells, T-INST |
| Overlap select/stop (601580/6013d0) | T-POSS 255, T-CONC b748, T-PRESS b824, P-KIN, MA B-cluster, MI E1 |
| PostPlay mask/stop/lanes (620590/620900/61f940/61f8b0/620ca0/6210e0 + a8ff10/a62610/a62690/a62960/a4e720/a4e120) | T-POSS 255, T-CONC b748, P-IMG holder, P-KIN, MA B-cluster + C1/C3, MI E1, T-SHAPE 4c00/slotPoint |
| 5a8290/5a5400 mask sub-tests | T-CONC b748/b8b8-class (500200 mask), T-SHAPE, P-KIN, T-INST, W-S4 ring |
| Chance stop leaves 5a7fe0/5a7be0/5a7d10 | P-AIM all rows, MA C1/B-cluster, T-INST, P-KIN, W-A3/A4/A5 |
| 503430 writer-A base/k1/k2 | P-KIN (55c/564/554/5d0), P-AIM (38/45/46), MA B-cluster, MI E-ring (a3e190), W-A1/A2, G+0x180 (W-A3) |
| 4f4be0 chase predicate + leaves (4c8e60/44e020/4c8ea0/43a0a0/a4f1e0) | T-POSS 255/254, T-SHAPE 4c00, MA B-cluster + C1/C3, MI E1/E9/1748-block, P-KIN, ring A1 (newest) |
| 536fc0 press writer + inputs | T-PRESS all rows, T-CONC (f1/b709…), T-SHAPE 4c00, P-KIN/PA+1, MA B-cluster, MI E1/1748-block, ring A1/A7, W-R1..R4 |
| a06dd0 chooser caller (+table 63eca0) | MI E1/E9, PM+0x5d0 (P-KIN), ring A1, W-C1..C4; anime predicates → Areas |
| 5e9f50 support search + 5e05c0/5e0180 cost | T-POSS 255, T-LVL b8a4, MI E1/1198-frame/11a0, MA B-cluster, P-KIN, P-IMG holder, T-INST, T-SHAPE, W-P1..P9, W-G1..G3 |
| 41-entry pipeline (5d8b70/5d1810/5eb0c0/5db000/5d4b00/5e6e80…) | T-CONC, T-SHAPE, T-POSS, P-KIN/P-AIM, T-INST, W-P9 flags, W-G1 grid |

## 3. Closure statement

- New structures: 6 (W-SEL, W-AIM, W-SUP, W-GRID, W-PRESSX, W-CHOOSER) + 1 misc bucket (W-TMISC). No further
  structure appeared while writing edges: every seed's reads landed in round-1 rows or the 7 groups above.
- One-more-level sample (5 fns called by seeds, decompiled c596c30/c599d90/c596470/c593bd0/c5a5240):
  only 2 candidate-new fields (T+0x40 in 593bd0, T+0x44 in 5a5400 — both single reads, semantics open;
  596470's 0x5e9-table and 5a5240's MI+0x1554 were already captured/cited). New-field rate ≈ 0.4/fn,
  all S-size with open semantics.
- Verdict: NOT CLOSED. The list is finite and trustworthy for the seed set, but one more level out still
  yields ~0.4 narrow rows/fn (team-record byte/float reads whose writers need per-function scans). The
  remainder is bounded: small-displacement T-record reads, not new structures — a further level is
  expected to add rows at the same trickle rate and zero new structures, with the support pipeline's
  41 entries as the largest unreduced block (W-G3/W-P9).
- Live-read handoff (orchestrator rule — addresses only, no self-read), routed by area:
  - agent://LiveTeamPlayer (T+/PI fields): PI A+0x60 + A+0x1414/141c per carrier (expect stable mover values; reason: meaning ambiguous); T+(slot+0x5e9)*0xc (expect slot-ordered x); T+0x40 / T+0x44 single reads (semantics open); T+0xc134 (expect passer slot in possession); support +0x6c5[c] / +0x7ed4 / +0x80a0 (expect stable per-player flags across a possession).
  - agent://LiveMatchBall (G globals): G+0x180 24×0x74 rows (+0x20 = receiver id with +0xd==0 at a keep).
  - agent://LivePoke (MA list): MA+0x2470c (expect the side's 11 ids, stable across ticks); MA+0x23f7c+(q*0x16+id)*4 (expect bearing(q→id) ±5°).
- MA status this match: RELOCK FAILED (LivePoke full streamed scan, 0 candidates; old MA stale, ball copy 20.74 m off). MA-based rows (W-A4/A5 and all MA-cited readers) are STATIC-ONLY this match until a new lock lands. G/MI reads unaffected.
- Live corrections (new match): G+0x2aa8 is NOT null in valid windows — alternates 0 / 0x1e00e454 once per sample (earlier "null live" was a sampling artifact; relayed by LiveMatchBall-2). No Chance (0x1e) team-state seen yet (states 0/54 only), so G+0x180 capture still pending.
- W-A3 partial (run 5, LiveMatchBall-2): G+0x180 rows logged at +0x10 only (0..23 = 0, 1, 0xFFFFFFFF); +0xd/+0x20 + Chance states NOT in log. Full table capture needs one more sampler run (accepted below).
- W-A3 run 6 NEGATIVE (LiveMatchBall-2, 1500 s, 797.7 s in play, out/lmb-widen-6.jsonl): 134 team-Chance(0x1e) episodes (median 1.3 s); 288 readable rows during Chance, ALL (+0xd,+0x20)=(0,255) — no receiver-id row. Either the 5a7fe0 keep is not team state 0x1e, or +0x20 is not the receiver id. OPEN: needs a run tagging the 5a7fe0 keep directly (offer accepted pending).
- W-A3 run 7 NEGATIVE (LiveMatchBall-2, out/keep-7.jsonl, 1500 s all phases): G+0x180 rows NEVER changed — one start snapshot, all (0,255), 0 keep candidates in 25 min. Two readings: sub-0.1 s write (invisible at 10 Hz) or the keep does not write this table in this build. CLOSED as static-only: next step is a static writer search for the +0x20 field (stride 0x74), not more sampling. Details: local/pes-re-live-matchball.md.

## 4. Counts

Structures: 7 groups (6 + TMISC), 36 rows: faithful 0 / stand-in 0 / absent 33 / pure-helper 3.
Sizes: S 22 / M 10 / L 4. Needs live read: Y 5 (W-A1/A2/A3/A4/A5, W-P9) / settled-live 4 (W-T2 corrected symmetric lane-x, W-T3 const 0, W-T4 const 2/255 by side, W-P8 const 255 not passer slot — LiveTeamPlayer-2 pes-re-live-teamplayer.md). Unmeasured: W-A1/A2 (carrier untrusted), W-P9.
Seed→structure edges: 13 seeds × rows above.
