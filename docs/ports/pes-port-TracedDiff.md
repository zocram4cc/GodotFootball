# TracedDiff: traced real calls as the bit-exact floor (26-10-26)

Branch `TracedDiff` (worktree `<workdrive>/gf-TracedDiff`, from `origin/TracedCases` e7b7428). Commits: **91c5e42b** (tooling), **7c3074f0** (results). `git ls-remote origin TracedDiff` confirms 7c3074f0. Scratch (banks, snapshots' runs, builds) is PES-derived and not committed: `<workdrive>/gf-TracedDiff-scratch/`.

## 0. Result in one paragraph

Replaying every traced call exactly (its own instance, its real inputs, no perturbation) gives **0 functions at the floor**. Four functions replay fully and are bit-exact (11,951 cases, 0 mismatches), but all four sit below the 0.50 own-block floor. The earlier floor of 15 (TracedCases union) cannot be reproduced under exact replay: those cases drew random values for every input an instance did not hold. The team tick `1404F6C30` is still refused. The brief's "traced calls become the floor" holds only for these four, and only as bit-exact with low coverage.

## 1. What changed

- **Traced-call mode** (`run.py --traced-cases`, `driver.py --traced-cases`). Each bank instance is one case, replayed with its real values. No perturbation, no random or guided extras. A case is **excluded, not written** when it: (a) does not hold a spec input (`unheld input`), or (b) reads memory outside the spec's regions (`unmodelled read`). Excluded cases are not diffed.
- **Entry scalars** (`frame_trace.py`, `run.py` `held_value`). Register and stack inputs are recorded per instance at call entry (artifact key `scalars`). Before this, every `scalar_rcx/rdx/r8` and `stk0` input was unheld. u64 fields are now real values in bank mode too (previously always a draw).
- **Keep-all banks** (`frame_trace --cap 0`). Every distinct traced call, deduplicated by sha256 of its instance. Replaces the 32-instance reservoir.
- **Abort traps** (`stubs.json`, `driver._cut_at_abort`, `run.py`). Every `abort-trap` stub is installed in every spec. A case reaching one is recorded in `--aborts` and excluded, never an output. A blocker chain is cut at its first abort-trap callee, so the links below a trap do not block the function.
- **Fast-fail reporter `14159FCCC` added as an abort-trap** (`stubs.json`, see §3 deviation).
- **Stable merge** (`bankstore merge`). Deduplicated by digest, written in digest order, root kinds take the smaller on conflict. The merged bank is byte-identical for two source orders (`diff -rq` rc=0 for traced and union banks).
- **Diff main prints every mismatching field** (`diffmain.gen`, the first-ten cap removed).

## 2. Measurements

### 2a. Reach per snapshot (frame trace, keep-all, 50 frames, `--cap 0`)

| Snapshot | Reached (impure) | Distinct instances |
|---|---|---|
| FullSnap snap1-142012 | 20 | 6,450 |
| FullSnap snap2-142339 | 19 | 8,600 |
| FullSnap snap3-142654 | 19 | 6,700 |
| STOP snap1-162213 (frame 22150) | 15 | 1,700 |
| STOP snap2-162511 | 14 | 1,650 |
| STOP snap3-163043 | 15 | 1,650 |
| STOP snap4-163107 | 16 | 2,548 |
| **union of 7** | **23** | 23 traced specs in the bank |

No snapshot stopped early (`stop=None`, 0 frame errors). The 7-run `traced_cov` union gives 23 reached, **15 at or above 0.50** own-block coverage over all traced calls (`traced_cov_all7.json`, median over reached 0.5714). That coverage is the frame-trace measure over every traced call, before exact replay. The official coverage (below) is over the replayed cases.

### 2b. Exact replay (driver, traced-cases mode, union of 7 runs)

| Function | Replayed cases (of bank) | Bit-exact | Own-block (over replayed cases) | Floor |
|---|---|---|---|---|
| 1404FA720 | 700 / 700 | 700 | 0.09 | no |
| 1404FB2A0 | 300 / 700 | 300 | 0.31 | no |
| 140509A40 | 601 / 618 | 601 | 0.06 | no |
| 14057A3E0 | 10,350 / 10,367 | 10,350 | 0.29 | no |

**Mismatches: 0** across 11,951 replayed cases, traced-only and union runs alike. **Aborts: 0** (no replayed case reached an abort trap; no frame stopped on one). The other 46 specs have 0 replayable cases, so their corpus is empty and they are not diffed.

Floor count (bit-exact and own-block ≥ 0.50): **0 (traced-only and union)**.

### 2c. Stability

Traced-only driver run, forward and reversed bank order: `results.json` identical (`cmp` rc=0). Union bank byte-identical across orders. The floor computation does not depend on bank order.

## 3. Deviations and blockers (CONFIRMED unless tagged)

1. **Global store `14159FD0F` is not a spec output. It is a fast-fail path.** CONFIRMED: `1404F52E0` reaches `14159FCCC` (`MOV ECX,8; CALL 14159FCE0`) from cold blocks at `1404F5D93/99/9F`, entered by `JMP 1404F580D`, `JAE 1404F5911`, `JAE 1404F5CF0`. `14159FCE0` stores `0xC0000409` (`STATUS_STACK_BUFFER_OVERRUN`), writes the crash-report globals `14159FD0F/14159FD1F`, and ends in `INT 0x29` or a call to `14159FBC4` (not decoded). So the store is a side effect of a fatal report, and I modelled `14159FCCC` as an **abort-trap** (the same class as the thunk `1415A1714`), not as a spec output. No `record-write` kind exists; none was needed. [INFERENCE] No traced tick reached it (`stop=None`, 0 aborts).
2. **Team tick `1404F6C30` is still refused.** Its remaining blocker is outside the abort cut: `140537AC0` (a function-local static init) calls CRT import slot `14252F6E8` (`_Init_thread_header` per `ledger/indirect.py` KNOWN_SLOTS, no name in the image) and writes global `140537C4E`. Its closure also exceeds the 60-callee cap (`runnable_extra` logs `closure larger than 60`, which the driver ignores; pre-existing, see open item 4). The earlier abort-slot blocker `14252FDE0` is fixed (cut at `1415A1714`).
3. **Unseeded regions.** `build_live_seed` leaves unresolved chains zeroed. `IMG.1436F3DC8.E1EE` is unseeded in 16 of 23 specs, `A0.BB8`/`A0.6D50` in `1404F6C30`. A spec whose input sits in an unseeded region has no replayable case (1404FEDC0: 700/700 held-input failures, all on E1EE). CONFIRMED.
4. **Unmodelled reads.** A traced call reads memory outside the spec's region set, and the harness refuses it (`unmodelled read`). Seen on 14051CF70 and 140687310 in a one-frame validation. The spec closures came from the canonical capture and do not cover all traced reads. CONFIRMED for the validation run; [INFERENCE] for the 46 zero-case specs.
5. **Canonical captures carry no entry scalars**, so under exact replay their cases are excluded. The canonical-only specs and the union drop to near zero. This is why the earlier 15 does not carry over.

## 4. Checks run

- `py_compile` on every changed file. `bankstore.py --selftest` ok (dedupe count, order-free merge).
- Blocker rule, 5 assertions on `_chain_dispatchable`: 0 mismatches. Mutant (no abort traps): 1 mismatch, caught.
- One-frame scalar validation: 129 instances, all carry `scalars`. Replay of 14051CF70 / 140687310 no longer fails on inputs.
- PII grep over changed files: clean.
- Gate not run (Main gates). `driver --check` not run. Diff suite not run under the gate.

## 5. Open items (numbered)

1. Make the 46 zero-case specs replayable: seed the unseeded chains (E1EE, A0.BB8, A0.6D50), and widen the spec region set to cover traced reads. Without this the traced floor stays at zero.
2. Team tick `1404F6C30`: model CRT `_Init_thread_header` (`14252F6E8`) and global `140537C4E`, or confirm they are unreached in traced ticks with a per-address trace (`--trace-addrs`).
3. Floor count depends on which replays are exact. Report it with the list of excluded reasons, not as a property of the function.
4. `driver.runnable_extra` computes `extra_blocked` (closure cap 60) and never uses it. A callee beyond the cap becomes out-of-set (zeroed rax, the translator's OUT-OF-SET path). Pre-existing; should refuse the spec or raise the cap.
5. `diffmain` prints every mismatch now (`if (bad < 10)` replaced in the generated source). Confirm the gate's diff tests do not assume the cap.
6. `held_value` makes u64 and entry-scalar inputs real in bank mode. This changes bank-mode behaviour (previously random draws) and may move the gate's `diff_gen_*` results. Main should gate it.


## Main's reading

Exact replay of real traced calls diffs 11,951 cases with 0 mismatches, so wherever real calls can be replayed the
translation agrees with PES. The floor is 0 because the **spec model** cannot hold what real calls read: 46 of 50 specs
replay zero cases (their region set doesn't cover the memory the traced calls read; E1EE unseeded in 16 of 23). The next
step is to drop the spec region model for traced calls: run both sides against the snapshot itself as flat memory (the
diff main already maps a flat image copy-on-write), with each call's recorded registers and stack, comparing the return
value and every byte written.
