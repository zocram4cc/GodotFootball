# CutsceneAnchor14 (CutWire): the group-list format is decoded (the fill's list is the install's WESYS object table); four import models are added (CreateSemaphoreW, WaitForSingleObject INFINITE, InitializeCriticalSectionAndSpinCount, SetEvent), which move the group builder's replay past its first two blockers; the next blocker is PES's pool region release (VirtualQuery/VirtualFree) on the element free path. The wide test is NOT green and was not run (no GF change).

Branch `CutsceneAnchor`, pushed head `913e81d68b8402c8355190877ccaa8382b859335` (confirmed by `git ls-remote origin CutsceneAnchor`). Commits this round:

- `972cfd53` import model: KERNEL32!CreateSemaphoreW (fixed handle 4, code_hex `48b80400000000000000c3`).
- `77dc67a4` import models: WaitForSingleObject INFINITE answers WAIT_OBJECT_0 (was the constant 258: the busy-semaphore acquire spun); InitializeCriticalSectionAndSpinCount (`do: cs_init`) with its xlate.py mirror.
- `04e129ea` import model: kernel32!SetEvent returns TRUE.
- `913e81d6` translated layer: five closure members regenerated for the changed model mirrors (1403AB2B0, 1403FF3E0, 14159F918, 141B37E50, 1423D90E0).

Labels: CONFIRMED = read, decompiled, traced or run this round; INF = inference; OPEN = named blocker.

## 0. Status against the assignment

- Item 2 (decode the fill's list and dispatch, flag meaning): the list format is DECODED (section 2). The dispatch that calls the fill's loader slot is not needed by GF and was not decoded (section 2, OPEN). The `flag` meaning in 0x141E5CB40 is INF (= the WESYS record's size).
- Item 3 (translate and prove 0x141E5C1E0): NOT proven. Its replay gets past two imports now and stops on the pool region release (section 4, the blocker).
- Item 4 (element path for the world build): not started; depends on item 3.
- Item 5 (key source): Main's decision recorded; not implemented this round (section 5).
- Item 6 (wide test): not run, not green (no GF change this round). Last run: RED, 9 EXPECT failures (CutsceneAnchor10).

## 1. Import models (CONFIRMED, tests run)

Each entry has a `code_hex` the harness runs; the translator mirror follows the entry's `do`. Tests are red before each entry and green after, and each mutation was run and reverted.

| Export | Model | Test | Mutation |
|---|---|---|---|
| kernel32 CreateSemaphoreW | do return 0x4, `48b80400000000000000c3` | test_create_semaphore_model.py 3/3 | code returns 0: red (1 failure) |
| kernel32 WaitForSingleObject | do wait, `83faff7406b802010000c331c0c3`: INFINITE → 0, else 0x102 | test_wait_model.py 3/3 | cmp edx,-2: red (INFINITE timed out) |
| kernel32 InitializeCriticalSectionAndSpinCount | do cs_init, `48c70100000000c74108ffffffffc7410c...`: LockCount -1, SpinCount = edx, TRUE | test_cs_init_model.py 4/4 | LockCount 0: red (1 failure) |
| kernel32 SetEvent | do return 1, `b801000000c3` | test_setevent_model.py 2/2 | none (trivial model) |

ported from origin/AnimeLayer (wait, `861f1a7e` tree) and origin/Presentation (`317585ac`, cs_init). `test_memset_model.py` still passes. `gen_translated.py --check` passes (see section 6).

## 2. The fill's list is the install's WESYS object table (CONFIRMED)

- `0x141E5D560` (the fill) takes `(this = loader, param_2 = list)`. Its list accessors `0x1414A9800`, `0x141030AB0`, `0x1414A98D0/9900/98A0` read the table at `count` (+0), `recordsOffset` (+4, relative to the list start), records of 12 bytes (argOff, flag, nameOff), all relative to the list start.
- The inflated WESYS blob of the install's `constant_stadium.bin` has exactly that layout: `count = 84` at +0, table offset `8` at +4, records `(dataOff, size, nameOff)`. CONFIRMED by script on `cutlib/constant_stadium.bin` (8074 B file, WESYS, zlib at +0x10, inflates to 38832 B).
- Record `demoarea_st001.o`: `dataOff 8592`, `size 432`, name at the name offset. Its 432 bytes EQUAL the extracted `demoarea_st001.o` (the object the node parser was validated on). So the fill's list = the unwrapped WESYS blob, and `argOff = dataOff`, `nameOff = nameOff`; `flag = size` (INF: the name match's r9 is not decoded).
- Consequence for GF: no list conversion is needed. GF can `PesVfs::Read` the constant bin, `UnwrapWesys`, and pass the blob to the fill.
- The fill's dispatch: the fill has no direct E8 caller and its only data reference is the loader vtable slot `0x142B48C88` (CONFIRMED by a qword scan). The loader completion path `0x1414B06C0/0x1414B0DE0` is a pending-request list (append/remove), not the dispatcher; the call through slot +8 is OPEN. GF does not need it: the boundary is the native read, then the translated fill.

## 3. Group builder 0x141E5C1E0 (CONFIRMED decode, replay state)

- The real entry is `0x141E5C1E0` (`.pdata` 0x141E5C1E0..0x141E5C332); `0x141E5C274` is a loop label. Callers: `0x14041BD00` (world load: clear, then the builder) and `0x1420B83B0` (the world state machine).
- The nine constant paths are at `0x143445E70` (`constant_{match,player,stadium,team,tutorial,tutorialConsole,positionCK,positionPK,shootAging}.bin`); the boot loop `0x140419610` calls `0x141E5BC40(G, path)` for each.
- Synthetic replay (`synth.py` over the `cstates/create_st001` memory; three states: one group, a null slot then a group, nine groups), traced with `frame_trace.py recorded` (CONFIRMED runs, not bit-exact):
  - Before this round: the run died at `CreateSemaphoreW` (unmodelled).
  - After the CreateSemaphoreW model: at `InitializeCriticalSectionAndSpinCount` (unmodelled).
  - After the cs_init model: a 50M-instruction spin at `0x1423D911D` inside `0x1423D90E0` (the busy-semaphore acquire), rax 0x102. Cause: `WaitForSingleObject` returned the constant 258 for every wait. Fixed by the wait model.
  - After the wait model: `SetEvent` (unmodelled, from the completion tail of `0x1414A9680`). Fixed.
  - After SetEvent: the run stops at `VirtualQuery` (CONFIRMED: `err: unmodelled OS export KERNEL32.dll!VirtualQuery`), see section 4.

## 4. Blocker: the pool free reaches PES's region release (CONFIRMED path; decision needed)

- Every group element is freed (`0x141E5D4E0` after its holder is made), so every replay of the builder frees: `0x1403FB5B0` (pool free) → vtable `+8` = `0x1403FBCB0` (heap free, a native service in production, PES bytes in the harness) → the region path.
- The region path: `0x1403FC080(addr, size)` loops: `VirtualQuery(addr, &mbi, 0x30)` (its single call site `0x1403FC0AE`, IAT `0x14252F6C8`), checks `mbi.BaseAddress == addr`, `AllocationBase == addr`, `RegionSize <= size`, `State == MEM_COMMIT (0x1000)`, then `VirtualFree(addr, 0, MEM_RELEASE)` (IAT `0x14252F6D0`), and advances. It returns 0 when every region is released, else -1. CONFIRMED by decompile.
- The fill over the real stadium list also calls `0x1403FB5B0` 23,037 times (name-match temporaries) but did not reach `VirtualQuery`, so this is the element free only.
- Options (for Main, policy 6 applies, this is PES's heap boundary):
  (a) a `VirtualQuery` + `VirtualFree` model with region semantics: needs the MBI fields the caller checks (decoded above) and a region-state model consistent with the pool; a fake answer is a stand-in and must be labelled;
  (b) treat the pool free as a boundary service (`stubs.json` native_services contract) so the harness does not run the region release. The production native (GF's pool) is then the answer on both sides; this needs the harness to stop at `0x1403FBCB0`, which the current harness does not do.
  Recommendation: (b), because the region release is OS memory management that GF's native pool replaces; (a) would model the OS.

## 5. Stadium key (Main's decision, not implemented)

- Stadium names (CONFIRMED, `cutlib/names.txt`): index 112 = `demoarea_st000`, 113 = `demoarea_st001` (key 0x71 at seq204), 158 = `demoarea_st046` (key 0x9E at seq321). `key = 112 + stadium id` holds for the names (INF that it is PES's formula for the match stadium).
- The GF source is `pes::TeamStadiumId(vfs, teamId)` (`src/pes/stadium.hpp`). The writer of `T+0x1C` is still not found (OPEN).
- Per Main: the wide test loads the capture's key 0x71 as pre-state; GF's runtime feed needs a `ponytail:` note naming the unfound writer and citing the evidence above. Not done this round: nothing reads a home team into the selector yet.
- Correction carried forward: the live node 0x71 holds `-80.0` at `+8` and `16.0` at `+0xC` (CONFIRMED from `cutlib/live_node_71.bin`). The brief's order is reversed. Node 0x9E: `+8 = 74.094498`, `+0xC = 6.233300`.

## 6. Checks (CONFIRMED this round)

- Model tests: as in section 1 (each run with the venv Python, `GF_DIFF_PYTHON`).
- `gen_translated.py --check`: first run reported 5 drifts (the five files in `913e81d6`); after the regeneration, a fresh `--check` is byte-identical: 5786 functions (3768 recorded bit-exact, 2018 closure). Rerun after the commit: rc 0.
- `tools/dev/ledger.py --check`: rc 0 (LEDGER.md current, 44499 rows, no status regression).
- PII: `git diff --name-only aef12c1f HEAD` grepped for `<workdrive>/` and `<home>`: no hits.
- Not run: the wide test and its neighbours (no GF change this round); the gate (agents do not run it); the snapdiff bounded proofs (no record changed).
- Note for the next agent: `git -c status.submodule=none status` fails with the godot-cpp symlink present (the error is hidden by `2>/dev/null` and makes status look empty). Swap the symlink for an empty directory for any git command that reads the index, then restore it.

## 7. Fill over the real stadium list (trace only, not a proof)

- Synthetic state: the fill's list is the install's unwrapped `constant_stadium.bin` (38832 B, 84 objects), `this` with kind 0, the cut-library slot 113 empty (memory of `cstates/create_st001`). Spec `fill_stadium_spec.json` (scratch).
- `frame_trace.py recorded`: rc 0, `err: null`, returns after the full loop; the notify `0x14069EE40` runs once. This shows the fill completes on the real list; it is not bit-exact (no `snapdiff --bounded` run, no record).
- Next: a `snapdiff --bounded` proof over this state (needs a `closure.py --members-file` root spec for 0x141E5D560 under the core set and a record); not done.

## 8. Open items (numbered)

1. Decide the pool-free boundary (section 4, (a) or (b)); then rerun the group builder trace to its return.
2. Prove 0x141E5C1E0 bit-exact (snapdiff --bounded over the three group states, which need regenerating: each is ~3.8 GB; delete after the replay).
3. Prove the fill over the real stadium list (section 7) and record it.
4. Translate the element path for the world build: `0x141E5BAF0` (group table), `0x141E5BC40`, `0x141E5BEE0`, `0x141E5C060`, the flag `0x141B34850`; and wire GF: `PesVfs::Read` + `UnwrapWesys` of the nine constant bins, the group holder per path, the fill per stadium group.
5. The fill's dispatch through loader slot +8 (OPEN, not needed by GF's wiring).
6. The stadium-key feed (section 5) with the `ponytail:` note, after the writer is found or the note is written.
7. Wide test after 1-6; not green now.
