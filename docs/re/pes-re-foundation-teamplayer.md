# Foundation inventory: team-owned and player-owned PES runtime state (TeamPlayer) — TIGHTENED

Scope: every T+… field and every per-player record field (PI/PA/PM/hu(i)) that
ported or pending GF AI reads. MI/MA/event-ring/ball/G+match rows belong to
FoundationMatchBall, cited here only as dependencies. Ticket items all covered.
Revision notes (tightening pass): faithful re-audited under the strict rule
(GF computes the same rule, not the same read site) — 22 → 3; 6a4 co-writers
VERIFIED by decompile; live-Y cut 51 → 17 with per-row reasons; every
"writer not found" now names its search. Merges OffBallSelectors relayed points
(its inventory file was absent at `local://`, so the relayed key points are
cited as OBS-relayed, method: their disp32/SIB scan): T+0x255 three stores,
T+0xb6e0 0/1 pair, T+0xc12c/c12b writers, T+0xb8c0 writers, no G+0x25cf8/0x25d38
writer under disp32/SIB/lea/SSE forms, no holder rec+0xd4+4k writer, PA+1
readers only. New rows from those points: T+0xc12b, T+0xc12c, T+0x6d4.

Conventions: OBS = seen in code/live at the stated VA; INF = inference.
**faithful** = a GF function computes the same rule (named + one-line why);
**stand-in** = GF carries the term from its own producer (GF symbol cited);
**absent** = no GF counterpart. S/M/L = scalar / small struct-or-function /
subsystem. hu(i) = FUN_140a48910 = PA. Scans: **S1** = disp32+SIB(mod=2) store
scan over .text of pes_full.bin (base 0x140000000; LE32(disp) with store/movimm/
aluimm opcode + modrm mod=2, or SIB form; script
gf-FoundationTeamPlayer-scratch/extscan.py; OffBallSelectors find_writers*.py,
same method). **S2** = raw-imm scan (any instruction referencing the constant,
incl. lea/loads). NOT covered by S1/S2: mod=0 SIB (base-less indexed disp32),
rep-movs block copy, computed-offset stores with small disp. Disps ≤ 0x4c are
below the scan floor (constant collides everywhere); 0x78–0x2xx with 100+ hits
are unattributable without base anchoring.

## Ranked structures (by ported/pending gates unblocked)

| Rank | Structure | Rows | Gates it unblocks |
|---|---|---|---|
| 1 | P-AIM: pass-aim/action records (PI+0x14/1c/20/38/45/48/1490/1494/13ac/13c4/14a8, PA+1/8/11c/1a/1c) | 15 | stop leaves 5a7fe0/5a7be0/5a7d10, PASS_GET arbitration, receive first-touch |
| 2 | T-INST: active instruction table G+0x25cf8/0x25d38 + sheet recs T+0x8e08+0xef8… | 2 | ~298 a63950 sites: run selectors, counter 5a58b0, marking, support, outlets |
| 3 | T-REACT: reaction-delay T+0x6a4/T+0xb860 (writers VERIFIED) | 2 | human-dodge tail FUN_140a0b350, defender L timing |
| 4 | T-CONC: concepts T+0xb6e8/b718/b748/b7d8/f1/a2a0/28c/a2cc + supporter c12b/c12c | 9 | Chance caps/admission, b748==0x30 gates, transitions, supporter |
| 5 | T-LVL: level/strategy T+0xb8a4/b8a2/b8a3/134/210/b8c0 | 6 | line/width/length, box counts, counter tests, slide, keeper dist |
| 6 | T-SHAPE: shape block T+0xc48 (S+lines), bands, points, slots | 12 | line chain, cover drop, blend refs, formation targets, keeper pos |
| 7 | T-SHEET: tactics sheet T+0x8e08 | 1 | concept gates (ported), O6 branches, support dist, press depth |
| 8 | T-PRESS: mark/press row a548/a558/a53c/a540/a50c/a298/87d0/87cc/b824/b834/e4/c12a/88/14/b6e0/29c-fam | 13 | press-writer wiring, slide L, block, Delay/Press, trap, region gates |
| 9 | P-KIN: kinematics PM+0x554/55c/560/564/574/578/57c/5d0, hu+0x4c/1/1a/1c | 9 | net/post/human gates (ported w/ stand-ins), caller gating, challenge |
| 10 | P-IMG: carrier image + holder/stat block (image+0x32B0, box floats, rec+0xdc, ai134-group, T+0x6d4) | 7 | dribble W, shot/plan gates, support f0–f2, GK-coaching dispatch |
| 11 | T-POSS: possession/counter T+0x255/254/78/brain+0x6cf0/a508 | 5 | value-2 branches (2 sites), Chance team gate, keeper possession |

## Field table

`field | meaning (OBS/INF) | PES writer VA(s) | PES readers (ported/pending) | GF status + symbol | blocking items | size | live?`

### T-POSS + T-LVL

| T+0x254 (i8) | attack sign ±1 (OBS) | copy 0x1404167b3 (S1); setter-family 0x140a42570 (S1); init open | ported: line/cover/offside dirs | faithful: `w.s[t] = -side` (teamAIcontroller.cpp:2267) — same definitional value by fixture side; convention verified by 0.000 m live line reproduction | — | S | N |
| T+0x255 (byte) | in-possession latch (OBS) | setter FUN_140a428d0 @0x140a428d5 VERIFIED (`*(p1+0x255)=arg!=0`); copy 0x1404167c0; menu/UI 0x1415d983e; serialiser 0x14215d395; rule FUN_1404fe9c0 (PoseVeto §15, reused) | ported: Chance gate, keeper inPossession, dodge teamGate; pending: value-2 branches | stand-in: `RunWorld::possession` from `GetLastTouchTeamID()==t` (teamAIcontroller.cpp:2269) — GF's own possession, not the 4-step latch | latch-rule port | S | N |
| T+0x78 (u32) | fallback/last-kick player id (OBS) | S1 233 hits, unattributable (disp too common); base-anchored search from a5969b0 open | ported: focus fallback, challenge owner; pending: 593430 carrier race | stand-in: `RunWorld::receiver` + `BallEvent.target` (livepoke §2) | PASS_GET arbitration | S | Y (attribute writer; capture fallback values) |
| T+0xa508 (i32) | counter enable 0/1/2, 2 latched 48 ticks (OBS) | FUN_140581980 @0x1405819c0; call 0x1404f6f42 (T+0x255-gated); latch brain+0x6cf0=0x30; copy; serialiser (counter-trigger §1, reused) | ported: SideCounter `attackAllowed`; pending: ==2 sites 59ba60/664ee0 | stand-in: `SideCounterInput::attackAllowed` (gate only; value-2 false, pesoffball.hpp:316-318 ponytail) | 2 branch sites | S | N |
| brain+0x6cf0 (i32) | value-2 latch countdown (OBS) | FUN_140581980 (sets 0x30, counts down) | pending: same 2 sites | absent | value-2 branches | S | N (writer + meaning OBS statically) |
| T+0x134 (i32) | CPU strategy 0..6 (OBS) | FUN_14051e970 via 51e910 (phase 3); T+0x134=brain+0xbcc by team tick (S1 296 common hits unattributable; copy OBS per match-mgmt) | ported: StrategyAttackLevel map; pending: modes 1–6, plan queue | stand-in: `StrategyAttackLevel()` (GF strategy input, not PES R-state) | modes 1–6 | S | N |
| T+0x210 (i32) | manual/target level, init 2 (OBS) | S1 653 hits, unattributable; O4: no in-match writer (menu provenance open) | ported: 210-clamp (no-op at 2) | stand-in: none carried | — | S | Y (find menu feeder) |
| T+0xb8a4 (i32) | attack level 0..4 (OBS) | S1 5: copy 0x1404172d8; 0x1404f84bb/0x1404f86e7 inside FUN_1404f8350 VERIFIED; init 0x140a416ae →2; serialiser 0x14215e96c | ported: level map, caps, box counts, length; pending: concept starts, keeper dist | stand-in: `AttackLevel(mentalityRung)` + defshape `ponytail: 2` (GF rung, not PES level rule) | — | S | N (live L=2 ×7298, static writer shut) |
| T+0xb8a2 (byte) | L>=4 flag (OBS) | team tick after 4f8350 (r12a.c:216-222) | slide L+=1 iff !=0 (0x140972052) | absent: GF slide L excludes it (defender-decisions-port: unresolved, not counted) | — | S | N |
| T+0xb8a3 (byte) | L<=0 flag (OBS) | 0x1404f6e4f/0x1404f6e58 (clear + set when b8a4<1) | ported reads; pending: 5a980 gate, 682540, step-up skip | stand-in: `attackLevelZero = mentality==AllOutDefence` (teamAIcontroller.cpp:1802) — GF rung, not the L<=0 rule | — | S | N |
| T+0xb8c0 (frame) | level-change stamp (OBS) | FUN_1404f8350 (=frame on change); OffBallSelectors VAs 0x14041732c (copy) / 0x1404f870c (inside 4f8350 — VERIFIED same function by decompile-at-VA) | none in GF | absent | — | S | N |

### T-REACT

| T+0x6a4[11] (i32) | per-player reaction-delay frames, never drops below running value (OBS duels-fouls §1.1) | VERIFIED three writers: FUN_1405027A0 (primary loop, in-play); FUN_1404fdd20 (S1 hits 0x1404fe305/0x1404fe8b1 in-range; decompile = event/carrier-gated delay writer); FUN_140502190 (zero path: stride-4 loop over `[b860]` and `[b860-0xb1bc]`=[6a4] ×11 when match+0x291c==3) | ported: none fires (=0); pending: human-gate tail, defender latency | stand-in: `rosterSlotValue = 0` (pesdodge.hpp:281; ponytail never-fires) | human-gate port, L timing | M | N (writers + formula OBS; live distribution not needed for the nonzero-blocks rule) |
| T+0xb860[11] (i32) | delay mirror (OBS) | S1 5 paired hits 0x1404fc485/0x1404fe30d/0x1404fe8b9/0x1405026fa/0x140503382 (same three writers) | pending: same | absent | same | S | N |

### T-INST + T-SHEET

| G+0x25cf8/0x25d38 (4×(id,slot) per team) | active instruction pairs, slot 0xff = team-wide (OBS h1 §9) | S1 0 hits; S2 9 refs each, ALL attributed readers: tester 0x140a6397a, helpers 623ab/62494/633ab/6354b/6444b/6448b, UI 0x14147bcd0, reflection 0x142160bd9 — no lea/memcpy setup refs the base | ported: none (all terms false); pending: ~298 a63950 sites | absent (ponytail-false terms named) | ~30 selectors, 5a58b0, marking L, support | L | Y (O7 change-logger to find activator; remaining avenue: computed-offset stores in setter family 0x140a42430-0x140a42940, S1/S2-blind) |
| T+0x8e08 sheet block (plan/sliders/toggles/roles/coords/slot-recs/slot→player/instr-recs/designated) (OBS tactics §1) | team tactics sheet | S1 0 (base derived arithmetically; accessor family OBS); setters 42600/42880/42430 OBS; live both teams | ported: SR/DL/Comp + 7 toggles + selector (literal, exe defaults); pending: O6 branches, taker sheet branch, T+0x4b40 outlet | stand-in: `pesteamconcepts.*` (row-level: sliders/toggles/selector faithful as port, roles/coords/takers pending) | O6, takers, outlet | L | N (live 7298, 0 violations; rest is static) |

### T-CONC

| T+0xb6e8[48] (bytes) | concept running flags (OBS) | S1-relayed 5: copy 0x1404171bf; 0x14057acec=0 / 0x14057ad05=1 movimm8 pair; init 0x140a41691; serialiser 0x14215d68b (containing functions open) | ported: TransitionChase/SideCounter flags (GF-side); pending: 46 concepts | stand-in: GF `ConceptOrder` (GF ids, not PES ids) | concept ports | M | N (set/clear VAs shut; ids are static) |
| T+0xb718[48] (bytes) | start-test byte c+0x10 (OBS) | S1 1: 0x140579b4d (in selector 579840) | pending: restart tests | absent | — | S | N |
| T+0xb748[11] (bytes) | per-slot concept id, 0x30 = none (OBS) | S1 128 movimm32 in concept bodies 0x14053e825…0x140541b39 (FOUND VAs, per-site mapping open); clearer 57a3e0 | ported: ==0x30 gate assumed | absent (no GF slot values) | run admission | S | N (sites are static work) |
| T+0xb7d8..0xb824 | release scratch (OBS) | clearer 57a3e0 | none | absent | — | S | N |
| T+0xa2a0[11] (i32) | team action state per slot (OBS) | writer FUN_140a42940 OBS; ring reader a401c0 | ported reads; pending: release 5a8290 | stand-in: `PlayerFacts::state` from GF run states (`run->second.state`, teamAIcontroller.cpp:2300) — GF states, not PES writer output | Chance release | M | N |
| T+0x28c[11] | run sub-kind (OBS) | S1 91 hits, unattributable (mixed bases) | pending: 691300 sub-8, 5a8290 sibling | absent (no RunRecord sub-kind) | SecondLine sub 8 | S | Y (attribute sub-2/8 sites) |
| T+0xf1 (+b709/b712/b715/b70c row) | press/concept-armed flags (INF med) | S1 40 mixed, open (tracer §1.8 item 4 stands) | ported: `presserFlag` carried | stand-in (value GF-side; ids open) | press-family disambiguation | S | Y (press-episode tracer with b748) |
| T+0xa2cc ring | state-age ring (OBS) | S1 0 hits → register-valued writer open (O6: statesites.py no-imm tuned) | ported: `PlayerFacts::t` age | stand-in (GF duration, not PES ring) | 5a8290 0.2 s compare | S | Y (O6 writer scan + ring semantics) |
| T+0xc12b (byte) | supporter-wanted flag (INF; NEW from relay) | S1-relayed 4: copy 0x140417534; support-pipeline 0x140501a05/0x140501e78; serialiser 0x14215d853 | supporter pipeline (off-ball 4.1) | stand-in: `ProcessDesignatedSupporter` (carrier-in-open-play assumption; ponytail undecoded) | supporter admission | S | Y (confirm c12b matches carrier-in-open-play) |
| T+0xc12c | supporter request (INF; NEW from relay) | S1-relayed 5: copy 0x140417541; 0x140501fbd/0x1405020ea movimm8; 0x140a41948; serialiser | same | stand-in (same feature) | same (shared episode) | S | Y (shared with c12b) |

### T-SHAPE

| T+0xc48 (ptr/base) | shape/line-work block S (OBS) | S1 38 mixed, unattributed (likely mixed-base coincidences; base alloc open) | ported: line/width/length formulas; pending: S+0x7b38 phase-1 rear, cover 5f4f50 | stand-in: `ShapeField`+`pesdefshape.*` (formulas faithful, S storage GF-side) | cover drop (2.9 m gap) | L | Y (MA relocation for S+0x7b38; ko60c.py ready) |
| S+0x7b10+0x14·line(+0x3c) / S+0x7b14 | line bands {rear, front} own-frame (OBS EV-DEF-0009, live) | FUN_1405fbcb0 via 5f9eb0/5fa990/5fb6f0/5fb2d0 | ported: front/target/width/length | faithful: `TargetLineCushionFactor` (1405f7a00), `DefensiveLineStepUp` (1405ef910), `LineHalfWidth_m` (140688300+63f230), `TeamLength_m` — literal arithmetic ports, VA constants (pesdefshape.cpp:121-220) | — | M | N (live median 0.000 m, n=3950) |
| T+0x875c / T+0x8770 | back/mid band own-frame (OBS) | S1 0 hits → band writers use computed base; values OBS live (phase-0 edge) | ported reads + lineDown test | stand-in: `backBand=ownFrameLine, midBand=+gap` (teamAIcontroller.cpp:428-429) — GF's line, not the 5fa990 band rule | phase-1 rear | S | N |
| T+0x89a0 / T+0x89a4 | measured back/mid world-x (OBS) | S1 0 hits (computed-base stores); writer FUN_140687820 OBS | ported reads | stand-in: `MeasuredLine()` GF mean-clamped (teamAIcontroller.cpp:445; ponytail: slot line ≈ role line) — GF-designed, not the 687820 rule | — | S | N |
| T+0x8898 | band depth (OBS) | S1 0 hits → open | pending: 581370/580ef0 | absent | special-situation fidelity | S | N (static tests) |
| T+0x250 | cone centre (OBS) | S1 1159 common → unattributable | pending: 643a30 branch | absent | carrier veto | S | N (call-site meaning OBS) |
| T+0x26c / T+0x27c | active/queued concept point (OBS; 27c init −200) | FUN_140634cb0 via 42a40 OBS | ported reads | stand-in: `order->target` (GF concept point, elizacontroller.cpp:622) | cover enrolment | S | N |
| T+0x4b4c+12·slot | slot formation point (OBS) | S1 0 hits → sheet-coords path; store open | ported reads | stand-in: `slotPoint = GetAdaptedFormationPosition()` (GF adapted, not PES slot write) | — | S | N |
| T+0x4b40 (slot) | outlet slot (OBS) | S1 0 hits → open | pending: wall skip, FK exclusion | absent (ponytail) | wall/outlet | S | Y (outlet selection is dynamic per restart) |
| T+0x4c00[11] (i32) | role class 0..9 (OBS) — array B = S+0x3FB8 | rebuilt every team tick (`FUN_1404F5F10`) + setup/phase (`FUN_140A59F90`); source registry holds 0..12 (S1 found nothing because the stores use the S base +0x3FB8) | ported: role gates, keeper test, GKCoaching | stand-in: `Position(gfRole)` (mapping, not PES value) | keeper/outlet/roles | M | N (mapping OBS from readers; rest static) |
| T+0x4bd4[11] (=T+0x3f8c+0xc48) | role class 0..9 (OBS) — array A = S+0x3F8C, identical to B after every bulk writer | same writers (store pair `0x140A425A0`/`0x140A42590`, same value); only divergence is the B-swap `FUN_140A422C0` | ported: sub thresholds, budgets, rush race | stand-in (GF-role derived) | — | S | N |
| T+0x558[11] | mark target per slot (OBS) | 534a20-family (report) + setter-family store 0x140a428c4 (S1) | ported: mark passes | stand-in: `w.p[target].marker` from GF marking (teamAIcontroller.cpp:2319) — GF assignment, not the 534a20 order | — | S | N |

### T-PRESS

| T+0xa548[4] (i32) | press-slot men (OBS) | S1 3: copy 0x1404170a1; press-family 0x140537411/0x14053784c (writer region; 536fc0 itself one-input, hook unresolved) | ported: picker/passes/rings; pending: table wiring | stand-in: `pressSlotMan` from GF ported picker (table d² unwired) | writer wiring | M | Y (locate MA to verify MA+0x24710 d² — livepoke §1 item 1) |
| T+0xa558 (i32) | press-excluded man (OBS) | S1 4: copy 0x1404170d1; press-family 0x140537089; init 0x140a41973; serialiser | ported: `notInA558` term | stand-in (term shape ported; value from GF excluded man, writer 537089-region not ported) | — | S | N |
| T+0xa53c (i32) | press timer (OBS EV-TCR-0008) | S1 5: copy 0x14041707d; 0x14053618f movimm8; 0x1405361ee VERIFIED setter; init 0x140a41657; serialiser; predicate 535650; stamp a540 | ported: hysteresis bool only; pending: L+1, 62ade0 ramp | stand-in (bool only) | full timer port | M | Y (confirm timer/stamp semantics over a press episode; meanings Tentative) |
| T+0xa540 | press-rise stamp (OBS) | 0x1405361e7 VERIFIED (rising edge stamps MI+0x1198) | pending: 4·a544 approx | absent | — | S | Y (shared episode with a53c) |
| T+0xa50c (i32) | ball-zone index (OBS) | S1 5: copy 0x140417057; 0x140535e15 (zone-writer region); 0x140537a2c; init; serialiser | pending: 3 special-situation tests | absent | — | S | N (attribute 535e15-region statically) |
| T+0xa298 (byte) | defensive mode byte (OBS; item 5 open) | S1 4: copy 0x140416fd0; init 0x140a41636; setter-family 0x140a424d0 (!); serialiser — writers FOUND, meaning open | pending: slide L, box rule | absent | mode-byte meaning | S | Y (item-5 experiment to label the byte) |
| T+0x87d0[11] | mark-slot busy==2 (OBS) | S1 0 hits → mark-family loop (computed base) | ported: wall skip + budgets read ==2 | stand-in (==2 shape ported; busy value GF-side) | — | S | N |
| T+0x87cc | 68fa10 gate (OBS) | S1 0 hits → open | pending: 68fa10 release | absent | Chance release | S | N (static) |
| T+0xb824 | team region code (OBS) | S1 5 FOUND in concept bodies 0x140553e69/0x14055d99a/0x14056f423/0x14057a363/0x14057a53e (functions open) | pending: 68bd90, 5a5400, executor zone | absent | 3 gates | S | N (attribute sites statically) |
| T+0xb834[slot] (float) | mark-spot rate TELEMETRY (OBS §2.1: no load outside copy/serialiser) | stores 0x140624235/@0x1406259e0; copy; serialiser | blend law decoded | absent: no GF counterpart — and PES itself never reads the stored value (rate passes in register) | — | S | N |
| T+0xe4 | trap arm (OBS) | S1 377 common → sheet path; live off both teams OBS | ported: trap gate (default off) | stand-in (tactics-gated; live value = off) | — | S | N |
| T+0xc12a (byte) | retreat flag (OBS) | FUN_140533d10 OBS | pending: retreat behaviour | absent | — | S | N |
| T+0x88 / T+0x14 | enrolment terms (OBS) | open | pending: cover enrolment | absent | — | S | N |
| T+0xb6e0 | concept-enable row | writers above (57acec/57ad05 pair) | pending: enable gating | absent | — | S | N |
| T+0x29c+0x40·s / T+0x2a3 / T+0xc0 / T+0x88d8 | kind byte / byte / AI class / far-post man (OBS) | 29c: S1 82 mixed; 2a3: S1 1 (0x141fd8e33, likely foreign base); c0: S1 4227 common; 88d8: S1 4 in set-piece region 0x14057d211/2b0/30d/333 (setter region OBS) | ported: assumptions (0/clear) | stand-in (assumed) | support branch | M | Y (confirm kind bytes stay clear in open play) |

### P-AIM (live values OBS livepoke §2: 267 snaps + 31 stops; P-AIM writers below)

| PI base `*(G+(k+0xf3)*0x28)` | leaves' record A (OBS a489f0) | accessor | readers below | absent | whole structure | M | N |
| PI+0x14 (u32) | ==8 gate | S1 430 common → unattributable | pending: 5a7be0 | absent | stop leaf | S | N (base-anchor from leaf readers) |
| PI+0x1c (u32) | ==id gate | S1 74 mixed → unattributable | pending | absent | stop leaf | S | N (same) |
| PI+0x20 (u32) | ==0x18 gate | below scan floor | pending | absent | stop leaf | S | N |
| PI+0x38 (float) | route bearing vs MA+0x247d4 | below floor | pending | stand-in: `aim` point (not a bearing) | stop leaf | S | N |
| PI+0x45 (byte) | {2,3} gate | below floor | pending | absent | stop leaf | S | N |
| PI+0x48 (u32) | keeper-case ==0x18 (live always 0) | below floor; trigger value NEVER observed | pending: keeper branch | absent | keeper stop | S | Y (capture the 0x18 trigger — unseen in 267 snaps) |
| PI+0x1490 (u32) | receiver id (live 255 at stops) | S1 15 FOUND: pair 0x140431cd9/0x140431ce5 (w/1494); 0x14092e4f4/52a; 0x140a3bb64/0x140a3c621; 0x140a58e94; pair 0x141e5fc74/0x141e5fc81 (functions open) | pending: aim leaves | stand-in: per-team receiver + last-kick target | carrier aim 5a7d10 | M | N (attribute VAs statically; MA Y covers carrier aim) |
| PI+0x1494 (u32) | receive state {2,3} | S1 4 FOUND (above pairs + 0x14092e6d4 + 0x140a58d8d) | pending | absent | stop + release | S | N (same) |
| PI+0x13ac / PI+0x13c4 | ==0x18 / ==id gates | scan open (large disp, scannable) | pending | absent | stop leaf | S | N |
| PI+0x14a8 (frame) | stamp vs MI+0x1198 | scan open | pending timing | stand-in: `t` age (not a stamp) | timing | S | N |
| PA base `*(G+(k+0xa5)*0x28)` | action object (OBS a48910 = hu) | accessor | readers below | absent (kinematics mirrored separately) | — | M | N |
| PA+1 (byte) | move-action byte {9,0x23,0x29}; ≠ team state (0/5553 agree) | readers only (relayed searched); producers = action dispatcher (static work) | pending: leaf masks | absent | stop + aim | S | N (resolve via dispatcher, not live) |
| PA+8 (flags) | action flags | below floor | pending leaf bodies | absent | — | S | N |
| PA+0x11c (float) | facing deg | S1 285 common → unattributable | pending | stand-in: `facing` is +0x554 (move object) | — | S | N |
| PA+0x1a/0x1c (u16) | clip counters (+1a<+1c never true in Chance) | below floor (u16 clip fields) | pending timing | absent | — | S | N |

### P-KIN (PM = FUN_140a48a10; GF producer = GF locomotion/dispatcher, not PES tick — hence stand-ins)

| PM base `*(G+(k+0xbf)*0x28)` | move object (OBS) | accessor | readers below | faithful (structural): GF Player/Humanoid root is the single authoritative per-player kinematic object both AIs read pos/vel/facing from | — | M | N |
| PM+0x554 (float) | body facing (OBS) | S1 23 mixed (0x140a6c569/664 predictor region) | ported reads | stand-in: `f.facing = pesDeg(GetDirectionVec())` — same quantity, GF integrator | — | S | N |
| PM+0x55c/0x560/0x564 | position x/y/z (OBS; NOT ability anchors — carrier-ai correction) | S1 84 mixed → unattributable | ported: posX/posZ, NetGateInput, mark/cover/challenge | stand-in: same quantity, GF integrator | caller gating | S | N |
| PM+0x574/0x578/0x57c | velocity per-frame (OBS) | S1 55 mixed (0x14091e6xx locomotion region) | ported: moveXYZ, order key, reach | stand-in: same quantity, GF integrator (per-tick units documented) | — | S | N |
| PM+0x5d0 | current AI action (OBS) | OffBallSelectors: 89 hits (dispatcher writes; base anchoring open) | ported: `action[22]`, roleCheck, slide/tackle gates | stand-in: GF action id at the same read sites | — | S | N |
| PM+0x5b0 | movement-state branch (OBS) | S1 121 common → unattributable | pending: state-5 branch | stand-in: speed floor (ponytail) | state-5 branch | S | N |
| hu+0x4c (byte) | anime state, ==2 yields outside branch (OBS) | S1 53 common → unattributable | pending: outside branch | stand-in: treated not-set (ponytail) | outside branch | S | N (lift needs viewer frame, not live read) |
| hu+1 / +0x1a / +0x1c | anime id / clip frame / contact frame (OBS) | clip layer (common/below floor) | ported: tackle/slide/contact windows, keeper reach | stand-in: GF clip playback at the same read sites | — | S | N (25k-frame live shut: ai134==0; 1/1/10 starts) |

### P-IMG

| image+0x32B0+8 (u8) | carrier image state 0..4 → W ladder (OBS) | S1 3 FOUND: 0x140458c98/0x140459266/0x14210da93 (functions open) | ported: 72×15 search (W param); pending: W wiring | stand-in: W as parameter (unwired) | dribble aim, plan/shoot | M | N (ladder observed 892 snaps; wiring is static) |
| imageA+0xb3c/b54/b58/b60 + sector +0xb44 | box floats + zone sector (OBS wide_eval) | S1 b3c 15 / b44 17 FOUND in image-init regions 0x14118xxx/0x1413fxxx (functions open) | pending: wide_eval/aim/lane | absent | shot sub-kinds | M | N (attribute statically) |
| rec+0xdc+4k (u32×3) via FUN_140a50250 | holder attr; f0–f2 = block +0xd4/+0xd8/+0xdc (OBS mechanism, labels open) | S1 0xdc 434 common → unattributable; relayed: no holder rec+0xd4+4k writer under searched forms | ported: none (terms false); pending: support flags, 609350, Chance holder terms | absent (ponytail-false) | support scorer, Chance gate | M | Y (O5: log +0xd4 block per carrier live to label f0–f2) |
| ai+0x134 (byte) | human-challenge intent (OBS §4: writer FUN_140a596f0 from 92d580; AI-vs-AI always 0, 181k frames) | VERIFIED writer + reset 58930 + mirror 431800 | ported: intent input = 0 | stand-in: constant 0 (value matches PES AI-vs-AI; pad-table rule not computed) | — | S | N |
| ai+0x1474/1478/12a0/148c/14a0 | contact-candidate frame/angles + last-touch flag (OBS) | publisher 92d580 | none in GF | absent | — | S | N |
| T+0x6d4 (i32) | GK-coaching team-table mode (OBS carunits §3.9; NEW from relay-client scan) | S1 26 mixed (0x1405d9dbe LongPass region etc.; attribution open) | pending: GKCoaching dispatch (modes 1–6 replay outfield answers) | absent (no GK long-ball-bias dispatch) | keeper distribution | S | N (mode-0 in open play; never ripe in 892 — static) |

### Dependencies (FoundationMatchBall-owned, cited not duplicated)

MI+0x116c/1198/11a0/1748, MA+0x24710/2601c/2483c/24840-34, event ring + kinds
9..14, ball record. T+0x255 owned here, cited there as blocking input.

## Counts

- Structures: 11. Rows: **81 — faithful 3 / stand-in 38 / absent 40**.
- Sizes: **S 65 / M 13 / L 3** (T-INST table, T-SHEET block, T-SHAPE block).
- Needs live read: **Y 17 / N 64**. Every Y above names its exact reason
  (writer attribution, named O-episode, trigger capture, or meaning experiment);
  N = writer found + meaning OBS statically, already observed live, or no GF
  reader needs the value.
