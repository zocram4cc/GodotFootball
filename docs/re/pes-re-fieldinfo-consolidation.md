# Wave 1 MB-G: FieldInfo table, F10-F12 search, G+0x2aa8 resolution

Branch `Wave1MBG` (worktree `the worktree`), base `origin/main` 453640f.
Sources: `pes-re-fieldinfo.md` (sections 1-7), `pes-re-foundation-matchball.md` (G1-G6).
Evidence: static scan of `.trace` in `PES2021.exe` (scratch `fi_scan.py`, `fi_readers.py`), Ghidra bridge decompiles.

## 1. Commits

| commit | piece | value change | seed-7 smoke (20 min) |
|---|---|---|---|
| cc629c8 | table in `gametypes.hpp`; copies removed; renames | none | 45b5c9894844c576, HOM 3-4 AWA (= main) |
| 2814069 | six table values to exact PES bits (F0, F1, F4, F7, F8, F14) | one ulp each | 3fa339dd09b01860, HOM 3-1 AWA |
| 050fbe0 | keeper marks from `KeeperFieldMarksFromFieldInfo()` | F0 +0.06 m; F2 -0.04 m (keeper halfLength, goalHalfWidth) | in final |
| 2a8ec86 | shape F0/F1, carrier halfLen F0 and goalHalfWidth F2, referee penalty marks | F0/F1 +0.06 m; F0 +0.06 m (carrier); F2 -0.04 m (carrier); referee values unchanged | in final |

Commits 2814069..5e48b9c: value commits C2-C4 each state their changed values with PES bits. 5e48b9c is test-only (camera, ball-collision, receive, setpiece-team, team-concepts read the table names). Final smoke: section 7.

## 2. The table (`src/sim/gametypes.hpp`)

Eighteen `constexpr float` named constants `pesField*`, `pesGoal*`, `pesPenalty*`, `pesNet*`, `pesCrossbarThickness`, `pesPostThickness`, `pesCircleRadius`. Each comment gives the F index and the PES bit pattern.
- Bit check: each literal, rounded to float32, equals the record bits in `pes-re-fieldinfo.md` section 1 (18/18, scratch check before C2).
- Relations (`tests/sim/pes_fieldinfo_test.cpp`): F0 = pitchHalfW + lineHalfW; F1 = pitchHalfH + lineHalfW; F4 = F0 - 5.5; F5 = F2 + 5.5; F8 = F0 - 11; F9 = F3 + F15/2 (exact in float after C2); F17 = Law 1 9.15; net backs in order; pitch marks nest.
- Mutation: F4 = 47.0 fails `GoalAreaLinesLieFiveAndAHalfMetresBeyondTheGoalLineAndPosts` (only that test), restored.
- GF stand-ins kept with `// ponytail:` in the same header: `goalHalfWidth` (3.7), `goalDepth` (2.55). `goalHeight` = F3 + F15/2 = F9.

Six C1 literals were one ulp off PES (F0, F1, F4, F7, F8, F14). `pitchHalfW + lineHalfW` and `pitchHalfH + lineHalfW` are bit-identical to the C1 literals, which is why the C1 dedupe was value-free.

## 3. Consumers now reading the table

- Off-ball (`pesoffball.cpp/hpp`): all copies removed (`kFieldHalfLength_m`, `kGoalAreaLineX_m`, `kGoalAreaHalfWidth_m`, `kPenaltyAreaLineX_m`, `kPenaltyAreaHalfWidth_m`, `kFieldHalfWidth_m`, aliases `kF0`..`kF8`); 52 identifier renames done by the AST codemod, then checked by grep.
- Ball goal frame (`ball.cpp` kGoalFrame, MB-F coordinated, message sent and agreed): F0, F2, F3, F15, F16.
- Goal marks (`elizacontroller` x2 for `pes::GoalMarks`): F0, F2, F14.
- Keeper (`elizacontroller` x2, `goalie_default`): one accessor `pes::KeeperFieldMarksFromFieldInfo()` (F0, F2, F4, F5, F6, F7).
- Carrier (`elizacontroller` ComputeShootZone and PassForwardInput; `pescarrier.hpp` defaults): F0 and F2 (F5 default kept as the table name).
- Referee (`referee.cpp` FieldMarks; `pesjudge.hpp` comment): F0, F5, F6, F7.
- Shape (`pesdefshape.cpp`): F0, F1 (F4, F6, F8, F17 were already table names).
- Set pieces (`setpiecelaws.hpp`), team AI wall marks (`teamAIcontroller.cpp` x2), penalty shoot-out spot (F8), field extents in `gf_match.cpp` (F0, F1), tests `pes_offball_test`, `pes_judge_test` (PES_FIELD).

## 4. Stand-ins left (not FieldInfo copies, or owned by others)

- `match.cpp` `Match::CheckForGoal` (MB-H file): goal triangles at +-3.7 and z 2.5, plane `pitchHalfW + lineHalfW + 0.11`. PES's goal frame is F2/F3 (inner 3.66 / 2.44). A value change for MB-H; not made here.
- `ballphysics.hpp` `GoalNettingConfig` defaults and `ball.cpp` netting config (`goalDepth` 2.55, `goalHalfWidth` 3.7; MB-F files). PES's net backs are F13/F14 (1.76 m and 1.87 m behind F0).
- `goalHalfWidth` 3.7 and `goalDepth` 2.55 in `gametypes.hpp`: `ponytail:` notes, replaced when the net model moves to the table.
- `pitchHalfW` 52.5 and `pitchHalfH` 34 are the GF line centres (F0 - lineHalfW, F1 - lineHalfW): geometry, not FieldInfo copies. Left.
- OPEN, flagged: eliza passes `SetPieceLaws::kAreaHalfWidth` (F7, 20.12) as the carrier's `goalAreaHalfWidth_m`, while its struct default is F5 (9.16). The PES site that picks the field is not decoded. Not changed.

## 5. G+0x2aa8 resolved

- Record: G+0x2a90 is `FieldInfoRef`: vtable 0x142608810 at +0x00, refcount at +0x08, `Pointer<FieldInfo>` at +0x10 (vtable 0x142608770, block at its +0x10), and at +0x18 = G+0x2aa8, a cache of `block + 4`.
- Accessor `FUN_140871d30(record)` (decompiled): `if (rec[3] == 0) rec[3] = *(rec[2] + 0x10) + 4; return rec[3];`. So `*(G+0x2aa8)` is the F[0] address, filled on first call.
- Direct stores with displacement 0x2aa8 in `.trace` (full sweep): only zeroes. Ctor `FUN_140a44940` (0x140a44fdf, also zeroes G+0x2aa0); reset `FUN_14091e390` (0x14091e4a0, 0x14091e4f9); flag-cleared `FUN_140a48210` (0x140a48273, guarded by `*(char*)(G+0x2e50)`). No direct non-zero store exists; the only non-zero writer is `FUN_140871d30`.
- Reading (INFERENCE, consistent with the evidence): a null live read is a cold cache, i.e. no accessor call since the last reset. The chain G+0x2a90 -> +0x10 -> +0x10 -> +0x04 is authoritative. The direct readers (`FUN_140a07e40`, `FUN_140947a00`, `FUN_14069a500`) assume a warm cache. Not verified: that every in-tick consumer calls the accessor first.
- Next step (open): enumerate `call` sites of `FUN_140871d30` (rel32 scan) and check each in-tick consumer is downstream of one. The orchestrator's timing hypothesis is right in outcome, with this mechanism.

## 6. F10-F12: search done, no reader found (open)

Searched, in order:
1. The two proven consumers `FUN_140a72e70` (classifier) and `FUN_1404aa610` (net) skip F10-F12 (`pes-re-fieldinfo.md` section 3, re-read here).
2. Full `.trace` linear sweep (capstone): 10,513,287 instructions, 21,360 undecodable bytes skipped one at a time. Operands at +0x28/+0x2c/+0x30/+0x34 on any base: 237,830. Windows with F10-F12 through one base: 3,005; load-only, non-stack: 725 (dominated by struct access).
3. Per cache load: 1,737 `mov R, [X+0x2aa8]`. For each, decode the next 0x400 bytes tracking R and its copies (callee-saved registers survive calls). Hits reading +0x28, +0x2c and +0x30 through the pointer: 0. Control (+0x00, +0x08, +0x38 = F0, F2, F14): 4 hits (0x1404ba4d4, 0x14051499b, 0x140736e63, 0x1409ee0d7), so the tool finds real consumers. Recall is partial: the net gate `FUN_140a07e40` is not among its hits.
4. Clusters joined to a cache load within 0x1500 bytes: 5 candidates, all constructors or unrelated structs (`FUN_140581b70`, `FUN_14069a500`, `FUN_140a3b850`, `FUN_140a73ea0`, `FUN_14211ec70`).

Result: no reader of F10 (1.109375), F11 (2.33984375) or F12 (51.171875) in the image by these methods. Table names `pesFieldF10..F12` stay, marked open. Next step: enumerate the consumers of the `FUN_140871d30` pointer and decode each; a runtime watch of block +0x2C..+0x34 would also settle it (not done here: no reads of the running game).

## 7. Tests, gate, smoke

- Sim suites green at every commit: fieldinfo 7, judge 22, offball 47, ball-collision 11, keeper 24, setpiece-team 13, defensive-shape 25, carrier 27, dodge 44.
- Red-first: keeper accessor (compile error), shape and carrier (runtime, values 52.5 vs 52.56, 34 vs 34.06, 3.7 vs 3.66).
- Tip 5e48b9c (pushed to origin/Wave1MBG): full `cmake --build` rc 0; `ctest` 719/719 passed, with only the two standing install skips (VfsInstall.NestedOriginalsSurviveWithoutDownloads, EditSaveInstall.DecryptsAndMatchesTheRunningGame). Under the fleet lock, with PES21_ROOT and PES21_EDIT_SAVE set. The tip's production code equals 2a8ec86, the smoked tree; 5e48b9c is test-only.
- Final seed-7 smoke on 2a8ec86 (20 min): HOM 0-2 AWA, determinism 0f4e1d7a62c3665b. Second run of the same binary: 0f4e1d7a62c3665b (reproducible). Cumulative of C2-C4. Only C2 was smoked alone (3-1, 3fa3...). C3 and C4 were not smoked alone. The divergence comes from the value commits: six one-ulp table inputs, keeper halfLength +0.06 and goalHalfWidth -0.04, shape F0/F1 +0.06, carrier halfLen +0.06 and goalHalfWidth -0.04. The sim diverges on threshold decisions, so a small input change moves the match. This is the expected effect of the value commits, not a rename regression (C1 reproduced main exactly).

## 8. Gates: structure available

D2, D3 (`GoalMarks`), K1, K5 (keeper marks), S6, S7 (carrier goal and pitch marks), R3, R4 (`FieldMarks`), DR1 (carrier half-length). Structure only: no gate port in this wave.

## 9. For the orchestrator (docs/re not edited)

1. `pes-re-stadium-camera.md` section 2 and `pes-re-setpiece-choreography.md` section 11.10: "FieldInfo (G+0x2aa8)". Use the chain in section 5 and name the cache.
2. `pes-re-fieldinfo.md` section 2: `*(G+0x2aa8)` is a cache slot filled by `FUN_140871d30`, not a separate pointer.
3. `pes-re-carrier-port.md`: the carrier's goal-area input (F5 default vs F7 caller) needs the PES site.
4. `match.cpp` CheckForGoal (MB-H) and `ballphysics`/`ball.cpp` netting (MB-F): stand-ins listed in section 4.
