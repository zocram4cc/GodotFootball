# CutAnchor2: GF's goal cutscene camera is placed by PES's translated anchor (0x140474F00 + writer wrap + flip 0x140478FF0) on GF state; anchor bit-exact to PES on 7 captured goals; Godot frame taken, PES frame pair: see §5 (29-10-26)

Branch `CutsceneSelect` (worktree `gf-CutsceneSelect`). Pushed heads (each `git ls-remote origin CutsceneSelect`): `12dfeeec` (merge of `origin/main`), `634ef11d` (WIP), `8f488acb` (anchor wiring + tests); final head at the end of §7. Labels: CONFIRMED = tool output this session; INF = inference; OPEN = blocker named. PES captures/frames are PES-derived: scratch only (`gf-CutAnchor2-scratch/`), never committed.

## 0. Status against the ticket

| item | result |
|---|---|
| 1. merge `origin/main`, push | DONE (`12dfeeec`, pushed first). |
| 2. `driver --check --only` on the 4 changed-body functions | 3 of 4 bit-exact, 1 not runnable (§1). |
| 3. GF goal -> anchor inputs -> translated calc -> `set_stage_default` | **DONE** (§2, §3): scene rows kept from `table_3.bin`, `FixDemoSelector::PlaceGoal`, `GfMatch::PlaceGoalScene`, `match.gd`. Three unknown writers: decoded as far as the bridge allows, stand-ins named (§4). Red-first test: 7/7 captured goals bit-exact (§2). |
| 4. frames | Godot goal frames taken from a GPU run (§5); PES frame pair: §5. |
| 5. fast tier | interim line at the end. |

## 1. Driver verdicts (CONFIRMED, `driver.py --image pes_full.bin --spec-dir specs/core --only 0x1409F2820 0x140A6FF50 0x141FF52B0 0x140A7C640`, 147 s)

`--check --only` does not combine (`--check` overwrites `--only` with every recorded-diffed VA, 2253 functions, ~2.5 h), so I ran the same driver over those four VAs with `--only` and no `--check`: same corpora, same comparison. `results_core.json` records three of them as diffed (none has `corpus: bank`).

| VA | verdict |
|---|---|
| `0x1409F2820` | translated, **98 cases bit-exact** (1 skipped), provisional (live-state globals at on-disk values), own-block coverage 0.10 |
| `0x140A6FF50` | **88 cases bit-exact** (3 skipped), coverage 0.01 |
| `0x141FF52B0` | **64 cases bit-exact**, coverage 0.00 |
| `0x140A7C640` | translated, **not diffed: "harness cannot run: corpus empty (0 cases; 64 failed skipped)"**. It has no record in any `results*.json` (only appears as a coverage entry of `results_fixdemo.json`): a closure member "not recorded bit-exact on its own", so a `--check` run never included it and its status is unchanged by the relink. OPEN: a real-call capture of it (it is a callee of the `0x140A7CBA0` scene-row accessor chain) would prove it. |

## 2. What the anchor reads, and the GF arena (CONFIRMED by byte-level read traces)

Tool: `tools/dev/cutscene/anchor_cases.py` (committed; PES-derived output stays in scratch): for each anchor capture it reads the call's inputs from the snapshot and emulates PES's own bytes (`frame_trace.run_recorded`) for PES's answer (calc `*r8` pos, `*r9` rotY; writer capture one call earlier: `scene+0x110..0x120` after the wrap, and the flip byte `+0x120`). I also traced every non-stack read of the calc on `seq1532/3783/2152` with a unicorn read hook (scratch `trace.py`): the reads are exactly MI+0x1588/+0x1590 (and +0x15C8/+0x15CC, the place path), the field info, the side-B team info, the anime/man/run table entries of the scorer's slot, man `+0x55C..` (pos), `+0x574..` (vel), `+0x5B0`, run object `+0x18..`, A0 `+8/+0xC`, DB `+0x448/+0x450`, row `+2/+3/+4` (+ row floats `+0xC`, `+0x2C` for mode 3), the globals `0x1436F3DC8` (G), `0x1436F4FB8` (DB), `0x14351AE54` (fps), and the image's math table (sin quarter-wave `0x142655950` etc.). The writer's closure additionally reads/writes dozens of other objects (`0x140473990` touches the world's tables and writes DemoInfo scratch); GF calls the calc and re-does the writer's two post steps rather than the whole writer (§3).

GF code:
- `src/pes/fixdemo_table.{hpp,cpp}`: the goal table's per-scene row (5 u8, 12 f32, 4 u8) is kept (`FixDemoTable::goalSceneRows`), no longer discarded. PES's DB layout (CONFIRMED against the capture rows): u8s at `+0`, 3 padding bytes (stack garbage in PES's copy), f32s at `+8`, tail at `+0x38`, 0x3C bytes, a `std::vector` at `DB+0x448/+0x450`.
- `src/pes/fixdemo_select.{hpp,cpp}`: `FixDemoSelector::PlaceGoal(GoalAnchorFacts) -> optional<GoalAnchor>`: builds G (match info MI at `G+0x270` with `+0x1588 = 1`, `+0x1590 = slot`; field info `G+0x2AA8`; man and run tables), writes the scorer's facts, calls the translated `pes_fn_140474F00`, applies the writer's wrap (`[0x14259858C]` read from the install) and calls the translated `pes_fn_140478FF0` for the flip. The image tables the math routines read (`0x1426550E0..0x142656160`) are mapped from the install exe (`PeImage::ReadVa`), nothing pinned.
- `src/gdext/gf_match.{hpp,cpp}`: `PlaceGoalScene` builds the facts from GF state on a goal: scorer = `Match::GetLastGoalScorer()`, slot = side*11 + his index in the side, pose through `ToFox`, velocity in m per PES frame (`/ TICK_S / kPesFps`), facing as PES's bearing atan2(dx, dz), pitch halves `pesFieldHalfLength/Width`, fps `pes::offball::kPesFps` (documented there as `[0x14351AE54]`). `take_cutscene()` returns `anchor: {pos, rot_y, flip}` for goals.
- `godot/match/match.gd`: `cam.set_stage_default(anchor.pos, anchor.rot_y, anchor.flip)` on the first playable step, with a log line `goal anchor pos ... rotY ... flip ...`.

Red-first test `tests/pes_goal_anchor_test.cpp` (`pes_goal_anchor_test`, 6 tests, `PES21_ROOT` set): **`EqualsPesOnTheSevenCapturedGoals`**: GF's anchor on the inputs of the 7 captured goals (`pid2621613/seq1532`; `pid132854/seq2152, 2962, 3783, 4195, 4721, 5566`, writer captures one call earlier) equals PES's, **pos (3 floats), rotY and flip bit-exact** (floats compared with `EXPECT_EQ`). The cases span scene rows (1,1,1) and (3,1,0), a push by the run radius, both signs of x and z. It did not compile on `origin/main` (no `PlaceGoal`), and before mapping the math table it failed on 6 of 7 goals (rotY +13.98 deg off, positions up to 12 m off): the translated sin table read was unmapped.

Other tests: `DoesNotDependOnTheScorersSlot`, `FollowsTheScorerAndIsPushedOutByTheRunRadius`, `RotationIsWrappedIntoOneTurn` (a rotation-kind-2 row's float +0x28 pushes past 0/360; relation to the same row without it), `HasNoAnchorWithoutARowOrASlot`, `TheInstallsSceneRowIsTheRowPesHeld` (relation: the install's decoded `table_3.bin` row, laid into the DB, equals the 0x3C bytes PES held for the captured scenes except the padding bytes; skips when the install's rows differ).

Mutations (each applied to a scratch copy of `fixdemo_select.cpp`, rebuilt, run, the repo file never touched):

| mutation | result |
|---|---|
| radius not written | KILLED (`Equals…`, `…RunRadius`) |
| run target not written | KILLED (`Equals…`) |
| wrap removed / negative branch removed | KILLED (`RotationIsWrappedIntoOneTurn`) |
| wrap's `>= 360` arm removed | **SURVIVED**: the calc already wraps values past 360 (probe: row offset 400 gives base+40 before the writer's wrap); the arm is PES's but unreachable on every input I could construct |
| flip inverted | KILLED (7 captures all flip 0) |
| row floats at +4 instead of +8 | KILLED (`Equals…`, `TheInstallsSceneRow…`) |
| table stride 0x20 instead of 0x28 | KILLED (3 tests) |
| DB row-end pointer not written | **SURVIVED** (PES's accessor does not bound the row by it for in-range ids; GF's own guard `HasNoAnchor…` covers out-of-range ids) |
| slot written as 0 | KILLED |
| scene id not written / scorer pos not written / half dims swapped | KILLED |
| fps not written | **SURVIVED** (fps only scales an intermediate rotation that every tried row overwrites) |
| scorer velocity / facing+move word not written | **SURVIVED**: no captured goal, and none of 8 install row kinds with facing, velocity and move-word variants (explore, scratch `explore3.cpp`), changes the anchor with them. They are PES inputs and written; their wiring is unpinned. The earlier CutAnchor note that "rotY = heading of velocity" is wrong for every row tried. |
| scene category check / row bound / slot bound removed | KILLED (`HasNoAnchor…`) |
| math tables unmapped | KILLED |

## 3. GF wiring and what is NOT ported

- Goal poll: `PollCutscenes` -> `PickGoalScene` -> `BeginEvent` -> `Request` -> `PlaceGoalScene` -> `take_cutscene().anchor` -> `PesCutsceneCamera::set_stage_default` (applies to the first playable step). A foul gets no anchor (unchanged).
- Clean cutover: the zero stage default stand-in in `PesCutsceneCamera` is replaced by the translated anchor for goals (`set_stage_default` was already the only entry). Per-step defaults: the emulated scene-start values (`scene_start_oracle.py`) are used only as an oracle, never at runtime. Steps after the first keep the anchor (PES: placement mode 2 walks them along the waypoint map, §4).
- Not ported: the writer's full body (`0x140473990` ...), the placement-mode switch `0x141F5CD00`, the foul swap.

## 4. The three unknown writers: one decode attempt each (CONFIRMED by bridge decompiles; stand-ins named in `gf_match.cpp`)

1. **Run object +0x18 (anchor target T) and the event position `A0+0x30`.** In all 7 captures T equals `A0+0x30` exactly. `A0+0x30` is written by `FUN_140478190` (the goal DemoInfo builder): `param_1[0xc..0xe] = *FUN_140476C50(A0, &out, MI, 0)`, the event-position function that switches on the scene row's first byte (`*FUN_140A7CBA0(DB, sceneId)`: 1 -> `FUN_140476190`, 3 -> `FUN_1404B77D0`, 4 -> stadium record, 5, 6, 7 -> `FUN_140476010`, default -> `FUN_140476190` or `FUN_1404B6ED0` by `A0+0xBC`). `FUN_140476C50` also calls `0x140A489A0` (the run object accessor); the value is the corner-side celebration spot in the captures (`(±49.56, ±32.06)` or `(±49.56, ±31.52)`, `(55.06, 20.9)` for kind 3). Not in the committed layer (closure translation + a layer regen would be needed). Stand-in: T = the goal's event position GF already feeds the FixDemo ctx (the ball), one value for both PES uses; `ponytail:` names `0x140476C50`. Consequence: GF's anchor differs from PES's wherever the ball is not at the corner spot (INF: it is the main remaining difference in framing).
2. **Radius `A0+0xC`.** Written at `0x140478190`: `param_1[3] = FUN_140474CF0(A0, &cutList)` = sum over the scene's cut ids (list from `FUN_140477BC0`) of `FUN_141F5EC60(cut)` for cuts whose kind (via `0x140A7C800(DB, k, cut)`) is 5, 7, 0xB, 0xC or 0xD; scene `0x3013B` takes the constant `[0x14258C264]`; zero when `A0+0x131` is set. The 7 captured radii vary (5.2 .. 28.77), so it is derived, not constant. Stand-in 0 (no push; PES's answer when the scorer is farther than the radius); `ponytail:` names `0x140474CF0`.
3. **`scene+0x60` waypoints.** The map is a `std::map<u32, obj*>` keyed by the step's first cut id; `0x141F5CD00` case 2 only reads it (the value object's pos at `+8..+0x10`, rotY by `0x141EA2F60`) and inserts a null-valued node when the key is absent (`FUN_141F53D00` + `FUN_141F54270`, lines 121/151), so the real fill is elsewhere. Functions that use the same node tag `DAT_142BA5BE0` (candidates for the filler, undecoded): `0x141F5C530`, `0x141F58160`, `0x141F58440`, `0x141F57A00`, `0x141F57FF0`, `0x141F57E60`, `0x141F563E0`, `0x141F54B80`. OPEN. Stand-in: later steps keep the step-1 anchor (`ponytail:` in `match.gd` names `0x141F5CD00` mode 2).
4. Also OPEN: the step's placement mode (`step node +0x10`, selects anchor vs waypoint vs zero) is not in the decoded table step fields; the node is built from the cut row in `0x141F616F0`. GF applies the anchor to the first playable step.


## 5. Frames (CONFIRMED: I looked at both rows; scratch `gf-CutAnchor2-scratch/godot_goal.jpg`, `pair_goal.jpg`)

**Godot** (GPU: `Vulkan 1.4.354 ... AMD Radeon RX 6900 XT (RADV NAVI21)`; `gamescope --backend headless -W 1280 -H 720 -- tools/godot.sh --write-movie ... --fixed-fps 30 --quit-after 4500 res://match/match.tscn -- --home-team 1 --away-team 2 --pes-loop`; the first run without `tools/godot.sh` failed (no project import: `PesLibrary` not found), the wrapper imports first). Goal at movie frame 4201: scene `0x300a4`, layers `goal_celebrate_0302_base`, `..._mayaL0x`, `goal_Effect_h`; log `goal anchor pos (45.69, 0, -9.88) rotY 157.94 flip false`; scene ended at 4.700 s (frame 4341). Frames 4203, 4230, 4260, 4300, 4335:
- 4203, 4230: a low camera (about 1 m up, 3-5 m away) on a group of 5-6 players (green and white kits) in front of the stand; the nearest player is cut at the thighs.
- 4260: the clip tilts up and away: stand and roof, the goal frame at the lower left. 4300, 4335: roof and upper tier; a goal post and net at the bottom edge.
- **Before this change** (CutsceneFrames, same match, same frame 4203): the first shot showed a player from behind facing the empty stand, then empty stand and roof for the rest. So the anchor moved the cut from the pitch origin to the scorer's spot and the camera now frames players at the goal end: the subject changed from nothing to the celebrating group.

**PES**: no goal happened in the live match during my watch (a poller on the ctx event position `[[0x1437F5D50]+0x5208]` of pid 132854, 00:44 to 01:43, read-only `sudo dd` of `/proc/<pid>/mem`, bursts armed for `|x| > 45 && |z| > 20`; zero triggers; the `live2` hooks are capped, so no new capture either). The PES frames I compare against are therefore the earlier live goal burst of CutsceneFrames (`gf-CutsceneFrames-scratch/pairs/pes_goal_row.jpg`, 8 fps, window capture 1284x724, a different goal and a different scene pick, not decoded):
- PES shot 1: the scorer alone at the net, camera head-high 2-3 m in front of him, the net filling the background; shot 2: sky (a tilt up); shot 3: two players close-up with the keeper's legs; shot 4: a tracking mid-shot of the scorer and a mate, camera 3-4 m.
- Camera framing, Godot vs PES: **distance** similar (3-5 m vs 2-4 m), **angle** similar in kind (low, near ground, then a tilt up to sky/roof in both); **subject** differs: PES's first shot is the scorer at the net, Godot's is a group of players at the goal side; the ending shots differ (PES keeps on the players, Godot's clip tilts to the roof by 4.3 s).
- **Not like-for-like, plainly:** different goal and different cut/variant (PES's pick unknown; GF picked `0x300a4`, row 0), different teams, kits, stadium and time of day, no shot sequencing in GF (one clip camera; PES plays several cuts), GF's T and radius are stand-ins (§4), the scorer's spot comes from GF's own sim. So the pair shows that the camera is now at the goal-side players with a plausible low angle; it does **not** show that GF reproduces PES's framing. Not verified: framing equality, any pixel-level claim, and the PES side for this exact goal.

## 6. Tests run (no gate, no sim slot; Main gates)

- `pes_goal_anchor_test` 6/6, `pes_fixdemo_select_test` 6/6, `pes_fixdemo_table_test` 6/6, `pes_fdc_test` 45/45 (`PES21_ROOT` set).
- All 99 `pes_*` binaries (`build/tests`, `build/tests/sim`; every one links `pesfmt` or the sim library holding the changed files), two at a time, `timeout 600` each: **96 rc=0**; 3 rc=124 at the cap: `pes_forfeit_test` (8 cases OK, the 9th running), `pes_restart_delay_test` (2 OK), `pes_throwin_release_test` (2 OK). They are slow multi-minute sim cases under load (the last two timed out the same way in CutsceneFrames); none touches the cutscene, FixDemo or anchor files. OPEN: Main's gate decides; not proven on `origin/main`.
- `gen_translated.py --check`: not run: no file under `src/pes/translated/` or `tools/dev/translate/` changed in this session (the merge of `origin/main` brought docs only: 6 files).
- **Fast-tier interim line:** `GATE_TIER` is not on this tree. `GATE fast (interim): 96/99 pes_* binaries GREEN (3 at the 600 s cap: pes_forfeit_test, pes_restart_delay_test, pes_throwin_release_test, slow sim cases); pes_goal_anchor_test 6/6 GREEN; gen_translated.py --check not run (no translated file changed).`
- `godot_*` tests not run; the `godotfootball` build and a 4500-frame Godot run completed.

## 7. Open items (numbered)

1. **T (event position `A0+0x30`, the run object +0x18)** needs `0x140476C50` and its callees (`0x140476190`, `0x1404B77D0`, `0x1404B6ED0`, `0x140476010`) translated; until then T is the ball. Biggest remaining cause of a different anchor.
2. **Radius** `0x140474CF0` (+ `0x140477BC0`, `0x141F5EC60`, `0x140A7C800`) translated; stand-in 0.
3. **`scene+0x60` waypoint filler and the step placement mode** (`0x141F5CD00` case 2): decoded as far as §4; later steps keep the anchor.
4. The scorer's facing/velocity/move-word wiring is unpinned (no input changes the anchor); a row kind or capture that does would pin it. The writer's `>= 360` arm is unreachable on tried inputs.
5. `0x140A7C640` has no real-call proof (driver: corpus empty); a capture of a call to it would.
6. A PES frame of the same goal as a Godot frame: PES had no goal during 59 min of polling; a PES restart with the goal-pick hooks armed (caps raised) or an owner-triggered goal is needed. Also the anchor hooks' 7 captures hold only row kinds (1,1,1) and (3,1,0); the writer's kind 2/5 arms (`0x14047ECD0`, `0x1404864B0`) and calc rows of other kinds (the install has them: heads 2, 6) have no captures.
7. Foul swap (`kOrdinaryFoulScene` -> `SanctionScene()`) not done: it waits on FoulSanction (not merged: it edits `pesworldrecord.*`).
8. Multi-step, multi-layer playback and the procedural cameras (unchanged from CutsceneFrames).

## 8. Files

`src/pes/fixdemo_table.{hpp,cpp}`, `src/pes/fixdemo_select.{hpp,cpp}`, `src/gdext/gf_match.{hpp,cpp}`, `godot/match/match.gd`, `tests/pes_goal_anchor_test.cpp`, `tests/CMakeLists.txt`, `tools/dev/cutscene/anchor_cases.py`. No `pesworldrecord.*` edit (the anchor builds its own arena in the selector, so FoulSanction's file ownership is untouched).
