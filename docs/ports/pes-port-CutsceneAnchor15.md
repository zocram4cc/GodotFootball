# CutsceneAnchor15 (CutWire): the group-list builder 0x141E5C1E0 is proven bit-exact (3 synthetic states), the fill 0x141E5D560 is proven over the install's real 84-object stadium list, and the heap region release is a boundary stub. The world-build wiring (items 4-6) and the wide test are NOT done: the budget ran out first. The wide test is not green and was not run.

Branch `CutsceneAnchor`, pushed head `f1897f4819c61e7f5a72e213a6bbae9b51dcf4c0` (`git ls-remote origin CutsceneAnchor`). Labels: CONFIRMED = read, decompiled, traced, replayed or run this round; INF = inference; OPEN = named blocker.

## 0. Commits this round (on top of 913e81d6)

| SHA | Step |
|---|---|
| `954f65c2` | heap-boundary stub: the pool region release 0x1403FC080 answers 0 (`release_ok`) |
| `98959082` | the group builder 0x141E5C1E0 proven (bounded, 3 synthetic states); layer regenerated |
| `f1897f48` | the fill 0x141E5D560 proven over the real stadium list; replay guard 2M -> 50M per call |

Earlier this session (CutWire, previous report): `972cfd53` CreateSemaphoreW, `77dc67a4` WaitForSingleObject INFINITE and InitializeCriticalSectionAndSpinCount, `04e129ea` SetEvent, `913e81d6` five closure members regenerated.

## 1. Item 3: the group builder 0x141E5C1E0 (CONFIRMED, bit-exact)

- `snapdiff --bounded` (`prove_bounded.py --merge`, tag `cutgroup`): 3 of 3 SYNTHETIC states replayed bit-exact, 0 mismatches, own-block 25/29. States (synth.py over `cstates/create_st001`, spec `grp_spec.json`): one group; a null slot then one group; nine groups (the nine stock constant paths read from the image table at 0x143445E70).
- Record: `results_trapaction.json`, closure `closure_cutgroup.json` (44 members; the eight heap native services excluded, as for the fill), root spec `specs/trapaction/141E5C1E0.json`, set file `specgen/sets/closure-cutgroup.txt`.
- Layer: 5794 functions (3769 recorded bit-exact, +1: 0x141E5C1E0 is now recorded).

## 2. Item 3b: the fill 0x141E5D560 over the real 84-object list (CONFIRMED, bit-exact)

- State: the fill's list is the install's unwrapped `constant_stadium.bin` (38832 B, WESYS count 84, table at +8), `this` with kind 0, cut-library slot 113 empty (memory of `cstates/create_st001`). Spec `fill_stadium_spec.json` (scratch).
- `snapdiff --bounded` (tag `cutfillst`): 1 of 1 replayed bit-exact, 0 mismatches, own-block 22/27. Recorded in `results_trapaction.json`; closure `closure_cutfillst.json`.
- The first attempt was EXCLUDED with "did not return to 0x14042200e": the replay's per-call guard `snapcase.INSTR_BUDGET` was 2,000,000 instructions and the fill runs ~23k pool allocations. The standalone trace (`frame_trace.py recorded`, guard 50M) had returned cleanly. Raised the replay guard to 50,000,000 (the frame_trace H-step guard). No existing proof changes: the layer regenerates byte-identical.
- Note: 0x141E5D560 was already in `results_core` (3 synthetic cases); this adds the real-list case in the trapaction set.

## 3. Boundary: the pool region release 0x1403FC080 (Main ruling (b); CONFIRMED, implemented)

- The fill, the group builder and every element free go through PES's pool free `0x1403FB5B0` → heap free `0x1403FBCB0` → the region release `0x1403FC080`: `VirtualQuery` (IAT 0x14252F6C8), then `VirtualFree(addr, 0, MEM_RELEASE)` (IAT 0x14252F6D0). It returns 0 when every region was released; both callers branch on that result (CONFIRMED decompile of 0x1403FC6A4 and 0x1403FE710: the released size is added to the caller's count and the pool's bytes).
- Stub `stubs.json` kind `heap-boundary`, op `release_ok`: harness (`run.py`) sets rax = 0; translator (`xlate.stub_lines`) emits `s.gpr[0] = 0ULL;`. A kind of `engine-noop` (rax untouched) would have left the callers reading garbage, so `noop` was rejected.
- Test: `test_release_ok_stub.py` 3/3 (table entry, translator text, MODEL_DOS). The stale `specs/core/1403FC080.json` was removed (a callee is a model or a spec, not both). The translated body `closure/pes_fn_1403FC080.cpp` is kept, since other closure records still list the function as a member (gen --check requires it).

## 4. Checks (CONFIRMED)

- `gen_translated.py --check`: byte-identical at each pushed tip (last run after the fill commit: 5794 functions, 3769 recorded bit-exact, 2025 closure).
- `tools/dev/ledger.py --check`: rc 0 (LEDGER.md current, 44499 rows).
- `test_release_ok_stub.py` 3/3; `test_native_services.py` OK; the model tests from the previous report still green.
- Trace evidence: the group builder (three states) and the fill (real list) both return with `err: null` on `frame_trace.py recorded`.
- Not run: the wide test and neighbours (no GF code changed); the gate (agents do not run it).
- Debris: `universe.json` is modified by each `closure.py` run; committed with the fill step. Scratch `cutlist/` is 4 MB (state copies deleted).

## 5. Items 4, 5, 6: NOT DONE (budget)

4. Element path and world-build wiring. Not started beyond the decode in report 13/14. Needed in order:
   - translate and prove `0x141E5BAF0` (group table: 0x40 object and `0x141E5B900`, 234 empty slots) and wire it before the builder in the selector's world build (`fixdemo_select.cpp`, which runs `0x141E5C8F0` today);
   - translate `0x141E5BC40` (path to element; its string helpers `0x14116DE60`, `0x14069BFD0`, `0x140402240`, `0x141B34850` flag) and `0x141E5BEE0`/`0x141E5C060` (pushes): these are the nine boot pushes at `0x140419610`;
   - GF side: `PesVfs::Read` of each constant bin, `UnwrapWesys`, the group holder per path (`0x141E5D700`), and the fill per stadium group (proven above). The fill's notify (`0x141E5BA70` → `0x14069EE40`) stores the cut-id list into the holder, so the else arm's lookup `0x141E5D4C0` finds node 0x71 after the fill.
   - The fill's dispatch (the loader slot +8 caller) is NOT needed by this wiring; GF calls the translated fill directly (OPEN only for PES's own async path).
5. Stadium key feed: Main's ruling applies (wide test loads the capture's key 0x71 as pre-state; GF's runtime feed `112 + TeamStadiumId(home)` with a `ponytail:` note citing st001 <-> 0x71 and st046 <-> 0x9E). Not implemented: the selector has no home-team input yet. The writer of `T+0x1C` is OPEN.
6. Wide test `GoalAnchorKindTwo.WideChannelScorerTakesTheElseArmAndEqualsPes`: not run. Last result (CutsceneAnchor10): RED, 9 EXPECT failures (`T[0]` -41.5502167 vs -41.6211472, `pos[0]` 0 vs -41.5502167, `pos[2]` 0 vs 34). Expected to stay red until items 4-5 are in GF.

## 6. Corrections and facts to keep

- Live node 0x71: `-80.0` at `+8`, `16.0` at `+0xC` (CONFIRMED, `cutlib/live_node_71.bin`). The brief's order is reversed.
- The fill's list = the inflated WESYS table (count at +0, table offset at +4, records `(dataOff, size, nameOff)`; `flag` = size is INF). CONFIRMED by byte equality of the `demoarea_st001` record.
- The group builder's real entry is 0x141E5C1E0 (0x141E5C274 is a label).
- Stadium name indices: 112 = `demoarea_st000`, 113 = `demoarea_st001` (key 0x71), 158 = `demoarea_st046` (key 0x9E).
- Tool notes: `closure.py` writes to `/tmp/closure-<tag>-*` (delete after each run); `prove_bounded.py` reads specs from `specs/trapaction`; `git status` fails while the godot-cpp symlink is present (swap it out, then restore).

## 7. Next highest-value step

Item 4's first piece: translate and prove `0x141E5BAF0` plus the nine-path loop `0x140419610` (one batch of closure members, one bounded proof), then wire the selector's world build (native `PesVfs::Read` + `UnwrapWesys` + the translated group table, holders and fill). That is the last gap before the wide test can say anything about the else arm.
