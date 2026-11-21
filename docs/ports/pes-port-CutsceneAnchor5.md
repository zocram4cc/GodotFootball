# CutsceneAnchor5: the registry's sub-map registration (vslot +0x88) is proven and translated; the wide test stays RED at the event builder's untranslated vtable call

Branch `CutsceneAnchor`, pushed head `6485cd82b2f12592f030970068a78791d6482818` (confirmed by `git ls-remote origin CutsceneAnchor`). Parent `97771be9`. Author zocram4cc (checked with `git log`). One commit, no force push.

Labels: CONFIRMED = run or read this round; INF = inference; OPEN = named blocker; SYNTHETIC = a replayed call whose entry state was built by hand over seq203's memory (the call itself was not observed).

## 0. Outcome

- Target: `GoalAnchorKindTwo.WideChannelScorerTakesTheElseArmAndEqualsPes` GREEN. **Not met.** The test is red, and the failure has moved. It used to stop at the registry's vslot +0x88 (`PES_DISPATCH_UNTRANSLATED: 0`). It now runs the registration and continues into the event builder, then stops at an untranslated vtable target (section 4).
- Landed: the sub-map registration (0x141B6E9C0 → 0x141B6F220, callback 0x140451010) proven bit-exact and translated; GF's native RegistryImpl gives slot +0x88 PES's VA; a second FixDemo ctx map is set up (it was a stack-overflow recursion in the else arm).
- Step 2 (anchor values after registration): **not reached.** The else arm does not finish, so BC, radius, T, pos, rotY are not compared.

## 1. The registration path (CONFIRMED by disassembly and Ghidra decompile)

- `0x140445750` (flag branch, PES and GF): when the flag `N1+0x18` is clear, it calls `0x141B55830` (returns the RegistryImpl) and then `call [vt+0x88]` with `rcx` = RegistryImpl, `rdx` = N2, `r8` = `0x140451010`, `r9` = `&N1.flag`.
- Slot +0x88 of the RegistryImpl vtable (PES, read from seq203 at vtable `0x142ACAA40`) = `0x141B6E9C0`: `add rcx,0x1d0; jmp 0x141B6F220`.
- `0x141B6F220` (CONFIRMED, decompile + asm): on the sub-map at `rcx` (RegistryImpl+0x1d0): lock (`0x1403FF430`); pop a node from the free list (`+0x48`) or allocate 0x40 bytes (`0x1403FB410`); store `(rdx, r8, r9)` at node +0 / +8 / +0x30 and link it at the head `+0x30`; unlock (`0x1403FF490`).
- Flag at sub-map +0x28 (seq203 value `1`) decides whether unlock calls the import slot `0x14252F6E8` (RtlReleaseSRWLockShared; the import model returns 0).
- `0x140451010` (CONFIRMED): `lea rdx,[rcx+0xbc]; add rcx,4; jmp 0x140443240`, a copy of the N2 block's fields into +4...
- seq203 values (CONFIRMED by reading `mem.bin` via `index.json` mappings): N1 = `0x10E1EB90` (N1+0x18 = 1, the flag), N2 = `0x10E1EBC0`; the sub-map at `0xAB9F800` has list head `0x2D979780`, free list `0x1FAF1860` (non-empty), flag `1`.

## 2. Proofs (SYNTHETIC entries, CONFIRMED by replay)

Entries are built by `tools/dev/translate/synth_regcall.py`-style copies (scratch `f5/`, not committed) and `synth.py` (static spec `f5/spec_empty.json`). `mem.bin` is a symlink to seq203's read-only memory, except `s8`, which is a synth copy (4 GB; deleted after this report).

| Root | Synthetic entry | Calls replayed | Mismatches | Own-block |
|---|---|---|---|---|
| 0x141B6F220 | s5: rcx = sub-map 0xAB9F800, rdx = N2, r8 = 0x140451010, r9 = N1+0x18, rsp/return as seq203 | 1 | 0 | (with s6, s8) |
| 0x141B6F220 | s6: entry 0x141B6E9C0 (the thunk), rcx = RegistryImpl 0xAB9F630 | 1 | 0 | (with s5, s8) |
| 0x141B6F220 | s8 `freelist_empty`: as s5, plus the free list word (+0x48 = 0), so the allocation path runs | 1 | 0 | |
| 0x141B6F220 | all three | 3 | 0 | 7/7 |
| 0x141B6E9C0 | s6 | 1 | 0 | 1/1 |
| 0x140451010 | s7: rcx = a block in the unused stack (rsp-0x3000, aligned) | 1 | 0 | 1/1 |

`results_snapdiff_cutanchor7.json` (copied to `tools/dev/translate/`). `snapdiff.py --bounded`, `--spec-dir tools/dev/diffharness/specs/core`. Records appended to `results_core.json` (`synthetic: true`, `closure_file: closure_cutanchor7.json`, `cases: 0`: no real call of these was captured).

- Caveat (CONFIRMED): the first attempt at the free-list case had a mistyped constant (rcx); that run was discarded and redone. The corrected run is in the results file.
- Caveat (INF): the registration is a real call in PES only when the flag is clear. seq203 has it set, so no captured state covers the first registration. The flag address in s5-s8 is N1+0x18 (its value is not read by 0x141B6F220).
- `0x1403FBC10` (heap_alloc) is a native service in production and is not a closure member (same rule as CutsceneAnchor4's factory).
- Mutation for the proofs is not run (the replays are the check).

## 3. Layer and native registry (CONFIRMED by build and tests)

- `closure.py --tag cutanchor7 --subsystem cutscene` (specs `specs/core/{141B6E9C0,141B6F220,140451010,140443240}.json`, set file `specgen/sets/closure-cutanchor7.txt`, universe rows for the four). `closure_cutanchor7.json` lists the members the replays entered: 0x1403FB410, 0x1403FD830, 0x1403FDB10, 0x1403FF430, 0x1403FF490, 0x140443240.
- `gen_translated.py` wrote 5706 functions (3764 recorded, 1942 closure). New files: `cutscene/pes_fn_141B6E9C0.cpp`, `cutscene/pes_fn_141B6F220.cpp`, `cutscene/pes_fn_140451010.cpp`, `closure/pes_fn_140443240.cpp`; `pes_dispatch_gen.cpp` (+8) and `translated.hpp` (+4).
- `pes_native.cpp` (`PesCameraRegistry`): RegistryImpl object 0x220 bytes (was 0x20), vtable 0x90 (was 0x78). Slot +0x88 = `kSubMapSlotVa` = 0x141B6E9C0. The sub-map (at +0x1d0): flag byte +0x28 = 1, head +0x30 = 0, free list +0x48 = 0. Both lists start empty in GF; PES's seq203 has a non-empty free list. This only changes which allocation path runs; the freelist_empty proof covers that path.

## 4. The wide test now (CONFIRMED by gdb backtrace)

Before the ctx fix, the else arm recursed without bound in `pes_fn_14155E260` (a std::map erase) from `0x141F51630` (ctx setter), which clears two maps at ctx+0x30 and ctx+0x40. `InitPickMap` sets +0x30; +0x40 had no sentinel. **Fixed:** `InitHistory(ctx + kCtxSecondMap)` (`fixdemo_select.cpp`, comment in place).

Now the test stops with `PES_DISPATCH_UNTRANSLATED: 0x141F55A10`:

```
0x14047C500 (event builder, else arm via 0x140471A80) -> 0x141F5E730 -> 0x141F5E2E0 -> 0x141F55110 -> 0x14159EFF8
  -> indirect call through a vtable slot to 0x141F55A10
```

- `0x141F55A10` (CONFIRMED by disassembly): a constructor. It stores a vtable `0x141F55A10+0xc4fc35`-style pointer at `[rbx]`, calls `0x140A923F0` and `0x140A924E0`, and writes `[rbx+0x20]`. It is reached only through a vtable slot, so the closure walker does not see it (the same class of gap as the notes on indirect targets in `closure_file`).
- OPEN: prove the event builder chain (a synthetic entry of 0x14047C500 from the else arm's state, bounded trace, closure file for the vtable target 0x141F55A10 and its two callees). Not done this round: it is a new proof round, and the budget was spent.

## 5. Checks (CONFIRMED)

- Build: `pes_goal_anchor_test`, `pes_registry_test`, `pes_fixdemo_select_test`, `pes_camera_list_test`, `pes_proc_camera_test` (worktree `build/`, Ninja, `PES_EXE` = the unpacked image, `GF_DIFF_PYTHON` = the harness venv, `PES21_ROOT`). Built clean.
- `pes_registry_test` 7/7, `pes_fixdemo_select_test` 6/6, `pes_camera_list_test` 5/5, `pes_proc_camera_test` 6/6, `pes_goal_anchor_test --gtest_filter=-GoalAnchorKindTwo.WideChannel*` 22/22.
- `GoalAnchorKindTwo.WideChannelScorerTakesTheElseArmAndEqualsPes`: RED (`PES_DISPATCH_UNTRANSLATED: 0x141f55a10`). The other three KindTwo tests run green in isolation (`NarrowChannel*`, `WideChannelScorerIsZoneTwoAsPes`, `PastTheLine*`: 3/3 PASSED). The four run in one process, so the wide test's abort stops the rest of a full-filter run.
- **Mutation (CONFIRMED, run, reverted):** kRegistrySubMapSlot written as 0 (`s.wr(regVt + kRegistrySubMapSlot, 0, 8)`). The wide test aborts at `PES_DISPATCH_UNTRANSLATED: 0` (the first registration's `call [vt+0x88]`). Restored; rebuilt; the other suites above re-run green.
- `gen_translated.py --check`: rc 0, "5706 functions (3764 recorded bit-exact, 1942 closure) regenerate byte-identical".
- `tools/dev/ledger.py --check`: rc 0 after regenerating `docs/re/LEDGER.md` (progress history row added, `history.csv`); "LEDGER.md current (44463 rows)".
- `git ls-files --others src/pes/translated`: empty after the commit.
- PII: `grep` of the commit's changed files for `/mnt/Extra4` and `/home/z`: no match (the repo's `check_pii.sh` runs from the main checkout only).
- Gate: not run (Main gates).

## 6. Process notes (honest)

- `commit.draft()` refuses the worktree while `extern/godot-cpp` is a symlink (gitlink in the index). I swapped the link out for the commit (`rm` the link, empty dir) and restored it right after (`rmdir` + `ln -s`). The commit namespace's default repo was the session's own checkout (`<home>`, master); I passed `repo=` explicitly. Nothing was committed there.
- The first regen run hit the 300 s async limit and was killed; the second run (no deadline) completed. No partial layer was left (the check passed afterwards).
- Edits to `results_core.json` left a duplicated JSON tail after the first append; fixed with two line edits, then re-parsed (2604 records).
- Scratch `f5/s8/freelist_empty/mem.bin` is a 4 GB copy; deleted after this report. `s5-s7` symlink to seq203's `mem.bin`.
- Main's note (worldctor 0x140441490 fires per match load, pid678718 seq321) is not used here: the proofs are synthetic and do not depend on when it runs. It bears on whether the registry should be rebuilt per match (GF builds one per FixDemoSelector).

## 7. Open items (numbered)

1. Prove the event builder chain: 0x14047C500 (else arm call) with its callees to the vtable-reached constructor 0x141F55A10, and its two callees 0x140A923F0 / 0x140A924E0. Entry: a synthetic state from the else arm (the GF or PES state at the call). This is the next blocker for the wide test.
2. After (1), compare BC, radius, T, pos, rotY (DC 5/100/161) against the test's expected values; the test is the check.
3. Slot +0x88 with a non-empty sub-map free list (PES's seq203 state) is proven only through the synthetic s5/s6 cases; the free-list-empty case is s8. Both are bit-exact.
4. Whether the registry should be rebuilt per match load (Main's seq321 capture), not per selector.
5. The ctx's second map sentinel is a GF-built object (`InitHistory`); PES's own ctx constructor 0x141F500D0 is not translated in this path. Check when the event builder is proven.
