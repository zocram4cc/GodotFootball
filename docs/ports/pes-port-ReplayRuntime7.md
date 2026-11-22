# pes-port-ReplayRuntime7: the seven headers are all present (my round-6 "six" was a miscount); slot 0x142532330 is KERNEL32!GetCurrentDirectoryW (a named kernel export, not protection code), not yet modelled; the four required test binaries are green; 1408BED00 harness extension, 140A8EE20 bind, cursor decode and slice 3 are NOT done

Branch `Presentation`, `d3f04aa1` (unchanged this round: no code commit; `git ls-remote origin Presentation` = `d3f04aa1f83e`). Build dir removed. Scratch 4.7 MB. No copies of captures remain.

Labels: CONFIRMED = a test run, a PE export-table parse of the DLL on disk, a capture read, a disassembly. INF/OPEN as marked.

## 1. The seven hand-written headers (item 3, CONFIRMED)

- All seven are present in `src/pes/translated/` and tracked in git: `pes_cond.inl` (71 lines), `pes_dispatch.hpp` (21), `pes_native.cpp` (402), `pes_native.hpp` (162), `pes_state.hpp` (302), `pes_stubs.hpp` (249), `pes_survey.hpp` (113). `git diff HEAD` on all seven is empty, and `pes_stubs.hpp` at HEAD carries the three mirrors (`pes_model_vsnprintf_s`, `pes_model_strncmp`, `pes_model_toupper`).
- Round 6 said "restored six": that was my miscount. All seven were restored from HEAD by one `git checkout`, and `pes_stubs.hpp` was then re-applied by hand (committed in `d3f04aa1`). None is missing. They are hand-written, not generated, so "regenerable" means "restored from git", which is what happened.

## 2. Slot 0x142532330 (item 1, CONFIRMED classification; model NOT written)

- The image holds `0x1401C1390` in the slot (an in-image value, not an import key). `import_slots.json` has no name for it. Its neighbours are named Wlan and Steam imports, which made me suspect protection code.
- The runtime value (the capture, `pid950952` seq002, via a trace run) is `0x6F76A60DE2BC`. Its mapping in the capture's `index.json` is `kernel32.dll` (base `0x6F76A60D0000`), so the RVA is `0xE2BC`.
- Parsing the on-disk `/usr/lib/wine/x86_64-windows/kernel32.dll` export table (the Wine build in this environment): RVA `0xE2BC` is export index 414, **`GetCurrentDirectoryW`**. Its chain: the call site `0x14037FC35` (`xor edx,edx; xor ecx,ecx; call [slot]`) is `GetCurrentDirectoryW(0, NULL)`, a size query.
- The trace fault (`unmapped data at 0`) comes from running Wine's real body with a NULL buffer inside the harness, not from PES. So this is a named kernel export and not protection code. Under Main's rule it is modelled at the boundary, not stubbed silently.
- Why it is NOT modelled yet: the return value of a size query is the length of the PES process's current directory plus one. That length is a recorded fact of the capture (it is machine-dependent), and a constant would hard-wire one machine's path. The capture holds it in the PEB: `TEB+0x60` → `PEB+0x20` (ProcessParameters) → `+0x38` (CurrentDirectory `UNICODE_STRING`, `Length` in bytes). The model must answer that recorded value (as the heap allocator's answers do) rather than a constant. I did not implement the answer path this round (budget). Next step: an import-model kind that answers `Length/2 + 1` from the recorded PEB for the `(0, NULL)` case, with a unit test and a mutation, then re-prove `1408BE3C0` (its other imports are already modelled).

## 3. 1408BED00 and the GF-service boundary (item 2, NOT done)

- Not implemented. The harness still refuses `gf-service` stubs (`run.py` `service_` branch), so the proof of `1408BED00` (one capture, `pid1845381` seq7492) remains excluded at `0x141957C50` (the graphics-device allocator). Extending the harness to answer `GetAllocator`-style boundary services from the capture's recorded results is the owner-ruled change; it touches the proof harness (`run.py`/`snapcase.py`) and needs its own tests. Not started this round.

## 4. Slice 2 and slice 3 (NOT done)

- `140A8EE20` (unit bind): no capture has landed in the folders I checked (`pid1845381` lists `rec_140A8ED70` and the human writers only, per Main's last note). Not proven.
- Cursor advance: still not located. The decoded unit ops are the reset `140A8ED70` (proven, 1 call) and the bind `140A8EE20` (`FUN_140a8ee20`: vcall slot `+0x48`, frame array `= src+0x3AA0`, cursor/capacity/total copied from `src+0x5E..0x64`, flag at `+0x28`). Neither advances the cursor.
- Slice 3 (playback) plan step: not written this round. The plan file `local://replay-system-plan.md` still has S3 as written in round 3 (the `14217DEF0` copy and the absent marker against recorded unit frames).

## 5. Test binaries (CONFIRMED this round, `build/` built for these four targets only)

| target | result |
|---|---|
| `pes_replay_human_test` | 5 PASS, 3 SKIP (no `GF_PES_COPY_CAPTURES`), rc 0 |
| `pes_native_service_test` | 7 PASS, rc 0 |
| `pes_hud_clock_test` | 4 PASS, rc 0 |
| `pes_anime_engine_test` | 11 PASS, 1 SKIP (render-plot env var), rc 0 |

No new proof tests were added this round. The unit tests for the three models (`test_vsnprintf_model.py` 3/3, `test_strncmp_model.py` 6/6, `test_toupper_model.py` 3/3) were run in round 6 and are unchanged.

## 6. Process

- No `sed` on repo files this round (rule followed).
- Build: `cmake --build build --target <4 targets> -j8` under `flock /tmp/gf-build.lock`, `nice -n 19`; ~1500 s with ccache. The build dir is removed.

## 7. Next (ordered)

1. Implement the recorded-answer model for `GetCurrentDirectoryW(0, NULL)` (section 2): the PEB read is the answer; test and mutation; re-prove `1408BE3C0` on `pid950952` seq002–012 (copies, outside live2).
2. Extend the harness for gf-service boundary answers from the capture (section 3), then prove `1408BED00` on its capture.
3. Capture `140A8EE20` (Main's hook on `pid1845381`, or the next launch) and prove the bind; decode the cursor advance from the writers' captures.
4. Slice 3 plan step (playback against recorded unit frames).
