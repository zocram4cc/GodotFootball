# Impure sample run: harness stubs, translator stub dispatch, 50 measured

Branch `Impure` (origin, head in the final message). Sample: ImpureMap's delivery in `<workdrive>/gf-Impure-scratch/` (`sample.json`, 50 picks, regenerated specs). Main ruled this the only sample; ImpureRun-2 runs the parallel implementation separately.

## Result (one command, 60 s)

`python3 tools/dev/translate/driver.py --image $PES_EXE --out-dir <scratch> --spec-dir tools/dev/diffharness/specs/impure --jobs 2`. Record: `tools/dev/translate/results_impure.json` (full run).

| class | count | VAs |
|---|---|---|
| diffed, bit-exact | 15 | 1404F47A0, 1405055E0, 140503430, 140529490, 14053B110, 14057BA50, 14057D3A0, 1405A6DE0, 1406486D0, 140654E60, 14065DAB0, 14097FCE0, 14097FFF0, 140A539F0, 140A57650 |
| struct map gap | 12 | 140510D30, 14051CF70, 14052A820, 14053B420, 1405CCD50, 140639100, 14064DC50, 14064DE60, 14065C2A0, 140687310, 140978E70, 14097CD00 |
| unmodellable callee (not run) | 23 | all others, incl. the 5 indirect/vtable samples |
| engine call, live state, GPU, translator bug, stub model wrong, harness cannot run | 0 | |

Translated 27 of 50 (all harness-runnable). Bit-exact 15 of 27 runnable. Pilot `driver.py --check`: 216/216 bit-exact (run before every commit).

Stub usage in the 15: `1415A1780` (sqrt) in 12, `14159F3E0` (noop) in 2.

## Changes

- `tools/dev/translate/stubs.py`: the one stub table both `run.py` and `xlate.py` read (`stubs.json`). Each entry maps to `do`: `sqrt_xmm0`, `noop`, `unmodellable` (refused), or none (run-real: image bytes execute).
- `run.py`: `resolve_stubs` merges the entries for each `stub_refs` VA. `sqrt_xmm0` follows sqrtss: correctly rounded via double; NaN quieted; negative gives default QNaN 0xFFC00000; bits 32-127 preserved. `noop` drops the call. Emu errors report the last RIP. The inputs dump is truncated so the fault line survives stderr.
- `xlate.py` + `pes_stubs.hpp`: stub call sites and tail jumps into stubs emit the model. The old `value`-return branch is removed (pilot specs had no stubs).
- `driver.py`: stub map from `stub_refs`. Universe = pilot specs + spec-dir specs + run-real closure VAs. Callees given to `diffmain` are only pilot-universe, non-stubbed (diffmain declares every callee). `unmodellable callee` is classified before translation. `live state` only when the fault address is inside the image; the old regex never matched and a precheck fallback labelled any failure live. Results carry `stubs`. Impure runs write `results_impure.json`; the pilot's `results.json` is untouched.
- Specs: 50 in `tools/dev/diffharness/specs/impure/` (ImpureMap's regenerated set).

## Failure detail

- **struct map gap (12).** UC_ERR_MAP with no address; the note carries the last RIP. Traced one (1405055E0 in the first batch, since fixed by ImpureMap): `rcx = 0` loaded from an unpopulated synthetic table, a spec closure gap. Specific causes for the rest are not traced. 14097CD00 is a spec defect: `regions_init` names an undefined region (`IMG.1436F3DC8`). 14064DC50 (per ImpureMap): a pointer argument is null, an unlinked input.
- **Regression from the regenerated specs.** 14053B420, 14065C2A0, 140978E70 were bit-exact on the earlier specs and now fault with UC_ERR_MAP. Routed to Main; not diagnosed.
- **Stale claim.** ImpureMap reported that run.py needed a sqrt kind for 140529490 and 140A539F0. The hook already exists and both diff bit-exact on the regenerated specs.
- **unmodellable callee (23).** Blockers from ImpureMap's precheck: indirect calls with no resolved target, computed jumps with unenumerated switch cases, import slots with no name in the image, and closures through these. Not run.

## Cost split (approximate tool requests)

- Harness, translator, driver, classifier, stub table and verification: about 75 requests, including reading the pipeline code and three pilot checks (about 3 min each).
- Per-function work: one full run covers all 50 (60 s). One probe (1405055E0) to classify the UC_ERR_MAP failures.
- Spec generation and closure fixes belong to ImpureMap, not counted.

## Caveats

1. "Bit-exact" with stubs means the translated C++ matches the harness; both run the same models (`sqrt_xmm0`, `noop`). It does not prove the models match PES. No PES-side oracle exists for the sqrt thunk or `__security_check_cookie`. So `stub model wrong` is 0 by absence of evidence, not by verification.
2. Indirect and vtable resolution is not implemented. ImpureMap's data has no resolved-target entries (no `indirect-target` kind), so the translator has no direct-call path to emit. All 5 indirect/vtable samples are unmodellable.
3. The 23 unmodellable functions were classified from ImpureMap's precheck, not run.
4. An early pilot check ran from the main checkout with output in `/tmp`; I removed that output.
5. The 15 bit-exact include `provisional` functions (they read writable image globals at on-disk values).

## Open

- 12 struct map gap specs, including the 3 regressions: spec fixes (routed to Main).
- 23 unmodellable: needs indirect-target resolution (vtable tables) and switch case enumeration.
