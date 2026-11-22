# pes-port-PresReplay20: the release entry 0x14037FDB0 is a boundary service; the release test no longer crashes, and is still red on record bytes (round 19)

Branch `Presentation`, pushed `e1cae390..cb5b55b3` (`git ls-remote origin Presentation` = `cb5b55b325de5388d43a22aa64ce3e68857bf505`). Build dir removed; unpacked captures removed (scratch 12 MB).

Labels: CONFIRMED = gdb backtrace, a PES replay (`frame_trace.run_recorded`), a test run, or a scan of the image; INF = inferred; OPEN = not settled.

## 1. The entry on the release path (CONFIRMED)

The release test's gdb backtrace (bottom to top, the crash in the recursion): `14217DEF0 -> 14217DE20 (release) -> 141EB6FF0 -> 141AFE690 -> 141AFE4F0 -> 1403CF7E0 -> 1423A1770 -> 1423A14D0 -> 1403E1CC0 (destructor: writes vtable 0x14258A4B0, then frees through 1403CF770) -> [14037FDB0, a tail jump] -> 14038E6F0 -> 14038DF60 -> 14038F320 (the TLS-slot region lookup, gs:[0x1480+idx*8]) -> 1403AB660 (region create) -> 1403AB470 <-> 1403AB570 (recursion, SIGSEGV in PesMem::wr)`.

- `1403E1CC0` (the destructor's release) calls `14037FDB0` directly, then the free entry `1403CF770` (already a boundary service). `14037FDB0`'s bytes are `jmp 14038E6F0` (`e9 3b e9 00 00`), the region release core. Its callers in the layer: `14028EA30, 140298060, 1403E1CC0, 1403E1DE0, 1403F2F60, 1415D18E0`; I checked each call site: rax is overwritten or unused before it is read.
- So the entry on the release path that the port did not model is `0x14037FDB0`, the region release counterpart of the malloc/free boundary (round 16 recorded `1403CF770 -> 14037FDB0 -> 14038E6F0` as the free route).

## 2. The change (CONFIRMED)

- `tools/dev/translate/stubs.json`: a `native_services` entry `{"va": "14037FDB0", "op": "free", "code_hex": "e93be90000cc…"}` (the 24 image bytes at its entry, checked by the generator). The op `free` is the existing `pes_native_free`: null is a no-op, a block the GF arena holds is released, any other pointer is nothing (PES state the port never allocated).
- Layer regenerated: `closure/pes_fn_14037FDB0.cpp` removed; its six callers now dispatch through the table (`pes_dispatch_gen.cpp`, `translated.hpp`). `gen_translated.py --check`: 5868 functions (3769 recorded bit-exact, 2099 closure), byte-identical.
- Proof: `prove_bounded` over the six real calls and the five synthetic copy states (the round-18 set, rebuilt in scratch): 11 of 11 bit-exact, own-block 13/13, 0 mismatches; merged into `results_presentation.json`.
- OPEN: `14038E6F0`, `14038DF60`, `14038F320` remain translated. They are closure members the proof's PES trace enters, but the port no longer reaches them (the boundary stops the route). Pruning them needs the port-side reach of the closure (the members the translated run enters), not the PES trace. Not done.

## 3. The release test (CONFIRMED: red; the crash is gone)

- With `GF_PES_RELEASE_CAPTURE` naming the flipped state (`pid2998104/seq567`, stored byte 0 flipped, PES-replayed by copy_livein.py) the test now runs to its comparison and fails on the record bytes (before this change it aborted in the recursion).
- The copy's source is the same as PES's: the probe printed `1402647D0`'s stored sources `23ba0070`, `23ba0110`, `23ba0110` (PES's `S` lines exactly).
- Result: port `fd ff 00 00 a5 19 01 00 00 00 47 43 ec 77 74 be …`; PES (the flipped state's after.bin) `fe ff 00 00 97 24 01 00 00 00 34 43 84 e5 9e 3e …`. 31 of 160 bytes differ. The port's bytes equal the seq205 capture's PES after bytes (a different state) except two bytes.
- PES's own flipped record before the call (R+0x50): `01 ff 00 00 97 24 01 00 00 00 34 43 84 e5 9e 3e …`: the PES after keeps bytes 4..15 of it and changes byte 0 (`01` -> `fe`). So in PES the copy leaves those bytes as they were before the call; the port writes a different value there (`a5 19 01 00 00 00 47 43 ec 77 74 be`).
- INF: the port's values at 4..15 come from a state the port holds (the install's data or a synthetic window) rather than the flipped state's pre-state. Not confirmed: I did not trace the port's writes to R+0x50. OPEN.
- Inputs used: the call's own (human 15, guard 0x867, active 1, palette 15) and PresentHuman's palette entry (the palette entry bytes are not the capture's; the copy's source is the registry entry, which matches PES). The palette entry is a candidate input difference: OPEN.

## 4. Mutation (run, reverted)

- `pes_native.cpp`: the test-mode rule in `PesHeap::Free` (`if (foreignFreesOk_) return;`) removed. `pes_native_service_test --gtest_filter='*InTestMode*'` ABORTS (exit 134: "free of a pointer this heap did not allocate"). Reverted; the native service tests then pass 7/7.

## 5. Checks

- Copy tests with `GF_PES_COPY_CAPTURE=seq205`: 7/7 PASS. Without any capture: 5 PASS, 3 SKIP (named reasons). Release test with its capture: FAIL (section 3).
- `pes_native_service_test` 7/7. `pes_hud_clock_test` 4/4. `test_native_services.py` OK (the new native entry checked against the image). `ledger.py --check` current (44459 rows).
- `gen_translated.py --check`: byte-identical (5868).

## 6. Commit

- `cb5b55b3` on `Presentation` (pushed; ls-remote confirmed). Paths: `tools/dev/translate/stubs.json`, `tools/dev/translate/results_presentation.json`, `src/pes/translated/` (9 files: the removed `closure/pes_fn_14037FDB0.cpp`, six caller files, `pes_dispatch_gen.cpp`, `translated.hpp`). Explicit paths, `git -c status.submodule=none`.

## 7. Open items

1. The release test's byte mismatch (section 3): trace the port's writes to R+0x50 in the release run to find which state supplies bytes 4..15; check whether the palette entry (PresentHuman vs the capture's entry for palette 15) is an input of the copy.
2. Prune `14038E6F0`, `14038DF60`, `14038F320` from the closure with the port-side reach.
3. The second-copy hook recipe (round 19 report) is armed on the rig; captures pending (Main).
