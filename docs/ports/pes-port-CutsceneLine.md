# CutsceneLine round 4: the translator fixes are landed on the branch with the recompute proof; the kind-2 anchor case is not done (GF needs the bc writer and the else arm)

Branch `CutsceneLine`, pushed head `ee383c13` (`git ls-remote origin CutsceneLine` = `ee383c13ec06...`). Round 4 adds one commit on `12c3c64a`. Labels: CONFIRMED = run or read this round; INF = inference; OPEN = blocker named.

## 1. What landed (commit ee383c13)

- **bake_window coverage.** A declared-table hull is used only if it covers every byte the guard-bounded index can reach. Otherwise the read falls to bake_ro's self-guard window (`bounded_index` gains a fallback: a guard on the index register itself, `cmp r8, 0x1f; ja`). Order in `mem_val`: copy-guard bake, hull (coverage-checked), self-guard bake, else live.
- **ro_rd traps.** An out-of-window baked read aborts with `PES_RO_OUT_OF_WINDOW: table at <addr> (<len> bytes): read of <n> bytes at offset <off>` (`src/pes/translated/pes_state.hpp`). It no longer reads 0. The message names the table by its load address, not its image VA. The generated call does not carry the VA, and adding it would change every function's text; the VA is in the generated window name (`k_t<VA>_<len>`).
- **ro_tables.json.** Presentation's two declarations (ce6b19b8): `0x142581374` (0x80 bytes) and `0x142B67EF0` (2 bytes).
- **Switch bound through a register.** `mov ebx,7; cmp r8d,ebx; ja` resolves the jump table (jump-table prepass).
- **strcpy_s.** `ucrtbase.dll!strcpy_s` (slot 0x142530660) is modelled by the owner's rule: a CRT string copy with errno returns. `import_stubs.json` has `"do": "strcpy_s"`, and `xlate.import_model_lines` mirrors its code_hex register for register. The contract test `tools/dev/translate/test_strcpy_model.py` runs the model's seven paths (OK). The mirror itself is checked by the snapdiff proof below, not by a separate C++ test.
- **Duplicate memset branch removed** (the second `do == "memset"` in `import_model_lines`, unreachable).
- **Recompute proof recorded.** `0x140471A80` (cutscene) is recorded in `results_core.json` with `proof: snapdiff`, source `results_snapdiff_recompute.json`.

## 2. Verification (CONFIRMED)

- **Re-proof of the recompute** (`snapdiff --only 0x140471A80` on the final tree, pid2024691 seq203 = BC 2, the else arm): 1 call, 1 replayed, **0 mismatches**, proven. Own-block 54/138 (recorded, not gated). It runs `0x1404B6BD0` and its callees, including `0x1404B8290` (the switch), so those are re-proved on this call.
- **Layer changes.** `gen_translated.py` (before the fixes it reported 51 recorded changes): now **0 recorded functions change text**. Changed closure members: `1403AE220`, `1404B8290`, `1408C75E0`, `1415A1708`, `1415A1720`, `1415A1726`; plus 43 new closure files. Their proof is the root proof above. The only recorded caller of a changed member is `0x1404B6BD0` (calls `1404B8290`), re-proved on the same call.
- **Held out** (same three as main, not in main's layer): `0x1403BB670`, `0x1405549C0`, `0x14202BA30`.
- **gen_translated --check:** 5695 functions regenerate byte-identical.
- **Ledger:** `tools/dev/ledger.py` rc 0, no tracked change.
- **Tests:** `pes_goal_anchor_test` 20/20, `pes_fixdemo_select_test` 6/6, `pes_camera_list_test` 5/5 (rebuilt after the layer and header change, `godotfootball`, `gfsim_run` and these three link: rc 0). Python: `test_strcpy_model`, `test_memset_model`, `test_memmove_model`, `test_memchr_model`, `test_fdtest_model`, `test_image_reads` (9/9), `test_snapmem_holes` all OK.
- **Not run:** the other `pes_*` binaries and the `godot_*` tests; the gate; smokes. The layer header change touches every translated function; Main, please gate and smoke the merged head.

Mutations: none this round (no GF logic changed).

## 3. Not done: the kind-2 anchor case (OPEN)

`pes_goal_anchor_test` pins GF's `PlaceGoal` against PES. GF's arena takes the builder default arm (BC = 8). PES's pid2024691 goal is BC = 2 (`0x1404B88B0` wrote 2 from the scorer state), and its T and radius come from the else arm (`0x140471A80`), which is now proven. A kind-2 case needs GF to:
1. run the bc writer `0x1404B88B0` (inputs: the scorer's man record, the clock draw `0x141E532E0` against A0+0xDC, `0x1414C1300`, `0x140478FE0`, `0x1404B6960`) so A0+0xBC comes out 2 in GF's arena, and
2. run the else arm `0x140471A80` (per tick when A0+0xEA is set) before the anchor.
Both are in the layer, but step 1 means feeding the bc writer's world reads into GF's arena, which is a new feed like the builder's. Not started: it does not fit the round's budget with a red test and mutations. Round 5 candidate.

## 4. D/F

Main armed `recompute_471A80` on `A0+0xBC <= 1` for kinds 0 and 1 (D and F). No kind-0/1 capture yet. When it lands: replay its recompute with `frame_trace.py recorded --spec-dir <impure spec with args>` and `snapdiff --only 0x140471A80` (the method in §2). The D/F anchor cases then need the same GF feed as §3.

## 5. Notes

- A `/tmp` write (a `cmake --build` log redirect) slipped in once and was removed.
- Scratch: `gf-CutsceneLine-scratch/recompute/seq203` (unpacked copy, ~4 GB): delete once Main has the record.
