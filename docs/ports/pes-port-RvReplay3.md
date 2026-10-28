# RvReplay3: both mismatches fixed by general translator changes; seq013/seq023 replayed; seq008 BLOCKED; type-13 and hook specs NOT DONE (27-10-26)

Branch `RvReplay` pushed: `git ls-remote origin RvReplay` = `e8b4af53fee2` (CONFIRMED). Commits: `f84c1247` (merge of `origin/main` into RvReplay; one add/add conflict in `tools/dev/rendezvous/rendezvous.lua`, resolved by keeping the RvReplay copy (the one the captures came from); Main should review it), `e8b4af53` (translator fixes, lockstep tooling, Wine import models, regenerated translated layer). Worktree `<workdrive>/gf-RvReplay`; scratch `<workdrive>/gf-RvReplay-scratch/` (PES-derived, not committed). Gate not run (Main gates). PES was not signalled, written or paused; all reads are from the captures under `live/`. Labels: CONFIRMED = read in tool output or by script; INF = inferred, reason given; OPEN = not settled.

## 1. Summary

| ticket item | status |
|---|---|
| 1. Hook specs from captures (specgen impure), per-hook replay verdicts | **NOT DONE.** No spec was generated for any hooked entry (`1420E2910`, `141F4B6C0`, `141F4D2F0`, `141F41CE0`, `141F57840`, `140A115A0`, camera, restart). Per-hook verdicts are therefore not available; the only verdicts are per-callee (below). |
| 2. Mismatches `141F5F330`, `1408A40F0` | **FIXED** (general translator fixes; lockstep shows the exact class). Each is bit-exact on its captured case in the capture the lockstep ran (seq022, seq024). seq025 (second `141F5F330` case) not re-run. |
| 3. Import models: `FlsGetValue`, `RtlAcquireSRWLockShared`/`Release`; reprocess seq008, seq013, seq023 | **Models DONE** (evidence: Wine ntdll/kernelbase disassembly). **seq013 and seq023 replayed, 0 mismatches. seq008 BLOCKED** after the FLS and SetLastError models (emulation invalid instruction, §4). |
| 4. Type-13 (seq014/015): `G+0x180`, bindings, forward run, frame counter `0x140A128E4`, write-hooks | **NOT DONE.** No spec-driven emulation of the type-13 body was built this session. OPEN (§5). |
| 5. Totals per subsystem | Partial (§6). `aggregate.py` was not re-run after this session's changes. |
| Push | Done (`e8b4af53fee2`). |

## 2. Mismatch 1: `1408A40F0` (seq024), CONFIRMED fixed

- Lockstep (new tooling, §3): control flow identical over 231 instructions; the first state difference is at `1408A46B1`: `xmm9` harness `+0.0`, translated `-0.0`. Producer `1408A46AC`: `maxss xmm9, xmm0` after `xorps xmm9, xmm14` (the sign flip) on a tie.
- Cause (CONFIRMED by Intel MAXSS/MINSS rule): when the two values compare equal (here `-0.0` vs `+0.0`) or either is NaN, MAXSS/MINSS return the **source** operand. The scalar emitter in `xlate.py` (`do_ssarith`) used `>=`/`<=`, which returned the destination on ties. The packed MINPS/MAXPS emitters already use `<`/`>` and were correct.
- Fix: scalar MAXSS uses `dst > src ? dst : src`; MINSS uses `dst < src ? dst : src`.
- Result: lockstep 231 PCs and 231 states identical; snapdiff seq024 `0x1408A40F0`: 1 case, 0 mismatches. Own-block coverage 0.35 (recorded, not a gate).

## 3. Mismatch 2: `141F5F330` (seq022, seq025), CONFIRMED fixed for seq022

- Lockstep: first control divergence at `141F5F409 → 141F5FC00` (harness) vs fall-through (translated). The translated `jmp rcx` at `141F5FC95` was emitted as `/*UNRESOLVED JMP rcx*/return;`.
- Cause (CONFIRMED): the prepass finds a switch's base register by a backward scan with a 20-instruction window. The `lea r8,[0x140000000]` sits 21 instructions above the JMP, so the table dispatch was not resolved, and the unresolved JMP silently returned. That returned early (rax 0 vs 0x34f92e50) and changed the write stream at write record 5012.
- Fix (general): (a) the base register's value comes from its nearest preceding write, with no fixed window; (b) an unresolved register JMP now emits `__builtin_trap()` (like an untaken out-of-body Jcc) instead of a silent `return`. Committed code had 0 UNRESOLVED JMP, so (b) changes no committed behaviour. Scratch closure builds had 44 files with the silent return (including FixDemo `141F4D2F0`, `141F4B1D0` builds).
- Result: seq022 `141F5F330`: 1 case, 0 mismatches. seq025 (the other case) **not re-run**.

## 4. seq008 (`cut_create_141F57840`): BLOCKED

- Before this session: `kernelbase!FlsGetValue` unmodelled.
- Models added (hand-encoded Wine x64, `import_stubs.json` `code_hex`; C++ mirrors in `xlate.py`; verified by capstone): `kernelbase!FlsGetValue` (`fls_get`), `ntdll!RtlSetLastWin32Error` (`setlasterror`). Evidence: Wine `kernelbase.dll` FlsGetValue at file offset `0x218A0` and `ntdll.dll` RtlFlsGetValue at `0x29540` (reads FLS vector at `TEB+0x17C8`, `gs:[0x30]` = TEB self, failure status `0xC000000D`), RtlSetLastWin32Error at `0x39CB0` (`mov [rax+0x68], ecx`).
- Current state (CONFIRMED): `recorded.json` `err`: `emu error: Invalid instruction (UC_ERR_INSN_INVALID) (rip 0x6ffffbc71f26)`, rax `0x6b0`. The rip is in the import page, at a non-entry offset.
- Checked and refuted (CONFIRMED): the hypothesis that another export's model overlaps the FLS body. `kernelbase!GetLastError` is at RVA `0x89150`, `FlsGetValue` at `0x218A0`.
- OPEN: which instruction transfers to that address. Next test: log every RIP entering the import page (a code hook over the import page, the last 20 RIPs before the fault) and the value returned by the FLS model in this call.
- The earlier `ntdll!RtlAcquireSRWLockShared` model was the blocker for seq013 and seq023 (not seq008).

## 5. Type-13 (seq014/seq015): NOT DONE (OPEN)

- Not run: no `G+0x180` read, no per-frame binding read, no emulated frame advance (`MI+0x1198` +1), no write-hooks for the ball or S4. The record/type-13 notes in `docs/re/pes-re-records.md` and `pes-re-type13.md` are unchanged.
- Blocker: the type-13 body `0x140A115A0` has no spec (`specs-union`); the captured calls seq014 and seq015 have 0 traced callees, so the existing replay machinery has nothing to run forward. Building the spec (specgen impure for `140A115A0`, plus the two hand-kept `G+0x180` relink specs `14065C2A0`, `140978E70` noted in `diffharness/README.md`) is the next step.

## 6. Replay results and totals

Per-capture results from this session (`proc/seqNNN/snapdiff.json`, CONFIRMED):

| capture | hook / call | recorded | functions replayed | cases | PROVEN (bit-exact) | mismatched |
|---|---|---|---|---|---|---|
| seq013 | (SRW, rerun) | ok, RAX 0 | 30 | 51 | 30 | 0 |
| seq023 | (SRW, rerun) | ok, RAX 0 | 56 | 104 | 56 | 0 |
| seq022 | (`141F5F330` lockstep) | ok (earlier) | 1 of its functions | 1 | 1 | 0 (was 1 before fix) |
| seq024 | (`1408A40F0` lockstep) | ok (earlier) | 1 | 1 | 1 | 0 (was 1 before fix) |
| seq008 | cut_create | **blocked** (§4) | - | - | - | - |

Previous per-function totals (RvReplay2, §5 there): 122 functions replayed, 120 PROVEN; mismatches `141F5F330`, `1408A40F0`. After this session, the two mismatched functions are bit-exact on their seq022/seq024 captured cases; seq025 is not re-run, so `141F5F330` is not yet proven on all its captures. The per-subsystem table was not regenerated; expect at most 122 PROVEN and 0 mismatched on the replayed cases once seq025 is re-run.

Driver `--check` (CONFIRMED, `check/*.out`):
- `cutscene`: 33 of 33 recorded-diffed functions still bit-exact (rc 0).
- `presentation`: 21 of 21 still bit-exact (rc 0).
- `core`: "all 0 recorded-diffed functions" and `impure`: "all 0". This is vacuous: `--check` re-runs only the record's diffed VAs that have a spec in the default spec dir, and for these two sets none do. Not a pass. OPEN.
- `pilot`: **rc 1**, output ends after generating dispatch headers with no traceback. Cause not isolated (OPEN). Treat as a failing check until diagnosed.

Committed translated layer: `gen_translated --check` (after regeneration from the merged, fixed translator): `3132 functions (2973 recorded bit-exact, 159 closure) regenerate byte-identical`, rc 0. 50 committed files changed (MAXSS/MINSS, base-LEA window, setlasterror). Two functions are held out by the generator (`0x1403BB670`, `0x141F5D480`: closure reaches a mistranslated callee); these lines are present in the pre-change output too (INF: not caused by this work).

## 7. Tooling added

- `tools/dev/translate/lockstep_trace.h`, `tools/dev/translate/lockstep.py`: flag-gated per-instruction trace (`GF_XLATE_TRACE=1` in the snapdiff build, `GF_LOCKSTEP_OUT` in the harness, `GF_TRACE_OUT` for the translated binary). Compares PCs, then the pre-instruction state (gpr, xmm); reports the first control or state divergence and its producer.
- `tools/dev/diffharness/snapcase.py`: records the state at each executed PES instruction (import-model and stub entries excluded).
- `import_stubs.json`: `code_hex` field and four new models (`srw_acquire_shared`, `srw_release_shared`, `fls_get`, `setlasterror`), each with its Wine evidence.

## 8. Open items

1. seq008 blocked (§4): find the transfer into the import page at `0x6ffffbc71f26`.
2. seq025 `141F5F330` case not re-run.
3. `driver --check` pilot rc 1: diagnose (no traceback printed).
4. `core` and `impure` checks vacuous (0 functions): fix the spec-dir selection, or name the right sets.
5. Type-13 emulated advance (§5) and write-hooks for the ball and S4: not started.
6. Hook specs from captures (item 1): not generated.
7. `tools/dev/rendezvous/rendezvous.lua`: merge conflict resolved as RvReplay's copy; Main to decide.
8. Held-out functions `0x1403BB670`, `0x141F5D480` (pre-existing): their closures reach mistranslated callees (`0x14037CB50`, `0x141F5B810`); not investigated.


## Main's note

The merge conflict in `tools/dev/rendezvous/rendezvous.lua` was resolved toward the branch's pre-fix copy (`require('ffi')`, an unpinned local handle, 5 s timeout), the version that failed to load in sider and then crashed the game by unloading the DLL. Main restored main's module on the branch.
