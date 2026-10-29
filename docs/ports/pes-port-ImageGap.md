# pes-port-ImageGap: the `0x1440xxxxx` code is in `pes_full.bin`; the gap was in the bridge, `closure.py` and slim replay

Branch `ImageGap` (worktree `<workdrive>/gf-ImageGap`), pushed head `2de0af12` (`git ls-remote origin ImageGap` = `2de0af12`). One commit on `origin/main` `113fc926`. Scratch (PES-derived, not committed): `<workdrive>/gf-ImageGap-scratch/`. Labels: CONFIRMED = read in tool output this session; INF = inferred; OPEN = not settled. Layer not regenerated; no live reads (only `/proc/<pid>/maps` of the running game).

## 0. Verdict

**CutsceneCam's "outside `pes_full.bin`" is wrong.** `0x1440558C0` and the other three preset bodies are plain, relocated game code, inside `pes_full.bin`, byte-identical in `PES2021.exe` on disk, in the image, and in a full capture. What they are outside is **the Ghidra bridge's program** (`/segments` = `ram: 140000000 - 143d13fff`), which is why `FUN_141F7BFE0` shows `thunk_EXT_FUN_1440558c0`. The image tooling had three real holes behind this, all closed or measured here (§3).

## 1. Where `0x1440xxxxx` lives (CONFIRMED)

| source | finding |
|---|---|
| `PES2021.exe` headers (on disk) and `pes_full.bin` headers | identical 19-section tables, `SizeOfImage 0x1B97C000`; `pes_full.bin` is exactly `0x1B97C000` bytes (462,929,920) |
| section holding the VA | `.impdata`, VA `0x143D14000`, VirtualSize `0x17C3D94F` (ends `0x15B95194F`), characteristics `0xE0000020` (rwx), raw data on disk `0x17C3DA00` bytes at file offset `0x391B400`. Not a packed section the unpacker skipped, not past the image, not another module |
| live process, `/proc/2621613/maps` | `143d14000-15b952000 rwxp`, anonymous: the same range |
| capture `live2/pid2621613/seq065/index.json` | mapping `va 0x143D14000 size 0x17C3E000 rwxp deferred false`: the whole section is in `mem.bin` |
| Ghidra bridge | `ram` ends at `0x143d13fff`: `.impdata` is not in that program. The headless project `PESLive` (`dec.sh`) has it |

Origin: this is the protector's section (Denuvo, `docs/re/pes-re-protection.md` §1.1). Its code is **not decrypted at run time** (§1.2 there, reconfirmed below); the packer relocated 2,273 game functions into it and left a `jmp rel32` thunk at each original `.trace` start.

## 2. Bytes (oracle: capture vs image) (CONFIRMED)

- Capture `pid2621613/seq065` (full) vs `pes_full.bin`, whole `.impdata` (97,342 pages): **142 bytes differ** (41 runs, 29 pages). 28 of them fall inside a `.pdata` function body; none inside the four presets. Over 20 full captures from 10 launches the union is 31 pages (`volatile_pages.py`, §3.3).
- The four bodies, 32 bytes each, are equal in capture, image and `PES2021.exe` on disk: `0x1440558C0 c641440731c0f30f100d06aa44ff…` (= `mov byte [rcx+0x44],7; xor eax,eax; movss …`), `0x144056E40 c6414406…`, `0x1440516B0 c641440a…`, `0x144052490 c641440a…`.
- On-disk `.impdata` vs image: 16 of 96 4-MB chunks differ (534 pages), the state words of `pes-re-protection.md` §1.2.
- Disassembly (capstone, then `dec.sh` decompile, both CONFIRMED): each is a 25-instruction leaf that fills a camera-preset record: `0x1440558C0` sets `[rcx+0x44]=7`, `[rcx+0x18]=0xF`, floats from `.data` `0x1434A02D0..0x1434A02F4`, ends `0x144055953`; `0x144056E40` `[rcx+0x44]=6`; `0x1440516B0` and `0x144052490` `[rcx+0x44]=0xA`. The bridge thunks are `0x1408924C0 -> 0x1440558C0`, `0x140892560 -> 0x144056E40`, `0x140892240 -> 0x1440516B0`, `0x1408922E0 -> 0x144052490`.
- **No `.pdata` entry** for any of the four (leaves, no unwind info). That is why `closure.py` could not take them (below). `.pdata` has 2,092 entries inside `.impdata` (1,163 below `0x145000000`).
- Slim captures (e.g. `pid161465/seq012`) hold 3 of 97,342 non-zero `.impdata` pages: the rest was dropped. `SnapMem` refused them (writable section, rwx mapping), so a replay of code in the region read "unmapped".

## 3. The three holes and what changed (branch `ImageGap`)

1. **`closure.py` could not decode region leaves, and trusted unresolved rows.** `Pdata.end_of` knew only `.pdata` starts; and `crawl` treated the ledger's size-0 "unresolved/protected" row (the bridge could not read it; 470 such rows in `universe.json`) as a present member with no callees, so specgen would size it 0. Now: for VA >= `0x143D14000`, a no-`.pdata` function gets its extent from a branch-aware linear decode (ends at the first RET/JMP past the furthest forward branch); a VM entry (`PUSHFQ` first), an opaque return (`XCHG [rsp],r; RET`, the mutated class), undecodable bytes or a body over 4000 instructions are **refused by name**. An unresolved region row is replaced by the decoded row (`source "image-leaf"` when no `.pdata`); a resolved row is never replaced; below `.impdata` nothing changes. `endclear` dry-run: 84 members, 0 rows added (unchanged).
2. **Slim replay could not serve `.impdata`.** `PeImage.servable` refused every writable section, and `SnapMem._served` also refused any `w` mapping. Now `.impdata` pages are served from the image except `IMPDATA_VOLATILE_RVA` (31 pages measured by comparing `pes_full.bin` against 20 full captures of 10 launches; 8 captures give the same 31).
3. **`tools/pes21_import/volatile_pages.py`** regenerates that constant from a dump and full captures (documented in `tools/pes21_import/README.md`, new `.impdata` section). Not committed: any PES bytes.

Tests (all CONFIRMED run):
- `tools/dev/translate/test_closure_region.py` (new): 8 checks on a synthetic image (tail JMP from a `.pdata` function reaches a no-`.pdata` leaf; unresolved row replaced; callee kind `tail`; RET inside a branch span does not end a leaf; VM entry and opaque return refused by name; rule unchanged below `.impdata`; resolved row kept). With `PES_EXE` + `PES_CAPTURE` it also asserts 847 of 847 clean no-`.pdata` thunk targets are byte-identical in image and capture and decode to a RET-ended row. rc 0 both ways.
- `tools/dev/translate/test_image_identity.py`: 12 checks (was 9): `.impdata` page served from the image under an rwx mapping; a read crossing into a volatile page refused; volatile page never served. rc 0. Also green: `test_snapmem_holes.py`, `tools/dev/rendezvous/test_slim_identity.py`.
- Mutations (each run on a scratch copy, then discarded; every one turned a check red): M1 unresolved rows kept (1 FAIL); M2 VM refusal removed (1); M3 opaque-return refusal removed (1); M4 branch span ignored (1); M5 leaf rule below `.impdata` (1); M6 `.impdata` never served (2); M7 volatile pages served (2); M8 rwx mapping blocks `.impdata` (2).
- Red on `origin/main`: `closure.py` there has no `refusal`/leaf rule and `PeImage` has no `is_impdata`, so both test files fail; I did not run them there (INF).

## 4. Callers into the region today

| measure | count | status |
|---|---|---|
| committed layer functions (3,961 `pes_fn_*.cpp`) whose body tail-jumps (`JMP`) into the region and emits `/*OUT-OF-RANGE JMP 0x… */return;` (a silent no-op) | **8** | CONFIRMED (grep): `140A3F730` (restart) and `140A3F770` (cutscene) -> `14417D0C0`; `1415184E0` -> `144ADC120`; `141B50460` -> `15B23CA30`; `140A426F0` -> `14417E440`; `14053ACE0` -> `143F6A7F0`; `14043A7E0` -> `143EF8930`; `14053A8A0` -> `143F667C0` |
| layer functions that `CALL` a `.trace` thunk into the region and abort (`OUT-OF-SET CALL`, `pes_dispatch_missing`) | **2** | CONFIRMED: `140484C30`, `140A23380` (3 distinct thunks) |
| universe rows that tail-jump into the region | 470, all unresolved/`protected` | CONFIRMED. Targets: 326 have `.pdata`, 123 clean no-`.pdata` leaves, 19 VM entries, 2 mutated (INF class by heuristic) |
| of those 470 as `closure.py` roots now | **410 resolve**, 60 refused | CONFIRMED: 34 call a `.trace` no-`.pdata` leaf that has no row (pre-existing rule, OPEN §5.1), 19 VM entry, 3 opaque return, 4 outside the image |
| all 2,273 `.trace` thunk targets | 1,431 `.pdata`, 670 clean leaves, 161 VM, 11 mutated | INF: my heuristic undercounts the mutated class (the doc says about 99) |

**Which become translatable:** the 7 distinct region targets of the 8 silent-return functions all resolve as `closure.py` roots (dry-run closures of 1, 3, 7, 7, 15, 1, 1 members), and so do the four camera presets and the two `.trace` thunks `0x1408924C0`, `0x140892560` (2 members each). Translation of region code is proven to work for two: `driver.translate_one` of `0x1440558C0` gives the 9 field stores and `.data` constant reads (`s.rdf(0x1434A02D4)`), and of `14043A7E0` with `143EF8930` in the universe gives `{ pes_fn_143EF8930(s); return; }` instead of `OUT-OF-RANGE JMP`. The layer still has the gap bodies until Main regenerates it; I did not (brief).

## 5. Open items

1. **The rig root `0x141F7BFE0` does not close yet:** `closure.py` stops at `0x14089CEB0`, a `.trace` function with no `.pdata` entry and no universe row ("is not a .pdata function start"). Same class as the 34 refusals above. The leaf rule is restricted to `.impdata`; widening it to any VA (a closure-time decode of a missing row, no row replaced) is a small change but affects every future closure, so I left it for Main's ruling. Not region-related.
2. Mutated/VM bodies (about 100 and 161 thunk targets) stay refused: translating them needs emulation (`emu.py`) or a native model; the camera presets are not in those classes.
3. `IMPDATA_VOLATILE_RVA` is per image/build; regenerate after a new dump or game update. A page that only a rare path writes could be missing from the 31 (20 captures, 10 launches saw none beyond them).
4. Ghidra: the bridge program stays without `.impdata`. A bridge decompile of `1440xxxxx` needs the `PESLive` project (`dec.sh`, works, 17 s for the four presets) or `closure.py`/capstone on the image. Adding the block to the live GUI project was not attempted (not safe while the bridge is up).
5. `docs/ports/pes-port-CutsceneCam.md` §3 should say "outside the bridge's program, inside `pes_full.bin`" (Main archives reports).

## 6. Fast-tier line (interim)

`GATE_TIER` is not on `origin/main` (`grep -c GATE_TIER tools/dev/gate.sh` = 0), so the interim form: no C++ `pes_*` binary links the changed files (Python tooling only); `gen_translated.py --check` result is in the final message.
