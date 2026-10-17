# PES 2021 player Playing Styles — full decode

Scope: the 21 player-side Playing Styles (distinct from the 7 COM cards in
`pes-re-com-styles.md`). Storage, every in-exe reader with VA, per-style
effect, the dt18 style-suffixed constants, Japanese-name check, and the GF
gap list. Read-only decode; no engine edits. Tags: **CONFIRMED**
(disasm/decompile/live/data) or **INF** (inferred, confidence stated).

Scratch: `<workdrive>/gf-PlayStyles-scratch/` (decompiles `dec_*.c`,
`team_stock.cpkraw`, `stock_constant_player.bin`, VFS dumps).

## 0. The list (CONFIRMED, binding table 0x1434A10D0 entries 42–62)

Engine bit order, bit = index into the `block+0x48` u32. Three independent
confirmations: the script-binding name table, the GK/SB/CF position-mask
triangulation (§4), carrier-ai §6.1.

| bit | engine name | UI name | EDIT order |
|---|---|---|---|
| 0 | GOAL_POACHER | Goal Poacher | 1 |
| 1 | DUMMY_RUNNER | Dummy Runner | 2 |
| 2 | FOX_IN_THE_BOX | Fox in the Box | 3 |
| 3 | POST_PLAYER | Target Man | 13 |
| 4 | CHANCE_MAKER | Creative Playmaker | 14 |
| 5 | WING_STRIKER | Prolific Winger | 4 |
| 6 | INSIDE_PLAYER | Roaming Flank | 18 |
| 7 | CROSSER | Cross Specialist | 19 |
| 8 | CLASSIC_NO_10 | Classic No. 10 | 5 |
| 9 | FREE_ROAMING | Hole Player | 6 |
| 10 | BOX_TO_BOX | Box-to-Box | 7 |
| 11 | ANCHOR_MAN | Anchor Man | 8 |
| 12 | ENFORCER | The Destroyer | 9 |
| 13 | PLAY_MAKER | Orchestrator | 20 |
| 14 | BUILD_UP | Build Up | 15 |
| 15 | OVERLAP | Extra Frontman | 10 |
| 16 | OFFENSIVE_SB | Offensive Full-back | 11 |
| 17 | DEFENSIVE_SB | Defensive Full-back | 12 |
| 18 | INNERLAP | Full-back Finisher | 21 |
| 19 | OFFENSIVE_GK | Offensive Goalkeeper | 16 |
| 20 | CLASSICAL_GK | Defensive Goalkeeper | 17 |

> **RETRACTED 16-10-26 (Main's Ghidra check):** the reader census in §2,
> the per-style INF verdicts in §3, and the exclusion claim in §10 are
> wrong. `FUN_1408CB0B0` has **four** callers, not one: `FUN_14063E890`
> plus `FUN_140A63FE0`, `FUN_140A64030` (generic wrappers taking the
> style BIT as param_2 with the slot from `team+0x4C00`) and
> `FUN_141465180`. `FUN_140A64030` alone has ~213 call sites and
> `FUN_140A63FE0` 13, across team/off-ball code — e.g. `FUN_14053B900`
> (bit 14 Build Up, then bit 0 Goal Poacher), `FUN_1405A0D20` (bit 17
> Defensive FB), `FUN_140591D70` (bit 2 Fox in the Box). My caller
> census of `0x1408CB0B0` returned only `0x14063E938` because the other
> three callers reach it through a path my scan missed (likely an
> indirect tail-call — `0x14063E938` is itself just `call 0x1408CB0B0`).
> A fresh agent will histogram the bit arguments. Do not use the
> retracted verdicts; the storage (§1), compat masks (§4), dead tables
> (§5), dt18 key census (§6), constants (§7), and live protocol (§9)
> stand.
CHANCE_MAKER/WING_STRIKER/INSIDE_PLAYER/FREE_ROAMING (bits 4/5/6/9),
PLAY_MAKER (bit 13), OVERLAP/INNERLAP (bits 15/18). The compat-mask
triangulation (§4) is what pins them: GK={19,20}, CF={0,1,2,3},
WG={4,5,6,7} force the assignment; only the *names* of the middle four
could swap. No Japanese pack is installed (dt20 has 15 locales, none
Japanese), the exe `.rdata` key tables carry English keys only, and no
Japanese description strings exist for these fields — the Japanese-name
check is COLD (no source for it in this install).

## 1. Storage (CONFIRMED)

- **Player.bin**: field id 0x40, 5-bit EDIT-order enum (matchlogic §1.2;
  accessor 0x141F0F560 has no Ghidra record — offset taken from that doc).
- **EDIT → engine pipeline** (all three stages disassembled):
  1. `FUN_1414C0300(rec)`: EDIT field 0x3b, clamp 0..0x15, remap through
     22 dwords at **0x142979080** =
     `[0,1,2,3,6,9,10,11,14,12,19,15,17,4,5,18,20,21,7,8,13,16]`.
  2. `FUN_141FC0AD0(out, t)`: intermediate → engine bit
     `1→0, 2→1, 3→2, 4→3, 5→4, 6→5, 7→6, 8→7, 9→8, 10→9, 11→10,
     12→12, 13→13, 14→11, 15→16, 16→18, 17→17, 18→14, 19→15,
     20→19, 21→20`, else invalid.
  3. `FUN_141FE9F40(block, …)`: `*(u32*)(block+0x48) \|= 1 << bit`;
     then the 7 COM cards from table **0x14352AC10** OR into `block+0x4C`.
- **In-match block** (0x128 B/player): `+0x40` u64 skills, `+0x48` u32
  style mask, `+0x4C` u32 COM mask. Object view: `0x140A3AF90` →
  `roster+idx*0x128+0xA8`, `0x141B59E20` = +0x30, so `sub+0x48` u64
  skills, **`sub+0x50` u32 styles**, `sub+0x54` u32 COMs (sub = block−8).
- **Live-stat copy**: `0x140A489F0(i)` + `0x14124DAD0` (= +0x70) = block
  bytes; `[copy+0x70+0x48]` = style u32 (CONFIRMED, `14124dad0` is one
  `lea`). This — not the sub-object — is the path most style readers use.
- Per-player AI copy `FUN_140922BC0` → `0x140A58F00` moves
  `sub+0x00..0x48` (ten qwords) only: **skills yes, style word no**.
- EDIT-save 0x17C table packed region: undecoded (unchanged).

## 2. Readers (CONFIRMED sites)

Census: 94 getter-host functions (147 `0x141B59E20` calls) + the
`14124DAD0` chain (324 callers, 235 hosts) swept for `+0x48` bit tests.

| # | site | test | system |
|---|---|---|---|
| R1 | `FUN_14063E890` → `FUN_1408CB0B0` @0x14063E890/0x1408CB0B0 | style bit AND `compat[slot]` (**0x142613D20**) | gate used by R2a/R2b |
| R2a | `FUN_142125C80` @0x1421264D5 (`mov r8d,8`) | **bit 8 CLASSIC_NO_10** | short-forward receiver: immediate return |
| R2b | `FUN_142132660` @0x142132A2B (literal 8) | **bit 8 CLASSIC_NO_10** | receiver scorer: immediate return |
| R3 | `FUN_14211AB10` @0x14211AC8D (`shr eax,7`) | **bit 7 CROSSER** (via live copy) | Cross-plan base-50 fallback |
| R4 | `FUN_140601110` @0x1406012EA (21-bit ROL loop) | all 21 bits → fn table **0x1436F2260** | team+0x228 priority fill (**inert**, §5) |
| R5a | `FUN_14078E5A0` @0x14078E9B4 | **bit 12 ENFORCER** (`shr 0xC`), +1 if `140A40BC0(T)==3`, 0 by team-sheet flag | standing-tackle reaction entry (`FUN_14096FDA0` family; defender-decisions §2 decodes the effect) |
| R5b | `FUN_1407902C0` @0x140790441 | **bit 12** same formula | tackle slot 13 readiness (`FUN_1407902C0`; feeds `FUN_14078B550` record) |
| R5c | `FUN_14096FDA0` @~0x14096FE?? (`>>0xC & 1`) | **bit 12** same formula | tackle geometry `A` level: behind-tolerance 11.25°/45°/0°, touch-timing 0.075/0.1/0.125·gs (defender-decisions §2) |
| R5d | `FUN_140971BA0` @~0x140971F85 (`>>0xC & 1`) | **bit 12** | slide-tackle aggression `L` += 1 (defender-decisions §3) |
| R6a | `FUN_14092BD60` @0x14092C576 (`>>0x13`, `>>0x14`) | **bits 19+20** (both GK styles) | keeper rush/select: non-OffGK + OffGK-bit → mode 0x10; DefGK-bit → 0x20 |
| R6b | `FUN_1409DD470` @0x1409DDB69/83 | **bit 20 then 19** (three-way) | keeper distribution thresholds: DefGK `f·0.25/0.0`, OffGK `f+1.1/…`, neither `f·0.6/…` (floats 0x142592228, 0x1425A3590, 0x14257A190) |
| R6c | `FUN_1409DE8C0` @0x1409DED4A | **bit 19** + keeper-slot checks | keeper distribution branch |
| R6d | `FUN_1409DF7D0` @0x1409E0032 (`>>0x14==0 && >>0x13!=0 → 17.0`) | **bit 19** (not-20 AND 19) | keeper range constant 15.0→**17.0** (0x14259BE0C→0x14259D614), full-attack bonus added |
| R6e | `FUN_1409E03BE` @0x1409E048A (`>>0x14`, `>>0x13`) | **bits 20+19** | keeper range pair (15.0 default; OffGK-only → 20.0/… branch) |
| R6f | `FUN_142115BB2` @0x142115CEC (`>>0x13`, 48.0→**43.0**) | **bit 19** | keeper angle window 48°→**43°** (0x142588214→0x1425CFA38) |
| R6g | `FUN_1421163F4` @0x142116852 (`>>0x13`, `>>0x14` → bVar27/28, 6 use sites) | **bits 19+20** | keeper positioning branches (six sites in one function) |

NOT a reader: `0x140A16E57` (`[x+0x48]=1` writes are a different object's
flags — sweep false positive).

## 3. Per-style table (effect per bit)

| bit | style | readers | effect | verdict |
|---|---|---|---|---|
| 0 | Goal Poacher | R1; R4 fn[0] | compat CF/SS only; inert priority cell | CONFIRMED gate; effect INF (no consumer beyond) |
| 1 | Dummy Runner | R1; R4 fn[1] | compat CF/AMF/SS-ish; +dt18 `_dummyRunner` Diagonal constants selected by run-quality gate, NOT the bit (§6) | CONFIRMED gate; per-bit effect INF |
| 2 | Fox in the Box | R1; R4 fn[2] | compat only | CONFIRMED gate; effect INF |
| 3 | Target Man | R1; R4 fn[3] | compat only (long-target rule keys on height stat, not the bit) | CONFIRMED gate; effect INF |
| 4 | Creative Playmaker | R1; R4 fn[4] | compat (SS/WG/AMF rows) | CONFIRMED gate; effect INF |
| 5 | Prolific Winger | R1; R4 fn[5] | compat (WG row) | CONFIRMED gate; effect INF |
| 6 | Roaming Flank | R1; R4 fn[6] | compat (WG/SMF rows) | CONFIRMED gate; effect INF |
| 7 | Cross Specialist | R1; R3; R4 fn[7] | **Cross base-50 fallback** (with skill 14, team byte, bypasses) | CONFIRMED |
| 8 | Classic No. 10 | R1; R2a; R2b; R4 fn[8] | **preferred pass target**: both receiver scorers return a No.10-in-a-pocket immediately | CONFIRMED |
| 9 | Hole Player | R1; R4 fn[9] | compat (AMF/CMF/WG rows) | CONFIRMED gate; effect INF |
| 10 | Box-to-Box | R1; R4 fn[10] | compat (DMF/CMF rows); dt18 `_BoxToBox` marking constants are role-selected, NOT bit-selected (§6) | CONFIRMED gate; per-bit effect INF |
| 11 | Anchor Man | R1; R4 fn[11] | compat DMF | CONFIRMED gate; effect INF |
| 12 | The Destroyer | R1; R5a–d; R4 fn[12] | **tackle aggression**: standing-tackle entry level, behind-tolerance 11.25°/45°/0°, touch-timing 0.075/0.1/0.125·gs, slide reach `L+=1` (constants in §7) | CONFIRMED |
| 13 | Orchestrator | R1; R4 fn[13] | compat DMF/CMF rows | CONFIRMED gate; effect INF |
| 14 | Build Up | R1; R4 fn[14] | compat CB row; `approachRateMax_buildUp` reader untraced (OPEN) | CONFIRMED gate; effect INF |
| 15 | Extra Frontman | R1; R4 fn[15] | compat CB row | CONFIRMED gate; effect INF |
| 16 | Offensive Full-back | R1; R4 fn[16] | compat SB row | CONFIRMED gate; effect INF |
| 17 | Defensive Full-back | R1; R4 fn[17] | compat SB row | CONFIRMED gate; effect INF |
| 18 | Full-back Finisher | R1; R4 fn[18] | compat SB row | CONFIRMED gate; effect INF |
| 19 | Offensive GK | R1; R6a–g; R4 fn[19] | **keeper distribution/positioning**: rush mode 0x10, range 17.0/20.0 terms, angle window 43°, six positioning branches | CONFIRMED |
| 20 | Defensive GK | R1; R6a–g; R4 fn[20] | **keeper distribution**: `f·0.25`, mode 0x20, conservative thresholds | CONFIRMED |

Cross-reference (styles × COM system): the only shared unit is the Cross
plan (style 7 + COM 3 + skill 14 in one fallback chain, com-styles §6.3).
All other COM-gated evaluators ignore the player style mask. Defender
branches (R5) live in the engaging-defender/tackle system
(defender-decisions §2–3, already ported in part); keeper branches (R6)
live in the 0x1409Dxxx distribution + 0x14092xxx rush code.

## 4. Compat masks 0x142613D20 (CONFIRMED, live == disk)

10-way formation slot → allowed style bits:
`0:GK 0x180000 (19,20) | 1:CB 0xD000 (12,14,15) | 2:SB 0x70000
(16,17,18) | 3:DMF 0x3C00 (10,11,12,13) | 4:CMF 0x3700 (8,9,10,12) |
5:SMF 0x6D0 (4,6,7,8,9,10) | 6:AMF 0x712 (1,4,5,8,9,10) |
7:SS 0x113 (0,4,8) | 8:WG 0xF0 (4,5,6,7) | 9:CF 0xF (0,1,2,3)`.
Slot order triangulated from mask content (GK/CF/WG/SB hard anchors;
middle four INF high).

## 5. Dead tables + dt18 playStyle.o

- `0x1436F2110` (41 skill thunks) / `0x1436F2260` (21 style thunks):
  **all-zero in image and live**; `FUN_140601110` skips everything;
  `team+0x228` priorities all 1.0 (matches off-ball EV-OFF-0006).
- `playStyle.o` (`constant_player.bin`, 2224 B: u32 total + 22×96 B +
  24 B dir): **zero-filled in stock dt18 and the live 4cc winner**.
  Runtime-filled template whose filler never runs. Port nothing from it.

## 6. dt18 style-suffixed constants (the corrected lead)

Census of style-ish keys in the exe JSON key tables (CONFIRMED strings):
- defenceMark: 12× `*_BoxToBox` (adjustDist, checkBackFollow*,
  checkZoneAreaRate, markContinueDist, remainMark*, zoneMark*; plus
  `approachRateMax_buildUp`, `approachRateMax_side`).
- diagonalRun: 4× `*_dummyRunner` (distXBpMax, fanAngleHalfMargin,
  fanAngle, fanFar).
- lineBreak: 7× `*_lineBreaker` (checkAreaScore, checkBodyAngle*,
  checkDistEyesOff, checkInterceptScore, checkSideAngleEyesOff,
  searchCount).
- `buildUpShort` (pass mode), `buildUp_value` (team attacking-type enum —
  a tactic, not the style).

Verdicts on selection (CONFIRMED where stated):
- `_BoxToBox` marking block `0x140533BC0`: selected by
  `FUN_14052C440` (marking-pass role check: slot == side index or
  `team+0x78`) **AND** MATCH_UP quality gate `0x1405C0F00(…,0x3A)` —
  **NOT the style bit**. The suffix is a tuning profile for a
  box-to-box *situation*, not a branch on the player having the style.
- `_dummyRunner` Diagonal branch `0x140613CC0`: selected by the Diagonal
  quality gate `0x1405C0F00(…,0x21)` — **NOT the style bit** (no style
  gate in `0x140613910`).
- `_lineBreaker`: readers at the seven `edx=0xBE` sites — OPEN.
- `approachRateMax_buildUp`, `buildUpShort`: readers untraced — OPEN.
- Japanese names: COLD — English keys only, no JP pack installed, no JP
  description strings in the exe.

## 7. Constants observed at the style branches (OBS)

Destroyer: behind-tolerance 11.25°/45°/0° (0x1425CC680/0x14259BE30/
0x14259BE20), touch-timing 0.075/0.1/0.125·gs, slide `d ≥ 4.0+0.8L`,
capsule `1.5−0.2L × 0.6d / 0.5d+0.5`, team-mode+1 (`140A40BC0==3`).
GK: 48°→43° (0x142588214→0x1425CFA38), 15.0→17.0 (0x14259BE0C→
0x14259D614), 0.25/1.1/0.6 (0x142592228/0x1425A3590/0x14257A190),
rush modes 0x10/0x20. Cross: base 50/70, clamp [40,80] (com-styles §6.3).

## 8. GF gap list (implementers)

READS TODAY (GF has them): `Player` enum (EDIT order ✓ matches §0);
`SuitsRole` (≈ compat masks, but GF's own table — replace with
0x142613D20 reads from the install); GK style branches in
`elizacontroller.cpp:540`/`goalie_default.cpp:159` (stand-ins — rewire to
R6 VAs); Destroyer card checks (elizacontroller 823/832 — extend to the
R5 aggression terms); COM plan roll (has COM/skill terms, missing the
style-7 Cross fallback @0x14211AC8D); `playerdata` import tags.
MISSING: EDIT-save style field in `pes/editsave.cpp` (style arrives only
via profile_xml); Classic-No.10 receiver preference (R2a/R2b — no GF
counterpart); per-style run-priority table (correctly absent — inert);
dt18 `*_BoxToBox/*_dummyRunner` variant selection (GF has no
tuning-profile switch on run/marking context — port the *selectors*,
0x14052C440/0x1405C0F00, not the style bit).
NO-PES-SITE knobs (mark `// ponytail:` or delete): `GetDepthOffset`,
`GetWidthOffset`, `GetPoacherTargetX`, `GetShotAppetite`,
`GetShootingRangeBonus`, `GetFinisherDemand`, `GetCreatorVision`,
`GetPassTypeBias`, `GetSpaceRatingWeight`, `GetDribbleDrive`,
`GetOpponentRepel`, `GetDribbleCenterPull`, `GetDribbleVelocity`,
`GetAttackingRunAffinity` — defined in `playingstyles.*`, no caller in
`src/` except depth/width/poacher/dribble fours listed in §8's grep.
OPEN (not in this pass): `_lineBreaker` readers, `approachRateMax_buildUp`
reader, EDIT 0x17C style field, live verification with a match loaded
(this session: gMatchContext set, roster/team pointers null — all live
table reads were menu-state).

## 9. Live session 16-10-26 (match live; writes performed + restored)

Correction to the first half of this section (kept for the record): the
"no live match" verdict was wrong — my context chain was. `0x1436F3DC8`
is not the match context in this session (null roster slots), but the
match IS live: 22 BallPlayers, live team objects from the team-tick
holder (`H=*(0x1436F3F98)=0x372284E0`, vtable 0x142597580 ✓), team data
via `H+0x970` (`T0=0x1D46E578`, `T1=0x1D48C628`, formation slots differ
per team), one roster object at **0x1D47A6D8** holding both XIs
(slots 0–10 home, 11–21 away, pids 70801–70823, names EDIT-ciphered),
and per-player live-stat copies (double-buffered, pid-keyed,
e.g. 0x1D47AE38/0x1D47DD38 for pid 70807). `MI+0x1168/0x116c/0x1574`
never touched. Every write below = one player's `block+0x48` **plus**
both copy mirrors (`copy+0x70+0x48`), logged in `stylelab.log`,
readback-verified, all restored (final verification: 7/7 blocks read
original values).

Step 1 — bit map on real squads: PASSES. All 22 live style words are
compat-legal under the §4 map: GKs bit 19, CBs bit 12, SBs bit 16,
DMF/CMF/AMF bit 10, AMF bit 1, SS bits 4/0 (SS row = 0,4,8 ✓✓).
Present bits: 0,1,4,10,12,16,19. (No external EDIT reference available;
internal-consistency verdict, INF high.)

Step 4 — function tables `0x1436F2110`/`0x1436F2260` all-zero **mid-match**
(OBS-live): the team-init priority fill is inert; `T+0x228` reads all
1.0 on both live teams. §5 stands.

Write protocol: each experiment = 100–120 s answer sampler (22 BPs'
`+0x41E4` adopted kind + payload at 2 Hz) with the style set, vs a
240-sample baseline. Adopted answers are sparse in open play (base:
5160/5280 reads = "none" 25; dribbles 84–147 the dominant answer) —
all firing observations below are NULL (no effect seen), labelled as
such, NOT refutations.

| experiment | write (logged) | window | observation |
|---|---|---|---|
| No.10 | slot 7 (AMF) `0x400→0x100`, block only | 200 samples | no pass named slot 7; passes near-absent phase-wide. NULL |
| Destroyer | slot 5 (DMF) `0x400→0x1000`, block + 2 mirrors | 200 samples | slot 5: 184×none, 8×dribble, 4×target, 4×direct-high (vs base 240×none). Tackle reactions bypass adopted answers, so R5 has no adopted-answer observable. NULL |
| Crosser | slot 3 (RB) `0x10000→0x80`, block + 2 mirrors | 200 samples | zero CENTERING answers phase-wide. NULL (static CONFIRMED stands) |
| DefGK | slot 0 (GK) `0x80000→0x100000`, block + 2 mirrors | 220 samples | keepers: 3× set-play (home), 3× dribble (away). No distribution watched (rare event). NULL (static CONFIRMED stands) |
| Poacher/Dummy/Frontman/Box | slots 9/7/10/14 → bits 0/1/15/10, blocks + mirrors | 220 samples | targets behave normally (dribbles, shots, one-twos); no receiver-preference shift, no new run shapes. NULL; off-ball sweep (13/14 copy-uses in 0x14060xxxx–0x14064xxxx touch no `+0x48` word) agrees: run choice never branches on the style mask |

Step 3 static remainder: `_lineBreaker` key string
(`checkAreaScore_lineBreaker` @0x142B58B90) has exactly one code xref
(`0x141E78847`, inside the lineBreak JSON deserializer); the seven
`edx=0xBE` sites are HUD-marker collisions; quality-gate census finds
**no 0x20 (LineBreak) gate call** — `_lineBreaker` selection lives inside
the unscouted LineBreak scorer (OPEN). `approachRateMax_buildUp` reader
untraced (OPEN).

All writes restored — `stylelab.log` ends with four RESTORE lines with
matching readbacks; final 7-block verification all OK.

## 10. Exclusion: the other 16 bits really have no in-match consumer

The five hiding places, ranked and closed (16-10-26 session):

1. **Direct-block readers bypassing both getters — CLOSED.** Exhaustive
   per-function sweep of every `dword [r+0x50]` site image-wide (13,351
   prefilter hits → per-function disasm with bit-test filter): in match
   code the only shift/bt-test sites are the known R-sites; the two
   remaining `bt` sites (`0x140702CE7`, `0x140704C6E`) use `[r+0x50]` as
   an 11-value enum index (`cmp eax,0xa; bt r9d,eax`), not a mask.
   No direct `block+0x48` reader exists outside the `141B59E20` (147
   calls) and `14124DAD0` (324 calls) chains, both fully censused.
2. **OPEN dt18 selectors (narrow):** (a) `_lineBreaker` — the LineBreak
   scorer has no run-quality gate (no 0x20 call to `0x1405C0F00`
   anywhere), no imm-index registry read (lineBreak is not 0xBE; the
   seven `edx=0xBE` sites are HUD-marker writes), and its key string has
   exactly one xref (its own deserializer). The struct is
   register-passed; store map recovered
   (`+0x10/+0x18/+0x20/+0x28/+0x30/+0x38` variant pairs). Whatever picks
   base vs `_lineBreaker` is inside the unscouted scorer. (b)
   `approachRateMax_buildUp` (+0x20) — deserializer store map recovered
   (75 keys, `+0x1c/+0x20/+0x24` trio); no reader found among the 16
   defenceMark getter-hosts. Both OPEN, both bounded to single-field
   selectors that the established pattern (role/context gates, never
   the style bit) predicts.
3. **Data-driven behavior — CLOSED.** No ai_selector blob exists in the
   VFS (`common/match/` holds only the nine constant_*.bin files). Every
   known data input (constant_team/player/match) is consumed by swept
   code; none of its readers tests the style bit except the R-sites.
4. **Denuvo VM bodies — bounded.** 38 vm/mutated bodies in
   0x140400000–0x140B00000 (census `nodes.tsv`); all 21 reader functions
   are `plain` bodies. None of the 38 calls any style/skill/COM
   accessor (147+324+935+38+10 caller sets checked); VM bodies with live
   callers are index/flag helpers (`0x140A62A20` returns a slot index
   for byte-flag stores) with visible continuations showing no mask
   reads; callerless ones are unreachable residue. Residual only: a
   fully self-contained VM reader (own roster walk + own bit test) —
   excluded in practice (no motive: style checks are not tamper
   targets; every found reader decompiles clean).
5. **Outside the match — CONFIRMED present, out of scope.** The EDIT
   style field reader (`0x1414C0300`) has 24 callers in 21 menu-side
   functions (0x140B5C7E0 UI binding with `loaderSwf_*` strings,
   0x140C6DD60 … 0x141FE9F40 loader chain): squad UI, formation setup,
   save/load. In-match behavior is unaffected.

Net: 5 styles branch in-match code (bits 7, 8, 12, 19, 20); 16 do not.
The per-role differentiation lives in the dt18 tuning profiles (selected
by tactical context: `0x14052C440` role check, `0x1405C0F00` run-quality
gates), the compat masks (§4) gate who plays where, and the suffix names
(`_BoxToBox`, `_dummyRunner`) label tuning profiles, not player-gated
branches. Most styles are labels with a compat row — dummied out of the
sim by design, not by omission.
