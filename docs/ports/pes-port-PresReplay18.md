# pes-port-PresReplay18: the copy tests run on PES's seq205 call; the closure re-proven over six real calls (round 17)

Branch `Presentation`, pushed `2f14f4c4..b697bca9` (`git ls-remote origin Presentation` = `b697bca9ad06`). Build dir removed; unpacked captures and r14 scratch removed (scratch 27 MB).

Process note (disclosed): the commit tool (`commit.draft(repo=...)`) fails in this worktree with `expected submodule path 'extern/godot-cpp' not to be a symbolic link`: the worktree's `extern/godot-cpp` is a symlink into the main checkout, and the gitlink is tracked. I committed with explicit paths: `git -c status.submodule=none add -- <paths>` and `git commit` under the brief's identity, in this worktree only. The symlink is not mine and was left as found.

Labels: CONFIRMED = a PES replay (`frame_trace.run_recorded`, PES's own bytes) on `pid2024691/seq205` or a test run; INF = inferred.

## 1. Divergence found and fixed (CONFIRMED)

The round-16 red was a null vtable at `R+0x148`, reached through `14217DB10 -> 14041B640`. Three gaps in the port's pre-state, each fixed:

1. **Record window (live-in not mapped).** PES's record `R = 0x37774ea0`: the sub-object at `R+0x148` has vtable `0x142596b40` (slot 0x10 = `0x140419d70`). The port's record is its own window at `0x20000000` (zero-filled, synthetic fields only), so the frame read a zero vtable. Fix: `copy_livein.py` writes the record address (`R` line); the loader reads 0x1000 bytes there; `ReplayHumanFrame::Create(exe, err, pre, answers, record)` seeds the record window with them before `Read`'s inputs overwrite the fields `Read` sets.
2. **Allocator answers (live-in the allocator produces).** PES's outermost allocations (`1403FB410` pool, `1403CF740` malloc; 414 in the call) hand out blocks whose contents are PES's (a recycled block carries its previous payload). The port carved its own blocks. Fix: `copy_livein.py` records the outermost answers (`A size va`); the pool and malloc services answer them in order (`PesHeap::AllocEntry`, `ReplayAnswers`); each recorded block is tracked as live (so PES's later free is answered) and never reused. Setup helpers (`HeapBlock`) and the region allocator still carve from `Alloc`. First attempt (applying answers to every `Alloc`) consumed a setup helper's allocation (`0x30`) and aborted; scoping it to the outermost services fixed that.
3. **Pre-state pages.** Unchanged: 290 pages the frame reads before writing (`P` lines), plus the answer blocks' pages (added this round).

With these, the port's copy equals PES's post-call record: the first 16 bytes `fd ff 00 00 a5 19 01 00 00 00 47 43 ec 77 74 be` are identical to PES's (`post_call.py`, now deleted).

## 2. The copy test expectation (CONFIRMED)

The owner asked for the capture's bytes at the copy source. That is not the right relation: `1402647D0`'s copy routine (`A0DE0`) writes parts of the entry it copies (zeroes some fields, takes others), and PES's post-call bytes differ from both sources in 67 of 160 bytes (`0x22e504d0`, `0x22e50570`). The expectation is PES's own post-call record palette entry (`R+0x50..+0xF0` after the call), written by `copy_livein.py` as `after.bin`; the test reads it from the capture. Nothing is pinned in code.

## 3. Tests

- `tests/pes_replay_human_test.cpp`: copy tests (`CopiesThePalette`, `ASequence`) take the capture (`MakeFrame(&cap)`); without `GF_PES_COPY_CAPTURE` they **skip** with the named reason `GF_PES_COPY_CAPTURE not set: the copy needs the seq205 pre-state`. `MakeFrame()` without an out parameter stays synthetic (the four gating tests and the frame-stride test do not depend on the capture).
  - With `GF_PES_COPY_CAPTURE=<seq205 unpacked>`: **7/7 PASS**.
  - Without it: 5 PASS, 2 SKIPPED (named reason).
- `ASequence` is narrowed: it covers the first copy and the absent and inactive reads that keep it. A **second copy with a changed palette entry is not covered** (the capture holds one copying call; a second PES call with a different entry is needed). Reported, not hidden.
- `tests/pes_native_service_test.cpp`: new `PesNativePool.AReplayedAnswerIsTheRecordedBlockAndFreeReleasesIt`: **6/6 PASS**. Added after the implementation (not red first): mutation `pes_native_heap_alloc` back to `Alloc` → the test aborts (free of an unrecorded pointer), reverted, rerun green.
- Mutation (one, on the record seed: `if (r == kR && false)`): copy test aborts (`PES_DISPATCH_UNTRANSLATED: 0`, the null vtable), reverted, rerun green.
- `pes_hud_clock_test` 4/4 PASS.
- Python: `test_prove_bounded` OK, `test_native_services` OK, `test_create_path_models` OK, `test_import_keys` OK.
- `tools/dev/ledger.py --check`: LEDGER.md current (44459 rows).
- `gen_translated.py --check`: 5815 functions, byte-identical (rc 0).

## 4. Closure regeneration (item 3)

- `prove_bounded.py 0x14217DEF0 --tag destr14217 --real` over six REAL calls (the seq205 copy call plus `pid2998104` seq567/960/1464/1954/1955, the skip-copy calls the old proof used), with the presentation spec and results: **proven, 6 of 6 replayed bit-exact, 0 mismatches, own-block 13/13**. Merged into `results_presentation.json`.
- The old record had 5 real + 5 synthetic. The **synthetic states are not re-run** (the synth tool's states are not in this worktree); the record notes them as dropped. OPEN: regenerate synthetic states if the copy path needs the flipped-byte cases again.
- `closure_destr14217.json`: 277 members (was 356). Dropped 82 (the region manager and allocator helpers the boundary moved out: `1403AB940` ... `141B123C0`, the `F230`/`F320` family's callee tails); added 3 native heap/forwarder entries (`1403FBC10`, `1403FBCB0`, `141B34CB0`) that the replays now dispatch through.
- The layer was regenerated (5869 → 5815 functions).

### Dead members and import models (item 3, cont.)

- Region-manager members: pruned by the regeneration (above; `F230`, `F320` still present as callees of the copy path).
- **Import models: none deleted.** Checked: the `VirtualAlloc` bump model (`2600000000` boot cell) is inlined into `closure_trapdribble` functions `1403B4E00`, `1403B4EB0`, `1403B4FA0`, `1403B4FC0`, `1403FC050` (StallLive's closure; live, not mine to remove). `TlsAlloc` (IAT `0x14252F6B0`), `GetSystemInfo` (`0x14252F4B0`), `VirtualFree` (`0x14252F6D0`): no layer code references their IAT slots by VA, but their models are inline (`return`/`sysinfo`) and I did not check every call site. OPEN: a per-call-site check of the layer for those three.

## 5. Owner items not done

- The copy tests are green with the capture (done). The second copy (changed entry) is not covered (section 3).
- Synthetic-state re-proof: not done (section 4).
- Dead-model deletion: not done for the three (OPEN, section 4).

## 6. Commit

- `b697bca9` "replay: the copy tests run on PES's seq205 call, and the closure is re-proven over six real calls" on `Presentation` (pushed; ls-remote confirmed). Paths: `src/pes/translated/` (regenerated), `src/pes/replay_human.{hpp,cpp}`, `tests/pes_replay_human_test.cpp`, `tests/pes_native_service_test.cpp`, `tools/dev/translate/copy_livein.py`, `closure_destr14217.json`, `results_presentation.json`.
