# PES attack level (mentality) → GF port plan

> **Main note 20-10-26:** a read-only plan (MentalityPlan). Its §4 corrections to docs/re/pes-re-attack-level-pull.md (forwards are in the quota's first `need` entries; the protection scan also breaks on the advance byte; pick arms 7/8/9; FUN_1405AAF00 admits all class > 1 at L1-3) are NOT yet Main-audited; the RE slices verify them first.

Read-only plan. No code edits, no builds. Every PES claim carries a VA and a verdict: CONFIRMED (disassembly or image bytes read this pass), INF (inference), OPEN (blocker named). Constants are read from the install image (pes_full.bin, offset = VA − 0x140000000); §0 is that script's output. Source doc: `docs/re/pes-re-attack-level-pull.md`.

Refs: GF `main` = `ed4cb7c` (the tree read; = origin/main). `origin/ClipWords-integrated-wip` = `fbaef6f`, 91 ahead and 4 behind main; it contains RoleArrays (role-class port 8cbf2a1 / 8f57eda, FUN_140a92950 port 9d31415). `origin/TeamSliceA` = `24621ce` (attack-level drift, team tick) and `origin/MergeRA` = `ceb7991` are NOT ancestors of ClipWords.

## Summary

1. The GF slider (`TeamInstructions::State::mentality`, five rungs identical to PES's T+0xB8A4 levels 0..4) already drives: set-piece box and far-post counts, the Chance run cap, the offball permit mask, and the ForeCheck line-down flag (T+0xB8A3). It does NOT drive the defensive shape: `teamAIcontroller.cpp:416` hard-codes `attackLevel = 2`, so line height, team length, half-width and steal radius ignore the slider.
2. The forward pull (sub_140682770: level factor, protected forward, floor, +10 m counter) is not ported. GF's out-of-possession forward target is the high end at every mentality (`teamAIcontroller.cpp:634-638`, with the team length built at the literal level 2). The counter-attack offset in `mixup` (`:325-335`) is a stand-in that must go at cutover.
3. The attack-join quota (sub_140500200) is ported with literal constants and matches the disassembly in ladder, ahead gate, −100 stamp and forward admission. On ClipWords its class test reads `roleClass` (array B = TeamData+0x4C00), so the RoleArrays landing supplies that input.
4. The forward pull does NOT read array B. Its pick, depth loop and the style pull's CF test read array P = TeamData+0x3FB8, copied into the function context by `FUN_1405EC600` (0x1405EC9E7/0x1405EC9EE). RoleArrays decodes only A (+0x4BD4) and B (+0x4C00). The RoleArrays merge does not unblock the forward pull; P's writer must be decoded first (D1, RE-1).
5. Doc errors (§4): forwards are not excluded from the quota (they consume it); the protection scan has a second break condition (a scalar advance byte); the pick arms are classes 7/8/9, not 6/7/8. The doc also misses the L1–3 permit branch FUN_1405AAF00, which can admit every class > 1 slot without the quota (RNG-gated).
6. Ready now, no busy files: the quota constants and gates (Q1), the pure forward module (F1), and the style and window pure functions (S1, S2, once their gates land). Anything in `teamAIcontroller.cpp` or `elizacontroller.cpp` waits for the MOVESTEP landing. Level wiring also waits for TeamSliceA.

## §0 Image constants (script output, pasted)

Script: read 4 bytes at VA − 0x140000000 from pes_full.bin, unpack little-endian float32. The instruction VA shown is where the disassembly read the operand.

```
0x142592228 0000803e 0.25   forward factor        (0x140682BE4 MULSS XMM7,[0x142592228])
0x1425656c0 00008040 4.0    level clamp           (0x140682BD5 COMISS XMM7,[0x1425656C0])
0x14256ae1c 0000803f 1.0    unit                  (0x140682B59 MOVSS XMM9,[0x14256AE1C])
0x1425656d0 00000080 -0.0   XMM12 reload          (0x1406830BC MOVSS XMM12,[0x1425656D0])
0x14259bdf4 00000041 8.0    style pull            (0x1405E7318 MOVSS XMM15,[0x14259BDF4]; doc 0x1405E7AE3)
0x14259bdf0 0000e040 7.0    other term            (0x1406828B8 ADDSS XMM1,[0x14259BDF0]); NOT the style pull
0x14259222c 0000a040 5.0    window                (0x1405E7723 MOVSS XMM0,[0x14259222C]; doc 0x1405E7B39)
0x14259d5d0 6666263f 0.65   rank scale            (0x1406832D9 MOVSS XMM6,[0x14259D5D0])
0x142598578 cdcc4c3e 0.2    rank scale            (0x1406830C7 MOVSS XMM15,[0x142598578]; 0x140682E30 MULSS XMM7,XMM15)
0x142591140 0000f041 30.0   pick bonus, CF        (0x14068305C ADDSS XMM6,[0x142591140]); dead for the pick
0x14259be84 000020c1 -10.0  pick bonus            (0x140683089 ADDSS XMM6,[0x14259BE84]); dead for the pick
0x1425c6614 0050c3c7 -100000.0 pick guard         (0x140682FA2 MOVSS XMM12,[0x1425C6614])
0x142588210 00002041 10.0   quota near edge; +10 m (0x14050059B MOVSS XMM15,[0x142588210]; 0x1406834BD MOVSS XMM2,[0x142588210])
0x14259be20 0000c841 25.0   quota far edge        (0x1405005AE MOVSS XMM7,[0x14259BE20])
0x14259d668 000070c1 -15.0  quota falloff divisor (0x1405005B9 MOVSS XMM12,[0x14259D668])
0x14259d654 000040c0 -3.0   quota turning side    (0x1405005CB MOVSS XMM10,[0x14259D654])
0x142588218 000080bf -1.0   sign                  (0x140500470 MOVSS XMM14,[0x142588218])
0x14259be0c 00007041 15.0   turning threshold     (0x1405004F0 COMISS XMM1,[0x14259BE0C])
0x14259be34 00004842 50.0   midpoint scale       (0x140682DE9 MULSS XMM8,[0x14259BE34])
0x14253ee30 0000c842 100.0  normaliser            (0x140682CA8 MOVSS XMM13,[0x14253EE30])
0x142565600 0000003f 0.5    half                  (0x140682CB1 MOVSS XMM15,[0x142565600])
0x14259a760 00000c42 35.0   ceiling               (0x140683536 COMISS XMM1,[0x14259A760])
0x1425c48c0 0000b041 22.0   ceiling               (0x140682EF8 COMISS XMM0,[0x1425C48C0])
0x14259be04 00004041 12.0   ramp span             (0x140682F08 DIVSS XMM6,[0x14259BE04])
0x142565604 00000040 2.0    base                  (0x140682B79 SUBSS XMM0,[0x142565604])
0x14257a194 3333333f 0.7    weight                (0x140682CD7 MOVSS XMM11,[0x14257A194])
```

Immediates (CONFIRMED in the disassembly): quota ladder `MOV byte [RSP+0x20],imm` = 0/2/4/6 for L = 0..3 (0x140500321, 0x14050031A, 0x140500313, 0x14050030C); L ≥ 4 `CMP EBX,0x4` at 0x140500232; forward stamp `MOV dword [RSP+RAX*8+0x44],0xC2C80000` = −100.0 (0x140500693); class gate `CMP EDX,0x7` (0x140500668).

## §0b Switch and jump tables (script output)

Targets are 0x140000000 + the table entry. Class index = class − 1 (classes 1..9).

```
table 0x140683D60 (pick, indirect jump at 0x140683025):   class 1-6 -> 0x140683099 (skip); class 7,8,9 -> 0x140683027 (arm)
table 0x140683D3C (line class, indirect jump at 0x140682D61): class 1,2 -> EDX=0 (0x140682D71); class 3-6 -> EDX=1 (0x140682D63); class 7,8,9 -> EDX=2 (0x140682D6A)
table 0x140683D18 (A array, indirect jump at 0x140682A2A): class 1,2 -> 0 (0x140682A3A); 3-6 -> 1 (0x140682A2C); 7-9 -> 2 (0x140682A33)
table 0x140683D84 (mid loop, indirect jump at 0x140683160):  class 1,2 -> 0 (0x140683178); 3-6 -> 1 (0x140683162); 7-9 -> 2 (0x14068316D)
```

Class names (`pesstyle.hpp:25`, `pesroles.hpp`): GK0 CB1 SB2 DMF3 CMF4 SMF5 AMF6 SS7 WF8 CF9. Line 2 (forwards) = {7,8,9}. The doc's "7/8/9 = forwards" is right. The directive doc's "6/7/8 arms" is wrong (C3).

## §1 GF consumers of the attack level

State key: wired = reads the mentality; stand-in = a literal or other source; absent = no reader.

| # | Consumer | GF site (main ed4cb7c) | Reads | State |
|---|---|---|---|---|
| 1 | Mentality from sheet | `teaminstructions.cpp:7-10`, enum `teaminstructions.hpp:14-24` | tactics "mentality", default Balanced | wired; rung = PES byte (identity, `pessetpieceteam.hpp:113-119`) |
| 2 | Set-piece level, box count, far-post man | `teamAIcontroller.cpp:1093-1094`, `:1245`, `:1266`; `pessetpieceteam.cpp:168-171` | mentality | wired (test `pes_setpiece_team_test.cpp:425-436`) |
| 3 | Chance run cap | `teamAIcontroller.cpp:2452-2456` → `pesoffball.cpp:257-263` (RunCap) | mentality | wired; tiki-taka off (no instruction 6) |
| 4 | Offball permit mask | `teamAIcontroller.cpp:2495` (ClipWords `:2533`) → `pesoffball.cpp:1639-1689` | mentality | wired; forward test: main = `position`, ClipWords = `roleClass` (B) |
| 5 | Chance gate, level < 1 | `pesoffball.cpp:1733-1734` (main), `:1724` (ClipWords `cls`) | level | wired; class base to verify (RE-13) |
| 6 | ForeCheck line-down flag (= T+0xB8A3) | `teamAIcontroller.cpp:1841-1842` → `pesteamconcepts.cpp:779-780` | mentality == AllOutDefence | wired (equal to level < 1 for 0..4) |
| 7 | Defensive line height, team length, half-width | `teamAIcontroller.cpp:413-416` (`attackLevel = 2`), `:425`, `:431`, `:472`, `:658-661` → `pesdefshape.cpp:153-226` | literal 2 | NOT wired |
| 8 | Steal radius | `teamAIcontroller.cpp:960-962` | `defensiveAttackLevel` (2) | NOT wired |
| 9 | Out-of-possession forward target | `teamAIcontroller.cpp:625-638` (`lineIndex == 2`: high end) | none | NOT a PES pull (no level term) |
| 10 | Counter-attack forward offset | `teamAIcontroller.cpp:325-335` (mixup), called `:513-548` | tactic `counter_attack` | stand-in; delete at cutover |
| 11 | Carrier touch level | `elizacontroller.cpp:2917-2918` → `pescarrier.cpp:2227-2228` | literal 0 ("G8 not carried") | NOT wired; always 0 |
| 12 | Eliza deep-defensive mode | `elizacontroller.cpp:836-844` | mentality ∈ {0,1} | stand-in for T+0xA298 |
| 13 | In-play strategy copy | `teamAIcontroller.cpp:2097-2101` (`UpdateStrategy`) | `StrategyAttackLevel(strategy)` | stand-in; TeamSliceA replaces it with the drift |
| 14 | Attack-level writer (drift) | none on main; TeamSliceA `FUN_1404F8350` port | — | pending (TeamSliceA) |
| 15 | Forward/line index | `teamAIcontroller.cpp:441`, `:625`, `:654` (`PesLineIndexForRole(role)`) | GF formation role | stand-in for P-based line index |
| 16 | Receiver (turning, −3 side) | `teamAIcontroller.cpp:2393-2397` (carrier / last touch / designated) | GF roles | stand-in for FUN_1405969B0 |
| 17 | Run-depth style term | `teamAIcontroller.cpp:746-754` | GoalPoacher / FoxInTheBox only | partial |

## §2 Rule → GF mapping

Columns: PES rule (doc §) · PES evidence · GF change (file:line) · GF today · missing data (§3) · lands with RoleArrays? · pinning test (§5 slice).

The forward target formula, read from the line-2 path (0x140682DD3–0x140682E59): `target = hi + (lo − hi)·f·scale`, with `f = clamp(level,0,4)·0.25`, `hi = [RBP−0x3C+4·line]` (deep: f = 0 gives a full drop) and `lo = [RBP−0x48+4·line]` (high: f = 1). INF on the names; CONFIRMED on the arithmetic.

| ID | PES rule (doc §) | PES evidence | GF change | GF today | Missing (§3) | RoleArrays? | Test |
|---|---|---|---|---|---|---|---|
| R1 | Dial = T+0xB8A4, rungs 0..4; −1 = byte 1 (§1) | store `0x1404F86E7`; gauge strings `0x1425C3B64` | none (identity) | identity, wired | — | no | existing `pes_setpiece_team_test.cpp:431-433` |
| R2 | Level drift and flags T+0xB8A2/B8A3 (§1, §6) | `0x1404F8350`; `0x1404F6E1C-58` | TeamSliceA `pesteamtick` (already written) | sheet level only | D12 | no (TeamSliceA) | slice L0b |
| R3 | Forward factor = clamp(level,0,4)·0.25 (§2) | `0x140682BB7-0x140682BEC`; 0.25 @`0x142592228`; 4.0 @`0x1425656C0` | new `pesforward.cpp` `ForwardFactor` (pure); call site `teamAIcontroller.cpp:625-638` | none | D12 | no | F1 `FactorIsLevelQuarter` |
| R4 | Forward target = hi + (lo−hi)·f·scale, i.e. deep + (high−deep)·factor (§2) | line-2 path `0x140682DD3-0x140682E59`; `hi`/`lo` rows filled by `CALL 0x140687FB0` (0x140682896) and `CALL 0x1406855F0` (0x140682901) | F2 `ForwardTarget` | `line + len` high end always | D4 | no (band fill undecoded) | F2 `TargetIsDeepPlusFactorSpan` |
| R5 | Protected forward = last eligible in slot order; the first counter or advance fallback ends the scan (§2) | pick loop `0x140682FA2-0x1406830AE`; fallback `0x1406830B0` falls through to `0x1406830B3` (past the loop); guard `COMISS` at `0x140683091` / `CMOVNC` at `0x140683095` | `pesforward.cpp` `ProtectedSlot` (pure) | none | D2, D3 (stand-ins false / > 1) | class input: NO (P) | F1 `ProtectedIsLastEligibleForward`, `CounterTargetStopsTheScan` |
| R6 | Eligible = on-pitch and class ∈ {7,8,9} (§2) | arm table `0x140683D60` (idx 6-8) | `ProtectedSlot` input | `PesLineIndexForRole` (GF role) | D1 | NO | F1 `ArmsAreSevenEightNine` (reads the table at runtime) |
| R7 | Line classes: 0–2 back, 3–6 mid, 7–9 forward (§2) | tables `0x140683D3C`, `0x140683D18`, `0x140683D84` | `pesforward.cpp` `LineOfClass`; replaces `PesLineIndexForRole` (`teamAIcontroller.cpp:441,625,654`) | GF formation role | D1 | NO | F1 `LineMapFromTable` |
| R8 | Rank scales for unprotected forwards, level < 4 (§2, §5). Two paths. (A) line-2 target, first loop: ×0.2 iff scalar `[obj+0x128] > 2` (gate `0x140682E02`) and predicate (`0x140682E1B`) and level (`0x140682E24`, `CMP R13B,4`); ×0.2 at `0x140682E30`. (B) pair path, second loop: needs count ≥ 3 (`0x140683285`), not protected (`0x14068328A`), mode-0 predicate true (`0x1406832A0`), level < 4 (`0x1406832AE`); then mode-1 predicate (`0x1406832C6`) → ×0.2 (`0x1406832CF`) else ×0.65 (`0x1406832D9`) | `pesforward.cpp` `RankScale` (predicate injected) | none | D5, D3 (scalar +0x128) | no | F3 `RankScalesOnlyWithThreeForwards` (B) and `LineTwoScaleNeedsScalarAboveTwo` (A) |
| R9 | Floor fmax(−ref, target) on the protected forward only (§2, §5) | `0x140683363` (`MAXSS XMM0,XMM7`), re-applied at `0x1406837B6`; ref `[[ctx]+0x44]` (`0x14068335A`); gate BL (see C10) | `pesforward.cpp` `ProtectedFloor` | none | D6 | no | F3 `FloorOnProtectedOnly` (gate OPEN) |
| R10 | Instruction 15 (READY_COUNTER) adds +10 m to the line-0 target (back line, class 1–2) (§6 of team-layer doc) | path `0x14068348E` (R10 = 0); `LEA R8D,[R10+0xF]` at `0x14068349F`; `CALL 0x140A63950` at `0x1406834B8`; `ADDSS XMM7,XMM2` (= +10.0) at `0x1406834C9` | F3 hook | none | D2 | no | F3 `ReadyCounterAddsTenMetresOnBackLine` (injected) |
| R11 | Style pull +8 m toward own goal for every forward past two gates (§3) | `0x1405E7AD1` gate, `0x1405E7AE3` `MULSS 8.0`, `0x1405E7AEE` `ADDSS` (doc; 8.0 at 0x14259BDF4 CONFIRMED) | `pesforward.cpp` `StylePullMetres`; wire in the master-equivalent slot target (`teamAIcontroller.cpp:~640-660`) | none | D7, D1 (CF), D2 (designated) | no | S1 `EveryForwardPastGatesGetEightMetres` |
| R12 | +5 m window on forwards not designated, not Fox, not CF (P=9), +0x80D4 clear (§3) | `0x1405E7B0B` Fox bit 2 (`edx=2`), `0x1405E7B25` `cmp edi,9`, `0x1405E7B39` `ADDSS 5.0` (doc) | `pesforward.cpp` `WindowMetres` | none | D7, D1, D2 | no | S2 `WindowFiveMetresForNonExempt` |
| R13 | Fox exemption = style bit 2 | `pes::style::Gate` exists (`pesstyle.hpp:34`; EngineBit FoxInTheBox = 2, `pesstyle.cpp:17-21`) | reuse `Gate`; input class per RE-10 | Gate present, unused by the master | D1 (class array for the call), D15 | class array: OPEN | S2 `FoxSkipsWindow` |
| R14 | Quota ladder: need = 0/2/4/6 for L = 0..3; all for L ≥ 4 (§4) | `0x1405002F4-0x140500321`; `0x140500232` (L ≥ 4 → all) | none (already ported) | ported (`pesoffball.cpp:1641-1643`) | — | ladder no change; class YES | Q1 `QuotaLadderByLevel` |
| R15 | Forwards force-permitted (`BTS` at `0x1405006A2`) and stamped −100.0 (`0x140500693`) (§4) | forward path `0x140500663-0x1405006A6` | class test `roleClass ≥ 7` | ClipWords ported (`pesoffball.cpp:1665-1667`); main uses `position` | — | YES (B) | Q1 `ForwardsAdmittedAndCountTowardNeed` |
| R16 | Ahead gate: if max(FUN_140A4F230 count, front-first beyond count) < need, permit all (§4, C5) | `CALL 0x140A4F230` at `0x140500341`; compare `0x1405003ED`, `JNC` `0x1405003F1` → all-slot fill `0x1405003F3-0x1405003FD` (→ `0x14050088E`); otherwise continue at `0x140500402` | none (ported `pesoffball.cpp:1644-1652`) | ported | — | no | Q1 `AheadGateAdmitsEveryone` |
| R17 | Non-forward score: line-list float − 5·falloff over 10..25 m; −3 when turning and same side (§4) | score load `0x1405006DF`; falloff `0x140500721-0x14050074B`; side test `0x14050074F-0x14050078E` (−3 at `0x14050078E`) | Q1 constants read from the image; score source (RE-14) | literals 10/25/−15/5/−3 (`pesoffball.cpp:1680-1683`); score = `pesFieldHalfLength − s·x` (INF) | D10 (side), D14 (score) | class YES; score source NO | Q1 `FalloffAndSideAreImageConstants` |
| R18 | Turning test: \|team+0x250 − receiver heading\| > 15.0 (§4, C9) | team+0x250 load `0x1405004B8`; `CALL 0x14043A0A0` at `0x1405004D9`; `COMISS` with 15.0 at `0x1405004F0` | Q3 | GF: velocity vs facing > 15 (`pesoffball.cpp:1664-1668`) | D10 | NO | Q3 `TurningUsesHeadingNotVelocity` |
| R19 | Receiver id = FUN_1405969B0 (§4, C8) | decompiled: restart taker (MI+0x173C/0x1740 under MI+0x1554 == 5 and MI+0x1558 == team), holder on team, MA+0x2483C, else T+0x78 | Q2 (replace `teamAIcontroller.cpp:2393-2397`) | carrier / last touch / designated | D9 | NO | Q2 `ReceiverOrder` |
| R20 | L1–3: FUN_1405AAF00 true → permit every slot with class > 1; no quota (§4, C6) | `CALL 0x1405AAF00` / `TEST AL,AL` / `JZ 0x1405002CA` at `0x140500262`; mask loop `0x140500266-0x1405002C5` (BTS on B > 1) | Q4 (new branch in `AttackSlotMask`) | never true (stand-in) | D8 (RNG, instr-2 gate) | class YES; predicate NO | Q4 `PermitAllAboveOneWhenPredicateTrue` |
| R21 | b8a3: level < 1 blocks positions 0..4 (non-forward through-ball) | ported (`pesoffball.cpp:1734`) | re-base check (RE-13) | ClipWords `roleClass` at `:1724` | — | YES (verify) | Q5 `LevelZeroBlocksClassFourOrLess` |
| R22 | Level drives defensive shape: line, length, width, steal radius (`pes-re-defensive-shape.md:296`: the live value is 2 for both teams) | `FUN_1405F9EB0` ramp; length 46/42 lerp; width −6·ramp (doc) | L1: replace literal 2 (`teamAIcontroller.cpp:416`); steal `:960-962` | literal 2 | D12 (teamTick) | no | L1 `DefensiveLineFollowsMentality`, `StealRadiusFollowsLevel` |
| R23 | Carrier run count reads the level (`pescarrier.cpp:2227-2228`, `in.attackLevel ≥ 3`) | — | L2: `elizacontroller.cpp:2918` = `pes::AttackLevel(mentality)` | literal 0 | — | no | L2 `CarrierTouchCarriesMentality` |
| R24 | Dial steers registry style state (0/1 → 1, 2 → flag, 3/4 → 0) (§4) | `0x140501061-0x140501121` | none yet | none | RE-15 | no | deferred |
| R25 | Equal drops of two strikers; Goal Poacher partner reason (§5) | doc §5 (dead reasons, audited) | no port of the partner; pin the equal-drop shape | — | — | no | F2 `TwoForwardsAtMinusOneShareLine` |
| R26 | Live check (§6): the COM byte stayed 2; the retune claim unsupported in that window | doc §6 | validation item, not a port | — | RE-12 | no | none (live frame) |

## §3 Missing data

| ID | Data | PES source | GF today | Lands with RoleArrays (ClipWords)? | RE ticket |
|---|---|---|---|---|---|
| D1 | Array P = TeamData+0x3FB8 (11 dwords): forward/line classes (ctx+0x128 copy), CF test in the style pull | `FUN_1405EC600` 0x1405EC9E7/0x1405EC9EE (writes ctx+0x128 = `&local_2a0`, `0x1405ECA0A`); master arg3 `0x1405E71EF`, `0x1405E73C9` | none (line index from GF role) | **NO**: RoleArrays decodes A (+0x4BD4) and B (+0x4C00) only | RE-1 |
| D2 | Counter-target record (instruction id 15) and id 2 (designated CF); advanced-instruction table (+0x25CF8/+0x25D38) | tester `FUN_140A63950` (`0x140683038`, `0x1406834B8`), `FUN_140A64030`; designated gate `FUN_140A63680` (`0x1405E7AFA`) | none; `teaminstructions.hpp:1-3` says no advanced instructions | NO | RE-6 |
| D3 | Scalar bytes of the per-tick object `[ctx+0x118]`: +0x129 (advance, ≤ 1 is a fallback) and +0x128 (path A gate > 2; pick gate == 2) | `0x140683041-0x140683050`; `0x140682E02`; `0x140683064` | defaults (advance > 1; scalar above 2 is not modelled) | NO | RE-5 |
| D4 | Line-2 band rows: lo = `[RBP−0x48+4·line]`, hi = `[RBP−0x3C+4·line]`, filled by `FUN_140687FB0` (0x140682896) and `FUN_1406855F0` (0x140682901); read at `0x140682DD3`, `0x140682DD9` | fills and reads above | GF has back and mid bands only (`teamAIcontroller.cpp:457-469`); no line-2 band | NO | RE-3 |
| D5 | Rank predicate `FUN_1406405C0`: calls at `0x140682E1B` (path A), `0x140683080` (pick, dead), `0x1406832A0` (B, mode 0), `0x1406832C6` (B, mode 1) | — | none | NO | RE-2 |
| D6 | Floor reference `[[ctx]+0]+0x44` and floor gate BL = `FUN_140687A80` return (`CALL` at `0x140682C74`, stored at `[RSP+0x20]` by `0x140682C9F`) | `0x14068335A`, `0x140683350` | none | NO | RE-4 |
| D7 | Style-pull gates: skip flag `test al,al` (0x1405E7ACD); var_3C4 (0x1405E7AD1); +0x80D4 byte (per-slot row 0x80D4..0x80DE) | doc §3 | none | NO | RE-10 |
| D8 | FUN_1405AAF00 sub-predicates: holder `FUN_140A627E0`; `FUN_140A50250(…,2)`; RNG `FUN_141E532D0` (draw, stream); `FUN_140A57800`; `FUN_140A4C9A0(…,−12.0 @0x14259D664)`; `FUN_140A4C720` (instruction-2 gate); SPACE_RUN (0x1E) scan over T+0xA2A0; thresholds DAT_14259D620, DAT_1425A9BF0, DAT_14259D5D4; MI+0x11A0 timer | decompile of `0x1405AAF00` (structure CONFIRMED) | none (predicate = false) | NO | RE-7 |
| D9 | Receiver: `FUN_140A6E320` (restart-taker predicate), `FUN_140A627E0` (holder), MA+0x2483C, T+0x78 designated | decompile of `0x1405969B0` (structure CONFIRMED) | carrier / last touch / designated | NO | RE-8 |
| D10 | Turning operands: team+0x250 (GF uses the attack direction, `teamAIcontroller.cpp:2467`), `FUN_1404AB8E0` (receiver heading), `FUN_14043A0A0` (fold); side test operands (aim.z − receiver.z) | `0x1405004B8-0x1405004F7`; side `0x14050052F-0x14050053F` | velocity vs facing | NO | RE-9 |
| D11 | pesPenaltyAreaLineX = `[[0x1436F3DC8]+0x2AA8]+0x18` (quota "beyond" count and FUN_1405AAF00 gate) | `0x14050035A-0x140500368` | GF constant `pesPenaltyAreaLineX` (value not compared) | NO | RE-15 |
| D12 | Attack level per team = drifted T+0xB8A4 (PES path) | TeamSliceA `FUN_1404F8350` | sheet mentality; literal 2 for defence | TeamSliceA not on ClipWords | L0b |
| D13 | Designated man TeamData+0x3EF8 (the master's designated tests) | `0x1405E94EC` (`CMP [RDI+0x3EF8],R13D`); writer FUN_1405FC860 (doc) | "none" (TeamSliceC notes) | NO | RE-1 (writer) |
| D14 | Quota score source: line-list record float | `FUN_140A714E0` records; load `0x1405006DF` `MOVSS XMM6,[RDX+RCX*8+4]` | `pesFieldHalfLength − s·x` | NO | RE-14 |
| D15 | Per-player style dword (doc: player+0x48) | `CheckStyleBitmask` (doc §3) | one style per player mapped to one bit (`pesstyle.cpp:17-21`) | NO (unless the roster record carries several bits) | RE-10 |

## §4 Corrections to docs/re/pes-re-attack-level-pull.md

- **C1 (array).** The pick, the depth loop and the CF test read P = TeamData+0x3FB8, copied by `FUN_1405EC600` (loop at 0x1405EC9E7/0x1405EC9EE: `[R8+0x3FB8+4k]` with R8 = `[R15+0xBA8]` = TeamData; INF(high) from the designated-field stores at `[R8+0x3EF4]`). The doc's "sub_140682770 reads the same array at 0x1406834DE" is only partly right: 0x1406834DE reads `[arg2+0x3FB8]` with arg2 = the shape block S (the S+0x7C40 stores at `0x140683CCB`), so it is B (= TeamData+0x4C00). The forward pull reads both arrays.
- **C2 (scan breaks).** The protected scan is last-wins until a fallback slot. The fallback is a counter-target test OR the scalar advance byte `[[ctx+0x118]+0x129] ≤ 1` (one byte per tick, not per slot: `0x140683041-0x140683050`). The directive doc §2.4 wrongly makes advance per-slot.
- **C3 (arms).** The pick arms are classes 7, 8, 9 (table `0x140683D60`, idx 6–8). The directive doc's "6/7/8" is off by one.
- **C4 (forwards consume the quota).** The forward path both `BTS`s the forward into the mask and writes its −100 entry into the sorted list (`0x1405006A2`, `0x140500693`). The first `need` sorted entries take the quota, so forwards count toward it. With ≥ need forwards no non-forward joins. The doc's "top 2 non-forwards" is wrong. GF `pesoffball.cpp:1673-1687` is right.
- **C5 (ahead gate missing from the doc).** The quota applies only if max(FUN_140A4F230 count, front-first beyond count) ≥ need; otherwise every slot is permitted (`0x1405003ED`, `0x1405003F1`, `0x140500402`).
- **C6 (L1–3 permit branch missing).** If `FUN_1405AAF00(w, team)` is true (`TEST AL,AL` at `0x140500262`), the mask is every slot with class > 1 and the quota is skipped (`0x140500266-0x1405002C5`). The predicate has an RNG draw and an instruction-2 gate. GF stubs it false.
- **C7 (level 0).** Quota 0 is right, and forwards are still admitted by the BTS path, so the level-0 mask is the forward set alone. "Force-permitted" holds only through BTS.
- **C8 (receiver).** The "−3 designated side" uses FUN_1405969B0's id: restart taker, else holder on team, else MA+0x2483C, else T+0x78 designated.
- **C9 (turning).** The turning test is |team+0x250 − receiver heading| > 15.0 (`0x14259BE0C`, `0x1405004F0`). The doc does not define it. GF's velocity-vs-facing test is a different quantity.
- **C10 (floor gate).** The gate at `0x140683350` is `TEST BL,BL` with BL = FUN_140687A80's return, stored at `[RSP+0x20]` (`0x140682C9F`). "level != 0" is INF until FUN_140687A80 is decoded.
- **C11 (7.0 vs 8.0).** 8.0 is the style pull (`0x14259BDF4`). 7.0 (`0x14259BDF0`) is another term at `0x1406828B8`. The community-QA "7.0 pull" stays refuted.
- **C12 ("takes it at once").** Right for the counter target: the fallback exits the loop. "Last live forward" holds only when no fallback slot exists.
- **C13 (quota forward class).** `0x140500663` `CMP EDX,7` reads TeamData+0x4C00 = B, not the registry position. RoleArrays provides that class.
- **C14 (rank scale has two paths).** The 0.2 in the line-2 target (path A) has no count test and needs scalar `[obj+0x128] > 2`. The 0.65/0.2 pair (path B) needs count ≥ 3 and not protected. The doc merges them.

## §5 Implementation plan (RE/TDD slices)

Dependency order: landing L0 (combined tree = ClipWords-integrated-wip + GoalKick + ThrowInPathB, Main's batch) and L0b (TeamSliceA, on RoleArrays, not yet in the combined tree) come from Main. RE tickets run in parallel with L0. Each TDD slice: red test on the landing tree first, smallest change, mutation check per branch, rebuilt. Tests needing the install read it from PES21_ROOT and `GTEST_SKIP` when unset. Constants are read at runtime from the install (`PeImage`). No numeric literal in code. Clean cutover: the stand-in it replaces is deleted in the same change.

| Slice | Goal (PES site) | RE prerequisite | Files | Red tests (numbers; mutation) | Depends on | Busy file? | RoleArrays? |
|---|---|---|---|---|---|---|---|
| L0 | Combined landing (RoleArrays into main) | — | Main | — | Main batch | no | yes (brings B) |
| L0b | TeamSliceA (drift, flags, restart word) | — | `pesteamtick.*`, `teamAIcontroller.cpp` | sketched in `pes-re-team-layer.md` §3 Slice A (`Level3SetsFullAttack`, `Level0SetsFullDefence`, `LevelFrozenWhileDeadBall`); not yet in the tree | L0 (rebase) | **yes** | no |
| Q1 | Quota: constants from the image, ladder, ahead gate, forwards (C4, C5) | none (all CONFIRMED) | `pesoffball.cpp` (`AttackSlotMask`, main 1639-1689 / ClipWords 1631), `pesoffball.hpp:238-240`, `tests/sim/pes_offball_test.cpp` | `QuotaLadderByLevel` (need 2/4/6; mutate need(3)=5 → fail); `ForwardsAdmittedAndCountTowardNeed` (3 forwards at L = 1 → exactly those 3; mutate no BTS → 3rd missing); `AheadGateAdmitsEveryone` (1 ahead, L = 2 → all 11; mutate gate off → 4 non-forwards, fail); `FalloffAndSideAreImageConstants` (10 m vs 25 m score gap = image 5.0; mutate 3.0 → fail); `FalloffLinearBetween10And25` (17.5 m → −2.5 with divisor −15.0; mutate −14 → fail) | L0 (class input; on the landing tree) | no | yes |
| Q5 | Level-zero b8a3 on the class base (RE-13) | RE-13 | `pesoffball.cpp:1724-1734` | `LevelZeroBlocksClassFourOrLess` (class 4 rejected at L = 0, class 7 admitted; mutate block class 7 → fail) | L0, RE-13 | no | yes |
| F1 | Pure forward module: factor, eligibility (table), protected scan, line map (R3, R5–R7) | none (table and factor CONFIRMED) | NEW `src/sim/onthepitch/pesforward.{hpp,cpp}`, NEW `tests/sim/pes_forward_test.cpp`, CMake | `FactorIsLevelQuarter` (L·f1 for L 0..4, f1 read at 0x142592228 = 0.25; mutate 0.2 → fail); `ProtectedIsLastEligibleForward` (eligible {1:P9, 4:P7} → 4; mutate first-wins → 1); `CounterTargetStopsTheScan` (eligible {1:P7 counter, 4:P9} → 1; mutate no-break → 4); `AdvanceAtMostOneStopsAtFirstEligible` (advance 1, {1:P8, 4:P9} → 1; mutate → 4); `ArmsAreSevenEightNine` (read table 0x140683D60 at runtime → {7,8,9}; mutate {6,7,8} → fail); `LineMapFromTable` (read 0x140683D3C → class 6 is line 1; mutate → fail) | none (pure; class array is an input) | no | no |
| F2 | Line-2 target: hi + (lo−hi)·f·scale (R4) | RE-3 (which row is deep/high) | `pesforward.cpp`, tests | `TargetIsDeepPlusFactorSpan` (deep −20, high 0, f 0.25 → −15; mutate f 0.5 → −10 fail); `TwoForwardsAtMinusOneShareLine` (two unprotected, no rank scale → equal targets) | F1, RE-3 | no | no |
| F3 | Rank scales (path A and B), floor, +10 m hook (R8–R10) | RE-2 (rank), RE-4 (floor gate), RE-6 (counter), RE-5 (scalar +0x128) | `pesforward.cpp`, tests | `RankScalesOnlyWithThreeForwards` (path B: count 3, level 1, mode-0 ∧ mode-1 → ×0.2 (mutate 0.65 → fail); level 4 → no scale); `LineTwoScaleNeedsScalarAboveTwo` (path A: scalar 2 → no scale; 3 → ×0.2 (mutate threshold 3 → fail)); `FloorOnProtectedOnly` (protected −30, ref 25 → −25; mutate all forwards → fail); `ReadyCounterAddsTenMetresOnBackLine` (injected true at R10 = 0 → +10.0 = image 0x142588210; mutate +5 → fail) | F1, F2, RE-2, RE-4, RE-5, RE-6 | no | no |
| S1 | Style pull +8 m (R11) | RE-10 (gates), RE-6 (designated CF), RE-1 (CF) | `pesforward.cpp`, tests | `EveryForwardPastGatesGetEightMetres` (classes 7/8/9 → −8.0 toward own goal, image 0x14259BDF4; mutate CF exempt → fail); `NonForwardsNeverPulled` (class 6 → 0) | F1, RE-10, RE-1 | no (pure); wiring busy | P for the CF gate |
| S2 | +5 m window with exemptions (R12, R13) | RE-10 (+0x80D4), RE-1, RE-6 | `pesforward.cpp`, tests | `WindowFiveMetresForNonExempt` (image 5.0 at 0x14259222C; mutate exempt Fox → fail); `FoxSkipsWindow` (`pes::style::Gate(compat, class, word, EngineBit(FoxInTheBox))`); `CfSkipsWindow` (P = 9) | S1, RE-10 | no | no (P) |
| Q2 | Receiver = FUN_1405969B0 (R19) | RE-8 | `teamAIcontroller.cpp:2393-2397` (BuildRunWorld), tests | `ReceiverOrder`: restart taker > holder on team > MA fallback > T+0x78 (mutate swap holder/fallback → fail) | RE-8, MOVESTEP | **yes** | no |
| Q3 | Turning = heading vs team dir (R18, C9) | RE-9 | `pesoffball.cpp:1664-1668`, RunWorld facts (heading field), BuildRunWorld | `TurningUsesHeadingNotVelocity`; `SideAgainstAimPenalisesSameSide` (−3.0 = image 0x14259D654; mutate opposite side → fail) | RE-9, Q2, MOVESTEP (builder) | builder yes | no |
| Q4 | L1–3 permit when FUN_1405AAF00 true (R20, C6) | RE-7 | `pesoffball.cpp` (`AttackSlotMask`), `teamAIcontroller.cpp:2452-2495` (inputs), `pesrandom.hpp` | `PermitAllAboveOneWhenPredicateTrue` (mask = class > 1; mutate quota → fail); `PredicateFalseKeepsQuota` | RE-7, Q1 | builder yes | yes (class) |
| L1 | Mentality drives the defensive shape (R22) | L0b (or mentality stand-in), RE-12 | `teamAIcontroller.cpp:413-416`, `:425`, `:472`, `:658-661`, `:960-962` | `DefensiveLineFollowsMentality` (mentality m ∈ 0..4 → `defensiveAttackLevel == m`; mutate literal 2 → fail at m ≠ 2); `StealRadiusFollowsLevel` (image-read 20 m below level 3, else 35 m; mutate threshold 2 → fail) | L0b or interim mentality, MOVESTEP | **yes** | no |
| L2 | Carrier touch level (R23) | none | `elizacontroller.cpp:2917-2918`, tests | `CarrierTouchCarriesMentality` (touch.attackLevel == AttackLevel(m); mutate 0 → fail) | MOVESTEP | **yes** | no |
| F4 | Wire forward pull into team AI, cutover (R3–R10) | RE-1 (P), RE-3, RE-5 | `teamAIcontroller.cpp:325-335` (delete mixup offsets), `:625-638` (replace), `pesforward` calls | sim test: a level-0 team's forwards sit at the deep anchor; level 4 at high | F1–F3, L0b, RE-1, MOVESTEP | **yes** | P |
| X | Cutover deletions | with F4, L1, L2, Q2 | stand-in sites `teamAIcontroller.cpp:416`, `:2393-2397`, `:325-335`, `elizacontroller.cpp:2918` | — (no shim left) | the slice that replaces each | yes | — |

Order: Q1 and F1 now (no busy files; Q1 on the landing tree). Then RE-1, RE-3, RE-5, RE-6, RE-7, RE-8, RE-9, RE-10 in parallel. Then L0b and MOVESTEP land. Then S1, S2, F2, F3, Q2, Q3, Q4, L1, L2, F4, X in that order.

## §6 Open RE items

| # | Question | Blocks | Cheapest decisive test |
|---|---|---|---|
| RE-1 | Writer and meaning of TeamData+0x3FB8 (array P). Is P ever ≠ B? Does `S = TeamData+0xC48` hold (RoleArrays)? | F4, S1 (CF), line index, D13 | Static: stores to +0x3FB8 in the team-tick callers. Live (read-only): read P and B for both sides on one tick. If equal always, P = B and the RoleArrays port is enough |
| RE-2 | FUN_1406405C0 body (rank predicate, mode 0 and mode 1; the R9B argument) | F3 | Decompile; test with two synthetic states |
| RE-3 | Line-2 band rows: FUN_140687FB0 / FUN_1406855F0 fill `[RBP−0x48+4·line]` (lo) and `[RBP−0x3C+4·line]` (hi). Which is deep and which is high? Is GF's `line + len` the high end? | F2, F4 | Decompile the fill; compare with `teamAIcontroller.cpp:634-638` |
| RE-4 | FUN_140687A80 return (floor gate BL); floor reference `[[ctx]+0x44]` | F3 | Decompile; a value read on a live tick |
| RE-5 | Writers of the scalar bytes of the per-tick object: +0x129 (advance) and +0x128 (path A gate > 2, pick gate == 2) | F1 (≤ 1 branch live), F3 (path A), F4 | Static scan of the object's writers (5FBCB0 / 5F9CD0 / 5FBE10 family) |
| RE-6 | Instruction records (ids 2, 15): writer of the advanced-instruction table; `FUN_140A63950`, `FUN_140A63680`, `FUN_140A64030` bodies | F3, S1, S2, Q4 | Find the in-match writer. Until then GF stays "none" (ponytail: `FUN_140A63950` = false) |
| RE-7 | FUN_1405AAF00 sub-predicates (list in D8). RNG draw count per call | Q4 | Decompile each; count RNG draws per tick on a live team |
| RE-8 | FUN_140A6E320 (restart taker) and FUN_140A627E0 (holder) bodies | Q2 | Decompile |
| RE-9 | FUN_1404AB8E0 (receiver heading pointer) and FUN_14043A0A0 (fold); team+0x250 meaning | Q3 | Decompile; compare with GF's attack-direction stand-in |
| RE-10 | Style-pull gates: skip flag (0x1405E7ACD), var_3C4 (0x1405E7AD1), +0x80D4 writer; whether the per-player dword holds several style bits (D15) | S1, S2 | Decompile the master's prologue to var_3C4; find the +0x80D4 writer; read one roster style dword |
| RE-11 | The "B == 2 and level ∈ {3,4}" term at `0x1406834DE-0x14068354F` (adds to XMM7) — not in the doc | F4 (full final-target loop) | Decompile the `0x140683553` block |
| RE-12 | Attack-level source for AI teams: doc live check (COM byte stayed 2) vs GF UpdateStrategy vs the sub_14051E970 retune claim | L1 | Read `docs/re/pes-re-match-management.md`; live sample with a non-2 strategy |
| RE-13 | Is the b8a3 test (`pos ≤ 4`) on class or registered position? | Q5 | Decompile the caller at `0x140606CD0` |
| RE-14 | Score source of the quota list (`FUN_140A714E0` record float, +4) and the sort comparator (team-layer2 says ascending and stable; not re-read here) | Q1 exactness | Decompile the `0x140A714E0` record builder |
| RE-15 | FUN_140A4F230 vs GF `AttackersAhead`; pesPenaltyAreaLineX value (`[ctx+0x2AA8]+0x18`) | Q1 | Compare GF function bodies with the decompile |

## §7 Coordination

- Busy files (no edits until the MOVESTEP landing): `teamAIcontroller.cpp/.hpp`, `elizacontroller.cpp`, `match.cpp`. Slices L1, L2, Q2, Q3 (builder), F4 and X touch them.
- Free now: `pesoffball.cpp/.hpp`, new `pesforward.{hpp,cpp}`, `tests/sim/pes_offball_test.cpp`, new `tests/sim/pes_forward_test.cpp`. Slices Q1, F1, F2, F3, S1, S2 touch only these. Q1 needs the landing tree (B is absent on main).
- Live reads: read-only only. Running PES: no writes, no pins of MI+0x1168/0x116C/0x1574.
- Behaviour risk: L1 changes the defensive line for every level other than 2. The live sample had both teams at 2 (doc §6), so L1 is unvalidated live for other levels. Gate it with a frame before a smoke claim.
- Archive: Main archives this report into docs/re/ when delivered.
