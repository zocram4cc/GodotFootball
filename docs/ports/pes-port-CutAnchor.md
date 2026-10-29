# CutAnchor: goal scene anchor `0x140474F00` and writer `0x14047D280` translated and proven bit-exact on 14 real calls; scene-start placement decoded and measured on 8 captures; GF wiring, the placement port and frames NOT done (29-10-26)

Branch `CutsceneSelect` (worktree `gf-CutsceneSelect`), pushed head `2f72c061` (`git ls-remote origin CutsceneSelect` = `2f72c0617cbcb1a327e28110878bfe39eb745bd8`), after a merge of `origin/main` (`726b4b82`; the add/add conflict in `pes-port-CutsceneCam.md` took main's archived copy). Labels: CONFIRMED = tool output this session; INF = inference; OPEN = blocker named. Captures are PES-derived and stay in `gf-Rendezvous-scratch/live2/`; scratch: `gf-CutAnchor-scratch/`.

## 0. Status against the ticket

| item | result |
|---|---|
| 1. worktree, merge `origin/main`, push | DONE (`726b4b82`, pushed within the first 20 requests). |
| 2. translate and prove `0x140474F00` + `0x14047D280` on every capture, layer regenerated | **DONE.** Calc `0x140474F00`: 7/7 calls bit-exact; writer `0x14047D280`: 7/7 (§1). Layer regenerated, `--check` clean. |
| 3. `0x141F5CD00` placement-mode switch | **DECODED + MEASURED, NOT PORTED.** An emulation oracle reads PES's own stage default after the switch on 8 captures (§3). The mode-2 map branch's source is OPEN. |
| 4. GF: feed `set_stage_default` from the translated anchor on GF state; foul `0x20008` -> `SanctionScene()` | **NOT DONE** (budget; §4 lists the exact inputs, what GF has and the three stand-ins). The foul swap is not attempted: `origin/FoulSanction` was not merged (it rewrites `pesworldrecord.*`, the file item 4 must edit; do it in one change). |
| 5. frame pair | **NOT DONE.** No Godot build or frame was taken; no claim about framing. |
| 6. fast tier | interim line at the end. |

Acceptance (Godot goal cutscene frames the scorer like PES, frame pair examined): **NOT MET.** Anchor and writer are translated and proven; nothing in GF calls them yet.

## 1. Anchor and writer: translated, proven bit-exact (CONFIRMED)

Specs: `tools/dev/translate/closure.py --spec-set core --tag cutanchor --subsystem cutscene 0x140474F00 0x14047D280` = 175 members (126 already in the layer, 49 new functions incl. `0x140473990`, `0x140478FF0`, `0x14047ECD0`, `0x1404864B0`, `0x14428B510` from `.impdata`), set file `specgen/sets/closure-cutanchor.txt`, 125 new specs in `specs/core/`. I deleted the generated `specs/core/1415A1720.json` (a stand-in callee): with it, `gen_translated.py` aborted with "0x1415A1720 translates differently under core" (2257 core roots saw one body, 3 other roots another); without it the layer builds.

Proof: `tools/dev/cutscene/replay_verdicts.py --hook 0x140474F00 --hook 0x14047D280` (`frame_trace.py recorded` then `snapdiff.py --hooks hooks.cfg --spec-dir specs/core`: PES's bytes in unicorn vs the translated closure over the same snapshot, rax, xmm0 and every written byte compared):

| function | captures | result | own-block coverage (best single run) |
|---|---|---|---|
| calc `0x140474F00` | `pid2621613/seq1532`; `pid132854/seq2152, 2962, 3783, 4195, 4721, 5566` | **7/7 PROVEN**, 0 excluded, 0 mismatches | 82/179 = 0.4581 |
| writer `0x14047D280` | `pid2621613/seq1531`; `pid132854/seq2151, 2961, 3782, 4194, 4720, 5565` | **7/7 PROVEN**, 0 excluded, 0 mismatches | 12/41 = 0.2927 |

Records: `results_snapdiff_cutanchor.json` (both functions, snapshot labels) and two `proof: snapdiff` records in `results_core.json` (the form EndClear3 used; `driver --check` does not re-run them). Layer: `gen_translated.py --check` = `4636 functions (3685 recorded bit-exact, 951 closure) regenerate byte-identical`; held out (unchanged class): `0x1403BB670`, `0x1403C55D0` (callee `0x14037CB50`), `0x140554700`, `0x1405549C0` (callee `0x140555110`), `0x14202BA30` (callee `0x14202C250`). Four existing functions changed body because callees they aborted on are now in the layer (`1409F2820` -> `140470EB0`, `140A6FF50` -> `14159EFF8`, `141FF52B0` -> `1403FB410/1403FB5B0/14184D5C0/141B34AA0`, `140A7C640` -> `14428B510`): their recorded proofs predate the link (OPEN: re-run `driver --check --only` on them).

Coverage gap (OPEN): all 14 calls are kind-1 goals. The writer's kind-2/5 arms (`0x14047ECD0`, `0x1404864B0`) and the calc's mode-0 (`0x1404771F0`), mode-2/3 arms (`0x140A60530`, `0x140A7C9B0/9F0`) are not exercised by any capture. Need goal captures with a scene row whose mode bytes are not (1,1,1); the 12 new captures were all that mode.

## 2. What the anchor reads (CONFIRMED from `pid2621613/seq1532` memory, `rd.py` in scratch; this is the GF input list)

`0x140474F00(A0, arg2, &pos, &rotY)`; A0 = DemoInfo+0x1E8.
- Early-out zero for scene ids `0x3013D..0x3013F`, `0x3013B` and a bit mask of `0x300D7+` (`0x2400001`).
- Scene row: `0x140A7CBA0(DB, sceneId)` = `DB[0x448] + (id & 0xFFFF - 1) * 0x3C`; bytes `+2` x-mode, `+3` z-mode, `+4` rot-mode, float `+0x28`. Capture row: `00 00 01 01 01 ...` (modes 1,1,1, `+0x28` = 0). **The row is in the install's `table_<cat>.bin`**: the goal category's per-scene 0x3C rows (5 u8, 12 f32, 4 u8), which `src/pes/fixdemo_table.cpp:178-182` reads and discards. GF can keep them and lay them into the arena DB (`DB+0x448/0x450`, singleton `0x1436F4FB8`).
- Scorer: `MI+0x1588 == 1` ? slot `MI+0x1590` : `0x140A62EA0(.., MI+0x15C8)` (side lo dword, squad place hi dword through the side-B team info `0x140A3B320`: slot = side*11 + index of the place in the info's map). Capture: side 1, place 9 -> slot 21. Position `man+0x55C/0x564` = (-48.88, 16.32), velocity `+0x574..0x57C` (rotY = heading of `0x140A6C6D0` = velocity, or facing when action `+0x5B0 == 1`, times `[0x14259BDD0]/fps` = 0.6944444 / 54).
- Field info `G+0x2AA8` (52.56, 34.06). `G+0x2C0 -> +0x20` stadium id (162; only mode 0 reads the stadium record).
- **Target T = `*(G+0x21E8 + slot*0x28) + 0x18`** (`0x140A489A0`, 33 callers): the per-slot "run" object GF names `reserved_run` (`pesworldrecord.hpp`). Capture T = (-49.56, 0, 32.06) = DemoInfo `A0+0x30..0x38` (the event position written by the DemoInfo builder). Writer of the run object's `+0x18`: OPEN (not `0x140476C50`; its kind-3 branch `0x1404B77D0` starts from `(halfL + [0x14259CE3C]=7.5) * sign`, i.e. before later steps; my CutsceneCam §2 formula for this spot is NOT verified here).
- **Radius `A0+0xC` = 13.546170** (dist < radius pushes the anchor `radius - dist` away from T along the bearing). Writer OPEN: no store to `[reg+0x1F4]` or `[reg+0xC]` in `0x140470000..0x1404C0000` (capstone sweep of `pes_full.bin`, only the vec-copy at `0x140470645` matches), so a struct copy or a different base.
- Mode 1 path (the only one captured): `bearing(pos -> T)` `0x140439EF0`, `0x140470EB0` push, `0x140A8F7C0` pull inside the pitch by `[0x1425656C0]`, rotY from `0x140474C40` and `0x140470D00/D60`. Output pos y = 0.
- Writer `0x14047D280` (kind 1): `0x140473990`, calc, wrap rotY into [0,360) with `[0x14259858C]` = 360, store `scene+0x110..0x11C`, mirror to `DemoInfo+0x2940..`, flip byte `scene+0x120` from `0x140478FF0`.

## 3. Scene start `0x141F5CD00` placement switch (CONFIRMED by emulation of PES's bytes; NOT ported)

`tools/dev/cutscene/scene_start_oracle.py` (committed; output PES-derived, kept in scratch) runs `frame_trace.run_recorded` with the import models to the switch's join `0x141F5D211` (after the last `+0xAD0` store) and reads the player's stage default (`player+0xAC0..0xAD0`, player = `[scene+0xE0]`) and the scene anchor (`scene+0x110..0x120`):

| capture | stage default pos / rotY / flip | scene anchor |
|---|---|---|
| `pid2580963/seq042` | 0,0,0 / 0 / 0 (mode 0) | zero |
| `pid2580963/seq1311` (goal A step) | (-39.48, 0, -14.07) / 210.02 / 0 = the anchor (mode 1) | same |
| `pid2580963/seq1313` | (-46.69, 0, -26.89) / 210.02 / 0 (mode 2 map branch) | (-39.48, 0, -14.07) / 210.02 |
| `pid2580963/seq1315` | zero | same anchor |
| `pid132854/seq8454` | (34.81, 0, 5.98) / 30.02 / 0 = anchor | (34.81, 0, 5.98) / 30.02 |
| `pid132854/seq8456` | (42.94, 0, 19.90) / 30.02 / 0 (mode 2) | (34.81, 0, 5.98) |
| `pid132854/seq8461` | zero | same |
| `pid132854/seq8463` | (46.39, 0, 28.25) / **17.19** / 0 (mode 2, rotY from `0x141EA2F60`) | same |
| `pid132854/seq8470` (second goal) | (42.94, 0, 19.90) / 30.02 = its anchor | (42.94, 0, 19.90) / 30.02 |

The first two goal-A rows reproduce the live poller values of `pes-port-CutsceneCam.md` §3 (-39.48/-14.07 then -46.69/-26.89), so the emulated switch and the live game agree. Reading: mode 1 copies the anchor; mode 2 later steps walk the stage default along waypoints toward the corner (x from 34.81 to 42.94 to 46.39); the waypoint comes from the std::map at `scene+0x60` keyed by the step's first Cut id (`+0x1D0` list head `+0x1C` short), value `+8..+0x10`, rotY by `0x141EA2F60`. **OPEN: who fills `scene+0x60` (the waypoint map).** Why not ported: the placement switch is eight lines but its mode-2 arm needs that map; the whole function (virtual calls `[vt+0x78/0xB0/0x90]`, RenderPlugin `0x14195BD10`) is outside the harness (the full replay stops on `memmove`/`tanf` import models; the closure is 1283 members). The oracle gives the expected values for a future port: modes 0/3/4 give flip only (3: x<0, 4: x>0), 1 and 5 copy anchor / node fields.

## 4. GF wiring: what is missing (numbered, for the next run)

1. Merge `origin/FoulSanction` first (its `PesWorld` already has MI at `G+0x270`, field info `G+0x2AA8`, side-B team info with the place map, the world window and `NewCall` pattern the anchor needs); regenerate the layer after the merge.
2. Keep the goal scene rows in `FixDemoTable` (`fixdemo_table.cpp:178`) and write them into the arena DB at `DB+0x448/0x450` (stride 0x3C) so the translated `0x140A7CBA0` finds them.
3. Add an anchor image to the window: A0 (`+8` scene id, `+0xC` radius, `+0x30` event position), MI `+0x1588/+0x1590` (set `+0x1588 = 1`, `+0x1590 = scorer slot`), the scorer's man record (GF writes pos/vel/facing each tick: `teamAIcontroller.cpp:3418-3425`), the `G+0x21E8` run object for the scorer slot (`+0x18` = T), fps global `0x14351AE54`.
4. Call `pes_fn_140474F00` (then the writer's wrap `[0x14259858C]` and flip `0x140478FF0`) in `GfMatch::PollCutscenes` on a goal and pass pos/rotY/flip to `PesCutsceneCamera::set_stage_default` (godot side: `match.gd:_poll_cutscene`, `cam.set_stage_default(...)` before the first `update`).
5. **Three stand-ins GF needs, each with a named `ponytail:`**: T (the run object `+0x18` = DemoInfo `A0+0x30`, writer OPEN above), radius `A0+0xC` (writer OPEN; radius 0 never pushes, which is PES's behaviour whenever the scorer is farther than the radius from T: 15.77 m vs 13.55 in the capture), and the step-2+ waypoints (`scene+0x60` map, OPEN) so later steps keep the step-1 anchor.
6. Foul: replace `kOrdinaryFoulScene = 0x20008` (`gf_match.cpp:209`) with `match_->GetReferee()->SanctionScene()`; skip the cutscene when it returns `0x20022` (the 3-line change, per FoulSanction's report §3).
7. Frames: GF build + `tools/capture_viewer.sh`/movie, PES frames from the live window (decode checks in the brief); compare the camera framing of the scorer's celebration.
8. More captures: goal captures with a scene row whose mode bytes differ from (1,1,1) (calc modes 0/2/3, writer kinds 2 and 5), and `scene+0x60` map content (a hook on the map fill or a SIGSTOP read during a goal).

## 5. Files changed

`tools/dev/translate/results_core.json` (+2 records), `results_snapdiff_cutanchor.json` (new), `tools/dev/diffharness/specs/core/*.json` (+125 specs), `specgen/sets/closure-cutanchor.txt`, `tools/dev/ledger/universe.json`, `src/pes/translated/**` (+48 function files, 4 changed bodies, `translated.hpp`, `pes_dispatch_gen.hpp`), `tools/dev/cutscene/scene_start_oracle.py` (new). No GF engine or Godot code changed.

## 6. Tests run

- `replay_verdicts.py` anchor: 14 captures, 14 PROVEN (above). No mutation applies (no GF code written); the snapdiff comparator is the oracle.
- `gen_translated.py --image pes_full.bin --check`: 4636 functions regenerate byte-identical (11 m 37 s).
- `pestranslated` and `pesfmt` build (Release); `pes_fdc_test` 45/45, `pes_fixdemo_select_test` 6/6 (`PES21_ROOT` set). Not built: `godotfootball`, the other `pes_*` binaries (the layer is one static library; its additions are new functions no existing caller references except the four relinked bodies).
- `GATE_TIER` not on main: **interim fast-tier line: `pestranslated` builds; `pes_fdc_test` 45/45 and `pes_fixdemo_select_test` 6/6 GREEN; `gen_translated.py --check` GREEN; the remaining `pes_*` binaries not run (not rebuilt in this worktree: a Release build of the layer alone took 46 minutes under fleet load).** Main's gate decides.
