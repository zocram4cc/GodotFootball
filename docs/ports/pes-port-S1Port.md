# S1Port wrap-up for the batch merge (head `62e534c`)

> **Main audit 19-10-26:** `0x14063AA79` `movzx ecx,byte [rax+0x31]`, then `divss [0x14253EE30]` (100.0) and `comiss` against `movss xmm0,[rdi+0x44]` at `0x14063AA74`: matches (P+0xA1 given RAX = P+0x70, the agent's reading). Correction: OneTwo is not held by G3 (ported since A4LaneInputs); per A4Level's probe it dies at G12.
> **Main audit 19-10-26 (round 2, `90c94a3`):** `FUN_140587E70` calls `0x140A714C0` (Safety gate 4 reads the score row): confirmed. Safety now dies at gate 5 (backward mates; PES candidate set not compared). B+0x4C writer open (handed to RE for a live watch).
> **Main audit 19-10-26 (round 3, `666328b`):** the ball-track arm is AND, confirmed in raw asm `0x14063AAFA` `cmp [r12+0x4c],r13b; je skip` / `0x14063AB01` `test r14b,r14b; jne skip` / `0x14063AB06` `movb [rdi+0x6e],1`. Safety candidate set = PES receiver status 0x0B (0x142117C30). Units still silent.
> **Main 19-10-26 (wrap-up, `62e534c`):** lane stack ready for batch merge. Correction: ThrowInTaker did NOT port the momentum exemption (no MergeRA copy); only FirstTouchPort 54413ac and this branch carry it.

State: `origin/S1Port` = `62e534c` (ls-remote confirmed). Worktree clean, no probes in source or docs (marker grep = 0). Nothing new to push this round.

Smokes (clean tree, seed 7):
- Loop, 2 min, `--pes-loop`: exit 0, full time at 43016 ticks, `[determinism] 3d92d259fe45c5dd`, `[idle] 31 episodes, stalls 14`.
- Off, 2 min: exit 0, full time at 24090 ticks, `[determinism] 75a11cdb2b3c8ec5`, `[idle] 19 episodes, stalls 12`.
- Both hashes unchanged from the last green round.

## 1. Per-unit table (20-min seed-7 match, `62e534c`)

Counted with temporary funnel probes (all removed). Two runs: 516 s and 309 s wall; both `[  PASSED  ] 1 test.`. "Calls" is the number of rethink calls that reached each unit (the same 102 for every lane unit).

| Unit (kind) | Calls | Answers | Top killing gate (count) | PES-exact or upstream-starved |
|---|---|---|---|---|
| Long (9) | 61 | 61 | none (head and target gates 0 kills) | not a lane unit |
| Cross (7) | 102 | 2 | `laneOpen` = `carrierPressureClass >= 2` (100 of 102 calls) | **starved**: `elizacontroller.cpp:2615` stand-in; the lane test 0x14063CD90 is not wired |
| GoalFront (2) | 102 | 0 | per-mate `difficulty <= gfDifficultyMax` (314 of 601 mate tests); lane-free 28 calls; central 2 | **PES-exact** gate; input is receiver record A +0xF0 (no stand-in marked) |
| BackSpace (3) | 102 | 1 | decisive: curve/corner gate (176 of 177 mates reaching it); per mate, validator 666 of 843, with near-box gate 9a 210 | **PES-exact**: curve builder matches writer 0x142138220 (verified this round); near-box constants from the install. **Starved**: plan-4 arm is silent (0 of 101 calls): `elizacontroller.cpp:2810` (slot-10 plan target not carried). The 3 no-curve kills are "no opponent listed" (PES would keep the old table; GF has none) |
| Safety (6) | 102 | 0 | per mate (672): ShortLimit block 289 (of which the limit itself), turn arm 87, ceiling 174, keeper pre 24, field pre 21, distance 21, G1/pressure 45, class 5 | ShortLimit and ceiling **PES-exact** (ported from raw asm); lane gate (22 calls) **starved** by the carrier-facing lane heading stand-in (`elizacontroller.cpp:2782`); through path (56 calls) gated by stand-in pressure counts (`elizacontroller.cpp:2881`) |
| OneTwo (10) | 102 | 1 | OneTouch refused 88 of 94 calls: G12 band 35, score 23, close zone 15, lane free 15. Receivers (43): difficulty band 16, window 12, P2 keeper 5, openness 5, M[r][c] 4 | G12 band **PES-exact** (curve verified; silent because carriers stand 25+ m behind the defensive line in midfield). Score (S1 row), zone and lane core **PES-exact**. **Starved**: P2 role class (`elizacontroller.cpp:3022`, 5 kills); M[r][c] bearing stand-in (`pescarrier.cpp:2105`, 4 kills) |
| DirectHighBall (11) | 102 | 0 | volatile gate: 102 of 102 calls | **PES-exact** silence: `pes::VolatileBall` (pescarrier.hpp:1241) is a real port of arms A and B. No ball above the carrier's head in 102 calls, so it is silent. The older "K+0x6E stand-in" note is stale. Height input is the S13 stat read (`elizacontroller.cpp:3077`) |

Reading: the decisive gates are PES-exact (curve, near box, ShortLimit, G12 band, difficulty). Starvation is confined to a few wired-but-stubbed inputs: Cross `laneOpen`, the BackSpace plan-4 target, the Safety lane heading and pressure counts, and the OneTwo P2 role class and M-table bearings.

## 2. Stand-ins on the lane stack (name, file:line at `62e534c`, PES site that removes it)

Lane stack = files touched by the S1Port and A4Level commits: `pescarrier.{cpp,hpp}`, `peslane.cpp`, `pesdodge.{cpp,hpp}`, `elizacontroller.cpp` (lane wiring), `humanoid.cpp` (momentum exemption). `pesscorerows.*`: no stand-in markers.

### pescarrier.cpp
- atan2 for bearings: 106 → PES atan2 polynomial 0x140A92DC0 (lane-core O3)
- sin/cos in AdvancePoint: 115 → PES sine table 0x140A92C80
- dribble exponent curve 3+3u: 146 → the prior report's raw call (not re-verified)
- threat o* nearest point on path: 174 → the threat report's path parameter (not located)
- owned-opponent angle factor a_o = 1: 191 → the report's a_o (not located)
- dribble search 72 headings × 15 steps: 202 → cost ceiling; PES heading resolution (not located)
- breakthroughCap 70: 534 → 0x14211aa7e
- middleShotFloor −5: 535 → 0x14259be7c
- middleShotStatGain 15: 536 → 0x14259be0c
- longStatGain 10: 537 → report 4.4 P table (not located)
- throughClassOpen 4: 538 → report 2.6 (`rec.class < 4`)
- wide-shot evaluator and central_eval not ported: 659–660 → 0x142120e60, 0x14211fe20 (report 9.1)
- rolled-plan cancel test: 996–997 → plan vtable[3] (not decoded)
- plan mode defaults (Long, SideChange, PassThrough, Basic): 1031–1034 → 0x14211e140, 0x14211e160, 0x14211dc00
- plan modes 1/7, 3, 4, 5/6, 8: 1048–1051 → 0x14211E773, 0x14211E930, 0x14211E9B8, 0x14211EA5E, 0x14211E874 (plan-record fields)
- instant-return window 2.0 s: 1176 → PES 2·int(54) frames (carunits5.c:421; GF is in seconds)
- exit B facing (lane entry record facing): 1512–1514, 1517 → 0x140A48A10 (player record facing)
- exit B heading headQ = 0: 1515 → the entry's vel_eff heading (not located)
- ctx[0]+0x40 flag (min-|z| arm): 1659–1660 → the flag's writer (helpers §5, undecoded)
- IntoSpaceTarget penalty-line re-place not ported: 1704–1705 → 0x140A4DDC0
- Safety per-slot stat = 0: 1917 → imageA+0x898 (0x142137F90; v_c at 0x142138CF0 open)
- ClosingCount flag-1 per-side check: 2050–2051 → 0x140A48910 record filter
- TouchReceiverOk angle bearing for A_tab: 2085 → T18+0x22F7C (lane-units2 O9, writer open)
- M[r][c] bearing: 2105 → T18+0x23F7C (O9, writer open)
- M[c][r] bearing: 2109 → the M table (O9, writer open)
- BuildCurve record filter not applied; equal-key tie rule: 2151–2152 → 0x140A48910, 0x141E53540 (tie rule)
- DHB low-ball history position: 2283–2284 → 0x140A76240 (S17)
- DHB lane advance angle table: 2327 → 0x140A71260 (S10, writer open)
- UnitArmed touch-window frames, turn veto, restart veto: 2350–2351 → not located
- FirstBounceSample vertical velocity from height difference: 2564–2565 → the record's vec[4] (sampler 0x140874100)

### pescarrier.hpp
- ShootSubKind rows and skill bits: 524–525 → PES shot sub-kind rows (not located)
- mateLaneOk: 588–589 → 0x140a4c8e0 (not decoded)
- LongVetoed signed-X test: 713–715 → 0x14211a5bb..a5e4 with pitch row; veto 0x14211a570
- plan keep-running (cancel test): 775–778 → vtable[3] (as 996)
- nearestOppD2: 1091 → 0x140587E70 (team-table d²)
- TouchReceiver statusValid: 1163 → 0x142118D20 (S5)
- ownLine_m: 1185 → T18+(sd+3)·0x14 (S14)
- pitchBound_m: 1186 → F+0x18 (S8)
- markCount: 1197 → AI rec2 +0x12A4 (S11)
- flag15: 1199 → 0x140A62180 (S18)
- carrierAngle_deg: 1212 → K+0x34 (S9)
- ballFlag80: 1228 → B+0x80 writer (undecoded)
- publishedFlag: 1303 → anime record publish word hu+8 (no GF source)
- DHB tableAngle: 1342 → 0x140A71260 (S10)
- firstTimeWhileReceiving: 1353 → AI rec+1 bit 5, 0x140A6E5E0 (S12)
- carrierHeight_m: 1354 → AI rec2+0x70+0x31 (S13)
- teamLevelRaw (stock default, live level writer open): 1187 → 0x140A424E0 setter, 0x140A40BD0 reader, 0x1B5D8340 writer (G3)

### peslane.cpp
- atan2 for lane bearing: 52 → 0x140A92DC0 (spec O3; same as pescarrier.cpp:106)

### pesdodge.cpp / pesdodge.hpp (touched by the lane stack; not lane units)
- post grid [2][2] stand-in 0x316: cpp 190, hpp 57–58 → the exe table's ninth int (the post grid read)
- frame-parity and AI-action special cases: cpp 375–376 → 0x2f8 and 0x2ff (clip frame parity and AI action id)
- movement-state branch (+0x5b0), speed floor: cpp 515–516, 575–576 → +0x5b0 movement state
- outside branch anime state byte: cpp 544–545 → hu(i)+0x4c
- designated man, T+0x78 only: cpp 599 → MA+0x2470c table

### elizacontroller.cpp (lane wiring)
- Cross laneOpen = carrier pressure class: 2614–2615 → 0x14063CD90 (lane core call); 0x140474C40 (ball→carrier predicted) also not ported
- lane heading = carrier facing (BackSpace/Safety/GoalFront): 2673–2674, 2782–2783 → 0x140A71260 table writer (pes-re-lane-structs §4)
- humanCarrier (GoalFront): 2679–2680 → team mode record +0x6C
- team 0x21D8 flag and 0x2128 entry: 2684–2685 → same, the human-carrier angle raise
- mate statusValid (b6c): 2698–2699, 2994, 2965–2967, 3019 → 0x142118D20(team+0xB6C, slot%11)==0xB (S5)
- mate pressure entry: 2701–2702 → 0x14069A3E0(team+0x2124)[+0x10] (meaning open)
- danger (imageA+0x31C1): 2778–2779 → 0x140699960 (not ported)
- BackSpace carrierSlotStat: 2790 → imageA+0x898 (0x142137F90)
- BackSpace carrierNeed: 2791 → 0x1404C8E00 (team-table need)
- BackSpace sideAngle identity: 2792 → team+0x250 attack direction (identity read)
- team-table lines (BackSpace and Safety): 2794, 2837–2839 → T18+0x40 (team lines)
- BackSpace receiving: 2802 → ctx4+0x6F
- BackSpace humanCarrier: 2803 → ctx4+0x6C
- goal mouth (BackSpace, Safety): 2805, 2880 → imageA+0x31C0 (0x140699960)
- plan-4 target and bytes: 2810–2813 → FUN_14211CB00(plan, 10) (slot-10 target)
- box scalars zero: 2821–2822 → imageA+0xB44/0xB48/0xB3C/0xB40
- Safety plan heading: 2832 → ctx7+0xC0 (plan heading)
- Safety pressure counts: 2881–2885 → 0x14069A500 counts (+0x30/+0x34/+0x38/+0x20)
- Safety oppPressPair: 2888 → T18+0x2470C (meaning open)
- OneTwo ownLine: 2903 → T18+(sd+3)·0x14 (G1, S14)
- OneTwo pitchBound: 2904 → F+0x18 (G2, S8)
- OneTwo teamLevelRaw: 2905–2907 → 0x1B5D8340 writer, 0x140A424E0 setter (G3)
- OneTwo attackLevel = 0: 2910 → T+0xB8A4 (G8, S4)
- OneTwo oneTwoRunEvent, runCount: 2916–2917 → event record and one-two run (S7)
- OneTwo markCount = 0: 2918 → AI rec2 +0x12A4 (G10, S11)
- OneTwo flag15: 2920 → 0x140A62180 (S18)
- mate team angle: 2969 → 0x140A71260
- mate lineValue: 2970 → 0x14061E7C0 (team-table line value)
- mate pressureClass: 2977–2978 → 0x14069A500 (+0x10)
- mate slotStat = 0: 2979 → imageA+0x898
- mate zone build: 2980–2982 → 0x14063E270 (0x140698340/830), 0x14063E010
- Safety nearestOppD2: 2996 → 0x140587E70 (team-table d²)
- OneTwo receiver roleClass: 3020–3022 → T(side)+0x4BD4[r%11] (S6, writer open); CarrierRoleClass stands in
- OneTwo receiver oneTwoRun = false: 3023 → player(r)+0x5D0 == 0x25 (P3)
- DHB markCount = 0: 3040 → AI rec2 +0x12A4 (S11)
- OneTwo carrierAngle: 3067 → K+0x34 (S9)
- DHB firstTimeWhileReceiving: 3076 → AI rec+1 bit 5 (S12)
- DHB carrierHeight: 3077 → AI rec2+0x70+0x31 (S13)
- wide-side latch (plan record +0xD8/+0xD9, mode +0xC4, slot-10 target, imageA+0xB44, cross-line test): 3089–3092 → those fields
- FSM byte MI+0x1775: 3118 → writers undecoded
- designated kicker MA+0x7C: 3119 → accessor (pessetpiecetaker)
- tier gate off: 3128 → live tier 5 (zero delay)
- forced-clear opponent count: 3129–3130 → 0x140A4C820 (rule 7)
- keeperWithinNinety: 3135 → 0x140439FE0 (not ported)

### humanoid.cpp (the lane stack's momentum exemption; same change as FirstTouchPort 54413ac)
- too-similar requeue gate, locomotion clips exempt: 2019 → the PES locomotion handling (site not located; FUN_1406eebb0 is ChangeAction)
- momentum refusal exempt for locomotion clips: 2043–2046 → PES site not located (ThrowInTaker carries the same condition on MergeRA)

## 3. Corrections to older notes
- DirectHighBall "K+0x6E volatile stand-in false" (pes-port-A4Lanes3 and pes-re-carrier-port): stale. `pes::VolatileBall` is ported; the silence is PES-exact for ground balls.
- BackSpace "curve gate kills 53" (pes-re-carrier-port row 343): from an older tree. At `62e534c` it is 176 of 177 mates, 3 of them no-curve. The curve builder matches the decoded writer (`pes-re-image-rows` §2.3, updated this round).

## 4. Open items for the batch merge
- Not re-run this round: the carrier, lane, image, offball, scorerows and movestep binaries (green at `9932739`; the momentum exemption came after).
- Equal-key tie rule of the curve sort (0x141E53540): does not change the measured margins.
- Probes used to produce the table were removed; the 20-min runs passed with them in place. The clean tree was verified by both smokes.
