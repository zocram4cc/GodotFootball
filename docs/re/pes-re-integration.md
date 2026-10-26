# PES match-step map and staged cutover plan (Integration, 26-10-26)

Read-only RE. Image `pes_full.bin` (offset = VA − 0x140000000). No PES bytes quoted.
Labels: **CONFIRMED** = disassembly/capture/docs re-checked this pass; **INF** = inference; **OPEN** = blocker named.
Sources: `docs/re/pes-re-tick-order.md` (MB-H), `docs/re/pes-re-teamtick.md` (A4TeamTick), `docs/re/pes-re-tick-steps*.md` (batches 1–3), `docs/re/pes-re-gaps-a2.md` (GapsA2), `docs/re/pes-re-record-layouts.md` (RL), `docs/re/pes-re-player-record.md` (PR), `docs/re/pes-re-foundation-matchball.md` (MB), `docs/re/pes-re-foundation-teamplayer.md` (TP), `docs/ports/pes-port-Emulate.md`, `docs/ports/pes-port-FrameTrace.md`, `docs/ports/pes-port-RecordLayout.md`, `tools/dev/translate/results*.json`, `tools/dev/ledger/universe.json`+`joins.json`, GF `src/` at this checkout.

## 1. Per-frame call order with VAs and translation status

### 1.1 Frame entry → H (CONFIRMED wiring, OPEN order vs analyze listener)

- Hub `FUN_141b3edd0` dispatches a type-2 record to one object per slot, then `+0x1820` (GapsA2 §a4). Which listener sits in which slot is set by hub Register `vt+0x30`, **not decoded** → relative order of the two listeners below is OPEN.
- MatchSub path (GapsA2 §a2, OBS): MatchObjectListener slot 2 `FUN_140433760` → slot `+0x20` = `FUN_1404334e0` → gate `(this+0x28)->vt+0x8` (`0x140426c10` reads global byte `0x1436f09c2`); 1 → `FUN_140a8eff0` walks `this+0x30` vector, calls `vtbl[2]` per member = H slot 2.
- H slot 2 `0x14041ecf0` (vtable `0x142597580`, H ctor `FUN_14041e4d0`, H created in `FUN_140418940`, stored at match `+0x788`, global `*(0x1436F3F98)` = H; GapsA2/MB-H), in order (A4TeamTick §2.1, raw `0x14041ecf0`–`0x14041ef25`):

| # | VA | Action | Translated? |
|---|---|---|---|
| H.a | `0x14041ecf8`–`ed02` | `S = H+0x488`; `FUN_140a49c50(S)` per-match init of 40-entry flag block, guarded by `S+0x4b0` | `140A49C50`: **no** (not in any results) |
| H.b | `0x14041ed07`–`ed0e` | `MI = *(G+0x270)` (no null test); **if `MI+0x1168 == 0xa`: return** (whole frame skipped) | — |
| H.c | `0x14041ed22`–`ed76` | per-side init: `T_s` = team record (`+0xc148` of side owner), stored `H+0x970+0x28·s` | — |
| H.d | `0x14041edd7` | **team-tick loop** `s = 0, 1`: `brain = H[0x450+8s]`; if non-null `FUN_1404f6c30(brain, S, T_s)` | `1404F6C30`: **impure, translated=False — unmodellable** (closure reaches abort-trap import slot `14252FDE0` via `1404FEDC0 → 140A58550 → 1415A1714`; Emulate §2–3, FrameTrace: only this fn entered, 1/50 reach) |
| H.e | `0x14041ee57`–`ee62` | if `H[0x460]`: `FUN_1404405d0(H[0x460], S, {T_0,T_1}, …)` side planner / ball-phase writer (reads `T+0x78` both sides, writes `T+0x3c…`, calls `FUN_140a571b0/571e0/54fe0/5000`); then `vt+0x88` callbacks on `H+0x940`/`0x988` | `1404405D0`: **impure translated=True, diffed=False** ("harness cannot run", 128 cases @ own-block 0.07 < floor 0.50) |
| H.f | `0x14041ef06`–`ef25` | `FUN_1405069a0(H+0x18)`, tail-jump `FUN_140a484f0(S)` teardown when `S+0x4b0` | both **not in any results** |

### 1.2 Analyze-listener steps (same type-2 record, order vs H OPEN)

`FUN_14042b9f0` (vtable `0x14259a350` slot 2) record switch (MB-H §2; GapsA2: this is NOT the team-tick carrier): step 1 `FUN_140426a90` (rec `0x1050012`); step 2 `FUN_1404267a0` = `FUN_140a49c50` + `FUN_140426950`; step 3 `FUN_1404267f0` if `+0x50` (event stage, detector loop `FUN_140a0d3f0`, K=12); step 4 `FUN_140426860` if `+0x51` (`FUN_140a0d520`); step 5 `FUN_140426760` (per-pass reset `FUN_140a3e140`, `FUN_1404582c0`, via `FUN_140a0d190`); step 6 `FUN_140426810` (`FUN_140a484f0`); steps 7/8 recs `0x105000f/0x105000e`; step 9 rec `0x105002c`; step 10 `FUN_140426cd0` (switch). **None of `14042B9F0/1404267A0/…/140A0D3F0/140A0D520/140A0D190/140A3E140/1404582C0` is in any results** (all unattempted).

### 1.3 Team tick `FUN_1404f6c30` body (CONFIRMED order, A4TeamTick §2.2; step bodies batches 1–3)

Direct stores in body: `brain+0x6d48/6d50`, `T+0x8c`, `T+0x134` (= brain+0xbcc), `T+0xb8a2/a3`, `T+0x80`, `T+0x6e6`, `T+0xb6de`; SIM verdicts from batch 1–3 triage (SIM = a write reaches positions/decisions/possession/stamina/AI state):

| # | Call-site VA | Callee | SIM? | Translated? |
|---|---|---|---|---|
| 1 | `0x1404F6CC5` | `FUN_141e5dc40` + `FUN_1423e7fd0` (skip flag; UI-menu walk, output bit only) | body NON-SIM, bit SIM-relevant | `141E5D4C0`: cutscene **diffed bit-exact** (64 cases, provisional); `1423E7FD0` not in results |
| 2 | `0x1404F6CE9` | `FUN_140a0c3e0` (no stores) | NON-SIM | not in results |
| 5 | `0x1404F6D25` | `FUN_1404f6720` (writes `T+0xe4` trap arm) | **SIM** | `1404F6680` pilot **diffed** (71 cases); `1404F6720` not in results |
| 6 | `0x1404F6D2D` | `FUN_1404f7ad0` (`brain+0x6c5c`, `T+0xb8ac/a8`) | **SIM** | not in results |
| 8 | `0x1404F6D55` | `FUN_1405008e0` (`brain+0x6c40 = 2`) | **SIM** | pilot translated, **undiffed** (harness cannot run, 105 cases @ 0.31) |
| 9 | `0x1404F6D5D` | `FUN_140502190` (`T+0xb860/6a4` banks, `T+0x6c40 = 3`) | **SIM** | not in results |
| 11 | `0x1404F6DAA` | `FUN_1404fedc0` (weights `T+0x224`, `T+0x218`, `brain+0x6cc4/6cf4`) | **SIM** | **impure, translated=False — unmodellable** (global qword store + global call in closure) |
| 14/16/23/24 | — | `FUN_1404f6680/1404fa720/14053b860/1404f87d0` | NON-SIM (visible writes; 5 step-23 callees OPEN) | `1404FA720` impure translated undiffed; `1404F6680` pilot **diffed**; rest not in results |
| 15/17 | — | `FUN_1404fa840/1404fbca0` (`T+0xb8a8/ac`, `brain+0x6d44`, `T+0xb8b4`) | **SIM** | not in results |
| 19/20/21 | — | `FUN_140500200/1404ff8b0/1404f7ba0` (mask `T+0xb8b8`, per-slot `T+0x8c35/8c40/8d50`, best-slot `T+0xb8d0`) | **SIM** | not in results |
| 22 | `0x1404F6E7F` | `FUN_1404fd4b0` (designation clear `+0x38 = 0xff` + commit) | **SIM** (table; notifier NON-SIM) | not in results |
| 25 | `0x1404F6EAD` | `FUN_140500b90` (sheet publish `T+0x8e08` + change record) | **SIM** | not in results |
| 27a-pre…55 | — | `FUN_140537ac0`, `140a40240` ×11, `140a84e30`, `FUN_140579840/579bf0` (concepts), `FUN_140525530` (run manager, gated skip-bit + `MI+0x1198&1 == side`), `FUN_140538050` (mode dispatch), `FUN_1404fc720` (set-piece events), `FUN_140509170` → `T+0xb6de`, `FUN_14057ba50`, `FUN_140520930` (takers/subs), `FUN_14053c6e0`, `FUN_140539ed0`, `FUN_140421f40` ×2 (return-0 stubs), `FUN_140a408c0`, `FUN_140a42a70` (step 55) | mixed; 48-step skeleton CONFIRMED | **diffed**: `140A40820/140A42940/140A429A0/140A422C0/140A42430/140A42580` (64 cases each); impure translated undiffed: `1404FE9C0` (0.21), `14053B110` (0.26), `1404F8C70` (0.11), `14057BA50` (**diffed**, 70 cases); `140501160` translated with struct-map gap; rest not in results |
| 39 | `0x1404F7955` | `FUN_140535c60` → `FUN_140536fc0` = **MA build** (GapsA2 §a1: MA is built INSIDE the team tick, after run manager + 11× `140a40820` loop) | **SIM** | not in results |

No store to `T+0x78`, `G+0x180`, `X+0x18`, `MA+0x22e4c` in the tick body (CONFIRMED).

### 1.4 Per-player chain (CONFIRMED chain, OPEN position vs H)

`match::Player` vtable slot 2 `0x140925750` (vtable `0x1426329a8`, RTTI CONFIRMED) → `FUN_140925470` (driver; unconditional call) → `FUN_140928fc0` (AI decision) → `FUN_1409257b0` → `FUN_140929a00` (gate `MI+0x11b0 == 0x16`; writes `AI+0x8d4`) → `FUN_14092a340` → `FUN_14092c9f0` (reads `T+0xa2a0`, `T+0x28c/278`, `T+0x26c`, `T+0x78`, `T+0x255/254`) → BallPlayer update `FUN_1421019d0` → MainEval `0x142110d30`. Handoff: team→player via `T+0xa2a0/28c/278/26c/78/255/254`; player→team via `pl+0x5d0`, `AI+0x8d4`, `P+0xc6bc = MI+0x1198` stamp. Caller of slot 2 (the 22-player loop) **not found**; per-player loop after H is INF(m) only. **None of the chain is in any results.**

### 1.5 Referee, animation, camera (OPEN placement)

- Referee is event-driven from record handlers (vtable `0x142598558` slots 0/3 → `FUN_140422030/FUN_140423000`; record switch `FUN_14044a180`; whistle `FUN_14050e770` from judge-side fns), NOT in the tick chain (GapsA2 §a3). Position vs team tick depends on hub dispatch (OPEN §1.1).
- Animation/blend and camera have no placed per-tick caller in the cited docs (MB-H §4 OPEN). GF-side counterparts exist (`src/pes/movestep*.cpp`, `src/pes/camera.*`) but no PES call-site mapping.

### 1.6 Translation totals (script-counted this pass)

Unique VAs across `results.json` (252 entries, 252 translated, 183 diffed) + `results_impure.json` (50 entries, 46 translated, 8 diffed) + `results_cutscene.json` (833/833/549) + `results_presentation.json` (639/639/472): **1690 unique VAs, 1686 translated, 1140 diffed**. Unmodellable/unattempted in impure: **4** (`1404F6C30`, `1404FEDC0`, `14080EBA0` logf/powf, `140975710` indirect call). Ticket's "1,246 functions" predates the current files; use the counts above.
Five translated functions run inside GF today (RecordLayout §3): sheet readers `1408C5030/1408C5060/1408C5080/1408C52E0` + `140A71990` (LaneZ).

## 2. Records touched by the step

`G = *(0x1436F3DC8)` (MB notation). G header blob **0x2E58** bytes (PR §1.1, snapshot). Owner: PES match object; created at match setup (`0x140a49c3b` sets `G+0x2e50 = 1`), destroyed by match-object destructor `FUN_140a45780` → `FUN_140a48210` (clears `G+0x270/180/40/2aa8/310`, then `G+0x2e50`; A4TeamTick §4.1). GF holds **no G**; fragments below.

| Record | Size | Owner / lifetime | Ctor | Step reads / writes | GF holder / status |
|---|---|---|---|---|---|
| G header (`+0x0…0x2E58`) | **0x2E58** (CONFIRMED blob) | match object; setup → destructor | `FUN_140EF6BE0` candidate G-init (zeroes `+0x4…0x2588`; OPEN) | tables at `+0x180` (roster), `+0x40` (ball rec), `+0x270` MI, `+0x2c0` env, `+0x2a80` events, `+0x2aa8` FieldInfo, `+0x19C8/1DD8/25F8/1928/1978` tables, `+0x25cf8/25d38` instr table | none 1:1; stand-ins per row below |
| MI = `*(G+0x270)` | open (phase block ≥ `+0x1775`; fields to `+0x181e14` in event store object, different struct) | match; bound at setup, cleared at teardown; **per-tick set/clear site OPEN** (non-null in ~9% of live samples; A4TeamTick §4.1) | init writes `0x1408ae72b` (`+0x11a0 = 10` etc; INF base) | read: `+0x1168` (H.b skip == 10), `+0x116c` (1/2 phase), `+0x1554` (restart kind), `+0x11b4` (byte detector gate), `+0x1198` frame, `+0x11a0/119c` timers, `+0x1775` mode, `+0x1748[22]` on-pitch, `+0x14bc` table, `+0x1558/155c/1564`, `+0x119c` | no MI struct; `IsInPlay()/IsInSetPiece()` + `pesclock.*` + referee buffer (stand-ins) |
| MA (mental image) | open (fields to `+0x26020` grid; `+0x22e4c` lists; `+0x2482c` ball cluster; `+0x2601c` carrier) | match; ctor clears `+0x26014…` (`FUN_140a71c30` via `140a6f240` region; GapsA2/MB E5b–d) | `*(G+0x18)` path dead per LivePoke (MB B1); per-player `X+0x18` zeroed at build `FUN_140a45060`, setter OPEN (teamctx §1) | read: `+0x22e4c + 200·i` nearest lists (consumers `140530320/532f70/6c0170/6cbf70/587100/5b89f0/5e6e80`; **writer OPEN**), `+0x2482c/34` ball point, `+0x2601c` carrier, `+0x26020` grid, rank tables `+0x24710/247d4` (writer `140536fc0` wired, GF LivePoke branch only) | `MentalImage` snapshot at head of tick (wrong place: PES builds inside team tick §1.3); `OppByDist` stands in for `+0x22e4c` |
| H + S | H open; S = `H+0x488` (40-entry flag block, `S+0x4b0` guard) | match; H ctor `FUN_14041E4D0`, registered by `FUN_140418940` (match `+0x788`, global `0x1436F3F98`) | `FUN_14041e4d0` | H.b/c/d/e/f (§1.1) | none |
| Asset registry | object + 234-slot table (slot 183 = tick entry E; Emulate §2) | process-global `0x1437F4808`; E varies by phase | n/a (global) | step 1 `FUN_141e5dc40` reads it first; **uncaptured in all 32 captures** → frame run stops in frame 0 (Emulate §2, FrameTrace) | GF VFS (`src/pes/vfs.*`); no registry root |
| T (team record, per side) | GF `0xC138` (furthest spec read `T+0xc134`; PES alloc size OPEN; RecordLayout §1) | per side; `T_s = *(G+0x1928+0x28·s)` via `FUN_140a48a30` | sheet ctor `FUN_1408c5390` (sliders/captain/takers; RecordLayout) | §1.3 + S sheet `+0x8e08` (41 rows), `+0x134/210/218/224/250/254/255/78/7d8c/e4/3a9c/4c00/6a4/b860/b8a2-a4/b8b0-c0/b8d0/b8f8/8c35/8c40/8d50/a2a0/a508/a548/c134/6d00/6d48/6d50` | `PesTeamRecord`: only S (`+0x8e08`, `pesteamconcepts.hpp`) + `TeamTick` words (`pesteamtick.hpp`) populated; rest reserved zeroes |
| Per-player block | **0x3F40** stride on-pitch (i = 0..21); anime `+0x0` (≤ 0x32C), man `+0x32C` (≤ 0xDC4), slot `+0x10F0` (≤ 0x2E50); indices 22–24 stride 0x1220, no slot (PR §1.2–1.3, snapshot CONFIRMED; exact sizes OPEN-2) | per slot; tables `G+0x19C8` (anime, `A48910`), `G+0x1DD8` (man, `A48A10`), `G+0x25F8` (slot, `A489F0`), 0x28 pointer entries | **OPEN-1** (no alloc-size immediate; writers of entries 1..21 use computed bases) | man `+0x55c/564` pos, `+0x574/578/57c` vel, `+0x5d0/5d4/6b8` action triple (`FUN_140a6ca10`), `+0x661` touching, `+0x5b0`; slot `+0x1430/1438` (`FUN_14092ed30`), `+0x1414/141c` aim, `+0x1a/1c/22` u16 (F5 shape conflict: lo > hi live), `+0x1` class byte lives on **anime** (F1 mis-attribution corrected), `+0x8` flags | no persistent PES-layout block; `MoverRecord` per-call snapshots (no production caller); humanoid pos/vel; `match.hpp:520` touching; `pescarrier.hpp:1015` action (carrier only); slot counters/u16/class: **none** |

Class byte + 94-u32 table `0x1426484f0` + 7 predicates (`6E150/1C0/5E0/580/4B0/340/050`) ports verbatim (RL §3); 0.7 gate `0x14257a194` CONFIRMED, 0.35 `0x14259d5c8` is NOT a F0E50 input (RL §5).

## 3. GF's side (where GF runs each step today)

`Match::Process` (`src/sim/onthepitch/match.cpp:820`): `randomInfoProducer->Tick()` → pesLoop ? ball → event → referee : referee → ball → event (`ProcessBallStage:809` → `ball->Process():817`; `ProcessPesEventStage` `matchevents.cpp:68`; `ProcessRefereeStage:797` → `referee->Process():805`) → `RecomputePesHolder()` + restart-step stand-in (`0x0B/0x16` for `MI+0x11b4`-family cursor, writer OPEN) → `MentalImage::TakeSnapshot()` (`:865`, head of tick — PES builds it mid-team-tick, §1.3) → `TickScoreRows()` both sides (`teamAIcontroller.cpp:166`, S1 equivalent: both sides before either tick) → `teams[0/1]->Process()` (`:881`, `TickStage::Teams`) → `officials->Process()` (`:883`). PES order vs this: steps 2–6 + team tick run inside the type-2 dispatch with referee event-driven and AFTER-relevant (§1.5); GF's referee-before-ball/teams is unproven vs PES (MB-H §4 OPEN) — do not reorder on this map alone.
`Team::Process` (`src/sim/onthepitch/team.cpp:543`): `PesPressTick()` (`:2391`) → `UpdatePesPossession()` → `WriteRoleArrays(true)` (`:3264`, `FUN_14051d9e0` head) → `UpdatePesDesignation()` (`:3358`, `FUN_14051e350`, pes-loop only) → `UpdateStrategy()` (decider; GF stand-in for `FUN_14051e970` modes) → `UpdatePesSheetPlan()` → `TickMarkTable()` (`:191`, step-35 T7a equivalent) → in-play: controller `Process()` + `CalculateManMarking()` + `CalculateDynamicRoles()` (every 40 ticks) → `players[i]->Process()` (`player/player.cpp:461`, per-player chain stand-in) → `WriteRoleArrays(false)` → `PesStep48()` (`:2993`, `FUN_14057f600`) → `PesStep55()` (`:2989`, `FUN_140a42a70`) → PASS_GET receiver (`T+0x78` stand-in `kickMan/kickValid`, `teamAIcontroller.cpp:2509`).
GF state per PES field: `TeamTick` (`pesteamtick.hpp`: `b8a4/b8a2/b8a3/t80/b8c0Frame/b6c40`, reaction banks `6a4/b860`, support `218/224/6cc4/6cf4`, counters `b8a8/ac`, `trapArmed` = `T+0xe4`); `Roster` (`pesreactionblock.hpp:137`, `G+0x180` 24×0x74); ball-record words (`pesevents.hpp:389`, `G+0x40` + ball object `B+0x88/8c/90` per MB); `attackSign` (`pescarrier.hpp:582`, `T+0x254`); `GetTimeNeededToGetToBall_ms()` (need-table stand-in, RL §5); `SlotRecordA/B`, mark rows (`pesmarktable`, `pesscorerows`).

## 4. Staged cutover plan (one task-run each; later stages depend on earlier)

Proof key: T = existing tests (`pes_team_sheet_test`, tick-order tests), R = repro determinism-hash compare (seed 7, 2-min `--pes-loop`), A = new agreement test (same random inputs vs translated fn). Each stage deletes the GF path it replaces (no shims).

- **S0 Harness: registry-rooted capture + multi-frame run.** Capture `0x1437F4808` + table + phase-correct E + `0x140a3e8e0` region (Emulate §2 list 1–5); rerun `frame_trace.py` to ≥2 frames. Unblocks reach/coverage for every stage. Needs PES live. (No GF deletion.)
- **S1 G tables as pointer windows.** Move `G+0x1928/19C8/1DD8/25F8/1978` accessors (`140A48910/48A10/489F0/489D0/48A30`, all pilot-diffed) onto `PesMem::window`; GF table readers use them. Proof: A (accessor agreement on snapshot pointers) + R. Deletes: ad-hoc pointer chasing at those sites. *Needs no impure fn.*
- **S2 Per-player block in PES layout.** GF-owned `0x3F40`-stride blocks (anime/man/slot, §2 bounds) per on-pitch slot; move `+0x55c/564/574/578/57c/5d0/5d4/661/6b8/1414/1430/1438/1a/1c/22` + anime `+0x1` reads onto it (`// ponytail:` names writer VAs; allocator choice stands in for OPEN-1). Proof: A (field values vs live snapshot ranges, PR §2) + R. Deletes: `MoverRecord` snapshots, `OppByDist` later in S4. *Needs no impure fn* (geometry/reader fns already diffed: `1404AB8E0/140A6C6D0/140A6CA10`-family where proven).
- **S3 MI phase-word struct.** New `PesMI` (`+0x1168/116c/1554/11b4/1198/11a0/1775/1748`) fed from GF referee/clock/restart (writers stand-ins, `// ponytail:` per word); gate sites (`IsInPlay`, stoppage arms, H.b skip) read it. Proof: A (phase transitions vs live series, GapsA2 §b) + R. Deletes: scattered `IsInSetPiece`/`referee-buffer` reads at converted gates. *Needs no impure fn.*
- **S4 MA nearest-list rebuild.** Rebuild `MA+0x22e4c + 200·i` once per frame from man positions (layout CONFIRMED; builder escritor `// ponytail:` — writer OPEN) with on-pitch/tie rules marked; consumers (`1406c0170/6cbf70/587100/5b89f0` paths) read it. Proof: A (`d²` recompute match on snapshot pairs, A4TeamTick §3.2) + R. Deletes: `OppByDist`. *Needs no impure fn but ships a known-approx writer.*
- **S5 T words into the record.** Extend `PesTeamRecord` per §2 field list (batches 1–3 slices T1–T4: trap `e4`, counters `b8a8/ac`, `6c40`, banks `6a4/b860`, weights `218/224`, mask `b8b8`, slot tables `8c35/8c40/8d50`, `b8d0`, `254/255/78/a508`, `a2a0/28c/26c`, sheet-adjacent `6d00/6d48/6d50`); move `TeamTick` into the record. Proof: per-slice red tests (batches list them) + R. Deletes: `TeamTick` struct, `supportTimer`/mask stand-ins. **FLAG: slices touching `1404FEDC0`-fed weights need the unmodellable fn — split those out.**
- **S6 Switch proven tick callees.** Call translated `1404F6680/140A42580/140A40820/140A42940/140A429A0/140A422C0/140A42430` (+ `14057BA50`, `1404FA720` where harness allows) from `Team::Process` PES path at their §1.3 positions. Proof: A per fn (existing pilot banks) + R. Deletes: corresponding GF stand-ins (`StoppedSelect` etc where covered). **FLAG: `1404F61E0/1405008E0/140501160/1404FE9C0/14053B110/1404F8C70/1404405D0` are translated-but-undiffed (harness-cannot-run / map-gap) — switch only after diffed.**
- **S7 H frame skeleton.** Build GF H/S/brain records; run S-init → MI check → T bind → `1404F6C30` ×2 → `1404405D0` → teardown from `Match::Process`, replacing the Teams stage. Proof: frame-trace agreement (S0 runs) + R. **FLAG: BLOCKED on impure `1404F6C30` + `1404FEDC0` (both unmodellable) and unattempted `14041ECF0/140A49C50/1405069A0/140A484F0` + brain/`T+0x78`/MI-binder OPENs (§1.1–1.2, A4TeamTick §4).**
- **S8 Per-player driver.** Port the §1.4 chain after its loop caller is found (OPEN §1.4); wire team→player handoff fields from S5. **FLAG: entire chain unattempted; dispatcher unknown.**
- **S9 Referee/restart sequencer + animation/camera.** Place per hub-dispatch decode (Register `vt+0x30` OPEN) and MA-build-then-animate order; wire whistle → sequencer → restart. **FLAG: referee/animation/camera callers undecoded; GF referee/anim/camera stay until then.**

## 5. Open items (ranked)

1. Hub Register `vt+0x30` (listener order; blocks S9 and the GF `Process` reorder justification). 2. `match::Player` slot-2 loop caller (blocks S8 and team-vs-player order). 3. Per-tick MI/roster binder set/clear (blocks S3 liveness + S7). 4. `MA+0x22e4c` writer (S4 ships approx until found). 5. `T+0x78` writer (PASS_GET/kick paths). 6. Block allocator (S2 stands in). 7. Unmodellable `1404F6C30/1404FEDC0` + abort slot `14252FDE0` reachability (blocks S7; Emulate §6 item 3). 8. PES alloc sizes (T, H, MI, MA) and `+0x1a/1c` pair shape (PR F5). 9. Animation/camera per-tick callers (S9).
