# pes-port-ReplayRuntime8: GetCurrentDirectoryW model written but its unit test is RED (uncommitted); 1408BE3C0 not re-proven; harness boundary answers, 1408BED00, the bind and the cursor decode NOT done

Branch `Presentation`, `d3f04aa1` (UNCHANGED: nothing committed this round). `git ls-remote origin Presentation` = `d3f04aa1f83e`. The worktree has UNCOMMITTED changes (listed in section 1). Build dir: none (not created this round). Captures: the `pid950952` copies (23 GB) were deleted; scratch 4.7 MB.

Labels: CONFIRMED = a unicorn run, a PE export parse, a disassembly. INF/OPEN as marked.

## 1. GetCurrentDirectoryW model (item 1): written, test RED, not committed

Written (uncommitted in the worktree):
- `tools/dev/translate/getcurdirw_model.s`: `mov ecx,ecx; mov r10, qword ptr gs:[0x60]` (PEB); `mov r10,[r10+0x20]` (process parameters); `movzx r11d, word ptr [r10+0x38]; shr r11d,1` (CurrentDirectory length in characters); size query: if `rcx <= r11` return `r11+1`, else `int3` (copy path not modelled). `code_hex` = `89c9654c8b1425600000004d8b5220450fb75a3841d1eb4c39d97705498d4301c3cc` (34 bytes, `as` + `objcopy`).
- `tools/dev/translate/import_stubs.json`: entry `kernel32.dll!GetCurrentDirectoryW`, `do: getcurdirw`, with evidence (the slot resolves to kernel32 RVA 0xE2BC = export index 414, parsed from `/usr/lib/wine/x86_64-windows/kernel32.dll`).
- `tools/dev/translate/import_slots.json`: slot `0x142532330` named `kernel32.dll!GetCurrentDirectoryW` (this is what the translator needs to route the call; NOT yet checked against a regeneration: it would change the layer's output for any function calling that slot).
- `tools/dev/translate/xlate.py`: `do == "getcurdirw"` → `pes_model_getcurdirw(s)`.
- `src/pes/translated/pes_stubs.hpp`: `pes_model_getcurdirw` (reads `s.rd(s.gs_base + 0x60)`, `+0x20`, `+0x38`; traps on the copy path).
- `tools/dev/translate/test_getcurdirw_model.py`: three cases (size query 0 → 24; buffer equal to length → 24; copy path traps).

**Status: the unit test is RED.** The size query returns `3` where `24` is expected, and in an isolated run unicorn reports `Invalid memory write` at the model's address (`rip 0x1000a`, inside the gs-prefixed load). The run is not explained. My isolation probe used a malformed byte string, so it proved nothing. The C++ mirror is unverified. Next step: isolate the gs-prefixed read in unicorn (compare with `frame_trace`'s working PES gs reads) before anything is committed. I did not commit a red test.

The mutation (removing `shr r11d,1`) was not run as a valid check, because the base test is red.

## 2. 1408BE3C0 re-proof (item 1, second half): NOT done

The captures were copied out of live2 (`pid950952` seq002–012) and unpacked, then deleted with the rest of the copies. The trace reaches the GetCurrentDirectoryW slot only after the model exists and works, so this waits on section 1.

## 3. Harness boundary answers for GF-service calls (item 2): NOT done

`run.py`/`snapcase.py` still refuse `gf-service` (`service_*`) stubs. No recorded-answer path was written, so `1408BED00` (`pid1845381` seq7492) is still excluded at `0x141957C50`.

## 4. Ring bind `140A8EE20` (item 3): frequency expectation (INF)

No `rec_140A8EE20` capture has landed in the folders I checked. Expectation (INF, not verified by a caller scan this round): the bind binds a replay unit's frame array to a source buffer, which is setup work, so it runs about once per unit per match start (21 units, so a handful of calls per match, not per tick). EVERY 1 with cap 6 is therefore the right sampling; EVERY 20 may never sample it in one match. A caller scan of `140A8EE20` would confirm this and is the next step.

## 5. Cursor advance (item 4): NOT decoded this round

No new analysis beyond round 7. The unit ops decoded so far (`140A8ED70` reset, proven; `140A8EE20` bind, not captured) do not advance the cursor.

## 6. Tests (item "build and run")

Not rebuilt this round: the layer's `pes_stubs.hpp` is the committed version (no change to the committed tree), so the four binaries' last green results stand (round 7: `pes_replay_human_test` 5 pass / 3 skip, `pes_native_service_test` 7, `pes_hud_clock_test` 4, `pes_anime_engine_test` 11 pass / 1 skip). Re-running them is needed only after section 1 is committed.

## 7. Next (ordered)

1. Isolate the gs-prefixed read in unicorn (section 1); make `test_getcurdirw_model.py` green; run its mutation; then regenerate the layer (the named slot changes closure output) and check drift before committing.
2. Re-prove `1408BE3C0` on re-copied `pid950952` seq002–012.
3. Harness recorded answers for gf-service calls, with its own test; then prove `1408BED00`.
4. Caller scan of `140A8EE20`; a capture (EVERY 1 is the right rate, per section 4).
5. Cursor advance from consecutive writer captures.
