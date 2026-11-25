# GateFix1: diff_impure_recorded regression 0x1404405D0 (M6 harness fix)

Scenario: M6 (harness/tooling fix). Branch `GateFix`, tip `4bafe6c46`, pushed; `git ls-remote origin GateFix` = 4bafe6c4640086d26883c0276ae6b288bcf5dbdf. Base `origin/Integrate` e154ed687.

## Symptom
Gate 25-11 ~06:55 (`Integrate/build/Testing/Temporary/LastTest.log`, line 3091):
`driver --check: 1 recorded-diffed functions no longer diff: 0x1404405D0`, with
`0x1404405D0 translated=True diffed=False ... class=translator bug callee 0x14183820B: no instructions`.
The other 34 impure records passed in that run.

## Root cause (race CONFIRMED; the failing run itself was NOT reproduced)
`xlate.pdata_of` returned one process-wide `Pdata` per image (`_PDATA = {}`). The driver runs functions on a
`ThreadPoolExecutor` (`--jobs`, default 2). Every worker shared that `Pdata`, whose state is not thread-safe:

- `Pdata.read` (pdata.py:116) is `self.img.seek(va - base); self.img.read(n)` on one file object. A thread that switches
  between the seek and the read returns another address's bytes.
- the capstone handle `Pdata._md` and the memo dicts `_extent`, `_fall`, `_head_of` are shared mutable state.

Evidence:
- Probe `<workdrive>/gf-GateFix-scratch/read_race.py`: 200,000 concurrent 16-byte reads on the PES image, 4 threads,
  shared instance: **1,140 returned the wrong bytes**. Compared with `os.pread`.
- Red test `test_pdata_threads.py` on the unfixed code: **161 of 40,000** concurrent reads wrong (FAILED).
- The failing gate run's gen directory (`Integrate/build/impure-check/gen/1404405D0`) holds 167 callee TUs; a passing
  run (`<workdrive>/gf-GateFix-scratch/impure-full/gen/1404405D0`) holds 170. The failing run stops at callee
  0x14183820B and lacks three later callees (14043DD70, 141E5CA20, 141E5DE80). The reached trace is `{}` in both runs,
  so 0x14183820B came from the decode walk (`runnable_extra` / `tail_rows`), not from the harness. No generated file
  mentions that VA. INF on the exact path; CONFIRMED that a racy decode can produce such a target.
- 0x14183820B is the middle of an instruction, CONFIRMED by bytes: 0x14183820A = `4c 8d 45 10` (`lea r8,[rbp+0x10]`),
  0x14183820B = `8d 45 10 ...` (`lea eax,[rbp+0x10]`). No `.pdata` start is there.
- Single-VA rerun and two full reruns at e154ed687 (unfixed) passed: the failure is intermittent, which fits a race.

## Commit that introduced the exposure
- Shared `Pdata` via the `_PDATA` cache: `4ab3bbaf8` (2026-10-29); on origin/main and Integrate.
- Thread pool running functions concurrently: `55291d5b6` "Gate speed: ... diff jobs knob" (2026-11-02); on origin/main.
- Integrate adds more decoding per function (`f49996907` runs `runnable_extra` again on reached roots; the
  `1735ddaae` record merge and `9fb64855` input order are not on the check path). INF: this raises the odds of a
  race hit. Not bisected.
- origin/main pass/fail on 0x1404405D0: NOT VERIFIED. The same code is on main, and one run cannot settle an
  intermittent race, so a bisect by run outcome would prove nothing.

## Fix (commit 4bafe6c46)
`tools/dev/translate/xlate.py`: `pdata_of` caches one `Pdata` per thread (`threading.local`), so each worker has its
own file position, capstone handle and memos. No `Pdata` is shared across threads any more.
`tools/dev/translate/test_pdata_threads.py` (new): 40,000 concurrent 16-byte reads through `pdata_of` on a 64 MB
synthetic PE image (test_extent's builder plus padding), each compared with `os.pread`; must have 0 wrong reads.
Registered as ctest `translate_pdata_threads` in `tests/pilot/CMakeLists.txt`.

Mutation: the test on the unfixed shared `pdata_of` FAILS (161 wrong reads); on the fix it PASSES (run:
`python3 tools/dev/translate/test_pdata_threads.py`, 1 test OK, 3.5 s).

## Verification
- `driver.py --check` on `results_impure.json`, full 35 recorded-diffed functions, jobs 2, PES_EXE=pes_full.bin,
  banks_union9l, with the fix: **all 35 still diff bit-exact** (`impure-fix.log`, 470 s).
- Single-VA check on 0x1404405D0 at e154ed687 before the fix: passes (`repro1.log`).
- `diff_core_recorded` selection: `select_recorded.py --base e154ed687` selects **0 of 2,507** (plus 0/245 pilot, 0/687
  cutscene, 0/546 presentation, 0/35 impure). xlate.py is not GLOBAL and no generated file changed. The check prints
  "nothing to re-check" and rc 0.
- `gen_translated.py --check` (diff_translated_drift): **11191 functions (4161 recorded bit-exact, 7030 closure)
  regenerate byte-identical, rc=0** (`gen-check.log`, 2166 s).
- PII: changed files grepped for `<workdrive>` and `<home>`: no matches. (`tools/check_pii.sh` not run: it fails inside a
  worktree because of the symlink.)
- No record was written (check runs never write records). No committed result changed status: the record already
  holds 0x1404405D0 as diffed, so nothing moved fail->pass in a record file.
- Test binaries run: `test_pdata_threads.py` (1 test, OK; red on unfixed). Full `GATE_TIER=fast` not run (Main gates).

## Not done / open
1. origin/main pass/fail on 0x1404405D0: not verified (see above).
2. The gate's intermittent failing run was not reproduced; the race and the byte evidence are shown.
3. Startup cost: one `.pdata` parse per worker thread (2 with the default `--jobs`).
4. Not changed, out of scope: `tools/dev/diffharness/specgen/classify.py` keeps its own process-wide `_PDATA` (keyed by
   `id(AN.fh)`); used by the specgen tools only, not by the thread-pool driver.
5. Scratch kept in `<workdrive>/gf-GateFix-scratch` (`impure-full`, `impure-fix`, probes, logs); delete when Main archives this report.
