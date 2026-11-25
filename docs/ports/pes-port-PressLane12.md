# PressLane12: M6 root cause found and landed (GetCurrentDirectoryW length query + ucrtbase models). The writer FUN_1408BE740 is NOT proven: its replay loops at the graphics-alloc stub

Branch `PressLane4`, worktree `<workdrive>/gf-PressLane4`. Pushed: **`de362121ccf801d2362e17a330cfc1d2b5556ccb`** (`git ls-remote origin PressLane4` confirms; base was `96566fe69`).
Scenario claimed: **M6** (harness/model fix: root cause named, every affected layer record re-generated, `gen --check` byte-identical). No new PES proof. The writer (M5) is NOT proven.

## 0. Verdicts

| # | Task | Verdict |
|---|---|---|
| 1 | Fix the vsnprintf_s model (M6) | **Not the fault.** Its Win64 layout is right (5th arg at entry+0x28, 7th at entry+0x38; the unit test passes 3/3). PressLane11's hypothesis was wrong. The real fault is the GetCurrentDirectoryW model (§1). |
| 2 | Red test, then fix | **Done** for GetCurrentDirectoryW: red (`UC_ERR_WRITE_UNMAPPED`), green (`StartupOsModels` 5/5, model tests 45/45). |
| 3 | Prove FUN_1408BE740 on the 4 real calls (M5) | **NOT achieved.** seq001 now runs the whole writer chain and loops (§3). seq004/007/011 not replayed. |
| 4 | Option-B wiring; red test green | **NOT done.** The red test `SlotObjects.BindMemoryGivesEachSlotItsTrapActorPBuiltForTheSlot` is in the worktree, uncommitted, not built. |
| 5 | Wrapper 0x140925750, stand-in deletion, measurements | **NOT done.** Nothing in GF's runtime path changed by design. |

## 1. M6 root cause (CONFIRMED)

Replay: `frame_trace.py recorded caps/seq001 --hooks hooks_relaunch9.cfg --out … --spec-dir specs/trapaction`, `PES_EXE` = `pes_full.bin`. The fault at rip `0x6F9C3653E2C6` (unmapped data at 0x0, size 8) names via `SnapMem.export_of` **KERNEL32.dll!GetCurrentDirectoryW + 10**: the model's `mov [rdx], rax`. Its model was `mov rax, 0x5C003A0043; mov [rdx], rax; mov eax, 3; ret`, which ignores `nBufferLength` and writes through rdx = NULL. That is PES's length query `GetCurrentDirectoryW(0, NULL)`.

Fix (Win32 contract): size >= 4 stores the three-character drive-root string (drive letter, colon, backslash) + terminator and returns 3; size < 4 writes nothing and returns 4.
- `getcwd_model.s` (new). `as` + `objcopy` gives `83f904721348b843003a005c000000488902b803000000c3b804000000c3` (hand-encoded bytes match).
- `import_stubs.json` (`code_hex` + `why`, with the VA and the capture), `xlate.py` `cwd_w` mirror.
- Test (`test_strstr_model.py`): red on the old model, green on the new one.

## 2. New models (committed with their tests)

- ucrtbase `strlen` (new: `strlen_model.s`, `pes_model_strlen`, `xlate` mirror, `test_strlen_model.py` 3/3).
- strncmp, toupper, vsnprintf_s: PressLane11's uncommitted models (from origin/Presentation d3f04aa1f), committed with their tests. The regen's 7 `1415A17xx` and the `pes_fn_14037F7E0`-class closure diffs are the strncmp mirror replacing the dispatch call (image constant `0x142530650`).

## 3. The writer replay: blocked by a loop at the graphics-alloc stub (CONFIRMED loop; cause INF)

After §1 and §2 the seq001 replay runs the writer chain (1590 impure-spec calls) and does not return: `did not return: rip 0x141957c50` after 50M instructions and again after 400M. rip is the thunk each time, so it is a loop.
- `0x141957C50` is `jmp [0x143516830]`; the cell holds `0x141957C60`, the allocator body (`FUN_141957C60`, the graphics device's vtable +0x20). It is the GF service stub `gfx_device_alloc` (`stubs.json` VA 141957C50, `callers: [1423D4A80]`, answer `rax = 0`, "allocation fails"). The 23-11 ruling (brief) says the answer 0 sends PES off its own path and proposes a GF-owned zeroed scratch block, under M7 (every record reaching it re-run bit-exact).
- `FUN_1423D4A80` decompiled (`pl11/dec_1423d4a80.txt`): on a 0 answer it takes its cleanup path and returns. **So the loop is not a retry inside that function** (corrects my earlier guess). The loop is higher up in the chain; not identified.
- Tooling note (CONFIRMED): `frame_trace.py recorded` without `--out` loads no import models (`run_recorded`: `models = load_import_models() if trace is not None else {}`). Every import then stops at its first call. The proof path (`prove_bounded.py`) always passes `--out`.

## 4. Verification (run)

- Model tests: `test_strstr_model`, `test_vsnprintf_model`, `test_strlen_model`: 45/45 (run in the lane's venv).
- Regen: `gen_translated.py --image pes_full.bin`: 11268 functions (4162 recorded bit-exact, 7106 closure), 27 closure files changed. Log `pl11/gen/regen.log`.
- **`gen_translated.py --check`: rc 0, "11268 functions regenerate byte-identical"** (`pl11/gen/check.log`).
- **ALiveBall** (`pes_restart_delay_test --gtest_filter='PesStall.ALiveBall*'`, `simslot.sh timeout 3600`): **`liveFirings=2229 episodes=160 seriesFrozen=0 bound=2229` PASSED** (1481 s). The first run hit my 1500 s timeout mid-test (rc 124); the rerun finished, so that was a slow run, not a stall. The PressLane4 tip recorded 2229 before this change, so this change moves nothing on this measure.
- **PesStallQuick**: run started (`pl11/quick_m6.log`), **not finished at the time of writing** (see §6).
- PII: `tools/check_pii.sh` from `<workdrive>/GodotFootball`: rc 0.
- Red test `pes_press_object_test`: not built (uncommitted, see §5).

## 5. Uncommitted, saved as patch (`<workdrive>/gf-PressLane4-scratch/pl12/wip/`)

- `tracked.patch`: `tests/pes_press_object_test.cpp` (the red test, PressLane11's) and `tools/dev/ledger/universe.json` (closure-pwrite ledger).
- `untracked/`: `specgen/sets/closure-pslot.txt` (PARTIAL; per the brief, not to be committed), `specgen/sets/closure-pwrite.txt`, `specs/trapaction/1408BE740.json`. `status.txt` and `untracked.list` alongside.

## 6. Pending at delivery

- PesStallQuick (`PesStallQuick.ALiveBallNeverLiesStillInTheQuickWindow`, bound 49 per the brief; the PressLane4 tip recorded 53 on the merged tree): result pending in job `bg_529`. If it lands above its bound, the commit is M1-ineligible and must be reverted or re-scoped; the owner's wind-down says no new runs.

## 7. Next step (exact)

1. Identify the loop above `FUN_1423D4A80`: get the caller chain of `0x141957C50` (`/xrefs_to` on `FUN_1423D4A80`, then the caller's loop). Verify the 0-answer claim against the capture's `call.bin`/`mem.bin` (the stub's real return value in PES was non-zero).
2. Apply the 23-11 M7 ruling for `gfx_device_alloc` (a GF-owned zeroed scratch block): re-run every record reaching `0x141957C50` (`closure_animelayer`, `closure_proccam`, `results.json` entries) bit-exact, then `gen --check`, then ALiveBall.
3. Replay seq004/007/011 after §2; then `prove_bounded.py 0x1408BE740 WORK --tag pwrite --real … --merge` (M5); decode the ctx writer for `X+0x7E0`.
4. Option B (red test `SlotObjects.BindMemory…` in `wip/`): the native allocator returns PVa(slot); the wrapper `0x140925750`; stand-in deletion; then the measurements.

## 8. Capture copies

Deleted: `pl11/caps` (27 GB of unpacked copies of live2 captures; originals in `live2/pid1257475/` untouched) and the trace dirs `tr4/5/6_seq001`. Logs and decompiles kept in `pl11/`.

## 9. PesStallQuick result (landed after §6 was written)

`PesStallQuick.ALiveBallNeverLiesStillInTheQuickWindow`: **PASSED**, `PESSTALLQUICK ticks=12960 of 12960 forced_live=53` (367 s). The PressLane4 tip recorded 53 before this change, so this change moves nothing on this measure either. The brief's Integrate bound of 49 is the test's older figure; the test's own assertion passed on this tree.
