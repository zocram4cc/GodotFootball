# CallDelta: call-time memory for snapshot replay (26-10-26)

Branch `CallDelta` (worktree `<workdrive>/gf-CallDelta`, based on `origin/SnapDiff` `d39420dd`), pushed as `f8f16a18`; `git ls-remote origin CallDelta` confirms it. Scratch (traces, pools, builds) is in `<workdrive>/gf-CallDelta-scratch`, outside the repo and not committed.

## 1. Result

- Replays now start from the call-time memory, not the pristine snapshot. Snapshot 1 went from 74 replayable calls of 133 to 5,950 of 6,450. Over all seven snapshots (50 functions), 27,948 of 29,298 calls replay.
- Not bit-exact yet. Floor over seven snapshots: **9 of 50** functions. Two functions still mismatch (500 replays): `1405CE930` (300) and `1404405D0` (200). Both are OPEN (section 4).
- The team tick `1404F6C30` is still not replayed: 0 of 700 calls (section 5).

## 2. What was built (CONFIRMED)

- `tools/dev/translate/frame_trace.py`, class `CallDelta`. At every impure entry it records the delta: every page the emulated run wrote and that differs from its snapshot baseline, plus the stack (`0x10000000`, 32 pages) and the TEB page in full. Writes come from a unicorn write hook and from a patched `Uc.mem_write` (stub models and LazyMap). Import-model pages are excluded, because a replay maps them on fetch with their call counters.
- Storage, in the trace dir: `pool/pages.bin` holds each distinct 4 KiB page once; `deltas.jsonl` holds each distinct delta once as `[va, pool index]` pairs; `deltas.idx.jsonl` maps delta id to byte offset. `calls/<VA>.delta` gives each record's delta id. Records are deduplicated on (registers, delta).
- Replay, `tools/dev/diffharness/snapcase.py`: each call unmaps everything, maps its delta pages eagerly, and sets GS base to the TEB. It writes `ok_delta.bin` (per replayable call: u64 count, then u64 VA and u64 pool byte offset per page).
- Translated side, `src/pes/translated/pes_state.hpp`: `SnapView::over` (mutable, 4 KiB pages, sorted), checked before the snapshot runs in `at()`. `diffmain.py` reads `ok_delta.bin` and `pool/pages.bin` (argv 5, 6) and sets `over` per call. `snapdiff.py` passes both.
- Measured trace cost: snapshot 1 with deltas took 205 s (before: 185 s). Pool for snapshot 1: 1,139 pages (4.5 MB); 6,450 distinct (registers, delta) records; `deltas.jsonl` 6.9 MB.

## 3. Numbers

| Run | Calls | Replayable | Mismatches | Floor |
|---|---|---|---|---|
| Snapshot 1, pristine (before, origin/SnapDiff, `res_snap1.json`) | 133 | 74 (4 functions) | 0 | 0 of 50 |
| Snapshot 1, call-time deltas, before xmm fixes (`res_snap1_delta.json`) | 6,450 | 5,950 | 350 (4 functions, xmm0) | 2 of 50 |
| Seven snapshots, final (`res_final.json`, = `results_snapdiff.json`) | 29,298 | 27,948 | 500 (2 functions) | 9 of 50 |

Seven-snapshot floor set: 1404FB2A0, 1404FEDC0, 1405055E0, 14051CF70, 140529490, 14052A820, 14057BA50, 1405DE950, 140687310. Exclusion reasons in the final run: "unmodellable callee 0x1404F6720 entered" (700) and "unmodellable callee 0x140A79640 entered" (650). Per-function table: `tools/dev/translate/results_snapdiff.json`.

## 4. Mismatches (every one, by case)

- **1405CE930**, 300 replays (cases 0-99 on snap1-142012, snap2-142339, snap3-142654). Fields: rax, w, xmm0 all differ from case 0 on. Example, snap1 case 0: harness rax 0x2, translated rax 0x3; harness xmm0 low 0x422b7192, translated 0xc2359bb4. A control-flow or value divergence, not an upper-lane effect. OPEN: not yet traced to an instruction.
- **1404405D0**, 200 replays (cases 0-49 on snap1-142012, snap3-142654, snap1-162213, snap2-162511). Field: xmm0 upper lanes. Example, snap1 case 0: harness xmm0 `0x...c127568f` with upper lanes zero; translated upper lanes `0x75bc`, `0x7585` nonzero (they match the entry-state upper bits). The function's `mulss`/`addss` chain preserves upper lanes, which is correct, so some earlier op zeroes them on PES's side. The MOVD fix below did not clear it. OPEN: the zeroing op is not yet identified.

Fixed this run (CONFIRMED by replay): `14051CF70`, `140529490`, `14062E240` had xmm0 upper-lane mismatches from `MOVSS xmm, m32`; `xlate.py` kept the upper 96 bits. Now zeroed, per SDM. `14062E240` (50 mismatches before) now 0. The same translator also zeroes bits 32-127 for `MOVD xmm, r/m32` now (`xlate.py` MOVD branch); no measured change yet beyond the 1404405D0 result above.

## 5. Team tick `1404F6C30` (CONFIRMED status: not replayed)

- 700 traced calls over the seven snapshots, 0 replayable. All 700 excluded by "unmodellable callee 0x1404F6720 entered".
- `140A0C3E0` is now in the closure. Its earlier block, "closure larger than 60", is lifted by raising `EXTRA_CAP` (below). The closure at this cap is 981 functions.
- `1404F6720`: blocked by the cap of 1000 ("closure larger than 1000"). Its true closure size is not measured (a cap-5000 measurement did not apply; the edit was rejected).
- `140A79640`: blocked as unmodellable, with indirect calls at `141E535F9` and `141E5362F`. Not dispatchable in this build.
- Named blocker: `1404F6720` (closure over 1000) and `140A79640` (unmodellable). Clearing 1404F6720 needs a larger cap or a closure fix; that is a policy decision, not done here.

## 6. Closure gaps (`140A6CF70`, `1408C6430`)

The four builds that failed to link (`1404386D0`, `1404FE9C0`, `1404FEDC0`, `14053B110`) now link and replay. This came from the raised cap, not hand-added closure entries: `140A6CF70` enters the closure for `1404386D0` and `1404FE9C0`, and `1408C6430` is in the closure for `1404FEDC0`. I did not add them by hand.

## 7. Deviations and risks

1. `results_impure.json` was **not** overwritten. `gen_translated.py` reads its `diffed` and `failure_class` fields, and the snapdiff schema lacks them, so overwriting would mark every impure function undiffed and drift committed code. The seven-snapshot record is `tools/dev/translate/results_snapdiff.json`, mismatch values omitted (PES-derived).
2. **Committed translations drift.** The MOVSS and MOVD fixes change generated code for every committed function containing those forms (`src/pes/translated` hits ~189 `set_x_f32` lines, not all MOVSS memory form). `EXTRA_CAP` changes impure closure sets. Needs `gen_translated.py` regeneration and the gate; Main to run.
3. `EXTRA_CAP` is now a module constant at `driver.py:220`, value 1000 (was a local 60). Build size for roots reaching large closures grows; the team tick closure is 981 functions.
4. Replays of stack and TEB now start zeroed, as in the trace. Before, the replay read pristine snapshot bytes there. This is the source of the delta-mode change, and it is what the trace actually saw.
5. Traces run with `ulimit -v 2000000`. Unicorn fits. No gate or tests run; Main gates.

## 8. Checks run

- py_compile on `frame_trace.py`, `snapcase.py`, `snapdiff.py`, `diffmain.py`, `xlate.py`.
- Snapshot-1 delta trace (50 frames, 205 s). Seven delta traces, all rc=0 (the stop-snap1 trace was run by the first batch; the batch died after it, and stop-snap2..4 were rerun detached).
- snapdiff, snapshot 1 (50 functions), then the nine-function fix check (`res_fix1.json`: 9 functions, floor 3), then the full seven-snapshot diff twice (`res_all_fix.json` before MOVD, `res_final.json` after).
- Mutation checks: none run.
- PII: no machine paths or machine paths strings in changed files (path-limited grep). `tools/check_pii.sh` not run from the main checkout.

## 9. Next

1. Trace `1405CE930`'s first divergence (case 0, rax 2 vs 3), the highest-yield open mismatch: it fails on every case.
2. Find the upper-lane zeroing op in `1404405D0` (every xmm0 writer, including `CVT*`, `PXOR` and `MOVQ` variants).
3. Decide the cap policy for `1404F6720` (a closure-size measurement is needed first); this is what gates the team tick.
4. Main: regenerate committed translations and run the gate, since the xlate and driver changes drift committed code.


## Main's reading

Real calls replayed against the memory they saw (27,948 of 29,298 over seven snapshots) exposed two translator bugs
that every synthetic-case set had missed: `MOVSS xmm, m32` kept the upper lanes (the SDM zeroes them) and `MOVD xmm,
r/m32` did not zero-extend. Both are common instructions, so the fix must be re-checked against every recorded set and
the committed layer regenerated: that is the next batch. The real-call floor is 9 of 50 (bit-exact on every replayed
call and at least 50% own-block coverage over the seven snapshots); two functions still mismatch on real calls.
