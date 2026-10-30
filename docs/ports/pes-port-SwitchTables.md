# PES port SwitchTables: a generation-time lint for live reads of read-only image bytes, every one baked (137 layer files change), the CardLevel4 switch-table bake without the judge cutover; both 2-min smokes finish and hash exactly as main's (30-10-26)

Branch `SwitchTables` (worktree `<workdrive>/gf-SwitchTables`, scratch `<workdrive>/gf-SwitchTables-scratch`), confirmed by `git ls-remote origin SwitchTables` = `8bc504dc` (last push). Base: `origin/main` `009e3b50` (batch AP) merged in, then `f445018e` (docs only). CONFIRMED = tool output this session; INF = inference; OPEN = blocker named. Image `pes_full.bin`, base `0x140000000`.

## 0. Status against the ticket

| item | status |
|---|---|
| 1. branch from `origin/main`, take FoulSanction's `b4bcd543` (switch byte map + dword table baked) and `test_switch_tables.py`, without the judge cutover | DONE. Only the tool part of `b4bcd543` was applied (`xlate.py`, `test_switch_tables.py`: `git diff b4bcd543~1 b4bcd543 -- tools/dev/translate`); none of its 82 generated files were taken, the whole layer was regenerated instead. No FoulSanction file touched |
| 2. lint, red-first, per-class decisions, committed layer passes | DONE (section 1, 2). Lint = `gen_translated.py` fails the generation on any live read of a read-only image address; `test_image_reads.py` 8/8 (red on `origin/main` tooling: the API does not exist; three mutations killed, section 4) |
| 3. regenerate, `--check`, recorded re-checks | DONE: 4,346 functions (3,694 recorded, 652 closure), `--check` byte-identical; the five `diff_*_recorded` selections all bit-exact (section 3) |
| 4. `--pes-loop` 2-min smoke, bake only | **NO ABORT** (section 5): the abort CardLevel4 saw belongs to the judge cutover, not to the bake |
| 5. determinism, gate line, moved hashes | smokes deterministic; **no hash moved** (section 5); gate: see the end |

## 1. The lint (CONFIRMED, `tools/dev/translate/xlate.py`, `gen_translated.py`)

The class: translated code that reads a read-only image address (`.rdata`/`.text` tables) through `s.rd(...)` returns 0 at GF runtime (GF maps no image there), while the harness maps the image flat, so every diff stayed bit-exact. `b4bcd543` fixed the switch tables; the lint finds the rest.

How it decides (generation time, per function, no byte compared):
- `reaching_defs`: forward may-analysis over the function's CFG (labels, branch targets, resolved JMP-reg switch targets): for each register, the set of instructions whose write may reach each instruction.
- `image_pointer`: a register is exactly an image VA when every reaching definition is the same LEA of an image VA or the same load of an image pointer from a read-only image slot. This replaced `lea_const` (a straight-line scan that gave up at any label: it lost the `lea r9,[image]` of a switch that sits across a join, so a second-level table load stayed live).
- `image_origins`: the image VAs a register may be derived from, followed through `ADD/SUB/INC/DEC/AND`, register copies and LEAs (a loop-walked pointer keeps its origin).
- Every register-formed read the emitter would write as a live `s.rd/rdf/rdd` goes through, in order: constant base register folded to an absolute read (new: `[rcx+disp]` with `rcx` an exact image VA is the absolute operand, baked); guard-bounded window (`bake_ro`, unchanged, now using `image_pointer`); declared table window (`ro_tables.json`, new); else `note_live_ro` records `(site, image VA)` into `xlate.LIVE_RO[function]`.
- `gen_translated.lint(members)` fails the generation, naming `function 0x.. site 0x.. reads image 0x..`, for any finding on a committed function. `--check` runs it too (the layer drift ctest test therefore runs the lint).
- Writable sections (`.data` globals) are not in scope: they are live state, served by the world window / contexts.

Declared tables (`tools/dev/translate/ro_tables.json`, 45 entries, each with the decode that bounds it, tagged CONFIRMED or INF): a table a register walks without a guard the translator can bound (pointer-walk loops, parameter-driven indexes, loop trip counts). A table is baked whole (`ro_rd(k_t<VA>_<n>, n, addr - lo, size)`: the address's own offset into the window, so any base/index shape works); overlapping tables that hold the read's start bake their hull. Out-of-window offsets read 0 as `ro_rd` always did.

**Parameter-driven tables are sized to the harness corpus, not to the decode** (CONFIRMED by a red re-check, section 3): with decode-only windows 140A92470, 140A6D400 (and its caller 1406E6A40) stopped diffing, because the corpus draws integer parameters over `u8 0..255` / `i32 -100..100` (`diffharness/run.py _rand_case`) and an unguarded index reads PES's neighbouring bytes there. Those windows (INF in the file) span that domain; the extra bytes are PES's image bytes, so GF reads what PES reads.

## 2. Classes found, decision per class (CONFIRMED lint output, `scratch/lint*.txt`)

First lint run on the layer with the switch fix: 35 sites (straight-line scan), 122 with the dataflow (pointer walks, switch loads past a join), 113 after the reaching-defs rewrite (switch loads now baked), 7 more from the batch AP functions after the merge.

| class | sites | decision |
|---|---|---|
| second-level switch dword table loaded through a base that crosses a join (`mov ecx,[r9+rax*4+table]` feeding `jmp`: 140509E50, 1405CD050, 14063FA70, 1406EA840, 140A3F770, 140A484F0, 140A59F90, 140A8D8B0, 141F4D2F0, 141F56FD0 and others) | 9 functions | bake (translator: `image_pointer` by reaching defs; no declared table) |
| constant base register, constant displacement | several | bake (folded to the absolute operand) |
| guard-less table index (param, enum, signed char, byte) | ~30 tables | bake, declared window (decode bound, or the corpus domain, section 1) |
| pointer walks over a table (loops) | 8 | bake, declared window = the loop's trip count |
| `.data` globals read at absolute VAs | 2,775 sites in 939 files | out of scope (live state, not a table) |
| row pointer of a table the spec itself patches (1414FDBC0, slot `0x14297D808`, `patch_addrs IMG.14297D808`) | 1 table | **bind if loaded, binding NOT written** (OPEN 1): marked `"bound"` in `ro_tables.json` with the reason, read stays live, lint exempts it |

No class needed a runtime binding that exists on main: nothing is "served by pesimagetables / the TrapCutover dump / PesTrapData" in this change (those landed or are landing separately; the baked windows are independent of them and equal to their bytes where they overlap: anime class table 0x1426484F0 -0x80..0x5D entries, tier table 0x142645A40 0x22 rows, quarter sine 0x142655950 513 floats, same extents MatchInfo6 chose).

**Functions that changed behaviour in GF: 137 layer files** (`git diff 009e3b50 HEAD -- src/pes/translated`: 25 cutscene, 46 closure, 15 gameplay, 9 ball, 7 ai, 6 restart, 6 animation, 5 audio, 3 replay, 3 pre_match, 3 impure_sample, 3 camera, 2 hud, 2 crowd, 1 referee, 1 in_match_menu), **298 live reads now baked**. Each such read returned 0 in GF before and returns PES's bytes now. How many the GF smoke paths reach: none change an output there (section 5: hashes equal main's).

## 3. Recorded re-checks (CONFIRMED, drivers run on the merged tree)

Selection = `select_recorded.py --base origin/main`, the same the fast gate makes: 32 pilot, 48 cutscene, 27 presentation, 188 core, 18 impure.

- First pass, decode-only windows: presentation 26/27 (140A92470), cutscene 46/48 (1406E6A40, 140A6D400): corpus-domain finding above. Fixed in `ro_tables.json` (windows to the corpus domain).
- Second pass (merged tree, windows widened): pilot 32/32, impure 18/18 (`GF_IMPURE_BANKS=banks_union9l`), presentation 27/27, cutscene 48/48 bit-exact; core 186/188: 140A6E3C0 (a wider neighbouring window with a higher start cut the anime class table's negative ids: the choice "widest wins" was wrong, now the hull of the containing windows) and 1414FDBC0 (its spec patches a pointer slot in the table: a bake ignores the patch; now left live, named OPEN 1).
- Third pass (the 7 core functions the last regeneration touched: 140A6D120, 140A6DD30, 140A6E3C0, 14085F150's callers, 1414FDBC0): **7/7 bit-exact**.

## 4. Tests and mutations (CONFIRMED, each rebuilt, run, reverted)

`test_image_reads.py` (new, registered as ctest `translate_image_reads_test`; a synthetic image with a `.text` table, three functions translated, compiled with g++ and run with NO image view): undeclared pointer-walk and parameter-index reads are findings with the right image VA and site; a constant-displacement read is baked and answers `0x44332211`; declared tables answer from the bake (`3+4+5 = 12`, `3/5/0x44`); two overlapping declared tables bake their hull (index 7 only the narrower holds); a `bound` table stays live and is no finding. 8/8. `test_switch_tables.py` 3/3 (registered as `translate_switch_tables_test`). Other translator tests: `test_extent` 19, `test_closure_walk` 18, `test_setcc_store` 2, `test_select_recorded` 9, `test_recorded_call` 12, `test_memset_model`, `test_strcpy_model` OK.

Red first: against `origin/main`'s tooling the new test errors (no `LIVE_RO`, no declared-window API; copied tree in scratch). Mutations, in `xlate.py`:

| mutation | killed by |
|---|---|
| M1 `bake_window` returns None | both declared-table run tests |
| M2 `image_origins` straight-line only (one def) | undeclared pointer-walk finding, lint-lists test, declared pointer-walk run |
| M3 constant-base fold disabled | constant-displacement test |
| (hull) widest-wins instead of hull | `140A6E3C0` diff (real re-check), then the hull test |

## 5. Smokes (CONFIRMED, `gfsim_run --seed 7 --minutes 2`, merged tree, `GF_SIM_SLOTS=4`)

| run | rc | determinism | ticks |
|---|---|---|---|
| switch-off 1 | 0 | `e663d1e917d6c4f4` | 33628 to full time |
| switch-off 2 | 0 | `e663d1e917d6c4f4` | 33628 |
| `--pes-loop` 1 | 0 | `980c54e27f93ced2` | 25942, 9 stoppages |
| `--pes-loop` 2 | 0 | `980c54e27f93ced2` | 25942 |

**Moved hashes: none.** Both equal batch AP's baselines on main (`e663d1e917d6c4f4`, `980c54e27f93ced2`): the bake changes no output on the two smoke paths.

**Item 4 answer: `--pes-loop` does not abort with only the bake.** No `std::array<int,64>::operator[]` assertion, no `[stalemate]` crash, full time reached. CardLevel4's abort (`styleGroup[style*4 + member/4]` / `kindBandRow[kind + band*8]` index >= 64 in `pes::movestep::RunLoop`, at the first `[stalemate] 6.2s idle`) therefore belongs to the judge cutover (the translated judge now answering a foul, so the stoppage/foul flow reaches a style the cutover's GF side never staged), not to the 137 baked layer files. Nothing to fix on the GF side here; FoulSanction owns tracing the feeding value (`style` / `kind` from the translated judge's now-correct answer) when the cutover is merged on top of this branch: take this branch first, then re-run the pes-loop smoke.

## 6. Open items

1. 0x1414FDBC0 (`0x14297D800` table, 0x1C rows of id/pointer/count) is exempt from the lint by a `"bound"` note: its spec patches the pointer slot `0x14297D808`, so a bake breaks its bit-exact proof, and the row pointers lead to more image tables (a loaded-pointer chain this lint does not follow). No GF source calls it or its only layer caller 0x1414CB2E0 directly (they sit in the indirect-call dispatch table). When GF calls it, it needs a runtime binding (read the rows and their pointer targets from the install, as `pesimagetables` does) or a spec without the patch.
2. Lint limits (static, per function): a pointer loaded from a *register-addressed* table, a vtable read through an object pointer, and a table pointer a caller passes in are not followed. A dynamic complement (count `PesMem::rd` misses in `[0x140000000, image end)` during the pilot and smoke runs and fail on any) would close it; not written (shared hot-path header, forces a full layer rebuild).
3. INF windows (parameter-driven, corpus-sized): 140702680, 14085F150, 1408CB0B0, 140A65C80, 140A6D420, 140A6D120, 140A828B0, 140A92470, 140A6D400, 140846ED0, 140847090. Each is proven only over the corpus domain; a PES-side caller bound (an enum range) would let the window shrink to the decode, not needed for correctness.
4. `pesimagetables` (batch AP) now binds some of the same ranges at runtime; the baked copies are redundant there but harmless (equal bytes). Drop the duplicates when convenient.
5. The first merge of `origin/main` failed on the `extern/godot-cpp` symlink: remove the symlink, `mkdir extern/godot-cpp`, merge, `rmdir`, relink.

## 7. Checks run

- `gen_translated.py --check`: byte-identical (4,346 functions) after every regeneration; lint green.
- translator unit tests (section 4), five selected `diff_*_recorded` (section 3), both 2-min smokes twice (section 5).
- Fast gate (`GATE_TIER=fast GATE_SLOTS="3 2"`, branch `8bc504dc`): **INTERIM, no final line**: my 3600 s tool cap killed it. Seen before the kill: build OK; selection 144 changed paths -> 32 pilot, 48 cutscene, 27 presentation, 186 core, 18 impure recorded; `smoke-off rc=0` (`e663d1e917d6c4f4`, 33628 ticks), `smoke-pesloop rc=0` (`980c54e27f93ced2`, 25942 ticks); ctest reached #1101 of 2157 with 0 failures (the Skipped lines are the capture-dependent oracles: AngleOracle, TrapOracle, DangerOracle, WalkOracle, UnitOracle, VfsInstall, StadiumInstall). The diff_*_recorded tests, the drift test and the rest of ctest were not reached; the same five selections, the lint and the drift check were run by hand above (section 3, section 7).
