# pes-port-TrapChain: the kind-0 trap chain is NOT closed; the harness walk is fixed for leaves, one defect left (28-10-26 / 29-10-26)

**Status: NOT CLOSED.** The chain is not proven, GF's trap still runs `pes::PickTrapClip`, and `PesStall.DISABLED_ALiveBallNeverLiesStillLongEnoughForTheBreaker` is still disabled. What landed on branch `StoppageFlow` (`git ls-remote origin StoppageFlow` = `42499f9baf884686296a5087fd16eb4354bfcc73`): the merge, the walk/harness fixes with red tests and mutations, the kind-0 closure (170 members) in the spec layer, the replays of all 12 captures, and the pinned read set of the chain's records. Tags: **CONFIRMED** = read from a tool run this session; **INF** = inference; **OPEN** = not settled (blocker named).

## 1. Merge (step 1) CONFIRMED

`git merge origin/main origin/SpecGen origin/ImportModel` as an octopus fails ("Should not be doing an octopus", main conflicts with StoppageFlow), so three merges in turn. Two conflicts, both resolved by combining both sides:
- `tests/sim/pes_restart_delay_test.cpp` (main's seed-search loop `kArmedSeeds` + StoppageFlow's `ReadyForTake` and ready-point release gate: both kept).
- `tools/dev/diffharness/check_closure_stubs.py` (ImportModel's `CASES` list for `0x140A39A80` and `0x14044C880` over SpecGen's `reach_stubs`; `harness_fields` no longer returns `stub_refs`). Run: GREEN for both hooks, closure models `14159F3E0`, `1415A1714`, `1415A1780`.
No C++ test binary was built or run (no C++ changed).

## 2. The `0x140A4EC7F` defect (step 2): found, fixed for pdata-less functions, one more instance open

**Root cause CONFIRMED (three layers, each reproduced):**
1. `0x140A4D340` is a **pdata-less thunk** (`mov r9d,0xF9; xor r8d,r8d; jmp 0x140A54910`; x64 needs no unwind data for it). `xlate.capstone_decode` kept decoding past the `jmp`: the target lies inside its 64 KiB window, so it was "pending". It emitted **7,691 instructions** through the code after it, and one of those garbage `jmp`s targets `0x140A4EC7F`, an address inside an instruction (`0F 28 C6` at `0x140A4EC7D`; the epilogue starts at `0x140A4EC80`).
2. `snapdiff.build_closure` added every `OUT-OF-RANGE JMP` target as a function to translate without checking it is one. The translator returned no instructions, the build aborted after the replay had counted its matches.
3. The harness's reached-site list (`trace.json` keys are indirect **call sites**) was also made a root: `0x140864175` (`call [rdx]` inside `0x140864084`) and switch case labels (`jmp reg` targets such as `0x1406EA953` inside `0x1406EA840`) were translated as functions.

**Fixes (commits `4ab3bbaf`, `15260039`, then two more; all pushed):**
- New `tools/dev/translate/pdata.py`: the one `.pdata` reader (`Pdata`: `end_of`, `next_start`, `inside`, `containing`, `is_boundary`). `closure.py` uses it (`ClosurePdata`; its universe rows now also decode pdata-less leaves, bounded by the next `.pdata` start and by the first `ret`/tail jump with no forward branch pending; the old code stopped with "not a .pdata function start": `0x140849400`).
- `xlate.capstone_decode`: a function with **no** `.pdata` entry decodes at most to the next `.pdata` start (`decode_window`). A function with a `.pdata` entry is unchanged.
- `snapdiff.py`: an out-of-set target must be a function boundary or the closure is **not built** with the reason in the note (never silently translated); an indirect site is a root only if it starts a function; an indirect target in the same function as its site is a switch case (no root, no error); any other non-boundary indirect target fails the closure.
- `snapdiff.py` exits **1** when any result has no translated body (red test), never a pass.

**Tests (red first, then one mutation per binding, each run, reverted):** `tools/dev/translate/test_closure_walk.py` (11 tests, synthetic PE with a hand-built `.pdata`, no game bytes), registered as `translate_closure_walk_test` (skips 77 without `GF_DIFF_PYTHON`). Ran the script directly with the harness Python: `Ran 11 tests ... OK`.
| mutation | result |
|---|---|
| `decode_window` always the full window | `test_decode_of_a_thunk_with_a_forward_tail_jump_stops_at_the_jump` FAIL |
| `is_boundary` = `True` | 5 FAIL |
| `is_boundary` = `.pdata` start only | 4 FAIL + 1 error (a leaf after a jump table is refused) |
| `sys.exit(1)` -> `pass` | `test_a_closure_that_did_not_build_is_an_error_not_a_pass` FAIL |
| sites always roots | `test_an_indirect_site_inside_a_function_is_no_root` FAIL |
| non-boundary target check off | `test_an_indirect_target_inside_an_instruction_is_an_error` FAIL |
| switch-case exemption removed | `test_a_switch_case_in_its_own_function_is_no_root_and_no_error` FAIL |

**Layer drift from the decode bound (CONFIRMED, re-verified):** `gen_translated.py --check` after the bound: 41 drifts (the garbage tails of pdata-less thunks and import thunks left; some previously held-out records, e.g. `0x1403C55D0`, no longer reach a garbage callee). The layer was regenerated in place (`--check` then byte-identical: 3,949 functions, 3,680 recorded bit-exact, 269 closure). The 8 recorded functions whose files changed were re-run on the unicorn harness: cutscene `0x1403D2E60`, `0x140A7FA10` (the whole cutscene set: **685/685 still bit-exact**), presentation `0x140A812F0`, `0x141B50460` (**2/2**), core `0x1403C55D0`, `0x140A3F730`, `0x141FEE390`, `0x141FF52B0` (**4/4**). Closure-only changed files (callees, import thunks) are proven through their callers.

**OPEN (the blocker of every proof):** the same overrun exists for functions **with** a `.pdata` entry. `0x14084DF20` ends at `0x14084E20F` with `jmp 0x140857010`; the target is in the window, so the decode runs on, and its garbage tail has a `jmp 0x140850CA6`, an address inside the `.pdata` function `0x140850A79`. The new guard stops there with `closure walk: 0x140850CA6 (from 0x14084DF20) is not a function boundary` (CONFIRMED, last snapdiff run; before it: `0x14086403A`, `0x1406EA953`, `0x140849D70`, each a real instance of layers 1-3, each fixed). Bounding a `.pdata` function at its own end is **not safe on this image**: 40,135 of 158,648 `.pdata` entries are chained unwind chunks (PGO hot/cold splits; flags & 4 in the UNWIND_INFO), so a function's body is its range **plus the chunks that chain to it**. The fix, not done (budget): in `Pdata`, read each entry's UNWIND_INFO, record the parent of every chained chunk (the RUNTIME_FUNCTION after the unwind codes), give `extent(root)` = its range + chunks, make `containing()` return the root, `is_boundary()` false for a chunk start, and filter `capstone_decode`'s instructions to the extent (and `closure.ClosurePdata.body` the same). Then regenerate the layer, re-run `gen_translated --check` and the recorded checks of the functions that drift (as above).

## 3. Closure and replays (step 3, partly)

- `closure.py --spec-set core --tag trapchain 0x14079A470 0x140799440 0x14079FBC0 0x14079AFD0 0x14085B3E0`: **170 members**, 105 universe rows added, 65 existing; specs written to `tools/dev/diffharness/specs/core/` (51 existing kept), set file `specgen/sets/closure-trapchain.txt`. CONFIRMED (committed).
- `frame_trace.py recorded` on the 12 captures of `live2/pid463316` with the 170 specs as the traced set: **12/12 `err: null`**. Kind-0 returns (rax low byte): seq4310 0, seq4329 0, seq4351 0, seq4402 0, seq4469 **1** (issued), seq4531 0, all returning to `0x1407A03EE`; the 6 commit captures (seq4273-4278) return 0 to `0x14079A8C1` with 35 written bytes, only `0x14079AFD0` entered. The kind-0 captures enter 30-40 closure members each (e.g. seq4329: `1406ABBA0` 9,273 calls, `1406ABB30` 3,399, `140470D60` 119) so every callee has real call cases, in the traces under `<scratch>/trace/`.
- **Proof counts: 0 of 12.** `snapdiff.py --only 0x14079A470` over the 6 kind-0 captures: `calls=6 replayed=0` with the note above, exit 1 ("nothing compiled"). No bit-exact claim is made. The commit `0x14079AFD0` (12 calls: 6 own + 6 nested) was not run (same closure defect).
- New capture batch: PES pid 1467004 (`live2/pid1467004/`) had **1** `trap_kind0_79A470` capture at the time of reading (20 captures total, `seq001-020`); not replayed.

## 4. The chain's read set, pinned on the captures (step 4, pin only) CONFIRMED

`tools/dev/translate/readset.py` (committed) replays a capture with a memory hook and clusters the heap reads. Over the 6 kind-0 captures (`rs_*.json` in scratch), offsets relative to the entry registers (size in bytes; "6/6" = read in every capture):

| record | register | distinct (offset/size) | read in 6/6 |
|---|---|---|---|
| player/anime record P | rdx (`0x35290180` in seq4310) | 21 | 20: `+0x8/4 +0xC/4 +0x10/4 +0x14/4 +0x18/4 +0x1C/4 +0x20/4 +0x2FC/8 +0x304/4 +0x430/4 +0xA08/8 +0xAB8/8 +0xAC0/8 +0xAC9/1 +0xAE0/2 +0xB08/1 +0x2458/1 +0x377C/4 +0x3AC0/8 +0x3AD8/8 +0x3B18/4`; the 21st is read in fewer captures |
| Trap action | rcx (`0x35290DF8` = **P + 0xC78**, inside P's allocation) | **0**: the chain never reads the action's own bytes (rcx is passed on) | n/a |
| request | r9 (stack) | 1: `+0x0/4` (the kind word) | 1/1 |
| ball sample table | r8 (`0x033F8B04`, same in all 12) | 203 (`+0x4/4 +0x74/4 +0x78 +0x7C +0x88 ...` at stride 0x24/0x20 rows) | 50 |
- The doc's anime offsets `+0x70` (state), `+0xDC` (candidate index), `+0x19/+0x18/+0x1A` are **not read through rdx** in any of the 6 captures (including seq4469, the one that issued): they belong to the candidate object of the search, which these captures do not exercise; unchanged OPEN (`pes-re-trap-kind0.md` §4).
- Image globals the 170-member closure reads (static scan): G `0x1436F3DC8` (9 members), ball block `0x1436F2378` and `0x1436F23C0-0x1436F25A0`, `0x1436F3A40/3D60`, `0x1436EF2C8`, `0x1437F4808`, `0x1434997F0`, `0x14350B340`, `0x14351AE54`. Other heap clusters per capture (about 60) are G's tables, per-player blocks, the MA roster and motion data: not yet attributed.
- **Size of the wiring (INF):** the chain needs P (`+0xAB8` interface with vtable calls), the ball sample table, G's record tables, the match roster, and motion-table data (the `0x1b4...` clusters), all in PES layout in one `PesMem` window. `PesMem` has one window; `PesWorld` (owned by MatchInfo's branch) holds only G's header and 22 player blocks. No GF writer exists for P, the ball table or the motion data.

## 5. Not done

6. `_TrapCommand` still runs `pes::PickTrapClip` (`playercontroller.cpp:510`); the picker is not removed. The stall test stays DISABLED; its baseline (147 live firings at `16a99947`) is from TrapReplay and was not re-run. No `pes_*` binary was built or run; no GF mutation (no GF code changed).

## Unfinished (numbered)

1. **Chain-aware function extent** (design in §2): needed before any trap proof. Then `snapdiff.py --only 0x14079A470` (6 kind-0 captures) and `0x14079AFD0` (12), then the callees with real calls (`1406ABBA0`, `1406ABB30`, `1406ABDD0`, `140470D60`, `1404BCAD0`, `140439E50`, `14043A020`, `14043A0A0`...). Exit code and the refusal of non-boundaries now make a failure loud.
2. **Records in GF** (§4): P with its `+0xAB8` interface, the ball sample table, G's tables and roster, motion data, held at their PES writers (writers: `pes-re-record-ids.md` §1 W1-W6 for P's kick fields; none decoded for the rest). Needs a multi-window `PesMem` or one wider window (coordinate with MatchInfo, who owns `pesworldrecord*`).
3. `_TrapCommand` on the translated chain; delete `PickTrapClip` (`pesreceive.cpp:212`, `pesreceive.hpp:285`, its tests `pes_receive_test.cpp:445-662`); re-enable the stall test; mutations per binding; `pes_*` sweep (slots 2-3 via `GF_SIM_SLOTS=4`).
4. Replay and read-set the new pid1467004 captures (more kind-0 calls; seq4469-like "issued" calls are the only ones that reach the commit's candidate search, 1 of 6).
5. Record `0x14079A470` and `0x14079AFD0` in `results_core.json` (proof `snapdiff`) once proven.
6. Gate: not run (Main gates). Main should run `gen_translated.py --check` and the `diff_*` suites on the merged tree: the layer was regenerated (§2).

## Verification run

- `test_closure_walk.py`: 11 tests OK (7 mutations run and reverted, table above). Not run via ctest (build dir not reconfigured).
- `check_closure_stubs.py`: GREEN.
- `gen_translated.py --check`: before regeneration 41 drifts, after: byte-identical.
- `driver.py --check`: cutscene set 685/685; presentation (2 changed records) 2/2; core (4 changed records) 4/4.
- `frame_trace.py recorded`: 12/12 clean; `snapdiff.py`: 4 runs on `0x14079A470`, each stopped at a different real non-boundary (exit 1).
- Scratch (not committed): `<workdrive>/gf-TrapChain-scratch/` (`trace/`, `rs_*.json`, `specs170/`, logs, `tools/drift*.sh`).
