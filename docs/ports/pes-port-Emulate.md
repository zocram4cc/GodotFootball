# Emulate: the frame run made deterministic; the tick's missing registry object (26-10-26)

Branch `Emulate` (from `origin/FrameTrace` `02b03e81`). Pushed `d8bd482d` (determinism fix) and this report. Tool: `tools/dev/translate/frame_trace.py`. Scratch (captures, banks, runs) is PES-derived and not committed.

## Result

- **Determinism: fixed.** The stop frame varied because of a wall-clock timeout bug in the runner, not PES. Three identical runs per capture now give identical frame records (md5 match, all four captures).
- **Multi-frame run: NOT reached.** Every capture stops in frame 0, deterministically, on a read of an uncaptured heap object inside the tick. The cause is a missing registry object (§2), not a gate.
- **Reach: 1 of 50 per capture; union 1 of 50.** Only the team tick `1404F6C30` is entered. The other 49 are never entered.
- **Coverage: not measured.** The tick is `unmodellable` for the driver (closure reaches import slot `14252FDE0`, an abort trap in `stubs.py`). Translated: 0 of 1. Bit-exact: 0 of 1.
- **Comparison with 16/50 at the floor (median 0.30): not comparable.** The seed route (`seed.py` banks) reaches specs without running the tick. The frame route reaches only what the tick reaches, and the tick is blocked at its first call.

## 1. Determinism (item 1): cause and fix

**Cause (CONFIRMED).** `run_entry` passed `timeout_ms * 1000` to unicorn's `emu_start`, which takes microseconds. `--timeout` (default 120) is documented as seconds, so the wall-clock budget was 0.12 s. Unicorn returns from a timed-out `emu_start` with no error. A load-dependent cut therefore looked like a clean frame:

- Instrumented repeats of the unchanged runner, same hash seed and config: some runs returned with `err=None` and RIP at `0x1404F6C30` (the tick's first instruction, not executed) after 1055 recorded instructions; others reached the fault at `0x141E5CA52` after 1100 instructions. Both outcomes occurred for the same input.
- With `--timeout 100000` (wall clock cannot trigger): 6 of 6 identical, all faulting at frame 0.

**Fix (commit `d8bd482d`).** `run_entry` uses unicorn's instruction count (`count=budget`, `INSTR_BUDGET = 50_000_000` per H step, `--budget`). A return that does not reach the ret sentinel is now an error (`did not return: RIP ... after N-instruction budget`), never a clean frame.

**Proof (3 identical runs, PYTHONHASHSEED 1, 2, 3, 50 frames requested, `md5` of frame records minus wall-clock fields):**

| capture | frames run | reached | md5 run 1 / 2 / 3 |
|---|---|---|---|
| `f501-open-p3-play1-fr43009` | 1 | 1/50 | `0a28b9aca7` ×3 |
| `f1323-open-p1-play1-fr13724` | 1 | 1/50 | `220c4dac24` ×3 |
| `f1559-open-p1-play1-fr19044` | 1 | 1/50 | `8624436078` ×3 |
| `f01-open-p3-play1-fr30490` | 1 | 1/50 | `0a28b9aca7` ×3 |

The acceptance "deterministic multi-frame run" is not met: the runs are deterministic but stop at frame 0 (§2).

**Same bug class, not changed (open).** `diffharness/run.py` `run_case` (line 724) and `translate/snapshot.py` `run_ctor` (line 211) call `emu_start` with a timeout and do not check RIP after the call. A timed-out case returns partial state with `err=None`, so a timeout would be compared as a completed case. The driver's 5 s per-case budget makes this unlikely, not impossible. Recommended fix: the same RIP check as `frame_trace.run_entry`. Not applied here (it changes the driver's gate path).

## 2. Missing heap (item 2): the registry the tick reads first

The tick's first call chain (CONFIRMED, disassembly and capture bytes):

```
GLOBAL  0x1437F4808   (image .data; read at 0x141E5D4C0, the test rcx,rcx gate)
  -> obj  0x2976B10   (captured in all 4 FrameTrace captures and 28 Captures captures)
  -> table 0x2976B60  (234 pointer slots: 0x141E5DE80 returns the constant 0xEA)
  -> slot 183 (0xB7, edx set at 0x1404F6C83) at 0x2977118  -> entry object E
  -> E+8 read at 0x141E5CA52 (fault if E is uncaptured)
```

**The entry E differs by phase, and is captured in none of the 32 captures except `f1323`:**

| capture set | E (slot 183) | E+8 captured |
|---|---|---|
| `f501`, `f01` (FrameTrace) | `0x363E2880` | no: fault at `0x363E2888` |
| `f1559` (FrameTrace) | `0x2CA3A7D0` | no: fault at `0x2CA3A7D8` |
| `f1323` (FrameTrace) | `0x2CA3A7D0` | yes (value `0x33132F70`); next fault below |
| Captures `captures_e`, `_f`, `_g`, `_h` (per 3 at f37k–45k; per 8–10 at f48k–57k) | `0x2A69ED40` | no |
| Captures `captures_i` (per 1–3, f18k–32k) | `0x2CBB7A70` | no |

(`chain.py` scratch. The 28 Captures captures are every capture with a `live.json` in `captures_e` through `captures_i`; all 28 show E+8 uncaptured.)

**`f1323` goes further and stops here:** the tick reaches `0x140A3E8E0`, which reads 4 bytes at `0x7FF5DFDE7208`. No captured word points at that page (scan of `f1323` and `f1559`: 0 containers). So its source is outside the captured state (computed from a register; a thread-local or per-thread region is likely, not verified). OPEN.

**Why the BFS did not capture E.** `capture_frame.py` walks from G, H, the teams, MI and MA with a 1500-object budget. The registry global `0x1437F4808` is not a root, and the budget is spent before the registry is reached (`bfs.objs = 1500` in every capture). Each capture's E is therefore whatever the BFS happened to touch.

**What a future live capture must include (named, not captured):**
1. `GLOBAL 0x1437F4808` as a root (8 bytes), and the object it points to (the registry object, ≥ 8 bytes at its start).
2. The table it points to (234 × 8 bytes), at least slot 183.
3. Entry E for the phase of the capture: its first 16 bytes (the +8 field) and the pointer chain from E+8 (the object read at `0x140A3E8E0` in `f1323`).
4. The source of `0x7FF5DFDE7208`: the register base at `0x140A3E8E0`'s caller, not yet traced. Needs its own region in the capture.
5. Capture the registry in the same phase as the H object: E changes with match phase (§2 table), so captures cannot be pooled.

**Zero-fill diagnostic (scratch, not a fidelity result).** Zero-mapping each faulting page and rerunning: after `0x363E2000` the next uncaptured access is `0x7FF5DFDE7208` (f1323) or `0x0A3FED20` (read at `0x141B5238F`), then `0x68` at `0x141B52392`, then a fetch at `0xFA8`. Zeroed pointers derail the tick into a zero vtable, so zero-fill cannot enumerate the whole required set. Only the first object is established.

## 3. Early exit (item 3): gates and the state they test

There is **no branch in the tick's first ~30 instructions that sends it out** (CONFIRMED, `1404F6C30`–`1404F6CA5`). It stores the two arguments into `[rcx+0x6D48]` and `[rcx+0x6D50]`, loads `G = [0x1436F3DC8]`, reads `MI = [G+0x270]` into `r12`, sets `edx = 0xB7`, and calls `0x141E5D4C0`.

Inside the lookup chain (CONFIRMED):
- `0x141E5D4C0`: `test rcx,rcx` on the registry global. Non-null in every capture (`0x2976B10`). Passes.
- `0x141E5CA20`: `cmp eax(0xEA), ebx(0xB7); jg` passes (234 > 183).
- `0x141E5CA50`: `je` if the slot E is null: returns 0. This is the only gate in the lookup. Every capture has E non-null, so the path continues to the `[E+8]` read and faults.

So the "early exit" in the FrameTrace report was the uncaptured `E+8` read, not a gate. The silent exit was the timeout bug (§1). The gates the tick applies after the call (`[G+0x3B0]` non-null at `0x1404F6CBE`, then `0x1423E7FD0`) are reached only if the call returns; in `f1323` the tick passes the registry and fails later, so their state is not yet tested.

## 4. Trace (item 4)

The 50 impure entries are hooked (`frame_trace.py`, unchanged design). Across the four captures, one frame each: **only `1404F6C30` is entered**, once per capture. Its banks (driver format, one instance per capture): merged union of 4 instances, `banks/1404F6C30.bank.json` (scratch). The other 49 have 0 calls. Impure coverage cannot come from these runs until the registry is available (§2).

## 5. Measure (item 5)

- Frames run per capture: **1** (of 50 requested) for each of the four. Stops: `f501`, `f01`: `0x141E5CA52` read `0x363E2888`; `f1559`: `0x141E5CA52` read `0x2CA3A7D8`; `f1323`: `0x140A3E8E0` read `0x7FF5DFDE7208`.
- Reached: **1/50** per capture, union **1/50** (`1404F6C30`).
- Own-block coverage: not measured. Translated side: none.
- Driver (`--only 1404F6C30`, union bank, 4 instances): `translated=False diffed=False cases=64 class=unmodellable`, callee chain `1404FEDC0` → `140A58550` → `1415A1714` → import slot `14252FDE0`, which `stubs.py` (`ABORT_SLOTS`) names as an abort trap. Bit-exact: 0 of 1.
- Comparison with the Captures union (16/50 at the floor, median 0.30): not comparable (see Result).

## 6. Open items (numbered)

1. **Registry capture (blocks the multi-frame run and all reach):** a live capture including the registry root, table and phase-correct E (§2, list 1–5). The 1500-object BFS must not spend its budget before the registry. Needs PES live and read-only pread only.
2. **`0x7FF5DFDE7208`:** find the register base at `0x140A3E8E0`'s caller, then whether the region is captured. OPEN.
3. **Abort slot `14252FDE0`:** the tick's static closure reaches it via `1404FEDC0` → `140A58550` → `1415A1714`. The frame run never reaches that call (it stops earlier). Whether PES reaches the tamper path needs the translation-side decision (model the call path or prove it unreached).
4. **Silent-stop check in `run.py` `run_case` and `snapshot.py` `run_ctor`** (§1): recommended fix, not applied.
5. **Captures_* phase split:** the registry entry changes per phase (§2 table). Any union over phases is invalid for the tick.

## 7. Verification

- Runs (scratch, PES-derived): repeated `frame_trace.py` runs on `f501` and the three other captures (3 identical runs each, §1), instrumented diagnostics (`diag_seq.py`, `diag_main.py`, `enum_missing.py`, `chain.py`, `scan_ptr.py`), driver `--only 1404F6C30` (rc 0, translated 0).
- Tests: none new (harness tooling). The gate was not run (Main gates).
- Mutations: none run beyond the timeout experiment: `--timeout 100000` (6 of 6 identical, all frame-0 faults) versus the default (both outcomes seen).
- Commit: `d8bd482d` (`tools/dev/translate/frame_trace.py` only; `extern/godot-cpp` symlink not staged). `ls-remote` confirms `refs/heads/Emulate` at `d8bd482d`.
