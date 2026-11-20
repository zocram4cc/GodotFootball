# pes-port-ReplayRecorder1: GetCurrentDirectoryW modelled (size and copy, from the process's own current directory); the proof harness answers a declared GF service; 1408BED00 proven but held out of the layer; 1408BE3C0 not proven (relocated code cave in the closure); ring bind and cursor not decoded

Branch `Presentation`, pushed `d3f04aa1..77d69487`. `git ls-remote origin Presentation` = `77d6948776432ea4ec5d0b790d7e0c30a47a191d` (confirmed). One commit, `77d69487`. Worktree `<workdrive>/gf-PresReplay2`: build dir removed; capture copies (`caps/`, six pid950952 seqs and pid1845381 seq7492) and their work dirs removed; scratch `<workdrive>/gf-ReplayRecorder-scratch` keeps logs and `humanupd_record.diff` (the held 1408BED00 record, not committed).

Labels: CONFIRMED = a unicorn test or mutation run, a `prove_bounded` record, a headless read of the flat image (`pes_full.bin`, file offset = VA - 0x140000000), `gen_translated --check`, `ledger.py --check`. INF / OPEN as marked.

## 1. GetCurrentDirectoryW (slice 1 item 1): DONE, committed

- The red test was not a gs-read fault. The loaded import table held TWO `GetCurrentDirectoryW` entries: a committed stand-in `cwd_w` (writes a fixed three-character drive-root directory, returns 3) and the new model. The dict kept the later one, so the test ran the stand-in (CONFIRMED: `load_import_models()` returned the 19-byte `cwd_w` code). The earlier "unicorn fault at 0x1000a" came from a malformed isolation probe, not from unicorn.
- Model rewritten (`getcurdirw_model.s`, code_hex 74 bytes, `as` + `objcopy`): size query when `nBufferLength <= len` (returns len+1, writes nothing); otherwise the NUL-terminated copy and return len. Reads the directory through gs:[0x60] -> PEB+0x20 -> parameters+0x38 (Length) / +0x40 (Buffer). No trap on the copy path (the previous design trapped there, which would have broken the start-up initializers that the stand-in served).
- `cwd_w` deleted with its import entry, its xlate branch, and its StartupOsModels case (clean cutover).
- GF runtime: `pesfoxworld.cpp` builds the process block in `FillThreadBlock` (PEB+0x20 -> RTL_USER_PROCESS_PARAMETERS at TEB+0x2000; CurrentDirectory UNICODE_STRING at +0x38; UTF-16 text at TEB+0x2100; TEB window 0x2000 -> 0x4000). The directory is `std::filesystem::current_path()`, so GF's own working directory. A directory the window cannot hold throws (not truncated). ponytail: a non-ASCII working directory needs a UTF-8 locale (the narrow-to-wide conversion is std::filesystem's).
- The C++ mirror `pes_model_getcurdirw` (pes_stubs.hpp) is the same copy path, register for register (r8 = last character, r10/r11 as the asm).
- Tests: `test_getcurdirw_model.py` 4/4 (size query; buffer equal to length; copy with terminator and nothing past it; empty directory). Mutants (run in memory by patching the loaded code, unmutated repo files; each red): no NUL store 2 failures; no character store 1 failure; size answer off by one 2 failures. `test_strstr_model.py` 40/40 (its cwd_w case removed).

## 2. Proof harness: declared GF service answers (slice 2 item 3, first half): DONE, committed

- `stubs.json` gfx_device_alloc (0x141957C50, a gf-service) now declares `value: 0x0` (GF's `pes_service_gfx_device_alloc` returns 0). `stubs.load` carries it; `stubs.run_stub` builds the harness entry; `run.py resolve_stubs` and `frame_trace.stubs_for_run` use it; the `service_` branch in `install_hooks` answers with it, or stops the replay when no value is declared (the old behaviour, kept for every other gf-service: fs_*, packer_*).
- Why a declared answer: NO capture records a return of 0x141957C50 (no hook names it in any cfg; the capture index holds entry state only). So the answer is the table's declared value, not a recorded one. Every proof that reaches it is conditional on that answer; both the PES replay and the translated side use it. This is the one open point the owner should confirm.
- Test `test_service_answer.py` 3/3 (declared answer returned and the call returns; a different declared value is what comes back; a service without a value stops). Mutant (answer write dropped): 2 red. Reverted; verified by diff.
- Side effect (CONFIRMED): `pes_service_gfx_device_alloc` is now `static inline` in `pes_stubs.hpp` (removed from `pes_native.cpp`) so the proof builds, which do not link pes_native.cpp, get one definition. Before this, `1408BED00` replayed and then failed to link.

## 3. 1408BE3C0 (ReplayHuman create): NOT proven, NOT landed

- The trace on pid950952 seq002 now returns cleanly (err None, recorded return) with the models above.
- Proof (6 real calls, seq002/004/006/008/010/012, copies unpacked): `calls=6 replayed=0 excluded=6`, error `closure walk: 0x141EC3F21 (from 0x14252E690) is not a function boundary`.
- Cause (CONFIRMED by disassembly and .pdata): 0x141EC3F21 is a `je` target INSIDE the .pdata function 0x141EC3A20..0x141EC42F4. The function reaches it through a relocated code cave at 0x14252E690 (`cmp dword [rsi+0x30],2; mov eax,0x3E; ...; mov eax,[rsi+0x28]; jmp back`), entered by `jmp 0x14252E690` at 0x141EC3F0D. The cave is game logic (field reads and a compare, no integrity check). The closure walker follows the out-of-line target and has no rule for a cave that returns into its owner function.
- Needs: a closure rule that treats a jump into a relocated cave as part of its owner function (translator change, with a test), then re-prove. Not done this session.

## 4. 1408BED00 (Human per-tick update): PROVEN on its capture, record HELD OUT of the layer

- Capture: pid1845381 seq7492 (hook rec_1408BED00; one call). Hook line via `untranslated_callees.hooks_for_image`: `hook rec_1408BED00 0x1408bed00 14 1 1 40534883ec204883b9d807000000`.
- Result (CONFIRMED, `prove_bounded --merge`, budget 200M): `calls=1 replayed=1 excluded=0 mismatches=0 own_block 7/7 (1.0)`. This is ONE call: thin evidence; the owner's correctness rule accepts it, coverage is recorded, not a gate.
- Why not landed: the merged closure (`closure_humanupd.json`) reaches `0x141EBBDB0`, which reads 32 `.rdata` addresses in 0x142B67F40..0x142B6898C (two dword arrays, 0x200 apart, e.g. 0x141EBBE6A reads 0x142B67F40) with no `ro_tables.json` declaration. `gen_translated.py` refuses that (SystemExit: live reads of a read-only image address; they read 0 at GF runtime). Declaring the tables needs the index range decoded from 0x141EBBDB0's loop (an OPEN decode). The record diff is in scratch (`humanupd_record.diff`), not committed; the closure file was removed.
- The three "held out ... mistranslated callee" lines (0x1403BB670, 0x1405549C0, 0x14202BA30) are existing `translator bug` records (e.g. results_core 0x140555110 "62 mismatch"); they print at every gen run and do not fail it.

## 5. Ring (slice 2): bind and cursor NOT proven

- Bind `140A8EE20`: no capture (Main: EVERY 1 on pid2168045 did not fire at kick-off). Static (CONFIRMED): no direct E8/E9 caller and no absolute pointer except two UnitReplay vtable slots: 0x142653F90 (vtable base 0x142653F58 + 0x38) and 0x142653F18 (base - 0x40). Reset `140A8ED70` is slot +0x08 (0x142653F60). So the bind is a VIRTUAL call (slot +0x38 of UnitReplay), reached from 1271 `call [reg+0x38]` sites in the image; I did not narrow them (a scan for a preceding `[reg+0x6A8]` unit load returned 0, but my byte test was wrong, so that scan proves nothing). OPEN.
- Likely caller path (INF): the unit is bound when a replay plays (after a goal or highlight), which fits Main's note that kick-off does not fire it. A hook that names the replay start (the FixDemo state-7 / `1423E0170` path, decode §5) would sample the bind's caller on that path.
- Cursor advance: not decoded (OPEN; the unit reset and bind do not advance `+0x10`).

## 6. Layer, build, checks

- `gen_translated.py --image pes_full.bin` regenerated (10232 functions, 3832 recorded, closure 6400); `--check`: byte-identical, rc 0. Changed: closure `pes_fn_14037FAF0.cpp`, `pes_fn_14037FBA0.cpp`, `pes_fn_1403BAC60.cpp` (the start-up initializers that call GetCurrentDirectoryW), plus `pes_native.cpp`, `pes_stubs.hpp`. Recorded functions unchanged.
- `ledger.py --check`: LEDGER.md current (44,479 rows), no regression.
- Built (flock, nice, -j8): `pes_replay_human_test`, `pes_native_service_test`, `pes_hud_clock_test`, `pes_anime_engine_test`. Run with `env -u GF_DEBUG_RECEIVE -u GF_DEBUG_ANKLE -u GF_PES_SURVEY`, PES21_ROOT, PES_EXE set:
  - pes_replay_human_test: 5 pass, 3 skip (GF_PES_COPY_CAPTURES not set; the copy proofs did not run here).
  - pes_native_service_test: 7/7.
  - pes_hud_clock_test: 4/4.
  - pes_anime_engine_test: 11 pass, 1 skip (render-plot env var). It links PesFoxWorld, so it exercises FillThreadBlock.
- Not run: the Main gate (agents do not run it).
- Pre-existing, not mine: `pes_fox_world_test` does not compile at HEAD (`pes_fn_1403CF740` and `pes_fn_1403FB410` are not declared in translated.hpp at HEAD). Not in the four named targets; flagged for Main.

## 7. Process notes

- The `commit` namespace fails on this worktree (`expected submodule path 'extern/godot-cpp' not to be a symbolic link`), the symlink the brief warned about. I committed with path-limited git (explicit paths, owner identity, `status.submodule=none`); extern/godot-cpp not included.
- A `/tmp` file was briefly created for an assembly check and removed. The build lock `/tmp/gf-build.lock` is the brief's.
- `gen_translated.py` run through bash exceeds the tool's 300 s deadline; run it with no deadline.

## 8. Open items (numbered)

1. Owner to confirm: the gfx device allocator's declared answer (0x0) is the proof's boundary, not a recorded result (section 2). Every proof reaching 0x141957C50 is conditional on it.
2. 1408BE3C0: a closure rule for relocated code caves (0x14252E690 -> back into 0x141EC3A20 at 0x141EC3F21), with a test, then re-prove on the six seqs (section 3).
3. 1408BED00: decode the read-only tables 0x142B67F40.. and 0x142B68140.. in 0x141EBBDB0 (index range from its loop), declare them in `ro_tables.json` with CONFIRMED evidence, then merge the record (section 4). The record diff is in scratch.
4. Bind 140A8EE20: capture it on a replay start (hook the FixDemo state-7 path or `1423E0170` with a predicate; batch with Main's next restart). Static caller set is 1271 indirect sites (section 5).
5. Cursor advance (`+0x10` of the unit, wrap 660): not decoded; needs the bind and the writers' consecutive captures.
6. `pes_fox_world_test` compile failure at HEAD (section 6): owner or Main to decide.
7. The `commit` namespace cannot see this worktree (extern symlink): Main to fix the symlink or commit from a clean checkout.
8. Slice 3 (ReplayHuman playback from recorded unit frames) and slice 4 (state-7 create) not started.
