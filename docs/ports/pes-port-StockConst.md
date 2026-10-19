# StockConst: audit of internalized dt18 values + stock fixes (owner ruling: stock in tree, overridable at runtime)

Branch `StockConst`, worktree `<workdrive>/gf-StockConst`, base `origin/main` `8fc3292`. Pushed; `git ls-remote origin StockConst` = `bfcd6a9`.

Owner ruling chain: dt18 values internalized into the tree (code constants/defaults, test pins, fixtures, comments, docs quotes) must be the STOCK values from `Data/dt18_all.cpk`. At runtime GF reads dt18 through the VFS winner exactly like the game, so mods override. Internalizing is a last resort: delete the copy and read from the VFS wherever a runtime read exists.

This install: `download/4cc_06_gameplay.cpk` overrides all three constant files. Sizes: constant_team stock 1354 B (tag `ff 10 51`) vs mod 1513 B (tag `00 10 01`); constant_match stock 4817 B vs mod 4851 B; constant_player stock 40643 B vs mod 82986 B. Offset-level diffs from a scratch tool (not committed) via the cpk index + `ParseUniformTable`, cross-checked against `docs/re/pes-re-constant-team.md` §5 (445 fields) and `docs/re/pes-re-defender-decisions.md` §12.5.

## 0. Verdict

- Loaders: all constant reads go through the VFS winner at runtime (unchanged). No production loader edited.
- src/ internalized STOCK: ball header's `nonSpinMax 105` comment (§pesball.hpp:188) is stock. The VFS-layout claims (WESYS tags, `kScalarBytes = 0x110`, offsets) describe layout, not values.
- src/ internalized MOD kept-with-label: the defshape transcribed `basePosition.o` block (mod values; no VFS read exists) and its install test are now labelled as mod, with the stock column named as the crib. The movestep loader comment now quotes stock bounds with the mod beside it.
- tests/ fixed to stock relations/fixtures: ball 106→105, defender 2.6/2.5/0.9/2.0→0.5/0.4/0.2/1.0, receive lockouts→0.15/0.25/0.15/0.4/0.5, freemove bounds test→recompute-from-record. Boundary tests that need over-1 thresholds use a local override, labelled as shape probes.
- tests/ left: UNKNOWN/arbitrary fixtures (contact dS/dF pairs, offball `chanceScoreLimit = 60`, teamconcepts `fill(true)`) match neither column; reported, not changed.
- docs/: old/new list in §3 for Main to apply. Nothing in docs/ edited by this branch.

## 1. Commits, tests, mutation

| SHA | Content |
|---|---|
| `35d1318` | RED (VFS `ReadFrom` stub + failing stock test) — superseded by owner priority |
| `a487dbb` | Revert of `35d1318`: branch nets to origin/main; no production loader change |
| `bfcd6a9` | Stock fixes: 3 test fixtures, 2 test rewrites, defshape test + 2 source comments |

Tests (touched binaries, `PES21_ROOT=<workdrive>/PES21`, install present):
- `pes_defender_decisions_test` 22/22 after the fixture swap + boundary retarget; `pes_receive_test` 21/21 after the same treatment.
- `pes_movestep_test` 92/92 (freemove test now recomputes from the winner's record).
- `pes_defensive_shape_test` 25/25; `pes_ball_physics_test` 43/43; `pes_ball_collision_test` 11/11; `pes_ball_track_test` 14/14; `pes_contact_test` 37/37; `pes_offball_test` 62/62; `pes_team_concepts_test` 31/31; `pes_uniform_test` 36/36.
- Mutation: defender stock pins (`0.5/0.4/0.2/1.0`) reverted to mod values → `PesDefenderBlock.LooseBallWindowIsNormalOrSpeedBall` fails (stock-value asserts catch it); restored; 22/22. Run, rebuilt, reverted as reported.

## 2. Audit tables (verified by agent against files + scratch diff)

Key: dt18 field named by the game's own JSON key where the reports name it, else object+blob offset. Stock = dt18_all.cpk, mod = 4cc winner.

### 2.1 src/ transcriptions (decision per site)

| file:line | value | dt18 field | stock | mod | verdict | fix |
|---|---|---|---|---|---|---|
| pesdefshape.cpp:79-111 | -9/-8/-20, 1/-14, -10, 46/42/32, 0.33, 15/19/23, 2.85, 6/5, 0.61/0.34, 6/4/20/2, 35/20, 7.25, 36, 4 | basePosition.o §8 rows | e.g. -8.1, 8/-5, 10/-5, -6, 35/35/30, 0.5, 16/20/24, 2.5, 9/7, 0.55/0.8, (2–6 span, +2), 35/20, 9.25, 10, 10 | the transcribed values | **MOD** (~20 of 32 fields; 115/35/15.12/6-codemix are exe/immediates, not dt18) | kept, labelled (no VFS read exists; delete-for-read is the follow-up) |
| pesdefshape.hpp:43-100 | `bp -9` etc comments | same | per-field stock | per-field mod | **MOD**-quoting comments | kept (they document the transcription); stock crib named in the loader comment |
| movestep_common.cpp:189-191 | `{42.0, 18.0, 69.5}` comment | freemove.o +0x20 | {20.0, 18.0, 67.5} | {42.0, 18.0, 69.5} | was **MOD** | fixed: quotes stock, mod beside it |
| pesball.hpp:188 etc | `105.0 km/h` comments | ball.o scalars | match stock | differ where noted below | **STOCK** for `nonSpinMax 105`; other quoted scalars identical or exe-side | none |

### 2.2 tests/ pins and fixtures

| file:line | value | dt18 field | stock | mod | verdict | fix |
|---|---|---|---|---|---|---|
| pes_synthetic_ball.hpp:36 | nonSpinMax 106.0f | ball.o +0xAC | 105.0 | 106.0 | was **MOD** | → 105.0 + comment |
| pes_defender_decisions_test.cpp:41-42,56-57 | 2.6/2.5/0.9/2.0 | tackle.o +0x0c/+0x10, block.o +0x40/+0x44 | 0.5/0.4/0.2/1.0 | 2.6/2.5/0.9/2.0 | was **MOD** | → stock + provenance comments; boundary tests use explicit local terms |
| pes_receive_test.cpp:21-25 | 0.16/0.26/0.05/0.5/0.75 | trap.o +0x3c0..+0x3d0 | 0.15/0.25/0.15/0.4/0.5 | 0.16/0.26/0.05/0.5/0.75 | was **MOD** | → stock + comments; expectations rebuilt from the fixture |
| pes_movestep_test.cpp:1200-1214 | 42.0/18.0/69.5 pins | freemove.o +0x20 | 20.0/18.0/67.5 | 42.0/18.0/69.5 | was **MOD** | test recomputes from the winner's record; comment gives stock + mod |
| pes_contact_test.cpp:21-32 | dS/dF/vS/vF/disps | contact.o +0x04..+0x44 | 56,…,16/24 etc | 88,…,36/42 etc | **UNKNOWN** (arbitrary, fits neither) | none |
| pes_offball_test.cpp:25 | chanceScoreLimit 60 | spaceRun.chanceScore | 100 | 102 | **UNKNOWN** (not a dt18 scalar) | none |
| pes_team_concepts_test.cpp:23 | enabled.fill(true) | subConcept enables | 0 on 21 ids | 1 on 21 ids | **UNKNOWN** (broader than both) | none |
| pes_ball_physics_test.cpp:503-518 | 96.995/56.9956/0.035, mode-1 row | ball.o | identical | identical | **STOCK** (no diff at those offsets) | none |

### 2.3 Loaders verified clean (runtime VFS reads; offsets and layout only)

pesball.cpp (ball.o scalars + 24 blocks), camera.cpp (cameraInplay.o, identical stock=mod), movestep_common.cpp (freemove by name), pesdefenderdecisions.cpp (tackle/block via the shared object reader), pescontact.cpp, pesreceive.cpp, pesoffball.cpp (selectorVision/spaceRun + chanceScore low byte), pesteamconcepts.cpp (subConcept list). Struct defaults are 0/{}.

## 3. Docs old/new (for Main to apply; nothing edited)

Each row: `doc:line | old fragment | new text`. Stock numbers from the scratch diff and `pes-re-constant-team.md` §5.

1. `docs/re/pes-re-off-ball.md:109` | "`constant_team.bin`, VFS winner 1513 B" | "`constant_team.bin` (mod winner 1513 B; stock 1354 B in `Data/dt18_all.cpk`)" — the offerNum table below (rows L=2..4) is the mod's list; add "(mod; stock: diagonalRun[2,4]=1,2; lineBreak[2]=1; pullAway[1,3,4]=0,1,1; spaceRunChance[2,3,4]=2,3,3)".
2. `docs/re/pes-re-off-ball.md:153-154` | "`chanceScore` 102 / `_good` 88, `checkBallDist_chance` 27.5 / 33.0, `checkLastLineDist_chance` 24 / 30" | "mod `chanceScore` 102 / `_good` 88 (stock 100 / 85); mod `checkBallDist_chance` 27.5 / 33.0 (stock 25 / 28); mod `checkLastLineDist_chance` 24 / 30 (stock 19 / 23)".
5. `docs/re/pes-re-receiving.md:63-65,167-173,181` + `docs/re/pes-re-transitions.md:302-306,348-350` | "lockouts (live): offence 0.05 / defenceKeep 0.26 / defence 0.16 … shoot 0.5 / shootSpeedBall 0.75" | "lockouts (live = 4cc mod): … (stock: offence 0.15 / defenceKeep 0.25 / defence 0.15 / shoot 0.4 / shootSpeedBall 0.5)".
6. `docs/re/pes-re-defender-decisions.md:141,216-219,236-247` | live 2.5/2.6, windows 0.9/2.0, `normal` 0.9, `speedBall` 2.0 (says "live winner = 4cc" already) | add per-value stock: "2.5/2.6 (stock 0.4/0.5), `normal` 0.9 (stock 0.2), `speedBall` 2.0 (stock 1.0)"; §12.5 already marks stock→mod — keep.
7. `docs/re/pes-re-a3helpers.md:185-190` + `docs/ports/pes-fix-BugFix.md:11-16` | "freemove floats 42.0, 18.0, 69.5" | "mod (4cc) freemove floats 42.0, 18.0, 69.5; stock {20.0, 18.0, 67.5}".
8. `docs/re/pes-re-tactics-clock.md:459,464` | "(VFS winner `Data/dt18_all.cpk`)" and "`constant_player.bin` (40643 B) | 36" | "(stock base `Data/dt18_all.cpk`; the runtime winner is the download pack on a modded install)" — the sizes quoted are the stock member sizes; add "(stock sizes; mod winner differs for team/player)".
9. `docs/re/pes-re-contact.md:146-159` | table with stock and "live (4cc)" columns | already correct — keep. Its "code fallbacks 56 / vF 27, vS 10" (line 159) are exe immediates, not dt18.
10. `docs/re/pes-re-ball-constants.md` (§3 table, line ~108: `nonSpinMax` 105.0) | 105.0 | stock — keep, add "(stock; the 4cc mod ships 106.0)".
11. `docs/re/pes-re-setpiece-referee.md:51` | "`constant_team.bin` (1354 B)" | stock size — keep, add "(stock; mod winner 1513 B)".
12. `docs/re/pes-re-constant-team.md:25-29` | stock vs mod inputs | already marks both — keep as the audit's ground truth.
13. `docs/re/pes-re-off-ball.md:126-131` (live cap arrays) | live manager values | already "Live (OBS)" — add "(4cc mod values)".

## 4. Smokes

Baseline (untouched `8fc3292` binary copy, run before any edit): switch-off hash `a3d5e43a3b7b03af`, `--pes-loop` hash `a817a13303958f90`, passes 10-10. (bg_278: OFF_EXIT=0 PES_EXIT=0.)
Post-fix (`bfcd6a9`, rebuilt gfsim_run): switch-off `a3d5e43a3b7b03af`, `--pes-loop` `a817a13303958f90`, passes 10-10, both exit 0 (bg_303). No hash move, as expected: runtime loads the same winner; only tests/comments changed.

## 5. Open items

1. Delete-for-VFS-read of the defshape transcribed block (the "killed hypothesis" in report 11 is itself killed by constant-team §2.3: flat struct = blob+8). Needs a RE read of `FUN_1405f9eb0`-family struct offsets → blob offsets per field, then `LoadDefensiveShapeConstants(exe, vfs)` reading the winner's basePosition.o at runtime. Biggest remaining item.
2. `tests/sim/pes_defensive_shape_test.cpp` Synthetic + install pins then re-resolve against the VFS read (relations, not stock pins).
3. Apply §3 doc edits (Main or docs agent).
4. The AuditDocs scout listed extra sites (uniform-params, injury-fatigue, header-contact-live, officials-ball 311-313 ball.o/cameraInplay size) — spot-check each before applying; not all verified by agent.
5. Two parked peers FirstTouchPort and TeamSliceA touch referee/humanoid/pessequencer loader call lines; this branch did not touch those files.
