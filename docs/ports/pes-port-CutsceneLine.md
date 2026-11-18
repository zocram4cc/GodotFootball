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

# CutsceneLine round 5: the five out-of-window traps are not yet explained; the branch stays red on three diff tests (ee383c13); no fix pushed

Branch `CutsceneLine`, head `ee383c13` (unchanged this round). Labels: CONFIRMED = read or run this round; INF = inference; OPEN = blocker named.

## 1. What the trap sites are (CONFIRMED by disassembly and the generated code)

- **0x140A6E320 (ro 0x1426484F0, 380 B) and 0x140A6E340 (ro 0x1426484F2, 377 B).** Each is `cmp ecx, 0x5e; jl` (a SIGNED compare) then `movsxd rax, ecx; ... [table + rax*4]`. Any `ecx < 0x5e` passes the guard, including negative values, so PES reads before the table for negative `ecx`. The generated code bakes the window from the table base with no lower bound.
- **Real PES callers can pass negative values.** `0x14052A7C0` calls 0x140A6E320 with `ecx = movsx byte [rax+1]` (a signed byte from a record: -128..127). So a negative index is a real PES read: the window must start at base-0x200 (4 * -128) for that caller. (INF for the other callers: `0x14052B888` passes `ecx = edx`, its origin not traced.)
- **0x140A92C80 (ro 0x142655950, 2052 B).** Guarded by `cmp ecx, 0x1ff; ja` (unsigned), so it never reads negative. It did not trap.
- **0x14038F0E0 (ro 0x14257F780, 7168 B).** The code is `r8 = round-up(rcx)`, `cmp r8, 0xe000; ja` (checked: the generated file has `cond("a")` on 57344 at line 215, before the read), then `byte [table + ((r8-1)>>3)]`. With that guard the index is at most 0x1BFF, inside the window. The trap offset 23058430 = 0x15FD7FE does not fit this guard.

## 2. What the traps show (CONFIRMED)

The driver's recorded check (`driver.py --check --record results_impure.json --seed-bank banks_union9l`, `--only` ignored, 34 functions attempted) reproduces the trap for `0x14052A820`: `PES_RO_OUT_OF_WINDOW: table at ... (380 bytes): read of 4 bytes at offset 184467440`. `184467440` (0xAFEBFF0) is POSITIVE and equals `gpr0*4`. A signed index of -k would appear as a wrapped 2^64-4k, not this value. So the five traps are NOT explained by the negative-index path in §1. The same value appears for the byte table, halved (23058430 = 184467440/8).

The seed bank (`banks_union9l/<VA>.bank.jsonl`) holds memory regions only; register inputs come from the spec (`specs/impure/14052A820.json`, inputs from A0/A1 fields with generic domains: u8 0..255, f32 -50..50). So the failing index is produced by the generator, not read from PES state. (INF: the value is an artefact of how the generated inputs feed `edx`/`ecx`, not a PES value.)

## 3. What is NOT done

- No fix is pushed: the declared negative window for 0x1426484F0/F2 would be correct for PES's signed-byte caller, but it does not explain the positive 184467440 traps, so it cannot close the three red tests on its own.
- The 14038F0E0 trap contradicts its own guard (§1); that needs the failing input (a per-case dump), not a window change.
- Next step (round 6): instrument the diff run to dump the input registers and memory of the first trapping case of `0x14052A820` (the bank run above), find which input produces `gpr0 = 46116860`, and trace that input to its PES source (the spec field or the caller). Same for `0x14038F0E0`: print r8 at the read. Then either the spec input domain is wrong (fix the spec's generic domain to PES's real domain for that field) or the window/bake is wrong (fix the cause).

## 4. Interjections noted

- Main (after round 5): the kind-2 goal-anchor feed is decoded in `docs/re/pes-re-goal-anchor-feed.md` on main (GoalAnchorFeed). BC from `0x1404B88B0` via `0x1404B7F70`/`0x1404B6960`; kind 2 = scorer in the wide channel; the else arm iff `(BC-6)&~2 != 0`. Use it for the kind-2 anchor case once the gate is green.

## 5. Status for the gate

ee383c13 is RED on `diff_pilot_recorded`, `diff_core_recorded`, `diff_impure_recorded` (the five traps). It is not yet fixed. Main: do not re-gate until round 6 lands a cause fix.

# CutsceneLine round 6: the traps were a bake defect (signed guard) and a spec domain defect; fixed at the cause, re-proved, pushed as a3a81555

Branch `CutsceneLine`, pushed head `a3a815553dd37f...` (`git ls-remote origin CutsceneLine`). Labels: CONFIRMED = run or read this round; INF = inference.

## 1. The first trapping case of each function (CONFIRMED)

**0x14052A820.** The trapping read is in the reader 0x140A6E320 (table 0x1426484F0, 380 B), reached as `call 0x140A6E320` with `ecx = edx` (0x14052B886). Running the translated binary on corpus case 24 under gdb aborts with `PES_RO_OUT_OF_WINDOW ... offset 18446744073709551548` = 2^64 - 68 = 4 * (-17). So the index is -17, a negative value.

PES's own side (unicorn over the image, `pes_trace.py`, all 105 corpus cases): the reader is called with signed indexes -104, -53, -17 (and 0..105). Three cases (24, 42, 48) give negative calls, two per case. For each, PES reads a real image dword below the table: at -17 the dword at 0x1426484AC is `0x69727473` ("strI" in .rdata), at -53 it is `0x69427470`, at -104 it is `0x69726353`. Each masked by `0x30000` is nonzero, so PES's `al` is 1, while the translated read gave 0.

The caller is real: `0x14052A7C0` passes `ecx = movsx byte [rax+1]`, so ids -128..127 reach the reader from record data.

**0x14038F0E0.** Case 64 (rcx = 0, rdx = 214) and case 67 (rdx = 0), 70, 72 give `r8 = 0`, so the index `(r8-1)>>3` is 2^61-1. The first trap (gdb) is `offset 2305843009213693951`. PES's side for case 64 (`pes_case.py`) read the byte at 0x14257F77F, one below the table, with value 0. That address is `2^61-1 + base`, which is non-canonical in x86-64 (0x200000014257F77F); unicorn truncates it, so PES's emulated read is an artefact.

Note: the log in round 5 gave offsets 184467440 and 23058430, which did not reproduce in the corpus I ran; the reproduced values are the negative and 2^61 ones above.

## 2. The two questions (CONFIRMED)

**What PES read at the cases that were recorded bit-exact with a 0 read?**
- 0x14052A820: the real image bytes at ids -17, -53, -104 (listed above), nonzero masked, so PES's reader returned 1. The old translation returned 0. The outputs still matched PES for all 105 cases, both before and after the fix. So the old output match stands, but it was a match on a wrong intermediate in those 3 cases (the reader's result did not reach the compared outputs there). The fixed translation reads the same bytes as PES.
- 0x14038F0E0: the four synthetic cases (rcx = 0 or rdx = 0) reach an address PES cannot form; unicorn's truncated read was the zero byte below the table, and the old translation's 0 matched it. These cases are not reachable from PES's caller, so their bit-exact status was a coincidence of an impossible input. They are removed by the domain fix below.

**Does the old bit-exact count stand?** The output comparison for 0x14052A820 stands (105/105 before and after). The 0x14038F0E0 count does not: its old corpus included four unreachable inputs. Its re-proof is on the cited domain (section 4).

## 3. The causes and the fixes

**A. The bake (0x140A6E320/340 and 26 recorded functions).** `bounded_index` took the bound from `cmp ecx,0x5e; jl` on the taken path and gave `count = 94`. A signed guard bounds the index from above only, so the bake started at the table base and dropped the negative ids. The declared table `0x1426482F0` (0x378 bytes, "ids -0x80..0x5D", already in `ro_tables.json`) covers exactly -128..93 (offset 0x200 below the base).

Fix (`xlate.py`, `SIGNED_JCC`): both bound searches skip signed jumps. The reads then take `bake_window`'s declared hull `k_t1426482F0_1210` (offset = gpr0*4 + 0x200, so -128 reads at offset 0). The window the base-only bake gave is gone: this is the base-0x200 window you asked for, and it is declared, not invented.

Effect: 26 recorded functions change generated text (all on this table: 140A6E050..630, 140A6D420, 140A6E320, 140A6E340). No closure member changes.

**B. The allocator domain (0x14038F0E0).** Its caller 0x14038E3E0 requires the alignment `rdi` (= rdx) to be a power of two (`lea rax,[rdi-1]; test rdi,rax; jne`) and passes `rcx = r8 if nonzero else 1` (`test rbx,rbx; mov ecx,1; cmovne rcx,rbx`). So `rcx >= 1` and `rdx` is a power of two. The spec's generic 0..255 domain for both fields was wrong.

Fix: `specgen/spec.py` `ROOT_INPUT_DOMAINS` (keyed by root VA and register, with the caller evidence in the comment) and `specs/core/14038F0E0.json` `input_schema` updated to match: rcx `u8` lo 1; rdx `choice` {1,2,4,...,128}. rdx = 0 is excluded by INF: PES's r9 (the caller's minimum) is >= 1, so `cmp rdi,rsi; jb` rejects rdi = 0.

## 4. Re-proof (CONFIRMED)

The changed recorded functions (26), the five trapping roots (14052A820, 14063FDC0, 140578BA0, 140593290, 14038F0E0), and their recorded callers (30 more) were re-run with `driver.py --check --select` on the three recorded sets (the gate's selection mechanism):
- pilot (results.json): 13 of 13 bit-exact
- core (results_core.json): 41 of 41 bit-exact
- impure (results_impure.json, `--seed-bank banks_union9l`): 1 of 1 bit-exact

55 functions in total, all bit-exact on the final tree.

## 5. Other checks (CONFIRMED)

- `gen_translated.py --check`: 5695 functions regenerate byte-identical.
- `ledger.py`: rc 0, no tracked change.
- Build (`godotfootball`, `gfsim_run`, `pes_goal_anchor_test`, `pes_fixdemo_select_test`, `pes_camera_list_test`): rc 0.
- C++ tests: `pes_goal_anchor_test` 20/20, `pes_fixdemo_select_test` 6/6, `pes_camera_list_test` 5/5.
- Python: `test_strcpy_model`, `test_memset_model`, `test_memmove_model`, `test_memchr_model`, `test_fdtest_model`, `test_image_reads` (9), `test_snapmem_holes` all OK.
- PII scan of the changed files: no machine paths.
- Build artifacts `godot/**/*.uid` are untracked and not committed.

## 6. Not done / for Main

- The full `diff_pilot_recorded`, `diff_core_recorded`, `diff_impure_recorded` tests were not run to completion. The re-proof above is the same driver over every function whose generated text or callee changed, which is the set the gate's selection would pick. Please gate the merged head to confirm.
- Smokes were not run (Main gates).
- No unit test for `SIGNED_JCC`. The proof is the recorded re-check of the 26 functions and the five roots. A unit test would need a synthetic emitter; not done in this round.
- Kind-2 anchor case: not started (Main's feed doc `docs/re/pes-re-goal-anchor-feed.md` is the input for round 7).

# CutsceneLine round 7: the SIGNED_JCC unit test is pushed (test-only, 74e180ba); the kind-2 anchor case is not started in code: every function of its chain is translated, so the gap is GF's arena feed

Branch `CutsceneLine`, pushed head `74e180bafc65c2...`. The gate on 708e37a1 is still running; the lock `/tmp/gf-build.lock` was held by another agent's build at the time of the check, so this round built nothing. Labels: CONFIRMED = run or read this round; INF = inference.

## 1. SIGNED_JCC unit test (DONE, test-only)

`tools/dev/translate/test_bake_signed.py`, registered as `translate_bake_signed` in `tests/pilot/CMakeLists.txt`. Four behavioural checks on synthetic functions over a fake image whose bytes are a function of the address:
- signed fixture (`cmp ecx,0x5e; jl READ`, the 1426484F0 shape): `bake_ro` declines a window from the table base; `mem_val` takes a window that covers ids -0x80..0x5D (the declared hull, `ro_tables.json` 0x1426482F0); the window's bytes equal the image's at each id (-128, -53, -17, -1, 0, 93).
- unsigned control (`cmp ecx,0x5e; ja`): the bake keeps its window from the base, 95 entries, first entry equal to the image.

All 4 pass on the committed translator (`python3 test_bake_signed.py`, harness venv). Mutation (CONFIRMED, run and reverted): the `if mn in SIGNED_JCC: continue` skip disabled in both bound searches → 3 of 4 fail (`declines`, `covers`, `holds the image bytes`); the control still passes. `xlate.py` reverted to the committed file (`git checkout`), and the tests pass again.

The ctest entry runs the same script under `GF_DIFF_PYTHON` (exit 77 when unset), like the other translator tests. It has not been run through ctest: no build this round.

## 2. Kind-2 goal-anchor case (NOT STARTED in code)

What the chain needs is already translated (CONFIRMED by `ls src/pes/translated`):
- `0x1404B88B0` (the BC writer): `restart/` (recorded);
- `0x1404B7F70` (its zone decision), `0x1404B6960` (the scorer-zone transform), `0x1404B6400`, `0x1404B6ED0`, `0x1404B9B70`, `0x14047AB20`: `closure/`;
- `0x140A901E0` (the quad test, a signed-area point-in-quad check: decompiled, CONFIRMED): `cutscene/`;
- `0x1404B6BD0` (the else arm's list and T): `cutscene/`;
- `0x140471A80` (the recompute, else arm proven at `ee383c13`): `cutscene/`.

So the kind-2 case is a GF arena feed, not a translation gap. GF must, before the anchor: (1) write the inputs the writer reads (G slot table G+0x180, the squad table, MI pair 0x1588/0x1590 and side/key, G+0x2AA8 field info, the scorer man/assist records, the ball object G+0x40, the `ctx+0x18 -> +0x22E4C` slot tables, the DC counter A0+0xDC); (2) run `0x1404B88B0` on the arena; (3) run `0x140471A80` gated by EA; (4) then the anchor. Doc `pes-re-goal-anchor-feed.md` §3 lists these gaps; its §5 item 1 (the decision-tick inputs for BC = 2, i.e. the scorer position in the wide channel at DC in [5,171]) is still OPEN, because every captured pre-decision state gives BC 8.

Why I did not write a red test this round: a kind-2 case needs its cut list and the scene list from the decision, and the decision's inputs are not captured (doc §2). A red test built on the recorded post-state (BC 2, C0 8, EA 1, 14h 162) would test a state GF does not compute from inputs, which is the copy-the-answer test the brief forbids. The zone rule's unknowns are also real: `0x1404B6960` reads the field table through `DAT_1436F3DC8+0x2AA8` and builds corner candidates (decompiled above), and I did not establish the coordinate frame it returns; a hand port would be a guess.

## 3. What would unblock it

1. A `bcwriter_4B88B0` capture armed on its return value (the doc's open item 1): the decision tick's inputs with BC = 2. Main can arm it with `--when` on the return, or a DC-windowed hook.
2. A GF arena feed for the writer's inputs (doc §3), replacing the G+0x180 0xFF stand-in (it diverges from the live table on this goal).
3. Then: a kind-2 `Case` (the anchorH.json values: scene 0x300D9, slot 19, pos (-41.5502, 0, 29.5574), half (52.56, 34.06), target (-41.6211, 0, 35.56), radius 0.0, PES pos (-41.5502, 0, 34.0), rotY 0) with a `DISABLED_` test until the feed lands, and the flip from `0x140478FF0` (not in the anchor capture; needs its own read).

## 4. Notes

- No build this round (lock held). The round-6 translator change and this test are both on the branch; the gate on 708e37a1 does not include the test file.
- Nothing pushed other than 74e180ba.
