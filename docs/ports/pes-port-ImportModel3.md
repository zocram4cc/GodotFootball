# PES port: ImportModel3: slim-page refusal, DLL identity, reach_stubs cause, ten-hook verdicts (28-10-26)

Branch `ImportModel`, pushed head `cb45b37a` (`git ls-remote origin ImportModel` = `cb45b37a`). Commits: `dac5058d` (red test), `941e91ac` (fix), `cb45b37a` (frame_trace). Base `d9b4583e`. Gate not run (Main gates). Labels: CONFIRMED = read in tool output this session; INF = inferred, reason given; OPEN = not settled.

## 0. Summary

- **Item 1a, missing page never read as zeros: DONE.** A page a slim capture dropped is a hole of `mem.bin`. `SnapMem.page_ok` now requires the page's bytes to be allocated (SEEK_DATA/SEEK_HOLE). Refusals say `case excluded: page not captured at 0x...`. Red on `dac5058d`, green after. Mutation run (§2).
- **Item 1b, DLL identity from mapped memory: DONE for full captures, NOT for slim.** Module = nearest preceding `MZ` mapping whose SizeOfImage covers the VA; export names read from the module's own export directory. On full captures `ucrtbase.dll` at `0x6FFFFEC60000` reads 2480 exports, RVA `0x17E00` = `strcpy_s` (CONFIRMED, pid407792 seq022-116). On slim captures the header/export pages are gone: 0 exports, refused.
- **Item 1c, 480910's two aborts (`0x6FFFFEC77E00`): NOT CLOSED (OPEN).** Both aborts are in slim captures (`live/seq085, seq089`). No full 480910 capture exists yet (§4, item 2).
- **Item 2, 480910 replay on full captures of the current launch: NOT DONE (blocker, §4).**
- **Item 3, `reach_stubs` misses `sqrt` `0x1415A1780` and cookie `0x14159F3E0` for 44C880: CAUSE CONFIRMED, FIXED, check GREEN.** The walk used `body()`, which does not follow switch tables. 44C880's body is 37 instructions ending in a computed `jmp rcx` at `0x14044C8F5` and has no direct call (CONFIRMED). The ledger's universe walk decodes that switch and reaches 2134 functions from 44C880, including both stubs (CONFIRMED). Fix: `closure_edges` takes the ledger's walked edges (direct, tail, table) and falls back to `body()` only for unwalked functions.
- **Item 4, ten-hook verdicts over all 123 surviving captures (30 full, 93 slim): proven 0 of 10.** Table §5.
- **Checks green on the committed tree (run after the last commit):** `driver.py --check` (34/34 bit-exact), `gen_translated.py --check` (3951 byte-identical), `check_runnable_extra.py` GREEN, `check_closure_stubs.py` GREEN (both hooks), `test_snapmem_holes.py` rc 0.

## 1. What changed (paths)

- `tools/dev/translate/test_snapmem_holes.py` (new, `dac5058d`): six checks on a sparse `mem.bin` with a dropped middle page. Exit 77 when the file system keeps no holes.
- `tools/dev/translate/full_snapshot.py`: `_data_extents` (SEEK_DATA/SEEK_HOLE); `SnapMem._captured`, `page_dropped`, `page_ok` (gated on capture); `IMAGE_BASE`; `module_base`, `_size_of_image`, `_exports_of`, `export_of`, `_parse_exports` (returns the module's Name and its sorted exports; empty when the tables are not captured), `entries_of`. The index-hole rule is unchanged.
- `tools/dev/translate/frame_trace.py`: `LazyMap.unmapped_reason` (named exclusion for a dropped page); `_fetch` routes by module identity (image at `IMAGE_BASE`, DLL only at export entries, unidentified code refused); `PES_EXE` mapping-name fallback for the image when its header is dropped; `recorded_main` no longer crashes (`KeyError: 'kept'`) when a run is refused before its trace; a hook entry on a dropped page is refused when entered, not at setup.
- `tools/dev/diffharness/run.py` (`hook_bad`) and `snapcase.py` (stack slot): use `unmapped_reason`.
- `tools/dev/translate/import_slots.py`: `_parse_exports` call replaced by `_exports_of(base)[1]`. `import_slots.json` NOT regenerated (its module discovery still uses mapping names; OPEN).
- `tools/dev/diffharness/specgen/classify.py`: `ledger_callees`, `closure_edges`, `reach_stubs` (walks `closure_edges`).
- `tools/dev/diffharness/check_closure_stubs.py`: now checks `ref_restart_cmd_A39A80` and `restart_44C880`. RED on the committed tree for 44C880 (`stub_refs []`, `missing ['14159F3E0','1415A1780']`), GREEN after.

Committed impure specs (`specs/impure`, 50 files): `stub_refs` recomputed with the fixed walk: no change (CONFIRMED). Scratch specs for the ten hooks (`ImportModel-scratch/specs10/gen`, regenerated with `specgen generate --mode impure`): `stub_refs` differ from the old sweep copies only for `445E90` (+`1415A1780`), `446390` (+`1415A1780`) and `44C880` (+`1415A1780`, +`14159F3E0`). Those specs are not committed (none exist in the tree).

## 2. Verification

- Red on the committed tree (`dac5058d`, before the fix): `test_snapmem_holes.py` FAIL on four checks (`page_ok` True, `read` returns zeros for the dropped page, the range read returns zeros). Green after `941e91ac`.
- Mutation (CONFIRMED, run, reverted): `page_ok` with the capture check removed (`return m is not None and page not in self.holes`): `test_snapmem_holes.py` FAIL on 3 checks (not readable, reads None, range reads None), rc 1. Reverted, rc 0 after.
- Mutation / red for reach_stubs: `check_closure_stubs.py` on the committed tree is RED for 44C880 (§0). The ref_restart_cmd case stays GREEN before and after.
- Binaries run: `test_snapmem_holes.py`, `check_closure_stubs.py`, `check_runnable_extra.py`, `driver.py --check --spec-dir specs/impure --seed-bank <workdrive>/gf-Captures-scratch/banks_union9l` (34/34), `gen_translated.py --image pes_full.bin --check` (3951 byte-identical). A first driver run used a cwd-relative bank name and failed on a missing bank file (not a code regression); the absolute path passes.

## 3. Module identity and what slim captures lose (CONFIRMED)

- Full captures (pid407792 seq022-116): `ucrtbase.dll`, 2480 exports, `strcpy_s` at RVA `0x17E00`.
- Slim captures (`live2/seq082-089`): export directory gone, 0 exports; `live2/seq085` module reads as `eFootball`, no names.
- Image identity: a slim capture drops the image's header page. Its unnamed section pages then have no `MZ` base. The name fallback (`PES2021.exe` mapping) only covers a named mapping, so those section pages are refused: `fetch from code outside every PE module of the snapshot`. This is the dominant exclusion in §5. OPEN: identify the image from `IMAGE_BASE` plus SizeOfImage read from `pes_full.bin` (the on-disk header), not from the capture. Not done (budget).

## 4. Item 2, and the 480910 blocker

- Watcher (`live2/watch.log`) released every capture after 14:21 with `disk under 450 GiB` (CONFIRMED; 449G free). I told Main; Main freed 67 GB and the watcher resumed (CONFIRMED in `watch.log`: `seq4275 ... -> ok`).
- Since then no `foul_sanction_caller_480910` capture has landed in `live2/pid463316/` (CONFIRMED at the last check). Current launch: 0 full 480910 captures. The hook is `FIRST 1 EVERY 3` in `<workdrive>/PES21/rendezvous/hooks.cfg` (CONFIRMED).
- No 480910 replay on a full capture was possible. The 480910 captures that exist are slim (`live/seq083-097`, `live2/seq108`), and all of them are refused (§5). So 480910 is NOT PROVEN, and its two aborts (`0x6FFFFEC77E00`) are unchanged: OPEN.
- Action: when a full 480910 capture arrives, `frame_trace.py recorded <dir> --hooks <workdrive>/PES21/rendezvous/hooks.cfg --out <trace> --spec-dir <specs10/gen>` then `snapdiff.py ... --only 0x140480910`.

## 5. Item 4: verdicts over all surviving captures (CONFIRMED, `verdict/sweep10.json`, `sweep10.log`)

123 captures (30 full, 93 slim). "Bit-exact" = replayed (no mismatches anywhere). Excluded counts are per call.

| hook | VA | calls | bit-exact | excluded | mismatched | verdict | exclusion reasons |
|---|---|---|---|---|---|---|---|
| foul_sanction_47F170 | 0x14047F170 | 14 | 0 | 14 | 0 | NOT PROVEN | image header dropped (slim) |
| foul_sanction_caller_480910 | 0x140480910 | 7 | 0 | 7 | 0 | NOT PROVEN | image header dropped (slim); no full capture (§4) |
| ref_involve_A3A690 | 0x140A3A690 | 34 | 18 | 16 | 0 | NOT PROVEN | image header dropped |
| ref_judge_A3A930 | 0x140A3A930 | 11 | 4 | 7 | 0 | NOT PROVEN | image header dropped |
| ref_latch_A3A870 | 0x140A3A870 | 9 | 2 | 7 | 0 | NOT PROVEN | image header dropped |
| ref_restart_cmd_A39A80 | 0x140A39A80 | 22 | 7 | 15 | 0 | NOT PROVEN | image header dropped |
| linesman_lookat_A38F20 | 0x140A38F20 | 16 | 4 | 12 | 0 | NOT PROVEN | image header dropped |
| restart_446390 | 0x140446390 | 9 | 2 | 7 | 0 | NOT PROVEN | image header dropped |
| restart_44C880 | 0x14044C880 | 15 | 4 | 11 | 0 | NOT PROVEN | 9 image header dropped; 2 `VCRUNTIME140.dll!memset` unmodelled |
| restart_driver_445E90 | 0x140445E90 | 9 | 2 | 7 | 0 | NOT PROVEN | 6 image header dropped; 1 `VCRUNTIME140.dll!memset` unmodelled |

Reading the table:
- The 47 "image header dropped" refusals are slim captures. They refuse, they do not pass; that is the intended effect of §0.
- Old PROVEN verdicts (`445E90` 3/3, `446390` 7/7 in the ImportModel doc) ran over zero-filled dropped pages. They do not survive the hole rule. This is the regression the rule was meant to expose.
- 44C880 and 445E90 now reach `VCRUNTIME140.dll!memset` (`0x6FFFFBC719A1`), an OS export with no model. DLL identity makes it visible; the old code ran its bytes as PES code. OPEN: a model (named, from the DLL's bytes), not done.
- The `sqrt` and cookie stubs are no longer the exclusion for 44C880. Whether the fixed closure is bit-exact on the full captures is not yet shown, because `memset` refuses the same calls.
- Full-capture-only check: the 30 full captures give 445E90 and 44C880 their replays; the rest are slim.

## 6. Open items (numbered)

1. **480910 full replay** (§4): waiting for a full 480910 capture in `live2/pid463316/`. Blocker is the arrival of a foul, not code. Then close the two aborts with the `strcpy_s` model (`import_stubs.json`, reachable through §0 DLL identity).
2. **Image identity on slim captures** (§3): read SizeOfImage from `pes_full.bin`, or identify the image by `IMAGE_BASE`. Without this, every slim PES-code call is refused.
3. **`VCRUNTIME140.dll!memset` model** (§5): needed for 445E90 and 44C880 full captures to reach bit-exact.
4. **Allocator `0x1403FB410`** (Main's request, not done): its indirect vtable calls (`call [rax+0x10]` at `0x1403FB47E`, `call [rax+8]`-style at `0x1403FB49F`) need a model (vtable targets read from the capture, or a named heap model). It blocks 480910's regression and CutsceneHybrid2's caller `0x141F41CE0`.
5. **`import_slots.json`** not regenerated; module discovery still uses mapping names (OPEN).
6. **Scratch specs** `specs10/gen` (ten hooks, regenerated) are not committed. Main should decide whether the replay specs belong in the tree.
7. **`1404488E0` switch case bodies** and `140A79640` (`[RSP+0x98]`): not done.
8. **Pre-existing**: `reach_stubs` ceiling: functions outside the ledger still use `body()`, which does not decode switch tables.
