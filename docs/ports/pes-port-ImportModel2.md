# PES port: ImportModel2: 480910 regression root cause, strcpy_s model, and what is not done (28-10-26)

Branch `ImportModel`, pushed head `d9b4583e` (`git ls-remote origin ImportModel` = `d9b4583e`). Base `2eccea24`. Commit `d9b4583e` adds the named `ucrtbase.dll!strcpy_s` model and its test, nothing else. The harness work is **not** on the branch: it is saved as `<workdrive>/gf-ImportModel-scratch/wip-ImportModel2-harness.patch` (339 lines) and reverted because it breaks 445E90's PROVEN status (§1.4).

Labels: CONFIRMED = read in tool output this session; INF = inferred, reason given; OPEN = not settled.

## 0. Summary

- **Item 1, 480910 regression (7 to 5): root cause CONFIRMED, regression NOT closed.** The callee is the allocator wrapper `0x1403FB410`. Its code page is absent from the slim captures, and the harness read absent pages as zeros. The fix that stops the false read is in the WIP patch; it does not restore the three cases, because the stack page at RSP is also absent from those captures.
- **Item 2, strcpy_s: model added by name and tested (`d9b4583e`), not reachable yet.** The thunk is `strcpy_s` (CONFIRMED against the PES prefix's own DLL). Name resolution for DLL code is broken (cause found); the WIP fix breaks 445E90, so it is not landed.
- **Item 3, `sqrt` in `reach_stubs`: NOT investigated (OPEN).**
- **Item 4, `140A79640` and `1404488E0`: NOT done.**
- **Item 5, verdicts over all surviving captures: NOT re-run.** The table in §5 is the earlier 66-capture sweep, unchanged. The gates were run on the WIP tree (driver) and the committed tree (gen).

## 1. Item 1: the 480910 regression

### 1.1 The callee (CONFIRMED)

- Replay of `live2_seq088` (`run.py --spec 140480910.json --snapshot-dir live2_seq088 --calls trace/.../140480910.bin`) reports `unmapped read at 0x0 (size 1)`. I added the faulting RIP to the message (temporary, reverted; the WIP patch keeps it). The RIP is `0x1403FB410`, the entry of the allocator wrapper; the trail of executed code is `...0x14047BB48 (call 0x1403FB410)` then `0x1403FB410`.
- The snapshot's bytes at `0x1403FB410` are zeros. Its mapping is `r-xp`, `deferred: True` in `index.json`, and the page is a sparse hole in `mem.bin` (CONFIRMED by `full_snapshot.SnapMem`). The first zero bytes decode as `add byte ptr [rax], al` with `rax = 0`, which is a 1-byte read at `0x0` (CONFIRMED: register dump at the fault shows `rax=0x0`).
- The image bytes at that VA (`pes_full.bin`) are the real allocator: `mov [rsp+0x10],rbx ... mov ebx,[rcx]; ...; movzx eax, byte [rdx+rax]`. The disk file is correct there; the snapshot was not.

### 1.2 Why it appeared now (CONFIRMED, mechanism; INF on the slim rule)

- The wider walk (budget 2000, depth 80, commit `2eccea24`) puts `0x1403FB410` into 480910's closure. The earlier budget left it out.
- `slim.py` keeps only the pages its recorded replay read (plus neighbours). Its validation checks only that recorded replay. The walk's callees read pages outside that set, and those pages are sparse zeros in the slim file.
- The same captures also lack the stack page at the call's RSP (`0x1cb5f8d8`, `rw-p`): `snapcase.py:206` reports `stack slot at rsp not in the snapshot` once the code page is not zero. So the three cases are not replayable from these captures at all. INF: slim dropped them for the same reason.

### 1.3 What the fix does (WIP, not on the branch)

- `SnapMem` reads the file's allocated extents (`SEEK_DATA`/`SEEK_HOLE`). A page not in an allocated extent is a hole, never zeros.
- A hole page that is executable code of the PES image (name `''`, `r-x`, VA inside the image) reads from `pes_full.bin` at `VA - 0x140000000`. Evidence it is the same bytes: `verify_snapshot.py` on `live2_seq088`: 592 kept code pages equal the image; 106,253 zero pages are the dropped set; 26 non-zero differences are rendezvous hook jumps (`ff25…`), which `restore` already covers.
- Result on the three cases: `unmapped read at 0x0` is gone; they now exclude with `stack slot at rsp not in the snapshot`. Honest, but the regression is not closed.

### 1.4 Why it is not landed

- With the WIP patch, 445E90 (3 captures, previously PROVEN 3/3) excludes at `fetch from unmapped or unreadable snapshot memory at 0x6FFFFFBC73B0`. That is `ntdll.dll` code (its mapping is named `/usr/lib/wine/x86_64-windows/ntdll.dll`, module base `0x6FFFFFBA0000`); the page was dropped by slim, and the image fallback covers only the PES image. CONFIRMED.
- The earlier PROVEN verdict ran over zero-filled dropped pages with no refusal. Whether it executed any dropped page is OPEN. Landing the patch would flip a PROVEN verdict on an unresolved question, so it is reverted.

### 1.5 Closing the 7 to 5 regression

- The three cases need their stack and code pages. The `live2` captures no longer have them (slimmed; the full `mem.bin` was removed). They cannot be recovered.
- The current launch (`live2/pid161465`) has unslimmed captures (about 3.9 GB each), but none for `foul_sanction_caller_480910` yet (CONFIRMED: 20 `pid161465` hook rows, none 480910). A 480910 capture from this launch is what closes the regression.
- Ceiling to fix in `slim.py`: keep every page the closure can touch, or record the dropped pages as holes in `index.json` (OPEN).

## 2. Item 2: the ucrtbase thunk `0x6FFFFEC77E00` is `strcpy_s`

### 2.1 Identity (CONFIRMED)

- The PES prefix's DLL is `<local>/Games/Heroic/Prefixes/default/eFootball PES 2021/drive_c/windows/system32/ucrtbase.dll` (986,504 B, SizeOfImage `0xF4000`). Export RVA `0x17E00` = function index 2383, ordinal 2384, name `strcpy_s` (`dll_exports.py`). RVA `0xA550` = `_errno`; RVA `0x684C0` = `_invalid_parameter_noinfo`.
- Correction to Main's note: the path is under `eFootball PES 2021`. The `Prefixes/default/drive_c/.../ucrtbase.dll` file is SizeOfImage `0x44A000` (879,134 B, Wine builtin copy), which is a different DLL. Main's note should cite the `eFootball PES 2021` path.

### 2.2 Behaviour (CONFIRMED from the loaded DLL's bytes in `live_seq085` at `0x6FFFFEC77E00`)

- `dst == 0` or `size == 0`: EINVAL (0x16), no write.
- `src == 0`: `*dst = 0`, EINVAL.
- Copy: `src` to `dst` up to and including its NUL, within `size`: returns 0.
- No NUL within `size`: `*dst = 0`, ERANGE (0x22 = 34).
- The DLL also stores errno through `_errno` and calls `_invalid_parameter_noinfo`. The model does neither (ponytail; OPEN: whether the game reads errno after `strcpy_s`).

### 2.3 Model and test (committed, `d9b4583e`)

- `tools/dev/translate/import_stubs.json`: entry `ucrtbase.dll` / `strcpy_s`, `code_hex` 62 bytes (hand-encoded; disassembly checked with capstone), `why` text.
- `tools/dev/translate/test_strcpy_model.py`: 6 cases, one per path (null dst, zero size, null src, fits, empty src, too long). Loads the model by `(module, export)` as `LazyMap` does. `OK` (6 tests) on the committed tree.
- Red: before this entry the lookup `load_import_models()[("ucrtbase.dll","strcpy_s")]` raises `KeyError` (by construction; not re-run).
- Not yet reached in the replay: see §2.4.

### 2.4 Why the model is not reached (CONFIRMED cause; fix in WIP)

- `full_snapshot.export_of` and `entries_of` identify a module by its **mapping name**. A DLL's code sections are unnamed (only its header page is named), so `frame_trace._fetch` treats DLL code as PES image code (`if not m["name"]: handle`) and never checks the export table. `0x6FFFFEC77E00` therefore never gets its name.
- WIP fix (in the patch): module identity from the nearest `MZ` mapping whose SizeOfImage covers the VA; the module name from the export directory's `Name`; `_fetch` routes DLL code to export entries (a modelled export runs its model, any other stops the run); a model's bytes must not cover another export entry (`int3` there).
- The WIP fix also needs two follow-ups, both OPEN:
  1. DLL code pages dropped by slim (like `ntdll` `0x6FFFFFBC73B0`, §1.4) need a file fallback: the module's file on disk with the PE section map. Without it the fix breaks 445E90.
  2. `import_slots.py` module discovery still uses mapping names. Regenerating `import_slots.json` from `live_seq085` with the WIP fix marks 157 previously named slots `unmapped` and names none of the `ucrtbase` slots (`0x142530660` stays `unmapped`). Not adopted; `import_slots.json` is unchanged on the branch.
- So the abort `PES_DISPATCH_UNTRANSLATED: 0x6FFFFEC77E00` in `live_seq085/089` (translated side) is unchanged. The harness now refuses earlier, at a dropped `ntdll` page (§1.4).

## 3. Item 3: `sqrt` (`0x1415A1780`) and the cookie missing from `reach_stubs`: NOT investigated (OPEN)

- The 44C880 exclusion remains `unmodellable callee 0x1415a1780 entered` (`sweep.json`).
- Not started. Next step: trace `reach_stubs` (`tools/dev/diffharness/check_closure_stubs.py`, `tools/dev/translate/stubs.py`) for why the 215 closure functions that call `0x1415A1780` or `0x14159F3E0` directly are not reached.

## 4. Item 4: `140A79640` and `1404488E0`: NOT done (OPEN)

- `140A79640` (`[RSP+0x98]` call-time pointer; `RtlLeaveCriticalSection` model): not started. The `RtlLeaveCriticalSection` slot is named `ntdll.dll!RtlLeaveCriticalSection` in `import_slots.json` (CONFIRMED, from the doc's prior work).
- `1404488E0` case bodies: not enumerated.

## 5. Item 5: verdicts and checks

### 5.1 Verdicts (`sweep.json`, 66 captures; NOT re-run over the 113 surviving captures)

| hook | VA | calls | replayed | bit-exact | excluded | mismatched | verdict |
|---|---|---|---|---|---|---|---|
| foul_sanction_47F170 | 0x14047F170 | 20 | 20 | 20 | 0 | 0 | PROVEN |
| foul_sanction_caller_480910 | 0x140480910 | 10 | 7 | 5 | 3 | 2 | NOT PROVEN (§1, §2) |
| ref_involve_A3A690 | 0x140A3A690 | 22 | 22 | 22 | 0 | 0 | PROVEN |
| ref_judge_A3A930 | 0x140A3A930 | 12 | 12 | 12 | 0 | 0 | PROVEN |
| ref_latch_A3A870 | 0x140A3A870 | 6 | 6 | 6 | 0 | 0 | PROVEN |
| ref_restart_cmd_A39A80 | 0x140A39A80 | 6 | 6 | 6 | 0 | 0 | PROVEN |
| linesman_lookat_A38F20 | 0x140A38F20 | 6 | 6 | 6 | 0 | 0 | PROVEN |
| restart_446390 | 0x140446390 | 7 | 7 | 7 | 0 | 0 | PROVEN |
| restart_44C880 | 0x14044C880 | 9 | 8 | 8 | 1 | 0 | NOT PROVEN (§3, sqrt) |
| restart_driver_445E90 | 0x140445E90 | 3 | 3 | 3 | 0 | 0 | PROVEN (committed tree; the WIP harness would make it 0/3, §1.4) |

- Surviving captures not in this table: 47 (`live2` and `pid161465`, `captures_all_rv.tsv`: 58 `live`, 35 `live2`, 20 `pid161465` rows). Not swept. The 480910 `live2` cases use the slim captures described in §1.

### 5.2 Gates and tests

- `gen_translated.py --image pes_full.bin --check`: rc 0, "3951 functions (3672 recorded bit-exact, 279 closure) regenerate byte-identical" (run with the committed `import_stubs.json`).
- `driver.py --check --spec-dir specs/impure` with `--record` = a scratch copy of `results_impure.json`: rc 0, "all 34 recorded-diffed functions still diff bit-exact". Run on the WIP tree (harness patch applied), not the committed tree; the committed harness is HEAD.
- `test_strcpy_model.py`: 6 tests, OK (committed tree).
- `check_runnable_extra.py`, `check_closure_stubs.py`: NOT run.
- Mutations: NOT run.

## 6. Open items (numbered)

1. **480910 7 to 5 regression**: needs a 480910 capture with its stack and code pages (from `live2/pid161465`, or a new launch); or `slim.py` must keep the closure's pages. Root cause in §1.1.
2. **Harness fix for absent pages** (WIP patch, `<workdrive>/gf-ImportModel-scratch/wip-ImportModel2-harness.patch`): needs (a) a file fallback for DLL code pages (PE section map from the module's file, verified like `verify_snapshot.py`), (b) `import_slots.py` module discovery by `module_base`, before it can land. Until then 445E90 is 0/3 under it.
3. **ucrtbase `strcpy_s` model**: committed; reached only after item 2. Also decide whether the game reads errno after `strcpy_s` (§2.2).
4. **`reach_stubs` misses `sqrt` and the cookie** for 44C880 (§3): not investigated.
5. **`140A79640`** (`[RSP+0x98]`, `RtlLeaveCriticalSection`) and **`1404488E0`** case bodies (§4): not done.
6. **Main's note**: cite the `eFootball PES 2021` prefix path for `ucrtbase.dll` (§2.1), not `Prefixes/default/drive_c`.
7. **Verdict sweep over all 113 surviving captures** (`captures_all_rv.tsv`): not run.
8. **`check_runnable_extra.py`, `check_closure_stubs.py`, mutations**: not run.
