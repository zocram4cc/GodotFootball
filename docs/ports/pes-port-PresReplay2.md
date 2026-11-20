# pes-port-PresReplay2: replay frame 0x14217DEF0 on the destructor path, layer, GF port (19-11-26)

Branch `Presentation` (origin), worktree `<workdrive>/gf-PresReplay2`, scratch `<workdrive>/gf-PresReplay2-scratch` (deleted captures; 7.6 MB left). Pushed: `ce6b19b8..4c827ab4` (merge of origin/main), then `4c827ab4..204cd348` (proof + layer, then the port). Verified with `git ls-remote origin Presentation`.

## 1. Merge (step 1)
- origin/main `009f902e` merged. One conflict: `tools/dev/ledger/universe.json` (generated). Took main's regenerated file (CrawlFix round 3); the Presentation side's `meta.closures` key is provenance only (written by `closure.py`), and every replay row is present in main's file.
- `ledger.py --check`: LEDGER.md current (44459 rows). `gen_translated --check`: byte-identical (5874 functions: 3769 recorded bit-exact, 2105 closure).

## 2. Why 0x14217DEF0 was held out (CONFIRMED)
- Generator rule (`gen_translated.build`): a recorded function is held out when its closure reaches a callee recorded as `translator bug`.
- Chain: 0x14217DEF0 -> 0x14217DE20 (when the copy happens; `R+0x48 != 0` frees the object) -> 0x141EB6FF0 -> 0x141978300 (recorded `translator bug`, cutscene set) -> `CALL [RAX]` on the object -> vtable `0x142612F98` slot 0 = `0x1408BE090` (CONFIRMED from the image). Also `0x1408BE090` is a `draw` boundary (not walked) in the universe.
- Also holding out 0x140471A80 (through 0x141978300).

## 3. Destructor captures and the proof
- Captures: `live2/pid2998104` seq567, 960, 1464, 1954, 1955 (hook `replay_frame_14217DEF0`). CONFIRMED at entry in all five: `R+0x48` non-zero (heap pointer), `R+0x118` non-zero, `R+0x1C = -1`, `R+0x21 = 1`. Registered in `run/capsets.json` as `replay14217_destructor_pid2998104`, status `protected`. Scratch copies deleted.
- **Real calls (5): 5/5 bit-exact, 0 mismatches. They do NOT run the destructor.** `FUN_1408bdf70` (the copy gate: nonzero when the stored record differs from the palette entry) returns 0, so the copy and `14217DE20` are skipped. Trace: `1408BDF70`, `1420B6A30`, `14217DEF0` only.
- **Synthetic states (5, labelled SYNTHETIC):** each real snapshot with one byte of the stored record flipped (`R+0x50` byte 0; `synth2.py`). The copy and the destructor run on real PES memory. `frame_trace recorded` rc 0 on all five.
- `snapdiff.py --bounded --only 0x14217DEF0 0x141978300` over the 10 snapshots: **0x14217DEF0 calls=10 replayed=10 mismatches=0 own_block=1.0 proven=True; 0x141978300 calls=15 replayed=15 mismatches=0 own_block=1.0 proven=True.** The proven members include 0x1408BE090, 0x14217DE20, 0x141EB6FF0, 0x140227930, 0x141EBED30.
- Rejected: flipping the palette entry instead of the record. It made the replay enter a loop over `1408BDF70` and an unmodelled import (`ucrtbase __stdio_common_vsnprintf_s`), rc 1. Not used.
- Records: `results_presentation.json` 0x14217DEF0 (bounded, synthetic, closure `closure_destr14217.json`, 356 members) and 0x141978300 (`closure_destr141978.json`, 16 members). Six GF-native services (0x1403FBC10, 0x1403FBCB0, 0x1403FBD00, 0x141B34C30, 0x141B34CB0, 0x141B34D80; `stubs.json` native_services) were removed from the closure members: `gen_dispatch` refuses a VA both translated and native. Production dispatch routes them to GF natives.
- `ro_tables.json`: 0x142B635D0, 0x100 bytes, for the byte-table read in 0x141E99BB0 (the lint required it). INF: the 0x100 window is the byte domain of the slot index; the callers' index range is not measured.
- Layer regenerated; `gen_translated --check` byte-identical. Commit `204cd348`'s first commit holds the proof and the layer.

## 4. GF port (step 3)
- No GF code reads replay frames. The only GF replay arithmetic is `src/sim/onthepitch/goalsequence.cpp` (`ReplayStartOffset_ms`, `ReplayFiresAt_ms`: goal timing and the replay window), not a frame read. Nothing deleted: none replaced by 0x14217DEF0.
- Ported `src/pes/replay_human.{hpp,cpp}` (predecessor's uncommitted work from `<workdrive>/gf-Presentation`, adopted) and `tests/pes_replay_human_test.cpp`; registered in `CMakeLists.txt` (pesfmt) and `tests/CMakeLists.txt`. Comments corrected: the object at +0x48 is not created by the read.
- Clock/replay frame to Godot: **no seam.** `godot/` has no replay code; the HUD clock (`src/pes/hud_clock_build.cpp`) is not exposed to `match.gd`.

## 5. Tests (pes_replay_human_test, `PES21_ROOT=<workdrive>/PES21`)
- Run: 4 of 6 PASS, **2 FAIL (red)**.
  - `PesReplayHuman.CopiesThePaletteEntryWhenTheHumanIsPresent`: the record's bytes stay zero (no copy).
  - `PesReplayHuman.ASequenceKeepsTheLastCopiedEntryAndReleasesAfterEachCopy` (new, pinned to the captured release state `+0x1C=-1`, `+0x21=1`): same failure, plus `+0x1C`=0 and `+0x21`=0.
- Observed: the release values change, which suggests the release ran but the 0xA0 copy into `R+0x50` does not land in the host record window. Not diagnosed. The image globals `DAT_14351AE30/34` match the install (`ffff0000`, `00000000`), so the palette gate is not the cause.
- Red state before the layer: link error `pes_fn_14217DEF0` undefined (feature missing), as expected.
- Mutation check: **not run** (no green baseline yet).

## 6. Not done / open
1. The two red tests above: root cause in the port's host-window write path (`replay_human.cpp` Read / the `PesMem::Window` setup), not in the translated function. Next step: dump `R+0x50` after the call from the state, and confirm the window pointer for kR.
2. Mutation check on the port after green (planned: GF frame stride 300 -> 299).
3. `diff_presentation_recorded` / presentation `--check` not run here (long run; Main gates).
4. Gate: not run (Main gates). `GATE fast` line: not produced.
5. Visual: none claimed.
6. Destructor proof uses synthetic states for the destructor branch; the five real calls only prove the copy-skip path. A real call that copies (a palette change by PES) would prove the branch without synthesis.
7. `tools/dev/translate/prove_bounded.py` is hard-wired to results_trapaction.json; its presentation use needs the spec dir and results file parameterised (not changed here).
