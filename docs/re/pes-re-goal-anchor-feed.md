# pes-re-goal-anchor-feed: the BC writer 0x1404B88B0 — inputs, writers, the BC rule, BC=2 on pid2024691, GF mapping, test facts

Branch context: `CutsceneLine` round 4 (`ee383c13`) left the kind-2 anchor case OPEN: GF takes the
cut-list builder's default arm (BC 8), PES's live goal (BC 2) the recompute `0x140471A80` else arm.
This report decodes the writer that produces BC, reads its inputs from the captures, maps each input
to GF, and gives the per-kind facts for red tests. No GF code changed (decode + mapping task).

Labels: CONFIRMED = run or read this session (replay, capture bytes, image bytes, Ghidra
disassembly). INF = inference. OPEN = named blocker. PES captures are PES-derived: scratch only
(`gf-GoalAnchorFeed-scratch/`, 25 MB), never committed.

## 0. Constants (script-read from `pes_full.bin`, file offset = VA − 0x140000000; pasted output)

```
0x14258c264 f=3.0  0x14259bdf0 f=7.0  0x14259bdf4 f=8.0  0x14259bdfc f=11.0
0x14259be1c f=20.0 0x14259d5d4 f=1.2  0x1425656c0 f=4.0  0x142565d30 u64=0x7fffffffffffffff (fabs mask)
0x14256ae1c f=1.0  0x142588218 f=-1.0 0x142588210 f=10.0 0x14259a764 f=60.0
0x1425aa1bc f=75.0 0x142572348 f=0.8  0x14259bde4 f=2.5  0x14253ee30 f=100.0
```

Operand-VA pairs (instruction → constant it reads; Ghidra disassembly + capstone byte check agree):
`0x1404B69D4`→[0x14256AE1C] 1.0, `0x1404B69DC`→[0x142588218] −1.0,
`0x1404B6A2C`→[0x14259BDF4] 8.0 (F1−8), `0x1404B6A35`→[0x14258C264] 3.0 (F0/3),
`0x1404B6A64`→[0x14259BDF0] 7.0 (dist gate), `0x1404B6AD1`/`0x1404B6AEC`→[0x142565D30] fabs,
`0x1404B809D`→[0x14258C264] 3.0 (clock×3, ball-dist<3), `0x1404B880B`→[0x14253EE30] 100.0 (8790 gate),
`0x1404B93E7`→[0x14259BE1C] 20.0, `0x1404B943C`→[0x14259D5D4] 1.2, `0x1404B9481`→[0x1425656C0] 4.0,
`0x1404B94BD`→[0x14259BDFC] 11.0, `0x1404B9500`→[0x14259A764] 60.0, `0x1404B952E`→[0x1425AA1BC] 75.0,
`0x1404B9417`→[0x142572348] 0.8, `0x141E532E0`+6 reads `[0x14351AE50]` (clock u32; `8B 05 …` verified).

## 1. The BC rule (CONFIRMED by decompile + asm + replay)

`0x1404B88B0(A0, ctx)` (single direct caller `0x1404B6335` in `0x1404B5FC0`, E8-scan CONFIRMED) sets
`A0+0xBC = 0x1404B7F70(A0, ctx, slot1, flag, 0)` at **`0x1404B89F9`** (`MOV [RDI+0xBC],ESI`;
ESI = its return, `0x1404B8926 CALL` / `0x1404B892B MOV ESI,EAX). param_5 = 0 at both
`0x1404B7F70` call sites (`0x1404B8917`, `0x1404B8B89` `MOV [RSP+0x20],0`). Main-path epilogue
stores (all byte-verified): `0x1404B89FF` C0=8, `0x1404B89DB`/`0x1404B8AEF` EA=1,
`0x1404B899D`/`0x1404B8AFB`/`0x1404B896A` A0+0x14=(clock×3)/[G+0x2C0+0x5C].
Other A0+0xBC writers (Ghidra + capstone agree): `0x1404723EC` (=8, in `0x140472300`),
`0x1404B6033` (=8, in `0x1404B5FC0`), loop `0x1404B8BFB`/`0x1404B8C20` (=6). Full-section
displacement sweep (10.5M insns, `.pdata`-anchored) found no other writer in a cutscene function.

`0x1404B7F70(A0, ctx, slot, flag, 0)` with `s1 = 0x140474C90(A0,ctx,1)` (= slot here), `s0` = flag-0
slot, `F` = field floats at `[G+0x2AA8]`, `P` = scorer pos (`[posrec+0x55C]`, `[posrec+0x564]`,
posrec = `0x140A48A10(G, slot)`), `man` = `0x140A48910(G, slot)`:

- Early 8: slot/s1 > 0x15; `A0+0x54+slot*4 == 6` (**`0x1404B7FDD`**); `man+1 == 'V'`; assist-man
  (`0x140A489A0(G, slot`))+0x48 == 6 (`0x1414C1300`); `A0+200 != 9` (`0x140478FE0`).
- 6-arm: `0x140471590` true, ball within 3.0 of `man`, `A0+0xAD == 0` (**`0x1404B80E0`**),
  (`slot != s1` or `0x1404770A0 != 1`), `A0+0x131 == 0` (**`0x1404B810D`**) → return 6.
- 7-arm: `slot == s1`, `A0+0x131 == 0`, `man+0x4C != 1`, `flag == 0`, `clock×3.0 < A0+0xDC`
  (**`0x1404B8139`**), `0x1404B8790(A0,ctx) != 0xFF` → return 7.
- Else `z = 0x1404B6960(scorer pos)`: dist vs 7.0 (**`0x1404B6A64`**): < 7 → 3;
  `|x| ≤ F0`: [¬(`F0−7 ≤ |x|` ∨ `|z| ≤ F1−8`) ∨ inside-box-quad (`0x140A901E0` true)] → **2**,
  else outside-quad → 8, or 5 if `|x| < F0/3`; `|x| > F0`: `|z| > F5=9.16` → (`|z| > F7=20.12`) ? 1 : 0,
  else 4. Merge (**`0x1404B8247`**): `A0+0x131 == 0` → return `z` (param_5 == 0).
  So with `A0+0x131 == 0`: **BC=2 ⟺ zone(scorer pos) = 2**, the only 2-source in the function.

Recompute gate (CONFIRMED asm): `0x140471A80` reads BC at **`0x140471ACB`**, tests
`(BC−6)&0xFFFFFFFD==0` (`0x140471AD2/AD5`, `JNZ 0x140471F6C` → else arm calling `0x1404B6BD0`
at **`0x140471F76`**). BC=2: `(2−6)&… = 0xFFFFFFFC ≠ 0` → else arm. Tick `0x14047B460`:
`0x14047B835` tests EA, **`0x14047B851`** calls the recompute, `0x14047B856` clears EA.

## 2. The captures (streamed with `capz`/`capread.py`; G = 0x2DB0C420, A0 = 0x31C53DB8, clock 54)

Two goals share A0/ctx. Goal 1: bcwriter seq003–006 (A0+0xDC = 1,2,3,4; entry BC 8) →
recompute seq203 (BC **2**, C0 8, EA 1, 14h 162, DC 200) → anchor seq204. Goal 2: bcwriter
seq456–459 (DC 1) → recompute seq628 (BC **2**, C0 8, EA 1, 14h 162, DC 172) → anchor seq629.
14h = 162 = (54×3)/1.0 with `[G+0x2C0+0x5C]` = 1.0 (CONFIRMED bytes). A0+0x24 = 2, +0xAC = 0,
+0xB0/+0x28 = 0xFF: the 6-loop extras are absent, so the footprint is uniquely the main-path
store `0x1404B89F9` with `0x1404B7F70` = 2 (the loop only writes 6).

Inputs read at the bcwriter entries (goal 1, seq003; goal 2 identical): slots s0 = s1 = 0
(MI+0x1588 = 0 → table path `0x140A62EA0(ctx, MI+0x15C8 = 0)`: side-0 table, key 0 found at index 0;
asm `0x140474CAB/CB4/CC7` CONFIRMED); A0+0x54+0 = 8; man+1 = 0x68, man+0x48 = 7, man+0x4C = 2;
assist-man+0x48 = 8; A0+200 = 9; scorer pos (−49.54, −4.04) (posrec 0x2DD7E354);
flag `0x140478C20` = 0 (G+0x180 entry 1 has +0x20 == 0, byte 0 == 0); F0 = 52.56, F1 = 34.06;
A0+0x131 = 0, +0xAD = 0. Zone eval: dist 30.2 ≥ 7, |x| = 49.5 ≤ 52.56, near-line true
(45.56 ≤ 49.5), quad-`0x140A901E0` false (coded float32 eval) → **8**. 7-arm closed (162 < DC
false at DC 1–4; and `0x1404B8790` = 0xFF: man+0x54 ≈ 0 < 11.0 kills `0x1404B9330`).

Replay proof (unicorn `run_recorded` on unpacked seq006 in scratch, deleted after):
bcwriter returns 0, writes nothing (BC 8, C0 2, EA 0) — the model reproduces PES on captured bytes.

Consequence: every captured pre-decision state gives 8, so the 2-selecting evaluation happened at
an un-captured tick (DC ∈ [5,171] for goal 1, [5,163] for goal 2). The captured scorer trajectory
(−49.5,−4.0 → −48.9,−3.0, always narrow + near-line → zone 8) excludes slot 0 at those states;
the decision needs zone-2 inputs — a scorer pos with `|x| < 45.56 ∧ |z| > 26.06` (both disjuncts
false), i.e. a wide-channel position, or a transient slot selection (MI+0x1588/0x1590 pair or
MI+0x15C8 side/key flip; all 0 in every capture). Which of the two differs at the decision tick is
OPEN: arm `bcwriter_4B88B0` with `--when` on its return (or a DC-windowed capture) to catch it.

## 3. GF mapping (`src/pes/fixdemo_select.cpp` lines; `fixdemo_select.hpp` for facts)

Have in PES layout: A0 defaults incl. BC/C0/DC/slot-kinds via translated `0x140478090` (PlaceGoal
:443–447; defaults `0x14047811E/128/14A` CONFIRMED); MI pair `0x1588/0x1590` = 1/scorerSlot (:426–427)
— note PES live holds 0/0 (table path, slot 0); MI side/key `0x15C8/0x15CC` (:428–429, 0/0 for a
slot-0 scorer = live); scorer pos/vel/facing/moveMode (:435–441, `kManPos` 0x55C ✓); field floats
(:430); bearings (:431–432); time scale 1.0 (:331–333); clock VA `0x14351AE50` = fps (:433–434,
live 54 = `kPesFps`); A0+0x131/+0xAD/+0xAC = 0 ✓ (live 0); A0+0x38 = T.z (:491–493) ✓.
Stand-in (named `ponytail:` in code): slot table G+0x180 = 0xFF (:312) — DIVERGES from this goal's
live table (entry 1 +0x20 == 0 → flag 0; GF's 0xFF → flag 1: changes the 7-arm gate); 0xA5
man +0x48/+0x4C zeroed (live 7/2; branch outcomes coincide: ≠6, ≠1); 0xD9 assist table = GF run
table (run+0x48 = 0 vs live 8; outcomes coincide); presence/history minimal; MI+0x116C = 0 vs live 1
(both keep `0x140A63FB0` false).
Missing: the bcwriter + recompute calls themselves (PlaceGoal :422–530 runs 0x140478090 →
optionally 0x140477BC0 → 0x140474CF0 → 0x140476C50 → 0x140474F00/0x478FF0 → 0x14047A710; no
`0x1404B88B0`, no per-tick EA-gated `0x140471A80`); ball object G+0x40 (6-arm needs ball pos);
`ctx+0x18 → +0x22E4C` slot tables (8790 helper); A0+0xDC incrementer (counts 1..4 → 172/200;
no store in bcwriter/caller/recompute/tick — OPEN); MI+0x1588 dword writers (candidates
`0x1422A4010/5696/5ED0`, base-unattributed — OPEN).

## 4. Test facts per BC kind (for red tests; captured values where they exist)

- BC 8: slot 0, man+1 0x68, assist+0x48 8, A0+200 9, scorer (−49.54,−4.04), F0 52.56/F1 34.06 →
  zone 8 (replay-PROVEN: seq006 rax 0, BC stays 8). GF already produces 8 (defaults, no writer).
- BC 2: needs zone 2 (`|x| < 45.56 ∧ |z| > 26.06` at F0/F1 above, dist ≥ 7) with A0+0x131 = 0,
  then BC/C0/EA/14h = 2/8/1/162 and recompute else-arm (T from `0x1404B6BD0`, radius `0x141F5EC60`).
  Post-state pinned from seq203/628 bytes; the 2-selecting scorer pos is OPEN ( §2).
- BC 6: ball < 3.0 from scorer + `0x140471590` true + A0+0xAD = 0 + (slot≠s1 or `0x1404770A0` ≠ 1)
  + A0+0x131 = 0. No capture (needs a close-ball decision tick) — OPEN oracle.
- BC 7: slot==s1 + A0+0x131 = 0 + man+0x4C ≠ 1 + flag 0 + `clock×3 < A0+0xDC` (e.g. 162 < 200 at
  seq203) + `0x1404B8790` ≠ 0xFF (needs man+0x54 > 11.0; live ≈ 0 → FF). No capture — OPEN oracle.
- BC 5: zone near-line-or-narrow, outside-quad, `|x| < F0/3` (rule-derived; e.g. |x| < 17.52 here).
- BC 3: scorer within 7.0 of the (F0·sx, F1·sz) corner. BC 4: past-line (`|x| > F0`), `|z| ≤ 9.16`.
  BC 0/1: past-line, `9.16 < |z| ≤ 20.12` → 0, `|z| > 20.12` → 1 (rule-derived; D/F are 1/0).
- Recompute arm: BC ∈ {6,8} (+ A0+0x24 == 1 for 6) → default rebuild; else (`BC = 2` here) →
  `0x1404B6BD0` list + `0x141F5EC60(A0+8)` radius, A0+0x10 = 0. GF needs the EA-gated per-tick call.

## 5. Open items (numbered)

1. Decision-tick inputs for BC=2 (zone-2 scorer pos or transient MI pair/side flip, DC 5..171):
   needs a `bcwriter_4B88B0` capture armed on its return value, or a DC-windowed hook.
2. A0+0xDC incrementer (1,2,3,4 → 172/200): no store in writer/caller/recompute/tick.
3. MI+0x1588/0x1590/0x15C8 writers: which of the 14/13 store sites targets MI and when the pair flips.
4. No captured 6/7 decisions: 8790-needs-man+0x54 > 11 oracle, close-ball oracle missing.
5. GF feed (round 5): run translated `0x1404B88B0` + EA-gated `0x140471A80` in the arena; replace the
   G+0x180 0xFF stand-in, add ball + `+0x22E4C` tables, transient-pair handling. Coordinate with
   CutsceneTiming (`gf_match.cpp` trigger/feed owns playback timing).
