# SnapDiff: traced calls diffed on the snapshot's own memory (26-10-26)

Branch `SnapDiff`. Commits: `c8de205d` (harness, diff main, orchestrator), and the return-slot counter commit that follows it. Scratch (traces, snapshot copies, build trees) is PES-derived and not committed; it lives in the SnapDiff scratch directory outside the repo.

## 0. Result in one paragraph

The snapshot-backed path works end to end on one function. `14057A3E0` replays 67 of 67 traced calls on snapshot 1 (`snap1-142012`) from the snapshot's own pages. The translated function over the same pages matches PES on every replayed call: rax, full xmm0, and every written byte (0 mismatches). Own-block coverage on that snapshot is 4 of 14 blocks (0.29), below the 0.50 floor. The full seven-snapshot run is **not done**: snapshots 2-7 are still being traced (the first trace chain stopped after snapshot 1; relaunched as `run_rest.sh`). No floor count is established yet. The team tick `1404F6C30` is **not unblocked**; its exact blocker is in §4.

## 1. What is built

- **Entry state** (`frame_trace.py`, `entry_record`): every impure call's 16 GPRs, 16 XMM registers and return address, deduplicated per function, written to `calls/<VA>.bin` (392 bytes a call).
- **Harness** (`diffharness/snapcase.py`, `run.py --snapshot-dir ... --calls ... --out-dir ...`): one unicorn engine reused for the run; between calls every mapped page is unmapped and copied again from `mem.bin` on first touch, so every call starts from pristine snapshot memory. It records rax, full xmm0 and every written byte (address and value, in order). Calls that leave the model are excluded with a reason: unmapped or hole access, abort trap, OS import model, entry into an untranslated (blocked) callee, budget overrun, or a return that does not reach its caller.
- **Translated side** (`src/pes/translated/pes_state.hpp`, `diffmain.gen_snap`): `SnapView` copies pages on first touch in 256 KiB blocks and drops them per call; `PesMem` gets a per-byte write log. Both are inert outside snapshot mode.
- **Orchestrator** (`translate/snapdiff.py`): builds the closure (direct callees, runnable extras, harness-reached indirect targets), translates it, compiles the diff main, runs it on the replayable calls per snapshot, compares, and computes own-block coverage over the replayed calls (`driver.block_coverage`). Floor: no mismatches, at least one replayed call, own-block fraction at least 0.50.

## 2. Measurements

### 2a. `14057A3E0`, snapshot 1 (CONFIRMED, this run)

| Replayed | Mismatches (rax, xmm0, writes) | Own-block | Floor | Return-slot reads |
|---|---|---|---|---|
| 67 of 67 | 0 | 4 / 14 = 0.29 | no | 0 on all 67 |

### 2b. Snapshot 1, all reached functions (PARTIAL, `res_snap1.log`)

Replayable calls in the first functions (of 23 reached): `1404FA720` 2 of 2 (0 mismatches, own-block 0.09), `140509A40` 3 of 3 (0 mismatches, 0.03), `14057A3E0` 67 of 67. The rest replay 0 calls on this snapshot. Exclusion reasons, 92 excluded on the first 13 functions:

| Reason | Calls |
|---|---|
| unmapped or hole read at a small or garbage address (`0x44`, `0x255`, `0x811c3a2943129`) | 33 |
| entry into an untranslated callee (`140A0C3E0`, `140A62690`) | 5 |
| unmapped write at `0x0` | 2 |

The unmapped-pointer exclusions are the main limit of this design. A traced call's registers point into memory the emulated frame built, and the pristine snapshot does not hold that memory at the same address. Such calls cannot be replayed from the snapshot as the brief specifies; they are excluded, not diffed.

### 2c. Floor count

Not established. It needs all seven snapshots (§5).

## 3. Deviations and caveats (CONFIRMED unless tagged)

1. **CALL return-address pushes are normalised.** The translator's `CALL` writes 0 where PES writes the return address (`xlate.py` emits `s.wr(s.gpr[4], 0ULL, 8)`), and a stubbed call pushes nothing in the translation. The harness normalises the same way on its side: a stubbed call's push is dropped, any other call's push is recorded as 0. Count of normalised pushes per case is in `uc._call_pushes` (not yet in the corpus). Fixing it means `emit_branch` carrying the next address, which changes committed generated code; not done here (gate drift).
2. **The one case the normalisation could hide** (Main's flag): a function reading its own return address from the stack. `snapcase` counts every read of a normalised slot that is not the callee's own `ret` (`retread`, in each corpus row). `14057A3E0`: 0 on all 67 calls. Other functions not yet checked.
3. **Pristine snapshot memory, not call-time memory.** A call later in a frame reads state the frame wrote before the call; the snapshot does not hold it. Both sides start from the same pristine pages, so a replayed call is a valid differential test on those inputs, but many calls cannot replay (§2b).
4. **Stubs are modelled, not untranslated.** `stubs.json` stubs (`14159F3E0` noop, `sqrt`) are removed from the blocked set before the harness; they remain the translator's models.
5. **Import models excluded.** A call that runs an OS import model is excluded; the translator has no import call.

## 4. Team tick `1404F6C30` (item 5): NOT unblocked

- The trace's blocker text (`140537AC0` calls CRT slot `14252F6E8` and writes `140537C4E`) does **not** match the image. `140537AC0` is a cookie-checked function; its calls are `140537BF0` (an accessor), `140A48A10`, `140A429A0` and the cookie stub `14159F3E0`. It has no call through `14252F6E8` and no write to `140537C4E`.
- What the image does show: `140537C4E` is the first byte of `mov dword ptr [rip+...], 0FFh` inside the static-init path at `140537C03`. That path is taken only when the dword at `[rip+...]` equals -1, then calls a thunk (`14159F850`) that leads to the CRT slot. The function at `140537C03` has no direct `E8` callers in `.text`; it is reached indirectly.
- Replays of the team tick's three traced calls on snapshot 1 are excluded before they reach that path: their registers point at `0x40` and `0xB4` (unmapped in the pristine snapshot), and one run enters the untranslated callee `140A0C3E0`. So "unreached" is **not proven**, and the tick cannot be diffed.
- Named blocker: the tick's recorded argument objects are not in the pristine snapshot (§3.3), plus untranslated callee `140A0C3E0`.

## 5. Not done (numbered)

1. Traces for snapshots 2-7 (`run_rest.sh`, log `runs_rest.log`). Then run `snapdiff.py` with all seven `--snapshot SNAPDIR=TRACEDIR` pairs and the impure spec dir: `python3 tools/dev/translate/snapdiff.py --image $PES_EXE --spec-dir tools/dev/diffharness/specs/impure --out-dir <scratch>/build --snapshot ... --results <scratch>/res_all.json`. Commit `results_impure.json` from that run only.
2. Exclusion of blocked callees and pristine-memory exclusions are the floor limits; widen by replaying with call-time memory (a different harness design), not by relaxing the diff.
3. Team tick: prove or model `140537C03`'s init path once a replayable call exists; name `140A0C3E0` as the blocker until then.
4. Translator: write the real return address on CALL (removes §3.1); coordinate with the gate because committed generated code would drift.
5. Gate and `driver.py --check` not run (Main gates). Diff suite not run.
