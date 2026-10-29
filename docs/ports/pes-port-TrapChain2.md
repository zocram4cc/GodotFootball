# pes-port-TrapChain2: chunk-aware function extent landed and tested; layer drift-green; trap commit 0x14079AFD0 PROVEN 12/12; kind-0 root 0x14079A470 builds and replays 7/7 but 6 of 7 are not bit-exact (29-10-26)

Branch `StoppageFlow`, head `4fd2d6f3` (`git ls-remote origin StoppageFlow`). Tags: **CONFIRMED** = read from a tool run this session; **INF** = inference; **OPEN** = not settled.

## 1. The extent (`tools/dev/translate/pdata.py`) CONFIRMED

The TrapChain report's design (flag-4 chunks) was necessary but NOT sufficient; the first implementation broke 5 recorded records on regeneration (cutscene `0x140005530 0x14008C980 0x14008CE00 0x140104DA0`, presentation `0x140049DE0`: "translator bug", 64 cases). Cause: on this image a `.pdata` entry is often not a function:
- **chained chunk** (UNWIND_INFO flag 4; 40,135 of 158,648; 12,761 roots with chunks, chains up to depth > 1);
- **continuation**: an unchained entry the code of the entry before it falls into (`0x140005530` [..5549) ends in `jl`; `0x140005549` and `0x140005639` hold the rest of its body; each has its own unwind info with prolog bytes, no chain flag);
- **conditionally reached region**: `0x14008C980`'s `ret` is at its range end, and `je 0x14008C9EA` / `jle 0x14008CA7D` go to four unchained entries after it.

Final rule (`Pdata.extent` / `ranges_of` / `decode`): a function = head entry + its chunks + continuations (adjacent entry, last instruction of the walk is no `ret/jmp/call/ud2/int3/hlt/nop`; **never** an entry `tools/dev/ledger/universe.json` lists as a function: 113 known functions are fallen into, e.g. `0x1400167D0`) + unchained entries its **conditional** jumps land in (within 64 KiB); a `jmp` to another entry is a tail call, never body (`0x14084DF20` `jmp 0x140857010` was the overrun). Each range decodes from its first byte with the old early-stop rule (inline jump tables follow the last `ret`: 612 of 15,000 sampled ranges), plus code only another range reaches (cold chunk jumping back into hot). `is_boundary()` is false for chunk and continuation starts; `containing()` returns the head.

Consumers (one reader): `xlate.capstone_decode` + `resolve_rip` sizes, `closure.ClosurePdata.decode_row`, `classify.body` (walk follows the extent), `snapdiff` (`boundary_map`, `containing`, new `indirect_kind` / `same_body`). Corrupt entries (`end <= begin` or > 1 MB, 29, all in the data region) are dropped. Not migrated: `argroots.pdata_functions`, `recordmap.pdata_ends` (their own readers; sizes only), listed as follow-up.

Two more defects the trap build exposed, fixed:
- `snapdiff.build_closure`: a `jmp reg` site in a pdata-less leaf (`0x140848B40`, a switch inside leaf `0x140848AF0`) and its case `0x140848B6E` were made roots and translated from mid-function ("label used but not defined"). Now a `jmp reg` site is never a function start and its same-body targets are cases (`same_body`: same extent, or no entry start between them; ponytail ceiling noted in code).
- `xlate`: `JMP qword ptr [reg+disp]` (vtable tail call, `0x1408550A0` `jmp [rax+0x98]`) emitted `__builtin_trap`; now a dispatch-table tail call (as the absolute-slot form). Guarded by the replay, no source-text test.

## 2. Tests and mutations CONFIRMED

`test_extent.py` (new, `translate_extent_test` in `tests/CMakeLists.txt`, skips 77 without `GF_DIFF_PYTHON`): 19 tests. Synthetic PE (no game bytes): chain of two chunks with a contiguous join, code reached only from a chunk, data in a range, continuation entry, call-ended / ret-ended range stays a function, absorbed conditional target, tail `jmp` not absorbed, absorption bound, classify walk. Real image (`PES_EXE`): red test on **`0x14084DF20`** (extent = its own range; no `jmp 0x140850CA6` decoded), chained functions **`0x140007130`** (chunks `0x1400071B9`, `0x1400071D0`, continuation `0x140007140`), **`0x140010890`**, **`0x14002CF80`**, continuation **`0x140005530`**, absorption **`0x14008C980`**, ledger override **`0x1400167D0`**. First commit `ad538c94` was the red run (8 failing). `test_closure_walk.py`: 13 OK (2 new: leaf `jmp reg`).
Mutations (scratch copy, each run and reverted): no join (1 FAIL), adjacent-only join (1 FAIL, after adding the 0x14008C980 test), chunks ignored (9), no islands (3), pending unbounded (2), chunk start is boundary (5), containing = chunk (4), no continuation (3), call falls through (1), no conditional absorption (1), jmp absorbed (2), absorption unbounded (1), classify primary-range only (1), classify boundary check off (1 error). One survivor found and killed (absorption bound).

## 3. Layer and verdicts CONFIRMED

- `gen_translated.py --check` before regeneration: 156 drifts; after `gen_translated.py` (3 times, last after the xlate fix): **byte-identical, 3,930 functions (3,681 recorded bit-exact, 249 closure)**. 85k lines of garbage tail left the layer; `__builtin_trap` files 142 -> 29.
- Recorded functions whose file changed: 104 (pilot 8, impure 9, cutscene 8, presentation 4, core 75). Re-run on the unicorn harness (`driver.py --check` on a subset record): **pilot 8/8, impure 9/9 (bank corpus), cutscene 8/8, presentation 4/4, core 75/75 bit-exact** (core: 73 in the batch; `0x14087B000` and `0x140923C30` reported "struct map gap ... FileNotFoundError" because a killed earlier batch's driver was still writing the same scratch out-dir, then re-ran alone: 100 and 80 cases bit-exact). **Verdict changes: none in the final layer.** The only change seen was the 5 failures of the first (chain-only) implementation, fixed by the continuation/absorption rules above.
- Gate not run (Main gates). Run `gen_translated.py --check`, `translate_extent_test`, `translate_closure_walk_test`, and the `diff_*` suites with `GF_DIFF_PYTHON`, `PES_EXE`.

## 4. Trap chain proof (step 3)

Captures: pid463316: 6 `trap_kind0_79A470` (seq4310 4329 4351 4402 4469 4531) + 6 `trap_commit_79AFD0` (seq4273-4278); pid1467004: 1 kind-0 (`seq019`, replayed this session, trace in scratch); pid1645544: **no captures** (empty directory; the process is gone; later pids 1660292, 1675604, 1679850 also empty). Command: `snapdiff.py --spec-dir specs/core --hooks <workdrive>/PES21/rendezvous/hooks.cfg --snapshot <capture>=<trace> ... --only 0x14079A470 0x14079AFD0` (needs `--spec-dir` and `--hooks`; without them: "no spec" / "fetch from code outside every PE module").

| function | calls | replayed | bit-exact | verdict |
|---|---|---|---|---|
| **`0x14079AFD0`** commit (198-target closure) | 12 (6 own + 6 nested) | 12 | **12/12** (rax, xmm0, every written byte), own-block 0.1111 | **PROVEN** |
| `0x14079A470` kind-0 root (347-target closure, builds and compiles) | 7 | 7 | **1/7** (pid1467004 `seq019`); 6/7 differ | NOT proven |

0x14079A470 mismatches (CONFIRMED, `sd_trap2.json`): seq4310, 4329, 4469 differ in the ordered write log only (first differing write at byte 11,997 / 25,330 / 7,639 of the stream); seq4351, 4402, 4531 differ in `xmm0` (harness `0xc2b9e544` vs translated `0x3da79e2b`; `0x3db3f7ce` vs `0`; `0x3ea4c6ce` vs `0x426cf17c`) and the write log. Before the `jmp [mem]` fix those six crashed (SIGILL in `0x1408550A0`, reached from `0x14084E930 <- 14084ECA0 <- 14084DB00 <- 14079A470`); after it they run to the end and differ. The one passing capture (seq019) is a different frame (rax `0x1c0fee00`). **Blocker (OPEN): a mistranslated or stub-modelled function in the 347-member closure, not isolated.** Next step: `lockstep.py` (translated vs harness instruction trace) on seq4310 from the entry, first divergent instruction -> the function; xmm0 differences point at a float path (the closure's `1406ABBA0`, `1406ABB30`, `140470D60` take 119 to 9,273 calls). The commit root passing 12/12 includes its nested calls, so the commit path is not the cause.

Not done: recording both in `results_core.json` (`proof: snapdiff`); `0x14079AFD0` qualifies, but adding a record adds layer files, so it was left for the next layer regeneration.

## 5. Records GF must hold for the chain (step 4, pin only; `pesworldrecord*` untouched) CONFIRMED on seq4310, one capture

`attr.py` (scratch; replays the capture with a read hook and attributes reads by following pointers read from the snapshot): offsets relative to each object, size in bytes.
- **Player record P** (`rdx`, `+0xC78` holds the trap action): 16 offsets in seq4310, 21 over the 6 captures: `+0x8 +0xC +0x10 +0x14 +0x18 +0x1C +0x20` (4), `+0x2FC` (8), `+0x304` (4), `+0x430` (4), `+0xA08` (8), **`+0xAB8` (8, interface pointer; vtable calls)**, `+0xAC0` (8), `+0xAC9` (1), `+0xAE0` (2), `+0xB08` (1); in other captures `+0x2458` (1), `+0x377C` (4), `+0x3AC0` (8), `+0x3AD8` (8), `+0x3B18` (4). The action `rcx` is never read (passed on).
- **Request** (`r9`, caller stack `0x1BCFDFC0`): 23 offsets `+0x0 +0x4 +0x8 +0xC +0x10 +0x14 +0x18 +0x20..+0x48`; `+0x0/4` is the kind word. It carries pointers into the caller's stack frame (see below).
- **Ball sample table** (`r8`, `0x033F8B04`): 97 offsets: `+0x4`, `+0x74 +0x78 +0x7C`, `+0x88 +0x8C +0x90`, `+0xB0 +0xB4 +0xB8`, `+0xD8 +0xDC +0xE0`, then `+0x104 +0x12C +0x154 +0x17C +0x1A4 +0x1CC +0x1F4` (stride 0x28), `+0x218..+0x220`, `+0x240..+0x248`, `+0x268..+0x270`, `+0x290..+0x298` (stride 0x28, three-word rows).
- **G** (`*(0x1436F3DC8)`, `0x2E9BC400`): `+0x40` (8), `+0x270` (8, MI), then 25 eight-byte slots `+0x19C8, +0x1A18, +0x1A40, ... +0x1D10, +0x1E00, +0x1EF0, +0x20D0, +0x2620, +0x27B0` (stride 0x28 from `0xA1*0x28`): pointers to per-slot blocks. Blocks read: `+0x1/1`, `+0x880 +0x888 +0x88C +0x890 +0x8A0 +0x8A8 +0x8DC`, `+0x2528/4` (each of ~10 slots), the `G+0x19C8` block `+0x47C8..+0x47E8` (4), the `G+0x2620` block `+0x20 +0x88 +0x9B +0xA4 +0xB0 +0xC8 +0xD0 +0xD4 +0xE0 +0xE8 +0xF0 +0xF4 +0xFC +0x108 +0x110 +0x114 +0x120`.
- **Caller-built stack block** (INF, the dominant read set: 141 + 99 + 78 + 70 + ... offsets over `S+0x180 .. S+0xB00`, where `S` = entry `rsp`): arrays of 0x28/0x30-stride records the caller (`0x1407A03xx`, returns to `0x1407A03EE`) builds on its stack and the request points at (e.g. `+0x70`, `+0x74`, `+0x78` words per element). GF must build this block by the same code as PES's caller; it is not a persistent record. Not decoded.
- **Motion rows** (INF): four rows read at `+0x0/4 +0x4/1 +0x8/4 +0x24..+0x4C/4 +0x50/4 +0x60/4` and four at `+0x4..+0x10`, `+0x14 +0x18`, pointed from slots in a module data page (`0x7FF5DA80xxxx`): the motion/clip tables.
- Image globals (static scan, TrapChain report §4): `0x1436F3DC8`, ball block `0x1436F2378`, `0x1436F23C0-0x1436F25A0`, `0x1436F3A40/3D60`, `0x1436EF2C8`, `0x1437F4808`, `0x1434997F0`, `0x14350B340`, `0x14351AE54`.

## 6. Unfinished (numbered)

1. (done, see §3: core 75/75.)
2. **0x14079A470: isolate the mismatching closure member** (lockstep on seq4310), fix it (a general translator fix), re-run `snapdiff` over all 7 captures; then record both roots (`results_core.json`, proof snapdiff) and regenerate.
3. Migrate `argroots.pdata_functions` and `recordmap.pdata_ends` to `Pdata`.
4. Replay more kind-0 captures (only 7 exist; pid1645544 produced none).
5. GF side: records of §5 in a PES-layout window (MatchInfo's `pesworldrecord*`), `_TrapCommand` on the translated chain, delete `PickTrapClip`, re-enable `PesStall.DISABLED_...` (unchanged from the TrapChain report; no GF code changed).

## Verification run

`test_extent.py` 19 OK, `test_closure_walk.py` 13 OK (harness Python, `PES_EXE` set); 14 mutations run and reverted; `gen_translated.py --check` byte-identical; `driver.py --check` subsets: pilot 8/8, impure 9/9, cutscene 8/8, presentation 4/4, core 75/75; `snapdiff.py`: commit 12/12 PROVEN, kind-0 1/7. No C++ test binary built or run (no C++ changed). Scratch: `<workdrive>/gf-TrapChain2-scratch/` (not committed).
