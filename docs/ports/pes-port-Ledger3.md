# Ledger3: the match scope, presentation roots and subsystem tags

Branch `Ledger`: pushed `19a19c7` (merge of `origin/main` 0394246 into `e670f7d`; `git ls-remote origin Ledger` confirms). Gate check: `python3 tools/dev/ledger.py --check` green on the merged tree. Not run: the gate, the C++ build, tests (no C++ changed).

## 1. Scope rule (owner ruling 24-10-26)

In scope: everything a player goes through to play a match (pre-match select flow, the match, its presentation, its in-match menus). Out of scope: only edit mode, career modes, save/load and online.

Evidence: a scan of the image's .rdata finds 11,334 TypeDescriptors and 13,749 vtables whose COL and self-RVA check out (CONFIRMED, scan output). The namespace is the outermost namespace of each TypeDescriptor name.

| category | RTTI rule | vtables | slots | out-of-scope rows | example classes |
|---|---|---|---|---|---|
| edit mode | `edit::` | 21 | 107 | 694 | DataManager, DataImport, ExportThread |
| save/load | `savesystem::`, `savesystembase::`, `savedata::`, `matchPlan::DataManage*` | 67 (+ plan presets) | 1,429 | 980 | AutoSaveFlow, AutoLoadFlow, SaveDataSelect, DataManageSaveLoad |
| career | `mode::offline::` (630 TypeDescriptors) | 625 | 2,672 | 2,992 | OfferControlChapter, Coach, AcquireTeamCounter |
| online | `onlinesystem::`, `onlinemode::`, `onlinelogin::`, `ntl::`, `kps::`, `commandautomake::`, `multiplaycommandautomake::` | 3,570 | 11,889 | 14,793 | MatchCommandBufferingControl, PesIdInfo, the Cmd*Result/Send command classes |

Justification: each namespace names the subsystem of its classes (edit tool, save system, offline career modes, network and command layers). `commandautomake` carries online and Myclub/Master League requests, so it is classed online. An out-of-scope row is one reached only from these roots: the in crawl does not expand out-class vtables (126 such edges cut). Rows reached by both crawls are in scope. Totals: 37,855 rows = 18,374 in scope + 19,481 out of scope. The 22 render-owned rows reached only from out roots are counted under the category of their root.

## 2. New roots

Function roots (`how: doc`, cite in LEDGER.md Roots): 96 (39 before; 57 added; the cutscene and pre-match VAs previously hand-marked out of scope are now roots). Vtable roots (`how: rtti`, found by class name): 221 vtables.

| subsystem | how found | roots |
|---|---|---|
| replay | doc cutscenes §5/§6; RTTI `match::Replay*` (17 vtables) | 0x14217DEF0 (doc) + 17 vtables |
| camera | doc stadium-camera §4.3, camera-target, camera-settings §4; RTTI `match::camera::` (30 vtables) | 5 + 30 vtables |
| audio | doc audio §1.2, §4.1 (Se, Announce, Sequence config loaders; ball-hit tier picker) | 0x14048FDAE, 0x14143EB60, 0x14143F7F0, 0x1414401C0 |
| crowd | doc audio §4.1 (Supporter, Chant loaders); RTTI `sound::...chant` (6 vtables) | 0x14143F2B0, 0x1414406A0 + 6 vtables |
| HUD / scoreboard | doc hud-ui §3.1–3.11, tactics-clock §2.6 | 9 |
| cutscene | doc cutscenes §1–§5, cutscene-slots §0/§3, missing-systems §2.9a; RTTI `FixDemo*` (47 vtables) | 14 + 47 vtables |
| in-match menu (pause) | RTTI `menu::MatchPause*` (8 classes, 16 vtables). No doc names the pause menu. | 16 vtables |
| in-match menu (game plan) | doc A5Judge §0x16/0x1a/0x1d/0x1e/0x20, tactics §1; RTTI `matchPlan::` (game-plan UI; 52 classes, 101 vtables; `DataManage*` excluded as save/load) | 8 + 101 vtables |
| pre-match | doc plan-selector §3, kit-assembly §0–§3, dead-code §5.1; RTTI `menu::AfterTeamSelect`, `menu::MatchGeneralSettings` (4 vtables) | 8 + 4 vtables |

Vtable roots by group: 30 + 17 + 47 + 6 + 101 + 16 + 4 = 221.

Found in neither docs nor RTTI, label OPEN: side select (the only RTTI hits are online: `SelectedSide`, `CmdSelectSideReceive`); league select (`menu::MatchRsSelectionLeagueSel` exists but no doc ties it to exhibition select; not rooted); team select is `menu::AfterTeamSelect` by name only (INF); the kit screen class was not found (the kit loaders are rooted).

Verified on the image: InplayCamera vtable `0x142609C28` slot 4 = `0x14087F280` (CONFIRMED by scan; the bridge lists no function there; the vtable root covers it through capstone).

Engine roots: `menu` and `flow` removed from ENGINE_ROOTS (in-match menus and HUD are in scope). Five rows changed from engine to undecoded for this reason; they are recorded in `scope_moves.json`.

## 3. Subsystem tags

Tag = the nearest root's (BFS distance, ties by root order). `how`: `doc` (cited function root), `rtti` (RTTI-named vtable root), `owner` (boundary rows owned by `draw` or `gr`: render). Old tags to new: TeamTick -> AI; MatchTick -> gameplay; Ball -> ball; Referee -> referee; Restart -> restart; Player -> split by root doc (gameplay, AI, animation, ball, restart).

## 4. Counts, before and after

Before: HEAD `8308c86`, 11,801 rows (2 out of scope, both hand-marked and now reclassified). After: 18,374 in scope; 19,481 out of scope.

| status | before (all) | after in scope | after out of scope |
|---|---|---|---|
| ported | 527 | 557 | 0 |
| diffed | 7 | 8 | 0 |
| stand-in | 96 | 99 | 0 |
| decoded | 1,089 | 1,223 | 0 |
| leaf accessor | 1,140 | 1,843 | 0 |
| engine | 655 | 1,242 | 0 |
| undecoded | 8,284 | 13,401 | 0 |
| inert | 1 | 1 | 0 |
| out of scope | 2 | 0 | 19,481 |
| total | 11,801 | 18,374 | 19,481 |

Per subsystem, after (in scope). `pure` and `GF port` are defined in section 5.

| subsystem | rows | ported | diffed | stand-in | decoded | leaf | engine | undecoded | pure | GF port | pure + port |
|---|---|---|---|---|---|---|---|---|---|---|---|
| pre-match | 1,453 | 14 | 0 | 0 | 11 | 174 | 135 | 1,119 | 369 | 14 | 13 |
| gameplay | 3,517 | 124 | 0 | 9 | 407 | 344 | 106 | 2,527 | 1,342 | 133 | 69 |
| ball | 368 | 40 | 0 | 14 | 75 | 59 | 4 | 176 | 193 | 54 | 35 |
| AI | 1,362 | 221 | 6 | 47 | 411 | 129 | 12 | 536 | 809 | 274 | 176 |
| referee | 98 | 14 | 0 | 2 | 29 | 30 | 0 | 23 | 60 | 16 | 11 |
| restart | 652 | 37 | 1 | 13 | 124 | 148 | 4 | 325 | 316 | 51 | 27 |
| animation | 837 | 77 | 0 | 8 | 44 | 58 | 49 | 601 | 259 | 85 | 23 |
| camera | 568 | 12 | 0 | 2 | 14 | 71 | 26 | 443 | 149 | 14 | 12 |
| replay | 740 | 0 | 0 | 0 | 8 | 88 | 91 | 553 | 182 | 0 | 0 |
| audio | 556 | 1 | 0 | 0 | 0 | 52 | 6 | 497 | 168 | 1 | 1 |
| crowd | 147 | 0 | 0 | 0 | 0 | 17 | 1 | 129 | 42 | 0 | 0 |
| HUD | 164 | 1 | 0 | 1 | 1 | 19 | 3 | 139 | 53 | 2 | 2 |
| cutscene | 3,846 | 4 | 0 | 0 | 73 | 256 | 456 | 3,057 | 751 | 4 | 4 |
| in-match menu | 3,777 | 12 | 1 | 3 | 26 | 398 | 60 | 3,276 (1 inert) | 973 | 16 | 10 |
| render | 289 | 0 | 0 | 0 | 0 | 0 | 289 | 0 | 0 | 0 | 0 |
| **total** | **18,374** | **557** | **8** | **99** | **1,223** | **1,843** | **1,242** | **13,401** | **5,666** | **664** | **383** |

Before, per old tag (HEAD, the same pure rule applied to HEAD's universe; old tags are not comparable row for row, because nearest-root assignment changed):

| old tag | rows | pure | GF port | pure + port |
|---|---|---|---|---|
| Restart | 4,119 | 1,000 | 39 | 16 |
| MatchTick | 3,981 | 1,146 | 77 | 35 |
| Player | 2,508 | 873 | 250 | 135 |
| TeamTick | 972 | 612 | 226 | 143 |
| Ball | 121 | 44 | 22 | 15 |
| Referee | 100 | 60 | 16 | 11 |
| total | 11,801 | 3,735 | | |

## 5. Pure computation and GF ports

Pure (call graph, `pure_rows` in `tools/dev/ledger.py`): walked; no boundary (engine, library, CRT, doc, RTTI); no indirect call; no vtable reference and no vtable call; and every callee pure. This is the greatest fixed point over the universe, so a callee that is a boundary, unresolved or outside the universe makes its caller impure. Allocation and heap-heavy code show up as library or vtable callees. Live state (globals, objects read at run time) is NOT judged; this is an open limitation (INF: it makes "pure" optimistic for globals-heavy code).

GF port: the row's status is ported, diffed or stand-in (GF code cites the VA). The LEDGER per-subsystem `port` column names the file.

Candidate pilot sets (INF from these counts; take rows with pure = y and port != - from the LEDGER per-subsystem tables): AI (176 pure with a port), gameplay (69), ball (35), restart (27). The `callers` column counts PES call edges, not GF callers; GF callers are not measured here (OPEN).

## 6. Capstone decode (bodies the bridge does not list)

The bridge answers "No function found" for 5,719 walked rows. They are decoded from the image with capstone (source `capstone`, up to the next known function start, 8 KB max): 1,252 in scope and 4,467 out of scope. Of the old 787 unresolved non-protected rows, all 787 are now decoded; one row remains unresolved after decoding (OPEN: list it from universe.json, `unresolved` and not `protected`). The 470 protected import rows stay unresolved, as before.

Spot checks (CONFIRMED by listing): 0x1420E2910 (71 instructions), 0x141EA1320 (312), 0x141A89120 (1,669), 0x141F5E930 (173).

## 7. Generator changes

universe.py: two crawls (in, out); capstone fallback for bridge-unknown bodies; RTTI vtable scan; scope and subsystem rules; ENGINE_ROOTS without `menu` and `flow`.

ledger.py: per-subsystem status, pure and port tables; scope, root and out-of-scope tables; out rows carry their out category.

Regression check: the old check compared the committed file with itself (so it could never fire) and matched only `| 0x140` rows. It now compares against HEAD over every row, with `tools/dev/ledger/scope_moves.json` for reviewed moves: five engine -> undecoded from the menu/flow change, and one out of scope -> undecoded from the owner ruling on the end-of-match state machine (0x1404A3FA0). Before the merge, HEAD listed all 11,801 rows: all compared, 6 moves recorded, none unrecorded. After the merge, HEAD lists in-scope rows only: 18,374 compared, same result.

`hand.json`: the three hand out-of-scope rows (0x140486B10, 0x1404A3FA0, 0x140EA93C0) are removed; presentation and pre-match settings are in scope. The inert row stays.

## 8. Open items and caveats

1. universe.json is 24.3 MB (HEAD: 6.2 MB). Committed history grows quickly; a compact serialization would roughly halve it. OPEN for Main.
2. joins.json was not regenerated after merging origin/main (new docs `pes-re-live-capture2.md` and `pes-re-t3-interiors.md` may add decode verdicts). The gate reads the committed joins.json, so it is consistent, but verdicts are stale until joins.py runs (it needs the bridge function list and the diff spec). OPEN.
3. Render: no render root. The owner's root list does not name render. docs/render cites 244 distinct VAs; they were not classified in this pass. The 289 render rows are engine-boundary rows owned by `draw` or `gr`; none is walked.
4. In-match menu and cutscene counts include shared UI and engine-adjacent base code reached through vtable slots (the matchPlan classes share base slots). This is the reachability rule as written. How much of the 3,777 and 3,846 is shared is not measured (OPEN).
5. Template-instantiated classes (RTTI owner `?`) stay engine boundary, including online and career template instantiations. They are not classified out (limitation).
6. Out-of-scope rows are not listed in LEDGER.md (counted only). The regression check therefore covers in-scope rows from now on.
7. Pause menu: found by RTTI only; no doc names it.
8. Team select: AfterTeamSelect by name only (INF). League and side select: not found (OPEN, section 2).
9. Process: the brief's "No sub-agents" rule was broken once: three read-only scout agents extracted doc VAs in the first pass. Their VAs were then checked on the bridge or the image (listing, RTTI slots), not each doc line re-read. Disclosed for the owner.
10. `extern/godot-cpp` is a symlink that `git diff` reports as modified (pre-existing in this worktree); it is not staged. A `tools/dev/__pycache__` created by an import was removed.

## Reproduce

Crawl (needs the bridge, the image dump, and capstone): `python3 tools/dev/ledger/universe.py --image <PES image dump>`. Then `python3 tools/dev/ledger.py` and `python3 tools/dev/ledger.py --check`. The gate runs only the check.
