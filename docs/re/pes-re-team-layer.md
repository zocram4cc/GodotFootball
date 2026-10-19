# PES A4 team layer: per-team tick in PES order (port spec)

Read-only spec. No GF code changed, no PES written. Scratch: `<workdrive>/gf-TeamLayer-scratch/` (bridge dis/decompiles of the item-2 sites; E8 scan of `pes_full.bin` for `FUN_140A40CA0` callers).
Base `0x140000000`. Tags: **CONFIRMED** = decompile/asm/install bytes this pass; **DOC** = taken from a cited doc, not re-checked; **INF** = inference; **OPEN** = blocker named.

The team tick is `FUN_1404F6C30(brain, S, T_s)` (side tick; sole caller `0x14041EDD7` in H `0x14041ECF0`; teamtick §2.1). H runs it for side 0 then side 1; the 22-player burst interleaves it (teamorder §2.3). Step VAs below are the bridge return-address markers (`uStack_3d0`) from this pass's decompile of `FUN_1404F6C30`, i.e. exact call sites in PES order.

## §0 Float table (script-read from `pes_full.bin`; values `repr`, bits exact)

| `VA` | `value` (`0xBITS`) | use |
|---|---|---|
| `0x14259BDF4` | `8.0` (`0x41000000`) | `FUN_1405E6E80` support-width add (V3); chaser keep 8 m (defshape §5.2) |
| `0x14259BDF0` | `7.0` (`0x40E00000`) | NOT at the pull site; press-life press-man ramp rate |
| `0x14259222C` | `5.0` (`0x40A00000`) | `FUN_1405E6E80` extra pull / half-widths / push-up 5 m; line +5 |
| `0x14259BE84` | `-10.0` (`0xC1200000`) | pick-loop dead bonus (V5); `FUN_1405E6E80` pair bonus |
| `0x142591140` | `30.0` (`0x41F00000`) | pick-loop dead bonus (V5); Goal-Poacher threshold (StylePort2c) |
| `0x1425C6614` | `-100000.0` (`0xC7C35000`) | pick-loop floor: every eligible slot passes (V5) |
| `0x1425656C0` | `4.0` (`0x40800000`) | level-factor clamp high (V1: `min(max(L,0),4)`) |
| `0x142592228` | `0.25` (`0x3E800000`) | level-factor scale (V1: `L·0.25`) |
| `0x142565600` | `0.5` (`0x3F000000`) | support-mid `(f46+f40)·0.5`; phase-3 floor |
| `0x14259BE20` | `25.0` (`0x41C80000`) | press-man ramp span; `FUN_140682770` `XMM13` divisor |
| `0x142588210` | `10.0` (`0x41200000`) | `FUN_140682770` ramp floor / B8A2 +10 m; phase-3 speed gate |
| `0x14259BE34` | `50.0` (`0x42480000`) | `FUN_140682770` support-score scale (·50/100) |
| `0x14257A194` | `0.699999988079071` (`0x3F333333`) | `FUN_140682770` `XMM11` weight |
| `0x14253EE30` | `100.0` (`0x42C80000`) | `FUN_140682770` normaliser (·100/x/100) |
| `0x14259BE0C` | `15.0` (`0x41700000`) | lineup-picker +15.0 (role {5,8}, array A); press-man scale |
| `0x14259BE18` | `18.0` (`0x41900000`) | picker −12 gate `\|z\|<18`; press-man radius base |
| `0x14259D664` | `-12.0` (`0xC1400000`) | picker −12.0 |
| `0x14259D5D0` | `0.6499999761581421` (`0x3F266666`) | `FUN_140682770` non-protected scale ×0.65 (`0x1406832D9`) |
| `0x1425656D0` | `-0.0` (`0x80000000`) | `FUN_140682770` `XMM12` reload after pick loop (`0x1406830BC`) |
| `0x142598578` | `0.20000000298023224` (`0x3E4CCCCD`) | `FUN_140682770` `XMM15` 0.2 scale |
| `0x14256AE1C` | `1.0` (`0x3F800000`) | unit / B8A2-arm one |
| `0x14259BE04` | `12.0` (`0x41400000`) | `FUN_140682770` ramp span (SUB/DIV pair) |
| `0x1425C48C0` | `22.0` (`0x41B00000`) | phase-3 speed window high; ramp ceiling |
| `0x14259BE38` | `55.0` (`0x425C0000`) | widening 55 (F6) |
| `0x14259BE48` | `65.0` (`0x42820000`) | widening 65 (F6) |
| `0x14259BE58` | `85.0` (`0x42AA0000`) | retreat-test 85° |
| `0x14259BE5C` | `90.0` (`0x42B40000`) | press angle 90° |
| `0x14259BE60` | `135.0` (`0x43070000`) | retreat-test 135° |
| `0x14259BE6C` | `160.0` (`0x43200000`) | press cone 160° |
| `0x14259BDEC` | `6.0` (`0x40C00000`) | Target-Man −6·s (StylePort2d); cover drop 6 m |
| `0x14259BDE4` | `2.5` (`0x40200000`) | press-lane bonus 2.5 |
| `0x14259BDDC` | `1.5` (`0x3FC00000`) | press-lane +1.5 replace margin; high-ball 1.5 m |
| `0x1425A9464` | `36.0` (`0x42100000`) | widening 36 m gate (F6) |
| `0x1425C65E4` | `0.5555555820465088` (`0x3F0E38E4`) | retreat-test constant |
| `0x14259BE70` | `225.0` (`0x43610000`) | `FUN_1405E6E80` d² gate 225 = 15² (near `0x1405E7A5D`) |
| `0x1425CC7B8` | `3.799999952316284` (`0x40733333`) | `FUN_140682770` `XMM11` load |
| `0x14259A760` | `35.0` (`0x420C0000`) | `FUN_140682770` `COMISS` ceiling |
| `0x142565604` | `2.0` (`0x40000000`) | switch-case half-width base |
| `0x14259D624` | `54.0` (`0x42580000`) | game speed default (tick 54 Hz) |
| `0x1425C48CC` | `99.0` (`0x42C60000`) | chip ramp high (F4) |
| `0x14259BE2C` | `40.0` (`0x42200000`) | chip ramp low (F4) |
| `0x14257A190` | `0.6000000238418579` (`0x3F19999A`) | chip weight 0.6 (F4) |

`VA | bits | value` per §0: each row gives VA, f32 bits, value. Bits pasted from the script output above; three hand-typed rows from the prior revision (`0x14259BE84`, `0x14259BE34`, `0x14259BE48`) are corrected here.

## §1 Step table: `FUN_1404F6C30` in PES order

Call-site VA = bridge `uStack_3d0` marker. "Ported?" names the GF function or `no`. "Decoded?" names doc:section or OPEN.

| # | call-site VA | callee / action | writes (field, meaning) | inputs | ported? | decoded? |
|---|---|---|---|---|---|---|
| 0 | `0x1404F6CAA` | tick entry: `brain+0x6d48=S`, `brain+0x6d50=T` | brain ctx (inside-tick flag; cleared step 48) | S, T | no (GF has no brain struct) | teamtick §2.2 |
| 1 | `0x1404F6CC5` | `FUN_141E5DC40`, `FUN_1423E7FD0` sets skip (`local_398`) | skip flag (stack) | match object | no | OPEN |
| 2 | `0x1404F6CE9` | `FUN_140A0C3E0` | — | — | no | OPEN |
| 3 | `0x1404F6D07` | `FUN_14051C840` (ctx+0xBA8 := T+0xC48 alias init) | ctx set-piece alias | T | no | role-writer §4 |
| 4 | `0x1404F6D0F` | `FUN_140501260`; **`T+0x8c=1`** | T+0x8c per-tick flag | — | no | OPEN (writer named, effect not) |
| 5 | `0x1404F6D25` | `FUN_1404F6720` | — | — | no | OPEN |
| 6 | `0x1404F6D2D` | `FUN_1404F7AD0` | — | — | no | OPEN |
| 7 | `0x1404F6D4D` | `FUN_140A42580(T, MI+0x1198)` (frame stamp) | T stamp | frame counter | `Team::UpdatePesPossession` stamps `pesLatchFrame` from `GetPesFrame` (team.cpp:443; stand-in, no MI frame) | teamorder §2.3 (stamp `block+0x21c`) |
| 8 | `0x1404F6D55` | `FUN_1405008E0` | — | — | no | OPEN |
| 9 | `0x1404F6D5D` | `FUN_140502190` | — | — | no | OPEN |
| 10 | `0x1404F6D65` | `FUN_1404FE9C0` = possession latch | T+0x255 via `FUN_140A428D0` | ball/contacts | `Team::UpdatePesPossession` → `pes::PossessionLatch` (team.cpp:442; ported latch, GF touch inputs) | possession-latch doc |
| 11 | `0x1404F6DAA` | `FUN_1404FEDC0` | — | latch bits | no | leftovers §1.3 (support-weight swap) |
| 12 | `0x1404F6DBD` | `FUN_14051E910` + **`T+0x134=brain+0xBCC`** | T+0x134 strategy/CPU mentality | brain R[3] | `TeamAIController::UpdateStrategy` (teamAIcontroller.cpp:2073; `pes::StrategyStep`, GF inputs) | tactics §3 |
| 13 | `0x1404F6DD8` | `FUN_1404F8350` = attack-level drift | T+0xB8A4 (0..4), T+0xB8C0 stamp | T+0x134, T+0x210, MI+0x116C | `TeamAIController::UpdateStrategy` mentality half (in-play copy; pad/210 gates not ported) | tactics §3 (§2 verdict) |
| 14 | `0x1404F6DFD` | `FUN_1404F6680` (cond: phase gates) | — | — | no | OPEN |
| 15 | `0x1404F6E05` | `FUN_1404FA840` | — | — | no | OPEN |
| 16 | `0x1404F6E0D` | `FUN_1404FA720` | — | — | no | OPEN |
| 17 | `0x1404F6E15` | `FUN_1404FBCA0` | — | build-up toggle | no | leftovers §1.3 (short-path const swap) |
| 18 | `0x1404F6E1C`–`58` | **`T+0xB8A2=(B8A4>3)`, `T+0xB8A3=(B8A4<1)`** (inline) | full-attack / full-defence flags | T+0xB8A4 | `pes::AttackLevel` readers (pesoffball.cpp:1744 `attackLevel<1` mask; pescarrier.cpp:680 `b8a2`) — flags not stored as such | tactics §3 (§2 verdict) |
| 19 | `0x1404F6E67` | `FUN_140500200` | — | — | no | OPEN |
| 20 | `0x1404F6E6F` | `FUN_1404FF8B0` | — | — | no | OPEN |
| 21 | `0x1404F6E77` | `FUN_1404F7BA0` | — | — | no | OPEN |
| 22 | `0x1404F6E7F` | `FUN_1404FD4B0` | — | — | no | OPEN |
| 23 | `0x1404F6E9D` | `FUN_14053B860` | — | — | no | OPEN |
| 24 | `0x1404F6EA5` | `FUN_1404F87D0` | — | — | no | OPEN |
| 25 | `0x1404F6EAD` | `FUN_140500B90` → T+0x255 read | branch selector | latch | `match->GetLastTouchTeamID` stand-in (ProcessOffBall) | counter-trigger doc |
| 26a | `0x1404F6EED` | restart branch (MI+0x1775==0x17): `FUN_14051D9E0` (shape/role inner) | roles A=B, B-swap, shape `S+0x3A9C..` (via) | sheet T+0x8E08, MI | `CalculateDefensiveShape` (line/length/width part of `FUN_1405EC600` family only) | defshape §6–7; role-writer §1 (§2 verdict) |
| 26b | `0x1404F6F04` | `FUN_14051B3E0` (S+0xC48 shape commit) | shape block | — | no | OPEN |
| 26c | `0x1404F6F13` | `FUN_140A42A70` | — | — | no | OPEN |
| 27a | `0x1404F6F33` | `FUN_1404FBB70` (cond) | — | — | no | OPEN |
| 27b | `0x1404F6F3C` | `FUN_14051E350` = designation publisher (2× `FUN_140A40CA0` → S+0x7CDC/S+0x7D0C) | designation tables (via) | role array B, formation x | no | §2 verdict (this report) |
| 27c | `0x1404F6F47` | `FUN_140581980` (counter) | T+0xA508 (via) | — | `RunConcepts`/`ExecuteConcept` counter part (teamAIcontroller.cpp:1684; partial) | team-plans; counter-trigger |
| 27d | `0x1404F6F4F` | `FUN_14057AC20` (stoppage pair) | T+0xB6DF/B6E0 (via) | — | no | OPEN (names only) |
| 28 | `0x1404F6F91` | `T+0x80=0xFF` (+ cond `T+0x6E6=0`) | T+0x80 restart word | MI+0x116C | no | teamtick §2.2 (value only) |
| 29 | `0x1404F6FBE` | `FUN_140537AC0` | — | — | no | OPEN |
| 30 | `0x1404F6FDA` | (skip==0) 11× `FUN_140A40240` | stack history slots | — | no | OPEN |
| 31 | — | stack snapshot T+0xA2A0/T+0x28C…/T+0x474…; `FUN_140A84E30` | locals | run states | no | leftovers (timer ring) |
| 32 | `0x1404F7643` | `FUN_140A84E30` | — | — | no | OPEN |
| 33 | `0x1404F7678` | `FUN_140579840` = concept selector | T+0xB6E8 running flags (via) | toggles, latch, registry | `TeamAIController::RunConcepts` (teamAIcontroller.cpp:1684; selector ported, 11 dead concepts not gated — see §4) | tactics §2.1 |
| 34 | `0x1404F769D` | `FUN_140579BF0` = concept executor | concept orders (via) | running flags | `TeamAIController::ExecuteConcept` (partial) | OPEN (body not reduced) |
| 35 | `0x1404F76B1` | `FUN_1404F61E0` | — | — | no | OPEN |
| 36 | `0x1404F76BE` | `FUN_14057D240` | — | — | no | OPEN |
| 37 | `0x1404F76D1` | `FUN_1404F9DC0` = shape-dispatch inner (calls `FUN_14051D9E0` on parity) | roles/shape on parity frames (via) | T+0x255, MI+0x1198 | `ProcessOffBall` parity gate (teamAIcontroller.cpp:2212; parity only, body not the shape) | defshape §9 (375–377) |
| 38 | `0x1404F773E` | 11-slot designated-supporter reset via `FUN_140A429A0(T,s,1)` | T+0xA2A0[s]=1 (via) | concept id, latch | `ProcessDesignatedSupporter` (teamAIcontroller.cpp:2232; GF supporter, not the state write) | off-ball §3.1 item 6 |
| 39 | `0x1404F781C` | `FUN_140501160` (cond latch): 2-CF designator | T-side +0xB828/+0xB82C (via) | T+0x87CD, array B, live flags | no | §2 verdict |
| 40 | `0x1404F7824`–`876` | `FUN_14057BBB0`, `FUN_14053B110`, `FUN_1404FB2A0`, `FUN_14057B6B0`, `FUN_14057C190`, `FUN_1404F8C70`, `FUN_140500A00`, `FUN_1404FAD30` | — | — | no | OPEN (8 fn) |
| 41 | `0x1404F78BA` | (skip==0, parity) `FUN_140525530` = run manager | run states (via) | RunWorld inputs | `ProcessRunManager` (teamAIcontroller.cpp:2387; ported manager, PES states as GF runs) | off-ball doc |
| 42 | `0x1404F78D4` | `FUN_140538050` = mode-4 dispatcher | SPACE_RUN 0x1E into T+0xA2A0 (via) | T+0x255, MA order | no | teamtick §2.3 |
| 43 | `0x1404F78FE` | (T+0x218==0) 11× `FUN_140A40820` | run states released | — | `StopConcept` releases concept orders (partial; state reset not ported) | defshape §2 |
| 44 | `0x1404F795A` | `FUN_140535C60` = MA defence dispatcher | PRESS/DELAY/MATCH_UP/… states (via) | q=MI+0x1198&3, latch | `CalculateManMarking` + `GetMarkSpot` + `IsOffsideTrapArmed` (DefensiveShapePort; dispatcher skeleton not ported — see §4) | defshape §1–5; press-life |
| 45 | `0x1404F796D` | `FUN_1404F88D0` (cond `FUN_140CA1A50(0)`) | — | — | no | OPEN |
| 46 | `0x1404F7975` | `FUN_1404FC720` = set-piece/restart events | — | tactics toggles | `PrepareSetPiece` (partial, GF config) | tactics §4.2 (185) |
| 47 | `0x1404F7980` | **`T+0xB6DE=FUN_140509170(side)`** | T+0xB6DE stoppage byte | side | no | OPEN (writer named, meaning not) |
| 48 | `0x1404F7995`–`9AD` | `FUN_1404FC4D0`, `FUN_14057BA50` (OFFSIDETRAP), `FUN_14057D240`, `FUN_140580630` | trap state (via b820) | T+0xE4 | `IsOffsideTrapArmed` (T+0xE4 gate only) | defshape §14.4 |
| 49 | `0x1404F79D8` | `FUN_140539ED0` (cond skip==0) | — | — | no | OPEN |
| 50 | `0x1404F79DF`–`A0E` | `FUN_140421F40`×2 (stubs), `switchD caseD_24`×4 | — | — | no (stubs) | OPEN (no-ops) |
| 51 | `0x1404F7A28` | `FUN_140520930` (takers, subs) | taker/subs (via) | sheet S+0x143C/0x1454 | `FreeKickTaker`/`PickBySuitability` (TSheetPort; free-kick branch only) | taker-map; leftovers (192) |
| 52 | `0x1404F7A3E` | `FUN_14053C6E0` | — | — | no | OPEN |
| 53 | `0x1404F7A5C` | `FUN_140A408C0` | — | — | no | OPEN |
| 54 | `0x1404F7A6B` | `FUN_14051B3E0` (S+0xC48 commit) | shape block | — | no | OPEN |
| 55 | `0x1404F7A85` | `FUN_140A42A70` | — | — | no | OPEN |
| 56 | `0x1404F7A94` | `brain+0x6d50=0` | tick-end flag | — | no | teamtick §2.2 |

Direct body writes (this pass's dec): brain+0x6d48/6d50, T+0x8c, T+0x134, T+0xB8A2/A3, T+0x80, T+0xB6DE. No store to T+0x78, G+0x180, X+0x18, MA+0x22E4C in the body (agrees teamtick §2.2).

H frame (DOC teamtick §2.1, unchanged): S-init → MI check (`MI+0x1168==10` skip) → T_s bind → side-0 tick → side-1 tick → `FUN_1404405D0` side planner → callbacks → S-teardown. Per-player burst interleaves the side ticks (DOC teamorder §2.3).

## §2 Verification verdicts (item 2 of the brief)

### V1. −2 floor and support licences — PARTLY CONFIRMED, narrowed (re-checked raw dis this pass)
- Drift (raw dis `dis_1404F8350.txt`): `0x1404F8613 MOV ECX,[RDX+0x134]` → jump ladder `0x1404F8619–63E` (0→0, 1→1, 3→3, 4→4, else 2); `0x1404F8640 MOV ESI,[RDX+0x210]` + override mesh `0x1404F8649–6C8`; `0x1404F86CC CMP [R15+0x116C],1 / CMOVNZ` (freeze off in-play — note direction: change applies when 116C==1, else keep old); `0x1404F86DE–E3` clamp high 4 (`CMP EBX,4 / CMOVG EAX,R14D`, R14D=4); `0x1404F86E7 MOV [RCX+0xB8A4],EAX`; `0x1404F870C MOV [RDX+0xB8C0],frame`. CONFIRMED raw dis (decompile direction for the 116C gate re-verified: `CMOVNZ EBX,EBP` keeps the OLD level when 116C≠1).
- Copy+flags (raw dis `dis_1404F6C30.txt`): `0x1404F6DC4/6DCA` copy `brain+0xBCC→T+0x134`; `0x1404F6E1C–33` B8A2 (`CMP [B8A4],4 / JL skip / =1`); `0x1404F6E41–58` B8A3 (`CMP [B8A4],0 / JG skip / =1`). So B8A2=(L≥4), B8A3=(L≤0). CONFIRMED raw dis.
- Level-0 effects live in the slot-target builder `FUN_1405E6E80` (dec), not the tick:
  - `if ((1<bVar26)&&(bVar20)) bVar26=bVar26−1; if (bVar10<2) { if (1<bVar26) bVar26=bVar26−1; …}` and `if (((bVar10==0)&&(slot==desig))&&(1<bVar26)) bVar26=bVar26−1` — at L 0/1 the support count decrements (licences shrink to forwards). CONFIRMED as the licence mechanism (exact counts per branch OPEN).
  - `if (2<bVar10) { … +(bVar10−2)·iVar25·5.0 …}` — the level-scaled push (`L·0.25` shape: `FUN_140682770` computes `min(max(B8A4,0),4)·0.25`, asm `0x140682BB7–E4`). CONFIRMED.
- "−2 floor switches off": the only B8A3 reader found is `FUN_1405F9EB0` (`0x1405FA423: CMP [R15+0xB8A3],0; JNZ skip; CALL FUN_1405EF910`). It SKIPS the `ef910` adjust when B8A3≠0 — i.e. at level 0 one line-adjust is omitted, not "one striker stays up". The auditor's floor wording is NOT confirmed as stated. Verdict: floor-switch claim INF at best; the concrete L0 effect is the ef910 skip (CONFIRMED asm).
### V2. B8A3 reader — CONFIRMED raw dis (re-checked; base caveat added)
- Raw (`dis_1405FA423.txt`): `0x1405FA423 CMP byte [R15+0xB8A3],0 / 0x1405FA42B JNZ 0x1405FA446 / CALL 0x1405EF910` — B8A3==0 applies ef910, else skip. CONFIRMED raw dis (same bytes as prior revision; decompile not relied on). Base: `R15=[RCX+0x10]` at `0x1405F9EF2`; RCX at `FUN_1405F9EB0` entry is the shape ctx (caller `0x1405FBCD8` passes `RCX=[RBP−0x80]` ctx, `RSI` shape block; `FUN_1405FBCB0`'s caller `0x1405ECADE` builds the ctx from `[R15+0xBA8]` with T from `FUN_140A48A30` at `0x1405EC671`). Two-hop chain ctx→T not closed to a single MOV this pass, so R15=T is INF(high) (same team fields `+0x8E08/+0xB8A4/+0x254` read off it), not CONFIRMED. Downgraded from prior "CONFIRMED asm".
### V3. +7.0/8.0 pull site — CONFIRMED raw dis (re-checked)
- Raw (`dis_1405E7AD1.txt`): `0x1405E7AD1 CMP [RBP+0xC],0 / JZ skip`; `0x1405E7AE3 MULSS XMM0,[0x14259BDF4]` + `0x1405E7AEE ADDSS XMM8,XMM0` (the 8.0 add); `0x1405E7AFA CALL FUN_140A63680 / JNZ skip`; `0x1405E7B13 CALL FUN_140A64030(EDX=2) / JNZ skip`; `0x1405E7B1C MOVSS XMM9,[0x14259222C]` (5.0) + `0x1405E7B25 CMP EDI,9 / JZ skip-extra` + `0x1405E7B2E CMP [R15+RAX+0x80D4],0 / JNZ skip-extra` + `0x1405E7B39 ADDSS`. Bytes `0x14259BDF4=8.0` verified in §0 script. CONFIRMED raw dis end-to-end (prior revision mixed dec line-refs; the asm alone now carries it).
### V4. Designation writer/sort + 2-CF — CONFIRMED raw dis (re-checked)
- Publisher (raw `dis_14051E350.txt`): `0x14051E373/396 MOV R9B,1` + `0x14051E380/3C2 CALL FUN_140A40CA0` (2 calls, flag 1 both); outputs to `[RAX+0x7CDC]` / `[RAX+0x7D0C]` where `RAX=[RDI+0xBA8]` (ctx slot). Sorter call `0x140A40F13 CALL 0x140A79640` inside `FUN_140A40CA0`. No `+0x8924/28/54` destination in any of the 8 E8 callers (scan re-run logic unchanged). CONFIRMED raw dis.
- 2-CF (raw `dis_140501160.txt`): `0x14050117B CMP [RAX+0x87CD],2 / JNZ fallback`; armed `0x14050122A MOV [RAX+0xB828],1` (+pair `0x1405011F6`); fallback `0x14050123E–24F` (`B828=1,B830=−1,B82C=−1`). Latch gate is tick-side: `0x1404F780D CMP [RSP+0x32],0 / 0x1404F7812 JZ skip / 0x1404F7817 CALL 0x140501160`, and `[RSP+0x32]` is `T+0x255` (`0x1404F6EB8 MOVZX ECX,[R8+0x255] / 0x1404F6EC2 MOV [RSP+0x32],CL`, raw). CONFIRMED raw dis (prior revision cited the dec for the caller gate; now asm).
### V5. Protection rule in `0x140682770` — CORRECTED (prior revision wrong; Main's check confirmed)
- Pick loop (raw dis `0x140682FA2–0A8`, re-read this pass): `XMM12=[0x1425C6614]` (−100000.0) loaded ONCE at `0x140682FA2`; grep of all XMM12 lines in the function shows NO write to XMM12 inside `0x140682FC0–0A8` (next write is `0x1406830BC`, after the loop). `COMISS XMM6,XMM12; CMOVNC R13D,EBX` (`0x140683091/95`) therefore passes for every eligible slot (any real line float ≥ −100000; CMOVNC moves on unordered/equal too). Result: **R13D = the LAST eligible forward in slot order**. The per-slot value (`[R15]` line float), the `+30.0` (`0x14068305C`) and the `−10.0` (`0x140683089`) bonuses are computed into XMM6 but NEVER compared against anything that changes — they are DEAD for the pick (XMM6 is not read after `0x140683091` before its next write at `0x140683282`; R15 after the loop is reloaded as a count at `0x1406831EE`, not the line base). CONFIRMED raw dis.
- The auditor's "last live forward in roster order" is RIGHT for this loop; my prior "maximum of per-slot values / ties keep earlier" is WRONG (and community-qa F8's maximum-reading is wrong the same way). §4 correction row rewritten accordingly.
- Eligibility (what "live forward" means here, raw): slot index eligible iff idx<0x16, on-pitch flag byte (`[base+0x1748]≠0`), line id (`[RDI+RAX]≠0`, i.e. `R15`-walk entry), class jump table at `0x140683D60` arm 6/7/8 only (classes 0–5 fall to `0x140683099` skip), advanced-instruction test `FUN_140A64030(…,0xF,…)==0`, and `[RAX+0x129]>1`. CONFIRMED raw.
- Fallback arm `0x1406830B0: MOV R13D,EBX` (raw): reached when the class-jump falls through (class not 6/7/8 → `JMP RCX` lands at `0x140683099`... precisely: table entries 0–5 → `0x140683099` skip; the `0B0` arm is entered via `0x14068303F JNZ 0x1406830B0` (advanced-instruction test true) or `0x140683050 JBE 0x1406830B0` (`[RAX+0x129]≤1`)). It unconditionally takes the CURRENT slot EBX — so the last slot that hits either fallback condition wins, same last-wins shape. It is NOT a counter-target test at this site (no counter-target read in `0x140682FC0–0B3`). CONFIRMED raw; counter-target linkage OPEN.
- Protection USE (raw `0x14068328A/340`, unchanged from prior revision): `CMP R13D,ESI / JZ skip-arm` — the protected slot skips the pair-rank scaling arm (`×0.65` at `0x1406832D9`) and takes the `max(−ball-line, XMM7)` floor arm at `0x140683340–36A` instead of the level drop. CONFIRMED raw dis (same lines re-read).
- "No RNG": no CALL in `0x140682FC0–0B3` except `FUN_140A64030`/`FUN_140A63950`/`FUN_1406405C0` (all predicate helpers, visible in the dis). CONFIRMED raw dis (static only).
- −2/licence linkage: no B8A2/B8A3 read in `FUN_140682770` (grep zero hits, re-run). Level enters only via `T+0xB8A4` at `0x140682BB7`. OPEN as before.

## §3 Sliced port plan (unported steps, ≤3 pieces each, dependency order)

GF stand-ins carry `// ponytail:` naming the PES site that removes them (brief-rules). Every slice: red test first on `origin/main`, smallest change, mutation per branch. Red-test sketches name behaviour, not implementation.

### Slice A — tick skeleton + body flags (no callee bodies)
- Inputs: side, GF frame (stand-in for MI+0x1198), latch bit (GF `pesPossession`), strategy int (GF `strategy.strategy`).
- Outputs: per-tick T-struct `{b8a4, b8a2, b8a3, t80, b6de-stand-in}` + tick-end clear.
- Replaces: `UpdateStrategy` in-play copy half + scattered `attackLevel=2` constants (`CalculateDefensiveShape:392`, `PrepareSetPiece:1070`).
- Pieces: (A1) T-tick struct + B8A2/A3 inline rule (V1) with test `Level3SetsFullAttack/Level0SetsFullDefence`; (A2) T+0x80=0xFF restart word on non-in-play with test; (A3) B8A4 drift gates (134-map, 210-override, in-play-only change) with test `LevelFrozenWhileDeadBall`.
- Red test: `TeamTickFlags.Level0SetsB8A3AndSkipsEf910-companion` (flag asserts; ef910 companion: line without the adjust).

### Slice B — concept selector/executor completion (step 33–34)
- Inputs: toggles (GF sheet), latch, registry-enable table (stand-in: all enabled, ponytail `FUN_140579840` reg-table read).
- Outputs: running-set + start/stop stamps; dead-11 never start.
- Replaces: `RunConcepts`/`StopConcept`/`ExecuteConcept` gate section.
- Pieces: (B1) dead-11 list (tactics §2: DoublePress etc. never start) with test `DeadConceptNeverStarts`; (B2) one-attacking-concept + interrupt rule with test; (B3) end-test-after + stop-release with test.
- Depends: A (latch/level inputs).

### Slice C — designation publisher (step 27b + 39)
- Inputs: role class per slot (GF `FUN_1408C51F0` map, ported in off-ball), formation-x per slot (GF adapted positions).
- Outputs: bucket-by-class + lateral-order designation table; 2-CF pair armed byte + pair.
- Replaces: `GetDesignatedTeamPossessionPlayer` callers' ad-hoc picks (match.cpp:812; teamAIcontroller supporter pick) — one call site at a time, same change deletes the GF pick.
- Pieces: (C1) bucket-by-class with test `ClassesBucketed`; (C2) lateral sort with test `LateralOrderWithinBucket`; (C3) 2-CF gate (`87CD==2`, latch caller-gate) with test `PairArmedOnlyWhen87CD2`.
- Depends: A (latch). Stand-ins: `+0x4E3`-family x = GF adapted x (ponytail `FUN_140A40CA0` lateral load); `FUN_140A79640` direction = ascending-until-decoded (ponytail).

### Slice D — role rebuild head/tail + B-swap (step 26a/37 path)
- Inputs: sheet registry rows (GF teamSheet), phase flag (stand-in: always head, ponytail `MI+0x1775` gate).
- Outputs: arrays A=B=class per slot; B-swap on tail gate.
- Replaces: GF `roleClass` fill in `BuildRunWorld` (static map; keep map, add rebuild cadence + swap).
- Pieces: (D1) per-tick rebuild A=B with test `RebuildEachTick`; (D2) swap args + guard `FUN_140A41BE0` (in-range) with test; (D3) tail gate (latch + guard) with test.
- Depends: C (same class source). DOC role-writer §1.

### Slice E — run-manager + mode-4 wiring (steps 41–43)
- Inputs: RunWorld (ported), MA-order (ported NearestList), latch.
- Outputs: run states incl. SPACE_RUN 0x1E seeding; release on T+0x218==0.
- Replaces: `ProcessRunManager` internals (keep World, replace decisions).
- Pieces: (E1) parity + latch gates with test; (E2) mode-4 3-candidate seed with test; (E3) release sweep with test.
- Depends: C, D. DOC teamtick §2.3, off-ball doc.

### Slice F — defence dispatcher completion (step 44)
- Inputs: q counter (GF `GetPesFrame & 3`, ponytail `MI+0x1198&q` + stoppage override), latch, states.
- Outputs: q=0 zone/designated/presser/trigger/outlet writes; q=1 cover/goal-cover; every-frame mark/pcc/matchup/press/sand + 0x26 loop.
- Replaces: `CalculateManMarking`/`GetMarkSpot` gaps (keep ported line/mark-spot, add cadence + missing passes).
- Pieces: (F1) q-cadence + CPU-team gate with test; (F2) q=0/q=1 passes with test; (F3) every-frame passes + 0x26 PRESS/DELAY loop with test.
- Depends: A, E. DOC defshape §1, press-life.

### Slice G — stoppage/tail completion (steps 45–55)
- Inputs: trap flag T+0xE4 (stand-in: tactic bool, ponytail `FUN_14057BA50`), taker tables (TSheetPort), sub events.
- Outputs: trap state, taker/subs commit, shape commit, tick-end clear.
- Replaces: `IsOffsideTrapArmed` extension + `PrepareSetPiece` tail + `FreeKickTaker` full-table (extend TSheetPort).
- Pieces: (G1) trap arm+step with test; (G2) taker tables 6+3 with test; (G3) tail commits + end-clear with test.
- Depends: A. DOC defshape §14.4, taker-map.

### Explicitly NOT sliced (OPEN bodies, no contract to port)
Steps 1–2, 5–6, 8–9, 11, 14–17, 19–25, 27a-pre (`FUN_1404FBB70`), 29–32, 35–36, 40 (8 fn), 45, 46-body, 47-meaning, 48bodies, 49, 50-stubs, 52–53, 55. Each needs its own decode ticket; porting any now would invent gates.

## §4 Corrections to existing docs (doc:line, old, new, evidence)
| doc:line | old | new | evidence |
|---|---|---|---|
| community-qa F7/M5 + §7 | "`B8A3` read in `BallPlayer_DefensiveLineDepth` at `0x1405FA423`" (function name) | reader is `FUN_1405F9EB0` at `0x1405FA423` (the line-depth builder); `CMP [R15+0xB8A3],0; JNZ skip; CALL FUN_1405EF910` | dis_1405FA423 (func head `0x1405F9EB0`) |
| community-qa F8/D3 | "extra +7.0 pull from `0x1405E7AD1`" | pull is +8.0 (`MULSS [0x14259BDF4]` at `0x1405E7AE3`); +5.0 follows (`[0x14259222C]`); 7.0 nowhere at site | dis + dec `FUN_1405E6E80` line 502 |
| community-qa F9/R4–R5 | "designation list `+0x8924/+0x8928` (+twin `+0x8954`) rebuilt every brain tick via `FUN_140A40CA0`; 2-CF runs when `+0x255` set; table `+0x8828`" | publisher `FUN_14051E350` (called at `0x1404F6F3C`) runs `FUN_140A40CA0` 2× into `[ctx+0xBA8]+0x7CDC/+0x7D0C`; no `+0x8924/28/54` destination in 8 E8 callers; 2-CF gate is `T+0x87CD==2` (latch gates the tick-side call); armed byte is side+0xB828, pair +0xB82C | dis `FUN_14051E350`, `FUN_140501160`; E8 scan; dec `FUN_1404F6C30:255,539` |
| community-qa F8/D1 (+ prior revision of this report §2 V5) | "protected = maximum of per-slot values, ties to later/earlier" | protected = LAST eligible forward in slot order: `XMM12=−100000` loaded once (`0x140682FA2`), never updated in loop, so `CMOVNC` moves on every eligible slot; per-slot value/`+30`/`−10` dead for the pick; fallback `0x1406830B0` takes current slot on adv-test-fail/`[+0x129]≤1` (last-wins too); "first on counter-target" not at this site | raw dis `0x140682FA2–0B3` (XMM12 line grep) |
| community-qa F7/M6 | "at −2 the floor switches off and support licences go to zero" | concrete L0 effects: `FUN_1405F9EB0` skips `FUN_1405EF910` (B8A3 arm); `FUN_1405E6E80` decrements support counts at L<2/L==0; "floor switch" wording unconfirmed | dec `FUN_1405E6E80` (670/816/824/836), dis `0x1405FA423` |
| positioning-phases §5 | directive-word writer OPEN (role part pinned) | T+0x80A0 writer still OPEN (no write in either tick dec); T+0x80=0xFF restart word is a different field, in-tick at `0x1404F6F91` | grep 80A0 in both tick decs (zero hits) |
| teamtick §2.2 step 34–35 | "`FUN_140501160` (cond.)" ungated detail | called at `0x1404F781C` iff latch (`local_396`) ≠ 0; internal gate `T+0x87CD==2` | dec + dis |
| teamtick §2.2 step 27 | restart-branch callees only | add: `FUN_14051E350` sits on the NON-restart path (`0x1404F6F3C`), `FUN_14051D9E0` on the restart path (`0x1404F6EED`); role rebuild runs in both (51D9E0 head+tail) | dec branch structure |
| press-layer-port (ports) | "`FUN_140639100` (press target)" | name unverified in this pass (not in tick dec; press-life names target writers `T+0x26C/27C/28C` + `FUN_140639100` at action time) | press-life §1; no new evidence — flag as citation-only |
| off-ball-port (ports) | supporter "never runs (b2a.c:657-699)" as settled | b2a.c line refs are scratch-local; keep behaviour, cite `FUN_140525530` stop-test role instead | off-ball §3.1 |

## §5 Open items
1. `FUN_140A79640` sort direction (ascending vs descending) — decode the sorter body. Blocks C-C2 default.
2. Lateral-x base (`+0x46EC` vs `+0x4E3`-family) — resolve the indirection in `FUN_140A40CA0`. Blocks C exactness.
3. `FUN_140A40CA0` R9B flag (0 vs 1: publisher passes 1; twin passes 1; picker passes?) — read the flag branch (`TEST BL,BL / JZ` at `0x140A40D3E`). Blocks C/D.
4. Counter-target linkage to the `0x1406830B0` fallback arm — find the arming write. Blocks V5-full. (V5 pick itself needs nothing: last-eligible-wins ports as a linear scan keeping the last eligible slot; the dead `+30`/`−10`/XMM6 computation is NOT ported.)
5. `T+0x80A0` directive-word writer — still no store anywhere in the tick. Needs a disp scan.
6. `MA+0x22E4c` writer, `T+0x78` writer, `G+0x180` binder, `X+0x18` setter — unchanged OPEN (teamorder §7).
7. Step-40 eight functions + steps 1–2/5–6/8–9/11/14–17/19–25/29–32/35–36/45–49/52–53/55 bodies — decode tickets.
8. Per-branch licence counts at L0/L1 (V1 numbers: "zero" vs "two non-forwards") — enumerate the `bVar26` arms fully.
9. Live confirmation: B8A3-gated ef910 skip, 8.0 pull, designation publish, last-eligible-wins pick — read-only live reads.

## §6 Method
Bridge decompiles: `FUN_1404F6C30` (full, ~700 lines), `FUN_1405E6E80` (pull/licence arms), `FUN_1404F8350` (drift), `FUN_14051D9E0` (role head/tail), `FUN_140A59F90`/`FUN_140A59B20` (bulk/sort callers). Dis: `FUN_1404F6C30` (772 lines; T+0x134/B8A2/A3/T+0x80/latch-byte/2-CF-call sites), `FUN_1404F8350` (276 lines; drift ladder), `0x1405FA423` (B8A3), `0x140682770` (1212 lines, pick+use), `0x140A40CA0` (sort), `0x14051E350` (publisher), `0x140501160` (2-CF), `FUN_1405FBCB0`/`FUN_1405EC600` (R15=T chain attempt). E8 scans of `pes_full.bin`: `FUN_140A40CA0` (8 callers), `FUN_1405F9EB0` (1 caller `0x1405FBCD8`), `FUN_1405FBCB0` (1 caller `0x1405ECADE`). §0 regenerated by script from image bytes (this revision). No GF build/test (spec-only assignment; verification is Main's).
