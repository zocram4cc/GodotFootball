# PES integration map: subsystems, cost drivers, tooling and a parallel plan (IntegrationMap, 18-11-26)

Read-only survey. Checked: `origin/main` (e1f4a0cc), the remote branch heads, `docs/ports`, `docs/re`, `tasks/17-11-26.md`, `brief-rules.md`, `tools/dev/translate`, and `src/` on the main checkout. Only `git fetch origin` (refs) was run. No code, branch, capture or worktree was changed.
Labels: **CONFIRMED** = read this session (doc, tool output, git). **INF** = inference, reason given. **OPEN** = blocker named.
"Run" = one agent round, numbered as in the port docs. Branch counts are per the commands named.

## 0. Summary

- **Nothing of the gameplay chain is on `origin/main`.** Of 28 branches checked, 5 are landed (CutsceneTiming, FoulSanction, StoppageFlow, MotionClips, AIConcepts), 15 are not on main, and 8 have no remote branch. The six chain and gameplay branches measured sit 161 to 228 commits behind main and carry 54 to 291 commits of their own. CONFIRMED (`git merge-base --is-ancestor`, `git rev-list --left-right --count`).
- **Worked example (OneChainCut):** 12 rounds (one unheaded plus Rounds 2 to 12), round 13 running at the handover. About 5 rounds went to memory mapping and state (one unmapped memory window, found at Round 11). About 4 went to oracle generation and capture batching. About 2 went to per-byte `memset` speed. About 2 went to finding a baseline on another branch. CONFIRMED (round-by-round read in section 2a).
- **Parallel now:** 3 agent lines (owner cap, 17-11) plus the Main lane that lands `OneWorld-series-wip`. The 4th brief is queued. After OneWorld lands, the team/AI and ball lines run, then movestep/kick, referee/restart, keeper.
- **Top tooling proposal (T1):** an engine-memory fixture whose mapped windows come from each capture's replay read set, with a build-time self-check that names any unmapped address. It would have turned the Round 8 to 11 hunt into a one-step failure. INF on the outcome, CONFIRMED on the bug class.

## 1. Subsystem table

Ledger numbers are from `docs/re/LEDGER.md` (in-scope rows). Layer numbers are file counts and line counts under `src/pes/translated/<dir>/`. Results are from `tools/dev/translate/results_*.json`. "Stand-in" = ledger rows in status stand-in (GF code cites the VA). "ponytail" = `ponytail:` lines in src.

### 1a. Roots, stand-ins, translated and proven coverage

| # | Subsystem (ledger tag) | PES roots (VA, doc) | GF stand-ins in place | Translated / proven |
|---|---|---|---|---|
| W | Trap / anime chain (trapchain, trapdribble, trapact4, trapkind; animation rows) | Trap kind-0 `0x14079A470`; trap tick `0x140797CD0`; driver `0x141EB5E70`; runner `0x141AFC4A0` (OneChainCut R5); anime ctor `0x141EB5250`; gate 4a `0x1406B4C10` (TrapAdmit); current action `0x14079BBE0`; guard `0x141AE8950` (R3) | On OneWorld-series-wip the chain stand-in is deleted (BuildAnimeChain 0 hits). On main 0 hits, on cut4 5 hits (CONFIRMED) | trapchain 105 rows (ported 1, decoded 8, leaf 46, undecoded 50); trapdribble 178 (ported 2, decoded 20, undecoded 111); trapact4 20 (ported 2); trapkind 13 (undecoded 12). Layer: trapchain 2 files, trapaction 3, others 1 each. Results: trapaction 17/17 diffed; trap-kind-0 snapdiff 72/72 bit-exact (R7, R11); TrapAdmit 13/13 |
| 1 | Ball and contact (ball) | Ball integrator `0x140876360`; acceleration `0x140874470`; ball init `0x140874860`; ground/air driver `0x140873EE0`; net `0x140425E20`; BallControl slot 4 `0x140424F70`; BallPlayer tick `0x1421019D0` | 14 stand-in rows (e.g. `0x140870A40` ball-collision netbody, `pes-re-ball-collision-netbody.md`). ponytail: ball.cpp 2, matchevents.cpp 11 | ledger 368 rows: ported 38, diffed 1, translated 2, stand-in 14, decoded 75, leaf 58, engine 4, undecoded 176. Layer 180 files / 19.2k lines |
| 2 | Team tick and AI concepts (AI) | Team tick `0x1404F6C30`; H slot 2 `0x14041ECF0`; plan selector `0x140500B90`; CPU strategy `0x14051E970`; attack level `0x1404F8350`; concept selector `0x140579840`; concept start `0x1406424A0`; defensive style gate `0x140A62180`; pass/shot block `0x140A08EA0` | 48 AI stand-in rows. Two biggest by callers: `0x140A48A30` (1286 callers, size 9) and `0x140A48A10` (1281 callers, size 9). Their translated bodies already exist (`src/pes/translated/ai/pes_fn_140A48A30.cpp`, `pes_fn_140A48A10.cpp`, CONFIRMED) but GF still cites stand-ins: ponytail notes in movestep_run.cpp:1150, movestep_step.cpp:137, movestep_turn.cpp:154, matchevents.cpp:206 say the record is "not in GF". ponytail: elizacontroller.cpp 95, teamAIcontroller.cpp 73, pesoffball.cpp 34, pescarrier.cpp 29, pespress.hpp 14, pesmentality.cpp 11 | ledger 1362 rows: ported 90, diffed 128, translated 56, stand-in 48, decoded 383, leaf 112, engine 12, undecoded 533. Layer ai/ 290 files / 35.8k lines. Results: aiconcepts 9/9 diffed; core 2598 entries (2281 diffed, 684 with failed>0) |
| 3 | Player AI, movestep, animation (animation; AI rows) | Player driver `0x140925470`; per-player owner tick `0x1408BFCF0`; PvP contacts `0x1408C48C0`; AnimePlayer Exec `0x1406E5FC0`; action tick `0x1407824A0`; class-0x53 gate `0x140789DD0`; velocity integrator `0x1407D1E60`. Player slot-2 wrapper `0x140925750` is virtual-only (no direct caller; `pes-re-teamtick.md`) | animation 8 stand-in rows; AI stand-in rows cited to movestep and anim-select docs (`0x1406ABB30`, `0x1406AF8E0`, `0x1406B1D00`, `0x1406B1F00`, `0x1406DE990`, `0x140A6E290`, `0x140A6E2E0`, `0x140A73E20`). ponytail: pesmovestep.cpp 43, movestep_run.cpp 38, movestep_step.cpp 15, humanoid.cpp 14, movestep.hpp 9, movestep_turn.cpp 8 | animation 837 rows: ported 75, diffed 1, translated 1, stand-in 8, decoded 44, leaf 58, engine 49, undecoded 601. Layer animation 188 files / 16.0k lines. Results: motion 8/8 diffed; stoppage 10 (3 diffed, 4 failed) |
| 4 | Keeper (no ledger row; rows sit in AI and restart) | Decoded keeper functions per `pes-re-goalkeeper.md`: `0x1404AC320`, `0x1404AD860`, `0x1404AE050`, `0x140506BA0`, `0x140581D40`. Shoot-out G formula: `pes-re-keeper-dive.md` | Named gaps (owner 18-11): save gate `Player::KeeperAttemptsSave`; keeper mover plays back at 1.0 (KeeperDive). ponytail: peskeeper.hpp 2, peskickerror.cpp 6 | No keeper-tagged results file. Not counted in ledger subsystem rows (INF: counted under AI and restart) |
| 5 | Referee, judge, restarts, set pieces (referee; restart) | Judge tick `0x1404A56C0`; whistle `0x14050E770`; referee chief `0x140A38A00`; PK placement `0x140A38A70`; referee dispatcher `0x140A38C40`; situation sequencer `0x14044C880`; sequencer caller `0x140445E90`; restart-step container `0x140422030`; event record slot 3 `0x140423000`; throw-in phases `0x140791EC0`; restart release `0x140792E10` | referee 2 stand-in rows (`0x14050B9B0` referee.cpp; `0x140447B50` match/referee); restart 13 (`0x14050F7E0` pesjudge.cpp; `0x1404493D0`, `0x1404499D0` pesclock.cpp; `0x14050E770` whistle in teamAIcontroller, pesstoppage, pessequencer, offsiderule; `0x1404A8930` pessetpieceteam.hpp). ponytail: referee.cpp 11, matchevents.cpp 11, pesclock.cpp 7, pessetpieceteam.hpp 6, offsiderule.cpp 2, pessequencer.cpp 2 | referee 101 rows: ported 12, diffed 2, translated 1, stand-in 2, decoded 34, leaf 29, undecoded 21. restart 678 rows: ported 38, diffed 7, translated 1, stand-in 13, decoded 122, leaf 148, undecoded 345. Layer referee 55 files / 6.1k lines, restart 301 / 25.3k. Results: judge 3 (0 diffed), danger 3 (0 diffed), endclear 5 (0 diffed) |
| 6 | Cutscene and FixDemo (cutscene; presentation) | Parse `0x141F40C00`; decision-tree matcher `0x141F4B1D0`; condition evaluator `0x141F4D2F0`; slot iteration `0x141F57840`; FixDemoMatch vtable `0x141F5E930`; slot-list build `0x141F5F330`; foul/card `0x140480910`; substitution `0x140484A10`; goal variant `0x140471A80`; goal celebration `0x140478190`; period end `0x140488470`; end of match `0x14048AB10`, scene id `0x1404A3FA0`. Hooks in use: FixDemo wrapper `0x141F4FDA0`, ctx init `0x141F514D0`, scene start `0x141F5CD00`, step end `0x141F572F0`, step machine `0x141F59380`, cut offset `0x141F39CC0`, proc camera `0x141F7BFE0` | 0 ledger stand-in rows. ponytail: fixdemo_select.cpp 9 (+3 in .hpp), fixdemo_camera_rig.cpp 4, gdext pes_cutscene_camera.cpp 3 (+3 in .hpp) | cutscene 3846 rows: ported 2, diffed 4, decoded 71, leaf 256, engine 456, undecoded 3057. Layer cutscene 614 files / 51.6k lines. Results: cutscene 833 (685 diffed, 207 with failed>0); fixdemo 6 (4 diffed, 1 failed); proccam 2/2; presentation 639 (546 diffed, 141 with failed>0) |
| 7 | Broadcast and in-play camera (camera) | InplayCamera ctor `0x14087E480`; record builder `0x1408A1A70`; stadium table builder `0x1408A57F0`; mode-switch blend `0x14087F4A0`; target smoothing `0x1408A2180` | 2 stand-in rows. No camera ponytail in src | camera 568 rows: ported 9, diffed 3, translated 1, stand-in 2, decoded 13, leaf 71, engine 26, undecoded 443. Layer camera 138 / 7.8k. No camera results file |
| 8 | Replay (replay) | ReplayHuman frame read `0x14217DEF0` (`pes-re-cutscenes.md` §6) | 0 stand-in rows. No GF replay module found (keyword grep hits only movestep, fixdemo, cutscene_timeline) | replay 740 rows: ported 0, diffed 0, translated 0, decoded 8, leaf 88, engine 91, undecoded 553. Layer replay 161 / 9.5k, but ledger credits 0 (credit gap, section 2f) |
| 9 | HUD, scoreboard, clock (HUD) | Scorebug `0x1420F0EB0`; radar `0x1420E2910`; tactics banner `0x1420FBF20`; replay control bar `0x1420EB0C0`; clock block `0x1420EFEC0`; clock digits `0x1420F1750`; HMS conversion `0x140A6BD80` | 1 stand-in row. ponytail: none tagged HUD (pesclock.cpp carries the clock ponytails) | HUD 164 rows: ported 1, stand-in 1, decoded 1, leaf 19, engine 3, undecoded 139. Layer hud 52 / 3.6k |
| 10 | Crowd (crowd) | Supporter config loader `0x14143F2B0`; chant config loader `0x1414406A0` | 0 stand-in rows. GF has its own crowd model (crowdmood, goalcelebration, goalsequence), not cited to PES | crowd 147 rows: leaf 17, engine 1, undecoded 129. Layer crowd 41 / 3.0k |
| 11 | Audio cues, non-commentary (audio) | Ball-frame speed to tier picker `0x14048FDAE` (SE_PL_BALLHIT); Se loader `0x14143EB60`; Announce loader `0x14143F7F0`; Sequence loader `0x1414401C0` | 0 stand-in rows. No GF audio module found (keyword grep hits incidental only) | audio 556 rows: ported 1, leaf 52, engine 6, undecoded 497. Layer audio 109 / 6.2k |
| 12 | In-match menus (in-match menu) | Team management `0x14044AF70`; substitution window `0x14044B1D0`; sub-windows `0x14044B380`; sub/UI window `0x14044BB40`; armed set-play counter `0x14044D2E0`; game-plan toggles `0x1408C5030`; sliders `0x1408C52E0`; sheet plan writer `0x1408C5E20` | 3 stand-in rows. Largest by callers: `0x140CA1A50` (1823 callers, size 2), cited to `pes-re-attack-decisions.md` but tagged in-match menu in the gap list (OPEN: tag check) | in-match menu 3777 rows: ported 9, diffed 4, stand-in 3, decoded 26, leaf 398, engine 60, undecoded 3276. Layer in_match_menu 30 / 3.3k |
| 13 | Pre-match flow (pre-match) | Setup builder `0x141FC1590`; start-plan reader `0x1408E16D0`; kit config loader `0x141EA3190`; missing-kit fallback `0x141EA29C0`; per-kit loaders `0x141EA1320`, `0x141EA54A0`; kit assembly `0x141A89120`; match general settings `0x140EA93C0` | 0 stand-in rows (GF pre-match code is GF-authored, not cited to PES) | pre-match 1453 rows: ported 13, diffed 1, decoded 11, leaf 174, engine 135, undecoded 1119. Layer pre_match 309 / 20.9k, ledger credits 0 (section 2f) |
| 14 | Gameplay residual: match loop, listeners, possession, dribble (gameplay) | Match tick `0x14042B9F0`; listener registrar `0x140418940`; listener builder `0x14042FE20`; MatchSub listener `0x140433760`; `0x1404334E0`; to team-tick owner `0x140A8EFF0` | gameplay 9 stand-in rows. ponytail: humanoid.cpp 14, match.cpp 18, team.cpp 13, gf_match.cpp 12, match.hpp 3, playercontroller.cpp 3 | gameplay 3517 rows: ported 122, diffed 3, translated 1, stand-in 9, decoded 407, leaf 343, engine 106, undecoded 2526. Layer gameplay 1226 / 161.0k (largest dir). Results: core 2598 (2281 diffed) |

Totals across the tree: 651 `ponytail:` lines (62 files in `src/sim/onthepitch`, 23 in `src/pes`, 7 in `src/gdext`, 4 in `src/pes/translated`), 102 ledger stand-in rows, 10,056 functions in the generated layer (3,817 bit-exact recorded, 6,239 closure: OneChainCut R2). CONFIRMED.

### 1b. State, captures and GF files per subsystem

Capture sets are from `tasks/17-11-26.md`, `brief-rules.md` (kept and protected lists), and the port docs. "Kept" = on disk and named in a doc or rule. "New hook" = not armed today. Hook names give the VA low digits (e.g. `kick_chooser_8364D0` = `0x1408364D0`, the VA in `pes-port-KickKind` docs).

| # | State records needed (PES layout) | Capture sets: kept / new hooks | GF files it would touch |
|---|---|---|---|
| W | P (player): P+0x18 index, P+0x2460 flags, P+0x3B18 height, P+0xA08 chain, P+0xAB8 anime interface, P+0xAC8/AC9 action byte, P+0xC40 action table, P+0x354 motion table. Chain: A = P+0xA08, B = A+0xF0, O2 = A+0x188, O1 = A+0x118 (heap, OS arena). Globals G, MI = G+0x270. Ball record (ball+0xB0 samples, ball+0x279C; 250-sample prediction table). TEB/TLS and stack for trap calls | Kept: trap kind-0 pid2286718 (24), pid2523621 (8), pid2571173 (24), pid3727195 (16); traptick pid2523621 (14); anime_tick pid3778393 (19); height pid848145 seq8934 to 8939 (6); chainX pid848145 (12); strike pid1115004 seq456 to 606 (38). New: none; O1 tick fixture open | src/sim/onthepitch/pestrapactor.*, pestrapworld.hpp, pesanimechain.* (on OneWorld-series-wip only), src/pes/translated/trap*, tests/pes_engine_trap.hpp and trap/anime tests (OneChainCut is editing these now) |
| 1 | Ball record (ball+0xB0 samples, ball+0x279C, prediction table), MA nearest list (MA+0x22e4c, writer OPEN, `pes-re-integration-open.md`), touch flags P+0x2460 bits 9/10 | Kept: kind-0 and traptick sets (ball state inside); RestingBall pid130478 seq7134 (cited); act4 pid615595 seq7805 (only real call). New: ball driver `0x140873EE0`, EVERY 20, cap 12 | ball.cpp, ball.hpp, pesballtrack.hpp, pescontact.*, matchevents.cpp (ball part), pesworldrecord.*, humanoid.cpp (touch) |
| 2 | T (team): T+0x78, T+0x254 direction (s8), T+0x8e08 sheet publish, handoff fields T+0xa2a0/28c/278/26c/255/254, T+0xb6de, T+0xe4 trap arm. H (brain): H+0x450 + 8s, H+0x6d48/6d50. MA build inside team tick (INF, `pes-re-integration.md` §1.3). MI phase words (+0x1168, +0x1198). Per-player index 0 to 21, side fold at 11 | Kept: team tick pid296049 seq003, 4261, 4479 (protected); team_pass pid1792161 seq16184 (protected); carrier probes pid2392772 (11, cited); ai_arrival pid1887719 (12, cited); PortMap press pid296049 and pid1037286. New: MA nearest-list writer (OPEN, needs a site) | teamAIcontroller.cpp/.hpp, pesteamtick.*, team.cpp/.hpp, pesteamconcepts.*, pesoffball.*, pescarrier.*, pespress.*, pesmentality.*, player/controller/elizacontroller.cpp, match.hpp/.cpp |
| 3 | P action byte, P+0xAB8 interface, P+0xC40 table, P+0x354 motion table. Joint block A/B (B+0x81/82, +0x90/98 strike, +0xA4/AC yaw, +0xB4 height per AnimeLayer7 doc). Clip database global (`kClipDatabaseGlobalVa`). Motion record stride 0x390 (TrapTick docs) | Kept: anime_tick pid3778393; height pid848145; chainX pid848145; strike pid1115004 (38); kick root pid1152106 seq003 to 005 (and 12 kick sets); anime_displace pid3439759 seq10956 (cited); trap_action_tick pid3741448 (24). New: none armed (MoveStepCell hooks dropped after proof, 05-11) | pesmovestep.*, src/pes/movestep*.cpp (_run, _step, _turn, _style, _ramps, _group_a, _bundle), src/pes/animetable.*, mbinfo.*, humanoid/*, src/gdext/pes_animation.cpp |
| 4 | G formula inputs (G = 1.0 at 180 cm, `pes-re-keeper-dive.md`), P height, step playback at G, MI phase (shoot-out) | None kept for keeper. New: keeper step/save entries OPEN (name them in the keeper brief) | peskeeper.cpp/.hpp, peskickerror.*, movestep keeper gates |
| 5 | MI phase words (+0x1168 period, +0x1198 frame, +0x116C in-play), DRV+0x2790 (period 10), DRV+0x2aa8 (named sent-off), E+0x17 (min players, 7 live), player+0x18 injury mark, F record (per-player removal; F = S+0x750D0 + t*0x59650 + 0x64D8 + id*0x2134), referee record table index 0x16 | Kept: judge commit at level 2 or more, pid2738437 seq147059, 280370, 376981 (protected); kick chooser/header/root sets pid1152106 (12 each); foul_sanction (cap 6); goal anchors pid2122415, pid3234533. New: restart release `0x140792E10` and throw-in `0x140791EC0` (predicates OPEN) | referee.cpp, pesjudge.*, pessequencer.*, pesgoalkick.*, pessetpieceteam.*, pesstoppage.cpp, pesclock.*, offsiderule.cpp, matchevents.cpp, match.cpp |
| 6 | FixDemo mgr (mgr+0x78 slot list), FixDemoHuman, fdc scene records (`pes-re-fdc-format.md`), DemoInfo, step machine (+0x1d8), ctx (+0xe0), CameraPose (`pes-re-camera-pose.md`), MI clock for timing | Kept: proc camera INIT pid3741448 (cap 12; goal seq71718 to 71750, 9); anchors pid3741448 seq70461, 70462 (full); scene start pid3234533 seq6218, 6219; proc camera pid3234533 seq6220, 6224, 6228; DemoInfo pid2122415 seq70116; A0 anchors pid2122415 seq12946, 31076, 40581, 53099, 53100; fixdemo wrapper and ctx init pid1792161. New: none armed (fixdemo step hooks dropped at rvwatch36, re-arm needs a goal) | src/pes/fixdemo_select.*, fixdemo_camera_rig.*, fixdemo_table.*, fdc.*, cutscene_timeline.*, src/gdext/pes_cutscene_camera.*, godot/match/match.gd, godot/tests/test_cutscene_camera.gd |
| 7 | Stadium table, target focus (ball and man), spring k, mode index, CameraPose | None kept (CameraSelect's capture set is not in any kept list: OPEN). New: InplayCamera ctor `0x14087E480` (rare), mode-switch blend `0x14087F4A0` (rare) | src/pes/camera.*, src/gdext/pes_broadcast_camera.*, src/gdext/gf_match.cpp (INF), godot/match/match.gd |
| 8 | ReplayHuman frame records, DataManagerReplay, replay ring, pump and detector ring records (`pes-re-records.md`) | None kept. New: `0x14217DEF0`, cap 6, predicate "replay active" (OPEN: the endless testbot match may never show a replay) | New files: src/pes/replay*.{cpp,hpp}, new godot scripts |
| 9 | MI clock (HMS source), period, score (team records), tactics banner (plan), radar inputs (P and ball positions) | None kept. New: scorebug `0x1420F0EB0` is per frame, so EVERY 1000 or armed on a clock change (rule, brief-rules 06-11). HMS conversion `0x140A6BD80`, cap 6 | src/sim/onthepitch/pesclock.*, match.hpp, godot/match/rig_view.gd, godot/match/match.gd (read only) |
| 10 | Config tables (supporter, chant) read through the VFS, runtime not state (brief-rules dt18 rule). Crowd mood inputs: score, time, ball events | None kept. New: chant loader `0x1414406A0`, any call, cap 3 (load time, OPEN whether reached in the endless match) | crowdmood.*, goalcelebration.*, goalsequence.* |
| 11 | Ball-frame speed (ball record), events (goal, whistle, foul), Se and Announce and Sequence config (VFS) | None kept. New: `0x14048FDAE`, ball-hit predicate, EVERY 20, cap 12 | New: src/pes/audio*.{cpp,hpp}, new godot audio scripts (commentary out of scope) |
| 12 | Sheet plan record (T+0x8e08 publish, SetPlan writer `0x1408C5E20`), substitution state (MI), game-plan sliders, armed set-play counter | None kept. New: sheet plan writer `0x1408C5E20`, cap 6 (OPEN: the testbot runs with no tactics, so it may never fire) | src/sim/onthepitch/substitutions.hpp, src/gdext/gf_match.cpp, src/sim/data/formations.*, src/pes/formation.* |
| 13 | Start plan (setup builder reads it), kit records (uniform), stadium table (`src/pes/stadium.hpp`), settings. Edit mode and career are out of scope (owner rulings 24-10-26) | None kept. No hook: data comes from the install's files (INF) | src/gdext/pes_library.*, src/pes/formation.*, uniform.hpp, playerdb.cpp, stadium.hpp, derivation.* (editsave.* out of scope) |
| 14 | Hub records (type 2, per-player type indices, divisor word at `0x1434456B8`, runtime, OPEN), match M (M+0x10 player array), G+0x270 (MI), H brain, listener order | Kept: team pass and tt sets (above). New: none; hub order OPEN (`pes-re-integration-open.md` §1) | match.hpp/.cpp (13 and 12 branch overlaps), matchprogression.hpp (10), gf_match.cpp |

## 2. Cost drivers, with evidence

### 2a. The worked example, round by round (OneChainCut, `docs/ports/pes-port-OneChainCut.md`)

| Round | What the run went to | Class |
|---|---|---|
| 1 | Trap calls mapped the window only (`NewCall(Bytes())`); four test-fixture fixes (constructor stores, ArmYaw rdx, image tables, PeImage open) | State and mapping |
| 2 | Memset thunk abort; guard unknown; pes-loop timed out at 900 s (per-byte memset); trap stack overran slot 21 (P) and TLS block missing | Mapping, perf |
| 3 | Bulk memset (output identical by write-record proof); pes-loop hash moved, not explained | Perf, baseline |
| 4 | Determinism rerun; snapshot replay not done (budget); kick oracle tests skip because every oracle set's mem.bin links into deleted folders | Oracle and capture plumbing |
| 5 | Replays of runner and adjuster bit-exact; root oracle fails (trap_oracle.py assumes kind-0 P layout) | Oracle generator |
| 6 | First divergence needed a debug dump and a second worktree on another branch (baseline was not on the line); about 1 h of builds | Baseline and tooling |
| 7 | Trap oracle on 72 captures in nine batches of eight (3.9 GB per capture, unpacked and repacked per batch); 11 mismatches | Oracle and capture batching |
| 8 | 11 refusals unresolved; diagnostics; builder change kept but not the fix; trap-tick fixture not started | State |
| 9 | Eliminated eight candidate causes; no fix | Debugging |
| 10 | Probe; a typo in a probe address produced a false "zeroed velocity" lead | Debugging |
| 11 | Root cause: trap calls did not map PesMemory's OS arena, where the player's chain objects live. `SharedWindows` fix plus regression test | State and mapping |
| 12 | Coverage and issued tests; a binding fix; O1 identified; the four trap-tick tests and 14 tick captures still not ported | Test port |
| 13 | Running at handover | — |

Counts (CONFIRMED from the table): mapping and state about 5 rounds (1, 2, 8 to 11), oracle and capture about 4 (4, 5, 7, 12), perf 2 (2, 3), baseline and tooling 1 to 2 (6, and 3 at the cost of a hash), and the trap-tick port carried unfinished from Round 4 to Round 12. The port-doc families give a proxy for the whole chain: TrapTick 13, AnimeLayer 13, TrapContact 9, ProcCamera 8, TrapCutover 3, TrapChain 3, TrapAdmit 2, OneWorld 4 files (about 55 docs).

### 2b. Mapping and state is the largest driver

- The OS arena fix is on `origin/OneWorld-series-wip` only: `SharedWindows`, `MemoryCall`, `WriteSpan` have 0 hits on `origin/main` and 6, 2, 1 on the branch (CONFIRMED). Every new branch that runs trap calls must rediscover it.
- Stand-ins for the same reason: the top stand-ins by callers (`0x140A48A30`, `0x140A48A10`) already have translated bodies (CONFIRMED). The ponytail notes say the record they read "is not in GF" (movestep_run.cpp:1150, movestep_step.cpp:137). The blocker is the GF state layout, not the translator (INF, consistent with the notes).
- The four round-1 fixture fixes, the round-2 stack and TLS fixes and the round-11 arena fix are four separate discoveries of one class: a PES call reads memory the test did not map.

### 2c. Oracle generation and capture plumbing

- 207 `mem.bin` links are broken (`find -xtype l -name mem.bin`, maxdepth 4, CONFIRMED). Most sit in agent scratch: gf-FoulSanction-scratch/links 51, gf-StoppageFlow-scratch 53 across four dirs, gf-CutsceneTiming-scratch 45 across two dirs, gf-TrapTick6 31, gf-TrapTick7 27, gf-TrapS5 29, gf-TrapTick5 9, gf-TrapOpen 6+. The brief names deleted folders pid615629, pid802552 (18-11).
- Capture sweeps have deleted kept or cited captures at least four times (brief-rules: 30-10 goal-time captures; 04-11 sweep of 666 captures, including KickChooser's traced-but-uncited set; 05-11 sweeps 2 to 4; 31-10 deletes). Each time the fix was another PROTECTED list maintained by hand (CONFIRMED).
- Each oracle generator is per kind: `trap_oracle.py` (kind-0 only; the tick variant is missing, R12), `rootrd.py` (added R6), kick chains in scratch shell scripts (chain.sh, gen_chain.sh, not in the repo). Per-kind code is what made R5 and R12 expensive (CONFIRMED).
- Test env knobs: 13 `GF_PES_*_ORACLE`-style variables, each a separate hand-wired path (CONFIRMED, grep of tests/). Ten test files depend on them.

### 2d. Merge tax and collision surface

- Each chain and gameplay branch is 161 to 228 commits behind main (main-only count) and carries 54 to 291 of its own (CONFIRMED).
- Hand-written files: 208 distinct files touched across the 15 unlanded branches. 167 are touched by at least two branches; 105 by at least three (CONFIRMED, three-dot diff against origin/main, `src/`, `tests/`, `godot/`).
- Hot files: `match.hpp` 13 branches, `match.cpp` 12, `teamAIcontroller.cpp` 10, `pesworldrecord.{hpp,cpp}` 10, `matchprogression.hpp` 10, `ball.{hpp,cpp}` 9, `pesmovestep.cpp` 8, `pestrapactor.cpp` 8 (CONFIRMED).
- Generated layer: 10,403 touched entries across the same branches, 5,062 distinct layer files. So about 96% of the touched entries are generated code that every branch regenerates (CONFIRMED). `translated.hpp` and `pes_dispatch_gen.cpp` are each touched by 10 branches (CONFIRMED).
- Open gates on merged branches: TrapAdmit ("GATE branch NOT RUN; mutations NOT run"), TrapContact ("mutations not run; smokes not done"), PassArrival still red waiting for the merge lane (handover 17-11). Each is a future run (CONFIRMED, port docs and handover).

### 2e. Rig and hook time

- Every hook batch is a PES restart plus a "prove the match runs" step (brief-rules 30-10). Per-frame hooks at EVERY 1 park the game: 0 trap captures in 25 min (18-11), and 36 new hooks at kick-off held PES past the bot's clock (03-11). Each hook batch therefore costs a restart and a wait, and a wrong predicate costs a whole launch (CONFIRMED, brief-rules).

### 2f. Ledger credit (the owner's bottleneck a)

- The translated layer and the ledger disagree by subsystem: pre-match layer 309 files, ledger translated 0; replay 161 files, ledger translated 0; cutscene 614 files, ledger translated 0; gameplay 1226 files, ledger translated 1 (CONFIRMED numbers). The cause is in LedgerCredit's scope (INF).

### 2g. Smoke and first divergence

- The pes-loop hash moved at R3 with no located cause. R4 did not locate it. R6 found it by a debug dump on two trees (tick 1, 16 men, one ulp), after two 15-minute smokes and an hour of builds (CONFIRMED, port doc). The debug dump was removed, so the next divergence costs the same again.

## 3. Tooling and process proposals (five)

**T1. Engine-memory fixture from the replay's read set (top proposal).**
- What: each captured call already has a bounded replay (`snapdiff.py --bounded`, `readset.py` in `tools/dev/translate/`). Emit, per call, the list of windows that cover its read and write set (base, size, source). Build the test fixture (`PesEngineFixture`) from that list plus the standard windows (image, window, TEB and TLS, stack, OS arena). Add a build-time check: any address the replay reads outside every mapped window fails the fixture and names the address. The same generator writes the PES-layout state from the capture, replacing hand-written `Build` code (pes_trap_oracle_test, pes_kick_oracle_common).
- Cuts: Round 1 window mapping, Round 2 stack and TLS, Rounds 8 to 11 arena (about 4 rounds). INF: with this check, the 11 refusals of Round 8 fail at the first arena read, named.
- Acceptance: run the 72 trap kind-0 captures through the fixture with the arena window removed; expect each of the 11 seq027 to 037 refusals to name an arena address.
- Effort: small to medium (one generator flag in snapdiff, one fixture header).

**T2. Capture-set registry, with generated oracle dirs.**
- One file (`run/capsets.json`): each set = name, hook, launch, seqs, status (active, cited, protected), owner branch, doc. Oracle dirs are generated from it with manifests that point at real paths, never symlinks. The gate fails if a registered path is missing (today 207 broken). `auto_slim`, `capz` and any sweep read the registry and refuse active or protected entries.
- Cuts: Round 4 blocked kick tests; the four sweep losses; the hand-kept PROTECTED lists.
- Acceptance: `gate` reports 0 broken oracle links; a sweep dry run refuses every entry the handover names as protected.

**T3. One oracle generator, driven by spec files.**
- `tools/dev/translate/oracle.py --spec <spec> --set <capset>`: frame_trace, then snapdiff `--bounded`, then manifest. Kind-agnostic. Replaces `trap_oracle.py` (kind-0 only), `rootrd.py` (per spec), and the scratch chain scripts. The trap tick variant becomes a spec file, not new code.
- Cuts: Rounds 5, 6, 7, 12 (about 4 rounds).
- Acceptance: regenerate the kind-0, chooser, header and root oracles from the registry with no per-kind code; the kick and trap tests pass on the regenerated dirs.

**T4. Regenerate the layer in the merge lane, not on branches; a hot-file rule.**
- Branches commit hand-written code and closure JSON. The merge lane regenerates the translated layer once per batch (ccache: 536 s with a warm cache, brief-rules 03-11) and runs the drift check. A branch that edits a hot file (match.*, teamAIcontroller.*, ball.*, pesworldrecord.*, pesmovestep.*) registers it in the registry; at most one branch per batch holds each hot file.
- Cuts: the 10,403 layer entries and 3-way conflicts in generated files; the merge tax (section 2d).
- Acceptance: after the next OneWorld merge, a branch diff against main has no `src/pes/translated` change except closure JSON, and the drift check passes.

**T5. First-divergence tool and smoke-baseline registry.**
- Commit the per-man per-tick dump behind an env flag (GF_TICK_DUMP, removed in R6), a smoke-hash table keyed by commit (written by the gate), and `first_divergence.py A B` printing tick, man and ulp distance.
- Cuts: Rounds 3 to 6 (two rounds plus about an hour of builds).
- Acceptance: one command reproduces the R6 result (tick 1, 16 men differing by one ulp at 38 m) from two stored dumps.

## 4. Subsystem dependencies and the parallel plan

### 4a. Dependency order

- **Gate:** `OneWorld-series-wip` lands first (Main lane). It carries the trap and anime chain, the joint block, the `SharedWindows` arena fix, and it is the base for TrapContact, TeamTick, MoveStepCell, KickMotion, PressAI, PassArrival, WhistleBall and ForfeitSettings (all overlap its hot files, section 2d).
- **Team/AI** (team accessor, team tick, PressAI/PressCut): waits on the chain (match.*, teamAIcontroller.*).
- **Ball** (TrapContact, RestingBall, WhistleBall): waits on the chain (ball.*, pesworldrecord.*, matchevents.cpp, which Team/AI also touches).
- **Movestep and kick** (MoveStepCell, StepLand, StepAudit, KickMotion): waits on the chain (joint block) and the team record (Team/AI).
- **Referee, restart, set pieces:** waits on Ball (possession) and Movestep/kick (restart kick path).
- **Keeper:** waits on Movestep/kick (save gate, mover playback) and Referee (penalties).
- **Cutscene and camera** (CutAnchor6, ProcCamera, CameraSelect): reads kept captures. Does not wait on the gameplay merges, unless FixDemoHuman's anime path (`0x141EB9640`, blend) reads the chain (OPEN: check before launch, section 4c).
- **Replay, HUD, crowd, audio:** new files only. Needs one hook batch (section 4c). No dependency on gameplay merges.
- **Pre-match and in-match menus:** reimplementation from PES's own data is allowed (owner 25-10-26). Independent of the sim. Lowest priority (menus are nice-to-have).

### 4b. Waves

| Wave | Line | Starts when | Main files it owns (disjoint within the wave) | Captures or hooks |
|---|---|---|---|---|
| 0 | Main lane: land OneWorld-series-wip alone (merge main, fast gate) | Now (OneChainCut's round 13 first) | merge only | none |
| 0 | B1 Tooling: T1 and T2 | Now | `tools/dev/translate/{snapdiff,readset}.py`, `tools/dev/capsets/` (new), `tests/pes_engine_fixture.*` (new). Does not touch `pes_engine_trap.hpp` or trap tests | kept sets only |
| 0 | B2 Presentation greenfield: replay, HUD clock, crowd, audio cues | Now (needs one restart) | new `src/pes/{replay,hud,audio,crowd}*`, new godot scripts; no edit to `match.gd` | 4 hooks, one restart (section 4c) |
| 0 | B3 Cutscene and camera: CutAnchor6 and ProcCamera to landing | Now (check the anime-path question first) | `src/pes/fixdemo_*`, `cutscene_timeline.*`, `src/gdext/pes_cutscene_camera.*`, `godot/match/match.gd` (this line owns it) | kept captures (section 1b row 6) |
| 0 (queued) | B4 Pre-match and in-match menus from PES data | First free slot | `src/gdext/pes_library.*`, `src/pes/formation.*`, `uniform.hpp`, `playerdb.cpp`, `stadium.hpp`, `substitutions.hpp` | install data (no hook) |
| 1 | Team/AI line (team accessor, team tick, PressAI/PressCut) | Chain on main | teamAIcontroller.*, pesteamtick.*, team.*, pesteamconcepts.*, elizacontroller.cpp | team_pass and ai_arrival (kept) |
| 1 | Ball line (TrapContact, RestingBall, WhistleBall) | Chain on main; after Team/AI owns matchevents.cpp | ball.*, pescontact.*, pesballtrack.hpp, humanoid.cpp | kept kind-0 and RestingBall; ball driver hook |
| 2 | Movestep and kick (MoveStepCell, StepLand, StepAudit, KickMotion) | Team/AI and Ball on main | pesmovestep.*, src/pes/movestep*.cpp, animetable.*, mbinfo.* | kept anime_tick, strike, kick sets |
| 2 | Referee, restart, set pieces | Ball on main; kick path from Wave 2 movestep | pesjudge.*, pessequencer.*, pesgoalkick.*, pessetpieceteam.*, referee.cpp, pesclock.* | judge, kick, foul sets (kept) |
| 3 | Keeper | Movestep and referee on main | peskeeper.*, peskickerror.* | new keeper hooks (OPEN, name in the brief) |
| 3 | In-play camera (CameraSelect) | B3 on main | src/pes/camera.*, src/gdext/pes_broadcast_camera.* | CameraSelect set (OPEN) |

Concurrency: owner cap 3 agents (17-11) and one full build at a time under `nice -n 19 cmake --build … -j8` (16-11, 17-11). Wave 0 is three agent lines plus the Main lane. B4 takes the slot when one frees.

### 4c. Hook batch for Wave 0 (one restart, Main to arm)

| Hook | VA | Rule | Predicate and cap | Line |
|---|---|---|---|---|
| replay frame read | 0x14217DEF0 | Rare entry, EVERY 1 OK | replay active, cap 6 (OPEN: whether the testbot match produces replays; if not, Main needs a goal or a replay trigger) | B2 |
| scorebug update | 0x1420F0EB0 | Per frame: EVERY >= 1000 | on clock change, cap 6 | B2 |
| ball-hit tier picker | 0x14048FDAE | Per ball hit: EVERY 20 | ball-hit predicate, cap 12 | B2 |
| chant loader | 0x1414406A0 | Load time: any call | cap 3, one call per launch | B2 |

No new hook for B1 (kept sets). B3 needs no new hook unless it wants a fresh goal (the proc-camera and anchor sets are kept). A "prove the match runs" check follows the restart (brief-rules 30-10).

### 4d. Open items for Main

1. Confirm the Wave 0 cap (3 agents, 17-11) and whether the Main lane counts as the fourth slot.
2. Confirm one restart for the four B2 hooks, and the predicate for the replay hook.
3. B3: check whether FixDemoHuman's anime blend (`0x141EB9640`) reads the chain. If it does, B3 waits for the chain landing on main, and runs only its non-anime parts now (OPEN).
4. Tag check: `0x140CA1A50` (1823 callers) is cited to attack decisions (AI) but tagged in-match menu in the gap list. LedgerCredit should confirm the tag.

## 5. Method and caveats

- Commands: `grep`, `awk`, `jq` over `docs/`, `tools/dev/translate/results_*.json`, `src/`, `tests/`; `git rev-list --left-right --count`, `git merge-base --is-ancestor`, `git diff --name-only origin/main...origin/<b>`; `find -xtype l`. All read-only.
- Branch overlap counts use three-dot diffs against `origin/main`, so they include files a branch changed in its own history. Branch counts include merged-in commits. Treat the commit counts as a proxy for merge size, not as run counts.
- "Run" counts come from the round headings of the OneChainCut doc (12 rounds) and port-doc family counts (a proxy). They are not a measured per-subsystem cost. The owner's "about 40 runs per subsystem" is not checked here.
- `results_*.json` has two verdict fields (`diffed` for the harness, a separate proof path for snapdiff files). Counts above use `diffed` only.
- The rig is off (brief-rules, 18-11 latest). Nothing here reads PES live.
- Process note: I wrote two temporary files under `/tmp` (overlap lists) and removed them. Rule: scratch goes in `<workdrive>/gf-<Name>-scratch`.
