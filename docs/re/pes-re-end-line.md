# Why PES produces corners and goal kicks, and GF's PES loop produces none

Tags: **CONFIRMED** (live read / OBS disassembly / repo code at this commit),
**INF** (inference with stated confidence), **OPEN** (not determined + what would settle it).
No PES bytes below; VAs and immediates only. Read-only RE; no repo edits.
Scratch: `<workdrive>/gf-EndLine-scratch/` (`endline3.py` = the extractor).

## 0. Headline

- Live PES (2 matches in `liveflow.jsonl`, polled read-only): **13 open-play end-line
  outs — 5 corners, 8 goal kicks** (plus 6 shootout-period stops, excluded from the
  classification). 4 of the 8 GKs wear **kind 1 / reason 3** mid-hold (the unresolved
  "goal-kick TK" label, live-flow §7.2); 2 wear kind 3 / reason 4; all 5 corners wear
  kind 4 / reason 4. The end-line decision itself (`FUN_14050e020`: GK iff
  `dir_T·ball.x >= 0` else corner, reason 4; A5Judge §3 / §9.1) is CONFIRMED code and GF
  already ports it (`EndLineRestart`; StoppageFlow2 §3) — **the rule is not the gap**.
- The gap is upstream of the rule: in PES, balls cross the end line via (a) the kick
  **execution scatter** (yaw up to (1−q)·22.5°, pitch ditto — a shot aimed *inside* the
  mouth still lands behind the goal), (b) the **keeper deflect arm** (contact point
  1–2 m off the keeper, raised 2.4 m — built to send balls over the bar), and (c) the
  danger **clear** plus ordinary blocks. GF's pes-loop aims every shot so its launch
  vz lands inside a 0.3–2.1 m arrival band on the goal line, holds most on-target
  balls (catch window 14–34 m/s, above PES's ~21 m/s catch boundary), has no Clear
  think unit at all (kind 1 answers nothing), and computes kick quality from the stat
  ramp alone while PES soft-mins stat × situation × launch × run — so GF's scatter
  is systematically too small in exactly the pressured/off-balance situations that
  produce PES's misses.

## 1. What the live log shows (n = 2 matches, 13 open-play end-line outs)

Whistle = last fused frame before the kind transition (1 less than live-flow's
numbering). `pos` = whistle/dead-ball position `(width, length)` in m (goal line
|l| = 52.56, mouth |w| ≤ 3.66, touchline |w| = 34.06); `depth` = |l| − 52.56.
`last-in` = last poll with the ball in pitch (the position stream is a coarse
snapshot — §4); `holder` = dribble-carrier id (lags the true last touch by the
stated frames; it is NOT the toucher, live-flow §5.5).

Terminology: `end` = sign of length. `kind 1` = "TK?" (goal-kick-taker release
sequence), `kind 3` = goal kick, `kind 4` = corner.

### Match 1 H2 (`G 0x1c821d60`, 8 outs: 4 corners + 4 GK)

| # | whistle | kind/reason | end | dead pos (w,l) | depth | last-in (w,l) @fr | holder (team, lag) |
|---|---------|-------------|-----|----------------|-------|------------------|--------------------|
| G1 | 23483 | 1 / 3 | +y | (−5.6, +76.7) | 24.2 | (+9.4, +36.6) h21 @23427 | 21 (T1, −36f) |
| G2 | 25713 | 1 / 3 | −y | (−2.3, −72.3) | 19.8 | (−1.5, −34.0) @25614 | 10 (T0, −100f) |
| G3 | 28862 | 1 / 3 | +y | (−8.5, +66.4) | 13.8 | (+4.5, +39.2) h19 @28793 | 19 (T1, −39f) |
| G4 | 44640 | 1 / 3 | +y | (+5.7, +74.4) | 21.8 | sentinel (0,0)* @44439 | 19 (T1, −51f) |
| C1 | 33814 | 4 / 4 | −y | (+3.1, −65.8) | 13.2 | (−8.0, −40.0) h9 @33733 | 9 (T0, −81f) |
| C2 | 37450 | 4 / 4 | +y | (−22.3, +55.3) | 2.8 | (−2.6, +30.3) h19 @37325 | 19 (T1, −106f) |
| C3 | 38608 | 4 / 4 | +y | (+5.1, +67.9) | 15.3 | (−20.1, +51.3) @38539 | 14 (T1, −80f) |
| C4 | 41868 | 4 / 4 | +y | (−27.9, +54.3) | 1.7 | (−25.8, +51.4) @41826 | 20 (T1, −51f) |

\* G4's last-in is the `(0,0,−1)` reset sentinel — the live position before this out
is unobserved; geometry only from the whistle pos.

Direction check (A5 rule vs holder side, INF med): corners say T1 attacks +y / T0
attacks −y; the four kind-1 GKs say the opposite. The holder lags 0.5–1.9 s, so it
cannot adjudicate direction — the corner/GK *award itself* is PES's own last-toucher
verdict (CONFIRMED code), the holder is only supporting context.

### Match 2 H2-period-3 (`G 0x1de409a0`, 5 outs: 1 corner + 4 GK)

| # | whistle | kind/reason | end | dead pos (w,l) | depth | last-in (w,l) @fr | holder (team, lag) |
|---|---------|-------------|-----|----------------|-------|------------------|--------------------|
| G5 | 31868 | 3 / 4 | −y | (+5.0, −62.1) | 9.5 | (+0.5, −52.3) @31778 | 8 (T0, −91f) |
| G6 | 33051 | 3 / 4 | +y | (+3.8, +69.5) | 16.9 | (+2.6, +34.9) h21 @33004 | 21 (T1, −47f) |
| G7 | 35283 | 1 / 3 | +y | (+9.3, +60.4) | 7.9 | (−4.8, +47.2) h18 @35215 | 18 (T1, −31f) |
| G8 | 37123 | 1 / 3 | −y | (−3.9, −67.9) | 15.4 | (−1.1, −35.6) h8 @37037 | 8 (T0, −36f) |
| C5 | 42808 | 4 / 4 | −y | (+10.4, −60.6) | 8.0 | dead snap (−48.5, 0.0)* @42742 | 15 (T1, −67f) |

\* C5's last-in is a dead placement (|w| = 48.5 > 34) — live position unobserved.
Excluded: 6 period-9 shootout stops (49489/51562/53932 kind-1+penalties) — different
restart regime, not open-play end-line outs.

### Classification by source (CONFIRMED kind/region; cause INF med — see §4)

- **Keeper parry / tip over or wide (7 of 13, INF med): G1, G2, G5, G6, G8, C1, C3.**
  Signature: crosses the mouth/near-post region (|w| ≤ 9.3 at the whistle; five are
  |w| ≤ 5.7) and rests **deep** (9.5–24.2 m behind). Last live snapshots sit 12–19 m
  out near the box (G1 (9.4,36.6), G2 (−1.5,−34.0), G3 (4.5,39.2), G6 (2.6,34.9),
  G8 (−1.1,−35.6)) — an on-target ball met by the keeper/defender and lifted over,
  or a fingertip that pushes a goal-mouth ball wide of the post (G3 at w = −8.5,
  just outside the woodwork). G5 is the extreme: last seen (+0.5, −52.3), 0.3 m from
  the line, dead-centre — a goal-line save/parry.
- **Blocked / cleared at the line, low (3 of 13, INF med): C2, C4, G7.** Signature:
  crosses wide (|w| 9–28) and rests **shallow** (1.7–7.9 m). C4 (−27.9, 54.3, d1.7)
  and C2 (−22.3, 55.3, d2.8) barely cross the line near the byline — a defender's
  block/clearance or an overhit low cross that the last touch sends just over.
  Last-insit positions are right at the line (C3/C4 at l = 51.3/51.4, 1.2–1.3 m out).
- **Over-the-bar shot without a keeper touch (1 of 13, INF low): C1/C3 overlap.**
  Cannot be separated from a parry on geometry alone: a corner at the mouth with a
  deep dead ball and an attacker as last holder is either a shot over the bar or a
  fingertip parry. The holder lag (51–106 f) forbids a firm call.
- **Unresolved (2 of 13, OPEN): G4, C5** — live position stream dead before the out
  (sentinel / dead snap); kind + whistle pos only.

Counts for the implementation agent: **parry/tip ≈ 7, line block/clearance ≈ 3,
unresolved/overlap ≈ 3.** Every out is within |w| ≤ 28 of the goal centre; none
comes from a cross-field hoof — all 13 originate in the attacking third.
Pre-out velocity is NOT measurable in this log (§4) — the "last toucher + velocity"
leg of the ticket is satisfied by holder-team (proxy) and by PES's own kind award,
not by a speed number.

## 2. The PES code behind the sources (all OBS unless marked)

### 2.1 Shot target: aimed INSIDE the mouth, never deliberately wide

`0x14211f090` (carrier-units §3.3, OBS skeleton): two candidate aim points seeded
from the goal mouth at **±(halfWidth − 0.16)** (0.16 @`0x1425a9bd4`), height 0.75,
adjusted by `count/2·1` and the `width/3` pressure term; each scores
`w_lane·w_ang·w_open` with a `0x14063de20(len=dist, wid=60, h=1)` lane call plus an
11-opponent `0x140a90fd0` intercept race; sorted by `0x141e53540(.,2,0x10,...)`.
The wide evaluator `0x142120e60` and `central_eval 0x14211fe20` then gate the shot
(lane `0x14211faf0`, reach, keeper-distance, 6.25 m shot-line pressure scan).
**Shots leave the boot aimed at the paint, 0.16 m off the post — the end-line outs
from shots are execution scatter (§2.2) or keeper contact (§2.3), not aim.** VAs in
carrier-units §3.3.

### 2.2 Shot error / spread: the scatter that puts mouth-aimed shots behind the goal

`FUN_1408398d5` (kick-scatter, OBS): per kick, yaw magnitude `(1−q1)·22.5°`, pitch
`(1−q2)·22.5°`, speed `(1−q3)·0.3` (22.5 @`0x14259d618`, 0.3 @`0x142590d98`), amps
`(1−q)²·4.25+0.75` (4.25 @`0x1425ef9f8`, 0.75 @`0x14259bdd4`); `Normal` is
Box–Muller half-normal `|z|·amp/3.532` (3.532 @`0x1426550e4`); sign from success
rolls `FUN_1406ebf30`. **At q = 0 the yaw error is a full 22.5°: from 20 m that is
~8 m wide — behind the goal from a mouth aim.** Quality `qi = soft-min(I, X, F, R)`
per draw (`FUN_1408413c0`): the stat enters only the amps (`A = 0.75a+0.25`,
`a` = kick-type quality q(S) of Finishing 0x1a / Low Pass 0x1b / Lofted 0x1c /
Heading 0x1d + skill +5s, accuracy §2), while the scales take the situation
product Q (contact class, subclass, first-time/volley, body-turn, AI bits), the
launch-power/elevation penalty I, and the run term R. **A pressured, off-balance,
first-time shot scatters wide even with a 99 stat — that is PES's wide-shot and
skied-shot corner/GK mechanism.**
Predictor side (`FUN_1407324b0`, kick-launch §2–5, OBS): logistic
`errScale = 1/(1+exp(5.5·clamp01((S−40)/59)−2.5))`, one-sided exponential quantile
table @`0x1426548c0`, 3 Normal + 6 Range draws — all on **slot 2 of the kicker's
own generator** (`player+0x32d0`), never the match RNG (accuracy §6).

### 2.3 Keeper parry / tip: the deflect arm is built to send balls over the bar

Think cascade (goalkeeper §§11.1–11.4, OBS): `FUN_142112a50` → catch-eval
`FUN_14058bcf0` → 0x43 KP_CATCH; punch-eval → 0x44; **deflect-eval chain
`FUN_14058bd90 / 14058e700 / 14058c910` → 0x46**; clear/block tails. Speed ladder
(`FUN_140586a10`, OBS): catch below `loftCap·(60 + c·(10+20r))` km/h, punch
between, **deflect at/above `80 + 30c`** (at Catching c = 0.8: catch < ~76, punch
to ~104, deflect above — i.e. boundaries ≈ 21 / 29 m/s, all in *ball* km/h from
`FUN_140a57650`, plus the height/angle pre-gates and the `FUN_1405912d0` frame
race). No RNG anywhere in the choosers (§11.5). **A match-speed on-target shot
(~25–30 m/s) is above the catch boundary by construction: PES parries most shots
a GF-match would call saves.**
Deflect contact (`ActionKeeperDeflect::exec 0x1409d75d0 → 0x1440f35f0`,
keeper-port §1, OBS): contact point 1 m out on the heading±90° branch, **2 m out
on the requested-angle branch** (2 @`0x142565604`, 1 @`0x14256ae1c`, gate 120°
@`0x14256ae30`, far side heading∓60 @`0x14259a764`), **raised 2.4 m**
(@`0x1425c7660`) when no height input. The exit velocity is the anim's own —
a 1–2 m, head-high contact is geometry for over-the-bar and wide-of-the-post
deflections, i.e. exactly the §1 parry signature.

### 2.4 Clearance target: upfield through a validated corridor, never behind

Clear kind 1 `0x142109EB0` (carrier-ai §5.3, OBS): fires only on `imageA+0x31C1`
danger, answer `{type 5, a = clear angle 0x142108630}`, usually replaced by a pass
(rule 6). `0x142108630` (carrier-units §3.2, OBS structure): candidate-corridor
analysis with 60°/120°/180° bearing gates, 25.0 m (@`0x14259be20`), half-length
range, height byte +0x31 as h/100, corridor validation `0x14063d2f0` +
`0x1421B7D40`; tie-break OPEN (the `0x141e532e0()/6` read is the 54 fps game
speed, not an RNG draw). **A deliberate clearance never targets the end line.**
Corners credited to clearances in §1 are therefore (a) the same `FUN_1408398d5`
scatter applied to the clear kick (it is a Kick), or (b) deflections off bodies.
Block deflection physics: every ball–object contact goes through the single
`.trace` response `FUN_14086d6f0` (ball-collision §0–1, OBS: per-collider-class
restitution E / retention T, analytic goal-frame normals, net as `GoalnetFilm`
with SIDENET/UPPERNET/BACKNET touches). **Keeper block arms 0x47 KP_BLOCK /
0x48 KP_BLOCK_LATE and the outfield body block reuse this path — a block contact
near the line is a corner/GK with no think decision behind it.**

## 3. What GF's PES loop does instead (repo code at this commit)

### 3.1 Shot targeting — aimed at the post, banded under the bar

- Pes-loop decision (`elizacontroller.cpp:2500–2541`): `pes::ShootSubKind(shoot,
  carrier)` (pescarrier.hpp; Shoot unit ported, Clear kind 1 **answers nothing** —
  `FillUnitTable` lists kinds 0/4/9/7/2/3/6/10/11 only) → aim = goal centre biased
  to **±goalHalfWidth (3.66) by sector** (the post itself, 0.16 m wider than PES),
  blended 0.7 aim / 0.3 facing (`autoDirectionBias = 1.0`), power from goalDist.
  **Bias 1.0 also bypasses the ±18° stored-direction clamp** (humanoid.cpp:946).
- Execution (humanoid.cpp:933–1067, both loops): `GetShotVector` → vz forced into
  the arrival band `ClampShotArrivalVelocityZ` (**crosses the line at 0.3–2.1 m**,
  gameplaytuning.hpp:216–235) → `ApplyPesKickError` (ported §2.2 scatter,
  humanoid.cpp:1057).
- **Differences the implementation agent must change:** (i) GF's vz band has no PES
  counterpart — PES lets the pitch scatter (±22.5°) sail balls over (that's §1's
  deep dead balls); GF pulls every on-mouth strike back under the bar, converting
  would-be overs into on-target balls the keeper then holds. (ii) `pes::KickQuality`
  (peskickerror.cpp:83) is a pure stat ramp; PES's qi is the **soft-min of four
  factors** (stat × situation × launch × run, scatter §6) — GF over-estimates q for
  pressured/off-balance/first-time shots, shrinking exactly the scatter that makes
  PES's misses. (iii) PES validates the lane to the aim (`0x14211faf0`, 3.0 m
  corridor); GF's 0.7/0.3 blend can aim through bodies. Non-pes-loop only:
  `AI_GetShotDirection` (AIsupport/AIfunctions.cpp:1346) uses a post-inset by
  Finishing (0.2–3.7 m + calmness) + xG lane pricing — closer to PES's 0.16 m idea
  than the pes-loop's post-line aim.

### 3.2 Keeper save outcome — catches what PES parries, parries safely by construction

- The deflect handler (humanoid.cpp:1190–1308) runs in **both** loops: hold iff
  `relativeSpeed ≤ GetKeeperCatchSpeed_ms` (**14 + 20·catching m/s**, i.e. up to
  34 m/s ≈ 122 km/h; gameplaytuning.hpp:615–623), else parry with
  `KeeperTouchAwayFromOwnGoal` + push `2.2 + 3·clearing` + speed
  `|v|·0.3 + |move|·2.5` + lift `+1.2` (= half of PES's 2.4).
- **Differences:** (i) GF has **no punch band and no deflect-above tier** —
  goalkeeper §11.9's GF row: PES's ladder is absolute ball speed (catch ~21,
  punch ~29, deflect above, in m/s terms), GF's 14–34 hold window sits over PES's
  whole catch band and parries the rest, so GF holds balls PES deflects and
  deflects balls PES would punch. (ii) GF's parry is **always pushed away from its
  own goal** (touch[0] rewrite, :1299–1300); PES's deflect exits along the anim's
  contact geometry. (iii) GF has no frame-race reach test (`FUN_1405912d0`), no
  opponent-beats-him gate, no aim-error ladder, no block arms 0x47/0x48 (§11.1–11.4
  GF rows) — the reach decision is the contact itself. Net effect: fewer parries
  than PES (narrower parry population), and the parries that happen are steered
  away from goal instead of over it — both dry up GKs and the parried corner.

### 3.3 Clearance — GF has no danger clear; the panic hoof goes to the safest lane

- Pes-loop carrier has **no Clear unit** (§3.1): under `danger` the walk yields no
  type-5 answer, so a pressed defender in the pes-loop dribbles or passes instead
  of hoofing — the §2.4 clearance (and its scatter tail that produces PES's
  line-blocks) never fires.
- What fires instead is GF's own `_AddPanicPass` (elizacontroller.cpp:3366–3400,
  shared by both loops): probe the default away direction ±0.25π, pick the safest
  of three by `_GetPassingOdds` at an 18 m probe point, `isClearance = true`,
  `autoDirectionBias = 0`. It is a sensible-away hoof, not PES's validated
  corridor angle — and being GF's own error model upstream of the ported scatter,
  its miss distribution is unmeasured.
- Keeper distribution, for completeness: converter ported (kp 3/2 → power/angle,
  d² ladder 100/196/361/484/676, 50 m cap, 3 m walk-out; goalkeeper §11.6) — not a
  suspected cause.

### Why the loop yields zero (mechanism, INF high)

GF's pes-loop chain — sector-post aim → vz arrival band → stat-ramp quality →
14–34 m/s catch window → away-from-goal parries → no danger clear — removes every
PES miss path of §2 except the yaw scatter on good-quality kicks, which the
soft-min gap then shrinks. StoppageFlow2 §1/§3 (seed-7 two-minute pes-loop runs:
five end-line events, all goals; no non-goal end-line out) is the measurement this
explains. The order to restore them: (1) `KickQuality` soft-min parity, (2) Clear
kind-1 unit + `0x142108630` corridor angle, (3) vz-band removal for shots (or a
PES-cited over-bar path), (4) keeper ladder parity (punch/deflect tiers + reach
race).

## 4. Method limits (read before reusing the table)

- **Velocity before the out is not in the log** (CONFIRMED): 19 columns, no
  velocity. Consecutive-poll deltas at the out are 10–85 m in one 17.5 ms frame
  (up to ~4800 m/s) — the position is a coarse/dead snapshot, not physics.
- **Crossing width is the whistle (dead) position**, not the line-crossing point:
  the ball rolls/settle-moves after crossing (C2 refined (+1.1, +79.6) → (−22.3,
  +55.3) across 37 polls). Treat |w| as ±3 m.
- **Holder ≠ toucher** (CONFIRMED live-flow §5.5): the carrier id lags 27–106 f
  and contradicts the A5 direction in 4/8 match-1 stops; PES's own kind award is
  the authoritative last-touch verdict.
- G4/C5 geometry is whistle-only (dead stream before the out) — marked OPEN.
- Ghidra bridge was down this session; all PES VAs/constants are cited from
  `docs/re` OBS reports re-verified above, plus capstone-checked VAs already in
  those reports. No new disassembly was produced.
