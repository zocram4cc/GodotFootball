# PES FixDemo selection on GF memory: the evaluator and matcher run on a GF-owned ctx/DB/statics arena and equal PES's picks for four live-captured events; goal/foul wiring to the Godot camera is in, frames are NOT taken (CutsceneHybrid4, 29-10-26)

Branch `CutsceneSelect`, worktree `<workdrive>/gf-CutsceneSelect`, pushed head `f003c2fd` (`git ls-remote origin CutsceneSelect` = `f003c2fd8481a2387bfe3cf066befc833d42f3eb`). Gate not run (Main gates). Labels: CONFIRMED = tool output or test this session; INF = inference; OPEN = blocker named. Captures are PES-derived (outside the repo): `live2/pid1792161/seq16355,16360` (+ wrapper hook seq16356-16358), `live2/pid1981120/seq8080,8091,8469,9831,9843`.

## 0. Status against the ticket

| item | status |
|---|---|
| 1. merge origin/main, NativeSeam, FdcCamera one at a time; push | DONE (`f093475e`; FdcCamera was already contained). Symlink swapped out for the merges (`rm extern/godot-cpp`, relinked, not committed). |
| 2. manager arena + ctx in PES layout on `PesHeap`, only the evaluator/matcher fields for foul and goal | DONE (§1, §2): `src/pes/fixdemo_select.{hpp,cpp}` (`pes::FixDemoSelector`). |
| 3. run the translated evaluator/matcher on it; oracle = PES's request for captured events; red-first; mutation | DONE for goal (3 events) and foul (1 event): rows and RNG words equal PES (§3). Red: the header/target did not exist on `origin/main`. Six mutants run and killed (§4). |
| 4. wire GF foul stop and goal to `PesCutsceneCamera`; delete GF's goal camera choice | WIRED (§5), compiled only as syntax-checked; **not run in Godot** (godot-cpp first build, see §7). `SeedFor`/`Choose`/`PickCamera` deleted (no callers, grep). |
| 5. Godot frame of a foul and a goal cutscene camera + PES frame | **NOT DONE.** Nothing was rendered. No PES frame taken. |
| 6. fast-tier interim line | see end. |

Acceptance: "GF produces PES's request for captured events (counts)": MET for the captured set (4 events, 8 cut picks, 4 RNG end states). "A GF goal and a GF foul play PES's chosen cutscene camera in Godot (frames)": **NOT MET** (no frame).

## 1. What the capture comparison showed about PES's FixDemo state (CONFIRMED by script over the captures, bridge decompiles)

- **The "manager" is the ctx.** `0x141F4FDA0(mgr, cutId)` is called with `rcx` = the FixDemoEnv ctx (`[0x1437F5D50]`, 0x52E0 bytes); its per-event pick cache is the `std::map<cutId,row>` at ctx+0x30 (the setter `0x141F51630` clears it: `FUN_140a7cfd0(ctx+0x30)`). So one arena object serves as ctx and manager. At the entry of the next ctx-init call the map still holds the previous event's picks: that is how an event's request is read from a capture (`call N+1` entry = event `N`'s picks; `call N` entry = the session state before it).
- **`0x1437F5D58` is not a per-category constant table.** It is eleven rotation words: seeded by `0x141F51FD0(seed)` (all 11, session RNG `0x1437F5D90`) and `0x141F52060(seed)` (words 2..6, match RNG `0x1437F6340`), both called from the ctx constructor `0x141F4C950` with the clock modulo 10000 (`thunk_141B34A30`/`0x141B34AD0`, OPEN which clock), and **incremented by `0x141F4D070(cat)`** when a scene list ends (`0x141F58FC0` at `0x141F58FF8`..: state `+0x1D8 >= 5`, scene not `0x60001` and not `0x40011..0x4001B`). The documented table `{0,6,2,7,...}` was one snapshot of a moving counter. Condition types 0x0D..0x16 read it (`&1`, `%3`, `&3`, `&7`).
- **Random1000 `0x1437F6340`:** seed word `+0x10` (LCG `s = s*0xD2DD4D1D + 0x4513 & 0x7FFFFFFF`), phase word `+0x20`. The condition draw `0x141F7A100` is one LCG step plus `(t + phase + 0xAA) & 0x3FF`; the raw draw `0x141F7A150` is one LCG step only. Verified by script on the captures: event A's two draws reproduce seed `0x75EE2113`, phase `0x1A` exactly.
- **PES draws once more than the picks on the foul** (event D): LCG steps from before to after = 2, one 0x43 pick; the extra step is a raw draw by the scene loader (callers of `0x141F7A150`: `0x141F55110`, `0x141F2DD80`, `0x141F31F00`, `0x141F3D600`, `0x141F50160`, ...; which one and when is OPEN). Order (raw, then pick) is fixed by the captured phase word (`0xE7` only fits raw-then-pick). GF has no such consumer; the oracle test applies that draw (documented in the test).
- **Evaluator switch tables are in PES's code, not in the layer.** The translated `0x141F4D2F0` and `0x141F50450` read `0x141F4F9E0` (byte index), `0x141F4F948`, `0x141F4FAAC`, `0x141F4FC14`, `0x141F508B0`, `0x141F509DC`... through `s.rd(0x140000000 + ...)`. Without them every `0x01` condition answered only `0x100` (my first run: 2 of 8 picks wrong, `0x3014C` row 169 instead of 220). `FixDemoSelector::Create(tables, exe)` maps `[0x141F4F900, +0x1200)` from the install's `PES2021.exe` (bytes equal the unpacked image there, checked). The layer's other users on GF memory (`pesworldrecord`) do not use jump tables, so this was never exercised before.
- **Build hazard found:** a worktree whose object files were copied from another worktree keeps dependency files pointing at that tree's headers; `pes_state.hpp` gained `services`/`heap` in NativeSeam, and stale objects of the other 3,400 layer TUs read a `PesState` of the old layout (segfault in `PesMem::find`). I replaced the layer objects with `gf-NativeSeam`'s (identical sources: `git diff origin/NativeSeam HEAD -- src/pes/translated` is empty; those objects are `-O0`) and deleted the objects of the five other TUs that include `pes_state.hpp`. Anyone reusing objects across worktrees must do this.

## 2. The arena (`src/pes/fixdemo_select.cpp`)

- `PesHeap` + `PesState`: host stack, scratch, the statics window `[0x1437F5000, +0x1400)` (ctx slot, rotation words, both Random1000 objects, per-category pick histories `0x1437F5DC0 + cat*0x40` and ctrl histories `0x1437F6080 + cat*0x40`, each initialised to an empty `std::map` sentinel + empty vectors), the env globals page (`G1s` `0x14351AE30` = 0xFFFF, `G1` `0x14351AE34` = 0, image values), the DB page (`[0x1436F4FB8]` -> a 0x460-byte DB in the arena).
- DB rows in PES layout from `DecodeFixDemoTable`: per category `{u16 start,u16 count}` pairs at `DB+cat*0x20+0x168`, count at `+0x178`, 0x78-byte rows (`u16 key[10]` at +0) at `+0x180`. `FixDemoCutGroup::firstRow` is new (the group's slice start; the history key uses it). The variant pick is a port of the wrapper `0x141F4FDA0` over the **translated** `0x140A7CB40` (DB accessor) and `0x141F4B1D0` (matcher); evaluator, history map (`0x141F4FFF0` -> `0x141F4C6D0`), vector growth and allocator are the translated layer on `PesHeap`.
- `BeginEvent(FixDemoFacts)`: zeroes the ctx and writes only the fields the foul types (0x01, 0x0D, 0x43) and the 21 goal types read: `+0x50/54/58` environment, `+0x5204` subject, `+0x5208/520C/5210` event position, `+0x5214/5218` event player, `+0x5160` goal difference, `+0x51F8` phase, `S(s)+0x2818` players on pitch, plus named stand-ins (next paragraph), `+0x5228 = 2`.
- **ponytail stand-ins** (in `fixdemo_select.cpp`, each names the PES site that removes it): competition `0x5174 = 0x20`, `0x5170 = 0x22`, stage 0, tie context 0, environment `+0x14 = 9` and `+0x20 = 1`, fixture word `0xFFFF/0`, team byte `S+0x2819 = 23` (values of PES's exhibition match in the captures); the pick cache is a GF `std::map` beside the arena (the wrapper's `0x141056DF0`/`0x141056F40` map insert is not in the layer); `EndScene` advances the rotation word but does not age the pick history (`0x141F52D20`/`0x141F52D80` not in the layer: a history node keeps its last range).

## 3. Oracle (CONFIRMED: `pes_fixdemo_select_test` 6/6, `PES21_ROOT` set)

Each case injects the session state PES held before the event (rotation words, RNG seed/phase, history nodes) and the event's facts, asks the cuts in PES's order, and compares every row and the RNG after.

| event | captures (before -> after) | cuts: PES row = GF row | RNG after (seed, phase) |
|---|---|---|---|
| A goal, pickup-ball scene | pid1792161 seq16355 -> 16360 | `0x3014B`:1, `0x3014C`:220 (stadium `0x4A` row), `0x3014D`:0 | `0x75EE2113`, `0x1A` equal |
| B goal, celebration + run | pid1981120 seq8080 -> 8091 | `0x30005`:1, `0x30103`:6 | unchanged, equal |
| D foul, ordinary warning (scene `0x20008`, cut `0x20007`) | pid1981120 seq8469 -> 9831 | `0x20007`:0 | `0x0466C975`, `0xE7` equal (with the loader's raw draw applied, §1) |
| E goal, celebration + run | pid1981120 seq9831 -> 9843 | `0x30037`:0, `0x30104`:9 | unchanged, equal |

Also: scene request (`Request(0x20008)`) names the picked variant's layers; the pick is cached per event and a new event draws again; an unknown group answers nullopt with no draw; seeding is a function of the two seeds and fills the rotation words below 16; `EndScene` advances a category except `0x60001`/`0x40011..0x4001B`. `pes_fixdemo_table_test` 6/6 and `pes_fdc_test` 41/41 pass after the table change (`FixDemoSceneFdcPaths`, a first-variant debug lister, deleted; its test now checks `FixDemoCutDataPath` over every variant of scene `0x20009`).

Not covered by an oracle: PES's foul scene choice (GF uses `0x20008`, §5); goal events other than these three; any 0x03 branch (G1 != 0); cards, penalties, injuries; every condition type outside these cuts (the translated evaluator is bit-exact per earlier work, but the GF ctx fields for types such as 0x02 competition are stand-ins).

## 4. Mutations (each applied to a scratch copy of `fixdemo_select.cpp`, built against the test, run; repo untouched)

M1 stadium word not written -> oracle test FAILED. M2 event X sign flipped -> FAILED. M3 no pick cache -> `APickIsCachedForTheEventAndAFreshEventDrawsAgain` FAILED. M4 history start word 0 -> oracle FAILED. M5 rotation exemptions removed -> `AFinishedSceneAdvances...` FAILED. M6 image tables not mapped -> oracle FAILED. 6/6 killed. Runner: scratch `mut/run.sh`.

## 5. Wiring (compiled by syntax check only; Godot not run)

- `GfMatch` (`src/gdext/gf_match.*`): builds a `FixDemoSelector` at `start()` from the VFS tables (foul, goal) and the library's exe (`PesLibrary::exe()`, new); `set_cutscene_environment(stadium, time, weather)`; `step()` polls: a score increase -> `PickGoalScene` (existing `pesgoalscene`, goal RNG from the match seed) -> `BeginEvent` -> `Request(scene)`; a new referee stop with `FreeKick`/`Penalty` -> `Request(0x20008)`. `take_cutscene()` returns `{scene, category, steps:[{cut,row,fdc}]}`; `cutscene_finished(scene)` -> `EndScene`. I did not edit `referee.cpp` (StoppageFlow owns it): the foul stop is detected from `RefereeBuffer` edges. `// ponytail:` on the foul scene id (sanction tree `0x14047F170`, proven but not in the layer, observed `0x20008` for an ordinary foul: `docs/re/pes-re-sanction-switch.md`), on the goal level/stage gate (`pesgoalscene.hpp`), and on the seeds (PES: clock mod 10000).
- `PesCutsceneCamera::load_layers(library, category, names, camera_index)` replaces `load_scene`; `match.gd` polls `take_cutscene()` each tick, plays the first layer that has the camera, and ends the scene with its clip (`cutscene_finished`). `--fdc-scene` (first-variant debug entry) removed; `--fdc-file` kept.
- Deleted: `GoalCelebration::SeedFor`, `Choose`, `PickCamera` and their declarations.

## 6. Open items (numbered)

1. **Frames (foul and goal) not taken.** `godotfootball` was started (`cmake --build build -j3 --target godotfootball`, log in scratch) and is still compiling godot-cpp (generated classes) under machine load ~35; it had not reached the extension at the time of writing. A later run: `PES21_ROOT=... tools/capture_viewer.sh --scene res://match/match.tscn --frames N -- --home-team 1 --away-team 2 --pes-loop` (a goal/foul prints `cutscene scene 0x... cut ... row ... layers [...]`). Whether the first layer of a picked variant has a camera record at index 0 (the camera binding takes camera 0 of the first layer that has one) is unverified; PES plays every layer's cameras by key time.
2. The foul scene id for cards, penalties, injuries (sanction tree `0x14047F170`: not in the layer).
3. Scene-loader raw draws before the picks (§1): which function, how many per scene kind.
4. History aging at scene end (`0x141F52D20`/`0x141F52D80`) and the wrapper's cache map (`0x141056DF0`/`0x141056F40`): translate them (NativeSeam/XlateLoud's lane), then delete the two ponytail stand-ins.
5. PES's seed clock (`0x141B34A30`/`0x141B34AD0`) and the goal object's RNG construction site.
6. The goal subject side/goal-difference semantics: INF from three captures (`subject` = scoring side, goal difference counted after the goal); the ctx fields are written by `0x14047C500`/`0x140478190`.
7. `gen_translated.py --check` not run (no layer file changed by me; the layer equals `origin/NativeSeam`'s).

## 7. Files

`src/pes/fixdemo_select.{hpp,cpp}` (new), `src/pes/fixdemo_table.{hpp,cpp}` (`firstRow`, `FixDemoCutDataPath`; `FixDemoSceneFdcPaths` removed), `src/gdext/gf_match.{hpp,cpp}`, `src/gdext/pes_cutscene_camera.{hpp,cpp}`, `src/gdext/pes_library.{hpp,cpp}` (`exe()`), `godot/match/match.gd`, `src/sim/onthepitch/goalcelebration.{hpp,cpp}`, `tests/pes_fixdemo_select_test.cpp` (new), `tests/pes_fixdemo_table_test.cpp`, `tests/CMakeLists.txt`, `CMakeLists.txt`. Scratch (not committed): `<workdrive>/gf-CutsceneSelect-scratch/` (`rdw.py` capture reader, `mut/`, `dbg/`, bridge decompiles `dec_*.txt`).

## Tests run (no gate, no sim)

`pes_fixdemo_select_test` 6/6, `pes_fixdemo_table_test` 6/6, `pes_fdc_test` 41/41 (all with `PES21_ROOT`). Not run: other `pes_*` binaries, `godot_*` tests, gfsim smoke, `gen_translated.py --check`.

GATE fast (interim): not run; the three binaries above GREEN; `godotfootball` and the gdext wiring are syntax-checked only (godot-cpp still compiling).
