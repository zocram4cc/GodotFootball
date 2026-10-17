# Foundation inventory: areas round 1 did not cover (Areas)

Owner: FoundationAreas. Read-only round, no sim code touched. Contract paths:
repo `<workdrive>/GodotFootball` (main, origin/main 2026-10-15), docs `docs/re/`,
scratch `<workdrive>/gf-FoundationAreas-scratch/` (decompiles `7324b0.c`, `50f7e0.c`,
`9fa720.c`; scanners `relscan.py` (flat-image E8 caller scan, off = VA − base),
`areasscan.py` (S1 disp32+SIB store scan), `extscan.py` (round-1 copy)).
Ghidra bridge decompile endpoint only (`decompile_function_by_address`,
`get_function_by_address`, `disassemble_function`); no xref endpoint exists, all
caller lists are OBS from `relscan.py` over `pes_full.bin`.

Round-1 rows are cited, never duplicated: `pes-re-foundation-teamplayer.md`
(81 rows) and `pes-re-foundation-matchball.md` (54 rows) at the session path
`<local>`.
Boundary with FoundationWiden (acked both ways): no new rows on T+/PI/PA/PM/hu
team-player fields, MI/MA/ring/ball match-ball fields — cited as deps only.
Widen's draft (`pes-re-foundation-widen.md`, same dir) was read; no row key
collides (its W-rows avoid anime/keeper/judge/PASS_GET/kick-error).

Conventions: G=`*(0x1436F3DC8)`, hu=`FUN_140a48910(G,i)`, ai=`FUN_140a489f0(G,i)`,
PM=`FUN_140a48a10(G,i)`, P=AnimePlayer, MI=`*(G+0x270)`, MA=`*(G+0x18)`, gs=54.
OBS/INF per decompile/scan. **faithful** = GF computes the SAME RULE (named +
one line why); **stand-in** = GF carries the term from its own producer (GF
symbol + `ponytail:` line cited); **absent** = no GF counterpart. S/M/L =
scalar / small struct-or-function / subsystem.

## 0. Ranked structures (new rows only; by pending gates unblocked)

| rank | structure | rows | gates it unblocks | f/s/a |
|---|---|---|---|---|
| 1 | K-SEL: pass/shot selection + PASS_GET + first-touch + lockout (K-S6..K-S11) | 6 | receiver arbitration, first-touch kinds, re-touch timing, shot-branch parity | 0/6/0 |
| 2 | A-CLS: anime-class predicates + slot tests (A-AN3/4/11/12) | 4 | dodge tails, arbitration class gates, judge scoring, tackle clips | 1/2/1 |
| 3 | J-JDG: judge record + advantage + card memory (J-1..J-6) | 6 | foul parity, advantage play/call-back, late cards, repeat-offender rule | 0/5/1 |
| 4 | K-ERR: kick request + error + predictor + assist (K-S1..K-S5) | 5 | kick parity (stat/quality/scatter), AI reachability, curl assist | 0/3/2 |
| 5 | K-GKP: keeper think/claim/distribution (K-1..K-9 area 2) | 12 | save-chooser parity, claim timing, distribution kind/target | 2/4/6 |
| 6 | A-MOV: locomotion FSM + rate + tables (A-AN5..10/16) | 7 | locomotion port, rate caps, clip-table coverage | 2/2/3 |
| 7 | J-PRE: whistle/scene/gesture/officials + action requests (J-7..J-13, A-AN14/15) | 9 | presentation (stub per priority §11), dive rule, DOGSO, pick parity | 0/3/6 |

Totals: 7 structures, **52 new rows: faithful 5 / stand-in 27 / absent 20**.
3 rows sit outside the ranked structures (A-AN1 anime-state byte, K-S12 keeper-
distribution kicks, K-S13 restart choreography); counted in the totals above.
Cited-not-duplicated: P-KIN hu+1/+0x1a/+0x1c, P-IMG image+0x32B0+8, E12 MI-scene
block, T+0x6d4 GK-coaching, T+0x255 latch, MA/MI/ball deps.
Effort: S 22 / M 25 / L 5. Needs live read: Y 8 / N 44.
Resources: static scans (relscan/areasscan) finished before the owner order; no
sampler/build/test run by this agent; scratch only under
`<workdrive>/gf-FoundationAreas-scratch/`, nothing in /tmp.

## 1. Field table

`field | meaning (OBS/INF) | PES writer VA(s) | PES readers (ported=P / pending=p) |
GF status + symbol | blocking items | size | live?`

### AREA 1 — animation/anime layer

PES fns ported (P): ball selector `FUN_1407358e0` (caller 73c0a4), net selector
`FUN_14073c5d0` (caller 73bd3e), post selector `FUN_1407368d0` (caller 73ca19),
`FUN_140739f90` kind→row (5 callers), `FUN_140735870` kind→column,
`FUN_140736d30` speed class (4 callers), `FUN_140a6d380` ball band,
`FUN_140a07bf0` ball eligibility (caller a07345), `FUN_140a07570`/`FUN_140a07880`
sub-tests, `FUN_140a07e40` net gate (caller a07dfe), `FUN_140a0b350` human gate
(caller a08ebe), `FUN_14078b550` sub-kind pick (caller 7919db),
`FUN_14078e5a0` readiness (caller 791973), `FUN_14078f290` clip mask (caller
78b5dd), `FUN_1406de990` column picker (46 callers), Animetable accessors
`FUN_1406abb30` (235 callers) + abd60/abdd0, `FUN_1406b4580` slot-7 test
(77 callers), class predicates below (A-AN12). Decoded-pending (p): MOVESTEP
`FUN_1406ba900` (caller 6b85c2) + 20 state handlers (6c0170/6c4c10/6bdf10/
6c9e20/6bb570/6bcd20/6c14c0/6c1070/6c7400/6c3470/…), rate fns `FUN_1406b9e80` /
`1406ba070` / `1406e9f10` / `1406eab00` / `1406f9a2c`, kick align
`FUN_140733a30` / `1407302b0`, Dodge slot-13 `FUN_14073bc70`, gate
`FUN_14073cbc0` (caller 73bc9e), human kind `FUN_14073a3d0` (caller 73614f),
gesture `FUN_1407a1ac0` (caller 7a1513), `FUN_1406f0f70` SetMotion (157 callers),
`FUN_1406e5fc0` AnimePlayer::Exec (3 callers).

| A-AN1 | hu+0x4c (byte) anime state; ==2 yields the net-gate outside branch (OBS per pesdodge.cpp:531) | writer NOT FOUND (areasscan: disp 0x4c below scan floor, unattributable) | readers: net-gate outside branch (P, yielded); p: other outside branches | stand-in: treated not-set (`pesdodge.cpp:531` ponytail) | outside-branch parity | S | Y (LivePoke 1: confirm 2 coincides with the yield) |
| A-AN3 | hu+8 (u32) action flags: bit13→6e3c0, bit16 tackle, bit29 victim-fell, bit30 stagger, bit34 jostle, bit6 tackle (OBS decompiles + duels-fouls §2) | action dispatcher (no S1 scan run; flags written with the action id) | readers: 6e3c0 (p), judge gate 50d6a0 bit29 (p), P_action bits (p) | absent (no per-player flag word in GF) | judge + arbitration class gates | S | N (flag→reader mapping is static work) |
| A-AN4 | hu+0x44 (byte) tackle sub-kind 1/2/3 (OBS duels-fouls §2.3: W=20 / base 5/15/25 / angle formula) | writer `FUN_14078b550` region + dispatcher (default sub-kind 7 store OBS `pestackle.cpp:17`) | readers: judge P_action (p); P: `ResolveClip` group tables | faithful: `pes::PickSubKind` (`pestackle.cpp:236`) — same kinds-1..8 first-ready loop over the same group tables | judge scoring | S | N |
| A-AN5 | P+0x245c (byte) MOVESTEP state 0..23 + P+0x245d previous (OBS names @0x1425dd698, dispatch `FUN_1406ba900`) | writer `FUN_1406ba900` tail (req[0x38]→new state, OBS); init stores 0x1406b530f/5359/539f/6b8410/6eb45f (areasscan 258 hits, attribution open) | readers: the 1406ba900 switch (p — all 20 handlers) | absent (GF `Humanoid::SelectAnim` is query-based, no FSM state) | locomotion port | M | Y (LivePoke 2: confirm INIT/IDLE/RUN_LOOP/STEP_LOOP transitions) |
| A-AN6 | P+0x245a (byte) locomotion speed class 0..4 + P+0x245b foot/start band (OBS anim-select §2.3) | open (areasscan 62 hits incl locomotion region, unattributed) | readers: INIT gate, run-entry band cap (p) | stand-in: `pes::SpeedClass` (dodge 140736d30 only; locomotion 0..4 class not carried) | locomotion entry | S | N |
| A-AN7 | P+0x377c (f32) rateMax + P+0x3778 guard + P+0x3758 playbackSpeed + P+0x2814/2816 recent-tick stamps (OBS anim-select §3.5: live 377c inverts to Speed 60..99) | writer `FUN_1406f9a2c` rate cap (report OBS; areasscan 377c 0 hits → computed-base store) | readers: run-entry `rate ≤ 0.95·rateMax` (p) | absent (GF plays clips at authored rate; no Speed→rateMax rule) | locomotion rate parity | S | N (live values already OBS in-report) |
| A-AN8 | P+0x32d0 per-player Random (slot-2 kick/dodge draws; idle 4/12-way picks via 6e6bf0/6ebf30; OBS anim-select §2.4) | writer `FUN_1406eb670` seeder (ctor defaults per rng report; derivation unproven) | readers: P: kick draws via `Player::GetPesRandom()`; p: idle picks, tackle delay roll | stand-in: match seed + player id (`peskickerror` ponytail; PES derivation open) | exact draw streams | S | N |
| A-AN9 | Animetable.bin rows×cols cells {motionNo+1, mirror} + layout (rows/cols/mask/shift from exe immediates; OBS animtable-port) | data (install; no writer) | readers: P: `BallSelector`/`NetSelector`/`PostSelector`/`KindRow`/`ResolveClip` via `Cell/Column`; p: locomotion rows, gesture rows 179-193 | faithful: `pes::Animetable::Cell/Column` (`pes/animetable.hpp`) — same install bytes, same nearest-populated-column picker | locomotion + gesture rows | M | N |
| A-AN10 | Mbinfo bins: Animation.bin timing (end/blend/root), HitData hit frame/place, CancelData cancel frame (OBS animselect §2.1-2.2) | data (install; no writer) | readers: P: column-9 hit-window fit in `KindRow`; p: cancel frames, loop tails | faithful: `pes::Mbinfo::Timing/Hits/Cancel` (`pes/mbinfo.hpp`) — same records from the same bins | cancel-frame gates | M | N |
| A-AN11 | anime-slot test `FUN_1406b4c10(ap,0x15)` (OBS: 347 B decompile = `p+0xc40` vtable slot+0x10 dispatch) | n/a (pure reader of the anime slot table) | readers: 18 E8 callers OBS (588ed3/58f6f5/845b81/970e5b/9bdb92/9bdc62/9fa550/a072f0/a07eef/a085a2/a08d37/a0b417/a0b7b6/a0b8ea/a0bab5/a0bcfb/a0c2a9/a0c33f); P: net gate slot term, ball sub-tests | stand-in: `animeSlot=true` hard-coded (`elizacontroller.cpp:1226`; `pesdodge.hpp:257`) — always admitted, never refused | slot-occupancy port (dodge during committed clips) | S | N (18 callers OBS statically) |
| A-AN12 | anime-class predicates over table 0x1426484f0 ids ≤0x5d (OBS decompiles): 6e5e0 bit5 / 6e340 bit0 / 6e300 bit15 / 6e1c0 bit4 / 6e3c0 bit13-or-action>0x5d / +6e320 0x30000 committed / 6e450 bit9 / 6e110 bit27 / 6e290 motion-id set / 6e470 bit25 | n/a (pure readers of the class table) | readers: E8 callers OBS (6e5e0:158 / 6e340:243 / 6e300:44 / 6e1c0:71 / 6e3c0:22 / incl 14210f6fc/142111c73 carrier, 933xxx receive, 50d6a0 judge); P: `BallClassAdmits` (6e470+6e3f0+6e5e0 bits, `pesdodge.cpp:408`) | stand-in: dodge triple faithful (same bits, same table); receive-arbitration gates (931c60: 6e050/6e1c0/6e5e0) + judge-gate terms dropped from GF ports | arbitration + judge class parity | M | N |
| A-AN14 | action-request slots: throw/punt kind→[action+0x20/0x24/0x28], keeper cmd+0xff0 throw variant / +0xff4 punt type, +0xe60 request angle, +0xe68 height/urgency, +0xe6c=10 (OBS keeper-port §11.6: CanEnter 9d0950/9cef10; variant fns 9d04a0/9ce630) | writers `FUN_1409d0950`/`FUN_1409cef10` (E8-callers 0: reached by vtable, OBS) | readers: deflect/throw/punt execs (p) | absent (GF kinds are `e_FunctionType`; no kind→slot switch) | distribution kind/target (open item 1) | M | Y (LivePoke 6: kind byte + target triple at the 0x40/0x41 release tick) |
| A-AN15 | referee/linesman gesture rows 179-193 (`judge_*`) + referee cmd+0x85c (OBS foul-choreo §4.4: chooser 7a1ac0 reads cmd + MI+0x1554/0x1588; cases 1/2/3/4/5/9 decoded) | chooser `FUN_1407a1ac0` (OBS decompile) | readers: `Judge@action@anime` Update 7a14e0 (p) | absent (GF officials only position, no gestures) | presentation only | S | N |
| A-AN16 | root-alignment spread: P+0x428 AdjustTransXZ / +0x498 AdjustTransY / +0x4d0 AdjustAngle + P+0x3b18 scale (OBS anim-select §6: 7302b0 writes height+yaw only, AL-12) | writer `FUN_1407302b0` partial (OBS: no 0x428/6e85b0 ref); approach alignment open | readers: open-play kick approach (p) | absent (GF taker turn removed; throw-in hold teleport is a GF-designed stand-in, `referee.cpp` ponytail) | kick-approach parity | M | N |

Cited (not rows): P-KIN hu+1/+0x1a/+0x1c (GF clip playback stand-in);
P-IMG image+0x32B0+8 states 0..4→W (GF `in.imageState=0` neutral,
`elizacontroller.cpp` ponytail; W param ported, wiring pending).

### AREA 2 — keeper state

PES fns ported (P): deflect exec `0x1409d75d0→0x1440f35f0`, `ActionKeeperAfterCatchMove::exec`
0x1409e8880, `FUN_1409e0390` base-position core, `FUN_1409df7d0` in-possession step-up,
`FUN_1421162c0` rush race, Throw/Punt CanEnter 0x1409d0950/0x1409cef10
(E8-callers 0 each: vtable-reached, OBS), throw variant `FUN_1409d04a0`, punt tail
`FUN_1409ce630`, target pickers `FUN_142132660`/`FUN_142132020`. Decoded-pending (p):
think dispatchers `FUN_142113390`/`FUN_1421139a0`, claim solvers `FUN_14210be70`/
`FUN_14210b890`, speed ladder `FUN_140586a10` (caller 142113ee3), reach test
`FUN_1405912d0` (13 callers), leaves 58bcf0/58e680/58eb90/58bd90 (3 callers)/
58e700/58c910/585020 (8 callers)/58f600/588b60/506ba0, refreshers 582e10
(3 callers)/588ca0/58ec10, GK think units 142132b40/142132e20/1421333f0/142133120/
142133950 (T+0x6d4 dispatch, round-1 cited).

| K-1 | ai+0x12d8 claim point +0x12e4 frames·gs/48 +0x12fc/0x12fd kind +0x35 outcome 1/2/3 (OBS goalkeeper §11.1: solvers 14210be70/10b890) | writers: claim solvers (decompiled, exact stores open) | readers: save choosers + save actions (p) | absent (GF: bound-for-goal + `InterceptTarget`, `goalie_default.cpp:63`) | save parity | M | Y (LivePoke 5: 1308 countdown to −1 at catch) |
| K-2 | ai+0x1304 clock +0x1308/0x130c frames-to-claim +0x1310/1311/1329 catch flags; +0x1324/1328 punch; +0x1330/1334/1338 +0x133a urgency; +0x133c/1340-56 late-block caches (OBS §11.4) | writers: refreshers 582e10/58ec10/588ca0 on clock mismatch (areasscan: 1304/1308/1310/1324/1334/1338/133c hits incl 582xxx stores) | readers: leaf evaluators (p) | absent (GF arrival gate is the contact itself) | save parity | M | Y (same read; countdown OBS in-report, −1-at-catch to confirm) |
| K-3a | style bits 19/20 (D+0x48) → rush factor 1.1/1.2/1.3 (OBS keeper-port-2 §3: immediates @0x142115e6) | team setup (style word; both keepers 0x00080000 Offensive OBS live) | readers: P: `PesKeeperRushFactor` | faithful: `pes::PesKeeperRushFactor` (`peskeeper.cpp:333`) — same bit→factor map | — | S | N |
| K-3b | ai+0x1399/0x139a urgency + skill bits 35/18 punt type (D+0x40; OBS §11.6/§12.2: both keepers 0x840000000 → bit35=1) | think/distribution (areasscan 1399: 6 hits 142112xxx; 139a: 1) | readers: punt-type gate, pickup chain (p) | absent (no urgency bytes, no skill punt-type in GF) | punt-type parity | S | N |
| K-3c | ai+0x142c frames-to-claim (clear-eval gate `1.5·gs < 142c`, OBS §11.4) | publisher 92d580 path (open exact store; areasscan 5 hits 431b62/92f134/…) | readers: clear-eval (p) | absent | clear parity | S | N (static gate; live value needs a claim episode — covered by K-2 read) |
| K-4 | MA+0x251fa[side]/0x251fc claim/seen-off enable (OBS §11.7/§12.3: 28 cmp reads incl 8a8aa8/932823/9d4d4a; 0 S1 mov stores → wider-store pipeline 1404ad860/1404ac320 narrowed) | writer NOT FOUND (exact store unpinned; upstream of the 1404ad860 pipeline) | readers: 0x57 seen-off arm, CLEAR arms (p) | absent | claim-gate parity | S | Y (LivePoke 4: byte0=1 claim-enable, byte3 ~2 s pulse at claim) |
| K-5 | save-chooser cascade order + speed ladder (586a10: catch < ~76·loftCap / punch band / deflect ≥ 80+30c; OBS §11.2) + reach test 5912d0 frame windows by kind 0x38/0x39/0x3a (OBS §11.3) | n/a (pure deciders over K-1/K-2/ball) | readers: think dispatchers (p) | stand-in: `GetKeeperCatchSpeed_ms` window 14+20c (`humanoid.cpp:1196`) — same catch/no-catch question, different rule (GF window sits inside PES's catch band; no punch/deflect-above tiers) | save parity | L | N (ladder OBS statically; 0x44/0x4a/0x46 arms closed live in-report §12.7) |
| K-6a | after-catch carry `min(x+4,−A)` + 2 m lateral + `\|z\| ≤ F7−1` clamp (OBS keeper-port §2.1/2.2, corrected port-2 §1) | writer `FUN_1409e8880` (E8-callers 0: vtable-reached, OBS) | readers: P: `PesAfterCatchCarry` | faithful: `pes::PesAfterCatchCarry` — same own-frame min, same 2 m, same clamp | — | S | N |
| K-6b | throw/punt kind switch (3→power slot, 2→angle slot) + throw-variant d² ladder 100/196/361/484/676 + 50 m receiver-less cap + 3 m walk-out + pickers 142132660/32020 (OBS §11.6) | writers: CanEnter gates + variant fns (above) | readers: throw/punt execs (p) | stand-in: kind→`ShortPass`/`HighPass`/`LongPass` mapped; carry→nearest-mate is the `ponytail:` for the per-keeper table (`elizacontroller.cpp:306`); variant ladder / skill punt-type / walk-out absent | distribution parity | M | N (kind byte at release stays a live item — A-AN14 read covers it) |
| K-7 | positioning inputs: MA+0x228d4/d angle term a2 (writer open), offside-line L_t, wide/0x81a1 branch, T+0xb6de release, 1v1 rush, near-post/threat shifts (OBS keeper-port-2 §2) | mixed (a2 + 0x81a1 writers open; line/setup known) | readers: P: `PesKeeperBasePosition` core | stand-in: R-line core/caps/ramp/shuffle ported; `ponytail:` a2→θ, wide branch, airborne mode, 1v1, T+0xb6de exceptions (`peskeeper.cpp:86-90`) | positioning edge parity | M | N |
| K-8 | keeper RNG: slot 1 of per-clip `anim+0x19c0` `Range0(6)` shuffle; zero draws in choosers/converter (OBS §11.5: no Random call in gk_save/leaf/core/sub) | seeder per-clip (install streams) | readers: P: `KeeperShuffleDraw` | stand-in: per-keeper generator seeded matchSeed+id, slot `kRange0Slot` (`goalie_default.cpp:23`; ponytail: stream is per-keeper not per-clip; <0.6 m lateral noise) | exact shuffle stream | S | N |
| K-9 | keeper action-answer aids 0x3e/3f/43/44/46/47/48/4a/4b/4c/4d/4e/40/41/50/57/15/5/4/3/16 (OBS §11.1/§12.6: 0x46/0x4a closed live, 0x40 short-carry routed) | think dispatchers (above) | readers: keeper execs (p) | absent (GF action ids differ; aids aid 0x40/0x41 only as chain evidence) | keeper-action mapping | S | N |

### AREA 3 — referee/officials

PES fns ported (P): `JudgePoints`/`JudgeLevelFromPoints`/`JudgeLevel`
(points + 40/60/80/110 thresholds), `ShouldPlayAdvantage` (distance part),
`AdvanceAdvantage` (4 s / double-speed / 1 s-per-pass), `InAdvantageDecisionWindow`,
`ChiefRefereeSpot`/`FreeKickSpot`/`ThrowInSpot`/`ZoneOf`, whistle holds
(0.8 s contact/dive, gs/2 offside), late-card window (`MI+0x1690`<16),
offside line/test/categories (offside-port). Decoded-pending (p): gate
`FUN_14050d6a0`, scorer `FUN_1405b7470` (caller 50e58d), builder `FUN_1405b8000`
(caller 5b7f48), `FUN_14050ee40` SetPending (4 E8 callers OBS:
5b67d7/5b8282/5b88a8/5b8982 = contact/offside/dive/2nd), update `FUN_14050b490`
(caller 50c41d), `FUN_14050ff50` (caller 50b4f0), `FUN_14050f7e0` (callers
51046a/5104c8), `FUN_1405103e0`/`FUN_1405105c0`/`FUN_14050dd10`/`FUN_14050de50`,
DOGSO `FUN_1405b7390`, dive `FUN_1405b83b0` (0 E8 callers: vtable/jmp-reached),
`FUN_1405b6490`/`FUN_1405b6660`/`FUN_1405b5a00` offside, whistle `FUN_14050e770`
(7 E8 callers OBS: 50cba2/50ccbc/50e0f5/50e2f2/50e47d + protected 3f54758/3f55a64),
scene `FUN_14047f170` (caller 47e977) → `FUN_140480910`, `FUN_14047feb0`,
officials pick + factory `FUN_140a35a00`.

| J-1 | judge object Flow+0x908: +0x08 pending / +0x0c adv timer / +0x10 hold / +0x14 record / +0x58 adv copy / +0xa8 late-card slots / +0xc4 owner / +0xcc ball-out / +0xe0 warning memory (OBS duels-fouls §3) | writers: 50ee40/50b490/50de50 | readers: P: `Referee::foul` (pending/hold/window/timer); p: +0x58/+0xe0/+0xcc paths | stand-in: `Referee::foul` (`referee.hpp:49`) — GF's own pending/hold/advantage, not PES's record (no adv-copy, no warning memory, no ball-out code) | foul-machine parity | M | N |
| J-2 | JR=MatchControl+0x2910 → MI+0x1554.. copy (OBS 422030): restart/kind/card/fouler/victim/spot/frame/callback/line-x/snapshot/category/repeat/late-window; card scale 1/2/3/4, kind 2 direct / 1 offside / 3 sim (OBS §2.7) | writers: 50e770 + 7 callers; copy 422030 | readers: P: `foulType` 0/1/2/3 + `CardBook`; p: kind byte, warning level | stand-in: `Referee::foul.foulType` + `CardBook::Book` — GF scale has no warning level and no kind byte (cites round-1 E12 for the scene block) | card/kind parity | M | Y (LivePoke 7: 5=FK/6=PK + kind 2 at next foul) |
| J-3 | foul-point inputs: ai(B)+0x12a0 approach θ, +0x1478/1474 target/track, +0x14a0 intent, +0x148c fair, +0x12a7 duel byte, hu+0x3c==7, hu+0x44 sub-kind, PM+0x5d0 action ids, velocities via 6c8f0 (OBS §2.3) | publishers (92d580 contact-candidate path; exact stores open) | readers: 5b7470 scorer (p); P: `JudgeContact` geometry part | stand-in: `pes::JudgeContact` (`pesjudge.hpp:125`) — same P_angle/P_speed/P_action from GF values; ai+ target/intent/fair/duel bytes absent | scoring parity | M | N |
| J-4 | advantage machine: 4·gs window, double-speed while fouled team has it, −gs per 1..5 event, StillOn (fouling-team owner/event, candidate compare +10/+5 m slack), DelayWhistle (<5-frame event, kind-5 shot, SHOOT/0x19), ShouldPlay gates (0.53 s unlock, never kind 1/3/PK/level-4, level-3 needs attack-chance + unbooked) (OBS §3) | writer: 50b490 (state machine) | readers: P: `CheckFoul` window/double-speed/pass-cost; p: candidate compare, DelayWhistle shot-hold, level-3 attack-chance | stand-in: `AdvanceAdvantage` + distance-only `ShouldPlayAdvantage` (`pesjudge.cpp:316`; ponytail: attack-chance 50f7e0 not ported) | advantage parity | M | N |
| J-5 | attack-chance test 50f7e0 inputs (OBS `50f7e0.c` saved: team+0x254 sign, field+0x44 line, MA+0x2482c/34 ball, 22e4c+key·200 lane table, 35 m/1225 gate, zone windows ±5/±10/±3/15/17°, 70° cut-off, margins 7/6/4, last-event kinds) | n/a (pure decider) | readers: 50ff50 level-3/fallen-victim arms (p) | absent (`pesjudge.cpp:310` ponytail: distance test alone decides) | yellow-level advantage | L | N (structure + .rdata consts OBS statically) |
| J-6 | card memory judge+0xe0 (warning points ≤45 min, +⌊m/3⌋ re-score capped 3) + late-card slots → JR+0x3c/38/44/4c (OBS §2.5.4/§3-late-card) | writers: 5b7470 memory store, 50de50 late write | readers: P: `IssueDeferredCards` + `MI+0x1690`<16 window | stand-in: `CardBook::Drain`/`IssueDeferredCards` (`referee.cpp:600`) — deferral mechanism ported; +⌊m/3⌋ repeat-offender memory absent | persistent-infringement rule | S | N |
| J-7 | DOGSO inputs (MA+0x40/+0x54 last-defender lines, spot, \|z\|<20.12; OBS 5b7390) + double-jeopardy (PK: non-DOGSO 3/4→1, DOGSO 4→3; OBS §2.5.6) | n/a (pure decider) | readers: 5b7470 override + P_action +30 (p) | absent (no DOGSO symbol in `pesjudge.hpp`/`referee.hpp`; no last-defender-line rule in GF) | DOGSO parity | S | N |
| J-8 | dive check 5b83b0 inputs (DIVE 0xf, 22e4c nearest, 1.25 m, contact frame +3·gs/54, fixed-seed roll always 98 → diver booked + indirect; OBS §2.8) | n/a (event-driven on the dive) | readers: none in GF | absent (correctly: GF has no outfield dive, duel-sim report) | — | S | N |
| J-9 | offside flags MA+0x253c4/253bc/254ec/254fc + involvement 5b5a00 (keeper-disturb/3 m/8-20 m/2 m/30°/race) + pending 5b6660 + enable (G+0x2c0)+0x6e (OBS §5) | writer: detector `FUN_140a7b7a0` per ball event (offside.md; flag-block store form open per §5.3) | readers: 5b6490/5b6660 (p); P: GF offside line/test/categories/pending-whistle | stand-in: `offsiderule.cpp` + `Referee::pendingOffside` — line/test/categories ported; flag words + keeper-disturb/race involvement pending | involvement parity | M | N |
| J-10 | whistle 50e770 + 7 callers + restart/reason map (1 KO / 2 throw / 3 GK / 4 corner / 5 FK / 6 PK / 7 end; reason 4 ball-out / 5 foul / 9 shootout) + spot clamp (OBS §2.1) | writers: 7 callers (5 image + 2 protected) | readers: P: `RefereeBuffer` restart kinds; p: reason codes | stand-in: `RefereeBuffer::{desiredSetPiece,restartPos}` — kinds ported, reason codes absent | whistle parity | S | N |
| J-11 | foul-scene machine: 47f170 inputs (victim/fouler/card/kind/record +0x18/+0x1c/+0x42/booking-count/restart/15b8/period/spot) + table_foul 33 scenes + DC writes + Flow steps 3-0xb (OBS foul-choreo §1) | writer: ObserverFoul 13bc0 → 3dac0 → ring-4 (round-1 A3) | readers: DemoControl pipeline (p; GF has none) | absent (GF `FoulSequence`/`CardBook` own path; no scenes) | presentation only | L | N |
| J-12 | officials: factory 140a35a00 (chief 0x30E0 / linesman 0x3080), setup ids S.u32[0x30+4i], xorshift128-global shuffle pick (pool by mode/competition), BasePosition/OutofPlay placement, linesman flag (OBS officials-pick/setpiece-ref §5) | writers: 4187b0 entity creation, ENV-commit chain | readers: P: `ChiefRefereeSpot` teleport (`referee.cpp:668`); p: pick, diagonal, flag | stand-in: `Officials` 1+2 creation — chief spot faithful (same rule); shuffle pick + linesman diagonal/flag absent | official placement | M | N |
| J-13 | referee strictness: NONE (thresholds are immediates in protected code; no referee record read by the pipeline; OBS §2.7) | n/a | readers: none | absent (correctly: nothing to port) | — | S | N |

### AREA 4 — kick/shot selection

PES fns ported (P): zone builder `0x140698d10`/`0x140699610` → `ComputeShootZone`
+ predicates (`InShootRange`/`IsCentral`/`IsMid`/`IsClose`/`InBox`/`InHeaderRange`),
`ShootSubKind` a=1 arms (just-shoot/box/header/plan-8), plan roll/probability
(4 of 10 plans wired: 1/2/5/6), `PassForward` 3 tiers, `ReceiverDifficulty`/
`OpennessClassFromMargin`/`LaneOpennessMargin`/`DirectionClass`/`TurnClass`,
`DribbleThreat`/`DribbleDirection` (pure, wiring reverted), `FUN_140933450`/
`FUN_140931c60` (caller 930df0 / 930db6+93357f) → `PassGetSelected`/
`ArbitrationWins`/`ReceivingPoint`/`WithinBallDistanceTable`, `FUN_1409f8a50`
(callers 9f4ac1/9f86a7) → `ChooseFirstTouch` (interception/planned/guard/clamp),
`FUN_1409fa720` (caller 9fa6fb) → `RetouchLockout_s`/`RetouchAllowed`,
shot-through → `ShotRunsThrough`, stat select `FUN_14071d770` (callers
71cf36/72344c) → `KickQuality`, scatter `FUN_1408398d5` (E8-callers 0:
vtable +0x40 override via 717c50@717dfe, OBS) → `BuildScatter`/`DrawKickError`.
Decoded-pending (p): aim `0x14211f090`, `central_eval 0x14211fe20`,
`wide_eval 0x142120e60`, finesse/chip gates, ActionShoot map `0x140979170`,
plan targets +0xCC/angle +0xC0, Cross 142129590/EarlyCross 12a2d0/BackSpace
129110/Special 12bb00/OneTwo 12aeb0/SideChange 12b8b0/GoalFront 12aba0/Target
12b9e0/Safety 12b520/Long 12ad40/RespondRequest 12b230/OnePattern 133c80/
DirectHighBall 129be0, adopter overrides, delay tables `0x142110700`,
predictor `FUN_1407324b0` (callers 731ffb/732316), assist `FUN_14083c590`
(caller 717e10), keeper punt `0x140748b20`, inplay-demo `0x1407e7f20`.

| K-S1 | ap kick-request block: +0x2bc8 aim +0x2bcc height +0x2bd0 power +0x2bf4 target +0x2d04/0x2d0c curve +0x2752 kind +0x2754 category +0x2d14 flags (OBS kick-accuracy §8: req builder 833cd0; +0x5c dead zeroed) | writers: request builders (833cd0 OBS; S1: 2bc8/2bcc/2bd0 hits in 6e3xxx/6f8xxx regions, 2bf4/2d0c/2d14 0 hits → computed-base) | readers: 7324b0 predictor, 8398d5 scatter (p); P: GF launch vector | stand-in: launch dir+speed+power/height params (`ApplyPesKickError`, `humanoid.cpp:57`) — request block absent | kick-input parity | M | Y (LivePoke 6: tgt confirmed ±10° on 3 kicks; aim consumed at contact) |
| K-S2 | kick-type stat select 71d770 (0x1a/1b/1c/1d + skill +5s + weak-foot −5 + tail 0x34/0x26; OBS accuracy §2) | n/a (pure select over the ability block) | readers: P: `KickStat`+`KickQuality` | stand-in: `KickQuality` — same 4-type select + q(S) curve; weak-foot −5 and 6 other +5 bonuses absent (accuracy-port §Not-ported) | weak-foot + bonus parity | S | N |
| K-S3 | scatter records + 6×`Normal` draws + soft-min q over (I,X,F,R) + path-B/A + yaw-redraw≤6 + pSign (OBS scatter §§1-6; `7324b0.c`-class detail in-report) | writer 8398d5 (vtable-reached, see above) | readers: P: `BuildScatter`/`DrawKickError` | stand-in: records/soft-min/Normal/path-B ported; `ponytail:` Q/F1/F2=1, redraw skipped, pSign 0.5, 0.02g term out, R ramp/running neutral (`peskickerror.cpp:36-181`) | pressured-kick parity | L | N |
| K-S4 | reachability predictor 7324b0 (errScale logistic + spin tilt + 3 Normal + 6 Range, all slot ap+0x32d0; OBS `7324b0.c` saved: foot gate lVar16+0x34, stat(0x20), ANIME-id switch 0x20/26/32/21/27/3f) | n/a (pure predictor; callers 731ffb/732316) | readers: AI kick planning (p; GF uses the challenge-target rule instead, receiving-port) | absent (GF has no AI reachability predictor) | AI kick planning | M | N |
| K-S5 | pass-assist curl 83c590 (50 % roll, curl via sin/cos ×4; OBS accuracy §5) | writer 83c590 (caller 717e10) | readers: pass actions (p) | absent | curl parity | S | N |
| K-S6 | shot aim `0x14211f090` + plan desired angle +0xC0 + plan target +0xCC + image-state W (OBS carrier-ai §4.4/§5.1) | writers: plan units (angle/target), image builder (W) | readers: shoot answer (p); P: GF own aim | stand-in: goal-centre + sector post-bias aim (`elizacontroller.cpp:2098` ponytail: 11f090 not decoded); planAngle→attack-dir, imageState→0 (same-file ponytails) | aim parity | M | N |
| K-S7 | ShootSubKind full: central_eval (4 knuckle / 5 volley / else 1) + finesse 3 + chip 2 + ActionShoot power map (OBS carrier-ai §5.1) | n/a (pure decider) | readers: P: a=1 arms; p: 2/3/4/5 + power map | stand-in: `ShootSubKind` (`pescarrier.cpp:387`) — just-shoot/box/header/plan-8 + central-in-front; `ponytail:` wide/central_eval not ported | sub-kind + power parity | M | N |
| K-S8 | pass choosers beyond Forward (Cross…DirectHighBall list above) + adopter overrides (cut-back/clear→pass 3→4→9→6/cross) + reaction-delay tables + tier (BallPlayer+0x60/0x8C) (OBS carrier-ai §5 + carrier-units §3-5) | writers: think-unit evaluators (listed) | readers: P: `PassForward` tiers + 4 plans; p: the other 12 choosers + overrides + delays | stand-in: `PassForward` + Breakthrough/MiddleShot/Cross/Long rolls (`elizacontroller.cpp:2021-2270`); no difficulty tier, no other choosers | pass-variety parity | L | N |
| K-S9 | PASS_GET full: 933450 movement selector (T+0x78 target test, 6ce10 busy, 627e0 owner race, 931af0, class gates, 24840 3-slot scan, action gates 3e/4d/3/9/10) + 931c60 arbitration (OBS decompiles saved: rival-target capsule 481b00/8ff10, in-play 116c/3c950 gates) | writers: passer (T+0x78), event ring (busy/kinds) | readers: P: `Team::PassGetReceiver` (`team.cpp:369`: target + arbitration + 3-slot scan) | stand-in: target/arbitration/scan ported; 6-route fallthrough (9319a0/931360/9316e0), owner-race, class/action gates absent | receiver parity | M | N |
| K-S10 | first-touch full 9f8a50 (pre-decided R=3 one-touch, interception 9fa0b0, own-team 5 m, running-trap dead, line guard, turn-clamp 9fbce0, 9f8870/9f9600 conflict, R flags; OBS receiving §4) | writer: reaction entry-3 9f8220 (E8-callers 0: table-dispatched, OBS) | readers: P: `ChooseFirstTouch` (interception/planned/guard/clamp); p: kind-3 one-touch, conflict loop, R-flags | stand-in: trap kinds ported; conflict/9f9600/kind-3-brain-kick absent | first-touch parity | M | N |
| K-S11 | lockout full 9fa720 (L-ladder 0.05/0.26/loose+0.1(1−s²)/0.5/0.75, ×3/×2, shot-through 35 km/h/30°; OBS `9fa720.c` saved) + 9fa400 distance-table/same-side loop + 9fa520 slot/keeper/kicker gates + 9fbb40/9fbba0 dribble-touch gates | writers: trap.o (VFS install data, P-loaded) + event ring (ages) | readers: P: `RetouchLockout_s`/`RetouchAllowed`/`ShotRunsThrough`/`WithinBallDistanceTable`; p: 22-player conflict, pad/jostle tail | stand-in: ladder + shot-through + table ported; conflict loop/tail absent | re-touch parity | M | N |
| K-S12 | keeper-distribution kicks (punt 748b20, throw paths; GK think long/short; cites round-1 T+0x6d4 GKCoaching dispatch) | GK think units (above) | readers: keeper execs (p) | absent (GF keeper passes via GF rules; GKCoaching dispatch absent) | keeper-distribution parity | M | N |
| K-S13 | restart-kick choreography (throw-in pickup paths/release frames/launch band; taker rules; OBS throwin-choreo) | restart machine | readers: P: taker waterfall + throw-in hold (GF-designed `ponytail`, `referee.cpp`) | stand-in (GF restart flow, not PES's phase machines) | restart parity | M | N |

## 2. GF verdict index (where each PES function lives in GF)

Anime: `pes::Animetable/Mbinfo` (faithful tables) → `BallSelector/NetSelector/
PostSelector/KindRow/KindColumn/SpeedClass/BallBand` (`pesdodge.cpp:283-441`) →
`BallDodgeEligible/SubTestFast/SubTestMouth/NetDodgeGate/HumanDodgeGate`
(`pesdodge.cpp:420-576`, gates faithful except A-AN1/A-AN12 ponytails) →
`PickSubKind/ResolveClip` (`pestackle.cpp:236-272`) → `Humanoid::SelectAnim`
pesClip path + pose veto (`humanoid.cpp:2186`, veto stays per 5-seed table).
Keeper: `PesDeflectPoint/PesAfterCatchCarry/PesAfterCatchWalk/
PesKeeperBasePosition/PesKeeperRushFactor/PesKeeperRushLateralLimit_m/
PesKeeperRushRaceWins` (`peskeeper.*`) → `GoalieDefaultStrategy::RequestInput/
InterceptTarget/KeeperShuffleDraw` → eliza keeper/retainer branches
(`elizacontroller.cpp:294-355,495-560`). Judge: `JudgePoints/JudgeLevel* /
ShouldPlayAdvantage/AdvanceAdvantage/InAdvantageDecisionWindow/ChiefRefereeSpot`
(`pesjudge.*`) → `Referee::ContactEvent/JudgeContact/CheckFoul/
IssueDeferredCards/CommitPendingOffside` + `CardBook` + `offsiderule.cpp`.
Kick: `ComputeShootZone/ShootSubKind/PlanStartProbability/PlanRoll/PassForward/
ReceiverDifficulty/Openness/LaneMargin/DribbleThreat/DribbleDirection`
(`pescarrier.*`) → eliza shoot/plan/pass (`elizacontroller.cpp:2015-2290`) →
`PassGetSelected/ArbitrationWins/ReceivingPoint/ChooseFirstTouch/
RetouchLockout_s/ShotRunsThrough` (`pesreceive.*`) → `Team::PassGetReceiver`
→ `KickQuality/SoftMin4/Normal/BuildScatter/DrawKickError/ApplyKickError`
(`peskickerror.*`) via `ApplyPesKickError` (`humanoid.cpp:57`).

## 3. Live reads (orchestrator-routed; all read-only)

| # | field | confirming value | routed to | status |
|---|---|---|---|---|
| 1 | hu+0x4c == 2 at outside yield (A-AN1) | 2 coincides with yield, others don't | LiveTeamPlayer-2 | CLOSED-WEAK: single 2 at run10 t40.791 fr120353, keeper k=11 (PA+1=67, PM (53.7,7.2) f47.5) in FROZEN stoppage — value exists; branch coincidence unproven |
| 2 | P+0x245c transitions (A-AN5) | INIT/IDLE/RUN_LOOP/STEP_LOOP order | LiveTeamPlayer-2 | BLOCKED-STATIC: PM+0x7D8 not the AnimePlayer ptr this build (0x0-0xFFF vtable-0x1425e34d8 probe, 22 players, nothing) — anim-select base must be re-derived |
| 3 | image+0x32B0+8 0..4 + W (P-IMG cited) | states hit; 2..4→W=1.0 | LivePoke (2 items kept) | sampling (base = bp+0x32B8 u8, vtable-found; NOT a G chain) |
| 4 | MA+0x251fa/251fc at claim (K-4) | byte0=1 claim-enable, byte3 ~2 s pulse | LivePoke | PARKED: no MA lock this match |
| 5 | ai+0x1308 → −1 at catch (K-2) | countdown consumed per tick | LiveTeamPlayer-2 | CORRECTED: six countdown segments — 3 stall at 1 (289/177/short frames) then 0; 3 descend 5/11/31 → −1; −1 is also idle between claims; contact unchecked — arrival-at-catch unproven |
| 6 | ap+0x2bf4 tgt vs launch (K-S1) | bearing within ~10° pre-contact | LiveTeamPlayer-2 | BLOCKED-STATIC: same AnimePlayer base issue — needs static re-derivation |
| 7 | MI+0x1554/0x1588 at foul (J-2) | 5=FK/6=PK + kind 2 | LiveMatchBall-2 + LiveTeamPlayer-2 | CONFIRMED-PARTIAL (run 5, 912 s): 14 stoppage onsets (1554,1574): 1/3 ×6, 3/4 ×5, 4/4 ×2, 2/4 ×1, 5/5 ×1; only foul (ring-4 kind 30) at 564.2 with 1554=5 + 1588=1 (→0 at 580.3) — 5-at-foul fits FK. Ring-5 kind-37 MODE matches 1554 on 2 logged payloads. OPEN: 6=PK never seen; 1588-offside unlogged; 2=direct single case |

Match turnover: previous match over. MA relock FAILED this match (LivePoke full
streamed scan: 0 candidates; old MA stale) — item 4 (K-4 MA+0x251fa/251fc) has
no base and is parked until a lock exists. G/MI fine (frame ~52/s). ClockFreeze
pins new-match MI+0x1168/0x116c/0x1574 natives. Static rows unaffected.
PoseVeto's items (MI+0x119c/11a0 nonzero? kind-27 records? vtable-0x142597580
team-tick slot? T+0x255 flips vs holder + T+0x6a4?) are LiveMatchBall-2/
LiveTeamPlayer-2 assignments, outside these areas — no results held here;
nothing sent to PoseVeto.

## 4. Open questions (needs-live Y rows excluded above)

1. hu+0x4c writer: no S1-scan form hits it (below floor); needs base-anchored
   attribution from the net-gate outside branch, not live values.
2. A-AN5 init stores (1406b530f/5359/539f…): which set state vs neighbouring
   bytes — static attribution from 6ba900's data refs.
3. K-4 exact store: upstream of the 1404ad860 pipeline (`FUN_1404ad980` callees);
   watchpoint blocked (registers held), static trace is the avenue.
4. J-9 flag-block store form: index-form/displacement scans negative per §5.3;
   `this`-relative or bulk-copy writer still open.
5. K-S4/K-S5 trigger conditions (which kicks take predictor path A vs scatter
   path B, mode-1/2 producers): static kind-gate work, not live.
6. A-AN6 P+0x245a writer: 62 S1 hits unattributed; anchor from INIT-gate readers.

## 5. Session notes

- `relscan.py` flat-image E8 scan validated: 1,739,032 E8 sites; known callers
  reproduce (50e770→7 incl 2 protected; 50ee40→4; 6b4c10→18; 6abb30→235).
- Zero-E8-caller functions are vtable/table-reached, not dead: 8398d5 (kick
  slot +0x40 via 717c50), 9f8220 (reaction table entry 3), 9d0950/9cef10/9e8880
  (action vtables), 5b83b0 (protected jmp), 73bc70 (slot-13 dispatch).
- Decompile endpoint verified per-address; no batch/xref endpoints exist.
- Strict-faithful audit: data-table loads (A-AN9/10), bit-map ports (K-3a),
  carry rule (K-6a), sub-kind loop (A-AN4) pass; everything with a `ponytail:`
  neutral (Q/F1/F2=1, imageState=0, planAngle=attack-dir, rosterSlot=0,
  animeSlot=true) is stand-in at best.
