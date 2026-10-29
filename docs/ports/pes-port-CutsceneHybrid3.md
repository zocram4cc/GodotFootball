# PES FixDemo selection in GF, part 3: merges done, goal scene pick ported, hybrid loop NOT closed (allocator and ctx blockers named) (CutsceneHybrid3, 29-10-26)

Branch `CutsceneSelect`, worktree `<workdrive>/gf-CutsceneSelect`, pushed head `dc5375e1` (`git ls-remote origin CutsceneSelect` = `dc5375e1`). Gate not run (Main gates). Labels: CONFIRMED = tool output or test result this session; INF = inference; OPEN = blocker named. No PES live read was made (PES was restarted by Main for new hooks; no new captures were read).

## 0. Status against the ticket

| item | status |
|---|---|
| 1. merge origin/main, SpecGen, ImportModel, FdcCamera one at a time; push | DONE (`a6aad88b`, `c501020b`, `de4db989`, fix `a72f035c`). Symlink swapped for an empty dir during the merges and restored (not committed). One conflict: `tools/dev/diffharness/check_closure_stubs.py` (SpecGen's `C.reach_stubs` vs ImportModel's two-hook `CASES`): resolved by combining (both hooks, `reach_stubs`); the check prints GREEN for 0x140A39A80 and 0x14044C880 (CONFIRMED). |
| 2. re-diff caller `0x141F41CE0` and wrapper `0x141F4FDA0` | Caller: replayed on the merged harness, 2 of 12 captures replay, both bit-exact; 10 excluded; NOT added to the layer (§1). Wrapper: no captures exist; blocker named (§1). |
| 3. translate/prove ctx setter `0x141F51630`; arena + ctx in PES layout; GF request for goal and foul stop | NOT DONE. Blockers named (§2, §3). |
| 4. wire request to `PesCutsceneCamera`; delete GF's goal camera choice | NOT DONE. Part done: the goal scene pick (§4). `goalcelebration.cpp` `SeedFor`/`Choose`/`PickCamera` untouched: nothing replaces the variant choice yet. (They have no callers in `src`, `apps`, `tests` or `godot` (grep): dead code, not a live camera path.) |
| 5. red-first, mutation, tests, counts | For the goal scene pick only (§4, §5). |

Acceptance (GF produces PES's request for captured events; a GF goal plays PES's chosen cutscene camera in Godot, frame): **NOT MET**. No frame was taken.

## 1. Caller and wrapper on the merged harness

**Caller `0x141F41CE0` (CONFIRMED, `snapdiff.py` via `tools/dev/cutscene/replay_verdicts.py`, results `tools/dev/cutscene/verdict_141F41CE0_merged.json`):** 12 captures hold calls; **2 replayed, 0 mismatches, 10 excluded**, own-block coverage 0.6875 (22/32), `proven: False`. The two replayed captures are the full ones (`pid1467004/seq007`, `pid1694737/seq009`). The 10 excluded are the old `live/seq*` captures: `case excluded: page not captured` (they were slimmed since the 18/18 run of CutsceneSelect: pages the replay reads were dropped; INF from the reason text, the verdict file keys the reason as `case excluded`). The closure note: `call 141F500D0: global store ... at 141F50116; callee 1403FB410: indirect call qword ptr [rax + 0x10] at 1403FB47E / 0x1403FB49F`: the harness excludes any case that enters the allocator. Under the owner's correctness rule (every replayed case bit-exact) the caller holds on 2 real calls now and held on 18 earlier; **not added** to the committed layer because (a) `proven` is False under the harness's own "no excluded call" rule and (b) the request path does not need it (it is the cut-function dispatcher of playback, which Godot replaces). `results_fixdemo.json` still lists it as not diffed.

**Wrapper `0x141F4FDA0` (OPEN):**
- It has no rendezvous hook, so no real call exists to replay. Main added `fixdemo_wrapper_141F4FDA0`, `fixdemo_ctx_init_141F514D0` and `goal_pick_140477400` (FIRST 1, EVERY 1) at the PES restart in progress; captures land in `live2/pid<new>/`. Main REFUSED a hook at `0x141F51630`: a `je` at `0x141F51633` lies inside the patch window (CONFIRMED by Main's planner). Hookable alternatives: `0x141F514D0` (ctx init, calls the setter with the world's pointers).
- The unicorn corpus cannot prove it: its closure includes the std::map node allocation (`141056DF0` -> `140EBF370` -> `1403FB410`), and the corpus keeps image globals at on-disk values (the allocator table global `0x1436EF2C8`, the singletons `0x1437F5D50`, `0x1436F4FB8`). Results: `results_fixdemo.json` (`unmodellable callee`).
- **ImportModel4's question ("is the allocator a blocker?") answered:** the harness and unicorn do read the vtable target at call time, so on a snapshot the allocator runs as PES bytes; the blocker is on the GF side. The vtable targets are not in the committed layer (CONFIRMED by grep of `pes_dispatch_gen.hpp`: `0x141B34CB0`, `0x1403FBAC0`, `0x1403FBCB0`, `0x1403FBC10`, `0x1403FB600` absent; `0x1403FB410`, `0x1403FB5B0`, `0x1403FB860`, `0x141F533D0` present). Live objects (snapshot `pid1694737/seq009`, read by script through `SnapMem`): allocator table `[0x1436EF2C8]` = `0x1436EF300`; entry id 0x19 (the ctx/table-manager id) -> object `0x1437F0110`, vtable `0x142AC0090`, slot +0x10 = `0x141B34CB0` (decompile: `FUN_1403fb600(this+8)` then a tail call through another vtable's +0x10); entries 1 and 2 -> object `0x1436EFF80`, vtable `0x14258E390`, slot +0x10 = `0x1403FBC10`, a tracking wrapper (`param_1+8` inner heap through `0x1403FDB10`, size classes, a list at `+0x50`). A real heap sits behind both. GF cannot run it without translating that heap (with OS imports).

## 2. Why the matcher cannot run in GF yet (CONFIRMED by decompile)

`FUN_141f4d2f0` (evaluator) pushes onto a `std::vector<u32>` (`FUN_14047be40`, growth `FUN_141f533d0`) and the matcher's 0x43 path grows two more vectors; every growth calls `FUN_1403fb410`. The matcher keeps its vector locals zero-initialised, so the allocator is reached on **every** call, not only on the 0x43 path. The committed matcher/evaluator are bit-exact (unicorn corpus and 4/4 live replays) but only because the replay builds translate the allocator targets for the run; the committed layer does not.

**Seam needed (not built):** an allocator the translated code can call. The dispatch table is generated and `pes_dispatch_lookup` is inline in `pes_dispatch_gen.hpp`, so a GF-native vtable target needs a one-line change to `gen_dispatch.py` (`default: return pes_dispatch_native(t)`) plus a declaration in `pes_dispatch.hpp`: that recompiles all ~4,000 layer TUs and was not attempted at this budget. Rejected: overriding `pes_dispatch_missing` (ODR), or translating the PES heap (OS imports, a tracking allocator chain).

## 3. The ctx fields the request needs (new, from the install tables; CONFIRMED by script over `scene_map.json`)

Condition types in the variant keys (count of key columns): **foul (cat 2): 0x01 (308), 0x0d (16), 0x43 (269) only.** 0x01 reads `ctx+0x50` (stadium id), 0x0d depends on the category only (`tblCat` at `0x1437F5D58`), 0x43 is the RNG draw (`0x1437F6340`, `ctx+0x5228 == 2`). **Goal (cat 3): 21 types**: 0x01, 0x02, 0x03, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0d, 0x0e, 0x0f, 0x13, 0x15, 0x1c, 0x1d, 0x20, 0x22, 0x40, 0x43, 0x45, 0x52 (these ids are `value >> 8` as decimal in the script output: 1,2,3,6,7,8,9,10,13,14,15,19,21,28,29,32,34,64,67,69,82; the hex above is the same set). Several read ctx fields the setter fills from the world (teams, score, scorer; Appendix A of `pes-re-cutscenes.md`). The foul request is therefore small (stadium, category, RNG) once the allocator exists; the goal request needs most of `FUN_141f51630`.

The setter `0x141F51630` (decompile this session): a jump-table function over nine pointer arguments (`world+0x2c0`, `+0x270`, `+0x180`, `+0x2ad0`, and four side/slot pointers from `0x140A48A30` / `0x140A489D0`), about 35 callees (environment copy `FUN_1414c07b0`, per-team blocks `setup+0xc400+s*0x2804`, `FUN_141f52ad0`, `FUN_1414c3960`, ...), phase switch on `MI+0x1168` into `ctx+0x51f8`. Not translated: no hook (Main refused `0x141F51633`), and the world records it reads are only partly GF's. `FUN_141f514d0` sets `ctx+0x5228 = 2` and calls it.

## 4. Done: PES's goal scene pick (`src/sim/onthepitch/pesgoalscene.{hpp,cpp}`)

`GoalScenePool` / `PickGoalScene` port `FUN_140477400` (`0x140477400`, decompile this session): pool = goal scenes whose **first step row** has flag bit 6 (`FUN_140a7cce0(.., 3, 6)`, row `+0xc`), then the level bit (level 0 -> 11, 1 -> 12, 2 -> 13; `FUN_140a7ce90`), then for levels 1 and 2 with the stage gate on, bit 16 (level 0 jumps past the gate); empty scenes (`list entry +2 == 0`) are excluded; draw `Random::Range0(pool size)` (`0x140a92530`, slot 1, existing `pesrandom.hpp`); id = `(3 << 16) | (index + 1)`; an empty pool is PES's trap (`nullopt`). Inputs the builder takes from the goal context are `ponytail:` stand-ins (level = `param_1[0]`, stage gate = `DAT_143446058` and the not-in-mode flag of vtable `+0x88` of `0x1434A2D40`); the PES site that removes them: the writer of `param_1[0]` of `0x140477400` (its callers). The bit numbers are immediates of the calls in the decompile; they are **not** opcode-checked in code (INF, owner rule asks `PeImage::ReadVa`): OPEN. The seeding (match triple t, t+1, t+2 into the goal object, `FUN_14047ab90`) stays with GF's `Random` setup: not wired.

Oracle for it: none from PES yet; the new hook `goal_pick_140477400` will give real calls (pool inputs and result) once a goal is scored in the bot match.

## 5. Tests (CONFIRMED)

- `tests/sim/pes_goal_scene_test.cpp` (target `pes_goal_scene_test`, `PES21_ROOT` set): **5/5 PASSED**. Cases: pool bit then level bit in table order (and empty scenes); first step row decides; stage gate for levels 1 and 2 only; `PickGoalScene` equals `pool[Range0(n)]` on a second `Random` with the same seed and leaves slot 2 untouched (20 seeds); empty pool -> `nullopt`. Red: the header and target did not exist on `origin/main` (the test did not compile/link).
- Mutations (each applied, rebuilt, run, reverted; the reverted file hash equals the committed one): **A** stage gate also on level 0 -> `StageGateKeepsBit16OnlyForLevelsOneAndTwo` FAILED. **B** level bits `{12, 11, 13}` -> four tests FAILED. Not mutated: the pool-bit constant, the id numbering, the draw count.
- Merged tree: `pes_fixdemo_table_test` 6/6 PASSED, `pes_fdc_test` 41/41 PASSED (after `cmake -S . -B build`). Build note: a fresh worktree compiles 3,987 layer TUs; I reused the object files of `gf-FdcCamera`'s build for byte-identical sources (compiled `-O2 -g`; flags differ from this tree's `-O3`: no fast-math in either).
- Not run: gate, any sim, any Godot run (no frame).

## 6. Open items (numbered)

1. **Allocator seam** (§2): a GF-native vtable target in the dispatch (generator change, full layer recompile), or translate PES's heap. Blocks the matcher, evaluator and wrapper in GF.
2. **Wrapper proof**: needs the `fixdemo_wrapper_141F4FDA0` captures (hook live after Main's restart); a cache-miss call enters the allocator and is excluded unless item 1 is solved on the harness side too. The per-manager cache `std::map` at `+0x30` stays PES layout only once the wrapper runs in GF; until then no GF stand-in was written (the ticket forbids an MSVC map shim).
3. **Ctx setter `0x141F51630`** (§3): translate from `fixdemo_ctx_init_141F514D0` captures; the foul request needs only `ctx+0x50`, `ctx+0x5228`, the category table and the RNG; the goal request needs the world-fed fields.
4. **Goal wiring**: GF goal event -> `PickGoalScene` -> `PesCutsceneCamera::load_scene` (it takes the first `.fdc` layer with the requested camera; PES chooses a variant through the matcher, so this camera choice is not PES's until items 1-3 land). Then delete `SeedFor`/`Choose`/`PickCamera`.
5. Opcode-check the six bit immediates of `0x140477400` through `PeImage::ReadVa`.
6. Foul scene id: the sanction tree `0x14047F170` is snapdiff-proven (14/14, ImportModel4) but not in the committed layer (no `pes_fn_14047F170`); GF's foul state does not yet feed its inputs.
7. Carried: 15 type-99 `.fdc` files, 21 dangling `mode` `.fdc` names, ctx fields of unknown meaning (Appendix A), non-zero cut offset never observed (FdcCamera5).

## 7. Files (branch `CutsceneSelect`)

`src/sim/onthepitch/pesgoalscene.hpp`, `.cpp`; `tests/sim/pes_goal_scene_test.cpp`, `tests/sim/CMakeLists.txt`; `tools/dev/cutscene/verdict_141F41CE0_merged.json`; `tools/dev/diffharness/check_closure_stubs.py` (merge fixup). Scratch (not committed): `<workdrive>/gf-CutsceneSelect-scratch/` (`replay3/`, `dec3a..f.txt` decompiles, `build1.log`).
