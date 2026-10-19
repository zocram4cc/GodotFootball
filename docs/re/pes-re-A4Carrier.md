# A4Carrier: PES 2021 per-player brain (MainEval `FUN_142110D30`), its units, arbitration and command path

Scope: decode only. Read-only: no engine edits, no live writes. Builds on `pes-re-carrier-ai.md` (§0–§6),
`pes-re-carrier-units.md` (§1–§5), `pes-re-attack-decisions.md` (§2–§3), `pes-re-player-ai.md` (§2),
`pes-re-playing-styles.md` (§0–§8), `pes-re-kick-launch.md` (§1–§2), `pes-re-movestep-wave1.md`.

Evidence tags:
- **C-now**: decompiled or read this session. Scratch: `<workdrive>/gf-A4Carrier-scratch/dec_<VA>.c` (Ghidra bridge, image `pes_full.bin`, base 0x140000000). The VA is the function entry; line numbers cite that file.
- **C-prior**: OBS from the cited doc, which raw-checked it. Not re-read here.
- **INF(H/M/L)**: inference, with confidence.
- **OPEN**: what blocks it, and the experiment that would settle it.

Where a doc and this session's decompile disagree, the decompile wins and the conflict is listed in §11.

---

## 0. Answers in short

1. **Arbitration is a priority walk, not a score max.** Units emit a typed answer or nothing. MainEval adopts the first listed kind that survives the walk in §5. (C-now `dec_0x142110d30.c` L86–90, `FUN_14210F110`; C-prior carrier-ai §3.)
2. **25 kinds exist.** Outfield lists are a fixed head plus a plan-dependent tail. Kind 16 Dribble is never listed; it is the default answer computed every frame. (C-now L59–69; C-prior carrier-ai §1, §3.)
3. **Keep is not a unit.** It is Dribble answer type 3, sub-kind **2**, written by the fallback writer `FUN_142119E90` (C-now). Sub-kind 2 maps to action 0 NOTHING (stand on the ball). Sub-kinds 0 and 1 have no writer. This corrects carrier-units §2.2, which said only 3/4/5 are stored: 2 and 6 are stored too (§6).
_Corrected 17-10-26 from pes-port-A4Walk.md l.15–19 (Main ruling): PES stores no sub 2. The chooser returns only 4/5/6. The sub-2 statements in this item, the §6 row at l.174 and the notes at l.185 and l.190 are superseded._
4. **Plans are 10 image units.** The admission order is 9, 8, 2, 3, 1, 4, 5, 6, 7, else Basic (C-now `FUN_14211E2D0` locals; C-prior carrier-ai §4.4). The plan changes which pass kinds are listed and Dribble's preferred direction.
5. **Command path.** MainEval writes an action id to `BallPlayer+0x4180` → `FUN_142101930` → carrier-sense `FUN_14092C9F0` → `FUN_1409F5240` → `FUN_1405140E0` (action table at `P_ai+0x19E90`) → Action object enter/update → anime Kick → `FUN_1407324B0` (kick launch). The link from the Action object to MOVESTEP is C-prior only, and the writer of `PM+0x5D0` is OPEN (§7).
6. **GF status.** Of the 25 kinds: 2 wired but partial (Shoot as a binary answer, PassForward), 1 pure and tested but unwired (Dribble direction search), 21 absent, and 1 not ported by design (15, a debug unit PES never lists). Plan 6 (Long) and plan 3 (CurveShot) are admitted in GF but never produce output. The old Dribble tail still runs (`elizacontroller.cpp:2400`). GF's order is shot-then-pass; PES's is the §5 walk.
7. **Playing styles.** Of PES's five real reads (bits 7, 8, 12, 19, 20), the carrier brain reads none in GF. Bit 12 (ENFORCER) and bits 19/20 (GK) are read outside it. Bit 7 (CROSSER) is absent: the comment at `pescarrier.cpp:644` says it is read, but the code takes `skill || com`. Bit 8 (CLASSIC_NO_10) is absent from PassForward's short tier.
8. **MA+0x22e4c** is read by four carrier units: PassForward's short tier (`0x142125C80`), DirectHighBall (`0x142129BE0`), Special (`0x14212BB00`) and the OnePattern emitter (`0x142133CD0`). **GF on origin/main has the list now**: `NearestList` (`ob::NearestEntry`, 200 B, `static_assert`, `src/sim/onthepitch/pesoffball.hpp:200–209`), built by `BuildNearest` and read by `NearestOpponents` (`pesoffball.cpp:1319`, `:1340`). The carrier path in the checkout still scans the mental image linearly (§8.5, §14).

---

## 1. MainEval `FUN_142110D30` (C-now `dec_0x142110d30.c`)

Per logic frame, for the BallPlayer `BP` (`param_1`) and its context (`param_2`). The caller is the BallPlayer tick (§2). Order as decompiled:

| step | lines | what it does |
|---|---|---|
| a | L27–36 | Player index `BP+0x1C`; team `FUN_140A48A30`; per-player AI record `P_ai = FUN_140A489F0`; anime state `hu = FUN_140A48910(idx)`; world `G`. |
| b | L37–40 | **Busy stamp.** If `hu+8` bit 0 and `FUN_140A6E320(hu+1)` (property `0x30000`, C-prior), then `BP+0x4224 = MI+0x1198` (frame). The think timer restarts while the carrier is committed. |
| c | L41–47 | Only if `BP+0x10` (active) is set. Intention mode arg = 0 when `BP+0x80 != 0` and a held answer exists (`FUN_142119DF0(BP+0x41E8)`), else 1. `FUN_142117370(BP+0x14)` refreshes the Intention. |
| d | L48–57 | Image units: `FUN_142119390` → image A (`BP+0xEC`); `FUN_1421197B0` → `BP+0x32B0`; `FUN_14211D710` → plan (`BP+0x32C0`); `FUN_14211ED10` → 72-byte ctx (C-prior carrier-ai §1). |
| e | L58–59 | **Committed?** Not committed if neither `hu+1` nor `P_ai+0x14` has the `0x30000` property. |
| f | L60–77 | **Default answer** (not committed). Clear answer `BP+0x41E4 = 0x19` and payload (`FUN_142119D20`). If `BP+0x78 == 0` (AI, not human): kind `0x10` (Dribble) or `0x13` (GKDribble, if `FUN_140A638C0` says the carrier is the keeper) → `FUN_1421102D0`, which copies the ripe record into a temp at `BP+0x4204`, then copies it into `BP+0x41E8`. Then `BP+0x41E4 = 0x10`, **unconditionally** (L74). |
| g | L78–83 | **Re-think gate.** `(uint)(gs·0.1+0.5) < (uint)(MI+0x1198 − BP+0x4224)` (gs = 54, so 5 frames), and Intention bit 3 or bit 4. |
| h | L84–88 | Bit 3 → `FUN_1421111B0` (rebuild the list). Then `FUN_142110370` (run the list, §3). |
| i | L89–105 | If `BP+0x18 & 0x10`: adopt with `FUN_14210F110` (§5). If the id ≠ `0x19`, store it at `BP+0x41E4` and copy its 0x1C-byte record from `FUN_14211EF20(BP+0x3E20, id)`. |
| j | L108–127 | Clear the output block: `BP+0x417C` (publish flag) … `+0x41D2`. |
| k | L128–139 | If `BP+0x41E4 ≠ 0x19`: `FUN_14210E790(BP+0x417C, BP+0x41E8, ctx)` translates the answer to an action id at `BP+0x4180` (§6). Copy aim vectors `+0xB8/+0xBC/+0xC0` → `+0x41D4..+0x41DC`. Log "Set action … to output buffer" (`FUN_140A85E80`). |
| l | L142–144 | Committed → `FUN_14210DD60(BP, ctx)` continues the running action instead. |
| m | L146–148 | `BP+8 := BP+0x4180` (action id). Return `BP+0x417C != 0`. |

Two details the earlier notes did not carry:
- **L74 writes id `0x10` even for the keeper** (GKDribble kind `0x13` only selects the payload source). Whether `0x10` vs `0x13` matters downstream is OPEN; the id is only compared to `0x19` (L93, L128) and stored (L130).
- **Stale default payload.** `FUN_1421102D0` writes the temp only when the record is ripe. Otherwise L70 copies whatever `BP+0x4204` holds. Whether that can be a stale answer at tiers 0–4 is OPEN. Tier 5 (live) has zero delay, so it is immediate there (C-prior carrier-units §5).

**Tick entry.** `FUN_1421019D0` (C-now `dec_0x142101aa7.c`; the notes' 0x142101AA7 is an address inside it) sets `BP+0x4259 = carrier flag` (L12). For outfield slots this is the caller's team argument. For the GK slot (formation 0) it is "is the newest event's actor" (L31–32). It then chooses the list: carrier → `FUN_142111DB0(BP+0x4228)`, else `FUN_1421128D0(BP+0x4240)` (L34–39). It clears `BP+0x425A` (L40), calls MainEval (L41), and sets `BP+0x425A = 1` on true (L42–44).

---

## 2. Who thinks, and when (C-now `dec_0x14092c9f0.c`, `dec_0x14092a340.c`)

- `FUN_14092C9F0` (carrier-sense) runs per player. Its `param_1` is the per-player AI record `P_ai`; BallPlayer is at `P_ai+0x980`, and the tick call is at L199: `FUN_1421019D0(P_ai+0x980, …, team+0x255 != 0, …)`.
- The tick is gated at L194–196 by `(G+0x400 object test: FUN_141F9C3B0 / FUN_140A59AF0)` or `(!bVar4 || bVar2)`. `bVar2` and `bVar4` come from the 24-entry slot table at stride `0x74` (`lVar21`) and roster slot `team+0x6A4` (L59–183). **OPEN:** their exact meaning. Working reading (INF L): the gate decides whether this player is the carrier candidate this frame.
- The team argument passed to the tick is `team+0x255` (C-prior: "team has the ball", pes-re-possession-latch).
- The else branch (L247–294) calls `FUN_142101910` (clears the held answer: `FUN_14210E5F0`, `FUN_142111DA0(BP+0x4228)`) and then runs the restart and set-piece remaps (not re-read in full).
- `FUN_14092A340` (C-now call sites L149, L163, L313, L346) reads `PM+0x5D0` (current AI action) at L163 and applies the chosen id through `FUN_1409F5240` (L313/L346).

**Cadence.** MainEval runs once per logic frame (1/54 s) for each BallPlayer whose `BP+0x10` is set. The full think runs only when the rethink gate passes (§1 step g), so the default Dribble answer is the only thing recomputed every frame. The BallPlayer count is 22 live (C-prior pes-re-playing-styles §9 and carrier-ai §8).

**OPEN:** the writer of `BP+0x10` (active flag) and of `BP+0x4259` for outfield slots. Experiment: write-watch `BP+0x10` in a live match, or find its stores in `FUN_14092C9F0`'s callers.

---

## 3. The list, the arming gate and the time-slicing (C-now `dec_0x142110370.c`; C-prior carrier-ai §3)

**List builder** `FUN_1421112C0` (C-prior). Outfield order: `23 OnePattern, 22 GKCoaching, 0 Shoot, 1 Clear, 17 Feint, 14 RespondRequest, 11 DirectHighBall, 12 Special`, then the plan-dependent tail (§4.4 table in carrier-ai). Live Basic list: `23,22,0,1,17,14,11,12,2,3,10,4[,7,6]` (C-prior, 251 of 892 snapshots). Keeper: `14,20,21,18`. Human-side (`BP+0x78 == 1`): `14,24`. Dribble (16) never appears.

**Run loop** `FUN_142110370` (C-now):
- If Intention bit 3: clear the 25-byte "was evaluated" mask to `0x01` (`BP+0x4160..0x4178`).
- For each index `i` until `0x19`: `kind = FUN_14211EF40(table, i)`; `unit = FUN_1421105B0(kind)`; arm gate `FUN_142111610` (C-prior §3.1).
- **Time-slicing by the Intention flags (C-now, the nested `if` after the unit lookup):**

  | bit 3 | bit 4 | units run |
  |---|---|---|
  | 0 | 0 | none |
  | 1 | 0 | even list indices only |
  | 0 | 1 | odd list indices only |
  | 1 | 1 | all |

  The flags are written by `FUN_142117370`, which was not decoded here. C-prior INF(H): in AI-vs-AI the flags are `0x1F` every frame, so all listed units run.
- A unit whose state (`FUN_1408AEF50(unit)`) is 0 is reset (`vtable+8`). Otherwise it is evaluated (`FUN_14211EEC0`). If its result record is valid and ripe (`FUN_142110AC0`), the 0x1C-byte record is copied to the table with `FUN_14211F050`.
- A held answer is not re-evaluated; it is published only when ripe, then the unit re-arms (C-prior carrier-ai §2).

**Ripeness** `FUN_142110AC0` (C-prior carrier-units §5): tier delay in frames `T = gs·T[tier]/60`, with T1 `{60,40,30,20,10,0,0}` (pass/clear, type 2 or 5) and T2 `{40,30,20,10,5,0,0}` (dribble, type 3). Shots (type 1) and mask `0x1C40800` (kinds 11, 18, 22, 23, 24) are immediate. Overrides: `BP+0x8C > 54` (1 s of possession), or type 3 with the ball within 3 m of a touch/goal line. Human carriers: zero delay. Live tier is 5 (`BP+0x60 = 500`), so the delay is zero in every live sample.

---

## 4. The units: list, inputs, outputs, GF status

Output columns give the answer type, sub-kind/kind, and the translated action id (§6). Inputs are the cited doc's, unless tagged.

**Attribute ids in the table** (C-prior carrier-ai §5–§6, as named there): `0x17` Dribbling, `0x1A` Finishing (INF), `0x1B` Low pass, `0x1C` Lofted pass (INF, "LoP"), `0x26` foot (stat 3 = two-footed, INF), `0x27` Speed, `0x28` Physical contact, `0x2A` Kicking power (INF), `0x31` height (cm/100), `0x34` stronger foot (curl side).

**Style reads inside the carrier brain** (C-prior pes-re-playing-styles §2, §3; carrier-ai §6.1): bit 7 CROSSER, bit 8 CLASSIC_NO_10. Bits 12, 19, 20 are read outside the carrier brain (§8).

| kind | unit (VA) | listed when | inputs | output | GF |
|---|---|---|---|---|---|
| 0 | Shoot `0x142121920` (C-prior §5.1) | always (3rd) | shoot zone (imageA+0x9CC: d, angle, band, sector, box); first-time gates (incoming 120°; ball→goal² > 400 → no; > 225 with body angle > 70° → no); height 0x31 and ball above head (0.8·h/100); keeper present/settled (`0x140A62690`); plan 8 → shot; skills **39/33/8** (+3 m far range), **7** finesse (plan 2/3), **8** knuckle (sub 4), **33/34** drop/rising; COM **5** LONG_RANGE (`central_eval`); stat 0x1A/0x2A (Rn ramp) | type 1, `a` ∈ {1 normal, 2 chip, 3 finesse, 4 knuckle, 5 volley}; ActionShoot 25 (kick style via `0x140979170`, C-prior) | **partial**: `ShootSubKind` returns 0/1: just-shoot (20 m, 40°, keeper settled), box, header, plan 8, central + aim ≤ 40°. No wide/central_eval/finesse/chip/knuckle/volley. Aim is GF's own (ponytail). |
| 1 | Clear `0x142109EB0` (C-prior §3.2, §5.3) | 4th | danger flag `imageA+0x31C1`; clear angle `0x142108630` (opponent sectors 60°/120°/180°, 25 m, height 0x31) | type 5, `a` = angle → ActionClear 21 | **absent**. Replaced by a pass if one holds (§5 rule 6). |
| 2 | GoalFront `0x14212ABA0` (C-prior §3.6) | plan tail | not central; free lane (20 m, 80°) → no answer; else receiver `0x1421265C0` (skill 12 adds 20 to LowPass) | type 2, b=0 → 22 | **absent** |
| 3 | BackSpace `0x142129110` (C-prior §2.3; saved decompile `AttackDecisionsRE/cross.c:352`, not re-read) | plan tail | zone shootable + corridor (20 m, 80°) vs recv (lead < 36 m); ctx6F = 0 and (owned or ball ≤ 1 m); cut-back picker `0x142125140`; plan 4 → one-two target (plan slot 10); downgrade to b=0 if mate pressured | type 2, b=3 → 24 | **absent**. No Ghidra function at this VA in the shared project (C-now probe). |
| 4 | PassForward `0x14212A700` (C-prior §5.2) | plan tail | gate: (owned or press.cls < 2) and not close. Short `0x142125C80` (LowPass 0x1B, +20 with skill 12; openness need from image state; receiver 60° and 15 m rules; turn < 80°). Through `0x142127FF0` (team table > 40). Lofted (image state 1/2; dead per PES: squared/linear mismatch) | type 2 with b = 0 (short, 22), 3 (through, 24), 1 (lofted, 18). `f10` one-touch (skill 0x0C) | **partial**: `PassForward` (`pescarrier.cpp:516`) has the tiers, openness and difficulty. GF's `_AddPass` maps Through → ShortPass (`elizacontroller.cpp:2391–2392`). No style term (bit 8 absent). The lofted tier is a stand-in. |
| 5 | SideChange `0x14212B8B0` (C-prior §3.5) | plan 7 | plan target valid; dist ≥ 20 m; pass angle vs facing ≤ 60° | type 2, b=1 → 18 | **absent** (plan 7 never admitted, `elizacontroller.cpp:2202`) |
| 6 | Safety `0x14212B520` (gate decoded: carrier-helpers §2.1) | tail (`plan+0xD8` or `+0xC4 == 3`) | lane `0x14063DE20`: len = runtime sqrt(nearestOppD2), cone 80°, rad 3.0 — FREE ⇒ no answer; k = 2 if press+0x30 > 0 else 1 if +0x34 > 1, +1 iff +0x38 != 0 and folded facing-vs-plan diff > 135°; short `0x142126B70`; through `0x142128490`. _Corrected 19-10-26 from pes-re-carrier-helpers.md §6 items 6-7: raw 0x14212B5F5-7D8; old "free lane 80 m / 3.0" + "k = press counts + facing vs plan angle > 135°"._ | type 2, b=0 (22) or b=3 (24, f12) | **absent** |
| 7 | Cross `0x142129590` (C-prior §2.1; saved decompile `AttackDecisionsRE/cross.c:5`, not re-read) | plans 1–3/5, tail | carry bearing vs facing ≤ 130°; aim = carrier + attack·34.06 m; lane (5 m, 30 m); cross target `0x142125AB0`; receiver lead < 16 m; beaten-man flag (duel byte +0x12A7); f12 when pred. dist ≤ 30 m and advance ≤ 8 m | type 2, b=2 → 19 CENTERING | **absent**. The Cross plan start (plan 5) is wired (`elizacontroller.cpp:2124–2130`), the unit is not. |
| 8 | EarlyCross `0x14212A2D0` (C-prior §2.2) | plan 5 | — | type 2, b=2 → 19 | **absent** |
| 9 | PassLong `0x14212AD40` (C-prior §5.2; saved decompile `units_missing.c`) | plan 6, tail | own (or danger and ball < 1.5 m); target = plan target if valid, else most advanced valid mate; taller by > 5 cm wins within 10 m (stat 0x31); b=0 if openCls ≥ 4 else b=1 | type 2, b ∈ {0,1} → 22/18 | **absent** as a unit. The Long **plan** (`0x14211B1D0`) is ported as admission only (`LongScore`, `LongAdmitted`, `LongVetoed`, `pescarrier.cpp:668–747`). Plan 6 emits no pass. |
| 10 | OneTwo `0x14212AEB0` (C-prior §3.4, fully decoded; saved `carunits2.c:530`) | default + plan 1–4 | opponent within 6 m in 16 frames (`0x140A4E720`) or facing within 90° of attack; one-touch ok (`0x142128C10`); target `0x142126A80` | type 2, b=0, `f10` wall flag → 22 | **absent** |
| 11 | DirectHighBall `0x142129BE0` (C-prior §5.2; saved `units_missing.c:387`, reads 22E4C at :546) | always (7th) | first-time while receiving (`0x140A6E5E0`); ball ≤ height/100 + 0.1 m; receiver records; keeper > 25 m; team pass-request table. **Reads MA+0x22E4C.** | type 2, b=3 → 24 | **absent**. Most frequent adopted pass live (C-prior: 284/2454 think snapshots; 155/892 this session). |
| 12 | Special `0x14212BB00` (C-now `dec_0x14212bb00.c`, reads 22E4C at L73–89) | always (8th) | shootable zone; aim = team angle + 180°; closing (130°, 8 m, `0x140A4E720`); opp-line and cand-line ≤ 16; side test; scan 11 slots (TB ≤ 110°, TA ≤ 80°); lo/hi 6–25 m (10–3 m when receiving); lane width 3 m, budget 120; skill 39 via `0x14063E960`; bearing ≥ 45° or speed ≤ 15 | type 2, b=3 → 24 | **absent** |
| 13 | PassTarget `0x14212B9E0` (C-prior §3.10) | plan 9 | plan target ≠ carrier: not owned or target invalid → type 3 sub 4 (dribble toward target); else pass b=0 | type 3/4, or type 2 b=0 | **absent** |
| 14 | RespondRequest `0x14212B230` (C-prior §3.7) | always (6th) | set-piece (MI+0x116C = 3) events 1/5/6; open play: 11 slots, receiver status 0xB, request table | **type 7** (pass to requester) | **absent**. Request-table writer OPEN. |
| 15 | PassRandomTest `0x14212AFD0` | never listed (debug) | — | — | not ported (by design; C-prior §3.10) |
| 16 | **Dribble** `0x14212FE70` (C-now body tail L430–533; C-prior §5.4) | never listed; default every frame | "let it run" pre-check (`0x142131810`); desired `plan+0xC0`; W ladder by image state (`+0x32B0+8`); 72 × 15 direction search (`0x14212EC20`); threat `0x14212E490` over the opponent list (R_o from image state, keeper × 2); edge 100 at ≤ 0.5 m; sub-kind chooser `0x14212F1C0` (30°/60°/10 m/40°/15 m speed, room 12 m); shield branch `0x142130EE0` | type 3, sub-kind `a`: **2 keep** (`FUN_142119E90`), 3 (`FUN_142119EB0`), 4/5 (`FUN_142119E20`, +1 for 5), 6 feint (`FUN_142119E60`). Via `0x14210E880`: 2 → 0 NOTHING; 3/4 → 3 DRIBBLE mode 3; 5 → 3 DRIBBLE mode 4; 6 → 27 FEINT (kind 0xE) | **partial**: `DribbleDirection` (`pescarrier.cpp:160`) and `DribbleThreat` (`:128`) are ported with tests (`tests/sim/pes_carrier_test.cpp:802–833`) but **no caller in src**. The live dribble is still `AI_GetBestDribbleMovement` (`elizacontroller.cpp:2400`, `AIfunctions.cpp:473`). Keep, the sub-kind chooser and the shield branch are absent. |
| 17 | Feint `0x14212D910` (C-prior §5.5) | always (5th) | cap `0x1E` (tier ≥ 2); skill 3 SOMBRERO, 4 QUICK_TOUCH, 21 MALICIA (INF), COM 2 MAZING_RUN | type 4 → 26 KICK_FEINT / 27 FEINT | **absent** |
| 18 | GKClear `0x142132B40` | GK list | keeper | keeper answers (6/8/9/11 family, §6) | absent (keeper family, `peskeeper.*` partial) |
| 19 | GKDribble `0x142132E20` | keeper fallback | keeper | type 3 | absent |
| 20 | GKPassShort `0x1421333F0` | GK list | keeper | pass | absent |
| 21 | GKPassLong `0x142133120` | GK list | keeper | pass | absent |
| 22 | GKCoaching `0x142133950` (C-prior §3.9) | always (2nd) | team mode `T+0x6D4` (1–6) picks an outfield answer from the keeper | type 2 b=0/1/3/…, type 1, type 5 | **absent** |
| 23 | OnePattern `0x142133C80` (C-prior §3.8) | always (1st) | precondition `0x142133ED0`; emitter `0x142133CD0` reads **MA+0x22E4C** (C-now `dec_0x142133cd0.c:38`) | type 2/3 (stationary pattern) | **absent** (precondition false in open play; C-prior live 0/892) |
| 24 | SetPlay `0x142135C50` | set-piece list | set-piece | set-piece answers | absent (set-piece family is `pessetpiece*`, a separate port) |

Types 6, 8, 9, 11 are keeper answers (C-now translator §6): 87 KP_PICKUP_BALL, 102, 90 DIVE, 84 KP_COACHING.

**Plan image units** (C-prior carrier-ai §4.4; C-now dispatcher `FUN_14211E2D0`):
Ten plans, index = plan id: 0 Basic, 1 Breakthrough, 2 MiddleShot, 3 CurveShot, 4 PassThrough, 5 Cross, 6 Long, 7 SideChange, 8 ShotPassFromGoal, 9 BackDFLine. Admission order 9, 8, 2, 3, 1, 4, 5, 6, 7 (C-now: 8-dword table `0x142BF02C0`/`0x142BF02D0` + inline 7 at L35), else Basic. Ported starts and COM/skill/stat inputs:
- **Breakthrough** (1): base 70 with skill 37 DOUBLE_TOUCH or `BP+0x68 == 2`, 40 with `BP+0x68 == 1`, 20 with COM 1 DARTING_RUN / 2 MAZING_RUN / 4 INCISIVE_RUN; + 15·clamp01((x−70)/20) for stats 0x17, 0x27, 0x28; cap 70. GF passes stat 0x17 only and COM names SpeedingBullet/MazingRun/IncisiveRun (`elizacontroller.cpp:2103–2108`, `pescarrier.cpp:630–637`).
- **MiddleShot** (2): 70 with skill 39 MIDDLE_SHOT, 8 KNUCKLE, 33 DROP_SHOT or **34 RISING_SHOT**, or COM 5 LONG_RANGE; +40 at state 2; + 15·clamp01((x−70)/29) for 0x2A, 0x1A; clamp 5..80. GF's `longShotSkill` (`elizacontroller.cpp:2039–2041`) omits **34**, and passes COM 5 as `false`. GF passes the finishing stat only.
- **CurveShot** (3): 30 with COM 4 INCISIVE_RUN + 30 with skill 7 LONG_RANGE_DRIVE; + 10·clamp01((x−70)/29) for 0x2A, 0x1A; clamp 5..70. Admitted in GF (`CurveShotAdmitted`), but its output is never used (no finesse sub-kind), so it is inert.
- **Cross** (5): 50 with style **7 CROSSER**, COM 3 EARLY_CROSS or skill 14 PINPOINT_CROSS, or team instruction; + 20·clamp01((x−70)/29) for 0x2A and 0x1A; clamp 40..80. GF: `skill || com` only (`pescarrier.cpp:644`, `elizacontroller.cpp:2124–2128`). **Style 7 is not read.**
- **Long** (6): base 30/60/80 by pressure class 0–1/2/3; +50 COM 6 LONG_PASS, +50 skill 13 WEIGHTED_PASS, +50 skill 17 LOW_LOB; + 10·clamp01((0x1C−70)/29). GF: `LongScore`/`LongAdmitted` with `comLongBallExpert` (`elizacontroller.cpp:2176`), ponytails for team instruction and full attack (`:2163–2175`).

---

## 5. Arbitration: the walk (C-prior carrier-ai §3.2; carrier-units §4 rules 1–11, raw-checked there)

There is no score comparison. `FUN_14210F110` (28 KB; head read C-now, rules C-prior) walks the active list in insertion order. For each listed kind `k` with answer type `t ≠ 0`, the first survivor is returned. If none survives, the Dribble/keep default stands (C-now L89–105).

1. **Instant win.** Outside set pieces (`MI+0x116C ≠ 3`): kind 22 returns at once. (Never fires in open play.)
2. **Kind 16 is dead** (never listed; its dribble-vs-pass overrides are unreachable in open play).
3. **First-time gate** (tiers 0–1, capability 0x14 off): if not owned, accept only if `[ballY ≥ height/100 (0x31), or (t ≠ 1 or skill 11) and t ≠ 5] and [t ≠ 2 or skill 12]`. Live tier 5 means the gate is off.
4. **Held dribble stickiness:** `BP+0xC8` set and held answer is Dribble and `k` is Dribble → keep or skip (no live sample with `+0xC8` set; INF on meaning).
5. **Restart vetoes:** `MI+0x1775 ∈ {3, 9, 10}` skips t ∈ {2, 5}; mode 2 accepts only t = 2.
6. **Cut-back beats the long shot:** a shot (t=1) outside the close band is replaced by kind 3 BackSpace if 3 holds an answer.
7. **Clear replaced by a pass:** the first of **3 BackSpace, 4 Forward, 9 Long, 6 Safety** listed after `k` with an answer replaces the clear, unless the forced-clear test passes (counts opponents with 90°/135° sectors and 1.5/2.0 m; C-prior, not fully resolved). A Safety back-pass to the keeper is accepted only when owned or the keeper is within 90° of body direction.
8. **Pass-kind vetoes:** Safety dropped in the close band and (image state 4) unless back-pass within 100° of dribble direction; Forward dropped if its receiver passed to the carrier < 2·54 frames ago; non-conforming pass/dribble kinds filtered; the corridor/lane battery drops failing lanes, with the 3/4/6/10/0/1/18/20/21 ripe-fallback chain.
9. **Cross override:** skill 14 and t ∈ {2,3} from kind ∉ {7,8,23,24} → return 7 if listed and ripe.
10. **Final ownership check:** accept if owner unknown or teammate, or t = 3. Set pieces: accept if the carrier is not the designated kicker, or t ∈ {1, 5}, or a pass with `0x14063E3A0`.
11. **Arming gate** `0x142111610` (C-prior §3.1): team caps `0x1E/0x1C/0x1D` for kinds 17/3/6; 11-frame touch window (kick: 22); 27/54-frame windows; 5 m ball distance (`dist² ≤ 25`); 60° turn veto for pass kinds; nearest-opponent skip; ball-height veto (`ball height > byte/200`).

Live: 892 snapshots, adopted by kind: Dribble-fallback 324, DirectHighBall 155, Safety 78, Forward 48, Shoot 44, OneTwo 41, SetPlay 37, Cross 30, Long 27, BackSpace 20, Clear 19 (C-prior carrier-units §7).

**GF ordering versus PES.** GF runs: panic pass (not a PES unit), then `ShootSubKind == 1` → shot and no pass, then `if (!shooting && rethink)` → `PassForward`, then the old dribble tail (`elizacontroller.cpp:2224–2400`). GF has no cut-back override (rule 6), no clear→pass chain (rule 7), no cross override (rule 9), and no Feint/OneTwo/Safety/SideChange. This is the structural gap.

---

## 6. Output translation (C-now `dec_0x14210e790.c`, `dec_0x14210e880.c`, `dec_0x14210ee00.c`, `dec_0x142119e90/e20/eb0/e60.c`)

**`FUN_14210E790(out, answer)`** switch on `answer.type`:

| type | handler (C-now) | action written to `out+4` (C-now) |
|---|---|---|
| 1 shoot | `FUN_14210F090` | SHOOT 25 (shot sub-kind `a`), 104 in PK mode (C-prior) |
| 2, 7 pass | `FUN_14210EE00` | `b` 0 → **0x16 SHORT_PASS 22**; 1 → **0x12 LONG_PASS 18**; 2 → **0x13 CENTERING 19**; 3 → THROUGH 24 (C-prior §1; elided branch L119–124, not re-read). Receiver index `a` → `out+0x1C`; flags `f11→|1`, `f12→|2`, `f13→|8`, `f14→|0x10` at `out+0x20/0x21` (C-now L96–108). `b` values come from §4. |
| 3 dribble/keep | `FUN_14210E880` | sub 0/1 → **9 MOVE_STOP_FRONT / 10 MOVE_STOP_GOAL** (C-now L16–39); sub 2 → **0 NOTHING** (stand on ball); sub 3 → 3 DRIBBLE mode 3; sub 4 → 3 DRIBBLE mode 3; sub 5 → 3 DRIBBLE mode 4; sub 6/7 → **27 FEINT** kind `0xE` / `0xF` (C-now L183 case 6/7, `+0x40 = 0xE/0xF`); target point = carrier + 10 m along `b` (`0x142588210`) |
| 4 feint | `FUN_14210ECA0` | 26 KICK_FEINT (in play) / 27 FEINT (C-prior) |
| 5 clear | inline (C-now L22–26) | **21 CLEAR**, `out+0x18 = angle` |
| 6 | inline (L27–29) | **0x57 = 87 KP_PICKUP_BALL** |
| 8 | inline (L30–35) | **0x66 = 102**, `out+0x1C = a` |
| 9 | inline (L36–39) | **0x5A = 90 DIVE** |
| 10 | inline (L40–42) | sets `out+0x55 = 1` (no action) |
| 11 | inline (L43–46) | **0x54 = 84 KP_COACHING**, `out+0x56 = 1` |
| default | inline (L6–8) | `out+4 = 0` (none) |

**Dribble sub-kind writers (C-now):**
- `FUN_142119E90` (keep): `type 3`, `sub = 2`, dir 0. Called when the chooser returns a value other than 3–6 (`0x14212FE70` L525, default `iVar10 = 4` at L86).
_Corrected 17-10-26 from pes-port-A4Walk.md l.17: the keep arm is reached only for a value outside 3..6, which the chooser never returns, so this writer is unreachable from the chooser path._
- `FUN_142119EB0`: `type 3`, `sub = 3`, dir `+0x2A0` (L494–495).
- `FUN_142119E20(…, 0)`: `type 3`, `sub = 4`; `(…, 1)`: `sub = 5` (L497–513).
- `FUN_142119E60`: `type 3`, `sub = 6` (FEINT, L515–522).

So **sub-kinds 0 and 1 have no writer**, and **sub 2 (keep) is not stored**; 6 (feint) is. Corrected 17-10-26 from pes-port-A4Walk.md l.15–19 (the earlier "2 and 6 stored" reading is withdrawn).

**Pass translator `FUN_14210EE00`** (C-now L18–125): sets `out+4` from `b` (0/1/2 decoded; 3 → 24 per C-prior), receiver to `out+0x1C`, flags to `out+0x20/0x21`. It also calls `FUN_140A48A10`/`FUN_140A48910` for the sender's record and the restart remaps at L27–94 (not re-read).

---

## 7. Command path: MainEval output to MOVESTEP and the kick

Chain, with what was verified and what was not:

1. **MainEval** writes the answer to `BP+0x417C` (publish flag), `BP+0x4180` (action id, §6), `BP+0x41E0` (id), `BP+0x41EC..+0x41F8` (payload), `BP+0x41D4..+0x41DC` (aim vector). Then `BP+8 := BP+0x4180` (C-now L146). **C-now.**
2. **`FUN_142101930(BP)`** returns the id this frame: if `BP+0x425A` (decided), `BP+8`; else if `BP+0x4259` (carrier), `BP+0x4230`; else `BP+0x4248` (C-now). The two fallback slots belong to the two list sub-objects (`BP+0x4228` carrier, `BP+0x4240` other). Which builder writes which slot is INF(M), not traced.
3. **Carrier-sense `FUN_14092C9F0`** (C-now L199–200): `uVar19 = FUN_142101930(BP)`. Then the owner and restart remaps (L225–232: `MI+0x1775 == 3` maps `0x34 → 0x33` when `team+0x255 == 0`; `== 0x17` maps to 1). `FUN_140A65C80` remaps the class (L218–221, not re-read in full). The result goes through `FUN_1409F5360` (C-now: set-piece remaps: `0x12` → 100 or 98, `0x16` → 103 or 99, `0x18` → 0x65, and `1`/`0x3F` → 0 in set pieces when `FUN_140A3C8F0` holds). If `local_64 == 0`, `FUN_1409F5260` runs next (not re-read). `0x7B` then replaces the id when `FUN_140A6E470` is false, the newest event's `+0x2D` flag or open play holds, and the MA designated slot is this player (L434–445). `0x70` replaces it when `MI+0x116C == 2`, `MI+0x1574 == 1`, a carrier exists and the id is 1 (L454–456). The returned id is the player's AI action.
4. **Caller `FUN_14092A340`** (C-now L149, L163, L313, L346): reads `PM+0x5D0` (current AI action) at L163, then applies the returned id with `FUN_1409F5240(P_ai, ctx, player, id)` (C-now L313, L346).
5. **`FUN_1409F5240`** (C-now, 236 bytes): `FUN_1405140E0(P_ai+0x19E90, ctx, player, &id)`.
6. **`FUN_1405140E0`** (C-now `dec_0x1405140e0.c`): table `P_ai+0x19E90`, 8-byte entries indexed by `id & 0xFF` → an `Action` object. If `id` differs from `PM+0x5D0` (or the `0xE` case), it calls the object's `vtable+8`, then always `vtable+0x10` on the same object (both on the new id's object; slot semantics C-prior: start/update). It writes `player+0x1978` (object `+8`), `player+0x19AD` (object `+0xC`), `player+0x10E4` (MI+0x1198 frame stamp), and for id `0x43` copies `+0x8B0..` → `+0x1988..`. **C-now.**
7. **Action objects** `match::player::Action*` (C-prior matchlogic: 112 classes; C-prior dead-code: dispatched by the switch `FUN_1409F8030`, RVA table 0x1409F8170, not re-read here).
8. **Kick.** Shot and pass actions use the anime `Kick` family: ANIME 8 LONG_PASS, 9 SHORT_PASS, 10 SHOOT → writers `0x140717C50` → `0x1408397A0`/`0x14083C590` → `FUN_1407324B0` (C-prior kick-launch §1–2). The shot answer's `a` (1/2/3/4/5) selects the kick style; `a = 2` → style 2 power `0.2·clamp01((d−10)/25)+0.55`, `a = 3` → style 1 power `0.3·clamp01((d−10)/25)+0.6` (C-prior carrier-ai §5.1, `0x140979170`).
9. **MOVESTEP.** Action updates drive the 24-state MOVESTEP machine: handlers write `req[0x38]` and `req[0x39]`; the tail of `FUN_1406B8340` stores the state (C-prior pes-re-movestep-wave1 §"Corrections" 5; the wave-1 port `pesmovestep.*` exists).

**OPEN (as of turn 1; §14 resolves the first item):**
- **`PM+0x5D0` writer.** Resolved in §14.1: the setter is `FUN_140A6CA10`.
- **Action → MOVESTEP / anime request.** Not traced here. Experiment: decompile `FUN_1409F8030`'s `case 0x19` (SHOOT) and `case 0x16` (SHORT_PASS), then follow to the anime request writer.
- **Kick timing.** Which Action update calls `FUN_1407324B0` and from which anime frame. Experiment: follow `ANIME_SHOOT` writers from §7.8.

**GF command path, for contrast.** `GetOnTheBallCommands` appends `PlayerCommand`s with GF function types (`e_FunctionType_ShortPass/HighPass/Shot`) to a queue (`elizacontroller.cpp:2260, 2393`). GF's kick and movement are GF's own. The PES action table, the MOVESTEP port (`pesmovestep.*`) and the kick-launch port are not on this path. No PES action id reaches a GF command.

---

## 8. Inputs: the five real style reads, skills, COM, team context, MA list

### 8.1 The five real playing-style reads (C-prior pes-re-playing-styles §2–§3, §7; C-now GF lines)
_Corrected 17-10-26 from pes-re-playing-styles.md (RETRACTED banner, its l.42; pes-re-style-census.md §4): the reader census this section cites is retracted. The five reads below are unverified until re-checked._

| bit | style | PES read (VA) | effect in PES | in the carrier brain? | GF status |
|---|---|---|---|---|---|
| 7 | CROSSER | R3 `FUN_14211AB10` @`0x14211AC8D` | Cross plan base 50 (§4) | **yes** (plan 5 start) | **not read.** `pescarrier.cpp:644` says "with the Crosser style" but the code is `(skill \|\| com)`. The comment is stale; the port is missing the term. |
| 8 | CLASSIC_NO_10 | R2a `FUN_142125C80` @`0x1421264D5` (mov r8d, 8), through the R1 gate `FUN_14063E890` | receiver scorer: a No.10 in a pocket returns immediately | **yes** (PassForward short tier) | **absent.** PassForward has no style term. R2b (`FUN_142132660` @`0x142132A2B`) is the keeper/goal-kick short picker, not the carrier. |
| 12 | ENFORCER | R5a–d (`FUN_14078E5A0`, `FUN_1407902C0`, `FUN_14096FDA0`, `FUN_140971BA0`) | tackle/slide aggression | no (defender) | **read**: `elizacontroller.cpp:824`, `:833`; `pesdefenderdecisions.hpp:250` |
| 19 | OFFENSIVE_GK | R6a–g | keeper distribution, positioning | no (keeper) | **read**: `elizacontroller.cpp:538–544`, `goalie_default.cpp:184–188`, `peskeeper.hpp:213` |
| 20 | CLASSICAL_GK | R6a–g | keeper distribution (`f·0.25`) | no (keeper) | **read**: same |

Other style bits (0–6, 9–11, 13–18) have only the R1 compat gate (C-prior §3). The compat table is `0x142613D20` (C-prior §4). GF uses its own `SuitsRole` table.

**Style-bit id source.** The engine bit names (C-prior carrier-ai §6.1) are the PES names; GF's `PlayingStyles::Player` enum is the same set by name (C-now `elizacontroller.cpp:824`).

### 8.2 Skills (C-prior carrier-ai §6.1 engine bits; GF names from `playerskills.hpp`)

| bit | PES name | used by | GF status |
|---|---|---|---|
| 7 | LONG_RANGE_DRIVE | CurveShot plan (+30), finesse (shot) | `curveShot.skill7 = LongRangeDrive` ✓ (`elizacontroller.cpp:2146`) |
| 8 | KNUCKLE_SHOT | shot far range (+3 m), knuckle sub-kind 4, MiddleShot start | in `longShotSkill` ✓ (`:2040`) |
| 11 | ONE_TOUCH_SHOOT | first-time shot gate (adopter rule 3) | GF: `FirstTimeShot` (not in the carrier path) |
| 12 | ONE_TOUCH_PASS | first-time pass gate (adopter rule 3); LowPass +20 (PassForward) | GF: `OneTouchPass` in receiver difficulty ✓ (`:2354`) |
| 13 | WEIGHTED_PASS | Long plan +50 | ✓ `longPlan.skillWeightedPass` (`:2177`) |
| 14 | PINPOINT_CROSS | Cross plan start; Cross override (rule 9) | ✓ Cross plan (`:2127`); rule 9 not ported |
| 17 | LOW_LOB | Long plan +50 | ✓ `skillLowLoftedPass` (`:2178`) |
| 33 / 34 | DROP_SHOT / RISING_SHOT | shot far range (33), MiddleShot start (33, **34**) | 33 ✓ (`DippingShots`); **34 absent** from `longShotSkill` (`:2039–2041`) |
| 37 | DOUBLE_TOUCH | Breakthrough base 70 | ✓ (`skillBreakthrough`, `:2104`) |
| 39 | MIDDLE_SHOT (Player.bin: Long Range Shooting) | shot far range; MiddleShot start | ✓ `LongRangeShooting` (`:2039`) |

### 8.3 COM cards (C-prior carrier-ai §6.1; GF `playingstyles.hpp:45–53`)

COM bit names are the PES names. GF maps by name, so GF's bit positions differ, which is not a bug. Mapping: PES 1 DARTING_RUN = GF `SpeedingBullet`; 2 MAZING_RUN = `MazingRun`; 3 EARLY_CROSS = `EarlyCross`; 4 INCISIVE_RUN = `IncisiveRun`; 5 LONG_RANGE = `LongRanger`; 6 LONG_PASS = `LongBallExpert`. PES 0 TRICK_STAR = `Trickster`.

| COM | used by | GF |
|---|---|---|
| 1, 2, 4 | Breakthrough (+20 base) | ✓ |
| 3 | Cross plan (+50) | ✓ |
| 4 | CurveShot (+30) | ✓ `curveShot.incisiveRun` |
| **5** | MiddleShot (70); shot `central_eval` | **not read** (MiddleShot is called with COM `false`, `:2112–2113`) |
| 6 | Long plan (+50) | ✓ `comLongBallExpert` |
| 2 | Feint (MAZING_RUN) | absent (no Feint) |

### 8.4 Team context (C-prior; GF ponytails)

| input | PES use | GF |
|---|---|---|
| `team+0x255` | has-ball flag (carrier-sense argument; chase predicate) | ✓ (`team.cpp` possession) |
| `BP+0x70` team instruction (1, 2) | Cross 50 → 70, Breakthrough, Long +20/+40 | **stand-in 0** (`elizacontroller.cpp:2171–2174`, ponytail) |
| `team+4 == 3`, `TeamData+47266` (full attack) | Long, Cross, Breakthrough gates | **stand-in false** (`:2154`, `:2165–2166`) |
| `team+0x250` attack angle | plan and Cross direction | absent (C-prior C2 open) |
| `team+0x4C00[slot]` | formation slot (keeper = 0) | ✓ (`GetDynamicFormationEntry().role`) |
| `MI+0x1775`, `MI+0x116C`, `MI+0x1554` | restart vetoes, set-piece gates | partial (`elizacontroller` set-piece, `setpiecelaws.*`) |
| `BP+0x8C` (possession frames) | ripe override after 1 s | ✓ possession duration (`:2022`) |

### 8.5 The MA+0x22E4C list (C-prior carrier-ai §4.3; C-now readers)

`MA+0x22E4C` is the per-player sorted distance list: 200-byte stride per player, 11 entries per team, each `{id, dist²}`, rank byte at `+0xB2`. Readers in the carrier brain (C-now unless marked):
- **PassForward short tier** `FUN_142125C80` (C-now, 2 hits: `dec_0x142125c80.c:219, :222`), for the receiver's nearest-opponent distance.
- **DirectHighBall** `FUN_142129BE0` (grep of saved decompile `units_missing.c:546`; not re-read).
- **Special** `FUN_14212BB00` (C-now, 4 hits: `dec_0x14212bb00.c:73–89`).
- **OnePattern emitter** `FUN_142133CD0` (C-now, 2 hits: `dec_0x142133cd0.c:38, :41`).
- Image-A receiver record B (carrier-ai §4.3, C-prior; the builder `0x1421174F0` not re-read here).

**Not read by** (C-now grep of decompiles): PassForward proper (`0x14212A700`), Cross (`0x142129590`), OneTwo (`0x14212AEB0`), Clear (`0x142109EB0`), the through picker (`0x142127FF0`), the cut-back picker (`0x142125140`). Long, Safety, BackSpace and PassTarget have no Ghidra function in the shared project, and their saved decompiles show no read.

GF's carrier path (`elizacontroller.cpp:1947`, `:2078–2085`, `:2372–2375`) still uses the mental image and a linear nearest-opponent scan. The sorted list exists on main as `NearestList` (§0 item 8, §14.3), so the carrier units can read it once they are wired.

---

## 9. GF map: what each PES piece becomes, and what GF has that PES lacks

### 9.1 PES → GF

| PES piece | GF site | status |
|---|---|---|
| MainEval steps a–m (§1) | `ElizaController::GetOnTheBallCommands` (`elizacontroller.cpp:1929–2402`) | GF runs its own sequence, not the walk. |
| Intention ratchet `0x1421197B0` | `NextIntention` / `SetPesIntention` (`elizacontroller.cpp:2046–2070`, `pescarrier.hpp:521`) | ported; three ponytail inputs (corridor block, `a5+0xB58/B3C`, heading pair). |
| Plan dispatcher `0x14211E2D0` (order, admission, cancel) | `AdvanceImageSelection` (`elizacontroller.cpp:2181–2214`, `pescarrier.hpp:553–575`) | ported; admission order and rolls ✓; **effects inert**: `carrierImage.id` reaches only `shoot.planId` (`:2229`), so only plan 8 changes output. |
| Plan start probabilities | `PlanStartProbability` (`pescarrier.cpp:613–652`) | ported for 1, 2, 5; stat/COM inputs reduced (§4). |
| Plan rolls | `PlanRoll` / `CarrierSeedTick` (`pescarrier.cpp:652–665`) | ported; seed from `RandomInfo` snapshot ✓ (C-prior com-styles §3.8–3.11). |
| Shoot `0x142121920` | `ShootSubKind` (`pescarrier.cpp:419`), `ComputeShootZone` (`:342`) | partial (binary, §4 row 0). |
| PassForward `0x14212A700` | `PassForward` (`pescarrier.cpp:516`), candidate build (`elizacontroller.cpp:2299–2384`) | partial (§4 row 4). |
| Dribble `0x14212FE70` / direction `0x14212EC20` | `DribbleDirection` (`pescarrier.cpp:160`), `DribbleThreat` (`:128`) | **pure, tested, unwired** (§4 row 16). |
| Dribble tail | `AI_GetBestDribbleMovement` (`AIfunctions.cpp:473`) | **wired** (old search). Must go when the port lands (clean cutover). |
| LongScore / LongAdmitted (plan 6) | `pescarrier.cpp:668–747` | ported; plan-only, no output. |
| CurveShotScore / Admitted (plan 3) | `pescarrier.cpp:668–707` | ported; plan-only, no output. |
| Adopter walk `0x14210F110` (§5) | — | **absent.** |
| Output translation `0x14210E790` (§6) | — | **absent.** GF writes `PlayerCommand` directly. |
| Carrier-sense / action table (§7) | — | **absent** on the carrier path. |
| Reaction tiers (§3) | — | absent. Live tier 5 is zero delay; GF has no tier. |

### 9.2 What GF has that PES lacks

| GF piece | where | PES site |
|---|---|---|
| **Panic pass under pressure** (`_AddPanicPass`, `MatchPressure::ShouldStumble`, `PlayerSkills::GetStumbleChanceMultiplier`) | `elizacontroller.cpp:1951–1977`, `:1991–2000` | **none** among the 25 kinds or MainEval steps. GF-only. |
| Mindset panic (`AI_GetMindSet`, `panicProneness`, `goalCloseness`) | `elizacontroller.cpp:1980–2001` | **none**. GF-only. |
| Shoot before pass (`if (!shooting && rethink)`) | `elizacontroller.cpp:2269` | PES: shot is 3rd in the walk, Clear/Feint/Pass in order; a later pass can win (rule 5/6/7). |
| `opponentPlayerImages` + linear nearest-opponent scan | `elizacontroller.cpp:1947`, `:2078` | PES: MA+0x22E4C sorted lists (§8.5). |

**Dead local in GF.** `style` (`elizacontroller.cpp:1940`) is never used in `GetOnTheBallCommands`.

**RNG rule.** The brief says RNG only through `pesrandom.hpp`. GF's `random(0.0f, 1.0f)` (`:1971`) and the shot power `random(...)` (`:2259`) use GF's helper. Check that this helper is the only RNG entry point, and move these two calls to `pes::` if not.

---

## 10. Port order (for the A4 carrier family)

Each step is a pure function with a red test first. Steps 1–2 gate the rest.

1. **Output records and translator.** Port `FUN_14210E790`'s table (§6) and the output selector `FUN_142101930` as pure functions over an answer record. Add the sub-kind writers (2, 3, 4/5, 6) and the keeper types. Tests: one per type → action id, and the keep (sub 2) → action 0 case.
2. **List, arming, ripe, walk.** Port the list builder (§3, plan tail table), the time-slice rule (bits 3/4 → parity), the arm gate, the ripe gate (tier 5 zero delay, `BP+0x8C` override), and the walk (§5, rules 1–11) as one pure function over `{kind, answer}` pairs. Tests: the walk order on synthetic lists (rule 6 cut-back, rule 7 clear→pass, rule 9 cross override, rule 3 first-time gate), and the parity slicing.
3. **Dribble.** Wire `DribbleDirection` in place of `AI_GetBestDribbleMovement` (clean cutover: delete the old call and `AI_GetBestDribbleMovement` if unused). Add keep (sub 2) and the sub-kind chooser `0x14212F1C0` (needs a raw re-read of the w_spd divisor at `0x14259D668`, C-prior open item). Add the shield branch `0x142130EE0` (C-prior). Tests: direction search (existing) + keep when no direction beats the costs.
4. **PassForward fixes.** Through → kind 3 (24), not ShortPass. Add the bit-8 term to the short tier (R2a, `0x142125C80`). Add LowPass +20 for skill 12 (C-prior §5.2). Tests: each tier, and the bit-8 No.10 immediate return.
5. **Shot sub-kinds.** Port `central_eval` (`0x14211FE20`), `wide_eval` (`0x142120E60`), finesse (`0x14211FBF0`), chip (`0x142120BD0`), knuckle/volley (C-prior §5.1). Blocked on the lane helpers `0x14063DE20` / `0x14063CD90` / `0x140A90FD0` (open internals, C-prior). Aim `0x14211F090` likewise.
6. **Pass units.** In this order: OneTwo (10, fully decoded), DirectHighBall (11, reads 22E4C), Special (12, reads 22E4C), Cross (7) with its picker `0x142125AB0`, EarlyCross (8), Long unit (9, `0x142128A20` picker), Safety (6), BackSpace (3, picker `0x142125140`), GoalFront (2), SideChange (5), PassTarget (13). Tests per unit: gate, output type/b.
7. **Clear (1)** with angle `0x142108630` (C-prior: port-ready except corridor helpers). Tests: the replaced-by-pass rule.
8. **Plan effects.** Plan → listed kinds (§3 table), plan W ladder for Dribble, shot planId for ShotPassFromGoal, CurveShot → finesse (plan 3 + skill 7). Fix MiddleShot's inputs: add skill **34**, COM **5**, stats 0x2A/0x1A. Fix Breakthrough stats 0x27/0x28 and Cross stat 0x2A. Remove the Cross comment at `pescarrier.cpp:644` or make it true.
9. **Feint (17)** with the anime FEINT (`0x14212D910`).
10. **Shared structure.** Per-player sorted distance lists (11 × `{id, dist²}` per team, rank byte). Needed by steps 4, 6, and OnePattern (23). Build once in the match tick, not per call.
11. **Keeper family (18–22)**, separate port (`peskeeper.*`).
12. **Command bridge.** Map each action id from §6 to a GF `e_FunctionType`, replacing the direct `PlayerCommand` writes once the walk is wired.

---

## 11. Conflicts and corrections

| claim | source | resolution |
|---|---|---|
| "only stored Dribble subs are 3/4/5 and type-4 0x11" | carrier-units §2.2 | **Corrected:** sub 2 (keep, `FUN_142119E90`) and sub 6 (feint, `FUN_142119E60`) are stored too (C-now). 0 and 1 have no writer. |
| "keep = no direction" vs "sub 2 = stand on the ball" | carrier-units §2.1 vs carrier-ai §5.4 | Both: the chooser returns a non-3–6 value → `FUN_142119E90` writes sub 2 (C-now). |
| "style bit 7 is read in the carrier code only once (Cross plan)" | carrier-ai §6.2 line 669 | **Incomplete.** Bit 8 is also read in the carrier brain, in PassForward's short tier (`0x142125C80`, C-now). |
| `pescarrier.cpp:644` "with the Crosser style" | GF code comment | **Stale.** The code does not read the style. |
| `0x142101AA7` is the tick entry | player-ai §1, carrier-ai §2 | Address inside `FUN_1421019D0` (C-now). |
| player-ai §2.3–2.4 (list and adoption) | player-ai | Superseded by carrier-ai §0 and §3 (no score; walk). |
| "tier 5 means zero delay" | carrier-ai §2 | Consistent with live `BP+0x60 = 500` (C-prior). |
| "answer id = 0x19 none; dribble kind 16 stored" | MainEval | C-now: default stores id `0x10` unconditionally (L74). GK kind `0x13` only selects the payload source. |
| Ghidra has no function at 0x142129110, 0x14212AD40, 0x142129BE0, 0x14212B9E0, 0x14212B520 | turn 1 probe | The shared bridge (port 8080) does not define them. **Resolved in §14.2**: defined in the PESLive project via `dec.sh`, and re-verified against the saved decompiles. |

---

## 12. Open items, with the experiment that would settle each

| item | why it matters | experiment |
|---|---|---|
| `PM+0x5D0` writer | the AI action state every other system reads | write-watch live `PM+0x5D0` for frames where `FUN_1405140E0` changes the id |
| `BP+0x10` (active flag) and `FUN_14092C9F0` gate `bVar2`/`bVar4` | which player thinks as carrier each frame | write-watch `BP+0x10`; log `bVar2`, `bVar4` per frame in a live match |
| `FUN_142117370` (Intention writer) | whether all listed units run (§3) | decompile; log the flags at L41–47 live |
| Stale default payload (`BP+0x4204`) | a wrong answer at tiers 0–4 | read `FUN_142119D20`, `+0x4204` writers |
| Id `0x10` vs `0x13` for the keeper default | which unit's record is the answer | trace `BP+0x41E4` on a keeper frame |
| Action → MOVESTEP / anime request | how the command reaches the body | decompile `FUN_1409F8030` cases 0x16, 0x19, 0x12; follow to `req[0x38]` writers |
| Kick launch timing | which Action update launches the ball | follow ANIME_SHOOT/SHORT_PASS writers (§7.8) |
| Lane helpers `0x14063DE20`, `0x14063CD90`, `0x140A90FD0` | blocks shot sub-kinds and aim | decompile (C-prior: open internals) |
| `0x14212F1C0` w_spd divisor (`0x14259D668`) | Dribble keep/turn economics | raw re-read (C-prior open) |
| Forced-clear test (adopter rule 7) | clear vs pass | decompile counts in `FUN_14210F110` |
| Live capture of the walk | whether the listed kinds match | the carrier snapshots in `live_carriers.jsonl` (C-prior) |

No live capture was taken for this report. All live counts are C-prior.

---

## 13. Verdict per requested item

- **1. Unit list and arbitration:** CONFIRMED. §0 (1–2), §1 (C-now), §3 (C-now run loop, C-prior list), §5 (C-prior walk).
- **2. Each unit's inputs and output:** CONFIRMED for the 25 kinds in §4 (C-prior VAs; C-now for 12, 14-related, 16, 23 tails and the translator). The "output" column is the translator's action id (§6, C-now). Units with no Ghidra function are marked in §11.
- **3. How the command reaches MOVESTEP and the kick:** CONFIRMED to the Action dispatch (§7.1–7.6, C-now). Action → MOVESTEP and the kick trigger are OPEN (§12).
- **GF port map:** §9. Ported (wired, partial): Shoot (binary), PassForward. Ported (pure, unwired): Dribble direction. Plan-only: Long (plan 6), CurveShot (plan 3). Absent (21 kinds): 1, 2, 3, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 17, 18–24. Not ported by design: 15.

---

## 14. Turn 2: Main's four items, and the GF map correction

### 14.0 Live state and method
- Live PES (pid 2510857, `PES2021.exe`) is in a menu: `G = *(0x1436F3DC8) = 0x371d5090`, `MI = *(G+0x270) = 0`, so no match is loaded. **No live read this turn.** `/proc/<pid>/mem` needs `sudo -n` (works here); the read-only scripts are `live_mi.py` and `live_gate.py` in the scratch dir.
- Binary scans (scratch): `scan5d0.py` (pdata functions), `scanlin5d0.py` (linear sweep of `.text`, covers functions with no `.pdata`), `scanlea.py` (address-of `+0x5D0`), `scancall.py` (call and jmp sites to a VA). Image: `~/ghidra-projects/scratch/Unpacker/pes_full.bin`, base 0x140000000.
- Ghidra: the five unit entries are defined in the PESLive project through `dec.sh` (read-only, creates functions on demand). The shared bridge on port 8080 still does not define them.

### 14.1 Item 1: writer of `PM+0x5D0`: setter found (CONFIRMED), link to PM INF(high)
- **Setter `FUN_140A6CA10`** (C-now decompile and disasm `0x140A6CA10–0x140A6CA37`, after `int3` padding; no `.pdata`, so the pdata scan missed it; the linear sweep found it):
  ```
  prev = rec.+0x5D0;  rec.+0x5D4 = prev;  rec.+0x5D0 = id
  if prev == id: rec.+0x6B8 += 1   else: rec.+0x6B8 = 0
  ```
  So `+0x5D4` is the previous action and `+0x6B8` is the frames-held counter.
- **Callers** (linear call scan, two sites, C-now):
  - `FUN_14092D580` @0x14092D607, the per-player publisher: `SetAction(param_1+0xC8, param_1+0xB5AC)`.
  - `FUN_140A36D5F` @0x140A36D6F (a mid-function entry; the decompile reads `unaff_RSI`): `FUN_1405140E0(…)` @0x140A36D5F, then `SetAction(RSI+0xC8, id)`.
- **Why the record is PM (INF high):** `FUN_1405140E0` compares `PM+0x5D0` with the new id (C-now `dec_0x1405140e0.c` L17–18) before the setter writes it. The compare is only meaningful if the setter writes that same object, so `record+0xC8` is the object `FUN_140A48A10` returns. Both `record+0xC8` and PM are passed to `FUN_140A76240` and `FUN_140A6E6B0` in the same way (C-now). Not yet checked by a live write-watch.
- **Oddity to check:** the publisher reads PM+0x5D0 at L126 and compares it with `param_1+0x698` (that is `record+0xC8+0x5D0`), right after the setter wrote the same value. If the objects are shared, that compare is always true, and the `+0x146C` counter then counts every frame. Verify live.
- **Killed candidates** (C-now): `FUN_1409F4580` writes `param_3+0x5D0` (the decision record, L179), not PM. `FUN_14095E890` writes its own object's `+0x5DC`. `FUN_1404A0B90` copies a vec3 from `+0x5C0`. `FUN_14093B530` writes a vec3 position at `+0x5D0`. `FUN_140921BD0` (initializer) writes `PM+0x5D0 = 0xFF` as the reset. `FUN_1409212C0` writes `qword 7` at `+0x5D0` in a different record.
- The output-side pair `FUN_140A85A70` / `FUN_140A85A40` writes `rec+0xC` (id, through `FUN_140A76070` or `FUN_140A76020`), `rec+0x578` (tier) and `rec+0x575` (flag). `FUN_140A85E80(id)` returns the action name for the log. 46 call sites (linear scan).

### 14.2 Item 3: the five unit entry points: defined and re-verified
Defined in PESLive (`units5.txt`), then diffed against the saved decompiles (`CarrierAIRE/units_missing.c`, `AttackDecisionsRE/cross.c`) after normalizing Ghidra naming (`func_0x`/`FUN_`, `_DAT_`/`DAT_`) and whitespace.

| VA | unit (table §4 row) | result vs saved |
|---|---|---|
| `0x142129110` | BackSpace, kind 3 | same callees and constants; one wrapped condition |
| `0x14212AD40` | PassLong, kind 9 | identical |
| `0x142129BE0` | DirectHighBall, kind 11 | same callees, constants and globals; control flow reshaped (`return` against `goto` to one epilogue, merged `\|\|` conditions), one double sign-mask written as two halves, one `0xb` loop bound reshaped. Equivalent on inspection, not proven |
| `0x14212B9E0` | PassTarget, kind 13 | identical except the stack-cookie call `fun_14159f3e0` |
| `0x14212B520` | Safety, kind 6 | identical except one wrapped call line |

The five are now C-now decompiles. I did **not** re-audit each table row's inputs and outputs line by line this turn; the rows keep their C-prior marks.

### 14.3 Main's correction: `MA+0x22E4C` is on main as `NearestList`
- `src/sim/onthepitch/pesoffball.hpp:200–209`: `struct NearestEntry`, 200 bytes (`static_assert`). `team[2][11] {id, d2}` at `+0x00` (team 0) and `+0x58` (team 1); `count[2]` at `+0xB0`; `rank[22]` at `+0xB2`. This matches my §8.5 layout.
- Built by `BuildNearest` (`pesoffball.cpp:1319`), read by `NearestOpponents(const RunWorld&, int id)` (`pesoffball.cpp:1340`, declared `pesoffball.hpp:232`). `kNearRankAbsent = 0x0a`.
- Carrier readers in §8.5 (PassForward short tier, DirectHighBall, Special, OnePattern emitter) map to `NearestOpponents`. The carrier path in the checkout (`elizacontroller.cpp:2078–2085`, `:2372–2375`) still scans linearly; it is not wired to `NearestOpponents` yet.
- Clean cutover (when a carrier unit is wired): delete the linear nearest scans at those lines, and use `NearestOpponents`.

### 14.4 Item 4: the `bVar2`/`bVar4` gate in carrier-sense: decoded (C-now)
- The tick call (L199) sits in the `param_5 == NULL` branch (no event record, L114–246). The `else` branch (L247) handles event-driven carriers and never runs the tick.
- **Gate** (L194–196): `(A && B && !C) || (!bVar4 || bVar2)`
  - **A** `G+0x400 != 0`: the online-practice object (C-prior duel-sim E0, setpiece-kicker §1.2).
  - **B** `*(G+0x400) != 0` byte (`FUN_141F9C3B0` = `*param`, C-now).
  - **C** `FUN_140A59AF0(G+0x400, player)`: a per-player bit in the list at `obj+0x3C` (team 0) or `obj+0x40` (team 1) via `FUN_140A59B20` (C-now).
- **bVar4 = `!bVar22 && R`**
  - **R** (restart loop, L117–157): set piece (`MI+0x116C == 3`), `P.+0x14 == 0`, `MI+0x173C == player`. Then the 24-entry pad table `T = *(G+0x180)`, stride 0x74: entries with `+6 == 0`, `id(+0x20) < 0x16`, on the player's side. R is true when the first such entry is this player, or the count from the `FUN_141F9C3B0` call is ≤ 1. Ghidra shows that call with no argument (INF: it is the same object).
  - **bVar22** (L158–177): the player's own pad entry has `+0xD != 0` (disabled).
  - The pad-table semantics come from C-prior docs: `+0x20` = controlled player id (255 = none), `+0xD` = disabled.
- **bVar2 (final, L185–192)**: some AI record of the player's team (`FUN_140A489F0` over `team·11 … team·11+10`) has `+4 != 0`.
- **Reading:** the tick is skipped only when A–C do not open it, the player is a set-piece taker whose pad entry is disabled (or R), and no teammate's AI record has `+4`. That is the whole gate.
- **Offline it is open.** Live runs 6 and 7 (C-prior, `pes-re-live-matchball.md`) saw all 24 rows at `(+0xD, +0x20) = (0, 255)` for about 2300 s. Id 255 is never < 0x16, so R is false and bVar4 is false, and the gate passes. INF (high) for offline modes with no pad-controlled players. Not re-observed this turn (menu state). Confirm with `live_gate.py <pid>` once a match is loaded.

### 14.5 Item 2: Action → MOVESTEP and the kick trigger: PARTIAL
- **Confirmed chain (C-now):** MainEval answer (`BP+8`) → carrier-sense `FUN_14092C9F0` (id) → `FUN_14092A340` (L313, L346) `FUN_1409F5240(P_ai, ctx, player, id)` → `FUN_1405140E0` (table `P_ai+0x19E90`; old object vt+8 on a change, new object vt+0x10) → `FUN_140A36D5F` after it: `SetAction(RSI+0xC8, id)` (§14.1).
- **Decision and output record** (C-now): `FUN_1409F4580` (called from `0x1409F209B`) takes the decision record `param_3` and PM as `param_4`. It sets `param_3+4` (the id, after its remaps), `param_3+0x5D0` (L179), and calls `FUN_140A85A40/A70(param_3+0x50, …)`, which writes `rec+0xC` (id), `rec+0x578` (tier), and `rec+0x575` (flag), with `rec = param_3+0x50`.
- **OPEN:** who reads `param_3+0x50+0xC/0x575/0x578`, which is the step into MOVESTEP (`FUN_1406BA900` / `FUN_1406B8340`, the request `req[0x38]/[0x39]`). Not traced this turn. Next: scan loads of `+0x578` / `+0x575` and their callers for the MOVESTEP request writer.
- **Kick trigger: OPEN.** C-prior only: `FUN_1407324B0` callers at `0x140731FFB` and `0x140732316`, the request builder `FUN_140833CD0` (caller `FUN_140717E10`), and the anime Kick writers `0x140717C50`. Not re-traced this turn.

### 14.6 Corrections to earlier sections
- §7 OPEN "`PM+0x5D0` writer not found": resolved in §14.1 (setter `FUN_140A6CA10`; link INF high).
- §11 "Ghidra has no function at …": resolved in §14.2.
- §12 items "PM writer" and "`bVar2`/`bVar4`": resolved in §14.1 and §14.4. "Action → MOVESTEP" and "kick timing": still open (§14.5).
- §8.5 and §9: the MA+0x22E4C GF status changed (§14.3).

### 14.7 Summary for Main
1. `PM+0x5D0` setter: `FUN_140A6CA10` (prev → `+0x5D4`, new → `+0x5D0`, held counter `+0x6B8`). Callers `FUN_14092D580` and `FUN_140A36D5F`, both on `record+0xC8`, which is PM by INF (high).
2. Five unit entries: defined in PESLive, matched to the saved decompiles (four identical or cosmetic, one control-flow reshape, equivalent on inspection).
3. Gate: decoded. Offline it is always open (C-prior live data, INF high). Pad and online paths close it.
4. Action → MOVESTEP: partial. The decision record `param_3+0x50` is identified; its reader is open. Kick trigger: open.
5. GF: `MA+0x22E4C` is on main (`NearestList`). The carrier path still scans linearly.

