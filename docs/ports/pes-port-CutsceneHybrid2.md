# PES FixDemo selection in GF, part 2: chain partly committed, table decoder shipped, GF request NOT built (CutsceneHybrid2, 28-10-26)

Branch `CutsceneSelect`, worktree `<workdrive>/gf-CutsceneSelect`. Pushed `96834d0a` (`git ls-remote origin CutsceneSelect` = `96834d0a`), after the merge `7a5fbce5` (`origin/main` 863b72f8). Gate not run (Main gates). PES was not touched (no live reads in this pass). Labels: CONFIRMED = tool output or test result; INF = inference, reason given; OPEN = blocker named.

## 0. Status against the ticket

| item | status |
|---|---|
| 1. merge `origin/main`, push | DONE. `7a5fbce5`. Symlink swapped for an empty dir for the merge, then restored (tasks/27-10-26.md). |
| 2. commit the FixDemo chain with `gen_translated.py`; `--check` green | PARTLY. Matcher `141F4B1D0`, evaluator `141F4D2F0` and two callees (`141F500D0`, `140A7CC00`) are diffed and committed (36 closure members). Caller `141F41CE0` and wrapper `141F4FDA0` are NOT diffed (OPEN, §2.3). `--check` green: 3987 functions regenerate byte-identical (CONFIRMED). |
| 3. GF glue: C++ decoder of `table_<cat>.bin` + FixDemo arena in PES layout | DECODER DONE (`src/pes/fixdemo_table.{hpp,cpp}`, tests green). ARENA NOT BUILT (§4). |
| 4. oracle: GF request == PES request on captured events | NOT MET. 0 of 6 sanction captures produced by GF (§4). No red test for the request exists, because there is no GF request to test. |
| 5. replace goal-celebration choice (`goalcelebration.cpp`) | NOT DONE. Gap named (§5). `SeedFor`, `Choose`, `PickCamera` untouched. |
| 6. run test binaries | `pes_fixdemo_table_test` 5/5 (CONFIRMED). gfsim-linked binaries not built (§7). |

## 1. Merge

`git merge origin/main` clean, `7a5fbce5`. Pushed. The brief's symlink trap was hit on `git status`/`merge`: `extern/godot-cpp` is a symlink into the main checkout; git refuses it. Swapped for an empty dir during the merge, restored after. The symlink is still in place and is NOT committed (the index has a `T` entry for it; never stage it).

## 2. Translated chain (item 2)

### 2.1 Method (CONFIRMED)

- Specs generated from the image by `specgen generate --mode impure` on `tools/dev/diffharness/specgen/sets/fixdemo.txt` (6 VAs; committed). Specs are in `tools/dev/diffharness/specs/fixdemo/` (6 files, VAs and layout only, no PES bytes).
- Driver run: `driver.py --spec-dir specs/fixdemo --record tools/dev/translate/results_fixdemo.json --set ...` (committed as `results_fixdemo.json`). Harness: unicorn, the unpacked image `pes_full.bin`, `GF_DIFF_PYTHON` = `<workdrive>/gf-DiffHarnessRE-scratch/venv/bin/python`.
- `gen_translated.py`: new set `("results_fixdemo.json", "fixdemo")` added to `SETS`.

### 2.2 Results (CONFIRMED, driver output)

| VA | role | translated | diffed | cases | note |
|---|---|---|---|---|---|
| `141F4B1D0` | FixDemo variant matcher | yes | **yes** | 88 | bit-exact; 40 cases failed and were skipped; own-block coverage 0.11 |
| `141F4D2F0` | FixDemo condition evaluator | yes | **yes** | 128 | bit-exact; own-block coverage 0.02 |
| `141F500D0` | ctx singleton accessor (callee of 141F41CE0) | yes | **yes** | 68 | bit-exact; coverage 0.29 |
| `140A7CC00` | callee of 141F4FDA0 | yes | **yes** | 68 | bit-exact; coverage 0.29 |
| `141F41CE0` | FixDemo caller | no | no | 64 | unmodellable callee (below) |
| `141F4FDA0` | variant-pick wrapper (`cutRef` -> row) | no | no | 64 | unmodellable callee (below) |

All four diffed records are marked "provisional (live-state globals at on-disk values)" by the driver: their globals were read at on-disk values, not runtime values. The earlier snapshot replay (CutsceneSelect report, §1) is the stronger proof on live calls: matcher 4/4, evaluator 4/4 PROVEN. The committed records rest on the unicorn corpus.

### 2.3 Why 141F41CE0 and 141F4FDA0 are not committed (CONFIRMED)

Both reach `1403FB410` (through the closure), which has two indirect calls through an object vtable: `call qword ptr [rax+0x10]` at `0x1403FB47E` and at `0x1403FB49F`. The harness has no model of the vtable target, so the closure is unmodellable. `141F41CE0` also writes a global at `0x141F50116` (inside the callee `141F500D0`), which is now diffed on its own. OPEN: the vtable target at `0x1403FB47E` and `0x1403FB49F`; needs a harness model of the object's vtable slot (ImportModel/harness owner).

### 2.4 Layer (CONFIRMED)

- `gen_translated --check`: `3987 functions (3676 recorded bit-exact, 311 closure) regenerate byte-identical`, rc 0.
- New committed files: 36 under `src/pes/translated/` (closure members of the four diffed records). `translated.hpp` and `pes_dispatch_gen.hpp` extended. No existing `pes_fn_*.cpp` changed (`git diff --name-status` shows only the two headers as `M` under the layer).
- Held out by the generator (pre-existing, not from this change): `0x140554700`, `0x1405549C0` (closure reaches mistranslated `0x140555110`), `0x14202BA30` (reaches `0x14202C250`), and `0x1403BB670`, `0x1403C55D0` (reach `0x14037CB50`). Unchanged by this branch.

## 3. Table decoder (item 3, part a) — DONE

- `src/pes/fixdemo_table.hpp` / `.cpp` (in `pesfmt`): `DecodeFixDemoTable(bytes, category, &err)` decodes `table_<cat>.bin` per `docs/re/pes-re-cutscenes.md` §2.1–2.2: the 14-word header, column streams A (f32), B (u32), W (u16), U (u8), strings; version 0/1; cut groups (T1), scenes (T2), goal extras (edit scenes, playerId→scene pairs). T3 control rows are consumed to keep the read count exact and are not kept (OPEN: no consumer). Scene id = `(cat<<16)|(index+1)`; cut group id likewise. `FixDemoTablePath(cat)` gives `common/demo/fixdemo/<cat>/table_<cat>.bin`.
- Consumption rule (same as `scene_map.py`): every stream read to its header count, and the section-end read counter must equal the header value, or decode fails.
- Read via `PesVfs` (the mounted VFS), nothing PES-derived committed.
- Port of `tools/dev/cutscene/scene_map.py`'s `decode`, not yet cross-checked row-for-row against it (OPEN: a dump comparison of counts). Install gate below decodes all nine tables exactly.

### 3.1 Tests (CONFIRMED)

`tests/pes_fixdemo_table_test.cpp`, target `pes_fixdemo_table_test` (in `tests/CMakeLists.txt`):
- `DecodesVersionOneGroupsScenesAndSteps`: synthetic container (one group, one variant, one scene, one step); checks ids, keys, layers, b0/f/b1, kind, fade, cut, w0, w1, w2.
- `RejectsSectionEndThatIsNotTheReadTotal`: section end one short of the true read total (28).
- `RejectsAStreamThatIsNotFullyConsumed`: U-stream header count 7 against 6 bytes read.
- `RejectsTruncatedFile`.
- `InstallTablesDecodeExactlyAndCutsStayInCategory` (`PES21_ROOT`): all nine categories decode through the VFS; every scene id is in its category; every step cut is 0 or in its category. Relations only, no pinned counts.

Run: `PES21_ROOT=<workdrive>/PES21 ./build/tests/pes_fixdemo_table_test` → 5/5 PASSED (CONFIRMED).

Mutation (ran, rebuilt, reverted; CONFIRMED):
- M1: step w2 read gated on `version == 0` instead of `version == 1` (`src/pes/fixdemo_table.cpp`). Result: `DecodesVersionOneGroupsScenesAndSteps` FAILED (line 76) and `InstallTablesDecodeExactlyAndCutsStayInCategory` FAILED (line 135); 3 others passed. Reverted; 5/5 PASSED again.

Also: `fdc_parse.py --selftest` still exits 0 (CONFIRMED).

## 4. GF arena and FixDemoSceneRequest (item 3, part b; item 4) — NOT BUILT

Acceptance "GF produces PES's FixDemoSceneRequest from its own state for the captured events (counts)": **0 of 6** (CONFIRMED by absence: no GF code builds a request; no test exists). Blockers, each CONFIRMED against the docs and driver output:

1. **The matcher's input is the ctx singleton (`141F500D0`), a ~0x5230-byte struct, and most of its fields come from match state through setters not yet translated.** The setter `FUN_141F51630` (env copy from `world+0x35960+0x70`, team blocks at `setup+0xc400+s*0x2804`, match phase `G+0x270+0x1168`, goal difference `FUN_141F514D0`) is not in the layer. Without it GF cannot fill `ctx`.
2. **Fields with no known meaning** (`pes-re-cutscenes.md` Appendix A, "Remaining unknowns"): `P+0x44` (types 0x11, 0x14, 0x15, 0x26, 0x28..0x3d), `P+0xfa/0xfb` (0x3e..0x42), env `+0x08`, `+0x14`, `+0x20` (partly), `0x2911`, `0x2918/0x291c/0x291d`, `0x5200`, `0x5168`, `0x515c`, the `0x51fc` list. A condition read from one of these cannot be reproduced from GF state, so the request would be wrong for those keys.
3. **The caller `141F41CE0` and wrapper `141F4FDA0` are not translated** (§2.3), so the path `cutRef -> FUN_141f4fda0 -> matcher` cannot run end to end.
4. **The arena itself** needs PES layout for: the `std::map<cutId,rowIndex>` cache at `+0x30` (MSVC layout, sentinel node), the history at `0x1437F5DC0 + cat*0x40` (an image global, so it must sit inside the window), the RNG instances `0x1437F5D90` / `0x1437F6340` (`pesrandom.hpp` slots), and the table rows as a region. The window pattern exists (`pesworldrecord.cpp`: `kWorldWindowVa`), so the mechanics are known; the layout values are not yet written down.

What GF can do now, and this branch does: decode the tables (§3). A request needs (1)–(4).

## 5. Goal celebration (item 5) — NOT REPLACED, gap named

- `src/sim/onthepitch/goalcelebration.cpp`: `SeedFor` (line 71), `Choose` (line 81), `PickCamera` (line 98) are left in place. Nothing is deleted.
- The goal event's request is NOT proven: the goal pool pick `FUN_140A92530` over goal context `+0x108` is not replayed (`pes-re-scene-choice.md` §2a, OBS only). The goal wrapper `141F4FDA0` is blocked (§2.3). The goal extras are decoded by the new decoder (§3), but the counts were NOT re-checked in this pass (the 362/338/162/35 figures come from `pes-re-cutscenes.md` §2.2, OBS), so the table side is decoded-not-verified against the request.
- Replacement waits on: (a) `141F4FDA0` diffed, (b) the goal pool pick translated and proven on a live call, (c) the ctx of §4.

## 6. Open items (numbered, with blocker)

1. `1403FB410` vtable calls `call [rax+0x10]` at `0x1403FB47E`, `0x1403FB49F`: the harness cannot model the target. Blocks `141F41CE0`, `141F4FDA0` (OPEN; harness/ImportModel owner).
2. Translate `FUN_141F51630` (ctx setter from match state): env, team blocks, phase, goal difference (OPEN; next slice).
3. ctx fields with unknown meaning (§4.2) (OPEN; decompile `FUN_141b59e20`, `FUN_1414c0b40`, the `world+0x35960` owner, `FUN_140a55d40/140a55ef0`).
4. Arena layout: MSVC `std::map` at `+0x30`, history, RNG instances, rows region (OPEN; write it down from `FUN_141f4fda0` and `FUN_141f4b1d0` disassembly).
5. The four committed records are unicorn-diffed with on-disk globals ("provisional"); a snapshot replay of the matcher/evaluator with live globals is the stronger check (OPEN; `snapdiff.py`, the earlier 4/4 is on live snapshots).
6. Decoder cross-check: row-for-row comparison against `scene_map.py --json` counts (OPEN; scratch only).
7. Goal pool pick `FUN_140A92530` not replayed (OPEN).
8. 15 type-99 `.fdc` files and 21 dangling `mode` `.fdc` names (carried from CutsceneHybrid §6; unchanged).

## 7. Checks run (CONFIRMED) and not run

Run:
- `pes_fixdemo_table_test`: 5/5 PASSED with `PES21_ROOT=<workdrive>/PES21`. Mutation M1 (§3.1) failed 2 tests, reverted, 5/5 again.
- `gen_translated.py --check` (image `pes_full.bin`): `3987 functions (3676 recorded bit-exact, 311 closure) regenerate byte-identical`, rc 0. (The pre-change baseline on this branch was also green: 3951 functions.)
- `driver.py` on the six FixDemo specs: 4 diffed, 2 not (§2.2).
- `fdc_parse.py --selftest`: exit 0.

Not run:
- gfsim-linked test binaries and `pes_*` cutscene/goal tests: `pestranslated` now compiles 3987 TUs (36 new), and the build dir was fresh, so a link of any gfsim consumer is a full layer build. Main's gate covers it. The changed-file consumers to run: `diff_translated_drift` (pilot) and the gfsim-linked `pes_*` binaries.
- The gate: not run (Main gates).
- No sim run; no PES live read or capture in this pass.

## 8. Files (branch `CutsceneSelect`, commit `96834d0a`)

- `src/pes/fixdemo_table.hpp`, `src/pes/fixdemo_table.cpp` (new), `CMakeLists.txt` (pesfmt source), `tests/pes_fixdemo_table_test.cpp` (new), `tests/CMakeLists.txt` (target).
- `src/pes/translated/` (36 new `pes_fn_*.cpp`, `translated.hpp`, `pes_dispatch_gen.hpp`).
- `tools/dev/translate/gen_translated.py` (SETS += fixdemo), `tools/dev/translate/results_fixdemo.json`.
- `tools/dev/diffharness/specs/fixdemo/*.json` (6 specs), `tools/dev/diffharness/specgen/sets/fixdemo.txt`.
- Scratch only (not committed): `<workdrive>/gf-CutsceneSelect-scratch/` (`spec_fixdemo*/`, `build_fixdemo/`, `check_base.out`).

## 9. Next highest-value step

Item 1 (§6): model the `1403FB410` vtable dispatch in the harness. It unblocks the caller and wrapper, which are the two translated functions the request path needs. Without it no oracle comparison can run even after the arena exists.
