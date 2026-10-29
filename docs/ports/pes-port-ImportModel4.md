# PES port: ImportModel4: image identity from the file, served pages, slim identity pages, memset model (28-10-26)

Branch `ImportModel`, head `b1d215dc` (`git ls-remote origin ImportModel` = `b1d215dc`). Commits: `005e1ec0` (red tests), `5f27b039` (identity and slim), `fbb05cf5` (memset model), `b1d215dc` (test expectation). Base: `cb45b37a`. Labels: CONFIRMED = read in tool output this session; INF = inferred, reason given; OPEN = not settled.

## 0. Status by item

| item | status |
|---|---|
| 1. recover slimmed captures (image identity, served pages), re-run 10 hooks | DONE for identity and served pages. Re-run: 5 of 10 hooks decided; 5 pending (§4). |
| 2. slim keeps headers and identity pages; red test | DONE (`5f27b039`). Merge of origin/main NOT done (§6). |
| 3. `VCRUNTIME140.dll!memset` model; 44C880 and 445E90 on full captures | memset model DONE (`fbb05cf5`). 44C880 / 445E90 on full captures NOT run (OPEN). |
| 4. allocator `0x1403FB410` model | NOT DONE. Diagnosed: it is not the current blocker for 480910 or 44C880 on slim captures (§4.3). |
| 5. final sweep, verdict table, FixDemo caller/wrapper | PARTIAL: 5 of 10 hooks; FixDemo caller/wrapper not run. |
| checks | GREEN, §5. Mutation checks NOT run. |

## 1. What changed (paths)

- `tools/dev/translate/full_snapshot.py`: `PeImage` (new): reads `pes_full.bin` (`PES_EXE`): ImageBase must equal `IMAGE_BASE`, `SizeOfImage`, `SizeOfHeaders`, sections, data directory 12. `servable(rva)`: a header page or a page of a non-writable section, outside the import table, and not in `UNSERVABLE_RVA` (`0x2647000`). `SnapMem(snap_dir, restore=None, image=None)`: `image=""` for none, default `PES_EXE`. New `_captured_page`, `_served`; `page_ok` = captured or served; `page_dropped` = mapped, not a hole, not captured, not served; `read` is per page within one mapping (the restore applies as before). `module_base`: any address in `[IMAGE_BASE, IMAGE_BASE+SizeOfImage)` is the image, with or without its header. `module_bases()` (public). SnapMem refuses an image whose SizeOfImage differs from a captured header.
- `tools/dev/translate/frame_trace.py`: `_fetch` routes image fetches by `module_base == IMAGE_BASE`. Deleted: the `PES_EXE = "PES2021.exe"` mapping-name fallback (cutover).
- `tools/dev/rendezvous/slim.py`: `identity_reads(cap)` (file pages read by naming every module: each mapping's MZ scan, each PE's header and export tables), `keep_for(cap, touched, size)`; `trim` keeps `keep_for`.
- `tools/dev/translate/import_stubs.json`: `VCRUNTIME140.dll` / `memset` `code_hex` model (`4989c9574889cf89d04c89c1f3aa4c89c85fc3`).
- Tests (red first, then green): `tools/dev/translate/test_image_identity.py` (9 checks), `tools/dev/rendezvous/test_slim_identity.py` (4 checks), `tools/dev/translate/test_memset_model.py` (6 cases).

## 2. Image identity: the served-page evidence (CONFIRMED)

- Survey of a slim capture (`live2/seq085`): the image's header page is dropped; `.trace` keeps 1,119 of 9,518 pages; `.rdata` 209 of 3,692; `.data` (writable) 132 of 1,834.
- Comparison of every captured non-writable image page of eight full captures (`live2/pid407792/seq001-008`, same launch) against `pes_full.bin` (scratch `verify_served.py`): per capture 13,667 pages identical; 26 pages differ only in hook-patch bytes (the rendezvous restore reproduces them); 1 page differs in non-hook bytes: `0x142647000` (`r--p`, 8 bytes: live `a5d46853a5d46853` at `0x142647C64`, file `00f0524700341E48`), identical across the eight captures. It is excluded (`UNSERVABLE_RVA`). Its cause is OPEN.
- The header's data directory 12 (IAT) reads `0x1B960000` (inside `.tls$`), and the code calls through slots in `.rdata` (`call [0x14252F340]`, CONFIRMED in the ImportModel3 thunk note). So the IAT is not taken from the header. The pages the loader writes (IAT, on `rw` mappings in the live capture) are excluded by the mapping-permission rule (INF: those pages do not appear among the non-writable mismatches above).
- Tests: `test_image_identity.py` 9/9 green (code identified with the header dropped; read-only page served; IAT and writable pages refused; no-image behaviour unchanged).

## 3. Slim identity pages (CONFIRMED)

- Cause: a replay that runs a DLL's code never reads its header or export directory (the module scan reads only mapping starts when a module is first needed), so a slim built from the replay alone drops them and the module names no export.
- Fix: `keep_for` unions the replay's read set with `identity_reads`. `test_slim_identity.py` 4/4 green. Mutation (`keep_for` without identity pages) NOT run (OPEN).

## 4. Hook verdicts (sweep `verdict11/sweep11.log`, 125 captures; `verdict/sweep10.log` for 123 captures before)

The sweep's replay stage ran over all 125 captures (103 traces rc=0, 22 rc=1 = a refused case). The snapdiff stage reached five hooks before the report; the remaining five are OPEN, not decided. "Before" = ImportModel3 table (123 captures, old code).

| hook | VA | before: replayed / calls | now: replayed / calls | excluded before -> now | mismatches | verdict now |
|---|---|---|---|---|---|---|
| foul_sanction_47F170 | 0x14047F170 | 0 / 14 | 14 / 14 | 14 -> 0 | 0 | PROVEN (every case bit-exact) |
| foul_sanction_caller_480910 | 0x140480910 | 0 / 7 | 0 / 7 | 7 -> 7 | 0 | NOT PROVEN: refused, `page not captured at 0x6FFFFBC73B0` (§4.3) |
| ref_involve_A3A690 | 0x140A3A690 | 18 / 34 | 62 / 62 | 16 -> 0 | 0 | PROVEN |
| ref_judge_A3A930 | 0x140A3A930 | 4 / 11 | 18 / 18 | 7 -> 0 | 0 | PROVEN |
| ref_latch_A3A870 | 0x140A3A870 | 2 / 9 | 9 / 9 | 7 -> 0 | 0 | PROVEN |
| ref_restart_cmd_A39A80 | 0x140A39A80 | 7 / 22 | not decided | 15 -> OPEN | OPEN | OPEN (snapdiff pending) |
| linesman_lookat_A38F20 | 0x140A38F20 | 4 / 16 | not decided | 12 -> OPEN | OPEN | OPEN (snapdiff pending) |
| restart_446390 | 0x140446390 | 2 / 9 | not decided | 7 -> OPEN | OPEN | OPEN (snapdiff pending) |
| restart_44C880 | 0x14044C880 | 4 / 15 | not decided | 11 -> OPEN | OPEN | OPEN: its recorded replay refuses at `0x6FFFFBC73B0` (§4.3) |
| restart_driver_445E90 | 0x140445E90 | 2 / 9 | not decided | 7 -> OPEN | OPEN | OPEN (snapdiff pending) |

Counts over the five decided hooks: excluded calls 51 -> 7 (480910 only). Calls and replayed counts differ partly because the capture set grew (123 -> 125 and new captures). Oracle: the replay verdicts (snapdiff) are the ruling. Hooks PROVEN: 4 of 10 (47F170, A3A690, A3A930, A3A870); 1 NOT PROVEN (480910); 5 OPEN.

### 4.1 Driver and generation (CONFIRMED)
- Not compared against the pre-change code in this session. The generation and driver checks in §5 are the evidence for the committed translations; the 4 PROVEN verdicts rest on the sweep's snapdiff output (`verdict11/sweep11.log`).

### 4.2 Item 3 (memset) (CONFIRMED)
- `VCRUNTIME140.dll!memset` is at RVA `0x19A0` (`0x6FFFFB8219A0` in `pid407792/seq001`). Its first instructions are a vector fill (movabs `0x0101010101010101`, imul, punpcklbw). The model implements the contract (low byte of c, n bytes, return dst, rdi/rsi kept), not the DLL's code. `test_memset_model.py` 6/6 green.
- 44C880 / 445E90 on full captures: NOT run in this session (OPEN).

### 4.3 Allocator and the remaining blocker (CONFIRMED / INF)
- `0x1403FB410` (CONFIRMED, `pes_full.bin`): reads its object from a global table (`mov rax,[rip+...]`, index `ebx*64`), then `call [rax+0x10]` at `0x1403FB47E` and `0x1403FB49F`, where `rax = [r14]` (the object's vtable). The target is read from the object at call time. The emulator (unicorn) already does that, so no model is needed unless the object or vtable page is missing from the capture.
- The 480910 and 44C880 refusals in this sweep are not the allocator: their recorded replays stop at `rip 0x6FFFFBC73B0` with `case excluded: page not captured at 0x6fffffbc73b0` (CONFIRMED, trace `.out`). That is a DLL export-entry page that the slim capture dropped (the DLL's code page was not in the replay's read set, and identity pages cover headers and exports only, not code). INF: the DLL code page at export entries must also be kept by slim (or its replay must be verified on a full capture). The allocator is therefore not the current blocker for these two hooks on slim captures; it still needs a full 480910 capture to decide (none in this launch, per ImportModel3 §4).

## 5. Checks (run on `b1d215dc`, CONFIRMED)

- `driver.py --check` (34 recorded VAs, `--record results_impure.json`, `--seed-bank <workdrive>/gf-Captures-scratch/banks_union9l`): 34/34 still diff bit-exact, rc 0.
- `gen_translated.py --image pes_full.bin --check`: 3951 functions byte-identical (3672 recorded bit-exact, 279 closure), rc 0.
- `check_runnable_extra.py`: GREEN (1906 runnable, 4 blocked for 0x140445E90), rc 0.
- `check_closure_stubs.py`: GREEN for 0x140A39A80 and 0x14044C880, rc 0.
- `test_snapmem_holes.py`: rc 0 (all checks ok).
- `test_image_identity.py`: 9/9 ok, rc 0. `test_slim_identity.py`: 4/4 ok, rc 0. `test_memset_model.py`: 6 tests OK. `test_strcpy_model.py`: OK.
- Mutation checks: NOT run. The brief asks for one per branch; this is OPEN.
- Gate: not run (Main gates).

## 6. Merge and peers

- Main's batch AL2 (`d6e99c8a`, RvReplay: closure stub_refs, referee universe rows, `slim.py --scratch`) landed on origin/main after my branch base. NOT merged. `slim.py` and possibly `full_snapshot.py` will conflict: merge `origin/main` before the branch lands.
- TrapReplay (peer message, out of scope): `snapdiff --only 0x14079AFD0` gives `translate 0x140A4EC7F: no instructions`: `runnable_extra` admits a walked callee whose address is mid-instruction. Not touched here.

## 7. Open items (numbered)

1. Merge `origin/main` (AL2) into `ImportModel`; resolve `slim.py` (keep AL2's `--scratch`, keep `keep_for`/`identity_reads`).
2. Finish the 10-hook sweep: `verdict11/sweep11.log` (the snapdiff stage was still running). Decide the five OPEN hooks from it.
3. Slim must keep DLL code at export entries that a replay enters (`0x6FFFFBC73B0` case), or the identity/served rules must cover DLL pages. Then re-run 480910 and 44C880.
4. Allocator `0x1403FB410`: no model needed if its object and vtable pages are captured; confirm on a 480910 full capture (none in this launch).
5. Measured divergent page `0x142647000` (8 bytes, r--p): explain (unpacker artifact or runtime write) and whether other launches differ. Guard: `verify_served.py` over full captures (scratch, not committed; to be promoted to a test on full captures).
6. Mutation checks (one per branch): `keep_for` without identity pages; `servable` without the writable check; `_served` without the mapping permission check; memset without `push rdi`/`pop rdi`.
7. 44C880 and 445E90 on full captures (item 3): run after item 2.
8. FixDemo caller `0x141F41CE0` and wrapper `0x141F4FDA0` (item 5): 18 caller captures and 4 matcher captures exist in `live`/`live2`; not yet replayed. Blocked by 3 and 4.
9. `import_slots.json` not regenerated (module discovery still uses mapping names; from ImportModel3).
