# Foundation inventory: match-, analysis- and ball-owned PES state (MatchBall)

Owner: FoundationMatchBall. Team/player records owned by FoundationTeamPlayer (T+, PI/PA/PM/hu(i),
G+0x25cf8, rec+0xd4); cited here as blocking inputs, not duplicated. T+0x255 row lives with
FoundationTeamPlayer (setter FUN_140a428d0, rule FUN_1404fe9c0, PoseVeto §15).

Notation: G=`*(0x1436F3DC8)`, MI=`*(G+0x270)`, MA=`*(G+0x18)`, gs=`FUN_141e532d0()`=54.
OBS/INF per report. GF symbols from origin/main 2026-10-15.

## 0. Ranked structures (by ported/pending gates unblocked)

| rank | structure | fields | gates unblocked | faithful/stand-in/absent |
|---|---|---|---|---|
| 1 | A. Event ring (E ctx + rings 0/1/4/5, K table) | 8 | ~10: dodge window+busy, reaction writers A/B, receive lockout, offside passer search, kickoff timer, foul ring, possession-rule steps 2-3, kind-5 shot gates | 0/3/5 |
| 2 | B. MA ball cluster (2482c/34 point, 2483c holder, 24840 list, 2601c carrier, 24844 arrival) | 5 | ~8: press head, chase predicate, off-ball ref, shape ball, counter master gate, possession-rule steps 1+3, keeper claim, mark MATCH_UP | 0/3/2 |
| 3 | E. MI clock/phase block | 14 | ~8: every phase gate, strategy periods, stamina break, setpiece mode gates, kickoff press, detector enable, staged-restart gate | 0/7/7 |
| 4 | C. MA rank tables (24710/247be, 247d4, 22e4c) | 3 | ~6: press fill+writer, mark compare, keeper distances, outlets, contact nearest | 0/1/2 |
| 5 | F. Ball record + prediction fns | 7 | ~5: all prediction consumers, velocity floor, K=9 trigger, challenge target, high-ball gates | 2/3/2 |
| 6 | H. Match setup + tick order | 4 | ~3 structural: latch-vs-K12 order, detector parity, strategy rating | 0/2/2 |
| 7 | D. MA line block | 2 | ~3: offside line, concept ownLine/teamMid (D2 done) | 1/1/0 |
| 8 | G. FieldInfo | 6 | ~4 small: edge/box/net/circle parity (mostly ported constants) | 0/4/2 |

Totals: 9 structures, 54 rows: faithful 3, stand-in 26, absent 22, resolved 3 (E1x, E6x, C1-verified).
Effort: S 30, M 17, L 7. Needs live read: Y 24, N 30.

## 1. Field table

`field | meaning (OBS/INF) | PES writer VA(s) | PES readers (ported=P / pending=p) |
GF status + symbol | blocking items | size | live?`

### A. Event ring

A1 | ring-0 ball-event records, 8x800B (`E+0x1900` count, `+0x1904` head, newest copy `E+0x180698`;
empty sentinel 0x37) | OBS | writer FUN_140a3d060 (0x140a3d060, 56 call sites, PoseVeto §11) via
recorder FUN_140a3dc40 (ring map @0x142642e60) | readers: FUN_140a3e190(E,0) (P: receive lockout
FUN_1409fa720, dodge window step 8, offside passer FUN_140a3e190, kickoff timer FUN_140a74ec0 via
ring-5); p: busy FUN_140a6ce10, writers A/B dispatch FUN_140502190 | stand-in:
Match::ballEvents[2] (match.hpp:146, 2-deep, 4 kinds, {player,teamID,kind,time_ms}, pushed at
Team::TouchBall team.cpp:366) + ob::BallEvent (pesoffball.hpp:191, PES-numbered kind, age, pos,
target, per-team world) | kinds 5-14, busy set, reaction writers, passer search, payload team/frame | M | N
(static complete; Y for per-K trigger names)
A2 | ring-1 previous-event keys (`E+0x1908`, count `+0x3208`, head `+0x320c`, key rec+0x1c) | OBS |
writer FUN_140a3dc40 | readers: FUN_140a3e7d0 (P: none in GF), FUN_140a627e0(ctx,1) (p: possession-rule
step 3 second-newest actor) | absent | possession-rule step 3 | S | N
A3 | ring-4 foul records (kind 0x1e=30 only per table @0x142642e60) | OBS (foul-choreography §1.2,
defence-leftovers §2.1) | writer FUN_140a3dac0 <- FUN_140a13bc0 (ObserverFoul record match,
MI+0x116c==2 && MI+0x1574==5 path) | readers: foul/card pipeline | absent (GF referee.cpp own path,
no ring) | foul-event parity | S | N
A4 | ring-5 restart records (kind 0x25; +0x04/+0x08 frame, +0x14 team, +0x18 MODE 1/3/4/5/6/7,
+0x1c point, +0x20 player; written at the restart, TransitionsRE §1.2 live) | OBS live+code |
writer FUN_140a3dc40 | readers: FUN_140a74ec0 kickoff timer (P: inPlaySinceRestart_ms, teamAIcontroller
~252), FUN_14062c400/FUN_140509890 corner gates (p) | stand-in: referee buffer (no MODE, no per-kind
record) | kickoff-gated shape/press/counter (5s/8s line hold, 4s/7s mark, 6s counter) | M | Y (MODE
write timing per restart kind)
A5 | K detector->kind table (56 sites: K=0,1,6,7,8,9,10,11,12,13,14,15-26,29,30,32-47,51-54;
K=0x1d+phase site 140a13f5d; data-driven sites 140a1ae43/140a1a4a9; NEVER produced: 2,3,4,5,27,28,31,
48,49,50) | OBS sites, INF names | writers: detectors FUN_140a13640..FUN_140a1aec0 + 223-object loop
FUN_140a0d3f0/0a0d520/0a0e400 | reader: FUN_140a3dc40 ring router | absent (GF synthesizes kTouch/
kPass/kHighPass/kShot at the kick in BallEventKindOf, team.cpp:31) | every kind reader; busy-set names
(K=9-14 producers: 140a178d0/17810/15978/14d40/15f50/1a4c0) | L | Y (per-producer trigger confirm)
A6 | ring payload layout (rec[0]=K,[1..2]=frame,[3]=state byte,[4]=p6; +4 team for ball events;
+0x10 newest frame; kind read at ring +0x14; ring-5 +0x14 team/+0x18 MODE) | OBS, WITH OPEN
INCONSISTENCY (PoseVeto §8: kind-7/8 records from FUN_140a0dfd0 write a frame at +4 where ball-event
readers expect a team) | writer FUN_140a3d060 | readers: window step 8 (team+frame), 502190 dispatch
(kind+actor+frame) | stand-in (player/teamID/time_ms only; no state/p6/MODE/target) | window team test,
dispatch, MODE gates | M | Y (which producers feed E+0x180698 live)
A7 | busy predicate FUN_140a6ce10 (kind 9-14; kinds 6/7 only with no carrier; kind 5 never with ctx3=0)
| OBS | no writer (pure reader of ring-0 + carrier) | readers: human-gate tail (P: rosterSlotValue=0
stand-in, elizacontroller HumanDodgeGate), FUN_1404fdd20, FUN_1405027a0 | absent | dodge tail,
reaction writers | M (needs A5 kinds + B4 carrier) | N
A8 | ring-0 count/head E+0x1900/0x1904 + ring-1 count/head E+0x3208/0x320c | OBS | writer FUN_140a3dc40 |
reader FUN_140a3e190/FUN_140a3e7d0 | absent (GF fixed 2-deep, no counts) | ring depth parity (8-deep
history, second-newest) | S | N

### B. MA ball cluster

B1 | MA+0x2482c/+0x24834 (f32 x/z analysis-ball point) | OBS live (kickcap2 mirror; defshape §388/§644) |
MA LOCATED by LivePoke scan (MA=0x7ffffb000018 that session; findma.py: sim-ball x±2.5m at +0x2482c, z
at +0x24834; relocate per match on G change; G+0x18 pointer path confirmed dead) — per-tick writer at
0x140a6f29e stays NOFUNC (interior) | readers (P: as Ball::Predict(0) via w.ball teamAIcontroller:2256,
PressPickInput.ballX/Z; p: a4f1e0 zone classifier, 5807d0 stale-ball veto) | stand-in
Ball::Predict(0)/positionBuffer (diverges from MA in flight) | MA-vs-live error series in flight | M |
Y (watcher-ready: mirror pair + sim ball below)
B2 | MA+0x2483c (u32 ball holder id) | OBS (defshape §399; chance-lanes §528) | setter VERIFIED
FUN_140a72a60 (E5c; caller chain open); MA locatable by findma.py | readers (p: fe9c0 step 3 via
4f4be0, 593290 fallback, press head) | stand-in GetBallRetainer()/designated | possession rule,
fallback ref | S | Y (watcher-ready)
B3 | MA+0x24840 (per-team 11x{id,dist-or-frames} list; [this+0x1e8]+0x24840 camera-target; offside §209
11x{id,frames}) | OBS layout, INF cell semantics | writer NOT FOUND; MA locatable by findma.py |
readers (p: press pass-1 first defender, duels-fouls §464, keeper 10.6, offside loop, counter 5807d0
dist) | absent | presser pick, keeper, offside, counter | M | Y (watcher-ready)
B4 | MA+0x2601c (u8 ball carrier id, >=22 none; FUN_140a627e0(ctx,0)) | OBS (pose-veto §34; press-layer
§158; counter §85) | clear VERIFIED FUN_140a71c30 (E5b); SET 0x14045725a NOFUNC; MA locatable by
findma.py | readers: 5807d0, fe9c0 step 1, off-ball ref, 502190, duel-sim D, dash-reserve | stand-in
GetBallRetainer()/designated | possession + every carrier gate | M | Y (watcher-ready)
B5 | MA+0x24844 (2nd f32 of 24840 records = arrival value; FUN_14044e020(MA+0x24840,id) reads
+4+(rank+11*team)*8; FUN_1404c8ea0 pairwise via MA+0x22e4c lists) | OBS addressing (PoseVeto §18) |
writer NOT FOUND | reader: FUN_1404f4be0 chase timed tests (p, structure OBS) | absent | chase
predicate | M | Y

### C. MA rank tables

C1 | MA+0x24710 (per-side sorted {f32 d2,u32 id} list) + MA+0x247be (rank byte per man; bucket +0/+11 by
side; idx = rank + 11*side) | OBS layout static (press-layer-port §40/§134) + OBS VALUES VERIFIED LIVE
by LivePoke (MA=0x7ffffb000018 that session): rank bytes MA+0x247be[0..21] are per-side permutations of
0..10; entry MA+0x24710+(rank[k]+11*side)*8 {f32 d2, u32 id} matches (player-ball)^2 within max(1,2%);
21 of 22 within 1%. MA found by SCAN, not a pointer chain (depth-2 via G+0x18 found nothing):
findma.py scans writables (numpy f32) for sim-ball x±2.5m at MA+0x2482c and z at MA+0x24834, accepts at
>=18 players, retries torn reads; address moves per match, relocate on G change
(~/ghidra-projects/scratch/LivePoke/findma.py). `_DAT_1425301f0` = ucrtbase sqrtf IAT slot, so
FUN_14044e0d0 returns sqrt(d2) metres; flagA = <4.0m. | writer FUN_140536fc0 (named, not ported, input
not wired) | readers (P: PressFillMan sqrt(tableDistSq)<4.0 with caller-computed d2, pespress.hpp:198 +
FlagAIsDistanceNotSquaredDistance test; p: mark compare attack-decisions §477, keeper 6m gate defshape
§531) | stand-in (caller computes d2; writer not wired; VALUES now live-confirmed, layout frozen) |
press writer wiring, mark/keeper gates | L | done (Y this session; re-verify per build only)
C2 | MA+0x247d4 (f32 vs PI+0x38 heading) | OBS single reader (LivePoke §2: 5a7fe0 heading test) |
writer NOT FOUND | reader: Chance stop-leaf 5a7fe0 | absent | runner stop leaves (not ported: GF has no
per-player pass-aim record) | S | Y
C3 | MA+0x22e4c (per-player sorted {id,dist}x11 lists, stride 200, rank byte +0xb2+id; FUN_140a71540
outlet list; FUN_1404c8ea0 pairwise) | OBS (defshape §473/§1016; ai-map §41: NOT a keeper table) |
writer NOT FOUND | readers (P: ad hoc sorts at pesoffball.cpp:559, keeper elizacontroller; p: outlets,
contact nearest 1.5m, keeper 10.6.1 neighbour list) | absent (no stored lists) | keeper/outlet/
contact parity | M | Y

### D. MA line block

D1 | MA+0x40+0x14*side (deepest outfielder world x) / MA+0x3c+0x14*side (front-mid mean) /
MA+0x44+0x14*t (team lineX) | OBS (pesteamconcepts.hpp:172, pesoffball.hpp:206, peskeeper.hpp:191) |
writers: team-side line builders (FUN_1405fbcb0 chain via FUN_14051d9e0; team-owned, see
FoundationTeamPlayer) | readers (P: RunWorld.lineX = AI_GetOffsideLine, teamAIcontroller:2268;
concept ownLine/teamMid; keeper offsideLineX) | stand-in AI_GetOffsideLine (final-line median error
0.000m live, midfield 0.009m; defshape port) | line parity for concepts/offside | M | Y (done:
live-described; open: midfield depth/spacing code)
D2 | MA+0x3fb8 (+0xc48 = opponent registered position) / MA+0x4b54 (opponent slot-point z) | OBS
(OffBallSelectors: NOT new state; arithmetic views) | no writer (views) | readers: off-ball position
class/Attacking-Area tests | faithful Position()/slotPoint (pesoffball.hpp) | none | — | N

### E. MI clock/phase block

E1 | MI+0x116c (phase: 1 in play, 2 staged-restart/stop; PoseVeto §2 step 5; contact §93; 5807d0) |
OBS readers (502190 line 131 `*(lVar11+0x116c)==1` with sibling +0x1775/+0x1198 on the SAME lVar11 from
the FUN_140a48a10/48910 match-context path) | half-time transition writer (clock-live §5: 116c +=
1198-saved, 1168++) for the MI-base object | readers: every phase gate (P: IsInPlay()/IsInSetPiece()
+ e_MatchPhase_Penalties, Match::Process match.cpp:664) | stand-in (3-state collapsed; phase-2
keeper-hold semantics missing) | all phase gates | S | N
E1x | CONFLICT-1 RESOLVED — same displacement, DIFFERENT struct (OBS raw decompile /tmp/115a0.txt):
the inventory's "FUN_140a1197a writes +0x116c as a clock" site is an address INSIDE FUN_140a115a0's
case-7 statistics tail (`116c += MI+0x1198 - FieldInfo[5]; 1168 += 1; 728 = 116c/1168`, lines 308-319),
whose base param_2 carries the +0x710-0x724 counters and the case-0xd +0x118c/+0x1190/+0x1198/+0x1d0/
+0x1e0 stamp block — a per-kind statistics accumulator (sum/count/mean), not *(G+0x270). MI+0x1194/
+0x1198 writes in case-0xd sit on the same statistics param_2, not on MI either. The open-play ==1
readers stay OBS on the MI-base object. GF: no change (IsInPlay() stands) | none | — | N
E6x | CONFLICT-2 RESOLVED — (team,idx) at +0x18 AND +0x1c, both OBS, different records: ring-0 reader
FUN_140a3e190 returns piVar2+5 for n>=1 (word index 5 = byte +0x14), so its (team,idx) key read
`+4/+7`-style lands at record+0x18/+0x1c (115a0.txt:214-221: `*(lVar20+0x1c)==0` walk,
`FUN_140a60320(lVar20+4,...)` key compare); ring-1 reader FUN_140a3e7d0 returns the 8-byte key at
`*(rec+0x1C)` i.e. rec+0x1C (OBS decompile); newest copy E+0x180698 layout from 3dc40's iVar1==7 arm
(rec+5 -> +0x180698 qwords). So: ring-0 payload (team,idx) base is +0x14 with the key at +0x18;
ring-1 key is at +0x1C of the 0x1908-ring record. GF: Match::BallEvent needs the +0x18 key on ring-0
reads and +0x1c on ring-1 reads when rings deepen | ring depth parity | S | N
E5 | MI+0x11a0 (possession timer, frames; constant-team §6: >=10 gate) | VERIFIED init writers (OBS
dec.sh mb-writers): FUN_1408ae72b `*(RBX+0x11a0)=10` (init w/ +0x11b4/+0x11cc/+0x11ec detector pair,
+0x11a4=2, +0x1198 vtable UNK_1426107d0); FUN_14100ce84 `*(RBX+0x11a0)=EAX` (param copy);
FUN_14118221c `*(RBX+0x11a0)=EDI` (+0x11a4-0x11f8 zero block, reset path); base RBX unverified
(noanalysis leaf) but the triple-store shape matches MI init/reset/copy | readers (P:
possessionChange_ms/possessionAge, teamAIcontroller:252) | stand-in | per-tick increment writer
still open (case-0x1b `+=1` per PoseVeto §19 unconfirmed live) | S | Y
E5b | MA+0x2601c clear VERIFIED (OBS bridge FUN_140a71c30): `*(param_1+0x26014/0x26018/0x2601c)=0xff`
triple clear + `+0x2600c/+0x26010=0xff`, with +0x48=0xff id-init — the MA constructor path (calls
140a77780 loop, 140a71e10, 140a723f0, 140a72890). SET at 0x14045725a: NOFUNC (no function boundary,
interior store) — base unverified | carrier readers (5807d0, fe9c0, off-ball) | stand-in
GetBallRetainer()/designated | set-site function boundary (linear sweep from 140457070 head) | S | N
E5c | MA+0x2483c setter VERIFIED (OBS bridge): FUN_140a72a60 (FUN_140a72a68 region): conditional
`if (*(p+0x2483c)!=param_2) *(p+0x2483c)=param_2`. Value source (param_2 caller chain) not traced |
fallback readers (593290, chance-lanes, 5807d0) | absent | caller/value-source trace | S | N
E5d | MA+0x2482c init VERIFIED (OBS bridge FUN_140a6f240): `*(p+0x2482c)=0; *(p+0x24834)=0` inside the
MA constructor (before 1404567e0 list inits + 140a71c30 carrier clear). Per-tick store at 0x140a6f29e:
NOFUNC (interior) — base unverified | ball-point readers | stand-in Ball::Predict(0) | per-tick writer
confirmation (needs MA located live first) | S | Y
E6 | MI+0x118c/0x1190/0x1194 (wall-us stamps ~1.74-1.99e9, ~1e5/s drift; INF med-high, clock-live §5) |
INF | writer: FUN_140a115a0 case 0xd (`118c += dt; 1190 += 1; 1d0=(118c/1190)/rate`) | readers: UNTRACED
(open) | absent (no stamps in GF; pesclock.cpp is a derived game-clock, different object) | stamp-model
ports (display source, half_base/gain) | M | Y (hold-118c HUD test; u32 ~2e9 scan; reader xrefs)
E7 | MI+0x11b4 (==0 detector enable for K=9/10 producers) | OBS gate | writer unknown | readers:
140a178d0/17810 gates | absent | busy-kind triggers | S | Y
E8 | MI+0x1748[22] (on-pitch flags; writer-A gate `MI+0x1748[id]!=0`) | OBS | writer unknown (match
setup) | readers (P: IsActive()/PressPickInput.onField/RunWorld via GetActivePlayers) | stand-in
(active ⟺ on-pitch; no detached flag) | writer ports, press, off-ball | S | N
E9 | MI+0x1775 (mode: 0x17 team-tick branch, 0x18 event gate, 3/9/10 carrier restart vetoes, 0x17
counter-tick gate) | OBS | writer unknown | readers: 4f6c30 branch, 502190 gate, carrier units t=2/5
veto | absent (GF: IsInSetPiece + referee buffer only) | mode-gated ports | M | Y
E10 | MI+0x14c8+4*side (kickoff type: 1 two-man; setpiece-choreo §11.9) | OBS | writer unknown |
readers: 633940/623750 4s press (p) | absent | kickoff press | S | Y
E11 | MI+0x11d4 (==2 staged restart; keeper-hold path with anime 0x28) | OBS (counter §1.3) | writer
unknown | reader: 5807d0 gate (p) | absent (referee buffer partial cover) | counter gate | S | Y
E12 | MI+0x1554/155c/1564/1574/1584-1594/15b8/1690/11bb/135c (foul+restart scene: MODE, spot, OOP
reason 5=foul, players, advantage/card) | OBS (foul-choreography §§1.2-1.3) | writers: ObserverFoul
slot 1 FUN_140a13bc0, FUN_140447230 stop stamp | readers: foul/card/whistle pipeline | absent (GF
referee own state machine) | foul parity | L | Y
E13 | MI+0x11a4/0x1c0/0x1c4/0x728 (non-0x1b sum + 119c/11a0 averages + rate) | OBS (PoseVeto §19) |
writer: event-switch non-0x1b arm + half-time transition | readers: none ported | absent | none
today | S | N
E14 | MI+0x1168 consumers FUN_140a74d70 minute (floor(t/3600), 45-min boundary) + FUN_140a3c050 period
seconds | OBS | — | readers (P: pesclock.cpp/tactics-clock, matchTime_ms/matchTimeExact_ms
match.hpp:459) | stand-in (GF own tick-driven clock; tactics-clock report: same period ends) | clock
parity | S | N

### F. Ball record + prediction

F1 | G+0x40 BallInfoRef → +0x88 live xyz (|v|≈14 m/s pass live; defshape §18 continuous 60 polls) |
OBS live | writer: engine sim step | readers (P: Ball::positionBuffer/Predict(0), ball.cpp) |
faithful | — | S | N
F2 | BallInfo +0x2948 velocity m/s (FUN_140a57720; defshape §398) | OBS live | writer: engine |
readers (P: Ball::GetMovement()) | faithful | — | S | N
F3 | FUN_140a57800 (ball position/prediction point; press-layer refX/refZ; offside b=s*ball.x;
s1#21 goal-centre INF) | OBS addressing, INF goal-point | writer: engine prediction | readers (P:
Ball::Predict(t)/GetPredictionArray + pesball physics port ball.cpp:136; p: MA-vs-live divergence) |
stand-in (GF predicts with ported PES flight; MA mirror never compared) | ref parity, divergence
bound | M | Y (MA-vs-live error series in flight)
F4 | FUN_140a6c6d0(p,out,minSpeed) velocity with floor 0.6944444/gs (0x14259bdd0; state-5 slow players
read 2.5km/h instead of 0; contact-port §19 correction, attack-leftovers §20, dash-reserve §82) | OBS |
no writer (pure) | readers (P: Player::GetMovement() unfloored; p: floor + state-5 rule) | stand-in
(no floor) | contact closing speed, dash reserve, duel sim | S | N
F6 | FUN_140a6c990 (mover velocity) + FUN_140458740 (bearing of predicted displacement; offball PBrg) |
OBS | no writer (pure) | readers (P: dodger pos+movement in NetDodgeGate pesdodge.cpp; PostPlay PBrg
pending) | stand-in (ported where used) | — | S | N
F7 | BallInfo aux: +0x27a0 set-piece point (defshape §466), +0x2798 change-int (K=10 payload),
+4 y 1.5m high-ball tests (defshape §148/§813: 57800 prediction + heading 75°) | OBS | writer: engine |
readers: deep-line ball height, K=10 detector (p) | absent (GF: Predict z + no 2798 int) | K=10,
high-ball plan gates | S | Y

### G. FieldInfo (chain G+0x2a90 -> +0x10 -> +0x10 -> +0x04; 0x4c block {refcount,f32 F[18]};
*(G+0x2aa8) is null live — fieldinfo §2; net-dodge §49 conflict noted: dodge gates dereference
G+0x2aa8 in play, needs the live re-check)

G1 | F0/F1 52.56/34.06 line OUTER edges (+0.06 vs centres) | OBS live+classifier 140a72e70 | writer:
Denuvo-VM ctor 0x140a85f40 (no exe constant, fieldinfo §4 exhaustive) | readers (P: pitchHalfW/H 52.5/
34 centres) | stand-in (−0.06 on axes) | edge parity (in-goal, corner flags ±52.47) | S | N
G2 | F2/F3/F15/F16 3.66/2.44/0.12/0.12 goal inner + frame | OBS (collision §3.2: post axis ±52.50/±3.72,
bar 2.50) | same ctor | readers (P: goalInnerHalfWidth/Height/FrameThickness exact; goalHalfWidth 3.7
vs F2+F16/2=3.72) | stand-in (+0.02 post centre) | frame parity | S | N
G3 | F4/F5/F6/F7/F8 47.06/9.16/36.14/20.12/41.56 boxes + spot | OBS (classifier; keeper; F6/F7 8cm
inside Law 1) | same ctor | readers (P: goalAreaHalfWidth exact; box 36.0/20.15 vs 36.14/20.12;
F4/F8: GF hard-codes 11.0 matchdata.hpp:85) | stand-in (two edits: 36.0->36.14, 20.15->20.12; add
F4/F8) | box/spot parity | S | N
G4 | F9/F13/F14 2.5/54.32031/54.43359 net H + roof/floor backs (1404aa610+1404bece0 lerp; NOT two
depths) | OBS code | same ctor | readers (P: goalDepth 2.55 vs roof 1.76031/floor 1.87359 from F0) |
stand-in (GF net 0.68-0.79m too deep) | net-slab parity | S | N
G5 | F17 9.15 circle/arc radius (classifier `bx<F17 && bz<F17` -> bit 0x4000) | OBS code (upgraded) |
same ctor | readers: none in GF (comment in pessetpieceteam.hpp:56 only) | absent | circle/arc tests
| S | N
G6 | F10/F11/F12 1.109375/2.33984375/51.171875 UNNAMED, no consumer found (FI-01) | open | writer
unknown | readers: none | absent (unneeded: GF ports nothing that reads them) | FI-01 filter scan
(firescan3.py: consecutive [+0x28][0x2c][0x30][0x34] readers) | S | Y

### H. Match setup + tick order

H1 | G+0x2ad0 match setup, +0xc400+side*0x2804 team blocks, +0xc404 rating float (INF team strength;
match-management §69, tactics §121) | INF(high) | writer: match load | reader: strategy decider
FUN_14051e970 (P: pesstrategy mode-0 without rating term) | absent | strategy decider strength/SPD
diff | S | N
H2 | match tick FUN_14042b9f0 10 steps (OBS PoseVeto §17): 1 FUN_140426a90, 2 FUN_1404267a0,
3 FUN_1404267f0 EVENT STAGE (calls detector loop FUN_140a0d3f0 -> K=12), 4 FUN_140426860,
5 FUN_140426760, 6 FUN_140426810, 7 FUN_140426940, 8 FUN_140426a80, 9 FUN_140426750,
10 FUN_140426cd0 (message switch; stamina-live §8.9 driver chain 42b800/42b6c0/42b7d0/42b830) | OBS
order, INF step semantics | writer: engine message dispatch | readers: all analysis/team writers sit
inside | stand-in Match::Process (match.cpp:664: collisions -> referee -> ball+momentum snapshot ->
mental images -> teams -> officials -> possession stats/designated -> stalemate -> contacts -> clock/
actualTime -> goal-line -> goals -> cards/subs/heatmap -> UpdateBodies) | per-step mapping (which PES
step holds physics vs events vs teams) | M | N
H3 | team tick FUN_1404f6c30 via FUN_14041edd7 (dec.sh body 0x14041edd7-0x14041ef05 line 22 calls
4f6c30; Ghidra zero xrefs: INDIRECT vtable/ptr call; position vs 42b9f0 OPEN; PoseVeto §19) | OBS
existence+call, open placement | writer: engine indirect dispatch | readers: fe9c0 latch, 581980
counter writer, 525530 run manager | stand-in Team::Process (after mental images, before officials;
latch therefore same-tick, NOT previous-tick as PES if team tick runs after event stage) |
latch-vs-K12 order question (§17: K=12 reads previous tick's latch unless team tick first) | M | Y
(caller hunt: computed-index/vtable scan from 41edd7)
H4 | detector loop (FUN_140a0d3f0/0a0d520/0a0e400 over 223 objects at +0xb600 -> FUN_140a3dc40;
input record type 0x14 FUN_140a0dfd0 pushes K=7+8) | OBS (PoseVeto §5) | writer: engine per-step-3 |
readers: ring writers | absent (GF pushes 4-kind events only at Team::TouchBall) | detector parity
(deflect/out/clear/save/whistle triggers) | L | Y

## 2. Ball prediction: PES fns vs GF (acceptance item)

| PES | meaning | GF | verdict |
|---|---|---|---|
| FUN_140a57800 (ball pos/prediction; readers: press ref, offside, s1#21) | predicted point at horizon | Ball::Predict(t)/GetPredictionArray + MentalImage::GetBallPrediction(t) (ball.cpp:136 ported PES flight: IsResting gate, keep-factor k^(0.01*54), grounder/bounce/spin per pesball) | stand-in (flight ported; MA-mirror divergence unmeasured; mental blend EnforceMaximumDeviation 2.5m has no PES counterpart) |
| FUN_140a57720 (+0x2948 vel) | live velocity | Ball::GetMovement() | faithful |
| FUN_140a6c6d0 (floored velocity, 0.69444/gs) | mover velocity | Player::GetMovement() | stand-in (no floor/state-5 rule; F4) |
| FUN_140a6cf70 (slot samples/contact points) | indexed sample | none (challenge uses Predict(200/210) elizacontroller:1006) | absent (F5) |
| FUN_140a6c990 + 140458740 (dodger vel/bearing) | mover state | dodger pos+movement (net-dodge port) | stand-in, ported where used |
| GF-only extras with no PES counterpart | — | MentalImage 2.5m EnforceMaximumDeviation; ballPredictionSize ring; RecentHit memory; weather wind/wetness (roadmap 3.8) | DESIGNED (label in code) |

## 3. Event ring vs GF BallEvent (acceptance item)

PES: 55 kinds (56 sites §11 + data-driven; ring router @0x142642e60: 7->0 ball, 8->1, 11->2, 23->3,
30->4 foul, 0x25->5 restart, 14->7, else->10); ring-0 8-deep 800B payloads (kind/team/frames/state/p6/
target/point/player); rings 1 (keys), 4 (foul), 5 (restart MODE). Kind vocabulary OBS-by-reader:
1/2/3 passes, 0x10 deliberate touch, 5 shot, 6/7 dribble-touch, 4 trap, 8/11/12 deliberate play,
9 deflection INF-med, 10 out, 13 save/touch, 14 dead-ball/whistle, 0x1e foul, 0x25 restart.
GF: Match::ballEvents[2] 4 kinds (kTouch/kPass/kHighPass/kShot, pesreceive.hpp:89) pushed only at
kicks (team.cpp:366) + ob::BallEvent PES-numbered kind/age/pos/target per-team world. Missing: 51
kinds, rings 1/4/5, 8-deep history, payload (state/p6/MODE/point), non-kick producers (deflect/out/
save/whistle/restart). Readers by kind: EvKick (1,2,3,0x10) in offball stops (pesoffball.cpp:607);
IsKick (kPass/kHighPass) + kShot branch in receive (pesreceive.cpp:58,136); passInFlight in
PassGetReceiver (team.cpp:375); shot.ownTeamShot (playercontroller.cpp:412).

## 4. Tick build order (acceptance item)

PES FUN_14042b9f0: [1 426a90] [2 4267a0] [3 4267f0 EVENT STAGE + detectors] [4 426860] [5 426760]
[6 426810] [7 426940] [8 426a80] [9 426750] [10 426cd0 msg switch]. Team tick 4f6c30 (latch fe9c0,
counter writer 581980, run manager 525530) via INDIRECT 41edd7 — placement vs the 10 steps OPEN;
K=12 (step 3) reads previous tick's latch unless team tick runs first.
GF Match::Process: collisions -> referee -> ball snapshot+Process -> mental images -> teams ->
officials -> possession stats/designated/stalemate -> contacts -> clock/actualTime -> goal-line
crossing -> goals/score/events -> cards/subs/heatmap -> UpdateBodies. Key deltas: GF snapshots ball
BEFORE physics (PES order inside steps unknown); GF builds mental images BEFORE teams (PES image timing
unplaced); GF referee runs BEFORE ball/teams (PES 426cd0 is step 10, last); GF detector-equivalent
(kick push) fires inside team/player updates, not in a pre-team event stage.

## 5. Counts

Structures 9 (A-H + §2-4). Rows 54: faithful 3 (D2, F1, F2), stand-in 26, absent 22, resolved 3
(E1x, E6x + C1 live-verified). Effort S 30 / M 17 / L 7. Live-read Y 24 / N 30 (C1 + B1-location done;
watcher list below is the remaining Y).
Largest single scheduling win: rank 1-3 (A+B+E = 30 rows) carry ~26 gates; rank 4-5 (C+F = 10 rows)
carry ~11; H is structural prerequisite to A/B ordering.
Orchestrator conflicts: (1) RESOLVED — same disp, different structs (statistics param_2 vs MI base);
(2) RESOLVED — +0x18 on ring-0, +0x1c on ring-1, both OBS. Owned writers verified: MI+0x11a0 triple
init (8ae72b=10, 100ce84=EAX, 118221c=EDI), MA+0x2601c clear (71c30), MA+0x2483c setter (72a60),
MA+0x2482c init (6f240); MA per-tick writers locatable now that MA is found by scan.

## 6. LivePoke next-match watcher: exact address list (MA-relative; MA via findma.py scan, NOT G+0x18)

Chain: G = *(u64*)0x1436F3DC8; B = *(G+0x40), sim ball xyz f32 at B+0x88/+0x8C/+0x90;
MI = *(G+0x270), frame u32 MI+0x1198. MA = findma.py scan (writables, numpy f32: sim-ball x±2.5m at
candidate+0x2482c AND z at +0x24834; verify rank bytes + C1 table >=18/22; retry torn reads; relocate
on G change; last session MA=0x7ffffb000018 — DO NOT reuse, rescan).
Sample each tick with frame + sim ball:
MA+0x2482c (f32 ball x mirror), MA+0x24834 (f32 ball z mirror) — B1: MA-vs-live error series;
MA+0x2483c (u32 holder) — B2; MA+0x24840[0..21] 11x{u32 id, f32 dist-or-frames} per team (44 B per
team) — B3 (+0x24844 arrival column — B5, same read); MA+0x2601c (u8 carrier, 0xff none) — B4;
MA+0x24710[0..21] 22x{f32 d2, u32 id} + MA+0x247be[0..21] rank bytes — C1 regression (expect 21-22/22
within 1%); MA+0x22e4c[0..21] per-player lists (200 B stride, rank +0xb2+id) — C3 first light;
MA+0x25cf8/0x25d38 team-instruction id/value tables (team*4+slot, LivePoke: FUN_140a63950 reads
ctx+0x18=MA — confirms MA base independently) — bonus. Skip: E-block (MI-side, no MA read needed).
