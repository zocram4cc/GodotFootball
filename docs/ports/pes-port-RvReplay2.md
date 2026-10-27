# RvReplay2: all live captures processed, FixDemo mismatches fixed in xlate.py, import models, translated code regenerated (27-10-26)

Branch `RvReplay` pushed; `git ls-remote origin RvReplay` = `86240e65`. Commits: `d329f4bd` (translator fix, import models, diffmain TEB), `86240e65` (regenerated translated code, 802 files). Worktree `<workdrive>/gf-RvReplay`. Scratch `<workdrive>/gf-RvReplay-scratch/` (PES-derived; not committed). Gate not run (Main gates). Live inputs read only; nothing written to `live/` or `rendezvous/`; PES not signalled.

Labels: CONFIRMED = read in tool output or by script; INF = inferred, reason given; OPEN = not settled.

## 1. Summary

- **Both FixDemo mismatches are fixed (CONFIRMED).** `141F4D2F0` 4/4 cases bit-exact (was 3 of 4 wrong); `141F4B1D0` 1/1 bit-exact (was rax and written bytes wrong).
- **Named class (CONFIRMED):** indirect JMP through a jump table, `mov eax,[rdx+rax*4+disp]; add rax,rdx; jmp rax`. The table load overwrote the index register. `xlate.py` switched at JMP time on the register holding base + table entry (truncated to uint32), so no case matched and every such dispatch took `default`. Divergence: the harness stores 0x147 at `[rbp+0x20]` (case 70, VA `141F4DC6A`); the translation stored 0x100 via the default path (`141F4DD2B`).
- **Fix (general, `xlate.py` JMP-reg emission and prepass):** dispatch on the JMP register minus the table base; each distinct table entry that is a local label is a case; the unused `index` key is gone. Every table-dispatched function is affected; the committed translated code (`86240e65`) is regenerated from the fixed translator.
- **Checks (CONFIRMED):** `driver.py --check` rc=0 for core, cutscene, presentation, impure and pilot (every recorded diffed VA still bit-exact). The tool prints no per-VA before/after verdict list. `gen_translated.py --check`: first run after the fix reported 802 drifts (expected: the generated code changed); after regeneration, `3132 functions (2973 recorded bit-exact, 159 closure) regenerate byte-identical`, rc=0.
- **"seq010/seq011 snapdiff crash" (CONFIRMED):** not a traceback. `xlate.py` raised `SystemExit` ("no translator model for do='qpc'", `kernel32.dll!QueryPerformanceCounter`). Fixed by translator models for `qpc`, `tls_get`, `tls_set` and the new `tebread`. Both replay: 4 functions each, 0 mismatches.
- **GetLastError:** modelled as `tebread` (DWORD at `gs:[0x68]`) for `kernel32.dll` and `kernelbase.dll`. Offset `+0x68` is [INF] (standard x64 TEB layout; ClientId.Thread at +0x48 confirmed for this snapshot in `pes-re-import-slots.md`).

## 2. Code changes

| commit | file | change |
|---|---|---|
| d329f4bd | `tools/dev/translate/xlate.py` | JMP-reg table dispatch on the entry; `import_model_lines` gains `qpc`, `tls_get`, `tls_set`, `tebread`; `index` removed from `jmpreg` |
| d329f4bd | `tools/dev/translate/frame_trace.py` | `load_import_models`: `tebread` stub (`65 8B 04 25 <disp32>; C3`) |
| d329f4bd | `tools/dev/translate/import_stubs.json` | `GetLastError` for `kernel32.dll` (translator key) and `kernelbase.dll` (harness key); contract text |
| d329f4bd | `tools/dev/translate/diffmain.py` | `gs_base` from `argv[7]` (the TEB); it was uncommitted in `0a0478a7` despite that commit's message |
| 86240e65 | `src/pes/translated/**` | regenerated: 787 modified, 15 new closure files |

Checks run: `py_compile` of `xlate.py` and `frame_trace.py`; `import_stubs.json` parses; `driver.py --check` (above); `gen_translated.py --check` (above). `test_recorded_call.py` not re-run. `tools/check_pii.sh` not run from the main checkout; changed files grepped for machine paths and machine paths: none in `src/pes/translated`.

## 3. Per-capture results (`proc/seqNNN/recorded.json`, `snapdiff.json`)

| capture | recorded call | RAX | traced callees | replayed funcs | PROVEN | mismatched |
|---|---|---|---|---|---|---|
| seq001 | ok (HUD radar, no spec: not replayed) | 0x1 | 172 | 14 | 14 | 0 |
| seq002 | ok | 0x1 | 14 | 2 | 2 | 0 |
| seq003 | ok | 0x0 | 1 | 1 | 1 | 0 |
| seq004 | ok | 0x100 | 3 | 2 | 2 | 0 |
| seq005 | ok | 0x1 | 14 | 2 | 2 | 0 |
| seq006 | ok | 0x29f | 8 | 4 | 4 | 0 |
| seq007 | ok | 0x47 | 6 | 3 | 3 | 0 |
| seq008 | **blocked**: `kernelbase.dll!FlsGetValue` | - | 80 | 16 | 16 | 0 |
| seq009 | ok | 0x0 | 0 | 0 | 0 | 0 |
| seq010 | ok | 0x1 | 6 | 4 | 4 | 0 |
| seq011 | ok | 0x1 | 6 | 4 | 4 | 0 |
| seq012 | ok | 0x0 | 27 | 23 | 23 | 0 |
| seq013 | **blocked**: `ntdll.dll!RtlAcquireSRWLockShared` | 0x6b9 | 4 | 4 | 4 | 0 |
| seq014 | ok (type-13 hook `140A115A0`) | 0x2e | 0 | 0 | 0 | 0 |
| seq015 | ok (type-13 hook `140A115A0`) | 0xb | 0 | 0 | 0 | 0 |
| seq016 | ok | 0x2025e1c0 | 72 | 14 | 14 | 0 |
| seq019 | ok | 0x1 | 46 | 3 | 3 | 0 |
| seq020 | ok | 0x0 | 2 | 2 | 2 | 0 |
| seq021 | ok | 0x0 | 2 | 2 | 2 | 0 |
| seq022 | ok | 0x34f92e50 | 59 | 6 | 5 | 1 (`141F5F330`) |
| seq023 | **blocked**: `ntdll.dll!RtlAcquireSRWLockShared` | 0x6b9 | 56 | 31 | 31 | 0 |
| seq024 | ok | 0x0 | 96 | 51 | 50 | 1 (`1408A40F0`) |
| seq025 | ok | 0x205b0470 | 49 | 6 | 5 | 1 (`141F5F330`) |
| seq026 | ok | 0x2c | 19 | 8 | 8 | 0 |
| seq027 | ok | 0x1 | 15 | 7 | 7 | 0 |
| seq028 | ok | 0x1 | 172 | 14 | 14 | 0 |

seq017 and seq018 do not exist in `live/`. No MI-refresh capture (`113B280`, `113B520`) landed in seq001–028. Recorded-call replay: snapdiff does not replay the hook entry itself (no spec, §5).

## 4. Mismatches and blockers

1. **`141F5F330` (cutscene; seq022 1 case, seq025 1 case):** written-bytes mismatch; not investigated (OPEN). Same jump-table class is the first suspect; lockstep to the first divergent write, as for FixDemo.
2. **`1408A40F0` (camera; seq024, 1 case):** mismatch; not investigated (OPEN).
3. **`kernelbase.dll!FlsGetValue` (seq008):** needs an import model. The FLS slot location in the x64 TEB is not confirmed here (OPEN: read the Wine `kernelbase.dll` export bytes).
4. **`ntdll.dll!RtlAcquireSRWLockShared` (seq013, seq023):** needs an import model (OPEN: verify the Wine ntdll bytes before modelling).
5. **Hook-entry specs (ticket item 3): not done.** The captured calls (cutscene create/load, FixDemo matcher/evaluator/callers, restart builders, camera and its caller, HUD radar, MI refresh, type-13) are judged only through their callees. Needed: specgen impure specs for each hook VA (`tools/dev/diffharness/specgen`, `generate --mode impure` over a set file of the universe rows, which are keyed without `0x`), copied into `specs-union`.
6. **Type-13 emulated frame advance (ticket item 5): not run.** `G+0x180`, per-frame bindings, and the `MI+0x1198` +1 advance need a spec-driven emulation. No MI-refresh capture exists in the set.

## 5. PROVEN totals (per capture replays; function-level)

A function is PROVEN when every replayed case is bit-exact in every capture it appears in. Coverage (own block) is recorded, not a gate. Subsystem = first ledger root of the function (`tools/dev/ledger/universe.json`, `functions[].roots`).

| subsystem | functions replayed | PROVEN |
|---|---|---|
| camera | 27 | 26 |
| cutscene | 25 | 24 |
| referee | 14 | 14 |
| HUD | 13 | 13 |
| AI | 12 | 12 |
| restart | 11 | 11 |
| pre-match | 8 | 8 |
| replay | 6 | 6 |
| gameplay | 3 | 3 |
| in-match menu | 3 | 3 |
| **all** | **122** | **120** |

Mismatched functions: `141F5F330` (cutscene), `1408A40F0` (camera). The two FixDemo functions (`141F4D2F0`, `141F4B1D0`) are PROVEN after the fix.

Full per-function table (122 rows: captures, replayed cases, coverage, verdict): `<workdrive>/gf-RvReplay-scratch/table.md` (generated by `aggregate.py`).

## 6. Open items

1. `141F5F330` and `1408A40F0` mismatches (§4.1–2).
2. Import models: `FlsGetValue`, `RtlAcquireSRWLockShared` (§4.3–4).
3. Hook-entry specs and their replay (§4.5), type-13 spec first.
4. G+0x180 and the one-frame advance (§4.6).
5. A per-VA verdict-change list for `--check` is not produced by the tool; the pre-fix records were not diffed one by one.
