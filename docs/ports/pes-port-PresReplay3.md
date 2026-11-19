# pes-port-PresReplay3: ReplayHuman port, round 2 (20-11-26)

Branch `Presentation`, worktree `<workdrive>/gf-PresReplay2`, scratch `<workdrive>/gf-PresReplay2-scratch`. Base: `204cd348` (round 1). Commit and push: see section 7.
Labels: CONFIRMED = read from the image, the decompile, a debugger trace or a test run. INF = inferred. OPEN = not settled.

## 1. Result (short)

- **Red test 1 cause found and fixed (CONFIRMED).** `PesHeap::Attach` clears `s.mem.services` (`src/pes/translated/pes_native.cpp:54`). `ReplayHumanFrame::Create` pushed the ten host windows first and attached the heap after, so every region read as zero: `R+0x18` read 0 at the first gate, and the function returned after its two stack pushes. Fix: attach the heap first, then push the windows (`src/pes/replay_human.cpp`).
- **Red test 1 is still red, for a second cause (CONFIRMED).** After the fix the copy runs, and the next thing it hits is the record's create path. That path calls kernel32 and CRT imports through IAT keys that the production dispatch does not have. I routed the first one (`InitializeCriticalSectionAndSpinCount`, key `0x6FFFFF9CF314`) as a native. The next missing key is `0x6FFFFED00840` (called from `0x14038E7B0`). Every copy needs the create path, so **GF cannot run any palette copy until a generic import layer exists** (section 3).
- **Mutations:** 4 run, 4 caught, all reverted (section 5).
- **prove_bounded:** `--spec-dir` and `--results` are arguments now (defaults: the trapaction set). `merge_record` is a tested function. Tests: 3 of 3 green.
- **Godot seam:** none added. The clock is already visible to Godot; the HUD clock and the replay frame are not (section 4).

## 2. Diagnosis of the copy (CONFIRMED)

Method: an in-port write log (`PesMem::wlog`) around the call, a dump of `R+0x50..0xF0` after the call, `gpr` state, and temporary `fprintf` in the translated `0x14217DEF0` (restored from a backup, verified clean). Findings:

| step | observation |
|---|---|
| after the call | `R+0x50..0xEF` all zero; `R+0x21 = 0`; the palette entry at `P+0x80` is correct (`03 0a 11 18 ...`) |
| write log | 16 bytes total, none inside the record window |
| entry of `0x14217DEF0` | `R=0x20000000`, `L=0x20010000`, `K=0x20030000`, **`R+0x18 = 0`** (should be 1) |
| cause | `PesHeap::Attach` ran after the window pushes and cleared `services` |

So the function returned at its first gate, not at the copy. The predecessor's test values were not at fault for this failure.

Second cause (CONFIRMED by the decompile of `0x14217DE20` / `0x14217DB10` and a gdb backtrace):

1. A copy goes through `0x14217DE20` (release). It frees `R+0x48` if set, **sets `R+0x48 = 0`**, then tail-jumps through the record's vtable slot `+0x28`, which is `0x14217DB10`.
2. `0x14217DB10` with `R+0x48 == 0` takes the create path: `1403FB410` (pool alloc, native), then `141E534F0`, `141EBEB30`, `141EB6840` → `14029D370` → `1403434D0` → `1403CF740` → `14037FDA0` → `14038E6B0` → `14038E3E0` → `14038EE80` → `1403AB2B0` (kernel32 CS init, IAT `0x14252F440`) and further up the chain `14038E7B0` (IAT `0x14252F6B8` / `0x1425300C8`).
3. So every copy in PES goes through the create path. The five real captures all skip the copy (`FUN_1408bdf70` finds the entry equal), which is why they never showed it.

The create path's imports are not in the production dispatch table (`pes_dispatch_gen.cpp` has no `0x6fff…` key at all), and the harness has no model for them either (grep: no `InitializeCriticalSection` anywhere in `tools/dev/diffharness` or `src/pes`). The earlier proofs ran the path inside the harness, which can run PES's bytes there; GF's production build cannot.

## 3. Changes (what landed, and why each)

| file | change | why |
|---|---|---|
| `src/pes/replay_human.cpp` | heap attached before the host windows, with a comment on why | the fix for red test 1's first cause |
| `tools/dev/translate/stubs.json` | `native_services` entry `6FFFFF9CF314` → `kernel32_cs_init` | the first missing import; Win64 `CRITICAL_SECTION` init, single thread, returns TRUE (OS API semantics, not a PES guess) |
| `tools/dev/translate/stubs.py` | op added to `KNOWN_NATIVE_OPS` | the stub loader refuses unknown ops |
| `src/pes/translated/pes_native.{hpp,cpp}` | `pes_native_kernel32_cs_init`: writes the 40-byte section (DebugInfo 0, LockCount -1, RecursionCount 0, OwningThread 0, LockSemaphore 0, SpinCount = rdx), `rax = 1` | the native behind the key |
| `src/pes/translated/pes_dispatch_gen.cpp` | regenerated (only file of the layer that changed) | `gen_translated` |
| `tests/pes_replay_human_test.cpp` | stride test changed to an absent human at index 2 (below) | the old version (present human at index 2) reached the copy and aborted; the new one passes today and fails on a wrong stride |
| `tools/dev/translate/prove_bounded.py` | `build_parser()`, `merge_record()`, `main(argv)`; `--spec-dir`, `--results`; the record's `source` is the results path | item 3 |
| `tools/dev/translate/test_prove_bounded.py` | new: defaults, overrides, merge replaces only the root's record | item 3 |

Not changed: the copy tests' expectations (`CopiesThePalette`, `ASequence…`). They are still red; see section 6.

## 4. Godot seam (item 4): none added, here is what it needs

- **Match clock:** `GfMatch.match_time_ms()` is bound (`src/gdext/gf_match.cpp:57`). Godot can read the sim clock now.
- **PES HUD clock:** `ClockStateBuilder` (`src/pes/hud_clock_build.{hpp,cpp}`) builds the 17-word HUD clock state from the install, but nothing hands its output to Godot, and `godot/` has no HUD clock display. Needed: a `GfMatch` property wrapping `Build(ClockEnvInputs)` with the env fields filled from the match (`liveSeconds` from `match_time_ms`, the stop flag, the limit fields `E+0x9E..0x9F`, which GF does not model yet). Not small: several `ClockEnvInputs` fields have no GF source. Left for a separate slice.
- **Replay frame:** no seam. GF does not record or replay match frames (`src/sim/onthepitch/goalsequence.cpp` is timing only). A replay frame needs a GF recorder that fills the 300-byte human records and palette entries, which is a design item, not a seam.

## 5. Mutations (each run, rebuilt, reverted; `grep MUTATION` = 0 after)

| id | mutation | expected | observed |
|---|---|---|---|
| M1 | heap re-attached after the windows (the original bug) | copy test red on an assertion | `CopiesThePalette`: `Failure` at lines 43, 45; `rc=1` |
| M2 | `kFrameStride` 300 → 299 | absent-human stride test red | `AnAbsentHumanAtAnIndex…`: `rc=134` (the wrong stride reads an unmarked record, takes the copy, aborts at the import). Detected, but via the import, not an assertion |
| M3 | `merge_record` keeps only the new record | prove_bounded merge test red | `test_prove_bounded`: `KeyError '0x141978300'`, 1 error |
| M4 | palette entry never written | copy test red on an assertion | `CopiesThePalette`: `Failure` at lines 43, 45; `rc=1` |

Red-first: the prove_bounded refactor and its tests were written together, so I did not see the tests fail on the old code. M3 shows the merge test discriminates. The replay fix was red first (M1 shows the copy test fails on the bug).

## 6. Tests (current)

- `pes_replay_human_test` (`PES21_ROOT=<workdrive>/PES21`):
  - green: `AnAbsentHumanLeavesTheRecordAlone`, `AnInactiveRecordLeavesItAlone`, `APaletteIndexBeyondTheCountIsNotCopied`, `AnEntryWithoutTheCompareWordIsNotCopied`, `AnAbsentHumanAtAnIndexOtherThanZeroIsFoundAtItsOwnFrameRecord` (5 of 5 with the filter `-*CopiesThePalette*:*ASequence*`).
  - **red / aborts**: `CopiesThePaletteEntryWhenTheHumanIsPresent`, `ASequenceKeepsTheLastCopiedEntryAndReleasesAfterEachCopy`. Cause: the create path's imports (section 2). Unfiltered, the binary aborts (`rc=134`, `PES_DISPATCH_UNTRANSLATED: 0x6ffffed00840`), so the whole binary is red.
- `pes_hud_clock_test`: 4 of 4 green.
- `test_prove_bounded.py`: 3 of 3 green.
- `test_native_services.py`: 3 of 3 green (after the stubs change).
- `ledger.py --check`: LEDGER.md current (44,459 rows).
- `gen_translated --check`: see section 7 (run after the dispatch regeneration).

## 7. Layer and branch

- The dispatch table changed (kernel32 key) and was regenerated: `gen_translated` wrote 5,874 functions (3,769 recorded bit-exact, 2,105 closure). Only `pes_dispatch_gen.cpp` differs from the round-1 layer.
- `gen_translated --check`: run in the background (`gen_check3.out` in scratch). Result: see the final message.
- Commits and push: see the final message (SHA confirmed with `ls-remote`).

## 8. Open items (for Main to rule)

1. **The import layer (blocker for both copy tests and for any GF palette copy).** The production dispatch has no import keys. Needed: a generic import mechanism (the IAT keys `0x6FFF…` the image bakes; the P0d table, 134 slots in `docs/re/pes-re-import-slots.md`), or natives per import on the create path (three so far: `InitializeCriticalSectionAndSpinCount` done; `0x6FFFFED00840` next). Scope call for Main: per-import natives (small, but the chain is long) or the generic layer (shared infrastructure).
2. **Named gap or red?** I left the two copy tests red and did not skip them. A skip with a named gap (the owner ruling 19-11 pattern) is an option; it would hide the abort, so I want your ruling.
3. The sequence test's post-copy values (`+0x1C = -1`, `+0x21 = 1`) are the predecessor's expectation, matched to the captured entry state. They are not yet checked against a GF run; they can only be checked once the create path runs.
4. `prove_bounded` is parameterised but its presentation run was not repeated here (it needs real captures; the destructor proof from round 1 stands as recorded).
5. Visual claims: none.

## 9. Commits and checks (final)

- Branch `Presentation`, pushed `204cd348..2c347d21` (`git ls-remote origin Presentation` = `2c347d2151d5`). Three commits: the kernel32 native and the dispatch regeneration; the replay port's heap-order fix and stride test; prove_bounded parameters and its test.
- `gen_translated --check`: byte-identical (5874 functions, 3769 recorded bit-exact, 2105 closure), run as a tracked job.
- `ledger.py --check`: current (44,459 rows).
- Tests run: `pes_replay_human_test` (5 green with the filter `-*CopiesThePalette*:*ASequence*`; the two copy tests red and aborting), `pes_hud_clock_test` 4/4, `test_prove_bounded.py` 3/3, `test_native_services.py` 3/3.
- Gate: not run (Main gates).
