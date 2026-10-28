# PES cutscene hybrid: selection FAILs, GF-callable selection (not built), and the `.fdc` parser (CutsceneHybrid, 28-10-26)

Branch `CutsceneSelect`, worktree `<workdrive>/gf-CutsceneSelect`. Pushed: `906404f2` (merge of `origin/main` 886bde16) and `57d95744` (parser); `git ls-remote origin CutsceneSelect` = `57d95744`. Scratch `<workdrive>/gf-CutsceneSelect-scratch/` (PES-derived, never committed). Gate not run (Main gates). PES was live (pids 407792/161465 captures); it was read only, never written, paused or signalled.

Labels: CONFIRMED = read from tool output, replay bytes, install bytes or disassembly; INF = inferred, reason given; OPEN = not settled, blocker named.

## 0. Summary against the ticket

| item | status |
|---|---|
| 1. merge `origin/main`, push | DONE. Clean merge; 8 files from main (CutsceneSelect report, ImportModel2, RecordMap, TrapClipPort). |
| 2a. `141F5F330` seq083 FAIL | BLOCKED, precisely located (§2.1). Not fixed. |
| 2b. `141F4B6C0` x4 import thunk `0x6FFFFA6612F0` | NOT AN IMPORT (§2.2). Nothing to model; ImportModel3 told. |
| 2c. `141F50FC0` "emu error 16" | CORRECTED and blocked (§2.3): the 16 is a count; the real fault is `UC_ERR_READ_UNMAPPED`. |
| 3. GF-callable selection → `FixDemoSceneRequest` | NOT BUILT (§3). 0 of the captured events produced. No GF choice code deleted; hand code named for the next slice. |
| 4. `.fdc` parser and format doc | DONE for the parser and the install run (§4). 4,910 of 4,925 consume every byte; 15 OPEN. Format doc `pes-re-fdc-format.md`. |
| 5. tests: red→green for GF glue; one mutation | PARTIAL. No GF glue, so no glue test. Parser selftest and one mutation (§5). |

## 1. Merge

`git merge origin/main` (the `extern/godot-cpp` symlink did not block the merge; `status`/`fetch` print a submodule-symlink error but still work with `-c status.submodule=none`). Result `906404f2`, pushed. CONFIRMED.

## 2. Selection FAILs (oracle: replay of PES bytes from the captured entry state; snapdiff per hook)

Data: `replay/verdict/0X<VA>.json` (snapdiff results) and `replay/build/gen/<VA>/snap_<label>/corpus.jsonl` and `translated.jsonl` (full write streams). Write streams are 18-hex entries: 8-byte address and 1-byte value (CONFIRMED from `snapdiff.py` `first_write_diff`).

### 2.1 `141F5F330` seq083: first diverging write (BLOCKED)

- `rax`: harness `0x3DC30250`, translated `0x3616B4B8` (CONFIRMED).
- Write streams: harness 6,128 entries, translated 5,582. The first 5,110 entries are identical (address and value). Entry 5,110 (0-based, `first_write_diff`):
  - harness: address `0x1C35F830`, value `0x10`; the next three entries write `0x1C35F831..33` (`0xA3`, `0xC8`, `0x2A`).
  - translated: address `0x1C35F848`, value `0x00`; then `0x1C35F840` (`0x01`), `0x1C35F818` (`0x00`), `0x1C35F819` (`0x00`) (CONFIRMED).
- So the two runs take different paths from entry 5,110. This is control flow, not one wrong store.
- Function (decompile, dec.sh, `FUN_141F5F330`, CONFIRMED): reads the record-kind byte at `*(param_1+8)+2`, dispatches (kinds 5, 2, 3, 4, 6, 7 → `FUN_141F5F480`, `FUN_141F5FA90`, `FUN_141F5FC00`, `FUN_141F60140`, `FUN_141F60590`, `FUN_141F5F610`; else `FUN_141F561A0`), calls `FUN_141F57540`, then a dedup loop that compares `(char)*puVar4 == (char)*puVar7` and calls `(*_DAT_14252FD68)(dst, src, n)` (a memmove-shaped call through a data slot).
- The slot `0x14252FD68` reads `0` in the seq083 snapshot (CONFIRMED, `mem.bin`), and `import_slots.json` lists it as `unmapped`. The next slot `0x14252FD70` is `memcpy` (msvcrt). INF: either the decompile's slot label is off by 8, or the dedup branch is not taken in PES's replay and the translated side takes it. Not settled.
- **Cause: OPEN.** Next step: record the instruction (RIP) of write #5,110 on both sides, with a write hook in the harness; that needs a harness change, which ImportModel3 owns. Not done here.

### 2.2 `141F4B6C0` ×4: `0x6FFFFA6612F0` is not an import (CONFIRMED)

- The mapping containing it is `comctl32.dll`. The prefix's `system32\comctl32.dll` (PE32+, ImageBase `0x180000000`, SizeOfImage `0x4A6000`) matches the mapped span `0x6FFFFA630000`..`0x6FFFFAAD6000` (`0x4A6000`).
- RVA `0x312F0` (base `0x6FFFFA630000`) is `00 00` = `add byte ptr [rax], al`, zero padding before `lea rcx,[rsp+0x70]` at `0x312F2` (`call rsi` follows). Read from the snapshot's `mem.bin` and from the file; they agree.
- No export of that DLL has RVA `0x312F0` (176 exports checked), so it is not an import thunk.
- No 8-byte slot in the seq002 snapshot's `mem.bin` holds `0x6FFFFA6612F0` or `…12F1` (full search): the target is computed in a register.
- Verdict: a computed indirect target that points into alignment padding. There is no import model to write. Message sent to ImportModel3.
- **OPEN:** which indirect site in `141F4B6C0` (or its callee) computes the target. Not identified.

### 2.3 `141F50FC0` ×16: "emu error 16" is a count (CONFIRMED)

- The verdict's `reasons` is `{"emu error": 16}`: 16 excluded cases, not a Unicorn code. The per-snapshot corpus rows say `emu error: Invalid memory read (UC_ERR_READ_UNMAPPED) (last RIP 0x141f50ff3)` for all 16 snapshots.
- `0x141F50FF3` is `call 0x141F4D2F0` (the FixDemo condition evaluator; disassembled from `pes_full.bin`, CONFIRMED). The read fault is inside the evaluator's path, from the caller's frame.
- The faulting address is not recorded (the harness's `hook_bad` message does not carry it). **OPEN:** print the address from `mu._bad` in the harness (harness change; ImportModel3's area).

## 3. GF-callable selection (ticket item 3): NOT BUILT

Reasons, each CONFIRMED by repo search:

1. The chain is not committed as translated code. `src/pes/translated/` (committed by `gen_translated.py`) has no `pes_fn_141F4B1D0`, `141F4D2F0`, `141F4FDA0`, `141F41CE0`, `141F4B6C0`, `141F5F330`, `141F616F0`, `141F40C00` or `14047F170` files. Those functions run only in the scratch build (`replay/build/gen`).
2. Committed translated code runs on the emulated `PesState`/`PesMem` (`src/pes/translated/translated.hpp`, `pes_state.hpp`). No GF caller of any cutscene function exists. A GF caller must build the FixDemo manager arena: the VFS tables, the per-manager cache at `+0x30`, the history at `0x1437F5DC0 + cat*0x40`, and the event state.
3. There is no C++ decoder for `table_<cat>.bin` (T1/T2). Only `tools/dev/cutscene/scene_map.py` (Python) exists.
4. GF's cutscene choice code: no sanction (foul card) choice exists. `foulsequence.hpp` holds timing only. The goal-celebration hand code is `src/sim/onthepitch/goalcelebration.cpp`: `SeedFor` (line 71), `Choose` (line 81), `PickCamera` (line 98). It is named here for the next slice and not deleted, because no PES replacement exists to cut over to.

So the acceptance "GF produces PES's FixDemoSceneRequest for captured events (case counts)" is **not met**: 0 of the 6 sanction captures in `replay/` (seq084, 086, 088, 090, 096, 098; scene ids in the cutscene report §2) were produced by GF. No test was written, so none was seen to fail.

**Next slice (named):** commit the translated chain (`gen_translated.py`: `141F4FDA0`, `141F4B1D0`, `141F4D2F0`, `141F41CE0`, then the table decode); add a GF-side manager arena builder; then a test that feeds the six sanction scene ids (and the captured matcher inputs) and asserts the request equals the replayed PES output.

## 4. `.fdc` container and records (item 4)

- Parser: `tools/dev/cutscene/fdc_parse.py` (committed, `57d95744`). Reads the install through `pesfmt_dump ls`/`cat` (default path is the worktree's `build/`; pass `--pesfmt` to a built binary). Layout from `docs/re/pes-re-cutscenes.md` §3; field offsets for types 0, 2, 4, 5, 6 (and the partial type 1) from the doc. Every other type is kept as hex and marked OPEN.
- Install run (CONFIRMED): **4,925 files** listed (`ls` of nine categories; the doc says 4,925), 4,925 parsed, 0 parse errors. Records: Cut 2,428; Env 1,444; Timing 3,507; Effect 5,881; Human 6,924; Object 4,830; Camera 9,302; Function 624; HumanControl 3; Disabled 15.
- Object: 4,830 type-5 records, of which 420 are 440 bytes. The doc's 4,410 counts only the 444-byte variant. Corrected in the format doc.
- Coverage oracle: **4,910 of 4,925 files consume every byte.** Zero-valued alignment padding inside the cut block is counted apart (39,509 bytes over the install).
- **15 files fail (OPEN):** all are the 15 type-99 `Disabled` records. Their type byte is `0x63` ('c'), the first character of the file's own path; the entry-0 name pointer then reads `dat/common/…`, so 3 bytes (`pk_`) are claimed by nothing. The parser reports them `ok: false` rather than hiding them.
- Format doc: `pes-re-fdc-format.md` (for `docs/re/`, archived by Main). It holds the container, the block and type pool, the padding rule, every record class with its layout status, what Godot playback needs (layers, actor tracks, camera tracks, timing, asset references), and the open items.
- Not checked: decoded field values against playback (no replay oracle for `.fdc` yet).

## 5. Tests and mutations

- `fdc_parse.py --selftest` (in the script, an assert demo per the ponytail rule): a file built from the layout parses with zero gaps; three stray bytes appended are reported as a gap of 3. Passes.
- **Mutation (ran, reverted):** on a scratch copy of the parser, the record type-byte claim removed. Selftest fails: `AssertionError: {'gap_bytes': 1, 'gap_offsets': [44], …}` (CONFIRMED, exit 1). The repo file was never edited for the mutation.
- Earlier red runs (recorded so they are not lost): the first parser version failed the selftest on `_cstr` bounds (a name near the file end), and on a whole-block claim that overlapped its parts (`overlap_bytes: 81`). Both fixed in the committed version.
- Install full run after each rule change: 4,925 files each time; coverage failures went 4,925 → 3,699 (pad rule per type-pool byte was missing) → 15 (pad rule refined to "zero and unreferenced") → 15 (final).
- GF glue red test and its mutation: not written (no glue, §3).
- Gate: not run (Main gates). No C++ test binaries were built or run.

## 6. Open items (numbered)

1. `141F5F330` seq083: first diverging write entry 5,110; cause OPEN. Needs the RIP of write #5,110 on both sides (harness write hook), and a check of slot `0x14252FD68` against `memcpy` (`0x14252FD70`).
2. `141F4B6C0` ×4: the dispatch site that computes `0x6FFFFA6612F0` (padding in comctl32) is OPEN. No import model applies (ImportModel3 told).
3. `141F50FC0` ×16: `UC_ERR_READ_UNMAPPED` at the evaluator call `0x141F50FF3`; the faulting address is OPEN (harness change needed to print it).
4. GF-callable selection (item 3): NOT BUILT. Next slice: commit translated chain, arena builder, a test on the six sanction captures. Hand code named: `goalcelebration.cpp` `SeedFor`/`Choose`/`PickCamera`.
5. `.fdc`: 15 type-99 files do not consume every byte (§4). Effect, Function, HumanControl and Env-flag offsets are OPEN (decode `FUN_141F3E050`, `FUN_141F3E530`, `FUN_141F3E6D0`, `FUN_141F3E1F0`).
6. Timing `word0` split and Human IK byte pairs: INF med in the doc.
7. Mode table's 21 dangling `.fdc` names (485 references) are not mounted: the parser never sees them.

## 7. Files

- Repo (branch `CutsceneSelect`, `57d95744`): `tools/dev/cutscene/fdc_parse.py`.
- Local, for `docs/re/`: `pes-re-fdc-format.md`, this report.
- Scratch only: `fdc_install.json` (parsed install), `fdc_install.out`, `pe_name.py` (PE export and SizeOfImage reader), the replay build and verdict files.
